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
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>
#include <crypto/engine.h>
#include <crypto/hash.h>
#include <crypto/md5.h>
#include <crypto/scatterwalk.h>
#include <crypto/sha.h>
#include <crypto/internal/hash.h>
#include "crypto.h"

#define CE_MAX_DIGEST_SIZE	64
#define CE_TOTAL_BITLEN_SIZE	32
#define MD5_CE_OUTPUT_LEN	16
#define SHA1_CE_OUTPUT_LEN	20
#define SHA224_CE_OUTPUT_LEN	32
#define SHA256_CE_OUTPUT_LEN	32
#define SHA384_CE_OUTPUT_LEN	64
#define SHA512_CE_OUTPUT_LEN	64

#define FLG_HMAC		BIT(0)
#define FLG_MD5			BIT(1)
#define FLG_SHA1		BIT(2)
#define FLG_SHA224		BIT(3)
#define FLG_SHA256		BIT(4)
#define FLG_SHA384		BIT(5)
#define FLG_SHA512		BIT(6)
#define FLG_FIRST		BIT(7)
#define FLG_UPDATE		BIT(8)
#define FLG_FINAL		BIT(9)

#define aligned_addr64bit(ptr)	(((((dma_addr_t)(ptr)) + 7) >> 3) << 3)

struct aic_hash_alg {
	struct ahash_alg alg;
	struct aic_crypto_dev *ce;
};

struct aic_hash_tfm_ctx {
	struct crypto_engine_ctx enginectx;
	struct aic_crypto_dev *ce;
	bool hmac;
	void *remain_buf;
	u32 remain_len;
	u32 total_len;
};

struct aic_hash_reqctx {
	struct task_desc *task;
	dma_addr_t phy_task;
	unsigned char *ivbuf;
	dma_addr_t phy_ivbuf;
	void *src_cpy_buf;
	dma_addr_t src_phy_buf;
	int src_cpy_buf_len;
	int tasklen;
	unsigned int digest_size;
	unsigned long flags;
	unsigned char digest[CE_MAX_DIGEST_SIZE];
	bool src_map_sg;
};

static inline bool is_hmac(unsigned long flg)
{
	return (flg & FLG_HMAC);
}

static inline bool is_md5(unsigned long flg)
{
	return (flg & FLG_MD5);
}

static inline bool is_sha1(unsigned long flg)
{
	return (flg & FLG_SHA1);
}

static inline bool is_sha224(unsigned long flg)
{
	return (flg & FLG_SHA224);
}

static inline bool is_sha256(unsigned long flg)
{
	return (flg & FLG_SHA256);
}

static inline bool is_sha384(unsigned long flg)
{
	return (flg & FLG_SHA384);
}

static inline bool is_sha512(unsigned long flg)
{
	return (flg & FLG_SHA512);
}

static inline bool is_hmacsha1(unsigned long flg)
{
	return (flg & FLG_HMAC && (flg) & FLG_SHA1);
}

static inline bool is_hmacsha256(unsigned long flg)
{
	return (flg & FLG_HMAC && (flg) & FLG_SHA256);
}

static inline bool is_final(unsigned long flg)
{
	return (flg & FLG_FINAL);
}

static void aic_hash_task_cfg(struct task_desc *task,
			      struct aic_hash_reqctx *rctx, dma_addr_t din,
			      dma_addr_t iv_addr, dma_addr_t dout, u32 dlen,
			      u32 total, u32 last_flag)
{
	if (last_flag)
		task->data.total_bytelen = total;

	task->data.last_flag = last_flag;
	task->data.in_addr = cpu_to_le32(din);
	task->data.in_len = dlen;
	task->data.out_addr = cpu_to_le32(dout);
	task->alg.hash.iv_mode = 1;
	task->alg.hash.iv_addr = cpu_to_le32(iv_addr);

	if (is_md5(rctx->flags)) {
		task->alg.hash.alg_tag = ALG_TAG_MD5;
		task->data.out_len = MD5_CE_OUTPUT_LEN;
	} else if (is_sha1(rctx->flags)) {
		task->alg.hash.alg_tag = ALG_TAG_SHA1;
		task->data.out_len = SHA1_CE_OUTPUT_LEN;
	} else if (is_sha224(rctx->flags)) {
		task->alg.hash.alg_tag = ALG_TAG_SHA224;
		task->data.out_len = SHA224_CE_OUTPUT_LEN;
	} else if (is_sha256(rctx->flags)) {
		task->alg.hash.alg_tag = ALG_TAG_SHA256;
		task->data.out_len = SHA256_CE_OUTPUT_LEN;
	} else if (is_sha384(rctx->flags)) {
		task->alg.hash.alg_tag = ALG_TAG_SHA384;
		task->data.out_len = SHA384_CE_OUTPUT_LEN;
	} else if (is_sha512(rctx->flags)) {
		task->alg.hash.alg_tag = ALG_TAG_SHA512;
		task->data.out_len = SHA512_CE_OUTPUT_LEN;
	}
}

