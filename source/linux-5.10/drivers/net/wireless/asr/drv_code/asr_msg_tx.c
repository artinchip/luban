/**
 ******************************************************************************
 *
 * @file asr_msg_tx.c
 *
 * @brief TX function definitions
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */
#include <linux/version.h>
#include <linux/bitops.h>
#include <net/cfg80211.h>
#include "asr_msg_tx.h"
#include "asr_mod_params.h"
#ifdef CONFIG_ASR_SDIO
#include "asr_sdio.h"
#ifdef CONFIG_ASR595X
#define MSG_SIZE SDIO_BLOCK_SIZE_TX
#else
#define MSG_SIZE SDIO_BLOCK_SIZE
#endif
#else
#define MSG_SIZE 32
#endif
#ifdef CFG_SNIFFER_SUPPORT
#include "reg_mac_core.h"
#endif
#include <linux/kernel.h>
#ifdef CONFIG_HW_MIB_TABLE
#include "hal_machw_mib.h"
#endif
const struct mac_addr mac_addr_bcst = { {0xFFFF, 0xFFFF, 0xFFFF} };

/// Authentication algorithm definition
#define MAC_AUTH_ALGO_OPEN                0
#define MAC_AUTH_ALGO_SHARED              1
#define MAC_AUTH_ALGO_FT                  2
#define MAC_AUTH_ALGO_SAE                 3

/* Default MAC Rx filters that can be changed by mac80211
 * (via the configure_filter() callback) */
#define ASR_MAC80211_CHANGEABLE        (                                       \
                                         NXMAC_ACCEPT_BA_BIT                  | \
                                         NXMAC_ACCEPT_BAR_BIT                 | \
                                         NXMAC_ACCEPT_OTHER_DATA_FRAMES_BIT   | \
                                         NXMAC_ACCEPT_PROBE_REQ_BIT           | \
                                         NXMAC_ACCEPT_PS_POLL_BIT               \
                                        )

/* Default MAC Rx filters that cannot be changed by mac80211 */
#define ASR_MAC80211_NOT_CHANGEABLE    (                                       \
                                         NXMAC_ACCEPT_QO_S_NULL_BIT           | \
                                         NXMAC_ACCEPT_Q_DATA_BIT              | \
                                         NXMAC_ACCEPT_DATA_BIT                | \
                                         NXMAC_ACCEPT_OTHER_MGMT_FRAMES_BIT   | \
                                         NXMAC_ACCEPT_MY_UNICAST_BIT          | \
                                         NXMAC_ACCEPT_BROADCAST_BIT           | \
                                         NXMAC_ACCEPT_BEACON_BIT              | \
                                         NXMAC_ACCEPT_PROBE_RESP_BIT            \
                                        )

/* Default MAC Rx filter */
#define ASR_DEFAULT_RX_FILTER  (ASR_MAC80211_CHANGEABLE | ASR_MAC80211_NOT_CHANGEABLE)

#ifdef CFG_SNIFFER_SUPPORT
#define ASR_MAC80211_FILTER_MONITOR  (0xFFFFFFFF & ~(NXMAC_ACCEPT_ERROR_FRAMES_BIT | NXMAC_EXC_UNENCRYPTED_BIT |                      \
                                                  NXMAC_EN_DUPLICATE_DETECTION_BIT))
#endif

static const int bw2chnl[] = {
	[NL80211_CHAN_WIDTH_20_NOHT] = PHY_CHNL_BW_20,
	[NL80211_CHAN_WIDTH_20] = PHY_CHNL_BW_20,
	[NL80211_CHAN_WIDTH_40] = PHY_CHNL_BW_40,
	[NL80211_CHAN_WIDTH_80] = PHY_CHNL_BW_80,
	[NL80211_CHAN_WIDTH_160] = PHY_CHNL_BW_160,
	[NL80211_CHAN_WIDTH_80P80] = PHY_CHNL_BW_80P80,
};

static const int chnl2bw[] = {
	[PHY_CHNL_BW_20] = NL80211_CHAN_WIDTH_20,
	[PHY_CHNL_BW_40] = NL80211_CHAN_WIDTH_40,
	[PHY_CHNL_BW_80] = NL80211_CHAN_WIDTH_80,
	[PHY_CHNL_BW_160] = NL80211_CHAN_WIDTH_160,
	[PHY_CHNL_BW_80P80] = NL80211_CHAN_WIDTH_80P80,
};

/*****************************************************************************/
/*
 * Parse the ampdu density to retrieve the value in usec, according to the
 * values defined in ieee80211.h
 */
static inline u8 asr_ampdudensity2usec(u8 ampdudensity)
{
	switch (ampdudensity) {
	case IEEE80211_HT_MPDU_DENSITY_NONE:
		return 0;
		/* 1 microsecond is our granularity */
	case IEEE80211_HT_MPDU_DENSITY_0_25:
	case IEEE80211_HT_MPDU_DENSITY_0_5:
	case IEEE80211_HT_MPDU_DENSITY_1:
		return 1;
	case IEEE80211_HT_MPDU_DENSITY_2:
		return 2;
	case IEEE80211_HT_MPDU_DENSITY_4:
		return 4;
	case IEEE80211_HT_MPDU_DENSITY_8:
		return 8;
	case IEEE80211_HT_MPDU_DENSITY_16:
		return 16;
	default:
		return 0;
	}
}

static inline bool use_pairwise_key(struct cfg80211_crypto_settings *crypto)
{
	if ((crypto->cipher_group == WLAN_CIPHER_SUITE_WEP40) || (crypto->cipher_group == WLAN_CIPHER_SUITE_WEP104))
		return false;

	return true;
}

static inline bool is_non_blocking_msg(int id)
{
	return ((id == MM_TIM_UPDATE_REQ) || (id == ME_RC_SET_RATE_REQ) || (id == ME_TRAFFIC_IND_REQ));
}

static inline u8 passive_scan_flag(u32 flags)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	if (flags & IEEE80211_CHAN_NO_IR)
#else
	if (flags & IEEE80211_CHAN_PASSIVE_SCAN)
#endif
		return SCAN_PASSIVE_BIT;
	return 0;
}

/**
 ******************************************************************************
 * @brief Allocate memory for a message
 *
 * This primitive allocates memory for a message that has to be sent. The memory
 * is allocated dynamically on the heap and the length of the variable parameter
 * structure has to be provided in order to allocate the correct size.
 *
 * Several additional parameters are provided which will be preset in the message
 * and which may be used internally to choose the kind of memory to allocate.
 *
 * The memory allocated will be automatically freed by the kernel, after the
 * pointer has been sent to ke_msg_send(). If the message is not sent, it must
 * be freed explicitly with ke_msg_free().
 *
 * Allocation failure is considered critical and should not happen.
 *
 * @param[in] id        Message identifier
 * @param[in] dest_id   Destination Task Identifier
 * @param[in] src_id    Source Task Identifier
 * @param[in] param_len Size of the message parameters to be allocated
 *
 * @return Pointer to the parameter member of the ke_msg. If the parameter
 *         structure is empty, the pointer will point to the end of the message
 *         and should not be used (except to retrieve the message pointer or to
 *         send the message)
 ******************************************************************************
 */
static inline struct lmac_msg *asr_msg_zalloc(lmac_msg_id_t const id,
					      lmac_task_id_t const dest_id,
					      lmac_task_id_t const src_id, u16 const param_len)
{
	struct lmac_msg *msg;
	gfp_t flags;
	uint32_t tx_data_end_token = 0xAECDBFCA;

	if (is_non_blocking_msg(id) && in_softirq())
		flags = GFP_ATOMIC;
	else
		flags = GFP_KERNEL;

	msg = (struct lmac_msg *)kzalloc(sizeof(struct lmac_msg) + param_len + MSG_SIZE, flags);
	if (msg == NULL) {
		dev_err(g_asr_para.dev, "%s: msg allocation failed\n", __func__);
		return NULL;
	}

	msg->id = id;
	msg->dest_id = dest_id;
	msg->src_id = src_id;
	msg->param_len = param_len;

	//memcpy((u8 *)msg + sizeof(struct lmac_msg) + param_len, &tx_data_end_token, 4);
    #ifdef CONFIG_ASR_SDIO
    memcpy((u8*)msg + ASR_ALIGN_BLKSZ_HI(sizeof(struct lmac_msg) + param_len + 4) - 4, &tx_data_end_token, 4);
    #else
    memcpy((u8 *)msg + sizeof(struct lmac_msg) + param_len, &tx_data_end_token, 4);
    #endif
	return msg;
}

static int asr_send_msg(struct asr_hw *asr_hw, struct lmac_msg *msg, int reqcfm, lmac_msg_id_t reqid, void *cfm)
{
	struct asr_cmd cmd;
	bool nonblock = false;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);
	if(!asr_hw->plat||g_asr_para.dev_driver_remove){
		dev_info(asr_hw->dev, "%s: drop msg if drv has beed removed\n", __func__);
		return 0;
	}

	memset(&cmd, 0, sizeof(cmd));

	if (asr_hw->driver_mode != DRIVER_MODE_NORMAL) {
		dev_info(asr_hw->dev, "%s: drop msg in driver_mode(%d)\n", __func__, asr_hw->driver_mode);
		return 0;
	}

	if (!test_bit(ASR_DEV_STARTED, &asr_hw->phy_flags) &&
	    reqid != MM_RESET_CFM && reqid != MM_VERSION_CFM &&
	    reqid != MM_START_CFM && reqid != MM_SET_IDLE_CFM &&
	    reqid != ME_CONFIG_CFM && reqid != MM_SET_PS_MODE_CFM &&
	    reqid != ME_CHAN_CONFIG_CFM && reqid != MM_FW_SOFTVERSION_CFM &&
	    reqid != MM_FW_MAC_ADDR_CFM && reqid != MM_SET_FW_MAC_ADDR_CFM &&
	    reqid != MM_SET_EFUSE_TXPWR_CFM &&
	    reqid != DBG_SET_MOD_FILTER_CFM &&
#ifdef CONFIG_ASR_SDIO
	    reqid != MM_HIF_SDIO_INFO_IND &&
#endif
	    reqid != MM_GET_INFO_CFM && reqid != MM_SET_TX_PWR_RATE_CFM) {
		dev_err(asr_hw->dev, "%s: bypassing (ASR_DEV_RESTARTING set) 0x%02x\n", __func__, reqid);
		//kfree(msg);
		return -EBUSY;
	} else if (!asr_hw->ipc_env) {
		dev_err(asr_hw->dev, "%s: bypassing (restart must have failed)\n", __func__);
		//kfree(msg);
		return -EBUSY;
	}

	nonblock = is_non_blocking_msg(msg->id);

	cmd.result = -EINTR;
	cmd.id = msg->id;
	cmd.reqid = reqid;
	cmd.a2e_msg = msg;
	cmd.e2a_msg = cfm;

	if (nonblock || (!reqcfm))
		cmd.flags = ASR_CMD_FLAG_NONBLOCK;
	if (reqcfm)
		cmd.flags |= ASR_CMD_FLAG_REQ_CFM;

	ret = asr_hw->cmd_mgr.queue(&asr_hw->cmd_mgr, &cmd);

	return ret;
}

/******************************************************************************
 *    Control messages handling functions (SOFTMAC and  FULLMAC)
 *****************************************************************************/
int asr_send_reset(struct asr_hw *asr_hw)
{
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* RESET REQ has no parameter */
	msg = asr_msg_zalloc(MM_RESET_REQ, TASK_MM, DRV_TASK_ID, 0);
	if (!msg)
		return -ENOMEM;

	ret = asr_send_msg(asr_hw, msg, 1, MM_RESET_CFM, NULL);

	kfree(msg);
	return ret;
}

int asr_send_start(struct asr_hw *asr_hw)
{
	struct mm_start_req *start_req_param = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the START REQ message */
	msg = asr_msg_zalloc(MM_START_REQ, TASK_MM, DRV_TASK_ID, sizeof(struct mm_start_req));
	if (!msg)
		return -ENOMEM;

	start_req_param = (struct mm_start_req *)msg->param;

	/* Set parameters for the START message */
	memcpy(&start_req_param->phy_cfg, &asr_hw->phy_config, sizeof(asr_hw->phy_config));
	start_req_param->uapsd_timeout = (u32) asr_hw->mod_params->uapsd_timeout;
	start_req_param->lp_clk_accuracy = (u16) asr_hw->mod_params->lp_clk_ppm;

	/* Send the START REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, MM_START_CFM, NULL);

	kfree(msg);
	return ret;
}
u8 get_ps_mode_type(bool ps_on)
{
	u8 mode = PS_MODE_OFF;
	if(!ps_on)
		return mode;

	#if (!defined CONFIG_ASR_PM && !defined CONFIG_ASR_USB_PM)
		mode = PS_MODE_MODEMSLEEP_0_PIN;
	#elif (defined CONFIG_ASR_USB_PM)
		mode = PS_MODE_LIGHTSLEEP;
	#elif (defined CONFIG_GPIO_WAKEUP_MOD && !defined CONFIG_GPIO_WAKEUP_HOST && defined CONFIG_ASR5505)
		mode = PS_MODE_LIGHTSLEEP_1_PIN;
	#elif (defined CONFIG_GPIO_WAKEUP_MOD && defined CONFIG_GPIO_WAKEUP_HOST && defined CONFIG_ASR5505)
		mode = PS_MODE_LIGHTSLEEP;
	#elif (defined CONFIG_GPIO_WAKEUP_MOD && defined CONFIG_GPIO_WAKEUP_HOST && defined CONFIG_ASR5825)
		mode = PS_MODE_MODEMSLEEP_2_PIN;
	#endif

	return mode;
}

int asr_send_set_ps_mode(struct asr_hw *asr_hw, bool ps_on)
{
	struct mm_set_ps_mode_req *set_ps_mode_req_param = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	asr_hw->ps_on = ps_on;
	msg = asr_msg_zalloc(MM_SET_PS_MODE_REQ, TASK_MM, DRV_TASK_ID, sizeof(struct mm_set_ps_mode_req));
	if (!msg)
		return -ENOMEM;

	set_ps_mode_req_param = (struct mm_set_ps_mode_req *)msg->param;

	set_ps_mode_req_param->new_state = get_ps_mode_type(ps_on);


	ret = asr_send_msg(asr_hw, msg, 1, MM_SET_PS_MODE_CFM, NULL);

	kfree(msg);
	return ret;
}

#ifdef CFG_SNIFFER_SUPPORT
int asr_send_set_idle(struct asr_hw *asr_hw, int idle)
{
	struct mm_set_idle_req *set_idle_req_param = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	msg = asr_msg_zalloc(MM_SET_IDLE_REQ, TASK_MM, DRV_TASK_ID, sizeof(struct mm_set_idle_req));
	if (!msg)
		return -ENOMEM;

	set_idle_req_param = (struct mm_set_idle_req *)msg->param;

	set_idle_req_param->hw_idle = idle;

	ret = asr_send_msg(asr_hw, msg, 1, MM_SET_IDLE_CFM, NULL);

	kfree(msg);
	return ret;
}
#endif

int asr_send_version_req(struct asr_hw *asr_hw, struct mm_version_cfm *cfm)
{
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* VERSION REQ has no parameter */
	msg = asr_msg_zalloc(MM_VERSION_REQ, TASK_MM, DRV_TASK_ID, 0);
	if (!msg)
		return -ENOMEM;

	ret = asr_send_msg(asr_hw, msg, 1, MM_VERSION_CFM, cfm);

	kfree(msg);
	return ret;
}

