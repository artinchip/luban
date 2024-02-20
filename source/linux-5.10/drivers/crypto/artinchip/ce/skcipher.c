// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, Artinchip Technology Co., Ltd
 *
 * Wu Dehuang <dehuang.wu@artinchip.com>
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/kfifo.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>
#include <crypto/des.h>
#include <crypto/aes.h>
#include <crypto/internal/des.h>
#include <crypto/engine.h>
#include <crypto/scatterwalk.h>
#include <crypto/internal/skcipher.h>
#include "crypto.h"
#include "ssram.h"

#define CE_CIPHER_MAX_KEY_SIZE	64
#define CE_CIPHER_MAX_IV_SIZE   AES_BLOCK_SIZE
#define FLG_DEC		BIT(0)
#define FLG_AES		BIT(1)
#define FLG_DES		BIT(2)
#define FLG_TDES	BIT(3)
#define FLG_ECB		BIT(4)
#define FLG_CBC		BIT(5)
#define FLG_CTR		BIT(6)
#define FLG_CTS		BIT(7)
#define FLG_XTS		BIT(8)
#define FLG_GENHSK	BIT(9)  /* Gen HSK(Hardware Secure Key) to SSRAM */

#define FLG_KEYOFFSET   (16)
#define FLG_KEYMASK	GENMASK(19, 16) /* eFuse Key ID */

struct aic_skcipher_alg {
	struct skcipher_alg alg;
	struct aic_crypto_dev *ce;
};

struct aic_skcipher_tfm_ctx {
	struct crypto_engine_ctx enginectx;
	unsigned char *inkey;
	int inkeylen;
	struct aic_crypto_dev *ce;
};

struct aic_skcipher_reqctx {
	struct task_desc *task;
	dma_addr_t phy_task;
	unsigned char *key;
	dma_addr_t phy_key;
	unsigned char *iv;
	unsigned char *backup_ivs; /* Back up iv for CBC decrypt */
	dma_addr_t phy_iv;
	dma_addr_t ssram_addr;
	dma_addr_t backup_phy_ivs;
	dma_addr_t next_iv; /* Next IV address for CBC encrypt */
	int tasklen;
	int keylen;
	int ivsize;
	int blocksize;
	int backup_iv_cnt;
	unsigned long flags;
	void *src_cpy_buf;
	void *dst_cpy_buf;
	dma_addr_t src_phy_buf;
	dma_addr_t dst_phy_buf;
	bool src_map_sg;
	bool dst_map_sg;
};

static inline bool is_aes_ecb(unsigned long flg)
{
	return (flg & FLG_AES && flg & FLG_ECB);
}

static inline bool is_aes_cbc(unsigned long flg)
{
	return (flg & FLG_AES && flg & FLG_CBC);
}

static inline bool is_aes_ctr(unsigned long flg)
{
	return (flg & FLG_AES && flg & FLG_CTR);
}

static inline bool is_aes_cts(unsigned long flg)
{
	return (flg & FLG_AES && flg & FLG_CTS);
}

static inline bool is_aes_xts(unsigned long flg)
{
	return (flg & FLG_AES && flg & FLG_XTS);
}

static inline bool is_des_ecb(unsigned long flg)
{
	return (flg & FLG_DES && flg & FLG_ECB);
}

static inline bool is_des_cbc(unsigned long flg)
{
	return (flg & FLG_DES && flg & FLG_CBC);
}

static inline bool is_tdes_ecb(unsigned long flg)
{
	return (flg & FLG_TDES && flg & FLG_ECB);
}

static inline bool is_tdes_cbc(unsigned long flg)
{
	return (flg & FLG_TDES && flg & FLG_CBC);
}

static inline bool is_cbc_dec(unsigned long flg)
{
	return (flg & FLG_DEC && flg & FLG_CBC);
}

static inline bool is_cbc_enc(unsigned long flg)
{
	return ((~flg) & FLG_DEC && flg & FLG_CBC);
}

static inline bool is_cbc(unsigned long flg)
{
	return (flg & FLG_CBC);
}

static inline bool is_ctr(unsigned long flg)
{
	return (flg & FLG_CTR);
}

static inline bool is_dec(unsigned long flg)
{
	return (flg & FLG_DEC);
}

static inline bool is_enc(unsigned long flg)
{
	return ((~flg) & FLG_DEC);
}

static inline bool is_need_genhsk(unsigned long flg)
{
	return (flg & (FLG_KEYMASK));
}

static inline bool is_gen_hsk(unsigned long flg)
{
	return (flg & FLG_GENHSK);
}

static u32 aic_skcipher_keysize(int keylen)
{
	u32 ksize;

	switch (keylen) {
	case 8:
		ksize = KEY_SIZE_64;
		break;
	case 16:
		ksize = KEY_SIZE_128;
		break;
	case 24:
		ksize = KEY_SIZE_192;
		break;
	case 32:
		ksize = KEY_SIZE_256;
		break;
	}

	return ksize;
}

static void aic_skcipher_task_cfg(struct task_desc *task,
				  struct aic_skcipher_reqctx *rctx,
				  dma_addr_t din, dma_addr_t dout, u32 dlen,
				  u32 first_flag, u32 last_flag)
{
	u32 opdir, keysize, keysrc;

	if (is_aes_xts(rctx->flags))
		keysize = aic_skcipher_keysize(rctx->keylen / 2);
	else
		keysize = aic_skcipher_keysize(rctx->keylen);
	opdir = rctx->flags & FLG_DEC;

	task->data.in_addr = cpu_to_le32(din);
	task->data.in_len = cpu_to_le32(dlen);
	task->data.out_addr = cpu_to_le32(dout);
	task->data.out_len = cpu_to_le32(dlen);

	if (is_gen_hsk(rctx->flags))
		keysrc = ((rctx->flags) & FLG_KEYMASK) >> FLG_KEYOFFSET;
	else
		keysrc = CE_KEY_SRC_USER;

	if (is_aes_ecb(rctx->flags)) {
		task->alg.aes_ecb.alg_tag = ALG_TAG_AES_ECB;
		task->alg.aes_ecb.direction = opdir;
		task->alg.aes_ecb.key_siz = keysize;
		task->alg.aes_ecb.key_src = keysrc;
		if (rctx->ssram_addr)
			task->alg.aes_ecb.key_addr = rctx->ssram_addr;
		else
			task->alg.aes_ecb.key_addr = rctx->phy_key;
	} else if (is_aes_cbc(rctx->flags)) {
		task->alg.aes_cbc.alg_tag = ALG_TAG_AES_CBC;
		task->alg.aes_cbc.direction = opdir;
		task->alg.aes_cbc.key_siz = keysize;
		task->alg.aes_cbc.key_src = keysrc;
		if (rctx->ssram_addr)
			task->alg.aes_cbc.key_addr = rctx->ssram_addr;
		else
			task->alg.aes_cbc.key_addr = rctx->phy_key;
		if (rctx->next_iv)
			task->alg.aes_cbc.iv_addr = rctx->next_iv;
		else
			task->alg.aes_cbc.iv_addr = rctx->phy_iv;
	} else if (is_aes_ctr(rctx->flags)) {
		task->alg.aes_ctr.alg_tag = ALG_TAG_AES_CTR;
		task->alg.aes_ctr.direction = opdir;
		task->alg.aes_ctr.key_siz = keysize;
		task->alg.aes_ctr.key_src = keysrc;
		task->alg.aes_ctr.key_addr = rctx->phy_key;
		task->alg.aes_ctr.ctr_in_addr = rctx->phy_iv;
		task->alg.aes_ctr.ctr_out_addr = rctx->phy_iv;
	} else if (is_aes_cts(rctx->flags)) {
		task->alg.aes_cts.alg_tag = ALG_TAG_AES_CTS;
		task->alg.aes_cts.direction = opdir;
		task->alg.aes_cts.key_siz = keysize;
		task->alg.aes_cts.key_src = keysrc;
		if (rctx->ssram_addr)
			task->alg.aes_cts.key_addr = rctx->ssram_addr;
		else
			task->alg.aes_cts.key_addr = rctx->phy_key;
		if (rctx->next_iv)
			task->alg.aes_cts.iv_addr = rctx->next_iv;
		else
			task->alg.aes_cts.iv_addr = rctx->phy_iv;
		task->data.last_flag = last_flag;
	} else if (is_aes_xts(rctx->flags)) {
		task->alg.aes_xts.alg_tag = ALG_TAG_AES_XTS;
		task->alg.aes_xts.direction = opdir;
		task->alg.aes_xts.key_siz = keysize;
		task->alg.aes_xts.key_src = keysrc;
		if (rctx->ssram_addr)
			task->alg.aes_xts.key_addr = rctx->ssram_addr;
		else
			task->alg.aes_xts.key_addr = rctx->phy_key;
		task->alg.aes_xts.tweak_addr = rctx->phy_iv;
		task->data.first_flag = first_flag;
		task->data.last_flag = last_flag;
	} else if (is_des_ecb(rctx->flags)) {
		task->alg.des_ecb.alg_tag = ALG_TAG_DES_ECB;
		task->alg.des_ecb.direction = opdir;
		task->alg.des_ecb.key_siz = keysize;
		task->alg.des_ecb.key_src = keysrc;
		task->alg.des_ecb.key_addr = rctx->phy_key;
	} else if (is_des_cbc(rctx->flags)) {
		task->alg.des_cbc.alg_tag = ALG_TAG_DES_CBC;
		task->alg.des_cbc.direction = opdir;
		task->alg.des_cbc.key_siz = keysize;
		task->alg.des_cbc.key_src = keysrc;
		task->alg.des_cbc.key_addr = rctx->phy_key;
		if (rctx->next_iv)
			task->alg.des_cbc.iv_addr = rctx->next_iv;
		else
			task->alg.des_cbc.iv_addr = rctx->phy_iv;
	} else if (is_tdes_ecb(rctx->flags)) {
		task->alg.des_ecb.alg_tag = ALG_TAG_TDES_ECB;
		task->alg.des_ecb.direction = opdir;
		task->alg.des_ecb.key_siz = keysize;
		task->alg.des_ecb.key_src = keysrc;
		task->alg.des_ecb.key_addr = rctx->phy_key;
	} else if (is_tdes_cbc(rctx->flags)) {
		task->alg.des_cbc.alg_tag = ALG_TAG_TDES_CBC;
		task->alg.des_cbc.direction = opdir;
		task->alg.des_cbc.key_siz = keysize;
		task->alg.des_cbc.key_src = keysrc;
		task->alg.des_cbc.key_addr = rctx->phy_key;
		if (rctx->next_iv)
			task->alg.des_cbc.iv_addr = rctx->next_iv;
		else
			task->alg.des_cbc.iv_addr = rctx->phy_iv;
	}
}

