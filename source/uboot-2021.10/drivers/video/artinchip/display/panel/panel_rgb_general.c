// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021 ArtInChip Technology Co.,Ltd
 * Authors: Huahui Mai <huahui.mai@artinchip.com>
 */

#include <common.h>
#include <dm.h>
#include <dm/uclass-internal.h>
#include <display.h>
#include <video.h>
#include <fdtdec.h>
#include <linux/fb.h>
#include <panel.h>
#include <dt-bindings/display/artinchip,aic-disp.h>

#include "panel_com.h"

#define PANEL_DEV_NAME		"panel_rgb_general"

static struct aic_panel_funcs panel_funcs = {
	.prepare = panel_default_prepare,
	.enable = panel_default_enable,
	.get_video_mode = panel_default_get_video_mode,
	.register_callback = panel_register_callback,
};

/* Init the videomode parameter, dts will override the initial value. */
static struct fb_videomode panel_vm = {
	.pixclock = 33300000,
	.xres = 800,
	.right_margin = 210,
	.left_margin = 36,
	.hsync_len = 10,
	.yres = 480,
	.lower_margin = 22,
	.upper_margin = 13,
	.vsync_len = 10,
	.flag = DISPLAY_FLAGS_HSYNC_LOW | DISPLAY_FLAGS_VSYNC_LOW |
		DISPLAY_FLAGS_DE_HIGH | DISPLAY_FLAGS_PIXDATA_POSEDGE
};

static int panel_rgb_parse_dt(struct udevice *dev, struct panel_priv *p)
{
	struct panel_rgb *rgb;
	ofnode node = dev_ofnode(dev);

	rgb = malloc(sizeof(*rgb));
	if (!rgb)
		return -ENOMEM;

	if (panel_parse_dts(dev) < 0) {
		free(rgb);
		return -1;
	}

	if (ofnode_read_u32(node, "interface-format", &rgb->format))
		rgb->format = PRGB_24BIT;

	if (ofnode_read_u32(node, "data-order", &rgb->data_order))
		rgb->data_order = RGB;

	if (ofnode_read_u32(node, "clock-phase", &rgb->clock_phase))
		rgb->clock_phase = DEGREE_0;

	rgb->data_mirror = ofnode_read_bool(node, "data-mirror");
	rgb->mode = PRGB;

	p->panel.rgb = rgb;
	return 0;
}

static int panel_probe(struct udevice *dev)
{
	struct panel_priv *priv = dev_get_priv(dev);
	int ret;

	ret = panel_rgb_parse_dt(dev, priv);
	if (ret < 0)
		return ret;

	panel_init(priv, dev, &panel_vm, &panel_funcs, NULL);

	return 0;
}

static const struct udevice_id panel_match_ids[] = {
	{ .compatible = "artinchip,aic-general-rgb-panel" },
	{ /* sentinel*/ },
};

U_BOOT_DRIVER(panel_rgb_general) = {
	.name      = PANEL_DEV_NAME,
	.id        = UCLASS_PANEL,
	.of_match  = panel_match_ids,
	.probe     = panel_probe,
	.priv_auto = sizeof(struct panel_priv),
};
