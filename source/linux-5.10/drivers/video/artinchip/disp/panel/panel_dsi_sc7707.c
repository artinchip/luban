// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for sc7707 DSI panel.
 *
 * Copyright (C) 2024 ArtInChip Technology Co., Ltd.
 * Authors: huahui.mai <huahui.ami@artinchip.com>
 *	    keliang.liu <keliang.liu@artinchip.com>
 */

#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/component.h>
#include <linux/platform_device.h>
#include <video/display_timing.h>

#include "panel_dsi.h"

#define PANEL_DEV_NAME		"dsi_panel_sc7707"

struct sc7707 {
	struct gpio_desc *power;
	struct gpio_desc *reset;
};

static inline struct sc7707 *panel_to_sc7707(struct aic_panel *panel)
{
	return (struct sc7707 *)panel->panel_private;
}

static int panel_enable(struct aic_panel *panel)
{
	struct sc7707 *sc7707 = panel_to_sc7707(panel);
	int ret;

	gpiod_direction_output(sc7707->reset, 1);
	aic_delay_ms(120);
	gpiod_direction_output(sc7707->power, 1);
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

static int panel_disable(struct aic_panel *panel)
{
	struct sc7707 *sc7707 = panel_to_sc7707(panel);

	gpiod_direction_output(sc7707->reset, 0);
	aic_delay_ms(120);
	gpiod_direction_output(sc7707->power, 0);
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
	.pixelclock = 84000000,
	.hactive = 720,
	.hfront_porch = 22,
	.hback_porch = 28,
	.hsync_len = 10,
	.vactive = 1560,
	.vfront_porch = 13,
	.vback_porch = 13,
	.vsync_len = 3,
	.flags = DISPLAY_FLAGS_HSYNC_HIGH | DISPLAY_FLAGS_VSYNC_HIGH |
		DISPLAY_FLAGS_DE_HIGH | DISPLAY_FLAGS_PIXDATA_POSEDGE
};

static struct panel_dsi dsi = {
	.format = DSI_FMT_RGB888,
	.mode = DSI_MOD_VID_BURST | DSI_CLOCK_NON_CONTINUOUS,
	.lane_num = 4,
};

static int panel_bind(struct device *dev, struct device *master, void *data)
{
	struct panel_comp *p;
	struct sc7707 *sc7707;

	p = devm_kzalloc(dev, sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	sc7707 = devm_kzalloc(dev, sizeof(*sc7707), GFP_KERNEL);
	if (!sc7707)
		return -ENOMEM;

	if (panel_parse_dts(p, dev) < 0)
		return -1;

	sc7707->power = devm_gpiod_get(dev, "power", GPIOD_ASIS);
	if (IS_ERR(sc7707->power)) {
		dev_err(dev, "failed to get power gpio\n");
		return PTR_ERR(sc7707->power);
	}

	sc7707->reset = devm_gpiod_get(dev, "reset", GPIOD_ASIS);
	if (IS_ERR(sc7707->reset)) {
		dev_err(dev, "failed to get reset gpio\n");
		return PTR_ERR(sc7707->reset);
	}

	p->panel.dsi = &dsi;
	panel_init(p, dev, &panel_vm, &panel_funcs, sc7707);

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
