/**
 ******************************************************************************
 *
 * @file asr_tx.c
 *
 * @brief tx related operation
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */
#include <linux/version.h>
#include <linux/dma-mapping.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>
#include "asr_defs.h"
#include "asr_tx.h"
#include "asr_msg_tx.h"
#include "asr_events.h"
#if (defined CFG_SNIFFER_SUPPORT || defined CFG_CUS_FRAME)
#include "asr_idle_mode.h"
#endif
#ifdef CONFIG_ASR_SDIO
#include "asr_sdio.h"
#endif
#include "asr_hif.h"
#include <linux/ktime.h>
#include "asr_bus.h"
#ifdef CONFIG_ASR_USB
#include "asr_usb.h"
#endif
#ifdef ASR_REDUCE_TCP_ACK
#include <net/tcp.h>
#endif

#if (defined CFG_SNIFFER_SUPPORT || defined CFG_CUS_FRAME)
//IDLE_HWQ_IDX:0-3
#define IDLE_HWQ_IDX    0
#endif

extern bool txlogen;
extern int flow_ctrl_high;
extern int flow_ctrl_low;
extern bool asr_rx_process_running(struct asr_hw *asr_hw);
extern bool mrole_enable;
extern bool p2p_debug;

/******************************************************************************
 * Power Save functions
 *****************************************************************************/
/**
 * asr_set_traffic_status - Inform FW if traffic is available for STA in PS
 *
 * @asr_hw: Driver main data
 * @sta: Sta in PS mode
 * @available: whether traffic is buffered for the STA
 * @ps_id: type of PS data requested (@LEGACY_PS_ID or @UAPSD_ID)
  */
void asr_set_traffic_status(struct asr_hw *asr_hw, struct asr_sta *sta, bool available, u8 ps_id)
{
	bool uapsd = (ps_id != LEGACY_PS_ID);
	asr_send_me_traffic_ind(asr_hw, sta->sta_idx, uapsd, available);
}

/**
 * asr_ps_bh_enable - Enable/disable PS mode for one STA
 *
 * @asr_hw: Driver main data
 * @sta: Sta which enters/leaves PS mode
 * @enable: PS mode status
 *
 * This function will enable/disable PS mode for one STA.
 * When enabling PS mode:
 *  - Stop all STA's txq for ASR_TXQ_STOP_STA_PS reason
 *  - Count how many buffers are already ready for this STA
 *  - For BC/MC sta, update all queued SKB to use hw_queue BCMC
 *  - Update TIM if some packet are ready
 *
 * When disabling PS mode:
 *  - Start all STA's txq for ASR_TXQ_STOP_STA_PS reason
 *  - For BC/MC sta, update all queued SKB to use hw_queue AC_BE
 *  - Update TIM if some packet are ready (otherwise fw will not update TIM
 *    in beacon for this STA)
 *
 * All counter/skb updates are protected from TX path by taking tx_lock
 *
 * NOTE: _bh_ in function name indicates that this function is called
 * from a bottom_half tasklet.
 */
void asr_update_sta_ps_pkt_num(struct asr_hw *asr_hw,struct asr_sta *sta)
{
	struct sk_buff *skb_pending;
	struct sk_buff *pnext;
	struct hostdesc * hostdesc_tmp = NULL;
	struct asr_vif *asr_vif_tmp = NULL;
	u8 tid_tmp ;

    if (sta == NULL)
		return ;

    #ifdef CONFIG_ASR_SDIO
	// loop list and get avail skbs.
	skb_queue_walk_safe(&asr_hw->tx_sk_list, skb_pending, pnext) {
		hostdesc_tmp = (struct hostdesc *)skb_pending->data;
		if ((hostdesc_tmp->vif_idx < asr_hw->vif_max_num + asr_hw->sta_max_num)) {
			asr_vif_tmp = asr_hw->vif_table[hostdesc_tmp->vif_idx];
		        if (asr_vif_tmp && hostdesc_tmp->staid == sta->sta_idx) {
                    /*
					hostdesc->tid = tid;
                    tid = 0xFF, non qos data or mgt frame.
                    tid TID_0~TID7,TID_MGT   qos data.

					hostdesc->queue_idx = txq->hwq->id;
					queue_idx : hw queue, BK/BE/VI/VO/Beacon.
					*/
					tid_tmp = hostdesc_tmp->tid;

					if (tid_tmp >= NX_NB_TXQ_PER_STA)
						tid_tmp = 0;

                    if ((sta->uapsd_tids & tid_tmp) && !(is_multicast_sta(asr_hw,sta->sta_idx)))
					    sta->ps.pkt_ready[UAPSD_ID] ++;
					else
					    sta->ps.pkt_ready[LEGACY_PS_ID] ++;

			    }
		} else {
				dev_err(asr_hw->dev," unlikely: mrole tx(vif_idx %d not valid)!!!\r\n",hostdesc_tmp->vif_idx);
		}
	}
    #endif
}

struct asr_traffic_status g_asr_traffic_sts;
void asr_ps_bh_enable(struct asr_hw *asr_hw, struct asr_sta *sta,
                       bool enable)
{
    //struct asr_txq *txq;

    if (enable) {
        //trace_ps_enable(sta);

        spin_lock(&asr_hw->tx_lock);
        sta->ps.active = true;
        sta->ps.sp_cnt[LEGACY_PS_ID] = 0;
        sta->ps.sp_cnt[UAPSD_ID] = 0;

        // remove sta per txq from hw_list
		// sdio mode not affect, instead ignore sta ps pkt in tx_sk_list.
        //asr_txq_sta_stop(sta, ASR_TXQ_STOP_STA_PS, asr_hw);

        if (is_multicast_sta(asr_hw,sta->sta_idx)) {
            //txq = asr_txq_sta_get(sta, 0, NULL,asr_hw);
			
            sta->ps.pkt_ready[LEGACY_PS_ID] = 0;  //skb_queue_len(&txq->sk_list);
            sta->ps.pkt_ready[UAPSD_ID] = 0;

            asr_update_sta_ps_pkt_num(asr_hw,sta);

            //txq->hwq = &asr_hw->hwq[ASR_HWQ_BCMC];
        } else {
            //int i;
            sta->ps.pkt_ready[LEGACY_PS_ID] = 0;
            sta->ps.pkt_ready[UAPSD_ID] = 0;

            #if 0
            foreach_sta_txq(sta, txq, i, asr_hw) {
                sta->ps.pkt_ready[txq->ps_id] += skb_queue_len(&txq->sk_list);
            }
            #else
            // txq will use LEGACY_PS_ID or UAPSD_ID.
            asr_update_sta_ps_pkt_num(asr_hw,sta);
            #endif
        }

        spin_unlock(&asr_hw->tx_lock);

        #if 0
        if (sta->ps.pkt_ready[LEGACY_PS_ID]) {
            asr_set_traffic_status(asr_hw, sta, true, LEGACY_PS_ID);
        }

        if (sta->ps.pkt_ready[UAPSD_ID])
            asr_set_traffic_status(asr_hw, sta, true, UAPSD_ID);

        if ( sta->ps.pkt_ready[LEGACY_PS_ID] ||  sta->ps.pkt_ready[UAPSD_ID])
	    dev_err(asr_hw->dev," [ps]true: sta-%d, uapsd=0x%x, (%d , %d) \r\n",sta->sta_idx,sta->uapsd_tids,
                                                   sta->ps.pkt_ready[LEGACY_PS_ID],  sta->ps.pkt_ready[UAPSD_ID] );
		 #else
         if (sta->ps.pkt_ready[LEGACY_PS_ID] || sta->ps.pkt_ready[UAPSD_ID]) {

               g_asr_traffic_sts.send = true;
			   g_asr_traffic_sts.asr_sta_ps = sta;
			   g_asr_traffic_sts.tx_ava = true;
			   g_asr_traffic_sts.ps_id_bits = (sta->ps.pkt_ready[LEGACY_PS_ID] ? CO_BIT(LEGACY_PS_ID) : 0) |
			                                  (sta->ps.pkt_ready[UAPSD_ID] ? CO_BIT(UAPSD_ID) : 0);
		 } else {
               g_asr_traffic_sts.send = false;
		 }
		#endif
    }
	else
    {
        //trace_ps_disable(sta->sta_idx);

        spin_lock(&asr_hw->tx_lock);
        sta->ps.active = false;

        #if 0  //not used for sdio mode.
        if (is_multicast_sta(asr_hw,sta->sta_idx)) {
            txq = asr_txq_sta_get(sta, 0, NULL,asr_hw);
			
            txq->hwq = &asr_hw->hwq[ASR_HWQ_BE];
            txq->push_limit = 0;

        } else {

            int i;
            foreach_sta_txq(sta, txq, i, asr_hw) {
                txq->push_limit = 0;
            }
        }

		
        // readd sta per txq to hw_list
		// sdio mode not affect, instead re-trigger tx task and sta pkt in tx_sk_list will process.
        asr_txq_sta_start(sta, ASR_TXQ_STOP_STA_PS, asr_hw);
		#endif

        spin_unlock(&asr_hw->tx_lock);

#ifdef CONFIG_ASR_SDIO
        // re-trigger tx task.
		set_bit(ASR_FLAG_MAIN_TASK_BIT, &asr_hw->ulFlag);
		wake_up_interruptible(&asr_hw->waitq_main_task_thead);
#endif

        #if 0
        if (sta->ps.pkt_ready[LEGACY_PS_ID])
            asr_set_traffic_status(asr_hw, sta, false, LEGACY_PS_ID);

        if (sta->ps.pkt_ready[UAPSD_ID])
            asr_set_traffic_status(asr_hw, sta, false, UAPSD_ID);

        if ( sta->ps.pkt_ready[LEGACY_PS_ID] ||  sta->ps.pkt_ready[UAPSD_ID])
	    dev_err(asr_hw->dev," [ps]false:sta-%d, uapsd=0x%x, (%d , %d) \r\n",sta->sta_idx,sta->uapsd_tids,
                                                   sta->ps.pkt_ready[LEGACY_PS_ID],  sta->ps.pkt_ready[UAPSD_ID] );
		#else
         if (sta->ps.pkt_ready[LEGACY_PS_ID] || sta->ps.pkt_ready[UAPSD_ID]) {

               g_asr_traffic_sts.send = true;
			   g_asr_traffic_sts.asr_sta_ps = sta;
			   g_asr_traffic_sts.tx_ava = false;
			   g_asr_traffic_sts.ps_id_bits = (sta->ps.pkt_ready[LEGACY_PS_ID] ? CO_BIT(LEGACY_PS_ID) : 0) |
			                                  (sta->ps.pkt_ready[UAPSD_ID] ? CO_BIT(UAPSD_ID) : 0);
		 } else {
               g_asr_traffic_sts.send = false;
		 }
		#endif
    }
}

/**
 * asr_ps_bh_traffic_req - Handle traffic request for STA in PS mode
 *
 * @asr_hw: Driver main data
 * @sta: Sta which enters/leaves PS mode
 * @pkt_req: number of pkt to push
 * @ps_id: type of PS data requested (@LEGACY_PS_ID or @UAPSD_ID)
 *
 * This function will make sure that @pkt_req are pushed to fw
 * whereas the STA is in PS mode.
 * If request is 0, send all traffic
 * If request is greater than available pkt, reduce request
 * Note: request will also be reduce if txq credits are not available
 *
 * All counter updates are protected from TX path by taking tx_lock
 *
 * NOTE: _bh_ in function name indicates that this function is called
 * from the bottom_half tasklet.
 */


/// This value is sent to host, as the number of packet of a service period, to inicate
/// that current service period has been interrrupted.
#define PS_SP_INTERRUPTED          0xff
void asr_ps_bh_traffic_req(struct asr_hw *asr_hw, struct asr_sta *sta,
                            u16 pkt_req, u8 ps_id)
{
    #if 0  //FIXME

    int pkt_ready_all;
    struct asr_txq *txq;

    if (WARN(!sta->ps.active, "sta %pM is not in Power Save mode",
             sta->mac_addr))
        return;

    spin_lock(&asr_hw->tx_lock);

    /* Fw may ask to stop a service period with PS_SP_INTERRUPTED. This only
       happens for p2p-go interface if NOA starts during a service period */
    if ((pkt_req == PS_SP_INTERRUPTED) && (ps_id == UAPSD_ID)) {
        int tid;
        sta->ps.sp_cnt[ps_id] = 0;
        foreach_sta_txq(sta, txq, tid, asr_hw) {
            txq->push_limit = 0;
        }
        goto done;
    }

    pkt_ready_all = (sta->ps.pkt_ready[ps_id] - sta->ps.sp_cnt[ps_id]);

    /* Don't start SP until previous one is finished or we don't have
       packet ready (which must not happen for U-APSD) */
    if (sta->ps.sp_cnt[ps_id] || pkt_ready_all <= 0) {
        goto done;
    }

    /* Adapt request to what is available. */
    if (pkt_req == 0 || pkt_req > pkt_ready_all) {
        pkt_req = pkt_ready_all;
    }

    /* Reset the SP counter */
    sta->ps.sp_cnt[ps_id] = 0;

    /* "dispatch" the request between txq */
    if (is_multicast_sta(sta->sta_idx)) {
        txq = asr_txq_sta_get(sta, 0, asr_hw);
        if (txq->credits <= 0)
            goto done;
        if (pkt_req > txq->credits)
            pkt_req = txq->credits;
        txq->push_limit = pkt_req;
        sta->ps.sp_cnt[ps_id] = pkt_req;
        asr_txq_add_to_hw_list(txq);
    } else {
        int i, tid;

        foreach_sta_txq_prio(sta, txq, tid, i, asr_hw) {
            u16 txq_len = skb_queue_len(&txq->sk_list);

            if (txq->ps_id != ps_id)
                continue;

            if (txq_len > txq->credits)
                txq_len = txq->credits;

            if (txq_len == 0)
                continue;

            if (txq_len < pkt_req) {
                /* Not enough pkt queued in this txq, add this
                   txq to hwq list and process next txq */
                pkt_req -= txq_len;
                txq->push_limit = txq_len;
                sta->ps.sp_cnt[ps_id] += txq_len;
                asr_txq_add_to_hw_list(txq);
            } else {
                /* Enough pkt in this txq to comlete the request
                   add this txq to hwq list and stop processing txq */
                txq->push_limit = pkt_req;
                sta->ps.sp_cnt[ps_id] += pkt_req;
                asr_txq_add_to_hw_list(txq);
                break;
            }
        }
    }

  done:
    spin_unlock(&asr_hw->tx_lock);

    #else
    // used to limit tx pkt num.


    #endif
}

/******************************************************************************
 * TX functions
 *****************************************************************************/
#define PRIO_STA_NULL 0xAA

static const int asr_down_hwq2tid[3] = {
	[ASR_HWQ_BK] = 2,
	[ASR_HWQ_BE] = 3,
	[ASR_HWQ_VI] = 5,
};

static void asr_downgrade_ac(struct asr_sta *sta, struct sk_buff *skb)
{
	s8 ac = asr_tid2hwq[skb->priority];

	WARN((ac > ASR_HWQ_VO), "Unexepcted ac %d for skb before downgrade", ac);
	if (ac > ASR_HWQ_VO) {
		ac = ASR_HWQ_VO;
	}

	while (sta->acm & BIT(ac)) {
		if (ac <= ASR_HWQ_BK) {
			skb->priority = 1;
			return;
		}
		ac--;
		skb->priority = asr_down_hwq2tid[ac];
	}
}

/**
 * u16 (*ndo_select_queue)(struct net_device *dev, struct sk_buff *skb,
 *                         void *accel_priv, select_queue_fallback_t fallback);
 *    Called to decide which queue to when device supports multiple
 *    transmit queues.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
u16 asr_select_queue(struct net_device * dev, struct sk_buff * skb, struct net_device * sb_dev)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
u16 asr_select_queue(struct net_device * dev, struct sk_buff * skb,
		     struct net_device * sb_dev, select_queue_fallback_t fallback)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
u16 asr_select_queue(struct net_device * dev, struct sk_buff * skb, void *accel_priv, select_queue_fallback_t fallback)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
u16 asr_select_queue(struct net_device * dev, struct sk_buff * skb, void *accel_priv)
#else
u16 asr_select_queue(struct net_device * dev, struct sk_buff * skb)
#endif
{
	struct asr_vif *asr_vif = netdev_priv(dev);
	struct asr_hw *asr_hw = asr_vif->asr_hw;
	struct wireless_dev *wdev = &asr_vif->wdev;
	struct asr_sta *sta = NULL;
	struct asr_txq *txq;
	u16 netdev_queue;
	ASR_DBG(ASR_FN_ENTRY_STR);

	switch (wdev->iftype) {
	case NL80211_IFTYPE_STATION:
	case NL80211_IFTYPE_P2P_CLIENT:
		sta = asr_vif->sta.ap;
		break;
	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_P2P_GO:
		{
			struct asr_sta *cur;
			struct ethhdr *eth = (struct ethhdr *)skb->data;

			if (is_multicast_ether_addr(eth->h_dest)) {
				sta = &asr_hw->sta_table[asr_vif->ap.bcmc_index];
			} else {
				list_for_each_entry(cur, &asr_vif->ap.sta_list, list) {
					if (!memcmp(cur->mac_addr, eth->h_dest, ETH_ALEN)) {
						sta = cur;
						break;
					}
				}
			}

			//dev_info(asr_hw->dev, "%s: %d,%d,%02X:%02X:%02X:%02X:%02X:%02X\n",
			//      __func__, asr_vif->ap.bcmc_index, sta?sta->sta_idx:-1,
			//      eth->h_dest[0], eth->h_dest[1], eth->h_dest[2], eth->h_dest[3], eth->h_dest[4], eth->h_dest[5]);
			break;
		}

	default:
		break;
	}

	if (sta && sta->qos) {
		/* use the data classifier to determine what 802.1d tag the
		 * data frame has */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
		skb->priority = cfg80211_classify8021d(skb, NULL) & IEEE80211_QOS_CTL_TAG1D_MASK;
#else
		skb->priority = cfg80211_classify8021d(skb) & IEEE80211_QOS_CTL_TAG1D_MASK;
