/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#ifndef _AICMAC_MDIO_H_
#define _AICMAC_MDIO_H_

#include <linux/phy.h>
#include <linux/platform_device.h>

#define MII_BUSY			0x00000001
#define MII_WRITE			0x00000002
#define MII_DATA_MASK			GENMASK(15, 0)

struct mii_regs {
	unsigned int addr;		/* MII Address */
	unsigned int data;		/* MII Data */
	unsigned int addr_shift;	/* MII address shift */
	unsigned int reg_shift;		/* MII reg shift */
	unsigned int addr_mask;		/* MII address mask */
	unsigned int reg_mask;		/* MII reg mask */
	unsigned int clk_csr_shift;
	unsigned int clk_csr_mask;
};

struct aicmac_mdio_bus_data {
	unsigned int phy_mask;
	int *irqs;
	int probed_phy_irq;
	bool needs_reset;
};

struct aicmac_mdio_data {
	struct device_node *mdio_node;
	struct mii_regs mii_reg;	/* MII register Addresses */
	struct mii_bus *mii_bus;	/* MII bus */
	struct aicmac_mdio_bus_data *mdio_bus_data;
};

struct aicmac_mdio_data *aicmac_mdio_init_data(struct platform_device *pdev,
					       struct device_node *np);
int aicmac_mdio_register(void *priv_ptr);
int aicmac_mdio_unregister(struct net_device *ndev);
#endif