int asr_send_mm_get_info(struct asr_hw *asr_hw, struct mm_get_info_cfm *cfm)
{
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the ADD_IF_REQ message */
	msg = asr_msg_zalloc(MM_GET_INFO_REQ, TASK_MM, DRV_TASK_ID, 0);
	if (!msg)
		return -ENOMEM;

	/* Send the ADD_IF_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, MM_GET_INFO_CFM, cfm);

	kfree(msg);
	return ret;
}

int asr_send_add_if(struct asr_hw *asr_hw, const unsigned char *mac,
		    enum nl80211_iftype iftype, bool p2p, struct mm_add_if_cfm *cfm)
{
	struct mm_add_if_req *add_if_req_param = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the ADD_IF_REQ message */
	msg = asr_msg_zalloc(MM_ADD_IF_REQ, TASK_MM, DRV_TASK_ID, sizeof(struct mm_add_if_req));
	if (!msg)
		return -ENOMEM;

	add_if_req_param = (struct mm_add_if_req *)msg->param;

	/* Set parameters for the ADD_IF_REQ message */
	memcpy(&(add_if_req_param->addr.array[0]), mac, ETH_ALEN);
	switch (iftype) {
	case NL80211_IFTYPE_P2P_CLIENT:
		add_if_req_param->p2p = true;
		add_if_req_param->type = MM_STA;
		break;
	case NL80211_IFTYPE_STATION:
		add_if_req_param->type = MM_STA;
		break;
	case NL80211_IFTYPE_P2P_GO:
		add_if_req_param->p2p = true;
		add_if_req_param->type = MM_AP;
		break;
	case NL80211_IFTYPE_AP:
		add_if_req_param->type = MM_AP;
		break;
#if (defined CFG_SNIFFER_SUPPORT || defined CFG_CUS_FRAME)
	case NL80211_IFTYPE_MONITOR:
		add_if_req_param->type = MM_MONITOR_MODE;
		break;
#endif
	default:
		add_if_req_param->type = MM_STA;
		break;
	}

	/* Send the ADD_IF_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, MM_ADD_IF_CFM, cfm);

	kfree(msg);
	return ret;
}

int asr_send_remove_if(struct asr_hw *asr_hw, u8 vif_index)
{
	struct mm_remove_if_req *remove_if_req = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the MM_REMOVE_IF_REQ message */
	msg = asr_msg_zalloc(MM_REMOVE_IF_REQ, TASK_MM, DRV_TASK_ID, sizeof(struct mm_remove_if_req));
	if (!msg)
		return -ENOMEM;

	remove_if_req = (struct mm_remove_if_req *)msg->param;

	/* Set parameters for the MM_REMOVE_IF_REQ message */
	remove_if_req->inst_nbr = vif_index;

	/* Send the MM_REMOVE_IF_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, MM_REMOVE_IF_CFM, NULL);

	kfree(msg);
	return ret;
}

int asr_send_set_channel(struct asr_hw *asr_hw, int phy_idx, struct mm_set_channel_cfm *cfm)
{
	struct mm_set_channel_req *set_chnl_par = NULL;
	enum nl80211_chan_width width;
	u16 center_freq, center_freq1, center_freq2;
	s8 tx_power = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 1)
	enum nl80211_band band;
#else
	enum ieee80211_band band;
#endif
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	if (phy_idx >= asr_hw->phy_cnt)
		return -ENOTSUPP;

	msg = asr_msg_zalloc(MM_SET_CHANNEL_REQ, TASK_MM, DRV_TASK_ID, sizeof(struct mm_set_channel_req));
	if (!msg)
		return -ENOMEM;

	set_chnl_par = (struct mm_set_channel_req *)msg->param;

	if (phy_idx == 0) {
		/* On FULLMAC only setting channel of secondary chain */
		wiphy_err(asr_hw->wiphy, "Trying to set channel of primary chain");

		kfree(msg);
		return 0;
	} else {
		struct asr_sec_phy_chan *chan = &asr_hw->sec_phy_chan;

		width = chnl2bw[chan->type];
		band = chan->band;
		center_freq = chan->prim20_freq;
		center_freq1 = chan->center_freq1;
		center_freq2 = chan->center_freq2;
	}

#ifdef CONFIG_ASR595X
	set_chnl_par->chan.band = band;
	set_chnl_par->chan.type = bw2chnl[width];;
	set_chnl_par->chan.prim20_freq = center_freq;
	set_chnl_par->chan.center1_freq = center_freq1;
	set_chnl_par->chan.center2_freq = center_freq2;
	set_chnl_par->index = phy_idx;
	set_chnl_par->chan.tx_power = tx_power;

	if (asr_hw->use_phy_bw_tweaks) {
		/* XXX Tweak for 80MHz VHT */
		if (width > NL80211_CHAN_WIDTH_40) {
			int _offs = center_freq1 - center_freq;
			set_chnl_par->chan.type = PHY_CHNL_BW_40;
			set_chnl_par->chan.center1_freq = center_freq + 10 *
			    (_offs > 0 ? 1 : -1) * (abs(_offs) > 10 ? 1 : -1);
			ASR_DBG("Tweak for 80MHz VHT: 80MHz chan requested\n");
		}
	}

	ASR_DBG("mac80211:   freq=%d(c1:%d - c2:%d)/width=%d - band=%d\n"
		"   hw(%d): prim20=%d(c1:%d - c2:%d)/ type=%d - band=%d\n",
		center_freq, center_freq1,
		center_freq2, width, band,
		phy_idx, set_chnl_par->chan.prim20_freq, set_chnl_par->chan.center1_freq,
		set_chnl_par->chan.center2_freq, set_chnl_par->chan.type, set_chnl_par->chan.band);

#else
	set_chnl_par->band = band;
	set_chnl_par->type = bw2chnl[width];
	set_chnl_par->prim20_freq = center_freq;
	set_chnl_par->center1_freq = center_freq1;
	set_chnl_par->center2_freq = center_freq2;
	set_chnl_par->index = phy_idx;
	set_chnl_par->tx_power = tx_power;

	if (asr_hw->use_phy_bw_tweaks) {
		/* XXX Tweak for 80MHz VHT */
		if (width > NL80211_CHAN_WIDTH_40) {
			int _offs = center_freq1 - center_freq;
			set_chnl_par->type = PHY_CHNL_BW_40;
			set_chnl_par->center1_freq = center_freq + 10 *
			    (_offs > 0 ? 1 : -1) * (abs(_offs) > 10 ? 1 : -1);
			ASR_DBG("Tweak for 80MHz VHT: 80MHz chan requested\n");
		}
	}

	ASR_DBG("mac80211:   freq=%d(c1:%d - c2:%d)/width=%d - band=%d\n"
		"   hw(%d): prim20=%d(c1:%d - c2:%d)/ type=%d - band=%d\n",
		center_freq, center_freq1,
		center_freq2, width, band,
		phy_idx, set_chnl_par->prim20_freq, set_chnl_par->center1_freq,
		set_chnl_par->center2_freq, set_chnl_par->type, set_chnl_par->band);
#endif

	/* Send the MM_SET_CHANNEL_REQ REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, MM_SET_CHANNEL_CFM, cfm);

	kfree(msg);
	return ret;
}

#if (defined CFG_SNIFFER_SUPPORT || defined CFG_CUS_FRAME)
/********** handle filter_change event*************************************/
int asr_send_set_monitor_channel(struct asr_hw *asr_hw, struct cfg80211_chan_def *chandef)
{
	struct mm_set_channel_req *set_chnl_par = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	msg = asr_msg_zalloc(MM_SET_CHANNEL_REQ, TASK_MM, DRV_TASK_ID, sizeof(struct mm_set_channel_req));

	if (!msg)
		return -ENOMEM;

	set_chnl_par = (struct mm_set_channel_req *)msg->param;

#ifdef CONFIG_ASR595X
	set_chnl_par->chan.band = NL80211_BAND_2GHZ;
	set_chnl_par->chan.type = bw2chnl[chandef->width];
	set_chnl_par->chan.prim20_freq = chandef->chan->center_freq;
	set_chnl_par->chan.center1_freq = chandef->center_freq1;
	set_chnl_par->chan.center2_freq = chandef->center_freq2;
	set_chnl_par->index = 0;
	set_chnl_par->chan.tx_power = chandef->chan->max_power;
#else
	set_chnl_par->band = NL80211_BAND_2GHZ;
	set_chnl_par->type = bw2chnl[chandef->width];
	set_chnl_par->prim20_freq = chandef->chan->center_freq;
	set_chnl_par->center1_freq = chandef->center_freq1;
	set_chnl_par->center2_freq = chandef->center_freq2;
	set_chnl_par->index = 0;
	set_chnl_par->tx_power = chandef->chan->max_power;
#endif

	/* Send the MM_SET_CHANNEL_REQ REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, MM_SET_CHANNEL_CFM, NULL);

	kfree(msg);
	return ret;
}

int asr_send_set_filter(struct asr_hw *asr_hw, uint32_t filter)
{
	struct mm_set_filter_req *set_filter_req_param = NULL;
	struct asr_vif *asr_vif = asr_hw->vif_table[asr_hw->monitor_vif_idx];
	uint32_t new_filter = asr_vif->sniffer.rx_filter;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	/* Build the MM_SET_FILTER_REQ message */
	msg = asr_msg_zalloc(MM_SET_FILTER_REQ, TASK_MM, DRV_TASK_ID, sizeof(struct mm_set_filter_req));
	if (!msg)
		return -ENOMEM;

	set_filter_req_param = (struct mm_set_filter_req *)msg->param;

	if (0 == filter)
		new_filter = 0x0;
	/* Set parameters for the MM_SET_FILTER_REQ message */
	if (filter & BIT(WLAN_RX_BEACON)) {
		new_filter |= NXMAC_ACCEPT_BROADCAST_BIT;
		new_filter |= NXMAC_ACCEPT_ALL_BEACON_BIT;
		new_filter |= NXMAC_ACCEPT_OTHER_MGMT_FRAMES_BIT;
	}
	if (filter & BIT(WLAN_RX_PROBE_REQ)) {
		new_filter |= NXMAC_ACCEPT_PROBE_REQ_BIT;
		new_filter |= NXMAC_ACCEPT_UNICAST_BIT;
		new_filter |= NXMAC_ACCEPT_OTHER_MGMT_FRAMES_BIT;
	}
	if (filter & BIT(WLAN_RX_PROBE_RES)) {
		new_filter |= NXMAC_ACCEPT_UNICAST_BIT;
		new_filter |= NXMAC_ACCEPT_OTHER_MGMT_FRAMES_BIT;
	}
	if (filter & BIT(WLAN_RX_ACTION)) {
		/* fw did not support action only */
		new_filter |= NXMAC_ACCEPT_OTHER_MGMT_FRAMES_BIT;
	}
	if (filter & BIT(WLAN_RX_MANAGEMENT)) {
		new_filter |= NXMAC_ACCEPT_PROBE_REQ_BIT;
		new_filter |= NXMAC_ACCEPT_OTHER_MGMT_FRAMES_BIT;
	}
	if (filter & BIT(WLAN_RX_DATA)) {
		new_filter |= NXMAC_ACCEPT_OTHER_DATA_FRAMES_BIT;
		new_filter |= NXMAC_ACCEPT_UNICAST_BIT;
		new_filter |= NXMAC_ACCEPT_MULTICAST_BIT;
		new_filter |= NXMAC_ACCEPT_OTHER_BSSID_BIT;
	}
	if (filter & BIT(WLAN_RX_MCAST_DATA)) {
		new_filter |= NXMAC_ACCEPT_OTHER_DATA_FRAMES_BIT;
		new_filter |= NXMAC_ACCEPT_MULTICAST_BIT;
		new_filter |= NXMAC_ACCEPT_OTHER_BSSID_BIT;
	}
	if (filter & BIT(WLAN_RX_MONITOR_DEFAULT)) {
		new_filter |= NXMAC_DONT_DECRYPT_BIT;
		new_filter |= NXMAC_ACCEPT_BROADCAST_BIT;
		new_filter |= NXMAC_ACCEPT_OTHER_BSSID_BIT;
		new_filter |= NXMAC_ACCEPT_MY_UNICAST_BIT;
		new_filter |= NXMAC_ACCEPT_PROBE_RESP_BIT;
		new_filter |= NXMAC_ACCEPT_PROBE_REQ_BIT;
		new_filter |= NXMAC_ACCEPT_ALL_BEACON_BIT;
		new_filter |= NXMAC_ACCEPT_DATA_BIT;
		new_filter |= NXMAC_ACCEPT_Q_DATA_BIT;
	}
	if (filter & BIT(WLAN_RX_ALL))
		new_filter |= ASR_MAC80211_FILTER_MONITOR;
	if (filter & BIT(WLAN_RX_SMART_CONFIG)) {
		new_filter |= NXMAC_DONT_DECRYPT_BIT;
		new_filter |= NXMAC_ACCEPT_BROADCAST_BIT;
		new_filter |= NXMAC_ACCEPT_MULTICAST_BIT;
		new_filter |= NXMAC_ACCEPT_OTHER_BSSID_BIT;
		new_filter |= NXMAC_ACCEPT_UNICAST_BIT;
		new_filter |= NXMAC_ACCEPT_DATA_BIT;
		new_filter |= NXMAC_ACCEPT_CFWO_DATA_BIT;
		new_filter |= NXMAC_ACCEPT_Q_DATA_BIT;
		new_filter |= NXMAC_ACCEPT_QCFWO_DATA_BIT;
		new_filter |= NXMAC_ACCEPT_QO_S_NULL_BIT;
		new_filter |= NXMAC_ACCEPT_OTHER_DATA_FRAMES_BIT;
	}
	if (filter & BIT(WLAN_RX_SMART_CONFIG_MC)) {
		new_filter |= NXMAC_DONT_DECRYPT_BIT;
		new_filter |= NXMAC_ACCEPT_BROADCAST_BIT;
		new_filter |= NXMAC_ACCEPT_MY_UNICAST_BIT;
		//new_filter |= NXMAC_ACCEPT_ALL_BEACON_BIT;
		new_filter |= NXMAC_ACCEPT_MULTICAST_BIT;
		new_filter |= NXMAC_ACCEPT_OTHER_BSSID_BIT;
		new_filter |= NXMAC_ACCEPT_UNICAST_BIT;
		//new_filter |= NXMAC_ACCEPT_DATA_BIT;
		//new_filter |= NXMAC_ACCEPT_CFWO_DATA_BIT;
		new_filter |= NXMAC_ACCEPT_Q_DATA_BIT;
		//new_filter |= NXMAC_ACCEPT_QCFWO_DATA_BIT;
		//new_filter |= NXMAC_ACCEPT_QO_S_NULL_BIT;
		new_filter |= NXMAC_ACCEPT_OTHER_DATA_FRAMES_BIT;
	}
	if (new_filter != asr_vif->sniffer.rx_filter) {
		/* Now copy all the flags into the message parameter */
		asr_vif->sniffer.rx_filter = new_filter;
		set_filter_req_param->filter = new_filter;
		/* Send the MM_SET_FILTER_REQ message to LMAC FW */
		ret = asr_send_msg(asr_hw, msg, 1, MM_SET_FILTER_CFM, NULL);

		kfree(msg);
		return ret;
	}

	kfree(msg);
	return 0;
}
#endif