static inline bool is_sk_block_aligned(unsigned int val, unsigned long flg)
{
	if (flg & FLG_AES) {
		if (val % AES_BLOCK_SIZE)
			return false;
		return true;
	}

	if (flg & FLG_DES) {
		if (val % DES_BLOCK_SIZE)
			return false;
		return true;
	}

	return false;
}

static inline bool aic_skcipher_sg_match(struct scatterlist *src,
					 struct scatterlist *dst,
					 unsigned int cryptlen)
{
	unsigned int sgs_cnt, sgd_cnt;
	struct scatterlist *sgs, *sgd;

	sgs_cnt = sg_nents_for_len(src, cryptlen);
	sgd_cnt = sg_nents_for_len(dst, cryptlen);

	if (sgs_cnt != sgd_cnt)
		return false;

	sgs = src;
	sgd = dst;
	while (sgs_cnt > 0) {
		if (sgs->offset != sgd->offset)
			return false;
		if (sgs->length != sgd->length)
			return false;
		sgs = sg_next(sgs);
		sgd = sg_next(sgd);
		sgs_cnt--;
	}

	return true;
}

static inline bool is_need_copy_dst(struct skcipher_request *req)
{
	struct aic_skcipher_reqctx *rctx;
	bool match;

	if (!is_word_aligned(req->dst->offset)) {
		pr_debug("%s, offset(%d) is not aligned.\n", __func__,
			 req->dst->offset);
		return true;
	}

	rctx = skcipher_request_ctx(req);

	/*
	 * Even it is ctr mode, if data length is not block size alignment,
	 * still need to use dst copy buffer.
	 */
	if (!is_sk_block_aligned(req->cryptlen, rctx->flags)) {
		pr_debug("%s, cryptlen(%d) is not aligned.\n", __func__,
			 req->cryptlen);
		return true;
	}

	if (!is_sk_block_aligned(req->dst->length, rctx->flags)) {
		pr_debug("%s, length(%d) is not aligned.\n", __func__,
			 req->dst->length);
		return true;
	}

	match = aic_skcipher_sg_match(req->src, req->dst, req->cryptlen);
	if (!match) {
		pr_debug("%s, src and dst is not match.\n", __func__);
		return true;
	}

	return false;
}

static inline bool is_need_copy_src(struct skcipher_request *req)
{
	struct aic_skcipher_reqctx *rctx;

	if (!is_word_aligned(req->src->offset)) {
		pr_debug("%s, offset(%d) is not aligned.\n", __func__,
			 req->src->offset);
		return true;
	}

	rctx = skcipher_request_ctx(req);
	if (!is_sk_block_aligned(req->src->length, rctx->flags)) {
		pr_debug("%s, length(%d) is not aligned.\n", __func__,
			 req->src->length);
		return true;
	}

	return false;
}

/*
 * Try best to avoid data copy
 */
static int aic_skcipher_prepare_data_buf(struct aic_crypto_dev *ce,
					 struct skcipher_request *req)
{
	struct aic_skcipher_reqctx *rctx;
	bool cpydst, cpysrc;
	int pages;

	rctx = skcipher_request_ctx(req);

	pages = get_order(ALIGN(req->cryptlen, rctx->blocksize));

	cpysrc = is_need_copy_src(req);
	if (cpysrc) {
		/* Allocate physical continuous pages */
		rctx->src_cpy_buf = (void *)__get_free_pages(GFP_ATOMIC, pages);
		if (!rctx->src_cpy_buf) {
			dev_err(ce->dev, "Failed to allocate pages for src.\n");
			return -ENOMEM;
		}

		aic_crypto_sg_copy(rctx->src_cpy_buf, req->src, req->cryptlen,
				   0);
	}

	cpydst = is_need_copy_dst(req);
	if (cpydst) {
		rctx->dst_cpy_buf = (void *)__get_free_pages(GFP_ATOMIC, pages);
		if (!rctx->dst_cpy_buf) {
			dev_err(ce->dev, "Failed to allocate pages for dst.\n");
			return -ENOMEM;
		}
	}

	return 0;
}

/*
 * Driver try to avoid data copy, but if the input buffer is not friendly
 * to hardware, it is need to allocate physical continuous pages as working
 * buffer.
 *
 * This case is both input and output working buffer are new allocated
 * physical continuous pages.
 */
static int prepare_task_with_both_cpy_buf(struct aic_crypto_dev *ce,
					  struct skcipher_request *req)
{
	struct aic_skcipher_reqctx *rctx;
	u32 bytelen;

	pr_debug("%s\n", __func__);
	rctx = skcipher_request_ctx(req);
	rctx->tasklen = sizeof(struct task_desc);
	rctx->task = dma_alloc_coherent(ce->dev, rctx->tasklen, &rctx->phy_task,
					GFP_KERNEL);
	if (!rctx->task)
		return -ENOMEM;

	memset(rctx->task, 0, rctx->tasklen);

	rctx->src_phy_buf = dma_map_single(ce->dev, rctx->src_cpy_buf,
					   req->cryptlen, DMA_TO_DEVICE);
	if (dma_mapping_error(ce->dev, rctx->src_phy_buf)) {
		dev_err(ce->dev, "Failed to dma map src_phy_buf\n");
		return -EFAULT;
	}
	rctx->dst_phy_buf = dma_map_single(ce->dev, rctx->dst_cpy_buf,
					   req->cryptlen, DMA_FROM_DEVICE);
	if (dma_mapping_error(ce->dev, rctx->dst_phy_buf)) {
		dev_err(ce->dev, "Failed to dma map dst_phy_buf\n");
		return -EFAULT;
	}

	rctx->next_iv = 0;
	bytelen = ALIGN(req->cryptlen, rctx->blocksize);
	aic_skcipher_task_cfg(rctx->task, rctx, rctx->src_phy_buf,
			      rctx->dst_phy_buf, bytelen, 1, 1);
	return 0;
}

static int backup_iv_for_cbc_decrypt(struct aic_crypto_dev *ce,
				     struct skcipher_request *req)
{
	struct aic_skcipher_reqctx *rctx;
	struct scatterlist *sg;
	int i, iv_cnt, ivbufsiz;
	u8 *ps, *pd;

	rctx = skcipher_request_ctx(req);

	rctx->backup_ivs = NULL;
	rctx->backup_phy_ivs = 0;

	if (rctx->src_cpy_buf && rctx->dst_cpy_buf) {
		/*
		 * Both src sg & dst sg are not hardware friendly, all of them
		 * are copied to physical continuous pages, only need to backup
		 * the last ciphertext block as next iv
		 */
		iv_cnt = 1;
		ivbufsiz = rctx->ivsize * iv_cnt;
		rctx->backup_ivs = dma_alloc_coherent(ce->dev, ivbufsiz,
						      &rctx->backup_phy_ivs,
						      GFP_KERNEL);
		if (!rctx->backup_ivs)
			return -ENOMEM;

		rctx->backup_iv_cnt = iv_cnt;
		ps = ((u8 *)rctx->src_cpy_buf) + req->cryptlen - rctx->ivsize;
		pd = rctx->backup_ivs;
		memcpy(pd, ps, rctx->ivsize);
	} else if (rctx->src_cpy_buf) {
		/*
		 * src sg is not hardware friendly, but dst sg is,
		 * backup iv according dst sg
		 */
		iv_cnt = sg_nents_for_len(req->dst, req->cryptlen);
		ivbufsiz = rctx->ivsize * iv_cnt;
		rctx->backup_ivs = dma_alloc_coherent(ce->dev, ivbufsiz,
						      &rctx->backup_phy_ivs,
						      GFP_KERNEL);
		if (!rctx->backup_ivs)
			return -ENOMEM;

		rctx->backup_iv_cnt = iv_cnt;
		for (i = 0, sg = req->dst; i < iv_cnt; i++, sg = sg_next(sg)) {
			ps = (u8 *)sg_virt(sg) + sg_dma_len(sg) - rctx->ivsize;
			pd = rctx->backup_ivs + i * rctx->ivsize;
			memcpy(pd, ps, rctx->ivsize);
		}
	} else {
		/*
		 * src sg is hardware friendly,
		 * backup iv according src sg
		 */
		iv_cnt = sg_nents_for_len(req->src, req->cryptlen);
		ivbufsiz = rctx->ivsize * iv_cnt;
		rctx->backup_ivs = dma_alloc_coherent(ce->dev, ivbufsiz,
						      &rctx->backup_phy_ivs,
						      GFP_KERNEL);
		if (!rctx->backup_ivs)
			return -ENOMEM;

		rctx->backup_iv_cnt = iv_cnt;
		for (i = 0, sg = req->src; i < iv_cnt; i++, sg = sg_next(sg)) {
			ps = (u8 *)sg_virt(sg) + sg_dma_len(sg) - rctx->ivsize;
			pd = rctx->backup_ivs + i * rctx->ivsize;
			memcpy(pd, ps, rctx->ivsize);
		}
	}

