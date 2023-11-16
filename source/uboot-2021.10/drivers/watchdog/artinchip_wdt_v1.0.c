// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * Copyright (C) 2021 ArtInChip Technology Co.,Ltd.
 */

#define LOG_CATEGORY UCLASS_WDT

#include <common.h>
#include <clk.h>
#include <reset.h>
#include <dm.h>
#include <log.h>
#include <syscon.h>
#include <wdt.h>
#include <asm/io.h>
#include <dm/device_compat.h>
#include <linux/bitops.h>
#include <linux/iopoll.h>

#define WDT_REG_CTRL       0x00
#define WDT_REG_CNTR       0x04
#define WDT_REG_ICR        0x08
#define WDT_REG_ISR        0x0C
/* 4 groups configuration n = [0,1,2,3] */
#define WDT_REG_CLR_CFG(n) (0x40 + n * 0x10)
#define WDT_REG_IRQ_CFG(n) (0x44 + n * 0x10)
#define WDT_REG_RST_CFG(n) (0x48 + n * 0x10)
#define WDT_REG_OP         0xE8
#define CNT_PER_MSECOND    (32)       /* 32KHz counter clock */

#define WDT_OP_CNT_CLR_CMD0  0xA1C55555
#define WDT_OP_CNT_CLR_CMD1  0xA1CAAAAA
#define WDT_OP_WRITE_EN_CMD0 0xA1C99999
#define WDT_OP_WRITE_EN_CMD1 0xA1C66666

#define WDT_MAX_TIMEOUT_MS   (3600000) /* 1 hour */

struct aic_wdt_priv {
	fdt_addr_t base;		/* registers addr in physical memory */
};


static int aic_wdt_reset(struct udevice *dev)
{
	struct aic_wdt_priv *priv = dev_get_priv(dev);

	/* Write OP commands in sequence to reset counter and reload cfg */
	writel(WDT_OP_CNT_CLR_CMD0, (void *)(priv->base + WDT_REG_OP));
	writel(WDT_OP_CNT_CLR_CMD1, (void *)(priv->base + WDT_REG_OP));

	return 0;
}

static int aic_wdt_start(struct udevice *dev, u64 timeout_ms, ulong flags)
{
	struct aic_wdt_priv *priv = dev_get_priv(dev);
	u32 val;

	if (timeout_ms > WDT_MAX_TIMEOUT_MS) {
		dev_err(dev, "Max timeout is 1hour\n");
		return -EINVAL;
	}

	val = timeout_ms * CNT_PER_MSECOND;

	writel(0, (void *)(priv->base + WDT_REG_CLR_CFG(0)));
	writel(val, (void *)(priv->base + WDT_REG_IRQ_CFG(0)));
	writel(val, (void *)(priv->base + WDT_REG_RST_CFG(0)));

	/* Reset counter and reload cfg */
	aic_wdt_reset(dev);

	/* Enable watchdog */
	writel(1, (void *)(priv->base + WDT_REG_CTRL));

	return 0;
}

static int aic_wdt_expire_now(struct udevice *dev, ulong flags)
{
	return aic_wdt_start(dev, 0, flags);
}

static int aic_wdt_stop(struct udevice *dev)
{
	struct aic_wdt_priv *priv = dev_get_priv(dev);

	writel(0, (void *)(priv->base + WDT_REG_CTRL));

	return 0;
}

static int aic_wdt_probe(struct udevice *dev)
{
	struct aic_wdt_priv *priv = dev_get_priv(dev);
	struct clk clk;
	struct reset_ctl reset;
	int ret;

	priv->base = dev_read_addr(dev);
	if (priv->base == FDT_ADDR_T_NONE)
		return -EINVAL;

	ret = clk_get_by_index(dev, 0, &clk);
	if (ret && ret != -ENOENT) {
		dev_err(dev, "failed to get clock\n");
		return ret;
	}

	ret = clk_enable(&clk);
	if (ret)
		return ret;

	ret = reset_get_by_index(dev, 0, &reset);
	if (ret && ret != -ENOENT) {
		dev_err(dev, "failed to get reset\n");
		return ret;
	}
	ret = reset_deassert(&reset);
	if (ret) {
		dev_err(dev, "failed to deassert reset\n");
		return ret;
	}

	dev_info(dev, "%s done\n", __func__);

	return 0;
}

static const struct wdt_ops aic_wdt_ops = {
	.start = aic_wdt_start,
	.stop = aic_wdt_stop,
	.expire_now = aic_wdt_expire_now,
	.reset = aic_wdt_reset,
};

static const struct udevice_id aic_wdt_match[] = {
	{ .compatible = "artinchip,aic-wdt-v1.0" },
	{ /* sentinel */ }
};

U_BOOT_DRIVER(aic_wdt) = {
	.name = "artinchip_aic_wdt_v1_0",
	.id = UCLASS_WDT,
	.of_match = aic_wdt_match,
	.priv_auto = sizeof(struct aic_wdt_priv),
	.probe = aic_wdt_probe,
	.ops = &aic_wdt_ops,
};
