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

#include "hw/rgb_reg.h"
#include "hw/reg_util.h"
#include "aic_com.h"

struct aic_rgb_priv {
	/* di_funcs must be the first member */
	struct di_funcs funcs;
	struct udevice *dev;
	void __iomem *regs;
	struct reset_ctl reset;
	struct clk mclk;
	struct clk sclk;
	ulong sclk_rate;
	struct panel_rgb *rgb;
};
static struct aic_rgb_priv *g_aic_rgb_priv;

static struct aic_rgb_priv *aic_rgb_request_drvdata(void)
{
	return g_aic_rgb_priv;
}

static void aic_rgb_release_drvdata(void)
{

}

static void aic_rgb_swap(void)
{
	struct aic_rgb_priv *priv = aic_rgb_request_drvdata();
	struct panel_rgb *rgb = priv->rgb;

	if (rgb->data_mirror)
		reg_set_bits(priv->regs + RGB_DATA_SEQ_SEL,
			RGB_DATA_OUT_SEL_MASK, RGB_DATA_OUT_SEL(7));

	if (rgb->data_order)
		reg_write(priv->regs + RGB_DATA_SEL, rgb->data_order);

	if (rgb->clock_phase)
		reg_set_bits(priv->regs + RGB_CLK_CTL,
			CKO_PHASE_SEL_MASK,
			CKO_PHASE_SEL(rgb->clock_phase));

	aic_rgb_release_drvdata();
}

static int aic_rgb_clk_enable(void)
{
	struct aic_rgb_priv *priv = aic_rgb_request_drvdata();
	int ret = 0;

	if (priv->sclk_rate)
		clk_set_rate(&priv->sclk, priv->sclk_rate);
	else
		debug("Use the default clock rate %ld\n",
						clk_get_rate(&priv->sclk));

	ret = reset_deassert(&priv->reset);
	if (ret) {
		debug("Couldn't deassert\n");
		aic_rgb_release_drvdata();
		return ret;
	}

	ret = clk_enable(&priv->mclk);
	if (ret) {
		debug("Couldn't enable mclk\n");
		aic_rgb_release_drvdata();
		return ret;
	}

	aic_rgb_release_drvdata();
	return 0;
}

static int aic_rgb_enable(void)
{
	struct aic_rgb_priv *priv = aic_rgb_request_drvdata();
	struct panel_rgb *rgb = priv->rgb;

	aic_rgb_swap();
	reg_set_bits(priv->regs + RGB_LCD_CTL,
			RGB_LCD_CTL_MODE_MASK,
			RGB_LCD_CTL_MODE(rgb->mode));

	switch (rgb->mode) {
	case PRGB:
		reg_set_bits(priv->regs + RGB_LCD_CTL,
				RGB_LCD_CTL_PRGB_MODE_MASK,
				RGB_LCD_CTL_PRGB_MODE(rgb->format));
		break;
	case SRGB:
		if (rgb->format)
			reg_set_bit(priv->regs + RGB_LCD_CTL,
				RGB_LCD_CTL_SRGB_MODE);
		break;
	default:
		debug("Invalid mode %d\n", rgb->mode);
		break;
	}

	reg_set_bit(priv->regs + RGB_LCD_CTL, RGB_LCD_CTL_EN);

	aic_rgb_release_drvdata();
	return 0;
}

static int aic_rgb_pixclk2mclk(ulong pixclk)
{
	s32 ret = 0;
	struct aic_rgb_priv *priv = aic_rgb_request_drvdata();
	struct panel_rgb *rgb = priv->rgb;

	debug("Current pix-clk is %ld\n", pixclk);
	if (rgb->mode == PRGB)
		priv->sclk_rate = pixclk * 4;
	else if (rgb->mode == SRGB)
		priv->sclk_rate = pixclk * 12;

	aic_rgb_release_drvdata();
	return ret;
}

static int aic_rgb_attach_panel(struct aic_panel *panel)
{
	struct aic_rgb_priv *priv = aic_rgb_request_drvdata();

	priv->rgb = panel->rgb;
	return 0;
}

static int init_module_funcs(struct udevice *dev)
{
	struct aic_rgb_priv *priv = dev_get_priv(dev);

	priv->funcs.clk_enable = aic_rgb_clk_enable;
	priv->funcs.enable = aic_rgb_enable;
	priv->funcs.attach_panel = aic_rgb_attach_panel;
	priv->funcs.pixclk2mclk = aic_rgb_pixclk2mclk;
	return 0;
}

static int aic_rgb_probe(struct udevice *dev)
{
	struct aic_rgb_priv *priv = dev_get_priv(dev);
	int ret;

	priv->dev = dev;

	priv->regs = (void *)dev_read_addr(dev);
	if (IS_ERR(priv->regs))
		return PTR_ERR(priv->regs);

	ret = clk_get_by_name(dev, "rgb0", &priv->mclk);
	if (ret) {
		debug("Couldn't get rgb0 clock\n");
		return ret;
	}

	ret = clk_get_by_name(dev, "sclk", &priv->sclk);
	if (ret) {
		debug("Couldn't get sclk clock\n");
		return ret;
	}

	ret = reset_get_by_name(dev, "rgb0", &priv->reset);
	if (ret) {
		debug("Couldn't get reset line\n");
		return ret;
	}

	g_aic_rgb_priv = priv;
	init_module_funcs(dev);
	return 0;
}

static const struct udevice_id aic_rgb_match_ids[] = {
	{ .compatible = "artinchip,aic-rgb-v1.0" },
	{ /* sentinel*/ },
};

U_BOOT_DRIVER(disp_rgb) = {
	.name      = "disp_rgb",
	.id        = UCLASS_DISPLAY,
	.of_match  = aic_rgb_match_ids,
	.probe     = aic_rgb_probe,
	.priv_auto = sizeof(struct aic_rgb_priv),
};
