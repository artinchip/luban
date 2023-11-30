// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 ArtInChip Technology Co.,Ltd
 * Authors: Huahui Mai <huahui.mai@artinchip.com>
 */

#include <common.h>
#include <dm.h>
#include <dm/uclass-internal.h>
#include <display.h>
#include <video.h>
#include <linux/fb.h>
#include <panel.h>
#include <dt-bindings/display/artinchip,aic-disp.h>

#include "panel_dbi.h"

#define PANEL_DEV_NAME		"panel_dbi_ili9488"

/* Init sequence, each line consists of command, count of data, data... */
static const u8 ili9488_commands[] = {
	0xf7,  4,  0xa9, 0x51, 0x2c, 0x82,
	0xec,  4,  0x00, 0x02, 0x03, 0x7a,
	0xc0,  2,  0x13, 0x13,
	0xc1,  1,  0x41,
	0xc5,  3,  0x00, 0x28, 0x80,
	0xb1,  2,  0xb0, 0x11,
	0xb4,  1,  0x02,
	0xb6,  2,  0x02, 0x22,
	0xb7,  1,  0xc6,
	0xbe,  2,  0x00, 0x04,
	0xe9,  1,  0x00,
	0xb7,  1,  0x07,
	0xf4,  3,  0x00, 0x00, 0x0f,
	0xe0,  15, 0x00, 0x04, 0x0e, 0x08, 0x17, 0x0a, 0x40, 0x79, 0x4d, 0x07,
		   0x0e, 0x0a, 0x1a, 0x1d, 0x0f,
	0xe1,  15, 0x00, 0x1b, 0x1f, 0x02, 0x10, 0x05, 0x32, 0x34, 0x43, 0x02,
		   0x0a, 0x09, 0x33, 0x37, 0x0f,
	0xf4,  3,  0x00, 0x00, 0x0f,
	0x35,  1,  0x00,
	0x44,  2,  0x00, 0x10,
	0x33,  6,  0x00, 0x00, 0x01, 0xe0, 0x00, 0x00,
	0x37,  2,  0x00, 0x00,
	0x2a,  4,  0x00, 0x00, 0x01, 0x3f,
	0x2b,  4,  0x00, 0x00, 0x01, 0xdf,
	0x36,  1,  0x08,
	0x3a,  1,  0x66,
	0x11,  0,
	0x00,  1,  120,
	0x29,  0,
	0x50,  0,
};

static struct aic_panel_funcs panel_funcs = {
	.prepare = panel_default_prepare,
	.enable = panel_dbi_default_enable,
	.get_video_mode = panel_default_get_video_mode,
	.register_callback = panel_register_callback,
};

/* Init the videomode parameter, dts will override the initial value. */
static struct fb_videomode panel_vm = {
	.pixclock = 8000000,
	.xres = 320,
	.right_margin = 2,
	.left_margin = 3,
	.hsync_len = 1,
	.yres = 480,
	.lower_margin = 3,
	.upper_margin = 2,
	.vsync_len = 1,
	.flag = DISPLAY_FLAGS_HSYNC_LOW | DISPLAY_FLAGS_VSYNC_LOW |
		DISPLAY_FLAGS_DE_HIGH | DISPLAY_FLAGS_PIXDATA_POSEDGE
};

static struct panel_dbi dbi = {
	.type = I8080,
	.format = I8080_RGB666_16BIT_3CYCLE,
	.commands = {
		.buf = ili9488_commands,
		.len = ARRAY_SIZE(ili9488_commands),
	}
};

static int panel_probe(struct udevice *dev)
{
	struct panel_priv *priv = dev_get_priv(dev);
	int ret;

	ret = panel_parse_dts(dev);
	if (ret < 0)
		return ret;

	priv->panel.dbi = &dbi;
	panel_init(priv, dev, &panel_vm, &panel_funcs, NULL);

	return 0;
}

static const struct udevice_id panel_match_ids[] = {
	{.compatible = "artinchip,aic-dbi-panel-simple"},
	{ /* sentinel*/ },
};

U_BOOT_DRIVER(panel_dbi_ili9488) = {
	.name      = PANEL_DEV_NAME,
	.id        = UCLASS_PANEL,
	.of_match  = panel_match_ids,
	.probe     = panel_probe,
	.priv_auto = sizeof(struct panel_priv),
};

