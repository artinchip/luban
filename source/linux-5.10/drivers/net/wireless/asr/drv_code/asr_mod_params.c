/**
******************************************************************************
*
* @file asr_mod_params.c
*
* @brief Set configuration according to modules parameters
*
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
*
******************************************************************************
*/
#include <linux/module.h>

#include "asr_defs.h"
#include "asr_tx.h"
#include "hal_desc.h"
#include "asr_cfgfile.h"

#define COMMON_PARAM(name, default_fullmac)    \
    .name = default_fullmac,

struct asr_mod_params asr_module_params = {
	/* common parameters */
	COMMON_PARAM(ht_on, true)
#ifdef CONFIG_ASR595X
	COMMON_PARAM(mcs_map, IEEE80211_VHT_MCS_SUPPORT_0_9)
#ifdef BASS_SUPPORT
	COMMON_PARAM(ldpc_on, false)
#else
	COMMON_PARAM(ldpc_on, true)
#endif
#else
	COMMON_PARAM(mcs_map, IEEE80211_VHT_MCS_SUPPORT_0_7)
	COMMON_PARAM(ldpc_on, false)
#endif
	COMMON_PARAM(phy_cfg, 0)
	COMMON_PARAM(uapsd_timeout, 300)
	COMMON_PARAM(ap_uapsd_on, true)
	COMMON_PARAM(sgi, true)
	COMMON_PARAM(sgi80, false)
#ifdef BASS_SUPPORT
	COMMON_PARAM(use_2040, false)
#else
	COMMON_PARAM(use_2040, true)
#endif
	COMMON_PARAM(use_80, false)
	COMMON_PARAM(custregd, false)
#ifdef CONFIG_ASR5531
	COMMON_PARAM(nss, 2)
#else
	COMMON_PARAM(nss, 1)
#endif
	COMMON_PARAM(roc_dur_max, 500)
	COMMON_PARAM(listen_itv, 0)
	COMMON_PARAM(listen_bcmc, true)
	COMMON_PARAM(lp_clk_ppm, 20)
	COMMON_PARAM(ps_on, false)
	COMMON_PARAM(tx_lft, ASR_TX_LIFETIME_MS)
	COMMON_PARAM(amsdu_maxnb, NX_TX_PAYLOAD_MAX)
    // By default, only enable UAPSD for Voice queue (see IEEE80211_DEFAULT_UAPSD_QUEUE comment)
	COMMON_PARAM(uapsd_queues, IEEE80211_WMM_IE_STA_QOSINFO_AC_VO)
#ifdef CONFIG_ASR5531
    /* 2X2 only parameter */
	COMMON_PARAM(ant_div, false)
#endif
#ifdef CONFIG_ASR_NAPI
	COMMON_PARAM(napi_on, true)
#ifdef CONFIG_ASR_GRO
	COMMON_PARAM(gro_on, true)
#endif
#endif
#ifdef CONFIG_ASR595X
	COMMON_PARAM(he_on, true)
	COMMON_PARAM(he_mcs_map, IEEE80211_HE_MCS_SUPPORT_0_9)
	COMMON_PARAM(he_ul_on, false)
	COMMON_PARAM(stbc_on, false)
	COMMON_PARAM(bfmee, false)
	COMMON_PARAM(twt_request, true)
#endif
    /* default every 6 tcp ack then send just one ack to fw */
	COMMON_PARAM(tcp_ack_num, 6)
	/* default ate at cmd is "get_ate_ver" */
	COMMON_PARAM(ate_at_cmd, "get_ate_ver\n")

#ifdef CONFIG_ASR_PM
	COMMON_PARAM(pm_cmd, "")
#ifdef CONFIG_GPIO_WAKEUP_MOD
	COMMON_PARAM(pm_out_gpio, 17)
#endif /* CONFIG_GPIO_WAKEUP_MOD */
#ifdef CONFIG_GPIO_WAKEUP_HOST
	COMMON_PARAM(pm_in_gpio, 18)
#endif /* CONFIG_GPIO_WAKEUP_HOST */
#endif /* CONFIG_ASR_PM */
#ifdef CONFIG_ASR_USB_PM
	COMMON_PARAM(usb_pm_cmd, "")
#endif
};