/*
 * Try best to reuse sg buffer as hardware buffer, if the last sg buffer is not
 * block aligned, just backup in remain buffer, and will be process in the next
 * request.
 */
static int prepare_task_with_src_sg_buf(struct aic_crypto_dev *ce,
					struct ahash_request *req)
{
	unsigned int bytelen, remain, todo, blocksz, cpycnt;
	dma_addr_t din, dout, next_addr;
	struct aic_hash_reqctx *rctx;
	struct aic_hash_tfm_ctx *ctx;
	struct scatterlist *sgs;
	struct crypto_ahash *tfm;
	struct task_desc *task;
	int ret, i, sg_cnt;
	u32 last_task, final;

	pr_debug("%s\n", __func__);
	rctx = ahash_request_ctx(req);
	tfm = crypto_ahash_reqtfm(req);
	ctx = crypto_ahash_ctx(tfm);

	blocksz = crypto_ahash_blocksize(tfm);
	sg_cnt = sg_nents_for_len(req->src, req->nbytes);

	if (is_final(rctx->flags))
		todo = req->nbytes;
	else
		todo = rounddown(req->nbytes, blocksz);

	cpycnt = req->nbytes - todo;
	if (cpycnt) {
		/* Backup not block aligned tail data in remain buffer, if this
		 * is not final step
		 */
		sg_copy_buffer(req->src, sg_cnt, ctx->remain_buf, cpycnt, todo,
			       true);
		ctx->remain_len = cpycnt;
	}
	if (0 == todo)
		return 0;

	ret = dma_map_sg(ce->dev, req->src, sg_cnt, DMA_TO_DEVICE);
	if (ret != sg_cnt) {
		dev_err(ce->dev, "Failed to dma map src sg\n");
		return -EFAULT;
	}
	rctx->src_map_sg = true;

	rctx->tasklen = sizeof(struct task_desc) * sg_cnt;
	rctx->task = dma_alloc_coherent(ce->dev, rctx->tasklen, &rctx->phy_task,
					GFP_KERNEL);
	if (!rctx->task)
		return -ENOMEM;

	memset(rctx->task, 0, rctx->tasklen);
	remain = todo;
	dout = rctx->phy_ivbuf;
	final = 0;
	for (i = 0, sgs = req->src; i < sg_cnt; i++, sgs = sg_next(sgs)) {
		task = &rctx->task[i];
		next_addr = rctx->phy_task + ((i + 1) * sizeof(*task));

		bytelen = min(remain, sg_dma_len(sgs));
		remain -= bytelen;
		ctx->total_len += bytelen;
		last_task = (remain == 0);

		din = sg_dma_address(sgs);
		aic_hash_task_cfg(task, rctx, din, dout, dout, bytelen,
				  ctx->total_len, final);
		if (last_task)
			task->next = 0;
		else
			task->next = cpu_to_le32(next_addr);
	}

	return 0;
}

/*
 * If source sg buffers cannot be used as hardware buffer, it is needed to
 * allocate a hardware buffer and copy input data to the new buffer
 */
static int prepare_task_with_src_cpy_buf(struct aic_crypto_dev *ce,
					 struct ahash_request *req)
{
	struct aic_hash_tfm_ctx *ctx;
	struct aic_hash_reqctx *rctx;
	struct crypto_ahash *tfm;
	unsigned int blocksz, total, todo;
	unsigned char *p;
	int pages;

	pr_debug("%s\n", __func__);
	tfm = crypto_ahash_reqtfm(req);
	ctx = crypto_ahash_ctx(tfm);
	blocksz = crypto_ahash_blocksize(tfm);

	rctx = ahash_request_ctx(req);
	total = ctx->remain_len + req->nbytes;
	todo = rounddown(total, blocksz);
	if (total < blocksz && is_final(rctx->flags) == false) {
		/* Not enough data to start CE, backup data in remain buffer */
		p = ctx->remain_buf;
		p += ctx->remain_len;
		aic_crypto_sg_copy(p, req->src, req->nbytes, 0);
		ctx->remain_len = total;

		return 0;
	}
	if (total > 0) {
		/* Final step or there is enough data to be processed */
		pages = get_order(total);
		rctx->src_cpy_buf = (void *)__get_free_pages(GFP_ATOMIC, pages);
		if (!rctx->src_cpy_buf) {
			dev_err(ce->dev, "Failed to allocate pages for src.\n");
			return -ENOMEM;
		}
		rctx->src_cpy_buf_len = total;
		p = rctx->src_cpy_buf;
		if (ctx->remain_len) {
			memcpy(p, ctx->remain_buf, ctx->remain_len);
			p += ctx->remain_len;
		}
		if (is_final(rctx->flags)) {
			/* If this is a final step, process all data */
			ctx->remain_len = 0;
			todo = total;
		} else {
			aic_crypto_sg_copy(p, req->src, req->nbytes, 0);
			/* If this is not final step, backup the tail data */
			ctx->remain_len = total % blocksz;
			if (ctx->remain_len) {
				p = rctx->src_cpy_buf;
				p += todo;
				memcpy(ctx->remain_buf, p, ctx->remain_len);
			}
		}
		rctx->src_phy_buf = dma_map_single(ce->dev, rctx->src_cpy_buf,
						   total, DMA_TO_DEVICE);
		if (dma_mapping_error(ce->dev, rctx->src_phy_buf)) {
			dev_err(ce->dev, "Failed to dma map src_phy_buf\n");
			return -EFAULT;
		}
	} else {
		rctx->src_cpy_buf = NULL;
		rctx->src_phy_buf = 0;
	}

