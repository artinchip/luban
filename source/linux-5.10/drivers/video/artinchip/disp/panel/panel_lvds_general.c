// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for general LVDS panel.
 *
 * Copyright (C) 2020-2022 ArtInChip Technology Co., Ltd.
 * Authors:  Matteo <duanmt@artinchip.com>
 */
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/component.h>
#include <linux/platform_device.h>
#include <video/display_timing.h>

#include "panel_com.h"

#define PANEL_DEV_NAME		"panel_lvds_general"

static struct aic_panel_funcs panel_funcs = {
	.disable = panel_default_disable,
	.unprepare = panel_default_unprepare,
	.prepare = panel_default_prepare,
	.enable = panel_default_enable,
	.get_video_mode = panel_default_get_video_mode,
	.register_callback = panel_register_callback,
};

/* Init the videomode parameter, dts will override the initial value. */
static struct videomode panel_vm = {
	.flags = DISPLAY_FLAGS_HSYNC_LOW | DISPLAY_FLAGS_VSYNC_LOW |
		DISPLAY_FLAGS_DE_HIGH | DISPLAY_FLAGS_PIXDATA_POSEDGE
};

static int lvds_mode_match(const char *str)
{
	if (strcmp(str, "jeida-18") == 0)
		return JEIDA_18BIT;
	else if (strcmp(str, "jeida-24") == 0)
		return JEIDA_24BIT;
	else if (strcmp(str, "vesa-24") == 0)
		return NS;

	pr_err("invalid data-mapping properety: %s", str);
	return NS;
}

static int lvds_link_mode_match(const char *str)
{
	if (strcmp(str, "single-link0") == 0)
		return SINGLE_LINK0;
	else if (strcmp(str, "single-link1") == 0)
		return SINGLE_LINK1;
	else if (strcmp(str, "double-screen") == 0)
		return DOUBLE_SCREEN;
	else if (strcmp(str, "dual-link") == 0)
		return DUAL_LINK;

	pr_err("invalid data-channel properety: %s", str);
	return SINGLE_LINK0;
}

static int panel_lvds_parse_dt(struct device *dev, struct panel_comp *p)
{
	struct device_node *np = dev->of_node;
	struct panel_lvds *lvds;
	const char *str;
	int ret;

	lvds = devm_kzalloc(dev, sizeof(*lvds), GFP_KERNEL);
	if (!lvds)
		return -ENOMEM;

	if (panel_parse_dts(p, dev) < 0)
		return -1;

	ret = of_property_read_string(np, "data-mapping", &str);
	if (ret) {
		dev_err(dev, "Can't parse data-mapping property\n");
		return ret;
	}
	lvds->mode = lvds_mode_match(str);

	ret = of_property_read_string(np, "data-channel", &str);
	if (ret) {
		dev_err(dev, "Can't parse data-mapping property\n");
		return ret;
	}
	lvds->link_mode = lvds_link_mode_match(str);

	p->panel.lvds = lvds;
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

	ret = panel_lvds_parse_dt(dev, p);
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
	{.compatible = "artinchip,aic-general-lvds-panel"},
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

MODULE_AUTHOR("Matteo <duanmt@artinchip.com>");
MODULE_DESCRIPTION("AIC-" PANEL_DEV_NAME);
MODULE_LICENSE("GPL");