	return 0;
}

/*
 * In this case, dst sg list is not hardware friendly, but src is.
 * ce output data is saved to dst_cpy_buf firstly, then copied to dst sg list.
 */
static int prepare_task_with_src_sg_dst_cpy_buf(struct aic_crypto_dev *ce,
						struct skcipher_request *req)
{
	dma_addr_t din, dout, next_addr;
	struct aic_skcipher_reqctx *rctx;
	struct scatterlist *sgs;
	u32 first_flag, last_flag;
	struct task_desc *task;
	unsigned int bytelen, remain;
	int ret, i, sg_cnt;

	pr_debug("%s\n", __func__);
	rctx = skcipher_request_ctx(req);

	rctx->dst_phy_buf = dma_map_single(ce->dev, rctx->dst_cpy_buf,
					   req->cryptlen, DMA_FROM_DEVICE);
	if (dma_mapping_error(ce->dev, rctx->dst_phy_buf)) {
		dev_err(ce->dev, "Failed to dma map dst_phy_buf\n");
		return -EFAULT;
	}

	sg_cnt = sg_nents_for_len(req->src, req->cryptlen);
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

	rctx->next_iv = 0;
	remain = req->cryptlen;
	dout = rctx->dst_phy_buf;
	for (i = 0, sgs = req->src; i < sg_cnt; i++, sgs = sg_next(sgs)) {
		task = &rctx->task[i];
		next_addr = rctx->phy_task + ((i + 1) * sizeof(*task));

		bytelen = min(remain, sg_dma_len(sgs));
		first_flag = (i == 0);
		last_flag = ((i + 1) == sg_cnt);
		din = sg_dma_address(sgs);
		bytelen = ALIGN(bytelen, rctx->blocksize);
		aic_skcipher_task_cfg(task, rctx, din, dout, bytelen,
				      first_flag, last_flag);
		if (last_flag)
			task->next = 0;
		else
			task->next = cpu_to_le32(next_addr);
		if (is_cbc_enc(rctx->flags)) {
			/* Last output as next iv, only CBC mode use it */
			rctx->next_iv = dout + bytelen - rctx->ivsize;
		}
		if (is_cbc_dec(rctx->flags)) {
			/* For decryption, next iv is last ciphertext block */
			rctx->next_iv = rctx->backup_phy_ivs + i * rctx->ivsize;
		}
		dout += bytelen;
	}

	return 0;
}

/*
 * In this case, src sg list is not hardware friendly, but dst is.
 * src data is copied to src_cpy_buf, but task need to build according dst sg
 * list.
 */
static int prepare_task_with_src_cpy_buf_dst_sg(struct aic_crypto_dev *ce,
						struct skcipher_request *req)
{
	dma_addr_t din, dout, next_addr;
	struct aic_skcipher_reqctx *rctx;
	struct scatterlist *sgd;
	u32 first_flag, last_flag;
	struct task_desc *task;
	unsigned int bytelen, remain;
	int i, sg_cnt, ret;

	pr_debug("%s\n", __func__);
	rctx = skcipher_request_ctx(req);

	rctx->src_phy_buf = dma_map_single(ce->dev, rctx->src_cpy_buf,
					   req->cryptlen, DMA_TO_DEVICE);
	if (dma_mapping_error(ce->dev, rctx->src_phy_buf)) {
		dev_err(ce->dev, "Failed to dma map src_phy_buf\n");
		return -EFAULT;
	}

	sg_cnt = sg_nents_for_len(req->dst, req->cryptlen);
	ret = dma_map_sg(ce->dev, req->dst, sg_cnt, DMA_FROM_DEVICE);
	if (ret != sg_cnt) {
		dev_err(ce->dev, "Failed to dma map dst sg\n");
		return -EFAULT;
	}
	rctx->dst_map_sg = true;

	rctx->tasklen = sizeof(struct task_desc) * sg_cnt;
	rctx->task = dma_alloc_coherent(ce->dev, rctx->tasklen, &rctx->phy_task,
					GFP_KERNEL);
	if (!rctx->task)
		return -ENOMEM;

	remain = req->cryptlen;
	din = rctx->src_phy_buf;
	for (i = 0, sgd = req->dst; i < sg_cnt; i++, sgd = sg_next(sgd)) {
		task = &rctx->task[i];
		next_addr = rctx->phy_task + ((i + 1) * sizeof(*task));

		bytelen = min(remain, sg_dma_len(sgd));
		first_flag = (i == 0);
		last_flag = ((i + 1) == sg_cnt);
		dout = sg_dma_address(sgd);
		bytelen = ALIGN(bytelen, rctx->blocksize);
		aic_skcipher_task_cfg(task, rctx, din, dout, bytelen,
				      first_flag, last_flag);
		if (last_flag)
			task->next = 0;
		else
			task->next = cpu_to_le32(next_addr);
		din += bytelen;
		if (is_cbc_enc(rctx->flags)) {
			/* Last output as next iv, only CBC mode use it */
			rctx->next_iv = dout + bytelen - rctx->ivsize;
		}
		if (is_cbc_dec(rctx->flags)) {
			/* For decryption, next iv is last ciphertext block */
			rctx->next_iv = rctx->backup_phy_ivs + i * rctx->ivsize;
		}
	}

	return 0;
}

/*
 * In this case, both src & dst sg list are hardware friendly, and src & dst
 * are match to each other.
 */
static int prepare_task_with_match_sg_mapping(struct aic_crypto_dev *ce,
					      struct skcipher_request *req)
{
	dma_addr_t din, dout, next_addr;
	struct aic_skcipher_reqctx *rctx;
	struct scatterlist *sgs, *sgd;
	u32 first_flag, last_flag;
	struct task_desc *task;
	unsigned int bytelen, remain;
	int i, ret, sg_cnt;

	pr_debug("%s\n", __func__);
	rctx = skcipher_request_ctx(req);

	sg_cnt = sg_nents_for_len(req->src, req->cryptlen);

	rctx->tasklen = sizeof(struct task_desc) * sg_cnt;
	rctx->task = dma_alloc_coherent(ce->dev, rctx->tasklen, &rctx->phy_task,
					GFP_KERNEL);
	if (!rctx->task)
		return -ENOMEM;

	if (req->src != req->dst) {
		ret = dma_map_sg(ce->dev, req->src, sg_cnt, DMA_TO_DEVICE);
		if (ret != sg_cnt) {
			dev_err(ce->dev, "Failed to map src sg.\n");
			return -EFAULT;
		}
		rctx->src_map_sg = true;

		ret = dma_map_sg(ce->dev, req->dst, sg_cnt, DMA_FROM_DEVICE);
		if (ret != sg_cnt) {
			dev_err(ce->dev, "Failed to map dst sg.\n");
			return -EFAULT;
		}
		rctx->dst_map_sg = true;
	} else {
		ret = dma_map_sg(ce->dev, req->src, sg_cnt, DMA_BIDIRECTIONAL);
		if (ret != sg_cnt) {
			dev_err(ce->dev, "Failed to map sg.\n");
			return -EFAULT;
		}
		rctx->src_map_sg = true;
		rctx->dst_map_sg = true;
	}

	rctx->next_iv = 0;
	remain = req->cryptlen;
	for (i = 0, sgs = req->src, sgd = req->dst; i < sg_cnt;
	     i++, sgs = sg_next(sgs), sgd = sg_next(sgd)) {
		task = &rctx->task[i];
		next_addr = rctx->phy_task + ((i + 1) * sizeof(*task));

		bytelen = min(remain, sg_dma_len(sgs));
		first_flag = (i == 0);
		last_flag = ((i + 1) == sg_cnt);
		din = sg_dma_address(sgs);
		dout = sg_dma_address(sgd);
		bytelen = ALIGN(bytelen, rctx->blocksize);
		aic_skcipher_task_cfg(task, rctx, din, dout, bytelen,
				      first_flag, last_flag);
		if (last_flag)
			task->next = 0;
		else
			task->next = cpu_to_le32(next_addr);
		if (is_cbc_enc(rctx->flags)) {
			/* For CBC encryption next iv is last output block */
			rctx->next_iv = dout + bytelen - rctx->ivsize;
		}
		if (is_cbc_dec(rctx->flags)) {
			/* For CBC decryption, next iv is last input block */
			rctx->next_iv = rctx->backup_phy_ivs + i * rctx->ivsize;
		}
	}

