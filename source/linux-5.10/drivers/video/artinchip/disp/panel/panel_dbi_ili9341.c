// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for MIPI DBI Type C (SPI) ILI9341 panel.
 *
 * Copyright (C) 2020-2023 ArtInChip Technology Co., Ltd.
 * Authors:  Huahui Mai <huahui.mai@artinchip.com>
 */

#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/component.h>
#include <linux/platform_device.h>
#include <video/display_timing.h>
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
	struct gpio_desc *reset;

	reset = devm_gpiod_get_optional(panel->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR_OR_NULL(reset)) {
		dev_err(panel->dev, "failed to get reset gpio\n");
		return PTR_ERR(reset);
	}

	gpiod_set_value(reset, 1);
	mdelay(5);
	gpiod_set_value(reset, 0);
	mdelay(5);
	gpiod_set_value(reset, 1);
	mdelay(5);

	return panel_dbi_default_enable(panel);
}

static struct aic_panel_funcs panel_funcs = {
	.disable = panel_default_disable,
	.unprepare = panel_default_unprepare,
	.prepare = panel_default_prepare,
	.enable = panel_enable,
	.get_video_mode = panel_default_get_video_mode,
	.register_callback = panel_register_callback,
};

/* Init the videomode parameter, dts will override the initial value. */
static struct videomode panel_vm = {
	.pixelclock = 3000000,
	.hactive = 240,
	.hfront_porch = 2,
	.hback_porch = 3,
	.hsync_len = 1,
	.vactive = 320,
	.vfront_porch = 3,
	.vback_porch = 1,
	.vsync_len = 2,
	.flags = DISPLAY_FLAGS_HSYNC_LOW | DISPLAY_FLAGS_VSYNC_LOW |
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

static int panel_bind(struct device *dev,
			struct device *master, void *data)
{
	struct panel_comp *p;
	int ret;

	p = devm_kzalloc(dev, sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	ret = panel_parse_dts(p, dev);
	if (ret < 0)
		return ret;

	p->panel.dbi = &dbi;
	panel_init(p, dev, &panel_vm, &panel_funcs, NULL);

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
	{.compatible = "artinchip,aic-dbi-panel-simple"},
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

MODULE_AUTHOR("Huahui Mai <huahui.mai@artinchip.com>");
MODULE_DESCRIPTION("AIC-" PANEL_DEV_NAME);
MODULE_LICENSE("GPL");
