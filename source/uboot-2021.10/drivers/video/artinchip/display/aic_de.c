// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2021 ArtInChip Technology Co.,Ltd
 * Huahui Mai <huahui.mai@artinchip.com>
 */

#include <common.h>
#include <display.h>
#include <dm.h>
#include <clk.h>
#include <reset.h>
#include <linux/ioport.h>
#include <dt-bindings/display/artinchip,aic-disp.h>

#include "aic_com.h"
#include "hw/de_hw.h"

#define MAX_LAYER_NUM 2
#define MAX_RECT_NUM 4
#define RECT_NUM_SHIFT 2
#define CONFIG_NUM   (MAX_LAYER_NUM * MAX_RECT_NUM)

struct aic_de_dither {
	unsigned int enable;
	unsigned int red_bitdepth;
	unsigned int gleen_bitdepth;
	unsigned int blue_bitdepth;
};

struct aic_de_configs {
	const struct aicfb_layer_num *layer_num;
	const struct aicfb_layer_capability *cap;
};

struct aic_de_priv {
	/* de_funcs must be the first member */
	struct de_funcs funcs;
	struct udevice *dev;
	struct fb_videomode vm;
	bool vm_flag;
	const struct aic_de_configs *config;
	struct aic_panel *panel;
	struct aic_de_dither dither;
	void __iomem *regs;
	struct reset_ctl reset;
	struct clk mclk;
	struct clk pclk;
	ulong mclk_rate;
};
static struct aic_de_priv *g_aic_de_priv;

static struct aic_de_priv *aic_de_request_drvdata(void)
{
	return g_aic_de_priv;
}

static void aic_de_release_drvdata(void)
{

}

static int aic_de_set_mode(struct aic_panel *panel, struct fb_videomode *vm)
{
	struct aic_de_priv *priv = aic_de_request_drvdata();
	int disp_dither = panel->disp_dither;

	memcpy(&priv->vm, vm, sizeof(struct fb_videomode));
	priv->vm_flag = true;
	priv->panel = panel;

	switch (disp_dither) {
	case DITHER_RGB565:
		priv->dither.red_bitdepth = 5;
		priv->dither.gleen_bitdepth = 6;
		priv->dither.blue_bitdepth = 5;
		priv->dither.enable = 1;
		break;
	case DITHER_RGB666:
		priv->dither.red_bitdepth = 6;
		priv->dither.gleen_bitdepth = 6;
		priv->dither.blue_bitdepth = 6;
		priv->dither.enable = 1;
		break;
	default:
		memset(&priv->dither, 0, sizeof(struct aic_de_dither));
		break;
	}

	aic_de_release_drvdata();
	return 0;
}

static int aic_de_clk_enable(void)
{
	struct aic_de_priv *priv = aic_de_request_drvdata();
	int ret = 0;

	ret = clk_set_rate(&priv->mclk, priv->mclk_rate);
	if (ret)
		debug("Failed to set CLK_DE %ld\n", priv->mclk_rate);

	ret = reset_deassert(&priv->reset);
	if (ret) {
		debug("%s() - Couldn't deassert\n", __func__);
		aic_de_release_drvdata();
		return ret;
	}

	ret = clk_enable(&priv->mclk);
	if (ret)
		debug("%s() - Couldn't enable mclk\n", __func__);

	aic_de_release_drvdata();
	return ret;
}

static int aic_de_clk_disable(void)
{
	struct aic_de_priv *priv = aic_de_request_drvdata();

	clk_disable(&priv->mclk);
	reset_assert(&priv->reset);

	aic_de_release_drvdata();
	return 0;
}