	return 0;
}

static int prepare_task_to_genhsk(struct aic_crypto_dev *ce,
				  struct skcipher_request *req)
{
	struct aic_skcipher_reqctx *rctx;
	unsigned char *keymaterial;
	dma_addr_t ssram_addr;
	int pages;
	u32 bytelen;

	pr_debug("%s\n", __func__);
	rctx = skcipher_request_ctx(req);
	rctx->tasklen = sizeof(struct task_desc);
	rctx->task = dma_alloc_coherent(ce->dev, rctx->tasklen, &rctx->phy_task,
					GFP_KERNEL);
	if (!rctx->task)
		return -ENOMEM;

	memset(rctx->task, 0, rctx->tasklen);

	bytelen = ALIGN(req->cryptlen, rctx->blocksize);
	keymaterial = (unsigned char *)req->src;
	ssram_addr = (dma_addr_t)req->dst;
	pages = get_order(ALIGN(req->cryptlen, bytelen));
	rctx->src_cpy_buf = (void *)__get_free_pages(GFP_ATOMIC, pages);
	if (!rctx->src_cpy_buf) {
		dev_err(ce->dev, "Failed to allocate pages for src.\n");
		return -ENOMEM;
	}
	memset(rctx->src_cpy_buf, 0, bytelen);
	memcpy(rctx->src_cpy_buf, keymaterial, req->cryptlen);
	rctx->src_phy_buf = dma_map_single(ce->dev, rctx->src_cpy_buf,
					   req->cryptlen, DMA_TO_DEVICE);
	if (dma_mapping_error(ce->dev, rctx->src_phy_buf)) {
		dev_err(ce->dev, "Failed to dma map src_phy_buf\n");
		return -EFAULT;
	}

	if (is_aes_ecb(rctx->flags))
		rctx->keylen = AES_BLOCK_SIZE;
	if (is_des_ecb(rctx->flags))
		rctx->keylen = DES_BLOCK_SIZE;
	aic_skcipher_task_cfg(rctx->task, rctx, rctx->src_phy_buf,
			      ssram_addr, bytelen, 1, 1);
	return 0;
}

static inline void encrypt_set_cbc_next_iv(struct skcipher_request *req,
					   unsigned int ivsize)
{
	unsigned int offset;

	offset = req->cryptlen - ivsize;
	scatterwalk_map_and_copy(req->iv, req->dst, offset, ivsize, 0);
}

static inline void decrypt_set_cbc_next_iv(struct skcipher_request *req,
					   struct aic_skcipher_reqctx *rctx)
{
	u8 *p, offset;

	offset = rctx->ivsize * (rctx->backup_iv_cnt - 1);
	p = rctx->backup_ivs + offset;
	memcpy(req->iv, p, rctx->ivsize);
}

static void update_next_iv(struct aic_crypto_dev *ce,
			   struct skcipher_request *req)
{
	struct aic_skcipher_reqctx *rctx;

	rctx = skcipher_request_ctx(req);
	if (rctx->phy_iv) {
		dma_unmap_single(ce->dev, rctx->phy_iv, rctx->ivsize,
				 DMA_BIDIRECTIONAL);

		if (is_ctr(rctx->flags)) {
			/* Set counter for next block */
			memcpy(req->iv, rctx->iv, rctx->ivsize);
		}
		if (is_cbc_enc(rctx->flags))
			encrypt_set_cbc_next_iv(req, rctx->ivsize);
		if (is_cbc_dec(rctx->flags))
			decrypt_set_cbc_next_iv(req, rctx);
		rctx->phy_iv = 0;
	}
}

static int aic_skcipher_unprepare_req(struct crypto_engine *engine, void *areq)
{
	struct aic_skcipher_reqctx *rctx;
	struct aic_skcipher_tfm_ctx *ctx;
	struct skcipher_request *req;
	struct crypto_skcipher *tfm;
	struct device *dev;
	int pages, sg_cnt;

	pr_debug("%s\n", __func__);
	req = container_of(areq, struct skcipher_request, base);
	rctx = skcipher_request_ctx(req);
	tfm = crypto_skcipher_reqtfm(req);
	ctx = crypto_skcipher_ctx(tfm);
	dev = ctx->ce->dev;

	if (req->src != req->dst) {
		if (rctx->src_map_sg) {
			sg_cnt = sg_nents_for_len(req->src, req->cryptlen);
			dma_unmap_sg(dev, req->src, sg_cnt, DMA_TO_DEVICE);
			rctx->src_map_sg = false;
		}
		if (rctx->dst_map_sg) {
			sg_cnt = sg_nents_for_len(req->dst, req->cryptlen);
			dma_unmap_sg(dev, req->dst, sg_cnt, DMA_FROM_DEVICE);
			rctx->dst_map_sg = false;
		}
	} else {
		if (rctx->src_map_sg || rctx->dst_map_sg) {
			sg_cnt = sg_nents_for_len(req->src, req->cryptlen);
			dma_unmap_sg(dev, req->src, sg_cnt, DMA_BIDIRECTIONAL);
			rctx->src_map_sg = false;
			rctx->dst_map_sg = false;
		}
	}

	if (rctx->task) {
		dma_free_coherent(dev, rctx->tasklen, rctx->task,
				  rctx->phy_task);
		rctx->task = NULL;
		rctx->phy_task = 0;
		rctx->tasklen = 0;
	}

	pages = get_order(ALIGN(req->cryptlen, rctx->blocksize));
	if (rctx->dst_cpy_buf) {
		if (rctx->dst_phy_buf) {
			dma_unmap_single(dev, rctx->dst_phy_buf, req->cryptlen,
					 DMA_FROM_DEVICE);
			rctx->dst_phy_buf = 0;
		}

		free_pages((unsigned long)rctx->dst_cpy_buf, pages);
		rctx->dst_cpy_buf = NULL;
	}
	if (rctx->src_cpy_buf) {
		if (rctx->src_phy_buf) {
			dma_unmap_single(dev, rctx->src_phy_buf, req->cryptlen,
					 DMA_TO_DEVICE);
			rctx->src_phy_buf = 0;
		}
		free_pages((unsigned long)rctx->src_cpy_buf, pages);
		rctx->src_cpy_buf = NULL;
	}

	if (rctx->phy_iv) {
		dma_unmap_single(dev, rctx->phy_iv, rctx->ivsize,
				 DMA_BIDIRECTIONAL);
		rctx->phy_iv = 0;
	}

	if (rctx->ssram_addr) {
		unsigned int hsklen;

		hsklen = ALIGN(rctx->keylen, rctx->blocksize);
		aic_ssram_free(ctx->ce, rctx->ssram_addr, hsklen);
		rctx->ssram_addr = 0;
	}

	if (rctx->backup_ivs) {
		dma_free_coherent(dev, rctx->backup_iv_cnt * rctx->ivsize,
				  rctx->backup_ivs, rctx->backup_phy_ivs);
		rctx->backup_ivs = NULL;
		rctx->backup_phy_ivs = 0;
	}

	if (rctx->iv) {
		kfree_sensitive(rctx->iv);
		rctx->iv = NULL;
		rctx->ivsize = 0;
	}

	if (rctx->phy_key) {
		dma_unmap_single(dev, rctx->phy_key, rctx->keylen,
				 DMA_TO_DEVICE);
		rctx->phy_key = 0;
	}
	if (rctx->key) {
		kfree_sensitive(rctx->key);
		rctx->key = NULL;
		rctx->keylen = 0;
	}

	return 0;
}

