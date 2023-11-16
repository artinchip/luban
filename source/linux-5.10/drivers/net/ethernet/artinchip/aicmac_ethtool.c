// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/interrupt.h>
#include <linux/mii.h>
#include <linux/phylink.h>
#include <linux/net_tstamp.h>
#include <linux/io.h>

#include "aicmac.h"
#include "aicmac_util.h"
#include "aicmac_dma.h"
#include "aicmac_ethtool.h"

static int aicmac_check_if_running(struct net_device *dev)
{
	if (!netif_running(dev))
		return -EBUSY;
	return 0;
}

static void aicmac_ethtool_getdrvinfo(struct net_device *dev,
				      struct ethtool_drvinfo *info)
{
	struct aicmac_priv *priv = netdev_priv(dev);

	strlcpy(info->driver, AICMAC_DEVICE_NAME, sizeof(info->driver));
	strlcpy(info->version, DRV_MODULE_VERSION, sizeof(info->version));

	sprintf(info->bus_info, "%s-%d:%.2d", AICMAC_DEVICE_NAME, priv->plat->bus_id,
		priv->plat->phy_data->phy_addr);
}

static u32 aicmac_ethtool_getmsglevel(struct net_device *dev)
{
	struct aicmac_priv *priv = netdev_priv(dev);

	return priv->msg_enable;
}

static void aicmac_ethtool_setmsglevel(struct net_device *dev, u32 level)
{
	struct aicmac_priv *priv = netdev_priv(dev);

	priv->msg_enable = level;
}

static void aicmac_ethtool_gregs(struct net_device *dev,
				 struct ethtool_regs *regs, void *space)
{
	struct aicmac_priv *priv = netdev_priv(dev);

	u32 *reg_space = (u32 *)space;

	aicmac_reg_dump_regs(priv->resource->ioaddr, reg_space);
}

static int aicmac_ethtool_get_regs_len(struct net_device *dev)
{
	return REG_SPACE_SIZE;
}

static int aicmac_nway_reset(struct net_device *dev)
{
	struct aicmac_priv *priv = netdev_priv(dev);

	return phylink_ethtool_nway_reset(priv->plat->phy_data->phylink);
}

static void aicmac_get_ringparam(struct net_device *netdev,
				 struct ethtool_ringparam *ring)
{
	ring->rx_max_pending = DMA_MAX_RX_SIZE;
	ring->tx_max_pending = DMA_MAX_TX_SIZE;
	ring->rx_pending = DMA_RX_SIZE;
	ring->tx_pending = DMA_TX_SIZE;
}

static void aicmac_get_pauseparam(struct net_device *netdev,
				  struct ethtool_pauseparam *pause)
{
	struct aicmac_priv *priv = netdev_priv(netdev);

	phylink_ethtool_get_pauseparam(priv->plat->phy_data->phylink, pause);
}

static int aicmac_set_pauseparam(struct net_device *netdev,
				 struct ethtool_pauseparam *pause)
{
	struct aicmac_priv *priv = netdev_priv(netdev);

	return phylink_ethtool_set_pauseparam(priv->plat->phy_data->phylink,
					      pause);
}

static void aicmac_get_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct aicmac_priv *priv = netdev_priv(dev);

	return phylink_ethtool_get_wol(priv->plat->phy_data->phylink, wol);
}

static int aicmac_set_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	int ret;
	struct aicmac_priv *priv = netdev_priv(dev);

	if (!device_can_wakeup(priv->device))
		return -EOPNOTSUPP;

	ret = phylink_ethtool_set_wol(priv->plat->phy_data->phylink, wol);

	if (!ret)
		device_set_wakeup_enable(priv->device, !!wol->wolopts);

	return ret;
}

