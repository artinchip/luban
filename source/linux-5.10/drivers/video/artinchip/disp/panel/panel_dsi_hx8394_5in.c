// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for hx8394 DSI panel.
 *
 * Copyright (C) 2020-2022 ArtInChip Technology Co., Ltd.
 * Authors: huahui.mai <huahui.ami@artinchip.com>
 */

#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/component.h>
#include <linux/platform_device.h>
#include <video/display_timing.h>

#include "panel_dsi.h"

#define PANEL_DEV_NAME		"dsi_panel_hx8394_5in"

struct hx8394 {
	struct gpio_desc *vdden;
	struct gpio_desc *reset;
	struct gpio_desc *iovcc;
	struct gpio_desc *blen;
};

static inline struct hx8394 *panel_to_hx8394(struct aic_panel *panel)
{
	return (struct hx8394 *)panel->panel_private;
}

static void panel_gpio_init(struct aic_panel *panel)
{
	struct hx8394 *hx8394 = panel_to_hx8394(panel);

	aic_delay_ms(30);
	gpiod_direction_output(hx8394->iovcc, 1);
	aic_delay_ms(60);
	gpiod_direction_output(hx8394->vdden, 1);
	aic_delay_ms(60);
	gpiod_direction_output(hx8394->reset, 1);
	aic_delay_ms(60);
	gpiod_direction_output(hx8394->blen, 1);
	aic_delay_ms(60);
}

