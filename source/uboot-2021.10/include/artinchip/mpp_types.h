/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Definitions for the ArtinChip media process platform interface
 *
 * Copyright (C) 2022-2023 ArtinChip Technology Co., Ltd.
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#ifndef _UAPI_MPP_TYPES_H_
#define _UAPI_MPP_TYPES_H_

#if defined(__cplusplus)
extern "C" {
#endif

struct mpp_rect {
	int x;
	int y;
	int width;
	int height;
};

struct mpp_point {
	int x;
	int y;
};

struct mpp_size {
	int width;
	int height;
};

enum mpp_pixel_format {
	MPP_FMT_ARGB_8888            = 0x00,
	MPP_FMT_ABGR_8888            = 0x01,
	MPP_FMT_RGBA_8888            = 0x02,
	MPP_FMT_BGRA_8888            = 0x03,
	MPP_FMT_XRGB_8888            = 0x04,
	MPP_FMT_XBGR_8888            = 0x05,
	MPP_FMT_RGBX_8888            = 0x06,
	MPP_FMT_BGRX_8888            = 0x07,
	MPP_FMT_RGB_888              = 0x08,
	MPP_FMT_BGR_888              = 0x09,
	MPP_FMT_ARGB_1555            = 0x0a,
	MPP_FMT_ABGR_1555            = 0x0b,
	MPP_FMT_RGBA_5551            = 0x0c,
	MPP_FMT_BGRA_5551            = 0x0d,
	MPP_FMT_RGB_565              = 0x0e,
	MPP_FMT_BGR_565              = 0x0f,
	MPP_FMT_ARGB_4444            = 0x10,
	MPP_FMT_ABGR_4444            = 0x11,
	MPP_FMT_RGBA_4444            = 0x12,
	MPP_FMT_BGRA_4444            = 0x13,

	MPP_FMT_YUV420P              = 0x20,
	MPP_FMT_NV12                 = 0x21,
	MPP_FMT_NV21                 = 0x22,
	MPP_FMT_YUV422P              = 0x23,
	MPP_FMT_NV16                 = 0x24,
	MPP_FMT_NV61                 = 0x25,
	MPP_FMT_YUYV                 = 0x26,
	MPP_FMT_YVYU                 = 0x27,
	MPP_FMT_UYVY                 = 0x28,
	MPP_FMT_VYUY                 = 0x29,
	MPP_FMT_YUV400               = 0x2a,
	MPP_FMT_YUV444P              = 0x2b,

	MPP_FMT_YUV420_64x32_TILE    = 0x30,
	MPP_FMT_YUV420_128x16_TILE   = 0x31,
	MPP_FMT_YUV422_64x32_TILE    = 0x32,
	MPP_FMT_YUV422_128x16_TILE   = 0x33,
	MPP_FMT_MAX,
};

struct mpp_buf {
	unsigned int		phy_addr[3];
	unsigned int            stride[3];
	struct mpp_size         size;
	unsigned int            crop_en;
	struct mpp_rect         crop;
	enum mpp_pixel_format   format;
	unsigned int            flags;
};

struct mpp_frame {
	struct mpp_buf          buf;
	long long               pts;
	unsigned int            id;
	unsigned int            flags;
};

#if defined(__cplusplus)
}
#endif

#endif /* _UAPI_MPP_TYPES_H_ */