#endif

		if (sta->acm)
			asr_downgrade_ac(sta, skb);

		txq = asr_txq_sta_get(sta, skb->priority, NULL, asr_hw);
		netdev_queue = txq->ndev_idx;
	} else if (sta) {
		skb->priority = 0xFF;

		txq = asr_txq_sta_get(sta, 0, NULL, asr_hw);
		netdev_queue = txq->ndev_idx;
	} else {
		/* This packet will be dropped in xmit function, still need to select
		   an active queue for xmit to be called. As it most likely to happen
		   for AP interface, select BCMC queue
		   (TODO: select another queue if BCMC queue is stopped) */
		skb->priority = PRIO_STA_NULL;
		netdev_queue = NX_NB_TID_PER_STA * asr_hw->sta_max_num;
	}

	BUG_ON(netdev_queue >= NX_NB_NDEV_TXQ);
	//dev_info(asr_hw->dev, "[%s]:netdev_queue=%d,sta_idx=%d\n",__func__,netdev_queue,sta?sta->sta_idx:-1);

	return netdev_queue;
}

/**
 * asr_get_tx_info - Get STA and tid for one skb
 *
 * @asr_vif: vif ptr
 * @skb: skb
 * @tid: pointer updated with the tid to use for this skb
 *
 * @return: pointer on the destination STA (may be NULL)
 *
 * skb has already been parsed in asr_select_queue function
 * simply re-read information form skb.
 */
static struct asr_sta *asr_get_tx_info(struct asr_vif *asr_vif, struct sk_buff *skb, u8 * tid)
{
	struct asr_hw *asr_hw = asr_vif->asr_hw;
	struct asr_sta *sta;
	int sta_idx;

	*tid = skb->priority;
	if (unlikely(skb->priority == PRIO_STA_NULL)) {
		return NULL;
	} else {
		int ndev_idx = skb_get_queue_mapping(skb);

		if (ndev_idx == NX_NB_TID_PER_STA * asr_hw->sta_max_num)
			sta_idx = asr_hw->sta_max_num + master_vif_idx(asr_vif);
		else
			sta_idx = ndev_idx / NX_NB_TID_PER_STA;

		//dev_info(asr_hw->dev, "%s:tid=%d,sta_idx=%d,ndev_idx=%d,vif_index=%d,priority=%u\n",
		//      __func__, *tid, sta_idx, ndev_idx, asr_vif->vif_index, skb->priority);

		sta = &asr_hw->sta_table[sta_idx];
	}

	return sta;
}

#ifdef CONFIG_ASR_AMSDUS_TX
/* return size of subframe (including header) */
static inline int asr_amsdu_subframe_length(struct ethhdr *eth, int eth_len)
{
	/* ethernet header is replaced with amdsu header that have the same size
	   Only need to check if LLC/SNAP header will be added */
	int len = eth_len;

	if (ntohs(eth->h_proto) >= ETH_P_802_3_MIN) {
		len += sizeof(rfc1042_header) + 2;
	}

	return len;
}

static inline bool asr_amsdu_is_aggregable(struct sk_buff *skb)
{
	/* need to add some check on buffer to see if it can be aggregated ? */
	return true;
}

/**
 * asr_amsdu_del_subframe_header - remove AMSDU header
 *
 * amsdu_txhdr: amsdu tx descriptor
 *
 * Move back the ethernet header at the "beginning" of the data buffer.
 * (which has been moved in @asr_amsdu_add_subframe_header)
 */
static void asr_amsdu_del_subframe_header(struct asr_amsdu_txhdr *amsdu_txhdr)
{
	struct sk_buff *skb = amsdu_txhdr->skb;
	struct ethhdr *eth;
	u8 *pos;

	pos = skb->data;
	pos += sizeof(struct asr_amsdu_txhdr);
	eth = (struct ethhdr *)pos;
	pos += amsdu_txhdr->pad + sizeof(struct ethhdr);

	if (ntohs(eth->h_proto) >= ETH_P_802_3_MIN) {
		pos += sizeof(rfc1042_header) + 2;
	}

	memmove(pos, eth, sizeof(*eth));
	skb_pull(skb, (pos - skb->data));
}

/**
 * asr_amsdu_add_subframe_header - Add AMSDU header and link subframe
 *
 * @asr_hw Driver main data
 * @skb Buffer to aggregate
 * @sw_txhdr Tx descriptor for the first A-MSDU subframe
 *
 * return 0 on sucess, -1 otherwise
 *
 * This functions Add A-MSDU header and LLC/SNAP header in the buffer
 * and update sw_txhdr of the first subframe to link this buffer.
 * If an error happens, the buffer will be queued as a normal buffer.
 *
 *
 *            Before           After
 *         +-------------+  +-------------+
 *         | HEADROOM    |  | HEADROOM    |
 *         |             |  +-------------+ <- data
 *         |             |  | amsdu_txhdr |
 *         |             |  | * pad size  |
 *         |             |  +-------------+
 *         |             |  | ETH hdr     | keep original eth hdr
 *         |             |  |             | to restore it once transmitted
 *         |             |  +-------------+ <- packet_addr[x]
 *         |             |  | Pad         |
 *         |             |  +-------------+
 * data -> +-------------+  | AMSDU HDR   |
 *         | ETH hdr     |  +-------------+
 *         |             |  | LLC/SNAP    |
 *         +-------------+  +-------------+
 *         | DATA        |  | DATA        |
 *         |             |  |             |
 *         +-------------+  +-------------+
 *
 * Called with tx_lock hold
 */
static int asr_amsdu_add_subframe_header(struct asr_hw *asr_hw, struct sk_buff *skb, struct asr_sw_txhdr *sw_txhdr)
{
	struct asr_amsdu *amsdu = &sw_txhdr->amsdu;
	struct asr_amsdu_txhdr *amsdu_txhdr;
	struct ethhdr *amsdu_hdr, *eth = (struct ethhdr *)skb->data;
	int headroom_need, map_len, msdu_len;
	dma_addr_t dma_addr;
	u8 *pos, *map_start;

	msdu_len = skb->len - sizeof(*eth);
	headroom_need = sizeof(*amsdu_txhdr) + amsdu->pad + sizeof(*amsdu_hdr);
	if (ntohs(eth->h_proto) >= ETH_P_802_3_MIN) {
		headroom_need += sizeof(rfc1042_header) + 2;
		msdu_len += sizeof(rfc1042_header) + 2;
	}

	/* we should have enough headroom (checked in xmit) */
	if (WARN_ON(skb_headroom(skb) < headroom_need)) {
		return -1;
	}

	/* allocate headroom */
	pos = skb_push(skb, headroom_need);
	amsdu_txhdr = (struct asr_amsdu_txhdr *)pos;
	pos += sizeof(*amsdu_txhdr);

	/* move eth header */
	memmove(pos, eth, sizeof(*eth));
	eth = (struct ethhdr *)pos;
	pos += sizeof(*eth);

	/* Add padding from previous subframe */
	map_start = pos;
	memset(pos, 0, amsdu->pad);
	pos += amsdu->pad;

	/* Add AMSDU hdr */
	amsdu_hdr = (struct ethhdr *)pos;
	memcpy(amsdu_hdr->h_dest, eth->h_dest, ETH_ALEN);
	memcpy(amsdu_hdr->h_source, eth->h_source, ETH_ALEN);
	amsdu_hdr->h_proto = htons(msdu_len);
	pos += sizeof(*amsdu_hdr);

	if (ntohs(eth->h_proto) >= ETH_P_802_3_MIN) {
		memcpy(pos, rfc1042_header, sizeof(rfc1042_header));
		pos += sizeof(rfc1042_header);
	}

	/* MAP (and sync) memory for DMA */
	map_len = msdu_len + amsdu->pad + sizeof(*amsdu_hdr);
	// not use pcie interface, remove dma_map_single later
	dma_addr = dma_map_single(asr_hw->dev, map_start, map_len, DMA_BIDIRECTIONAL);
	if (WARN_ON(dma_mapping_error(asr_hw->dev, dma_addr))) {
		pos -= sizeof(*eth);
		memmove(pos, eth, sizeof(*eth));
		skb_pull(skb, headroom_need);
		return -1;
	}

	/* update amdsu_txhdr */
	amsdu_txhdr->map_len = map_len;
	amsdu_txhdr->dma_addr = dma_addr;
	amsdu_txhdr->skb = skb;
	amsdu_txhdr->pad = amsdu->pad;

	/* update asr_sw_txhdr (of the first subframe) */
	BUG_ON(amsdu->nb != sw_txhdr->desc.host.packet_cnt);
	sw_txhdr->desc.host.packet_addr[amsdu->nb] = dma_addr;
	sw_txhdr->desc.host.packet_len[amsdu->nb] = map_len;
	sw_txhdr->desc.host.packet_cnt++;
	amsdu->nb++;

	amsdu->pad = AMSDU_PADDING(map_len - amsdu->pad);
	list_add_tail(&amsdu_txhdr->list, &amsdu->hdrs);
	amsdu->len += map_len;

	//trace_amsdu_subframe(sw_txhdr);
	return 0;
}

/**
 * asr_amsdu_add_subframe - Add this buffer as an A-MSDU subframe if possible
 *
 * @asr_hw Driver main data
 * @skb Buffer to aggregate if possible
 * @sta Destination STA
 * @txq sta's txq used for this buffer
 *
 * Tyr to aggregate the buffer in an A-MSDU. If it succeed then the
 * buffer is added as a new A-MSDU subframe with AMSDU and LLC/SNAP
 * headers added (so FW won't have to modify this subframe).
 *
 * To be added as subframe :
 * - sta must allow amsdu
 * - buffer must be aggregable (to be defined)
 * - at least one other aggregable buffer is pending in the queue
 *  or an a-msdu (with enough free space) is currently in progress
 *
 * returns true if buffer has been added as A-MDSP subframe, false otherwise
 *
 */
static bool asr_amsdu_add_subframe(struct asr_hw *asr_hw, struct sk_buff *skb, struct asr_sta *sta, struct asr_txq *txq)
{
	bool res = false;
	struct ethhdr *eth;

	/* immedialtely return if amsdu are not allowed for this sta */
	if (!txq->amsdu_len || asr_hw->mod_params->amsdu_maxnb < 2 || !asr_amsdu_is_aggregable(skb)
	    )
		return false;

	spin_lock_bh(&asr_hw->tx_lock);
	if (txq->amsdu) {
		/* aggreagation already in progress, add this buffer if enough space
		   available, otherwise end the current amsdu */
		struct asr_sw_txhdr *sw_txhdr = txq->amsdu;
		eth = (struct ethhdr *)(skb->data);

		if (((sw_txhdr->amsdu.len + sw_txhdr->amsdu.pad +
		      asr_amsdu_subframe_length(eth, skb->len)) > txq->amsdu_len)
		    || asr_amsdu_add_subframe_header(asr_hw, skb, sw_txhdr)) {
			txq->amsdu = NULL;
			goto end;
		}

		if (sw_txhdr->amsdu.nb >= asr_hw->mod_params->amsdu_maxnb) {
			asr_hw->stats.amsdus[sw_txhdr->amsdu.nb - 1].done++;
			/* max number of subframes reached */
			txq->amsdu = NULL;
		}
	} else {
		/* Check if a new amsdu can be started with the previous buffer
		   (if any) and this one */
		struct sk_buff *skb_prev = skb_peek_tail(&txq->sk_list);
		struct asr_txhdr *txhdr;
		struct asr_sw_txhdr *sw_txhdr;
		int len1, len2;

		if (!skb_prev || !asr_amsdu_is_aggregable(skb_prev))
			goto end;

		txhdr = (struct asr_txhdr *)skb_prev->data;
		sw_txhdr = txhdr->sw_hdr;
		if ((sw_txhdr->amsdu.len) || (sw_txhdr->desc.host.flags & TXU_CNTRL_RETRY))
			/* previous buffer is already a complete amsdu or a retry */
			goto end;

		eth = (struct ethhdr *)(skb_prev->data + sw_txhdr->headroom);
		len1 = asr_amsdu_subframe_length(eth, (sw_txhdr->frame_len + sizeof(struct ethhdr)));

		eth = (struct ethhdr *)(skb->data);
		len2 = asr_amsdu_subframe_length(eth, skb->len);

		if (len1 + AMSDU_PADDING(len1) + len2 > txq->amsdu_len)
			/* not enough space to aggregate those two buffers */
			goto end;

		/* Add subframe header.
		   Note: Fw will take care of adding AMDSU header for the first
		   subframe while generating 802.11 MAC header */
		INIT_LIST_HEAD(&sw_txhdr->amsdu.hdrs);
		sw_txhdr->amsdu.len = len1;
		sw_txhdr->amsdu.nb = 1;
		sw_txhdr->amsdu.pad = AMSDU_PADDING(len1);
		if (asr_amsdu_add_subframe_header(asr_hw, skb, sw_txhdr))
			goto end;

		sw_txhdr->desc.host.flags |= TXU_CNTRL_AMSDU;

		if (sw_txhdr->amsdu.nb < asr_hw->mod_params->amsdu_maxnb)
			txq->amsdu = sw_txhdr;
		else
			asr_hw->stats.amsdus[sw_txhdr->amsdu.nb - 1].done++;
	}

	res = true;

end:
	spin_unlock_bh(&asr_hw->tx_lock);
	return res;
}
#endif /* CONFIG_ASR_AMSDUS_TX */

void asr_tx_flow_ctrl_stop(struct asr_hw *asr_hw, struct asr_txq *txq)
{
	/* restart netdev queue if number of queued buffer is below threshold */
	//dev_err(asr_hw->dev, "-flowctrl %d\n",txq->ndev_idx);
	txq->status &= ~ASR_TXQ_NDEV_FLOW_CTRL;
	netif_wake_subqueue(txq->ndev, txq->ndev_idx);
}

void asr_tx_flow_ctrl_start(struct asr_hw *asr_hw, struct asr_txq *txq)
{
	//dev_err(asr_hw->dev, "+flowctrl %d\n",txq->ndev_idx);
	txq->status |= ASR_TXQ_NDEV_FLOW_CTRL;
	netif_stop_subqueue(txq->ndev, txq->ndev_idx);
}

#ifdef CONFIG_ASR_SDIO

extern int tx_aggr;

#ifdef CONFIG_ASR_KEY_DBG
int no_avail_port_flag = 0;
u32 tx_agg_port_num1 = 0;
u32 tx_agg_port_num2 = 0;
u32 tx_agg_port_num3 = 0;
u32 tx_agg_port_num4 = 0;
u32 tx_agg_port_num5 = 0;
u32 tx_agg_port_num6 = 0;
u32 tx_agg_port_num7 = 0;
u32 tx_agg_port_num8 = 0;
u32 tx_near_full_cnt = 0;
u32 no_avail_port_cnt = 0;
u32 tx_full1_cnt = 0;
u32 tx_full2_cnt = 0;
uint32_t tx_ring_nearly_full_empty_size;
#endif

#define ASR_WAIT_FW_MORE_AGG   0
int asr_check_hw_sdio_ports_ready(int ava_agg_num, uint16_t bitmap, uint8_t tx_idx)
{
	while (ava_agg_num) {
		if (bitmap & (1 << tx_idx)) {
			ava_agg_num--;
			bitmap &= ~(1 << tx_idx);
			tx_idx++;
			if (tx_idx == 16) {
				tx_idx = 1;
				//ava_agg_num--;
			}
		} else {
			return 0;
		}
	}
	return 1;
}

extern bool tx_wait_agger;
int asr_get_cnt_from_tx_bitmap(struct asr_hw *asr_hw)
{
	u16 bitmap = asr_hw->tx_use_bitmap;
	int cnt = 0;
	u8 idx = 1;
	while (idx < 16) {
		if (bitmap & ((1 << idx)))
			cnt++;
		idx++;
	}

	return cnt;
}

extern int tx_debug;
extern int tx_conserve;
/**
 *  @brief This function counts the bits of unsigned int number
 *
 *  @param num  number
 *  @return     number of bits
 */
u32 bitcount(u32 num)
{
	u32 count = 0;
	static u32 nibblebits[] = {
		0, 1, 1, 2, 1, 2, 2, 3,
		1, 2, 2, 3, 2, 3, 3, 4
	};
	for (; num != 0; num >>= 4)
		count += nibblebits[num & 0x0f];
	return count;
}

int tx_task_run_cnt;
extern bool asr_xmit_opt;
u32 asr_data_tx_pkt_cnt;
extern bool g_xmit_first_trigger_flag;
u16 asr_tx_aggr_get_ava_trans_num(struct asr_hw *asr_hw, uint16_t * total_ava_num)
{
	struct asr_tx_agg *tx_agg_env = &asr_hw->tx_agg_env;
	u16 ret = 0;

	spin_lock_bh(&asr_hw->tx_agg_env_lock);
	*total_ava_num = (tx_agg_env->aggr_buf_cnt);

	if (asr_hw->tx_agg_env.last_aggr_buf_next_addr >= asr_hw->tx_agg_env.cur_aggr_buf_next_addr) {
		ret = *total_ava_num;
	} else {
		if (tx_agg_env->aggr_buf_cnt > tx_agg_env->last_aggr_buf_next_idx)
			ret = tx_agg_env->aggr_buf_cnt - tx_agg_env->last_aggr_buf_next_idx;
		else {
			ret = tx_agg_env->aggr_buf_cnt;
		}
	}

	if (tx_agg_env->aggr_buf_cnt == tx_agg_env->last_aggr_buf_next_idx &&
	    asr_hw->tx_agg_env.last_aggr_buf_next_addr < asr_hw->tx_agg_env.cur_aggr_buf_next_addr) {
		tx_agg_env->cur_aggr_buf_next_idx = 0;
		tx_agg_env->cur_aggr_buf_next_addr = (uint8_t *) tx_agg_env->aggr_buf->data;
	}
	spin_unlock_bh(&asr_hw->tx_agg_env_lock);

	return ret;

}
#ifdef CONFIG_ASR_SDIO
bool cfg_vif_disable_tx = false;
bool tra_vif_disable_tx = false;

