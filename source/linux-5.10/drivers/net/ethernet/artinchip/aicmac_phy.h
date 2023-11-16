/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#ifndef _AICMAC_PHY_H_
#define _AICMAC_PHY_H_

#include <linux/phylink.h>
#include <linux/platform_device.h>

struct aicmac_phy_data {
	phy_interface_t phy_interface;
	int phy_addr;
	struct device_node *phy_node;
	struct device_node *phylink_node;
	struct phylink_config phylink_cfg;
	struct phylink *phylink;
};

struct aicmac_phy_data *aicmac_phy_init_data(struct platform_device *pdev,
					     struct device_node *np);
int aicmac_phy_init(void *priv_ptr);
void aicmac_phy_reset(struct gpio_desc *phy_gpio);
int aicmac_phy_connect(struct net_device *dev);

#endif
