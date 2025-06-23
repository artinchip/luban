// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for wuxga_7in DSI panel.
 *
 * Copyright (C) 2020-2022 ArtInChip Technology Co., Ltd.
 * Authors: huahui.mai <huahui.ami@artinchip.com>
 */

#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/component.h>
#include <linux/platform_device.h>
#include <video/display_timing.h>

#include "../hw/dsi_reg.h"
#include "panel_dsi.h"

#define PANEL_DEV_NAME		"dsi_panel_wuxga_7in"

struct wuxga {
	struct gpio_desc *dcdc_en;
	struct gpio_desc *reset;
};

static inline struct wuxga *panel_to_wuxga(struct aic_panel *panel)
{
	return (struct wuxga *)panel->panel_private;
}

static int panel_gpio_init(struct aic_panel *panel)
{
	struct wuxga *wuxga = panel_to_wuxga(panel);

	aic_delay_ms(10);
	gpiod_direction_output(wuxga->dcdc_en, 1);
	aic_delay_ms(20);
	gpiod_direction_output(wuxga->reset, 1);
	aic_delay_ms(10);
	gpiod_direction_output(wuxga->dcdc_en, 0);
	aic_delay_ms(120);
	gpiod_direction_output(wuxga->dcdc_en, 1);
	aic_delay_ms(20);
	return 0;
}

static int panel_enable(struct aic_panel *panel)
{
	enum dsi_mode mode = panel->dsi->mode;
	int ret;


	if (panel_gpio_init(panel) < 0)
		return -ENODEV;

	panel_di_enable(panel, 0);
	panel_dsi_send_perpare(panel);

	panel_dsi_dcs_send_seq(panel, DSI_DCS_SOFT_RESET);
	aic_delay_ms(5);
	panel_dsi_dcs_send_seq(panel, 0xb0, 0x00);
	panel_dsi_dcs_send_seq(panel, 0xb3, 0x04, 0x08, 0x00, 0x22, 0x00);
	panel_dsi_dcs_send_seq(panel, 0xb4, 0x0c);
	panel_dsi_dcs_send_seq(panel, 0xb6, 0x3a, 0xd3);
	panel_dsi_dcs_send_seq(panel, 0x51, 0xE6);
	panel_dsi_dcs_send_seq(panel, 0x53, 0x2c);
	panel_dsi_dcs_send_seq(panel, DSI_DCS_SET_PIXEL_FORMAT, 0x77);
	panel_dsi_dcs_send_seq(panel, DSI_DCS_SET_ADDRESS_MODE, 0x00);
	panel_dsi_dcs_send_seq(panel, DSI_DCS_SET_COLUMN_ADDRESS, 0x00, 0x00, 0x04, 0xaf);
	panel_dsi_dcs_send_seq(panel, DSI_DCS_SET_PAGE_ADDRESS, 0x00, 0x00, 0x07, 0x7f);

	ret = panel_dsi_dcs_exit_sleep_mode(panel);
	if (ret < 0) {
		pr_err("Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	aic_delay_ms(120);

	ret = panel_dsi_dcs_set_display_on(panel);
	if (ret < 0) {
		pr_err("Failed to set display on: %d\n", ret);
		return ret;
	}

	if (mode == DSI_MOD_CMD_MODE)
		panel_dsi_dcs_send_seq(panel, DSI_DCS_SET_TEAR_ON, 0x00);
	else
		panel_dsi_dcs_send_seq(panel, 0xb3, 0x14, 0x08, 0x00, 0x22, 0x00);

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
	.pixelclock = 150000000,
	.hactive = 1200,
	.hfront_porch = 200,
	.hback_porch = 200,
	.hsync_len = 100,
	.vactive = 1920,
	.vfront_porch = 3,
	.vback_porch = 6,
	.vsync_len = 2,
	.flags = DISPLAY_FLAGS_HSYNC_LOW | DISPLAY_FLAGS_VSYNC_LOW |
		DISPLAY_FLAGS_DE_HIGH | DISPLAY_FLAGS_PIXDATA_POSEDGE
};

static struct panel_dsi dsi = {
	.format = DSI_FMT_RGB888,
	.mode = DSI_MOD_VID_PULSE,
	.lane_num = 4,
};

static int panel_bind(struct device *dev, struct device *master, void *data)
{
	struct device_node *np = dev->of_node;
	struct panel_comp *p;
	struct wuxga *wuxga;
	const char *str;

	p = devm_kzalloc(dev, sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	wuxga = devm_kzalloc(dev, sizeof(*wuxga), GFP_KERNEL);
	if (!wuxga)
		return -ENOMEM;

	wuxga->dcdc_en = devm_gpiod_get(dev, "dcdc-en", GPIOD_ASIS);
	if (IS_ERR(wuxga->dcdc_en)) {
		dev_err(dev, "failed to get power gpio\n");
		return PTR_ERR(wuxga->dcdc_en);
	}

	wuxga->reset = devm_gpiod_get(dev, "reset", GPIOD_ASIS);
	if (IS_ERR(wuxga->reset)) {
		dev_err(dev, "failed to get reset gpio\n");
		return PTR_ERR(wuxga->reset);
	}

	if (panel_parse_dts(p, dev) < 0)
		return -1;

	p->panel.dsi = &dsi;
	panel_init(p, dev, &panel_vm, &panel_funcs, wuxga);

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