void tx_agg_port_stats(u8 avail_data_port)
{
    #ifdef CONFIG_ASR_KEY_DBG
	switch (avail_data_port)	// caculate tx aggregation status
	{
	case 1:
		tx_agg_port_num1++;
		break;
	case 2:
		tx_agg_port_num2++;
		break;
	case 3:
		tx_agg_port_num3++;
		break;
	case 4:
		tx_agg_port_num4++;
		break;
	case 5:
		tx_agg_port_num5++;
		break;
	case 6:
		tx_agg_port_num6++;
		break;
	case 7:
		tx_agg_port_num7++;
		break;
	case 8:
		tx_agg_port_num8++;
		break;
	default:
		break;
	}
    #endif
    return;
}

extern bool check_vif_block_flags(struct asr_vif *asr_vif);

bool check_sta_ps_state(struct asr_sta *sta)
{
      if (sta == NULL)
	      return false;
      else
          return (sta->ps.active);
}

u16 asr_tx_get_ava_trans_skbs(struct asr_hw *asr_hw)
{
	struct sk_buff *skb_pending;
	struct sk_buff *pnext;
	struct hostdesc * hostdesc_tmp = NULL;
	struct asr_vif *asr_vif_tmp = NULL;
    struct asr_sta *asr_sta_tmp = NULL;	
    u16 total_ava_skbs = 0;

	// loop list and get avail skbs.
	skb_queue_walk_safe(&asr_hw->tx_sk_list, skb_pending, pnext) {
		hostdesc_tmp = (struct hostdesc *)skb_pending->data;
		if ((hostdesc_tmp->vif_idx < asr_hw->vif_max_num + asr_hw->sta_max_num)) {
			asr_vif_tmp = asr_hw->vif_table[hostdesc_tmp->vif_idx];
			asr_sta_tmp = (hostdesc_tmp->staid < (NX_REMOTE_STA_MAX + NX_VIRT_DEV_MAX)) ? &asr_hw->sta_table[hostdesc_tmp->staid] : NULL;

		    if (asr_vif_tmp && (check_vif_block_flags(asr_vif_tmp) == false)
					        && (check_sta_ps_state(asr_sta_tmp) == false)) {
				total_ava_skbs++;
			}
		} else {
				dev_err(asr_hw->dev," unlikely: mrole tx(vif_idx %d not valid)!!!\r\n",hostdesc_tmp->vif_idx);
		}
	}

	return total_ava_skbs;

}

void asr_tx_get_hif_skb_list(struct asr_hw *asr_hw,u8 port_num)
{
	struct sk_buff *skb_pending;
	struct sk_buff *pnext;
	struct hostdesc * hostdesc_tmp = NULL;
	struct asr_vif *asr_vif_tmp = NULL;
    struct asr_sta *asr_sta_tmp = NULL;	

	skb_queue_walk_safe(&asr_hw->tx_sk_list, skb_pending, pnext) {	
		hostdesc_tmp = (struct hostdesc *)skb_pending->data;
		if ((hostdesc_tmp->vif_idx < asr_hw->vif_max_num + asr_hw->sta_max_num)) {
			asr_vif_tmp = asr_hw->vif_table[hostdesc_tmp->vif_idx];
			asr_sta_tmp = (hostdesc_tmp->staid < (NX_REMOTE_STA_MAX + NX_VIRT_DEV_MAX)) ? &asr_hw->sta_table[hostdesc_tmp->staid] : NULL;			
			if (asr_vif_tmp && (check_vif_block_flags(asr_vif_tmp) == false)
				            && (check_sta_ps_state(asr_sta_tmp) == false)) {
				skb_unlink(skb_pending, &asr_hw->tx_sk_list);
				skb_queue_tail(&asr_hw->tx_hif_skb_list, skb_pending);
				port_num-- ;

				if (port_num == 0)
					break;
			} else {
                             if (hostdesc_tmp->flags & TXU_CNTRL_MGMT) {

				dev_err(asr_hw->dev," bypass mgt mrole tx(vif_idx = %d sta=%d, )(%d %d), vif_tmp=%p ,flags=0x%lx !!!\r\n",hostdesc_tmp->vif_idx,hostdesc_tmp->staid,
                                                                             check_sta_ps_state(asr_sta_tmp), check_vif_block_flags(asr_vif_tmp),
                                                                             asr_vif_tmp,
                                                                             asr_vif_tmp ? asr_vif_tmp->dev_flags: 0x0);

                             }

                        }
		} else {
				dev_err(asr_hw->dev," unlikely: mrole tx(vif_idx %d not valid)!!!\r\n",hostdesc_tmp->vif_idx);
		}

	}

	if (port_num)
		dev_err(asr_hw->dev," unlikely: remain avail port error (%d) !!!\r\n",port_num);

}


u16 asr_hif_bufdata_prepare(struct asr_hw *asr_hw,struct sk_buff *hif_buf_skb,u8 port_num,u32 *tx_bytes)
{
	struct sk_buff *skb_pending;
	struct sk_buff *pnext;
	struct hostdesc * hostdesc_tmp = NULL;
	u16 trans_len = 0;
	u16 temp_len;
	//struct asr_vif *asr_vif_tmp = NULL;
	struct hostdesc *hostdesc_start = NULL;
	u8 hif_skb_copy_cnt = 0;

	asr_data_tx_pkt_cnt += port_num;

	skb_queue_walk_safe(&asr_hw->tx_hif_skb_list, skb_pending, pnext)
	{
	    hostdesc_tmp = (struct hostdesc *)skb_pending->data;

		temp_len = hostdesc_tmp->sdio_tx_len + 2;

		if (temp_len % 32 || temp_len > ASR_SDIO_DATA_MAX_LEN || (hostdesc_tmp->vif_idx >= asr_hw->vif_max_num + asr_hw->sta_max_num)) {
			dev_err(asr_hw->dev,"unlikely: len:%d not 32 aligned, port_num=%d ,vif_idx=%d , drop \r\n",temp_len,port_num,hostdesc_tmp->vif_idx);

			WARN_ON(1);

            // directly free : not rechain when sdio send fail.
			skb_unlink(skb_pending, &asr_hw->tx_hif_skb_list);
			dev_kfree_skb_any(skb_pending);
			break;

		} else {
            // copy to hifbuf.
                    memcpy(hif_buf_skb->data + trans_len, skb_pending->data ,temp_len);
		    trans_len += temp_len;
	            *tx_bytes += hostdesc_tmp->packet_len;				
		    hif_skb_copy_cnt++;

            if (txlogen)
		        dev_err(asr_hw->dev,"hif_copy: %d %d ,%d %d \r\n",temp_len,trans_len,hif_skb_copy_cnt,port_num);

		}
	}

        if (port_num != hif_skb_copy_cnt) {
		dev_err(asr_hw->dev," unlikely: port num mismatch(%d %d)!!!\r\n",port_num,hif_skb_copy_cnt);
	        trans_len = 0;
		*tx_bytes = 0;
        } else {
		hostdesc_start = (struct hostdesc *)(hif_buf_skb->data);
		hostdesc_start->agg_num = hif_skb_copy_cnt;
	}

	return trans_len;

}

void asr_drop_tx_vif_skb(struct asr_hw *asr_hw,struct asr_vif *asr_vif_drop)
{
	struct sk_buff *skb_pending;
	struct sk_buff *pnext;
	struct hostdesc * hostdesc_tmp = NULL;
	//struct asr_vif *asr_vif_tmp = NULL;

	skb_queue_walk_safe(&asr_hw->tx_sk_list, skb_pending, pnext) {
		hostdesc_tmp = (struct hostdesc *)skb_pending->data;		
		if ((asr_vif_drop == NULL) ||
			(asr_vif_drop && (hostdesc_tmp->vif_idx == asr_vif_drop->vif_index))) {
			skb_unlink(skb_pending, &asr_hw->tx_sk_list);
			dev_kfree_skb_any(skb_pending);
		}
	}
}

void asr_tx_skb_sta_deinit(struct asr_hw *asr_hw,struct asr_sta *asr_sta_drop)
{
	struct sk_buff *skb_pending;
	struct sk_buff *pnext;
	struct hostdesc * hostdesc_tmp = NULL;

	skb_queue_walk_safe(&asr_hw->tx_sk_list, skb_pending, pnext) {
		hostdesc_tmp = (struct hostdesc *)skb_pending->data;		
		if (asr_sta_drop && hostdesc_tmp && (hostdesc_tmp->staid == asr_sta_drop->sta_idx)) {
			skb_unlink(skb_pending, &asr_hw->tx_sk_list);
			dev_kfree_skb_any(skb_pending);
		}
	}
}

int asr_opt_tx_task(struct asr_hw *asr_hw)
{
	u8 avail_data_port;
	int ret;
	u8 port_num = 0;
	u16 ava_skb_num = 0;
	u16 trans_len = 0;
	struct hostdesc *hostdesc_start = NULL;
	struct hostdesc *hostdesc_tmp = NULL;
	int total_tx_num = 0;

	struct sk_buff *hif_buf_skb = NULL;
	struct sk_buff *hif_tx_skb = NULL;
	struct sk_buff *pnext = NULL;

	unsigned int io_addr;
	u32 tx_bytes = 0;
	struct asr_vif *asr_traffic_vif = NULL;
	struct asr_vif *asr_ext_vif = NULL;
	struct asr_vif *asr_vif_tmp = NULL;

	u16 bitmap_record;

	if (asr_hw == NULL)
	   return 0;

	tx_task_run_cnt++;

	while (1) {
		/************************** phy status check. **************************************************************/
		if (mrole_enable && (asr_hw->ext_vif_index < asr_hw->vif_max_num + asr_hw->sta_max_num)) {
			asr_ext_vif = asr_hw->vif_table[asr_hw->ext_vif_index];
		}
		
		// check traffic vif status.
		if (asr_hw->vif_index < asr_hw->vif_max_num + asr_hw->sta_max_num) {
			asr_traffic_vif  = asr_hw->vif_table[asr_hw->vif_index];
		}

		// wifi phy restarting, reset all tx agg buf.
		if (test_bit(ASR_DEV_RESTARTING, &asr_hw->phy_flags)) {
			//struct sk_buff *skb_pending;
			//struct sk_buff *pnext;

			dev_info(asr_hw->dev, "%s:phy drop,phy_flags(%08X),cnt(%u)\n", __func__, (unsigned int)asr_hw->phy_flags, skb_queue_len(&asr_hw->tx_sk_list));

			// todo. drop all skbs in asr_hw->tx_sk_list.
			spin_lock_bh(&asr_hw->tx_lock);
			asr_drop_tx_vif_skb(asr_hw,NULL);
			list_for_each_entry(asr_vif_tmp,&asr_hw->vifs,list){
				asr_vif_tmp->tx_skb_cnt = 0;
			}
			spin_unlock_bh(&asr_hw->tx_lock);
			break;
		}

		if (
		             asr_traffic_vif && ASR_VIF_TYPE(asr_traffic_vif) == NL80211_IFTYPE_STATION &&
			     (test_bit(ASR_DEV_STA_DISCONNECTING, &asr_traffic_vif->dev_flags) ||
			     ((p2p_debug == 0) && !test_bit(ASR_DEV_STA_CONNECTED, &asr_traffic_vif->dev_flags))  ||         // P2P mask:will send mgt frame from host before connected.
			     test_bit(ASR_DEV_CLOSEING, &asr_traffic_vif->dev_flags) ||
			     test_bit(ASR_DEV_PRECLOSEING, &asr_traffic_vif->dev_flags))) {

			        dev_info(asr_hw->dev, "%s:sta drop,dev_flags(%08X),vif_started_cnt=%d\n",
					 __func__, (unsigned int)asr_traffic_vif->dev_flags,asr_hw->vif_started);

				spin_lock_bh(&asr_hw->tx_lock);
				asr_drop_tx_vif_skb(asr_hw,asr_traffic_vif);
				asr_traffic_vif->tx_skb_cnt = 0;
			        spin_unlock_bh(&asr_hw->tx_lock);

			        if (asr_hw->vif_started <= 1)
			            break;
		}

                // check hif buf valid.
                if (skb_queue_empty(&asr_hw->tx_hif_free_buf_list)) {
			dev_info(asr_hw->dev, "%s: tx_hif_skb_list is empty ...\n", __func__);
			break;
                }

		/************************ tx cmd53 prepare and send.****************************************************************/
		spin_lock_bh(&asr_hw->tx_lock); 		  // use lock to prevent asr_vif->dev_flags change.
		
		ava_skb_num = asr_tx_get_ava_trans_skbs(asr_hw);
		if (!ava_skb_num) {			
		    spin_unlock_bh(&asr_hw->tx_lock);
		    break;
		}
		
		avail_data_port = asr_sdio_tx_get_available_data_port(asr_hw, ava_skb_num, &port_num, &io_addr, &bitmap_record);
		
		tx_agg_port_stats(avail_data_port);
		
		if (!avail_data_port) {
		    spin_unlock_bh(&asr_hw->tx_lock);
		    break;
		}

		asr_tx_get_hif_skb_list(asr_hw,port_num);

		spin_unlock_bh(&asr_hw->tx_lock);

          	// pop free hif buf and move tx skb to hif buf.
                hif_buf_skb = skb_dequeue(&asr_hw->tx_hif_free_buf_list);

		if (hif_buf_skb == NULL) {
              // free asr_hw->tx_hif_skb_list and break
			  dev_info(asr_hw->dev, "%s: unlikely , tx_hif_skb_list is empty ...\n", __func__);

			  skb_queue_walk_safe(&asr_hw->tx_hif_skb_list, hif_tx_skb, pnext) {
				  skb_unlink(hif_tx_skb, &asr_hw->tx_hif_skb_list);
				  dev_kfree_skb_any(hif_tx_skb);
			  }
			  break;
		}

                // to adapt to sdio aggr tx, better use scatter dma later.
		trans_len = asr_hif_bufdata_prepare(asr_hw,hif_buf_skb,port_num,&tx_bytes);

        if (trans_len) {
			hostdesc_start = (struct hostdesc *)hif_buf_skb->data;
			// send sdio .
			ret = asr_sdio_send_data(asr_hw, HIF_TX_DATA, (u8 *) hostdesc_start, trans_len, io_addr, bitmap_record);	// sleep api, not add in spin lock
			if (ret) {
				if (mrole_enable == false && asr_traffic_vif != NULL) {
					asr_traffic_vif->net_stats.tx_dropped += hostdesc_start->agg_num;
				}
				dev_info(asr_hw->dev, "%s: unlikely , sdio send fail ,rechain hif_tx_skb...\n", __func__);
			} else {
                if (txlogen)
	                dev_info(asr_hw->dev, "%s: sdio send %d ok(%d %d %d) ,vif_idx=%d, (%d %d %d) 0x%x...\n", __func__,
                                                                          trans_len,
                                                                          ava_skb_num,avail_data_port,port_num,
                                                                          hostdesc_start->vif_idx,
                                                                          hostdesc_start->sdio_tx_len,hostdesc_start->sdio_tx_total_len,hostdesc_start->agg_num,
                                                                          hostdesc_start->ethertype);
            }
        } else {
			ret = -EBUSY;
			dev_info(asr_hw->dev, "%s: unlikely , sdio send ebusy ...\n", __func__);

		}

		// tx_hif_skb_list free, if sdio send fail, need rechain to tx_sk_list
		spin_lock_bh(&asr_hw->tx_lock);
		skb_queue_walk_safe(&asr_hw->tx_hif_skb_list, hif_tx_skb, pnext) {
			skb_unlink(hif_tx_skb, &asr_hw->tx_hif_skb_list);
			if (ret) {
				skb_queue_tail(&asr_hw->tx_sk_list, hif_tx_skb);          // sdio send fail
			} else {
                                // flow ctrl update.
				hostdesc_tmp = (struct hostdesc *)hif_tx_skb->data;
				if ((hostdesc_tmp->vif_idx < asr_hw->vif_max_num + asr_hw->sta_max_num)) {
					asr_vif_tmp = asr_hw->vif_table[hostdesc_tmp->vif_idx];

					if (asr_vif_tmp)
					    asr_vif_tmp->tx_skb_cnt --;
				}
			        dev_kfree_skb_any(hif_tx_skb);
			}
		}
		spin_unlock_bh(&asr_hw->tx_lock);

	        /************************ tx stats and flow ctrl.****************************************************************/
		if (mrole_enable == false && asr_traffic_vif != NULL)
		{
			asr_traffic_vif->net_stats.tx_packets += hostdesc_start->agg_num;
			asr_traffic_vif->net_stats.tx_bytes += tx_bytes;
		}

		asr_hw->stats.last_tx = jiffies;
#ifdef ASR_STATS_RATES_TIMER
		asr_hw->stats.tx_bytes += tx_bytes;
#endif

	    // clear hif buf and rechain to free hif_buf list.
		memset(hif_buf_skb->data, 0, IPC_HIF_TXBUF_SIZE);
		// Add the sk buffer structure in the table of rx buffer
		skb_queue_tail(&asr_hw->tx_hif_free_buf_list, hif_buf_skb);

		// tx flow ctrl part.
		list_for_each_entry(asr_vif_tmp, &asr_hw->vifs, list) {
		    asr_tx_flow_ctrl(asr_hw,asr_vif_tmp,false);
		}

		if (skb_queue_len(&asr_hw->tx_sk_list) == 0)
		    g_xmit_first_trigger_flag = false;

		total_tx_num += hostdesc_start->agg_num;

	}

	return total_tx_num;
}