static int aic_skcipher_prepare_req(struct crypto_engine *engine, void *areq)
{
	struct aic_skcipher_tfm_ctx *ctx;
	struct aic_skcipher_reqctx *rctx;
	struct skcipher_request *req;
	struct crypto_skcipher *tfm;
	struct aic_crypto_dev *ce;
	unsigned int ivsize;
	int ret;

	pr_debug("%s\n", __func__);
	req = container_of(areq, struct skcipher_request, base);
	tfm = crypto_skcipher_reqtfm(req);
	ctx = crypto_skcipher_ctx(tfm);
	rctx = skcipher_request_ctx(req);
	ce = ctx->ce;

	rctx->blocksize = crypto_skcipher_blocksize(tfm);
	if (is_gen_hsk(rctx->flags)) {
		/* Prepare request for key decryption to SSRAM */
		ret = prepare_task_to_genhsk(ce, req);
		if (ret) {
			dev_err(ce->dev, "Failed to prepare task\n");
			goto err;
		}
		return 0;
	}

	/* Prepare request for data encrypt/decrypt */
	if (ctx->inkey) {
		/* Copy key for hardware */
		rctx->key = kmemdup(ctx->inkey, ctx->inkeylen,
				    GFP_KERNEL | GFP_DMA);
		if (!rctx->key) {
			ret = -ENOMEM;
			dev_err(ce->dev, "Failed to prepare key\n");
			goto err;
		}
		rctx->keylen = ctx->inkeylen;
		rctx->phy_key = dma_map_single(ce->dev, rctx->key, rctx->keylen,
					       DMA_TO_DEVICE);
		if (dma_mapping_error(ce->dev, rctx->phy_key)) {
			dev_err(ce->dev, "Failed to dma map key\n");
			ret = -EFAULT;
			goto err;
		}
	}

	ivsize = crypto_skcipher_ivsize(tfm);
	if (req->iv && ivsize > 0) {
		/* Copy IV for hardware */
		rctx->iv = kmemdup(req->iv, ivsize, GFP_KERNEL | GFP_DMA);
		if (!rctx->iv) {
			ret = -ENOMEM;
			dev_err(ce->dev, "Failed to prepare iv\n");
			goto err;
		}
		rctx->ivsize = ivsize;
		rctx->phy_iv = dma_map_single(ce->dev, rctx->iv, ivsize,
					      DMA_BIDIRECTIONAL);
		if (dma_mapping_error(ce->dev, rctx->phy_iv)) {
			dev_err(ce->dev, "Failed to dma map iv\n");
			ret = -EFAULT;
			goto err;
		}
	}

	ret = aic_skcipher_prepare_data_buf(ce, req);
	if (ret) {
		dev_err(ce->dev, "Failed to prepare data buf\n");
		goto err;
	}

	if (is_cbc_dec(rctx->flags)) {
		ret = backup_iv_for_cbc_decrypt(ce, req);
		if (ret) {
			dev_err(ce->dev, "Failed to backup iv\n");
			goto err;
		}
	}

	if (rctx->src_cpy_buf && rctx->dst_cpy_buf)
		ret = prepare_task_with_both_cpy_buf(ce, req);
	else if (rctx->src_cpy_buf)
		ret = prepare_task_with_src_cpy_buf_dst_sg(ce, req);
	else if (rctx->dst_cpy_buf)
		ret = prepare_task_with_src_sg_dst_cpy_buf(ce, req);
	else
		ret = prepare_task_with_match_sg_mapping(ce, req);

	if (ret) {
		dev_err(ce->dev, "Failed to prepare task\n");
		goto err;
	}

	return 0;

err:
	aic_skcipher_unprepare_req(engine, areq);
	return ret;
}

static int aic_skcipher_do_one_req(struct crypto_engine *engine, void *areq)
{
	struct skcipher_request *req;
	struct crypto_skcipher *tfm;
	struct aic_skcipher_tfm_ctx *ctx;
	struct aic_skcipher_reqctx *rctx;
	struct aic_crypto_dev *ce;
	unsigned int ret;
	u32 algo;

	pr_debug("%s\n", __func__);
	req = container_of(areq, struct skcipher_request, base);
	tfm = crypto_skcipher_reqtfm(req);
	ctx = crypto_skcipher_ctx(tfm);
	rctx = skcipher_request_ctx(req);
	ce = ctx->ce;

	if (!ce) {
		pr_err("Device is null.\n");
		return -ENODEV;
	}

	if (!aic_crypto_is_ce_avail(ce)) {
		dev_err(ce->dev, "Crypto engine is busy.\n");
		return -EBUSY;
	}

	if (!aic_crypto_is_accel_avail(ce, SK_ALG_ACCELERATOR)) {
		dev_err(ce->dev, "sk accelerator fifo is full.\n");
		return -EBUSY;
	}

	mutex_lock(&ce->sk_accel.req_lock);
	ret = kfifo_put(&ce->sk_accel.req_fifo, areq);
	mutex_unlock(&ce->sk_accel.req_lock);
	if (ret != 1) {
		dev_err(ce->dev, "req fifo is full.\n");
		return ret;
	}

	if (DEBUG_CE)
		aic_crypto_dump_task(rctx->task, rctx->tasklen);
	algo = rctx->task->alg.alg_tag;
	aic_crypto_irq_enable(ce, SK_ALG_ACCELERATOR);
	aic_crypto_enqueue_task(ce, algo, rctx->phy_task);
	if (DEBUG_CE)
		aic_crypto_dump_reg(ce);

	return 0;
}

void aic_skcipher_handle_irq(struct aic_crypto_dev *ce)
{
	struct aic_skcipher_reqctx *rctx;
	struct skcipher_request *req;
	void *areq;
	int err, ret;

	mutex_lock(&ce->sk_accel.req_lock);
	if (kfifo_len(&ce->sk_accel.req_fifo) < 1) {
		dev_err(ce->dev, "There is no req in fifo.\n");
		goto err;
	}

	/*
	 * TODO: Maybe more than one task is finished, should check here
	 */
	/* kfifo_peek(&ce->sk_accel.req_fifo, &areq); */
	ret = kfifo_get(&ce->sk_accel.req_fifo, &areq);
	if (ret != 1) {
		dev_err(ce->dev, "Get req from fifo failed.\n");
		goto err;
	}

	mutex_unlock(&ce->sk_accel.req_lock);

	req = container_of(areq, struct skcipher_request, base);
	rctx = skcipher_request_ctx(req);

	if (rctx->dst_phy_buf && rctx->dst_cpy_buf) {
		dma_unmap_single(ce->dev, rctx->dst_phy_buf, req->cryptlen,
				 DMA_FROM_DEVICE);

		/* Copy output data to dst sg list */
		aic_crypto_sg_copy(rctx->dst_cpy_buf, req->dst, req->cryptlen,
				   1);

		rctx->dst_phy_buf = 0;
	}

	err = (ce->err_status >> 8 * SK_ALG_ACCELERATOR) & 0xFF;
	if (!err)
		update_next_iv(ce, req);
	crypto_finalize_skcipher_request(ce->sk_accel.engine, req, err);
	if (DEBUG_CE)
		aic_crypto_dump_reg(ce);
	return;
err:
	mutex_unlock(&ce->sk_accel.req_lock);
}

static int aic_skcipher_alg_init(struct crypto_skcipher *tfm)
{
	struct aic_skcipher_tfm_ctx *ctx = crypto_skcipher_ctx(tfm);
	struct skcipher_alg *skalg = crypto_skcipher_alg(tfm);
	struct aic_skcipher_alg *aicalg;

	pr_debug("%s\n", __func__);
	memset(ctx, 0, sizeof(*ctx));

	aicalg = container_of(skalg, struct aic_skcipher_alg, alg);
	ctx->ce = aicalg->ce;

#ifdef CONFIG_PM
	if (ctx->ce->task_count == 0)
		pm_runtime_get_sync(ctx->ce->dev);
	ctx->ce->task_count++;
#endif

	ctx->enginectx.op.do_one_request = aic_skcipher_do_one_req;
	ctx->enginectx.op.prepare_request = aic_skcipher_prepare_req;
	ctx->enginectx.op.unprepare_request = aic_skcipher_unprepare_req;

	crypto_skcipher_set_reqsize(tfm, sizeof(struct aic_skcipher_reqctx));
	return 0;
}

static void aic_skcipher_alg_exit(struct crypto_skcipher *tfm)
{
	struct aic_skcipher_tfm_ctx *ctx = crypto_skcipher_ctx(tfm);

	pr_debug("%s\n", __func__);
	ctx = crypto_skcipher_ctx(tfm);

#ifdef CONFIG_PM
	ctx->ce->task_count--;
	if (ctx->ce->task_count == 0) {
		pm_runtime_mark_last_busy(ctx->ce->dev);
		pm_runtime_put_autosuspend(ctx->ce->dev);
	}
#endif

	kfree_sensitive(ctx->inkey);
	ctx->inkey = NULL;
}

static int aic_skcipher_alg_setkey(struct crypto_skcipher *tfm, const u8 *key,
				   unsigned int keylen)
{
	struct aic_skcipher_tfm_ctx *ctx = crypto_skcipher_ctx(tfm);

	pr_debug("%s\n", __func__);
	if (ctx->inkey)
		kfree_sensitive(ctx->inkey);
	ctx->inkeylen = 0;

	ctx->inkey = kmemdup(key, keylen, GFP_KERNEL);
	if (!ctx->inkey)
		return -ENOMEM;
	ctx->inkeylen = keylen;
	return 0;
}

static int aic_skcipher_crypt(struct skcipher_request *req, unsigned long flg)
{
	struct aic_skcipher_tfm_ctx *ctx;
	struct aic_skcipher_reqctx *rctx;
	struct crypto_skcipher *tfm;
	struct crypto_engine *eng;

	tfm = crypto_skcipher_reqtfm(req);
	ctx = crypto_skcipher_ctx(tfm);
	rctx = skcipher_request_ctx(req);

	pr_debug("%s\n", __func__);
	if (!ctx || !ctx->ce) {
		pr_err("aic skcipher, device is null\n");
		return -ENODEV;
	}

	memset(rctx, 0, sizeof(*rctx));

	/* Mark the request, and transfer it to crypto hardware engine */
	rctx->flags = flg;

	if (is_need_genhsk(rctx->flags)) {
		unsigned long efuse;
		unsigned int bs, hsklen;
		int ret;

		efuse = (rctx->flags & FLG_KEYMASK) >> FLG_KEYOFFSET;
		rctx->keylen = ctx->inkeylen;
		bs = crypto_skcipher_blocksize(tfm);
		hsklen = ALIGN(rctx->keylen, bs);
		rctx->ssram_addr = aic_ssram_alloc(ctx->ce, hsklen);
		if (!rctx->ssram_addr) {
			dev_err(ctx->ce->dev, "Failed to allocate ssram key\n");
			return -ENOMEM;
		}

		ret = aic_ssram_aes_genkey(ctx->ce, efuse, ctx->inkey,
					   ctx->inkeylen, rctx->ssram_addr);
		if (ret) {
			aic_ssram_free(ctx->ce, rctx->ssram_addr, hsklen);
			return ret;
		}
	}
	eng = ctx->ce->sk_accel.engine;
	return crypto_transfer_skcipher_request_to_engine(eng, req);
}

