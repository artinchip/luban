// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for s070wv20 DSI panel.
 *
 * Copyright (C) 2023 ArtInChip Technology Co., Ltd.
 * Authors: huahui.mai <huahui.ami@artinchip.com>
 */

#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/component.h>
#include <linux/platform_device.h>
#include <video/display_timing.h>

#include "panel_dsi.h"

#define PANEL_DEV_NAME		"dsi_panel_s070wv20"

struct s070wv20 {
	struct gpio_desc *power;
	struct gpio_desc *reset;
};

static inline struct s070wv20 *panel_to_s070wv20(struct aic_panel *panel)
{
	return (struct s070wv20 *)panel->panel_private;
}

static int panel_enable(struct aic_panel *panel)
{
	struct s070wv20 *s070wv20 = panel_to_s070wv20(panel);
	int ret;

	gpiod_direction_output(s070wv20->reset, 1);
	aic_delay_ms(120);
	gpiod_direction_output(s070wv20->reset, 0);
	aic_delay_ms(120);
	gpiod_direction_output(s070wv20->reset, 1);
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

static int panel_disable(struct aic_panel *panel)
{
	struct s070wv20 *s070wv20 = panel_to_s070wv20(panel);

	gpiod_direction_output(s070wv20->reset, 0);
	aic_delay_ms(120);
	gpiod_direction_output(s070wv20->power, 0);
	aic_delay_ms(120);

	return panel_default_disable(panel);
}

static struct aic_panel_funcs panel_funcs = {
	.disable = panel_disable,
	.unprepare = panel_default_unprepare,
	.prepare = panel_default_prepare,
	.enable = panel_enable,
	.get_video_mode = panel_default_get_video_mode,
	.register_callback = panel_register_callback,
};

/* Init the videomode parameter, dts will override the initial value. */
static struct videomode panel_vm = {
	.pixelclock = 70000000,
	.hactive = 800,
	.hfront_porch = 200,
	.hback_porch = 200,
	.hsync_len = 48,
	.vactive = 480,
	.vfront_porch = 23,
	.vback_porch = 49,
	.vsync_len = 3,
	.flags = DISPLAY_FLAGS_HSYNC_HIGH | DISPLAY_FLAGS_VSYNC_HIGH |
		DISPLAY_FLAGS_DE_HIGH | DISPLAY_FLAGS_PIXDATA_POSEDGE
};

static struct panel_dsi dsi = {
	.format = DSI_FMT_RGB888,
	.mode = DSI_MOD_VID_BURST,
	.lane_num = 4,
};

static int panel_bind(struct device *dev, struct device *master, void *data)
{
	struct panel_comp *p;
	struct s070wv20 *s070wv20;

	p = devm_kzalloc(dev, sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	s070wv20 = devm_kzalloc(dev, sizeof(*s070wv20), GFP_KERNEL);
	if (!s070wv20)
		return -ENOMEM;

	if (panel_parse_dts(p, dev) < 0)
		return -1;

	s070wv20->power = devm_gpiod_get(dev, "power", GPIOD_ASIS);
	if (IS_ERR(s070wv20->power)) {
		dev_err(dev, "failed to get power gpio\n");
		return PTR_ERR(s070wv20->power);
	}

	s070wv20->reset = devm_gpiod_get(dev, "reset", GPIOD_ASIS);
	if (IS_ERR(s070wv20->reset)) {
		dev_err(dev, "failed to get reset gpio\n");
		return PTR_ERR(s070wv20->reset);
	}

	p->panel.dsi = &dsi;
	panel_init(p, dev, &panel_vm, &panel_funcs, s070wv20);

	dev_set_drvdata(dev, p);
	return 0;
}

static const struct component_ops panel_com_ops = {
	.bind	= panel_bind,
	.unbind	= panel_default_unbind,
};

static int panel_probe(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s()\n", __func__);
	return component_add(&pdev->dev, &panel_com_ops);
}

static int panel_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &panel_com_ops);
	return 0;
}

static const struct of_device_id panal_of_table[] = {
	{.compatible = "artinchip,aic-dsi-panel-simple"},
	{ /* sentinel */}
};
MODULE_DEVICE_TABLE(of, panal_of_table);

static struct platform_driver panel_driver = {
	.probe = panel_probe,
	.remove = panel_remove,
	.driver = {
		.name = PANEL_DEV_NAME,
		.of_match_table = panal_of_table,
	},
};

module_platform_driver(panel_driver);

MODULE_AUTHOR("huahui.mai <huahui.mai@artinchip.com>");
MODULE_DESCRIPTION("AIC-" PANEL_DEV_NAME);
MODULE_LICENSE("GPL");
