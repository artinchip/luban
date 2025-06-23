/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Definitions for the ArtInChip frambuffer driver
 *
 * Copyright (C) 2020-2021 ArtInChip Technology Co., Ltd.
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#ifndef _UAPI__ARTINCHIP_FB_H_
#define _UAPI__ARTINCHIP_FB_H_

#include <dm/device.h>
#include <linux/fb.h>
#include "artinchip_video.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * The AICFB is developed based on the Linux frame buffer. Besides the basic
 * functions provided by the Linux frame buffer, the AICFB also provides
 * extendedfunctions for controlling graphics layers such as updating layer
 * config data.
 */

/**
 * struct aicfb_format - aicfb format
 *@name: format name
 *@bits_per_pixel: bits per pixel
 *@red: red bitfield
 *@green: green bitfield
 *@blue: blue bitfield
 *@transp: alpha bitfield
 *@format: color format
 */
struct aicfb_format {
	const char *name;
	u32 bits_per_pixel;
	struct fb_bitfield red;
	struct fb_bitfield green;
	struct fb_bitfield blue;
	struct fb_bitfield transp;
	enum aic_pixel_format format;
};

/**
 * struct aicfb_dt - aicfb data from dts
 * @width: buffer width
 * @height: buffer height
 * @stride: buffer stride
 * @size: buffer size
 * @format: the pointer of color format
 * @fb_np: the node of fb
 * @de_np: the node of diaplay engine
 * @di_np: the node of display interface
 * @panel_np: the node of panel
 */
struct aicfb_dt {
	u32 width;
	u32 height;
	u32 stride;
	u32 size; /* The final framebuffer size to be requested */
	struct aicfb_format *format;

	ofnode fb_np;
	ofnode de_np;
	ofnode di_np;
	ofnode panel_np;
};

#define AICFB_LAYER_TYPE_VIDEO	0
#define AICFB_LAYER_TYPE_UI	1

/**
 * struct aicfb_layer_num - aicfb layer number
 * @vi_num: number of video layers
 * @ui_num: number of UI layers
 *
 *  total_layer_num = vi_num + ui_num
 *
 *  layer id range: [0, total_layer_num - 1]
 */
struct aicfb_layer_num {
	unsigned int vi_num;
	unsigned int ui_num;
};

/**
 * Flags to describe layer capability
 */

 /* layer supports scaling */
#define AICFB_CAP_SCALING_FLAG           (1 << 0)

/* layer with multi-rectangular windows */
#define AICFB_CAP_4_RECT_WIN_FLAG        (1 << 1)

/* layer supports alpha blending */
#define AICFB_CAP_ALPHA_FLAG             (1 << 2)

/* layer supports color key blending */
#define AICFB_CAP_CK_FLAG                (1 << 3)

/**
 * struct aicfb_layer_capability - aicfb layer capability
 * @layer_id: the layer id
 * @layer_type: the layer type
 *  0: UI layer
 *  1: Video layer
 * @max_width:  the max pixels per line
 * @max_height: the max lines
 * @cap_flags:  flags of layer capability
 */
struct aicfb_layer_capability {
	unsigned int layer_id;
	unsigned int layer_type;
	unsigned int max_width;
	unsigned int max_height;
	unsigned int cap_flags;
};

/**
 * Flags to describe aicfb_buffer
 */

/* color space flags for YUV format */
#define AICFB_BUF_COLOR_SPACE_BT601                (0 << 0)
#define AICFB_BUF_COLOR_SPACE_BT709                (1 << 0)
#define AICFB_BUF_COLOR_SPACE_YCC_FULL_RANGE       (2 << 0)

/**
 * struct aicfb_buffer - aicfb frame buffer
 * @phy_addr[3]: address of frame buffer
 *  single addr for interleaved fomart with 1 plane,
 *  double addr for semi-planar fomart with 2 planes,
 *  triple addr for planar format with 3 planes
 * @size: width and height of aicfb_buffer
 * @stride[3]: stride for all planes
 * @crop_en: corp disable/enable ctrl
 *  0: disable crop the buffer
 *  1: enable crop the buffer
 * @crop: crop info
 * @format: color format
 * @buf_flags: aicfb buffer flags
 */
struct aicfb_buffer {
	unsigned int            phy_addr[3];
	struct aic_size         size;
	unsigned int            stride[3];
	unsigned int            crop_en;
	struct aic_rect         crop;
	enum aic_pixel_format   format;
	unsigned int            buf_flags;
};

