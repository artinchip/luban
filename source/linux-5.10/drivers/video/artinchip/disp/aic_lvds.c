// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2022 ArtInChip Technology Co., Ltd.
 * Authors:  Ning Fang <ning.fang@artinchip.com>
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
#include "hw/lvds_reg.h"
#include "hw/reg_util.h"
#include "aic_com.h"

struct aic_lvds_comp {
	/* di_funcs must be the first member */
	struct di_funcs funcs;
	struct device *dev;
	void __iomem *regs;
	struct reset_control *reset;
	struct clk *mclk;
	struct clk *sclk;
	struct panel_lvds *lvds;
	ulong sclk_rate;
	u32 sync_ctrl;
	struct lvds_info info;
};
static struct aic_lvds_comp *g_aic_lvds_comp;

/*TODO: static function begin*/

static struct aic_lvds_comp *aic_lvds_request_drvdata(void)
{
	return g_aic_lvds_comp;
}

static void aic_lvds_release_drvdata(void)
{

}

int lvds_0_swap(struct device *dev, u32 value)
{
	struct aic_lvds_comp *comp = dev_get_drvdata(dev);

	reg_write(comp->regs + LVDS_0_SWAP, value);
	return 0;
}

int lvds_1_swap(struct device *dev, u32 value)
{
	struct aic_lvds_comp *comp = dev_get_drvdata(dev);

	reg_write(comp->regs + LVDS_1_SWAP, value);
	return 0;
}

static void lvds_0_pol(struct aic_lvds_comp *comp, u32 value)
{
	reg_write(comp->regs + LVDS_0_POL_CTL, value);
}

static void lvds_1_pol(struct aic_lvds_comp *comp, u32 value)
{
	reg_write(comp->regs + LVDS_1_POL_CTL, value);
}

static void lvds_phy_0_init(struct aic_lvds_comp *comp, u32 value)
{
	reg_write(comp->regs + LVDS_0_PHY_CTL, value);
}

static void lvds_phy_1_init(struct aic_lvds_comp *comp, u32 value)
{
	reg_write(comp->regs + LVDS_1_PHY_CTL, value);
}

/*TODO: static function end*/

static int lvds_parse_dt(struct aic_lvds_comp *comp, struct device *dev)
{
	struct device_node *np = dev->of_node;

	/* enable lvds sync mode by default */
	if (of_property_read_u32(np, "sync-ctrl", &comp->sync_ctrl))
		comp->sync_ctrl = 1;

	/* optional: */
	if (of_property_read_u32(np, "pols", &comp->info.pols))
		comp->info.pols = 0;

	if (of_property_read_u32(np, "phys", &comp->info.phys))
		comp->info.phys = 0xFA;

	if (of_property_read_u32(np, "swap", &comp->info.swap))
		comp->info.swap = 0;

	if (of_property_read_u32(np, "link-swap", &comp->info.link_swap))
		comp->info.link_swap = 0;

	return 0;
}

static int lvds_clk_enable(void)
{
	struct aic_lvds_comp *comp = aic_lvds_request_drvdata();
	int ret = 0;

	if (comp->sclk_rate)
		clk_set_rate(comp->sclk, comp->sclk_rate);
	else
		dev_warn(comp->dev, "Use the default clock rate %ld\n",
			clk_get_rate(comp->sclk));

	ret = reset_control_deassert(comp->reset);
	if (ret) {
		dev_err(comp->dev, "Couldn't deassert\n");
		aic_lvds_release_drvdata();
		return ret;
	}

	ret = clk_prepare_enable(comp->mclk);
	if (ret) {
		dev_err(comp->dev, "Couldn't enable mclk\n");
		aic_lvds_release_drvdata();
		return ret;
	}

	aic_lvds_release_drvdata();
	return 0;
}

static int lvds_clk_disable(void)
{
	struct aic_lvds_comp *comp = aic_lvds_request_drvdata();

	clk_disable_unprepare(comp->mclk);
	reset_control_assert(comp->reset);
	aic_lvds_release_drvdata();
	return 0;
}

