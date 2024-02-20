/**
 ****************************************************************************************
 *
 * @file asr_msg_rx.c
 *
 * @brief RX function definitions
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ****************************************************************************************
 */
#include <linux/version.h>
#include "asr_defs.h"
#include "asr_tx.h"
#include "asr_events.h"
#include "asr_msg_tx.h"

#define MAC_INFOELT_LEN_OFT        1
#define MAC_INFOELT_INFO_OFT    2
#define MAC_SHRT_MAC_HDR_LEN    24
#define MAC_BEACON_VARIABLE_PART_OFT    (MAC_SHRT_MAC_HDR_LEN + 12)
#define MAC_ELTID_SSID            0
#define MAC_ELTID_DS            3

extern bool mrole_enable;
extern bool cfg_vif_disable_tx;
extern bool tra_vif_disable_tx;
static int asr_freq_to_idx(struct asr_hw *asr_hw, int freq)
{
	struct ieee80211_supported_band *sband;
	int band, ch, idx = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 1)
	for (band = NL80211_BAND_2GHZ; band < NUM_NL80211_BANDS; band++)
#else
	for (band = IEEE80211_BAND_2GHZ; band < IEEE80211_NUM_BANDS; band++)
#endif
	{
		sband = asr_hw->wiphy->bands[band];
		if (!sband) {
			continue;
		}

		for (ch = 0; ch < sband->n_channels; ch++, idx++) {
			if (sband->channels[ch].center_freq == freq) {
				goto exit;
			}
		}
	}

	BUG_ON(1);

exit:
	// Channel has been found, return the index
	return idx;
}

/***************************************************************************
 * Messages from MM task
 **************************************************************************/
static inline int asr_rx_chan_pre_switch_ind(struct asr_hw *asr_hw, struct asr_cmd *cmd, struct ipc_e2a_msg *msg)
{
	struct asr_vif *asr_vif;
	int chan_idx = ((struct mm_channel_pre_switch_ind *)msg->param)->chan_index;

	ASR_DBG(ASR_FN_ENTRY_STR);

	list_for_each_entry(asr_vif, &asr_hw->vifs, list) {
		if (asr_vif->up && asr_vif->ch_index == chan_idx) {
#ifdef CONFIG_ASR_USB
            netif_stop_queue(asr_vif->ndev);
#else
			asr_txq_vif_stop(asr_vif, ASR_TXQ_STOP_CHAN, asr_hw);
#endif
		}
	}



	return 0;
}

static inline int asr_rx_chan_switch_ind(struct asr_hw *asr_hw, struct asr_cmd *cmd, struct ipc_e2a_msg *msg)
{
	struct asr_vif *asr_vif;
	int chan_idx = ((struct mm_channel_switch_ind *)msg->param)->chan_index;
	bool roc = ((struct mm_channel_switch_ind *)msg->param)->roc;

	ASR_DBG(ASR_FN_ENTRY_STR);

	if (!roc) {
		list_for_each_entry(asr_vif, &asr_hw->vifs, list) {
			if (asr_vif->up && asr_vif->ch_index == chan_idx) {
#ifdef CONFIG_ASR_USB
                netif_wake_queue(asr_vif->ndev);
#else
				asr_txq_vif_start(asr_vif, ASR_TXQ_STOP_CHAN, asr_hw);
#endif
			}
		}
	} else {
		/* Retrieve the allocated RoC element */
		struct asr_roc_elem *roc_elem = asr_hw->roc_elem;
		/* Get VIF on which RoC has been started */
		asr_vif = netdev_priv(roc_elem->wdev->netdev);

		/* For debug purpose (use ftrace kernel option) */
		//trace_switch_roc(asr_vif->vif_index);

		/* If mgmt_roc is true, remain on channel has been started by ourself */
		if (!roc_elem->mgmt_roc) {
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 10, 0)
			/* Inform the host that we have switch on the indicated off-channel */
			cfg80211_ready_on_channel(roc_elem->wdev->netdev, (u64)
						  (asr_hw->roc_cookie_cnt),
						  roc_elem->chan, NL80211_CHAN_HT20, roc_elem->duration, GFP_KERNEL);
#else
			/* Inform the host that we have switch on the indicated off-channel */
			cfg80211_ready_on_channel(roc_elem->wdev, (u64)
						  (asr_hw->roc_cookie_cnt),
						  roc_elem->chan, roc_elem->duration, GFP_KERNEL);
#endif
		}

		/* Keep in mind that we have switched on the channel */
		roc_elem->on_chan = true;

		// Enable traffic on OFF channel queue
		asr_txq_offchan_start(asr_hw);
	}

	asr_hw->cur_chanctx = chan_idx;

	return 0;
}

static inline int asr_rx_remain_on_channel_exp_ind(struct asr_hw *asr_hw, struct asr_cmd *cmd, struct ipc_e2a_msg *msg)
{
	/* Retrieve the allocated RoC element */
	struct asr_roc_elem *roc_elem = asr_hw->roc_elem;
	/* Get VIF on which RoC has been started */
	struct asr_vif *asr_vif = netdev_priv(roc_elem->wdev->netdev);

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* For debug purpose (use ftrace kernel option) */
	//trace_roc_exp(asr_vif->vif_index);

	/* If mgmt_roc is true, remain on channel has been started by ourself */
	/* If RoC has been cancelled before we switched on channel, do not call cfg80211 */
	if (!roc_elem->mgmt_roc && roc_elem->on_chan) {
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 10, 0)
		/* Inform the host that off-channel period has expired */
		cfg80211_remain_on_channel_expired(roc_elem->wdev->netdev, (u64)
						   (asr_hw->roc_cookie_cnt), roc_elem->chan, NL80211_CHAN_HT20,
						   GFP_KERNEL);
#else
		/* Inform the host that off-channel period has expired */
		cfg80211_remain_on_channel_expired(roc_elem->wdev, (u64)
						   (asr_hw->roc_cookie_cnt), roc_elem->chan, GFP_KERNEL);
#endif
	}

	/* De-init offchannel TX queue */
	asr_txq_offchan_deinit(asr_vif);

	/* Increase the cookie counter cannot be zero */
	asr_hw->roc_cookie_cnt++;

	if (asr_hw->roc_cookie_cnt == 0) {
		asr_hw->roc_cookie_cnt = 1;
	}

	/* Free the allocated RoC element */
	kfree(roc_elem);
	asr_hw->roc_elem = NULL;

	return 0;
}