	rctx->tasklen = sizeof(struct task_desc);
	rctx->task = dma_alloc_coherent(ce->dev, rctx->tasklen, &rctx->phy_task,
					GFP_KERNEL);
	if (!rctx->task)
		return -ENOMEM;
	memset(rctx->task, 0, rctx->tasklen);

	ctx->total_len += todo;
	aic_hash_task_cfg(rctx->task, rctx, rctx->src_phy_buf, rctx->phy_ivbuf,
			  rctx->phy_ivbuf, todo, ctx->total_len,
			  is_final(rctx->flags));
	if (is_final(rctx->flags)) {
		ctx->total_len = 0;
	}

	return 0;
}

static inline bool is_hash_block_aligned(unsigned int val, unsigned long flg)
{
	if (is_md5(flg)) {
		if (val % MD5_HMAC_BLOCK_SIZE)
			return false;
		return true;
	}
	if (is_sha1(flg)) {
		if (val % SHA1_BLOCK_SIZE)
			return false;
		return true;
	}
	if (is_sha224(flg)) {
		if (val % SHA224_BLOCK_SIZE)
			return false;
		return true;
	}
	if (is_sha256(flg)) {
		if (val % SHA256_BLOCK_SIZE)
			return false;
		return true;
	}
	if (is_sha384(flg)) {
		if (val % SHA384_BLOCK_SIZE)
			return false;
		return true;
	}
	if (is_sha512(flg)) {
		if (val % SHA512_BLOCK_SIZE)
			return false;
		return true;
	}

	return false;
}

static inline bool can_use_src_sg_buf(struct ahash_request *req)
{
	struct aic_hash_reqctx *rctx;
	unsigned int sg_cnt, dlen;
	struct scatterlist *sg;

	if (!req->src)
		return false;

	rctx = ahash_request_ctx(req);

	sg_cnt = sg_nents_for_len(req->src, req->nbytes);
	if (sg_cnt <= 0)
		return false;

	sg = req->src;
	if (sg_cnt == 1) {
		if (!is_word_aligned(sg->offset))
			return false;

		/* Only one sg buffer, but the data length is not block aligned,
		 * cannot be used directly
		 */
		dlen = sg->length - sg->offset;
		if (!is_hash_block_aligned(dlen, rctx->flags))
			return false;

		return true;
	}

	/* If there are more than 1 sg buffers, dont' care the last one, try
	 * the best to reuse sg buffer
	 */
	while (sg_cnt > 0) {
		if (!is_word_aligned(sg->offset))
			return false;
		dlen = sg->length - sg->offset;

		if (sg_cnt > 0 && !is_hash_block_aligned(dlen, rctx->flags))
			return false;
		sg = sg_next(sg);
		sg_cnt--;
	}
	return true;
}

static void get_output_digest(struct aic_crypto_dev *ce,
			      struct ahash_request *req)
{
	struct aic_hash_reqctx *rctx;

	rctx = ahash_request_ctx(req);
	if (rctx->phy_ivbuf) {
		dma_unmap_single(ce->dev, rctx->phy_ivbuf, CE_MAX_DIGEST_SIZE,
				 DMA_BIDIRECTIONAL);
		memcpy(rctx->digest, rctx->ivbuf, CE_MAX_DIGEST_SIZE);
		rctx->phy_ivbuf = 0;
	}

	if (is_final(rctx->flags) && req->result)
		memcpy(req->result, rctx->digest, rctx->digest_size);
}