int asr_tx_task(struct asr_hw *asr_hw)
{
	u8 avail_data_port;
	int ret;
	struct asr_tx_agg *tx_agg_env = NULL;
	u8 port_num = 0;
	u16 ava_agg_num = 0;
	u16 trans_len = 0;
	u16 temp_len;
	struct hostdesc *hostdesc_start = NULL;
	u8 *temp_cur_next_addr;
	u16 temp_cur_next_idx;
	u16 total_ava_num;
	int total_tx_num = 0;
	unsigned int io_addr;
	u32 tx_bytes = 0;
	//uint8_t tx_cur_idx;
	struct hostdesc *hostdesc_temp = NULL;
	struct asr_vif *asr_vif_tmp = NULL;
	struct asr_vif *asr_traffic_vif = NULL;
	struct asr_vif *asr_ext_vif = NULL;

	//static int no_ready_times = 0;
	u16 bitmap_record;

    if (asr_hw == NULL)
       return 0;

    tx_agg_env = &(asr_hw->tx_agg_env);


	tx_task_run_cnt++;

	while (1) {

		/************************** dev & vif stats check. **************************************************************/
		if (mrole_enable && (asr_hw->ext_vif_index < asr_hw->vif_max_num + asr_hw->sta_max_num)) {
			asr_ext_vif = asr_hw->vif_table[asr_hw->ext_vif_index];
		}

        // check traffic vif status.
		if (asr_hw->vif_index < asr_hw->vif_max_num + asr_hw->sta_max_num) {
		    asr_traffic_vif  = asr_hw->vif_table[asr_hw->vif_index];
		}

        // wifi phy restarting, reset all tx agg buf.
		if (test_bit(ASR_DEV_RESTARTING, &asr_hw->phy_flags)) {
			dev_info(asr_hw->dev, "%s:phy drop,phy_flags(%08X),cnt(%u)\n",
				 __func__, (unsigned int)asr_hw->phy_flags, asr_hw->tx_agg_env.aggr_buf_cnt);

			spin_lock_bh(&asr_hw->tx_agg_env_lock);
			asr_tx_agg_buf_reset(asr_hw);
			list_for_each_entry(asr_vif_tmp,&asr_hw->vifs,list){
				asr_vif_tmp->txring_bytes = 0;
			}
			spin_unlock_bh(&asr_hw->tx_agg_env_lock);

			break;
		}

        // FIXME. p2p case will send mgt frame from host before connect.
		if ((p2p_debug == 0) &&
		         asr_traffic_vif && ASR_VIF_TYPE(asr_traffic_vif) == NL80211_IFTYPE_STATION &&
			     (test_bit(ASR_DEV_STA_DISCONNECTING, &asr_traffic_vif->dev_flags) ||
			     !test_bit(ASR_DEV_STA_CONNECTED, &asr_traffic_vif->dev_flags)  ||
			      test_bit(ASR_DEV_CLOSEING, &asr_traffic_vif->dev_flags) ||
			      test_bit(ASR_DEV_PRECLOSEING, &asr_traffic_vif->dev_flags))) {

			dev_info(asr_hw->dev, "%s:sta drop,dev_flags(%08X),cnt(%u),vif_started_cnt=%d\n",
				 __func__, (unsigned int)asr_traffic_vif->dev_flags,asr_hw->tx_agg_env.aggr_buf_cnt,asr_hw->vif_started);

			spin_lock_bh(&asr_hw->tx_agg_env_lock);
			if (asr_hw->vif_started > 1) {
				asr_tx_agg_buf_mask_vif(asr_hw,asr_traffic_vif);
			} else {
				asr_tx_agg_buf_reset(asr_hw);
				list_for_each_entry(asr_vif_tmp,&asr_hw->vifs,list){
					asr_vif_tmp->txring_bytes = 0;
				}
			}

			spin_unlock_bh(&asr_hw->tx_agg_env_lock);

			if (asr_hw->vif_started <= 1)
			    break;
		}

#ifdef CONFIG_ASR595X
        // add stop tx flag check, just stop send data from drv to fw.
		if (asr_traffic_vif && ASR_VIF_TYPE(asr_traffic_vif) == NL80211_IFTYPE_STATION
			&& (test_bit(ASR_DEV_STA_OUT_TWTSP, &asr_traffic_vif->dev_flags) ||
			    test_bit(ASR_DEV_TXQ_STOP_CSA, &asr_traffic_vif->dev_flags) ||
			    test_bit(ASR_DEV_TXQ_STOP_VIF_PS, &asr_traffic_vif->dev_flags) ||
			    test_bit(ASR_DEV_TXQ_STOP_CHAN, &asr_traffic_vif->dev_flags))) {

			dev_err(asr_hw->dev," stop tx, dev_flags=0x%x!!!\r\n",(unsigned int)(asr_traffic_vif->dev_flags));

			break;
		}
#endif

        /************************ tx cmd53 prepare and send.****************************************************************/
		ava_agg_num = asr_tx_aggr_get_ava_trans_num(asr_hw, &total_ava_num);
		if (!ava_agg_num) {
			break;
		}

		avail_data_port =
		    asr_sdio_tx_get_available_data_port(asr_hw, ava_agg_num, &port_num, &io_addr, &bitmap_record);

		tx_agg_port_stats(avail_data_port);

		if (!avail_data_port) {
			break;
		}

		temp_cur_next_addr = tx_agg_env->cur_aggr_buf_next_addr;
		temp_cur_next_idx = tx_agg_env->cur_aggr_buf_next_idx;

		hostdesc_start = (struct hostdesc *)(temp_cur_next_addr);
		hostdesc_start->agg_num = port_num;

		asr_data_tx_pkt_cnt += port_num;
		while (port_num--) {
			tx_bytes += ((struct hostdesc *)(temp_cur_next_addr))->packet_len;

			temp_len = *((u16 *) temp_cur_next_addr) + 2;
			if (temp_len % 32 || temp_len > ASR_SDIO_DATA_MAX_LEN) {
				dev_err(asr_hw->dev,
					"unlikely: temp_cur_next_addr:0x%x len:%d not 32 aligned\r\n",
					(u32) (uintptr_t) temp_cur_next_addr, temp_len);

				dev_err(asr_hw->dev,
					"[%s %d] wr:0x%x(%d) rd:0x%x(%d) aggr_buf(%p) cnt:%d - agg_ava_num:%d (%d,%d)\n",
					__func__, __LINE__, (u32) (uintptr_t)
					tx_agg_env->last_aggr_buf_next_addr,
					tx_agg_env->last_aggr_buf_next_idx, (u32) (uintptr_t)
					tx_agg_env->cur_aggr_buf_next_addr,
					tx_agg_env->cur_aggr_buf_next_idx,
					asr_hw->tx_agg_env.aggr_buf->data,
					tx_agg_env->aggr_buf_cnt, ava_agg_num, hostdesc_start->agg_num, port_num);

				WARN_ON(1);

				spin_lock_bh(&asr_hw->tx_agg_env_lock);
				asr_tx_agg_buf_reset(asr_hw);
				list_for_each_entry(asr_vif_tmp,&asr_hw->vifs,list){
					asr_vif_tmp->txring_bytes = 0;
				}
				spin_unlock_bh(&asr_hw->tx_agg_env_lock);

				return total_tx_num;
			} else {
                // multi vif tx ringbuf byte caculate for flow ctrl.
                hostdesc_temp = (struct hostdesc *)(temp_cur_next_addr);

				if ((hostdesc_temp->vif_idx < asr_hw->vif_max_num + asr_hw->sta_max_num)) {
					asr_vif_tmp = asr_hw->vif_table[hostdesc_temp->vif_idx];

					spin_lock_bh(&asr_hw->tx_agg_env_lock);
					if (asr_vif_tmp && (asr_vif_tmp->txring_bytes >= temp_len)) {
					    asr_vif_tmp->txring_bytes -= temp_len;
					    //dev_err(asr_hw->dev," mrole tx(vif_type %d): frm_len = %d \r\n",
						//                        ASR_VIF_TYPE(asr_vif_tmp),hostdesc_temp->packet_len);						
					} else {
                        if (asr_vif_tmp)
					        dev_err(asr_hw->dev," unlikely: mrole tx(vif_type %d): %d < %d !!!\r\n",
						                        ASR_VIF_TYPE(asr_vif_tmp),asr_vif_tmp->txring_bytes, temp_len);						
					}
					spin_unlock_bh(&asr_hw->tx_agg_env_lock);
				} else {
			            dev_err(asr_hw->dev," unlikely: mrole tx(vif_idx %d not valid)!!!\r\n",hostdesc_temp->vif_idx);
				}
			}
			trans_len += temp_len;
			temp_cur_next_addr += temp_len;
			temp_cur_next_idx++;
		}

		ret = asr_sdio_send_data(asr_hw, HIF_TX_DATA, (u8 *) hostdesc_start, trans_len, io_addr, bitmap_record);	// sleep api, not add in spin lock
		if (ret) {
			dev_err(asr_hw->dev,
				"asr_tx_push fail  with len:%d and src 0x%x ,ret=%d aggr_num=%d  ethertype=0x%x, bm=0x%x \n",
				trans_len, (u32) (uintptr_t) hostdesc_start, ret, hostdesc_start->agg_num, hostdesc_start->ethertype,
				bitmap_record);
			dev_err(asr_hw->dev, "[%s] wr:0x%x(%d) rd:0x%x(%d) cnt:%d \n", __func__, (u32) (uintptr_t)
				tx_agg_env->last_aggr_buf_next_addr,
				tx_agg_env->last_aggr_buf_next_idx, (u32) (uintptr_t)
				tx_agg_env->cur_aggr_buf_next_addr,
				tx_agg_env->cur_aggr_buf_next_idx, tx_agg_env->aggr_buf_cnt);

			if (mrole_enable == false && asr_traffic_vif != NULL) {
				asr_traffic_vif->net_stats.tx_dropped += hostdesc_start->agg_num;
			}
			break;
		}
		trans_len = 0;

        /************************ tx stats and flow ctrl.****************************************************************/

		spin_lock_bh(&asr_hw->tx_agg_env_lock);	//may only need to protect data from here

        if (mrole_enable == false && asr_traffic_vif != NULL)
		{
		    asr_traffic_vif->net_stats.tx_packets += hostdesc_start->agg_num;
		    asr_traffic_vif->net_stats.tx_bytes += tx_bytes;
		}

		asr_hw->stats.last_tx = jiffies;
#ifdef ASR_STATS_RATES_TIMER
		asr_hw->stats.tx_bytes += tx_bytes;
#endif

        // tx flow ctrl part.

		list_for_each_entry(asr_vif_tmp, &asr_hw->vifs, list) {
			asr_tx_flow_ctrl(asr_hw,asr_vif_tmp,false);
		}

		if (tx_agg_env->aggr_buf_cnt < hostdesc_start->agg_num) {

			dev_err(asr_hw->dev, "[%s] unlikely : wr:0x%x(%d) rd:0x%x(%d) cnt:%d agg_num:%d agg_ava_num:%d\n",
				__func__, (u32) (uintptr_t)
				tx_agg_env->last_aggr_buf_next_addr, tx_agg_env->last_aggr_buf_next_idx,
				(u32) (uintptr_t)
				tx_agg_env->cur_aggr_buf_next_addr, tx_agg_env->cur_aggr_buf_next_idx,
				tx_agg_env->aggr_buf_cnt, hostdesc_start->agg_num, ava_agg_num);

			asr_tx_agg_buf_reset(asr_hw);
			list_for_each_entry(asr_vif_tmp,&asr_hw->vifs,list){
				asr_vif_tmp->txring_bytes = 0;
			}
			spin_unlock_bh(&asr_hw->tx_agg_env_lock);
			return total_tx_num;
		} else
			tx_agg_env->aggr_buf_cnt -= hostdesc_start->agg_num;

		if (tx_agg_env->aggr_buf_cnt == 0)
			g_xmit_first_trigger_flag = false;

		total_tx_num += hostdesc_start->agg_num;
		tx_agg_env->cur_aggr_buf_next_addr = temp_cur_next_addr;
		tx_agg_env->cur_aggr_buf_next_idx = temp_cur_next_idx;

		spin_unlock_bh(&asr_hw->tx_agg_env_lock);

#if 0
		if ((bitcount(asr_hw->tx_use_bitmap) - 1 >= (tx_aggr / 4))
		    && ((ava_agg_num - hostdesc_start->agg_num) >= (tx_aggr / 4)))
			continue;
		else
			break;
#endif

	}

	return total_tx_num;
}
#endif

void asr_tx_agg_buf_reset(struct asr_hw *asr_hw)
{
    if (NULL == asr_hw)
		return;

	//clear the tx buffer, the buffered data will be dropped. there will no send data in tx direction
	asr_hw->tx_agg_env.last_aggr_buf_next_addr = (uint8_t *) asr_hw->tx_agg_env.aggr_buf->data;
	asr_hw->tx_agg_env.last_aggr_buf_next_idx = 0;
	asr_hw->tx_agg_env.cur_aggr_buf_next_addr = (uint8_t *) asr_hw->tx_agg_env.aggr_buf->data;
	asr_hw->tx_agg_env.cur_aggr_buf_next_idx = 0;
	asr_hw->tx_agg_env.aggr_buf_cnt = 0;

}

void asr_tx_agg_buf_mask_vif(struct asr_hw *asr_hw,struct asr_vif *asr_vif)
{
     // set tx pkt unvalid per vif and let fw hif to drop.
	 struct hostdesc *hostdesc_temp;
	 struct asr_tx_agg *tx_agg_env = &(asr_hw->tx_agg_env);
	 u16 aggr_buf_cnt = tx_agg_env->aggr_buf_cnt;
	 u8 *temp_cur_next_addr,*tx_agg_buf_wr_ptr;
	 u16 tx_agg_buf_wr_idx;	
	 u16 temp_len;
	 u16 vif_drop_cnt = 0;

	 temp_cur_next_addr = tx_agg_env->cur_aggr_buf_next_addr;

	 tx_agg_buf_wr_ptr  = tx_agg_env->last_aggr_buf_next_addr;
	 tx_agg_buf_wr_idx  = tx_agg_env->last_aggr_buf_next_idx;

	 while (aggr_buf_cnt)
	 {
	 	 temp_len = *((u16 *) temp_cur_next_addr) + 2;
		 hostdesc_temp = (struct hostdesc *)(temp_cur_next_addr);

		 if ((hostdesc_temp->vif_idx == asr_vif->vif_index)) {
              // set pkt unvalid.
              hostdesc_temp->flags |= TXU_CNTRL_DROP;
			  vif_drop_cnt++;
		 }

		 temp_cur_next_addr += temp_len;

		 aggr_buf_cnt--;

	    // handle cur next addr close to txbuf end.
		if ((aggr_buf_cnt == tx_agg_buf_wr_idx) &&
		    (tx_agg_buf_wr_ptr < temp_cur_next_addr)) {
			temp_cur_next_addr = (uint8_t *) tx_agg_env->aggr_buf->data;
	    }
	 }

	 dev_err(asr_hw->dev, "%s (%d,%d) vif frame drop cnt %d...\n", __func__,asr_vif->vif_index, ASR_VIF_TYPE(asr_vif),vif_drop_cnt);

}


//in queue
u8 *asr_tx_aggr_get_copy_addr(struct asr_hw * asr_hw, u32 len)
{
	u8 *addr;

	//as the tx buffer way have changed, the tx buffer may exist more little buf segment, so more tx msg, to avoid too much tx msg, limit it here
	//when there are more than 20 buffer is not processed in tx direction, pause send more tx msg to queue.
	//if (asr_hw->tx_agg_env.aggr_buf_cnt > TX_AGG_BUF_UNIT_CNT){
	//    dev_err(asr_hw->dev, "%s:aggr buf cnt=%d\n",__func__,asr_hw->tx_agg_env.aggr_buf_cnt);
	//    return 0;
	//   }
	if (asr_hw->tx_agg_env.last_aggr_buf_next_addr >= asr_hw->tx_agg_env.cur_aggr_buf_next_addr) {
		if (asr_hw->tx_agg_env.last_aggr_buf_next_addr + len <
		    (uint8_t *) asr_hw->tx_agg_env.aggr_buf->data + TX_AGG_BUF_SIZE) {
			addr = asr_hw->tx_agg_env.last_aggr_buf_next_addr;
			asr_hw->tx_agg_env.last_aggr_buf_next_addr += len;
			asr_hw->tx_agg_env.last_aggr_buf_next_idx++;
			asr_hw->tx_agg_env.aggr_buf_cnt++;
		} else {
			if ((uint8_t *) asr_hw->tx_agg_env.aggr_buf->data +
			    len < asr_hw->tx_agg_env.cur_aggr_buf_next_addr) {
				addr = (uint8_t *) asr_hw->tx_agg_env.aggr_buf->data;
				asr_hw->tx_agg_env.last_aggr_buf_next_addr = addr + len;
				asr_hw->tx_agg_env.last_aggr_buf_next_idx = 1;
				asr_hw->tx_agg_env.aggr_buf_cnt++;
			} else {
#ifdef CONFIG_ASR_KEY_DBG
				tx_full1_cnt++;
#endif
				return 0;
			}
		}
	} else {
		if (asr_hw->tx_agg_env.last_aggr_buf_next_addr + len < asr_hw->tx_agg_env.cur_aggr_buf_next_addr) {
			addr = asr_hw->tx_agg_env.last_aggr_buf_next_addr;
			asr_hw->tx_agg_env.last_aggr_buf_next_addr += len;
			asr_hw->tx_agg_env.last_aggr_buf_next_idx++;
			asr_hw->tx_agg_env.aggr_buf_cnt++;
		} else {
#ifdef CONFIG_ASR_KEY_DBG
			tx_full2_cnt++;
#endif
			return 0;
		}
	}
	return addr;
}
#ifdef CONFIG_ASR_SDIO
bool asr_tx_ring_is_nearly_full(struct asr_hw * asr_hw)
{
	s32 empty_size_byte = 0;

	if (asr_hw->tx_agg_env.cur_aggr_buf_next_addr == asr_hw->tx_agg_env.last_aggr_buf_next_addr) {
		return false;
	}

	empty_size_byte =
	    (s32) (asr_hw->tx_agg_env.cur_aggr_buf_next_addr - asr_hw->tx_agg_env.last_aggr_buf_next_addr);

	empty_size_byte = (empty_size_byte + TX_AGG_BUF_SIZE) % TX_AGG_BUF_SIZE;

	//dev_info(asr_hw->dev, "%s:empty_size_byte11=%d,empty_size_byte=%d,cur=%p,last=%p.\n"
	//      , __func__, empty_size_byte11, empty_size_byte, asr_hw->tx_agg_env.cur_aggr_buf_next_addr, asr_hw->tx_agg_env.last_aggr_buf_next_addr);

	if (empty_size_byte < (TX_AGG_BUF_UNIT_CNT / 8 * TX_AGG_BUF_UNIT_SIZE)) {
		return true;
	}

	return false;
}