static inline int asr_rx_p2p_vif_ps_change_ind(struct asr_hw *asr_hw,
                                                struct asr_cmd *cmd,
                                                struct ipc_e2a_msg *msg)
{
    int vif_idx  = ((struct mm_p2p_vif_ps_change_ind *)msg->param)->vif_index;
    int ps_state = ((struct mm_p2p_vif_ps_change_ind *)msg->param)->ps_state;
    struct asr_vif *vif_entry;

    ASR_DBG(ASR_FN_ENTRY_STR);

    vif_entry = asr_hw->vif_table[vif_idx];

    if (vif_entry) {
        goto found_vif;
    }

    goto exit;

found_vif:


    if (ps_state == MM_PS_MODE_OFF) {
        // Start TX queues for provided VIF
        asr_txq_vif_start(vif_entry, ASR_TXQ_STOP_VIF_PS, asr_hw);
    }
    else {
        // Stop TX queues for provided VIF
        asr_txq_vif_stop(vif_entry, ASR_TXQ_STOP_VIF_PS, asr_hw);
    }

exit:
    return 0;
}

static inline int asr_rx_channel_survey_ind(struct asr_hw *asr_hw, struct asr_cmd *cmd, struct ipc_e2a_msg *msg)
{
	struct mm_channel_survey_ind *ind = (struct mm_channel_survey_ind *)msg->param;
	// Get the channel index
	int idx = asr_freq_to_idx(asr_hw, ind->freq);
	// Get the survey
	struct asr_survey_info *asr_survey = &asr_hw->survey[idx];

	ASR_DBG(ASR_FN_ENTRY_STR);

	// Store the received parameters
	asr_survey->chan_time_ms = ind->chan_time_ms;
	asr_survey->chan_time_busy_ms = ind->chan_time_busy_ms;
	asr_survey->noise_dbm = ind->noise_dbm;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
	asr_survey->filled = (SURVEY_INFO_TIME | SURVEY_INFO_TIME_BUSY);
#else
	asr_survey->filled = (SURVEY_INFO_CHANNEL_TIME | SURVEY_INFO_CHANNEL_TIME_BUSY);
#endif

	if (ind->noise_dbm != 0) {
		asr_survey->filled |= SURVEY_INFO_NOISE_DBM;
	}

	return 0;
}

static inline int asr_rx_p2p_noa_upd_ind(struct asr_hw *asr_hw,
                                          struct asr_cmd *cmd,
                                          struct ipc_e2a_msg *msg)
{
    return 0;
}

static inline int asr_rx_rssi_status_ind(struct asr_hw *asr_hw, struct asr_cmd *cmd, struct ipc_e2a_msg *msg)
{
	struct mm_rssi_status_ind *ind = (struct mm_rssi_status_ind *)msg->param;
	int vif_idx = ind->vif_index;
	bool rssi_status = ind->rssi_status;

	struct asr_vif *vif_entry;

	ASR_DBG(ASR_FN_ENTRY_STR);

	vif_entry = asr_hw->vif_table[vif_idx];
	if (vif_entry) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		cfg80211_cqm_rssi_notify(vif_entry->ndev,
					 rssi_status ?
					 NL80211_CQM_RSSI_THRESHOLD_EVENT_LOW :
					 NL80211_CQM_RSSI_THRESHOLD_EVENT_HIGH, 0, GFP_KERNEL);
#else
		cfg80211_cqm_rssi_notify(vif_entry->ndev,
					 rssi_status ?
					 NL80211_CQM_RSSI_THRESHOLD_EVENT_LOW :
					 NL80211_CQM_RSSI_THRESHOLD_EVENT_HIGH, GFP_KERNEL);
#endif
	}

	return 0;
}

static inline int asr_rx_pktloss_notify_ind(struct asr_hw *asr_hw,
                                             struct asr_cmd *cmd,
                                             struct ipc_e2a_msg *msg)
{
    struct mm_pktloss_ind *ind = (struct mm_pktloss_ind *)msg->param;
    struct asr_vif *vif_entry;
    int vif_idx  = ind->vif_index;

    ASR_DBG(ASR_FN_ENTRY_STR);

    vif_entry = asr_hw->vif_table[vif_idx];
    if (vif_entry) {
        cfg80211_cqm_pktloss_notify(vif_entry->ndev, (const u8 *)ind->mac_addr.array,
                                    ind->num_packets, GFP_ATOMIC);
    }

    return 0;
}

static inline int asr_rx_csa_counter_ind(struct asr_hw *asr_hw, struct asr_cmd *cmd, struct ipc_e2a_msg *msg)
{
	struct mm_csa_counter_ind *ind = (struct mm_csa_counter_ind *)msg->param;
	struct asr_vif *vif;
	bool found = false;

	ASR_DBG(ASR_FN_ENTRY_STR);

	// Look for VIF entry
	list_for_each_entry(vif, &asr_hw->vifs, list) {
		if (vif->vif_index == ind->vif_index) {
			found = true;
			break;
		}
	}

	if (found) {
		if (vif->ap.csa)
			vif->ap.csa->count = ind->csa_count;
		else
			netdev_err(vif->ndev, "CSA counter update but no active CSA");
	}

	return 0;
}

static inline int asr_rx_ps_change_ind(struct asr_hw *asr_hw,
                                        struct asr_cmd *cmd,
                                        struct ipc_e2a_msg *msg)
{
    struct mm_ps_change_ind *ind = (struct mm_ps_change_ind *)msg->param;
    struct asr_sta *sta = &asr_hw->sta_table[ind->sta_idx];

    ASR_DBG(ASR_FN_ENTRY_STR);

    if (ind->sta_idx >= (NX_REMOTE_STA_MAX + NX_VIRT_DEV_MAX)) {
        wiphy_err(asr_hw->wiphy, "Invalid sta index reported by fw %d\n",
                  ind->sta_idx);
        return 1;
    }

