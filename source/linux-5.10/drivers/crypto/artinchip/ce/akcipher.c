// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, Artinchip Technology Co., Ltd
 *
 * Wu Dehuang <dehuang.wu@artinchip.com>
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <crypto/internal/rsa.h>
#include <crypto/engine.h>
#include <crypto/scatterwalk.h>
#include <crypto/internal/akcipher.h>
#include "crypto.h"
#include "ssram.h"

#define FLG_ENC		BIT(0)
#define FLG_DEC		BIT(1)
#define FLG_SIGN	BIT(2)
#define FLG_VERI	BIT(3)

#define FLG_KEYOFFSET   (16)
#define FLG_KEYMASK	GENMASK(19, 16) /* eFuse Key ID */

struct aic_akcipher_alg {
	struct akcipher_alg alg;
	struct aic_crypto_dev *ce;
};

struct aic_akcipher_tfm_ctx {
	struct crypto_engine_ctx enginectx;
	struct aic_crypto_dev *ce;
	unsigned char *n;
	unsigned char *e;
	unsigned char *d;
	unsigned int n_sz;
	unsigned int e_sz;
	unsigned int d_sz;
};

struct aic_akcipher_reqctx {
	struct task_desc *task;
	dma_addr_t phy_task;
	unsigned char *wbuf;
	dma_addr_t phy_wbuf;
	dma_addr_t ssram_addr;
	int tasklen;
	unsigned int wbuf_size;
	unsigned long flags;
};

static inline bool is_need_genhsk(unsigned long flg)
{
	return (flg & (FLG_KEYMASK));
}

static inline unsigned int rsa_key_size(unsigned int val)
{
	return val & (~0xF);
}

static int aic_rsa_task_cfg(struct aic_crypto_dev *ce,
			    struct akcipher_request *req)
{
	struct aic_akcipher_reqctx *rctx;
	dma_addr_t phy_in, phy_out, phy_n, phy_prim;

	pr_debug("%s\n", __func__);
	rctx = akcipher_request_ctx(req);
	rctx->tasklen = sizeof(struct task_desc);
	rctx->task = dma_alloc_coherent(ce->dev, rctx->tasklen, &rctx->phy_task,
					GFP_KERNEL);
	if (!rctx->task)
		return -ENOMEM;

	memset(rctx->task, 0, rctx->tasklen);

	phy_prim = rctx->phy_wbuf;
	phy_n = rctx->phy_wbuf + req->src_len;
	phy_in = rctx->phy_wbuf + req->src_len * 2;
	phy_out = rctx->phy_wbuf + req->src_len * 3;

	rctx->task->alg.rsa.alg_tag = ALG_TAG_RSA;
	if (req->src_len == 256) {
		rctx->task->alg.rsa.op_siz = KEY_SIZE_2048;
	} else if (req->src_len == 128) {
		rctx->task->alg.rsa.op_siz = KEY_SIZE_1024;
	} else if (req->src_len == 64) {
		rctx->task->alg.rsa.op_siz = KEY_SIZE_512;
	} else {
		dev_err(ce->dev, "Not supported key size %d.\n", req->src_len);
		return -1;
	}

	if (rctx->ssram_addr) {
		rctx->task->alg.rsa.d_e_addr = rctx->ssram_addr;
		rctx->task->alg.rsa.m_addr = rctx->ssram_addr + req->src_len;
	} else {
		rctx->task->alg.rsa.d_e_addr = phy_prim;
		rctx->task->alg.rsa.m_addr = phy_n;
	}

	rctx->task->data.in_addr = phy_in;
	rctx->task->data.in_len = (u32)req->src_len;
	rctx->task->data.out_addr = phy_out;
	rctx->task->data.out_len = (u32)req->dst_len;
	rctx->task->next = 0;

	return 0;
}

