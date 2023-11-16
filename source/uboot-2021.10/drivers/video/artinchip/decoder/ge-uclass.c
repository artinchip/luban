// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2023 ArtInChip Technology Co.,Ltd
 * Author:  Huahui Mai <huahui.mai@artinchip.com>
 */

#define LOG_CATEGORY UCLASS_GE

#include <common.h>
#include <dm.h>
#include <clk.h>
#include <reset.h>
#include <errno.h>
#include <log.h>
#include <artinchip_ge.h>
#include <dm/device_compat.h>
#include <dm/device-internal.h>
#include <dm/uclass-internal.h>

struct aic_ge_priv {
	struct clk clk;
	struct reset_ctl reset;
	ulong mclk_rate;
};

static int aic_ge_open(struct udevice *dev)
{
	struct aic_ge_priv *priv = dev_get_priv(dev);
	int ret;

	ret = clk_enable(&priv->clk);
	if (ret) {
		dev_err(dev, "Failed to enable GE clock\n");
		return ret;
	}

	ret = clk_set_rate(&priv->clk, priv->mclk_rate);
	if (ret) {
		dev_err(dev, "Failed to set CLK_GE %ld\n", priv->mclk_rate);
		return ret;
	}

	ret = reset_deassert(&priv->reset);
	if (ret) {
		dev_err(dev, "Failed to deassert GE reset\n");
		return ret;
	}

	return ret;
}

static void aic_ge_close(struct udevice *dev)
{
	struct aic_ge_priv *priv = dev_get_priv(dev);

	if (clk_valid(&priv->clk))
		clk_disable(&priv->clk);

	if (reset_valid(&priv->reset))
		reset_assert(&priv->reset);
}

static int aic_ge_probe(struct udevice *dev)
{
	int ret = 0;
	struct aic_ge_priv *priv = dev_get_priv(dev);
	ofnode node = dev_ofnode(dev);

	ret = clk_get_by_index(dev, 0, &priv->clk);
	if (ret < 0) {
		dev_err(dev, "failed to get clock\n");
		return ret;
	}

	ret = ofnode_read_u32(node, "mclk", (u32 *)&priv->mclk_rate);
	if (ret) {
		dev_err(dev, "Can't parse CLK_VE rate\n");
		return -EINVAL;
	}

	ret = reset_get_by_index(dev, 0, &priv->reset);
	if (ret && ret != -ENOENT) {
		dev_err(dev, "failed to get reset\n");
		return ret;
	}

	return ret;
}

static const struct ge_ops aic_ge_ops = {
	.open = aic_ge_open,
	.close = aic_ge_close,
};

static const struct udevice_id aic_ge_match_ids[] = {
	{.compatible = "artinchip,aic-ge-v1.0"},
	{ /* sentinel*/ },
};

U_BOOT_DRIVER(aic_ge) = {
	.name                     = "aic_ge",
	.id                       = UCLASS_GE,
	.of_match                 = aic_ge_match_ids,
	.ops                      = &aic_ge_ops,
	.priv_auto                = sizeof(struct aic_ge_priv),
	.probe                    = aic_ge_probe,
};

UCLASS_DRIVER(ge) = {
	.id		= UCLASS_GE,
	.name		= "ge",
};