int asr_send_key_add(struct asr_hw *asr_hw, u8 vif_idx, u8 sta_idx,
		     bool pairwise, u8 * key, u8 key_len, u8 key_idx, u8 cipher_suite, struct mm_key_add_cfm *cfm)
{
	struct mm_key_add_req *key_add_req = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the MM_KEY_ADD_REQ message */
	msg = asr_msg_zalloc(MM_KEY_ADD_REQ, TASK_MM, DRV_TASK_ID, sizeof(struct mm_key_add_req));
	if (!msg)
		return -ENOMEM;

	key_add_req = (struct mm_key_add_req *)msg->param;

	/* Set parameters for the MM_KEY_ADD_REQ message */
	if (sta_idx != 0xFF) {
		/* Pairwise key */
		key_add_req->sta_idx = sta_idx;
	} else {
		/* Default key */
		key_add_req->sta_idx = sta_idx;
		key_add_req->key_idx = (u8) key_idx;	/* only useful for default keys */
	}
	key_add_req->pairwise = pairwise;
	key_add_req->inst_nbr = vif_idx;
	key_add_req->key.length = key_len;
	memcpy(&(key_add_req->key.array[0]), key, key_len);

	key_add_req->cipher_suite = cipher_suite;

	dev_info(asr_hw->dev, "%s: sta_idx:%d key_idx:%d inst_nbr:%d cipher:%d key_len:%d key1(%x:%x:%x:%x),key2(%x:%x:%x:%x)\n",
	     __func__, key_add_req->sta_idx, key_add_req->key_idx,
	     key_add_req->inst_nbr, key_add_req->cipher_suite,
	     key_add_req->key.length, key_add_req->key.array[0],
	     key_add_req->key.array[1], key_add_req->key.array[2],
	     key_add_req->key.array[3], key_add_req->key.array[4],
	     key_add_req->key.array[5], key_add_req->key.array[6], key_add_req->key.array[7]);
#if defined(CONFIG_ASR_DBG) || defined(CONFIG_DYNAMIC_DEBUG)
	//print_hex_dump_bytes("key: ", DUMP_PREFIX_OFFSET, key_add_req->key.array, key_add_req->key.length);
#endif

	/* Send the MM_KEY_ADD_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, MM_KEY_ADD_CFM, cfm);

	kfree(msg);
	return ret;
}

int asr_send_key_del(struct asr_hw *asr_hw, u8 hw_key_idx)
{
	struct mm_key_del_req *key_del_req = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the MM_KEY_DEL_REQ message */
	msg = asr_msg_zalloc(MM_KEY_DEL_REQ, TASK_MM, DRV_TASK_ID, sizeof(struct mm_key_del_req));
	if (!msg)
		return -ENOMEM;

	key_del_req = (struct mm_key_del_req *)msg->param;

	/* Set parameters for the MM_KEY_DEL_REQ message */
	key_del_req->hw_key_idx = hw_key_idx;

	/* Send the MM_KEY_DEL_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, MM_KEY_DEL_CFM, NULL);

	kfree(msg);
	return ret;
}

int asr_send_bcn_change(struct asr_hw *asr_hw, u8 vif_idx, u8 * bcn_addr,
			u16 bcn_len, u16 tim_oft, u16 tim_len, u16 * csa_oft)
{
#ifdef CONFIG_OLD_USB
	return -ENOMEM;
#else

	struct lmac_msg *msg = NULL;
	int ret = 0;
	struct mm_bcn_change_req *req = NULL;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the MM_BCN_CHANGE_REQ message */
	msg = asr_msg_zalloc(MM_BCN_CHANGE_REQ, TASK_MM, DRV_TASK_ID, sizeof(struct mm_bcn_change_req));
	if (!msg)
		return -ENOMEM;

	req = (struct mm_bcn_change_req *)msg->param;

	/* Set parameters for the MM_BCN_CHANGE_REQ message */
	//req->bcn_ptr = bcn_addr;
	if (bcn_len < NX_BCNFRAME_LEN) {
		memset(req->bcn_ptr, 0, sizeof(NX_BCNFRAME_LEN));
		memcpy(req->bcn_ptr, bcn_addr, bcn_len);
		req->bcn_len = bcn_len;
	} else {
		dev_err(asr_hw->dev, "bcn_len(%d) out of range \n", bcn_len);

		kfree(msg);
		return -ENOMEM;
	}

	req->tim_oft = tim_oft;
	req->tim_len = tim_len;
	req->inst_nbr = vif_idx;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
	if (csa_oft) {
		int i;
		for (i = 0; i < BCN_MAX_CSA_CPT; i++) {
			req->csa_oft[i] = csa_oft[i];
		}
	}
#endif /* VERSION >= 3.16.0 */

	/* Send the MM_BCN_CHANGE_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, MM_BCN_CHANGE_CFM, NULL);

	kfree(msg);
	return ret;
#endif
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 10, 0)
void cfg80211_chandef_create(struct cfg80211_chan_def *chandef,
			     struct ieee80211_channel *chan, enum nl80211_channel_type chan_type)
{
	if (WARN_ON(!chan))
		return;

	chandef->chan = chan;
	chandef->center_freq2 = 0;

	switch (chan_type) {
	case NL80211_CHAN_NO_HT:
		chandef->width = NL80211_CHAN_WIDTH_20_NOHT;
		chandef->center_freq1 = chan->center_freq;
		break;
	case NL80211_CHAN_HT20:
		chandef->width = NL80211_CHAN_WIDTH_20;
		chandef->center_freq1 = chan->center_freq;
		break;
	case NL80211_CHAN_HT40PLUS:
		chandef->width = NL80211_CHAN_WIDTH_40;
		chandef->center_freq1 = chan->center_freq + 10;
		break;
	case NL80211_CHAN_HT40MINUS:
		chandef->width = NL80211_CHAN_WIDTH_40;
		chandef->center_freq1 = chan->center_freq - 10;
		break;
	default:
		WARN_ON(1);
	}
}
#endif

int asr_send_roc(struct asr_hw *asr_hw, struct asr_vif *vif, struct ieee80211_channel *chan, unsigned int duration)
{
	struct mm_remain_on_channel_req *req = NULL;
	struct cfg80211_chan_def chandef;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Create channel definition structure */
	cfg80211_chandef_create(&chandef, chan, NL80211_CHAN_NO_HT);

	/* Build the MM_REMAIN_ON_CHANNEL_REQ message */
	msg = asr_msg_zalloc(MM_REMAIN_ON_CHANNEL_REQ, TASK_MM, DRV_TASK_ID, sizeof(struct mm_remain_on_channel_req));
	if (!msg)
		return -ENOMEM;

	req = (struct mm_remain_on_channel_req *)msg->param;

	/* Set parameters for the MM_REMAIN_ON_CHANNEL_REQ message */
	req->op_code = MM_ROC_OP_START;
	req->vif_index = vif->vif_index;
	req->duration_ms = duration;
#ifdef CONFIG_ASR595X
	req->chan.band = chan->band;
	req->chan.type = bw2chnl[chandef.width];
	req->chan.prim20_freq = chan->center_freq;
	req->chan.center1_freq = chandef.center_freq1;
	req->chan.center2_freq = chandef.center_freq2;
	req->chan.tx_power = chan->max_power;
#else
	req->band = chan->band;
	req->type = bw2chnl[chandef.width];
	req->prim20_freq = chan->center_freq;
	req->center1_freq = chandef.center_freq1;
	req->center2_freq = chandef.center_freq2;
	req->tx_power = chan->max_power;
#endif

	/* Send the MM_REMAIN_ON_CHANNEL_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, MM_REMAIN_ON_CHANNEL_CFM, NULL);

	//dev_info(asr_hw->dev, "[send roc start] ret=%d,(%d %d %d)\n", ret, duration,chan->band,chan->center_freq);

	kfree(msg);
	return ret;
}

extern void asr_sched_timeout(int delay_jitter);
int asr_send_cancel_roc(struct asr_hw *asr_hw)
{
	struct mm_remain_on_channel_req *req = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the MM_REMAIN_ON_CHANNEL_REQ message */
	msg = asr_msg_zalloc(MM_REMAIN_ON_CHANNEL_REQ, TASK_MM, DRV_TASK_ID, sizeof(struct mm_remain_on_channel_req));
	if (!msg)
		return -ENOMEM;

	req = (struct mm_remain_on_channel_req *)msg->param;

	/* Set parameters for the MM_REMAIN_ON_CHANNEL_REQ message */
	req->op_code = MM_ROC_OP_CANCEL;

    // FIXME. add delay to workaround,avoid roc cancel before complete ngo action tx done.
    asr_sched_timeout(2);

	/* Send the MM_REMAIN_ON_CHANNEL_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 0, 0, NULL);
	dev_info(asr_hw->dev, "[send roc cancel] ret=%d \n", ret );

	kfree(msg);
	return ret;
}

int asr_send_set_power(struct asr_hw *asr_hw, u8 vif_idx, s8 pwr, struct mm_set_power_cfm *cfm)
{
	struct mm_set_power_req *req = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the MM_SET_POWER_REQ message */
	msg = asr_msg_zalloc(MM_SET_POWER_REQ, TASK_MM, DRV_TASK_ID, sizeof(struct mm_set_power_req));
	if (!msg)
		return -ENOMEM;

	req = (struct mm_set_power_req *)msg->param;

	/* Set parameters for the MM_SET_POWER_REQ message */
	req->inst_nbr = vif_idx;
	req->power = pwr;

	/* Send the MM_SET_POWER_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, MM_SET_POWER_CFM, cfm);

	kfree(msg);
	return ret;
}

int asr_send_set_edca(struct asr_hw *asr_hw, u8 hw_queue, u32 param, bool uapsd, u8 inst_nbr)
{
	struct mm_set_edca_req *set_edca_req = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the MM_SET_EDCA_REQ message */
	msg = asr_msg_zalloc(MM_SET_EDCA_REQ, TASK_MM, DRV_TASK_ID, sizeof(struct mm_set_edca_req));
	if (!msg)
		return -ENOMEM;

	set_edca_req = (struct mm_set_edca_req *)msg->param;

	/* Set parameters for the MM_SET_EDCA_REQ message */
	set_edca_req->ac_param = param;
	set_edca_req->uapsd = uapsd;
	set_edca_req->hw_queue = hw_queue;
	set_edca_req->inst_nbr = inst_nbr;

	/* Send the MM_SET_EDCA_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, MM_SET_EDCA_CFM, NULL);

	kfree(msg);
	return ret;
}

#ifdef CONFIG_ASR_P2P_DEBUGFS
int asr_send_p2p_oppps_req(struct asr_hw *asr_hw, struct asr_vif *asr_vif,
                            u8 ctw, struct mm_set_p2p_oppps_cfm *cfm)
{
    struct mm_set_p2p_oppps_req *p2p_oppps_req = NULL;
    int error;
    struct lmac_msg *msg = NULL;

    ASR_DBG(ASR_FN_ENTRY_STR);

    /* Build the MM_SET_P2P_OPPPS_REQ message */
    msg = asr_msg_zalloc(MM_SET_P2P_OPPPS_REQ, TASK_MM, DRV_TASK_ID,
                                    sizeof(struct mm_set_p2p_oppps_req));

    if (!msg) {
        return -ENOMEM;
    }

    p2p_oppps_req = (struct mm_set_p2p_oppps_req *)msg->param;

    /* Fill the message parameters */
    p2p_oppps_req->vif_index = asr_vif->vif_index;
    p2p_oppps_req->ctwindow = ctw;

    /* Send the MM_P2P_OPPPS_REQ message to LMAC FW */
    error = asr_send_msg(asr_hw, msg, 1, MM_SET_P2P_OPPPS_CFM, cfm);
    kfree(msg);

    return (error);
}

int asr_send_p2p_noa_req(struct asr_hw *asr_hw, struct asr_vif *asr_vif,
                          int count, int interval, int duration, bool dyn_noa,
                          struct mm_set_p2p_noa_cfm *cfm)
{
    struct mm_set_p2p_noa_req *p2p_noa_req = NULL;
    int error;
    struct lmac_msg *msg = NULL;