bool asr_tx_ring_is_ready_push(struct asr_hw * asr_hw)
{
	s32 empty_size_byte = 0;

	if (asr_hw->tx_agg_env.cur_aggr_buf_next_addr == asr_hw->tx_agg_env.last_aggr_buf_next_addr) {
		return true;
	}
#ifndef CONFIG_64BIT
	empty_size_byte =
	    (s32) ((u32) (asr_hw->tx_agg_env.cur_aggr_buf_next_addr) -
		   (u32) (asr_hw->tx_agg_env.last_aggr_buf_next_addr));
#else
	empty_size_byte =
	    (s64) ((u64) (asr_hw->tx_agg_env.cur_aggr_buf_next_addr) -
		   (u64) (asr_hw->tx_agg_env.last_aggr_buf_next_addr));
#endif

	//dev_info(asr_hw->dev, "%s:empty_size_byte=%d.\n", __func__, empty_size_byte);
	empty_size_byte = (empty_size_byte + TX_AGG_BUF_SIZE) % TX_AGG_BUF_SIZE;

	if (empty_size_byte > (TX_AGG_BUF_UNIT_CNT * 4 / 5 * TX_AGG_BUF_UNIT_SIZE)) {
		return true;
	}

	return false;
}

void asr_tx_opt_flow_ctrl(struct asr_hw * asr_hw,struct asr_vif *asr_vif,bool check_high_wm)
{
    if ((asr_hw == NULL) || (asr_vif == NULL)) {
		return;
	}

	if (check_high_wm) {
		// before vif xmit, check vif
		if (mrole_enable) {
			if (memcmp(asr_vif->wdev.netdev->name,"asrcfgwlan",10) == 0) {
				if ((asr_vif->tx_skb_cnt > 36)       // 180 *0.25 * 0.8
					&& !netif_queue_stopped(asr_vif->ndev)) {
						//dev_err(asr_hw->dev, "%s:tx buf full(%d),stop all cfg vif queue.\n", __func__,asr_vif->tx_skb_cnt);
						netif_tx_stop_all_queues(asr_vif->ndev);
						cfg_vif_disable_tx = true;
				}
			} else {
				if ((asr_vif->tx_skb_cnt > 108)     // 180 *0.75 * 0.8
					&& !netif_queue_stopped(asr_vif->ndev)) {
						//dev_err(asr_hw->dev, "%s:tx buf full(%d),stop all traffic vif queue.\n", __func__,asr_vif->tx_skb_cnt);
						netif_tx_stop_all_queues(asr_vif->ndev);
						tra_vif_disable_tx = true;
				}
			}
		} else {
			if ((asr_vif->tx_skb_cnt > 144)        // 180 * 0.8
				&& !netif_queue_stopped(asr_vif->ndev)) {
				//dev_info(asr_hw->dev, "%s:tx buf full,stop all unique vif queue.\n", __func__);
				netif_tx_stop_all_queues(asr_vif->ndev);
			}
		}
	}else {
		if (mrole_enable) {
			if (memcmp(asr_vif->wdev.netdev->name,"asrcfgwlan",10) == 0) {
				//	cfg vif flow ctrl.
				if ((asr_vif->tx_skb_cnt < 9)      // 180 *0.25 * 0.2
					&& netif_queue_stopped(asr_vif->ndev)) {
						//dev_err(asr_hw->dev, "%s:tx buf ready,wake all cfg vif.\n", __func__);
						netif_tx_wake_all_queues(asr_vif->ndev);
						cfg_vif_disable_tx = false;
				}
			} else {
				//	traffic vif flow ctrl.
				if ((asr_vif->tx_skb_cnt < 27)    // 180 *0.75 * 0.2
					&& netif_queue_stopped(asr_vif->ndev)){
						//dev_err(asr_hw->dev, "%s:tx buf ready,wake all cfg vif.\n", __func__);
						netif_tx_wake_all_queues(asr_vif->ndev);
						tra_vif_disable_tx = false;
				}
			}
		} else {
			if ((asr_vif->tx_skb_cnt < 36)       // 180  * 0.2
				&& netif_queue_stopped(asr_vif->ndev)) {
				//dev_info(asr_hw->dev, "%s:tx buf ready,wake all queue.\n", __func__);
				netif_tx_wake_all_queues(asr_vif->ndev);
			}
		}
	}
}


void asr_tx_agg_flow_ctrl(struct asr_hw * asr_hw,struct asr_vif *asr_vif,bool check_high_wm)
{
    if ((asr_hw == NULL) || (asr_vif == NULL)) {
		return;
	}

    if (check_high_wm) {
		// before vif xmit, check vif
		if (mrole_enable) {
			if (memcmp(asr_vif->wdev.netdev->name,"asrcfgwlan",10) == 0) {
				if ((asr_vif->txring_bytes > CFG_VIF_HWM)
					&& !netif_queue_stopped(asr_vif->ndev)) {
						//dev_err(asr_hw->dev, "%s:tx buf full(%d),stop all cfg vif queue.\n", __func__,asr_vif->txring_bytes);
						netif_tx_stop_all_queues(asr_vif->ndev);
						cfg_vif_disable_tx = true;
				}
			} else {
				if ((asr_vif->txring_bytes > TRA_VIF_HWM)
					&& !netif_queue_stopped(asr_vif->ndev)) {
						//dev_err(asr_hw->dev, "%s:tx buf full(%d),stop all traffic vif queue.\n", __func__,asr_vif->txring_bytes);
						netif_tx_stop_all_queues(asr_vif->ndev);
						tra_vif_disable_tx = true;
				}
			}
		} else {
			if ((asr_vif->txring_bytes > UNIQUE_VIF_HWM)
				&& !netif_queue_stopped(asr_vif->ndev)) {
				//dev_info(asr_hw->dev, "%s:tx buf full,stop all unique vif queue.\n", __func__);
				netif_tx_stop_all_queues(asr_vif->ndev);
			}
		}
    }else {
		if (mrole_enable) {
			if (memcmp(asr_vif->wdev.netdev->name,"asrcfgwlan",10) == 0) {
				//  cfg vif flow ctrl.
				if ((asr_vif->txring_bytes < CFG_VIF_LWM)
					&& netif_queue_stopped(asr_vif->ndev)) {
						//dev_err(asr_hw->dev, "%s:tx buf ready,wake all cfg vif.\n", __func__);
						netif_tx_wake_all_queues(asr_vif->ndev);
						cfg_vif_disable_tx = false;
				}
			} else {
				//  traffic vif flow ctrl.
				if ((asr_vif->txring_bytes < TRA_VIF_LWM)
					&& netif_queue_stopped(asr_vif->ndev)){
						//dev_err(asr_hw->dev, "%s:tx buf ready,wake all cfg vif.\n", __func__);
						netif_tx_wake_all_queues(asr_vif->ndev);
						tra_vif_disable_tx = false;
				}
			}
		} else {
			if ((asr_vif->txring_bytes < UNIQUE_VIF_LWM)
				&& netif_queue_stopped(asr_vif->ndev)) {
				//dev_info(asr_hw->dev, "%s:tx buf ready,wake all queue.\n", __func__);
				netif_tx_wake_all_queues(asr_vif->ndev);
			}
		}
	}
}

void asr_tx_flow_ctrl(struct asr_hw * asr_hw,struct asr_vif *asr_vif,bool check_high_wm)
{
      if (asr_xmit_opt)
		  asr_tx_opt_flow_ctrl(asr_hw,asr_vif,check_high_wm);
	  else
		  asr_tx_agg_flow_ctrl(asr_hw,asr_vif,check_high_wm);
}

#endif
bool asr_main_process_running(struct asr_hw * asr_hw)
{
	bool ret = false;

	spin_lock_bh(&asr_hw->pmain_proc_lock);
	/* Check if already processing */
	if (asr_hw->mlan_processing) {
		asr_hw->more_task_flag = true;
		ret = true;
	}
	spin_unlock_bh(&asr_hw->pmain_proc_lock);

	return ret;
}

long tx_evt_xmit_times;
long full_to_trigger;
extern bool asr_xmit_opt;
extern int tx_aggr_xmit_thres;
extern int tx_hr_timer;
bool g_xmit_first_trigger_flag = false;


#ifdef ASR_REDUCE_TCP_ACK
static bool asr_is_tcp_pure_ack(struct asr_hw *asr_hw, struct sk_buff *skb)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 2, 0)
	if (TCP_SKB_CB(skb)->flags == TCPHDR_ACK && skb->len <= 68) {
		//dev_info(asr_hw->dev, "%s: len=%d\n", __func__, skb->len);
		return true;
	} else {
		return false;
	}
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)
	if (TCP_SKB_CB(skb)->tcp_flags == TCPHDR_ACK && skb->len <= 68) {
		//dev_info(asr_hw->dev, "%s: len=%d\n", __func__, skb->len);
		return true;
	} else {
		return false;
	}
#else
	return skb_is_tcp_pure_ack(skb);
#endif
}

static bool asr_send_not_tcp_ack(struct asr_hw *asr_hw, struct asr_vif *asr_vif, struct sk_buff *skb)
{
	int tcp_ack_num = asr_hw->mod_params->tcp_ack_num;

	if (!asr_hw || !asr_vif || !skb) {
		return true;
	}

        // tcp_ack_num is zero means disable reduce tcp ack.
        if (tcp_ack_num == 0)
            return false;

	/* when get tcp_ack_num tcp acks and then just send 1 ack to fw */
	// __tcp_send_ack
	if(!asr_is_tcp_pure_ack(asr_hw, skb))// is just true ack
	{
		return false;
	}

	spin_lock_bh(&asr_hw->tcp_ack_lock);
	asr_hw->recvd_tcp_ack_count++;
	if(asr_hw->recvd_tcp_ack_count < tcp_ack_num && !asr_hw->first_ack)
	{
		if(asr_hw->saved_tcp_ack_sdk == NULL)
		{
			asr_hw->saved_tcp_ack_sdk = skb;
			//dev_info(asr_hw->dev, "%s: first ack:%p cnt:%d,seq=%u\n",
			//	__func__,asr_hw->saved_tcp_ack_sdk,asr_hw->recvd_tcp_ack_count, htonl(tcp_hdr(skb)->ack_seq));
		}
		else
		{
			dev_kfree_skb_any(asr_hw->saved_tcp_ack_sdk);// free the old ack skb
			//dev_info(asr_hw->dev, "%s: freed:%p saved ack:%p cnt:%d,seq=%u\n",
			//	__func__,asr_hw->saved_tcp_ack_sdk,skb,asr_hw->recvd_tcp_ack_count, htonl(tcp_hdr(skb)->ack_seq));
			asr_hw->saved_tcp_ack_sdk = skb; // save the new ack skb
		}
		spin_unlock_bh(&asr_hw->tcp_ack_lock);
		del_timer(&asr_hw->tcp_ack_timer);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		mod_timer(&asr_hw->tcp_ack_timer, jiffies + msecs_to_jiffies(ASR_TCP_ACK_TIMER_OUT));
#else
		asr_hw->tcp_ack_timer.expires = jiffies + msecs_to_jiffies(ASR_TCP_ACK_TIMER_OUT);
		add_timer(&asr_hw->tcp_ack_timer);
#endif

		return true;

	}
	//dev_info(asr_hw->dev, "%s:sended cnt:%d ack:%p,seq=%u\n", __func__,asr_hw->recvd_tcp_ack_count,skb, htonl(tcp_hdr(skb)->ack_seq));
	asr_hw->recvd_tcp_ack_count = 0;
	if (asr_hw->saved_tcp_ack_sdk) {
		dev_kfree_skb_any(asr_hw->saved_tcp_ack_sdk);
		asr_hw->saved_tcp_ack_sdk = NULL;
	}
	if(asr_hw->first_ack)
	{
		asr_hw->saved_tcp_ack_sdk = NULL;
		asr_hw->first_ack = 0;
	}
	spin_unlock_bh(&asr_hw->tcp_ack_lock);
	del_timer(&asr_hw->tcp_ack_timer);

	return false;
}
#endif

u32 g_tra_vif_drv_txcnt;
u32 g_cfg_vif_drv_txcnt;

static netdev_tx_t asr_start_xmit_opt_sdio(struct asr_hw *asr_hw, struct asr_vif *asr_vif, struct sk_buff *skb)
{
	int headroom = 0;
	u16 frame_len = 0;
	u16 frame_offset = 0;
	u32 temp_len = 0;
	struct hostdesc *hostdesc = NULL, *hostdesc_new = NULL;
	uint32_t tx_data_end_token = 0xAECDBFCA;
	struct asr_sta *sta;
	struct asr_txq *txq;
	u8 tid;
	struct ethhdr *eth;
	bool is_dhcp = false;
	u8 dhcp_type = 0;
	u8* d_mac = NULL;
    int max_headroom;
	int tx_sk_list_cnt;
    u32 tail_align_len = 0;



	if (!skb) {
		return false;
	}

	if (!asr_hw || !asr_vif) {
		dev_info(asr_hw->dev, "%s:vif is NULL\n", __func__);
		goto xmit_sdio_free;
	}

    max_headroom = sizeof(struct asr_txhdr) + 3;

    /* check whether the current skb can be used */
    if (skb_shared(skb) || (skb_headroom(skb) < max_headroom) || (skb_tailroom(skb) < 32) ||
        (skb_cloned(skb) && (asr_vif->ndev->priv_flags & IFF_BRIDGE_PORT))) {
        struct sk_buff *newskb = skb_copy_expand(skb, max_headroom, 32,
                                                 GFP_ATOMIC);
        if (unlikely(newskb == NULL))
            goto xmit_sdio_free;

        dev_kfree_skb_any(skb);

        skb = newskb;
    }

	/* Get the STA id and TID information */
	sta = asr_get_tx_info(asr_vif, skb, &tid);
	if (!sta) {
		dev_info(asr_hw->dev, "%s:sta is NULL , dev name (%s) , tid = %d\n", __func__,asr_vif->wdev.netdev->name, tid );
		goto xmit_sdio_free;
	}

	txq = asr_txq_sta_get(sta, tid, NULL, asr_hw);
	if (txq->idx == TXQ_INACTIVE) {
		dev_info(asr_hw->dev, "%s:drop,dev name (%s) , phy_flags(0x%X),dev_flags(0x%X),tid=%d,sta_idx=%d\n",
			 __func__, asr_vif->wdev.netdev->name, (unsigned int)asr_hw->phy_flags, (unsigned int)asr_vif->dev_flags,tid, sta->sta_idx);
		goto xmit_sdio_free;
	}

	if (!txq->hwq) {
		dev_err(asr_hw->dev, "%s frame drop, no hwq \n", __func__);
		goto xmit_sdio_free;
	}

    /*  add struct hostdesc and chain to tx_sk_list*/

	/* the real offset is :
	 sizeof(struct txdesc_api)(44) + pad(4) + sizeof(*eth)(14) and
	 it should not bellow the wireless header(46) + sdio_total_len_offset(4)
	 */

	/* Retrieve the pointer to the Ethernet data */
	eth = (struct ethhdr *)skb->data;

	headroom = sizeof(struct asr_txhdr);
	frame_len = (u16) skb->len - sizeof(*eth);
	frame_offset = headroom + sizeof(*eth);

	temp_len = ASR_ALIGN_BLKSZ_HI(frame_offset + frame_len + 4);

	if (temp_len > ASR_SDIO_DATA_MAX_LEN) {
		dev_err(asr_hw->dev, "%s frame drop, temp_len=%u,hostdesc=%u,headrom=%d,eth=%u,skb_len=%d \n",
            __func__, temp_len,(unsigned int)sizeof(struct hostdesc),headroom,(unsigned int)sizeof(*eth),(u16) skb->len );
		goto xmit_sdio_free;
	}

       #if 0
       if (ntohs(eth->h_proto) == ETH_P_PAE) {
		dev_err(asr_hw->dev, "%s tx eapol, da(%x:%x:%x:%x:%x:%x), sa(%x:%x:%x:%x:%x:%x), sta_idx=%d, vif_idx=%d  \n", __func__,
                                     eth->h_dest[0], eth->h_dest[1], eth->h_dest[2], eth->h_dest[3], eth->h_dest[4], eth->h_dest[5],
                                     eth->h_source[0], eth->h_source[1], eth->h_source[2], eth->h_source[3], eth->h_source[4], eth->h_source[5],
                                     sta->sta_idx,
                                     asr_vif->vif_index );

       }
	   #endif
    /****************************************************************************************************/

	is_dhcp = check_is_dhcp_package(asr_vif, true, ntohs(eth->h_proto), &dhcp_type, &d_mac,
				  skb->data + sizeof(*eth), (u16) skb->len - sizeof(*eth));

    /************************** hostdesc prepare . ******************************************************/

    /*
    hif data structure:
               hostdesc           data             tx_data_end_token
    len:       headroom           skb->len           4

    skb operation:
       skb_push(skb, headroom);  fill hostdesc.

       skb_put (skb,4)         ;  fill tx_data_end_token.

	*/
        skb_push(skb, headroom);
	hostdesc = (struct hostdesc *)skb->data;

	// Fill-in the hostdesc
	hostdesc->queue_idx = txq->hwq->id;
	memcpy(&hostdesc->eth_dest_addr, eth->h_dest, ETH_ALEN);
	memcpy(&hostdesc->eth_src_addr, eth->h_source, ETH_ALEN);
	hostdesc->ethertype = eth->h_proto;
	hostdesc->staid = sta->sta_idx;
	hostdesc->tid = tid;
	hostdesc->vif_idx = asr_vif->vif_index;

	/************** add multi-vif txcnt for tx fc.. ****************************************/
	if (asr_hw->vif_index == hostdesc->vif_idx)
			g_tra_vif_drv_txcnt++;

	if (asr_hw->ext_vif_index == hostdesc->vif_idx)
			g_cfg_vif_drv_txcnt++;

	/************** add multi-vif txcnt for tx fc.. ****************************************/

	if (asr_vif->use_4addr && (sta->sta_idx < asr_hw->sta_max_num))
		hostdesc->flags = TXU_CNTRL_USE_4ADDR;
	else
		hostdesc->flags = 0;

	hostdesc->packet_len = frame_len;
	hostdesc->packet_addr = 0;
	hostdesc->packet_offset = frame_offset;
	hostdesc->sdio_tx_total_len = frame_offset + frame_len;
	hostdesc->txq = (u32) (uintptr_t) txq;
	hostdesc->agg_num = 0;
	hostdesc->sdio_tx_len = temp_len - 2;
	hostdesc->timestamp = 0;
    hostdesc->sn = 0xFFFF;   // set default value.

	//memcpy((u8 *) hostdesc + headroom, skb->data, skb->len);

    // add tx_data_end_token to skb.
    tail_align_len = temp_len - skb->len ;

    if (tail_align_len < 4 || (skb->tail + tail_align_len > skb->end)) {
		dev_err(asr_hw->dev, "[%s] unlikely ,temp_len=%d ,skb_len=%d ,tail_len=%d, skb(0x%x ,0x%x)\n",__func__,temp_len,
                                                        skb->len,tail_align_len,
                                                        (u32)skb->tail, (u32)skb->end);
        goto xmit_sdio_free;
    }

	skb_put(skb,tail_align_len);

	memcpy((u8 *) hostdesc + hostdesc->sdio_tx_total_len, &tx_data_end_token, 4);

    // chain skb to tk_list.
    spin_lock_bh(&asr_hw->tx_lock);
	skb_queue_tail(&asr_hw->tx_sk_list, skb);
	asr_vif->tx_skb_cnt ++;	
	tx_sk_list_cnt = skb_queue_len(&asr_hw->tx_sk_list);
	spin_unlock_bh(&asr_hw->tx_lock);

    #if 1  //  new malloc skb for duplicate dhcp pkt.
	if (is_dhcp && dhcp_type != 0x1 && dhcp_type != 0x3) {
        struct sk_buff *skb_new = NULL;

        skb_new = dev_alloc_skb(temp_len);

        if (skb_new) {
            memset(skb_new->data,0,temp_len);
            hostdesc_new = (struct hostdesc *)skb_new->data;
			memcpy(hostdesc_new, hostdesc, temp_len);
			if (is_broadcast_ether_addr(eth->h_dest)) {
				memcpy(&hostdesc_new->eth_dest_addr, d_mac, ETH_ALEN);
			}
            spin_lock_bh(&asr_hw->tx_lock);
	        skb_queue_tail(&asr_hw->tx_sk_list, skb_new);
	        asr_vif->tx_skb_cnt ++;	
	        spin_unlock_bh(&asr_hw->tx_lock);
        }
	}
	#endif

    /****************************************************************************************************/

	if (tx_sk_list_cnt == 1) {
		//dev_info(asr_hw->dev, "%s:aggr_buf_cnt=%d \n", __func__, asr_hw->tx_agg_env.aggr_buf_cnt);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		mod_timer(&asr_hw->txflow_timer,
		jiffies + msecs_to_jiffies(ASR_TXFLOW_TIMER_OUT));
#else
		del_timer_sync(&asr_hw->txflow_timer);
		asr_hw->txflow_timer.expires = jiffies + msecs_to_jiffies(ASR_TXFLOW_TIMER_OUT);
		add_timer(&asr_hw->txflow_timer);
#endif
	}

	if (txlogen)
		dev_err(asr_hw->dev, "[%s] type=%d ,sta_idx=%d ,vif_idx=%d ,frm_len=%d (%d %d %ld),eth_type=0x%x\n",__func__,
									  ASR_VIF_TYPE(asr_vif), sta ? sta->sta_idx : 0xFF , asr_vif ? asr_vif->vif_index : 0xFF ,
									  frame_len,tail_align_len,headroom,sizeof(*eth),
                                                                          hostdesc->ethertype);

	if ((tx_sk_list_cnt == 1 || is_dhcp) &&
		((asr_hw->tx_use_bitmap & 0xFFFE) == 0xFFFE) && (g_xmit_first_trigger_flag == false)) {
		//stop the timer
		hrtimer_cancel(&(asr_hw->tx_evt_hrtimer));
		//tell to transfer, push to queue_front
		g_xmit_first_trigger_flag = true;

		tx_evt_xmit_times++;

		if (!asr_main_process_running(asr_hw)) {
			set_bit(ASR_FLAG_MAIN_TASK_BIT, &asr_hw->ulFlag);
			wake_up_interruptible(&asr_hw->waitq_main_task_thead);
		}

	} else {
		//start a timer or if timer exist do nothing
		if (hrtimer_active(&(asr_hw->tx_evt_hrtimer))) {
			hrtimer_cancel(&(asr_hw->tx_evt_hrtimer));
			hrtimer_start(&asr_hw->tx_evt_hrtimer, ktime_set(0, tx_hr_timer * 1000), HRTIMER_MODE_REL); //2ms
		} else {
			hrtimer_start(&asr_hw->tx_evt_hrtimer, ktime_set(0, tx_hr_timer * 1000), HRTIMER_MODE_REL); //2ms
		}
	}


	sta->stats.tx_pkts ++;
	sta->stats.tx_bytes += skb->len;

	//dev_kfree_skb_any(skb);   // move to tx task.

	return true;