static int aic_skcipher_efuse_decrypt(struct skcipher_request *req,
				      unsigned long flg)
{
	struct aic_skcipher_tfm_ctx *ctx;
	struct aic_skcipher_reqctx *rctx;
	struct crypto_skcipher *tfm;
	struct crypto_engine *eng;

	tfm = crypto_skcipher_reqtfm(req);
	ctx = crypto_skcipher_ctx(tfm);
	rctx = skcipher_request_ctx(req);
	memset(rctx, 0, sizeof(*rctx));

	rctx->flags = flg;
	eng = ctx->ce->sk_accel.engine;
	return crypto_transfer_skcipher_request_to_engine(eng, req);
}

/*
 * Use eFuse Key to decrypt key material and output to Secure SRAM
 */
static int aic_skcipher_gen_hsk(struct aic_crypto_dev *ce, char *alg_name,
				unsigned long flg, unsigned char *key_material,
				unsigned int mlen, dma_addr_t ssram_addr)
{
	struct crypto_skcipher *tfm;
	struct skcipher_request *req;
	struct crypto_wait wait;
	int ret;

	pr_debug("%s\n", __func__);
	tfm = crypto_alloc_skcipher(alg_name, 0, 0);
	if (IS_ERR(tfm)) {
		dev_err(ce->dev, "Failed to alloc tfm, err %ld\n", PTR_ERR(tfm));
		return PTR_ERR(tfm);
	}

	req = skcipher_request_alloc(tfm, GFP_KERNEL);
	if (!req) {
		ret = -ENOMEM;
		dev_err(ce->dev, "Failed to alloc request\n");
		goto err_free_tfm;
	}

	crypto_init_wait(&wait);

	skcipher_request_set_callback(req, CRYPTO_TFM_REQ_MAY_SLEEP |
					    CRYPTO_TFM_REQ_MAY_BACKLOG,
					    crypto_req_done, &wait);

	/* Special setup for request because here using private api instead of
	 * crypto_skcipher_decrypt to decrypt key material to ssram
	 */
	req->cryptlen = mlen;
	req->src = (struct scatterlist *)key_material;
	req->dst = (struct scatterlist *)ssram_addr;

	ret = crypto_wait_req(aic_skcipher_efuse_decrypt(req, flg), &wait);

	skcipher_request_free(req);
err_free_tfm:
	crypto_free_skcipher(tfm);
	return ret;
}

int aic_ssram_aes_genkey(struct aic_crypto_dev *ce, unsigned long efuse_key,
			 unsigned char *key_material, unsigned int mlen,
			 dma_addr_t ssram_addr)
{
	unsigned long flags, kflag;

	kflag = (efuse_key << FLG_KEYOFFSET) & FLG_KEYMASK;
	flags = FLG_AES | FLG_ECB | FLG_DEC | FLG_GENHSK | kflag;

	pr_debug("%s\n", __func__);
	return aic_skcipher_gen_hsk(ce, "ecb-aes-aic", flags, key_material,
				    mlen, ssram_addr);
}

int aic_ssram_des_genkey(struct aic_crypto_dev *ce, unsigned long efuse_key,
			 unsigned char *key_material, unsigned int mlen,
			 dma_addr_t ssram_addr)
{
	unsigned long flags, kflag;

	kflag = (efuse_key << FLG_KEYOFFSET) & FLG_KEYMASK;
	flags = FLG_DES | FLG_ECB | FLG_DEC | FLG_GENHSK | kflag;

	pr_debug("%s\n", __func__);
	return aic_skcipher_gen_hsk(ce, "ecb-des-aic", flags, key_material,
				    mlen, ssram_addr);
}

static int aic_skcipher_aes_ecb_encrypt(struct skcipher_request *req)
{
	return aic_skcipher_crypt(req, FLG_AES | FLG_ECB);
}

static int aic_skcipher_aes_ecb_decrypt(struct skcipher_request *req)
{
	return aic_skcipher_crypt(req, FLG_AES | FLG_ECB | FLG_DEC);
}

static int aic_skcipher_aes_cbc_encrypt(struct skcipher_request *req)
{
	return aic_skcipher_crypt(req, FLG_AES | FLG_CBC);
}

static int aic_skcipher_aes_cbc_decrypt(struct skcipher_request *req)
{
	return aic_skcipher_crypt(req, FLG_AES | FLG_CBC | FLG_DEC);
}

static int aic_skcipher_aes_ctr_encrypt(struct skcipher_request *req)
{
	return aic_skcipher_crypt(req, FLG_AES | FLG_CTR);
}

static int aic_skcipher_aes_ctr_decrypt(struct skcipher_request *req)
{
	return aic_skcipher_crypt(req, FLG_AES | FLG_CTR | FLG_DEC);
}

static int aic_skcipher_aes_cts_encrypt(struct skcipher_request *req)
{
	return aic_skcipher_crypt(req, FLG_AES | FLG_CTS);
}

static int aic_skcipher_aes_cts_decrypt(struct skcipher_request *req)
{
	return aic_skcipher_crypt(req, FLG_AES | FLG_CTS | FLG_DEC);
}

static int aic_skcipher_aes_xts_encrypt(struct skcipher_request *req)
{
	return aic_skcipher_crypt(req, FLG_AES | FLG_XTS);
}

static int aic_skcipher_aes_xts_decrypt(struct skcipher_request *req)
{
	return aic_skcipher_crypt(req, FLG_AES | FLG_XTS | FLG_DEC);
}

static int aic_skcipher_des_ecb_encrypt(struct skcipher_request *req)
{
	return aic_skcipher_crypt(req, FLG_DES | FLG_ECB);
}

static int aic_skcipher_des_ecb_decrypt(struct skcipher_request *req)
{
	return aic_skcipher_crypt(req, FLG_DES | FLG_ECB | FLG_DEC);
}

static int aic_skcipher_des_cbc_encrypt(struct skcipher_request *req)
{
	return aic_skcipher_crypt(req, FLG_DES | FLG_CBC);
}

static int aic_skcipher_des_cbc_decrypt(struct skcipher_request *req)
{
	return aic_skcipher_crypt(req, FLG_DES | FLG_CBC | FLG_DEC);
}

static int aic_skcipher_des3_ecb_encrypt(struct skcipher_request *req)
{
	return aic_skcipher_crypt(req, FLG_TDES | FLG_ECB);
}

static int aic_skcipher_des3_ecb_decrypt(struct skcipher_request *req)
{
	return aic_skcipher_crypt(req, FLG_TDES | FLG_ECB | FLG_DEC);
}

static int aic_skcipher_des3_cbc_encrypt(struct skcipher_request *req)
{
	return aic_skcipher_crypt(req, FLG_TDES | FLG_CBC);
}

static int aic_skcipher_des3_cbc_decrypt(struct skcipher_request *req)
{
	return aic_skcipher_crypt(req, FLG_TDES | FLG_CBC | FLG_DEC);
}

static int aic_skcipher_ssk_aes_ecb_encrypt(struct skcipher_request *req)
{
	unsigned long flags;

	flags = FLG_AES | FLG_ECB | (CE_KEY_SRC_SSK << FLG_KEYOFFSET);
	return aic_skcipher_crypt(req, flags);
}

static int aic_skcipher_ssk_aes_ecb_decrypt(struct skcipher_request *req)
{
	unsigned long flags;

	flags = FLG_AES | FLG_ECB | FLG_DEC | (CE_KEY_SRC_SSK << FLG_KEYOFFSET);
	return aic_skcipher_crypt(req, flags);
}

static int aic_skcipher_ssk_aes_cbc_encrypt(struct skcipher_request *req)
{
	unsigned long flags;

	flags = FLG_AES | FLG_CBC | (CE_KEY_SRC_SSK << FLG_KEYOFFSET);
	return aic_skcipher_crypt(req, flags);
}

static int aic_skcipher_ssk_aes_cbc_decrypt(struct skcipher_request *req)
{
	unsigned long flags;

	flags = FLG_AES | FLG_CBC | FLG_DEC | (CE_KEY_SRC_SSK << FLG_KEYOFFSET);
	return aic_skcipher_crypt(req, flags);
}

static int aic_skcipher_huk_aes_ecb_encrypt(struct skcipher_request *req)
{
	unsigned long flags;

	flags = FLG_AES | FLG_ECB | (CE_KEY_SRC_HUK << FLG_KEYOFFSET);
	return aic_skcipher_crypt(req, flags);
}

static int aic_skcipher_huk_aes_ecb_decrypt(struct skcipher_request *req)
{
	unsigned long flags;

	flags = FLG_AES | FLG_ECB | FLG_DEC | (CE_KEY_SRC_HUK << FLG_KEYOFFSET);
	return aic_skcipher_crypt(req, flags);
}