    ASR_DBG(ASR_FN_ENTRY_STR);

    /* Param check */
    if (count > 255)
        count = 255;

    if (duration >= interval) {
        dev_err(asr_hw->dev, "Invalid p2p NOA config: interval=%d <= duration=%d\n",
                interval, duration);
        return -EINVAL;
    }

    /* Build the MM_SET_P2P_NOA_REQ message */
    msg = asr_msg_zalloc(MM_SET_P2P_NOA_REQ, TASK_MM, DRV_TASK_ID,
                                  sizeof(struct mm_set_p2p_noa_req));

    if (!msg) {
        return -ENOMEM;
    }

    p2p_noa_req  = (struct mm_set_p2p_noa_req *)msg->param;

    /* Fill the message parameters */
    p2p_noa_req->vif_index = asr_vif->vif_index;
    p2p_noa_req->noa_inst_nb = 0;
    p2p_noa_req->count = count;

    if (count) {
        p2p_noa_req->duration_us = duration * 1024;
        p2p_noa_req->interval_us = interval * 1024;
        p2p_noa_req->start_offset = (interval - duration - 10) * 1024;
        p2p_noa_req->dyn_noa = dyn_noa;
    }

    /* Send the MM_SET_2P_NOA_REQ message to LMAC FW */
    error = asr_send_msg(asr_hw, msg, 1, MM_SET_P2P_NOA_CFM, cfm);
    kfree(msg);

    return (error);
}
#endif /* CONFIG_ASR_P2P_DEBUGFS */


/******************************************************************************
 *    Control messages handling functions (FULLMAC only)
 *****************************************************************************/
int asr_send_me_config_req(struct asr_hw *asr_hw)
{
	struct me_config_req *req = NULL;
	struct wiphy *wiphy = asr_hw->wiphy;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0)
	struct ieee80211_sta_ht_cap *ht_cap = &wiphy->bands[NL80211_BAND_2GHZ]->ht_cap;
#else
	struct ieee80211_sta_ht_cap *ht_cap = &wiphy->bands[IEEE80211_BAND_2GHZ]->ht_cap;
#endif

#ifdef CONFIG_ASR595X
	struct ieee80211_sta_he_cap const *he_cap;
#endif

	u8 *ht_mcs = (u8 *) & ht_cap->mcs.rx_mask;
	int i;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the ME_CONFIG_REQ message */
	msg = asr_msg_zalloc(ME_CONFIG_REQ, TASK_ME, DRV_TASK_ID, sizeof(struct me_config_req));
	if (!msg)
		return -ENOMEM;

	req = (struct me_config_req *)msg->param;

	/* Set parameters for the ME_CONFIG_REQ message */
	dev_info(asr_hw->dev, "HT supp %d\n", ht_cap->ht_supported);

	req->ht_supp = ht_cap->ht_supported;
	req->vht_supp = false;
	req->ht_cap.ht_capa_info = cpu_to_le16(ht_cap->cap);
	req->ht_cap.a_mpdu_param = ht_cap->ampdu_factor |
	    (ht_cap->ampdu_density << IEEE80211_HT_AMPDU_PARM_DENSITY_SHIFT);
	for (i = 0; i < sizeof(ht_cap->mcs.rx_mask); i++)
		req->ht_cap.mcs_rate[i] = ht_mcs[i];
	req->ht_cap.ht_extended_capa = 0;
	req->ht_cap.tx_beamforming_capa = 0;
	req->ht_cap.asel_capa = 0;

	req->vht_cap.vht_capa_info = 0;
	req->vht_cap.rx_highest = 0;
	req->vht_cap.rx_mcs_map = 0;
	req->vht_cap.tx_highest = 0;
	req->vht_cap.tx_mcs_map = 0;

#ifdef CONFIG_ASR595X
	if (wiphy->bands[NL80211_BAND_2GHZ]->iftype_data != NULL) {
		he_cap = &wiphy->bands[NL80211_BAND_2GHZ]->iftype_data->he_cap;

		req->he_supp = he_cap->has_he;
		for (i = 0; i < ARRAY_SIZE(he_cap->he_cap_elem.mac_cap_info); i++) {
			req->he_cap.mac_cap_info[i] = he_cap->he_cap_elem.mac_cap_info[i];
		}
		for (i = 0; i < ARRAY_SIZE(he_cap->he_cap_elem.phy_cap_info); i++) {
			req->he_cap.phy_cap_info[i] = he_cap->he_cap_elem.phy_cap_info[i];
		}
		req->he_cap.mcs_supp.rx_mcs_80 = cpu_to_le16(he_cap->he_mcs_nss_supp.rx_mcs_80);
		req->he_cap.mcs_supp.tx_mcs_80 = cpu_to_le16(he_cap->he_mcs_nss_supp.tx_mcs_80);
		req->he_cap.mcs_supp.rx_mcs_160 = cpu_to_le16(he_cap->he_mcs_nss_supp.rx_mcs_160);
		req->he_cap.mcs_supp.tx_mcs_160 = cpu_to_le16(he_cap->he_mcs_nss_supp.tx_mcs_160);
		req->he_cap.mcs_supp.rx_mcs_80p80 = cpu_to_le16(he_cap->he_mcs_nss_supp.rx_mcs_80p80);
		req->he_cap.mcs_supp.tx_mcs_80p80 = cpu_to_le16(he_cap->he_mcs_nss_supp.tx_mcs_80p80);
		for (i = 0; i < MAC_HE_PPE_THRES_MAX_LEN; i++) {
			req->he_cap.ppe_thres[i] = he_cap->ppe_thres[i];
		}
		req->he_ul_on = asr_hw->mod_params->he_ul_on;
	}
	if (asr_hw->mod_params->use_80)
		req->phy_bw_max = PHY_CHNL_BW_80;
	else if (asr_hw->mod_params->use_2040)
		req->phy_bw_max = PHY_CHNL_BW_40;
	else
		req->phy_bw_max = PHY_CHNL_BW_20;
#endif
	req->ps_on = asr_hw->mod_params->ps_on;
	req->tx_lft = asr_hw->mod_params->tx_lft;
#ifdef CONFIG_ASR5531
	req->ant_div_on = asr_hw->mod_params->ant_div;
#endif

	/* Send the ME_CONFIG_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, ME_CONFIG_CFM, NULL);

	kfree(msg);
	return ret;
}

int asr_send_me_chan_config_req(struct asr_hw *asr_hw)
{
	struct me_chan_config_req *req = NULL;
	struct wiphy *wiphy = asr_hw->wiphy;
	int i;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the ME_CHAN_CONFIG_REQ message */
	msg = asr_msg_zalloc(ME_CHAN_CONFIG_REQ, TASK_ME, DRV_TASK_ID, sizeof(struct me_chan_config_req));
	if (!msg)
		return -ENOMEM;

	req = (struct me_chan_config_req *)msg->param;

	req->chan2G4_cnt = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 1)
	if (wiphy->bands[NL80211_BAND_2GHZ] != NULL)
#else
	if (wiphy->bands[IEEE80211_BAND_2GHZ] != NULL)
#endif
	{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 1)
		struct ieee80211_supported_band *b = wiphy->bands[NL80211_BAND_2GHZ];
#else
		struct ieee80211_supported_band *b = wiphy->bands[IEEE80211_BAND_2GHZ];
#endif
		for (i = 0; i < b->n_channels; i++) {
			req->chan2G4[req->chan2G4_cnt].flags = 0;
			if (b->channels[i].flags & IEEE80211_CHAN_DISABLED)
				req->chan2G4[req->chan2G4_cnt].flags |= SCAN_DISABLED_BIT;
			req->chan2G4[req->chan2G4_cnt].flags |= passive_scan_flag(b->channels[i].flags);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 1)
			req->chan2G4[req->chan2G4_cnt].band = NL80211_BAND_2GHZ;
#else
			req->chan2G4[req->chan2G4_cnt].band = IEEE80211_BAND_2GHZ;
#endif
			req->chan2G4[req->chan2G4_cnt].freq = b->channels[i].center_freq;
			req->chan2G4[req->chan2G4_cnt].tx_power = b->channels[i].max_power;
			req->chan2G4_cnt++;
			if (req->chan2G4_cnt == SCAN_CHANNEL_2G4)
				break;
		}
	}

	req->chan5G_cnt = 0;

	/* Send the ME_CHAN_CONFIG_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, ME_CHAN_CONFIG_CFM, NULL);

	kfree(msg);
	return ret;
}

int asr_send_me_set_control_port_req(struct asr_hw *asr_hw, bool opened, u8 sta_idx)
{
	struct me_set_control_port_req *req = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the ME_SET_CONTROL_PORT_REQ message */
	msg = asr_msg_zalloc(ME_SET_CONTROL_PORT_REQ, TASK_ME, DRV_TASK_ID, sizeof(struct me_set_control_port_req));
	if (!msg)
		return -ENOMEM;

	req = (struct me_set_control_port_req *)msg->param;

	/* Set parameters for the ME_SET_CONTROL_PORT_REQ message */
	req->sta_idx = sta_idx;
	req->control_port_open = opened;

	/* Send the ME_SET_CONTROL_PORT_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, ME_SET_CONTROL_PORT_CFM, NULL);

	kfree(msg);
	return ret;
}

int asr_send_me_sta_add(struct asr_hw *asr_hw,
			struct station_parameters *params, const u8 * mac, u8 inst_nbr, struct me_sta_add_cfm *cfm)
{
	struct me_sta_add_req *req = NULL;
	u8 *ht_mcs = NULL;
	int i;
	struct lmac_msg *msg = NULL;
	int ret = 0;
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 1)
	struct link_station_parameters *sta_param = NULL;
	#else
	struct station_parameters *sta_param = NULL;
	#endif

	ASR_DBG(ASR_FN_ENTRY_STR);

	if (!params) {
		return -EFAULT;
	}

	#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
	sta_param = &params->link_sta_params;
	#else
	sta_param = params;
	#endif

	/* Build the MM_STA_ADD_REQ message */
	msg = asr_msg_zalloc(ME_STA_ADD_REQ, TASK_ME, DRV_TASK_ID, sizeof(struct me_sta_add_req));
	if (!msg)
		return -ENOMEM;

	req = (struct me_sta_add_req *)msg->param;

	/* Set parameters for the MM_STA_ADD_REQ message */
	memcpy(&(req->mac_addr.array[0]), mac, ETH_ALEN);

	req->rate_set.length = sta_param->supported_rates_len;
	for (i = 0; i < sta_param->supported_rates_len; i++)
		req->rate_set.array[i] = sta_param->supported_rates[i];

	req->flags = 0;
	if (sta_param->ht_capa) {
		const struct ieee80211_ht_cap *ht_capa = sta_param->ht_capa;

		ht_mcs = (u8 *) & sta_param->ht_capa->mcs.rx_mask;

		req->flags |= STA_HT_CAPA;
		req->ht_cap.ht_capa_info = cpu_to_le16(ht_capa->cap_info);
		req->ht_cap.a_mpdu_param = ht_capa->ampdu_params_info;
		for (i = 0; i < sizeof(ht_capa->mcs.rx_mask); i++)
			req->ht_cap.mcs_rate[i] = ht_mcs[i];
		req->ht_cap.ht_extended_capa = cpu_to_le16(ht_capa->extended_ht_cap_info);
		req->ht_cap.tx_beamforming_capa = cpu_to_le32(ht_capa->tx_BF_cap_info);
		req->ht_cap.asel_capa = ht_capa->antenna_selection_info;
	}
#ifdef CONFIG_ASR595X
	if (sta_param->he_capa) {
		const struct ieee80211_he_cap_elem *he_capa = sta_param->he_capa;
		struct ieee80211_he_mcs_nss_supp *mcs_nss_supp = (struct ieee80211_he_mcs_nss_supp *)(he_capa + 1);

		req->flags |= STA_HE_CAPA;
		for (i = 0; i < sizeof(he_capa->mac_cap_info); i++)
			req->he_cap.mac_cap_info[i] = he_capa->mac_cap_info[i];
		for (i = 0; i < sizeof(he_capa->phy_cap_info); i++)
			req->he_cap.phy_cap_info[i] = he_capa->phy_cap_info[i];
		req->he_cap.mcs_supp.rx_mcs_80 = mcs_nss_supp->rx_mcs_80;
		req->he_cap.mcs_supp.tx_mcs_80 = mcs_nss_supp->tx_mcs_80;

		/* not support above 80MHz */
		//req->he_cap.mcs_supp.rx_mcs_160 = mcs_nss_supp->rx_mcs_160;
		//req->he_cap.mcs_supp.tx_mcs_160 = mcs_nss_supp->tx_mcs_160;
		//req->he_cap.mcs_supp.rx_mcs_80p80 = mcs_nss_supp->rx_mcs_80p80;
		//req->he_cap.mcs_supp.tx_mcs_80p80 = mcs_nss_supp->tx_mcs_80p80;
	}
#endif

	if (params->sta_flags_set & BIT(NL80211_STA_FLAG_WME))
		req->flags |= STA_QOS_CAPA;

	if (params->sta_flags_set & BIT(NL80211_STA_FLAG_MFP))
		req->flags |= STA_MFP_CAPA;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	if (sta_param->opmode_notif_used) {
		req->flags |= STA_OPMOD_NOTIF;
		req->opmode = sta_param->opmode_notif;
	}
#endif

	req->aid = cpu_to_le16(params->aid);
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
	req->uapsd_queues = params->uapsd_queues;
	req->max_sp_len = params->max_sp * 2;
