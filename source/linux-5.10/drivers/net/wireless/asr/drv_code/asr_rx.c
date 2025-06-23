/**
 ******************************************************************************
 *
 * @file asr_rx.c
 *
 * @brief rx related function
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */
#include <linux/version.h>
#include <linux/ieee80211.h>
#include <linux/etherdevice.h>
#include <net/cfg80211.h>

#include "asr_defs.h"
#include "asr_rx.h"
#include "asr_tx.h"
#include "asr_events.h"
#include "asr_utils.h"
#include "asr_msg_tx.h"


extern bool rxlogen;

#ifdef CFG_SNIFFER_SUPPORT
monitor_cb_t sniffer_rx_cb;
monitor_cb_t sniffer_rx_mgmt_cb;
#endif

const u8 legrates_lut[] = {
	0,			/* 0 */
	1,			/* 1 */
	2,			/* 2 */
	3,			/* 3 */
	-1,			/* 4 */
	-1,			/* 5 */
	-1,			/* 6 */
	-1,			/* 7 */
	10,			/* 8 */
	8,			/* 9 */
	6,			/* 10 */
	4,			/* 11 */
	11,			/* 12 */
	9,			/* 13 */
	7,			/* 14 */
	5			/* 15 */
};

extern int asr_send_sm_disconnect_req(struct asr_hw *asr_hw, struct asr_vif *asr_vif, u16 reason);

/**
 * asr_rx_statistic - save some statistics about received frames
 *
 * @asr_hw: main driver data.
 * @hw_rxhdr: Rx Hardware descriptor of the received frame.
 * @sta: STA that sent the frame.
 */
static void asr_rx_statistic(struct asr_hw *asr_hw, struct hw_rxhdr *hw_rxhdr, struct asr_sta *sta)
{
//#ifdef CONFIG_ASR_DEBUGFS
	struct asr_stats *stats = &asr_hw->stats;
	struct asr_rx_rate_stats *rate_stats = &sta->stats.rx_rate;
	struct rx_vector_1 *rxvect = &hw_rxhdr->hwvect.rx_vect1;
	int mpdu, ampdu, mpdu_prev, rate_idx;

	/* save complete hwvect */
	sta->stats.last_rx = hw_rxhdr->hwvect;
	sta->stats.rx_pkts ++;
	sta->stats.rx_bytes += hw_rxhdr->hwvect.len;

	/* update ampdu rx stats */
	mpdu = hw_rxhdr->hwvect.mpdu_cnt;
	ampdu = hw_rxhdr->hwvect.ampdu_cnt;
	mpdu_prev = stats->ampdus_rx_map[ampdu];

	if (mpdu_prev < mpdu) {
		stats->ampdus_rx_miss += mpdu - mpdu_prev - 1;
	} else {
		stats->ampdus_rx[mpdu_prev]++;
	}
	stats->ampdus_rx_map[ampdu] = mpdu;

	/* update rx rate statistic */
	if (!rate_stats->size)
		return;

	if (rxvect->format_mod > FORMATMOD_NON_HT_DUP_OFDM) {
#ifdef CONFIG_ASR595X
		int mcs;
		int bw = rxvect->ch_bw;
		int sgi;
		int nss;
		switch (rxvect->format_mod) {
		case FORMATMOD_HT_MF:
		case FORMATMOD_HT_GF:
			mcs = rxvect->ht.mcs % 8;
			nss = rxvect->ht.mcs / 8;
			sgi = rxvect->ht.short_gi;
			rate_idx = N_CCK + N_OFDM + nss * 32 + mcs * 4 + bw * 2 + sgi;
			break;
		case FORMATMOD_VHT:
			mcs = rxvect->vht.mcs;
			nss = rxvect->vht.nss;
			sgi = rxvect->vht.short_gi;
			rate_idx = N_CCK + N_OFDM + N_HT + nss * 80 + mcs * 8 + bw * 2 + sgi;
			break;
		case FORMATMOD_HE_SU:
			mcs = rxvect->he.mcs;
			nss = rxvect->he.nss;
			sgi = rxvect->he.gi_type;
			rate_idx = N_CCK + N_OFDM + N_HT + N_VHT + nss * 144 + mcs * 12 + bw * 3 + sgi;
			break;
		default:
			mcs = rxvect->he.mcs;
			nss = rxvect->he.nss;
			sgi = rxvect->he.gi_type;
			rate_idx = N_CCK + N_OFDM + N_HT + N_VHT + N_HE_SU
			    + nss * 216 + mcs * 18 + rxvect->he.ru_size * 3 + sgi;
			break;
		}
#else
		int mcs = rxvect->mcs;
		int bw = rxvect->ch_bw;
		int sgi = rxvect->short_gi;
		int nss;
		if (rxvect->format_mod <= FORMATMOD_HT_GF) {
			nss = rxvect->stbc ? rxvect->stbc : rxvect->n_sts;
			rate_idx = 16 + nss * 32 + mcs * 4 + bw * 2 + sgi;
		} else {
			nss = rxvect->stbc ? rxvect->stbc / 2 : rxvect->n_sts;
			rate_idx = 144 + nss * 80 + mcs * 8 + bw * 2 + sgi;
		}
#endif
	} else {
		int idx = legrates_lut[rxvect->leg_rate];
		if (idx < 4) {
			rate_idx = idx * 2 + rxvect->pre_type;
		} else {
			rate_idx = 8 + idx - 4;
		}
	}
	if (rate_idx < rate_stats->size) {
#ifdef CONFIG_ASR595X
		if (!rate_stats->table[rate_idx])
			rate_stats->rate_cnt++;
#endif
		rate_stats->table[rate_idx]++;
		rate_stats->cpt++;
	} else {
		//wiphy_err(asr_hw->wiphy, "RX: Invalid index conversion => %d/%d\n", rate_idx, rate_stats->size);
	}
//#endif
}

/**
 * asr_rx_data_skb - Process one data frame
 *
 * @asr_hw: main driver data
 * @asr_vif: vif that received the buffer
 * @skb: skb received
 * @rxhdr: HW rx descriptor
 * @return: true if buffer has been forwarded to upper layer
 *
 * If buffer is amsdu , it is first split into a list of skb.
 * Then each skb may be:
 * - forwarded to upper layer
 * - resent on wireless interface
 *
 * When vif is a STA interface, every skb is only forwarded to upper layer.
 * When vif is an AP interface, multicast skb are forwarded and resent, whereas
 * skb for other BSS's STA are only resent.
 */
