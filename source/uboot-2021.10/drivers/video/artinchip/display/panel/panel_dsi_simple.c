// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for simple DSI panel.
 *
 * Copyright (C) 2020-2024 ArtInChip Technology Co., Ltd.
 * Authors: huahui.mai <huahui.ami@artinchip.com>
 */

#include <common.h>
#include <dm.h>
#include <dm/uclass-internal.h>
#include <dm/device_compat.h>
#include <video.h>
#include <panel.h>

#include "panel_dsi.h"

#define PANEL_DEV_NAME		"dsi_panel_simple"

struct simple {
	struct gpio_desc *reset;
};

static inline struct simple *panel_to_simple(struct aic_panel *panel)
{
	return (struct simple *)panel->panel_private;
}

static inline int gpiod_direction_output(struct gpio_desc *reset, u32 value)
{
	return	dm_gpio_set_value(reset, value);
}

static int panel_enable(struct aic_panel *panel)
{
	struct simple *simple = panel_to_simple(panel);

	if (simple->reset) {
		gpiod_direction_output(simple->reset, 1);
		aic_delay_ms(10);
		gpiod_direction_output(simple->reset, 0);
		aic_delay_ms(120);
		gpiod_direction_output(simple->reset, 1);
		aic_delay_ms(120);
	}

	panel_di_enable(panel, 0);
	panel_dsi_send_perpare(panel);

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
	struct simple *simple;
	int ret;

	if (panel_parse_dts(dev) < 0)
		return -1;

	simple = malloc(sizeof(*simple));
	if (!simple)
		return -ENOMEM;

	simple->reset = malloc(sizeof(*simple));
	if (!simple->reset) {
		free(simple);
		return -ENOMEM;
	}

	ret = gpio_request_by_name(dev, "reset-gpios", 0,
					simple->reset, GPIOD_IS_OUT);
	if (ret) {
		dev_dbg(dev, "Not found reset gpio\n");
		simple->reset = NULL;
	}

	priv->panel.dsi = &dsi;
	panel_init(priv, dev, &panel_vm, &panel_funcs, simple);
	return 0;
}

static const struct udevice_id panel_match_ids[] = {
	{.compatible = "artinchip,aic-dsi-panel-simple"},
	{ /* sentinel */}
};

U_BOOT_DRIVER(panel_dsi_simple) = {
	.name      = PANEL_DEV_NAME,
	.id        = UCLASS_PANEL,
	.of_match  = panel_match_ids,
	.probe     = panel_probe,
	.priv_auto = sizeof(struct panel_priv),
};

