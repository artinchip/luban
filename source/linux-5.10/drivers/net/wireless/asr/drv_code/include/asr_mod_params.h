/**
 ******************************************************************************
 *
 * @file asr_mod_params.h
 *
 * @brief Declaration of module parameters
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */

#ifndef _ASR_MOD_PARAM_H_
#define _ASR_MOD_PARAM_H_

#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
#else
#define IEEE80211_VHT_CAP_BEAMFORMEE_STS_SHIFT          13
#define IEEE80211_VHT_CAP_SOUNDING_DIMENSIONS_SHIFT     16
#endif

// defaut ate at cmd len is 50 Bytes
#define ATE_AT_CMD_LEN          512
struct asr_mod_params {
	bool ht_on;
	int mcs_map;
	bool ldpc_on;
	int phy_cfg;
	int uapsd_timeout;
	bool ap_uapsd_on;
	bool sgi;
	bool sgi80;
	bool use_2040;
	bool use_80;
	bool custregd;
	int nss;
	unsigned int roc_dur_max;
	int listen_itv;
	bool listen_bcmc;
	int lp_clk_ppm;
	bool ps_on;
	int tx_lft;
	int amsdu_maxnb;
	int uapsd_queues;
	bool ant_div;
#ifdef CONFIG_ASR_NAPI
    bool napi_on;
#ifdef CONFIG_ASR_GRO
    bool gro_on;
#endif
#endif
#ifdef CONFIG_ASR595X
	bool he_on;
	int he_mcs_map;
	bool he_ul_on;
	bool stbc_on;
	bool bfmee;
	bool twt_request;
#endif
	int tcp_ack_num;
	u8 ate_at_cmd[ATE_AT_CMD_LEN];
#ifdef CONFIG_ASR_PM
	char pm_cmd[20];
#ifdef CONFIG_GPIO_WAKEUP_MOD
	int pm_out_gpio;
#endif
#ifdef CONFIG_GPIO_WAKEUP_HOST
	int pm_in_gpio;
#endif
#endif
#ifdef CONFIG_ASR_USB_PM
	char usb_pm_cmd[20];
#endif
};

extern struct asr_mod_params asr_module_params;

struct asr_hw;
int asr_handle_dynparams(struct asr_hw *asr_hw, struct wiphy *wiphy);
void asr_enable_wapi(struct asr_hw *asr_hw);
void asr_enable_mfp(struct asr_hw *asr_hw);

#endif /* _ASR_MOD_PARAM_H_ */
