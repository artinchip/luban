// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Authors:  Huahui Mai <huahui.mai@artinchip.com>
 */

#include <common.h>
#include <display.h>
#include <dm.h>
#include <clk.h>
#include <reset.h>
#include <linux/ioport.h>
#include "hw/lvds_reg.h"
#include "hw/reg_util.h"
#include "aic_com.h"

struct aic_lvds_priv {
	/* di_funcs must be the first member */
	struct di_funcs funcs;
	struct udevice *dev;
	void __iomem *regs;
	struct panel_lvds *lvds;
	struct reset_ctl reset;
	struct clk mclk;
	struct clk sclk;
	ulong sclk_rate;
	u32 sync_ctrl;
	struct lvds_info info;
};
static struct aic_lvds_priv *g_aic_lvds_priv;

/*TODO: static function begin*/

static struct aic_lvds_priv *aic_lvds_request_drvdata(void)
{
	return g_aic_lvds_priv;
}

static void aic_lvds_release_drvdata(void)
{

}

static void lvds_0_lanes(struct aic_lvds_priv *priv, u32 value)
{
	reg_write(priv->regs + LVDS_0_SWAP, value);
}

static void lvds_1_lanes(struct aic_lvds_priv *priv, u32 value)
{
	reg_write(priv->regs + LVDS_1_SWAP, value);
}

static void lvds_0_pol(struct aic_lvds_priv *priv, u32 value)
{
	reg_write(priv->regs + LVDS_0_POL_CTL, value);
}

static void lvds_1_pol(struct aic_lvds_priv *priv, u32 value)
{
	reg_write(priv->regs + LVDS_1_POL_CTL, value);
}

static void lvds_phy_0_init(struct aic_lvds_priv *priv, u32 value)
{
	reg_write(priv->regs + LVDS_0_PHY_CTL, value);
}

static void lvds_phy_1_init(struct aic_lvds_priv *priv, u32 value)
{
	reg_write(priv->regs + LVDS_1_PHY_CTL, value);
}

static void lvds_get_option_config(ofnode np, const char *name, u32 *data)
{
	int i, ret;

	for (i = 0; i < 2; i++) {
		ret = ofnode_read_u32_index(np, name, i, data + i);
		if (ret)
			*(data + i) = 0x0;
	}
}

static int lvds_parse_dt(struct udevice *dev)
{
	struct aic_lvds_priv *priv = dev_get_priv(dev);
	ofnode np = dev_ofnode(dev);

	if (ofnode_read_u32(np, "sync-ctrl", &priv->sync_ctrl))
		priv->sync_ctrl = 1;

	if (ofnode_read_u32(np, "link-swap", &priv->info.link_swap))
		priv->info.link_swap = 0;

	lvds_get_option_config(np, "pols", priv->info.pols);
	lvds_get_option_config(np, "lanes", priv->info.lanes);
	lvds_get_option_config(np, "pctrl", priv->info.phys);
	return 0;
}

static int lvds_clk_enable(void)
{
	struct aic_lvds_priv *priv = aic_lvds_request_drvdata();
	int ret = 0;

	if (priv->sclk_rate)
		clk_set_rate(&priv->sclk, priv->sclk_rate);
	else
		debug("Use the default clock rate %ld\n",
						clk_get_rate(&priv->sclk));

	ret = reset_deassert(&priv->reset);
	if (ret) {
		debug("Couldn't deassert\n");
		aic_lvds_release_drvdata();
		return ret;
	}

	ret = clk_enable(&priv->mclk);
	if (ret) {
		debug("Couldn't enable mclk\n");
		aic_lvds_release_drvdata();
		return ret;
	}

	aic_lvds_release_drvdata();
	return 0;
}

static void
lvds_set_mode(struct aic_lvds_priv *priv, struct panel_lvds *lvds)
{
	reg_set_bits(priv->regs + LVDS_CTL, LVDS_CTL_MODE_MASK,
			 LVDS_CTL_MODE(lvds->mode));
	reg_set_bits(priv->regs + LVDS_CTL, LVDS_CTL_LINK_MASK,
			 LVDS_CTL_LINK(lvds->link_mode));
}