int rx_amsdu_skb_cnt;
static bool asr_rx_data_skb(struct asr_hw *asr_hw, struct asr_vif *asr_vif, struct sk_buff *skb, struct hw_rxhdr *rxhdr)
{
	struct sk_buff_head list;
	struct sk_buff *rx_skb;
	bool amsdu = rxhdr->flags_is_amsdu;
	bool resend = false, forward = true;
	bool is_dhcp = false;
	u8 dhcp_type = 0;
	u8 *d_mac = NULL;

	skb->dev = asr_vif->ndev;

	__skb_queue_head_init(&list);

	if (amsdu) {
		int count;
		//dev_err(asr_hw->dev, "rxamsdu:%d ",skb->len);
                rx_amsdu_skb_cnt++;
		ieee80211_amsdu_to_8023s(skb, &list, asr_vif->ndev->dev_addr, ASR_VIF_TYPE(asr_vif), 0,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 1)
					 NULL, NULL
#else
					 false
#endif
		    );

		count = skb_queue_len(&list);
		if (count > ARRAY_SIZE(asr_hw->stats.amsdus_rx))
			count = ARRAY_SIZE(asr_hw->stats.amsdus_rx);
		asr_hw->stats.amsdus_rx[count - 1]++;
	} else {
		asr_hw->stats.amsdus_rx[0]++;
		__skb_queue_head(&list, skb);
	}

	if ((ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_AP || ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_P2P_GO) && !(asr_vif->ap.flags & ASR_AP_ISOLATE)) {
		const struct ethhdr *eth;
		rx_skb = skb_peek(&list);
                if (NULL == rx_skb) {
		    dev_err(asr_hw->dev, "unlikely, rx skb null, amsdu:%d , skb_len = %d\n",amsdu,skb ? skb->len : 0);
                    return forward;
                }

		skb_reset_mac_header(rx_skb);
		eth = eth_hdr(rx_skb);

		if (unlikely(is_multicast_ether_addr(eth->h_dest))) {
			/* broadcast pkt need to be forwared to upper layer and resent
			   on wireless interface */
			resend = true;
		} else {
			/* unicast pkt for STA inside the BSS, no need to forward to upper
			   layer simply resend on wireless interface */
			if (rxhdr->flags_dst_idx != 0xFF) {
				struct asr_sta *sta = &asr_hw->sta_table[rxhdr->flags_dst_idx];
				if (sta->valid && (sta->vlan_idx == asr_vif->vif_index)) {
					forward = false;
					resend = true;
				}
			}
		}
	}
	//dev_info(asr_hw->dev, "%s:%u,%d,%d,%d,%d\n",
	//      __func__, skb->priority, rxhdr->flags_user_prio, rxhdr->flags_sta_idx,
	//      rxhdr->flags_vif_idx, rxhdr->flags_dst_idx);
       #if 1
       if ( ntohs(((struct ethhdr *)skb->data)->h_proto) == ETH_P_PAE) {
                struct ethhdr *eth = ((struct ethhdr *)skb->data);
		dev_err(asr_hw->dev, "%s rx eapol, da(%x:%x:%x:%x:%x:%x), sa(%x:%x:%x:%x:%x:%x),  vif_idx=%d  \n", __func__,
                                     eth->h_dest[0], eth->h_dest[1], eth->h_dest[2], eth->h_dest[3], eth->h_dest[4], eth->h_dest[5],
                                     eth->h_source[0], eth->h_source[1], eth->h_source[2], eth->h_source[3], eth->h_source[4], eth->h_source[5],
                                     asr_vif->vif_index );

       }
	   #endif
	   #if 0
	   else if ( ntohs(((struct ethhdr *)skb->data)->h_proto) == 0x0806) {
                struct ethhdr *eth = ((struct ethhdr *)skb->data);
		dev_err(asr_hw->dev, "%s rx arp, da(%x:%x:%x:%x:%x:%x), sa(%x:%x:%x:%x:%x:%x),  vif_idx=%d  \n", __func__,
                                     eth->h_dest[0], eth->h_dest[1], eth->h_dest[2], eth->h_dest[3], eth->h_dest[4], eth->h_dest[5],
                                     eth->h_source[0], eth->h_source[1], eth->h_source[2], eth->h_source[3], eth->h_source[4], eth->h_source[5],
                                     asr_vif->vif_index );
       }
       #endif

	is_dhcp = check_is_dhcp_package(asr_vif, false, ntohs(((struct ethhdr *)skb->data)->h_proto),
		&dhcp_type, &d_mac,
		skb->data + sizeof(struct ethhdr), (u16) skb->len - sizeof(struct ethhdr));

	if (is_dhcp && dhcp_type == 0x5 && d_mac && !memcmp(d_mac, asr_hw->mac_addr, ETH_ALEN)
		&& test_bit(ASR_DEV_STA_CONNECTED, &asr_vif->dev_flags)) {

		set_bit(ASR_DEV_STA_DHCPEND, &asr_vif->dev_flags);
		if(asr_hw->usr_cmd_ps){
			asr_send_set_ps_mode(asr_vif->asr_hw, true);
		}

		dev_info(asr_hw->dev, "sta dhcp success.\n");
	}

	while (!skb_queue_empty(&list)) {
		rx_skb = __skb_dequeue(&list);
		if (!rx_skb) {
			break;
		}
#if 1
		/* resend pkt on wireless interface */
		if (resend) {
			struct sk_buff *skb_copy;
			/* always need to copy buffer when forward=0 to get enough headrom for tsdesc */
			skb_copy =
			    skb_copy_expand(rx_skb, sizeof(struct asr_txhdr) + ASR_SWTXHDR_ALIGN_SZ, 0, GFP_ATOMIC);
			if (skb_copy) {
				int res;
				skb_copy->protocol = htons(ETH_P_802_3);
				skb_reset_network_header(skb_copy);
				skb_reset_mac_header(skb_copy);

				asr_vif->is_resending = true;
				res = dev_queue_xmit(skb_copy);
				asr_vif->is_resending = false;
				/* note: buffer is always consummed by dev_queue_xmit */
				if (res == NET_XMIT_DROP) {
					asr_vif->net_stats.rx_dropped++;
					asr_vif->net_stats.tx_dropped++;
				} else if (res != NET_XMIT_SUCCESS) {
					netdev_err(asr_vif->ndev, "Failed to re-send buffer to driver (res=%d)", res);
					asr_vif->net_stats.tx_errors++;
				}
			} else {
				netdev_err(asr_vif->ndev, "Failed to copy skb");
			}
		}
#endif

		/* forward pkt to upper layer */
		if (forward) {
#if 0				//def CONFIG_ASR_KEY_DBG
			struct ethhdr *ethhdr = (struct ethhdr *)rx_skb->data;
			dev_info(asr_hw->dev,
				 "%s:seq_num is %x,eth type is %x,len is %d,dst-%02x:%02x:%02x:%02x:%02x:%02x,src-%02x:%02x:%02x:%02x:%02x:%02x\n",
				 __func__, rxhdr->hwvect.seq_num, ethhdr->h_proto, rx_skb->len, ethhdr->h_dest[0],
				 ethhdr->h_dest[1], ethhdr->h_dest[2], ethhdr->h_dest[3], ethhdr->h_dest[4],
				 ethhdr->h_dest[5], ethhdr->h_source[0], ethhdr->h_source[1], ethhdr->h_source[2],
				 ethhdr->h_source[3], ethhdr->h_source[4], ethhdr->h_source[5]);
#endif
			rx_skb->protocol = eth_type_trans(rx_skb, asr_vif->ndev);
			memset(rx_skb->cb, 0, sizeof(rx_skb->cb));

			/* Update statistics */
			asr_vif->net_stats.rx_packets++;
			asr_vif->net_stats.rx_bytes += rx_skb->len;

#ifdef ASR_STATS_RATES_TIMER
			asr_hw->stats.rx_bytes += rx_skb->len;
			asr_hw->stats.last_rx_times = jiffies;
#endif

#ifdef CONFIG_ASR_NAPI
            if (asr_hw->mod_params->napi_on) {
                skb_queue_tail(&asr_vif->rx_napi_skb_list, rx_skb);
#if 0
                napi_schedule(&asr_vif->napi);
#else
                if (skb_queue_len(&asr_vif->rx_napi_skb_list) >= asr_hw->mod_params->tcp_ack_num) {
                    napi_schedule(&asr_vif->napi);
                }
                else if (!hrtimer_active(&(asr_vif->rx_napi_hrtimer))) {
                    hrtimer_start(&asr_vif->rx_napi_hrtimer, ktime_set(0, 1000 * 1000), HRTIMER_MODE_REL);
                }
#endif
            }
            else
#endif
            {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 1)
                netif_rx(rx_skb);
#else
                if (in_interrupt())
                    netif_rx(rx_skb);
                else
                    netif_rx_ni(rx_skb);
#endif
            }
		}
	}

	return forward;
}

/**
 * asr_rx_mgmt - Process one 802.11 management frame
 *
 * @asr_hw: main driver data
 * @asr_vif: vif that received the buffer
 * @skb: skb received
 * @rxhdr: HW rx descriptor
 *
 * Process the management frame and free the corresponding skb
 */
static void asr_rx_mgmt(struct asr_hw *asr_hw, struct asr_vif *asr_vif, struct sk_buff *skb, struct hw_rxhdr *hw_rxhdr)
{
	struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *)skb->data;
#ifdef CONFIG_SME
	struct wireless_dev *wdev = asr_vif->ndev->ieee80211_ptr;
	struct cfg80211_bss *bss = NULL;