    netdev_dbg(asr_hw->vif_table[sta->vif_idx]->ndev,
               "Sta %d, change PS mode to %s", sta->sta_idx,
               ind->ps_state ? "ON" : "OFF");

    if (sta->valid) {
        asr_ps_bh_enable(asr_hw, sta, ind->ps_state);
    } else if (asr_hw->adding_sta) {
        sta->ps.active = ind->ps_state ? true : false;
    } else {
        netdev_err(asr_hw->vif_table[sta->vif_idx]->ndev,
                   "Ignore PS mode change on invalid sta %d\n",ind->sta_idx);
    }

    return 0;
}

static inline int asr_rx_traffic_req_ind(struct asr_hw *asr_hw,
                                          struct asr_cmd *cmd,
                                          struct ipc_e2a_msg *msg)
{
    struct mm_traffic_req_ind *ind = (struct mm_traffic_req_ind *)msg->param;
    struct asr_sta *sta = &asr_hw->sta_table[ind->sta_idx];

    ASR_DBG(ASR_FN_ENTRY_STR);

    netdev_dbg(asr_hw->vif_table[sta->vif_idx]->ndev,
               "Sta %d, asked for %d pkt, uapsd=%d", sta->sta_idx, ind->pkt_cnt,ind->uapsd);

    asr_ps_bh_traffic_req(asr_hw, sta, ind->pkt_cnt,
                           ind->uapsd ? UAPSD_ID : LEGACY_PS_ID);

    return 0;
}

static inline int asr_rx_csa_finish_ind(struct asr_hw *asr_hw, struct asr_cmd *cmd, struct ipc_e2a_msg *msg)
{
	struct mm_csa_finish_ind *ind = (struct mm_csa_finish_ind *)msg->param;
	struct asr_vif *vif;
	bool found = false;

	ASR_DBG(ASR_FN_ENTRY_STR);

	// Look for VIF entry
	list_for_each_entry(vif, &asr_hw->vifs, list) {
		if (vif->vif_index == ind->vif_index) {
			found = true;
			break;
		}
	}

	if (found) {
		if (ASR_VIF_TYPE(vif) == NL80211_IFTYPE_AP ||
            ASR_VIF_TYPE(vif) == NL80211_IFTYPE_P2P_GO) {
			if (vif->ap.csa) {
				vif->ap.csa->status = ind->status;
				vif->ap.csa->ch_idx = ind->chan_idx;
				schedule_work(&vif->ap.csa->work);
			} else
				netdev_err(vif->ndev, "CSA finish indication but no active CSA");
		} else {
			if (ind->status == 0) {
				asr_chanctx_unlink(vif);
				asr_chanctx_link(vif, ind->chan_idx, NULL);
				if (asr_hw->cur_chanctx == ind->chan_idx) {
					asr_txq_vif_start(vif, ASR_TXQ_STOP_CHAN, asr_hw);
				} else
					asr_txq_vif_stop(vif, ASR_TXQ_STOP_CHAN, asr_hw);
			}
		}
	}

	return 0;
}

static inline int asr_rx_csa_traffic_ind(struct asr_hw *asr_hw, struct asr_cmd *cmd, struct ipc_e2a_msg *msg)
{
	struct mm_csa_traffic_ind *ind = (struct mm_csa_traffic_ind *)msg->param;
	struct asr_vif *vif;
	bool found = false;

	ASR_DBG(ASR_FN_ENTRY_STR);

	// Look for VIF entry
	list_for_each_entry(vif, &asr_hw->vifs, list) {
		if (vif->vif_index == ind->vif_index) {
			found = true;
			break;
		}
	}

	if (found) {
		if (ind->enable)
			asr_txq_vif_start(vif, ASR_TXQ_STOP_CSA, asr_hw);
		else
			asr_txq_vif_stop(vif, ASR_TXQ_STOP_CSA, asr_hw);
	}

	return 0;
}

#ifdef CONFIG_TWT
static int asr_rx_twt_traffic_ind(struct asr_hw *asr_hw, struct asr_cmd *cmd, struct ipc_e2a_msg *msg)
{
	struct mm_twt_traffic_ind *ind = (struct mm_twt_traffic_ind *)msg->param;
	//struct asr_sta *sta = &asr_hw->sta_table[ind->sta_idx];

	struct asr_vif *vif;
	bool found = false;

	//pr_info("sta-%d:vif-%d rx_twt_traffic_ind (%d)\n", ind->sta_idx, ind->vif_index, ind->enable);

	// Look for VIF entry
	list_for_each_entry(vif, &asr_hw->vifs, list) {
		if (vif->vif_index == ind->vif_index) {
			found = true;
			break;
		}
	}

	if (found) {
		if (ind->enable)
			asr_txq_vif_start(vif, ASR_TXQ_STOP_TWT, asr_hw);
		else
			asr_txq_vif_stop(vif, ASR_TXQ_STOP_TWT, asr_hw);
	}

	return 0;

}
#endif

/***************************************************************************
 * Messages from SCANU task
 **************************************************************************/
static inline int asr_rx_scanu_start_cfm(struct asr_hw *asr_hw, struct asr_cmd *cmd, struct ipc_e2a_msg *msg)
{
	struct asr_vif *asr_vif = NULL;
#ifdef CONFIG_ASR_SDIO
	struct asr_vif *asr_vif_tmp = NULL;
#endif

	ASR_DBG(ASR_FN_ENTRY_STR);

    // mrole check vif index.
	if (asr_hw->scan_vif_index < asr_hw->vif_max_num + asr_hw->sta_max_num) {
		asr_vif = asr_hw->vif_table[asr_hw->scan_vif_index];
	}

	if (asr_vif)
	    clear_bit(ASR_DEV_SCANING, &asr_vif->dev_flags);

	del_timer(&asr_hw->scan_cmd_timer);

	if (asr_hw->scan_request) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
		struct cfg80211_scan_info info = {
			.aborted = false,
		};
		cfg80211_scan_done(asr_hw->scan_request, &info);
#else
		cfg80211_scan_done(asr_hw->scan_request, false);
#endif
	}

	asr_hw->scan_request = NULL;
	asr_hw->scan_vif_index = 0xFF;


