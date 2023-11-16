// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2022 ArtInChip Technology Co., Ltd.
 * Authors: matteo <duanmt@artinchip.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/component.h>
#include <linux/reset.h>
#include <linux/pinctrl/consumer.h>
#include <dt-bindings/display/artinchip,aic-disp.h>

#include "hw/rgb_reg.h"
#include "hw/reg_util.h"
#include "aic_com.h"

struct aic_rgb_comp {
	/* di_funcs must be the first member */
	struct di_funcs funcs;
	struct device *dev;
	void __iomem *regs;
	struct reset_control *reset;
	struct clk *mclk;
	struct clk *sclk;
	struct panel_rgb *rgb;
	ulong sclk_rate;
};
static struct aic_rgb_comp *g_aic_rgb_comp;

/*TODO: static function begin*/

static struct aic_rgb_comp *aic_rgb_request_drvdata(void)
{
	return g_aic_rgb_comp;
}

static void aic_rgb_release_drvdata(void)
{

}

static void aic_rgb_swap(void)
{
	struct aic_rgb_comp *comp = aic_rgb_request_drvdata();
	struct panel_rgb *rgb = comp->rgb;

	if (rgb->data_mirror)
		reg_set_bits(comp->regs + RGB_DATA_SEQ_SEL,
			RGB_DATA_OUT_SEL_MASK, RGB_DATA_OUT_SEL(7));

	if (rgb->data_order)
		reg_write(comp->regs + RGB_DATA_SEL, rgb->data_order);

	if (rgb->clock_phase)
		reg_set_bits(comp->regs + RGB_CLK_CTL,
			CKO_PHASE_SEL_MASK,
			CKO_PHASE_SEL(rgb->clock_phase));

	aic_rgb_release_drvdata();
}

static int aic_rgb_clk_enable(void)
{
	struct aic_rgb_comp *comp = aic_rgb_request_drvdata();
	int ret = 0;

	if (comp->sclk_rate)
		clk_set_rate(comp->sclk, comp->sclk_rate);
	else
		dev_warn(comp->dev, "Use the default clock rate %ld\n",
			clk_get_rate(comp->sclk));

	ret = reset_control_deassert(comp->reset);
	if (ret) {
		dev_err(comp->dev, "Couldn't deassert\n");
		aic_rgb_release_drvdata();
		return ret;
	}

	ret = clk_prepare_enable(comp->mclk);
	if (ret) {
		dev_err(comp->dev, "Couldn't enable mclk\n");
		aic_rgb_release_drvdata();
		return ret;
	}

	aic_rgb_release_drvdata();
	return 0;
}

static int aic_rgb_clk_disable(void)
{
	struct aic_rgb_comp *comp = aic_rgb_request_drvdata();

	clk_disable_unprepare(comp->mclk);
	reset_control_assert(comp->reset);
	aic_rgb_release_drvdata();
	return 0;
}

static int aic_rgb_enable(void)
{
	struct aic_rgb_comp *comp = aic_rgb_request_drvdata();
	struct panel_rgb *rgb = comp->rgb;

	pinctrl_pm_select_default_state(comp->dev);
	reg_set_bits(comp->regs + RGB_LCD_CTL,
			RGB_LCD_CTL_MODE_MASK,
			RGB_LCD_CTL_MODE(rgb->mode));

	switch (rgb->mode) {
	case PRGB:
		reg_set_bits(comp->regs + RGB_LCD_CTL,
				RGB_LCD_CTL_PRGB_MODE_MASK,
				RGB_LCD_CTL_PRGB_MODE(rgb->format));
		break;
	case SRGB:
		if (rgb->format)
			reg_set_bit(comp->regs + RGB_LCD_CTL,
				RGB_LCD_CTL_SRGB_MODE);
		break;
	default:
		dev_err(comp->dev, "Invalid mode %d\n", rgb->mode);
		break;
	}

	aic_rgb_swap();
	reg_set_bit(comp->regs + RGB_LCD_CTL, RGB_LCD_CTL_EN);
	aic_rgb_release_drvdata();
	return 0;
}

