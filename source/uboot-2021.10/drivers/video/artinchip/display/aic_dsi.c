// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Authors: Huahui Mai <huahui.mai@artinchip.com>
 */

#include <common.h>
#include <display.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <clk.h>
#include <reset.h>
#include <linux/ioport.h>

#include "hw/dsi_reg.h"
#include "hw/reg_util.h"
#include "aic_com.h"

#define LANES_MAX_NUM	4
#define LN_ASSIGN_WIDTH	4

struct aic_dsi_priv {
	/* di_funcs must be the first member */
	struct di_funcs funcs;
	struct udevice *dev;
	void __iomem *regs;
	struct reset_ctl reset;
	struct clk mclk;
	struct clk sclk;
	struct panel_dsi *dsi;
	u32 ln_assign;
	u32 ln_polrs;
	bool dc_inv;
	ulong sclk_rate;
	u32 vc_num;
};
static struct aic_dsi_priv *g_aic_dsi_priv;

/*TODO: static function begin*/

static struct aic_dsi_priv *aic_dsi_request_drvdata(void)
{
	return g_aic_dsi_priv;
}

static void aic_dsi_release_drvdata(void)
{

}

static int aic_dsi_parse_dt(struct aic_dsi_priv *priv, struct udevice *dev)
{
	ofnode np = dev_ofnode(dev);
	u32 lane_assignments[LANES_MAX_NUM] = { 0, 1, 2, 3 };
	u32 lane_polarities[LANES_MAX_NUM] = {0};
	int len, num_lanes;
	u32 ln_assign = 0;
	u32 ln_polrs = 0;
	int i;

	priv->dc_inv = ofnode_read_bool(np, "data-clk-inverse");

	len = ofnode_read_size(np, "data-lanes");
	num_lanes = len / sizeof(lane_assignments[0]);
	if (num_lanes > 0 && num_lanes <= LANES_MAX_NUM) {
		ofnode_read_u32_array(np, "data-lanes",
					   lane_assignments, num_lanes);
		ofnode_read_u32_array(np, "lane-polarities",
					   lane_polarities, num_lanes);
	} else {
		num_lanes = LANES_MAX_NUM;
		dev_dbg(dev,
		"failed to find data lane assignments, using default\n");
	}

	/* Convert into register format */
	for (i = 0 ; i < num_lanes; i++) {
		ln_assign |= lane_assignments[i] << (LN_ASSIGN_WIDTH * i);
		ln_polrs |= lane_polarities[i] << lane_assignments[i];
	}

	priv->ln_assign = ln_assign;
	priv->ln_polrs = ln_polrs;

	return 0;
}

static int aic_dsi_clk_enable(void)
{
	struct aic_dsi_priv *priv = aic_dsi_request_drvdata();
	int ret = 0;

	if (priv->sclk_rate)
		clk_set_rate(&priv->sclk, priv->sclk_rate);
	else
		debug("Use the default clock rate %ld\n",
			clk_get_rate(&priv->sclk));

	ret = reset_deassert(&priv->reset);
	if (ret) {
		debug("Couldn't deassert\n");
		aic_dsi_release_drvdata();
		return ret;
	}

	ret = clk_enable(&priv->mclk);
	if (ret) {
		debug("Couldn't enable mclk\n");
		aic_dsi_release_drvdata();
		return ret;
	}

	aic_dsi_release_drvdata();
	return 0;
}

static int aic_dsi_enable(void)
{
	struct aic_dsi_priv *priv = aic_dsi_request_drvdata();
	struct panel_dsi *dsi = priv->dsi;

	reg_set_bit(priv->regs + DSI_CTL, DSI_CTL_EN);

	dsi_set_lane_assign(priv->regs, priv->ln_assign);
	dsi_set_lane_polrs(priv->regs, priv->ln_polrs);
	dsi_set_data_clk_polrs(priv->regs, priv->dc_inv);

	dsi_set_clk_div(priv->regs, priv->sclk_rate);
	dsi_pkg_init(priv->regs);
	dsi_phy_init(priv->regs, priv->sclk_rate, dsi->lane_num);
	dsi_hs_clk(priv->regs, 1);

	aic_dsi_release_drvdata();
	return 0;
}

