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
#include <linux/interrupt.h>
#include "hw/dsi_reg.h"
#include "hw/reg_util.h"
#include <video/mipi_display.h>
#include "aic_com.h"

#define LANES_MAX_NUM	4
#define LN_ASSIGN_WIDTH	4

struct aic_dsi_comp {
	/* di_funcs must be the first member */
	struct di_funcs funcs;
	struct device *dev;
	void __iomem *regs;
	struct reset_control *reset;
	struct clk *mclk;
	struct clk *sclk;
	struct panel_dsi *dsi;
	u32 ln_assign;
	u32 ln_polrs;
	bool dc_inv;
	ulong sclk_rate;
	s32 irq;
	u32 vc_num;
};
static struct aic_dsi_comp *g_aic_dsi_comp;

/*TODO: static function begin*/

static struct aic_dsi_comp *aic_dsi_request_drvdata(void)
{
	return g_aic_dsi_comp;
}

static void aic_dsi_release_drvdata(void)
{

}

static int aic_dsi_parse_dt(struct aic_dsi_comp *comp, struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 lane_assignments[LANES_MAX_NUM] = { 0, 1, 2, 3 };
	u32 lane_polarities[LANES_MAX_NUM] = {0};
	int num_lanes;
	u32 ln_assign = 0;
	u32 ln_polrs = 0;
	int i;

	comp->dc_inv = of_property_read_bool(np, "data-clk-inverse");

	num_lanes = of_property_count_u32_elems(np, "data-lanes");
	if (num_lanes > 0 && num_lanes <= LANES_MAX_NUM) {
		of_property_read_u32_array(np, "data-lanes",
					   lane_assignments, num_lanes);
		of_property_read_u32_array(np, "lane-polarities",
					   lane_polarities, num_lanes);
	} else {
		num_lanes = LANES_MAX_NUM;
		dev_dbg(dev,
		"failed to find data lane assignments, using default\n");
	}
	of_node_put(np);

	/* Convert into register format */
	for (i = 0 ; i < num_lanes; i++) {
		ln_assign |= lane_assignments[i] << (LN_ASSIGN_WIDTH * i);
		ln_polrs |= lane_polarities[i] << lane_assignments[i];
	}

	comp->ln_assign = ln_assign;
	comp->ln_polrs = ln_polrs;

	return 0;
}

int aic_dsi_set_mode(struct device *dev, enum dsi_mode mode)
{
	// struct aic_dsi_comp *comp = dev_get_drvdata(dev);

	// comp->mode = mode;
	return 0;
}

static int aic_dsi_clk_enable(void)
{
	struct aic_dsi_comp *comp = aic_dsi_request_drvdata();
	int ret = 0;

	if (comp->sclk_rate)
		clk_set_rate(comp->sclk, comp->sclk_rate);
	else
		dev_warn(comp->dev, "Use the default clock rate %ld\n",
			clk_get_rate(comp->sclk));

	ret = reset_control_deassert(comp->reset);
	if (ret) {
		dev_err(comp->dev, "Couldn't deassert\n");
		aic_dsi_release_drvdata();
		return ret;
	}

	ret = clk_prepare_enable(comp->mclk);
	if (ret) {
		dev_err(comp->dev, "Couldn't enable mclk\n");
		aic_dsi_release_drvdata();
		return ret;
	}

	aic_dsi_release_drvdata();
	return 0;
}

static int aic_dsi_clk_disable(void)
{
	struct aic_dsi_comp *comp = aic_dsi_request_drvdata();

	clk_disable_unprepare(comp->mclk);
	reset_control_assert(comp->reset);
	aic_dsi_release_drvdata();
	return 0;
}

