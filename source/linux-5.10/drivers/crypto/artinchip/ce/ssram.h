/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021, Artinchip Technology Co., Ltd
 *
 * Wu Dehuang <dehuang.wu@artinchip.com>
 */

#ifndef _AIC_SSRAM_H_
#define _AIC_SSRAM_H_

#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include "ce_hardware.h"
#include "crypto.h"

/* SSRAM allocate unit size is 16 bytes */
#define SSRAM_UNIT_SIZE 16

dma_addr_t aic_ssram_alloc(struct aic_crypto_dev *ce, unsigned int siz);
int aic_ssram_free(struct aic_crypto_dev *ce, dma_addr_t ssram_addr,
		   unsigned int siz);
int aic_ssram_aes_genkey(struct aic_crypto_dev *ce, unsigned long efuse_key,
			 unsigned char *key_material, unsigned int mlen,
			 dma_addr_t ssram_addr);
int aic_ssram_des_genkey(struct aic_crypto_dev *ce, unsigned long efuse_key,
			 unsigned char *key_material, unsigned int mlen,
			 dma_addr_t ssram_addr);

#endif
