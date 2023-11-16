/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2020-2022 ArtInChip Technology Co., Ltd.
 * Authors:  matteo <duanmt@artinchip.com>
 */

#ifndef _RGB_REG_H_
#define _RGB_REG_H_

#include <linux/bits.h>
#include <linux/io.h>
#include <linux/types.h>

enum aic_rgb_cko_phase_sel {
	CKO_PHASE_SEL_0 = 0x0,
	CKO_PHASE_SEL_90 = 0x1,
	CKO_PHASE_SEL_180 = 0x2,
	CKO_PHASE_SEL_270 = 0x3
};

#define RGB_LCD_CTL_MODE_MASK		GENMASK(5, 4)
#define RGB_LCD_CTL_MODE(x)		(((x) & 0x3) << 4)
#define RGB_LCD_CTL_EN			BIT(0)

#define RGB_CLK_CTL_CKO_PHASE_MASK	GENMASK(1, 0)
#define RGB_CLK_CTL_CKO_PHASE(x)	(((x) & 0x3) << 0)

#define RGB_LCD_CTL_PRGB_MODE_MASK	GENMASK(10, 8)
#define RGB_LCD_CTL_PRGB_MODE(x)	(((x) & 0x7) << 8)
#define RGB_LCD_CTL_SRGB_MODE		BIT(12)
#define RGB_LCD_CTL_I8080_MODE_MASK	GENMASK(19, 16)
#define RGB_LCD_CTL_I8080_MODE(x)	(((x) & 0xF) << 16)
#define RGB_LCD_CTL_SPI_MODE_MASK	GENMASK(21, 20)
#define RGB_LCD_CTL_SPI_MODE(x)		(((x) & 0x3)<<20)
#define RGB_LCD_CTL_SPI_FORMAT_MASK	GENMASK(27, 24)
#define RGB_LCD_CTL_SPI_FORMAT(x)	(((x) & 0xF)<<24)

#define RGB_DATA_SEL_EVEN_DP2316_MASK	GENMASK(25, 24)
#define RGB_DATA_SEL_EVEN_DP2316(x)	(((x) & 0x3) << 24)
#define RGB_DATA_SEL_EVEN_DP1508_MASK	GENMASK(21, 20)
#define RGB_DATA_SEL_EVEN_DP1508(x)	(((x) & 0x3) << 20)
#define RGB_DATA_SEL_EVEN_DP0700_MASK	GENMASK(17, 16)
#define RGB_DATA_SEL_EVEN_DP0700(x)	(((x) & 0x3) << 16)
#define RGB_DATA_SEL_OOD_DP2316_MASK	GENMASK(9, 8)
#define RGB_DATA_SEL_OOD_DP2316(x)	(((x) & 0x3) << 8)
#define RGB_DATA_SEL_OOD_DP1508_MASK	GENMASK(5, 4)
#define RGB_DATA_SEL_OOD_DP1508(x)	(((x) & 0x3) << 4)
#define RGB_DATA_SEL_OOD_DP0700_MASK	GENMASK(1, 0)
#define RGB_DATA_SEL_OOD_DP0700(x)	(((x) & 0x3) << 0)

#define RGB_DATA_OUT_SEL_MASK		GENMASK(2, 0)
#define RGB_DATA_OUT_SEL(x)		(((x) & 0x7) << 0)

#define CKO_PHASE_SEL_MASK		GENMASK(1, 0)
#define CKO_PHASE_SEL(x)		(((x) & 0x3) << 0)

#define RGB_LCD_CTL		0x00
#define RGB_CLK_CTL		0x10
#define RGB_DATA_SEL		0x20
#define RGB_OOD_DATA		0x24
#define RGB_EVEN_DATA		0x28
#define RGB_DATA_SEQ_SEL	0x30
#define RGB_VERSION		0xFC

#endif // end of _RGB_REG_H_
