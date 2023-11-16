// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/of_device.h>
#include <linux/of_mdio.h>
#include <linux/netdevice.h>

#include "aicmac.h"
#include "aicmac_util.h"
#include "aicmac_mac.h"
#include "aicmac_dma.h"
#include "aicmac_platform.h"
#include "aicmac_gmac_reg.h"
#include "aicmac_dma_reg.h"

static void aicmac_mac_validate(struct phylink_config *config,
				unsigned long *supported,
				struct phylink_link_state *state)
{
	struct aicmac_priv *priv = netdev_priv(to_net_dev(config->dev));
	__ETHTOOL_DECLARE_LINK_MODE_MASK(mac_supported) = { 0, };
	__ETHTOOL_DECLARE_LINK_MODE_MASK(mask) = {
		0,
	};
	int tx_cnt = priv->plat->tx_queues_to_use;
	int max_speed = priv->plat->mac_data->max_speed;

	phylink_set(mac_supported, 10baseT_Half);
	phylink_set(mac_supported, 10baseT_Full);
	phylink_set(mac_supported, 100baseT_Half);
	phylink_set(mac_supported, 100baseT_Full);
	phylink_set(mac_supported, 1000baseT_Half);
	phylink_set(mac_supported, 1000baseT_Full);
	phylink_set(mac_supported, 1000baseKX_Full);

	phylink_set(mac_supported, Autoneg);
	phylink_set(mac_supported, Pause);
	phylink_set(mac_supported, Asym_Pause);
	phylink_set_port_modes(mac_supported);

	/* Cut down 1G if asked to */
	if ((max_speed > 0) && (max_speed < 1000)) {
		phylink_set(mask, 1000baseT_Full);
		phylink_set(mask, 1000baseX_Full);
	}

	/* Half-Duplex can only work with single queue */
	if (tx_cnt > 1) {
		phylink_set(mask, 10baseT_Half);
		phylink_set(mask, 100baseT_Half);
		phylink_set(mask, 1000baseT_Half);
	}

	bitmap_and(supported, supported, mac_supported,
		   __ETHTOOL_LINK_MODE_MASK_NBITS);
	bitmap_andnot(supported, supported, mask,
		      __ETHTOOL_LINK_MODE_MASK_NBITS);
	bitmap_and(state->advertising, state->advertising, mac_supported,
		   __ETHTOOL_LINK_MODE_MASK_NBITS);
	bitmap_andnot(state->advertising, state->advertising, mask,
		      __ETHTOOL_LINK_MODE_MASK_NBITS);
}

static void aicmac_mac_config_interface(struct phylink_config *config,
					unsigned int mode,
					const struct phylink_link_state *state)
{
	struct aicmac_priv *priv = netdev_priv(to_net_dev(config->dev));
	u32 ctrl;

	ctrl = aicmac_mac_reg_get_config(priv->resource->ioaddr);
	ctrl &= ~priv->plat->mac_data->link.speed_mask;

	if (state->interface == PHY_INTERFACE_MODE_USXGMII) {
		switch (state->speed) {
		case SPEED_10000:
			ctrl |= priv->plat->mac_data->link.xgmii.speed10000;
			break;
		case SPEED_5000:
			ctrl |= priv->plat->mac_data->link.xgmii.speed5000;
			break;
		case SPEED_2500:
			ctrl |= priv->plat->mac_data->link.xgmii.speed2500;
			break;
		default:
			return;
		}
	} else {
		switch (state->speed) {
		case SPEED_2500:
			ctrl |= priv->plat->mac_data->link.speed2500;
			break;
		case SPEED_1000:
			ctrl |= priv->plat->mac_data->link.speed1000;
			break;
		case SPEED_100:
			ctrl |= priv->plat->mac_data->link.speed100;
			break;
		case SPEED_10:
			ctrl |= priv->plat->mac_data->link.speed10;
			break;
		default:
			return;
		}
	}

	if (!state->duplex)
		ctrl &= ~priv->plat->mac_data->link.duplex;
	else
		ctrl |= priv->plat->mac_data->link.duplex;

	if (state->pause)
		aicmac_mac_reg_flow_ctrl(priv->plat->mac_data, state->duplex,
					 FLOW_AUTO, PAUSE_TIME,
					 priv->plat->tx_queues_to_use);

	aicmac_mac_reg_set_config(priv->resource->ioaddr, ctrl);

	aicmac_mac_reg_enable_mac(priv->resource->ioaddr, true);
}