static int aic_skcipher_huk_aes_cbc_encrypt(struct skcipher_request *req)
{
	unsigned long flags;

	flags = FLG_AES | FLG_CBC | (CE_KEY_SRC_HUK << FLG_KEYOFFSET);
	return aic_skcipher_crypt(req, flags);
}

static int aic_skcipher_huk_aes_cbc_decrypt(struct skcipher_request *req)
{
	unsigned long flags;

	flags = FLG_AES | FLG_CBC | FLG_DEC | (CE_KEY_SRC_HUK << FLG_KEYOFFSET);
	return aic_skcipher_crypt(req, flags);
}

static int aic_skcipher_huk_aes_cts_encrypt(struct skcipher_request *req)
{
	unsigned long flags;

	flags = FLG_AES | FLG_CTS | (CE_KEY_SRC_HUK << FLG_KEYOFFSET);
	return aic_skcipher_crypt(req, flags);
}

static int aic_skcipher_huk_aes_cts_decrypt(struct skcipher_request *req)
{
	unsigned long flags;

	flags = FLG_AES | FLG_CTS | FLG_DEC | (CE_KEY_SRC_HUK << FLG_KEYOFFSET);
	return aic_skcipher_crypt(req, flags);
}

static int aic_skcipher_huk_aes_xts_encrypt(struct skcipher_request *req)
{
	unsigned long flags;

	flags = FLG_AES | FLG_XTS | (CE_KEY_SRC_HUK << FLG_KEYOFFSET);
	return aic_skcipher_crypt(req, flags);
}

static int aic_skcipher_huk_aes_xts_decrypt(struct skcipher_request *req)
{
	unsigned long flags;

	flags = FLG_AES | FLG_XTS | FLG_DEC | (CE_KEY_SRC_HUK << FLG_KEYOFFSET);
	return aic_skcipher_crypt(req, flags);
}

static struct aic_skcipher_alg sk_algs[] = {
	{
		.alg = {
			.base.cra_name = "ecb(aes)",
			.base.cra_driver_name = "ecb-aes-aic",
			.base.cra_priority = 400,
			.base.cra_flags = CRYPTO_ALG_ASYNC |
					  CRYPTO_ALG_ALLOCATES_MEMORY,
			.base.cra_blocksize = AES_BLOCK_SIZE,
			.base.cra_ctxsize = sizeof(struct aic_skcipher_tfm_ctx),
			.base.cra_alignmask = 0,
			.base.cra_module = THIS_MODULE,
			.init = aic_skcipher_alg_init,
			.exit = aic_skcipher_alg_exit,
			.setkey = aic_skcipher_alg_setkey,
			.decrypt = aic_skcipher_aes_ecb_decrypt,
			.encrypt = aic_skcipher_aes_ecb_encrypt,
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
			.ivsize = 0,
		},
	},
	{
		.alg = {
			.base.cra_name = "cbc(aes)",
			.base.cra_driver_name = "cbc-aes-aic",
			.base.cra_priority = 400,
			.base.cra_flags = CRYPTO_ALG_ASYNC |
					  CRYPTO_ALG_ALLOCATES_MEMORY,
			.base.cra_blocksize = AES_BLOCK_SIZE,
			.base.cra_ctxsize = sizeof(struct aic_skcipher_tfm_ctx),
			.base.cra_alignmask = 0,
			.base.cra_module = THIS_MODULE,
			.init = aic_skcipher_alg_init,
			.exit = aic_skcipher_alg_exit,
			.setkey = aic_skcipher_alg_setkey,
			.decrypt = aic_skcipher_aes_cbc_decrypt,
			.encrypt = aic_skcipher_aes_cbc_encrypt,
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
			.ivsize = AES_BLOCK_SIZE,
		},
	},
	{
		.alg = {
			.base.cra_name = "ctr(aes)",
			.base.cra_driver_name = "ctr-aes-aic",
			.base.cra_priority = 400,
			.base.cra_flags = CRYPTO_ALG_ASYNC |
					  CRYPTO_ALG_ALLOCATES_MEMORY,
			.base.cra_blocksize = AES_BLOCK_SIZE,
			.base.cra_ctxsize = sizeof(struct aic_skcipher_tfm_ctx),
			.base.cra_alignmask = 0,
			.base.cra_module = THIS_MODULE,
			.init = aic_skcipher_alg_init,
			.exit = aic_skcipher_alg_exit,
			.setkey = aic_skcipher_alg_setkey,
			.decrypt = aic_skcipher_aes_ctr_decrypt,
			.encrypt = aic_skcipher_aes_ctr_encrypt,
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
			.ivsize = AES_BLOCK_SIZE,
		},
	},
	{
		.alg = {
			.base.cra_name = "cts(aes)",
			.base.cra_driver_name = "cts-aes-aic",
			.base.cra_priority = 400,
			.base.cra_flags = CRYPTO_ALG_ASYNC |
					  CRYPTO_ALG_ALLOCATES_MEMORY,
			.base.cra_blocksize = AES_BLOCK_SIZE,
			.base.cra_ctxsize = sizeof(struct aic_skcipher_tfm_ctx),
			.base.cra_alignmask = 0,
			.base.cra_module = THIS_MODULE,
			.init = aic_skcipher_alg_init,
			.exit = aic_skcipher_alg_exit,
			.setkey = aic_skcipher_alg_setkey,
			.decrypt = aic_skcipher_aes_cts_decrypt,
			.encrypt = aic_skcipher_aes_cts_encrypt,
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
			.ivsize = AES_BLOCK_SIZE,
		},
	},
	{
		.alg = {
			.base.cra_name = "xts(aes)",
			.base.cra_driver_name = "xts-aes-aic",
			.base.cra_priority = 400,
			.base.cra_flags = CRYPTO_ALG_ASYNC |
					  CRYPTO_ALG_ALLOCATES_MEMORY,
			.base.cra_blocksize = AES_BLOCK_SIZE,
			.base.cra_ctxsize = sizeof(struct aic_skcipher_tfm_ctx),
			.base.cra_alignmask = 0,
			.base.cra_module = THIS_MODULE,
			.init = aic_skcipher_alg_init,
			.exit = aic_skcipher_alg_exit,
			.setkey = aic_skcipher_alg_setkey,
			.decrypt = aic_skcipher_aes_xts_decrypt,
			.encrypt = aic_skcipher_aes_xts_encrypt,
			.min_keysize = 2 * AES_MIN_KEY_SIZE,
			.max_keysize = 2 * AES_MAX_KEY_SIZE,
			.ivsize = AES_BLOCK_SIZE,
		},
	},
	{
		.alg = {
			.base.cra_name = "ecb(des)",
			.base.cra_driver_name = "ecb-des-aic",
			.base.cra_priority = 400,
			.base.cra_flags = CRYPTO_ALG_ASYNC |
					  CRYPTO_ALG_ALLOCATES_MEMORY,
			.base.cra_blocksize = DES_BLOCK_SIZE,
			.base.cra_ctxsize = sizeof(struct aic_skcipher_tfm_ctx),
			.base.cra_alignmask = 0,
			.base.cra_module = THIS_MODULE,
			.init = aic_skcipher_alg_init,
			.exit = aic_skcipher_alg_exit,
			.setkey = aic_skcipher_alg_setkey,
			.decrypt = aic_skcipher_des_ecb_decrypt,
			.encrypt = aic_skcipher_des_ecb_encrypt,
			.min_keysize = DES_KEY_SIZE,
			.max_keysize = DES_KEY_SIZE,
		},
	},
	{
		.alg = {
			.base.cra_name = "cbc(des)",
			.base.cra_driver_name = "cbc-des-aic",
			.base.cra_priority = 400,
			.base.cra_flags = CRYPTO_ALG_ASYNC |
					  CRYPTO_ALG_ALLOCATES_MEMORY,
			.base.cra_blocksize = DES_BLOCK_SIZE,
			.base.cra_ctxsize = sizeof(struct aic_skcipher_tfm_ctx),
			.base.cra_alignmask = 0,
			.base.cra_module = THIS_MODULE,
			.init = aic_skcipher_alg_init,
			.exit = aic_skcipher_alg_exit,
			.setkey = aic_skcipher_alg_setkey,
			.decrypt = aic_skcipher_des_cbc_decrypt,
			.encrypt = aic_skcipher_des_cbc_encrypt,
			.min_keysize = DES_KEY_SIZE,
			.max_keysize = DES_KEY_SIZE,
			.ivsize = DES_BLOCK_SIZE,
		},
	},
	{
		.alg = {
			.base.cra_name = "ecb(des3_ede)",
			.base.cra_driver_name = "ecb-des3_ede-aic",
			.base.cra_priority = 400,
			.base.cra_flags = CRYPTO_ALG_ASYNC |
					  CRYPTO_ALG_ALLOCATES_MEMORY,
			.base.cra_blocksize = DES3_EDE_BLOCK_SIZE,
			.base.cra_ctxsize = sizeof(struct aic_skcipher_tfm_ctx),
			.base.cra_alignmask = 0,
			.base.cra_module = THIS_MODULE,
			.init = aic_skcipher_alg_init,
			.exit = aic_skcipher_alg_exit,
			.setkey = aic_skcipher_alg_setkey,
			.decrypt = aic_skcipher_des3_ecb_decrypt,
			.encrypt = aic_skcipher_des3_ecb_encrypt,
			.min_keysize = DES3_EDE_KEY_SIZE,
			.max_keysize = DES3_EDE_KEY_SIZE,
		},
	},
	{
		.alg = {
			.base.cra_name = "cbc(des3_ede)",
			.base.cra_driver_name = "cbc-des3_ede-aic",
			.base.cra_priority = 400,
			.base.cra_flags = CRYPTO_ALG_ASYNC |
					  CRYPTO_ALG_ALLOCATES_MEMORY,
			.base.cra_blocksize = DES3_EDE_BLOCK_SIZE,
			.base.cra_ctxsize = sizeof(struct aic_skcipher_tfm_ctx),
			.base.cra_alignmask = 0,
			.base.cra_module = THIS_MODULE,
			.init = aic_skcipher_alg_init,
			.exit = aic_skcipher_alg_exit,
			.setkey = aic_skcipher_alg_setkey,
			.decrypt = aic_skcipher_des3_cbc_decrypt,
			.encrypt = aic_skcipher_des3_cbc_encrypt,
			.min_keysize = DES3_EDE_KEY_SIZE,
			.max_keysize = DES3_EDE_KEY_SIZE,
			.ivsize = DES3_EDE_BLOCK_SIZE,
		},
	},
	{
		.alg = {
			.base.cra_name = "ssk-protected(ecb(aes))",
			.base.cra_driver_name = "ssk-protected-ecb-aes-aic",
			.base.cra_priority = 400,
			.base.cra_flags = CRYPTO_ALG_ASYNC |
					  CRYPTO_ALG_ALLOCATES_MEMORY,
			.base.cra_blocksize = AES_BLOCK_SIZE,
			.base.cra_ctxsize = sizeof(struct aic_skcipher_tfm_ctx),
			.base.cra_alignmask = 0,
			.base.cra_module = THIS_MODULE,
			.init = aic_skcipher_alg_init,
			.exit = aic_skcipher_alg_exit,
			.setkey = aic_skcipher_alg_setkey,
			.decrypt = aic_skcipher_ssk_aes_ecb_decrypt,
			.encrypt = aic_skcipher_ssk_aes_ecb_encrypt,
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
			.ivsize = 0,
		},
	},
	{
		.alg = {
			.base.cra_name = "ssk-protected(cbc(aes))",
			.base.cra_driver_name = "ssk-protected-cbc-aes-aic",
			.base.cra_priority = 400,
			.base.cra_flags = CRYPTO_ALG_ASYNC |
					  CRYPTO_ALG_ALLOCATES_MEMORY,
			.base.cra_blocksize = AES_BLOCK_SIZE,
			.base.cra_ctxsize = sizeof(struct aic_skcipher_tfm_ctx),
			.base.cra_alignmask = 0,
			.base.cra_module = THIS_MODULE,
			.init = aic_skcipher_alg_init,
			.exit = aic_skcipher_alg_exit,
			.setkey = aic_skcipher_alg_setkey,
			.decrypt = aic_skcipher_ssk_aes_cbc_decrypt,
			.encrypt = aic_skcipher_ssk_aes_cbc_encrypt,
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
			.ivsize = AES_BLOCK_SIZE,
		},
	},
	{
		.alg = {
			.base.cra_name = "huk-protected(ecb(aes))",
			.base.cra_driver_name = "huk-protected-ecb-aes-aic",
			.base.cra_priority = 400,
			.base.cra_flags = CRYPTO_ALG_ASYNC |
					  CRYPTO_ALG_ALLOCATES_MEMORY,
			.base.cra_blocksize = AES_BLOCK_SIZE,
			.base.cra_ctxsize = sizeof(struct aic_skcipher_tfm_ctx),
			.base.cra_alignmask = 0,
			.base.cra_module = THIS_MODULE,
			.init = aic_skcipher_alg_init,
			.exit = aic_skcipher_alg_exit,
			.setkey = aic_skcipher_alg_setkey,
			.decrypt = aic_skcipher_huk_aes_ecb_decrypt,
			.encrypt = aic_skcipher_huk_aes_ecb_encrypt,
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
			.ivsize = 0,
		},
	},
	{
		.alg = {
			.base.cra_name = "huk-protected(cbc(aes))",
			.base.cra_driver_name = "huk-protected-cbc-aes-aic",
			.base.cra_priority = 400,
			.base.cra_flags = CRYPTO_ALG_ASYNC |
					  CRYPTO_ALG_ALLOCATES_MEMORY,
			.base.cra_blocksize = AES_BLOCK_SIZE,
			.base.cra_ctxsize = sizeof(struct aic_skcipher_tfm_ctx),
			.base.cra_alignmask = 0,
			.base.cra_module = THIS_MODULE,
			.init = aic_skcipher_alg_init,
			.exit = aic_skcipher_alg_exit,
			.setkey = aic_skcipher_alg_setkey,
			.decrypt = aic_skcipher_huk_aes_cbc_decrypt,
			.encrypt = aic_skcipher_huk_aes_cbc_encrypt,
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
			.ivsize = AES_BLOCK_SIZE,
		},
	},
	{
		.alg = {
			.base.cra_name = "huk-protected(cts(aes))",
			.base.cra_driver_name = "huk-protected-cts-aes-aic",
			.base.cra_priority = 400,
			.base.cra_flags = CRYPTO_ALG_ASYNC |
					  CRYPTO_ALG_ALLOCATES_MEMORY,
			.base.cra_blocksize = AES_BLOCK_SIZE,
			.base.cra_ctxsize = sizeof(struct aic_skcipher_tfm_ctx),
			.base.cra_alignmask = 0,
			.base.cra_module = THIS_MODULE,
			.init = aic_skcipher_alg_init,
			.exit = aic_skcipher_alg_exit,
			.setkey = aic_skcipher_alg_setkey,
			.decrypt = aic_skcipher_huk_aes_cts_decrypt,
			.encrypt = aic_skcipher_huk_aes_cts_encrypt,
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
			.ivsize = AES_BLOCK_SIZE,
		},
	},
	{
		.alg = {
			.base.cra_name = "huk-protected(xts(aes))",
			.base.cra_driver_name = "huk-protected-xts-aes-aic",
			.base.cra_priority = 400,
			.base.cra_flags = CRYPTO_ALG_ASYNC |
					  CRYPTO_ALG_ALLOCATES_MEMORY,
			.base.cra_blocksize = AES_BLOCK_SIZE,
			.base.cra_ctxsize = sizeof(struct aic_skcipher_tfm_ctx),
			.base.cra_alignmask = 0,
			.base.cra_module = THIS_MODULE,
			.init = aic_skcipher_alg_init,
			.exit = aic_skcipher_alg_exit,
			.setkey = aic_skcipher_alg_setkey,
			.decrypt = aic_skcipher_huk_aes_xts_decrypt,
			.encrypt = aic_skcipher_huk_aes_xts_encrypt,
			.min_keysize = 2 * AES_MIN_KEY_SIZE,
			.max_keysize = 2 * AES_MAX_KEY_SIZE,
			.ivsize = AES_BLOCK_SIZE,
		},
	},
};