#ifdef CONFIG_ASR_SDIO
	list_for_each_entry(asr_vif_tmp, &asr_hw->vifs, list) {
		asr_tx_flow_ctrl(asr_hw,asr_vif_tmp,false);
	}
#endif

	return 0;
}

static u8 *mac_ie_find(u8 * addr, u16 buff_len, u8 ie_id)
{
	u8 *end = addr + buff_len;

	while (addr < end) {
		if (ie_id == *(addr)) {
			return addr;
		}

		addr += *((u8 *) (addr + MAC_INFOELT_LEN_OFT)) + MAC_INFOELT_INFO_OFT;
	}

	return NULL;
}

static inline int asr_rx_scanu_result_ind(struct asr_hw *asr_hw, struct asr_cmd *cmd, struct ipc_e2a_msg *msg)
{
	struct cfg80211_bss *bss = NULL;
	struct ieee80211_channel *chan;
	struct scanu_result_ind *ind = (struct scanu_result_ind *)msg->param;
	struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *)ind->payload;
	u8 *elmt_addr = NULL;
	u8 ssid[33] = { 0 };
	u8 ssid_len = 0;
	u8 channel_ds = 0;
	int index = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	chan = ieee80211_get_channel(asr_hw->wiphy, ind->center_freq);

	//parse ssid
	elmt_addr = mac_ie_find((mgmt->u.beacon.variable), ind->length - MAC_BEACON_VARIABLE_PART_OFT, MAC_ELTID_SSID);
	if (elmt_addr) {
		ssid_len = *(elmt_addr + MAC_INFOELT_LEN_OFT);
		if (ssid_len > 32) {
			ssid_len = 32;
		}
		memcpy(ssid, (elmt_addr + MAC_INFOELT_INFO_OFT), ssid_len);
		ssid[ssid_len] = 0;

		if (ssid_len > 0) {
			for (index = 0; index < ssid_len; index++) {
				if (ssid[index] != 0) {
					break;
				}
			}
			if (index == ssid_len) {
				//dev_info(asr_hw->dev, "%s:%04X, check ssid\n",__func__,mgmt->frame_control);
				ind->length -= ssid_len;
				memcpy((elmt_addr + MAC_INFOELT_INFO_OFT),
				       (elmt_addr + MAC_INFOELT_INFO_OFT + ssid_len), ind->length);
				*((u8 *) (elmt_addr + MAC_INFOELT_LEN_OFT)) = 0;
			}
		}
	}
	//parse channel
	elmt_addr = mac_ie_find((mgmt->u.beacon.variable), ind->length - MAC_BEACON_VARIABLE_PART_OFT, MAC_ELTID_DS);
	if (elmt_addr != 0) {
		channel_ds = *(elmt_addr + MAC_INFOELT_INFO_OFT);
	}

	if (chan != NULL)
		bss = cfg80211_inform_bss_frame(asr_hw->wiphy, chan, (struct ieee80211_mgmt *)
						ind->payload, ind->length, ind->rssi * 100, GFP_ATOMIC);

	if (bss != NULL) {
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 10, 0)
		cfg80211_put_bss(bss);
#else
		cfg80211_put_bss(asr_hw->wiphy, bss);
#endif

		dev_info(asr_hw->dev,
			 "%s:%04X,%d,len=%d,chan=%d,rssi=%d,bssid=%02X:%02X:%02X:%02X:%02X:%02X,ssid(%d)=%s\n",
			 __func__, mgmt->frame_control, ind->center_freq,ind->length,
			 channel_ds, ind->rssi, bss->bssid[0], bss->bssid[1],
			 bss->bssid[2], bss->bssid[3], bss->bssid[4], bss->bssid[5]
			 , ssid_len, ssid);
	} else {

		dev_info(asr_hw->dev,
			 "%s:%04X,%d,chan=%d,rssi=%d,bssid=%02X:%02X:%02X:%02X:%02X:%02X,ssid(%d)=%s, bss=NULL\n",
			 __func__, mgmt->frame_control, ind->center_freq,
			 channel_ds, ind->rssi, mgmt->bssid[0], mgmt->bssid[1],
			 mgmt->bssid[2], mgmt->bssid[3], mgmt->bssid[4], mgmt->bssid[5]
			 , ssid_len, ssid);
	}

	return 0;
}

/***************************************************************************
 * Messages from ME task
 **************************************************************************/
static inline int asr_rx_me_tkip_mic_failure_ind(struct asr_hw *asr_hw, struct asr_cmd *cmd, struct ipc_e2a_msg *msg)
{
	struct me_tkip_mic_failure_ind *ind = (struct me_tkip_mic_failure_ind *)msg->param;
	struct asr_vif *asr_vif = asr_hw->vif_table[ind->vif_idx];
	struct net_device *dev = NULL;

	ASR_DBG(ASR_FN_ENTRY_STR);

	if (asr_vif == NULL) {
		return 0;
	}

	dev = asr_vif->ndev;
	if (dev == NULL) {
		return 0;
	}

	cfg80211_michael_mic_failure(dev, (u8 *) & ind->addr,
				     (ind->ga ? NL80211_KEYTYPE_GROUP :
				      NL80211_KEYTYPE_PAIRWISE), ind->keyid, (u8 *) & ind->tsc, GFP_ATOMIC);

	return 0;
}

static inline int asr_rx_me_tx_credits_update_ind(struct asr_hw *asr_hw, struct asr_cmd *cmd, struct ipc_e2a_msg *msg)
{
	struct me_tx_credits_update_ind *ind = (struct me_tx_credits_update_ind *)msg->param;

	ASR_DBG(ASR_FN_ENTRY_STR);

	asr_txq_credit_update(asr_hw, ind->sta_idx, ind->tid, ind->credits);

	return 0;
}

/***************************************************************************
 * Messages from SM task
 **************************************************************************/