static int aic_hash_unprepare_req(struct crypto_engine *engine, void *areq)
{
	struct aic_hash_reqctx *rctx;
	struct aic_hash_tfm_ctx *ctx;
	struct ahash_request *req;
	struct crypto_ahash *tfm;
	struct device *dev;
	int pages, sg_cnt;

	req = container_of(areq, struct ahash_request, base);
	rctx = ahash_request_ctx(req);
	tfm = crypto_ahash_reqtfm(req);
	ctx = crypto_ahash_ctx(tfm);
	dev = ctx->ce->dev;

	if (rctx->src_map_sg) {
		sg_cnt = sg_nents_for_len(req->src, req->nbytes);
		dma_unmap_sg(dev, req->src, sg_cnt, DMA_TO_DEVICE);
		rctx->src_map_sg = false;
	}

	if (rctx->task) {
		dma_free_coherent(dev, rctx->tasklen, rctx->task,
				  rctx->phy_task);
		rctx->task = NULL;
		rctx->phy_task = 0;
		rctx->tasklen = 0;
	}
	pages = get_order(rctx->src_cpy_buf_len);
	if (rctx->src_cpy_buf) {
		if (rctx->src_phy_buf) {
			dma_unmap_single(dev, rctx->src_phy_buf,
					 rctx->src_cpy_buf_len, DMA_TO_DEVICE);
			rctx->src_phy_buf = 0;
		}
		free_pages((unsigned long)rctx->src_cpy_buf, pages);
		rctx->src_cpy_buf = NULL;
	}

	if (rctx->phy_ivbuf) {
		dma_unmap_single(dev, rctx->phy_ivbuf, CE_MAX_DIGEST_SIZE,
				 DMA_BIDIRECTIONAL);
		rctx->phy_ivbuf = 0;
	}
	if (rctx->ivbuf) {
		kfree_sensitive(rctx->ivbuf);
		rctx->ivbuf = NULL;
	}

	return 0;
}

static int aic_hash_prepare_req(struct crypto_engine *engine, void *areq)
{
	struct aic_hash_tfm_ctx *ctx;
	struct aic_hash_reqctx *rctx;
	struct ahash_request *req;
	struct crypto_ahash *tfm;
	struct aic_crypto_dev *ce;
	unsigned int ds;
	int ret;

	req = container_of(areq, struct ahash_request, base);
	tfm = crypto_ahash_reqtfm(req);
	ctx = crypto_ahash_ctx(tfm);
	rctx = ahash_request_ctx(req);
	ce = ctx->ce;

	ds = CE_MAX_DIGEST_SIZE;
	rctx->ivbuf = kmemdup(rctx->digest, ds, GFP_KERNEL | GFP_DMA);
	if (!rctx->ivbuf) {
		ret = -ENOMEM;
		dev_err(ce->dev, "No mem for ivbuf\n");
		goto err;
	}
	rctx->phy_ivbuf = dma_map_single(ce->dev, rctx->ivbuf, ds,
					 DMA_BIDIRECTIONAL);
	if (dma_mapping_error(ce->dev, rctx->phy_ivbuf)) {
		dev_err(ce->dev, "Failed to dma map ivbuf\n");
		ret = -EFAULT;
		goto err;
	}

	if (ctx->remain_len == 0 && can_use_src_sg_buf(req))
		ret = prepare_task_with_src_sg_buf(ce, req);
	else
		ret = prepare_task_with_src_cpy_buf(ce, req);

	if (ret) {
		dev_err(ce->dev, "Failed to prepare task\n");
		goto err;
	}

	return 0;

err:
	aic_hash_unprepare_req(engine, areq);
	return ret;
}

static int aic_hash_do_one_req(struct crypto_engine *engine, void *areq)
{
	struct ahash_request *req;
	struct crypto_ahash *tfm;
	struct aic_hash_tfm_ctx *ctx;
	struct aic_hash_reqctx *rctx;
	struct aic_crypto_dev *ce;
	unsigned int ret;
	u32 algo;

	req = container_of(areq, struct ahash_request, base);
	tfm = crypto_ahash_reqtfm(req);
	ctx = crypto_ahash_ctx(tfm);
	rctx = ahash_request_ctx(req);
	ce = ctx->ce;

	if (!ce) {
		pr_err("Device is null.\n");
		return -ENODEV;
	}

	if (!rctx->task) {
		/* Not enough data to start CE, just finalize current request */
		crypto_finalize_hash_request(ce->hash_accel.engine, req, 0);
		return 0;
	}

	if (!aic_crypto_is_ce_avail(ce)) {
		dev_err(ce->dev, "Crypto engine is busy.\n");
		return -EBUSY;
	}

	if (!aic_crypto_is_accel_avail(ce, HASH_ALG_ACCELERATOR)) {
		dev_err(ce->dev, "hash accelerator fifo is full.\n");
		return -EBUSY;
	}

	mutex_lock(&ce->hash_accel.req_lock);
	ret = kfifo_put(&ce->hash_accel.req_fifo, areq);
	mutex_unlock(&ce->hash_accel.req_lock);
	if (ret != 1) {
		dev_err(ce->dev, "req fifo is full.\n");
		return ret;
	}

	if (DEBUG_CE)
		aic_crypto_dump_task(rctx->task, rctx->tasklen);

	algo = rctx->task->alg.alg_tag;
	aic_crypto_irq_enable(ce, HASH_ALG_ACCELERATOR);
	if (DEBUG_CE)
		aic_crypto_dump_reg(ce);
	aic_crypto_enqueue_task(ce, algo, rctx->phy_task);
	if (DEBUG_CE)
		aic_crypto_dump_reg(ce);
	return 0;
}