static int aic_rgb_disable(void)
{
	struct aic_rgb_comp *comp = aic_rgb_request_drvdata();

	reg_clr_bit(comp->regs + RGB_LCD_CTL, RGB_LCD_CTL_EN);
	aic_rgb_release_drvdata();
	pinctrl_pm_select_sleep_state(comp->dev);
	return 0;
}

static int aic_rgb_pixclk2mclk(ulong pixclk)
{
	s32 ret = 0;
	struct aic_rgb_comp *comp = aic_rgb_request_drvdata();
	struct panel_rgb *rgb = comp->rgb;

	dev_dbg(comp->dev, "Current pix-clk is %ld\n", pixclk);
	if (rgb->mode == PRGB)
		comp->sclk_rate = pixclk * 4;
	else if (rgb->mode == SRGB)
		comp->sclk_rate = pixclk * 12;

	aic_rgb_release_drvdata();
	return ret;
}

static int aic_rgb_attach_panel(struct aic_panel *panel)
{
	struct aic_rgb_comp *comp = aic_rgb_request_drvdata();

	comp->rgb = panel->rgb;
	return 0;
}

static int init_module_funcs(struct device *dev)
{
	struct aic_rgb_comp *comp = dev_get_drvdata(dev);

	comp->funcs.clk_enable = aic_rgb_clk_enable;
	comp->funcs.clk_disable = aic_rgb_clk_disable;
	comp->funcs.enable = aic_rgb_enable;
	comp->funcs.disable = aic_rgb_disable;
	comp->funcs.attach_panel = aic_rgb_attach_panel;
	comp->funcs.pixclk2mclk = aic_rgb_pixclk2mclk;
	return 0;
}

static int aic_rgb_bind(struct device *dev, struct device *master,
		     void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct aic_rgb_comp *comp;
	struct resource *res;
	void __iomem *regs;

	comp = devm_kzalloc(dev, sizeof(*comp), GFP_KERNEL);
	if (!comp)
		return -ENOMEM;

	comp->dev = dev;
	dev_set_drvdata(dev, comp);
	g_aic_rgb_comp = comp;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	comp->regs = regs;

	comp->mclk = devm_clk_get(dev, "rgb0");
	if (IS_ERR(comp->mclk)) {
		dev_err(dev, "Couldn't get rgb0 clock\n");
		return PTR_ERR(comp->mclk);
	}

	comp->sclk = devm_clk_get(dev, "sclk");
	if (IS_ERR(comp->sclk)) {
		dev_err(dev, "Couldn't get sclk clock\n");
		return PTR_ERR(comp->sclk);
	}

	comp->reset = devm_reset_control_get(dev, "rgb0");
	if (IS_ERR(comp->reset)) {
		dev_err(dev, "Couldn't get reset line\n");
		return PTR_ERR(comp->reset);
	}

	init_module_funcs(dev);
	return 0;
}

static void aic_rgb_unbind(struct device *dev, struct device *master,
			void *data)
{
}

static const struct component_ops aic_rgb_com_ops = {
	.bind	= aic_rgb_bind,
	.unbind	= aic_rgb_unbind,
};

static int aic_rgb_probe(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "%s()\n", __func__);
	return component_add(&pdev->dev, &aic_rgb_com_ops);
}

static int aic_rgb_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &aic_rgb_com_ops);
	return 0;
}

static const struct of_device_id aic_rgb_match_table[] = {
	{.compatible = "artinchip,aic-rgb-v1.0"},
	{},
};

MODULE_DEVICE_TABLE(of, aic_rgb_match_table);

static struct platform_driver aic_rgb_driver = {
	.probe = aic_rgb_probe,
	.remove = aic_rgb_remove,
	.driver = {
		.name = "disp rgb",
		.of_match_table	= aic_rgb_match_table,
	},
};

module_platform_driver(aic_rgb_driver);

MODULE_AUTHOR("matteo<duanmt@artinchip.com>");
MODULE_DESCRIPTION("AIC disp RGB driver");
MODULE_ALIAS("platform:rgb");
MODULE_LICENSE("GPL v2");