#endif
	req->vif_idx = inst_nbr;

	/* Send the ME_STA_ADD_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, ME_STA_ADD_CFM, cfm);

	kfree(msg);
	return ret;
}

int asr_send_me_sta_del(struct asr_hw *asr_hw, u8 sta_idx)
{
	struct me_sta_del_req *req = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the MM_STA_DEL_REQ message */
	msg = asr_msg_zalloc(ME_STA_DEL_REQ, TASK_ME, DRV_TASK_ID, sizeof(struct me_sta_del_req));
	if (!msg)
		return -ENOMEM;

	req = (struct me_sta_del_req *)msg->param;

	/* Set parameters for the MM_STA_DEL_REQ message */
	req->sta_idx = sta_idx;

	/* Send the ME_STA_DEL_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, ME_STA_DEL_CFM, NULL);

	kfree(msg);
	return ret;
}

int asr_send_me_traffic_ind(struct asr_hw *asr_hw, u8 sta_idx, bool uapsd, u8 tx_status)
{
	struct me_traffic_ind_req *req = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the ME_UTRAFFIC_IND_REQ message */
	msg = asr_msg_zalloc(ME_TRAFFIC_IND_REQ, TASK_ME, DRV_TASK_ID, sizeof(struct me_traffic_ind_req));
	if (!msg)
		return -ENOMEM;

	req = (struct me_traffic_ind_req *)msg->param;

	/* Set parameters for the ME_TRAFFIC_IND_REQ message */
	req->sta_idx = sta_idx;
	req->tx_avail = tx_status;
	req->uapsd = uapsd;

	/* Send the ME_TRAFFIC_IND_REQ to UMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, ME_TRAFFIC_IND_CFM, NULL);

	kfree(msg);
	return ret;
}

int asr_send_me_rc_stats(struct asr_hw *asr_hw, u8 sta_idx, struct me_rc_stats_cfm *cfm)
{
	struct me_rc_stats_req *req = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the ME_RC_STATS_REQ message */
	msg = asr_msg_zalloc(ME_RC_STATS_REQ, TASK_ME, DRV_TASK_ID, sizeof(struct me_rc_stats_req));
	if (!msg)
		return -ENOMEM;

	req = (struct me_rc_stats_req *)msg->param;

	/* Set parameters for the ME_RC_STATS_REQ message */
	req->sta_idx = sta_idx;

	/* Send the ME_RC_STATS_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, ME_RC_STATS_CFM, cfm);

	kfree(msg);
	return ret;
}

int asr_send_me_rc_set_rate(struct asr_hw *asr_hw, u8 sta_idx, u16 rate_cfg)
{
	struct me_rc_set_rate_req *req = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the ME_RC_SET_RATE_REQ message */
	msg = asr_msg_zalloc(ME_RC_SET_RATE_REQ, TASK_ME, DRV_TASK_ID, sizeof(struct me_rc_set_rate_req));
	if (!msg)
		return -ENOMEM;

	req = (struct me_rc_set_rate_req *)msg->param;

	/* Set parameters for the ME_RC_SET_RATE_REQ message */
	req->sta_idx = sta_idx;
	req->fixed_rate_cfg = rate_cfg;

	/* Send the ME_RC_SET_RATE_REQ message to FW */
	ret = asr_send_msg(asr_hw, msg, 0, 0, NULL);

	kfree(msg);
	return ret;
}

int asr_send_me_host_dbg_cmd(struct asr_hw *asr_hw, unsigned int host_dbg_cmd, unsigned int reg, unsigned int value)
{
	struct me_host_dbg_cmd_req *req = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the ME_RC_SET_RATE_REQ message */
	msg = asr_msg_zalloc(ME_HOST_DBG_CMD_REQ, TASK_ME, DRV_TASK_ID, sizeof(struct me_host_dbg_cmd_req));
	if (!msg)
		return -ENOMEM;

	req = (struct me_host_dbg_cmd_req *)msg->param;

	/* Set parameters for the ME_RC_SET_RATE_REQ message */
	req->host_dbg_cmd = host_dbg_cmd;
	req->host_dbg_reg = reg;
	req->host_dbg_value = value;

	/* Send the ME_RC_SET_RATE_REQ message to FW */
	ret = asr_send_msg(asr_hw, msg, 0, 0, NULL);

	kfree(msg);
	return ret;
}

#ifdef CONFIG_SME
int asr_send_sm_auth_req(struct asr_hw *asr_hw,
			 struct asr_vif *asr_vif, struct cfg80211_auth_request *auth_req, struct sm_auth_cfm *cfm)
{
	struct sm_auth_req *req = NULL;
	int i;
	u32 flags = 0;
	u8 ssid[32] = { 0 };
	#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 8, 0)
	const struct cfg80211_bss_ies *ies;
	#else
	const u8 *ies;
	#endif
	const u8 *ies_data = NULL;
	int ies_len = 0;
	const u8 *ssidie = NULL;
	size_t ssid_len = 0;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	if (!asr_vif || !asr_hw) {
		return -EBUSY;
	}

	/* Build the SM_CONNECT_REQ message */
	msg = asr_msg_zalloc(SM_AUTH_REQ, TASK_SM, DRV_TASK_ID, sizeof(struct sm_auth_req));
	if (!msg)
		return -ENOMEM;

	req = (struct sm_auth_req *)msg->param;

	/* Set parameters for the SM_AUTH_REQ message */

	memset(asr_vif->bssid, 0xff, ETH_ALEN);
	memset(asr_vif->ssid, 0, IEEE80211_MAX_SSID_LEN);
	asr_vif->ssid_len = 0;

	/// get SSID from BSS
	#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 8, 0)
	ies = rcu_access_pointer(auth_req->bss->ies);
	#else
	ies = rcu_access_pointer(auth_req->bss->information_elements);
	#endif
	if (!ies) {
		dev_err(asr_hw->dev, "asr_send_sm_auth_req ies=NULL \n");
		kfree(msg);
		return -EFAULT;
	} else {
		#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 8, 0)
		ies_len = ies->len;
		ies_data = ies->data;
		#else
		ies_len = auth_req->bss->len_information_elements;
		ies_data = ies;
		#endif
		dev_info(asr_hw->dev, "asr_send_sm_auth_req ies len=%d \n", ies_len);

		ssidie = cfg80211_find_ie(WLAN_EID_SSID, ies_data, ies_len);

		if (!ssidie) {
			dev_err(asr_hw->dev, "asr_send_sm_auth_req ssidie=NULL \n");
			kfree(msg);
			return -EFAULT;
		} else {
			ssid_len = ssidie[1];
			if ((ssid_len <= 32) && (ssid_len > 0)) {
				memcpy(ssid, ssidie + 2, ssid_len);
				// set ssid to auth req
				for (i = 0; i < ssid_len; i++)
					req->ssid.array[i] = ssid[i];
				req->ssid.length = ssid_len;

				memcpy(asr_vif->ssid, ssid, ssid_len);
				asr_vif->ssid_len = ssid_len;

			} else {
				dev_err(asr_hw->dev, "asr_send_sm_auth_req ssid_len=%u \n", (unsigned int)ssid_len);
			}
		}
	}

	dev_info(asr_hw->dev, "  * SSID=(%s) \n", ssid);

	/// BSSID to auth to (if not specified, set this field to WILDCARD BSSID)
	//if (auth_req->bss->bssid)
	memcpy(&req->bssid, auth_req->bss->bssid, ETH_ALEN);
	//else
	//      req->bssid = mac_addr_bcst;

	memcpy(asr_vif->bssid, &req->bssid, ETH_ALEN);

	dev_info(asr_hw->dev, "  * BSSID=(%x:%x:%x) \n", req->bssid.array[0], req->bssid.array[1], req->bssid.array[2]);

	/// Channel on which we have to connect (if not specified, set -1 in the chan.freq field)
	if (auth_req->bss->channel) {
		req->chan.band = auth_req->bss->channel->band;
		req->chan.freq = auth_req->bss->channel->center_freq;
		req->chan.flags = passive_scan_flag(auth_req->bss->channel->flags);
	} else {
		req->chan.freq = (u16) - 1;
	}

	req->flags = flags;

	asr_vif->auth_type = auth_req->auth_type;

	/// Authentication type
	/* Set auth_type */
	if (auth_req->auth_type == NL80211_AUTHTYPE_AUTOMATIC) {
		req->auth_type = NL80211_AUTHTYPE_OPEN_SYSTEM;
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 8, 0)
	} else if (auth_req->auth_type == NL80211_AUTHTYPE_SAE) {
		req->auth_type = MAC_AUTH_ALGO_SAE;
#endif
	} else {
		req->auth_type = auth_req->auth_type;
	}

	/// VIF index
	req->vif_idx = asr_vif->vif_index;
	dev_info(asr_hw->dev, "  * ch (band=%d  freq=%d flags=%d),flags=0x%x,auth_type=%d,vif_idx=%d  \n",
		 req->chan.band, req->chan.freq, req->chan.flags, req->flags, req->auth_type, req->vif_idx);

	// Extra IEs to add to Authentication frame or %NULL
	if (WARN_ON(auth_req->ie_len > sizeof(req->ie_buf))) {
		kfree(msg);
		return -EINVAL;
	}
	if (auth_req->ie_len)
		memcpy(req->ie_buf, auth_req->ie, auth_req->ie_len);
	req->ie_len = auth_req->ie_len;
	dev_info(asr_hw->dev, "  * ch :ie_len=%d,total_len=%u  \n", req->ie_len, (unsigned int)sizeof(struct sm_auth_req));

        // clear sae data and length.
        memset(req->sae_data,0,sizeof(req->sae_data));
        req->sae_data_len = 0;

#ifdef CONFIG_SAE
	// Non-IE data to use with SAE or %NULL.
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	if (WARN_ON(auth_req->auth_data_len > sizeof(req->sae_data))) {
		// for now max sae data len =512, which is enough for ecc group19.
		dev_err(asr_hw->dev, "  * sae data len out of range!!!(%u > %d)  \n", (unsigned int)(auth_req->auth_data_len),
			req->sae_data_len);

		kfree(msg);
		return -EINVAL;
	}
	if (auth_req->auth_data_len)
		memcpy(req->sae_data, auth_req->auth_data, auth_req->auth_data_len);
	req->sae_data_len = auth_req->auth_data_len;
	dev_err(asr_hw->dev, "  * ch :seq=%d,sae_len=%d,total_len=%u  \n", *((uint16_t*)(req->sae_data)), req->sae_data_len,
		(unsigned int)sizeof(struct sm_auth_req));
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
	if (WARN_ON(auth_req->sae_data_len > sizeof(req->sae_data))) {
		// for now max sae data len =512, which is enough for ecc group19.
		dev_err(asr_hw->dev, "  * sae data len out of range!!!(%u > %d)  \n",
			(unsigned int)auth_req->sae_data_len, req->sae_data_len);

		kfree(msg);
		return -EINVAL;
	}
	if (auth_req->sae_data_len)
		memcpy(req->sae_data, auth_req->sae_data, auth_req->sae_data_len);
	req->sae_data_len = auth_req->sae_data_len;
	dev_info(asr_hw->dev, "  * ch :seq=%d,sae_len=%d,total_len=%u  \n", *((uint16_t*)(req->sae_data)), req->sae_data_len,
		(unsigned int)sizeof(struct sm_auth_req));