static inline int asr_rx_sm_connect_ind(struct asr_hw *asr_hw, struct asr_cmd *cmd, struct ipc_e2a_msg *msg)
{
	struct sm_connect_ind *ind = (struct sm_connect_ind *)msg->param;
	struct asr_vif *asr_vif = asr_hw->vif_table[ind->vif_idx];
	struct net_device *dev = NULL;
	const u8 *req_ie, *rsp_ie;
	int index = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	if (asr_vif == NULL) {
		return 0;
	}

	dev = asr_vif->ndev;
	if (dev == NULL) {
		return 0;
	}

	clear_bit(ASR_DEV_STA_CONNECTING, &asr_vif->dev_flags);

	dev_info(asr_hw->dev, "%s:state code is %x,vif idx is %x ind->roamed is %x,ind->ap_idx=%d\n",
		 __func__, ind->status_code, ind->vif_idx, ind->roamed, ind->ap_idx);

	/* Retrieve IE addresses and lengths */
	req_ie = (const u8 *)ind->assoc_ie_buf;
	rsp_ie = req_ie + ind->assoc_req_ie_len;

	// Fill-in the AP information
	if (ind->status_code == 0) {
		struct asr_sta *sta = &asr_hw->sta_table[ind->ap_idx];
		u8 txq_status;

        if (mrole_enable == false) {
			//asr add : disable other sta , disable in mrole case.
			for (index = 0; index < asr_hw->sta_max_num; index++) {
				asr_hw->sta_table[index].valid = false;
			}
		}

		sta->valid = true;
		sta->sta_idx = ind->ap_idx;
		sta->ch_idx = ind->ch_idx;
		sta->vif_idx = ind->vif_idx;
		sta->vlan_idx = sta->vif_idx;
		sta->qos = ind->qos;
		sta->acm = ind->acm;
		sta->ps.active = false;
		sta->aid = ind->aid;
#ifdef CONFIG_ASR595X
		sta->band = ind->chan.band;
		//sta->width = ind->chan.width;
		sta->center_freq = ind->chan.prim20_freq;
		sta->center_freq1 = ind->chan.center1_freq;
		sta->center_freq2 = ind->chan.center2_freq;
#else
		sta->band = ind->band;
		sta->width = ind->width;
		sta->center_freq = ind->center_freq;
		sta->center_freq1 = ind->center_freq1;
		sta->center_freq2 = ind->center_freq2;
#endif
		asr_vif->sta.ap = sta;
		asr_vif->generation++;
		// TODO: Get chan def in this case (add params in cfm ??)
		asr_chanctx_link(asr_vif, ind->ch_idx, NULL);

		//#ifdef CFG_ROAMING
		//if(asr_vif->auth_type != NL80211_AUTHTYPE_FT)
		//#endif

		memcpy(sta->mac_addr, ind->bssid.array, ETH_ALEN);
		if (ind->ch_idx == asr_hw->cur_chanctx) {
			txq_status = 0;
		} else {
			txq_status = ASR_TXQ_STOP_CHAN;
		}
		memcpy(sta->ac_param, ind->ac_param, sizeof(sta->ac_param));
		asr_txq_sta_init(asr_hw, sta, txq_status);
		asr_dbgfs_register_rc_stat(asr_hw, sta);

#ifdef ASR_STATS_RATES_TIMER
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		mod_timer(&asr_hw->statsrates_timer, jiffies + msecs_to_jiffies(ASR_STATS_RATES_TIMER_OUT));
#else
		del_timer(&asr_hw->statsrates_timer);
		asr_hw->statsrates_timer.expires = jiffies + msecs_to_jiffies(ASR_STATS_RATES_TIMER_OUT);
		add_timer(&asr_hw->statsrates_timer);
#endif
#endif

		set_bit(ASR_DEV_STA_CONNECTED, &asr_vif->dev_flags);
		clear_bit(ASR_DEV_STA_DHCPEND, &asr_vif->dev_flags);
	} else {
		clear_bit(ASR_DEV_STA_CONNECTED, &asr_vif->dev_flags);
		clear_bit(ASR_DEV_STA_DHCPEND, &asr_vif->dev_flags);

        if (mrole_enable == false) {
			//disable other sta
			for (index = 0; index < asr_hw->sta_max_num; index++) {
				asr_hw->sta_table[index].valid = false;;
			}
		}
		asr_vif->sta.ap = NULL;

		memset(asr_vif->ssid, 0, IEEE80211_MAX_SSID_LEN);
		asr_vif->ssid_len = 0;
	}

	if (!ind->roamed

#ifdef CONFIG_SME
	    && (ind->status_code)	//when connect successful(status_code==0), no need to call cfg80211_connect_result twice bcz cfg80211_send_rx_assoc already done.
#endif
	    )
		cfg80211_connect_result(dev, (const u8 *)ind->bssid.array,
					req_ie, ind->assoc_req_ie_len, rsp_ie,
					ind->assoc_rsp_ie_len, ind->status_code, GFP_ATOMIC);

	if (ind->status_code == 0) {
		netif_tx_start_all_queues(dev);
		netif_carrier_on(dev);
	} else {
		netif_tx_stop_all_queues(dev);
		netif_carrier_off(dev);
	}

	return 0;
}