static void
lvds_set_option_config(struct aic_lvds_priv *priv, struct panel_lvds *lvds)
{
	reg_set_bits(priv->regs + LVDS_CTL, LVDS_CTL_SYNC_MODE_MASK,
			 LVDS_CTL_SYNC_MODE_EN(priv->sync_ctrl));
	reg_set_bits(priv->regs + LVDS_CTL, LVDS_CTL_SWAP_MASK,
			 LVDS_CTL_SWAP_EN(priv->info.link_swap));

	lvds_0_lanes(priv, priv->info.lanes[0]);
	lvds_1_lanes(priv, priv->info.lanes[1]);

	lvds_0_pol(priv, priv->info.pols[0]);
	lvds_1_pol(priv, priv->info.pols[1]);

	lvds_phy_0_init(priv, priv->info.phys[0]);
	lvds_phy_1_init(priv, priv->info.phys[1]);
}

static int lvds_enable(void)
{
	struct aic_lvds_priv *priv = aic_lvds_request_drvdata();
	struct panel_lvds *lvds = priv->lvds;

	lvds_set_mode(priv, lvds);
	lvds_set_option_config(priv, lvds);

	reg_set_bit(priv->regs + LVDS_CTL, LVDS_CTL_EN);
	aic_lvds_release_drvdata();
	return 0;
}

static int lvds_pixclk2mclk(ulong pixclk)
{
	s32 ret = 0;
	struct aic_lvds_priv *priv = aic_lvds_request_drvdata();
	struct panel_lvds *lvds = priv->lvds;

	debug("Current pix-clk is %ld\n", pixclk);
	if (lvds->link_mode != DUAL_LINK)
		priv->sclk_rate = pixclk * 7;
	else
		priv->sclk_rate = pixclk * 3 + (pixclk >> 1); // * 3.5

	aic_lvds_release_drvdata();
	return ret;
}

static int lvds_attach_panel(struct aic_panel *panel)
{
	struct aic_lvds_priv *priv = aic_lvds_request_drvdata();

	priv->lvds = panel->lvds;
	return 0;
}

static int init_module_funcs(struct udevice *dev)
{
	struct aic_lvds_priv *priv = dev_get_priv(dev);

	priv->funcs.clk_enable = lvds_clk_enable;
	priv->funcs.enable = lvds_enable;
	priv->funcs.attach_panel = lvds_attach_panel;
	priv->funcs.pixclk2mclk = lvds_pixclk2mclk;
	return 0;
}

static int lvds_probe(struct udevice *dev)
{
	struct aic_lvds_priv *priv = dev_get_priv(dev);
	int ret;

	priv->dev = dev;
	g_aic_lvds_priv = priv;

	priv->regs = (void *)dev_read_addr(dev);
	if (IS_ERR(priv->regs))
		return PTR_ERR(priv->regs);

	ret = clk_get_by_name(dev, "lvds0", &priv->mclk);
	if (ret) {
		debug("Couldn't get lvds0 clock\n");
		return ret;
	}

	ret = clk_get_by_name(dev, "sclk", &priv->sclk);
	if (ret) {
		debug("Couldn't get sclk clock\n");
		return ret;
	}

	ret = reset_get_by_name(dev, "lvds0", &priv->reset);
	if (ret) {
		debug("Couldn't get reset\n");
		return ret;
	}

	ret = lvds_parse_dt(dev);
	if (ret)
		return ret;

	init_module_funcs(dev);
	return 0;
}

static const struct udevice_id lvds_match_ids[] = {
	{.compatible = "artinchip,aic-lvds-v1.0"},
	{ /* sentinel*/ },
};

U_BOOT_DRIVER(disp_lvds) = {
	.name      = "disp_lvds",
	.id        = UCLASS_DISPLAY,
	.of_match  = lvds_match_ids,
	.probe     = lvds_probe,
	.priv_auto = sizeof(struct aic_lvds_priv),
};
