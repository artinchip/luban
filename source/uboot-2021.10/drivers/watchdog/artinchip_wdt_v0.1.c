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

#define WDT_REG_CTRL 0x10
#define WDT_REG_CONF 0x14
#define WDT_REG_MODE 0x18

#define WDT_MODE_RESET (0x01)
#define WDT_COUNTER_RESET (0x1 << 0)
#define WDT_KEY_FIELD (0xA57 << 1)
#define WDT_MODE_TIME_OFF (4)
#define WDT_MODE_TIME_MSK (0xF << 4)
#define WDT_MODE_DOG_EN (0x01)

#define WDT_MAX_TIMEOUT 16

struct aic_wdt_priv {
	fdt_addr_t base;		/* registers addr in physical memory */
};

/*
 * wdt_timeout_map maps the watchdog timer interval value in seconds to
 * the value of the register WDT_INTV
 *
 * [timeout seconds] = register value
 */

static const int wdt_timeout_map[] = {
	[0] = 0x0,  /* 0.5s */
	[1] = 0x1,  /* 1s  */
	[2] = 0x2,  /* 2s  */
	[3] = 0x3,  /* 3s  */
	[4] = 0x4,  /* 4s  */
	[5] = 0x5,  /* 5s  */
	[6] = 0x6,  /* 6s  */
	[7] = 0x7,  /* 8s  */
	[8] = 0x7,  /* 8s  */
	[9] = 0x8,  /* 10s */
	[10] = 0x8, /* 10s */
	[11] = 0x9, /* 12s */
	[12] = 0x9, /* 12s */
	[13] = 0xA, /* 14s */
	[14] = 0xA, /* 14s */
	[15] = 0xB, /* 16s */
	[16] = 0xB, /* 16s */
};

static int aic_wdt_reset(struct udevice *dev)
{
	struct aic_wdt_priv *priv = dev_get_priv(dev);

	writel(WDT_KEY_FIELD | WDT_COUNTER_RESET, priv->base + WDT_REG_CTRL);

	return 0;
}

static int aic_wdt_start(struct udevice *dev, u64 timeout_ms, ulong flags)
{
	struct aic_wdt_priv *priv = dev_get_priv(dev);
	u32 tmo, val;

	tmo = (u32)((timeout_ms + 500) / 1000);
	if (tmo > WDT_MAX_TIMEOUT) {
		pr_err("Max timeout is %d s\n", WDT_MAX_TIMEOUT);
		return -EINVAL;
	}

	/* Function config to reset the chip */
	val = WDT_MODE_RESET;
	writel(val, priv->base + WDT_REG_CONF);

	/* Set the timeout value */
	val = readl(priv->base + WDT_REG_MODE);
	val &= ~WDT_MODE_TIME_MSK;
	val |= (wdt_timeout_map[tmo] << WDT_MODE_TIME_OFF);
	writel(val, priv->base + WDT_REG_MODE);

	/* Reset counter */
	aic_wdt_reset(dev);

	/* Enable watchdog */
	val = readl(priv->base + WDT_REG_MODE);
	val |= WDT_MODE_DOG_EN;
	writel(val, priv->base + WDT_REG_MODE);

	return 0;
}

static int aic_wdt_stop(struct udevice *dev)
{
	struct aic_wdt_priv *priv = dev_get_priv(dev);
	u32 val;

	val = readl(priv->base + WDT_REG_MODE);
	val &= ~WDT_MODE_DOG_EN;
	writel(val, priv->base + WDT_REG_MODE);

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
	.reset = aic_wdt_reset,
};

static const struct udevice_id aic_wdt_match[] = {
	{ .compatible = "artinchip,aic-wdt-v0.1" },
	{ /* sentinel */ }
};

U_BOOT_DRIVER(aic_wdt) = {
	.name = "artinchip_aic_wdt_v0_1",
	.id = UCLASS_WDT,
	.of_match = aic_wdt_match,
	.priv_auto = sizeof(struct aic_wdt_priv),
	.probe = aic_wdt_probe,
	.ops = &aic_wdt_ops,
};
