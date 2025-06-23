/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#ifndef _AICMAC_GMAC_REG_H_
#define _AICMAC_GMAC_REG_H_

#include <linux/netdevice.h>

#include "aicmac_mac.h"
#include "aicmac_1588.h"

/*#define REGISTER_NAME		address	SHIFT	VALUE */

/* ----------------- 0x0000 MAC BASIC Configuration ---------------------*/
#define GMAC_BASIC_CONFIG	0x00000000	/* Configuration */
/* Software Reset*/
#define GMAC_BASIC_CONFIG_SFT_RESET		0x00000001
/* Port Select 00/01:GMI 10:10M  11:100M*/
#define GMAC_BASIC_CONFIG_PC_1000M		0x00000002
/* Port Select 00/01:GMI 10:10M  11:100M*/
#define GMAC_BASIC_CONFIG_PC_10M		0x00000004
/* Port Select 00/01:GMI 10:10M  11:100M*/
#define GMAC_BASIC_CONFIG_PC_100M		0x00000006
/* Port Select 00/01:GMI 10:10M  11:100M*/
#define GMAC_BASIC_CONFIG_PC_MASK		0x00000006
/* Loop-back mode */
#define GMAC_BASIC_CONFIG_LM			0x00000008
/* Duplex Mode*/
#define GMAC_BASIC_CONFIG_DM			0x00000010
/* Checksum Offload */
#define GMAC_BASIC_CONFIG_IPC			0x00000080

#define GMAC_BASIC_CONFIG_INIT			GMAC_BASIC_CONFIG_PC_MASK

/* ----------------- 0x001C MAC TX Function --------------------------*/
#define GMAC_TX_FUNC		0x0000001C	/* TX Function */
/* Transmitter Enable */
#define GMAC_TX_FUNC_TE				0x00000001
/* Disable carrier sense */
#define GMAC_TX_FUNC_DCRS			0x00000010
/* Frame Burst Enable */
#define GMAC_TX_FUNC_BE				0x00000100
/* Jabber disable */
#define GMAC_TX_FUNC_JD				0x00000200
#define GMAC_TX_FUNC_INIT			(GMAC_TX_FUNC_DCRS | \
						GMAC_TX_FUNC_JD | \
						GMAC_TX_FUNC_BE)

/* ----------------- 0x0020 MAC RX Function --------------------------*/
#define GMAC_RX_FUNC		0x00000020	/* RX Function */
/* Reveive Enable */
#define GMAC_RX_FUNC_RE				0x00000001
/* Auto Pad/FCS Stripping */
#define GMAC_RX_FUNC_ACS			0x00000002
/* Jumbo frame */
#define GMAC_RX_FUNC_JE				0x00000010
/* IEEE 802.3as 2K packets */
#define GMAC_RX_FUNC_2K				0x00000020
#define GMAC_RX_FUNC_INIT			GMAC_RX_FUNC_ACS

/* ----------------- 0x0034  MAC Flow Control --------------------------*/
#define GMAC_FLOW_CTRL		0x00000034	/* Flow Control */
/* Pause Time Mask */
#define GMAC_FLOW_CTRL_PT_MASK			0xffff0000
#define GMAC_FLOW_CTRL_PT_SHIFT		16
/* Unicast pause frame enable */
#define GMAC_FLOW_CTRL_UP			0x00000008
/* Rx Flow Control Enable */
#define GMAC_FLOW_CTRL_RFE			0x00000004
/* Tx Flow Control Enable */
#define GMAC_FLOW_CTRL_TFE			0x00000002
/* Flow Control Busy ... */
#define GMAC_FLOW_CTRL_FCB_BPA			0x00000001

/* ----------------- 0x0038 VLAN Tag -----------------------------*/
#define GMAC_VLAN_TAG		0x00000038	/* VLAN Tag */