static int asr_local_rx_deauth(struct asr_hw *asr_hw, struct asr_vif *asr_vif, struct asr_sta *sta, u16 reason_code)
{
	struct ieee80211_mgmt deauth_frame;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	struct wireless_dev *wdev;
#endif

	if (asr_hw == NULL || asr_vif == NULL) {
		return -1;
	}

	memset((char *)&deauth_frame, 0, sizeof(struct ieee80211_mgmt));
	deauth_frame.frame_control = 0xC0;
	deauth_frame.duration = 0;

	memcpy(deauth_frame.sa, sta->mac_addr, ETH_ALEN);

	if (ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_STATION) {
		memcpy(deauth_frame.bssid, deauth_frame.sa, ETH_ALEN);
	} else if (ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_AP) {
		memcpy(deauth_frame.bssid, asr_vif->ndev->dev_addr, ETH_ALEN);
	} else {
		//todo
		memcpy(deauth_frame.bssid, deauth_frame.sa, ETH_ALEN);
	}
	memcpy(deauth_frame.da, asr_vif->ndev->dev_addr, ETH_ALEN);
	deauth_frame.u.deauth.reason_code = reason_code;


#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 10, 0)
	dev_info(asr_hw->dev, "%s: sta=%d mac=%pM deauth reason=%d \n",
		__func__, sta->sta_idx, &sta->mac_addr,deauth_frame.u.deauth.reason_code);
#else
	dev_info(asr_hw->dev, "%s: sta=%d mac=%pM %s deauth reason=%d \n",
		__func__, sta->sta_idx, &sta->mac_addr,asr_vif->auth_type == NL80211_AUTHTYPE_SAE ? "SAE" : "",
		deauth_frame.u.deauth.reason_code);
#endif

	if (ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_AP) {
		#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
		cfg80211_rx_mgmt(&asr_vif->wdev, 0, -110, (u8 *)&deauth_frame, sizeof(struct ieee80211_mgmt), 0);
		#elif LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
		cfg80211_rx_mgmt(&asr_vif->wdev, 0, -110, (u8 *)&deauth_frame, sizeof(struct ieee80211_mgmt), GFP_ATOMIC);
		#elif LINUX_VERSION_CODE > KERNEL_VERSION(3, 2, 0)
		cfg80211_rx_mgmt(asr_vif->wdev.netdev, 0, -110, (u8 *)&deauth_frame, sizeof(struct ieee80211_mgmt), GFP_ATOMIC);
		#else
		cfg80211_rx_mgmt(asr_vif->wdev.netdev, 0, (u8 *)&deauth_frame, sizeof(struct ieee80211_mgmt), GFP_ATOMIC);
		#endif

	} else if (ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_STATION) {
		#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
		wdev = asr_vif->ndev->ieee80211_ptr;
		mutex_lock(&wdev->mtx);
		__acquire(wdev->mtx);
		cfg80211_rx_mlme_mgmt(asr_vif->ndev, (u8 *) & deauth_frame, sizeof(struct ieee80211_mgmt));
		__release(wdev->mtx);
		mutex_unlock(&wdev->mtx);
		#else
		cfg80211_send_deauth(asr_vif->ndev, (u8 *) & deauth_frame, sizeof(struct ieee80211_mgmt));
		#endif
	}

	return 0;
}

extern bool asr_xmit_opt;
void rx_deauth_work_func(struct work_struct *work)
{
	struct asr_work_ctx *temp_work = container_of(work, struct asr_work_ctx, real_work);

	asr_local_rx_deauth(temp_work->asr_hw, temp_work->asr_vif, temp_work->sta, temp_work->parm1);
}

int asr_rx_sm_disconnect_ind(struct asr_hw *asr_hw, struct asr_cmd *cmd, struct ipc_e2a_msg *msg)	// used by drv_sme ? todolalala
{
	struct sm_disconnect_ind *ind = (struct sm_disconnect_ind *)msg->param;
	struct asr_vif *asr_vif = asr_hw->vif_table[ind->vif_idx];
	struct net_device *dev = NULL;
#ifdef CONFIG_ASR_SDIO
    struct asr_vif *asr_vif_tmp = NULL;
#endif

	ASR_DBG(ASR_FN_ENTRY_STR);

	clear_bit(ASR_DEV_STA_DISCONNECTING, &asr_vif->dev_flags);
	clear_bit(ASR_DEV_STA_CONNECTING, &asr_vif->dev_flags);
	clear_bit(ASR_DEV_STA_DHCPEND, &asr_vif->dev_flags);

	if (asr_vif == NULL) {
		return 0;
	}

	dev = asr_vif->ndev;
	if (dev == NULL) {
		return 0;
	}

	dev_info(asr_hw->dev, "%s:reason_code is %x,dev_flags=0x%lX, up=%d,ft=%d, non-ft roam=%d \n", __func__, ind->reason_code, asr_vif->dev_flags,asr_vif->up,ind->ft_over_air,ind->normal_roam);

	//memset(asr_vif->ssid, 0, IEEE80211_MAX_SSID_LEN);
	//asr_vif->ssid_len = 0;

	/* if vif is not up, asr_close has already been called */
	if (asr_vif->up) {
		if (!ind->ft_over_ds && !ind->ft_over_air && !ind->normal_roam) {
	                memset(asr_vif->ssid, 0, IEEE80211_MAX_SSID_LEN);
	                asr_vif->ssid_len = 0;
#ifdef CONFIG_SME
			if (test_and_clear_bit(ASR_DEV_STA_CONNECTED, &asr_vif->dev_flags)) {
				//asr_local_rx_deauth(asr_hw, asr_vif, ind->reason_code);
				asr_hw->rx_deauth_work.asr_hw = asr_hw;
				asr_hw->rx_deauth_work.asr_vif = asr_vif;
				asr_hw->rx_deauth_work.sta = asr_vif->sta.ap;
				asr_hw->rx_deauth_work.parm1 = ind->reason_code;
				schedule_work(&asr_hw->rx_deauth_work.real_work);
			}
#else
			clear_bit(ASR_DEV_STA_CONNECTED, &asr_vif->dev_flags);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
			cfg80211_disconnected(dev, ind->reason_code, NULL, 0, false, GFP_ATOMIC);
#else
			cfg80211_disconnected(dev, ind->reason_code, NULL, 0, GFP_ATOMIC);
#endif

#endif  // !CONFIG_SME

#ifdef CONFIG_ASR_SDIO

			if (asr_xmit_opt) {

				spin_lock_bh(&asr_hw->tx_lock);
			    asr_drop_tx_vif_skb(asr_hw,asr_vif);
				asr_vif->tx_skb_cnt = 0;
				spin_unlock_bh(&asr_hw->tx_lock);

			} else {

				spin_lock_bh(&asr_hw->tx_agg_env_lock);

				if (asr_hw->vif_started > 1) {
					asr_tx_agg_buf_mask_vif(asr_hw,asr_vif);
				} else {
					asr_tx_agg_buf_reset(asr_hw);
					list_for_each_entry(asr_vif_tmp,&asr_hw->vifs,list){
	                    asr_vif_tmp->txring_bytes = 0;
			        }
			    }

				spin_unlock_bh(&asr_hw->tx_agg_env_lock);
			}
#endif
			//other code for del key
		}


		netif_tx_stop_all_queues(dev);
		netif_carrier_off(dev);
	}

	if (asr_vif->sta.ap) {
		asr_txq_sta_deinit(asr_hw, asr_vif->sta.ap);

#ifdef CONFIG_ASR_SDIO
	if (asr_xmit_opt)
		asr_tx_skb_sta_deinit(asr_hw, asr_vif->sta.ap);
#endif

		asr_dbgfs_unregister_rc_stat(asr_hw, asr_vif->sta.ap);
		asr_vif->sta.ap->valid = false;
		asr_vif->sta.ap = NULL;
		asr_vif->generation++;
	} else
		dev_info(asr_hw->dev, "[%s %d] asr_sta is null\n", __func__, __LINE__);

	asr_chanctx_unlink(asr_vif);


#ifdef ASR_STATS_RATES_TIMER
	del_timer_sync(&asr_hw->statsrates_timer);
	asr_hw->stats.txrx_rates[0].tx_rates = 0;
	asr_hw->stats.txrx_rates[0].rx_rates = 0;
#endif

	return 0;
}