module_param_named(ht_on, asr_module_params.ht_on, bool, S_IRUGO);
MODULE_PARM_DESC(ht_on, "Enable HT (Default: 1)");

module_param_named(mcs_map, asr_module_params.mcs_map, int, S_IRUGO);
MODULE_PARM_DESC(mcs_map, "VHT MCS map value  0: MCS0_7, 1: MCS0_8, 2: MCS0_9" " (Default: 0)");

module_param_named(amsdu_maxnb, asr_module_params.amsdu_maxnb, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(amsdu_maxnb, "Maximum number of MSDUs inside an A-MSDU in TX: (Default: NX_TX_PAYLOAD_MAX)");

module_param_named(ps_on, asr_module_params.ps_on, bool, S_IRUGO);
MODULE_PARM_DESC(ps_on, "Enable PowerSaving (Default: 1-Enabled)");

module_param_named(tx_lft, asr_module_params.tx_lft, int, 0644);
MODULE_PARM_DESC(tx_lft, "Tx lifetime (ms) - setting it to 0 disables retries "
		 "(Default: " __stringify(ASR_TX_LIFETIME_MS) ")");

module_param_named(ldpc_on, asr_module_params.ldpc_on, bool, S_IRUGO);
MODULE_PARM_DESC(ldpc_on, "Enable LDPC (Default: 1)");

module_param_named(phycfg, asr_module_params.phy_cfg, int, S_IRUGO);
MODULE_PARM_DESC(phycfg, "0 <= phycfg <= 5 : RF Channel Conf (Default: 2(C0-A1-B2))");

module_param_named(uapsd_timeout, asr_module_params.uapsd_timeout, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(uapsd_timeout, "UAPSD Timer timeout, in ms (Default: 300). If 0, UAPSD is disabled");

module_param_named(uapsd_queues, asr_module_params.uapsd_queues, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(uapsd_queues,
		 "UAPSD Queues, integer value, must be seen as a bitfield\n"
		 "        Bit 0 = VO\n" "        Bit 1 = VI\n"
		 "        Bit 2 = BK\n" "        Bit 3 = BE\n"
		 "     -> uapsd_queues=7 will enable uapsd for VO, VI and BK queues");

module_param_named(ap_uapsd_on, asr_module_params.ap_uapsd_on, bool, S_IRUGO);
MODULE_PARM_DESC(ap_uapsd_on, "Enable UAPSD in AP mode (Default: 1)");

module_param_named(sgi, asr_module_params.sgi, bool, S_IRUGO);
MODULE_PARM_DESC(sgi, "Advertise Short Guard Interval support (Default: 1)");

module_param_named(sgi80, asr_module_params.sgi80, bool, S_IRUGO);
MODULE_PARM_DESC(sgi80, "Advertise Short Guard Interval support for 80MHz (Default: 1)");

module_param_named(use_2040, asr_module_params.use_2040, bool, S_IRUGO);
MODULE_PARM_DESC(use_2040, "Use tweaked 20-40MHz mode (Default: 1)");

module_param_named(use_80, asr_module_params.use_80, bool, S_IRUGO);
MODULE_PARM_DESC(use_80, "Enable 80MHz (Default: 1)");

module_param_named(custregd, asr_module_params.custregd, bool, S_IRUGO);
MODULE_PARM_DESC(custregd, "Use permissive custom regulatory rules (for testing ONLY) (Default: 0)");

module_param_named(nss, asr_module_params.nss, int, S_IRUGO);
MODULE_PARM_DESC(nss, "1 <= nss <= 2 : Supported number of Spatial Streams (Default: 1)");

module_param_named(roc_dur_max, asr_module_params.roc_dur_max, int, S_IRUGO);
MODULE_PARM_DESC(roc_dur_max, "Maximum Remain on Channel duration");

module_param_named(listen_itv, asr_module_params.listen_itv, int, S_IRUGO);
MODULE_PARM_DESC(listen_itv, "Maximum listen interval");

module_param_named(listen_bcmc, asr_module_params.listen_bcmc, bool, S_IRUGO);
MODULE_PARM_DESC(listen_bcmc, "Wait for BC/MC traffic following DTIM beacon");

module_param_named(lp_clk_ppm, asr_module_params.lp_clk_ppm, int, S_IRUGO);
MODULE_PARM_DESC(lp_clk_ppm, "Low Power Clock accuracy of the local device");

#ifdef CONFIG_ASR_NAPI
module_param_named(napi_on, asr_module_params.napi_on, bool, S_IRUGO);
MODULE_PARM_DESC(napi_on, "Enable linux napi recv(Default: 1)");
#ifdef CONFIG_ASR_GRO
module_param_named(gro_on, asr_module_params.gro_on, bool, S_IRUGO);
MODULE_PARM_DESC(gro_on, "Enable linux napi GRO(generic recv offload)(Default: 1)");
#endif
#endif
#ifdef CONFIG_ASR595X
module_param_named(he_on, asr_module_params.he_on, bool, S_IRUGO);
MODULE_PARM_DESC(he_on, "Enable HE (Default: 1)");

module_param_named(he_mcs_map, asr_module_params.he_mcs_map, int, S_IRUGO);
MODULE_PARM_DESC(he_mcs_map, "HE MCS map value 0:MCS0_7, 1:MCS0_8, 2:MCS0_9 (Default:2)");

module_param_named(he_ul_on, asr_module_params.he_ul_on, bool, S_IRUGO);
MODULE_PARM_DESC(he_ul_on, "Enable HE OFDMA UL (Default: 0)");

module_param_named(stbc_on, asr_module_params.stbc_on, bool, S_IRUGO);
MODULE_PARM_DESC(stbc_on, "Enable STBC in RX (Default: 0)");

module_param_named(bfmee, asr_module_params.bfmee, bool, S_IRUGO);
MODULE_PARM_DESC(bfmee, "Enable Beamformee Capability (Default: 1-Enabled)");

module_param_named(twt_request, asr_module_params.twt_request, bool, S_IRUGO);
MODULE_PARM_DESC(twt_request, "Enable TWT request sta (Default: 0)");
#endif
/* default every 6 tcp ack then send just one ack to fw , 0 means disable reduce tcp ack*/
module_param_named(tcp_ack_num, asr_module_params.tcp_ack_num, int, 0644);
MODULE_PARM_DESC(tcp_ack_num, "1 <= phycfg <= 10 : tcp ack cnt (Default: 6)");
/* defaut ate at cmd len 50 Bytes  */
module_param_string(ate_at_cmd, asr_module_params.ate_at_cmd, ATE_AT_CMD_LEN, 0644);
MODULE_PARM_DESC(ate_at_cmd, "send ate cmd from drv (Default: get_ate_ver )");

#ifdef CONFIG_ASR_PM
module_param_string(pm_cmd, asr_module_params.pm_cmd, 20, 0664);
MODULE_PARM_DESC(pm_cmd, "Send PM command");
#ifdef CONFIG_GPIO_WAKEUP_MOD
module_param_named(pm_out_gpio, asr_module_params.pm_out_gpio, int, 0660);
MODULE_PARM_DESC(pm_out_gpio, "Pin code of PM output GPIO (Default:17)");
#endif /* CONFIG_GPIO_WAKEUP_MOD */
#ifdef CONFIG_GPIO_WAKEUP_HOST
module_param_named(pm_in_gpio, asr_module_params.pm_in_gpio, int, 0660);
MODULE_PARM_DESC(pm_in_gpio, "Pin code of PM input GPIO (Default:18)");
#endif /* CONFIG_GPIO_WAKEUP_HOST */
#endif /* CONFIG_ASR_PM */

#ifdef CONFIG_ASR_USB_PM
module_param_string(usb_pm_cmd, asr_module_params.usb_pm_cmd, 20, 0664);
MODULE_PARM_DESC(usb_pm_cmd, "Send USB PM command to driver");
#endif /* CONFIG_ASR_USB_PM */


/* Regulatory rules */
static const struct ieee80211_regdomain asr_regdom = {
	.n_reg_rules = 3,
	.alpha2 = "99",
	.reg_rules = {
		      REG_RULE(2412 - 10, 2472 + 10, 40, 0, 1000, 0),
		      REG_RULE(2484 - 10, 2484 + 10, 20, 0, 1000, 0),
		      REG_RULE(5150 - 10, 5850 + 10, 80, 0, 1000, 0),
		      }
};

/**
 * Do some sanity check
 *
 */
static int asr_check_fw_hw_feature(struct asr_hw *asr_hw, struct wiphy *wiphy)
{
	u32 sys_feat = asr_hw->version_cfm.features;
	u32 phy_feat = asr_hw->version_cfm.version_phy_1;

	if (!(sys_feat & BIT(MM_FEAT_UMAC_BIT))) {
		wiphy_err(wiphy, "Loading softmac firmware with fullmac driver\n");
		return -1;
	}

	if (!(sys_feat & BIT(MM_FEAT_ANT_DIV_BIT))) {
		asr_hw->mod_params->ant_div = false;
	}

	if (!(sys_feat & BIT(MM_FEAT_PS_BIT))) {
		asr_hw->mod_params->ps_on = false;
	}

	/* AMSDU (non)support implies different shared structure definition
	   so insure that fw and drv have consistent compilation option */
	if (sys_feat & BIT(MM_FEAT_AMSDU_BIT)) {
		wiphy_err(wiphy, "AMSDU enabled in firmware but support not compiled in driver\n");
		return -1;
	} else {
	}

	if (!(sys_feat & BIT(MM_FEAT_UAPSD_BIT))) {
		asr_hw->mod_params->uapsd_timeout = 0;
	}

	if (sys_feat & BIT(MM_FEAT_WAPI_BIT)) {
		asr_enable_wapi(asr_hw);
	}

	if (sys_feat & BIT(MM_FEAT_MFP_BIT)) {
		asr_enable_mfp(asr_hw);
	}
#define QUEUE_NAME "Broadcast/Multicast queue "

	if (sys_feat & BIT(MM_FEAT_BCN_BIT)) {
#if NX_TXQ_CNT == 4
		wiphy_err(wiphy, QUEUE_NAME "enabled in firmware but support not compiled in driver\n");
		return -1;
#endif /* NX_TXQ_CNT == 4 */
	} else {
#if NX_TXQ_CNT == 5
		wiphy_err(wiphy, QUEUE_NAME "disabled in firmware but support compiled in driver\n");
		return -1;
#endif /* NX_TXQ_CNT == 5 */
	}
#undef QUEUE_NAME

	switch (__MDM_PHYCFG_FROM_VERS(phy_feat)) {
	case MDM_PHY_CONFIG_TRIDENT:
	case MDM_PHY_CONFIG_ELMA:
		asr_hw->mod_params->nss = 1;
		break;
	case MDM_PHY_CONFIG_KARST:
		{
			int nss_supp = (phy_feat & MDM_NSS_MASK) >> MDM_NSS_LSB;
			if (asr_hw->mod_params->nss > nss_supp)
				asr_hw->mod_params->nss = nss_supp;
		}
		break;
	default:
		WARN_ON(1);
		break;
	}

	if (asr_hw->mod_params->nss < 1 || asr_hw->mod_params->nss > 2)
		asr_hw->mod_params->nss = 1;

	wiphy_info(wiphy, "PHY features: [NSS=%d][CHBW=%d]%s\n",
		   asr_hw->mod_params->nss,
		   20 * (1 << ((phy_feat & MDM_CHBW_MASK) >> MDM_CHBW_LSB)),
		   asr_hw->mod_params->ldpc_on ? "[LDPC]" : "");

#define PRINT_ASR_FEAT(feat)                                   \
    (sys_feat & BIT(MM_FEAT_##feat##_BIT) ? "["#feat"]" : "")

	wiphy_info(wiphy,
		   "FW features: %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n",
		   PRINT_ASR_FEAT(BCN), PRINT_ASR_FEAT(AUTOBCN),
		   PRINT_ASR_FEAT(HWSCAN), PRINT_ASR_FEAT(CMON),
		   PRINT_ASR_FEAT(MROLE), PRINT_ASR_FEAT(RADAR),
		   PRINT_ASR_FEAT(PS), PRINT_ASR_FEAT(UAPSD),
		   PRINT_ASR_FEAT(DPSM), PRINT_ASR_FEAT(AMPDU),
		   PRINT_ASR_FEAT(AMSDU), PRINT_ASR_FEAT(CHNL_CTXT),
		   PRINT_ASR_FEAT(REORD), PRINT_ASR_FEAT(P2P),
		   PRINT_ASR_FEAT(P2P_GO), PRINT_ASR_FEAT(UMAC),
		   PRINT_ASR_FEAT(VHT), PRINT_ASR_FEAT(BFMEE),
		   PRINT_ASR_FEAT(BFMER), PRINT_ASR_FEAT(WAPI),
		   PRINT_ASR_FEAT(MFP), PRINT_ASR_FEAT(MU_MIMO_RX),
		   PRINT_ASR_FEAT(MU_MIMO_TX), PRINT_ASR_FEAT(MESH), PRINT_ASR_FEAT(TDLS));
#undef PRINT_ASR_FEAT

	return 0;
}

#ifdef CONFIG_ASR595X
static void asr_set_he_capa(struct asr_hw *asr_hw, struct wiphy *wiphy)
{
	struct ieee80211_supported_band *band_2GHz = wiphy->bands[NL80211_BAND_2GHZ];
	int i;
	int nss = asr_hw->mod_params->nss;
	struct ieee80211_sta_he_cap *he_cap;
	int mcs_map;

	if (!asr_hw->mod_params->he_on) {
		band_2GHz->iftype_data = NULL;
		band_2GHz->n_iftype_data = 0;
		return;
	}

	he_cap = (struct ieee80211_sta_he_cap *)&band_2GHz->iftype_data->he_cap;
	he_cap->has_he = true;

	if (asr_hw->mod_params->twt_request)
		he_cap->he_cap_elem.mac_cap_info[0] |= IEEE80211_HE_MAC_CAP0_TWT_REQ;

	he_cap->he_cap_elem.mac_cap_info[2] |= IEEE80211_HE_MAC_CAP2_ALL_ACK;
	if (asr_hw->mod_params->use_2040) {
		he_cap->he_cap_elem.phy_cap_info[0] |= IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_IN_2G;
		he_cap->ppe_thres[0] |= 0x10;
	}
	if (asr_hw->mod_params->use_80) {
		he_cap->he_cap_elem.phy_cap_info[0] |= IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_80MHZ_IN_5G;
	}
	if (asr_hw->mod_params->ldpc_on) {
		he_cap->he_cap_elem.phy_cap_info[1] |= IEEE80211_HE_PHY_CAP1_LDPC_CODING_IN_PAYLOAD;
	} else {
		// If no LDPC is supported, we have to limit to MCS0_9, as LDPC is mandatory
		// for MCS 10 and 11
		asr_hw->mod_params->he_mcs_map = min_t(int, asr_hw->mod_params->mcs_map, IEEE80211_HE_MCS_SUPPORT_0_9);
	}
	he_cap->he_cap_elem.phy_cap_info[1] |= IEEE80211_HE_PHY_CAP1_HE_LTF_AND_GI_FOR_HE_PPDUS_0_8US |
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0)
	    IEEE80211_HE_PHY_CAP1_MIDAMBLE_RX_TX_MAX_NSTS;
#else
	    IEEE80211_HE_PHY_CAP1_MIDAMBLE_RX_MAX_NSTS;
#endif
	he_cap->he_cap_elem.phy_cap_info[2] |=
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0)
            IEEE80211_HE_PHY_CAP2_MIDAMBLE_RX_TX_MAX_NSTS |
#else
            IEEE80211_HE_PHY_CAP2_MIDAMBLE_RX_MAX_NSTS |
#endif
	    IEEE80211_HE_PHY_CAP2_NDP_4x_LTF_AND_3_2US | IEEE80211_HE_PHY_CAP2_DOPPLER_RX;
	if (asr_hw->mod_params->stbc_on)
		he_cap->he_cap_elem.phy_cap_info[2] |= IEEE80211_HE_PHY_CAP2_STBC_RX_UNDER_80MHZ;
	he_cap->he_cap_elem.phy_cap_info[3] |= IEEE80211_HE_PHY_CAP3_DCM_MAX_CONST_RX_16_QAM |
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 13, 0)
	    IEEE80211_HE_PHY_CAP3_DCM_MAX_RX_NSS_1 | IEEE80211_HE_PHY_CAP3_RX_PARTIAL_BW_SU_IN_20MHZ_MU;
#else
	    IEEE80211_HE_PHY_CAP3_DCM_MAX_RX_NSS_1 | IEEE80211_HE_PHY_CAP3_RX_HE_MU_PPDU_FROM_NON_AP_STA;
#endif
	if (asr_hw->mod_params->bfmee) {
		he_cap->he_cap_elem.phy_cap_info[4] |= IEEE80211_HE_PHY_CAP4_SU_BEAMFORMEE;
		he_cap->he_cap_elem.phy_cap_info[4] |= IEEE80211_HE_PHY_CAP4_BEAMFORMEE_MAX_STS_UNDER_80MHZ_4;
	}
	he_cap->he_cap_elem.phy_cap_info[5] |= IEEE80211_HE_PHY_CAP5_NG16_SU_FEEDBACK |
	    IEEE80211_HE_PHY_CAP5_NG16_MU_FEEDBACK;
	he_cap->he_cap_elem.phy_cap_info[6] |= IEEE80211_HE_PHY_CAP6_CODEBOOK_SIZE_42_SU |
	    IEEE80211_HE_PHY_CAP6_CODEBOOK_SIZE_75_MU |
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 13, 0)
	    IEEE80211_HE_PHY_CAP6_TRIG_SU_BEAMFORMING_FB |
	    IEEE80211_HE_PHY_CAP6_TRIG_MU_BEAMFORMING_PARTIAL_BW_FB |
#else
	    IEEE80211_HE_PHY_CAP6_TRIG_SU_BEAMFORMER_FB |
	    IEEE80211_HE_PHY_CAP6_TRIG_MU_BEAMFORMER_FB |
#endif
	    IEEE80211_HE_PHY_CAP6_PPE_THRESHOLD_PRESENT | IEEE80211_HE_PHY_CAP6_PARTIAL_BANDWIDTH_DL_MUMIMO;
	he_cap->he_cap_elem.phy_cap_info[7] |= IEEE80211_HE_PHY_CAP7_HE_SU_MU_PPDU_4XLTF_AND_08_US_GI;
	he_cap->he_cap_elem.phy_cap_info[8] |= IEEE80211_HE_PHY_CAP8_20MHZ_IN_40MHZ_HE_PPDU_IN_2G;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0)
	he_cap->he_cap_elem.phy_cap_info[9] |= IEEE80211_HE_PHY_CAP9_RX_FULL_BW_SU_USING_MU_WITH_COMP_SIGB |
	    IEEE80211_HE_PHY_CAP9_RX_FULL_BW_SU_USING_MU_WITH_NON_COMP_SIGB;
#endif
	mcs_map = asr_hw->mod_params->he_mcs_map;
	memset(&he_cap->he_mcs_nss_supp, 0, sizeof(he_cap->he_mcs_nss_supp));
	for (i = 0; i < nss; i++) {
		uint16_t unsup_for_ss = cpu_to_le16(IEEE80211_HE_MCS_NOT_SUPPORTED << (i * 2));
		he_cap->he_mcs_nss_supp.rx_mcs_80 |= cpu_to_le16(mcs_map << (i * 2));
		he_cap->he_mcs_nss_supp.rx_mcs_160 |= unsup_for_ss;
		he_cap->he_mcs_nss_supp.rx_mcs_80p80 |= unsup_for_ss;
		mcs_map = IEEE80211_HE_MCS_SUPPORT_0_9;
	}
	for (; i < 8; i++) {
		uint16_t unsup_for_ss = cpu_to_le16(IEEE80211_HE_MCS_NOT_SUPPORTED << (i * 2));
		he_cap->he_mcs_nss_supp.rx_mcs_80 |= unsup_for_ss;
		he_cap->he_mcs_nss_supp.rx_mcs_160 |= unsup_for_ss;
		he_cap->he_mcs_nss_supp.rx_mcs_80p80 |= unsup_for_ss;
	}
	mcs_map = asr_hw->mod_params->he_mcs_map;
	for (i = 0; i < nss; i++) {
		uint16_t unsup_for_ss = cpu_to_le16(IEEE80211_HE_MCS_NOT_SUPPORTED << (i * 2));
		he_cap->he_mcs_nss_supp.tx_mcs_80 |= cpu_to_le16(mcs_map << (i * 2));
		he_cap->he_mcs_nss_supp.tx_mcs_160 |= unsup_for_ss;
		he_cap->he_mcs_nss_supp.tx_mcs_80p80 |= unsup_for_ss;
		mcs_map = min_t(int, asr_hw->mod_params->he_mcs_map, IEEE80211_HE_MCS_SUPPORT_0_9);
	}
	for (; i < 8; i++) {
		uint16_t unsup_for_ss = cpu_to_le16(IEEE80211_HE_MCS_NOT_SUPPORTED << (i * 2));
		he_cap->he_mcs_nss_supp.tx_mcs_80 |= unsup_for_ss;
		he_cap->he_mcs_nss_supp.tx_mcs_160 |= unsup_for_ss;
		he_cap->he_mcs_nss_supp.tx_mcs_80p80 |= unsup_for_ss;
	}
}
#endif

