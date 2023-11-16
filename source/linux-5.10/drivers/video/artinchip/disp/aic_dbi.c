// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co., Ltd.
 * Authors: Huahui Mai <huahui.mai@artinchip.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/reset.h>
#include <linux/pinctrl/consumer.h>
#include <dt-bindings/display/artinchip,aic-disp.h>

#include "hw/dbi_reg.h"
#include "hw/reg_util.h"
#include "aic_com.h"

/**
 * DOC: overview
 *
 * This library provides helpers for MIPI Display Bus Interface (DBI)
 * compatible display controllers.
 *
 * There are 3 MIPI DBI implementation types:
 *
 * A. Motorola 6800 type parallel bus
 *
 * B. Intel 8080 type parallel bus
 *
 * C. SPI type parallel bus
 *
 * Currently aic mipi dbi only supports Type B and Type C.
 */

struct aic_dbi_comp {
	/* di_funcs must be the first member */
	struct di_funcs funcs;
	struct device *dev;
	void __iomem *regs;
	struct reset_control *reset;
	struct clk *mclk;
	struct clk *sclk;
	struct panel_dbi *dbi;
	ulong sclk_rate;
};
static struct aic_dbi_comp *g_aic_dbi_comp;

static int i8080_clk[] = {20, 30, 20, 15, 20, 10, 10, 10};
static int spi_clk[] = {72, 108, 108, 64, 96, 96, 16, 24, 24};

static struct aic_dbi_comp *aic_dbi_request_drvdata(void)
{
	return g_aic_dbi_comp;
}

static void aic_dbi_release_drvdata(void)
{

}

static int aic_dbi_send_cmd(u32 dt, const u8 *data, u32 len)
{
	struct aic_dbi_comp *comp = aic_dbi_request_drvdata();
	struct panel_dbi *dbi = comp->dbi;

	if (dbi->type == I8080)
		i8080_cmd_wr(comp->regs, dt, len, data);
	if (dbi->type == SPI)
		spi_cmd_wr(comp->regs, dt, len, data);

	aic_dbi_release_drvdata();
	return 0;
}

static void aic_dbi_i8080_cfg(void)
{
	struct aic_dbi_comp *comp = aic_dbi_request_drvdata();
	struct panel_dbi *dbi = comp->dbi;

	if (dbi->first_line || dbi->other_line)
		i8080_cmd_ctl(comp->regs, dbi->first_line,
				dbi->other_line);
}

static void aic_dbi_spi_cfg(void)
{
	struct aic_dbi_comp *comp = aic_dbi_request_drvdata();
	struct panel_dbi *dbi = comp->dbi;
	struct spi_cfg *spi = dbi->spi;

	if (dbi->first_line || dbi->other_line)
		spi_cmd_ctl(comp->regs, dbi->first_line,
				dbi->other_line);

	if (spi) {
		qspi_code_cfg(comp->regs, spi->code[0],
				spi->code[1], spi->code[2]);
		qspi_mode_cfg(comp->regs, spi->code1_cfg,
				spi->vbp_num, spi->qspi_mode);
	}
}

static int aic_dbi_clk_enable(void)
{
	struct aic_dbi_comp *comp = aic_dbi_request_drvdata();
	int ret = 0;

	if (comp->sclk_rate)
		clk_set_rate(comp->sclk, comp->sclk_rate);
	else
		dev_warn(comp->dev, "Use the default clock rate %ld\n",
			clk_get_rate(comp->sclk));

	ret = reset_control_deassert(comp->reset);
	if (ret) {
		dev_err(comp->dev, "Couldn't deassert\n");
		aic_dbi_release_drvdata();
		return ret;
	}

	ret = clk_prepare_enable(comp->mclk);
	if (ret) {
		dev_err(comp->dev, "Couldn't enable mclk\n");
		aic_dbi_release_drvdata();
		return ret;
	}

	aic_dbi_release_drvdata();
	return 0;
}

static int aic_dbi_clk_disable(void)
{
	struct aic_dbi_comp *comp = aic_dbi_request_drvdata();

	clk_disable_unprepare(comp->mclk);
	reset_control_assert(comp->reset);
	aic_dbi_release_drvdata();
	return 0;
}

static int aic_dbi_enable(void)
{
	struct aic_dbi_comp *comp = aic_dbi_request_drvdata();
	struct panel_dbi *dbi = comp->dbi;

	pinctrl_pm_select_default_state(comp->dev);
	reg_set_bits(comp->regs + DBI_CTL,
			DBI_CTL_TYPE_MASK,
			DBI_CTL_TYPE(dbi->type));

	switch (dbi->type) {
	case I8080:
		reg_set_bits(comp->regs + DBI_CTL,
				DBI_CTL_I8080_TYPE_MASK,
				DBI_CTL_I8080_TYPE(dbi->format));
		aic_dbi_i8080_cfg();
		break;
	case SPI:
		reg_set_bits(comp->regs + DBI_CTL,
				DBI_CTL_SPI_TYPE_MASK,
				DBI_CTL_SPI_TYPE(
					dbi->format / SPI_MODE_NUM));
		reg_set_bits(comp->regs + DBI_CTL,
				DBI_CTL_SPI_FORMAT_MASK,
				DBI_CTL_SPI_FORMAT(
					dbi->format % SPI_MODE_NUM));
		aic_dbi_spi_cfg();
		break;
	default:
		dev_err(comp->dev, "Invalid type %d\n", dbi->type);
		break;
	}

	reg_set_bit(comp->regs + DBI_CTL, DBI_CTL_EN);
	aic_dbi_release_drvdata();
	return 0;
}