static int panel_enable(struct aic_panel *panel)
{
	int ret;

	panel_gpio_init(panel);

	panel_di_enable(panel, 0);
	panel_dsi_send_perpare(panel);

	/* Set EXTC */
	panel_dsi_generic_send_seq(panel, 0xB9, 0xFF, 0x83, 0x94);
	/* Set MIPI */
	panel_dsi_generic_send_seq(panel, 0xBA, 0x63, 0x03, 0x68, 0x6B, 0xB2,
				0xC0);
	/* Set POWER */
	panel_dsi_generic_send_seq(panel, 0xB1, 0x50, 0x0F, 0x6F, 0x0A, 0x33,
				0x54, 0xB1, 0x31, 0x6B, 0x2F);
	/* Set Display */
	panel_dsi_generic_send_seq(panel, 0xB2, 0x00, 0x80, 0x64, 0x0E, 0x0D,
				0x2F);
	/* Set CYC */
	panel_dsi_generic_send_seq(panel, 0xB4, 0x73, 0x74, 0x73, 0x74, 0x73,
				0x74, 0x01, 0x0C, 0x86, 0x75, 0x00, 0x3F, 0x73,
				0x74, 0x73, 0x74, 0x73, 0x74, 0x01, 0x0C, 0x86);
	/* Set VCOM */
	panel_dsi_generic_send_seq(panel, 0xB6, 0x73, 0x73);
	/* Set D3 */
	panel_dsi_generic_send_seq(panel,  0xD3, 0x00, 0x00, 0x07, 0x07, 0x40,
				0x07, 0x10, 0x00, 0x08, 0x10, 0x08, 0x00, 0x08,
				0x54, 0x15, 0x0E, 0x05, 0x0E, 0x02, 0x15, 0x06,
				0x05, 0x06, 0x47, 0x44, 0x0A, 0x0A, 0x4B, 0x10,
				0x07, 0x07, 0x0E, 0x40);
	/* Set GIP */
	panel_dsi_generic_send_seq(panel, 0xD5, 0x1A, 0x1A, 0x1B, 0x1B, 0x00,
				0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
				0x09, 0x0A, 0x0B, 0x24, 0x25, 0x18, 0x18, 0x26,
				0x27, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
				0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
				0x18, 0x20, 0x21, 0x18, 0x18, 0x18, 0x18);
	/* Set D6 */
	panel_dsi_generic_send_seq(panel, 0xD6, 0x1A, 0x1A, 0x1B, 0x1B, 0x0B,
				0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03,
				0x02, 0x01, 0x00, 0x21, 0x20, 0x18, 0x18, 0x27,
				0x26, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
				0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
				0x18, 0x25, 0x24, 0x18, 0x18, 0x18, 0x18);
	/* Set Gamma */
	panel_dsi_generic_send_seq(panel, 0xE0, 0x00, 0x0C, 0x19, 0x20, 0x23,
				0x26, 0x29, 0x28, 0x51, 0x61, 0x70, 0x6F, 0x76,
				0x86, 0x89, 0x8D, 0x99, 0x9A, 0x95, 0xA1, 0xB0,
				0x57, 0x55, 0x58, 0x5C, 0x5E, 0x64, 0x6B, 0x7F,
				0x00, 0x0C, 0x18, 0x20, 0x23, 0x26, 0x29, 0x28,
				0x51, 0x61, 0x70, 0x6F, 0x76, 0x86, 0x89, 0x8D,
				0x99, 0x9A, 0x95, 0xA1, 0xB0, 0x57, 0x55, 0x58,
				0x5C, 0x5E, 0x64, 0x6B, 0x7F);
	/* Set C0 */
	panel_dsi_generic_send_seq(panel, 0xC0, 0x1F, 0x31);
	/* Set Panel */
	panel_dsi_generic_send_seq(panel, 0xCC, 0x0B);
	/* Set D4 */
	panel_dsi_generic_send_seq(panel, 0xD4, 0x02);
	/* Set BD */
	panel_dsi_generic_send_seq(panel, 0xBD, 0x02);
	/* Set D8 */
	panel_dsi_generic_send_seq(panel, 0xD8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
	/* Set BD */
	panel_dsi_generic_send_seq(panel, 0xBD, 0x00);
	/* Set BD */
	panel_dsi_generic_send_seq(panel, 0xBD, 0x01);
	/* Set GAS */
	panel_dsi_generic_send_seq(panel, 0xB1, 0x00);
	/* Set BD */
	panel_dsi_generic_send_seq(panel, 0xBD, 0x00);
	/* Set Power Option */
	panel_dsi_generic_send_seq(panel, 0xBF, 0x40, 0x81, 0x50, 0x00, 0x1A,
				0xFC, 0x01);
	/* Set ECO */
	panel_dsi_generic_send_seq(panel, 0xC6, 0xED);

	ret = panel_dsi_dcs_exit_sleep_mode(panel);
	if (ret < 0) {
		dev_err(panel->dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}

	aic_delay_ms(120);

	ret = panel_dsi_dcs_set_display_on(panel);
	if (ret < 0) {
		dev_err(panel->dev, "Failed to set display on: %d\n", ret);
		return ret;
	}

	panel_dsi_setup_realmode(panel);
	panel_de_timing_enable(panel, 0);
	panel_backlight_enable(panel, 0);
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
	.pixelclock = 82000000,
	.hactive = 720,
	.hfront_porch = 80,
	.hback_porch = 160,
	.hsync_len = 80,
	.vactive = 1280,
	.vfront_porch = 9,
	.vback_porch = 9,
	.vsync_len = 7,
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
	struct hx8394 *hx8394;
	const char *str;

	p = devm_kzalloc(dev, sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	hx8394 = devm_kzalloc(dev, sizeof(*hx8394), GFP_KERNEL);
	if (!hx8394)
		return -ENOMEM;

	if (panel_parse_dts(p, dev) < 0)
		return -1;

	if (!of_property_read_string(np, "dsi,mode", &str))
		dsi.mode = panel_dsi_str2mode(str);

	hx8394->vdden = devm_gpiod_get(dev, "vdden", GPIOD_ASIS);
	if (IS_ERR(hx8394->vdden)) {
		dev_err(dev, "failed to get power gpio\n");
		return PTR_ERR(hx8394->vdden);
	}

	hx8394->reset = devm_gpiod_get(dev, "reset", GPIOD_ASIS);
	if (IS_ERR(hx8394->reset)) {
		dev_err(dev, "failed to get reset gpio\n");
		return PTR_ERR(hx8394->reset);
	}

	hx8394->iovcc = devm_gpiod_get(dev, "iovcc", GPIOD_ASIS);
	if (IS_ERR(hx8394->iovcc)) {
		dev_err(dev, "failed to get reset gpio\n");
		return PTR_ERR(hx8394->iovcc);
	}

	hx8394->blen = devm_gpiod_get(dev, "blen", GPIOD_ASIS);
	if (IS_ERR(hx8394->blen)) {
		dev_err(dev, "failed to get reset gpio\n");
		return PTR_ERR(hx8394->blen);
	}

	p->panel.dsi = &dsi;
	panel_init(p, dev, &panel_vm, &panel_funcs, hx8394);

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