void aic_hash_handle_irq(struct aic_crypto_dev *ce)
{
	struct aic_hash_reqctx *rctx;
	struct ahash_request *req;
	void *areq;
	int err, ret;

	mutex_lock(&ce->hash_accel.req_lock);
	if (kfifo_len(&ce->hash_accel.req_fifo) < 1) {
		dev_err(ce->dev, "There is no req in fifo.\n");
		goto err;
	}

	/*
	 * TODO: Maybe more than one task is finished, should check here
	 */
	/* kfifo_peek(&ce->hash_accel.req_fifo, &areq); */
	ret = kfifo_get(&ce->hash_accel.req_fifo, &areq);
	if (ret != 1) {
		dev_err(ce->dev, "Get req from fifo failed.\n");
		goto err;
	}
	mutex_unlock(&ce->hash_accel.req_lock);

	req = container_of(areq, struct ahash_request, base);
	rctx = ahash_request_ctx(req);

	err = (ce->err_status >> 8 * HASH_ALG_ACCELERATOR) & 0xFF;
	if (!err)
		get_output_digest(ce, req);
	crypto_finalize_hash_request(ce->hash_accel.engine, req, err);

	return;

err:
	mutex_unlock(&ce->hash_accel.req_lock);
}

static inline struct ahash_alg *crypto_ahash_alg(struct crypto_ahash *hash)
{
	return container_of(crypto_hash_alg_common(hash), struct ahash_alg,
			    halg);
}

static int aic_hash_alg_init(struct crypto_tfm *tfm)
{
	struct aic_hash_tfm_ctx *ctx = crypto_tfm_ctx(tfm);
	struct crypto_ahash *hash = __crypto_ahash_cast(tfm);
	struct ahash_alg *halg = crypto_ahash_alg(hash);
	struct aic_hash_alg *aicalg;

	pr_debug("%s\n", __func__);
	crypto_ahash_set_reqsize(__crypto_ahash_cast(tfm),
				 sizeof(struct aic_hash_reqctx));

	memset(ctx, 0, sizeof(*ctx));
	aicalg = container_of(halg, struct aic_hash_alg, alg);
	ctx->ce = aicalg->ce;

#ifdef CONFIG_PM
	if (ctx->ce->task_count == 0)
		pm_runtime_get_sync(ctx->ce->dev);
	ctx->ce->task_count++;
#endif

	ctx->enginectx.op.do_one_request = aic_hash_do_one_req;
	ctx->enginectx.op.prepare_request = aic_hash_prepare_req;
	ctx->enginectx.op.unprepare_request = aic_hash_unprepare_req;

	ctx->remain_buf = kmalloc(crypto_ahash_blocksize(hash), GFP_KERNEL);
	if (!ctx->remain_buf)
		return -ENOMEM;

	return 0;
}

static int aic_hash_hmac_alg_init(struct crypto_tfm *tfm)
{
	struct aic_hash_tfm_ctx *ctx = crypto_tfm_ctx(tfm);
	int ret;

	ret = aic_hash_alg_init(tfm);
	ctx->hmac = true;

	return ret;
}

static void aic_hash_alg_exit(struct crypto_tfm *tfm)
{
	struct aic_hash_tfm_ctx *ctx = crypto_tfm_ctx(tfm);

#ifdef CONFIG_PM
	ctx->ce->task_count--;
	if (ctx->ce->task_count == 0) {
		pm_runtime_mark_last_busy(ctx->ce->dev);
		pm_runtime_put_autosuspend(ctx->ce->dev);
	}
#endif

	pr_debug("%s\n", __func__);
	kfree(ctx->remain_buf);
	memset(ctx, 0, sizeof(*ctx));
}

static u32 md5_iv[] = {
	MD5_H0,
	MD5_H1,
	MD5_H2,
	MD5_H3,
};

static u32 md5_iv_len = 16;

static u32 sha1_iv[] = {
	cpu_to_be32(SHA1_H0),
	cpu_to_be32(SHA1_H1),
	cpu_to_be32(SHA1_H2),
	cpu_to_be32(SHA1_H3),
	cpu_to_be32(SHA1_H4),
};

static u32 sha1_iv_len = 20;

static u32 sha224_iv[] = {
	cpu_to_be32(SHA224_H0),
	cpu_to_be32(SHA224_H1),
	cpu_to_be32(SHA224_H2),
	cpu_to_be32(SHA224_H3),
	cpu_to_be32(SHA224_H4),
	cpu_to_be32(SHA224_H5),
	cpu_to_be32(SHA224_H6),
	cpu_to_be32(SHA224_H7),
};

static u32 sha224_iv_len = 32;

static u32 sha256_iv[] = {
	cpu_to_be32(SHA256_H0),
	cpu_to_be32(SHA256_H1),
	cpu_to_be32(SHA256_H2),
	cpu_to_be32(SHA256_H3),
	cpu_to_be32(SHA256_H4),
	cpu_to_be32(SHA256_H5),
	cpu_to_be32(SHA256_H6),
	cpu_to_be32(SHA256_H7),
};

