/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 ArtInChip Technology Co.,Ltd
 * Author: Dehuang Wu <dehuang.wu@artinchip.com>
 */
#ifndef __AICUPG_SPL_SPIENC_H__
#define __AICUPG_SPL_SPIENC_H__
#include <common.h>
#include <asm/io.h>
#include <artinchip/aic_spienc.h>
void spi_enc_tweak_select(long sel);
void spi_enc_set_bypass(long status);
#endif /* __AICUPG_SPL_SPIENC_H__ */