static int aic_akcipher_unprepare_req(struct crypto_engine *engine, void *areq)
{
	struct aic_akcipher_tfm_ctx *ctx;
	struct aic_akcipher_reqctx *rctx;
	struct akcipher_request *req;
	struct crypto_akcipher *tfm;
	struct aic_crypto_dev *ce;

	pr_debug("%s\n", __func__);
	req = container_of(areq, struct akcipher_request, base);
	tfm = crypto_akcipher_reqtfm(req);
	ctx = akcipher_tfm_ctx(tfm);
	rctx = akcipher_request_ctx(req);
	ce = ctx->ce;

	if (rctx->wbuf) {
		dma_free_coherent(ce->dev, rctx->wbuf_size, rctx->wbuf,
				  rctx->phy_wbuf);
		rctx->phy_wbuf = 0;
		rctx->wbuf = NULL;
		rctx->wbuf_size = 0;
	}
	if (rctx->ssram_addr) {
		aic_ssram_free(ce, rctx->ssram_addr, ctx->n_sz * 3);
		rctx->ssram_addr = 0;
	}
	if (rctx->task) {
		dma_free_coherent(ce->dev, rctx->tasklen, rctx->task,
				  rctx->phy_task);
		rctx->task = NULL;
		rctx->phy_task = 0;
		rctx->tasklen = 0;
	}
	return 0;
}

static int aic_akcipher_prepare_req(struct crypto_engine *engine, void *areq)
{
	struct aic_akcipher_tfm_ctx *ctx;
	struct aic_akcipher_reqctx *rctx;
	struct akcipher_request *req;
	struct crypto_akcipher *tfm;
	struct aic_crypto_dev *ce;
	unsigned char *p, *n, *data;
	unsigned int key_size;
	int ret;

	pr_debug("%s\n", __func__);
	req = container_of(areq, struct akcipher_request, base);
	tfm = crypto_akcipher_reqtfm(req);
	ctx = akcipher_tfm_ctx(tfm);
	rctx = akcipher_request_ctx(req);
	ce = ctx->ce;

	key_size = rsa_key_size(ctx->n_sz);
	rctx->wbuf_size = key_size * 4; /* 3 operands + 1 output */
	rctx->wbuf = dma_alloc_coherent(ce->dev, rctx->wbuf_size,
					&rctx->phy_wbuf, GFP_KERNEL);

	if (!rctx->wbuf) {
		dev_err(ce->dev, "Failed to alloc work buffer.\n");
		return -ENOMEM;
	}

	/* copy d/e, n, data to working buffer */
	p = rctx->wbuf;
	n = p + key_size;
	data = n + key_size;
	aic_crypto_sg_copy(data, req->src, req->src_len, 0);
	aic_crypto_bignum_byteswap(data, req->src_len);

	if (ctx->d)
		aic_crypto_bignum_be2le(ctx->d, ctx->d_sz, p, key_size);
	if (ctx->e)
		aic_crypto_bignum_be2le(ctx->e, ctx->e_sz, p, key_size);
	aic_crypto_bignum_be2le(ctx->n, ctx->n_sz, n, key_size);

	if (DEBUG_CE)
		print_hex_dump(KERN_ERR, "wbuf: ", DUMP_PREFIX_NONE, 16, 1,
			       rctx->wbuf, rctx->wbuf_size, false);

	rctx->ssram_addr = 0;
	if (is_need_genhsk(rctx->flags)) {
		unsigned int km_siz;
		int efuse;

		km_siz = key_size * 2;
		rctx->ssram_addr = aic_ssram_alloc(ce, km_siz);
		if (!rctx->ssram_addr) {
			dev_err(ce->dev, "Failed to allocate ssram key\n");
			ret = -ENOMEM;
			goto err;
		}
		efuse = (rctx->flags & FLG_KEYMASK) >> FLG_KEYOFFSET;

		ret = aic_ssram_des_genkey(ce, efuse, rctx->wbuf, km_siz,
					   rctx->ssram_addr);
		if (ret) {
			dev_err(ce->dev, "Failed to gen hsk\n");
			goto err;
		}
	}

	ret = aic_rsa_task_cfg(ce, req);
	if (ret) {
		dev_err(ce->dev, "Failed to cfg task\n");
		goto err;
	}

	return 0;

err:
	aic_akcipher_unprepare_req(engine, areq);
	return ret;
}