static int aicmac_get_rxnfc(struct net_device *dev, struct ethtool_rxnfc *rxnfc,
			    u32 *rule_locs)
{
	struct aicmac_priv *priv = netdev_priv(dev);

	switch (rxnfc->cmd) {
	case ETHTOOL_GRXRINGS:
		rxnfc->data = priv->plat->rx_queues_to_use;
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static int aicmac_get_ts_info(struct net_device *dev,
			      struct ethtool_ts_info *info)
{
	struct aicmac_priv *priv = netdev_priv(dev);
	struct ptp_clock *ptp_clock = priv->plat->ptp_data->ptp_clock;

	if (priv->plat->hw_cap.time_stamp || priv->plat->hw_cap.atime_stamp) {
		info->so_timestamping = SOF_TIMESTAMPING_TX_SOFTWARE |
					SOF_TIMESTAMPING_TX_HARDWARE |
					SOF_TIMESTAMPING_RX_SOFTWARE |
					SOF_TIMESTAMPING_RX_HARDWARE |
					SOF_TIMESTAMPING_SOFTWARE |
					SOF_TIMESTAMPING_RAW_HARDWARE;

		if (ptp_clock)
			info->phc_index = ptp_clock_index(ptp_clock);

		info->tx_types = (1 << HWTSTAMP_TX_OFF) | (1 << HWTSTAMP_TX_ON);

		info->rx_filters = (1 << HWTSTAMP_FILTER_NONE) |
				    (1 << HWTSTAMP_FILTER_PTP_V1_L4_EVENT) |
				    (1 << HWTSTAMP_FILTER_PTP_V1_L4_SYNC) |
				    (1 << HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ) |
				    (1 << HWTSTAMP_FILTER_PTP_V2_L4_EVENT) |
				    (1 << HWTSTAMP_FILTER_PTP_V2_L4_SYNC) |
				    (1 << HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ) |
				    (1 << HWTSTAMP_FILTER_PTP_V2_EVENT) |
				    (1 << HWTSTAMP_FILTER_PTP_V2_SYNC) |
				    (1 << HWTSTAMP_FILTER_PTP_V2_DELAY_REQ) |
				    (1 << HWTSTAMP_FILTER_ALL);
		return 0;
	} else {
		return ethtool_op_get_ts_info(dev, info);
	}
}

static int aicmac_get_coalesce(struct net_device *dev,
			       struct ethtool_coalesce *ec)
{
	struct aicmac_priv *priv = netdev_priv(dev);

	ec->tx_coalesce_usecs = priv->tx_coal_timer;
	ec->tx_max_coalesced_frames = priv->tx_coal_frames;

	return 0;
}

static int aicmac_set_coalesce(struct net_device *dev,
			       struct ethtool_coalesce *ec)
{
	struct aicmac_priv *priv = netdev_priv(dev);

	if (ec->tx_coalesce_usecs == 0 && ec->tx_max_coalesced_frames == 0)
		return -EINVAL;

	if (ec->tx_coalesce_usecs > 10000 ||
	    ec->tx_max_coalesced_frames > 256)
		return -EINVAL;

	/* Only copy relevant parameters, ignore all others. */
	priv->tx_coal_frames = ec->tx_max_coalesced_frames;
	priv->tx_coal_timer = ec->tx_coalesce_usecs;
	priv->rx_coal_frames = ec->rx_max_coalesced_frames;

	return 0;
}

static void aicmac_get_channels(struct net_device *dev,
				struct ethtool_channels *chan)
{
	struct aicmac_priv *priv = netdev_priv(dev);

	chan->rx_count = priv->plat->rx_queues_to_use;
	chan->tx_count = priv->plat->tx_queues_to_use;
	chan->max_rx = priv->plat->hw_cap.number_rx_queues;
	chan->max_tx = priv->plat->hw_cap.number_tx_queues;
}

static int aicmac_set_channels(struct net_device *dev,
			       struct ethtool_channels *chan)
{
	return -EOPNOTSUPP;
}

static int aicmac_ethtool_get_link_ksettings(struct net_device *dev,
					     struct ethtool_link_ksettings *cmd)
{
	struct aicmac_priv *priv = netdev_priv(dev);

	return phylink_ethtool_ksettings_get(priv->plat->phy_data->phylink,
					     cmd);
}

static int
aicmac_ethtool_set_link_ksettings(struct net_device *dev,
				  const struct ethtool_link_ksettings *cmd)
{
	struct aicmac_priv *priv = netdev_priv(dev);

	return phylink_ethtool_ksettings_set(priv->plat->phy_data->phylink,
					     cmd);
}

static const struct ethtool_ops aicmac_ethtool_ops = {
	.supported_coalesce_params =
		ETHTOOL_COALESCE_USECS | ETHTOOL_COALESCE_MAX_FRAMES,
	.begin = aicmac_check_if_running,
	.get_drvinfo = aicmac_ethtool_getdrvinfo,
	.get_msglevel = aicmac_ethtool_getmsglevel,
	.set_msglevel = aicmac_ethtool_setmsglevel,
	.get_regs = aicmac_ethtool_gregs,
	.get_regs_len = aicmac_ethtool_get_regs_len,
	.get_link = ethtool_op_get_link,
	.nway_reset = aicmac_nway_reset,
	.get_ringparam = aicmac_get_ringparam,
	.get_pauseparam = aicmac_get_pauseparam,
	.set_pauseparam = aicmac_set_pauseparam,
	.get_wol = aicmac_get_wol,
	.set_wol = aicmac_set_wol,
	.get_rxnfc = aicmac_get_rxnfc,
	.get_ts_info = aicmac_get_ts_info,
	.get_coalesce = aicmac_get_coalesce,
	.set_coalesce = aicmac_set_coalesce,
	.get_channels = aicmac_get_channels,
	.set_channels = aicmac_set_channels,
	.get_link_ksettings = aicmac_ethtool_get_link_ksettings,
	.set_link_ksettings = aicmac_ethtool_set_link_ksettings,
};

void aicmac_set_ethtool_ops(struct net_device *netdev)
{
	netdev->ethtool_ops = &aicmac_ethtool_ops;
}

void aicmac_ethtool_init(void *priv_ptr)
{
}
