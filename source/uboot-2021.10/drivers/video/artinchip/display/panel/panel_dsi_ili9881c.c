// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for ili9881c DSI panel.
 *
 * Copyright (C) 2020-2023 ArtInChip Technology Co., Ltd.
 * Authors: huahui.mai <huahui.ami@artinchip.com>
 */

#include <common.h>
#include <dm.h>
#include <dm/uclass-internal.h>
#include <dm/device_compat.h>
#include <video.h>
#include <panel.h>

#include "panel_dsi.h"

#define PANEL_DEV_NAME		"dsi_panel_ili9881c"

struct ili9881c {
	struct gpio_desc reset;
};

static inline struct ili9881c *panel_to_ili9881c(struct aic_panel *panel)
{
	return (struct ili9881c *)panel->panel_private;
}

static int panel_enable(struct aic_panel *panel)
{
	struct ili9881c *ili9881c = panel_to_ili9881c(panel);
	int ret;

	panel_di_enable(panel, 0);

	aic_delay_ms(10);
	dm_gpio_set_value(&ili9881c->reset, 1);
	aic_delay_ms(1);
	dm_gpio_set_value(&ili9881c->reset, 0);
	aic_delay_ms(10);
	dm_gpio_set_value(&ili9881c->reset, 1);
	aic_delay_ms(120);

	panel_dsi_send_perpare(panel);
	panel_dsi_dcs_send_seq(panel, 0xFF, 0x98, 0x81, 0x03);

	/* GIP_1 */
	panel_dsi_dcs_send_seq(panel, 0x10, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x02, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x03, 0x53);
	panel_dsi_dcs_send_seq(panel, 0x04, 0x53);
	panel_dsi_dcs_send_seq(panel, 0x05, 0x13);
	panel_dsi_dcs_send_seq(panel, 0x06, 0x04);
	panel_dsi_dcs_send_seq(panel, 0x07, 0x02);
	panel_dsi_dcs_send_seq(panel, 0x08, 0x02);
	panel_dsi_dcs_send_seq(panel, 0x09, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x0a, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x0b, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x0c, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x0d, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x0e, 0xb0);
	panel_dsi_dcs_send_seq(panel, 0x0f, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x10, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x11, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x12, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x13, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x14, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x15, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x16, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x17, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x18, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x19, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x1a, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x1b, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x1c, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x1d, 0x00);

	panel_dsi_dcs_send_seq(panel, 0x1e, 0xc0);
	panel_dsi_dcs_send_seq(panel, 0x1f, 0x80);
	panel_dsi_dcs_send_seq(panel, 0x20, 0x02);
	panel_dsi_dcs_send_seq(panel, 0x21, 0x09);
	panel_dsi_dcs_send_seq(panel, 0x22, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x23, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x24, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x25, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x26, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x27, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x28, 0x55);
	panel_dsi_dcs_send_seq(panel, 0x29, 0x03);
	panel_dsi_dcs_send_seq(panel, 0x2a, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x2b, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x2c, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x2d, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x2e, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x2f, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x30, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x31, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x32, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x33, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x34, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x35, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x36, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x37, 0x00);

	panel_dsi_dcs_send_seq(panel, 0x38, 0x3c);
	panel_dsi_dcs_send_seq(panel, 0x39, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x3a, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x3b, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x3c, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x3d, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x3e, 0x3c);
	panel_dsi_dcs_send_seq(panel, 0x3f, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x40, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x41, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x42, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x43, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x44, 0x3c);

	/* GIP_2 */
	panel_dsi_dcs_send_seq(panel, 0x50, 0x01);
	panel_dsi_dcs_send_seq(panel, 0x51, 0x23);
	panel_dsi_dcs_send_seq(panel, 0x52, 0x45);
	panel_dsi_dcs_send_seq(panel, 0x53, 0x67);
	panel_dsi_dcs_send_seq(panel, 0x54, 0x89);
	panel_dsi_dcs_send_seq(panel, 0x55, 0xab);
	panel_dsi_dcs_send_seq(panel, 0x56, 0x01);
	panel_dsi_dcs_send_seq(panel, 0x57, 0x23);
	panel_dsi_dcs_send_seq(panel, 0x58, 0x45);
	panel_dsi_dcs_send_seq(panel, 0x59, 0x67);
	panel_dsi_dcs_send_seq(panel, 0x5a, 0x89);
	panel_dsi_dcs_send_seq(panel, 0x5b, 0xab);
	panel_dsi_dcs_send_seq(panel, 0x5c, 0xcd);
	panel_dsi_dcs_send_seq(panel, 0x5d, 0xef);

	/* GIP_2 */
	panel_dsi_dcs_send_seq(panel, 0x5e, 0x01);
	panel_dsi_dcs_send_seq(panel, 0x5f, 0x08);
	panel_dsi_dcs_send_seq(panel, 0x60, 0x02);
	panel_dsi_dcs_send_seq(panel, 0x61, 0x02);
	panel_dsi_dcs_send_seq(panel, 0x62, 0x0a);
	panel_dsi_dcs_send_seq(panel, 0x63, 0x15);
	panel_dsi_dcs_send_seq(panel, 0x64, 0x14);
	panel_dsi_dcs_send_seq(panel, 0x65, 0x02);
	panel_dsi_dcs_send_seq(panel, 0x66, 0x11);
	panel_dsi_dcs_send_seq(panel, 0x67, 0x10);
	panel_dsi_dcs_send_seq(panel, 0x68, 0x02);
	panel_dsi_dcs_send_seq(panel, 0x69, 0x0f);
	panel_dsi_dcs_send_seq(panel, 0x6a, 0x0e);
	panel_dsi_dcs_send_seq(panel, 0x6b, 0x02);
	panel_dsi_dcs_send_seq(panel, 0x6c, 0x0d);
	panel_dsi_dcs_send_seq(panel, 0x6d, 0x0c);
	panel_dsi_dcs_send_seq(panel, 0x6e, 0x06);
	panel_dsi_dcs_send_seq(panel, 0x6f, 0x02);
	panel_dsi_dcs_send_seq(panel, 0x70, 0x02);
	panel_dsi_dcs_send_seq(panel, 0x71, 0x02);
	panel_dsi_dcs_send_seq(panel, 0x72, 0x02);
	panel_dsi_dcs_send_seq(panel, 0x73, 0x02);
	panel_dsi_dcs_send_seq(panel, 0x74, 0x02);

	panel_dsi_dcs_send_seq(panel, 0x75, 0x06);
	panel_dsi_dcs_send_seq(panel, 0x76, 0x02);
	panel_dsi_dcs_send_seq(panel, 0x77, 0x02);
	panel_dsi_dcs_send_seq(panel, 0x78, 0x0a);
	panel_dsi_dcs_send_seq(panel, 0x79, 0x15);
	panel_dsi_dcs_send_seq(panel, 0x7a, 0x14);
	panel_dsi_dcs_send_seq(panel, 0x7b, 0x02);
	panel_dsi_dcs_send_seq(panel, 0x7c, 0x10);
	panel_dsi_dcs_send_seq(panel, 0x7d, 0x11);
	panel_dsi_dcs_send_seq(panel, 0x7e, 0x02);
	panel_dsi_dcs_send_seq(panel, 0x7f, 0x0c);
	panel_dsi_dcs_send_seq(panel, 0x80, 0x0d);
	panel_dsi_dcs_send_seq(panel, 0x81, 0x02);
	panel_dsi_dcs_send_seq(panel, 0x82, 0x0e);
	panel_dsi_dcs_send_seq(panel, 0x83, 0x0f);
	panel_dsi_dcs_send_seq(panel, 0x84, 0x08);
	panel_dsi_dcs_send_seq(panel, 0x85, 0x02);
	panel_dsi_dcs_send_seq(panel, 0x86, 0x02);
	panel_dsi_dcs_send_seq(panel, 0x87, 0x02);
	panel_dsi_dcs_send_seq(panel, 0x88, 0x02);
	panel_dsi_dcs_send_seq(panel, 0x89, 0x02);
	panel_dsi_dcs_send_seq(panel, 0x8a, 0x02);

	/* CMD Page 4 */
	panel_dsi_dcs_send_seq(panel, 0xFF, 0x98, 0x81, 0x04);
	panel_dsi_dcs_send_seq(panel, 0x6c, 0x15);
	panel_dsi_dcs_send_seq(panel, 0x6e, 0x30);
	panel_dsi_dcs_send_seq(panel, 0x6f, 0x33);
	panel_dsi_dcs_send_seq(panel, 0x8d, 0x1f);
	panel_dsi_dcs_send_seq(panel, 0x87, 0xba);
	panel_dsi_dcs_send_seq(panel, 0x26, 0x76);
	panel_dsi_dcs_send_seq(panel, 0xb2, 0xd1);
	panel_dsi_dcs_send_seq(panel, 0x35, 0x1f);
	panel_dsi_dcs_send_seq(panel, 0x33, 0x14);
	panel_dsi_dcs_send_seq(panel, 0x3a, 0xa9);
	panel_dsi_dcs_send_seq(panel, 0x3b, 0x3d);
	panel_dsi_dcs_send_seq(panel, 0x38, 0x01);
	panel_dsi_dcs_send_seq(panel, 0x39, 0x00);

	/* CMD Page 1 */
	panel_dsi_dcs_send_seq(panel, 0xFF, 0x98, 0x81, 0x01);
	panel_dsi_dcs_send_seq(panel, 0x22, 0x0a);
	panel_dsi_dcs_send_seq(panel, 0x31, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x40, 0x53);
	panel_dsi_dcs_send_seq(panel, 0x50, 0xc0);
	panel_dsi_dcs_send_seq(panel, 0x51, 0xc0);
	panel_dsi_dcs_send_seq(panel, 0x53, 0x47);
	panel_dsi_dcs_send_seq(panel, 0x55, 0x46);
	panel_dsi_dcs_send_seq(panel, 0x60, 0x28);
	panel_dsi_dcs_send_seq(panel, 0x2e, 0xc8);

	panel_dsi_dcs_send_seq(panel, 0xa0, 0x01);
	panel_dsi_dcs_send_seq(panel, 0xa1, 0x10);
	panel_dsi_dcs_send_seq(panel, 0xa2, 0x1b);
	panel_dsi_dcs_send_seq(panel, 0xa3, 0x0c);
	panel_dsi_dcs_send_seq(panel, 0xa4, 0x14);
	panel_dsi_dcs_send_seq(panel, 0xa5, 0x25);
	panel_dsi_dcs_send_seq(panel, 0xa6, 0x1a);
	panel_dsi_dcs_send_seq(panel, 0xa7, 0x1d);
	panel_dsi_dcs_send_seq(panel, 0xa8, 0x6b);
	panel_dsi_dcs_send_seq(panel, 0xa9, 0x1b);
	panel_dsi_dcs_send_seq(panel, 0xaa, 0x26);
	panel_dsi_dcs_send_seq(panel, 0xab, 0x5b);
	panel_dsi_dcs_send_seq(panel, 0xac, 0x1b);
	panel_dsi_dcs_send_seq(panel, 0xad, 0x1c);
	panel_dsi_dcs_send_seq(panel, 0xae, 0x4f);
	panel_dsi_dcs_send_seq(panel, 0xaf, 0x24);
	panel_dsi_dcs_send_seq(panel, 0xb0, 0x2a);
	panel_dsi_dcs_send_seq(panel, 0xb1, 0x4e);
	panel_dsi_dcs_send_seq(panel, 0xb2, 0x5f);
	panel_dsi_dcs_send_seq(panel, 0xb3, 0x39);

	panel_dsi_dcs_send_seq(panel, 0xc0, 0x0f);
	panel_dsi_dcs_send_seq(panel, 0xc1, 0x1b);
	panel_dsi_dcs_send_seq(panel, 0xc2, 0x27);
	panel_dsi_dcs_send_seq(panel, 0xc3, 0x16);
	panel_dsi_dcs_send_seq(panel, 0xc4, 0x14);
	panel_dsi_dcs_send_seq(panel, 0xc5, 0x2b);
	panel_dsi_dcs_send_seq(panel, 0xc6, 0x1d);
	panel_dsi_dcs_send_seq(panel, 0xc7, 0x21);
	panel_dsi_dcs_send_seq(panel, 0xc8, 0x6c);
	panel_dsi_dcs_send_seq(panel, 0xc9, 0x1b);
	panel_dsi_dcs_send_seq(panel, 0xca, 0x26);
	panel_dsi_dcs_send_seq(panel, 0xcb, 0x5b);
	panel_dsi_dcs_send_seq(panel, 0xcc, 0x1b);
	panel_dsi_dcs_send_seq(panel, 0xcd, 0x1b);
	panel_dsi_dcs_send_seq(panel, 0xce, 0x4f);
	panel_dsi_dcs_send_seq(panel, 0xcf, 0x24);
	panel_dsi_dcs_send_seq(panel, 0xd0, 0x2a);
	panel_dsi_dcs_send_seq(panel, 0xd1, 0x4e);
	panel_dsi_dcs_send_seq(panel, 0xd2, 0x5f);
	panel_dsi_dcs_send_seq(panel, 0xd3, 0x39);

	panel_dsi_dcs_send_seq(panel, 0xFF, 0x98, 0x81, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x35, 0x00);

	ret = panel_dsi_dcs_exit_sleep_mode(panel);
	if (ret < 0) {
		pr_err("Failed to exit sleep mode: %d\n", ret);
		return ret;
	}

	aic_delay_ms(120);

	ret = panel_dsi_dcs_set_display_on(panel);
	if (ret < 0) {
		pr_err("Failed to set display on: %d\n", ret);
		return ret;
	}

	aic_delay_ms(20);

	panel_dsi_setup_realmode(panel);
	panel_de_timing_enable(panel, 0);
	panel_backlight_enable(panel, 0);
	return 0;
}