#endif
	#if   LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
		struct cfg80211_rx_assoc_resp assoc_resp = {0};
	#endif
	int ret = 0;

	/*trace_mgmt_rx(hw_rxhdr->phy_prim20_freq, hw_rxhdr->flags_vif_idx,
	   hw_rxhdr->flags_sta_idx, mgmt); */

	if (ieee80211_is_beacon(mgmt->frame_control)) {
		dev_info(asr_hw->dev, "[asr_rx_mgmt] rx beacom.\n");

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
		cfg80211_report_obss_beacon(asr_hw->wiphy, skb->data, skb->len,
					    hw_rxhdr->phy_prim20_freq, hw_rxhdr->hwvect.rx_vect1.rssi1);
#elif LINUX_VERSION_CODE > KERNEL_VERSION(3, 2, 0)
		cfg80211_report_obss_beacon(asr_hw->wiphy, skb->data, skb->len,
					    hw_rxhdr->phy_prim20_freq, hw_rxhdr->hwvect.rx_vect1.rssi1, GFP_ATOMIC);
#else
		cfg80211_report_obss_beacon(asr_hw->wiphy, skb->data, skb->len,
					    hw_rxhdr->phy_prim20_freq, hw_rxhdr->hwvect.rx_vect1.rssi1);
#endif

	} else if ( ((ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_STATION) || (ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_P2P_CLIENT))
		   && (ieee80211_is_deauth(mgmt->frame_control) || ieee80211_is_disassoc(mgmt->frame_control))) {

                #ifdef CFG_ROAMING
                if (asr_vif->sta.ap == NULL) {
		          dev_info(asr_hw->dev,
				 "[asr_rx_mgmt] unlikely,rx deauth/disassoc but ap is null, (%02X:%02X:%02X:%02X:%02X:%02X) reason=%d flags:%08X\n",
				 mgmt->bssid[0], mgmt->bssid[1], mgmt->bssid[2], mgmt->bssid[3], mgmt->bssid[4], mgmt->bssid[5],
				 mgmt->u.deauth.reason_code,(u32)asr_vif->dev_flags);
                }
                #endif

		if ((!test_bit(ASR_DEV_STA_CONNECTED, &asr_vif->dev_flags) && !test_bit(ASR_DEV_STA_CONNECTING, &asr_vif->dev_flags))
				|| (asr_vif && asr_vif->sta.ap && !ether_addr_equal(asr_vif->sta.ap->mac_addr, mgmt->bssid))) {
			dev_info(asr_hw->dev,
				 "[asr_rx_mgmt] sta disconnected, ignore rx deauth (%02X:%02X:%02X:%02X:%02X:%02X) reason=%d flags:%08X\n",
				 mgmt->bssid[0], mgmt->bssid[1], mgmt->bssid[2], mgmt->bssid[3], mgmt->bssid[4], mgmt->bssid[5],
				 mgmt->u.deauth.reason_code,(u32)asr_vif->dev_flags);

		}
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
		else if (asr_vif->auth_type == NL80211_AUTHTYPE_SAE) {
			if (ieee80211_is_deauth(mgmt->frame_control)) {
				dev_info(asr_hw->dev,
					 "[asr_rx_mgmt] sae deauth protected=%d reason=%d \n",
					 ieee80211_has_protected(mgmt->frame_control), mgmt->u.deauth.reason_code);
			} else {
				dev_info(asr_hw->dev,
					 "[asr_rx_mgmt] sae disassoc protected=%d reason=%d \n",
					 ieee80211_has_protected(mgmt->frame_control), mgmt->u.deauth.reason_code);
			}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
			if (ieee80211_has_protected(mgmt->frame_control)) {
				mutex_lock(&wdev->mtx);
				__acquire(wdev->mtx);
				cfg80211_rx_mlme_mgmt(asr_vif->ndev, skb->data, skb->len);
				__release(wdev->mtx);
				mutex_unlock(&wdev->mtx);

			} else {
				mgmt->u.deauth.reason_code = WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA;
				cfg80211_rx_unprot_mlme_mgmt(asr_vif->ndev, skb->data, skb->len);
			}
#else
			if (ieee80211_has_protected(mgmt->frame_control)) {
				if (ieee80211_is_deauth(mgmt->frame_control)) {
					cfg80211_send_deauth(asr_vif->ndev, skb->data, skb->len);
				} else {
					cfg80211_send_disassoc(asr_vif->ndev, skb->data, skb->len);
				}
			} else {
				mgmt->u.deauth.reason_code = WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA;
				if (ieee80211_is_deauth(mgmt->frame_control)) {
					cfg80211_send_unprot_deauth(asr_vif->ndev, skb->data, skb->len);
				} else {
					cfg80211_send_unprot_disassoc(asr_vif->ndev, skb->data, skb->len);
				}
			}
#endif
		}
#endif
		else {
			dev_info(asr_hw->dev,
				 "[asr_rx_mgmt] auth_type=%d flags:%08X,%x\n",asr_vif->auth_type,(u32)asr_vif->dev_flags,mgmt->frame_control);
			if (ieee80211_is_deauth(mgmt->frame_control)) {
				dev_info(asr_hw->dev, "[asr_rx_mgmt] deauth reason=%d \n", mgmt->u.deauth.reason_code);
			} else {
				dev_info(asr_hw->dev, "[asr_rx_mgmt] disassoc reason=%d \n", mgmt->u.deauth.reason_code);
			}
#ifdef CONFIG_SME


			clear_bit(ASR_DEV_STA_CONNECTED, &asr_vif->dev_flags);
			clear_bit(ASR_DEV_STA_DHCPEND, &asr_vif->dev_flags);
			asr_send_sm_disconnect_req(asr_hw, asr_vif, mgmt->u.deauth.reason_code);



#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
			mutex_lock(&wdev->mtx);
			__acquire(wdev->mtx);
			cfg80211_rx_mlme_mgmt(asr_vif->ndev, skb->data, skb->len);
			__release(wdev->mtx);
			mutex_unlock(&wdev->mtx);
#else
			if (ieee80211_is_deauth(mgmt->frame_control)) {
				cfg80211_send_deauth(asr_vif->ndev, skb->data, skb->len);
			} else {
				cfg80211_send_disassoc(asr_vif->ndev, skb->data, skb->len);
			}
#endif
		}
	}
#ifdef CONFIG_SME
	else if (((ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_STATION) || (ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_P2P_CLIENT) )
                 && ieee80211_is_auth(mgmt->frame_control)) {
		dev_err(asr_hw->dev, "[asr_rx_mgmt]rx auth (%d %d %d)(%d %d)\n",mgmt->u.auth.auth_alg,mgmt->u.auth.auth_transaction,
			mgmt->u.auth.status_code,asr_vif->auth_type,asr_vif->auth_seq);
		dev_err(asr_hw->dev, "ASR: %s,phy_flags==0X%lX , dev_flags=0X%lX.\n", __func__, asr_hw->phy_flags ,asr_vif->dev_flags);
		if(asr_vif->auth_type==NL80211_AUTHTYPE_SAE &&(asr_vif->old_auth_transaction == mgmt->u.auth.auth_transaction||
			(asr_vif->auth_seq==2 && mgmt->u.auth.auth_transaction == 1)))
		{
			dev_err(asr_hw->dev, "free auth\n");
			dev_kfree_skb(skb);
			return;
		}else if(asr_vif->auth_seq == mgmt->u.auth.auth_transaction){
			asr_vif->old_auth_transaction = mgmt->u.auth.auth_transaction;
		}
		// fix zte ap sned auth commit and cfm frame at the same time ,lead to old_auth_transaction canont sync
		else if(mgmt->u.auth.auth_transaction == 2)
			asr_vif->old_auth_transaction = 2;// need sync with ap
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 1)
		mutex_lock(&wdev->mtx);
		__acquire(wdev->mtx);
		cfg80211_rx_mlme_mgmt(asr_vif->ndev, (const u8 *)skb->data, skb->len);
		__release(wdev->mtx);
		mutex_unlock(&wdev->mtx);
#else
		cfg80211_send_rx_auth(asr_vif->ndev, skb->data, skb->len);
#endif
	} else if (((ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_STATION)|| (ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_P2P_CLIENT))
                    && (ieee80211_is_assoc_resp(mgmt->frame_control)||ieee80211_is_reassoc_resp(mgmt->frame_control))) {
		if (!bss) {
			dev_err(asr_hw->dev,
				"[asr_rx_mgmt]rx assoc_resp ,search bss,ssid=(%s),ssid_len=(%d) bssid(%02X:%02X:%02X:%02X:%02X:%02X) conn(%p)\n",
				asr_vif->ssid, asr_vif->ssid_len, mgmt->bssid[0], mgmt->bssid[1], mgmt->bssid[2],
				mgmt->bssid[3], mgmt->bssid[4], mgmt->bssid[5], wdev->conn);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 1)
			bss = cfg80211_get_bss(wdev->wiphy, NULL, mgmt->bssid,
					       asr_vif->ssid,
					       asr_vif->ssid_len, IEEE80211_BSS_TYPE_ESS, IEEE80211_PRIVACY_ANY);
#else

			bss =
			    cfg80211_get_bss(wdev->wiphy,
					     wdev->channel, mgmt->bssid,
					     asr_vif->ssid, asr_vif->ssid_len,
					     WLAN_CAPABILITY_ESS, WLAN_CAPABILITY_ESS);
#endif
		}
		if (bss)
			dev_err(asr_hw->dev,
				"[asr_rx_mgmt]rx assoc_resp , get bss ok (%p) from bssid(%02X:%02X:%02X:%02X:%02X:%02X)\n",
				bss, mgmt->bssid[0], mgmt->bssid[1], mgmt->bssid[2], mgmt->bssid[3], mgmt->bssid[4],
				mgmt->bssid[5]);
		else{
			dev_err(asr_hw->dev, "[asr_rx_mgmt]get bss from bssid fail !!!!\n");
			return ;
		}