static void aicmac_mac_link_down(struct phylink_config *config,
				 unsigned int mode, phy_interface_t interface)
{
	struct aicmac_priv *priv = netdev_priv(to_net_dev(config->dev));

	aicmac_mac_reg_enable_mac(priv->resource->ioaddr, false);

	priv->link_state = LINK_STATE_DOWN;
}

static void aicmac_mac_link_up(struct phylink_config *config,
			       struct phy_device *phy, unsigned int mode,
			       phy_interface_t interface, int speed, int duplex,
			       bool tx_pause, bool rx_pause)
{
	struct aicmac_priv *priv = netdev_priv(to_net_dev(config->dev));
	u32 ctrl;

	ctrl = readl(priv->resource->ioaddr + GMAC_BASIC_CONFIG);
	ctrl &= ~priv->plat->mac_data->link.speed_mask;

	switch (speed) {
	case SPEED_2500:
		ctrl |= priv->plat->mac_data->link.speed2500;
		break;
	case SPEED_1000:
		ctrl |= priv->plat->mac_data->link.speed1000;
		break;
	case SPEED_100:
		ctrl |= priv->plat->mac_data->link.speed100;
		break;
	case SPEED_10:
		ctrl |= priv->plat->mac_data->link.speed10;
		break;
	default:
		return;
	}

	priv->plat->mac_data->mac_port_sel_speed = speed;

	if (!duplex)
		ctrl &= ~priv->plat->mac_data->link.duplex;
	else
		ctrl |= priv->plat->mac_data->link.duplex;

	/* Flow Control operation */
	if (tx_pause && rx_pause)
		aicmac_mac_reg_flow_ctrl(priv->plat->mac_data, duplex,
					 FLOW_AUTO, PAUSE_TIME,
					 priv->plat->tx_queues_to_use);

	writel(ctrl, priv->resource->ioaddr + GMAC_BASIC_CONFIG);

	aicmac_mac_reg_enable_mac(priv->resource->ioaddr, true);

	priv->link_state = LINK_STATE_UP;

#ifdef CONFIG_ARTINCHIP_GMAC_DEBUG
	aicmac_print_reg("linkup", priv->resource->ioaddr, AICMAC_GMAC_REGS_NUM);
#endif
}

static void aicmac_mac_pcs_get_state(struct phylink_config *config,
				     struct phylink_link_state *state)
{
	state->link = 0;
}

static void aicmac_mac_an_restart(struct phylink_config *config)
{
	pr_warn("%s called with unsupported!\n", __func__);
}

static const struct phylink_mac_ops aicmac_phylink_mac_ops = {
	.validate = aicmac_mac_validate,
	.mac_pcs_get_state = aicmac_mac_pcs_get_state,
	.mac_config = aicmac_mac_config_interface,
	.mac_an_restart = aicmac_mac_an_restart,
	.mac_link_down = aicmac_mac_link_down,
	.mac_link_up = aicmac_mac_link_up,
};

struct aicmac_mac_data *aicmac_mac_init_data(struct platform_device *pdev,
					     struct device_node *np)
{
	struct aicmac_mac_data *mac_data = devm_kzalloc(&pdev->dev,
			sizeof(struct aicmac_mac_data), GFP_KERNEL);