static int lvds_enable(void)
{
	struct aic_lvds_comp *comp = aic_lvds_request_drvdata();
	struct panel_lvds *lvds = comp->lvds;

	if (comp->info.swap) {
		lvds_0_swap(comp->dev, comp->info.swap);
		lvds_1_swap(comp->dev, comp->info.swap);
	}
	if (comp->info.pols) {
		lvds_0_pol(comp, comp->info.pols);
		lvds_1_pol(comp, comp->info.pols);
	}
	if (comp->info.phys) {
		lvds_phy_0_init(comp, comp->info.phys);
		lvds_phy_1_init(comp, comp->info.phys);
	}

	reg_set_bits(comp->regs + LVDS_CTL,
			 LVDS_CTL_MODE_MASK
			 | LVDS_CTL_LINK_MASK
			 | LVDS_CTL_SWAP_MASK
			 | LVDS_CTL_SYNC_MODE_MASK,
			 LVDS_CTL_MODE(lvds->mode)
			 | LVDS_CTL_LINK(lvds->link_mode)
			 | LVDS_CTL_SWAP_EN(comp->info.link_swap)
			 | LVDS_CTL_SYNC_MODE_EN(comp->sync_ctrl));

	reg_set_bit(comp->regs + LVDS_CTL, LVDS_CTL_EN);
	aic_lvds_release_drvdata();
	return 0;
}

static int lvds_disable(void)
{
	struct aic_lvds_comp *comp = aic_lvds_request_drvdata();

	reg_clr_bit(comp->regs + LVDS_CTL, LVDS_CTL_EN);
	aic_lvds_release_drvdata();
	return 0;
}

static int lvds_pixclk2mclk(ulong pixclk)
{
	s32 ret = 0;
	struct aic_lvds_comp *comp = aic_lvds_request_drvdata();
	struct panel_lvds *lvds = comp->lvds;

	dev_dbg(comp->dev, "Current pix-clk is %ld\n", pixclk);
	if (lvds->link_mode != DUAL_LINK)
		comp->sclk_rate = pixclk * 7;
	else
		comp->sclk_rate = pixclk * 3 + (pixclk >> 1); // * 3.5

	aic_lvds_release_drvdata();
	return ret;

}

static int lvds_attach_panel(struct aic_panel *panel)
{
	struct aic_lvds_comp *comp = aic_lvds_request_drvdata();

	comp->lvds = panel->lvds;
	return 0;
}

static int init_module_funcs(struct device *dev)
{
	struct aic_lvds_comp *comp = dev_get_drvdata(dev);

	comp->funcs.clk_enable = lvds_clk_enable;
	comp->funcs.clk_disable = lvds_clk_disable;
	comp->funcs.enable = lvds_enable;
	comp->funcs.disable = lvds_disable;
	comp->funcs.attach_panel = lvds_attach_panel;
	comp->funcs.pixclk2mclk = lvds_pixclk2mclk;
	return 0;
}

static int lvds_bind(struct device *dev, struct device *master,
		     void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct aic_lvds_comp *comp;
	struct resource *res;
	void __iomem *regs;
	int ret;

	comp = devm_kzalloc(dev, sizeof(*comp), GFP_KERNEL);
	if (!comp)
		return -ENOMEM;

	comp->dev = dev;
	dev_set_drvdata(dev, comp);
	g_aic_lvds_comp = comp;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	comp->regs = regs;

	comp->mclk = devm_clk_get(dev, "lvds0");
	if (IS_ERR(comp->mclk)) {
		dev_err(dev, "Couldn't get lvds0 clock\n");
		return PTR_ERR(comp->mclk);
	}

	comp->sclk = devm_clk_get(dev, "sclk");
	if (IS_ERR(comp->sclk)) {
		dev_err(dev, "Couldn't get sclk clock\n");
		return PTR_ERR(comp->sclk);
	}

	comp->reset = devm_reset_control_get(dev, "lvds0");
	if (IS_ERR(comp->reset)) {
		dev_err(dev, "Couldn't get reset line\n");
		return PTR_ERR(comp->reset);
	}

	ret = lvds_parse_dt(comp, dev);
	if (ret)
		return ret;

	init_module_funcs(dev);
	return 0;
}

static void lvds_unbind(struct device *dev, struct device *master,
			void *data)
{
}

static const struct component_ops lvds_com_ops = {
	.bind	= lvds_bind,
	.unbind	= lvds_unbind,
};

static int lvds_probe(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "%s()\n", __func__);
	return component_add(&pdev->dev, &lvds_com_ops);
}

static int lvds_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &lvds_com_ops);
	return 0;
}

static const struct of_device_id lvds_match_table[] = {
	{.compatible = "artinchip,aic-lvds-v1.0",},
	{},
};

MODULE_DEVICE_TABLE(of, lvds_match_table);

static struct platform_driver lvds_driver = {
	.probe = lvds_probe,
	.remove = lvds_remove,
	.driver = {
		.name = "disp lvds",
		.of_match_table	= lvds_match_table,
	},
};

module_platform_driver(lvds_driver);

MODULE_AUTHOR("Ning Fang <ning.fang@artinchip.com>");
MODULE_DESCRIPTION("AIC disp lvds driver");
MODULE_ALIAS("platform:lvds");
MODULE_LICENSE("GPL v2");