#endif
#endif
	asr_vif->auth_seq = *((u16*)(req->sae_data));
	/* Send the SM_AUTH_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, SM_AUTH_CFM, cfm);
	if (cfm->status != CO_OK) {
		ret = -EFAULT;
		dev_err(asr_hw->dev, "%s: cfm fail status=%d.\n", __func__, cfm->status);
	}

	kfree(msg);
	return ret;
}

int asr_send_sm_assoc_req(struct asr_hw *asr_hw,
			  struct asr_vif *asr_vif, struct cfg80211_assoc_request *assoc_req, struct sm_assoc_cfm *cfm)
{
	struct sm_assoc_req *req = NULL;
	// int i;
	u32 flags = 0;
	struct lmac_msg *msg = NULL;
	int ret = 0;
    bool is_wapi = false;
	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the SM_CONNECT_REQ message */
	msg = asr_msg_zalloc(SM_ASSOC_REQ, TASK_SM, DRV_TASK_ID, sizeof(struct sm_assoc_req));
	if (!msg)
		return -ENOMEM;

	req = (struct sm_assoc_req *)msg->param;

	/* Set parameters for the SM_ASSOC_REQ message */
	/// BSSID to assoc to (if not specified, set this field to WILDCARD BSSID)
	//if (assoc_req->bss->bssid)
	memcpy(&req->bssid, assoc_req->bss->bssid, ETH_ALEN);
	//else
	//      req->bssid = mac_addr_bcst;

	/// Listen interval to be used for this connection
	req->listen_interval = asr_module_params.listen_itv;

	/// Flag indicating if the we have to wait for the BC/MC traffic after beacon or not
	req->dont_wait_bcmc = !asr_module_params.listen_bcmc;

	if (assoc_req->crypto.n_ciphers_pairwise && ((assoc_req->crypto.ciphers_pairwise[0] == WLAN_CIPHER_SUITE_WEP40)
						     || (assoc_req->crypto.ciphers_pairwise[0] ==
							 WLAN_CIPHER_SUITE_TKIP)
						     || (assoc_req->crypto.ciphers_pairwise[0] ==
							 WLAN_CIPHER_SUITE_WEP104)))
		flags |= DISABLE_HT;

	if (assoc_req->crypto.control_port)
		flags |= CONTROL_PORT_HOST;

	//if (assoc_req->crypto.control_port_no_encrypt) //temp mask to prevent tx fourth epol after send add key cmd in fw.
	flags |= CONTROL_PORT_NO_ENC;

	if (use_pairwise_key(&assoc_req->crypto))
		flags |= WPA_WPA2_IN_USE;

	if (assoc_req->use_mfp != NL80211_MFP_NO)
		flags |= MFP_IN_USE;

	/// Control port Ethertype
	#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 2, 0)
	if (assoc_req->crypto.n_ciphers_pairwise && (assoc_req->crypto.ciphers_pairwise[0] == WLAN_CIPHER_SUITE_SMS4)) {
		is_wapi = true;
		req->ctrl_port_ethertype = cpu_to_be16(0x88B4);
                dev_err(asr_hw->dev, "%s: wapi,ctrl port.\n", __func__ );
	}
	else
	#endif
	if (assoc_req->crypto.control_port_ethertype){
		req->ctrl_port_ethertype = assoc_req->crypto.control_port_ethertype;
                dev_err(asr_hw->dev, "%s: ctrl port = 0x%x.\n", __func__, assoc_req->crypto.control_port_ethertype);
	}
	else {
		req->ctrl_port_ethertype = cpu_to_be16(ETH_P_PAE);
                dev_err(asr_hw->dev, "%s: default,ctrl port.\n", __func__ );
	}

	/// associate flags
	req->flags = flags;

	/// UAPSD queues (bit0: VO, bit1: VI, bit2: BE, bit3: BK)
	req->uapsd_queues = asr_module_params.uapsd_queues;

	//Use management frame protection (IEEE 802.11w) in this association
	req->use_mfp = assoc_req->use_mfp;

	/// VIF index
	req->vif_idx = asr_vif->vif_index;

	/// Extra IEs to add to (Re)Association Request frame or %NULL
	if (WARN_ON(assoc_req->ie_len > sizeof(req->ie_buf))) {
		kfree(msg);
		return -EINVAL;
	}
	if (assoc_req->ie_len)
		memcpy(req->ie_buf, assoc_req->ie, assoc_req->ie_len);
	req->ie_len = assoc_req->ie_len;

	if (is_wapi && assoc_req->ie_len)
        {
            // search WAPI IE and modify.
            u8 * ie_addr_start = (u8 *)req->ie_buf;
	    u8 * ie_addr = (u8 *)req->ie_buf;
	    u8 * ie_end =  ie_addr_start + req->ie_len;
	    int ie_total_len = req->ie_len;

            u8 wapi_buf[256] = {0};
            int offset = 0;
	    int copy_len = 0;

	    while (ie_addr < ie_end) {

                  if (68 == *(ie_addr)) {
		  // find wapi ie.
		        dev_err(asr_hw->dev, "%s: wapi ie modify(%d , %p ,%p ).\n", __func__ , offset, ie_addr,ie_addr_start );

			// copy pre IE.
			copy_len = offset + *(ie_addr + 1) + 2 ;

		        dev_err(asr_hw->dev, "%s: wapi ie modify, cp_len(%d ).\n", __func__ , copy_len );


                        memcpy(wapi_buf,ie_addr_start,copy_len );

			// set bk id count and modify len.
			memset(wapi_buf + copy_len, 0x0, 2);
                        wapi_buf[offset+1] += 2;

                        // copy last ie.
			memcpy(wapi_buf + copy_len +2 , ie_addr_start + copy_len , ie_total_len - copy_len);

			// overwrite req
       	                req->ie_len += 2;
	                memcpy(req->ie_buf, wapi_buf, req->ie_len);

			break;

		  }

		  // move on the next ie.
		  ie_addr += *(ie_addr + 1) + 2 ;
		  offset = ie_addr - ie_addr_start;
	    }

	 }

	/* Send the SM_ASSOC_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, SM_ASSOC_CFM, cfm);
	if (cfm->status != CO_OK) {
		ret = -EFAULT;
		dev_err(asr_hw->dev, "%s: cfm fail status=%d.\n", __func__, cfm->status);
	}

	kfree(msg);
	return ret;
}
#endif

#ifdef CONFIG_TWT
int asr_send_itwt_config(struct asr_hw *asr_hw, wifi_twt_config_t * wifi_twt_param)
{
	struct lmac_msg *msg = NULL;
	struct me_itwt_config_req *req = NULL;
	struct asr_vif *asr_vif = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the ME_CHAN_CONFIG_REQ message */
	msg = asr_msg_zalloc(ME_ITWT_CONFIG_REQ, TASK_ME, DRV_TASK_ID, sizeof(struct me_itwt_config_req));
	if (!msg)
		return -ENOMEM;

	if (asr_hw->vif_index < asr_hw->vif_max_num) {
		asr_vif = asr_hw->vif_table[asr_hw->vif_index];
	}

	// clear twt drv flag first.
	if (asr_vif)
	    clear_bit(ASR_DEV_STA_OUT_TWTSP, &asr_vif->dev_flags);

	req = (struct me_itwt_config_req *)msg->param;

	/* Set parameters for the ME_ITWT_CONFIG_REQ message */
	req->setup_cmd = wifi_twt_param->setup_cmd;	//0 :request 1:suggest 2:demand
	req->trigger = 1;
	req->implicit = 1;
	req->flow_type = wifi_twt_param->flow_type;
	req->twt_channel = 0;
	req->twt_prot = 0;
	req->wake_interval_exponent = wifi_twt_param->wake_interval_exponent;
	req->wake_interval_mantissa = wifi_twt_param->wake_interval_mantissa;
	req->target_wake_time_lo = 250000;
	req->monimal_min_wake_duration = wifi_twt_param->monimal_min_wake_duration;
	req->twt_upload_wait_trigger = wifi_twt_param->twt_upload_wait_trigger;

	/* Send the ME_ITWT_CONFIG_REQ message to FW */
	ret = asr_send_msg(asr_hw, msg, 0, 0, NULL);

	kfree(msg);
	return ret;
}

int asr_send_itwt_del(struct asr_hw *asr_hw, uint8_t flow_id)
{
	struct lmac_msg *msg = NULL;
	struct me_itwt_del_req *req = NULL;
	int ret = 0;
	struct asr_vif *asr_vif = NULL;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the ME_CHAN_CONFIG_REQ message */
	msg = asr_msg_zalloc(ME_ITWT_DEL_REQ, TASK_ME, DRV_TASK_ID, sizeof(struct me_itwt_del_req));
	if (!msg)
		return -ENOMEM;

	if (asr_hw->vif_index < asr_hw->vif_max_num) {
		asr_vif = asr_hw->vif_table[asr_hw->vif_index];
	}

	// clear twt drv flag first.
	if (asr_vif)
	    clear_bit(ASR_DEV_STA_OUT_TWTSP, &asr_vif->dev_flags);

	req = (struct me_itwt_del_req *)msg->param;

	/* Set parameters for the ME_ITWT_CONFIG_REQ message */
	req->flow_id = flow_id;

	/* Send the ME_ITWT_DEL_REQ message to FW */
	ret = asr_send_msg(asr_hw, msg, 0, 0, NULL);

	kfree(msg);
	return ret;
}
#endif

int asr_send_sm_connect_req(struct asr_hw *asr_hw,
			    struct asr_vif *asr_vif, struct cfg80211_connect_params *sme, struct sm_connect_cfm *cfm)
{
	struct sm_connect_req *req = NULL;
	int i;
	u32 flags = 0;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the SM_CONNECT_REQ message */
	msg = asr_msg_zalloc(SM_CONNECT_REQ, TASK_SM, DRV_TASK_ID, sizeof(struct sm_connect_req));
	if (!msg)
		return -ENOMEM;

	req = (struct sm_connect_req *)msg->param;

	/* Set parameters for the SM_CONNECT_REQ message */
	if (sme->crypto.n_ciphers_pairwise &&
	    ((sme->crypto.ciphers_pairwise[0] == WLAN_CIPHER_SUITE_WEP40) ||
	     (sme->crypto.ciphers_pairwise[0] == WLAN_CIPHER_SUITE_TKIP) ||
	     (sme->crypto.ciphers_pairwise[0] == WLAN_CIPHER_SUITE_WEP104)))
		flags |= DISABLE_HT;

	if (sme->crypto.control_port)
		flags |= CONTROL_PORT_HOST;

	if (sme->crypto.control_port_no_encrypt)
		flags |= CONTROL_PORT_NO_ENC;

	if (use_pairwise_key(&sme->crypto))
		flags |= WPA_WPA2_IN_USE;

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
	if (sme->mfp == NL80211_MFP_REQUIRED)
		flags |= MFP_IN_USE;
#endif

	if (sme->crypto.control_port_ethertype)
		req->ctrl_port_ethertype = sme->crypto.control_port_ethertype;
	else
		req->ctrl_port_ethertype = ETH_P_PAE;

	if (sme->bssid)
		memcpy(&req->bssid, sme->bssid, ETH_ALEN);
	else
		req->bssid = mac_addr_bcst;
	req->vif_idx = asr_vif->vif_index;
	if (sme->channel) {
		req->chan.band = sme->channel->band;
		req->chan.freq = sme->channel->center_freq;
		req->chan.flags = passive_scan_flag(sme->channel->flags);
	} else {
		req->chan.freq = (u16) - 1;
	}
	for (i = 0; i < sme->ssid_len; i++)
		req->ssid.array[i] = sme->ssid[i];
	req->ssid.length = sme->ssid_len;
	req->flags = flags;
	if (WARN_ON(sme->ie_len > sizeof(req->ie_buf))) {
		kfree(msg);
		return -EINVAL;
	}
	if (sme->ie_len)
		memcpy(req->ie_buf, sme->ie, sme->ie_len);
	req->ie_len = sme->ie_len;
	req->listen_interval = asr_module_params.listen_itv;
	req->dont_wait_bcmc = !asr_module_params.listen_bcmc;

	/* Set auth_type */
	if (sme->auth_type == NL80211_AUTHTYPE_AUTOMATIC)
		req->auth_type = NL80211_AUTHTYPE_OPEN_SYSTEM;
	else
		req->auth_type = sme->auth_type;

	/* Set UAPSD queues */
	req->uapsd_queues = asr_module_params.uapsd_queues;

	dev_info(asr_hw->dev, "%s: SSID=(%s),BSSID=(%x:%x:%x),auth_type=%d,flags=0x%X,ch(band=%d,freq=%d,flags=%d)\n",
		__func__, req->ssid.array, req->bssid.array[0], req->bssid.array[1], req->bssid.array[2],
		req->auth_type, req->flags, req->chan.band, req->chan.freq, req->chan.flags);

	/* Send the SM_CONNECT_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, SM_CONNECT_CFM, cfm);

	kfree(msg);
	return ret;
}

int asr_send_sm_disconnect_req(struct asr_hw *asr_hw, struct asr_vif *asr_vif, u16 reason)
{
	struct sm_disconnect_req *req = NULL;
	u32 timeout = 300;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	if (!asr_vif || !asr_hw) {
		return -EBUSY;
	}

	if (test_bit(ASR_DEV_CLOSEING, &asr_vif->dev_flags)
	    || !test_bit(ASR_DEV_STARTED, &asr_hw->phy_flags)) {
		return -EBUSY;
	}


	//#ifdef CFG_ROAMING
	//if(!asr_vif->is_roam)
	//#endif
	set_bit(ASR_DEV_STA_DISCONNECTING, &asr_vif->dev_flags);

	while (test_bit(ASR_DEV_SCANING, &asr_vif->dev_flags) && timeout--) {
		msleep(10);
	}

	if (test_bit(ASR_DEV_SCANING, &asr_vif->dev_flags)) {
		dev_info(asr_hw->dev, "%s: warnning disconnect in scan\n", __func__);
	}

	memset(asr_vif->ssid, 0, IEEE80211_MAX_SSID_LEN);
	asr_vif->ssid_len = 0;

	/* Build the SM_DISCONNECT_REQ message */
	msg = asr_msg_zalloc(SM_DISCONNECT_REQ, TASK_SM, DRV_TASK_ID, sizeof(struct sm_disconnect_req));
	if (!msg) {
		clear_bit(ASR_DEV_STA_DISCONNECTING, &asr_vif->dev_flags);
		return -ENOMEM;
	}

	req = (struct sm_disconnect_req *)msg->param;

	/* Set parameters for the SM_DISCONNECT_REQ message */
	req->reason_code = reason;
	req->vif_idx = asr_vif->vif_index;

	/* Send the SM_DISCONNECT_REQ message to LMAC FW */
	//return asr_send_msg(asr_hw, req, 1, SM_DISCONNECT_IND, NULL);
	ret = asr_send_msg(asr_hw, msg, 1, SM_DISCONNECT_CFM, NULL);
	if (ret || !test_bit(ASR_DEV_STA_CONNECTED, &asr_vif->dev_flags)) {
		clear_bit(ASR_DEV_STA_DISCONNECTING, &asr_vif->dev_flags);
	}

	kfree(msg);
	return ret;
}

int asr_send_apm_start_req(struct asr_hw *asr_hw, struct asr_vif *vif,
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 4, 0)
			   struct cfg80211_ap_settings *settings,
#else
			   struct beacon_parameters *info,
#endif
			   struct apm_start_cfm *cfm, struct asr_dma_elem *elem)
{
	struct apm_start_req *req = NULL;
	struct asr_bcn *bcn = &vif->ap.bcn;
	u8 *buf;
	u32 flags = 0;
	const u8 *rate_ie;
	u8 rate_len = 0;
	int var_offset = offsetof(struct ieee80211_mgmt, u.beacon.variable);
	const u8 *var_pos;
	int len, i;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 4, 0)
	if (!vif->ap_chandef.chan) {
		return -EIO;
	}
#endif

	/* Build the APM_START_REQ message */
	msg = asr_msg_zalloc(APM_START_REQ, TASK_APM, DRV_TASK_ID, sizeof(struct apm_start_req));
	if (!msg)
		return -ENOMEM;

	req = (struct apm_start_req *)msg->param;

	// Build the beacon
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 4, 0)
	bcn->dtim = (u8) settings->dtim_period;
	req->bcn_int = settings->beacon_interval;
	buf = asr_build_bcn(bcn, &settings->beacon);
#else
	bcn->dtim = (u8) info->dtim_period;
	req->bcn_int = info->interval;
	buf = asr_build_bcn(bcn, info);
#endif
	if (!buf) {
		kfree(msg);
		return -ENOMEM;
	}
	// Retrieve the basic rate set from the beacon buffer
	len = bcn->len - var_offset;
	var_pos = buf + var_offset;

	rate_ie = cfg80211_find_ie(WLAN_EID_SUPP_RATES, var_pos, len);
	if (rate_ie) {
		const u8 *rates = rate_ie + 2;
		for (i = 0; i < rate_ie[1]; i++) {
			if (rates[i] & 0x80)
				req->basic_rates.array[rate_len++] = rates[i];
		}
	}
	rate_ie = cfg80211_find_ie(WLAN_EID_EXT_SUPP_RATES, var_pos, len);
	if (rate_ie) {
		const u8 *rates = rate_ie + 2;
		for (i = 0; i < rate_ie[1]; i++) {
			if (rates[i] & 0x80)
				req->basic_rates.array[rate_len++] = rates[i];
		}
	}
	req->basic_rates.length = rate_len;

	// Fill in the DMA structure
	elem->buf = buf;
	elem->len = bcn->len;
	memcpy(req->bcn, buf, bcn->len);

	/* Set parameters for the APM_START_REQ message */
	req->vif_idx = vif->vif_index;
	req->bcn_len = bcn->len;
	req->tim_oft = bcn->head_len;
	req->tim_len = bcn->tim_len;
	req->chan.flags = 0;
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
#ifdef CONFIG_ASR595X
	req->chan.band = settings->chandef.chan->band;
	req->chan.prim20_freq = settings->chandef.chan->center_freq;
	//req->chan.flags = 0;
	req->chan.tx_power = settings->chandef.chan->max_power;
	req->chan.center1_freq = settings->chandef.center_freq1;
	req->chan.center2_freq = settings->chandef.center_freq2;
	//req->ch_width = bw2chnl[settings->chandef.width];