void aic_akcipher_handle_irq(struct aic_crypto_dev *ce)
{
	struct aic_akcipher_reqctx *rctx;
	struct akcipher_request *req;
	unsigned char *outbuf;
	void *areq;
	int err, ret;

	mutex_lock(&ce->ak_accel.req_lock);
	if (kfifo_len(&ce->ak_accel.req_fifo) < 1) {
		dev_err(ce->dev, "There is no req in fifo.\n");
		goto err;
	}

	/*
	 * TODO: Maybe more than one task is finished, should check here
	 */
	/* kfifo_peek(&ce->ak_accel.req_fifo, &areq); */
	ret = kfifo_get(&ce->ak_accel.req_fifo, &areq);
	if (ret != 1) {
		dev_err(ce->dev, "Get req from fifo failed.\n");
		goto err;
	}
	mutex_unlock(&ce->ak_accel.req_lock);

	req = container_of(areq, struct akcipher_request, base);
	rctx = akcipher_request_ctx(req);

	err = (ce->err_status >> 8 * AK_ALG_ACCELERATOR) & 0xFF;
	if (!err) {
		outbuf = rctx->wbuf + 3 * req->dst_len;
		aic_crypto_bignum_byteswap(outbuf, req->dst_len);
		aic_crypto_sg_copy(outbuf, req->dst, req->dst_len, 1);
	}
	crypto_finalize_akcipher_request(ce->ak_accel.engine, req, err);
	if (DEBUG_CE)
		aic_crypto_dump_reg(ce);
	return;
err:
	mutex_unlock(&ce->ak_accel.req_lock);
}

static int aic_akcipher_do_one_req(struct crypto_engine *engine, void *areq)
{
	struct akcipher_request *req;
	struct crypto_akcipher *tfm;
	struct aic_akcipher_tfm_ctx *ctx;
	struct aic_akcipher_reqctx *rctx;
	struct aic_crypto_dev *ce;
	unsigned int ret;
	u32 algo;

	pr_debug("%s\n", __func__);
	req = container_of(areq, struct akcipher_request, base);
	tfm = crypto_akcipher_reqtfm(req);
	ctx = akcipher_tfm_ctx(tfm);
	rctx = akcipher_request_ctx(req);
	ce = ctx->ce;

	if (!ce) {
		pr_err("Device is null.\n");
		return -ENODEV;
	}

	if (!aic_crypto_is_ce_avail(ce)) {
		dev_err(ce->dev, "Crypto engine is busy.\n");
		return -EBUSY;
	}

	if (!aic_crypto_is_accel_avail(ce, AK_ALG_ACCELERATOR)) {
		dev_err(ce->dev, "ak accelerator fifo is full.\n");
		return -EBUSY;
	}

	mutex_lock(&ce->ak_accel.req_lock);
	ret = kfifo_put(&ce->ak_accel.req_fifo, areq);
	mutex_unlock(&ce->ak_accel.req_lock);
	if (ret != 1) {
		dev_err(ce->dev, "req fifo is full.\n");
		return ret;
	}

	if (DEBUG_CE)
		aic_crypto_dump_task(rctx->task, rctx->tasklen);
	algo = rctx->task->alg.alg_tag;
	aic_crypto_irq_enable(ce, AK_ALG_ACCELERATOR);
	aic_crypto_enqueue_task(ce, algo, rctx->phy_task);
	if (DEBUG_CE)
		aic_crypto_dump_reg(ce);
	return 0;
}

static int aic_akcipher_rsa_alg_init(struct crypto_akcipher *tfm)
{
	struct aic_akcipher_tfm_ctx *ctx = akcipher_tfm_ctx(tfm);
	struct akcipher_alg *akalg = crypto_akcipher_alg(tfm);
	struct aic_akcipher_alg *aicalg;

	memset(ctx, 0, sizeof(*ctx));

	aicalg = container_of(akalg, struct aic_akcipher_alg, alg);
	ctx->ce = aicalg->ce;
	ctx->enginectx.op.do_one_request = aic_akcipher_do_one_req;
	ctx->enginectx.op.prepare_request = aic_akcipher_prepare_req;
	ctx->enginectx.op.unprepare_request = aic_akcipher_unprepare_req;
	return 0;
}

