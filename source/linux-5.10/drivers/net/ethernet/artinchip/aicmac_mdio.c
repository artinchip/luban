// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#include <linux/gpio/consumer.h>
#include <linux/netdevice.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/mii.h>
#include <linux/of_mdio.h>
#include <linux/phy.h>
#include <linux/property.h>
#include <linux/slab.h>

#include "aicmac.h"
#include "aicmac_platform.h"
#include "aicmac_gmac_reg.h"
#include "aicmac_mdio.h"
#include "aicmac_mac.h"
#include "aicmac_phy.h"

struct aicmac_mdio_data *aicmac_mdio_init_data(struct platform_device *pdev,
					       struct device_node *np)
{
	struct aicmac_mdio_data *mdio_data = devm_kzalloc(&pdev->dev,
			sizeof(struct aicmac_mdio_data), GFP_KERNEL);
	bool mdio = !of_phy_is_fixed_link(np);

	for_each_child_of_node(np, mdio_data->mdio_node) {
		if (of_device_is_compatible(mdio_data->mdio_node,
					    "aicmac-mdio"))
			break;
	}

	if (mdio_data->mdio_node)
		mdio = true;

	if (mdio) {
		mdio_data->mdio_bus_data =
			devm_kzalloc(&pdev->dev,
				     sizeof(struct aicmac_mdio_bus_data),
				     GFP_KERNEL);
		if (!mdio_data->mdio_bus_data)
			return mdio_data;

		mdio_data->mdio_bus_data->needs_reset = true;
	}

	mdio_data->mii_reg.addr = GMAC_MII_ADDR;
	mdio_data->mii_reg.data = GMAC_MII_DATA;
	mdio_data->mii_reg.addr_shift = 11;
	mdio_data->mii_reg.addr_mask = 0x0000F800;
	mdio_data->mii_reg.reg_shift = 6;
	mdio_data->mii_reg.reg_mask = 0x000007C0;
	mdio_data->mii_reg.clk_csr_shift = 2;
	mdio_data->mii_reg.clk_csr_mask = GENMASK(5, 2);

	pr_info("%s mdio:%d \n", __func__, mdio);
	return mdio_data;
}

static int aicmac_mdio_read(struct mii_bus *bus, int phyaddr, int phyreg)
{
	struct net_device *ndev = bus->priv;
	struct aicmac_priv *priv = netdev_priv(ndev);
	struct aicmac_mdio_data *mdio_data = priv->plat->mdio_data;
	unsigned int mii_address = mdio_data->mii_reg.addr;
	unsigned int mii_data = mdio_data->mii_reg.data;
	u32 value = MII_BUSY;
	int data = 0;
	u32 v;

	value |= (phyaddr << mdio_data->mii_reg.addr_shift) &
		 mdio_data->mii_reg.addr_mask;
	value |= (phyreg << mdio_data->mii_reg.reg_shift) &
		 mdio_data->mii_reg.reg_mask;
	value |= (priv->plat->clk_csr << mdio_data->mii_reg.clk_csr_shift) &
		 mdio_data->mii_reg.clk_csr_mask;

	if (readl_poll_timeout(priv->resource->ioaddr + mii_address, v,
			       !(v & MII_BUSY), 100, 10000))
		return -EBUSY;

	writel(data, priv->resource->ioaddr + mii_data);
	writel(value, priv->resource->ioaddr + mii_address);

	if (readl_poll_timeout(priv->resource->ioaddr + mii_address, v,
			       !(v & MII_BUSY), 100, 10000))
		return -EBUSY;

	/* Read the data from the MII data register */
	data = (int)readl(priv->resource->ioaddr + mii_data) & MII_DATA_MASK;

	return data;
}

static int aicmac_mdio_write(struct mii_bus *bus, int phyaddr, int phyreg,
			     u16 phydata)
{
	struct net_device *ndev = bus->priv;
	struct aicmac_priv *priv = netdev_priv(ndev);
	struct aicmac_mdio_data *mdio_data = priv->plat->mdio_data;
	unsigned int mii_address = mdio_data->mii_reg.addr;
	unsigned int mii_data = mdio_data->mii_reg.data;
	u32 value = MII_BUSY;
	int data = phydata;
	u32 v;

	value |= (phyaddr << mdio_data->mii_reg.addr_shift) &
		 mdio_data->mii_reg.addr_mask;
	value |= (phyreg << mdio_data->mii_reg.reg_shift) &
		 mdio_data->mii_reg.reg_mask;

	value |= (priv->plat->clk_csr << mdio_data->mii_reg.clk_csr_shift) &
		 mdio_data->mii_reg.clk_csr_mask;

	value |= MII_WRITE;

	/* Wait until any existing MII operation is complete */
	if (readl_poll_timeout(priv->resource->ioaddr + mii_address, v,
			       !(v & MII_BUSY), 100, 10000))
		return -EBUSY;

	/* Set the MII address register to write */
	writel(data, priv->resource->ioaddr + mii_data);
	writel(value, priv->resource->ioaddr + mii_address);

	/* Wait until any existing MII operation is complete */
	return readl_poll_timeout(priv->resource->ioaddr + mii_address, v,
				  !(v & MII_BUSY), 100, 10000);
}