#if   LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
		memset(&assoc_resp, 0, sizeof(struct cfg80211_rx_assoc_resp));
		assoc_resp.buf = skb->data;
		assoc_resp.len = skb->len;
		assoc_resp.links[0].bss = bss;
		assoc_resp.links[0].addr = (u8*)&mgmt->bssid;
		cfg80211_rx_assoc_resp(asr_vif->ndev, &assoc_resp);
#elif   LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0)
		cfg80211_rx_assoc_resp(asr_vif->ndev, bss, skb->data, skb->len, -1, NULL, 0);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
		cfg80211_rx_assoc_resp(asr_vif->ndev, bss, skb->data, skb->len, -1);
#elif LINUX_VERSION_CODE > KERNEL_VERSION(3, 2, 0)
		cfg80211_send_rx_assoc(asr_vif->ndev, bss, skb->data, skb->len);
#else
		cfg80211_send_rx_assoc(asr_vif->ndev, skb->data, skb->len);
#endif
	}
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
	else if ((ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_STATION) &&
		 (ieee80211_is_action(mgmt->frame_control) && (mgmt->u.action.category == 6))) {
		struct cfg80211_ft_event_params ft_event;
		ft_event.target_ap = (u8 *) & mgmt->u.action + ETH_ALEN + 2;
		ft_event.ies = (u8 *) & mgmt->u.action + ETH_ALEN * 2 + 2;
		ft_event.ies_len = hw_rxhdr->hwvect.len - (ft_event.ies - (u8 *) mgmt);
		ft_event.ric_ies = NULL;
		ft_event.ric_ies_len = 0;
		cfg80211_ft_event(asr_vif->ndev, &ft_event);
	}
#endif

#if (defined(CONFIG_SAE) && defined(CONFIG_SME))
	else if ((ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_STATION) &&
		 (ieee80211_is_action(mgmt->frame_control) && (mgmt->u.action.category == WLAN_CATEGORY_SA_QUERY))) {
		dev_err(asr_hw->dev, "[asr_rx_mgmt]rx sa query \n");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
		cfg80211_rx_mgmt(&asr_vif->wdev, hw_rxhdr->phy_prim20_freq,
				 hw_rxhdr->hwvect.rx_vect1.rssi1, skb->data, skb->len, 0);
#elif LINUX_VERSION_CODE > KERNEL_VERSION(3, 2, 0)
		cfg80211_rx_mgmt(&asr_vif->wdev, hw_rxhdr->phy_prim20_freq,
				 hw_rxhdr->hwvect.rx_vect1.rssi1, skb->data, skb->len, GFP_ATOMIC);
#else
		cfg80211_rx_mgmt(asr_vif->wdev.netdev, hw_rxhdr->phy_prim20_freq,
				 skb->data, skb->len, GFP_ATOMIC);
#endif
	}
#endif // CONFIG_SAE && CONFIG_SME

#ifdef CFG_ROAMING
	else if ((ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_STATION) &&
		(ieee80211_is_action(mgmt->frame_control) && (mgmt->u.action.category == WLAN_CATEGORY_RADIO_MEASUREMENT||
		mgmt->u.action.category==WLAN_CATEGORY_WNM))){

        // Bss Transition Mgt Request
        //      category(1,=10) + WNM action(1,=7) + Dialog token(1) + Request mode(1) + Disassoc Timer(2) + Validity Interval(1)
        //
        //   + Bss termi Duration(0 or 12) +  Session info URL(option) + BSS transition candidate list entries(option)

	//	dev_err(asr_hw->dev, "[asr_rx_mgmt]rx rm or btm request %d\n",mgmt->u.action.category);

        if (mgmt->u.action.category==WLAN_CATEGORY_WNM) {
			// 6 btm query / 7 btm req / 8 btm rsp.
			dev_err(asr_hw->dev, "[asr_rx_mgmt]rx wnm , action= %d\n", *(&(mgmt->u.action.category) + 1) );
		} else if (mgmt->u.action.category == WLAN_CATEGORY_RADIO_MEASUREMENT) {
		    // 0 radio mea req / 2 link mea req / 4 neigh rpt req
			dev_err(asr_hw->dev, "[asr_rx_mgmt]rx rm ,action= %d\n",mgmt->u.action.u.measurement.action_code);
		}

		//asr_vif->is_roam = true;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
		cfg80211_rx_mgmt(&asr_vif->wdev, hw_rxhdr->phy_prim20_freq,
			hw_rxhdr->hwvect.rx_vect1.rssi1, skb->data, skb->len, 0);
#elif LINUX_VERSION_CODE > KERNEL_VERSION(3, 2, 0)
		cfg80211_rx_mgmt(&asr_vif->wdev, hw_rxhdr->phy_prim20_freq,
			hw_rxhdr->hwvect.rx_vect1.rssi1, skb->data, skb->len, GFP_ATOMIC);
#else
		cfg80211_rx_mgmt(asr_vif->wdev.netdev, hw_rxhdr->phy_prim20_freq,
				 skb->data, skb->len, GFP_ATOMIC);
#endif
		}
#endif   // CFG_ROAMING

	else if (((ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_STATION) || (ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_P2P_CLIENT))
                  && (ieee80211_is_mgmt(mgmt->frame_control))) {
#ifdef CFG_SNIFFER_SUPPORT
		if (NULL != sniffer_rx_mgmt_cb)
			sniffer_rx_mgmt_cb(skb->data, skb->len);
        else
#endif
        {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
		    ret = cfg80211_rx_mgmt(&asr_vif->wdev, hw_rxhdr->phy_prim20_freq,
				       hw_rxhdr->hwvect.rx_vect1.rssi1, skb->data, skb->len, 0);
#elif LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
		    ret = cfg80211_rx_mgmt(&asr_vif->wdev, hw_rxhdr->phy_prim20_freq,
				       hw_rxhdr->hwvect.rx_vect1.rssi1, skb->data, skb->len, GFP_ATOMIC);
#elif LINUX_VERSION_CODE > KERNEL_VERSION(3, 2, 0)
		    ret = cfg80211_rx_mgmt(asr_vif->wdev.netdev, hw_rxhdr->phy_prim20_freq,
				       hw_rxhdr->hwvect.rx_vect1.rssi1, skb->data, skb->len, GFP_ATOMIC);
#else
		    ret = cfg80211_rx_mgmt(asr_vif->wdev.netdev, hw_rxhdr->phy_prim20_freq,
				       skb->data, skb->len, GFP_ATOMIC);
#endif
        }
		dev_info(asr_hw->dev, "[%s][asr_rx_mgmt] sta type: fc=0x%X,%d, ret=%d,sniffer_cb=%p\n",asr_vif->wdev.netdev->name, mgmt->frame_control, hw_rxhdr->phy_prim20_freq,ret,sniffer_rx_mgmt_cb);

	} else {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
		ret = cfg80211_rx_mgmt(&asr_vif->wdev, hw_rxhdr->phy_prim20_freq,
				       hw_rxhdr->hwvect.rx_vect1.rssi1, skb->data, skb->len, 0);
#elif LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
		ret = cfg80211_rx_mgmt(&asr_vif->wdev, hw_rxhdr->phy_prim20_freq,
				       hw_rxhdr->hwvect.rx_vect1.rssi1, skb->data, skb->len, GFP_ATOMIC);
#elif LINUX_VERSION_CODE > KERNEL_VERSION(3, 2, 0)
		ret = cfg80211_rx_mgmt(asr_vif->wdev.netdev, hw_rxhdr->phy_prim20_freq,
				       hw_rxhdr->hwvect.rx_vect1.rssi1, skb->data, skb->len, GFP_ATOMIC);
#else
		ret = cfg80211_rx_mgmt(asr_vif->wdev.netdev, hw_rxhdr->phy_prim20_freq,
				       skb->data, skb->len, GFP_ATOMIC);
#endif

		dev_info(asr_hw->dev,
			 "[asr_rx_mgmt] ap type: iftype=%d,frame=0x%X,freq=%d,(%d %d %d), sa=%02X:%02X:%02X:%02X:%02X:%02X,da =%02X:%02X:%02X:%02X:%02X:%02X ,len=%d,ret=%d\n",
			 ASR_VIF_TYPE(asr_vif), mgmt->frame_control, hw_rxhdr->phy_prim20_freq, hw_rxhdr->flags_vif_idx,hw_rxhdr->flags_sta_idx,hw_rxhdr->flags_dst_idx,
			 mgmt->sa[0],mgmt->sa[1], mgmt->sa[2], mgmt->sa[3], mgmt->sa[4], mgmt->sa[5],
			 mgmt->da[0],mgmt->da[1], mgmt->da[2], mgmt->da[3], mgmt->da[4], mgmt->da[5],skb->len, ret);
	}

	dev_kfree_skb(skb);
}

