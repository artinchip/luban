// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#include <linux/clk.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/interrupt.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/skbuff.h>
#include <linux/ethtool.h>
#include <linux/if_ether.h>
#include <linux/crc32.h>
#include <linux/mii.h>
#include <linux/if.h>
#include <linux/if_vlan.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/prefetch.h>
#include <linux/pinctrl/consumer.h>
#include <linux/net_tstamp.h>
#include <linux/phylink.h>
#include <net/pkt_cls.h>

#include "aicmac.h"
#include "aicmac_napi.h"

struct aicmac_napi_data *aicmac_napi_init_data(struct platform_device *pdev,
					       struct device_node *np)
{
	return devm_kzalloc(&pdev->dev, sizeof(struct aicmac_napi_data),
			    GFP_KERNEL);
}

int aicmac_napi_init(void *priv_ptr)
{
	struct aicmac_priv *priv = priv_ptr;
	struct net_device *ndev = priv->dev;
	int ret = 0;

	/* Configure real RX and TX queues */
	netif_set_real_num_rx_queues(ndev, priv->plat->rx_queues_to_use);
	netif_set_real_num_tx_queues(ndev, priv->plat->tx_queues_to_use);

	ndev->hw_features = NETIF_F_SG | NETIF_F_CSUM_MASK | NETIF_F_RXCSUM;

	ndev->priv_flags |= IFF_UNICAST_FLT;

	if (priv->plat->hw_cap.sphen)
		ndev->hw_features |= NETIF_F_GRO;

	ndev->features |= ndev->hw_features | NETIF_F_HIGHDMA;
	ndev->watchdog_timeo = msecs_to_jiffies(5000);

#ifdef AICMAC_VLAN_TAG_USED
	/* Both mac100 and gmac support receive VLAN tag detection */
	ndev->features |= NETIF_F_HW_VLAN_CTAG_RX | NETIF_F_HW_VLAN_STAG_RX;
	if (priv->plat->hw_cap.vlhash) {
		ndev->features |= NETIF_F_HW_VLAN_CTAG_FILTER;
		ndev->features |= NETIF_F_HW_VLAN_STAG_FILTER;
	}
	if (priv->plat->hw_cap.vlins) {
		ndev->features |= NETIF_F_HW_VLAN_CTAG_TX;
		if (priv->plat->hw_cap.dvlan)
			ndev->features |= NETIF_F_HW_VLAN_STAG_TX;
	}
#endif

	priv->msg_enable =
		netif_msg_init(AICMAC_NAPI_DEBUG, AICMAC_NAPI_MSG_LEVEL);

	if (priv->plat->hw_cap.rssen)
		ndev->features |= NETIF_F_RXHASH;

	ndev->min_mtu = ETH_ZLEN - ETH_HLEN;
	ndev->max_mtu = JUMBO_LEN;

	if (priv->plat->mac_data->maxmtu < ndev->max_mtu &&
	    priv->plat->mac_data->maxmtu >= ndev->min_mtu)
		ndev->max_mtu = priv->plat->mac_data->maxmtu;
	else if (priv->plat->mac_data->maxmtu < ndev->min_mtu)
		dev_warn(priv->device,
			 "%s: warning: maxmtu having invalid value (%d)\n",
			 __func__, priv->plat->mac_data->maxmtu);

	return ret;
}
