// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#include "aicmac.h"
#include "aicmac_1588.h"
#include "aicmac_dma_desc.h"
#include "aicmac_hwtstamp.h"
#include "aicmac_gmac_reg.h"

static int aicmac_1588_adjust_freq(struct ptp_clock_info *ptp, s32 ppb)
{
	struct aicmac_1588_data *ptp_data =
		container_of(ptp, struct aicmac_1588_data, ptp_clock_ops);
	unsigned long flags;
	u32 diff, addend;
	int neg_adj = 0;
	u64 adj;

	if (ppb < 0) {
		neg_adj = 1;
		ppb = -ppb;
	}

	addend = ptp_data->default_addend;
	adj = addend;
	adj *= ppb;
	diff = div_u64(adj, 1000000000ULL);
	addend = neg_adj ? (addend - diff) : (addend + diff);

	spin_lock_irqsave(&ptp_data->ptp_lock, flags);
	aicmac_hwtstamp_config_addend(ptp_data->ptpaddr, addend);
	spin_unlock_irqrestore(&ptp_data->ptp_lock, flags);

	return 0;
}

static int aicmac_1588_adjust_time(struct ptp_clock_info *ptp, s64 delta)
{
	struct aicmac_1588_data *ptp_data =
		container_of(ptp, struct aicmac_1588_data, ptp_clock_ops);
	unsigned long flags;
	u32 sec, nsec;
	u32 quotient, reminder;
	int neg_adj = 0;

	if (delta < 0) {
		neg_adj = 1;
		delta = -delta;
	}

	quotient = div_u64_rem(delta, 1000000000ULL, &reminder);
	sec = quotient;
	nsec = reminder;

	spin_lock_irqsave(&ptp_data->ptp_lock, flags);
	aicmac_hwtstamp_adjust_systime(ptp_data->ptpaddr, sec, nsec, neg_adj);
	spin_unlock_irqrestore(&ptp_data->ptp_lock, flags);

	return 0;
}

static int aicmac_1588_get_time(struct ptp_clock_info *ptp,
				struct timespec64 *ts)
{
	struct aicmac_1588_data *ptp_data =
		container_of(ptp, struct aicmac_1588_data, ptp_clock_ops);
	unsigned long flags;
	u64 ns = 0;

	spin_lock_irqsave(&ptp_data->ptp_lock, flags);
	aicmac_hwtstamp_get_systime(ptp_data->ptpaddr, &ns);
	spin_unlock_irqrestore(&ptp_data->ptp_lock, flags);

	*ts = ns_to_timespec64(ns);

	return 0;
}

static int aicmac_1588_set_time(struct ptp_clock_info *ptp,
				const struct timespec64 *ts)
{
	struct aicmac_1588_data *ptp_data =
		container_of(ptp, struct aicmac_1588_data, ptp_clock_ops);
	unsigned long flags;

	spin_lock_irqsave(&ptp_data->ptp_lock, flags);
	aicmac_hwtstamp_init_systime(ptp_data->ptpaddr, ts->tv_sec,
				     ts->tv_nsec);
	spin_unlock_irqrestore(&ptp_data->ptp_lock, flags);

	return 0;
}

static int aicmac_1588_enable(struct ptp_clock_info *ptp,
			      struct ptp_clock_request *rq, int on)
{
	int ret = -EOPNOTSUPP;

	switch (rq->type) {
	case PTP_CLK_REQ_PEROUT:
		/* Reject requests with unsupported flags */
		if (rq->perout.flags)
			return -EOPNOTSUPP;
		break;
	default:
		break;
	}

	return ret;
}

/* structure describing a PTP hardware clock */
static struct ptp_clock_info aicmac_1588_clock_ops = {
	.owner = THIS_MODULE,
	.name = "aicmac ptp",
	.max_adj = 62500000,
	.n_alarm = 0,
	.n_ext_ts = 0,
	.n_per_out = 0, /* will be overwritten in aicmac_ptp_register */
	.n_pins = 0,
	.pps = 0,
	.adjfreq = aicmac_1588_adjust_freq,
	.adjtime = aicmac_1588_adjust_time,
	.gettime64 = aicmac_1588_get_time,
	.settime64 = aicmac_1588_set_time,
	.enable = aicmac_1588_enable,
};

