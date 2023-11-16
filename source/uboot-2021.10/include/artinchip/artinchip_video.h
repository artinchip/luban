/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Definitions for the ArtInChip video interface
 *
 * Copyright (C) 2020-2021 ArtInChip Technology Co., Ltd.
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#ifndef _UAPI__ARTINCHIP_VIDOE_H_
#define _UAPI__ARTINCHIP_VIDOE_H_

#if defined(__cplusplus)
extern "C" {
#endif

struct aic_rect {
	int x;
	int y;
	int width;
	int height;
};

struct aic_point {
	int x;
	int y;
};

struct aic_size {
	int width;
	int height;
};

enum aic_color_space {
	AIC_BT601,
	AIC_BT709,
	AIC_YCC_FULL_RANGE,
};

enum aic_pixel_format {
	AIC_FMT_ARGB_8888            = 0x00,
	AIC_FMT_ABGR_8888            = 0x01,
	AIC_FMT_RGBA_8888            = 0x02,
	AIC_FMT_BGRA_8888            = 0x03,
	AIC_FMT_XRGB_8888            = 0x04,
	AIC_FMT_XBGR_8888            = 0x05,
	AIC_FMT_RGBX_8888            = 0x06,
	AIC_FMT_BGRX_8888            = 0x07,
	AIC_FMT_RGB_888              = 0x08,
	AIC_FMT_BGR_888              = 0x09,
	AIC_FMT_ARGB_1555            = 0x0a,
	AIC_FMT_ABGR_1555            = 0x0b,
	AIC_FMT_RGBA_5551            = 0x0c,
	AIC_FMT_BGRA_5551            = 0x0d,
	AIC_FMT_RGB_565              = 0x0e,
	AIC_FMT_BGR_565              = 0x0f,
	AIC_FMT_ARGB_4444            = 0x10,
	AIC_FMT_ABGR_4444            = 0x11,
	AIC_FMT_RGBA_4444            = 0x12,
	AIC_FMT_BGRA_4444            = 0x13,

	AIC_FMT_YUV420P              = 0x20,
	AIC_FMT_NV12                 = 0x21,
	AIC_FMT_NV21                 = 0x22,
	AIC_FMT_YUV422P              = 0x23,
	AIC_FMT_NV16                 = 0x24,
	AIC_FMT_NV61                 = 0x25,
	AIC_FMT_YUYV                 = 0x26,
	AIC_FMT_YVYU                 = 0x27,
	AIC_FMT_UYVY                 = 0x28,
	AIC_FMT_VYUY                 = 0x29,
	AIC_FMT_YUV400               = 0x2a,
	AIC_FMT_YUV444P              = 0x2b,

	AIC_FMT_YUV420_64x32_TILE    = 0x30,
	AIC_FMT_YUV420_128x16_TILE   = 0x31,
	AIC_FMT_YUV422_64x32_TILE    = 0x32,
	AIC_FMT_YUV422_128x16_TILE   = 0x33,
	AIC_FMT_MAX,
};

#if defined(__cplusplus)
}
#endif

#endif /* _UAPI__ARTINCHIP_VIDOE_H_ */
