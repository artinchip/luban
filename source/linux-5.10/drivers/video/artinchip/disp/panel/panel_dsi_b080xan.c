// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for general DSI panel.
 *
 * Copyright (C) 2020-2022 ArtInChip Technology Co., Ltd.
 * Authors:  Matteo <duanmt@artinchip.com>
 */

#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/component.h>
#include <linux/platform_device.h>
#include <video/display_timing.h>

#include "panel_dsi.h"

#define PANEL_DEV_NAME		"dsi_panel_b080xan"

static int panel_enable(struct aic_panel *panel)
{
	int ret;

	if (unlikely(!panel->callbacks.di_send_cmd))
		panic("Have no send_cmd() API for DSI.\n");

	panel_di_enable(panel, 0);
	panel_dsi_send_perpare(panel);

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
	return 0;
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
	.pixelclock = 80000000,
	.hactive = 768,
	.hfront_porch = 100,
	.hback_porch = 200,
	.hsync_len = 100,
	.vactive = 1024,
	.vfront_porch = 20,
	.vback_porch = 100,
	.vsync_len = 80,
	.flags = DISPLAY_FLAGS_HSYNC_LOW | DISPLAY_FLAGS_VSYNC_LOW |
		DISPLAY_FLAGS_DE_HIGH | DISPLAY_FLAGS_PIXDATA_POSEDGE
};

static int panel_bind(struct device *dev,
			struct device *master, void *data)
{
	struct panel_comp *p;
	struct panel_dsi *dsi;

	p = devm_kzalloc(dev, sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	dsi = devm_kzalloc(dev, sizeof(*dsi), GFP_KERNEL);
	if (!dsi)
		return -ENOMEM;

	if (panel_parse_dts(p, dev) < 0)
		return -1;

	dsi->mode = DSI_MOD_VID_PULSE;
	dsi->format = DSI_FMT_RGB888;
	dsi->lane_num = 4;
	p->panel.dsi = dsi;

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

MODULE_AUTHOR("Matteo <duanmt@artinchip.com>");
MODULE_DESCRIPTION("AIC-" PANEL_DEV_NAME);
MODULE_LICENSE("GPL");