int aicmac_1588_init(void *priv_ptr)
{
	int ret = 0;

	return ret;
}

int aicmac_1588_register(void *priv_ptr)
{
	struct aicmac_priv *priv = priv_ptr;
	struct aicmac_platform_data *plat = priv->plat;
	struct aicmac_1588_data *ptp_data = priv->plat->ptp_data;
	int i, ret = 0;

	if (!(plat->hw_cap.time_stamp || plat->hw_cap.atime_stamp))
		return -EOPNOTSUPP;

	priv->adv_ts = 0;
	if (priv->extend_desc && plat->hw_cap.atime_stamp)
		priv->adv_ts = 1;

	ptp_data->hwts_tx_en = 0;
	ptp_data->hwts_rx_en = 0;

	for (i = 0; i < plat->hw_cap.pps_out_num; i++) {
		if (i >= AICMAC_PPS_MAX)
			break;
		ptp_data->pps[i].available = true;
	}

	if (ptp_data->ptp_max_adj)
		aicmac_1588_clock_ops.max_adj = ptp_data->ptp_max_adj;

	aicmac_1588_clock_ops.n_per_out = priv->plat->hw_cap.pps_out_num;

	spin_lock_init(&ptp_data->ptp_lock);
	ptp_data->ptp_clock_ops = aicmac_1588_clock_ops;

	ptp_data->ptp_clock =
		ptp_clock_register(&ptp_data->ptp_clock_ops, priv->device);
	if (IS_ERR(ptp_data->ptp_clock)) {
		netdev_err(priv->dev, "ptp_clock_register failed\n");
		ptp_data->ptp_clock = NULL;
		ret = -1;
	} else if (ptp_data->ptp_clock) {
		netdev_info(priv->dev, "registered PTP clock\n");
	}

	return ret;
}

/**
 * aicmac_ptp_unregister
 * @priv: driver private structure
 * Description: this function will remove/unregister the ptp clock driver
 * from the kernel.
 */
void aicmac_1588_destroy(void *priv_ptr)
{
	struct aicmac_priv *priv = priv_ptr;
	struct aicmac_1588_data *ptp_data = priv->plat->ptp_data;

	if (ptp_data->clk_ptp_ref)
		clk_disable_unprepare(ptp_data->clk_ptp_ref);

	if (ptp_data->ptp_clock) {
		ptp_clock_unregister(ptp_data->ptp_clock);
		ptp_data->ptp_clock = NULL;
	}
}

struct aicmac_1588_data *aicmac_1588_init_data(struct platform_device *pdev,
					       struct device_node *np)
{
	struct aicmac_1588_data *ptp_data = devm_kzalloc(&pdev->dev,
		sizeof(struct aicmac_1588_data), GFP_KERNEL);

	return ptp_data;
}

void aicmac_1588_init_clk(struct platform_device *pdev,
			  struct aicmac_1588_data *ptp_data, struct clk *clk)
{
	ptp_data->clk_ptp_ref = devm_clk_get(&pdev->dev, AICMAC_PTP_REF_NAME);
	if (IS_ERR(ptp_data->clk_ptp_ref)) {
#ifdef CONFIG_DEBUG_ON_FPGA_BOARD_ARTINCHIP
		ptp_data->clk_ptp_rate = AICMAC_CSR_60_100M;
#else
		ptp_data->clk_ptp_rate = clk_get_rate(clk);
#endif

		ptp_data->clk_ptp_ref = NULL;
	} else {
		ptp_data->clk_ptp_rate = clk_get_rate(ptp_data->clk_ptp_ref);
	}
}