static struct aic_panel_funcs panel_funcs = {
	.prepare = panel_default_prepare,
	.enable = panel_enable,
	.get_video_mode = panel_default_get_video_mode,
	.register_callback = panel_register_callback,
};

/* Init the videomode parameter, dts will override the initial value. */
static struct fb_videomode panel_vm = {
	.pixclock = 70000000,
	.xres = 800,
	.right_margin = 100,
	.left_margin = 48,
	.hsync_len = 8,
	.yres = 1280,
	.lower_margin = 16,
	.upper_margin = 15,
	.vsync_len = 6,
	.flag = DISPLAY_FLAGS_HSYNC_LOW | DISPLAY_FLAGS_VSYNC_LOW |
		DISPLAY_FLAGS_DE_HIGH | DISPLAY_FLAGS_PIXDATA_POSEDGE
};

static struct panel_dsi dsi = {
	.format = DSI_FMT_RGB888,
	.mode = DSI_MOD_VID_BURST,
	.lane_num = 4,
};

static int panel_probe(struct udevice *dev)
{
	struct panel_priv *priv = dev_get_priv(dev);
	struct ili9881c *ili9881c;
	int ret;

	ili9881c = malloc(sizeof(*ili9881c));
	if (!ili9881c)
		return -ENOMEM;

	if (panel_parse_dts(dev) < 0) {
		free(ili9881c);
		return -1;
	}

	ret = gpio_request_by_name(dev, "reset-gpios", 0, &ili9881c->reset,
				GPIOD_IS_OUT);
	if (ret) {
		dev_err(dev, "Failed to get reset gpio\n");
		return ret;
	}
	dm_gpio_set_value(&ili9881c->reset, 0);

	priv->panel.dsi = &dsi;
	panel_init(priv, dev, &panel_vm, &panel_funcs, ili9881c);

	return 0;
}

static const struct udevice_id panel_match_ids[] = {
	{.compatible = "artinchip,aic-dsi-panel-simple"},
	{ /* sentinel */}
};

U_BOOT_DRIVER(panel_dsi_ili9881c) = {
	.name      = PANEL_DEV_NAME,
	.id        = UCLASS_PANEL,
	.of_match  = panel_match_ids,
	.probe     = panel_probe,
	.priv_auto = sizeof(struct panel_priv),
};