int asr_handle_dynparams(struct asr_hw *asr_hw, struct wiphy *wiphy)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 1)
	struct ieee80211_supported_band *band_2GHz = wiphy->bands[NL80211_BAND_2GHZ];
#else
	struct ieee80211_supported_band *band_2GHz = wiphy->bands[IEEE80211_BAND_2GHZ];
#endif

	u32 mdm_phy_cfg;

	int i, ret;
	int nss;
	int mcs_map;

	ret = asr_check_fw_hw_feature(asr_hw, wiphy);
	if (ret)
		return ret;

	/* FULLMAC specific parameters */
	wiphy->flags |= WIPHY_FLAG_REPORTS_OBSS;

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 0, 8)
	if (asr_hw->mod_params->ap_uapsd_on)
		wiphy->flags |= WIPHY_FLAG_AP_UAPSD;
#endif

	if (asr_hw->mod_params->phy_cfg < 0 || asr_hw->mod_params->phy_cfg > 5)
		asr_hw->mod_params->phy_cfg = 2;

	if (asr_hw->mod_params->mcs_map < 0 || asr_hw->mod_params->mcs_map > 2)
		asr_hw->mod_params->mcs_map = 0;

	mdm_phy_cfg = __MDM_PHYCFG_FROM_VERS(asr_hw->version_cfm.version_phy_1);
	if (mdm_phy_cfg == MDM_PHY_CONFIG_TRIDENT) {
		struct asr_phy_conf_file phy_conf;
		// Retrieve the Trident configuration
		asr_parse_phy_configfile(asr_hw, &phy_conf);
		memcpy(&asr_hw->phy_config, &phy_conf.trd, sizeof(phy_conf.trd));
	} else if (mdm_phy_cfg == MDM_PHY_CONFIG_ELMA) {
	} else if (mdm_phy_cfg == MDM_PHY_CONFIG_KARST) {
		struct asr_phy_conf_file phy_conf;
		// We use the NSS parameter as is
		// Retrieve the Karst configuration
		asr_parse_phy_configfile(asr_hw, &phy_conf);

		memcpy(&asr_hw->phy_config, &phy_conf.karst, sizeof(phy_conf.karst));
	} else {
		WARN_ON(1);
	}

	nss = asr_hw->mod_params->nss;

	/* VHT capabilities */
	/*
	 * MCS map:
	 * This capabilities are filled according to the mcs_map module parameter.
	 * However currently we have some limitations due to FPGA clock constraints
	 * that prevent always using the range of MCS that is defined by the
	 * parameter:
	 *   - in RX, 2SS, we support up to MCS7
	 *   - in TX, 2SS, we support up to MCS8
	 */
	mcs_map = asr_hw->mod_params->mcs_map;
	for (i = 0; i < nss; i++) {
		band_2GHz->ht_cap.mcs.rx_mask[i] = 0xFF;
	}

	/*
	 * LDPC capability:
	 * This capability is filled according to the ldpc_on module parameter.
	 * However currently we have some limitations due to FPGA clock constraints
	 * that prevent correctly receiving more than MCS4-2SS when using LDPC.
	 * We therefore disable the LDPC support if 2SS is supported.
	 */
	asr_hw->mod_params->ldpc_on = nss > 1 ? false : asr_hw->mod_params->ldpc_on;

	/* HT capabilities */
	band_2GHz->ht_cap.cap |= 1 << IEEE80211_HT_CAP_RX_STBC_SHIFT;
	if (asr_hw->mod_params->ldpc_on)
		band_2GHz->ht_cap.cap |= IEEE80211_HT_CAP_LDPC_CODING;

	if (asr_hw->mod_params->use_2040) {
		band_2GHz->ht_cap.mcs.rx_mask[4] = 0x1;	/* MCS32 */
		band_2GHz->ht_cap.cap |= IEEE80211_HT_CAP_SUP_WIDTH_20_40 | IEEE80211_HT_CAP_DSSSCCK40;
		band_2GHz->ht_cap.mcs.rx_highest = cpu_to_le16(135 * nss);
	} else {
		band_2GHz->ht_cap.mcs.rx_highest = cpu_to_le16(65 * nss);
	}
	if (nss > 1)
		band_2GHz->ht_cap.cap |= IEEE80211_HT_CAP_TX_STBC;

	if (asr_hw->mod_params->sgi) {
		band_2GHz->ht_cap.cap |= IEEE80211_HT_CAP_SGI_20;
		if (asr_hw->mod_params->use_2040) {
			band_2GHz->ht_cap.cap |= IEEE80211_HT_CAP_SGI_40;
			band_2GHz->ht_cap.mcs.rx_highest = cpu_to_le16(150 * nss);
		} else
			band_2GHz->ht_cap.mcs.rx_highest = cpu_to_le16(72 * nss);
	}
	band_2GHz->ht_cap.cap |= IEEE80211_HT_CAP_GRN_FLD;
	if (!asr_hw->mod_params->ht_on)
		band_2GHz->ht_cap.ht_supported = false;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	if (asr_hw->mod_params->custregd) {
		dev_info(asr_hw->dev, "\n\n%s: CAUTION: USING PERMISSIVE CUSTOM REGULATORY RULES\n\n", __func__);
		wiphy->regulatory_flags |= REGULATORY_CUSTOM_REG;
		wiphy_apply_custom_regulatory(wiphy, &asr_regdom);
	}