int aicmac_1588_hwtstamp_set(struct net_device *dev, struct ifreq *ifr)
{
	struct aicmac_priv *priv = netdev_priv(dev);
	struct aicmac_1588_data *ptp_data = priv->plat->ptp_data;
	struct hwtstamp_config config;
	struct timespec64 now;
	u64 temp = 0;
	u32 ptp_v2 = 0;
	u32 tstamp_all = 0;
	u32 ptp_over_ipv4_udp = 0;
	u32 ptp_over_ipv6_udp = 0;
	u32 ptp_over_ethernet = 0;
	u32 snap_type_sel = 0;
	u32 ts_master_en = 0;
	u32 ts_event_en = 0;
	u32 sec_inc = 0;
	u32 value = 0;

	if (!(priv->plat->hw_cap.time_stamp || priv->adv_ts)) {
		netdev_alert(priv->dev, "No support for HW time stamping\n");
		ptp_data->hwts_tx_en = 0;
		ptp_data->hwts_rx_en = 0;

		return -EOPNOTSUPP;
	}

	if (copy_from_user(&config, ifr->ifr_data, sizeof(config)))
		return -EFAULT;

	netdev_dbg(priv->dev,
		   "%s config flags:0x%x, tx_type:0x%x, rx_filter:0x%x\n",
		   __func__, config.flags, config.tx_type, config.rx_filter);

	/* reserved for future extensions */
	if (config.flags)
		return -EINVAL;

	if (config.tx_type != HWTSTAMP_TX_OFF &&
	    config.tx_type != HWTSTAMP_TX_ON)
		return -ERANGE;

	if (priv->adv_ts) {
		switch (config.rx_filter) {
		case HWTSTAMP_FILTER_NONE:
			/* time stamp no incoming packet at all */
			config.rx_filter = HWTSTAMP_FILTER_NONE;
			break;

		case HWTSTAMP_FILTER_PTP_V1_L4_EVENT:
			/* PTP v1, UDP, any kind of event packet */
			config.rx_filter = HWTSTAMP_FILTER_PTP_V1_L4_EVENT;
			/* 'xmac' hardware can support Sync, Pdelay_Req and
			 * Pdelay_resp by setting bit14 and bits17/16 to 01
			 * This leaves Delay_Req timestamps out.
			 * Enable all events *and* general purpose message
			 * timestamping
			 */
			snap_type_sel = PTP_TCR_SNAPTYPSEL_1;
			ptp_over_ipv4_udp = PTP_TCR_TSIPV4ENA;
			ptp_over_ipv6_udp = PTP_TCR_TSIPV6ENA;
			break;

		case HWTSTAMP_FILTER_PTP_V1_L4_SYNC:
			/* PTP v1, UDP, Sync packet */
			config.rx_filter = HWTSTAMP_FILTER_PTP_V1_L4_SYNC;
			/* take time stamp for SYNC messages only */
			ts_event_en = PTP_TCR_TSEVNTENA;

			ptp_over_ipv4_udp = PTP_TCR_TSIPV4ENA;
			ptp_over_ipv6_udp = PTP_TCR_TSIPV6ENA;
			break;

		case HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ:
			/* PTP v1, UDP, Delay_req packet */
			config.rx_filter = HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ;
			/* take time stamp for Delay_Req messages only */
			ts_master_en = PTP_TCR_TSMSTRENA;
			ts_event_en = PTP_TCR_TSEVNTENA;

			ptp_over_ipv4_udp = PTP_TCR_TSIPV4ENA;
			ptp_over_ipv6_udp = PTP_TCR_TSIPV6ENA;
			break;

		case HWTSTAMP_FILTER_PTP_V2_L4_EVENT:
			/* PTP v2, UDP, any kind of event packet */
			config.rx_filter = HWTSTAMP_FILTER_PTP_V2_L4_EVENT;
			ptp_v2 = PTP_TCR_TSVER2ENA;
			/* take time stamp for all event messages */
			snap_type_sel = PTP_TCR_SNAPTYPSEL_1;

			ptp_over_ipv4_udp = PTP_TCR_TSIPV4ENA;
			ptp_over_ipv6_udp = PTP_TCR_TSIPV6ENA;
			break;

		case HWTSTAMP_FILTER_PTP_V2_L4_SYNC:
			/* PTP v2, UDP, Sync packet */
			config.rx_filter = HWTSTAMP_FILTER_PTP_V2_L4_SYNC;
			ptp_v2 = PTP_TCR_TSVER2ENA;
			/* take time stamp for SYNC messages only */
			ts_event_en = PTP_TCR_TSEVNTENA;

			ptp_over_ipv4_udp = PTP_TCR_TSIPV4ENA;
			ptp_over_ipv6_udp = PTP_TCR_TSIPV6ENA;
			break;

		case HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ:
			/* PTP v2, UDP, Delay_req packet */
			config.rx_filter = HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ;
			ptp_v2 = PTP_TCR_TSVER2ENA;
			/* take time stamp for Delay_Req messages only */
			ts_master_en = PTP_TCR_TSMSTRENA;
			ts_event_en = PTP_TCR_TSEVNTENA;

			ptp_over_ipv4_udp = PTP_TCR_TSIPV4ENA;
			ptp_over_ipv6_udp = PTP_TCR_TSIPV6ENA;
			break;

		case HWTSTAMP_FILTER_PTP_V2_EVENT:
			/* PTP v2/802.AS1 any layer, any kind of event packet */
			config.rx_filter = HWTSTAMP_FILTER_PTP_V2_EVENT;
			ptp_v2 = PTP_TCR_TSVER2ENA;
			snap_type_sel = PTP_TCR_SNAPTYPSEL_1;
			ts_event_en = PTP_TCR_TSEVNTENA;
			ptp_over_ipv4_udp = PTP_TCR_TSIPV4ENA;
			ptp_over_ipv6_udp = PTP_TCR_TSIPV6ENA;
			ptp_over_ethernet = PTP_TCR_TSIPENA;
			break;

		case HWTSTAMP_FILTER_PTP_V2_SYNC:
			/* PTP v2/802.AS1, any layer, Sync packet */
			config.rx_filter = HWTSTAMP_FILTER_PTP_V2_SYNC;
			ptp_v2 = PTP_TCR_TSVER2ENA;
			/* take time stamp for SYNC messages only */
			ts_event_en = PTP_TCR_TSEVNTENA;

			ptp_over_ipv4_udp = PTP_TCR_TSIPV4ENA;
			ptp_over_ipv6_udp = PTP_TCR_TSIPV6ENA;
			ptp_over_ethernet = PTP_TCR_TSIPENA;
			break;

		case HWTSTAMP_FILTER_PTP_V2_DELAY_REQ:
			/* PTP v2/802.AS1, any layer, Delay_req packet */
			config.rx_filter = HWTSTAMP_FILTER_PTP_V2_DELAY_REQ;
			ptp_v2 = PTP_TCR_TSVER2ENA;
			/* take time stamp for Delay_Req messages only */
			ts_master_en = PTP_TCR_TSMSTRENA;
			ts_event_en = PTP_TCR_TSEVNTENA;

			ptp_over_ipv4_udp = PTP_TCR_TSIPV4ENA;
			ptp_over_ipv6_udp = PTP_TCR_TSIPV6ENA;
			ptp_over_ethernet = PTP_TCR_TSIPENA;
			break;

		case HWTSTAMP_FILTER_NTP_ALL:
		case HWTSTAMP_FILTER_ALL:
			/* time stamp any incoming packet */
			config.rx_filter = HWTSTAMP_FILTER_ALL;
			tstamp_all = PTP_TCR_TSENALL;
			break;

		default:
			return -ERANGE;
		}
	} else {
		switch (config.rx_filter) {
		case HWTSTAMP_FILTER_NONE:
			config.rx_filter = HWTSTAMP_FILTER_NONE;
			break;
		default:
			/* PTP v1, UDP, any kind of event packet */
			config.rx_filter = HWTSTAMP_FILTER_PTP_V1_L4_EVENT;
			break;
		}
	}
	ptp_data->hwts_rx_en =
		((config.rx_filter == HWTSTAMP_FILTER_NONE) ? 0 : 1);

	ptp_data->hwts_tx_en = config.tx_type == HWTSTAMP_TX_ON;

	if (!ptp_data->hwts_tx_en && !ptp_data->hwts_rx_en) {
		aicmac_hwtstamp_config_hw_tstamping(ptp_data->ptpaddr, 0);
	} else {
		value = (PTP_TCR_TSENA | PTP_TCR_TSCFUPDT | PTP_TCR_TSCTRLSSR |
			 tstamp_all | ptp_v2 | ptp_over_ethernet |
			 ptp_over_ipv6_udp | ptp_over_ipv4_udp | ts_event_en |
			 ts_master_en | snap_type_sel);
		aicmac_hwtstamp_config_hw_tstamping(ptp_data->ptpaddr, value);

		/* program Sub Second Increment reg */
		aicmac_hwtstamp_config_sub_second_increment(ptp_data->ptpaddr,
				ptp_data->clk_ptp_rate, &sec_inc);
		temp = div_u64(1000000000ULL, sec_inc);

		/* Store sub second increment and flags for later use */
		ptp_data->sub_second_inc = sec_inc;
		ptp_data->systime_flags = value;

		/* calculate default added value:
		 * formula is :
		 * addend = (2^32)/freq_div_ratio;
		 * where, freq_div_ratio = 1e9ns/sec_inc
		 */
		temp = (u64)(temp << 32);
		ptp_data->default_addend =
			div_u64(temp, ptp_data->clk_ptp_rate);
		aicmac_hwtstamp_config_addend(ptp_data->ptpaddr,
					      ptp_data->default_addend);

		/* initialize system time */
		ktime_get_real_ts64(&now);

		/* lower 32 bits of tv_sec are safe until y2106 */
		aicmac_hwtstamp_init_systime(ptp_data->ptpaddr, (u32)now.tv_sec,
					     now.tv_nsec);
	}

	memcpy(&ptp_data->tstamp_config, &config, sizeof(config));

	return copy_to_user(ifr->ifr_data, &config, sizeof(config)) ? -EFAULT :
								      0;
}

