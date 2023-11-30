// SPDX-License-Identifier: GPL-2.0
/*
 * ArtInChip UART driver
 *
 * Copyright (C) 2020 ArtInChip Technology Co.,Ltd
 * Author: Dehuang Wu <dehuang.wu@artinchip.com>
 */

#include <common.h>
#include <clk.h>
#include <reset.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <errno.h>
#include <serial.h>
#include <asm/io.h>
#include <asm/types.h>
#include <linux/delay.h>
#include <ns16550.h>

int aic_serial_of_to_plat(struct udevice *dev)
{
	struct ns16550_plat *plat = dev_get_plat(dev);

	plat->base = dev_read_addr(dev);
	plat->reg_offset = dev_read_u32_default(dev, "reg-offset", 0);
	plat->reg_shift = dev_read_u32_default(dev, "reg-shift", 0);
	plat->reg_width = dev_read_u32_default(dev, "reg-io-width", 1);
	plat->clock = dev_read_u32_default(dev, "clock-frequency", 0);
	if (!plat->clock) {
		debug("ns16550 clock not defined\n");
		return -EINVAL;
	}

	plat->fcr = UART_FCR_DEFVAL;

	return 0;
}

static int aic_serial_probe(struct udevice *dev)
{
	struct ns16550_plat *plat = dev_get_plat(dev);
	struct ns16550 *const com_port = dev_get_priv(dev);
#if !defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_CLK_ARTINCHIP)
	struct clk clk;
	struct reset_ctl reset;
	int ret;

	pr_info("%s\n", __func__);
	ret = clk_get_by_index(dev, 0, &clk);
	if (ret < 0) {
		dev_err(dev, "failed to get clk\n");
		return ret;
	}
	ret = reset_get_by_index(dev, 0, &reset);
	if (ret && ret != -ENOENT) {
		dev_err(dev, "failed to get reset\n");
		return ret;
	}

	ret = clk_enable(&clk);
	if (ret) {
		dev_err(dev, "failed to enable clock (ret=%d)\n", ret);
		return ret;
	}
	clk_set_rate(&clk, plat->clock);

	reset_assert(&reset);
	ret = reset_deassert(&reset);
	if (ret) {
		dev_err(dev, "failed to deassert reset\n");
		return ret;
	}
#endif

	com_port->plat = plat;
	ns16550_init(com_port, -1);

	return 0;
}

static const struct udevice_id aic_serial_ids[] = {
	{ .compatible = "artinchip,aic-uart-v1.0" },
	{ }
};

U_BOOT_DRIVER(aic_serial) = {
	.name                     = "aic_uart",
	.id                       = UCLASS_SERIAL,
	.of_match                 = aic_serial_ids,
	.of_to_plat               = aic_serial_of_to_plat,
	.probe                    = aic_serial_probe,
	.ops                      = &ns16550_serial_ops,
	.flags                    = DM_FLAG_PRE_RELOC,
	.priv_auto                = sizeof(struct ns16550),
	.plat_auto                = sizeof(struct ns16550_plat),
};