static int aicmac_mdio_reset(struct mii_bus *bus)
{
	struct net_device *ndev = bus->priv;
	struct aicmac_priv *priv = netdev_priv(ndev);

	if (priv->device->of_node) {
		struct gpio_desc *reset_gpio;
		u32 delays[3] = { 0, 0, 0 };

		reset_gpio = devm_gpiod_get_optional(priv->device, "aic,reset",
						     GPIOD_OUT_LOW);
		if (IS_ERR(reset_gpio))
			return PTR_ERR(reset_gpio);

		device_property_read_u32_array(priv->device,
					       "aic,reset-delays-us", delays,
					       ARRAY_SIZE(delays));

		if (delays[0])
			msleep(DIV_ROUND_UP(delays[0], 1000));

		gpiod_set_value_cansleep(reset_gpio, 1);
		if (delays[1])
			msleep(DIV_ROUND_UP(delays[1], 1000));

		gpiod_set_value_cansleep(reset_gpio, 0);
		if (delays[2])
			msleep(DIV_ROUND_UP(delays[2], 1000));
	}

	return 0;
}

int aicmac_mdio_register(void *priv_ptr)
{
	int err = 0;
	struct mii_bus *new_bus;
	struct aicmac_priv *priv = priv_ptr;
	struct aicmac_mdio_data *mdio_data = priv->plat->mdio_data;
	struct aicmac_phy_data *phy_data = priv->plat->phy_data;
	struct aicmac_mdio_bus_data *mdio_bus_data = mdio_data->mdio_bus_data;
	struct device_node *mdio_node = mdio_data->mdio_node;
	struct device *dev = priv->device;
	int addr, found;

	if (!mdio_bus_data)
		return 0;

	new_bus = mdiobus_alloc();
	if (!new_bus)
		return -ENOMEM;

	if (mdio_bus_data->irqs)
		memcpy(new_bus->irq, mdio_bus_data->irqs, sizeof(new_bus->irq));

	new_bus->name = AICMAC_DEVICE_NAME;

	new_bus->read = &aicmac_mdio_read;
	new_bus->write = &aicmac_mdio_write;

	if (mdio_bus_data->needs_reset)
		new_bus->reset = &aicmac_mdio_reset;

	snprintf(new_bus->id, MII_BUS_ID_SIZE, "%s-%x", new_bus->name,
		 priv->plat->bus_id);
	new_bus->priv = priv->dev;
	new_bus->phy_mask = mdio_bus_data->phy_mask;
	new_bus->parent = priv->device;

	err = of_mdiobus_register(new_bus, mdio_node);
	if (err != 0) {
		dev_err(dev, "Cannot register the MDIO bus\n");
		goto bus_register_fail;
	}

	if (phy_data->phy_node || mdio_node)
		goto bus_register_done;

	found = 0;
	for (addr = 0; addr < PHY_MAX_ADDR; addr++) {
		struct phy_device *phydev = mdiobus_get_phy(new_bus, addr);

		if (!phydev)
			continue;

		if (!mdio_bus_data->irqs &&
		    mdio_bus_data->probed_phy_irq > 0) {
			new_bus->irq[addr] = mdio_bus_data->probed_phy_irq;
			phydev->irq = mdio_bus_data->probed_phy_irq;
		}

		if (phy_data->phy_addr == -1)
			phy_data->phy_addr = addr;

		phy_attached_info(phydev);
		found = 1;
	}

	if (!found && !mdio_node) {
		dev_warn(dev, "No PHY found\n");
		mdiobus_unregister(new_bus);
		mdiobus_free(new_bus);
		return -ENODEV;
	}

bus_register_done:
	mdio_data->mii_bus = new_bus;

	return 0;

bus_register_fail:
	mdiobus_free(new_bus);
	return err;
}

int aicmac_mdio_unregister(struct net_device *ndev)
{
	struct aicmac_priv *priv = netdev_priv(ndev);

	if (!priv->plat->mdio_data->mii_bus)
		return 0;

	mdiobus_unregister(priv->plat->mdio_data->mii_bus);
	priv->plat->mdio_data->mii_bus->priv = NULL;
	mdiobus_free(priv->plat->mdio_data->mii_bus);
	priv->plat->mdio_data->mii_bus = NULL;

	return 0;
}