int aicmac_1588_hwtstamp_get(struct net_device *dev, struct ifreq *ifr)
{
	struct aicmac_priv *priv = netdev_priv(dev);
	struct hwtstamp_config *config = &priv->plat->ptp_data->tstamp_config;

	if (!(priv->plat->hw_cap.time_stamp || priv->plat->hw_cap.atime_stamp))
		return -EOPNOTSUPP;

	return copy_to_user(ifr->ifr_data, config, sizeof(*config)) ? -EFAULT :
								      0;
}

void aicmac_1588_get_tx_hwtstamp(void *priv_ptr, void *p_ptr,
				 struct sk_buff *skb)
{
	struct aicmac_priv *priv = priv_ptr;
	struct dma_desc *p = p_ptr;
	struct skb_shared_hwtstamps shhwtstamp;
	bool found = false;
	u64 ns = 0;

	if (!priv->plat->ptp_data->hwts_tx_en)
		return;

	/* exit if skb doesn't support hw tstamp */
	if (likely(!skb || !(skb_shinfo(skb)->tx_flags & SKBTX_IN_PROGRESS)))
		return;

	/* check tx tstamp status */
	if (aicmac_dma_desc_get_tx_timestamp_status(p)) {
		aicmac_dma_desc_get_timestamp(p, priv->adv_ts, &ns);
		found = true;
	}