/**
 * struct aicfb_layer_data - aicfb layer data
 * @enable
 *  0: disable the layer
 *  1: enable the layer
 * @layer_id: the layer id
 *
 * @rect_id: the rectanglular window id of the layer
 *  only used by layers with multi-rectangular windows
 *  for example: if the layer has 4 rectangular windows,
 *  rect_id can be 0,1,2 or 3 for different windows
 *
 * @scale_size:  scaling size
 *  if the layer can be scaled. the scaling size can be different
 *  from the input buffer. the input buffer can be original aicfb_buffer
 *  or crop aicfb_buffer, otherwise, the scaling size will be ignore
 *
 * @pos: left-top x/y coordinate of the screen in pixels
 * @buf: frame buffer
 */
struct aicfb_layer_data {
	unsigned int enable;
	unsigned int layer_id;
	unsigned int rect_id;
	struct aic_size scale_size;
	struct aic_point pos;
	struct aicfb_buffer buf;
};

/**
 * struct aicfb_config_lists - aicfb config lists
 * @num: the total number of layer data config lists
 * @layers[]: the array of aicfb_layer_data lists
 */
struct aicfb_config_lists {
	unsigned int num;
	struct aicfb_layer_data layers[];
};

/**
 * struct aicfb_alpha_config - aicfb layer alpha blending config
 *
 * @layer_id: the layer id
 *
 * @enable
 *  0: disable alpha
 *  1: enable alpha
 *
 * @mode: alpha mode
 *  0: pixel alpha mode
 *  1: global alpha mode
 *  2: mixder alpha mode(alpha = pixel alpha * global alpha / 255)
 *
 * @value: global alpha value (0~255)
 *  used by global alpha mode and mixer alpha mode
 *
 */
struct aicfb_alpha_config {
	unsigned int layer_id;
	unsigned int enable;
	unsigned int mode;
	unsigned int value;
};

/**
 * struct aicfb_ck_config - aicfb layer color key blending config
 *
 * @layer_id: the layer id
 *
 * @ck_enable
 *  0: disable color key
 *  1: enable color key
 *
 *
 * @ck_value: color key rgb value to match the layer pixels
 *  bit[31:24]: reserved
 *  bit[23:16]: R value
 *  bit[15:8]: G value
 *  bit[7:0]: B value
 *
 */
struct aicfb_ck_config {
	unsigned int layer_id;
	unsigned int enable;
	unsigned int value;
};

struct aicfb_ops {
	/**
	 * update_layer() - Update a UI layer
	 *
	 * @dev:	device to adjust
	 */
	void (*update_layer)(struct udevice *dev);

	/**
	 * enable_panel() - Enable a panel driver
	 *
	 * @dev:	device to enable
	 * @on:		true to enable panel
	 */
	void (*enable_panel)(struct udevice *dev, u32 on);

	/**
	 * draw_progress_bar() - Display a progress bar when updating
	 *
	 * @dev:	device to adjust
	 * @value:	percentage of progress
	 *
	 *	- if value is 0 then the image will be displayed
	 *	- if value is 0< && <100 then the image will be filled
	 *	  with another color based on the value
	 *	- if value is 100 then it will be filled to full
	 *	  and change the color angin.
	 */
	void (*draw_progress_bar)(struct udevice *dev, int value);
};

static inline void aicfb_update_ui_layer(struct udevice *dev)
{
	struct aicfb_ops *ops =
		(struct aicfb_ops *)dev_get_driver_data(dev);

	if (ops->update_layer)
		ops->update_layer(dev);
}

static inline void aicfb_startup_panel(struct udevice *dev)
{
	struct aicfb_ops *ops =
		(struct aicfb_ops *)dev_get_driver_data(dev);

	if (ops->enable_panel)
		ops->enable_panel(dev, 1);
}

static inline void aicfb_draw_progress_bar(struct udevice *dev, int value)
{
	struct aicfb_ops *ops =
		(struct aicfb_ops *)dev_get_driver_data(dev);

	if (ops->draw_progress_bar)
		ops->draw_progress_bar(dev, value);
}

void aicfb_draw_text(uint x_frac, uint y, int value);
void aicfb_draw_rect(struct udevice *dev,
			uint x, uint y, uint width, uint height,
			u8 red, u8 green, u8 blue);
int aic_bmp_display(struct udevice *dev, ulong bmp_image);

void draw_progress_bar(int value);

int aic_disp_logo(const char *name, int boot_param);

#if defined(__cplusplus)
}
#endif
#endif /* _UAPI__ARTINCHIP_FB_H_ */