/* ----------------- 0x0040  MAC Frame Filter ---------------------*/
#define GMAC_FRAME_FILTER	0x00000040	/* Frame Filter */
/* Promiscuous Mode */
#define GMAC_FRAME_FILTER_PR			0x00000001
/* Hash Unicast */
#define GMAC_FRAME_FILTER_HUC			0x00000002
/* Hash Multicast */
#define GMAC_FRAME_FILTER_HMC			0x00000004
/* DA Inverse Filtering */
#define GMAC_FRAME_FILTER_DAIF			0x00000008
/* Pass all multicast */
#define GMAC_FRAME_FILTER_PM			0x00000010
/* Disable Broadcast frames */
#define GMAC_FRAME_FILTER_DBF			0x00000020
/* Pass Control frames */
#define GMAC_FRAME_FILTER_PCF			0x00000080
/* Inverse Filtering */
#define GMAC_FRAME_FILTER_SAIF			0x00000100
/* Source Address Filter */
#define GMAC_FRAME_FILTER_SAF			0x00000200
/* Hash or perfect Filter */
#define GMAC_FRAME_FILTER_HPF			0x00000400
/* Receive all mode */
#define GMAC_FRAME_FILTER_RA			0x80000000

/* ----------------- 0x0044  Multicast Hash Table --------------------------*/
/* Multicast Hash Table High */
#define GMAC_HASH_HIGH		0x00000044
/* Multicast Hash Table Low */
#define GMAC_HASH_LOW		0x00000048

/* ----------------- 0x0050 MAC Addr * 8  -----------------------------*/
#define GMAC_ADDR_HIGH(reg)	(((reg) > 7 ? 0x00000800 : 0x00000050) + \
				(reg) * 8)
#define GMAC_ADDR_LOW(reg)	(((reg) > 7 ? 0x00000804 : 0x00000054) + \
				(reg) * 8)
#define GMAC_HI_REG_AE				0x80000000

#define GMAC_MII_ADDR		0x00000090	/* MII Address */
#define GMAC_MII_DATA		0x00000094	/* MII Data */

/* ----------------- 0x0500 Expanded DA Hash filter  ------------------------*/
#define GMAC_EXTHASH_BASE	0x000000500

int aicmac_mac_reg_reset(void __iomem *ioaddr);
void aicmac_mac_reg_core_init(struct aicmac_mac_data *mac,
			      struct net_device *dev);
u32 aicmac_mac_reg_get_config(void __iomem *ioaddr);
void aicmac_mac_reg_set_config(void __iomem *ioaddr, u32 para);
void aicmac_mac_reg_enable_mac(void __iomem *ioaddr, bool enable);
void aicmac_mac_reg_set_mac_addr(void __iomem *ioaddr, u8 addr[6],
				 unsigned int high, unsigned int low);
void aicmac_mac_reg_get_mac_addr(void __iomem *ioaddr, unsigned char *addr,
				 unsigned int high, unsigned int low);
void aicmac_mac_reg_set_eee_pls(void __iomem *ioaddr, int link);
int aicmac_mac_reg_rx_ipc_enable(struct aicmac_mac_data *mac);
void aicmac_mac_reg_get_umac_addr(struct aicmac_mac_data *mac,
				  unsigned char *addr, unsigned int reg_n);
void aicmac_mac_reg_set_umac_addr(struct aicmac_mac_data *mac,
				  unsigned char *addr, unsigned int reg_n);
void aicmac_mac_set_mchash(void __iomem *ioaddr, u32 *mcfilterbits,
			   int mcbitslog2);
void aicmac_mac_reg_set_filter(struct aicmac_mac_data *mac,
			       struct net_device *dev);
void aicmac_mac_reg_flow_ctrl(struct aicmac_mac_data *mac,
			      unsigned int duplex, unsigned int fc,
			      unsigned int pause_time, u32 tx_cnt);
int aicmac_mac_reg_flex_pps_config(void __iomem *ioaddr, int index,
				   struct aicmac_pps_cfg *cfg, bool enable,
				   u32 sub_second_inc, u32 systime_flags);
void aicmac_mac_set_mac_loopback(void __iomem *ioaddr, bool enable);
#endif