/**
 * asr_rx_get_vif - Return pointer to the destination vif
 *
 * @asr_hw: main driver data
 * @vif_idx: vif index present in rx descriptor
 *
 * Select the vif that should receive this frame returns NULL if the destination
 * vif is not active.
 * If no vif is specified, this is probably a mgmt broadcast frame, so select
 * the first active interface
 */
static inline struct asr_vif *asr_rx_get_vif(struct asr_hw *asr_hw, int vif_idx)
{
	struct asr_vif *asr_vif = NULL;

	if (vif_idx == 0xFF) {
		list_for_each_entry(asr_vif, &asr_hw->vifs, list) {
			if (asr_vif->up)
				return asr_vif;
		}
		return NULL;
	} else if (vif_idx < asr_hw->vif_max_num) {
		asr_vif = asr_hw->vif_table[vif_idx];
		if (!asr_vif || !asr_vif->up)
			return NULL;
	}

	return asr_vif;
}

static int rx_skb_realloc(struct asr_hw *asr_hw)
{
	struct sk_buff *skb = NULL;
	int malloc_retry = 0;

	while (malloc_retry < 3) {
		if (asr_rxbuff_alloc(asr_hw, asr_hw->ipc_env->rx_bufsz_split, &skb)) {
			malloc_retry++;
			dev_err(asr_hw->dev, "%s:%d: MEM ALLOC FAILED, malloc_retry=%d\n", __func__, __LINE__,
				malloc_retry);
			msleep(1);
		} else {
			// Add the sk buffer structure in the table of rx buffer
			skb_queue_tail(&asr_hw->rx_sk_split_list, skb);

			while (skb_queue_len(&asr_hw->rx_sk_split_list) < asr_hw->ipc_env->rx_bufnb_split) {
				if (asr_rxbuff_alloc(asr_hw, asr_hw->ipc_env->rx_bufsz_split, &skb)) {
					dev_err(asr_hw->dev, "%s:%d: MEM ALLOC FAILED, rx_sk_split_list len=%d\n",
						__func__, __LINE__, skb_queue_len(&asr_hw->rx_sk_split_list));
					break;
				} else {
					// Add the sk buffer structure in the table of rx buffer
					skb_queue_tail(&asr_hw->rx_sk_split_list, skb);
				}
			}

			//dev_err(asr_hw->dev, "%s: rx_sk_split_list realloc success len=%d\n",
			//	__func__, skb_queue_len(&asr_hw->rx_sk_split_list));

			malloc_retry = 0;
			//goto rx_sk_tryagain;
			return 0;
		}
	}

	return -1;
}

static int rx_data_dispatch(struct asr_hw *asr_hw, struct hw_rxhdr *hw_rxhdr, struct sk_buff *skb,uint16_t rx_status)
{
	struct asr_vif *asr_vif = NULL;

	/* Check if we need to forward the buffer */
	asr_vif = asr_rx_get_vif(asr_hw, hw_rxhdr->flags_vif_idx);

	if (!asr_vif) {
		dev_err(asr_hw->dev, "Frame received but no active vif (%d)", hw_rxhdr->flags_vif_idx);
		dev_kfree_skb(skb);
		return 0;
	}

    /*
        for normal mode:   go  asr_rx_mgmt / asr_rx_data_skb
        for sniffer mode:  call sniffer callback.
    */
    if ((ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_MONITOR)) {
		/* sniffer mode callback */
		if ((NULL != sniffer_rx_cb) && (rx_status & RX_STAT_MONITOR))
		    sniffer_rx_cb(skb->data, skb->len);
		else {

			// FIXME: may adapt to wireshark later.
			dev_err(asr_hw->dev, "sniffer mode but no cb (0x%x)", rx_status);
		}

		dev_kfree_skb(skb);

	} else if (hw_rxhdr->flags_is_80211_mpdu) {

		//dev_err(asr_hw->dev, "rxmgmt\n");
		asr_rx_mgmt(asr_hw, asr_vif, skb, hw_rxhdr);

	} else {

		if (hw_rxhdr->flags_sta_idx != 0xff) {
			struct asr_sta *sta = &asr_hw->sta_table[hw_rxhdr->flags_sta_idx];

			asr_rx_statistic(asr_hw, hw_rxhdr, sta);

			if (sta->vlan_idx != asr_vif->vif_index) {
				asr_vif = asr_hw->vif_table[sta->vlan_idx];
				if (!asr_vif) {

					dev_err(asr_hw->dev,
						"%s: drop data, asr_vif is NULL, %d,%d\n",
						__func__, hw_rxhdr->flags_sta_idx, sta->vlan_idx);

					dev_kfree_skb(skb);
					return 0;
				}
			}

			if (hw_rxhdr->flags_is_4addr && !asr_vif->use_4addr) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
				cfg80211_rx_unexpected_4addr_frame(asr_vif->ndev, sta->mac_addr, GFP_ATOMIC);
#endif
			}
		}

		skb->priority = 256 + hw_rxhdr->flags_user_prio;
		if (!asr_rx_data_skb(asr_hw, asr_vif, skb, hw_rxhdr)) {
			//dev_err(asr_hw->dev, "%s: drop data, forward is false\n", __func__);
			dev_kfree_skb(skb);
		}
	}

	return 0;
}

/**
 * asr_rxdataind - Process rx buffer
 *
 * @pthis: Pointer to the object attached to the IPC structure
 *         (points to struct asr_hw is this case)
 * @hostid: Address of the RX descriptor
 *
 * This function is called for each buffer received by the fw
 *
 */
extern int lalalaen;
int asr_rxdataind_run_cnt;
int asr_rxdataind_fn_cnt;

//#if defined(CONFIG_ASR_USB) || defined(SDIO_DEAGGR)
int asr_tra_rxdata_cnt;
int asr_cfg_rxdata_cnt;

