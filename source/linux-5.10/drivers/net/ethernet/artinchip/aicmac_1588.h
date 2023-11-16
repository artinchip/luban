/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#ifndef	__AICMAC_1588_H__
#define	__AICMAC_1588_H__

#include <linux/ptp_clock_kernel.h>
#include <linux/net_tstamp.h>

#include "aicmac.h"

#define AICMAC_PTP_REF_NAME    "ptp_ref"

/* IEEE 1588 PTP register offsets */
/* Timestamp Control Reg */
#define	PTP_TCR				0x00
/* Sub-Second Increment Reg */
#define	PTP_SSIR			0x04
/* System Time – Seconds Regr */
#define	PTP_STSR			0x08
/* System Time – Nanoseconds Reg */
#define	PTP_STNSR			0x0c
/* System Time – Seconds Update Reg */
#define	PTP_STSUR			0x10
/* System Time – Nanoseconds Update Reg */
#define	PTP_STNSUR			0x14
/* Timestamp Addend Reg */
#define	PTP_TAR				0x18

#define	PTP_STNSUR_ADDSUB_SHIFT		31
/* 10e9-1 ns */
#define	PTP_DIGITAL_ROLLOVER_MODE	0x3B9ACA00
/* ~0.466 ns */
#define	PTP_BINARY_ROLLOVER_MODE	0x80000000

/* PTP Timestamp control register defines */
/* Timestamp Enable */
#define	PTP_TCR_TSENA			BIT(0)
/* Timestamp Fine/Coarse Update */
#define	PTP_TCR_TSCFUPDT		BIT(1)
/* Timestamp Initialize */
#define	PTP_TCR_TSINIT			BIT(2)
/* Timestamp Update */
#define	PTP_TCR_TSUPDT			BIT(3)
/* Timestamp Interrupt Trigger Enable */
#define	PTP_TCR_TSTRIG			BIT(4)
/* Addend Reg Update */
#define	PTP_TCR_TSADDREG		BIT(5)
/* Enable Timestamp for All Frames */
#define	PTP_TCR_TSENALL			BIT(8)
/* Digital or Binary Rollover Control */
#define	PTP_TCR_TSCTRLSSR		BIT(9)
/* Enable PTP packet Processing for Version 2 Format */
#define	PTP_TCR_TSVER2ENA		BIT(10)
/* Enable Processing of PTP over Ethernet Frames */
#define	PTP_TCR_TSIPENA			BIT(11)
/* Enable Processing of PTP Frames Sent over IPv6-UDP */
#define	PTP_TCR_TSIPV6ENA		BIT(12)
/* Enable Processing of PTP Frames Sent over IPv4-UDP */
#define	PTP_TCR_TSIPV4ENA		BIT(13)
/* Enable Timestamp Snapshot for Event Messages */
#define	PTP_TCR_TSEVNTENA		BIT(14)
/* Enable Snapshot for Messages Relevant to Master */
#define	PTP_TCR_TSMSTRENA		BIT(15)
/* Select PTP packets for Taking Snapshots
 * On gmac4 specifically:
 * Enable SYNC, Pdelay_Req, Pdelay_Resp when TSEVNTENA is enabled.
 * or
 * Enable  SYNC, Follow_Up, Delay_Req, Delay_Resp, Pdelay_Req, Pdelay_Resp,
 * Pdelay_Resp_Follow_Up if TSEVNTENA is disabled
 */
#define	PTP_TCR_SNAPTYPSEL_1		BIT(16)
/* Enable MAC address for PTP Frame Filtering */
#define	PTP_TCR_TSENMACADDR		BIT(18)

/* SSIR defines */
#define	PTP_SSIR_SSINC_MASK		0xff
#define	GMAC4_PTP_SSIR_SSINC_SHIFT	16

#define AICMAC_PPS_MAX		4
struct aicmac_pps_cfg {
	bool available;
	struct timespec64 start;
	struct timespec64 period;
};

struct aicmac_1588_data {
	void __iomem *ptpaddr;
	s32 ptp_max_adj;

	struct clk *clk_ptp_ref;
	unsigned int clk_ptp_rate;
	unsigned int clk_ref_rate;
	unsigned int default_addend;

	spinlock_t ptp_lock;			/*global lock for ptp module*/
	struct ptp_clock *ptp_clock;
	struct ptp_clock_info ptp_clock_ops;
	struct hwtstamp_config tstamp_config;
	struct aicmac_pps_cfg pps[AICMAC_PPS_MAX];

	int hwts_tx_en;
	int hwts_rx_en;
	u32 sub_second_inc;
	u32 systime_flags;
};

struct aicmac_1588_data *aicmac_1588_init_data(struct platform_device *pdev,
					       struct device_node *np);
void aicmac_1588_init_clk(struct platform_device *pdev,
			  struct aicmac_1588_data *ptp_data, struct clk *clk);
int aicmac_1588_init(void *priv_ptr);
int aicmac_1588_register(void *priv_ptr);
void aicmac_1588_destroy(void *priv_ptr);
int aicmac_1588_hwtstamp_set(struct net_device *dev, struct ifreq *ifr);
int aicmac_1588_hwtstamp_get(struct net_device *dev, struct ifreq *ifr);
void aicmac_1588_get_tx_hwtstamp(void *priv_ptr, void *p_ptr,
				 struct sk_buff *skb);
void aicmac_1588_get_rx_hwtstamp(void *priv_ptr, void *p_ptr, void *np_ptr,
				 struct sk_buff *skb);

#endif	/* __AICMAC_1588_H__ */
