// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for s070wv20 DSI panel.
 *
 * Copyright (C) 2023 ArtInChip Technology Co., Ltd.
 * Authors: huahui.mai <huahui.ami@artinchip.com>
 */

#include <common.h>
#include <dm.h>
#include <dm/uclass-internal.h>
#include <dm/device_compat.h>
#include <video.h>
#include <panel.h>

#include "panel_dsi.h"

#define PANEL_DEV_NAME		"dsi_panel_s070wv20"

struct s070wv20 {
	struct gpio_desc power;
	struct gpio_desc reset;
};

static inline struct s070wv20 *panel_to_s070wv20(struct aic_panel *panel)
{
	return (struct s070wv20 *)panel->panel_private;
}

static int panel_enable(struct aic_panel *panel)
{
	struct s070wv20 *s070wv20 = panel_to_s070wv20(panel);
	int ret;

	dm_gpio_set_value(&s070wv20->reset, 1);
	aic_delay_ms(120);
	dm_gpio_set_value(&s070wv20->reset, 0);
	aic_delay_ms(120);
	dm_gpio_set_value(&s070wv20->reset, 1);
	aic_delay_ms(120);

	panel_di_enable(panel, 0);
	panel_dsi_send_perpare(panel);

	panel_dsi_generic_send_seq(panel, 0x7A, 0xC1);
	panel_dsi_generic_send_seq(panel, 0x20, 0x20);
	panel_dsi_generic_send_seq(panel, 0x21, 0xE0);
	panel_dsi_generic_send_seq(panel, 0x22, 0x13);
	panel_dsi_generic_send_seq(panel, 0x23, 0x28);
	panel_dsi_generic_send_seq(panel, 0x24, 0x30);
	panel_dsi_generic_send_seq(panel, 0x25, 0x28);
	panel_dsi_generic_send_seq(panel, 0x26, 0x00);
	panel_dsi_generic_send_seq(panel, 0x27, 0x0D);
	panel_dsi_generic_send_seq(panel, 0x28, 0x03);
	panel_dsi_generic_send_seq(panel, 0x29, 0x1D);
	panel_dsi_generic_send_seq(panel, 0x34, 0x80);
	panel_dsi_generic_send_seq(panel, 0x36, 0x28);
	panel_dsi_generic_send_seq(panel, 0xB5, 0xA0);
	panel_dsi_generic_send_seq(panel, 0x5C, 0xFF);
	panel_dsi_generic_send_seq(panel, 0x2A, 0x01);
	panel_dsi_generic_send_seq(panel, 0x56, 0x92);
	panel_dsi_generic_send_seq(panel, 0x6B, 0x71);
	panel_dsi_generic_send_seq(panel, 0x69, 0x2B);
	panel_dsi_generic_send_seq(panel, 0x10, 0x40);
	panel_dsi_generic_send_seq(panel, 0x11, 0x98);
	panel_dsi_generic_send_seq(panel, 0xB6, 0x20);
	panel_dsi_generic_send_seq(panel, 0x51, 0x20);

#ifdef BIST
	panel_dsi_generic_send_seq(panel, 0x14, 0x43);
	panel_dsi_generic_send_seq(panel, 0x2A, 0x49);
#endif

	panel_dsi_generic_send_seq(panel, 0x09, 0x10);

	ret = panel_dsi_dcs_exit_sleep_mode(panel);
	if (ret < 0) {
		pr_err("Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	aic_delay_ms(200);

	ret = panel_dsi_dcs_set_display_on(panel);
	if (ret < 0) {
		pr_err("Failed to set display on: %d\n", ret);
		return ret;
	}
	aic_delay_ms(200);

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
	.right_margin = 200,
	.left_margin = 200,
	.hsync_len = 48,
	.yres = 480,
	.lower_margin = 23,
	.upper_margin = 49,
	.vsync_len = 3,
	.flag = DISPLAY_FLAGS_HSYNC_HIGH | DISPLAY_FLAGS_VSYNC_HIGH |
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
	struct s070wv20 *s070wv20;
	int ret;

	if (panel_parse_dts(dev) < 0)
		return -1;

	s070wv20 = malloc(sizeof(*s070wv20));
	if (!s070wv20)
		return -ENOMEM;

	ret = gpio_request_by_name(dev, "power-gpios", 0,
					&s070wv20->power, GPIOD_IS_OUT);
	if (ret) {
		dev_err(dev, "failed to get power gpio\n");
		free(s070wv20);
		return ret;
	}

	ret = gpio_request_by_name(dev, "reset-gpios", 0,
					&s070wv20->reset, GPIOD_IS_OUT);
	if (ret) {
		dev_err(dev, "failed to get reset gpio\n");
		free(s070wv20);
		return ret;
	}

	priv->panel.dsi = &dsi;
	panel_init(priv, dev, &panel_vm, &panel_funcs, s070wv20);

	return 0;
}

static const struct udevice_id panel_match_ids[] = {
	{.compatible = "artinchip,aic-dsi-panel-simple"},
	{ /* sentinel */}
};

U_BOOT_DRIVER(panel_dsi_s070wv20) = {
	.name      = PANEL_DEV_NAME,
	.id        = UCLASS_PANEL,
	.of_match  = panel_match_ids,
	.probe     = panel_probe,
	.priv_auto = sizeof(struct panel_priv),
};