//#if defined(SDIO_DEAGGR)
#if defined(CONFIG_ASR_USB) || defined(SDIO_DEAGGR)
u8 asr_rxdataind(void *pthis, void *hostid)
{
	struct asr_hw *asr_hw = NULL;
	struct sk_buff *skb_head = NULL;
	struct sk_buff *skb = NULL;
	struct host_rx_desc *pre_host_rx_desc = NULL;
	struct host_rx_desc *host_rx_desc = NULL;
	struct hw_rxhdr *hw_rxhdr = NULL;
	int msdu_offset = 0;
	int frame_len = 0;
	u16 totol_frmlen = 0;
	u16 seq_num = 0;
	u16 fn_num = 0;
	u16 remain_len = 0;
	static volatile bool amsdu_flag = false;
	static volatile u16 pre_msdu_offset = 0;

	uint16_t  rx_status = 0;

	if (!pthis || !hostid) {
		return -EFAULT;
	}

	asr_hw = (struct asr_hw *)pthis;
	skb_head = (struct sk_buff *)hostid;
	skb = skb_head;

	if (!skb->data) {
		return -EFAULT;
	}

	host_rx_desc = (struct host_rx_desc *)skb->data;
	hw_rxhdr     = (struct hw_rxhdr *)&host_rx_desc->frmlen;
	msdu_offset  = host_rx_desc->pld_offset;
	frame_len    = host_rx_desc->frmlen;
	totol_frmlen = host_rx_desc->totol_frmlen;
	seq_num      = host_rx_desc->seq_num;
	fn_num       = host_rx_desc->fn_num;
	remain_len   = totol_frmlen;

	asr_rxdataind_run_cnt++;
    if (fn_num)
		asr_rxdataind_fn_cnt++;

	if (lalalaen)
		dev_err(asr_hw->dev, "asr_rxdataind [%d] [%d %d] [%d %d] [%d %d %d] [0x%x],(%d, %d,%d)\n",
                                                                                    amsdu_flag,
                                                                                    host_rx_desc->sta_idx,host_rx_desc->tid,
                                                                                    seq_num,fn_num,
                                                                                    totol_frmlen,msdu_offset,frame_len,
                                                                                    host_rx_desc->rx_status,
                                                                                    hw_rxhdr->flags_vif_idx,asr_hw->vif_index,asr_hw->ext_vif_index);

        if ((hw_rxhdr->flags_vif_idx != 0xFF ) && (hw_rxhdr->flags_vif_idx == asr_hw->vif_index )) {
             asr_tra_rxdata_cnt++;
        }
        else if ((hw_rxhdr->flags_vif_idx != 0xFF ) && (hw_rxhdr->flags_vif_idx == asr_hw->ext_vif_index )){
             asr_cfg_rxdata_cnt++;
        }

rx_sk_tryagain:
	if (amsdu_flag) {
		amsdu_flag = false;

		// amsdu case, not first part, dequeue last saved amsdu skb.
		if (!skb_queue_empty(&asr_hw->rx_sk_split_list) && (skb = skb_dequeue(&asr_hw->rx_sk_split_list))) {
			pre_host_rx_desc = (struct host_rx_desc *)(skb->data - pre_msdu_offset);

            // if current fn_num is 0, last saved amsdu should drop, current skb continue rx.
			if (fn_num == 0) {
				hw_rxhdr = (struct hw_rxhdr *)
					&host_rx_desc->frmlen;

				dev_err(asr_hw->dev,
					"amsdu drop %d: %d %d %d %d %d(%d %d %d %d %d)\n",
					__LINE__, msdu_offset,
					frame_len, totol_frmlen,
					seq_num, fn_num,
					pre_host_rx_desc->totol_frmlen,
					pre_host_rx_desc->pld_offset,
					pre_host_rx_desc->seq_num, pre_host_rx_desc->fn_num, skb->len);

                // free saved amsdu skb.
				dev_kfree_skb(skb);

				// reset skb and try rx again.
				skb = skb_head;
				goto rx_sk_tryagain;
			}

			hw_rxhdr = (struct hw_rxhdr *)&pre_host_rx_desc->frmlen;
			rx_status = pre_host_rx_desc->rx_status;

			remain_len = totol_frmlen - skb->len;

			if (totol_frmlen == pre_host_rx_desc->totol_frmlen &&
				seq_num == pre_host_rx_desc->seq_num &&
			    frame_len > 0 &&
			    remain_len >= frame_len &&
			    (skb->len + frame_len + pre_host_rx_desc->pld_offset < WLAN_AMSDU_RX_LEN)) {
				// correct second part, copy to last saved amsdu skb and free current skb.
				memcpy(skb->data + skb->len, (u8 *) host_rx_desc + msdu_offset, frame_len);
				skb_put(skb, frame_len);

                //free currnet skb
				dev_kfree_skb(skb_head);

				if (remain_len > frame_len) {
					// amsdu not finish, rechain amsdu skb to rx_sk_split_list.
					amsdu_flag = true;
					skb_queue_head(&asr_hw->rx_sk_split_list, skb);
					goto check_alloc;
				}

				// remain_len == frame_len, will rx_data_dispatch.
			} else {
			    // error second part , last saved amsdu and current skb both drop.
				dev_err(asr_hw->dev,
					"amsdu and current skb both drop %d: %d %d %d %d %d(%d %d %d %d %d)\n",
					__LINE__, msdu_offset,
					frame_len, totol_frmlen,
					seq_num, fn_num,
					pre_host_rx_desc->totol_frmlen,
					pre_host_rx_desc->pld_offset,
					pre_host_rx_desc->seq_num, pre_host_rx_desc->fn_num, skb->len);
				dev_kfree_skb(skb);
				dev_kfree_skb(skb_head);
				goto check_alloc;
			}
		} else {
		    dev_err(asr_hw->dev, "unlikely ,amsdu_flag is true but no split list\n");
			// free currnet skb.
			dev_kfree_skb(skb_head);
			goto check_alloc;
		}
	}else if (totol_frmlen > frame_len) {
		// first part of amsdu .
		if (frame_len <= 0 ||
			totol_frmlen + msdu_offset >= WLAN_AMSDU_RX_LEN ||
			frame_len + msdu_offset >= WLAN_AMSDU_RX_LEN ||
			fn_num != 0) {

			dev_err(asr_hw->dev,
				"amsdu drop %d: %d %d %d %d %d\n",
				__LINE__, msdu_offset, frame_len, totol_frmlen, seq_num, fn_num);
			dev_kfree_skb(skb_head);
			goto check_alloc;
		}

        // if amsdu part, use AMSDU size skb.
		// first check rx_sk_split_list and realloc when needed.
        if (skb_queue_empty(&asr_hw->rx_sk_split_list)){
			if (rx_skb_realloc(asr_hw)) {
				dev_err(asr_hw->dev, "%s: rx_sk_split_list realloc fail\n", __func__);
			}
		}

		if (!skb_queue_empty(&asr_hw->rx_sk_split_list) && (skb = skb_dequeue(&asr_hw->rx_sk_split_list))) {

			memcpy(skb->data, host_rx_desc, msdu_offset + frame_len);
			skb_reserve(skb, msdu_offset);
			skb_put(skb, frame_len);
			pre_msdu_offset = msdu_offset;
			pre_host_rx_desc = host_rx_desc;

			skb_queue_head(&asr_hw->rx_sk_split_list, skb);
			amsdu_flag = true;

		} else {
		    dev_err(asr_hw->dev, "%s: rx_sk_split_list empty\n", __func__);
		}

		// free current skb and alloc.
		dev_kfree_skb(skb_head);
		goto check_alloc;

	}
	else
	{
		// no-amsdu case.
		if (frame_len <= 0
			|| frame_len >= WLAN_AMSDU_RX_LEN
			|| totol_frmlen >= WLAN_AMSDU_RX_LEN
			|| msdu_offset + frame_len >= WLAN_AMSDU_RX_LEN) {
			dev_err(asr_hw->dev,
				"amsdu drop %d, (%p,%d,%d,%d,%d,%d)\n",
				__LINE__, host_rx_desc,
				msdu_offset, frame_len, totol_frmlen, seq_num, fn_num);

			dev_kfree_skb(skb_head);
			goto check_alloc;
		}

		rx_status = host_rx_desc->rx_status;

		skb_reserve(skb, msdu_offset);
		skb_put(skb, frame_len);

		pre_msdu_offset = 0;
	}

    // here skb is ethier single msdu skb or amsdu skb.
	rx_data_dispatch(asr_hw, hw_rxhdr, skb,rx_status);

check_alloc:

    #ifdef CONFIG_ASR_SDIO
    #ifdef SDIO_DEAGGR_AMSDU
	if (msdu_offset + frame_len > IPC_RXBUF_SIZE_SDIO_DEAGG) {
         /* this skb is amsdu skb, Check if we need to allocate a new buffer */
		 if (asr_rxbuff_alloc(asr_hw, asr_hw->ipc_env->rx_bufsz_sdio_deagg_amsdu, &skb)) {
			 dev_err(asr_hw->dev, "%s:%d: MEM ALLOC FAILED\n", __func__, __LINE__);
		 } else {
			 memset(skb->data, 0, asr_hw->ipc_env->rx_bufsz_sdio_deagg_amsdu);
			 // Add the sk buffer structure in the table of rx buffer
			 skb_queue_tail(&asr_hw->rx_sk_sdio_deaggr_amsdu_list, skb);
		 }

	} else
	#endif
	{
		/* Check if we need to allocate a new buffer */
		if (asr_rxbuff_alloc(asr_hw, asr_hw->ipc_env->rx_bufsz_sdio_deagg, &skb)) {
			dev_err(asr_hw->dev, "%s:%d: MEM ALLOC FAILED\n", __func__, __LINE__);
		} else {
			memset(skb->data, 0, asr_hw->ipc_env->rx_bufsz_sdio_deagg);
			// Add the sk buffer structure in the table of rx buffer
			skb_queue_tail(&asr_hw->rx_sk_sdio_deaggr_list, skb);
		}
	}
	#endif


	return 0;
}
#else