static void aic_akcipher_rsa_clear_key(struct aic_akcipher_tfm_ctx *ctx)
{
	if (ctx->n)
		kfree_sensitive(ctx->n);
	if (ctx->d)
		kfree_sensitive(ctx->d);
	if (ctx->e)
		kfree_sensitive(ctx->e);
	ctx->n = NULL;
	ctx->e = NULL;
	ctx->d = NULL;
}

static void aic_akcipher_rsa_alg_exit(struct crypto_akcipher *tfm)
{
	struct aic_akcipher_tfm_ctx *ctx = akcipher_tfm_ctx(tfm);

	aic_akcipher_rsa_clear_key(ctx);
}

static int aic_akcipher_rsa_crypt(struct akcipher_request *req,
				  unsigned long flag)
{
	struct aic_akcipher_tfm_ctx *ctx;
	struct aic_akcipher_reqctx *rctx;
	struct crypto_akcipher *tfm;
	struct crypto_engine *eng;
	unsigned int key_size;
	int ret;

	pr_debug("%s\n", __func__);
	tfm = crypto_akcipher_reqtfm(req);
	ctx = akcipher_tfm_ctx(tfm);
	rctx = akcipher_request_ctx(req);

	if (!ctx || !ctx->ce) {
		pr_err("aic akcipher, device is null\n");
		return -ENODEV;
	}

	rctx->flags = flag;

	key_size = rsa_key_size(ctx->n_sz);
	if (req->src_len != key_size || req->dst_len != key_size) {
		pr_err("src length is not the same with key size\n");
		return -EINVAL;
	}

	eng = ctx->ce->ak_accel.engine;
	ret = crypto_transfer_akcipher_request_to_engine(eng, req);

	return ret;
}

static int aic_akcipher_rsa_encrypt(struct akcipher_request *req)
{
	return aic_akcipher_rsa_crypt(req, FLG_ENC);
}

static int aic_akcipher_rsa_decrypt(struct akcipher_request *req)
{
	return aic_akcipher_rsa_crypt(req, FLG_DEC);
}

static int aic_akcipher_pnk_rsa_encrypt(struct akcipher_request *req)
{
	unsigned long flags;

	flags = FLG_ENC | (CE_KEY_SRC_PNK << FLG_KEYOFFSET);
	return aic_akcipher_rsa_crypt(req, flags);
}

static int aic_akcipher_pnk_rsa_decrypt(struct akcipher_request *req)
{
	unsigned long flags;

	flags = FLG_DEC | (CE_KEY_SRC_PNK << FLG_KEYOFFSET);
	return aic_akcipher_rsa_crypt(req, flags);
}

static int aic_akcipher_psk0_rsa_encrypt(struct akcipher_request *req)
{
	unsigned long flags;

	flags = FLG_ENC | (CE_KEY_SRC_PSK0 << FLG_KEYOFFSET);
	return aic_akcipher_rsa_crypt(req, flags);
}

static int aic_akcipher_psk0_rsa_decrypt(struct akcipher_request *req)
{
	unsigned long flags;

	flags = FLG_DEC | (CE_KEY_SRC_PSK0 << FLG_KEYOFFSET);
	return aic_akcipher_rsa_crypt(req, flags);
}

static int aic_akcipher_psk1_rsa_encrypt(struct akcipher_request *req)
{
	unsigned long flags;

	flags = FLG_ENC | (CE_KEY_SRC_PSK1 << FLG_KEYOFFSET);
	return aic_akcipher_rsa_crypt(req, flags);
}

static int aic_akcipher_psk1_rsa_decrypt(struct akcipher_request *req)
{
	unsigned long flags;

	flags = FLG_DEC | (CE_KEY_SRC_PSK1 << FLG_KEYOFFSET);
	return aic_akcipher_rsa_crypt(req, flags);
}

static int aic_akcipher_psk2_rsa_encrypt(struct akcipher_request *req)
{
	unsigned long flags;

	flags = FLG_ENC | (CE_KEY_SRC_PSK2 << FLG_KEYOFFSET);
	return aic_akcipher_rsa_crypt(req, flags);
}

