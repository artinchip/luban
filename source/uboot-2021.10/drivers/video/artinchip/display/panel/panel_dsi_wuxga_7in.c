// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for DSI wuga panel.
 *
 * Copyright (C) 2020-2021 ArtInChip Technology Co., Ltd.
 * Authors:  Huahui Mai <huahui.mai@artinchip.com>
 */

#include <common.h>
#include <dm.h>
#include <dm/uclass-internal.h>
#include <dm/device_compat.h>
#include <mipi_display.h>
#include <video.h>
#include <panel.h>

#include "panel_dsi.h"

#define PANEL_DEV_NAME		"dsi_panel_wuxga_7in"

struct wuxga {
	struct gpio_desc dcdc_en;
	struct gpio_desc reset;
};

static inline struct wuxga *panel_to_wuxga(struct aic_panel *panel)
{
	return (struct wuxga *)panel->panel_private;
}

static void panel_gpio_init(struct aic_panel *panel)
{
	struct wuxga *wuxga = panel_to_wuxga(panel);

	aic_delay_ms(10);
	dm_gpio_set_value(&wuxga->dcdc_en, 1);
	aic_delay_ms(20);
	dm_gpio_set_value(&wuxga->dcdc_en, 1);
	aic_delay_ms(10);
	dm_gpio_set_value(&wuxga->dcdc_en, 0);
	aic_delay_ms(120);
	dm_gpio_set_value(&wuxga->dcdc_en, 1);
	aic_delay_ms(20);
}

static int panel_enable(struct aic_panel *panel)
{
	enum dsi_mode mode = panel->dsi->mode;
	int ret;


	panel_gpio_init(panel);

	panel_di_enable(panel, 0);
	panel_dsi_send_perpare(panel);

	panel_dsi_dcs_send_seq(panel, MIPI_DCS_SOFT_RESET);
	aic_delay_ms(5);

	panel_dsi_dcs_send_seq(panel, 0xb0, 0x00);
	panel_dsi_dcs_send_seq(panel, 0xb3, 0x04, 0x08, 0x00, 0x22, 0x00);
	panel_dsi_dcs_send_seq(panel, 0xb4, 0x0c);
	panel_dsi_dcs_send_seq(panel, 0xb6, 0x3a, 0xd3);
	panel_dsi_dcs_send_seq(panel, 0x51, 0xE6);
	panel_dsi_dcs_send_seq(panel, 0x53, 0x2c);
	panel_dsi_dcs_send_seq(panel, MIPI_DCS_SET_PIXEL_FORMAT, 0x77);
	panel_dsi_dcs_send_seq(panel, MIPI_DCS_SET_ADDRESS_MODE, 0x00);
	panel_dsi_dcs_send_seq(panel, MIPI_DCS_SET_COLUMN_ADDRESS, 0x00, 0x00, 0x04, 0xaf);
	panel_dsi_dcs_send_seq(panel, MIPI_DCS_SET_PAGE_ADDRESS, 0x00, 0x00, 0x07, 0x7f);

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

	if (mode == DSI_MOD_CMD_MODE)
		panel_dsi_dcs_send_seq(panel, DSI_DCS_SET_TEAR_ON, 0x00);
	else
		panel_dsi_dcs_send_seq(panel, 0xb3, 0x14, 0x08, 0x00, 0x22, 0x00);

	aic_delay_ms(120);
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
	.pixclock = 150000000,
	.xres = 1200,
	.right_margin = 200,
	.left_margin = 200,
	.hsync_len = 100,
	.yres = 1920,
	.lower_margin = 3,
	.upper_margin = 6,
	.vsync_len = 2,
	.flag = DISPLAY_FLAGS_HSYNC_LOW | DISPLAY_FLAGS_VSYNC_LOW |
		DISPLAY_FLAGS_DE_HIGH | DISPLAY_FLAGS_PIXDATA_POSEDGE
};

static struct panel_dsi dsi = {
	.format = DSI_FMT_RGB888,
	.mode = DSI_MOD_VID_PULSE,
	.lane_num = 4,
};

static int panel_probe(struct udevice *dev)
{
	struct panel_priv *priv = dev_get_priv(dev);
	struct wuxga *wuxga;
	int ret;

	wuxga = malloc(sizeof(*wuxga));
	if (!wuxga)
		return -ENOMEM;

	if (panel_parse_dts(dev) < 0) {
		free(wuxga);
		return -1;
	}

	ret = gpio_request_by_name(dev, "dcdc-en-gpios", 0, &wuxga->dcdc_en,
				GPIOD_IS_OUT);
	if (ret) {
		dev_err(dev, "Failed to get dcdc_en gpio\n");
		return ret;
	}

	ret = gpio_request_by_name(dev, "reset-gpios", 0, &wuxga->reset,
				GPIOD_IS_OUT);
	if (ret) {
		dev_err(dev, "Failed to get reset gpio\n");
		return ret;
	}

	dm_gpio_set_value(&wuxga->dcdc_en, 0);
	dm_gpio_set_value(&wuxga->reset, 0);

	priv->panel.dsi = &dsi;
	panel_init(priv, dev, &panel_vm, &panel_funcs, wuxga);

	return 0;
}

static const struct udevice_id panel_match_ids[] = {
	{.compatible = "artinchip,aic-dsi-panel-simple"},
	{ /* sentinel */}
};

U_BOOT_DRIVER(panel_dsi_wuxga_7in) = {
	.name      = PANEL_DEV_NAME,
	.id        = UCLASS_PANEL,
	.of_match  = panel_match_ids,
	.probe     = panel_probe,
	.priv_auto = sizeof(struct panel_priv),
};