// note, this version will be removed later.
u8 asr_rxdataind(void *pthis, void *hostid)
{
	struct asr_hw *asr_hw = NULL;
	struct sk_buff *skb_head = NULL;
	struct sk_buff *skb = NULL;
	struct host_rx_desc *pre_host_rx_desc = NULL;
	struct host_rx_desc *host_rx_desc = NULL;
	struct hw_rxhdr *hw_rxhdr = NULL;
	int msdu_offset = 0;
	int frame_len = 0;
	int num = 0;
	u16 totol_frmlen = 0;
	u16 seq_num = 0;
	u16 fn_num = 0;
	u16 remain_len = 0;
	static volatile bool amsdu_flag = false;
	static volatile u16 pre_msdu_offset = 0;

	if (!pthis || !hostid) {
		return -EFAULT;
	}

	asr_hw = (struct asr_hw *)pthis;
	skb_head = (struct sk_buff *)hostid;
	skb = skb_head;

	if (!skb->data) {
		return -EFAULT;
	}

	host_rx_desc = (struct host_rx_desc *)skb->data;
	hw_rxhdr = (struct hw_rxhdr *)&host_rx_desc->frmlen;
	msdu_offset = host_rx_desc->pld_offset;
	frame_len = host_rx_desc->frmlen;
#ifdef CONFIG_ASR_USB
	num = 1;
#else
    #ifdef SDIO_DEAGGR
	num = 1;
	#else
	num = host_rx_desc->num;
	#endif
#endif

	totol_frmlen = host_rx_desc->totol_frmlen;
	seq_num = host_rx_desc->seq_num;
	fn_num = host_rx_desc->fn_num;
	remain_len = totol_frmlen;

	asr_rxdataind_run_cnt++;

    if (fn_num)
		asr_rxdataind_fn_cnt++;

    if (lalalaen)
	    dev_err(asr_hw->dev, "asr_rxdataind [%d %d] [%d %d][0x%x]\n", host_rx_desc->sta_idx,host_rx_desc->tid, seq_num,fn_num,host_rx_desc->rx_status);

	while (num--)
	{

rx_sk_tryagain:
		if (!skb_queue_empty(&asr_hw->rx_sk_split_list)
		    && (skb = skb_dequeue(&asr_hw->rx_sk_split_list))) {

			if (amsdu_flag) {
				// amsdu case,not first part.

				amsdu_flag = false;
				pre_host_rx_desc = (struct host_rx_desc *)(skb->data - pre_msdu_offset);

				if (fn_num == 0) {
					hw_rxhdr = (struct hw_rxhdr *)
					    &host_rx_desc->frmlen;

					dev_err(asr_hw->dev,
						"amsdu drop %d: %d %d %d %d %d(%d %d %d %d %d)\n",
						__LINE__, msdu_offset,
						frame_len, totol_frmlen,
						seq_num, fn_num,
						pre_host_rx_desc->totol_frmlen,
						pre_host_rx_desc->pld_offset,
						pre_host_rx_desc->seq_num, pre_host_rx_desc->fn_num, skb->len);

					dev_kfree_skb(skb);
					goto rx_sk_tryagain;
				}

				hw_rxhdr = (struct hw_rxhdr *)
				    &pre_host_rx_desc->frmlen;


				if (totol_frmlen ==
				    pre_host_rx_desc->totol_frmlen && seq_num == pre_host_rx_desc->seq_num) {
					remain_len = totol_frmlen - skb->len;
				} else {
					dev_err(asr_hw->dev,
						"amsdu drop %d: %d %d %d %d %d(%d %d %d %d %d)\n",
						__LINE__, msdu_offset,
						frame_len, totol_frmlen,
						seq_num, fn_num,
						pre_host_rx_desc->totol_frmlen,
						pre_host_rx_desc->pld_offset,
						pre_host_rx_desc->seq_num, pre_host_rx_desc->fn_num, skb->len);
					dev_kfree_skb(skb);
					goto check_alloc;
				}

				do {
					if (frame_len > 0
					    && remain_len >= frame_len
					    && skb->len + frame_len +
					    pre_host_rx_desc->pld_offset < WLAN_AMSDU_RX_LEN && fn_num != 0) {
						memcpy(skb->data + skb->len,
						       (u8 *) host_rx_desc + msdu_offset, frame_len);
						skb_put(skb, frame_len);
						remain_len -= frame_len;
                        //dev_err(asr_hw->dev, "amsdu3: %d %d %d %d %d(%d %d %d %d)\n",msdu_offset,frame_len,totol_frmlen,seq_num,fn_num,
                        //        pre_host_rx_desc->totol_frmlen,pre_host_rx_desc->seq_num,pre_host_rx_desc->fn_num,remain_len);
						if (remain_len == 0) {
							break;
						}
                        if (num == 0)
                            break;

					} else {
						dev_err(asr_hw->dev,
							"amsdu drop %d: %d %d %d %d %d(%d %d %d %d %d)\n",
							__LINE__, msdu_offset,
							frame_len, totol_frmlen,
							seq_num, fn_num,
							pre_host_rx_desc->totol_frmlen,
							pre_host_rx_desc->pld_offset,
							pre_host_rx_desc->seq_num, pre_host_rx_desc->fn_num, skb->len);
						dev_kfree_skb(skb);
						goto check_alloc;
					}

					#ifndef SDIO_DEAGGR
					// next rx pkt
					host_rx_desc = (struct host_rx_desc *)((u8 *)
									       host_rx_desc
									       + host_rx_desc->sdio_rx_len);
					if ((u8 *) host_rx_desc - (u8 *) skb_head->data + msdu_offset > IPC_RXBUF_SIZE) {
						dev_err(asr_hw->dev,
							"%s,%d: rx data over flow,%p,%p\n",
							__func__, __LINE__, skb_head->data, host_rx_desc);
						break;
					}

					msdu_offset = host_rx_desc->pld_offset;
					frame_len = host_rx_desc->frmlen;
					totol_frmlen = host_rx_desc->totol_frmlen;
					seq_num = host_rx_desc->seq_num;
					fn_num = host_rx_desc->fn_num;

					if ((u8 *) host_rx_desc -
					    (u8 *) skb_head->data + msdu_offset + frame_len > IPC_RXBUF_SIZE) {
						dev_err(asr_hw->dev,
							"%s,%d: rx data over flow,%p,%p,%d,%d\n",
							__func__, __LINE__,
							skb_head->data, host_rx_desc, msdu_offset, frame_len);
						break;
					}
					#endif
					//dev_err(asr_hw->dev, "amsdu4: %d %d %d %d %d %d\n",msdu_offset,frame_len,totol_frmlen,seq_num,fn_num,skb->len);
				} while (
				#ifdef SDIO_DEAGGR
				0
				#else
	            num--
				&& totol_frmlen ==
				pre_host_rx_desc->totol_frmlen && seq_num == pre_host_rx_desc->seq_num
		        #endif
				);

				if (remain_len) {
					skb_queue_head(&asr_hw->rx_sk_split_list, skb);
					amsdu_flag = true;
					break;
				}

			}
			else if (totol_frmlen > frame_len) {
				// first amsdu part.
				if (frame_len <= 0
				    || totol_frmlen + msdu_offset >=
				    WLAN_AMSDU_RX_LEN || frame_len + msdu_offset >= WLAN_AMSDU_RX_LEN || fn_num != 0) {

					dev_err(asr_hw->dev,
						"amsdu drop %d: %d %d %d %d %d\n",
						__LINE__, msdu_offset, frame_len, totol_frmlen, seq_num, fn_num);
					dev_kfree_skb(skb);
					goto check_alloc;
				}

				memcpy(skb->data, host_rx_desc, msdu_offset + frame_len);
				skb_reserve(skb, msdu_offset);
				skb_put(skb, frame_len);
				pre_msdu_offset = msdu_offset;
				remain_len = totol_frmlen;
				remain_len -= frame_len;
				pre_host_rx_desc = host_rx_desc;

				//dev_err(asr_hw->dev, "amsdu1: %d %d %d %d %d %d\n",msdu_offset,frame_len,totol_frmlen,seq_num,fn_num,remain_len);
                #ifndef SDIO_DEAGGR
				while (remain_len && num--) {
					// next rx pkt
					host_rx_desc = (struct host_rx_desc *)((u8 *)
									       host_rx_desc
									       + host_rx_desc->sdio_rx_len);

					if ((u8 *) host_rx_desc - (u8 *) skb_head->data + msdu_offset > IPC_RXBUF_SIZE) {
						dev_err(asr_hw->dev,
							"%s,%d: rx data over flow,%p,%p\n",
							__func__, __LINE__, skb_head->data, host_rx_desc);
						break;
					}

					msdu_offset = host_rx_desc->pld_offset;
					frame_len = host_rx_desc->frmlen;
					totol_frmlen = host_rx_desc->totol_frmlen;
					seq_num = host_rx_desc->seq_num;
					fn_num = host_rx_desc->fn_num;

					if ((u8 *) host_rx_desc -
					    (u8 *) skb_head->data + msdu_offset + frame_len > IPC_RXBUF_SIZE) {
						dev_err(asr_hw->dev,
							"%s,%d: rx data over flow,%p,%p,%d,%d\n",
							__func__, __LINE__,
							skb_head->data, host_rx_desc, msdu_offset, frame_len);
						break;
					}

					if (frame_len > 0
					    && seq_num ==
					    pre_host_rx_desc->seq_num
					    && remain_len >= frame_len
					    && skb->len + frame_len +
					    pre_msdu_offset < WLAN_AMSDU_RX_LEN && fn_num != 0) {
						memcpy(skb->data + skb->len,
						       (u8 *) host_rx_desc + msdu_offset, frame_len);
						skb_put(skb, frame_len);
						remain_len -= frame_len;
                        //dev_err(asr_hw->dev, "amsdu2: %d %d %d %d %d %d\n",msdu_offset,frame_len,totol_frmlen,seq_num,fn_num,remain_len);
					} else {
						if (fn_num == 0) {	//current fram not a part of this amsdu frag.
							hw_rxhdr = (struct hw_rxhdr *)
							    &host_rx_desc->frmlen;

							dev_err(asr_hw->dev,
								"amsdu drop %d: %d %d %d %d %d(%d %d %d %d %d)\n",
								__LINE__,
								msdu_offset,
								frame_len,
								totol_frmlen,
								seq_num, fn_num,
								pre_host_rx_desc->totol_frmlen,
								pre_host_rx_desc->pld_offset,
								pre_host_rx_desc->seq_num,
								pre_host_rx_desc->fn_num, skb->len);

							dev_kfree_skb(skb);
							goto rx_sk_tryagain;
						}
						dev_err(asr_hw->dev,
							"amsdu drop %d: %d %d %d %d %d %d(%d %d)\n",
							__LINE__, msdu_offset,
							frame_len, totol_frmlen,
							seq_num, fn_num,
							skb->len, pre_msdu_offset, pre_host_rx_desc->seq_num);
						dev_kfree_skb(skb);
						goto check_alloc;
					}
				}
				#endif
				if (remain_len) {
					skb_queue_head(&asr_hw->rx_sk_split_list, skb);
					amsdu_flag = true;
					break;
				}

			}
			else {
				// no-amsdu part.
				if (frame_len <= 0
				    || frame_len >= WLAN_AMSDU_RX_LEN
				    || totol_frmlen >= WLAN_AMSDU_RX_LEN
				    || msdu_offset + frame_len >= WLAN_AMSDU_RX_LEN) {
					dev_err(asr_hw->dev,
						"amsdu drop %d, (%p,%d,%d,%d,%d,%d,%d)\n",
						__LINE__, host_rx_desc,
						msdu_offset, frame_len, num, totol_frmlen, seq_num, fn_num);
					dev_kfree_skb(skb);
					goto check_alloc;
				}

				memcpy(skb->data, host_rx_desc, msdu_offset + frame_len);
				skb_reserve(skb, msdu_offset);
				skb_put(skb, frame_len);
				pre_msdu_offset = 0;
			}

			rx_data_dispatch(asr_hw, hw_rxhdr, skb, host_rx_desc->rx_status);

check_alloc:
			/* Check if we need to allocate a new buffer */
			if (asr_rxbuff_alloc(asr_hw, asr_hw->ipc_env->rx_bufsz_split, &skb)) {
				dev_err(asr_hw->dev, "%s:%d: MEM ALLOC FAILED\n", __func__, __LINE__);

			} else {
				// Add the sk buffer structure in the table of rx buffer
				skb_queue_tail(&asr_hw->rx_sk_split_list, skb);
			}
            #ifndef SDIO_DEAGGR
			if (num) {
				// next rx pkt
				host_rx_desc = (struct host_rx_desc *)((u8 *) host_rx_desc + host_rx_desc->sdio_rx_len);

				if ((u8 *) host_rx_desc - (u8 *) skb_head->data + msdu_offset > IPC_RXBUF_SIZE) {
					dev_err(asr_hw->dev,
						"%s,%d: rx data over flow,%p,%p\n",
						__func__, __LINE__, skb_head->data, host_rx_desc);
					break;
				}

				hw_rxhdr = (struct hw_rxhdr *)&host_rx_desc->frmlen;
				msdu_offset = host_rx_desc->pld_offset;
				frame_len = host_rx_desc->frmlen;
				totol_frmlen = host_rx_desc->totol_frmlen;
				seq_num = host_rx_desc->seq_num;
				fn_num = host_rx_desc->fn_num;

				if ((u8 *) host_rx_desc -
				    (u8 *) skb_head->data + msdu_offset + frame_len > IPC_RXBUF_SIZE) {
					dev_err(asr_hw->dev,
						"%s,%d: rx data over flow,%p,%p,%d,%d\n",
						__func__, __LINE__,
						skb_head->data, host_rx_desc, msdu_offset, frame_len);
					break;
				}
			}

			#endif
		} else {
			if (!rx_skb_realloc(asr_hw)) {
				goto rx_sk_tryagain;
			}
			dev_err(asr_hw->dev, "%s: rx_sk_split_list empty\n", __func__);
		}
	}

#ifdef CONFIG_ASR_SDIO

    #ifndef SDIO_DEAGGR
	memset(skb_head->data, 0, asr_hw->ipc_env->rx_bufsz);
	// Add the sk buffer structure in the table of rx buffer
	skb_queue_tail(&asr_hw->rx_data_sk_list, skb_head);
	#else
	memset(skb_head->data, 0, asr_hw->ipc_env->rx_bufsz_sdio_deagg);
	// Add the sk buffer structure in the table of rx buffer
	skb_queue_tail(&asr_hw->rx_sk_sdio_deaggr_list, skb_head);
	#endif

#else
	dev_kfree_skb(skb_head);
#endif

	return 0;
}
#endif