static int aic_dsi_enable(void)
{
	struct aic_dsi_comp *comp = aic_dsi_request_drvdata();
	struct panel_dsi *dsi = comp->dsi;

	reg_set_bit(comp->regs + DSI_CTL, DSI_CTL_EN);

	dsi_set_lane_assign(comp->regs, comp->ln_assign);
	dsi_set_lane_polrs(comp->regs, comp->ln_polrs);
	dsi_set_data_clk_polrs(comp->regs, comp->dc_inv);

	dsi_set_clk_div(comp->regs, comp->sclk_rate);
	dsi_pkg_init(comp->regs);
	dsi_phy_init(comp->regs, comp->sclk_rate, dsi->lane_num);
	dsi_hs_clk(comp->regs, 1);

	aic_dsi_release_drvdata();
	return 0;
}

static int aic_dsi_disable(void)
{
	struct aic_dsi_comp *comp = aic_dsi_request_drvdata();

	reg_clr_bit(comp->regs + DSI_CTL, DSI_CTL_EN);
	aic_dsi_release_drvdata();
	return 0;
}

static int aic_dsi_pixclk2mclk(ulong pixclk)
{
	s32 ret = 0;
	s32 div[DSI_MAX_LANE_NUM] = {24, 24, 18, 16};
	struct aic_dsi_comp *comp = aic_dsi_request_drvdata();
	struct panel_dsi *dsi = comp->dsi;

	dev_dbg(comp->dev, "Current pix-clk is %ld\n", pixclk);
	if (dsi->lane_num <= DSI_MAX_LANE_NUM)
		comp->sclk_rate = pixclk * div[dsi->format] / dsi->lane_num;
	else {
		dev_err(comp->dev, "Invalid lane number %d\n", dsi->lane_num);
		ret = -EINVAL;
	}

	aic_dsi_release_drvdata();
	return ret;
}

static int aic_dsi_set_vm(struct videomode *vm, int enable)
{
	struct aic_dsi_comp *comp = aic_dsi_request_drvdata();
	struct panel_dsi *dsi = comp->dsi;

	if (enable) {
		dsi_dcs_lw(comp->regs, false);
		dsi_set_vm(comp->regs, dsi->mode, dsi->format,
			dsi->lane_num, comp->vc_num, vm);
	} else {
		dsi_set_vm(comp->regs, DSI_MOD_CMD_MODE, dsi->format,
			dsi->lane_num, comp->vc_num, vm);
		dsi_dcs_lw(comp->regs, true);
	}

	aic_dsi_release_drvdata();
	return 0;
}

static int aic_dsi_send_cmd(u32 dt, const u8 *data, u32 len)
{
	struct aic_dsi_comp *comp = aic_dsi_request_drvdata();

	dsi_cmd_wr(comp->regs, dt, comp->vc_num, data, len);
	aic_dsi_release_drvdata();
	return 0;
}

static int aic_dsi_attach_panel(struct aic_panel *panel)
{
	struct aic_dsi_comp *comp = aic_dsi_request_drvdata();
	struct device_node *np = panel->dev->of_node;
	int ret;

	/* virtual channel num */
	ret = of_property_read_u32(np, "reg", &comp->vc_num);
	if (ret) {
		pr_debug("use default virtual channel 0\n");
		comp->vc_num = 0;
	}

	comp->dsi = panel->dsi;
	return 0;
}

static void aic_dsi_register_funcs(struct aic_dsi_comp *comp)
{
	struct di_funcs *f = &comp->funcs;

	f->clk_enable = aic_dsi_clk_enable;
	f->clk_disable = aic_dsi_clk_disable;
	f->enable = aic_dsi_enable;
	f->disable = aic_dsi_disable;
	f->attach_panel = aic_dsi_attach_panel;
	f->pixclk2mclk = aic_dsi_pixclk2mclk;
	f->set_videomode = aic_dsi_set_vm;
	f->send_cmd = aic_dsi_send_cmd;
}

static irqreturn_t aic_dsi_handler(int irq, void *ctx)
{
	//struct aic_dsi_comp *comp = ctx;
	//unsigned int status;
	// TODO: check interrupt
	return IRQ_HANDLED;
}

