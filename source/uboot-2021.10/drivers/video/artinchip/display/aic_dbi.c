// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2023 ArtInChip Technology Co.,Ltd
 * Huahui Mai <huahui.mai@artinchip.com>
 */

#include <common.h>
#include <display.h>
#include <dm.h>
#include <clk.h>
#include <reset.h>
#include <linux/ioport.h>
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

struct aic_dbi_priv {
	/* di_funcs must be the first member */
	struct di_funcs funcs;
	struct udevice *dev;
	void __iomem *regs;
	struct reset_ctl reset;
	struct clk mclk;
	struct clk sclk;
	ulong sclk_rate;
	struct panel_dbi *dbi;
};
static struct aic_dbi_priv *g_aic_dbi_priv;

static struct aic_dbi_priv *aic_dbi_request_drvdata(void)
{
	return g_aic_dbi_priv;
}

static int i8080_clk[] = {20, 30, 20, 15, 20, 10, 10, 10};
static int spi_clk[] = {72, 108, 108, 64, 96, 96, 16, 24, 24};

static void aic_dbi_release_drvdata(void)
{

}

static int aic_dbi_send_cmd(u32 dt, const u8 *data, u32 len)
{
	struct aic_dbi_priv *priv = aic_dbi_request_drvdata();
	struct panel_dbi *dbi = priv->dbi;

	if (dbi->type == I8080)
		i8080_cmd_wr(priv->regs, dt, len, data);
	if (dbi->type == SPI)
		spi_cmd_wr(priv->regs, dt, len, data);

	aic_dbi_release_drvdata();
	return 0;
}

static void aic_dbi_i8080_cfg(void)
{
	struct aic_dbi_priv *priv = aic_dbi_request_drvdata();
	struct panel_dbi *dbi = priv->dbi;

	if (dbi->first_line || dbi->other_line)
		i8080_cmd_ctl(priv->regs, dbi->first_line,
				dbi->other_line);
}

static void aic_dbi_spi_cfg(void)
{
	struct aic_dbi_priv *priv = aic_dbi_request_drvdata();
	struct panel_dbi *dbi = priv->dbi;
	struct spi_cfg *spi = dbi->spi;

	if (dbi->first_line || dbi->other_line)
		spi_cmd_ctl(priv->regs, dbi->first_line,
				dbi->other_line);

	if (spi) {
		qspi_code_cfg(priv->regs, spi->code[0],
				spi->code[1], spi->code[2]);
		qspi_mode_cfg(priv->regs, spi->code1_cfg,
				spi->vbp_num, spi->qspi_mode);
	}
}

static int aic_dbi_clk_enable(void)
{
	struct aic_dbi_priv *priv = aic_dbi_request_drvdata();
	int ret = 0;

	if (priv->sclk_rate)
		clk_set_rate(&priv->sclk, priv->sclk_rate);
	else
		debug("Use the default clock rate %ld\n",
						clk_get_rate(&priv->sclk));

	ret = reset_deassert(&priv->reset);
	if (ret) {
		debug("Couldn't deassert\n");
		aic_dbi_release_drvdata();
		return ret;
	}

	ret = clk_enable(&priv->mclk);
	if (ret) {
		debug("Couldn't enable mclk\n");
		aic_dbi_release_drvdata();
		return ret;
	}

	aic_dbi_release_drvdata();
	return 0;
}

static int aic_dbi_enable(void)
{
	struct aic_dbi_priv *priv = aic_dbi_request_drvdata();
	struct panel_dbi *dbi = priv->dbi;

	reg_set_bits(priv->regs + DBI_CTL,
			DBI_CTL_TYPE_MASK,
			DBI_CTL_TYPE(dbi->type));

	switch (dbi->type) {
	case I8080:
		reg_set_bits(priv->regs + DBI_CTL,
				DBI_CTL_I8080_TYPE_MASK,
				DBI_CTL_I8080_TYPE(dbi->format));
		aic_dbi_i8080_cfg();
		break;
	case SPI:
		reg_set_bits(priv->regs + DBI_CTL,
				DBI_CTL_SPI_TYPE_MASK,
				DBI_CTL_SPI_TYPE(
					dbi->format / SPI_MODE_NUM));
		reg_set_bits(priv->regs + DBI_CTL,
				DBI_CTL_SPI_FORMAT_MASK,
				DBI_CTL_SPI_FORMAT(
					dbi->format % SPI_MODE_NUM));
		aic_dbi_spi_cfg();
		break;
	default:
		debug("Invalid type %d\n", dbi->type);
		break;
	}

	reg_set_bit(priv->regs + DBI_CTL, DBI_CTL_EN);
	aic_dbi_release_drvdata();
	return 0;
}

static int aic_dbi_pixclk2mclk(ulong pixclk)
{
	s32 ret = 0;
	struct aic_dbi_priv *priv = aic_dbi_request_drvdata();
	struct panel_dbi *dbi = priv->dbi;

	debug("Current pix-clk is %ld\n", pixclk);
	if (dbi->type == I8080)
		priv->sclk_rate = pixclk * i8080_clk[dbi->format];
	else if (dbi->type == SPI)
		priv->sclk_rate = pixclk * spi_clk[dbi->format];

	aic_dbi_release_drvdata();
	return ret;
}

static int aic_dbi_attach_panel(struct aic_panel *panel)
{
	struct aic_dbi_priv *priv = aic_dbi_request_drvdata();

	priv->dbi = panel->dbi;
	return 0;
}

static int init_module_funcs(struct udevice *dev)
{
	struct aic_dbi_priv *priv = dev_get_priv(dev);

	priv->funcs.clk_enable = aic_dbi_clk_enable;
	priv->funcs.enable = aic_dbi_enable;
	priv->funcs.attach_panel = aic_dbi_attach_panel;
	priv->funcs.pixclk2mclk = aic_dbi_pixclk2mclk;
	priv->funcs.send_cmd = aic_dbi_send_cmd;
	return 0;
}

static int aic_dbi_probe(struct udevice *dev)
{
	struct aic_dbi_priv *priv = dev_get_priv(dev);
	int ret;

	priv->dev = dev;

	priv->regs = (void *)dev_read_addr(dev);
	if (IS_ERR(priv->regs))
		return PTR_ERR(priv->regs);

	ret = clk_get_by_name(dev, "dbi0", &priv->mclk);
	if (ret) {
		debug("Couldn't get dbi0 clock\n");
		return ret;
	}

	ret = clk_get_by_name(dev, "sclk", &priv->sclk);
	if (ret) {
		debug("Couldn't get sclk clock\n");
		return ret;
	}

	ret = reset_get_by_name(dev, "dbi0", &priv->reset);
	if (ret) {
		debug("Couldn't get reset line\n");
		return ret;
	}

	g_aic_dbi_priv = priv;
	init_module_funcs(dev);
	return 0;
}

static const struct udevice_id aic_dbi_match_ids[] = {
	{ .compatible = "artinchip,aic-dbi-v1.0" },
	{ /* sentinel*/ },
};

U_BOOT_DRIVER(disp_dbi) = {
	.name      = "disp_dbi",
	.id        = UCLASS_DISPLAY,
	.of_match  = aic_dbi_match_ids,
	.probe     = aic_dbi_probe,
	.priv_auto = sizeof(struct aic_dbi_priv),
};

