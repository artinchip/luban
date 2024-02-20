/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021, Artinchip Technology Co., Ltd
 *
 * Wu Dehuang <dehuang.wu@artinchip.com>
 */

#ifndef _AIC_CRYPTO_H_
#define _AIC_CRYPTO_H_

#include <linux/kfifo.h>
#include <crypto/engine.h>
#include <crypto/scatterwalk.h>
#include "ce_hardware.h"
#include "crypto.h"

#define DEBUG_CE 0

struct aic_alg_accelerator {
	struct crypto_engine *engine;
	struct mutex req_lock;
	DECLARE_KFIFO_PTR(req_fifo, void *);
};

struct aic_crypto_dev {
	struct device *dev;
	void __iomem *base;
	struct clk *clk;
	struct reset_control *reset;
	struct aic_alg_accelerator sk_accel;
	struct aic_alg_accelerator ak_accel;
	struct aic_alg_accelerator hash_accel;
	struct mutex ssram_lock;
	u64 ssram_bitmap;
	u32 irq_status;
	u32 err_status;
	u32 clk_rate;
	int task_count;
};

static inline bool is_word_aligned(unsigned int offset)
{
	return ((offset & 0x3) == 0);
}

int aic_crypto_skcipher_accelerator_init(struct aic_crypto_dev *dev);
int aic_crypto_skcipher_accelerator_exit(struct aic_crypto_dev *dev);

int aic_crypto_akcipher_accelerator_init(struct aic_crypto_dev *dev);
int aic_crypto_akcipher_accelerator_exit(struct aic_crypto_dev *dev);

int aic_crypto_hash_accelerator_init(struct aic_crypto_dev *dev);
int aic_crypto_hash_accelerator_exit(struct aic_crypto_dev *dev);

bool aic_crypto_is_ce_avail(struct aic_crypto_dev *dev);
bool aic_crypto_is_accel_avail(struct aic_crypto_dev *dev, int dma_chan);
bool aic_crypto_check_task_done(struct aic_crypto_dev *dev, int dma_chan);
void aic_crypto_pending_clear(struct aic_crypto_dev *dev, int dma_chan);
void aic_crypto_irq_enable(struct aic_crypto_dev *dev, int dma_chan);
void aic_crypto_irq_disable(struct aic_crypto_dev *dev, int dma_chan);
void aic_crypto_hardware_reset(struct aic_crypto_dev *dev);

int aic_crypto_enqueue_task(struct aic_crypto_dev *dev, u32 algo,
			    dma_addr_t phy_task);
void aic_skcipher_handle_irq(struct aic_crypto_dev *ce);
void aic_akcipher_handle_irq(struct aic_crypto_dev *ce);
void aic_hash_handle_irq(struct aic_crypto_dev *ce);

void aic_crypto_sg_copy(void *buf, struct scatterlist *sg, size_t len, int out);
void aic_crypto_dump_reg(struct aic_crypto_dev *dev);
void aic_crypto_dump_task(struct task_desc *task, int len);

int aic_crypto_bignum_byteswap(u8 *bn, u32 len);
int aic_crypto_bignum_le2be(u8 *src, u32 slen, u8 *dst, u32 dlen);
int aic_crypto_bignum_be2le(u8 *src, u32 slen, u8 *dst, u32 dlen);

#endif
