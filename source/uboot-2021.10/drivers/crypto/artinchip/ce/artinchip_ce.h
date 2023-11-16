// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020-2022 ArtInChip Technology Co.,Ltd
 * Author: Hao Xiong <hao.xiong@artinchip.com>
 */

#ifndef _AIC_CE_DEFINE_H_
#define _AIC_CE_DEFINE_H_
#include <linux/io.h>

#define CE_REG_ICR(priv)	(priv->base + 0x00)
#define CE_REG_ISR(priv)	(priv->base + 0x04)
#define CE_REG_TAR(priv)	(priv->base + 0x08)
#define CE_REG_TCR(priv)	(priv->base + 0x0C)
#define CE_REG_TSR(priv)	(priv->base + 0x10)
#define CE_REG_TER(priv)	(priv->base + 0x14)
#define CE_REG_VER(priv)	(priv->base + 0xFFC)

#define ALG_TAG_AES_ECB		(0x00)
#define ALG_TAG_AES_CBC		(0x01)
#define ALG_TAG_AES_CTR		(0x02)
#define ALG_TAG_AES_XTS		(0x03)
#define ALG_TAG_AES_CTS		(0x04)
#define ALG_TAG_DES_ECB		(0x10)
#define ALG_TAG_DES_CBC		(0x11)
#define ALG_TAG_TDES_ECB	(0x20)
#define ALG_TAG_TDES_CBC	(0x21)
#define ALG_TAG_RSA		(0x30)
#define ALG_TAG_SHA1		(0x40)
#define ALG_TAG_SHA224		(0x41)
#define ALG_TAG_SHA256		(0x42)
#define ALG_TAG_SHA384		(0x43)
#define ALG_TAG_SHA512		(0x44)
#define ALG_TAG_MD5		(0x45)
#define ALG_TAG_HMAC_SHA1	(0x46)
#define ALG_TAG_HMAC_SHA256	(0x47)
#define ALG_TAG_TRNG		(0x50)

#define KEY_SIZE_64		(0x00)
#define KEY_SIZE_128		(0x01)
#define KEY_SIZE_192		(0x02)
#define KEY_SIZE_256		(0x03)
#define KEY_SIZE_512		(0x04)
#define KEY_SIZE_1024		(0x05)
#define KEY_SIZE_2048		(0x06)

#define TASK_LOAD_BIT_OFFSET	(31)
#define TASK_ALG_BIT_OFFSET	(0)

/*
 * Key source
 *   - User input key(SRAM/DRAM/SecureSRAM)
 *   - eFuse key
 */
#define CE_KEY_SRC_USER		(0)
#define CE_KEY_SRC_SSK		(1)
#define CE_KEY_SRC_HUK		(2)
#define CE_KEY_SRC_PNK		(3)
#define CE_KEY_SRC_PSK0		(4)
#define CE_KEY_SRC_PSK1		(5)
#define CE_KEY_SRC_PSK2		(6)
#define CE_KEY_SRC_PSK3		(7)

/* Operation direction */
#define OP_DIR_ENCRYPT		(0)
#define OP_DIR_DECRYPT		(1)

#define SECURE_SRAM_SIZE	(1024)
#define SECURE_SRAM_BASE	(0x10021000)

/* Accelerator Queue size */
#define ACCEL_QUEUE_MAX_SIZE	(8)

#define SK_ALG_ACCELERATOR	0
#define HASH_ALG_ACCELERATOR	1
#define AK_ALG_ACCELERATOR	2

#endif