int aic_crypto_skcipher_accelerator_init(struct aic_crypto_dev *ce)
{
	struct crypto_engine *eng;
	int ret, i;

	eng = crypto_engine_alloc_init_and_set(ce->dev, true, NULL, true,
					       ACCEL_QUEUE_MAX_SIZE);
	if (!eng) {
		dev_err(ce->dev, "Failed to init crypto engine for skcipher\n");
		ret = -ENOMEM;
		return ret;
	}

	ce->sk_accel.engine = eng;
	ret = crypto_engine_start(ce->sk_accel.engine);
	if (ret) {
		dev_err(ce->dev, "Failed to start crypto skcipher engine\n");
		goto err;
	}

	mutex_init(&ce->sk_accel.req_lock);
	INIT_KFIFO(ce->sk_accel.req_fifo);
	ret = kfifo_alloc(&ce->sk_accel.req_fifo, ACCEL_QUEUE_MAX_SIZE,
			  GFP_KERNEL);
	if (ret) {
		dev_err(ce->dev, "Failed to alloc kfifo for skcipher\n");
		goto err2;
	}

	for (i = 0; i < ARRAY_SIZE(sk_algs); i++) {
		sk_algs[i].ce = ce;
		ret = crypto_register_skcipher(&sk_algs[i].alg);
		if (ret) {
			dev_err(ce->dev, "Failed to register skcipher algs\n");
			goto err3;
		}
	}

	return 0;

err3:
	for (--i; i >= 0; --i) {
		sk_algs[i].ce = NULL;
		crypto_unregister_skcipher(&sk_algs[i].alg);
	}
err2:
	kfifo_free(&ce->sk_accel.req_fifo);
err:
	crypto_engine_exit(ce->sk_accel.engine);
	return ret;
}

int aic_crypto_skcipher_accelerator_exit(struct aic_crypto_dev *ce)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sk_algs); i++) {
		sk_algs[i].ce = NULL;
		crypto_unregister_skcipher(&sk_algs[i].alg);
	}
	kfifo_free(&ce->sk_accel.req_fifo);
	crypto_engine_exit(ce->sk_accel.engine);

	return 0;
}