static int aic_akcipher_psk2_rsa_decrypt(struct akcipher_request *req)
{
	unsigned long flags;

	flags = FLG_DEC | (CE_KEY_SRC_PSK2 << FLG_KEYOFFSET);
	return aic_akcipher_rsa_crypt(req, flags);
}

static int aic_akcipher_psk3_rsa_encrypt(struct akcipher_request *req)
{
	unsigned long flags;

	flags = FLG_ENC | (CE_KEY_SRC_PSK3 << FLG_KEYOFFSET);
	return aic_akcipher_rsa_crypt(req, flags);
}

static int aic_akcipher_psk3_rsa_decrypt(struct akcipher_request *req)
{
	unsigned long flags;

	flags = FLG_DEC | (CE_KEY_SRC_PSK3 << FLG_KEYOFFSET);
	return aic_akcipher_rsa_crypt(req, flags);
}

static int aic_akcipher_rsa_set_pub_key(struct crypto_akcipher *tfm,
					const void *key, unsigned int keylen)
{
	struct aic_akcipher_tfm_ctx *ctx = akcipher_tfm_ctx(tfm);
	struct rsa_key rsa_key;
	int ret;

	pr_debug("%s\n", __func__);
	aic_akcipher_rsa_clear_key(ctx);
	if (DEBUG_CE)
		print_hex_dump(KERN_ERR, "pubkey: ", DUMP_PREFIX_NONE, 16, 1,
			       key, keylen, false);
	ret = rsa_parse_pub_key(&rsa_key, key, keylen);
	if (ret) {
		dev_err(ctx->ce->dev, "Parse pub key failed.\n");
		goto err;
	}
	ctx->n = kmemdup(rsa_key.n, rsa_key.n_sz, GFP_KERNEL);
	if (!ctx->n) {
		dev_err(ctx->ce->dev, "Copy RSA Key N failed.\n");
		ret = -ENOMEM;
		goto err;
	}
	ctx->n_sz = rsa_key.n_sz;
	ctx->e = kmemdup(rsa_key.e, rsa_key.e_sz, GFP_KERNEL);
	if (!ctx->e) {
		dev_err(ctx->ce->dev, "Copy RSA Key E failed.\n");
		ret = -ENOMEM;
		goto err;
	}
	ctx->e_sz = rsa_key.e_sz;
	return 0;
err:
	if (ctx->n)
		kfree_sensitive(ctx->n);
	if (ctx->e)
		kfree_sensitive(ctx->e);
	ctx->n = NULL;
	ctx->e = NULL;
	return ret;
}

static int aic_akcipher_rsa_set_priv_key(struct crypto_akcipher *tfm,
					 const void *key, unsigned int keylen)
{
	struct aic_akcipher_tfm_ctx *ctx = akcipher_tfm_ctx(tfm);
	struct rsa_key rsa_key;
	int ret;

	pr_debug("%s\n", __func__);
	aic_akcipher_rsa_clear_key(ctx);
	if (DEBUG_CE)
		print_hex_dump(KERN_ERR, "privkey: ", DUMP_PREFIX_NONE, 16, 1,
			       key, keylen, false);
	ret = rsa_parse_priv_key(&rsa_key, key, keylen);
	if (ret) {
		dev_err(ctx->ce->dev, "parse priv key error.\n");
		goto err;
	}

	if (DEBUG_CE) {
		print_hex_dump(KERN_ERR, "n: ", DUMP_PREFIX_NONE, 16, 1,
			       rsa_key.n, rsa_key.n_sz, false);
		print_hex_dump(KERN_ERR, "d: ", DUMP_PREFIX_NONE, 16, 1,
			       rsa_key.d, rsa_key.d_sz, false);
		print_hex_dump(KERN_ERR, "e: ", DUMP_PREFIX_NONE, 16, 1,
			       rsa_key.e, rsa_key.e_sz, false);
	}
	ctx->n = kmemdup(rsa_key.n, rsa_key.n_sz, GFP_KERNEL);
	if (!ctx->n) {
		dev_err(ctx->ce->dev, "Copy RSA Key N failed.\n");
		ret = -ENOMEM;
		goto err;
	}
	ctx->n_sz = rsa_key.n_sz;
	ctx->d = kmemdup(rsa_key.d, rsa_key.d_sz, GFP_KERNEL);
	if (!ctx->d) {
		dev_err(ctx->ce->dev, "Copy RSA Key D failed.\n");
		ret = -ENOMEM;
		goto err;
	}
	ctx->d_sz = rsa_key.d_sz;
	return 0;
err:
	if (ctx->n)
		kfree_sensitive(ctx->n);
	if (ctx->e)
		kfree_sensitive(ctx->e);
	ctx->n = NULL;
	ctx->e = NULL;
	return ret;
}