static int aic_dbi_disable(void)
{
	struct aic_dbi_comp *comp = aic_dbi_request_drvdata();

	reg_clr_bit(comp->regs + DBI_CTL, DBI_CTL_EN);
	aic_dbi_release_drvdata();
	pinctrl_pm_select_sleep_state(comp->dev);
	return 0;
}

static int aic_dbi_pixclk2mclk(ulong pixclk)
{
	s32 ret = 0;
	struct aic_dbi_comp *comp = aic_dbi_request_drvdata();
	struct panel_dbi *dbi = comp->dbi;

	dev_dbg(comp->dev, "Current pix-clk is %ld\n", pixclk);
	if (dbi->type == I8080)
		comp->sclk_rate = pixclk * i8080_clk[dbi->format];
	else if (dbi->type == SPI)
		comp->sclk_rate = pixclk * spi_clk[dbi->format];

	aic_dbi_release_drvdata();
	return ret;
}

static int aic_dbi_attach_panel(struct aic_panel *panel)
{
	struct aic_dbi_comp *comp = aic_dbi_request_drvdata();

	comp->dbi = panel->dbi;
	return 0;
}

static int init_module_funcs(struct device *dev)
{
	struct aic_dbi_comp *comp = dev_get_drvdata(dev);

	comp->funcs.clk_enable = aic_dbi_clk_enable;
	comp->funcs.clk_disable = aic_dbi_clk_disable;
	comp->funcs.enable = aic_dbi_enable;
	comp->funcs.disable = aic_dbi_disable;
	comp->funcs.attach_panel = aic_dbi_attach_panel;
	comp->funcs.pixclk2mclk = aic_dbi_pixclk2mclk;
	comp->funcs.send_cmd = aic_dbi_send_cmd;
	return 0;
}

static int aic_dbi_bind(struct device *dev, struct device *master,
		     void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct aic_dbi_comp *comp;
	struct resource *res;
	void __iomem *regs;

	comp = devm_kzalloc(dev, sizeof(*comp), GFP_KERNEL);
	if (!comp)
		return -ENOMEM;

	comp->dev = dev;
	dev_set_drvdata(dev, comp);
	g_aic_dbi_comp = comp;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	comp->regs = regs;

	comp->mclk = devm_clk_get(dev, "dbi0");
	if (IS_ERR(comp->mclk)) {
		dev_err(dev, "Couldn't get dbi0 clock\n");
		return PTR_ERR(comp->mclk);
	}

	comp->sclk = devm_clk_get(dev, "sclk");
	if (IS_ERR(comp->sclk)) {
		dev_err(dev, "Couldn't get sclk clock\n");
		return PTR_ERR(comp->sclk);
	}

	comp->reset = devm_reset_control_get(dev, "dbi0");
	if (IS_ERR(comp->reset)) {
		dev_err(dev, "Couldn't get reset line\n");
		return PTR_ERR(comp->reset);
	}

	init_module_funcs(dev);
	return 0;
}

static void aic_dbi_unbind(struct device *dev, struct device *master,
			void *data)
{
}

static const struct component_ops aic_dbi_com_ops = {
	.bind	= aic_dbi_bind,
	.unbind	= aic_dbi_unbind,
};

static int aic_dbi_probe(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "%s()\n", __func__);
	return component_add(&pdev->dev, &aic_dbi_com_ops);
}

static int aic_dbi_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &aic_dbi_com_ops);
	return 0;
}

static const struct of_device_id aic_dbi_match_table[] = {
	{.compatible = "artinchip,aic-dbi-v1.0"},
	{},
};

MODULE_DEVICE_TABLE(of, aic_dbi_match_table);

static struct platform_driver aic_dbi_driver = {
	.probe = aic_dbi_probe,
	.remove = aic_dbi_remove,
	.driver = {
		.name = "disp mipi-dbi",
		.of_match_table	= aic_dbi_match_table,
	},
};

module_platform_driver(aic_dbi_driver);

MODULE_AUTHOR("Huahui Mai<huahui.mai@artinchip.com>");
MODULE_DESCRIPTION("AIC disp MIPI DBI driver");
MODULE_ALIAS("platform:dbi");
MODULE_LICENSE("GPL v2");