#else
	req->chan.band = settings->chandef.chan->band;
	req->chan.freq = settings->chandef.chan->center_freq;
	req->chan.tx_power = settings->chandef.chan->max_power;
	req->center_freq1 = settings->chandef.center_freq1;
	req->center_freq2 = settings->chandef.center_freq2;
	req->ch_width = bw2chnl[settings->chandef.width];
#endif

#ifdef CFG_AP_HIDDEN_SSID
	req->ssid.length = settings->ssid_len;
	memcpy(req->ssid.array,settings->ssid,settings->ssid_len);
#endif

	req->bcn_int = settings->beacon_interval;
	if (settings->crypto.control_port)
		flags |= CONTROL_PORT_HOST;

	if (settings->crypto.control_port_no_encrypt)
		flags |= CONTROL_PORT_NO_ENC;

	if (use_pairwise_key(&settings->crypto))
		flags |= WPA_WPA2_IN_USE;

	if (settings->crypto.control_port_ethertype)
		req->ctrl_port_ethertype = settings->crypto.control_port_ethertype;
	else
		req->ctrl_port_ethertype = ETH_P_PAE;
#else
	req->chan.band = vif->ap_chandef.chan->band;
	req->chan.freq = vif->ap_chandef.chan->center_freq;
	req->chan.tx_power = vif->ap_chandef.chan->max_power;
	req->center_freq1 = vif->ap_chandef.chan->center_freq;
	req->center_freq2 = 0;
	req->ch_width = bw2chnl[vif->ap_chandef.width];
	req->ctrl_port_ethertype = htons(ETH_P_PAE);
	flags |= WPA_WPA2_IN_USE | CONTROL_PORT_HOST | CONTROL_PORT_NO_ENC;
#endif
	req->flags = flags;

#ifdef CONFIG_ASR595X
	dev_info(asr_hw->dev, "[%s] (%d %d %d)%u\n", __func__, req->chan.band, req->chan.center1_freq,
		 req->chan.center2_freq, req->bcn_int);
#else
	dev_info(asr_hw->dev, "[%s] (%d %d %d %d)%u\n", __func__, req->chan.band, req->center_freq1, req->center_freq2,
		 req->ch_width, req->bcn_int);
#endif

	/* Send the APM_START_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, APM_START_CFM, cfm);

	kfree(msg);
	return ret;
}

int asr_send_apm_stop_req(struct asr_hw *asr_hw, struct asr_vif *vif)
{
	struct apm_stop_req *req = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the APM_STOP_REQ message */
	msg = asr_msg_zalloc(APM_STOP_REQ, TASK_APM, DRV_TASK_ID, sizeof(struct apm_stop_req));
	if (!msg)
		return -ENOMEM;

	req = (struct apm_stop_req *)msg->param;

	/* Set parameters for the APM_STOP_REQ message */
	req->vif_idx = vif->vif_index;

	/* Send the APM_STOP_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, APM_STOP_CFM, NULL);

	kfree(msg);
	return ret;
}

int asr_send_scanu_req(struct asr_hw *asr_hw, struct asr_vif *asr_vif, struct cfg80211_scan_request *param)
{
	struct scanu_start_req *req = NULL;
	int i,k;
	u8 chan_flags = 0;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	if (test_bit(ASR_DEV_CLOSEING, &asr_vif->dev_flags)
	    || !test_bit(ASR_DEV_STARTED, &asr_hw->phy_flags)
	    || test_bit(ASR_DEV_STA_DISCONNECTING, &asr_vif->dev_flags)
	    || test_bit(ASR_DEV_STA_CONNECTING, &asr_vif->dev_flags)
	    || test_bit(ASR_DEV_RESTARTING, &asr_hw->phy_flags)) {

		dev_err(asr_hw->dev, "ASR: %s,phy_flags==0X%lX , dev_flags=0X%lX.\n", __func__, asr_hw->phy_flags ,asr_vif->dev_flags);

		return -EBUSY;
	}

#ifdef ASR_STATS_RATES_TIMER
	if (test_bit(ASR_DEV_STA_CONNECTED, &asr_vif->dev_flags) &&
		(asr_hw->stats.txrx_rates[0].tx_rates >= ASR_STATS_RATES_SCAN_THRESHOLD
			|| asr_hw->stats.txrx_rates[0].rx_rates >= ASR_STATS_RATES_SCAN_THRESHOLD)) {

		//dev_err(asr_hw->dev, "ASR: %s, hight flow(%lu,%lu).\n", __func__,
		//	asr_hw->stats.txrx_rates[0].tx_rates, asr_hw->stats.txrx_rates[0].rx_rates);

		return -EBUSY;
	}
#endif

	set_bit(ASR_DEV_SCANING, &asr_vif->dev_flags);

#ifdef CONFIG_ASR_SDIO
        // after refine scan drop pkt issue, should not stop tx when scan.
	//if (asr_vif && test_bit(ASR_DEV_STA_CONNECTED, &asr_vif->dev_flags) && !netif_queue_stopped(asr_vif->ndev)) {
        //	netif_tx_stop_all_queues(asr_vif->ndev);
        //}
#endif

	/* Build the SCANU_START_REQ message */
	msg = asr_msg_zalloc(SCANU_START_REQ, TASK_SCANU, DRV_TASK_ID, sizeof(struct scanu_start_req));
	if (!msg) {
		clear_bit(ASR_DEV_SCANING, &asr_vif->dev_flags);
		return -ENOMEM;
	}

	req = (struct scanu_start_req *)msg->param;

	/* Set parameters */
	req->vif_idx = asr_vif->vif_index;
	req->chan_cnt = (u8) min_t(int, SCAN_CHANNEL_MAX, param->n_channels);
	req->ssid_cnt = (u8) min_t(int, SCAN_SSID_MAX, param->n_ssids);
	req->bssid = mac_addr_bcst;
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
	req->no_cck = param->no_cck;
#endif

	if (req->ssid_cnt == 0)
		chan_flags |= SCAN_PASSIVE_BIT;
	for (i = 0; i < req->ssid_cnt; i++) {
		int j;
		for (j = 0; j < param->ssids[i].ssid_len; j++)
			req->ssid[i].array[j] = param->ssids[i].ssid[j];
		req->ssid[i].length = param->ssids[i].ssid_len;
	}

	if (param->ie) {
		memcpy(req->add_ie, param->ie, param->ie_len);
		req->add_ie_len = param->ie_len;
	} else {
		req->add_ie_len = 0;
	}

	for (i = 0; i < req->chan_cnt; i++) {
		struct ieee80211_channel *chan=NULL;
		k = (asr_hw->scan_reverse)?(req->chan_cnt-1-i):i;
		chan = param->channels[k];

		req->chan[i].band = chan->band;
		req->chan[i].freq = chan->center_freq;
		req->chan[i].flags = chan_flags | passive_scan_flag(chan->flags);
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
		req->chan[i].tx_power = chan->max_reg_power;
#endif
#ifdef CONFIG_ASR595X
		req->chan[i].scan_additional_cnt = SCAN_ADDITIONAL_TIMES;
#endif
	}
	//dev_info(asr_hw->dev, "asr_send_scanu_req %d %d, %d\n",req->chan_cnt,req->ssid_cnt,req->add_ie_len);
	asr_hw->scan_reverse =!asr_hw->scan_reverse;
	/* Send the SCANU_START_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 0, 0, NULL);
	if (ret) {
		clear_bit(ASR_DEV_SCANING, &asr_vif->dev_flags);
	}
	dev_info(asr_hw->dev, "[%d,%s]%s:ret=%d,vif index=%d.\n", current->pid, current->comm, __func__, ret,asr_vif->vif_index);

	if (test_bit(ASR_DEV_INITED, &asr_hw->phy_flags)
	    && !test_bit(ASR_DEV_RESTARTING, &asr_hw->phy_flags)
	    && test_bit(ASR_DEV_SCANING, &asr_vif->dev_flags)) {

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		mod_timer(&asr_hw->scan_cmd_timer, jiffies + msecs_to_jiffies(ASR_CMD_CRASH_TIMER_OUT));
#else
		asr_hw->scan_cmd_timer.expires = jiffies + msecs_to_jiffies(ASR_CMD_CRASH_TIMER_OUT);
		add_timer(&asr_hw->scan_cmd_timer);
#endif

	}

	kfree(msg);
	return ret;
}

int asr_send_apm_start_cac_req(struct asr_hw *asr_hw, struct asr_vif *vif,
			       struct cfg80211_chan_def *chandef, struct apm_start_cac_cfm *cfm)
{
	struct apm_start_cac_req *req = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the APM_START_CAC_REQ message */
	msg = asr_msg_zalloc(APM_START_CAC_REQ, TASK_APM, DRV_TASK_ID, sizeof(struct apm_start_cac_req));
	if (!msg)
		return -ENOMEM;

	req = (struct apm_start_cac_req *)msg->param;

	/* Set parameters for the APM_START_CAC_REQ message */
	req->vif_idx = vif->vif_index;
	req->chan.band = chandef->chan->band;
#ifdef CONFIG_ASR595X
	req->chan.prim20_freq = chandef->chan->center_freq;
	req->chan.flags = 0;
	req->chan.center1_freq = chandef->center_freq1;
	req->chan.center2_freq = chandef->center_freq2;
	//req->ch_width = bw2chnl[chandef->width];
#else
	req->chan.freq = chandef->chan->center_freq;
	req->chan.flags = 0;
	req->center_freq1 = chandef->center_freq1;
	req->center_freq2 = chandef->center_freq2;
	req->ch_width = bw2chnl[chandef->width];
#endif

	/* Send the APM_START_CAC_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, APM_START_CAC_CFM, cfm);

	kfree(msg);
	return ret;
}

int asr_send_apm_stop_cac_req(struct asr_hw *asr_hw, struct asr_vif *vif)
{
	struct apm_stop_cac_req *req = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the APM_STOP_CAC_REQ message */
	msg = asr_msg_zalloc(APM_STOP_CAC_REQ, TASK_APM, DRV_TASK_ID, sizeof(struct apm_stop_cac_req));
	if (!msg)
		return -ENOMEM;

	req = (struct apm_stop_cac_req *)msg->param;

	/* Set parameters for the APM_STOP_CAC_REQ message */
	req->vif_idx = vif->vif_index;

	/* Send the APM_STOP_CAC_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, APM_STOP_CAC_CFM, NULL);

	kfree(msg);
	return ret;
}

/**********************************************************************
 *    Debug Messages
 *********************************************************************/
int asr_send_dbg_trigger_req(struct asr_hw *asr_hw, char *dbg_msg, int len)
{
	struct mm_dbg_trigger_req *req = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the MM_DBG_TRIGGER_REQ message */
	msg = asr_msg_zalloc(MM_DBG_TRIGGER_REQ, TASK_MM, DRV_TASK_ID, sizeof(struct mm_dbg_trigger_req));
	if (!msg)
		return -ENOMEM;

	req = (struct mm_dbg_trigger_req *)msg->param;

	if (len >= sizeof(req->error)) {
		len = sizeof(req->error) - 1;
	}
	/* Set parameters for the MM_DBG_TRIGGER_REQ message */
	strncpy(req->error, dbg_msg, len);

	/* Send the MM_DBG_TRIGGER_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 0, -1, NULL);

	kfree(msg);
	return ret;
}

int asr_send_dbg_mem_read_req(struct asr_hw *asr_hw, u32 mem_addr, struct dbg_mem_read_cfm *cfm)
{
	struct dbg_mem_read_req *mem_read_req = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the DBG_MEM_READ_REQ message */
	msg = asr_msg_zalloc(DBG_MEM_READ_REQ, TASK_DBG, DRV_TASK_ID, sizeof(struct dbg_mem_read_req));
	if (!msg)
		return -ENOMEM;

	mem_read_req = (struct dbg_mem_read_req *)msg->param;

	/* Set parameters for the DBG_MEM_READ_REQ message */
	mem_read_req->memaddr = mem_addr;

	/* Send the DBG_MEM_READ_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, DBG_MEM_READ_CFM, cfm);

	kfree(msg);
	return ret;
}

int asr_send_dbg_mem_write_req(struct asr_hw *asr_hw, u32 mem_addr, u32 mem_data)
{
	struct dbg_mem_write_req *mem_write_req = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the DBG_MEM_WRITE_REQ message */
	msg = asr_msg_zalloc(DBG_MEM_WRITE_REQ, TASK_DBG, DRV_TASK_ID, sizeof(struct dbg_mem_write_req));
	if (!msg)
		return -ENOMEM;

	mem_write_req = (struct dbg_mem_write_req *)msg->param;

	/* Set parameters for the DBG_MEM_WRITE_REQ message */
	mem_write_req->memaddr = mem_addr;
	mem_write_req->memdata = mem_data;

	/* Send the DBG_MEM_WRITE_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, DBG_MEM_WRITE_CFM, NULL);

	kfree(msg);
	return ret;
}

int asr_send_dbg_set_mod_filter_req(struct asr_hw *asr_hw, u32 filter)
{
	struct dbg_set_mod_filter_req *set_mod_filter_req = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the DBG_SET_MOD_FILTER_REQ message */
	msg = asr_msg_zalloc(DBG_SET_MOD_FILTER_REQ, TASK_DBG, DRV_TASK_ID, sizeof(struct dbg_set_mod_filter_req));
	if (!msg)
		return -ENOMEM;

	set_mod_filter_req = (struct dbg_set_mod_filter_req *)msg->param;

	/* Set parameters for the DBG_SET_MOD_FILTER_REQ message */
	set_mod_filter_req->mod_filter = filter;

	/* Send the DBG_SET_MOD_FILTER_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, DBG_SET_MOD_FILTER_CFM, NULL);

	kfree(msg);
	return ret;
}

int asr_send_dbg_set_sev_filter_req(struct asr_hw *asr_hw, u32 filter)
{
	struct dbg_set_sev_filter_req *set_sev_filter_req = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the DBG_SET_SEV_FILTER_REQ message */
	msg = asr_msg_zalloc(DBG_SET_SEV_FILTER_REQ, TASK_DBG, DRV_TASK_ID, sizeof(struct dbg_set_sev_filter_req));
	if (!msg)
		return -ENOMEM;

	set_sev_filter_req = (struct dbg_set_sev_filter_req *)msg->param;

	/* Set parameters for the DBG_SET_SEV_FILTER_REQ message */
	set_sev_filter_req->sev_filter = filter;

	/* Send the DBG_SET_SEV_FILTER_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, DBG_SET_SEV_FILTER_CFM, NULL);

	kfree(msg);
	return ret;
}

int asr_send_dbg_get_sys_stat_req(struct asr_hw *asr_hw, struct dbg_get_sys_stat_cfm *cfm)
{
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Allocate the message */
	msg = asr_msg_zalloc(DBG_GET_SYS_STAT_REQ, TASK_DBG, DRV_TASK_ID, 0);
	if (!msg)
		return -ENOMEM;

	/* Send the DBG_MEM_READ_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, DBG_GET_SYS_STAT_CFM, cfm);

	kfree(msg);
	return ret;
}

int asr_send_cfg_rssi_req(struct asr_hw *asr_hw, u8 vif_index, int rssi_thold, u32 rssi_hyst)
{
	struct mm_cfg_rssi_req *req = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the MM_CFG_RSSI_REQ message */
	msg = asr_msg_zalloc(MM_CFG_RSSI_REQ, TASK_MM, DRV_TASK_ID, sizeof(struct mm_cfg_rssi_req));
	if (!msg)
		return -ENOMEM;

	req = (struct mm_cfg_rssi_req *)msg->param;

	/* Set parameters for the MM_CFG_RSSI_REQ message */
	req->vif_index = vif_index;
	req->rssi_thold = (s8) rssi_thold;
	req->rssi_hyst = (u8) rssi_hyst;

	/* Send the MM_CFG_RSSI_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 0, 0, NULL);

	kfree(msg);
	return ret;
}

int asr_send_fw_softversion_req(struct asr_hw *asr_hw, struct mm_fw_softversion_cfm *cfm)
{
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the MM_FW_SOFTVERSION_REQ message */
	msg = asr_msg_zalloc(MM_FW_SOFTVERSION_REQ, TASK_MM, DRV_TASK_ID, sizeof(struct mm_fw_softversion_cfm));
	if (!msg)
		return -ENOMEM;

	ret = asr_send_msg(asr_hw, msg, 1, MM_FW_SOFTVERSION_CFM, cfm);

	kfree(msg);
	return ret;
}

