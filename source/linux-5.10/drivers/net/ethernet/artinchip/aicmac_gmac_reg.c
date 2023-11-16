// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#include <linux/io.h>
#include <linux/bitrev.h>
#include <linux/crc32.h>
#include <linux/iopoll.h>
#include <net/dsa.h>

#include "aicmac_mac.h"
#include "aicmac_gmac_reg.h"

int aicmac_mac_reg_reset(void __iomem *ioaddr)
{
	u32 value = readl(ioaddr + GMAC_BASIC_CONFIG);

	/* DMA SW reset */
	value |= GMAC_BASIC_CONFIG_SFT_RESET;
	writel(value, ioaddr + GMAC_BASIC_CONFIG);

	return readl_poll_timeout(ioaddr + GMAC_BASIC_CONFIG, value,
				  !(value & GMAC_BASIC_CONFIG_SFT_RESET), 10000,
				  200000);
}

static void aicmac_mac_reg_init_basic_config(struct aicmac_mac_data *mac,
					     struct net_device *dev)
{
	void __iomem *ioaddr = mac->pcsr;
	u32 value = readl(ioaddr + GMAC_BASIC_CONFIG);

	switch (mac->mac_port_sel_speed) {
	case SPEED_100:
		value |= GMAC_BASIC_CONFIG_PC_100M;
		break;
	case SPEED_1000:
		value |= GMAC_BASIC_CONFIG_PC_1000M;
		break;
	default:
		value |= GMAC_BASIC_CONFIG_INIT;
		break;
	}

	writel(value, ioaddr + GMAC_BASIC_CONFIG);
}

static void aicmac_mac_reg_init_tx_func(struct aicmac_mac_data *mac,
					struct net_device *dev)
{
	void __iomem *ioaddr = mac->pcsr;
	u32 value = readl(ioaddr + GMAC_TX_FUNC);

	value |= GMAC_TX_FUNC_INIT;

	writel(value, ioaddr + GMAC_TX_FUNC);
}

static void aicmac_mac_reg_init_rx_func(struct aicmac_mac_data *mac,
					struct net_device *dev)
{
	struct aicmac_priv *priv = netdev_priv(dev);
	void __iomem *ioaddr = mac->pcsr;
	u32 value = readl(ioaddr + GMAC_RX_FUNC);
	int mtu = dev->mtu;

	value |= GMAC_RX_FUNC_INIT;

	if (netdev_uses_dsa(dev) || !priv->plat->hw_cap.enh_desc)
		value &= ~GMAC_RX_FUNC_ACS;

	if (mtu > 1500)
		value |= GMAC_RX_FUNC_2K;
	if (mtu > 2000)
		value |= GMAC_RX_FUNC_JE;

	writel(value, ioaddr + GMAC_RX_FUNC);
}

void aicmac_mac_reg_core_init(struct aicmac_mac_data *mac,
			      struct net_device *dev)
{
	aicmac_mac_reg_init_basic_config(mac, dev);
	aicmac_mac_reg_init_tx_func(mac, dev);
	aicmac_mac_reg_init_rx_func(mac, dev);
	//Mask GMAC interrupts delete
#ifdef AICMAC_VLAN_TAG_USED
	/* Tag detection without filtering */
	writel(0x0, mac->pcsr + GMAC_VLAN_TAG);
#endif
}

u32 aicmac_mac_reg_get_config(void __iomem *ioaddr)
{
	return readl(ioaddr + GMAC_BASIC_CONFIG);
}

void aicmac_mac_reg_set_config(void __iomem *ioaddr, u32 ctrl)
{
	writel(ctrl, ioaddr + GMAC_BASIC_CONFIG);
}

static void aicmac_mac_reg_enable_tx(void __iomem *ioaddr, bool enable)
{
	u32 value = readl(ioaddr + GMAC_TX_FUNC);

	if (enable)
		value |= GMAC_TX_FUNC_TE;
	else
		value &= ~GMAC_TX_FUNC_TE;

	writel(value, ioaddr + GMAC_TX_FUNC);
}

