// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for st7701s DSI panel.
 *
 * Copyright (C) 2020-2022 ArtInChip Technology Co., Ltd.
 * Authors: huahui.mai <huahui.ami@artinchip.com>
 */

#include <linux/module.h>
#include <linux/component.h>
#include <linux/platform_device.h>
#include <video/display_timing.h>

#include "panel_dsi.h"

#define PANEL_DEV_NAME		"dsi_panel_st7701s"

static int panel_enable(struct aic_panel *panel)
{
	panel_di_enable(panel, 0);
	panel_dsi_send_perpare(panel);

	/* Out sleep */
	panel_dsi_dcs_send_seq(panel, 0x11);

	aic_delay_ms(120);

	/* Display Control Setting */
	panel_dsi_dcs_send_seq(panel, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x10);
	panel_dsi_dcs_send_seq(panel, 0xC0, 0x63, 0x00);
	panel_dsi_dcs_send_seq(panel, 0xC1, 0x11, 0x02);
	panel_dsi_dcs_send_seq(panel, 0xC2, 0x37, 0x00);
	panel_dsi_dcs_send_seq(panel, 0xCC, 0x18);
	panel_dsi_dcs_send_seq(panel, 0xC7, 0x00);

	/* GAMMA Set */
	panel_dsi_dcs_send_seq(panel, 0xB0, 0x40, 0xC9, 0x91, 0x07, 0x02,
		0x09, 0x09, 0x1F, 0x04, 0x50);
	panel_dsi_dcs_send_seq(panel, 0x0F, 0xE4, 0x29, 0xDF);
	panel_dsi_dcs_send_seq(panel, 0xB1, 0x40, 0xCB, 0xD0, 0x11, 0x92, 0x07,
		0x00, 0x08, 0x07, 0x1C);
	panel_dsi_dcs_send_seq(panel, 0x06, 0x53, 0x12, 0x63, 0xEB, 0xDF);

	/* Power Control Registers Initial */
	panel_dsi_dcs_send_seq(panel, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x11);
	panel_dsi_dcs_send_seq(panel, 0xB0, 0x65);

	/* Vcom Setting */
	panel_dsi_dcs_send_seq(panel, 0xB1, 0x6A);

	/* End Vcom Setting */
	panel_dsi_dcs_send_seq(panel, 0xB2, 0x87);
	panel_dsi_dcs_send_seq(panel, 0xB3, 0x80);
	panel_dsi_dcs_send_seq(panel, 0xB5, 0x49);
	panel_dsi_dcs_send_seq(panel, 0xB7, 0x85);
	panel_dsi_dcs_send_seq(panel, 0xB8, 0x20);
	panel_dsi_dcs_send_seq(panel, 0xB9, 0x10);
	panel_dsi_dcs_send_seq(panel, 0xC1, 0x78);
	panel_dsi_dcs_send_seq(panel, 0xC2, 0x78);
	panel_dsi_dcs_send_seq(panel, 0xD0, 0x88);
	aic_delay_ms(100);

	/* GIP Set  */
	panel_dsi_dcs_send_seq(panel, 0xE0, 0x00, 0x00, 0x02);
	panel_dsi_dcs_send_seq(panel, 0xE1, 0x08, 0x00, 0x0A, 0x00, 0x07, 0x00,
		0x09, 0x00, 0x00, 0x33, 0x33);
	panel_dsi_dcs_send_seq(panel, 0xE2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	panel_dsi_dcs_send_seq(panel, 0xE3, 0x00, 0x00, 0x33, 0x33);
	panel_dsi_dcs_send_seq(panel, 0xE4, 0x44, 0x44);
	panel_dsi_dcs_send_seq(panel, 0xE5, 0x0E, 0x60, 0xA0, 0xA0, 0x10, 0x60,
		0xA0, 0xA0, 0x0A, 0x60, 0xA0, 0xA0, 0x0C, 0x60, 0xA0, 0xA0);
	panel_dsi_dcs_send_seq(panel, 0xE6, 0x00, 0x00, 0x33, 0x33);
	panel_dsi_dcs_send_seq(panel, 0xE7, 0x44, 0x44);
	panel_dsi_dcs_send_seq(panel, 0xE8, 0x0D, 0x60, 0xA0, 0xA0, 0x0F, 0x60,
		0xA0, 0xA0, 0x09, 0x60, 0xA0, 0xA0, 0x0B, 0x60, 0xA0, 0xA0);
	panel_dsi_dcs_send_seq(panel, 0xEB, 0x02, 0x01, 0xE4, 0xE4, 0x44, 0x00,
		0x40);
	panel_dsi_dcs_send_seq(panel, 0xEC, 0x02, 0x01);
	panel_dsi_dcs_send_seq(panel, 0xED, 0xAB, 0x89, 0x76, 0x54, 0x01, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x10, 0x45, 0x67, 0x98, 0xBA);
	panel_dsi_dcs_send_seq(panel, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x00);

	panel_dsi_dcs_send_seq(panel, 0x36, 0x00);
	panel_dsi_dcs_send_seq(panel, 0x3A, 0x70);
	panel_dsi_dcs_send_seq(panel, 0x29);

	aic_delay_ms(120);

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
	.pixelclock = 27000000,
	.hactive = 480,
	.hfront_porch = 20,
	.hback_porch = 35,
	.hsync_len = 4,
	.vactive = 800,
	.vfront_porch = 10,
	.vback_porch = 20,
	.vsync_len = 4,
	.flags = DISPLAY_FLAGS_HSYNC_LOW | DISPLAY_FLAGS_VSYNC_LOW |
		DISPLAY_FLAGS_DE_HIGH | DISPLAY_FLAGS_PIXDATA_POSEDGE
};

static int panel_bind(struct device *dev, struct device *master, void *data)
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
	dsi->lane_num = 2;
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

MODULE_AUTHOR("huahui.mai <huahui.mai@artinchip.com>");
MODULE_DESCRIPTION("AIC-" PANEL_DEV_NAME);
MODULE_LICENSE("GPL");
