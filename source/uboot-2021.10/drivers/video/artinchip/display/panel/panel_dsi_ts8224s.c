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

#define PANEL_DEV_NAME "dsi_panel_ts8824s"

struct ts8224s{
	struct gpio_desc reset;
};

static inline struct ts8224s *panel_to_ts8224s(struct aic_panel *panel)
{
	return (struct ts8224s *)panel->panel_private;
}

static int panel_enable(struct aic_panel *panel)
{
	struct ts8224s *ts8224s = panel_to_ts8224s(panel);
	dm_gpio_set_value(&ts8224s->reset, 1);
	aic_delay_ms(1);
	dm_gpio_set_value(&ts8224s->reset, 0);
	aic_delay_ms(10);
	dm_gpio_set_value(&ts8224s->reset, 1);
	aic_delay_ms(120);

	panel_di_enable(panel, 0);
	panel_dsi_send_perpare(panel);

	aic_delay_ms(120);

	panel_dsi_generic_send_seq(panel, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x10);
	panel_dsi_generic_send_seq(panel, 0xC0, 0x63, 0x00);
	panel_dsi_generic_send_seq(panel, 0xC1, 0x0A, 0x02);
	panel_dsi_generic_send_seq(panel, 0xC2, 0x31, 0x06);
	panel_dsi_generic_send_seq(panel, 0xCC, 0x10);
	panel_dsi_generic_send_seq(panel, 0xB0, 0x00, 0x06, 0x12, 0x0D, 0x10, 0x06, 0x06, 0x08,
			0x08, 0x27, 0x07, 0x14, 0x13, 0x30, 0x35, 0x18);
	panel_dsi_generic_send_seq(panel, 0xB1, 0x00, 0x06, 0x12, 0x0D, 0x10, 0x05, 0x07, 0x09,
			0x09, 0x27, 0x06, 0x14, 0x13, 0x31, 0x35, 0x18);
	panel_dsi_generic_send_seq(panel, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x11);
	panel_dsi_generic_send_seq(panel, 0xB0, 0x5D);
	panel_dsi_generic_send_seq(panel, 0xB1, 0x2B);
	panel_dsi_generic_send_seq(panel, 0xB2, 0x87);
	panel_dsi_generic_send_seq(panel, 0xB3, 0x80);
	panel_dsi_generic_send_seq(panel, 0xB5, 0x47);
	panel_dsi_generic_send_seq(panel, 0xB7, 0x85);
	panel_dsi_generic_send_seq(panel, 0xB8, 0x20);
	panel_dsi_generic_send_seq(panel, 0xB9, 0x10);
	panel_dsi_generic_send_seq(panel, 0xC1, 0x78);
	panel_dsi_generic_send_seq(panel, 0xC2, 0x78);
	panel_dsi_generic_send_seq(panel, 0xD0, 0x88);
	aic_delay_ms(100);
	panel_dsi_generic_send_seq(panel, 0xE0, 0x00, 0x00, 0x02);
	panel_dsi_generic_send_seq(panel, 0xE1, 0x04, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
			0x00, 0x20, 0x20);
	panel_dsi_generic_send_seq(panel, 0xE2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00);
	panel_dsi_generic_send_seq(panel, 0xE3, 0x00, 0x00, 0x33, 0x00);
	panel_dsi_generic_send_seq(panel, 0xE4, 0x22, 0x00);
	panel_dsi_generic_send_seq(panel, 0xE5, 0x04, 0x5C, 0xA0, 0xA0, 0x06, 0x5C, 0xA0, 0xA0,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	panel_dsi_generic_send_seq(panel, 0xE6, 0x00, 0x00, 0x33, 0x00);
	panel_dsi_generic_send_seq(panel, 0xE7, 0x22, 0x00);
	panel_dsi_generic_send_seq(panel, 0xE8, 0x05, 0x5C, 0xA0, 0xA0, 0x07, 0x5C, 0xA0, 0xA0,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	panel_dsi_generic_send_seq(panel, 0xEB, 0x02, 0x00, 0x40, 0x40, 0x00, 0x00, 0x00);
	panel_dsi_generic_send_seq(panel, 0xEC, 0x00, 0x00);
	panel_dsi_generic_send_seq(panel, 0xED, 0xFA, 0x45, 0x0B, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xB0, 0x54, 0xAF);
	panel_dsi_generic_send_seq(panel, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x00);
	aic_delay_ms(120);

	panel_dsi_dcs_send_seq(panel, 0x11);
	aic_delay_ms(20);
	panel_dsi_dcs_send_seq(panel, 0x29);

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
	.pixclock = 40000000,
	.xres = 480,
	.right_margin = 160,
	.left_margin = 160,
	.hsync_len = 20,
	.yres = 800,
	.lower_margin = 40,
	.upper_margin = 40,
	.vsync_len = 5,
	.flag = DISPLAY_FLAGS_HSYNC_LOW | DISPLAY_FLAGS_VSYNC_LOW |
			DISPLAY_FLAGS_DE_HIGH | DISPLAY_FLAGS_PIXDATA_POSEDGE};

static struct panel_dsi dsi = {
	.format = DSI_FMT_RGB888,
	.mode = DSI_MOD_VID_BURST,
	.lane_num = 2,
};

static int panel_probe(struct udevice *dev)
{
	struct panel_priv *priv = dev_get_priv(dev);
	struct ts8224s *ts8224s;
	int ret;

	ts8224s = malloc(sizeof(*ts8224s));
	if (!ts8224s)
		return -ENOMEM;

	if (panel_parse_dts(dev) < 0) {
		free(ts8224s);
		return -1;
	}

	ret = gpio_request_by_name(dev, "reset-gpios", 0, &ts8224s->reset,
							   GPIOD_IS_OUT);
	if (ret) {
		dev_err(dev, "Failed to get reset gpio\n");
		return ret;
	}
	dm_gpio_set_value(&ts8224s->reset, 0);

	priv->panel.dsi = &dsi;
	panel_init(priv, dev, &panel_vm, &panel_funcs, ts8224s);

	return 0;
}

static const struct udevice_id panel_match_ids[] = {
	{.compatible = "artinchip,aic-dsi-panel-simple"},
	{/* sentinel */}};

U_BOOT_DRIVER(panel_dsi_ts8224s) = {
	.name = PANEL_DEV_NAME,
	.id = UCLASS_PANEL,
	.of_match = panel_match_ids,
	.probe = panel_probe,
	.priv_auto = sizeof(struct panel_priv),
};
