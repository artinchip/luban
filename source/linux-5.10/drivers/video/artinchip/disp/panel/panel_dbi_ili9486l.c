// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for MIPI-DBI Type B (I8080) ILI9486L panel.
 *
 * Copyright (C) 2020-2023 ArtInChip Technology Co., Ltd.
 * Authors:  Huahui Mai <huahui.mai@artinchip.com>
 */

#include <linux/module.h>
#include <linux/component.h>
#include <linux/platform_device.h>
#include <video/display_timing.h>
#include <dt-bindings/display/artinchip,aic-disp.h>
#include "panel_dbi.h"

#define PANEL_DEV_NAME		"panel_dbi_ili9486l"

/* Init sequence, each line consists of command, count of data, data... */
static const u8 ili9486l_commands[] = {
	0xf2,  9,  0x18, 0xa3, 0x12, 0x02, 0xb2, 0x12, 0xff, 0x10, 0x00,
	0xf8,  2,  0x21, 0x04,
	0xf9,  2,  0x00, 0x08,
	0xf1,  6,  0x36, 0x04,   0x00,   0x3c,   0x0f,   0x8f,
	0xc0,  2,  0x19, 0x1a,
	0xc1,  1,  0x44,
	0xc2,  1,  0x11,
	0xc5,  2,  0x00, 0x06,
	0xb1,  2,  0x80, 0x11,
	0xb4,  1,  0x02,
	0xb0,  1,  0x00,
	0xb6,  3,  0x00, 0x22, 0x3b,
	0xb7,  1,  0x07,
	0xe0,  15, 0x1f, 0x25, 0x22, 0x0a, 0x05, 0x07, 0x50, 0xa8, 0x40, 0x0d,
		   0x19, 0x07, 0x00, 0x00, 0x00,
	0xe1,  15, 0x1f, 0x3f, 0x3f, 0x08, 0x06, 0x02, 0x3f, 0x75, 0x2e, 0x08,
		   0x0b, 0x05, 0x1d, 0x1a, 0x00,
	0x35,  1,  0x01,
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
	.pixelclock = 9216000,
	.hactive = 320,
	.hfront_porch = 2,
	.hback_porch = 3,
	.hsync_len = 1,
	.vactive = 480,
	.vfront_porch = 3,
	.vback_porch = 1,
	.vsync_len = 2,
	.flags = DISPLAY_FLAGS_HSYNC_LOW | DISPLAY_FLAGS_VSYNC_LOW |
		DISPLAY_FLAGS_DE_HIGH | DISPLAY_FLAGS_PIXDATA_POSEDGE
};

static struct panel_dbi dbi = {
	.type = I8080,
	.format = I8080_RGB666_8BIT,
	.commands = {
		.buf = ili9486l_commands,
		.len = ARRAY_SIZE(ili9486l_commands),
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