static int aic_de_timing_enable(void)
{
	struct aic_de_priv *priv = aic_de_request_drvdata();
	u32 active_w = priv->vm.xres;
	u32 active_h = priv->vm.yres;
	u32 hfp = priv->vm.right_margin;
	u32 hbp = priv->vm.left_margin;
	u32 vfp = priv->vm.lower_margin;
	u32 vbp = priv->vm.upper_margin;
	u32 hsync = priv->vm.hsync_len;
	u32 vsync = priv->vm.vsync_len;
	bool h_pol = !!(priv->vm.flag & DISPLAY_FLAGS_HSYNC_HIGH);
	bool v_pol = !!(priv->vm.flag & DISPLAY_FLAGS_VSYNC_HIGH);
	struct aic_tearing_effect *te = &priv->panel->te;
	int ret;

	if (!priv->vm_flag) {
		debug("%s() - videomode isn't init\n", __func__);
		aic_de_release_drvdata();
		return -EINVAL;
	}

	ret = clk_enable(&priv->pclk);
	if (ret) {
		debug("%s() - Couldn't enable pclk\n", __func__);
		aic_de_release_drvdata();
		return ret;
	}

	de_config_timing(priv->regs, active_w, active_h, hfp, hbp,
			 vfp, vbp, hsync, vsync, h_pol, v_pol);

	/* set default config */
	de_set_qos(priv->regs);
	de_set_blending_size(priv->regs, active_w, active_h);
	de_set_ui_layer_size(priv->regs, active_w, active_h, 0, 0);

	de_config_prefetch_line_set(priv->regs, 2);
	de_soft_reset_ctrl(priv->regs, 1);

	if (te->mode)
		de_config_tearing_effect(priv->regs,
				te->mode, te->pulse_width);

	if (priv->dither.enable)
		de_set_dither(priv->regs,
			      priv->dither.red_bitdepth,
			      priv->dither.gleen_bitdepth,
			      priv->dither.blue_bitdepth,
			      priv->dither.enable);

	/* global alpha */
	de_ui_alpha_blending_enable(priv->regs, 0xff, 0, 1);

	de_timing_enable(priv->regs, 1);

	aic_de_release_drvdata();
	return 0;
}

static int config_ui_layer_rect(struct aic_de_priv *priv,
				struct aicfb_layer_data *layer_data)
{
	enum aic_pixel_format format = layer_data->buf.format;
	u32 in_w = (u32)layer_data->buf.size.width;
	u32 in_h = (u32)layer_data->buf.size.height;

	u32 stride0 = layer_data->buf.stride[0];
	u32 addr0 = layer_data->buf.phy_addr[0];
	u32 x_offset = layer_data->pos.x;
	u32 y_offset = layer_data->pos.y;
	u32 crop_en = layer_data->buf.crop_en;
	u32 crop_x = layer_data->buf.crop.x;
	u32 crop_y = layer_data->buf.crop.y;
	u32 crop_w = layer_data->buf.crop.width;
	u32 crop_h = layer_data->buf.crop.height;

	switch (format) {
	case AIC_FMT_ARGB_8888:
	case AIC_FMT_ABGR_8888:
	case AIC_FMT_RGBA_8888:
	case AIC_FMT_BGRA_8888:
	case AIC_FMT_XRGB_8888:
	case AIC_FMT_XBGR_8888:
	case AIC_FMT_RGBX_8888:
	case AIC_FMT_BGRX_8888:
		if (crop_en) {
			addr0 += crop_x * 4 + crop_y * stride0;
			in_w = crop_w;
			in_h = crop_h;
		}
		break;
	case AIC_FMT_RGB_888:
	case AIC_FMT_BGR_888:
		if (crop_en) {
			addr0 += crop_x * 3 + crop_y * stride0;
			in_w = crop_w;
			in_h = crop_h;
		}
		break;
	case AIC_FMT_ARGB_1555:
	case AIC_FMT_ABGR_1555:
	case AIC_FMT_RGBA_5551:
	case AIC_FMT_BGRA_5551:
	case AIC_FMT_RGB_565:
	case AIC_FMT_BGR_565:
	case AIC_FMT_ARGB_4444:
	case AIC_FMT_ABGR_4444:
	case AIC_FMT_RGBA_4444:
	case AIC_FMT_BGRA_4444:
		if (crop_en) {
			addr0 += crop_x * 2 + crop_y * stride0;
			in_w = crop_w;
			in_h = crop_h;
		}
		break;
	default:
		debug("invalid ui layer format: %d\n", format);

		return -EINVAL;
	}

	de_set_ui_layer_format(priv->regs, format);
	de_ui_layer_enable(priv->regs, 1);

	de_ui_layer_set_rect(priv->regs, in_w, in_h, x_offset, y_offset,
			     stride0, addr0, layer_data->rect_id);
	de_ui_layer_rect_enable(priv->regs, layer_data->rect_id, 1);

	return 0;
}

static int update_one_layer_config(struct aic_de_priv *priv,
				   struct aicfb_layer_data *layer_data)
{
	int ret;

	ret = config_ui_layer_rect(priv, layer_data);
	if (ret)
		debug("ui layer rect condig err\n");