static unsigned int aic_akcipher_rsa_max_size(struct crypto_akcipher *tfm)
{
	struct aic_akcipher_tfm_ctx *ctx = akcipher_tfm_ctx(tfm);

	return rsa_key_size(ctx->n_sz);
}

static struct aic_akcipher_alg ak_algs[] = {
{
	.alg = {
		.encrypt = aic_akcipher_rsa_encrypt,
		.decrypt = aic_akcipher_rsa_decrypt,
		.set_pub_key = aic_akcipher_rsa_set_pub_key,
		.set_priv_key = aic_akcipher_rsa_set_priv_key,
		.max_size = aic_akcipher_rsa_max_size,
		.init = aic_akcipher_rsa_alg_init,
		.exit = aic_akcipher_rsa_alg_exit,
		.reqsize = sizeof(struct aic_akcipher_reqctx),
		.base = {
			.cra_name = "rsa",
			.cra_driver_name = "rsa-aic",
			.cra_priority = 400,
			.cra_module = THIS_MODULE,
			.cra_ctxsize = sizeof(struct aic_akcipher_tfm_ctx),
		},
	},
},
{
	.alg = {
		.encrypt = aic_akcipher_pnk_rsa_encrypt,
		.decrypt = aic_akcipher_pnk_rsa_decrypt,
		.set_pub_key = aic_akcipher_rsa_set_pub_key,
		.set_priv_key = aic_akcipher_rsa_set_priv_key,
		.max_size = aic_akcipher_rsa_max_size,
		.init = aic_akcipher_rsa_alg_init,
		.exit = aic_akcipher_rsa_alg_exit,
		.reqsize = sizeof(struct aic_akcipher_reqctx),
		.base = {
			.cra_name = "pnk-protected(rsa)",
			.cra_driver_name = "pnk-protected-rsa-aic",
			.cra_priority = 400,
			.cra_module = THIS_MODULE,
			.cra_ctxsize = sizeof(struct aic_akcipher_tfm_ctx),
		},
	},
},
{
	.alg = {
		.encrypt = aic_akcipher_psk0_rsa_encrypt,
		.decrypt = aic_akcipher_psk0_rsa_decrypt,
		.set_pub_key = aic_akcipher_rsa_set_pub_key,
		.set_priv_key = aic_akcipher_rsa_set_priv_key,
		.max_size = aic_akcipher_rsa_max_size,
		.init = aic_akcipher_rsa_alg_init,
		.exit = aic_akcipher_rsa_alg_exit,
		.reqsize = sizeof(struct aic_akcipher_reqctx),
		.base = {
			.cra_name = "psk0-protected(rsa)",
			.cra_driver_name = "psk0-protected-rsa-aic",
			.cra_priority = 400,
			.cra_module = THIS_MODULE,
			.cra_ctxsize = sizeof(struct aic_akcipher_tfm_ctx),
		},
	},
},
{
	.alg = {
		.encrypt = aic_akcipher_psk1_rsa_encrypt,
		.decrypt = aic_akcipher_psk1_rsa_decrypt,
		.set_pub_key = aic_akcipher_rsa_set_pub_key,
		.set_priv_key = aic_akcipher_rsa_set_priv_key,
		.max_size = aic_akcipher_rsa_max_size,
		.init = aic_akcipher_rsa_alg_init,
		.exit = aic_akcipher_rsa_alg_exit,
		.reqsize = sizeof(struct aic_akcipher_reqctx),
		.base = {
			.cra_name = "psk1-protected(rsa)",
			.cra_driver_name = "psk1-protected-rsa-aic",
			.cra_priority = 400,
			.cra_module = THIS_MODULE,
			.cra_ctxsize = sizeof(struct aic_akcipher_tfm_ctx),
		},
	},
},
{
	.alg = {
		.encrypt = aic_akcipher_psk2_rsa_encrypt,
		.decrypt = aic_akcipher_psk2_rsa_decrypt,
		.set_pub_key = aic_akcipher_rsa_set_pub_key,
		.set_priv_key = aic_akcipher_rsa_set_priv_key,
		.max_size = aic_akcipher_rsa_max_size,
		.init = aic_akcipher_rsa_alg_init,
		.exit = aic_akcipher_rsa_alg_exit,
		.reqsize = sizeof(struct aic_akcipher_reqctx),
		.base = {
			.cra_name = "psk2-protected(rsa)",
			.cra_driver_name = "psk2-protected-rsa-aic",
			.cra_priority = 400,
			.cra_module = THIS_MODULE,
			.cra_ctxsize = sizeof(struct aic_akcipher_tfm_ctx),
		},
	},
},
{
	.alg = {
		.encrypt = aic_akcipher_psk3_rsa_encrypt,
		.decrypt = aic_akcipher_psk3_rsa_decrypt,
		.set_pub_key = aic_akcipher_rsa_set_pub_key,
		.set_priv_key = aic_akcipher_rsa_set_priv_key,
		.max_size = aic_akcipher_rsa_max_size,
		.init = aic_akcipher_rsa_alg_init,
		.exit = aic_akcipher_rsa_alg_exit,
		.reqsize = sizeof(struct aic_akcipher_reqctx),
		.base = {
			.cra_name = "psk3-protected(rsa)",
			.cra_driver_name = "psk3-protected-rsa-aic",
			.cra_priority = 400,
			.cra_module = THIS_MODULE,
			.cra_ctxsize = sizeof(struct aic_akcipher_tfm_ctx),
		},
	},
},
};

