// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for SRGB ILI8961 panel.
 *
 * Copyright (C) 2020-2022 ArtInChip Technology Co., Ltd.
 * Authors:  Huahui Mai <huahui.mai@artinchip.com>
 */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/component.h>
#include <linux/platform_device.h>
#include <video/display_timing.h>
#include <dt-bindings/display/artinchip,aic-disp.h>

#include "panel_com.h"

#define PANEL_DEV_NAME		"panel_srgb_ili8961"


struct aic_srgb_comp {
	struct gpio_desc *cs;
	struct gpio_desc *scl;
	struct gpio_desc *sda;
};

static void ILI8961_send(struct aic_srgb_comp *comp, unsigned char data)
{
	int i;

	for (i = 0; i < 8; i++) {
		gpiod_set_value(comp->scl, 0);

		if (data & 0x80)
			gpiod_set_value(comp->sda, 1);
		else
			gpiod_set_value(comp->sda, 0);

		udelay(5);

		gpiod_set_value(comp->scl, 1);
		udelay(5);
		data <<= 1;
	}
}

static void ILI8961_write(struct aic_srgb_comp *comp, unsigned char code,
							unsigned char data)
{
	gpiod_set_value(comp->cs, 1);
	gpiod_set_value(comp->scl, 1);
	gpiod_set_value(comp->sda, 1);
	udelay(50);
	gpiod_set_value(comp->cs, 0);
	udelay(50);

	ILI8961_send(comp, code);
	ILI8961_send(comp, data);

	gpiod_set_value(comp->scl, 0);
	udelay(5);

	gpiod_set_value(comp->cs, 1);
	gpiod_set_value(comp->scl, 1);
	gpiod_set_value(comp->sda, 1);
}

static int ILI8961_init(struct device *dev)
{
	struct aic_srgb_comp *comp;


	comp = devm_kzalloc(dev, sizeof(*comp), GFP_KERNEL);
	if (!comp)
		return -ENOMEM;

	comp->cs = devm_gpiod_get_optional(dev, "cs", GPIOD_OUT_HIGH);
	if (IS_ERR(comp->cs))
		return PTR_ERR(comp->cs);

	comp->sda = devm_gpiod_get_optional(dev, "sda", GPIOD_OUT_HIGH);
	if (IS_ERR(comp->sda))
		return PTR_ERR(comp->sda);

	comp->scl = devm_gpiod_get_optional(dev, "scl", GPIOD_OUT_HIGH);
	if (IS_ERR(comp->scl))
		return PTR_ERR(comp->scl);

	udelay(10);
	ILI8961_write(comp, 0x05, 0x5F);
	udelay(5);
	ILI8961_write(comp, 0x05, 0x1F);
	udelay(10);
	ILI8961_write(comp, 0x05, 0x5F);
	udelay(50);
	ILI8961_write(comp, 0x2B, 0x01);
	ILI8961_write(comp, 0x00, 0x09);
	ILI8961_write(comp, 0x01, 0x9F);
	ILI8961_write(comp, 0x04, 0x09);
	ILI8961_write(comp, 0x16, 0x04);

	return 0;
}

int panel_prepare(struct aic_panel *panel)
{
	dev_info(panel->dev, "ILI8961 init\n");
	if (ILI8961_init(panel->dev))
		dev_err(panel->dev, "Failed to enable panel ILI8961\n");

	return panel_default_prepare(panel);
}

static struct aic_panel_funcs panel_funcs = {
	.disable = panel_default_disable,
	.unprepare = panel_default_unprepare,
	.prepare = panel_prepare,
	.enable = panel_default_enable,
	.get_video_mode = panel_default_get_video_mode,
	.register_callback = panel_register_callback,
};

/* Init the videomode parameter, dts will override the initial value. */
static struct videomode panel_vm = {
	.pixelclock = 33300000,
	.hactive = 320,
	.hfront_porch = 20,
	.hback_porch = 12,
	.hsync_len = 11,
	.vactive = 240,
	.vfront_porch = 8,
	.vback_porch = 5,
	.vsync_len = 16,
	.flags = DISPLAY_FLAGS_HSYNC_LOW | DISPLAY_FLAGS_VSYNC_LOW |
		DISPLAY_FLAGS_DE_HIGH | DISPLAY_FLAGS_PIXDATA_POSEDGE
};

static int panel_srgb_parse_dt(struct device *dev, struct panel_comp *p)
{
	struct panel_rgb *rgb;

	rgb = devm_kzalloc(dev, sizeof(*rgb), GFP_KERNEL);
	if (!rgb)
		return -ENOMEM;

	memset(rgb, 0, sizeof(*rgb));

	if (panel_parse_dts(p, dev) < 0)
		return -1;

	rgb->mode = SRGB;
	rgb->format = SRGB_8BIT;

	p->panel.rgb = rgb;
	return 0;
}

static int panel_bind(struct device *dev,
			struct device *master, void *data)
{
	struct panel_comp *p;
	int ret;

	p = devm_kzalloc(dev, sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	ret = panel_srgb_parse_dt(dev, p);
	if (ret < 0)
		return ret;

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
	{.compatible = "artinchip,aic-general-rgb-panel"},
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
