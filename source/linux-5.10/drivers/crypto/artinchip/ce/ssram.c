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
#include <crypto/des.h>
#include <crypto/aes.h>
#include <crypto/internal/des.h>
#include <crypto/engine.h>
#include <crypto/scatterwalk.h>
#include <crypto/internal/skcipher.h>
#include "crypto.h"
#include "ssram.h"

dma_addr_t aic_ssram_alloc(struct aic_crypto_dev *ce, unsigned int siz)
{
	u64 mask;
	int i, unitcnt;
	dma_addr_t addr;

	if (siz <= 0)
		return 0;

	mutex_lock(&ce->ssram_lock);
	unitcnt = DIV_ROUND_UP(siz, SSRAM_UNIT_SIZE);

	mask = GENMASK(unitcnt - 1, 0);

	for (i = 0; i < (SECURE_SRAM_SIZE / SSRAM_UNIT_SIZE); i++) {
		mask = (mask << i);
		if (!(ce->ssram_bitmap & mask)) {
			ce->ssram_bitmap |= mask;
			break;
		}
	}

	mutex_unlock(&ce->ssram_lock);
	if (i >= (SECURE_SRAM_SIZE / SSRAM_UNIT_SIZE))
		return 0;

	addr = (dma_addr_t)SECURE_SRAM_BASE;
	addr += (SSRAM_UNIT_SIZE * i);
	return addr;
}

int aic_ssram_free(struct aic_crypto_dev *ce, dma_addr_t ssram_addr,
		   unsigned int siz)
{
	dma_addr_t addr;
	int i, unitcnt;
	unsigned int mask;

	if (siz <= 0)
		return 0;

	addr = (dma_addr_t)SECURE_SRAM_BASE;
	if (ssram_addr < addr)
		return -EINVAL;

	i = (ssram_addr - addr) / SSRAM_UNIT_SIZE;
	if (i >= SECURE_SRAM_SIZE / SSRAM_UNIT_SIZE)
		return -EINVAL;

	unitcnt = DIV_ROUND_UP(siz, SSRAM_UNIT_SIZE);
	mask = GENMASK(unitcnt - 1 + i, i);

	mutex_lock(&ce->ssram_lock);
	ce->ssram_bitmap &= (~mask);
	mutex_unlock(&ce->ssram_lock);

	return 0;
}