int aic_crypto_akcipher_accelerator_init(struct aic_crypto_dev *ce)
{
	struct crypto_engine *eng;
	int ret, i;

	eng = crypto_engine_alloc_init_and_set(ce->dev, true, NULL, true,
					       ACCEL_QUEUE_MAX_SIZE);
	if (!eng) {
		dev_err(ce->dev, "Failed to init crypto engine for akcipher\n");
		ret = -ENOMEM;
		return ret;
	}

	ce->ak_accel.engine = eng;
	ret = crypto_engine_start(ce->ak_accel.engine);
	if (ret) {
		dev_err(ce->dev, "Failed to start crypto akcipher engine\n");
		goto err;
	}

	mutex_init(&ce->ak_accel.req_lock);
	INIT_KFIFO(ce->ak_accel.req_fifo);
	ret = kfifo_alloc(&ce->ak_accel.req_fifo, ACCEL_QUEUE_MAX_SIZE,
			  GFP_KERNEL);
	if (ret) {
		dev_err(ce->dev, "Failed to alloc kfifo for akcipher\n");
		goto err2;
	}

	for (i = 0; i < ARRAY_SIZE(ak_algs); i++) {
		ak_algs[i].ce = ce;
		ret = crypto_register_akcipher(&ak_algs[i].alg);
		if (ret) {
			dev_err(ce->dev, "Failed to register akcipher algs\n");
			goto err3;
		}
	}

	return 0;

err3:
	for (--i; i >= 0; --i) {
		ak_algs[i].ce = NULL;
		crypto_unregister_akcipher(&ak_algs[i].alg);
	}
err2:
	kfifo_free(&ce->ak_accel.req_fifo);
err:
	crypto_engine_exit(ce->ak_accel.engine);
	return ret;
}

int aic_crypto_akcipher_accelerator_exit(struct aic_crypto_dev *ce)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ak_algs); i++) {
		ak_algs[i].ce = NULL;
		crypto_unregister_akcipher(&ak_algs[i].alg);
	}
	kfifo_free(&ce->ak_accel.req_fifo);
	crypto_engine_exit(ce->ak_accel.engine);

	return 0;
}
