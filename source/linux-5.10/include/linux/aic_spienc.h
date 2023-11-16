/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 Artinchip Technology Co.,Ltd
 * Author: Dehuang Wu <dehuang.wu@artinchip.com>
 */
#ifndef __AIC_SPI_ENC_H__
#define __AIC_SPI_ENC_H__
#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/string.h>

struct aic_spienc_iv {
	u32 addr;
	u32 cpos;
	u32 tweak;
	u32 spi_id;
};

#define AIC_SPIENC_ALL_FF 0x0000FF00

#endif /* __AIC_SPI_ENC_H__ */
