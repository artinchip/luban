/**
 ****************************************************************************************
 *
 * @file asr_msg_tx.h
 *
 * @brief TX function declarations
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef _ASR_MSG_TX_H_
#define _ASR_MSG_TX_H_

#include "asr_defs.h"

/*
 * c.f LMAC/src/co/mac/mac_frame.h
 */
#define MAC_RSNIE_CIPHER_WEP40    0x00
#define MAC_RSNIE_CIPHER_TKIP     0x01
#define MAC_RSNIE_CIPHER_CCMP     0x02
#define MAC_RSNIE_CIPHER_WEP104   0x03
#define MAC_RSNIE_CIPHER_SMS4     0x04
#define MAC_RSNIE_CIPHER_AES_CMAC 0x05
#define MAC_RSNIE_CIPHER_GCMP_128 0x06
#define MAC_RSNIE_CIPHER_GCMP_256 0x07
#define MAC_RSNIE_CIPHER_CCMP_256 0x08
#define MAC_RSNIE_CIPHER_BIP_GMAC_128 0x09
#define MAC_RSNIE_CIPHER_BIP_GMAC_256 0x0a
#define MAC_RSNIE_CIPHER_BIP_CMAC_256 0x0b


enum asr_chan_types {
	PHY_CHNL_BW_20,
	PHY_CHNL_BW_40,
	PHY_CHNL_BW_80,
	PHY_CHNL_BW_160,
	PHY_CHNL_BW_80P80,
	PHY_CHNL_BW_OTHER,
};

enum {
	WLAN_RX_BEACON,		/* receive beacon packet */
	WLAN_RX_PROBE_REQ,	/* receive probe request packet */
	WLAN_RX_PROBE_RES,	/* receive probe response packet */
	WLAN_RX_ACTION,		/* receive action packet */
	WLAN_RX_MANAGEMENT,	/* receive ALL management packet */
	WLAN_RX_DATA,		/* receive ALL data packet */
	WLAN_RX_MCAST_DATA,	/* receive ALL multicast and broadcast packet */
	WLAN_RX_MONITOR_DEFAULT,	/* receive probe req/rsp/beacon, my unicast/bcast data packet */
	WLAN_RX_SMART_CONFIG,	/* ready for smartconfig packet broadcast */
	WLAN_RX_SMART_CONFIG_MC,	/* ready for smartconfig packet */
	WLAN_RX_ALL,		/* receive ALL 802.11 packet */
};

int asr_send_reset(struct asr_hw *asr_hw);
int asr_send_start(struct asr_hw *asr_hw);
int asr_send_version_req(struct asr_hw *asr_hw, struct mm_version_cfm *cfm);
int asr_send_mm_get_info(struct asr_hw *asr_hw, struct mm_get_info_cfm *cfm);
int asr_send_add_if(struct asr_hw *asr_hw, const unsigned char *mac,
		    enum nl80211_iftype iftype, bool p2p, struct mm_add_if_cfm *cfm);
int asr_send_remove_if(struct asr_hw *asr_hw, u8 vif_index);
int asr_send_set_channel(struct asr_hw *asr_hw, int phy_idx, struct mm_set_channel_cfm *cfm);
int asr_send_key_add(struct asr_hw *asr_hw, u8 vif_idx, u8 sta_idx,
		     bool pairwise, u8 * key, u8 key_len, u8 key_idx, u8 cipher_suite, struct mm_key_add_cfm *cfm);
int asr_send_key_del(struct asr_hw *asr_hw, u8 hw_key_idx);
int asr_send_set_ps_mode(struct asr_hw *asr_hw, bool ps_on);

int asr_send_bcn_change(struct asr_hw *asr_hw, u8 vif_idx, u8 * bcn_addr,
			u16 bcn_len, u16 tim_oft, u16 tim_len, u16 * csa_oft);
int asr_send_tim_update(struct asr_hw *asr_hw, u8 vif_idx, u16 aid, u8 tx_status);
int asr_send_roc(struct asr_hw *asr_hw, struct asr_vif *vif, struct ieee80211_channel *chan, unsigned int duration);
int asr_send_cancel_roc(struct asr_hw *asr_hw);
int asr_send_set_power(struct asr_hw *asr_hw, u8 vif_idx, s8 pwr, struct mm_set_power_cfm *cfm);
int asr_send_set_edca(struct asr_hw *asr_hw, u8 hw_queue, u32 param, bool uapsd, u8 inst_nbr);
#ifdef CONFIG_ASR_P2P_DEBUGFS
int asr_send_p2p_oppps_req(struct asr_hw *asr_hw, struct asr_vif *asr_vif,
                            u8 ctw, struct mm_set_p2p_oppps_cfm *cfm);
int asr_send_p2p_noa_req(struct asr_hw *asr_hw, struct asr_vif *asr_vif,
                          int count, int interval, int duration, bool dyn_noa,
                          struct mm_set_p2p_noa_cfm *cfm);
#endif
int asr_send_me_config_req(struct asr_hw *asr_hw);
int asr_send_me_chan_config_req(struct asr_hw *asr_hw);
int asr_send_me_set_control_port_req(struct asr_hw *asr_hw, bool opened, u8 sta_idx);
int asr_send_me_sta_add(struct asr_hw *asr_hw,
			struct station_parameters *params, const u8 * mac, u8 inst_nbr, struct me_sta_add_cfm *cfm);