static ssize_t
reg_show(struct device *dev, struct device_attribute *devattr, char *buf)
{
	struct aic_dsi_comp *comp = aic_dsi_request_drvdata();
	return sprintf(buf, "%#x\n", readl(comp->regs + DSI_GEN_PD_CFG));
}
static ssize_t reg_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct aic_dsi_comp *comp = aic_dsi_request_drvdata();
	unsigned long val;
	int err;

	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	dsi_cmd_wr(comp->regs, MIPI_DSI_DCS_READ, 0, (u8[]){ val }, 1);
	return count;
}
static DEVICE_ATTR_RW(reg);

static struct attribute *aic_dsi_attrs[] = {
	&dev_attr_reg.attr,
	NULL
};

static const struct attribute_group aic_dsi_attr_group = {
	.attrs = aic_dsi_attrs,
};

static int aic_dsi_bind(struct device *dev, struct device *master,
		     void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct aic_dsi_comp *comp;
	struct resource *res;
	void __iomem *regs;
	int irq;
	int ret;

	comp = devm_kzalloc(dev, sizeof(*comp), GFP_KERNEL);
	if (!comp)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	comp->regs = regs;

	comp->mclk = devm_clk_get(dev, "dsi0");
	if (IS_ERR(comp->mclk)) {
		dev_err(dev, "Couldn't get dsi0 clock\n");
		return PTR_ERR(comp->mclk);
	}

	comp->sclk = devm_clk_get(dev, "sclk");
	if (IS_ERR(comp->sclk)) {
		dev_err(dev, "Couldn't get sclk clock\n");
		return PTR_ERR(comp->sclk);
	}

	comp->reset = devm_reset_control_get(dev, "dsi0");
	if (IS_ERR(comp->reset)) {
		dev_err(dev, "Couldn't get reset line\n");
		return PTR_ERR(comp->reset);
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "Couldn't get disp mipi dsi interrupt\n");
		return irq;
	}

	comp->irq = irq;
	ret = devm_request_irq(dev, irq, aic_dsi_handler, 0,
			       dev_name(dev), comp);
	if (ret) {
		dev_err(dev, "Couldn't request the IRQ\n");
		return ret;
	}

	dev_set_drvdata(dev, comp);
	g_aic_dsi_comp = comp;
	comp->dev = dev;
	ret = aic_dsi_parse_dt(comp, dev);
	if (ret)
		return ret;

	ret = sysfs_create_group(&dev->kobj, &aic_dsi_attr_group);
	if (ret) {
		dev_err(dev, "Failed to create sysfs node.\n");
		return ret;
	}

	aic_dsi_register_funcs(comp);
	return 0;
}

static void aic_dsi_unbind(struct device *dev, struct device *master,
			void *data)
{
	struct aic_dsi_comp *comp = aic_dsi_request_drvdata();

	clk_disable_unprepare(comp->mclk);
	clk_put(comp->mclk);
	comp->mclk = NULL;
	sysfs_remove_group(&dev->kobj, &aic_dsi_attr_group);

	aic_dsi_release_drvdata();
}

static const struct component_ops aic_dsi_com_ops = {
	.bind	= aic_dsi_bind,
	.unbind	= aic_dsi_unbind,
};

static int aic_dsi_probe(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "%s()\n", __func__);
	return component_add(&pdev->dev, &aic_dsi_com_ops);
}

static int aic_dsi_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &aic_dsi_com_ops);
	return 0;
}

static const struct of_device_id aic_dsi_match_table[] = {
	{.compatible = "artinchip,aic-mipi-dsi-v1.0"},
	{},
};

MODULE_DEVICE_TABLE(of, aic_dsi_match_table);

static struct platform_driver aic_dsi_driver = {
	.probe = aic_dsi_probe,
	.remove = aic_dsi_remove,
	.driver = {
		.name = "disp mipi dsi",
		.of_match_table	= aic_dsi_match_table,
	},
};

module_platform_driver(aic_dsi_driver);

MODULE_AUTHOR("matteo<duanmt@artinchip.com>");
MODULE_DESCRIPTION("AIC disp MIPI DSI driver");
MODULE_ALIAS("platform:mipi-dsi");
MODULE_LICENSE("GPL v2");