int asr_send_fw_macaddr_req(struct asr_hw *asr_hw, struct mm_fw_macaddr_cfm *cfm)
{
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* VERSION REQ has no parameter */
	msg = asr_msg_zalloc(MM_FW_MAC_ADDR_REQ, TASK_MM, DRV_TASK_ID, 0);
	if (!msg)
		return -ENOMEM;

	ret = asr_send_msg(asr_hw, msg, 1, MM_FW_MAC_ADDR_CFM, cfm);

	kfree(msg);
	return ret;
}

int asr_send_set_fw_macaddr_req(struct asr_hw *asr_hw, const uint8_t * macaddr)
{
	struct mm_set_fw_macaddr_req *req = NULL;
	struct mm_fw_macaddr_cfm cfm;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the MM_CFG_RSSI_REQ message */
	msg = asr_msg_zalloc(MM_SET_FW_MAC_ADDR_REQ, TASK_MM, DRV_TASK_ID, sizeof(struct mm_set_fw_macaddr_req));
	if (!msg)
		return -ENOMEM;

	req = (struct mm_set_fw_macaddr_req *)msg->param;

	memcpy(req->mac, macaddr, MAC_ADDR_LEN);
	memset(&cfm, 0, sizeof(struct mm_fw_macaddr_cfm));

	/* Send the MM_CFG_RSSI_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, MM_SET_FW_MAC_ADDR_CFM, &cfm);

	dev_info(asr_hw->dev, "%s: %d,%d\n", __func__, ret, cfm.status);

	kfree(msg);
	return ret;
}

#ifdef CONFIG_ASR_SDIO
int asr_send_hif_sdio_info_req(struct asr_hw *asr_hw)
{
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* VERSION REQ has no parameter */
	msg = asr_msg_zalloc(MM_HIF_SDIO_INFO_REQ, TASK_MM, DRV_TASK_ID, 0);
	if (!msg)
		return -ENOMEM;

	ret = asr_send_msg(asr_hw, msg, 1, MM_HIF_SDIO_INFO_IND, NULL);

	kfree(msg);
	return ret;
}
#endif
int asr_send_set_tx_pwr_rate(struct asr_hw *asr_hw, struct mm_set_tx_pwr_rate_cfm *cfm,
			     struct mm_set_tx_pwr_rate_req *tx_pwr)
{
	struct mm_set_tx_pwr_rate_req *req = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	if (!asr_hw || !cfm || !tx_pwr) {
		return -1;
	}

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the MM_SET_POWER_REQ message */
	msg = asr_msg_zalloc(MM_SET_TX_PWR_RATE_REQ, TASK_MM, DRV_TASK_ID, sizeof(struct mm_set_tx_pwr_rate_req));
	if (!msg)
		return -ENOMEM;

	req = (struct mm_set_tx_pwr_rate_req *)msg->param;

	cfm->status = CO_OK;

	memcpy(req, tx_pwr, sizeof(struct mm_set_tx_pwr_rate_req));

	/* Send the MM_SET_TX_PWR_RATE_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, MM_SET_TX_PWR_RATE_CFM, cfm);

	dev_info(asr_hw->dev, "%s: %d,%d\n", __func__, ret, cfm->status);

	kfree(msg);
	return ret;
}
#ifdef CONFIG_HW_MIB_TABLE
int asr_send_reset_hw_mib_table_req(struct asr_hw *asr_hw)
{
	struct mm_reset_hw_mib_table_req *reset_hw_mib_table_req = NULL;
	struct mm_reset_hw_mib_table_cfm cfm;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the MM_RESET_HW_MIB_TABLE_REQ message */
	msg = asr_msg_zalloc(MM_RESET_HW_MIB_TABLE_REQ, TASK_DBG, DRV_TASK_ID, sizeof(struct mm_reset_hw_mib_table_req));
	if (!msg)
		return -ENOMEM;

	reset_hw_mib_table_req = (struct mm_reset_hw_mib_table_req *)msg->param;

	/* There is no Parameters for the MM_RESET_HW_MIB_TABLE_REQ message */

	/* Send the MM_RESET_HW_MIB_TABLE_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, MM_RESET_HW_MIB_TABLE_CFM, &cfm);

	kfree(msg);
	return ret;
}

int asr_send_get_hw_mib_table_req(struct asr_hw *asr_hw, struct machw_mib_tag *hw_mib_table)
{
	struct mm_get_hw_mib_table_req *get_hw_mib_table_req = NULL;
	struct mm_get_hw_mib_table_cfm cfm;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the MM_GET_HW_MIB_TABLE_REQ message */
	msg = asr_msg_zalloc(MM_GET_HW_MIB_TABLE_REQ, TASK_DBG, DRV_TASK_ID, sizeof(struct mm_get_hw_mib_table_req));
	if (!msg)
		return -ENOMEM;

	get_hw_mib_table_req = (struct mm_get_hw_mib_table_req *)msg->param;

	/* There is no Parameters for the MM_GET_HW_MIB_TABLE_REQ message */

	/* Send the MM_GET_HW_MIB_TABLE_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, MM_GET_HW_MIB_TABLE_CFM, &cfm);
	memcpy((uint32_t *)hw_mib_table, (uint32_t *)(cfm.mib_info), 255);
	kfree(msg);
	return ret;
}

#endif

int asr_send_set_cca(struct asr_hw *asr_hw, struct mm_set_get_cca_cfm *cfm,
			     struct mm_set_get_cca_req *cca_config)
{
	struct mm_set_get_cca_req *req = NULL;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	if (!asr_hw || !cfm || !cca_config) {
		return -1;
	}

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the MM_SET_POWER_REQ message */
	msg = asr_msg_zalloc(MM_SET_GET_CCA_THR_REQ, TASK_MM, DRV_TASK_ID, sizeof(struct mm_set_get_cca_req));
	if (!msg)
		return -ENOMEM;

	req = (struct mm_set_get_cca_req *)msg->param;

	*req = *cca_config;

	/* Send the MM_SET_GET_CCA_THR_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, MM_SET_GET_CCA_THR_CFM, cfm);

	//dev_info(asr_hw->dev, "%s: %d,%d(%d,%d,%d,%d)\n", __func__, ret, cfm->status,
	//	cfm->p_cca_rise, cfm->p_cca_fall, cfm->s_cca_rise, cfm->s_cca_fall);

	kfree(msg);
	return ret;
}

int asr_send_get_rssi_req(struct asr_hw *asr_hw, u8 staid, s8 *rssi)
{
	struct mm_get_rssi_cfm *req = NULL;
	struct mm_get_rssi_cfm cfm;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	if (!asr_hw || !rssi) {
		return -1;
	}

	ASR_DBG(ASR_FN_ENTRY_STR);

	*rssi = 0;

	/* Build the MM_SET_POWER_REQ message */
	msg = asr_msg_zalloc(MM_GET_RSSI_REQ, TASK_MM, DRV_TASK_ID, sizeof(struct mm_get_rssi_cfm));
	if (!msg)
		return -ENOMEM;

	req = (struct mm_get_rssi_cfm *)msg->param;

	req->staid = staid;

	/* Send the MM_GET_RSSI_CFM message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, MM_GET_RSSI_CFM, &cfm);

	if (cfm.status == CO_OK) {
		*rssi = cfm.rssi;
	}

	dev_info(asr_hw->dev, "%s: %d,%d,%d=%d\n", __func__, ret, cfm.status,
		cfm.staid, cfm.rssi);

	kfree(msg);
	return ret;
}

int asr_send_upload_fram_req(struct asr_hw *asr_hw, u8 vif_idx, u16 fram_type, u8 enable)
{
	struct mm_set_upload_fram_cfm *req = NULL;
	struct mm_set_upload_fram_cfm cfm;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	if (!asr_hw) {
		return -1;
	}

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the MM_SET_POWER_REQ message */
	msg = asr_msg_zalloc(MM_SET_UPLOAD_FRAM_REQ, TASK_MM, DRV_TASK_ID, sizeof(struct mm_set_upload_fram_cfm));
	if (!msg)
		return -ENOMEM;

	req = (struct mm_set_upload_fram_cfm *)msg->param;

	req->vif_idx = vif_idx;
	req->fram_type = (u8)fram_type;
	req->enable = enable;

	/* Send the MM_GET_RSSI_CFM message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, MM_SET_UPLOAD_FRAM_CFM, &cfm);

	dev_info(asr_hw->dev, "%s: %d,%d\n", __func__, ret, cfm.status);

	if (cfm.status) {
		ret = -2;
	}

	kfree(msg);
	return ret;
}

int asr_send_fram_appie_req(struct asr_hw *asr_hw, u8 vif_idx, u16 fram_type, u8 *ie, u8 ie_len)
{
	struct mm_set_fram_appie_req *req = NULL;
	struct mm_set_fram_appie_req cfm;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	if (!asr_hw || ie_len > FRAM_CUSTOM_APPIE_LEN) {
		return -1;
	}

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Build the MM_SET_POWER_REQ message */
	msg = asr_msg_zalloc(MM_SET_FRAM_APPIE_REQ, TASK_MM, DRV_TASK_ID, sizeof(struct mm_set_fram_appie_req));
	if (!msg)
		return -ENOMEM;

	req = (struct mm_set_fram_appie_req *)msg->param;

	req->vif_idx = vif_idx;
	req->fram_type = (u8)fram_type;
	req->ie_len = ie_len;
	if (req->ie_len) {
		memcpy(req->appie, ie, ie_len);
	}

	/* Send the MM_GET_RSSI_CFM message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, MM_SET_FRAM_APPIE_CFM, &cfm);

	dev_info(asr_hw->dev, "%s: %d,%d\n", __func__, ret, cfm.status);

	if (cfm.status) {
		ret = -2;
	}

	kfree(msg);
	return ret;
}

int asr_send_efuse_txpwr_req(struct asr_hw *asr_hw, uint8_t * txpwr, uint8_t * txevm, uint8_t *freq_err, bool iswrite, uint8_t *index)
{
	struct mm_efuse_txpwr_info *req = NULL;
	struct mm_efuse_txpwr_info cfm;
	struct lmac_msg *msg = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	if (!asr_hw || !txpwr || !txevm) {
		return -EINVAL;
	}

	msg = asr_msg_zalloc(MM_SET_EFUSE_TXPWR_REQ, TASK_MM, DRV_TASK_ID, sizeof(struct mm_efuse_txpwr_info));
	if (!msg)
		return -ENOMEM;

	req = (struct mm_efuse_txpwr_info *)msg->param;

	memset(&cfm, 0, sizeof(struct mm_efuse_txpwr_info));

	if (iswrite) {
		req->iswrite = iswrite;
		memcpy(req->txpwr, txpwr, 6);
		memcpy(req->txevm, txevm, 6);
		req->freq_err = *freq_err;
	}

	/* Send the MM_CFG_RSSI_REQ message to LMAC FW */
	ret = asr_send_msg(asr_hw, msg, 1, MM_SET_EFUSE_TXPWR_CFM, &cfm);

	if (!iswrite) {
		memcpy(txpwr, cfm.txpwr, 6);
		memcpy(txevm, cfm.txevm, 6);
		*freq_err = cfm.freq_err;
	}

	*index = cfm.index;

	dev_info(asr_hw->dev, "%s: %d,%d,%d,%pM,%pM,%u,0x%X\n"
		, __func__, ret, req->iswrite, cfm.status, cfm.txpwr, cfm.txevm, cfm.index,cfm.freq_err);

	kfree(msg);
	return ret | cfm.status;
}