xmit_sdio_free:
	if (asr_vif)
		asr_vif->net_stats.tx_dropped++;

	dev_kfree_skb_any(skb);
	return false;

}

bool asr_start_xmit_agg_sdio(struct asr_hw *asr_hw, struct asr_vif *asr_vif, struct sk_buff *skb)
{
	int headroom = 0;
	u16 frame_len = 0;
	u16 frame_offset = 0;
	u32 temp_len = 0;
	struct hostdesc *hostdesc = NULL, *hostdesc_new = NULL;
	uint32_t tx_data_end_token = 0xAECDBFCA;
	struct asr_sta *sta;
	struct asr_txq *txq;
	u8 tid;
	struct ethhdr *eth;
	bool is_dhcp = false;
	u8 dhcp_type = 0;
	u8* d_mac = NULL;

	if (!skb) {
		return false;
	}

	if (!asr_hw || !asr_vif) {
		dev_info(asr_hw->dev, "%s:vif is NULL\n", __func__);
		goto xmit_sdio_free;
	}

	/* Get the STA id and TID information */
	sta = asr_get_tx_info(asr_vif, skb, &tid);
	if (!sta) {
		dev_info(asr_hw->dev, "%s:sta is NULL , dev name (%s) , tid = %d\n", __func__,asr_vif->wdev.netdev->name, tid );
		goto xmit_sdio_free;
	}

	txq = asr_txq_sta_get(sta, tid, NULL, asr_hw);
	if (txq->idx == TXQ_INACTIVE) {
		dev_info(asr_hw->dev, "%s:drop,dev name (%s) , phy_flags(0x%X),dev_flags(0x%X),tid=%d,sta_idx=%d\n",
			 __func__, asr_vif->wdev.netdev->name, (unsigned int)asr_hw->phy_flags, (unsigned int)asr_vif->dev_flags,tid, sta->sta_idx);
		goto xmit_sdio_free;
	}
	if (!txq->hwq) {
		dev_err(asr_hw->dev, "%s frame drop, no hwq \n", __func__);
		goto xmit_sdio_free;
	}

	/* Retrieve the pointer to the Ethernet data */
	eth = (struct ethhdr *)skb->data;

	//dev_info(asr_hw->dev, "%s:0x%X,%d,%d,%d,0x%X,%02X:%02X:%02X:%02X:%02X:%02X,%02X:%02X:%02X:%02X:%02X:%02X\n",
	//              __func__, (unsigned int)asr_hw->drv_flags, tid, sta->sta_idx, txq->hwq->id,
	//              eth->h_proto, eth->h_dest[0], eth->h_dest[1], eth->h_dest[2], eth->h_dest[3], eth->h_dest[4], eth->h_dest[5],
	//              eth->h_source[0], eth->h_source[1], eth->h_source[2], eth->h_source[3], eth->h_source[4], eth->h_source[5]);


	/* the real offset is :sizeof(struct txdesc_api)(44) + pad(4) + sizeof(*eth)(14) and it should not bellow the wireless header(46)
	 + sdio_total_len_offset(4) */
	headroom = sizeof(struct asr_txhdr);
	frame_len = (u16) skb->len - sizeof(*eth);
	frame_offset = headroom + sizeof(*eth);
	temp_len = ASR_ALIGN_BLKSZ_HI(frame_offset + frame_len + 4);

	if (temp_len > ASR_SDIO_DATA_MAX_LEN) {
		dev_err(asr_hw->dev, "%s frame drop, temp_len=%d \n", __func__, temp_len);
		goto xmit_sdio_free;
	}

	spin_lock_bh(&asr_hw->tx_agg_env_lock);

	is_dhcp =
	    check_is_dhcp_package(asr_vif, true, ntohs(eth->h_proto), &dhcp_type, &d_mac,
				  skb->data + sizeof(*eth), (u16) skb->len - sizeof(*eth));

	hostdesc = (struct hostdesc *)asr_tx_aggr_get_copy_addr(asr_hw, temp_len);
	if (!hostdesc) {
		spin_unlock_bh(&asr_hw->tx_agg_env_lock);
		//dev_err(asr_hw->dev, "%s frame drop,no hostdesc\n",__func__);
		goto xmit_sdio_free;
	}

	hostdesc->queue_idx = txq->hwq->id;
	// Fill-in the descriptor
	memcpy(&hostdesc->eth_dest_addr, eth->h_dest, ETH_ALEN);
	memcpy(&hostdesc->eth_src_addr, eth->h_source, ETH_ALEN);
	hostdesc->ethertype = eth->h_proto;
	hostdesc->staid = sta->sta_idx;
	hostdesc->tid = tid;
	hostdesc->vif_idx = asr_vif->vif_index;

    // add multi-vif txcnt for tx fc..
	if (asr_hw->vif_index == hostdesc->vif_idx)
	        g_tra_vif_drv_txcnt++;

	if (asr_hw->ext_vif_index == hostdesc->vif_idx)
	        g_cfg_vif_drv_txcnt++;

    asr_vif->txring_bytes += temp_len;

	if (asr_vif->use_4addr && (sta->sta_idx < asr_hw->sta_max_num))
		hostdesc->flags = TXU_CNTRL_USE_4ADDR;
	else
		hostdesc->flags = 0;
	hostdesc->packet_len = frame_len;
	hostdesc->packet_addr = 0;
	hostdesc->packet_offset = frame_offset;
	hostdesc->sdio_tx_total_len = frame_offset + frame_len;
	hostdesc->txq = (u32) (uintptr_t) txq;
	hostdesc->agg_num = 0;
	hostdesc->sdio_tx_len = temp_len - 2;
	hostdesc->timestamp = 0;
	memcpy((u8 *) hostdesc + headroom, skb->data, skb->len);
	memcpy((u8 *) hostdesc + hostdesc->sdio_tx_total_len, &tx_data_end_token, 4);

	if (is_dhcp && dhcp_type != 0x1 && dhcp_type != 0x3) {

		hostdesc_new = (struct hostdesc *)asr_tx_aggr_get_copy_addr(asr_hw, temp_len);
		if (hostdesc_new) {

			asr_vif->txring_bytes += temp_len;

			memcpy(hostdesc_new, hostdesc, sizeof(struct hostdesc));
			if (is_broadcast_ether_addr(eth->h_dest)) {
				memcpy(&hostdesc_new->eth_dest_addr, d_mac, ETH_ALEN);
			}
			memcpy((u8 *) hostdesc_new + headroom, skb->data, skb->len);
			memcpy((u8 *) hostdesc_new + hostdesc_new->sdio_tx_total_len,
			       &tx_data_end_token, 4);
		}
	}

	if (asr_hw->tx_agg_env.aggr_buf_cnt == 1) {
		//dev_info(asr_hw->dev, "%s:aggr_buf_cnt=%d \n", __func__, asr_hw->tx_agg_env.aggr_buf_cnt);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		mod_timer(&asr_hw->txflow_timer,
			  jiffies + msecs_to_jiffies(ASR_TXFLOW_TIMER_OUT));
#else
		del_timer_sync(&asr_hw->txflow_timer);
		asr_hw->txflow_timer.expires = jiffies + msecs_to_jiffies(ASR_TXFLOW_TIMER_OUT);
		add_timer(&asr_hw->txflow_timer);
#endif
	}

    if (txlogen)
	    dev_err(asr_hw->dev, "[%s] type=%d ,sta_idx=%d ,vif_idx=%d ,frm_len=%d ,eth_type=0x%x\n",__func__,
                                      ASR_VIF_TYPE(asr_vif), sta ? sta->sta_idx : 0xFF , asr_vif ? asr_vif->vif_index : 0xFF ,
                                      frame_len,hostdesc->ethertype);

	spin_unlock_bh(&asr_hw->tx_agg_env_lock);

	//if ((asr_hw->tx_agg_env.aggr_buf_cnt % (tx_aggr_xmit_thres * tx_aggr) == 0) )
	if (((asr_hw->tx_agg_env.aggr_buf_cnt == 1 || is_dhcp)
	     && ((asr_hw->tx_use_bitmap & 0xFFFE) == 0xFFFE)
	     && (g_xmit_first_trigger_flag == false)
	     && (tx_aggr_xmit_thres == 0))
	    || (tx_aggr_xmit_thres
		&& asr_hw->tx_agg_env.aggr_buf_cnt %
		(tx_aggr_xmit_thres * tx_aggr) == 0)
	    ) {
		//stop the timer
		hrtimer_cancel(&(asr_hw->tx_evt_hrtimer));
		//tell to transfer, push to queue_front
		g_xmit_first_trigger_flag = true;

		tx_evt_xmit_times++;
		if (!asr_main_process_running(asr_hw)) {
			set_bit(ASR_FLAG_MAIN_TASK_BIT, &asr_hw->ulFlag);
			wake_up_interruptible(&asr_hw->waitq_main_task_thead);
		}
	} else {
		//start a timer or if timer exist do nothing
		if (hrtimer_active(&(asr_hw->tx_evt_hrtimer))) {
			hrtimer_cancel(&(asr_hw->tx_evt_hrtimer));
			hrtimer_start(&asr_hw->tx_evt_hrtimer, ktime_set(0, tx_hr_timer * 1000), HRTIMER_MODE_REL);	//2ms
		} else {
			hrtimer_start(&asr_hw->tx_evt_hrtimer, ktime_set(0, tx_hr_timer * 1000), HRTIMER_MODE_REL);	//2ms
		}
	}


	sta->stats.tx_pkts ++;
	sta->stats.tx_bytes += skb->len;
	dev_kfree_skb_any(skb);
	return true;

xmit_sdio_free:
        if (asr_vif)
	    asr_vif->net_stats.tx_dropped++;

	dev_kfree_skb_any(skb);
	return false;
}


netdev_tx_t asr_start_xmit_sdio(struct sk_buff *skb, struct net_device *dev)
{
	struct asr_vif *asr_vif = netdev_priv(dev);
	struct asr_hw *asr_hw = asr_vif->asr_hw;

	//int seconds;
	ASR_DBG(ASR_FN_ENTRY_STR);

	if (test_bit(ASR_DEV_RESTARTING, &asr_hw->phy_flags) ||
	    (ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_STATION &&
		(test_bit(ASR_DEV_STA_DISCONNECTING, &asr_vif->dev_flags) ||
		 !test_bit(ASR_DEV_STA_CONNECTED, &asr_vif->dev_flags) ||
		 test_bit(ASR_DEV_CLOSEING, &asr_vif->dev_flags) ||
		 test_bit(ASR_DEV_PRECLOSEING, &asr_vif->dev_flags)))) {

		dev_info(asr_hw->dev, "%s:drop,phy_flags(%08X),dev_flags(%08X)\n", __func__,
		                            (unsigned int)asr_hw->phy_flags,  (unsigned int)asr_vif->dev_flags);

		asr_vif->net_stats.tx_dropped++;
		dev_kfree_skb_any(skb);

		return NETDEV_TX_OK;
	}

#ifdef ASR_REDUCE_TCP_ACK
	if (asr_send_not_tcp_ack(asr_hw, asr_vif, skb)) {
		return NETDEV_TX_OK;
	}
#endif
	
	asr_tx_flow_ctrl(asr_hw,asr_vif,true);

	if (asr_xmit_opt) {

		return asr_start_xmit_opt_sdio(asr_hw, asr_vif, skb);
	}

	asr_start_xmit_agg_sdio(asr_hw, asr_vif, skb);

	return NETDEV_TX_OK;
}


