// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/of_device.h>
#include <linux/of_mdio.h>
#include <linux/ethtool.h>

#include "aicmac.h"
#include "aicmac_core.h"
#include "aicmac_phy.h"
#include "aicmac_mac.h"

struct aicmac_phy_data *aicmac_phy_init_data(struct platform_device *pdev,
					     struct device_node *np)
{
	struct aicmac_phy_data *phy_data = devm_kzalloc(&pdev->dev,
			sizeof(struct aicmac_phy_data), GFP_KERNEL);

	if (of_get_phy_mode(np, &phy_data->phy_interface))
		phy_data->phy_interface = PHY_INTERFACE_MODE_MII;

	phy_data->phy_node = of_parse_phandle(np, "phy-handle", 0);

	phy_data->phylink_node = np;

	if (of_property_read_u32(np, "phy-addr", &phy_data->phy_addr) != 0)
		phy_data->phy_addr = 0;

	pr_info("%s phy_addr = %d \n", __func__, phy_data->phy_addr);
	return phy_data;
}

int aicmac_phy_init(void *priv_ptr)
{
	struct aicmac_priv *priv = priv_ptr;
	struct phylink *phylink;
	struct aicmac_phy_data *phy_data = priv->plat->phy_data;
	struct fwnode_handle *fwnode = of_fwnode_handle(phy_data->phylink_node);

	phy_interface_t mode = phy_data->phy_interface;

	phy_data->phylink_cfg.dev = &priv->dev->dev;
	phy_data->phylink_cfg.type = PHYLINK_NETDEV;
	phy_data->phylink_cfg.pcs_poll = true;

	phylink = phylink_create(&phy_data->phylink_cfg, fwnode, mode,
				 priv->plat->mac_data->phylink_mac_ops);
	if (IS_ERR(phylink))
		return PTR_ERR(phylink);

	phy_data->phylink = phylink;
	return 0;
}

void aicmac_phy_reset(struct gpio_desc *phy_gpio)
{
	gpiod_set_value(phy_gpio, 0);
	msleep(50);
	gpiod_set_value(phy_gpio, 1);
	msleep(50);
}

int aicmac_phy_connect(struct net_device *dev)
{
	struct aicmac_priv *priv = netdev_priv(dev);
	struct device_node *node;
	struct aicmac_phy_data *phy_data = priv->plat->phy_data;
	struct ethtool_wolinfo wol = { .cmd = ETHTOOL_GWOL };
	int ret;

	node = phy_data->phylink_node;

	if (node)
		ret = phylink_of_phy_connect(phy_data->phylink, node, 0);

	/* Some DT bindings do not set-up the PHY handle. Let's try to
	 * manually parse it
	 */
	if (!node || ret) {
		int addr = phy_data->phy_addr;
		struct phy_device *phydev;

		phydev = mdiobus_get_phy(priv->plat->mdio_data->mii_bus, addr);
		if (!phydev) {
			netdev_err(priv->dev, "no phy at addr %d\n", addr);
			return -ENODEV;
		}

		ret = phylink_connect_phy(phy_data->phylink, phydev);
	}

	phylink_ethtool_get_wol(phy_data->phylink, &wol);
	device_set_wakeup_capable(priv->device, !!wol.supported);

	return ret;
}