	if (found) {
		memset(&shhwtstamp, 0, sizeof(struct skb_shared_hwtstamps));
		shhwtstamp.hwtstamp = ns_to_ktime(ns);

		netdev_dbg(priv->dev, "get valid TX hw timestamp %llu\n", ns);
		/* pass tstamp to stack */
		skb_tstamp_tx(skb, &shhwtstamp);
	}
}

void aicmac_1588_get_rx_hwtstamp(void *priv_ptr, void *p_ptr, void *np_ptr,
				 struct sk_buff *skb)
{
	struct aicmac_priv *priv = priv_ptr;
	struct skb_shared_hwtstamps *shhwtstamp = NULL;
	struct dma_desc *desc = p_ptr;
	u64 ns = 0;

	if (!priv->plat->ptp_data->hwts_rx_en)
		return;

	/* Check if timestamp is available */
	if (aicmac_dma_desc_get_rx_timestamp_status(p_ptr, np_ptr,
						    priv->adv_ts)) {
		aicmac_dma_desc_get_timestamp(desc, priv->adv_ts, &ns);
		netdev_dbg(priv->dev, "get valid RX hw timestamp %llu\n", ns);
		shhwtstamp = skb_hwtstamps(skb);
		memset(shhwtstamp, 0, sizeof(struct skb_shared_hwtstamps));
		shhwtstamp->hwtstamp = ns_to_ktime(ns);
	} else {
		netdev_dbg(priv->dev, "cannot get RX hw timestamp\n");
	}
}