static void aicmac_mac_reg_enable_rx(void __iomem *ioaddr, bool enable)
{
	u32 value = readl(ioaddr + GMAC_RX_FUNC);

	if (enable)
		value |= GMAC_RX_FUNC_RE;
	else
		value &= ~GMAC_RX_FUNC_RE;

	writel(value, ioaddr + GMAC_RX_FUNC);
}

void aicmac_mac_reg_enable_mac(void __iomem *ioaddr, bool enable)
{
	aicmac_mac_reg_enable_tx(ioaddr, enable);
	aicmac_mac_reg_enable_rx(ioaddr, enable);
}

void aicmac_mac_reg_set_mac_addr(void __iomem *ioaddr, u8 addr[6],
				 unsigned int high, unsigned int low)
{
	unsigned long data;

	data = (addr[5] << 8) | addr[4];
	writel(data | GMAC_HI_REG_AE, ioaddr + high);
	data = (addr[3] << 24) | (addr[2] << 16) | (addr[1] << 8) | addr[0];
	writel(data, ioaddr + low);
}

void aicmac_mac_reg_get_mac_addr(void __iomem *ioaddr, unsigned char *addr,
				 unsigned int high, unsigned int low)
{
	unsigned int hi_addr, lo_addr;

	/* Read the MAC address from the hardware */
	hi_addr = readl(ioaddr + high);
	lo_addr = readl(ioaddr + low);

	/* Extract the MAC address from the high and low words */
	addr[0] = lo_addr & 0xff;
	addr[1] = (lo_addr >> 8) & 0xff;
	addr[2] = (lo_addr >> 16) & 0xff;
	addr[3] = (lo_addr >> 24) & 0xff;
	addr[4] = hi_addr & 0xff;
	addr[5] = (hi_addr >> 8) & 0xff;
}

int aicmac_mac_reg_rx_ipc_enable(struct aicmac_mac_data *mac)
{
	void __iomem *ioaddr = mac->pcsr;
	u32 value = readl(ioaddr + GMAC_BASIC_CONFIG);

	value &= ~GMAC_BASIC_CONFIG_IPC;

	writel(value, ioaddr + GMAC_BASIC_CONFIG);

	value = readl(ioaddr + GMAC_BASIC_CONFIG);

	return !!(value & GMAC_BASIC_CONFIG);
}

void aicmac_mac_reg_set_umac_addr(struct aicmac_mac_data *mac,
				  unsigned char *addr, unsigned int reg_n)
{
	void __iomem *ioaddr = mac->pcsr;

	aicmac_mac_reg_set_mac_addr(ioaddr, addr, GMAC_ADDR_HIGH(reg_n),
				    GMAC_ADDR_LOW(reg_n));
}

void aicmac_mac_reg_get_umac_addr(struct aicmac_mac_data *mac,
				  unsigned char *addr, unsigned int reg_n)
{
	void __iomem *ioaddr = mac->pcsr;

	aicmac_mac_reg_get_mac_addr(ioaddr, addr, GMAC_ADDR_HIGH(reg_n),
				    GMAC_ADDR_LOW(reg_n));
}

void aicmac_mac_reg_set_mchash(void __iomem *ioaddr, u32 *mcfilterbits,
			       int mcbitslog2)
{
	int numhashregs, regs;

	switch (mcbitslog2) {
	case 6:
		writel(mcfilterbits[0], ioaddr + GMAC_HASH_LOW);
		writel(mcfilterbits[1], ioaddr + GMAC_HASH_HIGH);
		return;
	case 7:
		numhashregs = 4;
		break;
	case 8:
		numhashregs = 8;
		break;
	default:
		pr_info("AICMAC: err in setting multicast filter\n");
		return;
	}
	for (regs = 0; regs < numhashregs; regs++)
		writel(mcfilterbits[regs],
		       ioaddr + GMAC_EXTHASH_BASE + regs * 4);
}