static u32 sha256_iv_len = 32;

static u64 sha384_iv[] = {
	cpu_to_be64(SHA384_H0),
	cpu_to_be64(SHA384_H1),
	cpu_to_be64(SHA384_H2),
	cpu_to_be64(SHA384_H3),
	cpu_to_be64(SHA384_H4),
	cpu_to_be64(SHA384_H5),
	cpu_to_be64(SHA384_H6),
	cpu_to_be64(SHA384_H7),
};

static u64 sha384_iv_len = 64;

static u64 sha512_iv[] = {
	cpu_to_be64(SHA512_H0),
	cpu_to_be64(SHA512_H1),
	cpu_to_be64(SHA512_H2),
	cpu_to_be64(SHA512_H3),
	cpu_to_be64(SHA512_H4),
	cpu_to_be64(SHA512_H5),
	cpu_to_be64(SHA512_H6),
	cpu_to_be64(SHA512_H7),
};

static u64 sha512_iv_len = 64;

static int aic_hash_init(struct ahash_request *req)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct aic_hash_tfm_ctx *ctx = crypto_ahash_ctx(tfm);
	struct aic_hash_reqctx *rctx = ahash_request_ctx(req);

	pr_debug("%s\n", __func__);
	memset(rctx, 0, sizeof(*rctx));

	rctx->flags |= FLG_FIRST;
	if (ctx->hmac)
		rctx->flags |= FLG_HMAC;
	rctx->digest_size = crypto_ahash_digestsize(tfm);

	switch (rctx->digest_size) {
	case MD5_DIGEST_SIZE:
		rctx->flags |= FLG_MD5;
		memcpy(rctx->digest, md5_iv, md5_iv_len);
		break;
	case SHA1_DIGEST_SIZE:
		rctx->flags |= FLG_SHA1;
		memcpy(rctx->digest, sha1_iv, sha1_iv_len);
		break;
	case SHA224_DIGEST_SIZE:
		rctx->flags |= FLG_SHA224;
		memcpy(rctx->digest, sha224_iv, sha224_iv_len);
		break;
	case SHA256_DIGEST_SIZE:
		rctx->flags |= FLG_SHA256;
		memcpy(rctx->digest, sha256_iv, sha256_iv_len);
		break;
	case SHA384_DIGEST_SIZE:
		rctx->flags |= FLG_SHA384;
		memcpy(rctx->digest, sha384_iv, sha384_iv_len);
		break;
	case SHA512_DIGEST_SIZE:
		rctx->flags |= FLG_SHA512;
		memcpy(rctx->digest, sha512_iv, sha512_iv_len);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int aic_hash_export(struct ahash_request *req, void *out)
{
	/*
	 * TODO: Export rctx and hardware context to user
	 */
	return 0;
}

static int aic_hash_import(struct ahash_request *req, const void *in)
{
	/*
	 * TODO: Import rctx and hardware context from user
	 */
	return 0;
}

static int aic_hash_enqueue(struct ahash_request *req, unsigned long flg)
{
	struct aic_hash_tfm_ctx *ctx;
	struct aic_hash_reqctx *rctx;
	struct crypto_ahash *tfm;
	struct crypto_engine *eng;

	tfm = crypto_ahash_reqtfm(req);
	ctx = crypto_ahash_ctx(tfm);
	rctx = ahash_request_ctx(req);

	rctx->flags |= flg;

	eng = ctx->ce->hash_accel.engine;
	return crypto_transfer_hash_request_to_engine(eng, req);
}

/*
 * one data request is incoming, need to transfer to queue
 */
static int aic_hash_update(struct ahash_request *req)
{
	return aic_hash_enqueue(req, FLG_UPDATE);
}

/*
 * These is no data, just want to get the result.
 */
static int aic_hash_final(struct ahash_request *req)
{
	pr_debug("%s\n", __func__);
	return aic_hash_enqueue(req, FLG_FINAL);
}

/*
 * update and final
 */
static int aic_hash_finup(struct ahash_request *req)
{
	pr_debug("%s\n", __func__);
	return aic_hash_enqueue(req, FLG_UPDATE | FLG_FINAL);
}

/*
 * init, and finup
 */
static int aic_hash_digest(struct ahash_request *req)
{
	return 0;
}

static int aic_hash_setkey(struct crypto_ahash *tfm, const u8 *key,
			   unsigned int keylen)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static struct aic_hash_alg hash_algs[] = {
{
	.alg = {
		.init = aic_hash_init,
		.update = aic_hash_update,
		.final = aic_hash_final,
		.finup = aic_hash_finup,
		.digest = aic_hash_digest,
		.export = aic_hash_export,
		.import = aic_hash_import,
		.halg = {
			.digestsize = MD5_DIGEST_SIZE,
			.statesize = sizeof(struct aic_hash_reqctx),
			.base = {
				.cra_name = "md5",
				.cra_driver_name = "md5-aic",
				.cra_priority = 400,
				.cra_flags = CRYPTO_ALG_ASYNC |
						CRYPTO_ALG_TYPE_AHASH,
				.cra_blocksize = MD5_HMAC_BLOCK_SIZE,
				.cra_ctxsize = sizeof(struct aic_hash_tfm_ctx),
				.cra_alignmask = 3,
				.cra_init = aic_hash_alg_init,
				.cra_exit = aic_hash_alg_exit,
				.cra_module = THIS_MODULE,
			}
		}
	}
},
{
	.alg = {
		.init = aic_hash_init,
		.update = aic_hash_update,
		.final = aic_hash_final,
		.finup = aic_hash_finup,
		.digest = aic_hash_digest,
		.export = aic_hash_export,
		.import = aic_hash_import,
		.halg = {
			.digestsize = SHA1_DIGEST_SIZE,
			.statesize = sizeof(struct aic_hash_reqctx),
			.base = {
				.cra_name = "sha1",
				.cra_driver_name = "sha1-aic",
				.cra_priority = 400,
				.cra_flags = CRYPTO_ALG_ASYNC |
						CRYPTO_ALG_TYPE_AHASH,
				.cra_blocksize = SHA1_BLOCK_SIZE,
				.cra_ctxsize = sizeof(struct aic_hash_tfm_ctx),
				.cra_alignmask = 3,
				.cra_init = aic_hash_alg_init,
				.cra_exit = aic_hash_alg_exit,
				.cra_module = THIS_MODULE,
			}
		}
	}
},
{
	.alg = {
		.init = aic_hash_init,
		.update = aic_hash_update,
		.final = aic_hash_final,
		.finup = aic_hash_finup,
		.digest = aic_hash_digest,
		.export = aic_hash_export,
		.import = aic_hash_import,
		.halg = {
			.digestsize = SHA224_DIGEST_SIZE,
			.statesize = sizeof(struct aic_hash_reqctx),
			.base = {
				.cra_name = "sha224",
				.cra_driver_name = "sha224-aic",
				.cra_priority = 400,
				.cra_flags = CRYPTO_ALG_ASYNC |
						CRYPTO_ALG_TYPE_AHASH,
				.cra_blocksize = SHA224_BLOCK_SIZE,
				.cra_ctxsize = sizeof(struct aic_hash_tfm_ctx),
				.cra_alignmask = 3,
				.cra_init = aic_hash_alg_init,
				.cra_exit = aic_hash_alg_exit,
				.cra_module = THIS_MODULE,
			}
		}
	}
},
{
	.alg = {
		.init = aic_hash_init,
		.update = aic_hash_update,
		.final = aic_hash_final,
		.finup = aic_hash_finup,
		.digest = aic_hash_digest,
		.export = aic_hash_export,
		.import = aic_hash_import,
		.halg = {
			.digestsize = SHA256_DIGEST_SIZE,
			.statesize = sizeof(struct aic_hash_reqctx),
			.base = {
				.cra_name = "sha256",
				.cra_driver_name = "sha256-aic",
				.cra_priority = 400,
				.cra_flags = CRYPTO_ALG_ASYNC |
						CRYPTO_ALG_TYPE_AHASH,
				.cra_blocksize = SHA256_BLOCK_SIZE,
				.cra_ctxsize = sizeof(struct aic_hash_tfm_ctx),
				.cra_alignmask = 3,
				.cra_init = aic_hash_alg_init,
				.cra_exit = aic_hash_alg_exit,
				.cra_module = THIS_MODULE,
			}
		}
	}
},
{
	.alg = {
		.init = aic_hash_init,
		.update = aic_hash_update,
		.final = aic_hash_final,
		.finup = aic_hash_finup,
		.digest = aic_hash_digest,
		.export = aic_hash_export,
		.import = aic_hash_import,
		.halg = {
			.digestsize = SHA384_DIGEST_SIZE,
			.statesize = sizeof(struct aic_hash_reqctx),
			.base = {
				.cra_name = "sha384",
				.cra_driver_name = "sha384-aic",
				.cra_priority = 400,
				.cra_flags = CRYPTO_ALG_ASYNC |
						CRYPTO_ALG_TYPE_AHASH,
				.cra_blocksize = SHA384_BLOCK_SIZE,
				.cra_ctxsize = sizeof(struct aic_hash_tfm_ctx),
				.cra_alignmask = 3,
				.cra_init = aic_hash_alg_init,
				.cra_exit = aic_hash_alg_exit,
				.cra_module = THIS_MODULE,
			}
		}
	}
},
{
	.alg = {
		.init = aic_hash_init,
		.update = aic_hash_update,
		.final = aic_hash_final,
		.finup = aic_hash_finup,
		.digest = aic_hash_digest,
		.export = aic_hash_export,
		.import = aic_hash_import,
		.halg = {
			.digestsize = SHA512_DIGEST_SIZE,
			.statesize = sizeof(struct aic_hash_reqctx),
			.base = {
				.cra_name = "sha512",
				.cra_driver_name = "sha512-aic",
				.cra_priority = 400,
				.cra_flags = CRYPTO_ALG_ASYNC |
						CRYPTO_ALG_TYPE_AHASH,
				.cra_blocksize = SHA512_BLOCK_SIZE,
				.cra_ctxsize = sizeof(struct aic_hash_tfm_ctx),
				.cra_alignmask = 3,
				.cra_init = aic_hash_alg_init,
				.cra_exit = aic_hash_alg_exit,
				.cra_module = THIS_MODULE,
			}
		}
	}
},
{
	.alg = {
		.init = aic_hash_init,
		.update = aic_hash_update,
		.final = aic_hash_final,
		.finup = aic_hash_finup,
		.digest = aic_hash_digest,
		.export = aic_hash_export,
		.import = aic_hash_import,
		.setkey = aic_hash_setkey,
		.halg = {
			.digestsize = SHA1_DIGEST_SIZE,
			.statesize = sizeof(struct aic_hash_reqctx),
			.base = {
				.cra_name = "hmac(sha1)",
				.cra_driver_name = "hmac-sha1-aic",
				.cra_priority = 400,
				.cra_flags = CRYPTO_ALG_ASYNC |
						CRYPTO_ALG_TYPE_AHASH,
				.cra_blocksize = SHA1_BLOCK_SIZE,
				.cra_ctxsize = sizeof(struct aic_hash_tfm_ctx),
				.cra_alignmask = 3,
				.cra_init = aic_hash_hmac_alg_init,
				.cra_exit = aic_hash_alg_exit,
				.cra_module = THIS_MODULE,
			}
		}
	}
},
{
	.alg = {
		.init = aic_hash_init,
		.update = aic_hash_update,
		.final = aic_hash_final,
		.finup = aic_hash_finup,
		.digest = aic_hash_digest,
		.export = aic_hash_export,
		.import = aic_hash_import,
		.setkey = aic_hash_setkey,
		.halg = {
			.digestsize = SHA256_DIGEST_SIZE,
			.statesize = sizeof(struct aic_hash_reqctx),
			.base = {
				.cra_name = "hmac(sha256)",
				.cra_driver_name = "hmac-sha256-aic",
				.cra_priority = 400,
				.cra_flags = CRYPTO_ALG_ASYNC |
						CRYPTO_ALG_TYPE_AHASH,
				.cra_blocksize = SHA256_BLOCK_SIZE,
				.cra_ctxsize = sizeof(struct aic_hash_tfm_ctx),
				.cra_alignmask = 3,
				.cra_init = aic_hash_hmac_alg_init,
				.cra_exit = aic_hash_alg_exit,
				.cra_module = THIS_MODULE,
			}
		}
	}
},
};

int aic_crypto_hash_accelerator_init(struct aic_crypto_dev *ce)
{
	struct crypto_engine *eng;
	int ret, i;

	eng = crypto_engine_alloc_init_and_set(ce->dev, true, NULL, true,
					       ACCEL_QUEUE_MAX_SIZE);
	if (!eng) {
		dev_err(ce->dev, "Failed to init crypto engine for hash\n");
		ret = -ENOMEM;
		return ret;
	}

	ce->hash_accel.engine = eng;
	ret = crypto_engine_start(ce->hash_accel.engine);
	if (ret) {
		dev_err(ce->dev, "Failed to start crypto engine for hash\n");
		goto err;
	}

	mutex_init(&ce->hash_accel.req_lock);
	INIT_KFIFO(ce->hash_accel.req_fifo);
	ret = kfifo_alloc(&ce->hash_accel.req_fifo, ACCEL_QUEUE_MAX_SIZE,
			  GFP_KERNEL);
	if (ret) {
		dev_err(ce->dev, "Failed to alloc kfifo for hash\n");
		goto err2;
	}

	for (i = 0; i < ARRAY_SIZE(hash_algs); i++) {
		hash_algs[i].ce = ce;
		ret = crypto_register_ahash(&hash_algs[i].alg);
		if (ret) {
			dev_err(ce->dev, "Failed to register hash algs\n");
			goto err3;
		}
	}

	return 0;

err3:
	for (--i; i >= 0; --i) {
		hash_algs[i].ce = NULL;
		crypto_unregister_ahash(&hash_algs[i].alg);
	}
err2:
	kfifo_free(&ce->hash_accel.req_fifo);
err:
	crypto_engine_exit(ce->hash_accel.engine);
	return ret;
}

int aic_crypto_hash_accelerator_exit(struct aic_crypto_dev *ce)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(hash_algs); i++) {
		hash_algs[i].ce = NULL;
		crypto_unregister_ahash(&hash_algs[i].alg);
	}
	kfifo_free(&ce->hash_accel.req_fifo);
	crypto_engine_exit(ce->hash_accel.engine);

	return 0;
}