	return ret;
}

static int aic_de_update_layer_config(struct aicfb_layer_data *layer_data)
{
	int ret;
	struct aic_de_priv *priv = aic_de_request_drvdata();

	de_config_update_enable(priv->regs, 0);
	ret = update_one_layer_config(priv, layer_data);
	de_config_update_enable(priv->regs, 1);

	aic_de_release_drvdata();
	return ret;
}

static ulong aic_de_pixclk_rate(void)
{
	ulong rate;
	struct aic_de_priv *priv = aic_de_request_drvdata();

	rate = priv->vm.pixclock;
	aic_de_release_drvdata();
	return rate;
}

static int aic_de_pixclk_enable(void)
{
	s32 ret = 0;
	struct aic_de_priv *priv = aic_de_request_drvdata();

	ret = clk_set_rate(&priv->pclk, priv->vm.pixclock);
	if (ret)
		debug("Failed to set CLK_PIX %d\n", priv->vm.pixclock);

	aic_de_release_drvdata();
	return ret;
}

static int aic_de_parse_dt(struct udevice *dev)
{
	int ret;
	struct aic_de_priv *priv = dev_get_priv(dev);
	ofnode node = dev_ofnode(dev);

	ret = ofnode_read_u32(node, "mclk", (u32 *)&priv->mclk_rate);
	if (ret) {
		debug("Can't parse CLK_DE rate\n");
		return -EINVAL;
	}

	return 0;
}

static int aic_de_register_funcs(struct aic_de_priv *priv)
{
	priv->funcs.set_mode = aic_de_set_mode;
	priv->funcs.clk_enable = aic_de_clk_enable;
	priv->funcs.clk_disable = aic_de_clk_disable;
	priv->funcs.timing_enable = aic_de_timing_enable;
	priv->funcs.update_layer_config = aic_de_update_layer_config;
	priv->funcs.pixclk_rate = aic_de_pixclk_rate;
	priv->funcs.pixclk_enable = aic_de_pixclk_enable;
	return 0;
}

static int aic_de_probe(struct udevice *dev)
{
	struct aic_de_priv *priv = dev_get_priv(dev);
	int ret;

	priv->dev = dev;
	g_aic_de_priv = priv;

	priv->config = (const struct aic_de_configs *)dev_get_driver_data(dev);
	if (!priv->config) {
		debug("Couldn't retrieve aic_de_configs\n");
		return -EINVAL;
	};

	priv->regs = (void *)dev_read_addr(dev);
	if (IS_ERR(priv->regs))
		return PTR_ERR(priv->regs);

	ret = clk_get_by_name(dev, "de0", &priv->mclk);
	if (ret) {
		debug("Couldn't get de0 clock\n");
		return ret;
	}

	ret = clk_get_by_name(dev, "pix", &priv->pclk);
	if (ret) {
		debug("Couldn't get pix clock\n");
		return ret;
	}

	ret = reset_get_by_name(dev, "de0", &priv->reset);
	if (ret) {
		debug("Couldn't get reset line\n");
		return ret;
	}

	ret = aic_de_parse_dt(dev);
	if (ret)
		return ret;

	aic_de_register_funcs(priv);
	return 0;
}

static const struct aicfb_layer_num layer_num = {
	.vi_num = 1,
	.ui_num = 1,
};

static const struct aicfb_layer_capability aicfb_layer_cap[] = {
	{0, AICFB_LAYER_TYPE_VIDEO, 2048, 2048, AICFB_CAP_SCALING_FLAG},
	{1, AICFB_LAYER_TYPE_UI, 2048, 2048,
	AICFB_CAP_4_RECT_WIN_FLAG | AICFB_CAP_ALPHA_FLAG | AICFB_CAP_CK_FLAG},
};

static const struct aic_de_configs aic_de_cfg = {
	.layer_num = &layer_num,
	.cap = aicfb_layer_cap,
};

static const struct udevice_id aic_de_match_ids[] = {
	{.compatible = "artinchip,aic-de-v1.0",
	 .data = (ulong)&aic_de_cfg},
	{ /* sentinel*/ },
};

U_BOOT_DRIVER(disp_engine) = {
	.name      = "disp_engine",
	.id        = UCLASS_DISPLAY,
	.of_match  = aic_de_match_ids,
	.probe     = aic_de_probe,
	.priv_auto = sizeof(struct aic_de_priv),
};