void aicmac_mac_reg_set_filter(struct aicmac_mac_data *mac,
			       struct net_device *dev)
{
	void __iomem *ioaddr = (void __iomem *)dev->base_addr;
	unsigned int value = 0;
	unsigned int perfect_addr_number = mac->unicast_filter_entries;
	u32 mc_filter[8];
	int mcbitslog2 = mac->mcast_bits_log2;

	pr_info("%s: # mcasts %d, # unicast %d\n", __func__,
		netdev_mc_count(dev), netdev_uc_count(dev));

	memset(mc_filter, 0, sizeof(mc_filter));

	if (dev->flags & IFF_PROMISC)
		value = GMAC_FRAME_FILTER_PR | GMAC_FRAME_FILTER_PCF;
	else if (dev->flags & IFF_ALLMULTI)
		value = GMAC_FRAME_FILTER_PM; /* pass all multi */
	else if (!netdev_mc_empty(dev) && (mcbitslog2 == 0))
		/* Fall back to all multicast if we've no filter */
		value = GMAC_FRAME_FILTER_PM;
	else if (!netdev_mc_empty(dev)) {
		struct netdev_hw_addr *ha;

		value = GMAC_FRAME_FILTER_HMC;

		netdev_for_each_mc_addr(ha, dev) {
			int bit_nr =
				bitrev32(~crc32_le(~0, ha->addr, ETH_ALEN)) >>
				(32 - mcbitslog2);
			mc_filter[bit_nr >> 5] |= 1 << (bit_nr & 31);
		}
	}

	value |= GMAC_FRAME_FILTER_HPF;
	aicmac_mac_reg_set_mchash(ioaddr, mc_filter, mcbitslog2);

	if (netdev_uc_count(dev) > perfect_addr_number) {
		value |= GMAC_FRAME_FILTER_PR;
	} else {
		int reg = 1;
		struct netdev_hw_addr *ha;

		netdev_for_each_uc_addr(ha, dev) {
			aicmac_mac_reg_set_mac_addr(ioaddr, ha->addr,
						    GMAC_ADDR_HIGH(reg),
						    GMAC_ADDR_LOW(reg));
			reg++;
		}

		while (reg < perfect_addr_number) {
			writel(0, ioaddr + GMAC_ADDR_HIGH(reg));
			writel(0, ioaddr + GMAC_ADDR_LOW(reg));
			reg++;
		}
	}

#ifdef FRAME_FILTER_DEBUG
	/* Enable Receive all mode (to debug filtering_fail errors) */
	value |= GMAC_FRAME_FILTER_RA;
#endif
	writel(value, ioaddr + GMAC_FRAME_FILTER);
}

void aicmac_mac_reg_flow_ctrl(struct aicmac_mac_data *mac, unsigned int duplex,
			      unsigned int fc, unsigned int pause_time,
			      u32 tx_cnt)
{
	void __iomem *ioaddr = mac->pcsr;
	/* Set flow such that DZPQ in Mac Register 6 is 0,
	 * and unicast pause detect is enabled.
	 */
	unsigned int flow = GMAC_FLOW_CTRL_UP;

	if (fc & FLOW_RX)
		flow |= GMAC_FLOW_CTRL_RFE;
	if (fc & FLOW_TX)
		flow |= GMAC_FLOW_CTRL_TFE;

	if (duplex)
		flow |= (pause_time << GMAC_FLOW_CTRL_PT_SHIFT);

	writel(flow, ioaddr + GMAC_FLOW_CTRL);
}

void aicmac_mac_set_mac_loopback(void __iomem *ioaddr, bool enable)
{
	u32 value = readl(ioaddr + GMAC_BASIC_CONFIG);

	if (enable)
		value |= GMAC_BASIC_CONFIG_LM;
	else
		value &= ~GMAC_BASIC_CONFIG_LM;

	writel(value, ioaddr + GMAC_BASIC_CONFIG);
}
