/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#ifndef _AICMAC_PLATFORM_H_
#define _AICMAC_PLATFORM_H_

#include <linux/clk.h>
#include <linux/if_vlan.h>
#include <linux/netdevice.h>
#include <linux/phylink.h>
#include <linux/pci.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/net_tstamp.h>
#include <linux/reset.h>
#include <linux/platform_device.h>

#include "aicmac_mac.h"
#include "aicmac_phy.h"
#include "aicmac_mdio.h"
#include "aicmac_dma.h"
#include "aicmac_1588.h"
#include "aicmac_napi.h"

#define AICMAC_RESOURCE_NAME	"gmac"
#define AICMAC_OF_ALIAS_NAME	"gmac"
#define AICMAC_IRQ_NAME		"macirq"
#define AICMAC_PCLK_NAME	"pclk"
#define AICMAC_DEVICE_NAME	"aicgmac"

#define CSR_F_35M		35000000
#define CSR_F_50M		50000000
#define CSR_F_60M		60000000
#define CSR_F_100M		100000000
#define CSR_F_150M		150000000
#define CSR_F_250M		250000000
#define CSR_F_300M		300000000

/* MDC Clock Selection define*/
#define AICMAC_CSR_60_100M	0x0     /* MDC = clk_scr_i/42 */
#define AICMAC_CSR_100_150M	0x1     /* MDC = clk_scr_i/62 */
#define AICMAC_CSR_20_35M	0x2     /* MDC = clk_scr_i/16 */
#define AICMAC_CSR_35_60M	0x3     /* MDC = clk_scr_i/26 */
#define AICMAC_CSR_150_250M	0x4     /* MDC = clk_scr_i/102 */
#define AICMAC_CSR_250_300M	0x5     /* MDC = clk_scr_i/122 */

#define	MAC_CSR_H_FRQ_MASK	0x20

#define AICMAC_CSR_DEFAULT      AICMAC_CSR_150_250M

/* MTL algorithms identifiers */
#define MTL_TX_ALGORITHM_WRR	0x0
#define MTL_TX_ALGORITHM_WFQ	0x1
#define MTL_TX_ALGORITHM_DWRR	0x2
#define MTL_TX_ALGORITHM_SP	0x3
#define MTL_RX_ALGORITHM_SP	0x4
#define MTL_RX_ALGORITHM_WSP	0x5

#define AICMAC_PCS_RGMII	BIT(0)
#define AICMAC_PCS_SGMII	BIT(1)
#define AICMAC_PCS_TBI		BIT(2)
#define AICMAC_PCS_RTBI		BIT(3)

/* RX/TX Queue Mode */
#define MTL_QUEUE_AVB		0x0
#define MTL_QUEUE_DCB		0x1
#define MTL_MAX_RX_QUEUES	8
#define MTL_MAX_TX_QUEUES	8
#define AICMAC_CH_MAX		8

struct aicmac_resources {
	void __iomem *ioaddr;
	void __iomem *ptpaddr;
	unsigned char mac[MAX_ADDR_LEN];

	int wol_irq;
	int lpi_irq;
	int irq;
};

/* MAC/DMA HW capabilities */
struct aicmac_hw_features {
	unsigned int hw_cap_support;
	unsigned int mbps_10_100;
	unsigned int mbps_1000;
	unsigned int half_duplex;
	unsigned int hash_filter;
	unsigned int multi_addr;
	unsigned int pcs;
	unsigned int sma_mdio;
	unsigned int pmt_remote_wake_up;
	unsigned int pmt_magic_frame;
	unsigned int rmon;
	/* IEEE 1588-2002 */
	unsigned int time_stamp;
	/* IEEE 1588-2008 */
	unsigned int atime_stamp;
	/* 802.3az - Energy-Efficient Ethernet (EEE) */
	unsigned int eee;
	unsigned int av;
	unsigned int hash_tb_sz;
	unsigned int tsoen;
	/* TX and RX csum */
	unsigned int tx_coe;
	unsigned int rx_coe;
	unsigned int rxfifo_over_2048;
	/* TX and RX number of channels */
	unsigned int number_rx_channel;
	unsigned int number_tx_channel;
	/* TX and RX number of queues */
	unsigned int number_rx_queues;
	unsigned int number_tx_queues;
	/* PPS output */
	unsigned int pps_out_num;
	/* Alternate (enhanced) DESC mode */
	unsigned int enh_desc;
	/* TX and RX FIFO sizes */
	unsigned int tx_fifo_size;
	unsigned int rx_fifo_size;
	/* Automotive Safety Package */
	unsigned int asp;
	/* RX Parser */
	unsigned int frpsel;
	unsigned int frpbs;
	unsigned int frpes;
	unsigned int addr64;
	unsigned int rssen;
	unsigned int vlhash;
	unsigned int sphen;
	unsigned int vlins;
	unsigned int dvlan;
	unsigned int l3l4fnum;
	unsigned int arpoffsel;
};

