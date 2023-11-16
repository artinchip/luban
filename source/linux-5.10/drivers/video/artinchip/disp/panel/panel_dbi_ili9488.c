// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for MIPI-DBI Type B (I8080) ILI9488 panel.
 *
 * Copyright (C) 2022-2023 ArtInChip Technology Co., Ltd.
 * Authors:  Huahui Mai <huahui.mai@artinchip.com>
 */

#include <linux/module.h>
#include <linux/component.h>
#include <linux/platform_device.h>
#include <video/display_timing.h>
#include <dt-bindings/display/artinchip,aic-disp.h>
#include "panel_dbi.h"

#define PANEL_DEV_NAME		"panel_dbi_ili9488"

/* Init sequence, each line consists of command, count of data, data... */
static const u8 ili9488_commands[] = {
	0xf7,  4,  0xa9, 0x51, 0x2c, 0x82,
	0xec,  4,  0x00, 0x02, 0x03, 0x7a,
	0xc0,  2,  0x13, 0x13,
	0xc1,  1,  0x41,
	0xc5,  3,  0x00, 0x28, 0x80,
	0xb1,  2,  0xb0, 0x11,
	0xb4,  1,  0x02,
	0xb6,  2,  0x02, 0x22,
	0xb7,  1,  0xc6,
	0xbe,  2,  0x00, 0x04,
	0xe9,  1,  0x00,
	0xb7,  1,  0x07,
	0xf4,  3,  0x00, 0x00, 0x0f,
	0xe0,  15, 0x00, 0x04, 0x0e, 0x08, 0x17, 0x0a, 0x40, 0x79, 0x4d, 0x07,
		   0x0e, 0x0a, 0x1a, 0x1d, 0x0f,
	0xe1,  15, 0x00, 0x1b, 0x1f, 0x02, 0x10, 0x05, 0x32, 0x34, 0x43, 0x02,
		   0x0a, 0x09, 0x33, 0x37, 0x0f,
	0xf4,  3,  0x00, 0x00, 0x0f,
	0x35,  1,  0x00,
	0x44,  2,  0x00, 0x10,
	0x33,  6,  0x00, 0x00, 0x01, 0xe0, 0x00, 0x00,
	0x37,  2,  0x00, 0x00,
	0x2a,  4,  0x00, 0x00, 0x01, 0x3f,
	0x2b,  4,  0x00, 0x00, 0x01, 0xdf,
	0x36,  1,  0x08,
	0x3a,  1,  0x66,
	0x11,  0,
	0x00,  1,  120,
	0x29,  0,
	0x50,  0,
};

static struct aic_panel_funcs panel_funcs = {
	.disable = panel_default_disable,
	.unprepare = panel_default_unprepare,
	.prepare = panel_default_prepare,
	.enable = panel_dbi_default_enable,
	.get_video_mode = panel_default_get_video_mode,
	.register_callback = panel_register_callback,
};

/* Init the videomode parameter, dts will override the initial value. */
static struct videomode panel_vm = {
	.pixelclock = 8000000,
	.hactive = 320,
	.hfront_porch = 2,
	.hback_porch = 3,
	.hsync_len = 1,
	.vactive = 480,
	.vfront_porch = 3,
	.vback_porch = 2,
	.vsync_len = 1,
	.flags = DISPLAY_FLAGS_HSYNC_LOW | DISPLAY_FLAGS_VSYNC_LOW |
		DISPLAY_FLAGS_DE_HIGH | DISPLAY_FLAGS_PIXDATA_POSEDGE
};

static struct panel_dbi dbi = {
	.type = I8080,
	.format = I8080_RGB666_16BIT_3CYCLE,
	.commands = {
		.buf = ili9488_commands,
		.len = ARRAY_SIZE(ili9488_commands),
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