int asr_send_me_sta_del(struct asr_hw *asr_hw, u8 sta_idx);
int asr_send_me_traffic_ind(struct asr_hw *asr_hw, u8 sta_idx, bool uapsd, u8 tx_status);
int asr_send_me_rc_stats(struct asr_hw *asr_hw, u8 sta_idx, struct me_rc_stats_cfm *cfm);
int asr_send_me_rc_set_rate(struct asr_hw *asr_hw, u8 sta_idx, u16 rate_idx);

int asr_send_me_host_dbg_cmd(struct asr_hw *asr_hw, unsigned int host_dbg_cmd, unsigned int reg, unsigned int value);

#ifdef CONFIG_SME
int asr_send_sm_auth_req(struct asr_hw *asr_hw,
			 struct asr_vif *asr_vif, struct cfg80211_auth_request *auth_req, struct sm_auth_cfm *cfm);

int asr_send_sm_assoc_req(struct asr_hw *asr_hw,
			  struct asr_vif *asr_vif, struct cfg80211_assoc_request *assoc_req, struct sm_assoc_cfm *cfm);
#endif
int asr_send_sm_connect_req(struct asr_hw *asr_hw,
			    struct asr_vif *asr_vif, struct cfg80211_connect_params *sme, struct sm_connect_cfm *cfm);
int asr_send_sm_disconnect_req(struct asr_hw *asr_hw, struct asr_vif *asr_vif, u16 reason);

int asr_send_apm_start_req(struct asr_hw *asr_hw, struct asr_vif *vif,
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 4, 0)
			   struct cfg80211_ap_settings *settings,
#else
			   struct beacon_parameters *info,
#endif
			   struct apm_start_cfm *cfm, struct asr_dma_elem *elem);
int asr_send_apm_stop_req(struct asr_hw *asr_hw, struct asr_vif *vif);

int asr_send_scanu_req(struct asr_hw *asr_hw, struct asr_vif *asr_vif, struct cfg80211_scan_request *param);
int asr_send_apm_start_cac_req(struct asr_hw *asr_hw, struct asr_vif *vif,
			       struct cfg80211_chan_def *chandef, struct apm_start_cac_cfm *cfm);
int asr_send_apm_stop_cac_req(struct asr_hw *asr_hw, struct asr_vif *vif);

/* Debug messages */
int asr_send_dbg_trigger_req(struct asr_hw *asr_hw, char *msg, int len);
int asr_send_dbg_mem_read_req(struct asr_hw *asr_hw, u32 mem_addr, struct dbg_mem_read_cfm *cfm);
int asr_send_dbg_mem_write_req(struct asr_hw *asr_hw, u32 mem_addr, u32 mem_data);
int asr_send_dbg_set_mod_filter_req(struct asr_hw *asr_hw, u32 filter);
int asr_send_dbg_set_sev_filter_req(struct asr_hw *asr_hw, u32 filter);
int asr_send_dbg_get_sys_stat_req(struct asr_hw *asr_hw, struct dbg_get_sys_stat_cfm *cfm);
int asr_send_cfg_rssi_req(struct asr_hw *asr_hw, u8 vif_index, int rssi_thold, u32 rssi_hyst);
#ifdef CFG_SNIFFER_SUPPORT
int asr_send_set_monitor_channel(struct asr_hw *asr_hw, struct cfg80211_chan_def *chandef);
int asr_send_set_idle(struct asr_hw *asr_hw, int idle);
int asr_send_set_filter(struct asr_hw *asr_hw, uint32_t filter);
#endif

int asr_send_fw_softversion_req(struct asr_hw *asr_hw, struct mm_fw_softversion_cfm *cfm);
int asr_send_fw_macaddr_req(struct asr_hw *asr_hw, struct mm_fw_macaddr_cfm *cfm);
int asr_send_set_fw_macaddr_req(struct asr_hw *asr_hw, const uint8_t * macaddr);
int asr_send_hif_sdio_info_req(struct asr_hw *asr_hw);

int asr_send_set_tx_pwr_rate(struct asr_hw *asr_hw, struct mm_set_tx_pwr_rate_cfm *cfm,
			     struct mm_set_tx_pwr_rate_req *tx_pwr);

#ifdef CONFIG_TWT
int asr_send_itwt_config(struct asr_hw *asr_hw, wifi_twt_config_t * wifi_twt_param);
int asr_send_itwt_del(struct asr_hw *asr_hw, uint8_t flow_id);
#endif

int asr_send_set_cca(struct asr_hw *asr_hw, struct mm_set_get_cca_cfm *cfm,
			     struct mm_set_get_cca_req *cca_config);

int asr_send_get_rssi_req(struct asr_hw *asr_hw, u8 staid, s8 *rssi);
int asr_send_upload_fram_req(struct asr_hw *asr_hw, u8 vif_idx, u16 fram_type, u8 enable);
int asr_send_fram_appie_req(struct asr_hw *asr_hw, u8 vif_idx, u16 fram_type, u8 *ie, u8 ie_len);

int asr_send_efuse_txpwr_req(struct asr_hw *asr_hw, uint8_t * txpwr, uint8_t * txevm, uint8_t *freq_err, bool iswrite, uint8_t *index);

#endif /* _ASR_MSG_TX_H_ */