int asr_start_mgmt_xmit_opt_sdio(struct asr_vif *vif, struct asr_sta *sta,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
			     struct cfg80211_mgmt_tx_params *params,
#else
			     const u8 * buf, size_t len, bool no_cck,
#endif
			     bool offchan, u64 * cookie)
{
	struct asr_hw *asr_hw = vif->asr_hw;
	u16 frame_len;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	u8 *data;
#endif
	struct asr_txq *txq;
	bool robust;
	u16 temp_len;
	u16 frame_offset;
	struct hostdesc *hostdesc = NULL;
	uint32_t tx_data_end_token = 0xAECDBFCA;
	struct ieee80211_mgmt *mgmt = NULL;
	struct sk_buff *skb_new = NULL;
	#ifdef CFG_ROAMING
	struct ieee80211_hdr *hdr = NULL;
	//uint32_t timeout_ms = 0;
	#endif

	ASR_DBG(ASR_FN_ENTRY_STR);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	if (params == NULL) {
		return 0;
	}
	mgmt = (struct ieee80211_mgmt *)(params->buf);
	frame_len = params->len;
#else
	mgmt = (struct ieee80211_mgmt *)buf;
	frame_len = len;
#endif

	frame_offset = sizeof(struct txdesc_api) + HOST_PAD_LEN;
	temp_len = ASR_ALIGN_BLKSZ_HI(frame_offset + frame_len + 4);

	/* Set TID and Queues indexes */
#if (defined CFG_SNIFFER_SUPPORT || defined CFG_CUS_FRAME)
	if (asr_hw->monitor_vif_idx != 0xff)
		txq = asr_txq_vif_get_idle_mode(asr_hw, IDLE_HWQ_IDX, NULL);
	else if (sta) {
		txq = asr_txq_sta_get(sta, 8, NULL, asr_hw);	//VO
	} else
#else
	if (sta) {
		txq = asr_txq_sta_get(sta, 8, NULL, asr_hw);
	} else
#endif
	{
		if (offchan)
			txq =
			    &asr_hw->txq[asr_hw->sta_max_num * NX_NB_TXQ_PER_STA +
					 asr_hw->vif_max_num * NX_NB_TXQ_PER_VIF];
		else
			txq = asr_txq_vif_get(vif, NX_UNK_TXQ_TYPE, NULL);
	}

	/* Ensure that TXQ is active */
	if (txq->idx == TXQ_INACTIVE) {
		netdev_dbg(vif->ndev, "TXQ inactive\n");
		return -EBUSY;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	if (params->buf == NULL) {
		return 0;
	}
	if (params->len < IEEE80211_MIN_ACTION_SIZE) {
		robust = false;
	} else {
		robust = _ieee80211_is_robust_mgmt_frame((struct ieee80211_hdr *)(params->buf));
#ifdef CFG_ROAMING
		hdr = (struct ieee80211_hdr *)(params->buf);
		if (ieee80211_is_action(hdr->frame_control)) {
			u8 *category;
                        u8 *action_code;
			category = ((u8 *) hdr) + 24;
                        action_code = category + 1;
			if(*category == WLAN_CATEGORY_RADIO_MEASUREMENT||*category == WLAN_CATEGORY_WNM){
				dev_err(asr_hw->dev, "%s tx roaming : cate=%d, action=%d\n", __func__, *category,*action_code);
				dev_err(asr_hw->dev, "%s: dev_flags=%08X\n",__func__,(u32)vif->dev_flags);


				#if 0
				for (timeout_ms = 0; timeout_ms < 100; timeout_ms++) {
					if (vif != NULL
						&& ASR_VIF_TYPE(vif) == NL80211_IFTYPE_STATION
						&& (test_bit(ASR_DEV_TXQ_STOP_CHAN, &vif->dev_flags)
						&& (test_bit(ASR_DEV_STA_CONNECTED, &vif->dev_flags)))) {
						msleep(1);
					} else {
						break;
					}
				}
				if (timeout_ms >= 100) {
					dev_err(asr_hw->dev, "%s: cannot send data to fw timeout_ms=%d,dev_flags=%08X\n",
						__func__, timeout_ms, (u32)vif->dev_flags);
					return -EBUSY;
				}
				#endif

			}
		}
#endif		
	}
#else
	if (buf == NULL) {
		return 0;
	}
	robust = ieee80211_is_robust_mgmt_frame((struct ieee80211_hdr *)buf);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	/* Update CSA counter if present */
	if (unlikely(params->n_csa_offsets) && vif->wdev.iftype == NL80211_IFTYPE_AP && vif->ap.csa) {
		int i;
		data = (u8 *)(params->buf);
		for (i = 0; i < params->n_csa_offsets; i++) {
			data[params->csa_offsets[i]] = vif->ap.csa->count;
		}
	}
#endif

	if (temp_len > ASR_SDIO_DATA_MAX_LEN) {
		dev_err(asr_hw->dev, "%s frame drop, temp_len=%d \n", __func__, temp_len);
	} else if (txq->hwq) {

	    // malloc skb
		skb_new = dev_alloc_skb(temp_len);

		if (skb_new) {
			memset(skb_new->data, 0, temp_len);
			hostdesc = (struct hostdesc *)skb_new->data;
			hostdesc->queue_idx = txq->hwq->id;
			hostdesc->staid = (sta) ? sta->sta_idx : 0xFF;
			hostdesc->vif_idx = vif->vif_index;
			hostdesc->tid = 0xFF;
			hostdesc->flags = TXU_CNTRL_MGMT;
			hostdesc->txq = (u32) (uintptr_t) txq;
			if (robust)
				hostdesc->flags |= TXU_CNTRL_MGMT_ROBUST;

			hostdesc->packet_len = frame_len;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
			if (params->no_cck)
#else
			if (no_cck)
#endif
			{
				hostdesc->flags |= TXU_CNTRL_MGMT_NO_CCK;
			}

			hostdesc->agg_num = 0;
			hostdesc->packet_addr = 0;
			hostdesc->packet_offset = frame_offset;
			hostdesc->sdio_tx_len = temp_len - 2;
			hostdesc->sdio_tx_total_len = frame_offset + frame_len;
			hostdesc->timestamp = 0;
            hostdesc->sn = 0xFFFF;
			//dev_err(asr_hw->dev, "asr_start_mgmt_xmit %d %d %d %d \n",frame_offset,frame_len,temp_len,hostdesc->sdio_tx_len);

			/* Copy the provided data */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
			memcpy((u8 *) hostdesc + frame_offset, params->buf, frame_len);
#else
			memcpy((u8 *) hostdesc + frame_offset, buf, frame_len);
#endif
			memcpy((u8 *) hostdesc + frame_offset + frame_len, &tx_data_end_token, 4);

			// chain skb to tk_list.
			spin_lock_bh(&asr_hw->tx_lock);
			skb_queue_tail(&asr_hw->tx_sk_list, skb_new);
			// mrole add for vif fc.
			vif->tx_skb_cnt ++;
			spin_unlock_bh(&asr_hw->tx_lock);

		} else {
			dev_err(asr_hw->dev, "%s:%d: skb alloc of size %u failed\n\n", __func__, __LINE__, temp_len);
		}

	} else {
		hostdesc = NULL;
	}

	*cookie = 0xff;
	/* Confirm transmission to CFG80211 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	cfg80211_mgmt_tx_status(&vif->wdev, *cookie, params->buf, frame_len, hostdesc ? true : false, GFP_ATOMIC);
#elif LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
	cfg80211_mgmt_tx_status(&vif->wdev, *cookie, buf, frame_len, hostdesc ? true : false, GFP_ATOMIC);
#else
	cfg80211_mgmt_tx_status(vif->wdev.netdev, *cookie, buf, frame_len, hostdesc ? true : false, GFP_ATOMIC);
#endif

	dev_info(asr_hw->dev,
			 "[asr_tx_mgmt]iftype=%d,frame=0x%X,sta_idx=%d,vif_idx=%d ,da=%02X:%02X:%02X:%02X:%02X:%02X,len=%d,ret=%d\n",
			 ASR_VIF_TYPE(vif), mgmt->frame_control,
                         (sta) ? sta->sta_idx : 0xFF,
                         (vif) ? vif->vif_index : 0xFF,
                         mgmt->da[0], mgmt->da[1],mgmt->da[2],
			             mgmt->da[3], mgmt->da[4], mgmt->da[5],
			             frame_len, hostdesc ? true : false);

	if (!asr_main_process_running(asr_hw)) {
		set_bit(ASR_FLAG_MAIN_TASK_BIT, &asr_hw->ulFlag);
		wake_up_interruptible(&asr_hw->waitq_main_task_thead);
	}

	return 0;
}


int asr_start_mgmt_xmit_agg_sdio(struct asr_vif *vif, struct asr_sta *sta,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
			     struct cfg80211_mgmt_tx_params *params,
#else
			     const u8 * buf, size_t len, bool no_cck,
#endif
			     bool offchan, u64 * cookie)
{
	struct asr_hw *asr_hw = vif->asr_hw;
	u16 frame_len;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	u8 *data;
#endif
	struct asr_txq *txq;
	bool robust;
	u16 temp_len;
	u16 frame_offset;
	struct hostdesc *hostdesc = NULL;
	uint32_t tx_data_end_token = 0xAECDBFCA;
	struct ieee80211_mgmt *mgmt = NULL;
	#ifdef CFG_ROAMING
	struct ieee80211_hdr *hdr = NULL;
	//uint32_t timeout_ms = 0;
	#endif

	ASR_DBG(ASR_FN_ENTRY_STR);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	if (params == NULL) {
		return 0;
	}
	mgmt = (struct ieee80211_mgmt *)(params->buf);
	frame_len = params->len;
#else
	mgmt = (struct ieee80211_mgmt *)buf;
	frame_len = len;
#endif
	frame_offset = sizeof(struct txdesc_api) + HOST_PAD_LEN;
	temp_len = ASR_ALIGN_BLKSZ_HI(frame_offset + frame_len + 4);

	/* Set TID and Queues indexes */
#if (defined CFG_SNIFFER_SUPPORT || defined CFG_CUS_FRAME)
	if (asr_hw->monitor_vif_idx != 0xff)
		txq = asr_txq_vif_get_idle_mode(asr_hw, IDLE_HWQ_IDX, NULL);
	else if (sta) {
		txq = asr_txq_sta_get(sta, 8, NULL, asr_hw);	//VO
	} else
#else
	if (sta) {

		txq = asr_txq_sta_get(sta, 8, NULL, asr_hw);
	} else
#endif
	{
		if (offchan)
			txq =
			    &asr_hw->txq[asr_hw->sta_max_num * NX_NB_TXQ_PER_STA +
					 asr_hw->vif_max_num * NX_NB_TXQ_PER_VIF];
		else
			txq = asr_txq_vif_get(vif, NX_UNK_TXQ_TYPE, NULL);
	}

	/* Ensure that TXQ is active */
	if (txq->idx == TXQ_INACTIVE) {
		netdev_dbg(vif->ndev, "TXQ inactive\n");
		return -EBUSY;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	if (params->buf == NULL) {
		return 0;
	}
	if (params->len < IEEE80211_MIN_ACTION_SIZE) {
		robust = false;
	} else {
		robust = _ieee80211_is_robust_mgmt_frame((struct ieee80211_hdr *)(params->buf));
#ifdef CFG_ROAMING
		hdr = (struct ieee80211_hdr *)(params->buf);
		if (ieee80211_is_action(hdr->frame_control)) {
			u8 *category;
                        u8 *action_code;
			category = ((u8 *) hdr) + 24;
                        action_code = category + 1;
			if(*category == WLAN_CATEGORY_RADIO_MEASUREMENT||*category == WLAN_CATEGORY_WNM){
				dev_err(asr_hw->dev, "%s tx roaming : cate=%d, action=%d\n", __func__, *category,*action_code);
				dev_err(asr_hw->dev, "%s: dev_flags=%08X\n",__func__,(u32)vif->dev_flags);


				#if 0
				for (timeout_ms = 0; timeout_ms < 100; timeout_ms++) {
					if (vif != NULL
						&& ASR_VIF_TYPE(vif) == NL80211_IFTYPE_STATION
						&& (test_bit(ASR_DEV_TXQ_STOP_CHAN, &vif->dev_flags)
						&& (test_bit(ASR_DEV_STA_CONNECTED, &vif->dev_flags)))) {
						msleep(1);
					} else {
						break;
					}
				}
				if (timeout_ms >= 100) {
					dev_err(asr_hw->dev, "%s: cannot send data to fw timeout_ms=%d,dev_flags=%08X\n",
						__func__, timeout_ms, (u32)vif->dev_flags);
					return -EBUSY;
				}
				#endif
			}
		}
#endif		
	}
#else
	if (buf == NULL) {
		return 0;
	}
	robust = ieee80211_is_robust_mgmt_frame((struct ieee80211_hdr *)buf);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	/* Update CSA counter if present */
	if (unlikely(params->n_csa_offsets) && vif->wdev.iftype == NL80211_IFTYPE_AP && vif->ap.csa) {

#if 1
		int i;
		data = (u8 *)(params->buf);
		for (i = 0; i < params->n_csa_offsets; i++) {
			data[params->csa_offsets[i]] = vif->ap.csa->count;
		}
#endif
	}
#endif

	if (temp_len > ASR_SDIO_DATA_MAX_LEN) {
		dev_err(asr_hw->dev, "%s frame drop, temp_len=%d \n", __func__, temp_len);
	} else if (txq->hwq) {
		spin_lock_bh(&asr_hw->tx_agg_env_lock);
		hostdesc = (struct hostdesc *)asr_tx_aggr_get_copy_addr(asr_hw, temp_len);
		if (hostdesc) {
			hostdesc->queue_idx = txq->hwq->id;
			hostdesc->staid = (sta) ? sta->sta_idx : 0xFF;
			hostdesc->vif_idx = vif->vif_index;
			hostdesc->tid = 0xFF;
			hostdesc->flags = TXU_CNTRL_MGMT;
			hostdesc->txq = (u32) (uintptr_t) txq;
			if (robust)
				hostdesc->flags |= TXU_CNTRL_MGMT_ROBUST;

			hostdesc->packet_len = frame_len;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
			if (params->no_cck)
#else
			if (no_cck)
#endif
			{
				hostdesc->flags |= TXU_CNTRL_MGMT_NO_CCK;
			}

            // mrole add for vif fc.
			vif->txring_bytes += temp_len;

			hostdesc->agg_num = 0;
			hostdesc->packet_addr = 0;
			hostdesc->packet_offset = frame_offset;
			hostdesc->sdio_tx_len = temp_len - 2;
			hostdesc->sdio_tx_total_len = frame_offset + frame_len;
			hostdesc->timestamp = 0;
			//dev_err(asr_hw->dev, "asr_start_mgmt_xmit %d %d %d %d \n",frame_offset,frame_len,temp_len,hostdesc->sdio_tx_len);

			/* Copy the provided data */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
			memcpy((u8 *) hostdesc + frame_offset, params->buf, frame_len);
#else
			memcpy((u8 *) hostdesc + frame_offset, buf, frame_len);
#endif
			memcpy((u8 *) hostdesc + frame_offset + frame_len, &tx_data_end_token, 4);
		} else {
			dev_err(asr_hw->dev, "%s frame drop\n", __func__);
		}
		spin_unlock_bh(&asr_hw->tx_agg_env_lock);
	} else {
		hostdesc = NULL;
	}

	*cookie = 0xff;
	/* Confirm transmission to CFG80211 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	cfg80211_mgmt_tx_status(&vif->wdev, *cookie, params->buf, frame_len, hostdesc ? true : false, GFP_ATOMIC);
#elif LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
	cfg80211_mgmt_tx_status(&vif->wdev, *cookie, buf, frame_len, hostdesc ? true : false, GFP_ATOMIC);
#else
	cfg80211_mgmt_tx_status(vif->wdev.netdev, *cookie, buf, frame_len, hostdesc ? true : false, GFP_ATOMIC);
#endif

	//vif->net_stats.tx_packets++;
	//vif->net_stats.tx_bytes += frame_len;
	//asr_hw->stats.last_tx = jiffies;
#if 0
	if (tx_debug != 256) {
		set_bit(ASR_FLAG_TX_BIT, &asr_hw->ulFlag);
		wake_up_interruptible(&asr_hw->waitq_tx_thead);
	} else
#endif

		dev_info(asr_hw->dev,
			 "[asr_tx_mgmt]iftype=%d,frame=0x%X,sta_idx=%d,vif_idx=%d ,da=%02X:%02X:%02X:%02X:%02X:%02X,len=%d,ret=%d\n",
			 ASR_VIF_TYPE(vif), mgmt->frame_control,
                         (sta) ? sta->sta_idx : 0xFF,
                         (vif) ? vif->vif_index : 0xFF,
                         mgmt->da[0], mgmt->da[1],
			 mgmt->da[2], mgmt->da[3], mgmt->da[4], mgmt->da[5], frame_len, hostdesc ? true : false);

	if (!asr_main_process_running(asr_hw)) {
		set_bit(ASR_FLAG_MAIN_TASK_BIT, &asr_hw->ulFlag);
		wake_up_interruptible(&asr_hw->waitq_main_task_thead);
	}

	return 0;
}

/**
 * asr_start_mgmt_xmit - Transmit a management frame
 *
 * @vif: Vif that send the frame
 * @sta: Destination of the frame. May be NULL if the destiantion is unknown
 *       to the AP.
 * @params: Mgmt frame parameters
 * @offchan: Indicate whether the frame must be send via the offchan TXQ.
 *           (is is redundant with params->offchan ?)
 * @cookie: updated with a unique value to identify the frame with upper layer
 *
 */
int asr_start_mgmt_xmit_sdio(struct asr_vif *vif, struct asr_sta *sta,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
					 struct cfg80211_mgmt_tx_params *params,
#else
					 const u8 * buf, size_t len, bool no_cck,
#endif
					 bool offchan, u64 * cookie)
{
		if (asr_xmit_opt) {
		    return asr_start_mgmt_xmit_opt_sdio(vif, sta,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
								params,
#else
								buf, len, no_cck,
#endif
								offchan, cookie);
		} else {
		    return asr_start_mgmt_xmit_agg_sdio(vif, sta,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
						params,
#else
						buf, len, no_cck,
#endif
						offchan, cookie);
		}
}


#endif

#ifdef CONFIG_ASR_USB
netdev_tx_t asr_start_xmit_usb(struct sk_buff * skb, struct net_device * dev)
{
	struct asr_vif *asr_vif = netdev_priv(dev);
	struct asr_hw *asr_hw = asr_vif->asr_hw;
	struct asr_usbdev_info *asr_plat = asr_hw->plat;
	struct device *usb_dev = asr_plat->dev;
	struct asr_bus *bus_if = NULL;

	struct ethhdr *eth;
	struct asr_sta *sta;
	struct asr_txq *txq;
	int headroom;
	u16 frame_len;
	u16 frame_offset;
	u8 tid;
	u32 temp_len;
	struct hostdesc *hostdesc;
	int max_headroom;
#ifdef ASR_REDUCE_TCP_ACK
	int tcp_ack_num = asr_hw->mod_params->tcp_ack_num;
#endif
	bool is_dhcp = false;
	u8 dhcp_type = 0;
	u8 *d_mac = NULL;


	ASR_DBG(ASR_FN_ENTRY_STR);

	max_headroom = sizeof(struct hostdesc) + HOST_PAD_USB_LEN;
	//debug usb tx fail, temp codes
	if (skb->len > 2000)
		asr_err("skb->len:%u line:%d\n", skb->len, __LINE__);

	/* check whether the current skb can be used */
	if (skb_shared(skb) || (skb_headroom(skb) < max_headroom) ||
	    (skb_cloned(skb) && (dev->priv_flags & IFF_BRIDGE_PORT))) {
		struct sk_buff *newskb = skb_copy_expand(skb, max_headroom, 0,
							 GFP_ATOMIC);
		if (unlikely(newskb == NULL))
			goto free;

		dev_kfree_skb_any(skb);

		skb = newskb;
	}

	/* Get the STA id and TID information */
	sta = asr_get_tx_info(asr_vif, skb, &tid);
	if (!sta) {
		dev_info(asr_hw->dev, "%s:sta is NULL\n", __func__);
		goto free;
	}

	txq = asr_txq_sta_get(sta, tid, NULL, asr_hw);
	if (txq->idx == TXQ_INACTIVE)
		goto free;

	/* Retrieve the pointer to the Ethernet data */
	eth = (struct ethhdr *)skb->data;
	is_dhcp =
		check_is_dhcp_package(asr_vif, true, ntohs(eth->h_proto), &dhcp_type, &d_mac,
				  skb->data + sizeof(*eth), (u16) skb->len - sizeof(*eth));

    /* the real offset is :sizeof(struct txdesc_api)(44) + pad(4) + sizeof(*eth)(14) and it should not bellow the wireless header(46) */
	headroom = sizeof(struct hostdesc) + HOST_PAD_USB_LEN;

	frame_len = (u16) skb->len - sizeof(*eth);
	frame_offset = headroom + sizeof(*eth);
	temp_len = frame_offset + frame_len;
	if (skb->len > 2000)
		asr_err("skb->len:%u line:%d\n", skb->len, __LINE__);

	if (txq->hwq) {
		/* Push headrom */
		skb_push(skb, headroom);
		hostdesc = (struct hostdesc *)(skb->data);
		hostdesc->queue_idx = txq->hwq->id;
		// Fill-in the descriptor
		memcpy(&hostdesc->eth_dest_addr, eth->h_dest, ETH_ALEN);
		memcpy(&hostdesc->eth_src_addr, eth->h_source, ETH_ALEN);
		hostdesc->ethertype = eth->h_proto;
		hostdesc->staid = sta->sta_idx;
		hostdesc->tid = tid;
		hostdesc->vif_idx = asr_vif->vif_index;
		if (asr_vif->use_4addr && (sta->sta_idx < asr_hw->sta_max_num))
			hostdesc->flags = TXU_CNTRL_USE_4ADDR;
		else
			hostdesc->flags = 0;
		hostdesc->packet_len = frame_len;
		hostdesc->packet_addr = 0;
		hostdesc->packet_offset = frame_offset;
		hostdesc->sdio_tx_total_len = frame_offset + frame_len;
		hostdesc->txq = (u32) (uintptr_t) txq;
		hostdesc->agg_num = 0;
		hostdesc->sdio_tx_len = temp_len;
		hostdesc->timestamp = 0;

		if(hostdesc->sdio_tx_total_len == 0xffff){
			dev_info(asr_hw->dev, "%s:drv send sdio_tx_total_len=0xffff\n", __func__);
		}

		bus_if = dev_get_drvdata(usb_dev);

#ifdef ASR_REDUCE_TCP_ACK
		/* when get tcp_ack_num tcp acks and then just send 1 ack to fw */
		// __tcp_send_ack
	if(skb_is_tcp_pure_ack(skb))// is just true ack
	{
		del_timer_sync(&asr_hw->tcp_ack_timer);
		spin_lock_bh(&asr_hw->tcp_ack_lock);
		asr_hw->recvd_tcp_ack_count++;
		if(asr_hw->recvd_tcp_ack_count < tcp_ack_num && !asr_hw->first_ack)
		{
			if(asr_hw->saved_tcp_ack_sdk == NULL)
			{
				asr_hw->saved_tcp_ack_sdk = skb;
				//dev_info(asr_hw->dev, "%s: first ack:%p cnt:%d\n", __func__,asr_hw->saved_tcp_ack_sdk,asr_hw->recvd_tcp_ack_count);
			}
			else
			{
				dev_kfree_skb_any(asr_hw->saved_tcp_ack_sdk);// free the old ack skb
				//dev_info(asr_hw->dev, "%s: freed:%p saved ack:%p cnt:%d\n", __func__,asr_hw->saved_tcp_ack_sdk,skb,asr_hw->recvd_tcp_ack_count);
				asr_hw->saved_tcp_ack_sdk = skb; // save the new ack skb
			}
			spin_unlock_bh(&asr_hw->tcp_ack_lock);
			//
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
			mod_timer(&asr_hw->tcp_ack_timer, jiffies + msecs_to_jiffies(ASR_TCP_ACK_TIMER_OUT));
#else
			asr_hw->tcp_ack_timer.expires = jiffies + msecs_to_jiffies(ASR_TCP_ACK_TIMER_OUT);
			add_timer(&asr_hw->tcp_ack_timer);
#endif
			return NETDEV_TX_OK;

		}
		//dev_info(asr_hw->dev, "%s:sended cnt:%d ack:%p\n", __func__,asr_hw->recvd_tcp_ack_count,skb);
		asr_hw->recvd_tcp_ack_count = 0;
		if(asr_hw->first_ack)
		{
			asr_hw->saved_tcp_ack_sdk = NULL;
			asr_hw->first_ack = 0;
		}
		spin_unlock_bh(&asr_hw->tcp_ack_lock);
	}
#endif


		/* Use bus module to send data frame */
		if (asr_bus_txdata(bus_if, skb)) {
			asr_vif->net_stats.tx_dropped++;
		}
		else {
			sta->stats.tx_pkts ++;
			sta->stats.tx_bytes += frame_len;
			asr_vif->net_stats.tx_packets++;
			asr_vif->net_stats.tx_bytes += frame_len;
			asr_hw->stats.last_tx = jiffies;
		}


		return NETDEV_TX_OK;

	} else {
		dev_err(asr_hw->dev, "%s frame drop, no hwq \n", __func__);
	}

free:
	asr_vif->net_stats.tx_dropped++;
	dev_kfree_skb_any(skb);

	return NETDEV_TX_OK;

}

int asr_start_mgmt_xmit_usb(struct asr_vif *vif, struct asr_sta *sta,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
			    struct cfg80211_mgmt_tx_params *params,
#else
			    const u8 * buf, size_t len, bool no_cck,
#endif
			    bool offchan, u64 * cookie)
{
	struct asr_hw *asr_hw = vif->asr_hw;
	u16 frame_len;
	//u8 *data;
	struct asr_txq *txq;
	bool robust = false;
	u16 temp_len;
	u16 frame_offset;
	struct hostdesc *hostdesc = NULL;
	struct device *dev = asr_hw->plat->dev;
	struct asr_bus *bus_if = NULL;
	struct sk_buff *skb = NULL;
	#ifdef CFG_ROAMING
	struct ieee80211_hdr *hdr = NULL;
	uint32_t timeout_ms = 0;
	#endif
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	frame_len = params->len;
#else
	frame_len = len;
#endif

	frame_offset = sizeof(struct hostdesc) + HOST_PAD_USB_LEN;
	temp_len = frame_offset + frame_len;

	/* Set TID and Queues indexes */
#if (defined CFG_SNIFFER_SUPPORT || defined CFG_CUS_FRAME)
	if (asr_hw->monitor_vif_idx != 0xff)
		txq = asr_txq_vif_get_idle_mode(asr_hw, IDLE_HWQ_IDX, NULL);
	else if (sta) {
		txq = asr_txq_sta_get(sta, 8, NULL, asr_hw);	//VO
	} else
#else
	if (sta) {

		txq = asr_txq_sta_get(sta, 8, NULL, asr_hw);
	} else
#endif
	{
		if (offchan)
			txq =
			    &asr_hw->txq[asr_hw->sta_max_num * NX_NB_TXQ_PER_STA +
					 asr_hw->vif_max_num * NX_NB_TXQ_PER_VIF];
		else
			txq = asr_txq_vif_get(vif, NX_UNK_TXQ_TYPE, NULL);
	}

	/* Ensure that TXQ is active */
	if (txq->idx == TXQ_INACTIVE) {
		netdev_dbg(vif->ndev, "TXQ inactive\n");
		return -EBUSY;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	if (NULL == params || NULL == params->buf) {
		dev_info(asr_hw->dev, "%s temp_len:%d\n", __func__, temp_len);
		return -EBUSY;
	}
	if (params->len < IEEE80211_MIN_ACTION_SIZE) {
		robust = false;
	} else {
		robust = _ieee80211_is_robust_mgmt_frame((struct ieee80211_hdr *)(params->buf));
#ifdef CFG_ROAMING
		hdr = (struct ieee80211_hdr *)(params->buf);
		if (ieee80211_is_action(hdr->frame_control)) {
			u8 *category;
                        u8 *action_code;
			category = ((u8 *) hdr) + 24;
                        action_code = category + 1;
			if(*category == WLAN_CATEGORY_RADIO_MEASUREMENT||*category == WLAN_CATEGORY_WNM){
				dev_err(asr_hw->dev, "%s tx roaming : cate=%d, action=%d\n", __func__, *category,*action_code);
				dev_err(asr_hw->dev, "%s: dev_flags=%08X\n",__func__,(u32)vif->dev_flags);

				#if 0
				for (timeout_ms = 0; timeout_ms < 100; timeout_ms++) {
					if (vif != NULL
						&& ASR_VIF_TYPE(vif) == NL80211_IFTYPE_STATION
						&& (test_bit(ASR_DEV_TXQ_STOP_CHAN, &vif->dev_flags)
						&& (test_bit(ASR_DEV_STA_CONNECTED, &vif->dev_flags)))) {
						msleep(1);
					} else {
						break;
					}
				}
				if (timeout_ms >= 100) {
					dev_err(asr_hw->dev, "%s: cannot send data to fw timeout_ms=%d,dev_flags=%08X\n",
						__func__, timeout_ms, (u32)vif->dev_flags);
					return -EBUSY;
				}
				#endif

			}
		}
#endif
	}
#else
	if (NULL == buf) {
		dev_info(asr_hw->dev, "%s temp_len:%d\n", __func__, temp_len);
		return -EBUSY;
	}

	robust = ieee80211_is_robust_mgmt_frame((struct ieee80211_hdr *)buf);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	/* Update CSA counter if present */
	if (unlikely(params->n_csa_offsets) && vif->wdev.iftype == NL80211_IFTYPE_AP && vif->ap.csa) {
#if 0
		int i;
		u8 *data;
		data = params->buf;
		for (i = 0; i < params->n_csa_offsets; i++) {
			data[params->csa_offsets[i]] = vif->ap.csa->count;
		}
#endif
	}
#endif

	if (txq->hwq) {
		/*
		 * Create a SK Buff object that will contain the provided data
		 */
		skb = dev_alloc_skb(temp_len);
		if (NULL == skb) {
			dev_err(asr_hw->dev, "asr_start_mgmt_xmit : alloc skb fail\n");
			return -ENOMEM;
		} else {
			/*
			 * Move skb->data pointer in order to reserve room for hostdesc
			 * headroom value will be equal to sizeof(struct asr_txhdr)+4
			 */
			skb_reserve(skb, frame_offset);
#ifdef CONFIG_64BIT
			dev_err(asr_hw->dev, "[%s %d] : alloc skb len=%d %d,(%p %u %p %u) %d\n",
				__func__, __LINE__, skb->len, temp_len, (void *)skb->data,
				skb->tail, (void *)skb->head, skb->end, frame_offset);
#else
			dev_err(asr_hw->dev, "[%s %d] : alloc skb len=%d %d,(%p %p %p %p) %d\n",
				__func__, __LINE__, skb->len, temp_len, (void *)skb->data,
				(void *)skb->tail, (void *)skb->head, (void *)skb->end, frame_offset);
#endif
		}

		/* Fill the TX Header */
		skb_push(skb, frame_offset);

		hostdesc = (struct hostdesc *)(skb->data);
		hostdesc->queue_idx = txq->hwq->id;
		hostdesc->staid = (sta) ? sta->sta_idx : 0xFF;
		hostdesc->vif_idx = vif->vif_index;
		hostdesc->tid = 0xFF;
		hostdesc->flags = TXU_CNTRL_MGMT;
		hostdesc->txq = (u32) (uintptr_t) txq;
		if (robust)
			hostdesc->flags |= TXU_CNTRL_MGMT_ROBUST;

		hostdesc->packet_len = frame_len;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
		if (params->no_cck)
#else
		if (no_cck)
#endif
		{
			hostdesc->flags |= TXU_CNTRL_MGMT_NO_CCK;
		}

		hostdesc->agg_num = 0;
		hostdesc->packet_addr = 0;
		hostdesc->packet_offset = frame_offset;
		hostdesc->sdio_tx_len = temp_len;
		hostdesc->sdio_tx_total_len = frame_offset + frame_len;	//valid data length
		hostdesc->timestamp = 0;
#ifdef CONFIG_64BIT
		dev_err(asr_hw->dev, "[%s %d]%d %d %d, skb len=%d,(%p %u)\n", __func__,
			__LINE__, frame_offset, frame_len, hostdesc->sdio_tx_len, skb->len, (void *)skb->data,
			skb->tail);
#else
		dev_err(asr_hw->dev, "[%s %d]%d %d %d, skb len=%d,(%p %p)\n", __func__,
			__LINE__, frame_offset, frame_len, hostdesc->sdio_tx_len, skb->len, (void *)skb->data,
			(void *)skb->tail);
#endif

		/*
		 * Extend the buffer data area in order to contain the provided packet
		 * len value (for skb) will be equal to param->len
		 */

		skb_put(skb, frame_len);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
		memcpy((u8 *) hostdesc + frame_offset, params->buf, frame_len);
#else
		memcpy((u8 *) hostdesc + frame_offset, buf, frame_len);
#endif
#ifdef CONFIG_64BIT
		dev_err(asr_hw->dev, "[%s %d]tx skb len=%d,(%p %u %p %u)\n", __func__,
			__LINE__, skb->len, (void *)skb->data, skb->tail, (void *)skb->head, skb->end);
#else
		dev_err(asr_hw->dev, "[%s %d]tx skb len=%d,(%p %p %p %p)\n", __func__,
			__LINE__, skb->len, (void *)skb->data, (void *)skb->tail, (void *)skb->head, (void *)skb->end);
#endif
	} else {
		hostdesc = NULL;
	}

	*cookie = 0xff;
	/* Confirm transmission to CFG80211 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	cfg80211_mgmt_tx_status(&vif->wdev, *cookie, params->buf, frame_len, hostdesc ? true : false, GFP_ATOMIC);
#else
	cfg80211_mgmt_tx_status(&vif->wdev, *cookie, buf, frame_len, hostdesc ? true : false, GFP_ATOMIC);
#endif

	bus_if = dev_get_drvdata(dev);
	/* Use bus module to send data frame */
	if (skb){
		ret = asr_bus_txdata(bus_if, skb);	//asr_usb_tx(bus->dev, skb)
		if(ret)
			dev_err(asr_hw->dev, "%s tx send failed %d flags:%08X\n", __func__,ret,(u32)vif->dev_flags);
	}

	return 0;
}

#endif

/**
 * netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb,
 *                               struct net_device *dev);
 *    Called when a packet needs to be transmitted.
 *    Must return NETDEV_TX_OK , NETDEV_TX_BUSY.
 *        (can also return NETDEV_TX_LOCKED if NETIF_F_LLTX)
 *
 *  - Initialize the desciptor for this pkt (stored in skb before data)
 *  - Push the pkt in the corresponding Txq
 *  - If possible (i.e. credit available and not in PS) the pkt is pushed
 *    to fw
 */
netdev_tx_t asr_start_xmit(struct sk_buff * skb, struct net_device * dev)
{
#ifdef CONFIG_ASR_SDIO
	return asr_start_xmit_sdio(skb, dev);
#endif

#ifdef CONFIG_ASR_USB
	return asr_start_xmit_usb(skb, dev);
#endif
}

/**
 * asr_start_mgmt_xmit - Transmit a management frame
 *
 * @vif: Vif that send the frame
 * @sta: Destination of the frame. May be NULL if the destiantion is unknown
 *       to the AP.
 * @params: Mgmt frame parameters
 * @offchan: Indicate whether the frame must be send via the offchan TXQ.
 *           (is is redundant with params->offchan ?)
 * @cookie: updated with a unique value to identify the frame with upper layer
 *
 */
int asr_start_mgmt_xmit(struct asr_vif *vif, struct asr_sta *sta,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
			struct cfg80211_mgmt_tx_params *params,
#else
			const u8 * buf, size_t len, bool no_cck,
#endif
			bool offchan, u64 * cookie)
{

#ifdef CONFIG_ASR_SDIO
	return asr_start_mgmt_xmit_sdio(vif, sta,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
					params,
#else
					buf, len, no_cck,
#endif
					offchan, cookie);
#endif

#ifdef CONFIG_ASR_USB
	return asr_start_mgmt_xmit_usb(vif, sta,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
				       params,
#else
				       buf, len, no_cck,
#endif
				       offchan, cookie);
#endif

}

/**
 * asr_txq_credit_update - Update credit for one txq
 *
 * @asr_hw: Driver main data
 * @sta_idx: STA idx
 * @tid: TID
 * @update: offset to apply in txq credits
 *
 * Called when fw send ME_TX_CREDITS_UPDATE_IND message.
 * Apply @update to txq credits, and stop/start the txq if needed
 */
void asr_txq_credit_update(struct asr_hw *asr_hw, int sta_idx, u8 tid, s8 update)
{
	struct asr_sta *sta = &asr_hw->sta_table[sta_idx];
	struct asr_txq *txq;

	txq = asr_txq_sta_get(sta, tid, NULL, asr_hw);

	spin_lock_bh(&asr_hw->tx_lock);

	if (txq->idx != TXQ_INACTIVE) {
		txq->credits += update;
		//trace_credit_update(txq, update);
		if (txq->credits <= 0)
			asr_txq_stop(txq, ASR_TXQ_STOP_FULL);
		else
			asr_txq_start(txq, ASR_TXQ_STOP_FULL);
	}

	spin_unlock_bh(&asr_hw->tx_lock);
}