static int aic_dsi_pixclk2mclk(ulong pixclk)
{
	s32 ret = 0;
	s32 div[DSI_MAX_LANE_NUM] = {24, 12, 8, 6};
	struct aic_dsi_priv *priv = aic_dsi_request_drvdata();
	struct panel_dsi *dsi = priv->dsi;

	dev_dbg(priv->dev, "Current pix-clk is %ld\n", pixclk);
	if (dsi->lane_num <= DSI_MAX_LANE_NUM)
		priv->sclk_rate = pixclk * div[dsi->lane_num - 1];
	else {
		debug("Invalid lane number %d\n", dsi->lane_num);
		ret = -EINVAL;
	}

	aic_dsi_release_drvdata();
	return ret;
}

static int aic_dsi_set_vm(struct fb_videomode *vm, int enable)
{
	struct aic_dsi_priv *priv = aic_dsi_request_drvdata();
	struct panel_dsi *dsi = priv->dsi;

	if (enable) {
		dsi_dcs_lw(priv->regs, false);
		dsi_set_vm(priv->regs, dsi->mode, dsi->format,
			dsi->lane_num, priv->vc_num, vm);
	} else {
		dsi_set_vm(priv->regs, DSI_MOD_CMD_MODE, dsi->format,
			dsi->lane_num, priv->vc_num, vm);
		dsi_dcs_lw(priv->regs, true);
	}

	aic_dsi_release_drvdata();
	return 0;
}

static int aic_dsi_send_cmd(u32 dt, const u8 *data, u32 len)
{
	struct aic_dsi_priv *priv = aic_dsi_request_drvdata();

	dsi_cmd_wr(priv->regs, dt, priv->vc_num, data, len);
	aic_dsi_release_drvdata();
	return 0;
}

static int aic_dsi_attach_panel(struct aic_panel *panel)
{
	struct aic_dsi_priv *priv = aic_dsi_request_drvdata();
	ofnode np = dev_ofnode(panel->dev);
	int ret;

	ret = ofnode_read_u32(np, "reg", &priv->vc_num);
	if (ret) {
		debug("Can't parse DSI vc num property\n");
		priv->vc_num = 0;
	}

	priv->dsi = panel->dsi;
	return 0;
}

static void aic_dsi_register_funcs(struct aic_dsi_priv *priv)
{
	struct di_funcs *f = &priv->funcs;

	f->clk_enable = aic_dsi_clk_enable;
	f->enable = aic_dsi_enable;
	f->attach_panel = aic_dsi_attach_panel;
	f->pixclk2mclk = aic_dsi_pixclk2mclk;
	f->set_videomode = aic_dsi_set_vm;
	f->send_cmd = aic_dsi_send_cmd;
}

static int aic_dsi_probe(struct udevice *dev)
{
	struct aic_dsi_priv *priv = dev_get_priv(dev);
	int ret;

	debug("%s\n", __func__);

	priv->dev = dev;

	priv->regs = (void *)dev_read_addr(dev);
	if (IS_ERR(priv->regs))
		return PTR_ERR(priv->regs);

	ret = clk_get_by_name(dev, "dsi0", &priv->mclk);
	if (ret) {
		debug("Couldn't get dsi0 clock\n");
		return ret;
	}

	ret = clk_get_by_name(dev, "sclk", &priv->sclk);
	if (ret) {
		debug("Couldn't get sclk clock\n");
		return ret;
	}

	ret = reset_get_by_name(dev, "dsi0", &priv->reset);
	if (ret) {
		debug("Couldn't get reset line\n");
		return ret;
	}

	g_aic_dsi_priv = priv;
	ret = aic_dsi_parse_dt(priv, dev);
	if (ret)
		return ret;

	aic_dsi_register_funcs(priv);
	return 0;
}

static const struct udevice_id aic_dsi_match_ids[] = {
	{ .compatible = "artinchip,aic-mipi-dsi-v1.0" },
	{ /* sentinel*/ },
};

U_BOOT_DRIVER(disp_mipi_dsi) = {
	.name      = "disp_mipi_dsi",
	.id        = UCLASS_DISPLAY,
	.of_match  = aic_dsi_match_ids,
	.probe     = aic_dsi_probe,
	.priv_auto = sizeof(struct aic_dsi_priv),
};
