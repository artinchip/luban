/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 ArtInChip Technology Co.,Ltd
 * Author: Dehuang Wu <dehuang.wu@artinchip.com>
 */
#ifndef __SPI_ENC_H__
#define __SPI_ENC_H__
#include <common.h>
#include <asm-generic/ioctl.h>

struct spienc_crypt_cfg {
	u32 addr; /* Cipher data address in SPI Flash */
	u32 cpos; /* Cipher data start position in SPI transfer data */
	u32 clen; /* Cipher data length */
	u32 tweak;
	u32 spi_id;
};

#define AIC_SPIENC_USER_TWEAK         0
#define AIC_SPIENC_HW_TWEAK           1

#define IOC_TYPE_SPIE                 'E'
#define AIC_SPIENC_IOCTL_CRYPT_CFG    _IOW(IOC_TYPE_SPIE, 0x10, \
						struct spienc_crypt_cfg)
#define AIC_SPIENC_IOCTL_START        _IOW(IOC_TYPE_SPIE, 0x11, u32)
#define AIC_SPIENC_IOCTL_STOP         _IOW(IOC_TYPE_SPIE, 0x12, u32)
#define AIC_SPIENC_IOCTL_CHECK_EMPTY  _IOW(IOC_TYPE_SPIE, 0x13, u32)
#define AIC_SPIENC_IOCTL_TWEAK_SELECT _IOW(IOC_TYPE_SPIE, 0x14, u32)

#endif /* __SPI_ENC_H__ */
