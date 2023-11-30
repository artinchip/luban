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

#define PANEL_DEV_NAME		"panel_dbi_ili9341"

/* Init sequence, each line consists of command, count of data, data... */
static const u8 ili9341_commands[] = {
	0xcf,  3,  0x00, 0xc1, 0x30,
	0xed,  4,  0x64, 0x03, 0x12, 0x81,
	0xe8,  3,  0x85, 0x01, 0x78,
	0xcb,  5,  0x39, 0x2c, 0x00, 0x34, 0x02,
	0xf7,  1,  0x20,
	0xea,  2,  0x00, 0x00,
	0xc0,  1,  0x21,
	0xc1,  1,  0x12,
	0xc5,  2,  0x5a, 0x5d,
	0xc7,  1,  0x82,
	0x36,  1,  0x08,
	0x3a,  1,  0x66,
	0xb1,  2,  0x00, 0x16,
	0xb6,  2,  0x0a, 0xa2,
	0xf2,  1,  0x00,
	0xf2,  1,  0x00,
	0xf6,  2,  0x01, 0x30,
	0x26,  1,  0x01,
	0xe0,  15, 0x0f, 0x09, 0x1e, 0x07, 0x0b, 0x01, 0x45, 0x6d, 0x37, 0x08,
		   0x13, 0x01, 0x06, 0x06, 0x00,
	0xe1,  15, 0x00, 0x01, 0x18, 0x00, 0x0d, 0x00, 0x2a, 0x44, 0x44, 0x04,
		   0x11, 0x0c, 0x30, 0x34, 0x0f,
	0x2a,  2,  0x00, 0x00,
	0x2a,  4,  0x00, 0x00, 0x00, 0xef,
	0x2b,  4,  0x00, 0x00, 0x01, 0x3f,
	0x11,  0,
	0x00,  1,  120,
	0x29,  0,
};

static int panel_enable(struct aic_panel *panel)
{
	struct gpio_desc reset;
	int ret;

	ret = gpio_request_by_name(panel->dev, "reset-gpios", 0,
					&reset, GPIOD_IS_OUT);
	if (ret) {
		debug("Failed to get reset gpio\n");
		return ret;
	}

	dm_gpio_set_value(&reset, 1);
	aic_delay_ms(5);
	dm_gpio_set_value(&reset, 0);
	aic_delay_ms(5);
	dm_gpio_set_value(&reset, 1);
	aic_delay_ms(5);

	return panel_dbi_default_enable(panel);
}

static struct aic_panel_funcs panel_funcs = {
	.prepare = panel_default_prepare,
	.enable = panel_enable,
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
	.type = SPI,
	.format = SPI_4LINE_RGB888,
	.commands = {
		.buf = ili9341_commands,
		.len = ARRAY_SIZE(ili9341_commands),
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

U_BOOT_DRIVER(panel_dbi_ili9341) = {
	.name      = PANEL_DEV_NAME,
	.id        = UCLASS_PANEL,
	.of_match  = panel_match_ids,
	.probe     = panel_probe,
	.priv_auto = sizeof(struct panel_priv),
};

