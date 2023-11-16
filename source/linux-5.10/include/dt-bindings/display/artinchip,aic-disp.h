/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Device Tree support for Artinchip SoCs
 *
 * Copyright (C) 2022 Artinchip Technology Co., Ltd
 */

#ifndef __DT_BINDINGS_DISPLAY_AIC_DISP_H
#define __DT_BINDINGS_DISPLAY_AIC_DISP_H

/* RGB mode */
#define PRGB	0x0
#define SRGB	0x1
#define I8080	0x2
#define SPI	0x3

/*
 * PRGB format
 *
 * "HD" or "LD" specifies which of the 24 pins will be discarded: "HD" means
 * that the highest 6/8 pins of the 24 will be discarded, "LD" means that the
 * lowest 6/8 pins will be discarded.
 */
#define PRGB_24BIT	0x0
#define PRGB_18BIT_LD	0x1
#define PRGB_18BIT_HD	0x2
#define PRGB_16BIT_LD	0x3
#define PRGB_16BIT_HD	0x4

/* SRGB format */
#define SRGB_8BIT	0x0
#define SRGB_6BIT	0x1

/* I8080 format */
#define I8080_RGB565_8BIT		0x0
#define I8080_RGB666_8BIT		0x1
#define I8080_RGB666_9BIT		0x2
#define I8080_RGB666_16BIT_3CYCLE	0x3
#define I8080_RGB666_16BIT_2CYCLE	0x4
#define I8080_RGB565_16BIT		0x5
#define I8080_RGB666_18BIT		0x6
#define I8080_RGB888_24BIT		0x7

/* SPI format */
#define SPI_3LINE_RGB565	0x0
#define SPI_3LINE_RGB666	0x1
#define SPI_3LINE_RGB888	0x2
#define SPI_4LINE_RGB565	0x3
#define SPI_4LINE_RGB666	0x4
#define SPI_4LINE_RGB888	0x5
#define SPI_4SDA_RGB565		0x6
#define SPI_4SDA_RGB666		0x7
#define SPI_4SDA_RGB888		0x8

#define SPI_MODE_NUM		0x3

/* RGB interface pixel clock output phase */
#define DEGREE_0	0x0
#define	DEGREE_90	0x1
#define	DEGREE_180	0x2
#define	DEGREE_270	0x3

/* RGB interface output data sequence */
#define RGB	0x02100210
#define RBG	0x02010201
#define BGR	0x00120012
#define BRG	0x00210021
#define GRB	0x01200120
#define GBR	0x01020102

/* Tearing effect mode */
#define TE_BYPASS	0x0
#define TE_HOLD		0x1
#define TE_AUTO		0x2

/* Dither output color depth */
#define DITHER_RGB565	0x1
#define DITHER_RGB666	0x2

#endif /* __DT_BINDINGS_DISPLAY_AIC_DISP_H */
