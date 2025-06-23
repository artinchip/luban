// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for sc7707 DSI panel.
 *
 * Copyright (C) 2024 ArtInChip Technology Co., Ltd.
 * Authors: huahui.mai <huahui.ami@artinchip.com>
 *	    keliang.liu <keliang.liu@artinchip.com>
 */

#include <common.h>
#include <dm.h>
#include <dm/uclass-internal.h>
#include <dm/device_compat.h>
#include <video.h>
#include <panel.h>

#include "panel_dsi.h"

#define PANEL_DEV_NAME		"dsi_panel_sc7707"

struct sc7707 {
	struct gpio_desc power;
	struct gpio_desc reset;
};

static inline struct sc7707 *panel_to_sc7707(struct aic_panel *panel)
{
	return (struct sc7707 *)panel->panel_private;
}

static int panel_enable(struct aic_panel *panel)
{
	struct sc7707 *sc7707 = panel_to_sc7707(panel);
	int ret;

	dm_gpio_set_value(&sc7707->reset, 1);
	aic_delay_ms(120);
	dm_gpio_set_value(&sc7707->power, 1);
	aic_delay_ms(120);

	panel_di_enable(panel, 0);
	panel_dsi_send_perpare(panel);

	panel_dsi_generic_send_seq(panel, 0xB9, 0xF1, 0x12, 0x84);

	panel_dsi_generic_send_seq(panel, 0xB1, 0x22, 0x54, 0x23, 0x32, 0x32,
				   0x33, 0x77, 0x04, 0xDB, 0x0C);

	panel_dsi_generic_send_seq(panel, 0xB2, 0x86, 0x0A);

	panel_dsi_generic_send_seq(panel, 0xB3, 0x00, 0x00, 0x00, 0x00, 0x28,
				   0x28, 0x28, 0x28);

	panel_dsi_generic_send_seq(panel, 0xB4, 0x80);

	panel_dsi_generic_send_seq(panel, 0xB6, 0x95, 0x95);

	panel_dsi_generic_send_seq(panel, 0xB8, 0xA6, 0x23);

	panel_dsi_generic_send_seq(panel, 0xBA, 0x33, 0x81, 0x05, 0xF9, 0x0E,
				   0x0E, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
				   0x00, 0x00, 0x44, 0x25, 0x00, 0x90, 0x0A,
				   0x00, 0x00, 0x01, 0x4F, 0x01, 0x00, 0x00,
				   0x37);

	panel_dsi_generic_send_seq(panel, 0xBC, 0x47);

	panel_dsi_generic_send_seq(panel, 0xBF, 0x02, 0x11, 0x00);

	panel_dsi_generic_send_seq(panel, 0xC0, 0x73, 0x73, 0x50, 0x50, 0x80,
				   0x00, 0x12, 0x70, 0x00);

	panel_dsi_generic_send_seq(panel, 0xC7, 0xB8, 0x00, 0x02, 0x04, 0x1A,
				   0x04, 0x32, 0x01, 0x01);

	panel_dsi_generic_send_seq(panel, 0xCC, 0x0B);

	panel_dsi_generic_send_seq(panel, 0xE0, 0x00, 0x0B, 0x10, 0x34, 0x3F,
				   0x3F, 0x46, 0x3B, 0x07, 0x0D, 0x0D, 0x10,
				   0x12, 0x10, 0x12, 0x10, 0x13, 0x00, 0x0B,
				   0x10, 0x34, 0x3F, 0x3F, 0x46, 0x3B, 0x07,
				   0x0D, 0x0D, 0x10, 0x12, 0x10, 0x12, 0x10,
				   0x13);

	panel_dsi_generic_send_seq(panel, 0xE3, 0x07, 0x07, 0x0B, 0x0B, 0x0B,
				   0x0B, 0x00, 0x00, 0x00, 0xC0, 0x10);

	panel_dsi_generic_send_seq(panel, 0xE9, 0x02, 0x00, 0x01, 0x06, 0x1F,
				   0x80, 0x81, 0x12, 0x31, 0x23, 0x6F, 0x03,
				   0x80, 0x81, 0x47, 0x08, 0x00, 0x60, 0x1C,
				   0x00, 0x00, 0x00, 0x00, 0x60, 0x1C, 0x00,
				   0x00, 0x00, 0x88, 0x55, 0x47, 0x65, 0x43,
				   0x21, 0x00, 0x18, 0x88, 0x88, 0x88, 0x88,
				   0x55, 0x47, 0x65, 0x43, 0x21, 0x00, 0x18,
				   0x88, 0x88, 0x88, 0x00, 0x00, 0x00, 0x00,
				   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				   0x00, 0x00);

	panel_dsi_generic_send_seq(panel, 0xEA, 0x00, 0x00, 0x00, 0x00, 0x00,
				   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				   0x88, 0x50, 0x14, 0x56, 0x70, 0x12, 0x35,
				   0x48, 0x88, 0x88, 0x88, 0x88, 0x50, 0x14,
				   0x56, 0x70, 0x12, 0x35, 0x48, 0x88, 0x88,
				   0x88, 0x23, 0x00, 0x00, 0x00, 0x00, 0x40,
				   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				   0x60, 0x80, 0x81, 0x00, 0x00, 0x00, 0x00,
				   0x01, 0x09);

	ret = panel_dsi_dcs_exit_sleep_mode(panel);
	if (ret < 0) {
		pr_err("Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	aic_delay_ms(250);

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
	.pixclock = 84000000,
	.xres = 720,
	.right_margin = 22,
	.left_margin = 28,
	.hsync_len = 10,
	.yres = 1560,
	.lower_margin = 13,
	.upper_margin = 13,
	.vsync_len = 3,
	.flag = DISPLAY_FLAGS_HSYNC_HIGH | DISPLAY_FLAGS_VSYNC_HIGH |
		DISPLAY_FLAGS_DE_HIGH | DISPLAY_FLAGS_PIXDATA_POSEDGE
};

static struct panel_dsi dsi = {
	.format = DSI_FMT_RGB888,
	.mode = DSI_MOD_VID_BURST | DSI_CLOCK_NON_CONTINUOUS,
	.lane_num = 4,
};

static int panel_probe(struct udevice *dev)
{
	struct panel_priv *priv = dev_get_priv(dev);
	struct sc7707 *sc7707;
	int ret;

	if (panel_parse_dts(dev) < 0)
		return -1;

	sc7707 = malloc(sizeof(*sc7707));
	if (!sc7707)
		return -ENOMEM;

	ret = gpio_request_by_name(dev, "power-gpios", 0,
					&sc7707->power, GPIOD_IS_OUT);
	if (ret) {
		dev_err(dev, "failed to get power gpio\n");
		free(sc7707);
		return ret;
	}

	ret = gpio_request_by_name(dev, "reset-gpios", 0,
					&sc7707->reset, GPIOD_IS_OUT);
	if (ret) {
		dev_err(dev, "failed to get reset gpio\n");
		free(sc7707);
		return ret;
	}

	priv->panel.dsi = &dsi;
	panel_init(priv, dev, &panel_vm, &panel_funcs, sc7707);

	return 0;
}

static const struct udevice_id panel_match_ids[] = {
	{.compatible = "artinchip,aic-dsi-panel-simple"},
	{ /* sentinel */}
};

U_BOOT_DRIVER(panel_dsi_sc7707) = {
	.name      = PANEL_DEV_NAME,
	.id        = UCLASS_PANEL,
	.of_match  = panel_match_ids,
	.probe     = panel_probe,
	.priv_auto = sizeof(struct panel_priv),
};