static inline int asr_rx_sm_auth_ind(struct asr_hw *asr_hw, struct asr_cmd *cmd, struct ipc_e2a_msg *msg)	// todo lalala
{
	ASR_DBG(ASR_FN_ENTRY_STR);

	return 0;
}

static inline int asr_rx_sm_assoc_ind(struct asr_hw *asr_hw, struct asr_cmd *cmd, struct ipc_e2a_msg *msg)	// todo lalala
{
	ASR_DBG(ASR_FN_ENTRY_STR);

	return 0;
}

static inline int asr_rx_hif_sdio_info_ind(struct asr_hw *asr_hw, struct asr_cmd *cmd, struct ipc_e2a_msg *msg)
{
	struct mm_hif_sdio_info_ind *ind = (struct mm_hif_sdio_info_ind *)msg->param;

	ASR_DBG(ASR_FN_ENTRY_STR);

	asr_hw->hif_sdio_info_ind = *ind;

	dev_err(asr_hw->dev, "%s: %d %d %d.\n", __func__, ind->flag, ind->host_tx_data_cur_idx, ind->rx_data_cur_idx);

	return 0;
}

static inline int asr_sta_link_loss_ind(struct asr_hw *asr_hw, struct asr_cmd *cmd, struct ipc_e2a_msg *msg)
{
	struct mm_sta_link_loss_ind *ind = (struct mm_sta_link_loss_ind *)msg->param;
	struct asr_vif *asr_vif = NULL;

	ASR_DBG(ASR_FN_ENTRY_STR);

	dev_info(asr_hw->dev, "%s: %d,%d.\n", __func__, ind->vif_idx, ind->staid);

	if (ind->vif_idx >= NX_VIRT_DEV_MAX || ind->staid >= NX_REMOTE_STA_MAX) {
		return 0;
	}

	asr_vif = asr_hw->vif_table[ind->vif_idx];
	if (!asr_vif) {
		return 0;
	}

	if (asr_vif->up && asr_hw->sta_table[ind->staid].valid) {
		//asr_local_rx_deauth(asr_hw, asr_vif, ind->reason_code);
		asr_hw->sta_deauth_work[ind->staid].asr_hw = asr_hw;
		asr_hw->sta_deauth_work[ind->staid].asr_vif = asr_vif;
		asr_hw->sta_deauth_work[ind->staid].sta = &asr_hw->sta_table[ind->staid];
		asr_hw->sta_deauth_work[ind->staid].parm1 = 1;
		schedule_work(&asr_hw->sta_deauth_work[ind->staid].real_work);
	}

	return 0;
}

/***************************************************************************
 * Messages from APM task
 **************************************************************************/

/***************************************************************************
 * Messages from DEBUG task
 **************************************************************************/
volatile uint8_t dbg_err_ind = 0;
extern int sss0;
extern int sss0_pre;
extern int sss1;
extern int sss2;
extern int sss3;
extern int sss4;
extern int sss5;
extern int last_tx_time;

static inline int asr_rx_dbg_error_ind(struct asr_hw *asr_hw, struct asr_cmd *cmd, struct ipc_e2a_msg *msg)
{
	//int cur_time = ktime_to_us(ktime_get());
	dbg_err_ind = 1;
	ASR_DBG(ASR_FN_ENTRY_STR);

	//if(ktime_to_us(ktime_get())-last_tx_time < 10000)
	//dev_err(asr_hw->dev, "asr_err_ind txtime %d %d\n",last_tx_time,cur_time);

	dev_err(asr_hw->dev, "asr_err_ind %d %d %d %d %d %d %d\n", sss0_pre, sss0, sss1, sss2, sss3, sss4, sss5);

	asr_error_ind(asr_hw);

	return 0;
}

static msg_cb_fct mm_hdlrs[MSG_I(MM_MAX)] = {
	[MSG_I(MM_CHANNEL_SWITCH_IND)] = asr_rx_chan_switch_ind,
	[MSG_I(MM_CHANNEL_PRE_SWITCH_IND)] = asr_rx_chan_pre_switch_ind,
	[MSG_I(MM_REMAIN_ON_CHANNEL_EXP_IND)] = asr_rx_remain_on_channel_exp_ind,
    [MSG_I(MM_PS_CHANGE_IND)]          = asr_rx_ps_change_ind,
    [MSG_I(MM_TRAFFIC_REQ_IND)]        = asr_rx_traffic_req_ind,
    [MSG_I(MM_P2P_VIF_PS_CHANGE_IND)]  = asr_rx_p2p_vif_ps_change_ind,
	[MSG_I(MM_CSA_COUNTER_IND)] = asr_rx_csa_counter_ind,
	[MSG_I(MM_CSA_FINISH_IND)] = asr_rx_csa_finish_ind,
	[MSG_I(MM_CSA_TRAFFIC_IND)] = asr_rx_csa_traffic_ind,
	[MSG_I(MM_CHANNEL_SURVEY_IND)] = asr_rx_channel_survey_ind,
    [MSG_I(MM_P2P_NOA_UPD_IND)] = asr_rx_p2p_noa_upd_ind,
	[MSG_I(MM_RSSI_STATUS_IND)] = asr_rx_rssi_status_ind,
	#ifdef CONFIG_ASR595X
	[MSG_I(MM_PKTLOSS_IND)]     = asr_rx_pktloss_notify_ind,
	#endif
#ifdef CONFIG_ASR_SDIO
	[MSG_I(MM_HIF_SDIO_INFO_IND)] = asr_rx_hif_sdio_info_ind,
#endif
#ifdef CONFIG_TWT
	[MSG_I(MM_TWT_TRAFFIC_IND)] = asr_rx_twt_traffic_ind,
#endif
	[MSG_I(MM_STA_LINK_LOSS_IND)]     = asr_sta_link_loss_ind,
};