	if (of_get_phy_mode(np, &mac_data->mac_interface))
		mac_data->mac_interface = PHY_INTERFACE_MODE_MII;
	mac_data->mac_port_sel_speed =
		aicmac_interface_to_speed(mac_data->mac_interface);
	mac_data->max_speed = mac_data->mac_port_sel_speed;
	mac_data->ps = mac_data->mac_port_sel_speed;

	if (of_property_read_u32(np, "max-speed", &mac_data->max_speed))
		mac_data->max_speed = 1000;

	mac_data->maxmtu = 9000;
	mac_data->riwt_off = 1;
	mac_data->bugged_jumbo = 1;

	mac_data->en_tx_lpi_clockgating = false;
	mac_data->multicast_filter_bins = 256;
	mac_data->unicast_filter_entries = 8;
	mac_data->mcast_bits_log2 = ilog2(mac_data->multicast_filter_bins);
	;

	mac_data->pcs = 0;
	mac_data->pmt = 0;

	mac_data->link.duplex = GMAC_BASIC_CONFIG_DM;
	mac_data->link.speed10 = GMAC_BASIC_CONFIG_PC_10M;
	mac_data->link.speed100 = GMAC_BASIC_CONFIG_PC_100M;
	mac_data->link.speed1000 = GMAC_BASIC_CONFIG_PC_1000M;
	mac_data->link.speed_mask = GMAC_BASIC_CONFIG_PC_MASK;

	mac_data->phylink_mac_ops = &aicmac_phylink_mac_ops;

	pr_info("%s mac_interface:%d max_speed:%d\n", __func__, mac_data->mac_interface,
		mac_data->max_speed);
	return mac_data;
}

int aicmac_mac_ip_init(struct aicmac_priv *priv)
{
	struct aicmac_platform_data *plat = priv->plat;

	plat->hw_cap.hw_cap_support = 1;
	plat->hw_cap.mbps_10_100 = 1;
	plat->hw_cap.mbps_1000 = 1;
	plat->hw_cap.half_duplex = 1;
	plat->hw_cap.hash_filter = 1;
	plat->hw_cap.multi_addr = 1;
	plat->hw_cap.pcs = 0;
	plat->hw_cap.sma_mdio = 1;
	plat->hw_cap.pmt_remote_wake_up = 0;
	plat->hw_cap.pmt_magic_frame = 0;
	/* MMC */
	plat->hw_cap.rmon = 0;
	/* IEEE 1588-2002 */
	plat->hw_cap.time_stamp = 0;
	/* IEEE 1588-2008 */
	plat->hw_cap.atime_stamp = 1;
	/* 802.3az - Energy-Efficient Ethernet (EEE) */
	plat->hw_cap.eee = 0;
	plat->hw_cap.av = 1;
	/* TX and RX csum */
	plat->hw_cap.tx_coe = AICMAC_RX_COE_NONE;
	plat->hw_cap.rxfifo_over_2048 = 0;
	/* TX and RX number of channels */
	plat->hw_cap.number_rx_channel = 1;
	plat->hw_cap.number_tx_channel = 1;
	/* Alternate (enhanced) DESC mode */
	plat->hw_cap.enh_desc = 1;

	return 0;
}

int aicmac_mac_init(void *priv_ptr)
{
	struct aicmac_priv *priv = priv_ptr;
	struct aicmac_mac_data *mac_data = priv->plat->mac_data;

	int ret = 0;

	/* Initialize HW Interface */
	ret = aicmac_mac_ip_init(priv);
	if (ret)
		return ret;

	mac_data->pmt = priv->plat->hw_cap.pmt_remote_wake_up;
	if (priv->plat->hw_cap.hash_tb_sz) {
		mac_data->multicast_filter_bins =
			(BIT(priv->plat->hw_cap.hash_tb_sz) << 5);
		mac_data->mcast_bits_log2 =
			ilog2(mac_data->multicast_filter_bins);
	}

	return 0;
}