struct aicmac_tx_info {
	dma_addr_t buf;
	bool map_as_page;
	unsigned int len;
	bool last_segment;
	bool is_jumbo;
};

struct aicmac_rx_buffer {
	struct page *page;
	struct page *sec_page;
	dma_addr_t addr;
	dma_addr_t sec_addr;
};

struct aicmac_channel {
	struct napi_struct rx_napi ____cacheline_aligned_in_smp;
	struct napi_struct tx_napi ____cacheline_aligned_in_smp;
	struct aicmac_priv *priv_data;
	spinlock_t lock;
	u32 index;
};

struct aicmac_rx_queue {
	dma_addr_t *rx_skbuff_dma;
	struct sk_buff **rx_skbuff;
	unsigned int cur_rx;
	unsigned int dirty_rx;
	dma_addr_t dma_rx_phy;
	struct aicmac_priv *priv_data;
	struct dma_extended_desc *dma_erx;
	unsigned int rx_buff_size;
};

/* Frequently used values are kept adjacent for cache effect */
struct aicmac_tx_queue {
	u32 tx_count_frames;
	struct timer_list txtimer;
	u32 queue_index;
	struct aicmac_priv *priv_data;
	struct dma_extended_desc *dma_etx ____cacheline_aligned_in_smp;
	struct sk_buff **tx_skbuff;
	struct aicmac_tx_info *tx_skbuff_dma;
	unsigned int cur_tx;
	unsigned int dirty_tx;
	dma_addr_t dma_tx_phy;
	u32 tx_tail_addr;
	u32 mss;
};

struct aicmac_rxq_cfg {
	u8 mode_to_use;
	u32 chan;
	u8 pkt_route;
	bool use_prio;
	u32 prio;
};

struct aicmac_txq_cfg {
	u32 weight;
	u8 mode_to_use;
	/* Credit Base Shaper parameters */
	u32 send_slope;
	u32 idle_slope;
	u32 high_credit;
	u32 low_credit;
	bool use_prio;
	u32 prio;
};

struct aicmac_platform_data {
	struct aicmac_mac_data *mac_data;
	struct aicmac_phy_data *phy_data;
	struct aicmac_mdio_data *mdio_data;
	struct aicmac_dma_data *dma_data;
	struct aicmac_1588_data *ptp_data;
	struct aicmac_napi_data *napi_data;
	struct aicmac_hw_features hw_cap;

	int bus_id;
	int clk_csr;

	u32 rx_queues_to_use;
	u32 tx_queues_to_use;
	struct aicmac_rx_queue rx_queue[MTL_MAX_RX_QUEUES];
	struct aicmac_tx_queue tx_queue[MTL_MAX_TX_QUEUES];
	struct aicmac_channel channel[AICMAC_CH_MAX];

	u8 rx_sched_algorithm;
	u8 tx_sched_algorithm;
	struct aicmac_rxq_cfg rx_queues_cfg[MTL_MAX_RX_QUEUES];
	struct aicmac_txq_cfg tx_queues_cfg[MTL_MAX_TX_QUEUES];

	struct clk *aicmac_clk;
	struct clk *pclk;

	struct reset_control *aicmac_rst;
	struct gpio_desc *phy_gpio;

	unsigned long active_vlans[BITS_TO_LONGS(VLAN_N_VID)];
};

#endif