static msg_cb_fct scan_hdlrs[MSG_I(SCANU_MAX)] = {
	[MSG_I(SCANU_START_CFM)] = asr_rx_scanu_start_cfm,
	[MSG_I(SCANU_RESULT_IND)] = asr_rx_scanu_result_ind,
};

static msg_cb_fct me_hdlrs[MSG_I(ME_MAX)] = {
	[MSG_I(ME_TKIP_MIC_FAILURE_IND)] = asr_rx_me_tkip_mic_failure_ind,
	[MSG_I(ME_TX_CREDITS_UPDATE_IND)] = asr_rx_me_tx_credits_update_ind,
};

static msg_cb_fct sm_hdlrs[MSG_I(SM_MAX)] = {
	[MSG_I(SM_CONNECT_IND)] = asr_rx_sm_connect_ind,
	[MSG_I(SM_DISCONNECT_IND)] = asr_rx_sm_disconnect_ind,
#ifdef CONFIG_SME
	[MSG_I(SM_AUTH_IND)] = asr_rx_sm_auth_ind,
	[MSG_I(SM_ASSOC_IND)] = asr_rx_sm_assoc_ind,
#endif

};

static msg_cb_fct apm_hdlrs[MSG_I(APM_MAX)] = {
};

static msg_cb_fct dbg_hdlrs[MSG_I(DBG_MAX)] = {
	[MSG_I(DBG_ERROR_IND)] = asr_rx_dbg_error_ind,
};

static msg_cb_fct *msg_hdlrs[] = {
	[TASK_MM] = mm_hdlrs,
	[TASK_DBG] = dbg_hdlrs,
	[TASK_SCAN] = NULL,
	[TASK_TDLS] = NULL,
	[TASK_SCANU] = scan_hdlrs,
	[TASK_ME] = me_hdlrs,
	[TASK_SM] = sm_hdlrs,
	[TASK_APM] = apm_hdlrs,
};

static bool asr_msg_valid(struct ipc_e2a_msg *msg)
{

	if (!msg || MSG_T(msg->id) > TASK_APM || !msg_hdlrs[MSG_T(msg->id)]) {

		return false;
	}

	switch (MSG_T(msg->id)) {
	case TASK_MM:
		return (MSG_I(msg->id) < MSG_I(MM_MAX));
	case TASK_DBG:
		return (MSG_I(msg->id) < MSG_I(DBG_MAX));
	case TASK_SCANU:
		return (MSG_I(msg->id) < MSG_I(SCANU_MAX));
	case TASK_ME:
		return (MSG_I(msg->id) < MSG_I(ME_MAX));
	case TASK_SM:
		return (MSG_I(msg->id) < MSG_I(SM_MAX));
	case TASK_APM:
		return (MSG_I(msg->id) < MSG_I(APM_MAX));
	default:
		break;
	};

	return false;
}

/**
 *
 */
#ifdef CONFIG_TWT
bool is_log_ignore_msg(struct ipc_e2a_msg *msg)
{
	if (((MSG_T(msg->id) == TASK_MM) && ((MSG_I(msg->id) == MM_TWT_TRAFFIC_IND)
                                         ||  (MSG_I(msg->id) == MM_P2P_VIF_PS_CHANGE_IND)
                                         ||  (MSG_I(msg->id) == MM_CHANNEL_PRE_SWITCH_IND)
                                         ||  (MSG_I(msg->id) == MM_CHANNEL_SWITCH_IND)
										 ||  (MSG_I(msg->id) == MM_CHANNEL_SURVEY_IND)
                                         ||  (MSG_I(msg->id) == MM_P2P_NOA_UPD_IND)
                                         ||  (MSG_I(msg->id) == MM_PS_CHANGE_IND)
                                         ||  (MSG_I(msg->id) == MM_REMAIN_ON_CHANNEL_EXP_IND)
                                         ||  (MSG_I(msg->id) == MM_REMAIN_ON_CHANNEL_CFM)
                                         ||  (MSG_I(msg->id) == MM_TRAFFIC_REQ_IND)))
	     || ((MSG_T(msg->id) == TASK_ME) && (MSG_I(msg->id) == MSG_I(ME_TRAFFIC_IND_CFM)))) {
	    return true;
	} else
	    return false;
}
#endif

void asr_rx_handle_msg(struct asr_hw *asr_hw, struct ipc_e2a_msg *msg)
{
	if ((asr_hw == NULL) || (msg == NULL)) {
		dev_err(asr_hw->dev, "[%s %d]asr_hw=%p,msg=%p\n", __func__, __LINE__, asr_hw, msg);
		return;
	}

	if (!asr_msg_valid(msg)) {
		dev_err(asr_hw->dev, "%s: unvalid cmd T:%d I:%d\n", __func__, MSG_T(msg->id), MSG_I(msg->id));
		return;
	}
#ifdef CONFIG_TWT
	if (!is_log_ignore_msg(msg))
#endif
	{
		dev_info(asr_hw->dev, "%s %-24s T:%d - I:%d %d->%d l:%d\n", __func__,
			 ASR_ID2STR(msg->id), MSG_T(msg->id), MSG_I(msg->id), msg->dummy_src_id, msg->dummy_dest_id,
			 msg->param_len);
	}

	asr_hw->cmd_mgr.msgind(&asr_hw->cmd_mgr, msg, msg_hdlrs[MSG_T(msg->id)][MSG_I(msg->id)]);
}