#endif

	wiphy->max_scan_ssids = SCAN_SSID_MAX;
	wiphy->max_scan_ie_len = IEEE80211_MAX_DATA_LEN;

    /**
     * adjust caps with lower layers asr_hw->version_cfm
     */
	switch (mdm_phy_cfg) {
	case MDM_PHY_CONFIG_TRIDENT:
		{
			asr_dbg(INFO, "%s: found Trident phy .. using phy bw tweaks\n", __func__);
			asr_hw->use_phy_bw_tweaks = true;
			break;
		}
	case MDM_PHY_CONFIG_ELMA:
		asr_dbg(INFO, "%s: found ELMA phy .. disabling 2.4GHz and greenfield rx\n", __func__);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
		wiphy->bands[NL80211_BAND_2GHZ] = NULL;
#else
		wiphy->bands[IEEE80211_BAND_2GHZ] = NULL;
#endif
		band_2GHz->ht_cap.cap &= ~IEEE80211_HT_CAP_GRN_FLD;
		break;
	case MDM_PHY_CONFIG_KARST:
		{
			break;
		}
	default:
		WARN_ON(1);
		break;
	}
#ifdef CONFIG_ASR595X
	/* Set HE capabilities */
	asr_set_he_capa(asr_hw, wiphy);
#endif
	return 0;
}
