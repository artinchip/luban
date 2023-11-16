// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020-2022 ArtInChip Technology Co.,Ltd
 * Author: Hao Xiong <hao.xiong@artinchip.com>
 */

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <dm/device.h>
#include <dm/device_compat.h>
#include <dt-structs.h>
#include <mapmem.h>
#include <dma.h>
#include <errno.h>
#include <fdt_support.h>
#include <reset.h>
#include <wait_bit.h>
#include <asm/bitops.h>
#include <asm/gpio.h>
#include <asm/cache.h>
#include <asm/io.h>
#include <linux/iopoll.h>
#include <hexdump.h>
#include <artinchip_crypto.h>
#include "artinchip_ce.h"

DECLARE_GLOBAL_DATA_PTR;
struct aic_crypto_data {
	u32 fifo_depth;
	bool has_soft_reset;
	bool has_burst_ctl;
};

struct aic_crypto_platdata {
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	struct dtd_artinchip_aic_spi dtplat;
#endif
	struct aic_crypto_data *crypto_data;
	void __iomem *base;
	u32 max_hz;
};

struct aic_crypto_priv {
	struct device *dev;
	void __iomem *base;
	struct clk clk;
	struct reset_ctl reset;
	u32 irq_status;
	u32 err_status;
	ulong clock_rate;
};

static int aic_crypto_init(struct udevice *dev)
{
	struct aic_crypto_priv *priv = dev_get_priv(dev);

	writel(0x7, CE_REG_ICR(priv));
	/* Clear interrupt status  */
	writel(0xF, CE_REG_ISR(priv));
	writel(0xFFFFFFFF, CE_REG_TER(priv));

	return 0;
}

static int aic_crypto_start(struct udevice *dev, struct task_desc *task)
{
	struct aic_crypto_priv *priv = dev_get_priv(dev);

	writel((unsigned long)task, CE_REG_TAR(priv));
	writel(task->alg.alg_tag | (1UL << 31), CE_REG_TCR(priv));

	return 0;
}

static int aic_crypto_poll_finish(struct udevice *dev, u32 alg_unit)
{
	struct aic_crypto_priv *priv = dev_get_priv(dev);
	u32 status, except_status;
	int ret;

	except_status = (0x1 << alg_unit);
	ret = readl_poll_timeout(CE_REG_ISR(priv), status,
				 status & except_status, 5000000);
	if (ret != 0) {
		dev_err(dev, "Timeout to wait IRQ, ISR:0x%x\n", status);
		return ret;
	}

	return ret;
}

static void aic_crypto_pending_clear(struct udevice *dev, u32 alg_unit)
{
	struct aic_crypto_priv *priv = dev_get_priv(dev);
	u32 val;

	val = readl(CE_REG_ISR(priv));
	if ((val & (0x01 << alg_unit)) == (0x01 << alg_unit)) {
		val &= ~(0x0F);
		val |= (0x01 << alg_unit);
	}
	writel(val, CE_REG_ISR(priv));
}

static int aic_crypto_get_err(struct udevice *dev, u32 alg_unit)
{
	struct aic_crypto_priv *priv = dev_get_priv(dev);
	u32 val;
	int ret;

	val = readl(CE_REG_TER(priv));
	// clear err flag
	writel(val, CE_REG_TER(priv));
	ret = (val >> (8 * alg_unit)) & 0xFF;
	if (ret) {
		dev_err(dev, "err, TER:0x%x\n", val);
		return ret;
	}
	return ret;
}

static inline int aic_crypto_set_clock(struct udevice *dev, bool enable)
{
	int ret = 0;
#if !defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_CLK_ARTINCHIP)
	struct aic_crypto_priv *priv = dev_get_priv(dev);

	dev_dbg(dev, "enable %d\n", enable);
	if (!enable) {
		clk_disable(&priv->clk);
		if (reset_valid(&priv->reset))
			reset_assert(&priv->reset);
		return 0;
	}

	ret = clk_enable(&priv->clk);
	if (ret) {
		dev_err(dev, "failed to enable clock (ret=%d)\n", ret);
		return ret;
	}

	ret = clk_set_rate(&priv->clk, priv->clock_rate);
	if (ret) {
		dev_err(dev, "Failed to set CLK_CE %ld\n", priv->clock_rate);
		return ret;
	}

	if (reset_valid(&priv->reset)) {
		ret = reset_deassert(&priv->reset);
		if (ret) {
			dev_err(dev, "failed to deassert reset\n");
			goto err;
		}
	}
	return 0;

err:
	clk_disable(&priv->clk);
#endif
	return ret;
}

static int aic_crypto_probe(struct udevice *dev)
{
	int ret = 0;
	struct aic_crypto_priv *priv = dev_get_priv(dev);
	ofnode node = dev_ofnode(dev);

	priv->base = (void *)devfdt_get_addr(dev);

#if !defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_CLK_ARTINCHIP)
	ret = clk_get_by_index(dev, 0, &priv->clk);
	if (ret < 0) {
		dev_err(dev, "failed to get clock\n");
		return ret;
	}

	ret = ofnode_read_u32(node, "clock-rate", (u32 *)&priv->clock_rate);
	if (ret) {
		dev_err(dev, "Can't parse CLK_VE rate\n");
		return -EINVAL;
	}

	ret = reset_get_by_index(dev, 0, &priv->reset);
	if (ret && ret != -ENOENT) {
		dev_err(dev, "failed to get reset\n");
		return ret;
	}
#endif

	ret = aic_crypto_set_clock(dev, true);
	if (ret) {
		dev_err(dev, "failed to set clock\n");
		return ret;
	}

	return ret;
}

static const struct dm_crypto_ops aic_crypto_ops = {
	.init   = aic_crypto_init,
	.start	= aic_crypto_start,
	.poll_finish   = aic_crypto_poll_finish,
	.pending_clear = aic_crypto_pending_clear,
	.get_err    = &aic_crypto_get_err,
};

static const struct udevice_id aic_crypto_ids[] = {
	{.compatible = "artinchip,aic-crypto-v1.0"},
	{},
};

U_BOOT_DRIVER(aic_crypto) = {
	.name                     = "artinchip_aic_crypto",
	.id                       = UCLASS_CRYPTO,
	.of_match                 = aic_crypto_ids,
	.ops                      = &aic_crypto_ops,
	.plat_auto                = sizeof(struct aic_crypto_platdata),
	.priv_auto                = sizeof(struct aic_crypto_priv),
	.probe                    = aic_crypto_probe,
};