#ifdef CONFIG_ASR_NAPI
enum hrtimer_restart rx_napi_hrtimer_handler(struct hrtimer *timer)
{
	struct asr_vif *vif = container_of(timer, struct asr_vif, rx_napi_hrtimer);

    napi_schedule(&vif->napi);
	return HRTIMER_NORESTART;
}

static int napi_recv(struct asr_vif *asr_vif, int budget)
{
	struct sk_buff *rx_skb;
	int work_done = 0;
	u8 rx_ok;

	while ((work_done < budget) &&
	       (!skb_queue_empty(&asr_vif->rx_napi_skb_list))) {
		rx_skb = skb_dequeue(&asr_vif->rx_napi_skb_list);
		if (!rx_skb)
			break;

		rx_ok = false;

#ifdef CONFIG_ASR_GRO
		if (asr_vif->asr_hw->mod_params->gro_on) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 12, 0)
			if (napi_gro_receive(&asr_vif->napi, rx_skb) != GRO_DROP)
				rx_ok = true;
#else
			napi_gro_receive(&asr_vif->napi, rx_skb);
			rx_ok = true;
#endif
            //printk("GRO on!!");
			goto next;
		}
#endif /* CONFIG_ASR_GRO */

		if (netif_receive_skb(rx_skb) == NET_RX_SUCCESS)
			rx_ok = true;

next:
		if (rx_ok == true) {
			work_done++;
		} else {
		}
	}

	return work_done;
}

int asr_recv_napi_poll(struct napi_struct *napi, int budget)
{
	struct asr_vif *asr_vif = container_of(napi, struct asr_vif, napi);
	int work_done = 0;

	work_done = napi_recv(asr_vif, budget);
	if (work_done < budget) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
        napi_complete_done(napi, work_done);
#else
		napi_complete(napi);
#endif
		if (!skb_queue_empty(&asr_vif->rx_napi_skb_list))
			napi_schedule(napi);
	}

	return work_done;
}
#endif

