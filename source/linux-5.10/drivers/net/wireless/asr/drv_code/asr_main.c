/**
 ******************************************************************************
 *
 * @file asr_main.c
 *
 * @brief Entry point of the ASR driver
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/inetdevice.h>
#include <net/cfg80211.h>
#include <linux/etherdevice.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include "asr_defs.h"
#include "asr_msg_tx.h"
#include "asr_msg_rx.h"
#include "asr_tx.h"
#include "asr_rx.h"
#include "hal_desc.h"
#include "asr_cfgfile.h"
#include "asr_irqs.h"
#include "asr_version.h"
#include "asr_events.h"
#ifdef CONFIG_ASR_SDIO
#include "asr_sdio.h"
#endif
#include "asr_platform.h"
#include <linux/wireless.h>
#include "asr_idle_mode.h"
#include <linux/hrtimer.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include "ieee802_mib.h"
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 1)
#include <uapi/linux/sched/types.h>
#endif
#ifdef CONFIG_ASR_USB
#include "asr_usb.h"
#endif
#include "asr_ate.h"
#ifdef CONFIG_NOT_USED_DTS
static struct completion device_release;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
#include <linux/proc_fs.h>
#endif
#ifdef ASR_REDUCE_TCP_ACK
#include <net/tcp.h>
#endif

#define RW_DRV_DESCRIPTION  "ASR 11bgn driver for Linux cfg80211"
#define RW_DRV_COPYRIGHT    "Copyright(c) ASR"
#define RW_DRV_AUTHOR       "ASR Microelectronics Co.,Ltd."

bool skip_txflow = false;
bool force_bw20 = 0;
bool mrole_enable = 0;
bool p2p_debug=0;
int tx_status_debug = 0;
int tx_debug = 2;
bool asr_xmit_opt = 0;
bool tx_wait_agger = 0;
bool txlogen = 0;
bool rxlogen = 0;
bool irq_double_edge_en = 0;
bool main_task_clr_irq_en = 0;
bool test_host_re_read = 0;

int lalalaen = 0;
int dbg_type = 0;
int tx_aggr = 8;		//8 will error
int rx_aggr = 1;
int rx_ooo_upload = 1;
int tx_aggr_xmit_thres = 0;
int rx_thread_timer = 1;
int rx_period_timer = 100;
int tx_hr_timer = 4000;

int flow_ctrl_high = 300;	//150;
int flow_ctrl_low = 80;
int tx_conserve = 0;		//if tx_conserve == 1, tx_aggr = 4 will be better

/* Error bits */
int asr_msg_level = 0;		//ASR_USB_VAL;ASR_SDIO_VAL;

#ifdef CFG_SNIFFER_SUPPORT
extern monitor_cb_t sniffer_rx_cb;
extern monitor_cb_t sniffer_rx_mgmt_cb;
#endif

int downloadfw = 1;
int downloadATE = 0;
int driver_mode = DRIVER_MODE_NORMAL;
bool sdio1thread = false;
//const uint8_t g_fw_macaddr_default[] = {0x22,0x33,0x99,0x88,0x77,0x55};

static char *mac_param = NULL;

struct asr_global_param g_asr_para;

#define ASR_PRINT_CFM_ERR(req) \
        dev_err(asr_hw->dev, "%s: Status Error(%d)\n", #req, (&req##_cfm)->status)

#define ASR_HT_CAPABILITIES                                    \
{                                                               \
    .ht_supported   = true,                                     \
    .cap            = 0,                                        \
    .ampdu_factor   = IEEE80211_HT_MAX_AMPDU_64K,               \
    .ampdu_density  = IEEE80211_HT_MPDU_DENSITY_16,             \
    .mcs        = {                                             \
        .rx_mask = { 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, },        \
        .rx_highest = cpu_to_le16(65),                          \
        .tx_params = IEEE80211_HT_MCS_TX_DEFINED,               \
    },                                                          \
}

#define RATE(_bitrate, _hw_rate, _flags) {      \
    .bitrate    = (_bitrate),                   \
    .flags      = (_flags),                     \
    .hw_value   = (_hw_rate),                   \
}

#define CHAN(_freq) {                           \
    .center_freq    = (_freq),                  \
    .max_power  = 30, /* FIXME */               \
}

static struct ieee80211_rate asr_ratetable[] = {
	RATE(10, 0x00, 0),
	RATE(20, 0x01, IEEE80211_RATE_SHORT_PREAMBLE),
	RATE(55, 0x02, IEEE80211_RATE_SHORT_PREAMBLE),
	RATE(110, 0x03, IEEE80211_RATE_SHORT_PREAMBLE),
	RATE(60, 0x04, 0),
	RATE(90, 0x05, 0),
	RATE(120, 0x06, 0),
	RATE(180, 0x07, 0),
	RATE(240, 0x08, 0),
	RATE(360, 0x09, 0),
	RATE(480, 0x0A, 0),
	RATE(540, 0x0B, 0),
};

/* The channels indexes here are not used anymore */
static struct ieee80211_channel asr_2ghz_channels[] = {
	CHAN(2412),
	CHAN(2417),
	CHAN(2422),
	CHAN(2427),
	CHAN(2432),
	CHAN(2437),
	CHAN(2442),
	CHAN(2447),
	CHAN(2452),
	CHAN(2457),
	CHAN(2462),
	CHAN(2467),
	CHAN(2472),
	CHAN(2484),
};


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0)
#define ASR_HE_CAPABILITIES                                    \
{                                                               \
    .has_he = false,                                            \
    .he_cap_elem = {                                            \
        .mac_cap_info[0] = 0,                                   \
        .mac_cap_info[1] = 0,                                   \
        .mac_cap_info[2] = 0,                                   \
        .mac_cap_info[3] = 0,                                   \
        .mac_cap_info[4] = 0,                                   \
        .mac_cap_info[5] = 0,                                   \
        .phy_cap_info[0] = 0,                                   \
        .phy_cap_info[1] = 0,                                   \
        .phy_cap_info[2] = 0,                                   \
        .phy_cap_info[3] = 0,                                   \
        .phy_cap_info[4] = 0,                                   \
        .phy_cap_info[5] = 0,                                   \
        .phy_cap_info[6] = 0,                                   \
        .phy_cap_info[7] = 0,                                   \
        .phy_cap_info[8] = 0,                                   \
        .phy_cap_info[9] = 0,                                   \
        .phy_cap_info[10] = 0,                                  \
    },                                                          \
    .he_mcs_nss_supp = {                                        \
        .rx_mcs_80 = cpu_to_le16(0xfffa),                       \
        .tx_mcs_80 = cpu_to_le16(0xfffa),                       \
        .rx_mcs_160 = cpu_to_le16(0xffff),                      \
        .tx_mcs_160 = cpu_to_le16(0xffff),                      \
        .rx_mcs_80p80 = cpu_to_le16(0xffff),                    \
        .tx_mcs_80p80 = cpu_to_le16(0xffff),                    \
    },                                                          \
    .ppe_thres = {0x08, 0x1c, 0x07},                            \
}
#else
#define ASR_HE_CAPABILITIES                                    \
{                                                               \
    .has_he = false,                                            \
    .he_cap_elem = {                                            \
        .mac_cap_info[0] = 0,                                   \
        .mac_cap_info[1] = 0,                                   \
        .mac_cap_info[2] = 0,                                   \
        .mac_cap_info[3] = 0,                                   \
        .mac_cap_info[4] = 0,                                   \
        .phy_cap_info[0] = 0,                                   \
        .phy_cap_info[1] = 0,                                   \
        .phy_cap_info[2] = 0,                                   \
        .phy_cap_info[3] = 0,                                   \
        .phy_cap_info[4] = 0,                                   \
        .phy_cap_info[5] = 0,                                   \
        .phy_cap_info[6] = 0,                                   \
        .phy_cap_info[7] = 0,                                   \
        .phy_cap_info[8] = 0,                                   \
    },                                                          \
    .he_mcs_nss_supp = {                                        \
        .rx_mcs_80 = cpu_to_le16(0xfffa),                       \
        .tx_mcs_80 = cpu_to_le16(0xfffa),                       \
        .rx_mcs_160 = cpu_to_le16(0xffff),                      \
        .tx_mcs_160 = cpu_to_le16(0xffff),                      \
        .rx_mcs_80p80 = cpu_to_le16(0xffff),                    \
        .tx_mcs_80p80 = cpu_to_le16(0xffff),                    \
    },                                                          \
    .ppe_thres = {0x08, 0x1c, 0x07},                            \
}
#endif


#ifdef CONFIG_ASR595X
static struct ieee80211_sband_iftype_data asr_he_capa = {
	.types_mask = (BIT(NL80211_IFTYPE_STATION) | BIT(NL80211_IFTYPE_AP)),
	.he_cap = ASR_HE_CAPABILITIES,
};
#endif

static struct ieee80211_supported_band asr_band_2GHz = {
	.channels = asr_2ghz_channels,
	.n_channels = ARRAY_SIZE(asr_2ghz_channels),
	.bitrates = asr_ratetable,
	.n_bitrates = ARRAY_SIZE(asr_ratetable),
	.ht_cap = ASR_HT_CAPABILITIES,
#ifdef CONFIG_ASR595X
	.iftype_data = &asr_he_capa,
	.n_iftype_data = 1,
#endif
};

static struct ieee80211_iface_limit asr_limits[] = {
	{.max = NX_VIRT_DEV_MAX,.types = BIT(NL80211_IFTYPE_AP) | BIT(NL80211_IFTYPE_STATION)
                                         #if 0
                                         | BIT(NL80211_IFTYPE_P2P_CLIENT) | BIT(NL80211_IFTYPE_P2P_GO)
                                         #endif
                                         }
};

static const struct ieee80211_iface_combination asr_combinations[] = {
	{
	 .limits = asr_limits,
	 .n_limits = ARRAY_SIZE(asr_limits),
	 .num_different_channels = NX_CHAN_CTXT_CNT,
	 .max_interfaces = NX_VIRT_DEV_MAX,
	 },
};

/* There isn't a lot of sense in it, but you can transmit anything you like */
static struct ieee80211_txrx_stypes
 asr_default_mgmt_stypes[NUM_NL80211_IFTYPES] = {
	[NL80211_IFTYPE_STATION] = {
				    .tx = 0xffff,
				    .rx = BIT(IEEE80211_STYPE_ACTION >> 4) | BIT(IEEE80211_STYPE_PROBE_REQ >> 4),
				    },
	[NL80211_IFTYPE_AP] = {
			       .tx = 0xffff,
			       .rx = BIT(IEEE80211_STYPE_ASSOC_REQ >> 4) |
			       BIT(IEEE80211_STYPE_REASSOC_REQ >> 4) |
			       BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
			       BIT(IEEE80211_STYPE_DISASSOC >> 4) |
			       BIT(IEEE80211_STYPE_AUTH >> 4) |
			       BIT(IEEE80211_STYPE_DEAUTH >> 4) | BIT(IEEE80211_STYPE_ACTION >> 4),
			       },
	#ifdef P2P_SUPPORT
	[NL80211_IFTYPE_P2P_GO] = {
			       .tx = 0xffff,
			       .rx = BIT(IEEE80211_STYPE_ASSOC_REQ >> 4) |
			       BIT(IEEE80211_STYPE_REASSOC_REQ >> 4) |
			       BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
			       BIT(IEEE80211_STYPE_DISASSOC >> 4) |
			       BIT(IEEE80211_STYPE_AUTH >> 4) |
			       BIT(IEEE80211_STYPE_DEAUTH >> 4) | BIT(IEEE80211_STYPE_ACTION >> 4),
			       },
    [NL80211_IFTYPE_P2P_CLIENT] = {
			       .tx = 0xffff,
			       .rx = BIT(IEEE80211_STYPE_ACTION >> 4) | BIT(IEEE80211_STYPE_PROBE_REQ >> 4),
                   },
    [NL80211_IFTYPE_P2P_DEVICE] = {
        .tx = 0xffff,
        .rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
            BIT(IEEE80211_STYPE_PROBE_REQ >> 4),
    },
	#endif
};

static u32 cipher_suites[] = {
	WLAN_CIPHER_SUITE_WEP40,
	WLAN_CIPHER_SUITE_WEP104,
	WLAN_CIPHER_SUITE_TKIP,
	WLAN_CIPHER_SUITE_CCMP,
#ifdef CONFIG_ASR595X
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 33)
	WLAN_CIPHER_SUITE_CCMP_256,
	WLAN_CIPHER_SUITE_GCMP_256,
	WLAN_CIPHER_SUITE_BIP_GMAC_256,
#endif
#endif
	0,			// reserved entries to enable AES-CMAC and/or SMS4
	0,
};

#define NB_RESERVED_CIPHER 2;

static const int asr_ac2hwq[1][NL80211_NUM_ACS] = {
	{
	 [NL80211_TXQ_Q_VO] = ASR_HWQ_VO,
	 [NL80211_TXQ_Q_VI] = ASR_HWQ_VI,
	 [NL80211_TXQ_Q_BE] = ASR_HWQ_BE,
	 [NL80211_TXQ_Q_BK] = ASR_HWQ_BK}
};

const int asr_tid2hwq[IEEE80211_NUM_TIDS] = {
	ASR_HWQ_BE,
	ASR_HWQ_BK,
	ASR_HWQ_BK,
	ASR_HWQ_BE,
	ASR_HWQ_VI,
	ASR_HWQ_VI,
	ASR_HWQ_VO,
	ASR_HWQ_VO,
	/* TID_8 is used for management frames */
	ASR_HWQ_VO,
	/* At the moment, all others TID are mapped to BE */
	ASR_HWQ_BE,
	ASR_HWQ_BE,
	ASR_HWQ_BE,
	ASR_HWQ_BE,
	ASR_HWQ_BE,
	ASR_HWQ_BE,
	ASR_HWQ_BE,
};

static const int asr_hwq2uapsd[NL80211_NUM_ACS] = {
	[ASR_HWQ_VO] = IEEE80211_WMM_IE_STA_QOSINFO_AC_VO,
	[ASR_HWQ_VI] = IEEE80211_WMM_IE_STA_QOSINFO_AC_VI,
	[ASR_HWQ_BE] = IEEE80211_WMM_IE_STA_QOSINFO_AC_BE,
	[ASR_HWQ_BK] = IEEE80211_WMM_IE_STA_QOSINFO_AC_BK,
};

extern int asr_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd);
extern void set_mib_default(struct asr_hw *asr_hw);
extern void rx_deauth_work_func(struct work_struct *work);

/*********************************************************************
 * helper
 *********************************************************************/
struct asr_sta *asr_get_sta(struct asr_hw *asr_hw, const u8 * mac_addr)
{
	int i;

	for (i = 0; i < asr_hw->sta_max_num; i++) {
		struct asr_sta *sta = &asr_hw->sta_table[i];
		if (sta->valid && (memcmp(mac_addr, sta->mac_addr, 6) == 0))
			return sta;
	}

	return NULL;
}

void asr_enable_wapi(struct asr_hw *asr_hw)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
	cipher_suites[asr_hw->wiphy->n_cipher_suites] = WLAN_CIPHER_SUITE_SMS4;
	asr_hw->wiphy->n_cipher_suites++;
	asr_hw->wiphy->flags |= WIPHY_FLAG_CONTROL_PORT_PROTOCOL;
#endif
}

void asr_enable_mfp(struct asr_hw *asr_hw)
{
	cipher_suites[asr_hw->wiphy->n_cipher_suites] = WLAN_CIPHER_SUITE_AES_CMAC;
	asr_hw->wiphy->n_cipher_suites++;
}

u8 *asr_build_bcn(struct asr_bcn *bcn
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 4, 0)
		  , struct cfg80211_beacon_data *new
#else
		  , struct beacon_parameters *new
#endif
    )
{
	u8 *buf, *pos;

	if (new->head) {
		u8 *head = kmalloc(new->head_len, GFP_KERNEL);

		if (!head)
			return NULL;

		if (bcn->head)
			kfree(bcn->head);

		bcn->head = head;
		bcn->head_len = new->head_len;
		memcpy(bcn->head, new->head, new->head_len);
	}
	if (new->tail) {
		u8 *tail = kmalloc(new->tail_len, GFP_KERNEL);

		if (!tail)
			return NULL;

		if (bcn->tail)
			kfree(bcn->tail);

		bcn->tail = tail;
		bcn->tail_len = new->tail_len;
		memcpy(bcn->tail, new->tail, new->tail_len);
	}

	if (!bcn->head)
		return NULL;

	bcn->tim_len = 6;
	bcn->len = bcn->head_len + bcn->tail_len + bcn->ies_len + bcn->tim_len;

	buf = kmalloc(bcn->len, GFP_KERNEL);
	if (!buf)
		return NULL;

	// Build the beacon buffer
	pos = buf;
	memcpy(pos, bcn->head, bcn->head_len);
	pos += bcn->head_len;
	*pos++ = WLAN_EID_TIM;
	*pos++ = 4;
	*pos++ = 0;
	*pos++ = bcn->dtim;
	*pos++ = 0;
	*pos++ = 0;
	if (bcn->tail) {
		memcpy(pos, bcn->tail, bcn->tail_len);
		pos += bcn->tail_len;
	}
	if (bcn->ies) {
		memcpy(pos, bcn->ies, bcn->ies_len);
	}

	return buf;
}

static void asr_del_bcn(struct asr_bcn *bcn)
{
	if (bcn->head) {
		kfree(bcn->head);
		bcn->head = NULL;
	}
	bcn->head_len = 0;

	if (bcn->tail) {
		kfree(bcn->tail);
		bcn->tail = NULL;
	}
	bcn->tail_len = 0;

	if (bcn->ies) {
		kfree(bcn->ies);
		bcn->ies = NULL;
	}
	bcn->ies_len = 0;
	bcn->tim_len = 0;
	bcn->dtim = 0;
	bcn->len = 0;
}

/**
 * Link channel ctxt to a vif and thus increments count for this context.
 */
void asr_chanctx_link(struct asr_vif *vif, u8 ch_idx, struct cfg80211_chan_def *chandef)
{
	struct asr_chanctx *ctxt;

	if (ch_idx >= NX_CHAN_CTXT_CNT) {
		WARN(1, "Invalid channel ctxt id %d", ch_idx);
		return;
	}

	vif->ch_index = ch_idx;
	ctxt = &vif->asr_hw->chanctx_table[ch_idx];
	ctxt->count++;

	// For now chandef is NULL for STATION interface
	if (chandef) {
		if (!ctxt->chan_def.chan)
			ctxt->chan_def = *chandef;
		else {
			// TODO. check that chandef is the same as the one already
			// set for this ctxt
		}
	}
}

/**
 * Unlink channel ctxt from a vif and thus decrements count for this context
 */
void asr_chanctx_unlink(struct asr_vif *vif)
{
	struct asr_chanctx *ctxt;

	if (vif->ch_index == ASR_CH_NOT_SET)
		return;

	ctxt = &vif->asr_hw->chanctx_table[vif->ch_index];

	if (ctxt->count == 0) {
		WARN(1, "Chan ctxt ref count is already 0");
	} else {
		ctxt->count--;
	}

	if (ctxt->count == 0) {
		if (vif->ch_index == vif->asr_hw->cur_chanctx) {

		}
		/* set chan to null, so that if this ctxt is relinked to a vif that
		   don't have channel information, don't use wrong information */
		ctxt->chan_def.chan = NULL;
	}
	vif->ch_index = ASR_CH_NOT_SET;
}

int asr_chanctx_valid(struct asr_hw *asr_hw, u8 ch_idx)
{
	if (ch_idx >= NX_CHAN_CTXT_CNT || asr_hw->chanctx_table[ch_idx].chan_def.chan == NULL) {
		return 0;
	}

	return 1;
}

static void asr_del_csa(struct asr_vif *vif)
{
	//struct asr_hw *asr_hw = vif->asr_hw;
	struct asr_csa *csa = vif->ap.csa;

	if (!csa)
		return;

	if (csa->dma.buf) {
		kfree(csa->dma.buf);
	}
	asr_del_bcn(&csa->bcn);
	kfree(csa);
	vif->ap.csa = NULL;
}

/*
* common function used by usb and sdio
*/
static int asr_read_mm_info(struct asr_hw *asr_hw)
{
	int ret = -1;
	struct mm_get_info_cfm mm_info_cfm;

	if (asr_hw->driver_mode == DRIVER_MODE_ATE) {
		asr_hw->vif_max_num = 1;
		asr_hw->sta_max_num = 1;

		return 0;
	}

	memset(&mm_info_cfm, 0, sizeof(struct mm_get_info_cfm));
#ifdef CONFIG_FW_HAVE_NOT_MM_INFO_MSG
	// fw version not the latest
	ret = 0;
	asr_hw->vif_max_num = NX_VIRT_DEV_MAX;
	asr_hw->sta_max_num = NX_REMOTE_STA_MAX;
#else
	if ((ret = asr_send_mm_get_info(asr_hw, &mm_info_cfm))) {
		return ret;
	}
	if (!mm_info_cfm.status) {

		if (mm_info_cfm.vif_num > NX_VIRT_DEV_MAX || mm_info_cfm.sta_num > NX_REMOTE_STA_MAX) {
			dev_err(asr_hw->dev, "%s:ERROR,too large param, vif_num=%d,sta_num=%d\n",
				__func__, mm_info_cfm.vif_num, mm_info_cfm.sta_num);
			return -1;
		}

		ret = 0;

		dev_info(asr_hw->dev, "%s:vif_num=%d,sta_num=%d\n", __func__, mm_info_cfm.vif_num, mm_info_cfm.sta_num);
		asr_hw->vif_max_num = mm_info_cfm.vif_num;
		asr_hw->sta_max_num = mm_info_cfm.sta_num;
	} else {
		ret = -1;
	}
#endif

	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 0)
static void asr_csa_finish(struct work_struct *ws)
{
	struct asr_csa *csa = container_of(ws, struct asr_csa, work);
	struct asr_vif *vif = csa->vif;
	struct asr_hw *asr_hw = vif->asr_hw;
	int error = csa->status;

	if (!error)
		error =
		    asr_send_bcn_change(asr_hw, vif->vif_index, csa->dma.buf,
					csa->bcn.len, csa->bcn.head_len, csa->bcn.tim_len, NULL);

	if (error)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
		cfg80211_stop_iface(asr_hw->wiphy, &vif->wdev, GFP_KERNEL);
#else
	{
		dev_err(asr_hw->dev, "asr_csa_finish error\r\n");
	}
#endif
	else {
		mutex_lock(&vif->wdev.mtx);
		__acquire(&vif->wdev.mtx);
		spin_lock_bh(&asr_hw->cb_lock);
		asr_chanctx_unlink(vif);
		asr_chanctx_link(vif, csa->ch_idx, &csa->chandef);
		if (asr_hw->cur_chanctx == csa->ch_idx) {
			asr_txq_vif_start(vif, ASR_TXQ_STOP_CHAN, asr_hw);
		} else
			asr_txq_vif_stop(vif, ASR_TXQ_STOP_CHAN, asr_hw);
		spin_unlock_bh(&asr_hw->cb_lock);
		#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
		cfg80211_ch_switch_notify(vif->ndev, &csa->chandef, 0);
		#else
		cfg80211_ch_switch_notify(vif->ndev, &csa->chandef);
		#endif
		mutex_unlock(&vif->wdev.mtx);
		__release(&vif->wdev.mtx);

	}
	asr_del_csa(vif);
}
#endif

#ifdef CFG_SNIFFER_SUPPORT
extern int asr_sniffer_handle_cb_register(monitor_cb_t fn);

void lega_test_monitor_rx_cb(u8 * data, int len)
{
	/* defined for debug */
	if (*data == 0x40)
		printk("monitor_rx_cb:[0x%x 0x%x], data_len:%d \n", *data, *(data + 1), len);
}
#endif
/*********************************************************************
 * netdev callbacks
 ********************************************************************/
/**
 * int (*ndo_open)(struct net_device *dev);
 *     This function is called when network device transistions to the up
 *     state.
 *
 * - Start FW if this is the first interface opened
 * - Add interface at fw level
 */
static int asr_open(struct net_device *dev)
{
	struct asr_vif *asr_vif = netdev_priv(dev);
	struct asr_hw *asr_hw = asr_vif->asr_hw;
	struct mm_add_if_cfm add_if_cfm;
	int error = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	netdev_info(dev, "OPEN,type=%d",ASR_VIF_TYPE(asr_vif));

#ifdef CFG_SNIFFER_SUPPORT
	if (ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_MONITOR) {
		asr_sniffer_handle_cb_register(lega_test_monitor_rx_cb);
		asr_hw->ipc_env->cb.recv_data_ind = asr_rxdataind_sniffer;
	}
#endif

	// Check if it is the first opened VIF
	if (asr_hw->vif_started == 0) {

		// Start the FW
		if ((error = asr_send_start(asr_hw)))
			return error;

		/* Device is now started */
		set_bit(ASR_DEV_STARTED, &asr_hw->phy_flags);
	}

	/* Forward the information to the LMAC,
	 *     p2p value not used in FMAC configuration, iftype is sufficient */
	if ((error = asr_send_add_if(asr_hw, dev->dev_addr, ASR_VIF_TYPE(asr_vif), false, &add_if_cfm)))
		return error;

	if (add_if_cfm.status != 0) {
		ASR_PRINT_CFM_ERR(add_if);
		return -EIO;
	}

	/* Save the index retrieved from LMAC */
	spin_lock_bh(&asr_hw->cb_lock);
	asr_vif->vif_index = add_if_cfm.inst_nbr;
	asr_vif->up = true;
	asr_hw->vif_started++;
	asr_hw->vif_table[add_if_cfm.inst_nbr] = asr_vif;

    // two vif exist. one is normal interface, one is cfg interface(asrcfg).
	dev_info(asr_hw->dev, "%s dev name:%s , vif_index=%d, mac=%pM\n", __func__, asr_vif->wdev.netdev->name,asr_vif->vif_index, dev->dev_addr);

    if (memcmp(asr_vif->wdev.netdev->name,"asrcfgwlan",10) == 0)
        asr_hw->ext_vif_index = add_if_cfm.inst_nbr;
	else
	    asr_hw->vif_index = add_if_cfm.inst_nbr;

	memset(asr_vif->bssid, 0xff, ETH_ALEN);
	spin_unlock_bh(&asr_hw->cb_lock);

	netif_carrier_off(dev);

#ifdef CONFIG_ASR_SDIO
	if (asr_xmit_opt) {

	} else {

	    spin_lock_bh(&asr_hw->tx_agg_env_lock);
	    if (mrole_enable == false) {
	        asr_tx_agg_buf_reset(asr_hw);
	    }
		spin_unlock_bh(&asr_hw->tx_agg_env_lock);
	}
#endif

#ifdef CFG_SNIFFER_SUPPORT
	if (ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_MONITOR) {
		asr_txq_vif_init_idle_mode(asr_hw, 0);
		asr_hw->monitor_vif_idx = asr_vif->vif_index;
	}
#endif

	return error;
}

/**
 * int (*ndo_stop)(struct net_device *dev);
 *     This function is called when network device transistions to the down
 *     state.
 *
 * - Remove interface at fw level
 * - Reset FW if this is the last interface opened
 */
static int asr_close(struct net_device *dev)
{
	struct asr_vif *asr_vif = netdev_priv(dev);
	struct asr_hw *asr_hw = asr_vif->asr_hw;
	struct sk_buff *skb;
	int i = 0, timeout = 500;
#ifdef CONFIG_ASR_SDIO
	struct asr_vif *asr_vif_tmp = NULL;
#endif

	ASR_DBG(ASR_FN_ENTRY_STR);

	netdev_info(dev, "CLOSE");
#ifdef CONFIG_ASR_USB
	dev_info(asr_hw->dev, "%s flag:%d\n", __func__, asr_hw->usb_remove_flag);
#endif

	set_bit(ASR_DEV_PRECLOSEING, &asr_vif->dev_flags);
#ifdef CONFIG_ASR_USB
	if (!asr_hw->usb_remove_flag)
#endif
		asr_send_sm_disconnect_req(asr_hw, asr_vif, 1);

	set_bit(ASR_DEV_CLOSEING, &asr_vif->dev_flags);
	clear_bit(ASR_DEV_PRECLOSEING, &asr_vif->dev_flags);

#ifdef CONFIG_ASR_USB
	if (!asr_hw->usb_remove_flag) {
#endif

		while (test_bit(ASR_DEV_SCANING, &asr_vif->dev_flags) && timeout--) {
			msleep(10);
		}
#ifdef CONFIG_ASR_USB
	}
#endif
	clear_bit(ASR_DEV_SCANING, &asr_vif->dev_flags);
	clear_bit(ASR_DEV_STA_CONNECTING, &asr_vif->dev_flags);
	clear_bit(ASR_DEV_STA_DISCONNECTING, &asr_vif->dev_flags);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 10, 0)
	/* Abort scan request on the vif */
	if (asr_hw->scan_request && asr_hw->scan_request->dev == asr_vif->wdev.netdev) {
#else
	if (asr_hw->scan_request && asr_hw->scan_request->wdev == &asr_vif->wdev) {
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 1)
		struct cfg80211_scan_info info = {
			.aborted = true,
		};
		cfg80211_scan_done(asr_hw->scan_request, &info);
#else
		cfg80211_scan_done(asr_hw->scan_request, true);
#endif
		asr_hw->scan_request = NULL;
	    asr_hw->scan_vif_index = 0xFF;
	}
#ifdef CFG_SNIFFER_SUPPORT
	if (ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_MONITOR) {
		asr_sniffer_handle_cb_register(NULL);
		asr_vif->sniffer.rx_filter = 0;
		asr_vif->sniffer.chan_num = 0;
		asr_hw->monitor_vif_idx = 0xff;
		asr_txq_vif_deinit_idle_mode(asr_hw, NULL);
		asr_send_set_idle(asr_hw, 1);
	}
#endif
	//close start
#ifdef CONFIG_ASR_USB
	if (!asr_hw->usb_remove_flag)
#endif

	asr_send_remove_if(asr_hw, asr_vif->vif_index);

	if (asr_hw->roc_elem && (asr_hw->roc_elem->wdev == &asr_vif->wdev)) {
		dev_info(asr_hw->dev, "%s clear roc\n", __func__);
		/* Initialize RoC element pointer to NULL, indicate that RoC can be started */
		asr_hw->roc_elem = NULL;
	}

	/* Ensure that we won't process disconnect ind */
	spin_lock_bh(&asr_hw->cb_lock);

	asr_vif->up = false;
	if (netif_carrier_ok(dev)) {
		if (ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_STATION ||
                    ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_P2P_CLIENT) {
#ifdef CONFIG_SME
			clear_bit(ASR_DEV_STA_DHCPEND, &asr_vif->dev_flags);

			if (test_and_clear_bit(ASR_DEV_STA_CONNECTED, &asr_vif->dev_flags)) {
				//asr_local_rx_deauth(asr_hw, asr_vif, ind->reason_code);
				asr_hw->rx_deauth_work.asr_hw = asr_hw;
				asr_hw->rx_deauth_work.asr_vif = asr_vif;
				asr_hw->rx_deauth_work.sta = asr_vif->sta.ap;
				asr_hw->rx_deauth_work.parm1 = 0;
				schedule_work(&asr_hw->rx_deauth_work.real_work);
			}
#else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
			cfg80211_disconnected(dev, WLAN_REASON_DEAUTH_LEAVING, NULL, 0, false, GFP_ATOMIC);
#else
			cfg80211_disconnected(dev, WLAN_REASON_DEAUTH_LEAVING, NULL, 0, GFP_ATOMIC);
#endif
#endif
			netif_tx_stop_all_queues(dev);
			netif_carrier_off(dev);
		} else {
			netdev_warn(dev, "AP not stopped when disabling interface");
		}
	}

	asr_hw->vif_table[asr_vif->vif_index] = NULL;

	dev_info(asr_hw->dev, "%s dev name:%s , vif_index=%d\n", __func__, asr_vif->wdev.netdev->name,asr_vif->vif_index);
    if (memcmp(asr_vif->wdev.netdev->name,"asrcfgwlan",10) == 0)
        asr_hw->ext_vif_index = 0xFF;
	else
	    asr_hw->vif_index = 0xFF;

	spin_unlock_bh(&asr_hw->cb_lock);

	asr_hw->vif_started--;
	if (asr_hw->vif_started == 0) {
		dev_info(asr_hw->dev, "vif_started become 0\n");
#ifdef CONFIG_ASR_USB
		if (!asr_hw->usb_remove_flag)
#endif
			asr_send_reset(asr_hw);

		// Set parameters to firmware
#ifdef CONFIG_ASR_USB
		if (!asr_hw->usb_remove_flag)
#endif
			asr_send_me_config_req(asr_hw);

		// Set channel parameters to firmware
#ifdef CONFIG_ASR_USB
		if (!asr_hw->usb_remove_flag)
#endif

			asr_send_me_chan_config_req(asr_hw);

		clear_bit(ASR_DEV_STARTED, &asr_hw->phy_flags);

		// clean host tx path
#ifdef CONFIG_ASR_SDIO

		if (asr_xmit_opt)  {
			spin_lock_bh(&asr_hw->tx_lock);
			asr_drop_tx_vif_skb(asr_hw,NULL);
			list_for_each_entry(asr_vif_tmp,&asr_hw->vifs,list){
	            asr_vif_tmp->tx_skb_cnt = 0;
			}
			spin_unlock_bh(&asr_hw->tx_lock);

		} else {

			spin_lock_bh(&asr_hw->tx_agg_env_lock);
			asr_tx_agg_buf_reset(asr_hw);
			list_for_each_entry(asr_vif_tmp,&asr_hw->vifs,list){
	            asr_vif_tmp->txring_bytes = 0;
			}
			spin_unlock_bh(&asr_hw->tx_agg_env_lock);
		}
#endif

	} else {
		// mrole vif: clean host tx path
#ifdef CONFIG_ASR_SDIO

		if (asr_xmit_opt) {
			spin_lock_bh(&asr_hw->tx_lock);
		    asr_drop_tx_vif_skb(asr_hw,asr_vif);
			asr_vif->tx_skb_cnt = 0;
			spin_unlock_bh(&asr_hw->tx_lock);

		} else {

			spin_lock_bh(&asr_hw->tx_agg_env_lock);
			asr_tx_agg_buf_mask_vif(asr_hw,asr_vif);
			spin_unlock_bh(&asr_hw->tx_agg_env_lock);
		}
#endif
	}

	{
		//clear rx_to_os_skb_list be empty when close
		while (!skb_queue_empty(&asr_hw->rx_to_os_skb_list)) {
			skb = skb_dequeue(&asr_hw->rx_to_os_skb_list);
#ifdef CONFIG_ASR_SDIO
            #ifndef SDIO_DEAGGR
			memset(skb->data, 0, asr_hw->ipc_env->rx_bufsz);
			// Add the sk buffer structure in the table of rx buffer
			#ifndef SDIO_RXBUF_SPLIT
			skb_queue_tail(&asr_hw->rx_sk_list, skb);
			#else
			skb_queue_tail(&asr_hw->rx_data_sk_list, skb);
			#endif
            #else
			memset(skb->data, 0, asr_hw->ipc_env->rx_bufsz_sdio_deagg);
			// Add the sk buffer structure in the table of rx buffer
			skb_queue_tail(&asr_hw->rx_sk_sdio_deaggr_list, skb);
            #endif
#else
			dev_kfree_skb(skb);
#endif

		}

        #ifdef CFG_OOO_UPLOAD
		while (!skb_queue_empty(&asr_hw->rx_pending_skb_list)) {
			skb = skb_dequeue(&asr_hw->rx_pending_skb_list);

            #ifdef CONFIG_ASR_USB
			dev_kfree_skb(skb);
			#else
			memset(skb->data, 0, asr_hw->ipc_env->rx_bufsz_sdio_deagg);
			// Add the sk buffer structure in the table of rx buffer
			skb_queue_tail(&asr_hw->rx_sk_sdio_deaggr_list, skb);	
			#endif
		}
		#endif

		//clear msgind_task_skb_list be empty when close
		while (!skb_queue_empty(&asr_hw->msgind_task_skb_list)) {
			skb = skb_dequeue(&asr_hw->msgind_task_skb_list);

			memset(skb->data, 0, asr_hw->ipc_env->rx_bufsz);
			// Add the sk buffer structure in the table of rx buffer
			#ifndef SDIO_RXBUF_SPLIT
			skb_queue_tail(&asr_hw->rx_sk_list, skb);
			#else
			skb_queue_tail(&asr_hw->rx_msg_sk_list, skb);
			#endif
		}
	}

    if (mrole_enable == false) {
		//disable other sta
		for (i = 0; i < asr_hw->sta_max_num; i++) {
			asr_hw->sta_table[i].valid = false;;
		}
	}

#ifdef CFG_SNIFFER_SUPPORT
	if (ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_MONITOR) {
		asr_hw->ipc_env->cb.recv_data_ind = asr_rxdataind;
	}
#endif

	clear_bit(ASR_DEV_CLOSEING, &asr_vif->dev_flags);
	clear_bit(ASR_DEV_STA_CONNECTED, &asr_vif->dev_flags);
	clear_bit(ASR_DEV_STA_DHCPEND, &asr_vif->dev_flags);
	clear_bit(ASR_DEV_STA_DEL_KEY, &asr_vif->dev_flags);

#ifdef CONFIG_ASR_SDIO
	// sdio mode used: clr drv flag for tx task.
#ifdef CONFIG_TWT
	clear_bit(ASR_DEV_STA_OUT_TWTSP,&asr_vif->dev_flags);
#endif
	clear_bit(ASR_DEV_TXQ_STOP_CSA,&asr_vif->dev_flags);
	clear_bit(ASR_DEV_TXQ_STOP_CHAN,&asr_vif->dev_flags);
	clear_bit(ASR_DEV_TXQ_STOP_VIF_PS,&asr_vif->dev_flags);
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 10, 0)
	asr_vif->ap_chandef.chan = NULL;
#endif

	netdev_info(dev, "CLOSE end, vif flags=0x%lx\n",asr_vif->dev_flags);
#ifdef CONFIG_ASR_USB
	dev_info(asr_hw->dev, "%s %d\n", __func__, asr_hw->usb_remove_flag);
#endif

#ifdef ASR_STATS_RATES_TIMER
	asr_hw->stats.tx_bytes = 0;
	asr_hw->stats.rx_bytes = 0;
	memset(asr_hw->stats.txrx_rates, 0, sizeof(asr_hw->stats.txrx_rates));
#endif

	return 0;
}

/**
 * struct net_device_stats* (*ndo_get_stats)(struct net_device *dev);
 *    Called when a user wants to get the network device usage
 *    statistics. Drivers must do one of the following:
 *    1. Define @ndo_get_stats64 to fill in a zero-initialised
 *       rtnl_link_stats64 structure passed by the caller.
 *    2. Define @ndo_get_stats to update a net_device_stats structure
 *       (which should normally be dev->stats) and return a pointer to
 *       it. The structure may be changed asynchronously only if each
 *       field is written atomically.
 *    3. Update dev->stats asynchronously and atomically, and define
 *       neither operation.
 */
static struct net_device_stats *asr_get_stats(struct net_device *dev)
{
	struct asr_vif *vif = netdev_priv(dev);

	return &vif->net_stats;
}

/**
 * int (*ndo_set_mac_address)(struct net_device *dev, void *addr);
 *    This function  is called when the Media Access Control address
 *    needs to be changed. If this interface is not defined, the
 *    mac address can not be changed.
 */
static int asr_set_mac_address(struct net_device *dev, void *addr)
{
	struct sockaddr *sa = addr;
	int ret;

	ret = eth_mac_addr(dev, sa);

	return ret;
}

//
#if 0
#define CTRL_LEN_CHECK(__x__,__y__) \
    do { \
        if((__x__ < __y__) || (__y__ < 0)) { \
            ASR_DBG("!!! error [%s][%d] len=%d \n",__FUNCTION__, __LINE__, __y__); \
        } \
    } while(0)

/****************************************************************/
/**** add private ioctl here      *******************************/
/****************************************************************/
#define ASR_IOCTL_SET_TXPOWER  (SIOCDEVPRIVATE + 0x1)	// 0x89f1
#define ASR_IOCTL_GET_TXPOWER  (SIOCDEVPRIVATE + 0x2)	// 0x89f2

struct iw_priv_args wlan_private_args[] = {
	{ASR_IOCTL_SET_TXPOWER, IW_PRIV_TYPE_CHAR | 750, 0, "set_txpower"},
	{ASR_IOCTL_GET_TXPOWER, IW_PRIV_TYPE_CHAR | 40,
	 IW_PRIV_TYPE_BYTE | 128, "get_txpower"},
};

int asr_set_tx_power(struct asr_hw *asr_hw, u8 * tmpbuf)
{
	// add set api here

	return 0;
}

int asr_get_txpwr(struct asr_hw *asr_hw, u8 * tmpbuf, int sizeof_tmpbuf)
{
	// add get api here

	return 0;
}

int asr_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{

	struct asr_vif *asr_vif = netdev_priv(dev);
	struct asr_hw *asr_hw = asr_vif->asr_hw;
	unsigned long flags;
	struct iwreq *wrq = (struct iwreq *)ifr;
	int i = 0, ret = -1, sizeof_tmpbuf;
	u8 tmpbuf1[2048] = { 0 };
	u8 *tmpbuf;

	ASR_DBG(ASR_FN_ENTRY_STR);;

	sizeof_tmpbuf = sizeof(tmpbuf1);
	tmpbuf = tmpbuf1;

	switch (cmd) {
	case ASR_IOCTL_SET_TXPOWER:
		CTRL_LEN_CHECK(sizeof_tmpbuf, wrq->u.data.length);
		if (copy_from_user(tmpbuf, (void *)wrq->u.data.pointer, wrq->u.data.length))
			break;
		ret = asr_set_tx_power(asr_hw, tmpbuf);
		break;

	case ASR_IOCTL_GET_TXPOWER:
		CTRL_LEN_CHECK(sizeof_tmpbuf, wrq->u.data.length);
		if (copy_from_user(tmpbuf, (void *)wrq->u.data.pointer, wrq->u.data.length))
			break;

		spin_lock_irqsave(&asr_hw->tx_agg_env_lock, flags);
		i = asr_get_txpwr(asr_hw, tmpbuf, sizeof_tmpbuf);
		spin_unlock_irqrestore(&asr_hw->tx_agg_env_lock, flags);

		if (i > 0) {
			if (copy_to_user((void *)wrq->u.data.pointer, tmpbuf, i))
				break;
			wrq->u.data.length = i;
			ret = 0;
		}
		break;

	}

	return ret;

}
#endif

/****************************************************************/
/**** add proc here      *******************************/
/****************************************************************/

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
#define CONFIG_ASR_PROC
#define CONFIG_FW_TXAGG_PROC
#endif

#ifdef CONFIG_ASR_PROC
#define DRV_PROCNAME    "asr"
static struct proc_dir_entry *libasr_proc = NULL;

#ifdef CONFIG_FW_TXAGG_PROC
// proc tx_aggr_disable
unsigned int libasr_fwtxagg_disable = 0;
EXPORT_SYMBOL_GPL(libasr_fwtxagg_disable);
static int fwtxagg_disable_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", libasr_fwtxagg_disable);
	return 0;
}

static int fwtxagg_disable_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, fwtxagg_disable_proc_show, NULL);
}

static ssize_t fwtxagg_disable_proc_write(struct file *file, const char __user * buffer, size_t count, loff_t * pos)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
	struct asr_hw *asr_hw = (struct asr_hw *)proc_get_parent_data(locks_inode(file));
#else
	struct asr_hw *asr_hw = (struct asr_hw *)PDE_DATA(file_inode(file));
#endif
	unsigned int fwtxagg_disable_old = libasr_fwtxagg_disable;

	char buf[32] = { 0 };
	size_t len = min(sizeof(buf) - 1, count);
	unsigned int val;

	if (copy_from_user(buf, buffer, len))
		return count;
	buf[len] = 0;

	if (sscanf(buf, "%d", &val) != 1)
		printk("fwtxagg proc: %s is not in hex or decimal form.\n", buf);
	else
		libasr_fwtxagg_disable = val ? 1 : 0;

	// send msg to fw.
	printk(" disable_proc_write (%d %d ,0x%x)\n", fwtxagg_disable_old,
	       libasr_fwtxagg_disable, (unsigned int)(uintptr_t) asr_hw);

	if ((fwtxagg_disable_old != libasr_fwtxagg_disable) && asr_hw)
		asr_send_me_tx_agg_disable(asr_hw, libasr_fwtxagg_disable);

	return strnlen(buf, len);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
static const struct proc_ops fwtxagg_disable_proc_fops = {
	.proc_open = fwtxagg_disable_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
	.proc_write = fwtxagg_disable_proc_write,
};
#else
static const struct file_operations fwtxagg_disable_proc_fops = {
	.owner = THIS_MODULE,
	.open = fwtxagg_disable_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = fwtxagg_disable_proc_write,
};
#endif
#endif /* CONFIG_FW_TXAGG_PROC */

#ifdef CONFIG_TWT
// proc twt
extern bool g_twt_on;
extern wifi_twt_config_t g_wifi_twt_param;
static int twt_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s,%d,%d,%d,%d,%d,%d\n", (g_twt_on ? "add" : "del"),
	                               g_wifi_twt_param.setup_cmd,g_wifi_twt_param.flow_type,
	                               g_wifi_twt_param.wake_interval_exponent,g_wifi_twt_param.wake_interval_mantissa,
								   g_wifi_twt_param.monimal_min_wake_duration,g_wifi_twt_param.twt_upload_wait_trigger);
	return 0;
}

static int twt_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, twt_proc_show, NULL);
}

/*
 * add,[interval_exp],[interval_mantissa],[min_wake_dur],[setup_cmd],[flow_type]"
   del,[flow_id]
 *
*/
extern int set_twt(struct asr_hw *asr_hw, unsigned char *data);
static ssize_t twt_proc_write(struct file *file, const char __user * buffer, size_t count, loff_t * pos)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
	struct asr_hw *asr_hw = (struct asr_hw *)proc_get_parent_data(locks_inode(file));
#else
	struct asr_hw *asr_hw = (struct asr_hw *)PDE_DATA(file_inode(file));
#endif
	char buf[256] = { 0 };
	size_t len = min(sizeof(buf) - 1, count);
    int ret = -1;
    int copy_len = 0;

	if (copy_from_user(buf, buffer, len)){
		printk("twt copy from user fail, %d ,%d",(u32)count,(u32)len);
		return count;
	}

	buf[len] = 0;

    copy_len = strnlen(buf, len);

	printk(" twt_proc_write [%s] (%d,%d,0x%x), %d\n", buf,(u32)count,(u32)len,(unsigned int)(uintptr_t) asr_hw,copy_len);

	ret = set_twt(asr_hw,buf);
	if (ret){
		printk(" set twt fail \n");
	}

	return copy_len;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
static const struct proc_ops twt_proc_fops = {
	.proc_open = twt_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
	.proc_write = twt_proc_write,
};
#else
static const struct file_operations twt_proc_fops = {
	.owner = THIS_MODULE,
	.open = twt_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = twt_proc_write,
};
#endif
#endif /*CONFIG_TWT*/

#endif /*CONFIG_ASR_PROC*/

static const struct net_device_ops asr_netdev_ops = {
	.ndo_open = asr_open,
	.ndo_stop = asr_close,
	.ndo_start_xmit = asr_start_xmit,
	.ndo_get_stats = asr_get_stats,
	.ndo_do_ioctl = asr_ioctl,
	.ndo_select_queue = asr_select_queue,
	.ndo_set_mac_address = asr_set_mac_address
};

static void asr_netdev_setup(struct net_device *dev)
{
	//dev_err(asr_hw->dev, "%s\n", __func__);
	ether_setup(dev);
	dev->priv_flags &= ~IFF_TX_SKB_SHARING;
	dev->netdev_ops = &asr_netdev_ops;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	dev->priv_destructor = free_netdev;
#else
	dev->destructor = free_netdev;
#endif
	dev->watchdog_timeo = ASR_TX_LIFETIME_MS;

	dev->needed_headroom = sizeof(struct asr_txhdr) + ASR_SWTXHDR_ALIGN_SZ;
	dev_info(g_asr_para.dev, "%s maxheadroom %d\n", __func__, dev->needed_headroom);

	dev->hw_features = 0;
}

/*********************************************************************
 * Cfg80211 callbacks (and helper)
 *********************************************************************/
static struct wireless_dev *asr_interface_add(struct asr_hw *asr_hw,
					      const char *name, enum nl80211_iftype type, struct vif_params *params)
{
	struct net_device *ndev;
	struct asr_vif *vif;
	int min_idx, max_idx;
	int vif_idx = -1;
	int i;
    u8 perm_addr[ETH_ALEN] = {0};

	ASR_DBG(ASR_FN_ENTRY_STR);

	// Look for an available VIF
	min_idx = 0;
	max_idx = asr_hw->vif_max_num;

	for (i = min_idx; i < max_idx; i++) {
		if ((asr_hw->avail_idx_map) & BIT(i)) {
			vif_idx = i;
			break;
		}
	}
	if (vif_idx < 0)
		return NULL;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
	ndev = alloc_netdev_mqs(sizeof(*vif), name, NET_NAME_UNKNOWN, asr_netdev_setup, NX_NB_NDEV_TXQ, 1);
#else
	ndev = alloc_netdev_mqs(sizeof(*vif), name, asr_netdev_setup, NX_NB_NDEV_TXQ, 1);
#endif
	if (!ndev)
		return NULL;

	vif = netdev_priv(ndev);
	ndev->ieee80211_ptr = &vif->wdev;
	vif->wdev.wiphy = asr_hw->wiphy;
	vif->asr_hw = asr_hw;
	vif->ndev = ndev;
	vif->drv_vif_index = vif_idx;
	SET_NETDEV_DEV(ndev, wiphy_dev(vif->wdev.wiphy));
	vif->wdev.netdev = ndev;
	vif->wdev.iftype = type;
	vif->up = false;
	vif->ch_index = ASR_CH_NOT_SET;
	vif->generation = 0;
	// mrole add for fc.
	vif->txring_bytes = 0;
	vif->tx_skb_cnt = 0;

	memset(&vif->net_stats, 0, sizeof(vif->net_stats));

	switch (type) {
	case NL80211_IFTYPE_STATION:
    case NL80211_IFTYPE_P2P_CLIENT:
		vif->sta.ap = NULL;
		break;
	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_P2P_GO:
		INIT_LIST_HEAD(&vif->ap.sta_list);
		memset(&vif->ap.bcn, 0, sizeof(vif->ap.bcn));
		break;
#ifdef CFG_SNIFFER_SUPPORT
	case NL80211_IFTYPE_MONITOR:
		vif->sniffer.rx_filter = 0;
		vif->sniffer.chan_num = 0;
		break;
#endif
	default:
		break;
	}

	//different interface,different mac address	
	memcpy(perm_addr, asr_hw->wiphy->perm_addr, ETH_ALEN);
	perm_addr[5] ^= vif_idx;
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
	dev_addr_set(ndev, perm_addr);
	#else
	memcpy(ndev->dev_addr, perm_addr, ETH_ALEN);
	#endif


	if (params) {
		vif->use_4addr = params->use_4addr;
		ndev->ieee80211_ptr->use_4addr = params->use_4addr;
	} else
		vif->use_4addr = false;

	if (register_netdevice(ndev))
		goto err;

	spin_lock_bh(&asr_hw->cb_lock);
	list_add_tail(&vif->list, &asr_hw->vifs);
	spin_unlock_bh(&asr_hw->cb_lock);
	asr_hw->avail_idx_map &= ~BIT(vif_idx);

	return &vif->wdev;

err:
	free_netdev(ndev);
	return NULL;
}

/*
 * @brief Retrieve the asr_sta object allocated for a given MAC address
 * and a given role.
 */
static struct asr_sta *asr_retrieve_sta(struct asr_hw *asr_hw, struct asr_vif *asr_vif, u8 * addr, __le16 fc, bool ap)
{
	if (ap) {
		/* only deauth, disassoc and action are bufferable MMPDUs */
		bool bufferable = ieee80211_is_deauth(fc) || ieee80211_is_disassoc(fc) || ieee80211_is_action(fc);

		/* Check if the packet is bufferable or not */
		if (bufferable) {
			/* Check if address is a broadcast or a multicast address */
			if (is_broadcast_ether_addr(addr)
			    || is_multicast_ether_addr(addr)) {
				/* Returned STA pointer */
				struct asr_sta *asr_sta = &asr_hw->sta_table[asr_vif->ap.bcmc_index];

				if (asr_sta->valid)
					return asr_sta;
			} else {
				/* Returned STA pointer */
				struct asr_sta *asr_sta;

				/* Go through list of STAs linked with the provided VIF */
				list_for_each_entry(asr_sta, &asr_vif->ap.sta_list, list) {
					if (asr_sta->valid && ether_addr_equal(asr_sta->mac_addr, addr)) {
						/* Return the found STA */
						return asr_sta;
					}
				}
			}
		}
	} else {
		return asr_vif->sta.ap;
	}

	return NULL;
}

/**
 * @add_virtual_intf: create a new virtual interface with the given name,
 *    must set the struct wireless_dev's iftype. Beware: You must create
 *    the new netdev in the wiphy's network namespace! Returns the struct
 *    wireless_dev, or an ERR_PTR. For P2P device wdevs, the driver must
 *    also set the address member in the wdev.
 */
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0))
static struct wireless_dev *asr_cfg80211_add_iface(struct wiphy *wiphy, const char *name,
#else
static struct net_device *asr_cfg80211_add_iface(struct wiphy *wiphy, char *name,
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
						 unsigned char name_assign_type,
#endif
						 enum nl80211_iftype type,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
						 u32 * flags,
#endif
						 struct vif_params *params)
{
	struct asr_hw *asr_hw = wiphy_priv(wiphy);
	struct wireless_dev *wdev;

	wdev = asr_interface_add(asr_hw, name, type, params);

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0))
	return wdev;
#else
	return wdev->netdev;
#endif
}

/**
 * @del_virtual_intf: remove the virtual interface
 */
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0))
static int asr_cfg80211_del_iface(struct wiphy *wiphy, struct wireless_dev *wdev)
{
	struct net_device *dev = wdev->netdev;
#else
static int asr_cfg80211_del_iface(struct wiphy *wiphy, struct net_device *dev)
{
#endif

	struct asr_vif *asr_vif = netdev_priv(dev);
	struct asr_hw *asr_hw = wiphy_priv(wiphy);

	netdev_info(dev, "%s:Remove Interface", __func__);

	if (dev->reg_state == NETREG_REGISTERED) {
		/* Will call asr_close if interface is UP */
		unregister_netdevice(dev);
	}

	spin_lock_bh(&asr_hw->cb_lock);
	list_del(&asr_vif->list);
	spin_unlock_bh(&asr_hw->cb_lock);
	asr_hw->avail_idx_map |= BIT(asr_vif->drv_vif_index);
	asr_vif->ndev = NULL;

	/* Clear the priv in adapter */
	dev->ieee80211_ptr = NULL;

	return 0;
}

/**
 * @change_virtual_intf: change type/configuration of virtual interface,
 *    keep the struct wireless_dev's iftype updated.
 */
static int asr_cfg80211_change_iface(struct wiphy *wiphy, struct net_device *dev, enum nl80211_iftype type,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
				     u32 * flags,
#endif
				     struct vif_params *params)
{
	struct asr_vif *vif = netdev_priv(dev);
	ASR_DBG(ASR_FN_ENTRY_STR);

	if (vif->up)
		return (-EBUSY);

	switch (type) {
	case NL80211_IFTYPE_STATION:
	case NL80211_IFTYPE_P2P_CLIENT:
		vif->sta.ap = NULL;
		break;
	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_P2P_GO:
		INIT_LIST_HEAD(&vif->ap.sta_list);
		memset(&vif->ap.bcn, 0, sizeof(vif->ap.bcn));
		break;
	default:
		break;
	}

	vif->generation = 0;
	vif->wdev.iftype = type;
	if (params->use_4addr != -1)
		vif->use_4addr = params->use_4addr;

	return 0;
}

u8 scan_times = 0;
/**
 * @scan: Request to do a scan. If returning zero, the scan request is given
 *    the driver, and will be valid until passed to cfg80211_scan_done().
 *    For scan results, call cfg80211_inform_bss(); you can call this outside
 *    the scan/scan_done bracket too.
 */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 10, 0)
static int asr_cfg80211_scan(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_scan_request *request)
#else
static int asr_cfg80211_scan(struct wiphy *wiphy, struct cfg80211_scan_request *request)
#endif
{
	struct asr_hw *asr_hw = wiphy_priv(wiphy);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 10, 0)
	struct asr_vif *asr_vif = netdev_priv(request->dev);
#else
	struct asr_vif *asr_vif = container_of(request->wdev, struct asr_vif,
					       wdev);
#endif
	int error = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	if (scan_times++ >= 2) {
		print_dbg();
	}

	if (asr_hw->scan_vif_index != 0xFF) {
		dev_err(asr_hw->dev, "[%s %d] vif(%d) scan is on-going \n",
			__func__, __LINE__, asr_hw->scan_vif_index);
	    return -EBUSY;
	}
	if ((error = asr_send_scanu_req(asr_hw, asr_vif, request)))
		return error;

	asr_hw->scan_request = request;
    asr_hw->scan_vif_index = asr_vif->vif_index ;

	return 0;
}

/**
 * @add_key: add a key with the given parameters. @mac_addr will be %NULL
 *    when adding a group key.
 */
static int asr_cfg80211_add_key(struct wiphy *wiphy, struct net_device *netdev,
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
	int link_id,
	#endif
	u8 key_index, bool pairwise, const u8 * mac_addr, struct key_params *params)
{
	struct asr_hw *asr_hw = wiphy_priv(wiphy);
	struct asr_vif *vif = netdev_priv(netdev);
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
	int i = 0, error = 0;
#else
	int error = 0;
#endif
	struct mm_key_add_cfm key_add_cfm;
	u8 cipher = 0;
	struct asr_sta *sta = NULL;
	struct asr_key *asr_key;

	ASR_DBG(ASR_FN_ENTRY_STR);

	if (mac_addr) {
		#if 0//def CFG_ROAMING
		if(vif->auth_type == NL80211_AUTHTYPE_FT){
			for (i = 0; i < asr_hw->sta_max_num; i++) {
				struct asr_sta *sta = &asr_hw->sta_table[i];
				memcpy(sta->mac_addr, mac_addr, ETH_ALEN);
			}
		}
		#endif
		sta = asr_get_sta(asr_hw, mac_addr);
		if (!sta) {
			dev_err(asr_hw->dev, "[%s %d] err mac=(%x:%x:%x:%x:%x:%x)\n",
				__func__, __LINE__, mac_addr[0], mac_addr[1],
				mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
			return -EINVAL;
		}
		asr_key = &sta->key;
	} else
		asr_key = &vif->key[key_index];

	/* Retrieve the cipher suite selector */
	switch (params->cipher) {
	case WLAN_CIPHER_SUITE_WEP40:
		cipher = MAC_RSNIE_CIPHER_WEP40;
		break;
	case WLAN_CIPHER_SUITE_WEP104:
		cipher = MAC_RSNIE_CIPHER_WEP104;
		break;
	case WLAN_CIPHER_SUITE_TKIP:
		cipher = MAC_RSNIE_CIPHER_TKIP;
		break;
	case WLAN_CIPHER_SUITE_CCMP:
		cipher = MAC_RSNIE_CIPHER_CCMP;
		break;
	case WLAN_CIPHER_SUITE_AES_CMAC:
		cipher = MAC_RSNIE_CIPHER_AES_CMAC;
		break;
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
	case WLAN_CIPHER_SUITE_SMS4:
		{
			// Need to reverse key order
			u8 tmp, *key = (u8 *) params->key;
			cipher = MAC_RSNIE_CIPHER_SMS4;
			for (i = 0; i < WPI_SUBKEY_LEN / 2; i++) {
				tmp = key[i];
				key[i] = key[WPI_SUBKEY_LEN - 1 - i];
				key[WPI_SUBKEY_LEN - 1 - i] = tmp;
			}
			for (i = 0; i < WPI_SUBKEY_LEN / 2; i++) {
				tmp = key[i + WPI_SUBKEY_LEN];
				key[i + WPI_SUBKEY_LEN] = key[WPI_KEY_LEN - 1 - i];
				key[WPI_KEY_LEN - 1 - i] = tmp;
			}
			break;
		}
#endif
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 33)
	case WLAN_CIPHER_SUITE_GCMP:
		cipher = MAC_RSNIE_CIPHER_GCMP_128;
		break;
	case WLAN_CIPHER_SUITE_GCMP_256:
		cipher = MAC_RSNIE_CIPHER_GCMP_256;
		break;
	case WLAN_CIPHER_SUITE_CCMP_256:
		cipher = MAC_RSNIE_CIPHER_CCMP_256;
		break;
	case WLAN_CIPHER_SUITE_BIP_GMAC_128:
		cipher = MAC_RSNIE_CIPHER_BIP_GMAC_128;
		break;
	case WLAN_CIPHER_SUITE_BIP_GMAC_256:
		cipher = MAC_RSNIE_CIPHER_BIP_GMAC_256;
		break;
#endif
	default:
		return -EINVAL;
	}

	if ((error = asr_send_key_add(asr_hw, vif->vif_index,
				      (sta ? sta->sta_idx : 0xFF), pairwise,
				      (u8 *) params->key, params->key_len, key_index, cipher, &key_add_cfm)))
		return error;

	if (key_add_cfm.status != 0) {
		ASR_PRINT_CFM_ERR(key_add);
		return -EIO;
	}

	/* Save the index retrieved from LMAC */
	asr_key->hw_idx = key_add_cfm.hw_key_idx;

	if (mac_addr) {
		dev_info(asr_hw->dev, "%s: mac=%x:%x:%x:%x:%x:%x,key_index=%d,pairwise=%d , hw_idx=%d\n", __func__,
			 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5],
			 key_index, pairwise,asr_key->hw_idx);
	} else {
		dev_info(asr_hw->dev, "%s: mac_addr is NULL , key_index=%d,pairwise=%d , hw_idx=%d\n", __func__, key_index, pairwise,asr_key->hw_idx);
	}

	return 0;
}

/**
 * @get_key: get information about the key with the given parameters.
 *    @mac_addr will be %NULL when requesting information for a group
 *    key. All pointers given to the @callback function need not be valid
 *    after it returns. This function should return an error if it is
 *    not possible to retrieve the key, -ENOENT if it doesn't exist.
 *
 */
static int asr_cfg80211_get_key(struct wiphy *wiphy, struct net_device *netdev,
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
	int link_id,
	#endif
	u8 key_index, bool pairwise,
	const u8 * mac_addr, void *cookie, void (*callback) (void *cookie, struct key_params *))
{
	ASR_DBG(ASR_FN_ENTRY_STR);

	return -1;
}

/**
 * @del_key: remove a key given the @mac_addr (%NULL for a group key)
 *    and @key_index, return -ENOENT if the key doesn't exist.
 */
static int asr_cfg80211_del_key(struct wiphy *wiphy, struct net_device *netdev,
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
	int link_id,
	#endif
	u8 key_index, bool pairwise, const u8 * mac_addr)
{
	struct asr_hw *asr_hw = wiphy_priv(wiphy);
	struct asr_vif *asr_vif = netdev_priv(netdev);
	int error;
	struct asr_sta *sta = NULL;
	struct asr_key *asr_key;

	ASR_DBG(ASR_FN_ENTRY_STR);
	if (mac_addr) {
		sta = asr_get_sta(asr_hw, mac_addr);
		if (!sta) {
			dev_err(asr_hw->dev, "[%s %d] err mac=(%x:%x:%x:%x:%x:%x)\n",
				__func__, __LINE__, mac_addr[0], mac_addr[1],
				mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

			#ifdef CFG_ROAMING
			//asr_vif->is_ap1_deauthed = false;// the last time delete key mac is ap2
			if (key_index == ASR_PAIRWISE_KEY_NUM - 1 || mac_addr) {
				clear_bit(ASR_DEV_STA_DEL_KEY, &asr_vif->dev_flags);
				return -ENOENT;
			}
			#endif

			return -EINVAL;
		}
		asr_key = &sta->key;

		dev_info(asr_hw->dev, "%s: mac=%x:%x:%x:%x:%x:%x,key_index=%d,pairwise=%d , hw_idx=%d\n", __func__,
			 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5],
			 key_index, pairwise, asr_key->hw_idx);
	} else {
		asr_key = &asr_vif->key[key_index];

		dev_info(asr_hw->dev, "%s: mac is NULL , key_index=%d,pairwise=%d ,hw_idx=%d state:%08X\n", __func__, key_index, pairwise,asr_key->hw_idx,(u32)asr_vif->dev_flags);
	}

	if (key_index < ASR_PAIRWISE_KEY_NUM) {
		set_bit(ASR_DEV_STA_DEL_KEY, &asr_vif->dev_flags);
	}

    #if 0
	if (test_bit(ASR_DEV_STA_CONNECTED, &asr_vif->dev_flags)
	    && !test_bit(ASR_DEV_STA_DISCONNECTING, &asr_vif->dev_flags)
	    #ifdef CFG_ROAMING
	    &&!asr_vif->is_ap1_deauthed
	    #endif
		) {
                // 7205 bugfix for wpa_supplicant killed.
		// asr_send_sm_disconnect_req(asr_hw, asr_vif, 1);
		#ifdef CFG_ROAMING
	        asr_vif->is_ap1_deauthed = true;// delete key 3 times ,but only need send deauth only once enough
	        asr_vif->is_roam = false;
		dev_err(asr_hw->dev, "%s: sync disconnect state:%08X\n", __func__,(u32)asr_vif->dev_flags);
		#endif
	}
	#endif

	error = asr_send_key_del(asr_hw, asr_key->hw_idx);

	if (key_index == ASR_PAIRWISE_KEY_NUM - 1 || mac_addr) {
		clear_bit(ASR_DEV_STA_DEL_KEY, &asr_vif->dev_flags);
	}

	return error;
}

/**
 * @set_default_key: set the default key on an interface
 */
static int asr_cfg80211_set_default_key(struct wiphy *wiphy, struct net_device *netdev,
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
	int link_id,
	#endif
	u8 key_index, bool unicast, bool multicast)
{
	ASR_DBG(ASR_FN_ENTRY_STR);

	return 0;
}

/**
 * @set_default_mgmt_key: set the default management frame key on an interface
 */
static int asr_cfg80211_set_default_mgmt_key(struct wiphy *wiphy, struct net_device *netdev,
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
	int link_id,
	#endif
	u8 key_index)
{
	return 0;
}

/**
 * @set_power_mgmt: set power-save on an interface
 */

static int asr_cfg80211_set_power_mgmt(struct wiphy *wiphy, struct net_device *dev, bool enabled, int timeout)
{

	struct asr_hw *asr_hw = wiphy_priv(wiphy);
#ifdef CONFIG_ASR595X
	struct asr_vif *asr_vif = netdev_priv(dev);
#endif

	ASR_DBG(ASR_FN_ENTRY_STR);

#ifdef CONFIG_ASR595X
	if (asr_vif != NULL && ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_STATION
		&& (!enabled || (test_bit(ASR_DEV_STA_CONNECTED, &asr_vif->dev_flags)
		&& test_bit(ASR_DEV_STA_DHCPEND, &asr_vif->dev_flags) ) ) ) {

		dev_info(asr_hw->dev, "set power mgmt(%d)\n", enabled);

		/* Forward the information to the LMAC */
		return (asr_send_set_ps_mode(asr_hw, enabled));
	} else {
		dev_err(asr_hw->dev, "set power mgmt(%d) fail, dev_flags=0x%lX\n", enabled, asr_vif->dev_flags);
		return -EBUSY;
	}
#else
	dev_err(asr_hw->dev, "power save not supported !\n");
	return 0;
#endif

}

#ifdef CONFIG_SME
int asr_cfg80211_auth(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_auth_request *req)
{
	struct asr_hw *asr_hw = wiphy_priv(wiphy);
	struct asr_vif *asr_vif = netdev_priv(dev);
	struct sm_auth_cfm sm_auth_cfm;
	int error = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	if (test_bit(ASR_DEV_CLOSEING, &asr_vif->dev_flags)
	    || !test_bit(ASR_DEV_STARTED, &asr_hw->phy_flags)
	    || test_bit(ASR_DEV_STA_DISCONNECTING, &asr_vif->dev_flags)
	    || test_bit(ASR_DEV_SCANING, &asr_vif->dev_flags)
	    || test_bit(ASR_DEV_STA_DEL_KEY, &asr_vif->dev_flags)) {
		dev_err(asr_hw->dev, "%s:fail=-EBUSY,phy_flags=0x%lX , dev_flags=0x%lX\n", __func__, asr_hw->phy_flags,
		                                               asr_vif->dev_flags );
		return -EBUSY;
	}

	set_bit(ASR_DEV_STA_CONNECTING, &asr_vif->dev_flags);

	/* For SHARED-KEY authentication, must install key first */
	if (req->auth_type == NL80211_AUTHTYPE_SHARED_KEY && req->key) {
		struct key_params key_params;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
		key_params.key = req->key;
#else
		key_params.key = (u8 *) req->key;
#endif
		key_params.seq = NULL;
		key_params.key_len = req->key_len;
		key_params.seq_len = 0;
		key_params.cipher = WLAN_CIPHER_SUITE_WEP40;
		#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
		asr_cfg80211_add_key(wiphy, dev, 0, req->key_idx, false, NULL, &key_params);
		#else
		asr_cfg80211_add_key(wiphy, dev, req->key_idx, false, NULL, &key_params);
		#endif
	}

	/* Forward the information to the LMAC */
	if ((error = asr_send_sm_auth_req(asr_hw, asr_vif, req, &sm_auth_cfm))) {
		clear_bit(ASR_DEV_STA_CONNECTING, &asr_vif->dev_flags);
		return error;
	}

	if (sm_auth_cfm.status != CO_OK) {
		clear_bit(ASR_DEV_STA_CONNECTING, &asr_vif->dev_flags);
	}
	// Check the status
	switch (sm_auth_cfm.status) {
	case CO_OK:
		error = 0;
		break;
	case CO_BUSY:
		error = -EINPROGRESS;
		break;
	case CO_OP_IN_PROGRESS:
		error = -EALREADY;
		break;
	default:
		error = -EIO;
		break;
	}

	return error;
}

int asr_cfg80211_assoc(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_assoc_request *req)
{
	struct asr_hw *asr_hw = wiphy_priv(wiphy);
	struct asr_vif *asr_vif = netdev_priv(dev);
	struct sm_assoc_cfm sm_assoc_cfm;
	int error = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Forward the information to the LMAC */
	if ((error = asr_send_sm_assoc_req(asr_hw, asr_vif, req, &sm_assoc_cfm)))
		return error;

	// Check the status
	switch (sm_assoc_cfm.status) {
	case CO_OK:
		error = 0;
		break;
	case CO_BUSY:
		error = -EINPROGRESS;
		break;
	case CO_OP_IN_PROGRESS:
		error = -EALREADY;
		break;
	default:
		error = -EIO;
		break;
	}

	return error;
}
#endif

/**
 * @connect: Connect to the ESS with the specified parameters. When connected,
 *    call cfg80211_connect_result() with status code %WLAN_STATUS_SUCCESS.
 *    If the connection fails for some reason, call cfg80211_connect_result()
 *    with the status from the AP.
 *    (invoked with the wireless_dev mutex held)
 */
#ifndef CONFIG_SME
static int asr_cfg80211_connect(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_connect_params *sme)
{
	struct asr_hw *asr_hw = wiphy_priv(wiphy);
	struct asr_vif *asr_vif = netdev_priv(dev);
	struct sm_connect_cfm sm_connect_cfm;
	int error = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* For SHARED-KEY authentication, must install key first */
	if (sme->auth_type == NL80211_AUTHTYPE_SHARED_KEY && sme->key) {
		struct key_params key_params;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
		key_params.key = sme->key;
#else
		key_params.key = (u8 *) sme->key;
#endif
		key_params.seq = NULL;
		key_params.key_len = sme->key_len;
		key_params.seq_len = 0;
		key_params.cipher = sme->crypto.cipher_group;
		#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
		asr_cfg80211_add_key(wiphy, dev, 0, req->key_idx, false, NULL, &key_params);
		#else
		asr_cfg80211_add_key(wiphy, dev, req->key_idx, false, NULL, &key_params);
		#endif
	}

	/* Forward the information to the LMAC */
	if ((error = asr_send_sm_connect_req(asr_hw, asr_vif, sme, &sm_connect_cfm)))
		return error;

	// Check the status
	switch (sm_connect_cfm.status) {
	case CO_OK:
		error = 0;
		break;
	case CO_BUSY:
		error = -EINPROGRESS;
		break;
	case CO_OP_IN_PROGRESS:
		error = -EALREADY;
		break;
	default:
		error = -EIO;
		break;
	}

	return error;
}
#endif

#ifdef CONFIG_SME
int asr_cfg80211_deauth(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_deauth_request *req
	#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 2, 0)
	,void *cookie
	#endif
	)
{
	struct asr_hw *asr_hw = wiphy_priv(wiphy);
	struct asr_vif *asr_vif = netdev_priv(dev);
	u16 reason_code = req->reason_code;

	ASR_DBG(ASR_FN_ENTRY_STR);
	dev_info(asr_hw->dev, "%s: reason_code=%d,dev_flags=0x%lX\n", __func__, reason_code, asr_vif->dev_flags);

	if (test_bit(ASR_DEV_STA_CONNECTED, &asr_vif->dev_flags)
	    || test_bit(ASR_DEV_STA_CONNECTING, &asr_vif->dev_flags)) {

		return (asr_send_sm_disconnect_req(asr_hw, asr_vif, reason_code));
	}

	return 0;
}

int asr_cfg80211_disassoc(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_disassoc_request *req
	#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 2, 0)
	,void *cookie
	#endif
	)
{
	struct asr_hw *asr_hw = wiphy_priv(wiphy);
	struct asr_vif *asr_vif = netdev_priv(dev);
	u16 reason_code = req->reason_code;

	dev_info(asr_hw->dev, "%s: reason_code=%d,dev_flags=0x%lX\n", __func__, reason_code, asr_vif->dev_flags);

	if (test_bit(ASR_DEV_STA_CONNECTED, &asr_vif->dev_flags)
	    || test_bit(ASR_DEV_STA_CONNECTING, &asr_vif->dev_flags)) {

		return (asr_send_sm_disconnect_req(asr_hw, asr_vif, reason_code));
	}

	return 0;
}
#endif

/**
 * @disconnect: Disconnect from the BSS/ESS.
 *    (invoked with the wireless_dev mutex held)
 */
static int asr_cfg80211_disconnect(struct wiphy *wiphy, struct net_device *dev, u16 reason_code)
{
	struct asr_hw *asr_hw = wiphy_priv(wiphy);
	struct asr_vif *asr_vif = netdev_priv(dev);

	ASR_DBG(ASR_FN_ENTRY_STR);

	print_dbg();

	return (asr_send_sm_disconnect_req(asr_hw, asr_vif, reason_code));
}

/**
 * @add_station: Add a new station.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
static int asr_cfg80211_add_station(struct wiphy *wiphy,
				    struct net_device *dev, const u8 * mac, struct station_parameters *params)
#else
static int asr_cfg80211_add_station(struct wiphy *wiphy,
				    struct net_device *dev, u8 * mac, struct station_parameters *params)
#endif
{
	struct asr_hw *asr_hw = wiphy_priv(wiphy);
	struct asr_vif *asr_vif = netdev_priv(dev);
	struct me_sta_add_cfm me_sta_add_cfm;
	int error = 0;
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
	struct link_station_parameters *sta_param = &params->link_sta_params;
	#else
	struct station_parameters *sta_param = params;
	#endif

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Indicate we are in a STA addition process - This will allow handling
	 * potential PS mode change indications correctly
	 */
	asr_hw->adding_sta = true;

	/* Forward the information to the LMAC */
	if ((error = asr_send_me_sta_add(asr_hw, params, mac, asr_vif->vif_index, &me_sta_add_cfm))) {
		dev_info(asr_hw->dev,
			"ASR: %s,add sta fail..., error=%d,ch_index=%d,vif_index=%d.\n",
			__func__, error, asr_vif->ch_index, asr_vif->vif_index);
		return error;
	} else {
		dev_info(asr_hw->dev,
			"ASR: %s,status=%d,sta_idx=%d,ch_index=%d,vif_index=%d.\n",
			__func__, me_sta_add_cfm.status, me_sta_add_cfm.sta_idx, asr_vif->ch_index, asr_vif->vif_index);
	}

	// Check the status
	switch (me_sta_add_cfm.status) {
	case CO_OK:
		{
			struct asr_sta *sta = &asr_hw->sta_table[me_sta_add_cfm.sta_idx];
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
			int tid;
#endif
			sta->aid = params->aid;

			sta->sta_idx = me_sta_add_cfm.sta_idx;
			sta->ch_idx = asr_vif->ch_index;
			sta->vif_idx = asr_vif->vif_index;
			sta->vlan_idx = sta->vif_idx;
			sta->qos = (params->sta_flags_set & BIT(NL80211_STA_FLAG_WME))
			    != 0;
			sta->ht = sta_param->ht_capa ? 1 : 0;
			sta->acm = 0;
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
			for (tid = 0; tid < NX_NB_TXQ_PER_STA; tid++) {
				int uapsd_bit = asr_hwq2uapsd[asr_tid2hwq[tid]];
				if (params->uapsd_queues & uapsd_bit)
					sta->uapsd_tids |= 1 << tid;
				else
					sta->uapsd_tids &= ~(1 << tid);
			}
#endif
			memcpy(sta->mac_addr, mac, ETH_ALEN);
			asr_dbgfs_register_rc_stat(asr_hw, sta);

			/* Ensure that we won't process PS change or channel switch ind */
			spin_lock_bh(&asr_hw->cb_lock);
			asr_txq_sta_init(asr_hw, sta, asr_txq_vif_get_status(asr_vif));
			list_add_tail(&sta->list, &asr_vif->ap.sta_list);
			sta->valid = true;
			
			asr_ps_bh_enable(asr_hw, sta, sta->ps.active || me_sta_add_cfm.pm_state);
			
			spin_unlock_bh(&asr_hw->cb_lock);
			asr_vif->generation++;
			error = 0;

#define PRINT_STA_FLAG(f)                               \
                (params->sta_flags_set & BIT(NL80211_STA_FLAG_##f) ? "["#f"]" : "")

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
			netdev_info(dev,
				    "Add sta %d (%pM) uapsd(0x%x) ,flags=%s%s%s%s%s%s%s",
				    sta->sta_idx, mac,sta->uapsd_tids,
				    PRINT_STA_FLAG(AUTHORIZED),
				    PRINT_STA_FLAG(SHORT_PREAMBLE),
				    PRINT_STA_FLAG(WME), PRINT_STA_FLAG(MFP),
				    PRINT_STA_FLAG(AUTHENTICATED),
				    PRINT_STA_FLAG(TDLS_PEER), PRINT_STA_FLAG(ASSOCIATED));
#else
			netdev_info(dev,
				    "Add sta %d (%pM) uapsd(0x%x) ,flags=%s%s%s%s%s",
				    sta->sta_idx, mac,sta->uapsd_tids,
				    PRINT_STA_FLAG(AUTHORIZED),
				    PRINT_STA_FLAG(SHORT_PREAMBLE),
				    PRINT_STA_FLAG(WME), PRINT_STA_FLAG(MFP), PRINT_STA_FLAG(AUTHENTICATED));
#endif
#undef PRINT_STA_FLAG
			break;
		}
	default:
		error = -EBUSY;
		break;
	}

	asr_hw->adding_sta = false;

	return error;
}

/**
 * @del_station: Remove a station
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
static int asr_cfg80211_del_station(struct wiphy *wiphy, struct net_device *dev, struct station_del_parameters *params)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
static int asr_cfg80211_del_station(struct wiphy *wiphy, struct net_device *dev, const u8 * mac)
#else
static int asr_cfg80211_del_station(struct wiphy *wiphy, struct net_device *dev, u8 * mac)
#endif
{
	struct asr_hw *asr_hw = wiphy_priv(wiphy);
	struct asr_vif *asr_vif = netdev_priv(dev);
	struct asr_sta *cur, *tmp;
	int error = 0, found = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
	const u8 *mac = NULL;
	if (params)
		mac = params->mac;
#endif

	list_for_each_entry_safe(cur, tmp, &asr_vif->ap.sta_list, list) {
		if ((!mac) || (!memcmp(cur->mac_addr, mac, ETH_ALEN))) {
			netdev_info(dev, "Del sta %d (%pM)", cur->sta_idx, cur->mac_addr);
			/* Ensure that we won't process PS change ind */
			spin_lock_bh(&asr_hw->cb_lock);
			cur->ps.active = false;
			cur->valid = false;
			spin_unlock_bh(&asr_hw->cb_lock);
			asr_vif->generation++;

			asr_txq_sta_deinit(asr_hw, cur);

#ifdef CONFIG_ASR_SDIO
			if (asr_xmit_opt)
		        asr_tx_skb_sta_deinit(asr_hw, cur);
#endif

#ifdef CONFIG_ASR_USB
			if (!asr_hw->usb_remove_flag)
#endif
				error = asr_send_me_sta_del(asr_hw, cur->sta_idx);
			if ((error != 0) && (error != -EPIPE))
				return error;

			list_del(&cur->list);
			asr_dbgfs_unregister_rc_stat(asr_hw,cur);
			found++;
			break;
		}
	}

	return 0;
}

static void asr_set_cca_config(struct asr_hw *asr_hw)
{
#ifdef ASR_WIFI_CONFIG_SUPPORT
	struct mm_set_get_cca_req cca_req;
	struct mm_set_get_cca_cfm cca_cfm;

	//set cca
	if (g_wifi_config.cca_config.cca_valid) {
		memset(&cca_req, 0, sizeof(cca_req));
		memset(&cca_cfm, 0, sizeof(cca_cfm));

		cca_req.is_set_cca = true;
		cca_req.cca_threshold = g_wifi_config.cca_config.cca_threshold;
		cca_req.p_cca_rise = g_wifi_config.cca_config.cca_prise_thr;
		cca_req.p_cca_fall = g_wifi_config.cca_config.cca_pfall_thr;
		cca_req.s_cca_rise = g_wifi_config.cca_config.cca_srise_thr;
		cca_req.s_cca_fall = g_wifi_config.cca_config.cca_sfall_thr;

		asr_send_set_cca(asr_hw, &cca_cfm, &cca_req);
	}
#endif
}

/**
 * @change_station: Modify a given station. Note that flags changes are not much
 *    validated in cfg80211, in particular the auth/assoc/authorized flags
 *    might come to the driver in invalid combinations -- make sure to check
 *    them, also against the existing state! Drivers must call
 *    cfg80211_check_station_change() to validate the information.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
static int asr_cfg80211_change_station(struct wiphy *wiphy,
				       struct net_device *dev, const u8 * mac, struct station_parameters *params)
#else
static int asr_cfg80211_change_station(struct wiphy *wiphy,
				       struct net_device *dev, u8 * mac, struct station_parameters *params)
#endif
{
	struct asr_hw *asr_hw = wiphy_priv(wiphy);
	struct asr_vif *asr_vif = netdev_priv(dev);
	struct asr_sta *sta;
	uint32_t timeout_ms = 0;
	dev_err(asr_hw->dev, "%s: dev_flags=%08X mask=%08X\n",__func__,(u32)asr_vif->dev_flags,(u32)params->sta_flags_mask);
	for (timeout_ms = 0; timeout_ms < 100; timeout_ms++) {
		if (asr_vif != NULL
		    && ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_STATION
		    && (test_bit(ASR_DEV_STA_DISCONNECTING, &asr_vif->dev_flags)
			|| !test_bit(ASR_DEV_STA_CONNECTED, &asr_vif->dev_flags))) {

			msleep(1);
		} else {
			break;
		}
	}
	if (timeout_ms >= 100) {
		dev_err(asr_hw->dev, "%s: cannot set in disconnected timeout_ms=%d,dev_flags=%08X\n",
			__func__, timeout_ms, (unsigned int)asr_vif->dev_flags);
		return -EBUSY;
	}

	for (timeout_ms = 0; timeout_ms < 100; timeout_ms++) {
		sta = asr_get_sta(asr_hw, mac);
		if (!sta) {
			msleep(1);
		} else {
			break;
		}
	}
	if (!sta) {
		dev_err(asr_hw->dev, "[%s] ERROR: mac=(%x:%x:%x:%x:%x:%x) timeout_ms=%d\n",
			__func__, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], timeout_ms);
		return -EINVAL;
	}
	dev_info(asr_hw->dev, "%s: %d,0x%X(%02X:%02X:%02X:%02X:%02X:%02X)timeout_ms=%d,sta_idx=%d\n",
		 __func__, (params->sta_flags_set & BIT(NL80211_STA_FLAG_AUTHORIZED)) != 0,
		 params->sta_flags_mask, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], timeout_ms, sta->sta_idx);

	if (params->sta_flags_mask & BIT(NL80211_STA_FLAG_AUTHORIZED))
		asr_send_me_set_control_port_req(asr_hw, (params->sta_flags_set & BIT(NL80211_STA_FLAG_AUTHORIZED))
						 != 0, sta->sta_idx);

	asr_set_cca_config(asr_hw);

	return 0;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 4, 0)
/**
 * @start_ap: Start acting in AP mode defined by the parameters.
 */
static int asr_cfg80211_start_ap(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_ap_settings *settings)
#else
/**
 * @add_beacon: Add a beacon with given parameters, @head, @interval
 *	and @dtim_period will be valid, @tail is optional.
 */
static int asr_cfg80211_add_beacon(struct wiphy *wiphy, struct net_device *dev, struct beacon_parameters *info)
#endif
{
	struct asr_hw *asr_hw = wiphy_priv(wiphy);
	struct asr_vif *asr_vif = netdev_priv(dev);
	struct apm_start_cfm apm_start_cfm;
	struct asr_dma_elem elem;
	struct asr_sta *sta;
	int error = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Forward the information to the LMAC */
	if ((error = asr_send_apm_start_req(asr_hw, asr_vif
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 4, 0)
					    , settings
#else
					    , info
#endif
					    , &apm_start_cfm, &elem)))
		return error;

	// Check the status
	switch (apm_start_cfm.status) {
	case CO_OK:
		{
			u8 txq_status = 0;
			asr_vif->ap.bcmc_index = apm_start_cfm.bcmc_idx;
			asr_vif->ap.flags = 0;
			sta = &asr_hw->sta_table[apm_start_cfm.bcmc_idx];
			sta->valid = true;
			sta->aid = 0;
			sta->sta_idx = apm_start_cfm.bcmc_idx;
			sta->ch_idx = apm_start_cfm.ch_idx;
			sta->vif_idx = asr_vif->vif_index;
			sta->qos = false;
			sta->acm = 0;
			sta->ps.active = false;
			spin_lock_bh(&asr_hw->cb_lock);
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
			asr_chanctx_link(asr_vif, apm_start_cfm.ch_idx, &settings->chandef);
#else
			asr_chanctx_link(asr_vif, apm_start_cfm.ch_idx, &asr_vif->ap_chandef);
#endif
			if (asr_hw->cur_chanctx != apm_start_cfm.ch_idx) {
				txq_status = ASR_TXQ_STOP_CHAN;
			}
			asr_txq_vif_init(asr_hw, asr_vif, txq_status);
			spin_unlock_bh(&asr_hw->cb_lock);

			netif_tx_start_all_queues(dev);
			netif_carrier_on(dev);
			error = 0;
			/* If the AP channel is already the active, we probably skip radar
			   activation on MM_CHANNEL_SWITCH_IND (unless another vif use this
			   ctxt). In anycase retest if radar detection must be activated
			 */
			break;
		}
	case CO_BUSY:
		error = -EINPROGRESS;
		break;
	case CO_OP_IN_PROGRESS:
		error = -EALREADY;
		break;
	default:
		error = -EIO;
		break;
	}

	if (error) {
		dev_err(asr_hw->dev, "Failed to start AP (%d)", error);
	} else {
		dev_info(asr_hw->dev, "AP started: ch=%d, bcmc_idx=%d", asr_vif->ch_index, asr_vif->ap.bcmc_index);
	}

	// Free the buffer used to build the beacon
	kfree(elem.buf);

	return error;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 4, 0)
/**
 * @change_beacon: Change the beacon parameters for an access point mode
 *    interface. This should reject the call when AP mode wasn't started.
 *  firmware no adjust for mm_bcn_change_req, driver temp not adjust
 */
static int asr_cfg80211_change_beacon(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_beacon_data *info)
#else
/**
 * @set_beacon: Change the beacon parameters for an access point mode
 *	interface. This should reject the call when no beacon has been
 *	configured.
 */
static int asr_cfg80211_set_beacon(struct wiphy *wiphy, struct net_device *dev, struct beacon_parameters *info)
#endif
{
	struct asr_hw *asr_hw = wiphy_priv(wiphy);
	struct asr_vif *vif = netdev_priv(dev);
	struct asr_bcn *bcn = &vif->ap.bcn;
	struct asr_dma_elem elem;
	u8 *buf;
	int error = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	// Build the beacon
	buf = asr_build_bcn(bcn, info);
	if (!buf)
		return -ENOMEM;

	// Fill in the DMA structure
	elem.buf = buf;
	elem.len = bcn->len;

	// Forward the information to the LMAC
	error = asr_send_bcn_change(asr_hw, vif->vif_index, elem.buf, bcn->len, bcn->head_len, bcn->tim_len, NULL);
	kfree(elem.buf);

	return error;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 18, 0)
static int asr_cfg80211_stop_ap(struct wiphy *wiphy, struct net_device *dev, unsigned int link_id)
#elif LINUX_VERSION_CODE > KERNEL_VERSION(3, 4, 0)
/**
 * * @stop_ap: Stop being an AP, including stopping beaconing.
 */
static int asr_cfg80211_stop_ap(struct wiphy *wiphy, struct net_device *dev)
#else
/**
 * @del_beacon: Remove beacon configuration and stop sending the beacon.
 */
static int asr_cfg80211_del_beacon(struct wiphy *wiphy, struct net_device *dev)
#endif
{
	struct asr_hw *asr_hw = wiphy_priv(wiphy);
	struct asr_vif *asr_vif = netdev_priv(dev);
	struct asr_sta *cur, *tmp;
	uint32_t retry_time = 0;
	int ret = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
	struct station_del_parameters params;
	params.mac = NULL;
#endif
#ifdef CONFIG_ASR_USB
	if (!asr_hw->usb_remove_flag)
#endif
	asr_send_apm_stop_req(asr_hw, asr_vif);
	spin_lock_bh(&asr_hw->cb_lock);
	asr_chanctx_unlink(asr_vif);
	spin_unlock_bh(&asr_hw->cb_lock);

	/* delete any remaining STA */
	while (!list_empty(&asr_vif->ap.sta_list)) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
		ret = asr_cfg80211_del_station(wiphy, dev, &params);
#else
		ret = asr_cfg80211_del_station(wiphy, dev, NULL);
#endif
		if (ret && retry_time++ > 30) {
			list_for_each_entry_safe(cur, tmp, &asr_vif->ap.sta_list, list) {

				dev_info(asr_hw->dev, "Force unsync Del sta %d (%pM),retry_time=%d",
					 cur->sta_idx, cur->mac_addr, retry_time);
				/* Ensure that we won't process PS change ind */
				spin_lock_bh(&asr_hw->cb_lock);
				cur->ps.active = false;
				cur->valid = false;
				spin_unlock_bh(&asr_hw->cb_lock);

				asr_txq_sta_deinit(asr_hw, cur);
#ifdef CONFIG_ASR_SDIO
			    if (asr_xmit_opt)
		            asr_tx_skb_sta_deinit(asr_hw, cur);
#endif
				list_del(&cur->list);
			}
		}
	}

	asr_txq_vif_deinit(asr_hw, asr_vif);
	asr_del_bcn(&asr_vif->ap.bcn);
	asr_del_csa(asr_vif);

	netif_tx_stop_all_queues(dev);
	netif_carrier_off(dev);

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 10, 0)
	asr_vif->ap_chandef.chan = NULL;
#endif

	netdev_info(dev, "AP Stopped");

	return 0;
}

#ifdef CFG_SNIFFER_SUPPORT
int asr_sniffer_handle_cb_register(monitor_cb_t fn)
{
	sniffer_rx_cb = (monitor_cb_t) fn;

	return 0;
}

void asr_sniffer_handle_mgmt_cb_register(monitor_cb_t fn)
{
	sniffer_rx_mgmt_cb = (monitor_cb_t) fn;
}
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
/**
 * @set_monitor_channel: Set the monitor mode channel for the device. If other
 *    interfaces are active this callback should reject the configuration.
 *    If no interfaces are active or the device is down, the channel should
 *    be stored for when a monitor interface becomes active.
 */
static int asr_cfg80211_set_monitor_channel(struct wiphy *wiphy, struct cfg80211_chan_def *chandef)
{
	int ret = 0;
#ifdef CFG_SNIFFER_SUPPORT
	struct asr_hw *asr_hw = wiphy_priv(wiphy);
	struct asr_vif *asr_vif = asr_hw->vif_table[asr_hw->monitor_vif_idx];
	struct ieee80211_channel *pst_chan = &asr_vif->sniffer.st_chan;

	enum nl80211_channel_type chan_type_set = cfg80211_get_chandef_type(chandef);
	//u8 chan_num = chandef->chan->hw_value;
	u8 chan_num = ieee80211_frequency_to_channel(chandef->chan->center_freq);

	printk("freq %d, type %d, chan_num:%d,(%d %d %d %d)\n",
	       chandef->chan->center_freq, chan_type_set, chan_num,
	       chandef->width, chandef->center_freq1, chandef->center_freq2, chandef->chan->max_power);

	if ((asr_vif->sniffer.chan_num == chan_num)
	    && (asr_vif->sniffer.chan_type == chan_type_set)) {
		printk("sniffer set same chan\n");
		return 0;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 1)
	if (chandef->chan->band != NL80211_BAND_2GHZ)
#else
	if (chandef->chan->band != IEEE80211_BAND_2GHZ)
#endif
		return -1;

	if (asr_send_set_monitor_channel(asr_hw, chandef) < 0) {
		printk("[%s]ret %d", __func__, ret);
		return ret;
	}

	/* save the channel info to VIF */
	asr_vif->sniffer.chan_num = chan_num;
	asr_vif->sniffer.chan_type = chan_type_set;

	memset(pst_chan, 0, sizeof(struct ieee80211_channel));
	memcpy(pst_chan, chandef->chan, sizeof(struct ieee80211_channel));
#endif
	return ret;
}
#endif

/**
 * @asr_cfg80211_set_bitrate: Set fixed rate for specific station, if
 * addr is not set, set all station with same rateconfig.
 */
#define RC_FIXED_RATE_NOT_SET    (0xFFFF)	/// Disabled fixed rate configuration
static int asr_cfg80211_set_bitrate(struct wiphy *wiphy,struct net_device *dev,
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
	unsigned int link_id,
	#endif
	const u8 * addr, const struct cfg80211_bitrate_mask *mask)
{
	struct asr_hw *asr_hw = wiphy_priv(wiphy);
	struct asr_vif *asr_vif = netdev_priv(dev);
	u16 rate_cfg = RC_FIXED_RATE_NOT_SET;
	u8 mcsrate_2G = 0xff;
	u32 legacy = 0xfff;
	u32 ratemask = 0xfffff;
	u8 mcs, ht_format, short_gi, nss, mcs_index, bg_rate,bw;
	struct asr_sta *sta = NULL;
	int ret = -1;
        u8 gi_2G = 0;

        u16 he_mcsrate_2G = 0xffff;
	u8 he_gi_2G = 0;
	u8 he_ltf_2G = 0;
	u8 he_format = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	mcs = ht_format = short_gi = nss = bw = bg_rate = mcs_index = 0;
	/*

	   set fix rate cmd: iw dev wlan0 set bitrates [legacy-2.4 <legacy rate in Mbps>] [ht-mcs-2.4 <MCS index>]
	 */

	/* only check 2.4 bands, skip the rest */
	/* copy legacy rate mask */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 1)
	legacy = mask->control[NL80211_BAND_2GHZ].legacy;
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
	legacy = mask->control[IEEE80211_BAND_2GHZ].legacy;
#else
	legacy = mask->control[IEEE80211_BAND_2GHZ].legacy;
#endif
	ratemask = legacy;

	/* copy mcs rate mask , mcs[nss-1],for nss=1, use mcs[0]*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 1)
	mcsrate_2G = mask->control[NL80211_BAND_2GHZ].ht_mcs[0];
#elif(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
	mcsrate_2G = mask->control[IEEE80211_BAND_2GHZ].ht_mcs[0];
#elif (LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0))
	mcsrate_2G = mask->control[IEEE80211_BAND_2GHZ].mcs[0];
#endif
	ratemask |= mcsrate_2G << 12;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 1)
        gi_2G = mask->control[NL80211_BAND_2GHZ].gi;
        he_mcsrate_2G = mask->control[NL80211_BAND_2GHZ].he_mcs[0];
        he_gi_2G  = mask->control[NL80211_BAND_2GHZ].he_gi;
        he_ltf_2G = mask->control[NL80211_BAND_2GHZ].he_ltf;
#endif

	//set fix rate configuration (modulation,pre_type, short_gi, bw, nss, mcs)
	if (asr_hw->mod_params->sgi && gi_2G) {
		short_gi = 1;
	} else
		short_gi = 0;

	if (asr_hw->mod_params->use_2040 && !force_bw20) {
		bw = 1;
	} else
		bw = 0;

	/*
	   rate control
	                                   13~11                 10                9         8~7         6~0
	   self_cts | no_protection | modulation_mask(3) | premble_type(1) | short_GT(1) | bw(2) |    mcs index(7)
	   nss(4~3)   mcs(2~0)
	 */

	if (ratemask == 0xfffff && (he_mcsrate_2G == 0xffff))
		rate_cfg = RC_FIXED_RATE_NOT_SET;
	else {

		if ((mcsrate_2G == 0xff)
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
		    && (legacy != 0xfff)
#endif
		    ) {	
			if (he_mcsrate_2G == 0xffff)
			{
				// bg fixed rate set.
				bg_rate = ratemask & 0xfff;
				ht_format = 0;
				rate_cfg = ((ht_format & 0x7) << 11) | ((short_gi & 0x1) << 9) | (bg_rate & 0x7f);
			} else {

				he_format = 5;   //HE fixed rate config.
				mcs = 0;
				while (mcs_index < 10) {
					if (he_mcsrate_2G & BIT(mcs_index))
						mcs = mcs_index;

					mcs_index++;
				}

				if (asr_hw->mod_params->nss)
					nss = asr_hw->mod_params->nss - 1;

				rate_cfg =
					((he_format & 0x7) << 11) | ((he_gi_2G & 0x3) << 9) | ((bw & 0x3) << 7) | ((nss & 0x7) << 4) | (mcs & 0xf);
			}

		} else if (mcsrate_2G != 0xff) {	// ofdm rate
			ht_format = 2;   //HT-MF
			mcs = 0;
			while (mcs_index < 8) {
				if (mcsrate_2G & BIT(mcs_index))
					mcs = mcs_index;

				mcs_index++;
			}

			if (asr_hw->mod_params->nss)
				nss = asr_hw->mod_params->nss - 1;

			rate_cfg =
			    ((ht_format & 0x7) << 11) | (mcs & 0x7f) | ((bw & 0x3) << 7) | ((short_gi & 0x1) << 9);
		}
	}

	printk
	    ("[%s]he_mcsrate_2G (0x%x) legacy(0x%x) mcsrate_2G(0x%x) bg_rate(0x%x) (%x:%x:%x:%x) ratemask(0x%x) rate_cfg(0x%x) ,he_ltf_2G=(0x%x) \n",
	     __func__, he_mcsrate_2G,legacy, mcsrate_2G, bg_rate, ht_format, mcs, nss, short_gi, ratemask, rate_cfg,he_ltf_2G);

	if (asr_vif->wdev.iftype == NL80211_IFTYPE_AP) {
		struct asr_sta *asr_sta = NULL;
		list_for_each_entry(asr_sta, &asr_vif->ap.sta_list, list) {
			if (asr_sta && asr_sta->valid && ((addr == NULL)
					       || ether_addr_equal(asr_sta->mac_addr, addr))) {
				sta = asr_sta;
				ret = asr_send_me_rc_set_rate(asr_hw, sta->sta_idx, rate_cfg);
				printk("set station idx:[%d] %d\n", sta->sta_idx, ret);
			}
		}
	} else {
		sta = asr_vif->sta.ap;
		if (sta && sta->valid)
			ret = asr_send_me_rc_set_rate(asr_hw, sta->sta_idx, rate_cfg);
            else {
        		        printk("sta is NULL ,skip \r\n");
                        ret = 0;
            }
	}
	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0)
/**
 * @probe_client: probe an associated client, must return a cookie that it
 *    later passes to cfg80211_probe_status().
 */
int asr_cfg80211_probe_client(struct wiphy *wiphy, struct net_device *dev, const u8 * peer, u64 * cookie)
{
	return 0;
}
#endif

/**
 * @mgmt_frame_register: Notify driver that a management frame type was
 *    registered. Note that this callback may not sleep, and cannot run
 *    concurrently with itself.
 */
void asr_cfg80211_mgmt_frame_register(struct wiphy *wiphy
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
				      , struct wireless_dev *wdev
#else
				      , struct net_device *dev
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
				      , struct mgmt_frame_regs *upd)
#else
				      , u16 frame_type, bool reg)
#endif
{
#if 0
	struct asr_hw *asr_hw = wiphy_priv(wiphy);
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 0, 8)
	struct asr_vif *asr_vif = netdev_priv(wdev->netdev);
#else
	struct asr_vif *asr_vif = netdev_priv(dev);
#endif

	dev_info(asr_hw->dev, "%s:iftype=%d,frame_type=0x%X,reg=%d\n", __func__, ASR_VIF_TYPE(asr_vif), frame_type,
		 reg);
#endif
}

/**
 * @set_wiphy_params: Notify that wiphy parameters have changed;
 *    @changed bitfield (see &enum wiphy_params_flags) describes which values
 *    have changed. The actual parameter values are available in
 *    struct wiphy. If returning an error, no value should be changed.
 */
static int asr_cfg80211_set_wiphy_params(struct wiphy *wiphy, u32 changed)
{
	return 0;
}

/**
 * @set_tx_power: set the transmit power according to the parameters,
 *    the power passed is in mBm, to get dBm use MBM_TO_DBM(). The
 *    wdev may be %NULL if power was set for the wiphy, and will
 *    always be %NULL unless the driver supports per-vif TX power
 *    (as advertised by the nl80211 feature flag.)
 */
static int asr_cfg80211_set_tx_power(struct wiphy *wiphy,
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
				     struct wireless_dev *wdev,
#endif
				     enum nl80211_tx_power_setting type, int mbm)
{
	struct asr_hw *asr_hw = wiphy_priv(wiphy);
	struct asr_vif *vif = NULL;
	s8 pwr;
	int res = 0;

	if (type == NL80211_TX_POWER_AUTOMATIC) {
		pwr = 0x7f;
	} else {
		pwr = MBM_TO_DBM(mbm);
	}

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
	if (wdev) {
		vif = container_of(wdev, struct asr_vif, wdev);
		res = asr_send_set_power(asr_hw, vif->vif_index, pwr, NULL);
	} else {
#endif
		list_for_each_entry(vif, &asr_hw->vifs, list) {
			res = asr_send_set_power(asr_hw, vif->vif_index, pwr, NULL);
			if (res)
				break;
		}
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
	}
#endif

	return res;
}

static int asr_cfg80211_set_txq_params(struct wiphy *wiphy,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0)
				       struct net_device *dev,
#endif
				       struct ieee80211_txq_params *params)
{
	struct asr_hw *asr_hw = wiphy_priv(wiphy);
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
	struct asr_vif *asr_vif = netdev_priv(dev);
#endif
	u8 hw_queue, aifs, cwmin, cwmax;
	u32 param;

	ASR_DBG(ASR_FN_ENTRY_STR);

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 10, 0)
	hw_queue = asr_ac2hwq[0][params->queue];
#else
	hw_queue = asr_ac2hwq[0][params->ac];
#endif

	aifs = params->aifs;
	cwmin = fls(params->cwmin);
	cwmax = fls(params->cwmax);

	/* Store queue information in general structure */
	param = (u32) (aifs << 0);
	param |= (u32) (cwmin << 4);
	param |= (u32) (cwmax << 8);
	param |= (u32) (params->txop) << 12;

	/* Send the MM_SET_EDCA_REQ message to the FW */
	return asr_send_set_edca(asr_hw, hw_queue, param, false
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
				 , asr_vif->vif_index);
#else
				 , 0);
#endif
}

/**
 * @remain_on_channel: Request the driver to remain awake on the specified
 *    channel for the specified duration to complete an off-channel
 *    operation (e.g., public action frame exchange). When the driver is
 *    ready on the requested channel, it must indicate this with an event
 *    notification by calling cfg80211_ready_on_channel().
 */
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
static int asr_cfg80211_remain_on_channel(struct wiphy *wiphy, struct wireless_dev *wdev,
					  struct ieee80211_channel *chan, unsigned int duration, u64 * cookie)
{
	struct asr_vif *asr_vif = netdev_priv(wdev->netdev);
#else
static int asr_cfg80211_remain_on_channel(struct wiphy *wiphy, struct net_device *dev,
					  struct ieee80211_channel *chan, enum nl80211_channel_type channel_type,
					  unsigned int duration, u64 * cookie)
{
	struct asr_vif *asr_vif = netdev_priv(dev);
#endif

	struct asr_hw *asr_hw = wiphy_priv(wiphy);
	struct asr_roc_elem *roc_elem;
	int error;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* For debug purpose (use ftrace kernel option) */
	//trace_roc(asr_vif->vif_index, chan->center_freq, duration);

	/* Check that no other RoC procedure has been launched */
	if (asr_hw->roc_elem) {
		return -EBUSY;
	}

	/* Allocate a temporary RoC element */
	roc_elem = kmalloc(sizeof(struct asr_roc_elem), GFP_KERNEL);

	/* Verify that element has well been allocated */
	if (!roc_elem) {
		return -ENOMEM;
	}
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
	/* Initialize the RoC information element */
	roc_elem->wdev = wdev;
#else
	/* Initialize the RoC information element */
	roc_elem->wdev = dev->ieee80211_ptr;
#endif
	roc_elem->chan = chan;
	roc_elem->duration = duration;
	roc_elem->mgmt_roc = false;
	roc_elem->on_chan = false;

	/* Forward the information to the FMAC */
	asr_hw->roc_elem = roc_elem;
	error = asr_send_roc(asr_hw, asr_vif, chan, duration);

	/* If no error, keep all the information for handling of end of procedure */
	if (error == 0) {

		/* Set the cookie value */
		*cookie = (u64) (asr_hw->roc_cookie_cnt);

		/* Initialize the OFFCHAN TX queue to allow off-channel transmissions */
		asr_txq_offchan_init(asr_vif);
	} else {
		/* Free the allocated element */
		asr_hw->roc_elem = NULL;
		kfree(roc_elem);
	}

	return error;
}

/**
 * @cancel_remain_on_channel: Cancel an on-going remain-on-channel operation.
 *    This allows the operation to be terminated prior to timeout based on
 *    the duration value.
 */
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
static int asr_cfg80211_cancel_remain_on_channel(struct wiphy *wiphy, struct wireless_dev *wdev, u64 cookie)
#else
static int asr_cfg80211_cancel_remain_on_channel(struct wiphy *wiphy, struct net_device *dev, u64 cookie)
#endif
{
	struct asr_hw *asr_hw = wiphy_priv(wiphy);

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Check if a RoC procedure is pending */
	if (!asr_hw->roc_elem) {
		return 0;
	}

	/* Forward the information to the FMAC */
	return asr_send_cancel_roc(asr_hw);
}

/**
 * @dump_survey: get site survey information.
 */
static int asr_cfg80211_dump_survey(struct wiphy *wiphy, struct net_device *netdev, int idx, struct survey_info *info)
{
	struct asr_hw *asr_hw = wiphy_priv(wiphy);
	struct ieee80211_supported_band *sband;
	struct asr_survey_info *asr_survey;

	ASR_DBG(ASR_FN_ENTRY_STR);

	if (idx >= ARRAY_SIZE(asr_hw->survey))
		return -ENOENT;

	asr_survey = &asr_hw->survey[idx];

	// Check if provided index matches with a supported 2.4GHz channel
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 1)
	sband = wiphy->bands[NL80211_BAND_2GHZ];
#else
	sband = wiphy->bands[IEEE80211_BAND_2GHZ];
#endif

	if (!sband) {
		return -ENOENT;
	}

	if (idx >= sband->n_channels) {
		idx -= sband->n_channels;
		return -ENOENT;
	}
	// Fill the survey
	info->channel = &sband->channels[idx];
	info->filled = asr_survey->filled;

	if (asr_survey->filled != 0) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
		info->time = (u64) asr_survey->chan_time_ms;
		info->time_busy = (u64) asr_survey->chan_time_busy_ms;
#else
		info->channel_time = (u64) asr_survey->chan_time_ms;
		info->channel_time_busy = (u64) asr_survey->chan_time_busy_ms;
#endif
		info->noise = asr_survey->noise_dbm;

		// Set the survey report as not used
		asr_survey->filled = 0;
	}

	return 0;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
/**
 * @get_channel: Get the current operating channel for the virtual interface.
 *    For monitor interfaces, it should return %NULL unless there's a single
 *    current monitoring channel.
 */
static int asr_cfg80211_get_channel(struct wiphy *wiphy, struct wireless_dev *wdev,
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
	unsigned int link_id,
	#endif
	struct cfg80211_chan_def *chandef)
{
	struct asr_hw *asr_hw = wiphy_priv(wiphy);
	struct asr_vif *asr_vif = container_of(wdev, struct asr_vif, wdev);
	struct asr_chanctx *ctxt;

	if (!asr_vif->up || !asr_chanctx_valid(asr_hw, asr_vif->ch_index)) {
		return -ENODATA;
	}

	ctxt = &asr_hw->chanctx_table[asr_vif->ch_index];
	*chandef = ctxt->chan_def;

	return 0;
}
#endif

/**
 * @mgmt_tx: Transmit a management frame.
 */
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
static int asr_cfg80211_mgmt_tx(struct wiphy *wiphy, struct wireless_dev *wdev,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
				struct cfg80211_mgmt_tx_params *params,
#else
				struct ieee80211_channel *chan, bool offchan,
				unsigned int wait, const u8 * buf, size_t len, bool no_cck, bool dont_wait_for_ack,
#endif
				u64 * cookie)
#else
static int asr_cfg80211_mgmt_tx(struct wiphy *wiphy, struct net_device *dev,
				struct ieee80211_channel *chan, bool offchan, enum nl80211_channel_type channel_type,
				bool channel_type_valid, unsigned int wait, const u8 * buf, size_t len,
				bool no_cck,
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 2, 0)
				bool dont_wait_for_ack,
#endif
				u64 * cookie)
#endif
{
	struct asr_hw *asr_hw = wiphy_priv(wiphy);
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
	struct asr_vif *asr_vif = netdev_priv(wdev->netdev);
#else
	struct asr_vif *asr_vif = netdev_priv(dev);
#endif
	struct asr_sta *asr_sta;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	struct ieee80211_mgmt *mgmt = (void *)params->buf;
#else
	struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *)buf;
#endif
	int error = 0;
	bool ap = false;
	bool in_offchan = false;

	if (!asr_hw) {
		return -EIO;
	}
	//dev_info(asr_hw->dev, "%s:iftype=%d,frame_control=0x%X,freq=%d,len=%d\n"
	//              , __func__, ASR_VIF_TYPE(asr_vif), mgmt->frame_control, chan->center_freq, len);

	do {
		/* Check if provided VIF is an AP or a STA one */
		switch (ASR_VIF_TYPE(asr_vif)) {
		case NL80211_IFTYPE_AP:
		case NL80211_IFTYPE_P2P_GO:
			ap = true;
			break;
#if (defined CFG_SNIFFER_SUPPORT || defined CFG_CUS_FRAME)
		case NL80211_IFTYPE_MONITOR:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
			return asr_start_mgmt_xmit(asr_vif, NULL, params, in_offchan, cookie);
#elif LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
			return asr_start_mgmt_xmit(asr_vif, NULL, buf, len, no_cck, in_offchan, cookie);
#else
			return asr_start_mgmt_xmit(asr_vif, NULL, buf, len, false, in_offchan, cookie);
#endif
#endif
		case NL80211_IFTYPE_STATION:
		case NL80211_IFTYPE_P2P_CLIENT:
			break;
		default:
			break;
		}

		/* Get STA on which management frame has to be sent */
		asr_sta = asr_retrieve_sta(asr_hw, asr_vif, mgmt->da, mgmt->frame_control, ap);
		/* If AP, STA have to exist */
		if (!asr_sta) {
			if (!ap) {
				/* ROC is needed, check that channel parameters have been provided */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
				if (!params->chan)
#else
				if (!chan)
#endif
				{
					error = -EINVAL;
					break;
				}

				/* Check that a RoC is already pending */
				if (asr_hw->roc_elem) {
					/* Get VIF used for current ROC */
					struct asr_vif *asr_roc_vif = netdev_priv(asr_hw->roc_elem->wdev->netdev);

					/* Check if RoC channel is the same than the required one */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
					if ((asr_hw->roc_elem->chan->center_freq != params->chan->center_freq)
#else
					if ((asr_hw->roc_elem->chan->center_freq != chan->center_freq)
#endif
					    || (asr_vif->vif_index != asr_roc_vif->vif_index)) {
						error = -EINVAL;
						break;
					}
				} else {
					u64 cookie_tmp;

					/* Start a ROC procedure for 30ms */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
					error =
					    asr_cfg80211_remain_on_channel(wiphy, wdev, params->chan, 30, &cookie_tmp);
#elif LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
					error = asr_cfg80211_remain_on_channel(wiphy, wdev, chan, 30, &cookie_tmp);
#else
					error =
					    asr_cfg80211_remain_on_channel(wiphy, dev, chan, channel_type, 30,
									   &cookie_tmp);
#endif
					if (error) {
						break;
					}

					/*
					 * Need to keep in mind that RoC has been launched internally in order to
					 * avoid to call the cfg80211 callback once expired
					 */
					//asr_hw->roc_elem->mgmt_roc = true;
				}

				in_offchan = true;
			}
		}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
		/* Push the management frame on the TX path */
		error = asr_start_mgmt_xmit(asr_vif, asr_sta, params, in_offchan, cookie);
#elif LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
		error = asr_start_mgmt_xmit(asr_vif, asr_sta, buf, len, no_cck, in_offchan, cookie);
#else
		error = asr_start_mgmt_xmit(asr_vif, asr_sta, buf, len, false, in_offchan, cookie);
#endif
	} while (0);

	return error;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
/**
 * @start_radar_detection: Start radar detection in the driver.
 */
static
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 15, 0)
int asr_cfg80211_start_radar_detection(struct wiphy *wiphy,
				       struct net_device *dev, struct cfg80211_chan_def *chandef, u32 cac_time_ms)
#else
int asr_cfg80211_start_radar_detection(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_chan_def *chandef)
#endif
{
	ASR_DBG(ASR_FN_ENTRY_STR);

#if 0				//for now not support radar detection
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 15, 0)
	u32 cac_time_ms = 60000;
#endif

	struct asr_hw *asr_hw = wiphy_priv(wiphy);
	struct asr_vif *asr_vif = netdev_priv(dev);
	struct apm_start_cac_cfm cfm;

	asr_radar_start_cac(&asr_hw->radar, cac_time_ms, asr_vif);
	asr_send_apm_start_cac_req(asr_hw, asr_vif, chandef, &cfm);

	if (cfm.status == CO_OK) {
		spin_lock_bh(&asr_hw->cb_lock);
		asr_chanctx_link(asr_vif, cfm.ch_idx, chandef);
		if (asr_hw->cur_chanctx == asr_vif->ch_index)
			asr_radar_detection_enable(&asr_hw->radar, ASR_RADAR_DETECT_REPORT, ASR_RADAR_RIU);
		spin_unlock_bh(&asr_hw->cb_lock);
	} else {
		return -EIO;
	}
#endif

	return 0;
}

/**
 * @update_ft_ies: Provide updated Fast BSS Transition information to the
 *    driver. If the SME is in the driver/firmware, this information can be
 *    used in building Authentication and Reassociation Request frames.
 */
static
int asr_cfg80211_update_ft_ies(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_update_ft_ies_params *ftie)
{
	return 0;
}
#endif

/**
 * @set_cqm_rssi_config: Configure connection quality monitor RSSI threshold.
 */
static
int asr_cfg80211_set_cqm_rssi_config(struct wiphy *wiphy, struct net_device *dev, int rssi_thold, u32 rssi_hyst)
{
	struct asr_hw *asr_hw = wiphy_priv(wiphy);
	struct asr_vif *asr_vif = netdev_priv(dev);

	return asr_send_cfg_rssi_req(asr_hw, asr_vif->vif_index, rssi_thold, rssi_hyst);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 0)
/**
 *
 * @channel_switch: initiate channel-switch procedure (with CSA). Driver is
 *    responsible for veryfing if the switch is possible. Since this is
 *    inherently tricky driver may decide to disconnect an interface later
 *    with cfg80211_stop_iface(). This doesn't mean driver can accept
 *    everything. It should do it's best to verify requests and reject them
 *    as soon as possible.
 */
int asr_cfg80211_channel_switch(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_csa_settings *params)
{
	struct asr_hw *asr_hw = wiphy_priv(wiphy);
	struct asr_vif *vif = netdev_priv(dev);
	struct asr_dma_elem elem;
	struct asr_bcn *bcn, *bcn_after;
	struct asr_csa *csa;
	u16 csa_oft[BCN_MAX_CSA_CPT];
	u8 *buf;
	int i, error = 0;

	if (vif->ap.csa)
		return -EBUSY;

	if (params->n_counter_offsets_beacon > BCN_MAX_CSA_CPT)
		return -EINVAL;

	/* Build the new beacon with CSA IE */
	bcn = &vif->ap.bcn;
	buf = asr_build_bcn(bcn, &params->beacon_csa);
	if (!buf)
		return -ENOMEM;

	memset(csa_oft, 0, sizeof(csa_oft));
	for (i = 0; i < params->n_counter_offsets_beacon; i++) {
		csa_oft[i] = params->counter_offsets_beacon[i] + bcn->head_len + bcn->tim_len;
	}

	/* If count is set to 0 (i.e anytime after this beacon) force it to 2 */
	if (params->count == 0) {
		params->count = 2;
		for (i = 0; i < params->n_counter_offsets_beacon; i++) {
			buf[csa_oft[i]] = 2;
		}
	}

	elem.buf = buf;
	elem.len = bcn->len;

	/* Build the beacon to use after CSA. It will only be sent to fw once
	   CSA is over. */
	csa = kzalloc(sizeof(struct asr_csa), GFP_KERNEL);
	if (!csa) {
		error = -ENOMEM;
		goto end;
	}

	bcn_after = &csa->bcn;
	buf = asr_build_bcn(bcn_after, &params->beacon_after);
	if (!buf) {
		error = -ENOMEM;
		asr_del_csa(vif);
		goto end;
	}

	vif->ap.csa = csa;
	csa->vif = vif;
	csa->chandef = params->chandef;
	csa->dma.buf = buf;
	csa->dma.len = bcn_after->len;

	/* Send new Beacon. FW will extract channel and count from the beacon */
	error = asr_send_bcn_change(asr_hw, vif->vif_index, elem.buf, bcn->len, bcn->head_len, bcn->tim_len, csa_oft);

	if (error) {
		asr_del_csa(vif);
		goto end;
	} else {
		INIT_WORK(&csa->work, asr_csa_finish);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
		cfg80211_ch_switch_started_notify(dev, &csa->chandef, 0, params->count, false);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
		cfg80211_ch_switch_started_notify(dev, &csa->chandef, params->count, false);
#else
		cfg80211_ch_switch_started_notify(dev, &csa->chandef, params->count);
#endif
	}

end:
	kfree(elem.buf);

	return error;
}
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 10, 0)
/**
 *
 * @set_channel: Set channel for a given wireless interface. Some devices
 *	may support multi-channel operation (by channel hopping) so cfg80211
 *	doesn't verify much. Note, however, that the passed netdev may be
 *	%NULL as well if the user requested changing the channel for the
 *	device itself, or for a monitor interface.
 */
int asr_cfg80211_set_channel(struct wiphy *wiphy, struct net_device *dev,
			     struct ieee80211_channel *chan, enum nl80211_channel_type channel_type)
{
	//struct asr_hw *asr_hw = wiphy_priv(wiphy);
	struct asr_vif *asr_vif = netdev_priv(dev);

	if (asr_vif->ap_chandef.chan) {
		return -EBUSY;
	}

	asr_vif->ap_chan = *chan;
	asr_vif->ap_chandef.width = channel_type <= NL80211_CHAN_HT20 ? NL80211_CHAN_WIDTH_20 : NL80211_CHAN_WIDTH_40;
	asr_vif->ap_chandef.chan = &asr_vif->ap_chan;

	return 0;
}
#endif

/**
 * @change_bss: Modify parameters for a given BSS (mainly for AP mode).
 */
int asr_cfg80211_change_bss(struct wiphy *wiphy, struct net_device *dev, struct bss_parameters *params)
{
	struct asr_hw *asr_hw = wiphy_priv(wiphy);
	struct asr_vif *asr_vif = netdev_priv(dev);
	int res = -EOPNOTSUPP;

	dev_info(asr_hw->dev, "%s:iftype=%d,ap_isolate=%d\n", __func__, ASR_VIF_TYPE(asr_vif), params->ap_isolate);

	if (((ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_AP) ||
             (ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_P2P_GO))
                  && (params->ap_isolate > -1)) {

		if (params->ap_isolate)
			asr_vif->ap.flags |= ASR_AP_ISOLATE;
		else
			asr_vif->ap.flags &= ~ASR_AP_ISOLATE;

		res = 0;
	}

	return res;
}

#ifndef CONFIG_ASR595X
static int asr_fill_rate(int format, int mcs, int bw, int sgi, struct rate_info *rate)
{
    int bitrates[12] = { 10, 20, 55, 110, 60, 90, 120, 180, 240, 360, 480, 540};

    if (format <= FORMATMOD_NON_HT_DUP_OFDM)
    {
        rate->legacy = bitrates[mcs];
    }
    else if (format <= FORMATMOD_HT_GF)
    {
        rate->mcs = mcs;
        rate->flags |= RATE_INFO_FLAGS_MCS;
        rate->bw = RATE_INFO_BW_20;
        if (bw)
            rate->bw = RATE_INFO_BW_40;
        if (sgi)
            rate->flags |= RATE_INFO_FLAGS_SHORT_GI;
    }
    else
    {
        return -1;
    }

    return 0;
}
#endif

/**
 * @get_station: get station information for the station identified by @mac
 */
static int asr_fill_station_info(struct asr_sta *sta, struct asr_vif *vif,
                                  struct station_info *sinfo)
{
    struct asr_sta_stats *stats = &sta->stats;
    struct rx_vector_1 *rx_vect1 = &stats->last_rx.rx_vect1;
	#ifndef CONFIG_ASR595X
    int format, mcs, bw, sgi;
    struct me_rc_stats_cfm me_rc_stats_cfm;
    union asr_rate_ctrl_info r_cfg;
	#endif

    // Generic info
    sinfo->generation = vif->generation;

    //sinfo->inactive_time = jiffies_to_msecs(jiffies - stats->last_act);
    sinfo->rx_bytes = stats->rx_bytes;
    sinfo->tx_bytes = stats->tx_bytes;
    sinfo->tx_packets = stats->tx_pkts;
    sinfo->rx_packets = stats->rx_pkts;
    sinfo->signal = rx_vect1->rssi1;
    //sinfo->tx_failed = 1;
	
	#ifndef CONFIG_ASR595X
    format = rx_vect1->format_mod;
    mcs = rx_vect1->mcs;
    bw = rx_vect1->ch_bw;
    sgi = rx_vect1->short_gi;

    //asr_err("%s:rx:format %d, mcs %d, bw %d, sgi %d\n", __func__, format, mcs, bw, sgi);
    if (0 == asr_fill_rate(format, mcs, bw, sgi, &sinfo->rxrate))
        sinfo->filled |= BIT(NL80211_STA_INFO_RX_BITRATE);

    if ( asr_send_me_rc_stats(vif->asr_hw, sta->sta_idx, &me_rc_stats_cfm))
        return -EINVAL;
    r_cfg.value = me_rc_stats_cfm.rate_stats[me_rc_stats_cfm.retry_step_idx[0]].rate_config;
    format = r_cfg.formatModTx;
    mcs = r_cfg.mcsIndexTx;
    bw = r_cfg.bwTx;
    sgi = r_cfg.shortGITx;
    //asr_err("%s:tx:format %d, mcs %d, bw %d, sgi %d\n", __func__, format, mcs, bw, sgi);
    if (0 == asr_fill_rate(format, mcs, bw, sgi, &sinfo->txrate))
        sinfo->filled |= BIT(NL80211_STA_INFO_TX_BITRATE);
    #endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
    sinfo->filled |= (BIT(NL80211_STA_INFO_RX_BYTES64)    |
                     BIT(NL80211_STA_INFO_TX_BYTES64)    |
#else
    sinfo->filled |= (BIT(NL80211_STA_INFO_RX_BYTES)    |
                     BIT(NL80211_STA_INFO_TX_BYTES)    |
#endif
                     BIT(NL80211_STA_INFO_RX_PACKETS)    |
                     BIT(NL80211_STA_INFO_TX_PACKETS)    |
                     BIT(NL80211_STA_INFO_SIGNAL));

    return 0;
}

static int asr_cfg80211_get_station(struct wiphy *wiphy, struct net_device *dev,
	#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 33)
		const u8 *mac,
        #else
		u8 *mac,
        #endif
		struct station_info *sinfo)
{
    struct asr_vif *vif = netdev_priv(dev);
    struct asr_sta *sta = NULL;

    if (ASR_VIF_TYPE(vif) == NL80211_IFTYPE_MONITOR)
        return -EINVAL;
    else if ((ASR_VIF_TYPE(vif) == NL80211_IFTYPE_STATION) ||
             (ASR_VIF_TYPE(vif) == NL80211_IFTYPE_P2P_CLIENT)) {
        if (vif->sta.ap && ether_addr_equal(vif->sta.ap->mac_addr, mac))
            sta = vif->sta.ap;
    }
    else
    {
        struct asr_sta *sta_iter;
        list_for_each_entry(sta_iter, &vif->ap.sta_list, list) {
            if (sta_iter->valid && ether_addr_equal(sta_iter->mac_addr, mac)) {
                sta = sta_iter;
                break;
            }
        }
    }

    if (sta)
        return asr_fill_station_info(sta, vif, sinfo);

    return -EINVAL;
}
static struct cfg80211_ops asr_cfg80211_ops = {
	.add_virtual_intf = asr_cfg80211_add_iface,
	.del_virtual_intf = asr_cfg80211_del_iface,
	.change_virtual_intf = asr_cfg80211_change_iface,
	.scan = asr_cfg80211_scan,
#ifdef CONFIG_SME
	.auth = asr_cfg80211_auth,
	.assoc = asr_cfg80211_assoc,
	.deauth = asr_cfg80211_deauth,
	.disassoc = asr_cfg80211_disassoc,
#else
	.connect = asr_cfg80211_connect,
#endif
	.disconnect = asr_cfg80211_disconnect,
	.add_key = asr_cfg80211_add_key,
	.get_key = asr_cfg80211_get_key,
	.del_key = asr_cfg80211_del_key,
	.set_default_key = asr_cfg80211_set_default_key,
	.set_default_mgmt_key = asr_cfg80211_set_default_mgmt_key,
	.set_power_mgmt = asr_cfg80211_set_power_mgmt,
	.add_station = asr_cfg80211_add_station,
	.del_station = asr_cfg80211_del_station,
	.change_station = asr_cfg80211_change_station,
	.mgmt_tx = asr_cfg80211_mgmt_tx,
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
	.set_monitor_channel = asr_cfg80211_set_monitor_channel,
#endif
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 4, 0)
	.start_ap = asr_cfg80211_start_ap,
	.change_beacon = asr_cfg80211_change_beacon,
	.stop_ap = asr_cfg80211_stop_ap,
#else
	.add_beacon = asr_cfg80211_add_beacon,
	.set_beacon = asr_cfg80211_set_beacon,
	.del_beacon = asr_cfg80211_del_beacon,
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0)
	.probe_client = asr_cfg80211_probe_client,
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
	.update_mgmt_frame_registrations = asr_cfg80211_mgmt_frame_register,
#else
	.mgmt_frame_register = asr_cfg80211_mgmt_frame_register,
#endif
	.set_wiphy_params = asr_cfg80211_set_wiphy_params,
	.set_txq_params = asr_cfg80211_set_txq_params,
	.set_tx_power = asr_cfg80211_set_tx_power,
	.get_station = asr_cfg80211_get_station,
	.remain_on_channel = asr_cfg80211_remain_on_channel,
	.cancel_remain_on_channel = asr_cfg80211_cancel_remain_on_channel,
	.set_bitrate_mask = asr_cfg80211_set_bitrate,
	.dump_survey = asr_cfg80211_dump_survey,
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
	.get_channel = asr_cfg80211_get_channel,
	.start_radar_detection = asr_cfg80211_start_radar_detection,
	.update_ft_ies = asr_cfg80211_update_ft_ies,
#endif
	.set_cqm_rssi_config = asr_cfg80211_set_cqm_rssi_config,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 0)
	.channel_switch = asr_cfg80211_channel_switch,
#endif
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 10, 0)
	.set_channel = asr_cfg80211_set_channel,
#endif
	.change_bss = asr_cfg80211_change_bss,
};

/*********************************************************************
 * Init/Exit functions
 *********************************************************************/
static void asr_wdev_unregister(struct asr_hw *asr_hw)
{
	struct asr_vif *asr_vif, *tmp;

	rtnl_lock();
	list_for_each_entry_safe(asr_vif, tmp, &asr_hw->vifs, list) {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0))
		asr_cfg80211_del_iface(asr_hw->wiphy, &asr_vif->wdev);
#else
		asr_cfg80211_del_iface(asr_hw->wiphy, asr_vif->wdev.netdev);
#endif
	}
	rtnl_unlock();
}

static void asr_set_vers(struct asr_hw *asr_hw)
{
	u32 vers = asr_hw->version_cfm.version_lmac;

	ASR_DBG(ASR_FN_ENTRY_STR);

	snprintf(asr_hw->wiphy->fw_version,
		 sizeof(asr_hw->wiphy->fw_version), "%d.%d.%d.%d",
		 (vers & (0xff << 24)) >> 24, (vers & (0xff << 16)) >> 16,
		 (vers & (0xff << 8)) >> 8, (vers & (0xff << 0)) >> 0);
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
static void asr_reg_notifier(struct wiphy *wiphy, struct regulatory_request *request)
#else
static int asr_reg_notifier(struct wiphy *wiphy, struct regulatory_request *request)
#endif
{
	struct asr_hw *asr_hw = wiphy_priv(wiphy);

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
	// For now trust all initiator
#ifdef CONFIG_ASR_USB
	if (!asr_hw->usb_remove_flag)
#endif
		asr_send_me_chan_config_req(asr_hw);
#else
	// For now trust all initiator
	return asr_send_me_chan_config_req(asr_hw);
#endif
}

void asr_read_mac_from_fw(struct asr_hw *asr_hw, u8 * buf)
{
	struct mm_fw_macaddr_cfm fw_mac;

	if (!asr_send_fw_macaddr_req(asr_hw, &fw_mac)) {
		memcpy(buf, &fw_mac, MAC_ADDR_LEN);
		dev_info(asr_hw->dev, "ASR: read mac from fw(%02X:%02X:%02X:%02X:%02X:%02X)\n",
			 buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
	} else {
		dev_err(asr_hw->dev, "ASR: read mac from fw fail\n");
	}
}

int asr_get_random_int_c(void)
{
#if 0
	int i;
	srand(time(NULL));	//may be time function need replace
	i = rand();
	return i;
#else
	int random;
	get_random_bytes(&random, sizeof(int));
	return random;
#endif
}

#ifdef LOAD_MAC_ADDR_FROM_NODE
static void asr_read_mac_from_node(struct asr_hw *asr_hw, u8 * buf)
{
	struct device_node *np = NULL;
	const void *addr = NULL;

	np = of_find_compatible_node(NULL, NULL, "asr,asr-platform");
	if (np) {
		struct property *pp = of_find_property(np, "local-mac-address", NULL);

		if (pp && pp->length == ETH_ALEN && is_valid_ether_addr(pp->value))
			addr = pp->value;

		if (addr)
			memcpy(buf, addr, sizeof(buf[0]) * 6);
	}
}
#endif

void asr_get_mac(struct asr_hw *asr_hw, u8 * buf)
{
	int index = 0, ret = 0;
	char temp_buff[3] = { 0 };
	long res = 0;
	u32 *pmac = (u32 *) mac_param;

	memset(buf, 0, 6);

#ifdef ASR_WIFI_CONFIG_SUPPORT
	memcpy(buf, g_wifi_config.mac_addr, 6);
#endif

	if ((buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 0 && buf[4] == 0 && buf[5] == 0)) {

#ifdef LOAD_MAC_ADDR_FROM_FW
		asr_read_mac_from_fw(asr_hw, buf);
#elif defined(LOAD_MAC_ADDR_FROM_NODE)
		asr_read_mac_from_node(asr_hw, buf);
#endif

		if ((buf[0] == 0) && (buf[1] == 0) && (buf[2] == 0) && (buf[3] == 0) && (buf[4] == 0) && (buf[5] == 0)) {

			dev_info(asr_hw->dev, "mac_param=%s\n", mac_param ? mac_param : "NULL");

			if (mac_param && strlen(mac_param) >= 12 && !(pmac[0] == 0 && pmac[1] == 0 && pmac[2] == 0)) {
				dev_info(asr_hw->dev, "failed read mac address, will use modules param address\n");

				for (index = 0; index < 6; index++) {
					memcpy(temp_buff, mac_param + index * 2, 2);
					ret = kstrtol(temp_buff, 16, &res);
					buf[index] = (char)res;
					//dev_info(asr_hw->dev, "temp_buff=%s,ret=%d, buf[%d]=0x%02X\n", temp_buff, ret, index, buf[index]);
				}

			} else {
				dev_info(asr_hw->dev, "failed read mac address, will use random address\n");
				buf[0] = 0xAA;
				buf[1] = 0x00;
				buf[2] = 0xCD;
				buf[3] = 0xEF;	//(u8)((asr_get_random_int_c()>>16)&0xff);
				buf[4] = (u8) ((asr_get_random_int_c() >> 8) & 0xff);
				buf[5] = (u8) ((asr_get_random_int_c() >> 0) & 0xff);
			}
		}
	}
	//show current mac address we used
	dev_info(asr_hw->dev, "mac address is %02X:%02X:%02X:%02X:%02X:%02X\n", buf[0], buf[1], buf[2], buf[3], buf[4],
		 buf[5]);
}

int rx_to_os_thread(void *data)
{
	struct asr_hw *asr_hw = (struct asr_hw *)data;
	int ret = 0;

	set_user_nice(current, -20);

	while (!kthread_should_stop()) {
		do {
			ret =
			    wait_event_interruptible
			    (asr_hw->waitq_rx_to_os_thead, (((asr_hw->ulFlag & ASR_FLAG_RX_TO_OS) != 0)
							    || kthread_should_stop()));
		} while (ret != 0);

		if (kthread_should_stop()) {
			break;
		}

		if (test_and_clear_bit(ASR_FLAG_RX_TO_OS_BIT, &asr_hw->ulFlag))
			asr_rx_to_os_task(asr_hw);
		else
			dev_err(asr_hw->dev, "ASR_FLAG_RX_TO_OS_BIT clear fail");
	}
	dev_err(asr_hw->dev, "rx_to_os_thread exit!\n");
	return 0;
}


int msgind_task_thread(void *data)
{
	struct asr_hw *asr_hw = (struct asr_hw *)data;
	int ret = 0;

	set_user_nice(current, -20);

	while (!kthread_should_stop()) {
		do {
			ret = wait_event_interruptible(asr_hw->waitq_msgind_task_thead,
						       (((asr_hw->ulFlag & ASR_FLAG_MSGIND_TASK) != 0)
							|| kthread_should_stop()));
		} while (ret != 0);

		if (kthread_should_stop()) {
			break;
		}

		if (test_and_clear_bit(ASR_FLAG_MSGIND_TASK_BIT, &asr_hw->ulFlag)) {
			asr_msgind_task(asr_hw);
		} else {
			dev_err(asr_hw->dev, "ASR_FLAG_MSGIND_TASK_BIT clear fail");
		}
	}
	dev_err(asr_hw->dev, "msgind_task_thread exit!\n");
	return 0;
}



//=====================ASR_REDUCE_TCP_ACK==================================
#ifdef ASR_REDUCE_TCP_ACK

#ifdef CONFIG_ASR_USB
static void asr_tx_tcp_ack_usb(struct asr_hw *asr_hw)
{
	struct asr_usbdev_info *asr_plat = asr_hw->plat;
	struct device *usb_dev = asr_plat->dev;
	struct asr_bus *bus_if = NULL;

	//pr_info("saved_tcp_ack_sdk ack:%p\n",asr_hw->saved_tcp_ack_sdk);
	bus_if = (struct asr_bus *)dev_get_drvdata(usb_dev);
	if (asr_hw->saved_tcp_ack_sdk->len > 2000)
		asr_err("skb->len:%u\n", asr_hw->saved_tcp_ack_sdk->len);
	else
		asr_bus_txdata(bus_if, asr_hw->saved_tcp_ack_sdk);
	asr_hw->first_ack = 1;

	return;
}
#endif //CONFIG_ASR_USB

#ifdef CONFIG_ASR_SDIO
static void asr_tx_tcp_ack_sdio(struct asr_hw *asr_hw)
{
	struct asr_vif *asr_vif = NULL;

	if (asr_hw == NULL) {
		return;
	}

	asr_vif = asr_hw->vif_table[asr_hw->vif_index];

	//dev_info(asr_hw->dev, "%s:sended cnt:%d ack:%p,seq=%u\n",
	//	__func__,asr_hw->recvd_tcp_ack_count,asr_hw->saved_tcp_ack_sdk,
	//	htonl(tcp_hdr(asr_hw->saved_tcp_ack_sdk)->ack_seq));

	asr_start_xmit_agg_sdio(asr_hw, asr_vif, asr_hw->saved_tcp_ack_sdk);

	asr_hw->saved_tcp_ack_sdk = NULL;
	asr_hw->first_ack = 1;
}
#endif //CONFIG_ASR_SDIO

void tcp_ack_work_func(struct work_struct *work)
{
	struct asr_work_ctx *temp_work = container_of(work, struct asr_work_ctx, real_work);
	struct asr_hw *asr_hw = temp_work->asr_hw;

	if (asr_hw == NULL || asr_hw->mod_params->tcp_ack_num == 0) {
		return;
	}
	//pr_info("100ms rece tcp ack:%d\n",asr_hw->recvd_tcp_ack_count);
	spin_lock_bh(&asr_hw->tcp_ack_lock);
	asr_hw->recvd_tcp_ack_count = asr_hw->mod_params->tcp_ack_num;
	if(asr_hw->saved_tcp_ack_sdk)
	{
#ifdef CONFIG_ASR_SDIO
		asr_tx_tcp_ack_sdio(asr_hw);
#elif defined CONFIG_ASR_USB
		asr_tx_tcp_ack_usb(asr_hw);
#endif
	}
	spin_unlock_bh(&asr_hw->tcp_ack_lock);
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
void tcp_ack_timer_handler(struct timer_list *tl)
{
	struct asr_hw *asr_hw = from_timer(asr_hw, tl, tcp_ack_timer);
#else
void tcp_ack_timer_handler(unsigned long data)
{
	struct asr_hw *asr_hw = (struct asr_hw *)data;
#endif

	asr_hw->tcp_ack_work.asr_hw = asr_hw;
	schedule_work(&asr_hw->tcp_ack_work.real_work);//tcp_ack_work_func
}

#endif //ASR_REDUCE_TCP_ACK
void ate_at_cmd_work_func(struct work_struct *work)
{
	struct asr_work_ctx *temp_work = container_of(work, struct asr_work_ctx, real_work);
	struct asr_hw *asr_hw = temp_work->asr_hw;
	if (asr_hw == NULL || asr_hw->mod_params->ate_at_cmd[0] == 0 ||
		(asr_hw->mod_params->ate_at_cmd[0]=='r'&&
		asr_hw->mod_params->ate_at_cmd[1]=='s'&&
		asr_hw->mod_params->ate_at_cmd[2]=='p'&&
		asr_hw->mod_params->ate_at_cmd[3]==':') ){
		return;
	}
	//pr_info("%s \n",__func__);
	ate_data_direct_tx_rx(asr_hw, asr_hw->mod_params->ate_at_cmd);
	//pr_info("%s done\n",__func__);
}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
void ate_at_cmd_timer_handler(struct timer_list *tl)
{
	struct asr_hw *asr_hw = from_timer(asr_hw, tl, ate_at_cmd_timer);
#else
void ate_at_cmd_timer_handler(unsigned long data)
{
	struct asr_hw *asr_hw = (struct asr_hw *)data;
#endif
	asr_hw->ate_at_cmd_work.asr_hw = asr_hw;
	schedule_work(&asr_hw->ate_at_cmd_work.real_work);//ate_at_cmd_work_func
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		mod_timer(&asr_hw->ate_at_cmd_timer, jiffies + msecs_to_jiffies(ASR_ATE_AT_CMD_TIMER_OUT));
#else
		del_timer(&asr_hw->ate_at_cmd_timer);
		asr_hw->ate_at_cmd_timer.expires = jiffies + msecs_to_jiffies(ASR_ATE_AT_CMD_TIMER_OUT);
		add_timer(&asr_hw->ate_at_cmd_timer);
#endif
}
//=================================================================


#ifdef CONFIG_ASR_SDIO
int asr_sdio_init_config(struct asr_hw *asr_hw)
{
	int ioport;
	int ret = 0;

	/*step 0: get ioport */
	ioport = sdio_get_ioport(asr_hw->plat->func);
	if (ioport < 0) {
		dev_err(asr_hw->dev, "%s get ioport failed\n", __func__);
		goto exit;
	}
	asr_hw->ioport = ioport;
	dev_info(asr_hw->dev, "%s get ioport is %x\n", __func__, ioport);

	/*step 1: config host_int_status(0x03) clear when read */
	ret = asr_sdio_config_rsr(asr_hw->plat->func, HOST_INT_RSR_MASK);
	if (ret) {
		dev_err(asr_hw->dev, "%s config int rsr failed\n", __func__);
		goto exit;
	}

	/*step 2: config auto re-enable(0x6c) bit4 */
	ret = asr_sdio_config_auto_reenable(asr_hw->plat->func);
	if (ret) {
		dev_err(asr_hw->dev, "%s config auto re-enable failed\n", __func__);
		goto exit;
	}

	return 0;
exit:
	return -1;
}

int rx_thread_timer_cnt;
int rx_thread_timer_delay_cnt;
extern bool asr_main_process_running(struct asr_hw *asr_hw);
bool flag_rx_thread_timer;

int read_sdio_txrx_info(struct asr_hw *asr_hw)
{
	struct mm_hif_sdio_info_ind *sdio_info_ind = &(asr_hw->hif_sdio_info_ind);
	int ret = 0;

	//check fw tx rx idx
	memset(sdio_info_ind, 0, sizeof(struct mm_hif_sdio_info_ind));
	ret = asr_send_hif_sdio_info_req(asr_hw);
	if (ret) {
		dev_err(asr_hw->dev, "ASR: error hif sdio info read fail %d\n", ret);
		return ret;
	}

	if (sdio_info_ind->flag) {
		if (sdio_info_ind->host_tx_data_cur_idx >= 1 && sdio_info_ind->host_tx_data_cur_idx <= 15) {
			asr_hw->tx_data_cur_idx = sdio_info_ind->host_tx_data_cur_idx;
		}

		if (sdio_info_ind->rx_data_cur_idx >= 2 && sdio_info_ind->rx_data_cur_idx <= 15) {
			asr_hw->rx_data_cur_idx = sdio_info_ind->rx_data_cur_idx;
		}

		dev_err(asr_hw->dev, "ASR: hif sdio info (%d %d %d)\n", sdio_info_ind->flag,
			sdio_info_ind->host_tx_data_cur_idx, sdio_info_ind->rx_data_cur_idx);
	} else {

		dev_err(asr_hw->dev, "ASR: error hif sdio info read fail %d,%d\n", sdio_info_ind->flag, ret);

		return -1;
	}

	return ret;
}

long tx_evt_hrtimer_times;
enum hrtimer_restart tx_evt_hrtimer_handler(struct hrtimer *timer)
{
	struct asr_hw *asr_hw = container_of(timer, struct asr_hw, tx_evt_hrtimer);
	tx_evt_hrtimer_times++;

	//if (!asr_main_process_running(asr_hw))
	{
		set_bit(ASR_FLAG_MAIN_TASK_BIT, &asr_hw->ulFlag);
		wake_up_interruptible(&asr_hw->waitq_main_task_thead);
	}
	return HRTIMER_NORESTART;
}

int main_task_from_thread_cnt;

#ifdef CONFIG_ASR_KEY_DBG
extern u32 tx_agg_port_num1;
extern u32 tx_agg_port_num2;
extern u32 tx_agg_port_num3;
extern u32 tx_agg_port_num4;
extern u32 tx_agg_port_num5;
extern u32 tx_agg_port_num6;
extern u32 tx_agg_port_num7;
extern u32 tx_agg_port_num8;
extern u32 tx_near_full_cnt;
extern u32 no_avail_port_cnt;
extern uint32_t diff_bitmap_no_cnt;
extern uint32_t diff_bitmap_cnt;
extern uint32_t tx_ring_nearly_full_empty_size;
extern u32 tx_full1_cnt;
extern u32 tx_full2_cnt;

extern u32 rx_agg_port_num[];

extern long asr_irq_times, asr_sdio_times;
extern long ipc_host_irq_times;

extern u32 aggr_buf_cnt_100;
extern u32 aggr_buf_cnt_80;
extern u32 aggr_buf_cnt_40;
extern u32 aggr_buf_cnt_8;
extern u32 aggr_buf_cnt_less_8;

extern int ipc_read_sdio_reg_cnt;
extern int ipc_read_sdio_data_cnt;
extern int ipc_write_sdio_data_cnt;

extern int rx_thread_timer_cnt;
extern int rx_thread_timer_delay_cnt;
extern int ipc_read_sdio_data_restart_cnt;
extern unsigned int delta_time_max;
extern long tx_evt_irq_times;
extern long tx_evt_xmit_times;
extern int tx_task_run_cnt;
extern int tx_bitmap_change_cnt[16];
extern int sdio_isr_uplod_cnt;
extern long full_to_trigger;

extern int isr_write_clear_cnt;
extern int ipc_read_rx_bm_fail_cnt;
extern int ipc_read_tx_bm_fail_case2_cnt;
extern int ipc_read_tx_bm_fail_case3_cnt;
extern int ipc_isr_skip_main_task_cnt;
extern int more_task_update_bm_cnt;

extern int asr_rx_to_os_task_run_cnt;
extern int asr_main_task_run_cnt;
int min_rx_skb_free_cnt = 30;

extern u32 pending_data_cnt;
extern u32 direct_fwd_cnt;
extern u32 rxdesc_refwd_cnt;
extern u32 rxdesc_del_cnt;
extern int asr_rxdataind_run_cnt;
extern int asr_rxdataind_fn_cnt;
extern u32 min_deaggr_free_cnt;

// oob related.
extern long asr_oob_irq_times;
int  oob_intr_thread_cnt;
extern long asr_oob_times;

extern bool cfg_vif_disable_tx;
extern bool tra_vif_disable_tx;
extern u32 g_tra_vif_drv_txcnt;
extern u32 g_cfg_vif_drv_txcnt;

extern int asr_tra_rxdata_cnt;
extern int asr_cfg_rxdata_cnt;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
void tx_status_timer_handler(struct timer_list *tl)
{
	struct asr_hw *asr_hw = from_timer(asr_hw, tl, tx_status_timer);
#else
void tx_status_timer_handler(unsigned long data)
{
	struct asr_hw *asr_hw = (struct asr_hw *)data;
#endif
	struct asr_tx_agg *tx_agg_env = &(asr_hw->tx_agg_env);
	u32 tx_agg_num_total;
	u32 agg_buf_cnt_total =
	    aggr_buf_cnt_100 + aggr_buf_cnt_80 + aggr_buf_cnt_40 + aggr_buf_cnt_8 + aggr_buf_cnt_less_8;
	int i;
	int rx_skb_free_cnt;
	int rx_skb_to_os_cnt;
	#ifdef CFG_OOO_UPLOAD
    int rx_skb_total_cnt;
	int rx_deaggr_skb_free_cnt;
	#endif
    struct asr_vif *asr_tra_vif = NULL;
	struct asr_vif *asr_cfg_vif = NULL ;
    u32 tra_txring_bytes = 0;
	u32 cfg_txring_bytes = 0;
    u32 tra_skb_cnt = 0;
	u32 cfg_skb_cnt = 0;
        u32 tx_opt_skb_cnt = 0;

	tx_agg_num_total =
		    (tx_agg_port_num1 + tx_agg_port_num2 + tx_agg_port_num3 +
		     tx_agg_port_num4 + tx_agg_port_num5 + tx_agg_port_num6 + tx_agg_port_num7 + tx_agg_port_num8);

        if (asr_xmit_opt) {
                tx_opt_skb_cnt = skb_queue_len(&asr_hw->tx_sk_list);
		        dev_err(asr_hw->dev, "[%s] tx opt: bitmap:0x%x ,%d "
		                     " %d , agg_port_st(%d %d %d %d %d %d %d %d, %d - %d) \r\n",
			__func__,
			asr_hw->tx_use_bitmap,tx_task_run_cnt,
                        tx_opt_skb_cnt,
			tx_agg_port_num1,tx_agg_port_num2, tx_agg_port_num3, tx_agg_port_num4,
			tx_agg_port_num5, tx_agg_port_num6, tx_agg_port_num7,tx_agg_port_num8,
			tx_agg_num_total,(tx_agg_num_total / (tx_agg_port_num1 + 1)));
        } else {

	    if (tx_agg_env) {
		dev_err(asr_hw->dev, "[%s] tx agg: bitmap:0x%x (cnt:%d,delta_t:%u) aggr_buf_st(%d: %d %d %d %d %d) "
		                     "agg_port_st(%d %d %d %d %d %d %d %d, %d - %d) \r\n",
			__func__,
			asr_hw->tx_use_bitmap,
			tx_agg_env->aggr_buf_cnt,delta_time_max,
			agg_buf_cnt_total, aggr_buf_cnt_100,aggr_buf_cnt_80, aggr_buf_cnt_40, aggr_buf_cnt_8,aggr_buf_cnt_less_8,
			tx_agg_port_num1,tx_agg_port_num2, tx_agg_port_num3, tx_agg_port_num4,
			tx_agg_port_num5, tx_agg_port_num6, tx_agg_port_num7,tx_agg_port_num8,
			tx_agg_num_total,(tx_agg_num_total / (tx_agg_port_num1 + 1)));

	    }
        }

    // mrole tx debug
    if (asr_hw->vif_index < asr_hw->vif_max_num + asr_hw->sta_max_num) {
           asr_tra_vif      = asr_hw->vif_table[asr_hw->vif_index];
		   if (asr_tra_vif) {
               tra_txring_bytes = asr_tra_vif->txring_bytes;
			   tra_skb_cnt      = asr_tra_vif->tx_skb_cnt;
		   }
	}

    if (asr_hw->ext_vif_index < asr_hw->vif_max_num + asr_hw->sta_max_num){
           asr_cfg_vif     = asr_hw->vif_table[asr_hw->ext_vif_index];
		   if (asr_cfg_vif) {
		       cfg_txring_bytes  = asr_cfg_vif->txring_bytes;
			   cfg_skb_cnt       = asr_cfg_vif->tx_skb_cnt;
		   }
	}

    dev_err(asr_hw->dev, "[%s] mrole tx : cfg(%p,%d: %d, %d ,%d[%d-%d], %d) tra(%p,%d: %d, %d ,%d[%d-%d] , %d) \r\n" ,__func__,
	asr_cfg_vif,asr_hw->ext_vif_index,cfg_vif_disable_tx,g_cfg_vif_drv_txcnt,cfg_txring_bytes,CFG_VIF_LWM,CFG_VIF_HWM,tra_skb_cnt,
	asr_tra_vif,asr_hw->vif_index,    tra_vif_disable_tx,g_tra_vif_drv_txcnt,tra_txring_bytes,TRA_VIF_LWM,TRA_VIF_HWM,cfg_skb_cnt);

    g_cfg_vif_drv_txcnt = g_tra_vif_drv_txcnt = 0;

    // rx debug
    #ifndef SDIO_RXBUF_SPLIT
	rx_skb_free_cnt = skb_queue_len(&asr_hw->rx_sk_list);
	#else
	rx_skb_free_cnt = skb_queue_len(&asr_hw->rx_data_sk_list);
	#endif
	rx_skb_to_os_cnt = skb_queue_len(&asr_hw->rx_to_os_skb_list);

	dev_err(asr_hw->dev,
		"[%s] rx: isr clr fail(%d:%d %d %d %d) (up_bm:%d) (%lu + %d = %d)  (%d) rx_to_os[%d %d] rx aggr (%d %d %d %d %d %d %d %d) to_os(%d %d %d)\r\n",
		__func__,
		isr_write_clear_cnt, ipc_read_rx_bm_fail_cnt, ipc_read_tx_bm_fail_case2_cnt,ipc_read_tx_bm_fail_case3_cnt, ipc_isr_skip_main_task_cnt,
		more_task_update_bm_cnt,
		tx_evt_irq_times,main_task_from_thread_cnt, asr_main_task_run_cnt,
		ipc_write_sdio_data_cnt,
		ipc_read_sdio_data_cnt,asr_rx_to_os_task_run_cnt,
		rx_agg_port_num[0], rx_agg_port_num[1], rx_agg_port_num[2],rx_agg_port_num[3],
		rx_agg_port_num[4], rx_agg_port_num[5], rx_agg_port_num[6], rx_agg_port_num[7],
		rx_skb_free_cnt, rx_skb_to_os_cnt, min_rx_skb_free_cnt);

    // mrole rx debug.
	if (mrole_enable)
        dev_err(asr_hw->dev, "[%s] mrole rx : [%d-%d] \r\n" ,__func__,asr_tra_rxdata_cnt,asr_cfg_rxdata_cnt);

	asr_tra_rxdata_cnt = 0;
    asr_cfg_rxdata_cnt = 0;

    #ifdef OOB_INTR_ONLY
	dev_err(asr_hw->dev,
	"[%s] oob intr only: (%d ,%d ,%d) (%d ,%d)\r\n",
	__func__,
    asr_oob_irq_times,oob_intr_thread_cnt,asr_oob_times,tx_evt_irq_times,asr_main_task_run_cnt);
	
	asr_oob_irq_times = oob_intr_thread_cnt = asr_oob_times = 0;
	#endif

    #ifdef CFG_OOO_UPLOAD
    //  rx ooo debug
	rx_deaggr_skb_free_cnt = skb_queue_len(&asr_hw->rx_sk_sdio_deaggr_list);
	rx_skb_total_cnt = pending_data_cnt + direct_fwd_cnt;
	dev_err(asr_hw->dev,
	"[%s] ooo rx debug (%d %d %d %d) -> (%d == %d) fn_skb(%d) min_free_deaggr(%d) cur_free_deaggr(%d)\r\n",
	__func__,
    pending_data_cnt,direct_fwd_cnt,rxdesc_refwd_cnt,rxdesc_del_cnt,
	asr_rxdataind_run_cnt,rx_skb_total_cnt,
	asr_rxdataind_fn_cnt,min_deaggr_free_cnt,rx_deaggr_skb_free_cnt);
	#endif

	//clear
	delta_time_max = 0;
	tx_full1_cnt = tx_full2_cnt = 0;

	tx_agg_port_num1 = tx_agg_port_num2 = tx_agg_port_num3 =
	    tx_agg_port_num4 = tx_agg_port_num5 = tx_agg_port_num6 = tx_agg_port_num7 = tx_agg_port_num8 = 0;
	aggr_buf_cnt_100 = aggr_buf_cnt_80 = aggr_buf_cnt_40 = aggr_buf_cnt_8 = aggr_buf_cnt_less_8 = 0;
	tx_evt_hrtimer_times = tx_evt_irq_times = tx_evt_xmit_times =
	    full_to_trigger = tx_task_run_cnt = asr_main_task_run_cnt = 0;
	no_avail_port_cnt = diff_bitmap_no_cnt = diff_bitmap_cnt = 0;
	ipc_host_irq_times = asr_irq_times = asr_sdio_times =
	    sdio_isr_uplod_cnt = ipc_read_sdio_reg_cnt =
	    ipc_write_sdio_data_cnt = ipc_read_sdio_data_cnt = ipc_read_sdio_data_restart_cnt = 0;

	for (i = 0; i < 16; i++)
		tx_bitmap_change_cnt[i] = 0;

	isr_write_clear_cnt = ipc_read_rx_bm_fail_cnt =
	    ipc_read_tx_bm_fail_case2_cnt = ipc_read_tx_bm_fail_case3_cnt =
	    ipc_isr_skip_main_task_cnt = more_task_update_bm_cnt =
	    main_task_from_thread_cnt = asr_rx_to_os_task_run_cnt = 0;
	for (i = 0; i < 8; i++)
		rx_agg_port_num[i] = 0;

	if (tx_status_debug)
	{
        #if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	    mod_timer(&asr_hw->tx_status_timer, jiffies + msecs_to_jiffies(tx_status_debug * 1000));
        #else
		asr_hw->tx_status_timer.expires = jiffies + tx_status_debug * (HZ);	//2s
		add_timer(&asr_hw->tx_status_timer);
		#endif
	}
}
#endif

#ifdef OOB_INTR_ONLY
int oob_intr_thread(void *data)
{
	struct asr_hw *asr_hw = (struct asr_hw *)data;
	int ret = 0;

    // same as sdio_irq_thread
	//struct sched_param param = {.sched_priority = 1 };
	struct sched_param param = {.sched_priority = 100-3};
	sched_setscheduler(current, SCHED_FIFO, &param);

	while (!kthread_should_stop()) {
		do {
			ret = wait_event_interruptible(asr_hw->waitq_oob_intr_thead, (((asr_hw->ulFlag & ASR_FLAG_OOB_INTR) != 0)
							     || kthread_should_stop()));
		} while (ret != 0);

		if (kthread_should_stop()) {
			break;
		}

		if (test_and_clear_bit(ASR_FLAG_OOB_INT_BIT, &asr_hw->ulFlag)) {
			oob_intr_thread_cnt++;
			asr_oob_isr(asr_hw);
		} else
			dev_err(asr_hw->dev, "ASR_FLAG_OOB_INT_BIT clear fail");
	}
	dev_err(asr_hw->dev, "oob_intr_thread exit!\n");
	return 0;
}
#endif

int main_task_thread(void *data)
{
	struct asr_hw *asr_hw = (struct asr_hw *)data;
	int ret = 0;

	set_user_nice(current, -10);	//lower than to_os_task


	while (!kthread_should_stop()) {
		do {
			ret = wait_event_interruptible(asr_hw->waitq_main_task_thead,
				(((asr_hw->ulFlag & ASR_FLAG_MAIN_TASK) != 0)
				|| (asr_hw->ulFlag & ASR_FLAG_SDIO_INT_TASK) != 0
				|| kthread_should_stop()));
		} while (ret != 0);

		if (kthread_should_stop()) {
			break;
		}

		if (test_and_clear_bit(ASR_FLAG_SDIO_INT_BIT, &asr_hw->ulFlag)) {
			asr_sdio_dataworker(&asr_hw->datawork);
		}

		if (test_and_clear_bit(ASR_FLAG_MAIN_TASK_BIT, &asr_hw->ulFlag)) {
			main_task_from_thread_cnt++;
			asr_main_task(asr_hw, SDIO_THREAD);
		}
	}
	dev_err(asr_hw->dev, "main_task_thread exit!\n");
	return 0;
}

#ifdef CONFIG_ASR_SDIO
#ifdef ASR_MODULE_RESET_SUPPORT
static int dev_recover_link_up(struct asr_hw *asr_hw, struct asr_vif *asr_vif, unsigned long pre_drv_flags)
{
	int ret = -1;
	struct ipc_e2a_msg *msg = NULL;
	struct sm_disconnect_ind *disc_ind = NULL;
	struct mm_add_if_cfm add_if_cfm;
#ifdef ASR_WIFI_CONFIG_SUPPORT
	struct mm_set_tx_pwr_rate_cfm tx_pwr_cfm;
	struct mm_set_tx_pwr_rate_req tx_pwr_req;
#endif

	dev_err(asr_hw->dev, "%s: asr_vif=%p,up=%d\n", __func__, asr_vif, asr_vif ? asr_vif->up : 0);

	if (asr_vif && asr_vif->up) {

		dev_err(asr_hw->dev, "%s: recover up\n", __func__);

#ifdef ASR_WIFI_CONFIG_SUPPORT
		//set tx pwr
		if (g_wifi_config.pwr_config) {
			memset(&tx_pwr_cfm, 0, sizeof(tx_pwr_cfm));
			memcpy(&tx_pwr_req, g_wifi_config.tx_pwr, sizeof(tx_pwr_req));
			asr_send_set_tx_pwr_rate(asr_hw, &tx_pwr_cfm, &tx_pwr_req);
		}
#endif

		// Start the FW
		if ((ret = asr_send_start(asr_hw))) {
			dev_err(asr_hw->dev, "%s: asr_send_start fail\n", __func__);
			return ret;
		}

		/* Device is now started */
		set_bit(ASR_DEV_STARTED, &asr_hw->phy_flags);

		/* Forward the information to the LMAC,
		 *     p2p value not used in FMAC configuration, iftype is sufficient */
		if ((ret = asr_send_add_if(asr_hw, asr_vif->ndev->dev_addr, ASR_VIF_TYPE(asr_vif), false, &add_if_cfm))) {
			dev_err(asr_hw->dev, "%s: asr_send_add_if fail\n", __func__);
			return ret;
		}
		if (add_if_cfm.status != 0) {
			dev_err(asr_hw->dev, "%s: asr_send_add_if fail status=%d\n", __func__, add_if_cfm.status);
			return ret;
		}

		/* Abort scan request on the vif */
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
		if (asr_hw->scan_request && asr_hw->scan_request->wdev == &asr_vif->wdev) {
#else
		if (asr_hw->scan_request && asr_hw->scan_request->dev == asr_vif->wdev.netdev) {
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 37)
			struct cfg80211_scan_info info = {
				.aborted = true,
			};
			cfg80211_scan_done(asr_hw->scan_request, &info);
#else
			cfg80211_scan_done(asr_hw->scan_request, true);
#endif
			asr_hw->scan_request = NULL;
		    asr_hw->scan_vif_index = 0xFF;

		}
		//sync disconnect to wpa
		dev_err(asr_hw->dev, "%s: sync disconnect to wpa\n", __func__);

		//fix kernel net core status not sync to driver bug
		set_bit(ASR_DEV_STA_CONNECTED, &asr_vif->dev_flags);
		clear_bit(ASR_DEV_STA_DHCPEND, &asr_vif->dev_flags);

		msg = (struct ipc_e2a_msg *)kmalloc((sizeof(struct ipc_e2a_msg)), GFP_ATOMIC);
		if (msg) {
			disc_ind = (struct sm_disconnect_ind *)(msg->param);
			memset(disc_ind, 0, sizeof(struct sm_disconnect_ind));
			disc_ind->vif_idx = asr_hw->vif_index;
			disc_ind->reason_code = 1;
			disc_ind->ft_over_ds = 0;
			asr_rx_sm_disconnect_ind(asr_hw, NULL, msg);
			kfree(msg);
		}
	}

	return 0;
}

void dev_restart_work_func(struct work_struct *work)
{
	struct asr_work_ctx *temp_work = container_of(work, struct asr_work_ctx, real_work);
	struct asr_hw *asr_hw = temp_work->asr_hw;
	u32 reason = temp_work->parm1;
	struct asr_vif *asr_vif = NULL;
	struct asr_vif *asr_ext_vif = NULL;
	struct asr_vif *asr_vif_tmp = NULL;
	struct asr_plat *asr_plat = asr_hw->plat;
	unsigned long pre_drv_flags = asr_hw->phy_flags;
	struct mmc_host *mmc = NULL;
	int ret = 0;

	if (!asr_plat || g_asr_para.dev_driver_remove) {
		dev_err(asr_hw->dev, "%s: cancel in dev_driver_remove\n", __func__);
		return;
	}
	//delay for other thread run end
	msleep(100);

	dev_err(asr_hw->dev, "%s: [%d,%s]reset wifi,drv_flags=0X%X,reason=%u\n",
		__func__, current->pid, current->comm, (unsigned int)asr_hw->phy_flags, reason);

	if (asr_hw->vif_index < asr_hw->vif_max_num + asr_hw->sta_max_num) {
		asr_vif = asr_hw->vif_table[asr_hw->vif_index];
	}

	if (mrole_enable &&
		(asr_hw->ext_vif_index < asr_hw->vif_max_num + asr_hw->sta_max_num)) {
		asr_ext_vif = asr_hw->vif_table[asr_hw->ext_vif_index];
	}

	if (!test_and_set_bit(ASR_DEV_RESTARTING, &asr_hw->phy_flags)) {

#ifdef CONFIG_ASR_SDIO
		del_timer_sync(&asr_hw->txflow_timer);
#endif
		clear_bit(ASR_DEV_STARTED, &asr_hw->phy_flags);

		// vif dev flags clear.
		if (asr_vif) {
			clear_bit(ASR_DEV_CLOSEING, &asr_vif->dev_flags);
			clear_bit(ASR_DEV_SCANING, &asr_vif->dev_flags);
			clear_bit(ASR_DEV_STA_CONNECTING, &asr_vif->dev_flags);
			clear_bit(ASR_DEV_STA_CONNECTED, &asr_vif->dev_flags);
			clear_bit(ASR_DEV_STA_DISCONNECTING, &asr_vif->dev_flags);
			clear_bit(ASR_DEV_STA_DHCPEND, &asr_vif->dev_flags);
			clear_bit(ASR_DEV_STA_DEL_KEY, &asr_vif->dev_flags);
		}


        // if ext vif exist, clear dev flags.
		if (asr_ext_vif) {
			clear_bit(ASR_DEV_CLOSEING, &asr_ext_vif->dev_flags);
			clear_bit(ASR_DEV_SCANING, &asr_ext_vif->dev_flags);
			clear_bit(ASR_DEV_STA_CONNECTING, &asr_ext_vif->dev_flags);
			clear_bit(ASR_DEV_STA_CONNECTED, &asr_ext_vif->dev_flags);
			clear_bit(ASR_DEV_STA_DISCONNECTING, &asr_ext_vif->dev_flags);
			clear_bit(ASR_DEV_STA_DHCPEND, &asr_ext_vif->dev_flags);
			clear_bit(ASR_DEV_STA_DEL_KEY, &asr_ext_vif->dev_flags);
		}

		mmc = asr_plat->func->card->host;
		mmc->ops->enable_sdio_irq(mmc, false);

		//wifi module reset
		if (asr_set_wifi_reset(asr_hw->dev, 0)) {
			dev_err(asr_hw->dev, "%s: asr_set_wifi_reset fail\n", __func__);
			goto dev_restart_end;
		}

		sdio_claim_host(asr_plat->func);
		mmc->ops->enable_sdio_irq(mmc, true);
		ret = sdio_release_irq(asr_plat->func);
		sdio_release_host(asr_plat->func);
		if (ret) {
			dev_err(asr_hw->dev, "ERROR: release sdio irq fail %d.\n", ret);
		} else {
			dev_err(asr_hw->dev, "ASR: release sdio irq success.\n");
		}

dev_reset_retry:
		//wifi module reset
		if (asr_set_wifi_reset(asr_hw->dev, 50)) {
			dev_err(asr_hw->dev, "%s: asr_set_wifi_reset fail\n", __func__);
			goto dev_restart_end;
		}

		dev_info(asr_hw->dev, "ASR: sdio init start.\n");

		if (!test_and_set_bit(ASR_DEV_INITED, &asr_hw->phy_flags)) {
			dev_err(asr_hw->dev, "%s: handler in nornal process\n", __func__);

			g_asr_para.dev_reset_start = false;
			asr_sdio_detect_change();

			goto dev_restart_end;
		}

		g_asr_para.dev_reset_start = true;
		asr_sdio_detect_change();

		if (!wait_for_completion_timeout(&g_asr_para.reset_complete, msecs_to_jiffies(5000))) {
			dev_err(asr_hw->dev, "%s: asr_sdio_detect_change fail\n", __func__);
			goto dev_restart_end;
		}

		sdio_set_drvdata(asr_plat->func, asr_hw);

		//int sdio
		if (asr_sdio_init_config(asr_hw)) {
			dev_err(asr_hw->dev, "%s: asr_sdio_init_config fail\n", __func__);
			goto dev_restart_end;
		}
		//disable host interrupt first, using polling method when download fw
		asr_sdio_disable_int(asr_plat->func, HOST_INT_DNLD_EN | HOST_INT_UPLD_EN);

		if (asr_plat_lmac_load(asr_plat)) {
			dev_err(asr_hw->dev, "%s: asr_plat_lmac_load fail\n", __func__);
			msleep(3000);
			goto dev_reset_retry;
		}

		sdio_claim_host(asr_plat->func);
		/*set block size SDIO_BLOCK_SIZE */
		sdio_set_block_size(asr_plat->func, SDIO_BLOCK_SIZE);
		ret = sdio_claim_irq(asr_plat->func, asr_sdio_isr);
		sdio_release_host(asr_plat->func);
		if (ret) {
			dev_err(asr_hw->dev, "%s: sdio_claim_irq fail\n", __func__);
			goto dev_restart_end;
		}
		//enable sdio host interrupt
		ret = asr_sdio_enable_int(asr_plat->func, HOST_INT_DNLD_EN | HOST_INT_UPLD_EN);
		if (ret) {
			dev_err(asr_hw->dev, "%s: asr_sdio_enable_int fail\n", __func__);
			goto dev_restart_end;
		}

		if (asr_xmit_opt) {
			spin_lock_bh(&asr_hw->tx_lock);
		    asr_drop_tx_vif_skb(asr_hw,NULL);
			list_for_each_entry(asr_vif_tmp, &asr_hw->vifs, list) {
				asr_vif_tmp->tx_skb_cnt = 0;
			}
			spin_unlock_bh(&asr_hw->tx_lock);
		} else {
			spin_lock_bh(&asr_hw->tx_agg_env_lock);
			asr_tx_agg_buf_reset(asr_hw);
			list_for_each_entry(asr_vif_tmp, &asr_hw->vifs, list) {
				asr_vif_tmp->txring_bytes = 0;
			}
			spin_unlock_bh(&asr_hw->tx_agg_env_lock);
		}

		//clear cmd crash flag
		asr_hw->cmd_mgr.state = 0;
		asr_hw->tx_data_cur_idx = 1;
		asr_hw->rx_data_cur_idx = 2;
		asr_hw->tx_use_bitmap = 1;	//for msg tx
		g_asr_para.sdio_send_times = 0;

		msleep(20);

		/* Reset FW */
		if ((ret = asr_send_reset(asr_hw))) {
			dev_err(asr_hw->dev, "%s: asr_send_reset fail\n", __func__);
			goto dev_restart_end;
		}
		if ((ret = asr_send_version_req(asr_hw, &asr_hw->version_cfm))) {
			dev_err(asr_hw->dev, "%s: asr_send_version_req fail\n", __func__);
			goto dev_restart_end;
		}
		asr_set_vers(asr_hw);

		if ((ret = asr_send_fw_softversion_req(asr_hw, &asr_hw->fw_softversion_cfm))) {
			dev_err(asr_hw->dev, "%s: asr_send_fw_softversion_req fail\n", __func__);
			goto dev_restart_end;
		}
		dev_info(asr_hw->dev, "%s: fw version (%s).\n", __func__, asr_hw->fw_softversion_cfm.fw_softversion);

		/* Set parameters to firmware */
		asr_send_me_config_req(asr_hw);

		/* Set channel parameters to firmware (must be done after WiPHY registration) */
		asr_send_me_chan_config_req(asr_hw);

		if (dev_recover_link_up(asr_hw, asr_vif, pre_drv_flags)) {
			dev_err(asr_hw->dev, "%s: dev_recover_link_up fail\n", __func__);
			goto dev_restart_end;
		}

		dev_err(asr_hw->dev, "%s: dev restart end!\n", __func__);
	}

dev_restart_end:
	clear_bit(ASR_DEV_PRE_RESTARTING, &asr_hw->phy_flags);
	clear_bit(ASR_DEV_RESTARTING, &asr_hw->phy_flags);
	return;
}
#endif
#endif

#ifdef ASR_HEARTBEAT_DETECT
void heartbeat_work_func(struct work_struct *work)
{
	struct asr_work_ctx *temp_work = container_of(work, struct asr_work_ctx, real_work);
	struct asr_hw *asr_hw = temp_work->asr_hw;
	#if 0
	struct asr_vif *asr_vif = NULL;
	s8 rssi = 0;
	static bool fram_enable = true;
	u8 appie_beacom[] = {0xD0,0x9,0x0,0xE0,0xFD,0x1,0x2,0x3,0x4,0x5,0x6};
	u8 appie_probereq[] = {0xD0,0xA,0x0,0xE0,0xFD,0x7,0x8,0x9,0xa,0xb,0xc,0xd};
	u8 appie_probersp[] = {0xD0,0xB,0x0,0xE0,0xFD,0xe,0xf,0x10,0x11,0x12,0x13,0x14,0x15};
	#endif

	if (asr_hw == NULL) {
		return;
	}

	if (test_bit(ASR_DEV_INITED, &asr_hw->phy_flags)
	    && !test_bit(ASR_DEV_RESTARTING, &asr_hw->phy_flags)) {

		if (!(asr_send_fw_softversion_req(asr_hw, &asr_hw->fw_softversion_cfm))) {
			//dev_info(asr_hw->dev, "%s:fw version(%s).\n", __func__, asr_hw->fw_softversion_cfm.fw_softversion);
		}

		#if 0
		asr_vif = asr_hw->vif_table[asr_hw->vif_index];
		if (asr_vif && asr_vif->sta.ap && !asr_send_get_rssi_req(asr_hw, asr_vif->sta.ap->sta_idx, &rssi)) {
			dev_info(asr_hw->dev, "%s:%d,rssi=%d.\n", __func__, asr_vif->sta.ap->sta_idx,rssi);
		}
		if (!asr_send_upload_fram_req(asr_hw, asr_hw->vif_index, IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_BEACON, fram_enable)) {
			dev_info(asr_hw->dev, "%s:set vif=%d upload beacom %d.\n", __func__, asr_hw->vif_index, fram_enable);
		}
		if (!asr_send_upload_fram_req(asr_hw, asr_hw->vif_index, IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_PROBE_REQ, fram_enable)) {
			dev_info(asr_hw->dev, "%s:set vif=%d upload probereq %d.\n", __func__, asr_hw->vif_index, fram_enable);
		}
		if (!asr_send_upload_fram_req(asr_hw, asr_hw->vif_index, IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_PROBE_RESP, fram_enable)) {
			dev_info(asr_hw->dev, "%s:set vif=%d upload proberesp %d.\n", __func__, asr_hw->vif_index, fram_enable);
		}

		if (!asr_send_fram_appie_req(asr_hw, asr_hw->vif_index, IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_BEACON,
			appie_beacom, fram_enable ? sizeof(appie_beacom) : 0)) {
			dev_info(asr_hw->dev, "%s:set vif=%d appie beacom %lu.\n", __func__, asr_hw->vif_index, fram_enable ? sizeof(appie_beacom) : 0);
		}
		if (!asr_send_fram_appie_req(asr_hw, asr_hw->vif_index, IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_PROBE_REQ,
			appie_probereq, fram_enable ? sizeof(appie_probereq) : 0)) {
			dev_info(asr_hw->dev, "%s:set vif=%d appie probereq %lu.\n", __func__, asr_hw->vif_index, fram_enable ? sizeof(appie_probereq) : 0);
		}
		if (!asr_send_fram_appie_req(asr_hw, asr_hw->vif_index, IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_PROBE_RESP,
			appie_probersp, fram_enable ? sizeof(appie_probersp) : 0)) {
			dev_info(asr_hw->dev, "%s:set vif=%d appie probersp %lu.\n", __func__, asr_hw->vif_index, fram_enable ? sizeof(appie_probersp) : 0);
		}

		fram_enable = !fram_enable;
		#endif
	}

}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
void heartbeat_timer_handler(struct timer_list *tl)
{
	struct asr_hw *asr_hw = from_timer(asr_hw, tl, heartbeat_timer);
#else
void heartbeat_timer_handler(unsigned long data)
{
	struct asr_hw *asr_hw = (struct asr_hw *)data;
#endif

	//dev_info(asr_hw->dev,"%s:!\n", __func__);

	asr_hw->heartbeat_work.asr_hw = asr_hw;
	schedule_work(&asr_hw->heartbeat_work.real_work);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	mod_timer(&asr_hw->heartbeat_timer, jiffies + msecs_to_jiffies(ASR_HEARTBEAT_TIMER_OUT));
#else
	del_timer(&asr_hw->heartbeat_timer);
	asr_hw->heartbeat_timer.expires = jiffies + msecs_to_jiffies(ASR_HEARTBEAT_TIMER_OUT);
	add_timer(&asr_hw->heartbeat_timer);
#endif

}
#endif

//============scan cmd timeout func============================
void cmd_crash_work_func(struct work_struct *work)
{
	struct asr_work_ctx *temp_work = container_of(work, struct asr_work_ctx, real_work);
	struct asr_hw *asr_hw = temp_work->asr_hw;
    struct asr_vif *asr_vif = NULL;

	if (asr_hw == NULL) {
		return;
	}

	if (asr_hw->vif_index < asr_hw->vif_max_num + asr_hw->sta_max_num) {
		asr_vif = asr_hw->vif_table[asr_hw->vif_index];
	}

	if (test_bit(ASR_DEV_INITED, &asr_hw->phy_flags) &&
	    test_bit(ASR_DEV_SCANING, &asr_vif->dev_flags)) {

		dev_err(asr_hw->dev, "%s: phy_flags=0X%lX , dev_flags=0X%lX.\n", __func__, asr_hw->phy_flags,
		                                                         asr_vif->dev_flags);

		if (asr_hw->scan_request) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
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
		cmd_queue_crash_handle(asr_hw, __func__, __LINE__, ASR_RESTART_REASON_SCAN_FAIL);
#endif

		clear_bit(ASR_DEV_SCANING, &asr_vif->dev_flags);
	}
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
void scan_cmd_timer_handler(struct timer_list *tl)
{
	struct asr_hw *asr_hw = from_timer(asr_hw, tl, scan_cmd_timer);
#else
void scan_cmd_timer_handler(unsigned long data)
{
	struct asr_hw *asr_hw = (struct asr_hw *)data;
#endif

	dev_info(asr_hw->dev, "%s:phy_flags=0X%lX!\n", __func__, asr_hw->phy_flags);

	asr_hw->cmd_crash_work.asr_hw = asr_hw;
	schedule_work(&asr_hw->cmd_crash_work.real_work);

}

//==========================================================================

//=========================SDIO DEBUG TIMER=================================

#ifdef ASR_STATS_RATES_TIMER
void statsrates_work_func(struct work_struct *work)
{
	struct asr_work_ctx *temp_work = container_of(work, struct asr_work_ctx, real_work);
	struct asr_hw *asr_hw = temp_work->asr_hw;

	asr_stats_update_txrx_rates(asr_hw);

}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
void statsrates_timer_handler(struct timer_list *tl)
{
	struct asr_hw *asr_hw = from_timer(asr_hw, tl, statsrates_timer);
#else
void statsrates_timer_handler(unsigned long data)
{
	struct asr_hw *asr_hw = (struct asr_hw *)data;
#endif

	asr_hw->statsrates_work.asr_hw = asr_hw;
	schedule_work(&asr_hw->statsrates_work.real_work);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	mod_timer(&asr_hw->statsrates_timer, jiffies + msecs_to_jiffies(ASR_STATS_RATES_TIMER_OUT));
#else
	del_timer(&asr_hw->statsrates_timer);
	asr_hw->statsrates_timer.expires = jiffies + msecs_to_jiffies(ASR_STATS_RATES_TIMER_OUT);
	add_timer(&asr_hw->statsrates_timer);
#endif

}
#endif
//==========================================================================


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
void txflow_timer_handler(struct timer_list *tl)
{
	struct asr_hw *asr_hw = from_timer(asr_hw, tl, txflow_timer);
	unsigned int diff_ms = 0;
	unsigned int tx_skb_pending_cnt = 0;

#else
void txflow_timer_handler(unsigned long data)
{
	struct asr_hw *asr_hw = (struct asr_hw *)data;
	unsigned int diff_ms = 0;
	unsigned int tx_skb_pending_cnt = 0;
	if (!asr_hw) {
		return;
	}
#endif

        tx_skb_pending_cnt = asr_xmit_opt ? skb_queue_len(&asr_hw->tx_sk_list) : asr_hw->tx_agg_env.aggr_buf_cnt ;

	if (  tx_skb_pending_cnt > 0 && (skip_txflow == false)) {

		diff_ms = jiffies_to_msecs(jiffies - asr_hw->stats.last_tx);
		if (diff_ms > ASR_TXFLOW_TIMER_OUT) {

			asr_hw->restart_flag = true;
			dev_info(asr_hw->dev, "%s:pending_skb_cnt=%u,last_tx=%lu,diff_ms=%u.\n", __func__,
				 tx_skb_pending_cnt, asr_hw->stats.last_tx, diff_ms);

			set_bit(ASR_FLAG_MAIN_TASK_BIT, &asr_hw->ulFlag);
			wake_up_interruptible(&asr_hw->waitq_main_task_thead);
		}
		//dev_info(asr_hw->dev, "%s:aggr_buf_cnt=%u,last_tx=%lu,diff_ms=%u.\n", __func__,
		//              asr_hw->tx_agg_env.aggr_buf_cnt, asr_hw->stats.last_tx, diff_ms);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		mod_timer(&asr_hw->txflow_timer, jiffies + msecs_to_jiffies(ASR_TXFLOW_TIMER_OUT));
#else
		del_timer(&asr_hw->txflow_timer);
		asr_hw->txflow_timer.expires = jiffies + msecs_to_jiffies(ASR_TXFLOW_TIMER_OUT);
		add_timer(&asr_hw->txflow_timer);
#endif
	}
}

/**
 *
 */

extern int asr_txhifbuffs_alloc(struct asr_hw *asr_hw, u32 len, struct sk_buff **skb);


int asr_cfg80211_init(struct asr_plat *asr_plat, void **platform_data)
{
	struct asr_hw *asr_hw = NULL;
	int ret = 0;
	struct wiphy *wiphy = NULL;
	struct wireless_dev *wdev = NULL;
	int i;
#ifdef CONFIG_FW_TXAGG_PROC
	struct proc_dir_entry *e = NULL;
#endif
	struct wifi_mib *pmib = NULL;
	struct workqueue_struct *wq = NULL;
#ifdef ASR_WIFI_CONFIG_SUPPORT
	struct mm_set_tx_pwr_rate_cfm tx_pwr_cfm;
	struct mm_set_tx_pwr_rate_req tx_pwr_req;
#endif
	int index = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* create a new wiphy for use with cfg80211 */
	wiphy = wiphy_new(&asr_cfg80211_ops, sizeof(struct asr_hw));

	if (!wiphy) {
		dev_err(g_asr_para.dev, "Failed to create new wiphy\n");
		ret = -ENOMEM;
		goto err_out;
	}

	asr_hw = wiphy_priv(wiphy);
	g_asr_para.asr_hw = asr_hw;
	g_asr_para.sdio_send_times = 0;
	asr_hw->wiphy = wiphy;
	asr_hw->plat = asr_plat;
	asr_hw->dev = g_asr_para.dev;
	asr_hw->mod_params = &asr_module_params;
	asr_hw->tcp_pacing_shift = 7;
	asr_hw->driver_mode = driver_mode;

#ifdef CONFIG_ASR_USB
	asr_hw->usb_remove_flag = 0;
#endif

	INIT_WORK(&asr_hw->rx_deauth_work.real_work, rx_deauth_work_func);

	for(index = 0; index < NX_REMOTE_STA_MAX; index++) {
		INIT_WORK(&asr_hw->sta_deauth_work[index].real_work, rx_deauth_work_func);
	}

	asr_hw->sdio_reg_buff = devm_kzalloc(asr_hw->dev, SDIO_REG_READ_LENGTH, GFP_KERNEL | GFP_DMA);
	if (!asr_hw->sdio_reg_buff) {
		dev_err(g_asr_para.dev, "%s can't alloc %d memory\n", __func__, SDIO_REG_READ_LENGTH);
		ret = -ENOMEM;
		goto err_platon;
	}

#ifdef ASR_MODULE_RESET_SUPPORT
	INIT_WORK(&asr_hw->dev_restart_work.real_work, dev_restart_work_func);
#endif
#ifdef ASR_HEARTBEAT_DETECT
	INIT_WORK(&asr_hw->heartbeat_work.real_work, heartbeat_work_func);
#endif
	INIT_WORK(&asr_hw->cmd_crash_work.real_work, cmd_crash_work_func);
#ifdef ASR_STATS_RATES_TIMER
	INIT_WORK(&asr_hw->statsrates_work.real_work, statsrates_work_func);
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 4, 0)
	wq = alloc_ordered_workqueue("asr_wq/%s", WQ_MEM_RECLAIM, dev_name(asr_hw->dev));
#else
	wq = alloc_ordered_workqueue("asr_wq/%s", WQ_MEM_RECLAIM);
#endif
	if (!wq) {
		dev_err(asr_hw->dev, "insufficient memory to create txworkqueue\n");
		ret = -ENOMEM;
		goto err_platon;
	}
	INIT_WORK(&asr_hw->datawork, asr_sdio_dataworker);
	asr_hw->asr_wq = wq;
	/* set device pointer for wiphy */
	set_wiphy_dev(wiphy, asr_hw->dev);

	asr_hw->vif_started = 0;
	asr_hw->adding_sta = false;

#ifdef CFG_SNIFFER_SUPPORT
	asr_hw->monitor_vif_idx = 0xff;
#endif

	for (i = 0; i < NX_VIRT_DEV_MAX + NX_REMOTE_STA_MAX; i++)
		asr_hw->avail_idx_map |= BIT(i);

	asr_hwq_init(asr_hw);

	for (i = 0; i < NX_NB_TXQ; i++) {
		asr_hw->txq[i].idx = TXQ_INACTIVE;
	}

	/* Initialize RoC element pointer to NULL, indicate that RoC can be started */
	asr_hw->roc_elem = NULL;
	/* Cookie can not be 0 */
	asr_hw->roc_cookie_cnt = 1;

	wiphy->mgmt_stypes = asr_default_mgmt_stypes;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 1)
	wiphy->bands[NL80211_BAND_2GHZ] = &asr_band_2GHz;
#else
	wiphy->bands[IEEE80211_BAND_2GHZ] = &asr_band_2GHz;
#endif
	wiphy->interface_modes = BIT(NL80211_IFTYPE_STATION) |
#ifdef CFG_SNIFFER_SUPPORT
	    BIT(NL80211_IFTYPE_MONITOR) |
#endif
      #ifdef P2P_SUPPORT
        BIT(NL80211_IFTYPE_P2P_CLIENT) |
        BIT(NL80211_IFTYPE_P2P_GO) |
	  #endif
	    BIT(NL80211_IFTYPE_AP);

	wiphy->flags |=
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
	    WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL |
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 0)
	    WIPHY_FLAG_HAS_CHANNEL_SWITCH |
#endif
	    WIPHY_FLAG_4ADDR_STATION | WIPHY_FLAG_4ADDR_AP;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	wiphy->max_num_csa_counters = BCN_MAX_CSA_CPT;
#endif

	wiphy->max_remain_on_channel_duration = asr_hw->mod_params->roc_dur_max;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0)
   wiphy->features |= NL80211_FEATURE_SK_TX_STATUS;
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
	wiphy->features |= NL80211_FEATURE_NEED_OBSS_SCAN |
#ifdef CONFIG_SAE
	    NL80211_FEATURE_SAE |
#endif
	    NL80211_FEATURE_VIF_TXPOWER;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
	wiphy->features |= NL80211_FEATURE_AP_MODE_CHAN_WIDTH_CHANGE;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
	#ifdef CFG_ROAMING
	wiphy->features |= NL80211_FEATURE_TX_POWER_INSERTION;
	wiphy_ext_feature_set(wiphy, NL80211_EXT_FEATURE_RRM);
	#endif
#endif
	wiphy->iface_combinations = asr_combinations;

	wiphy->n_iface_combinations = ARRAY_SIZE(asr_combinations);
	wiphy->reg_notifier = asr_reg_notifier;

	wiphy->signal_type = CFG80211_SIGNAL_TYPE_MBM;

	wiphy->cipher_suites = cipher_suites;
	wiphy->n_cipher_suites = ARRAY_SIZE(cipher_suites) - NB_RESERVED_CIPHER;

    // add supported extended_capabilities which will be used in assoc-reqest IE.
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	if (asr_hw->mod_params->use_2040)
	    asr_hw->ext_capa[0] = WLAN_BSS_COEX_INFORMATION_REQUEST;

	asr_hw->ext_capa[0] |= WLAN_EXT_CAPA1_EXT_CHANNEL_SWITCHING;
#elif LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
	if (asr_hw->mod_params->use_2040)
		asr_hw->ext_capa[0] = BIT(0);

	asr_hw->ext_capa[0] |= BIT(2);
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
	asr_hw->ext_capa[7] = WLAN_EXT_CAPA8_OPMODE_NOTIF;

    #ifdef CONFIG_ASR595X

	// default use new tx and disable txflow bcz new powe-save.
	asr_xmit_opt = true;
	skip_txflow = true;

	if (asr_hw->mod_params->twt_request)
	    asr_hw->ext_capa[9] = BIT(5) ; //WLAN_EXT_CAPA10_TWT_REQUESTER_SUPPORT;
    #endif

	wiphy->extended_capabilities = asr_hw->ext_capa;
	wiphy->extended_capabilities_mask = asr_hw->ext_capa;
	wiphy->extended_capabilities_len = ARRAY_SIZE(asr_hw->ext_capa);
#endif

    // wait queue init.
	init_waitqueue_head(&asr_hw->waitq_main_task_thead);
    #ifdef OOB_INTR_ONLY
	init_waitqueue_head(&asr_hw->waitq_oob_intr_thead);
	#endif
	init_waitqueue_head(&asr_hw->waitq_rx_to_os_thead);
	init_waitqueue_head(&asr_hw->waitq_msgind_task_thead);

    // thread init.
	asr_hw->main_task_thread   = kthread_run(main_task_thread, asr_hw, "main_task_thread");
	#ifdef OOB_INTR_ONLY
	// used to replace sdio_irq_thread, trigger by oob intr.
	asr_hw->oob_intr_thread    = kthread_run(oob_intr_thread, asr_hw, "oob_intr_thread");
	#endif
	asr_hw->rx_to_os_thread    = kthread_run(rx_to_os_thread, asr_hw, "rx_to_os_thread");
	asr_hw->msgind_task_thread = kthread_run(msgind_task_thread, asr_hw, "msgind_thread");

	asr_hw->mlan_processing = false;
	asr_hw->more_task_flag = false;
	spin_lock_init(&asr_hw->pmain_proc_lock);
	spin_lock_init(&asr_hw->tx_msg_lock);
	spin_lock_init(&asr_hw->int_reg_lock);

	asr_hw->host_int_upld = 0;

	INIT_LIST_HEAD(&asr_hw->vifs);

	spin_lock_init(&asr_hw->tx_lock);
	mutex_init(&asr_hw->dbgdump_elem.mutex);
	mutex_init(&asr_hw->tx_msg_mutex);

	spin_lock_init(&asr_hw->cb_lock);
	spin_lock_init(&asr_hw->tx_agg_env_lock);

    #ifndef SDIO_RXBUF_SPLIT
	skb_queue_head_init(&asr_hw->rx_sk_list);
	#else
	skb_queue_head_init(&asr_hw->rx_data_sk_list);
	skb_queue_head_init(&asr_hw->rx_msg_sk_list);
	#endif

	#ifdef SDIO_DEAGGR
	skb_queue_head_init(&asr_hw->rx_sk_sdio_deaggr_list);
	#endif

	skb_queue_head_init(&asr_hw->rx_sk_split_list);

	#ifdef CFG_OOO_UPLOAD
	skb_queue_head_init(&asr_hw->rx_pending_skb_list);
	#endif

	skb_queue_head_init(&asr_hw->tx_sk_list);

	skb_queue_head_init(&asr_hw->rx_to_os_skb_list);
	skb_queue_head_init(&asr_hw->msgind_task_skb_list);

	asr_hw->tx_data_cur_idx = 1;
	asr_hw->rx_data_cur_idx = 2;
#ifdef TXBM_DELAY_UPDATE
	asr_hw->tx_last_trans_bitmap = 0;
#endif

#ifdef CONFIG_PM
	asr_hw->is_suspended = 0;
	asr_hw->hs_activated = 0;
	asr_hw->suspend_fail = 0;
	asr_hw->suspend_notify_req = 0;
	asr_hw->hs_skip_count = 0;
	asr_hw->hs_force_count = 0;
	init_waitqueue_head(&asr_hw->hs_activate_wait_q);
#endif

#if 0
	// check rd bitmap sync timer
	init_timer(&asr_hw->rx_thread_timer);
	asr_hw->rx_thread_timer.function = rx_thread_timer_handler;
	asr_hw->rx_thread_timer.data = (unsigned long)asr_hw;

	// period timer
	init_timer(&asr_hw->rx_period_timer);
	asr_hw->rx_period_timer.function = rx_period_timer_handler;
	asr_hw->rx_period_timer.data = (unsigned long)asr_hw;
#endif

#ifdef CONFIG_ASR_KEY_DBG
	// tx_status_timer used to detect tx status
	if (tx_status_debug)
	{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		timer_setup(&asr_hw->tx_status_timer, tx_status_timer_handler, 0);
#else
		init_timer(&asr_hw->tx_status_timer);
		asr_hw->tx_status_timer.function = tx_status_timer_handler;
		asr_hw->tx_status_timer.data = (unsigned long)asr_hw;
#endif
	}
#endif

#ifdef ASR_HEARTBEAT_DETECT
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	timer_setup(&asr_hw->heartbeat_timer, heartbeat_timer_handler, 0);
	mod_timer(&asr_hw->heartbeat_timer, jiffies + msecs_to_jiffies(ASR_HEARTBEAT_TIMER_OUT));
#else
	init_timer(&asr_hw->heartbeat_timer);
	asr_hw->heartbeat_timer.function = heartbeat_timer_handler;
	asr_hw->heartbeat_timer.data = (unsigned long)asr_hw;
	asr_hw->heartbeat_timer.expires = jiffies + msecs_to_jiffies(ASR_HEARTBEAT_TIMER_OUT);
	add_timer(&asr_hw->heartbeat_timer);
#endif
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	timer_setup(&asr_hw->scan_cmd_timer, scan_cmd_timer_handler, 0);
#else
	init_timer(&asr_hw->scan_cmd_timer);
	asr_hw->scan_cmd_timer.function = scan_cmd_timer_handler;
	asr_hw->scan_cmd_timer.data = (unsigned long)asr_hw;
#endif

#ifdef ASR_STATS_RATES_TIMER
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	timer_setup(&asr_hw->statsrates_timer, statsrates_timer_handler, 0);
#else
	init_timer(&asr_hw->statsrates_timer);
	asr_hw->statsrates_timer.function = statsrates_timer_handler;
	asr_hw->statsrates_timer.data = (unsigned long)asr_hw;
	asr_hw->statsrates_timer.expires = jiffies + msecs_to_jiffies(ASR_STATS_RATES_TIMER_OUT);
#endif
#endif

#ifdef CONFIG_ASR_SDIO
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	timer_setup(&asr_hw->txflow_timer, txflow_timer_handler, 0);
#else
	init_timer(&asr_hw->txflow_timer);
	asr_hw->txflow_timer.function = txflow_timer_handler;
	asr_hw->txflow_timer.data = (unsigned long)asr_hw;
#endif
#endif

#ifdef ASR_WIFI_CONFIG_SUPPORT
	//parsing config file
	memset(&g_wifi_config, 0, sizeof(g_wifi_config));
	g_wifi_config.pwr_config = false;
	asr_read_wifi_config(asr_hw);
#endif

#ifndef LOAD_MAC_ADDR_FROM_FW
	//get mac address
	asr_get_mac(asr_hw, asr_hw->mac_addr);
	memcpy(wiphy->perm_addr, asr_hw->mac_addr, ETH_ALEN);
#endif

	//tx evt hrtimer
	hrtimer_init(&asr_hw->tx_evt_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	asr_hw->tx_evt_hrtimer.function = tx_evt_hrtimer_handler;

#ifdef ASR_STATS_RATES_TIMER
	memset(asr_hw->stats.txrx_rates, 0, sizeof(asr_hw->stats.txrx_rates));
	asr_hw->stats.tx_bytes = 0;
	asr_hw->stats.rx_bytes = 0;
#endif

#ifdef ASR_REDUCE_TCP_ACK

    // FIX ME. temp disable reduce tcp ack when enable mrole.

    if (mrole_enable || asr_xmit_opt)
        asr_hw->mod_params->tcp_ack_num = 0;

	INIT_WORK(&asr_hw->tcp_ack_work.real_work, tcp_ack_work_func);
	asr_hw->recvd_tcp_ack_count = 0;
	asr_hw->saved_tcp_ack_sdk = NULL;
	asr_hw->first_ack = 1;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	timer_setup(&asr_hw->tcp_ack_timer, tcp_ack_timer_handler, 0);
#else
	init_timer(&asr_hw->tcp_ack_timer);
	asr_hw->tcp_ack_timer.function = tcp_ack_timer_handler;
	asr_hw->tcp_ack_timer.data = (unsigned long)asr_hw;
#endif
	spin_lock_init(&asr_hw->tcp_ack_lock);

#endif


	if (driver_mode == DRIVER_MODE_ATE)
	{
		/// ate at cmd init here
		INIT_WORK(&asr_hw->ate_at_cmd_work.real_work, ate_at_cmd_work_func);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		timer_setup(&asr_hw->ate_at_cmd_timer, ate_at_cmd_timer_handler, 0);
		mod_timer(&asr_hw->ate_at_cmd_timer, jiffies + msecs_to_jiffies(ASR_ATE_AT_CMD_TIMER_OUT));
#else
		init_timer(&asr_hw->ate_at_cmd_timer);
		asr_hw->ate_at_cmd_timer.function = ate_at_cmd_timer_handler;
		asr_hw->ate_at_cmd_timer.data = (unsigned long)asr_hw;
		asr_hw->ate_at_cmd_timer.expires = jiffies + msecs_to_jiffies(ASR_ATE_AT_CMD_TIMER_OUT);
		add_timer(&asr_hw->ate_at_cmd_timer);
#endif

		//spin_lock_init(&asr_hw->ate_at_cmd_lock);
		/// ate at cmd init end
	}

	if ((ret = asr_sdio_init_config(asr_hw)))
		goto err_workqueue;

	if ((ret = asr_platform_on(asr_hw)))
		goto err_workqueue;

	//download ate bin mode
	if (downloadATE) {
		goto err_lmac_reqs;
	}

	dev_info(asr_hw->dev, "%s: driver_mode=%d.\n", __func__, driver_mode);
	//driver work in ate mode
	if (driver_mode == DRIVER_MODE_ATE) {
		//char at_test_cmd[50] = "get_ate_ver\n";

		asr_ate_init(asr_hw);

		set_bit(ASR_DEV_STARTED, &asr_hw->phy_flags);

		//msleep(20);

		//ate_data_direct_tx_rx(asr_hw, at_test_cmd);

#ifdef ASR_WIFI_CONFIG_SUPPORT
		ate_sync_txpwr_rate_config(asr_hw);
#endif
	}

    /***********************************************************************************************/
	if (asr_xmit_opt) {
		struct sk_buff *hif_buf_skb = NULL;
		int i;

		skb_queue_head_init(&asr_hw->tx_hif_free_buf_list);
		skb_queue_head_init(&asr_hw->tx_hif_skb_list);

        // use hif buf skb list.
		for (i = 0; i < 2; i++) {
			// Allocate a new hif tx buff
			if (asr_txhifbuffs_alloc(asr_hw, IPC_HIF_TXBUF_SIZE, &hif_buf_skb)) {
				dev_err(asr_hw->dev, "%s:%d: hif_buf(%d) ALLOC FAILED\n", __func__, __LINE__,i);
			} else {
                if (hif_buf_skb) {
				    memset(hif_buf_skb->data, 0, IPC_HIF_TXBUF_SIZE);
				    // Add the sk buffer structure in the table of rx buffer
				    skb_queue_tail(&asr_hw->tx_hif_free_buf_list, hif_buf_skb);
                }
			}
		}
	} else {
		// use tx agg buf.
		asr_hw->tx_agg_env.aggr_buf = dev_alloc_skb(TX_AGG_BUF_SIZE);
		if (!asr_hw->tx_agg_env.aggr_buf)
			goto err_lmac_reqs;

		spin_lock_bh(&asr_hw->tx_agg_env_lock);
	    asr_tx_agg_buf_reset(asr_hw);
		spin_unlock_bh(&asr_hw->tx_agg_env_lock);

		dev_info(asr_hw->dev, "ASR: aggr_buf data (%p).\n", asr_hw->tx_agg_env.aggr_buf->data);
	}
    /**************************************************************************************************/

	//init mib
	// allocating memory for pmib
	pmib = (struct wifi_mib *)kmalloc((sizeof(struct wifi_mib)), GFP_ATOMIC);

	if (!pmib) {
		ret = -ENOMEM;
		dev_err(asr_hw->dev, "Can't kmalloc for wifi_mib (size %u)\n", (int)sizeof(struct wifi_mib));
		goto err_lmac_reqs;
	}
	memset(pmib, 0, sizeof(struct wifi_mib));
	asr_hw->pmib = pmib;
	set_mib_default(asr_hw);

    // init scan related in asr_hw.
	asr_hw->scan_request = NULL;
	asr_hw->scan_vif_index = 0xFF;

	msleep(20);
	/* Reset FW */
	if ((ret = asr_send_reset(asr_hw)))
		goto err_lmac_reqs;
	if ((ret = asr_send_version_req(asr_hw, &asr_hw->version_cfm)))
		goto err_lmac_reqs;
	asr_set_vers(asr_hw);

	if ((ret = asr_send_fw_softversion_req(asr_hw, &asr_hw->fw_softversion_cfm)))
		goto err_lmac_reqs;
	dev_info(asr_hw->dev, "ASR: fw version(%s)\n", asr_hw->fw_softversion_cfm.fw_softversion);

	ret = asr_read_mm_info(asr_hw);
	if (ret) {
		goto err_lmac_reqs;
	}

	if (asr_hw->driver_mode == DRIVER_MODE_ATE) {
		asr_hw->vif_max_num = 1;
		asr_hw->sta_max_num = 1;
	}
#if 0
	if ((ret = asr_send_set_fw_macaddr_req(asr_hw, g_fw_macaddr_default)))
		goto err_lmac_reqs;
#endif

#ifdef LOAD_MAC_ADDR_FROM_FW
	//get mac address
	asr_get_mac(asr_hw, asr_hw->mac_addr);
	memcpy(wiphy->perm_addr, asr_hw->mac_addr, ETH_ALEN);
#endif

	//if ((ret = read_sdio_txrx_info(asr_hw)))
	//    goto err_lmac_reqs;

	if (driver_mode == DRIVER_MODE_NORMAL) {
		if ((ret = asr_handle_dynparams(asr_hw, asr_hw->wiphy)))
			goto err_lmac_reqs;
	}

	/* Set parameters to firmware */
	asr_send_me_config_req(asr_hw);

	if ((ret = wiphy_register(wiphy))) {
		wiphy_err(wiphy, "Could not register wiphy device\n");
		goto err_register_wiphy;
	}

	/* Set channel parameters to firmware (must be done after WiPHY registration) */
	asr_send_me_chan_config_req(asr_hw);

#ifdef ASR_WIFI_CONFIG_SUPPORT
	//set tx pwr
	if (g_wifi_config.pwr_config) {
		memset(&tx_pwr_cfm, 0, sizeof(tx_pwr_cfm));
		memcpy(&tx_pwr_req, g_wifi_config.tx_pwr, sizeof(tx_pwr_req));
		asr_send_set_tx_pwr_rate(asr_hw, &tx_pwr_cfm, &tx_pwr_req);
	}
#endif

	*platform_data = asr_hw;
#ifdef CONFIG_ASR_DEBUGFS
	if ((ret = asr_dbgfs_register(asr_hw, "asr"))) {
		wiphy_err(wiphy, "Failed to register debugfs entries\n");
		goto err_debugfs;
	}
#endif

	rtnl_lock();

	/* Add an initial station interface */
	asr_hw->vif_index     = 0xFF;
	asr_hw->ext_vif_index = 0xFF;

	wdev = asr_interface_add(asr_hw, "wlan%d", NL80211_IFTYPE_STATION, NULL);

	rtnl_unlock();

	if (!wdev) {
		wiphy_err(wiphy, "Failed to instantiate network device\n");
		ret = -ENOMEM;
		goto err_add_interface;
	}

	wiphy_info(wiphy, "New interface create %s\n", wdev->netdev->name);

	// enable cfg interface
    if (asr_hw->vif_max_num > 1 && mrole_enable) {
		rtnl_lock();
		/* Add an initial ap interface */
		wdev = asr_interface_add(asr_hw, "asrcfgwlan", NL80211_IFTYPE_STATION, NULL);
		rtnl_unlock();

		if (!wdev) {
			wiphy_err(wiphy, "Failed to instantiate cfg network device\n");
			ret = -ENOMEM;
			goto err_add_interface;
		}
		wiphy_info(wiphy, "asr cfg interface create %s\n", wdev->netdev->name);	
	}

	if (0)			//(rx_period_timer)
	{
		asr_hw->rx_period_timer.expires = jiffies + rx_period_timer;
		add_timer(&asr_hw->rx_period_timer);
	}

#ifdef CONFIG_ASR_KEY_DBG
	if (tx_status_debug) {
        #if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	    mod_timer(&asr_hw->tx_status_timer, jiffies + msecs_to_jiffies(tx_status_debug * 1000));
        #else
		asr_hw->tx_status_timer.expires = jiffies + tx_status_debug * (HZ);
		add_timer(&asr_hw->tx_status_timer);
		#endif
	}
#endif

#ifdef CONFIG_ASR_PROC
	libasr_proc = proc_mkdir_data(DRV_PROCNAME, 0, init_net.proc_net, asr_hw);
	if (libasr_proc == NULL) {
		printk("Unable to create proc directory\n");
	}
#ifdef CONFIG_FW_TXAGG_PROC
	libasr_fwtxagg_disable = 0;
	if (libasr_proc) {
		e = proc_create_data("tx_aggr_disable", 0644, libasr_proc, &fwtxagg_disable_proc_fops, asr_hw);

		if (!e) {
			remove_proc_entry(DRV_PROCNAME, init_net.proc_net);
		} else
		    printk(" create proc tx_aggr_disable (0x%x)\n", (unsigned int)(uintptr_t) asr_hw);
	}
#endif /* CONFIG_FW_TXAGG_PROC */
#ifdef CONFIG_TWT
	if (libasr_proc) {
		e = proc_create_data("twt", 0644, libasr_proc, &twt_proc_fops, asr_hw);

		if (!e) {
			remove_proc_entry(DRV_PROCNAME, init_net.proc_net);
		} else
	        printk(" create proc twt (0x%x)\n", (unsigned int)(uintptr_t) asr_hw);
	}
#endif
#endif /* CONFIG_ASR_PROC*/

	set_bit(ASR_DEV_INITED, &asr_hw->phy_flags);

	return 0;

err_add_interface:
#ifdef CONFIG_ASR_DEBUGFS
err_debugfs:
#endif
	wiphy_unregister(asr_hw->wiphy);
err_register_wiphy:
err_lmac_reqs:
	asr_platform_off(asr_hw);
	if (asr_hw->pmib) {
		kfree(asr_hw->pmib);
		asr_hw->pmib = NULL;
	}

err_workqueue:
#ifdef ASR_HEARTBEAT_DETECT
	del_timer_sync(&asr_hw->heartbeat_timer);
#endif

#ifdef ASR_STATS_RATES_TIMER
	del_timer_sync(&asr_hw->statsrates_timer);
#endif

#ifdef CONFIG_ASR_SDIO
	del_timer_sync(&asr_hw->txflow_timer);
#endif
#ifdef ASR_DRV_DEBUG_TIMER
	del_timer_sync(&asr_hw->drvdebug_timer);
#endif

#ifdef ASR_REDUCE_TCP_ACK
	del_timer_sync(&asr_hw->tcp_ack_timer);
#endif

	del_timer_sync(&asr_hw->ate_at_cmd_timer);
	cancel_work_sync(&asr_hw->datawork);
	if (asr_hw->asr_wq) {
		destroy_workqueue(asr_hw->asr_wq);
		asr_hw->asr_wq = NULL;
	}
	//stop thread
	//kthread_stop(asr_hw->tx_thread);
	if (asr_hw->main_task_thread) {
		kthread_stop(asr_hw->main_task_thread);
		asr_hw->main_task_thread = NULL;
	}
	if (asr_hw->rx_to_os_thread) {
		kthread_stop(asr_hw->rx_to_os_thread);
		asr_hw->rx_to_os_thread = NULL;
	}
	if (asr_hw->msgind_task_thread) {
		kthread_stop(asr_hw->msgind_task_thread);
		asr_hw->msgind_task_thread = NULL;
	}

err_platon:
	flush_work(&asr_hw->rx_deauth_work.real_work);

	for(index = 0; index < NX_REMOTE_STA_MAX; index++) {
		flush_work(&asr_hw->sta_deauth_work[index].real_work);
	}

	if (asr_hw->sdio_reg_buff) {
		devm_kfree(asr_hw->dev, asr_hw->sdio_reg_buff);
		asr_hw->sdio_reg_buff = NULL;
	}

#ifdef ASR_MODULE_RESET_SUPPORT
	flush_work(&asr_hw->dev_restart_work.real_work);
#endif
	wiphy_free(wiphy);
#ifdef CONFIG_POWER_SAVE
	if (asr_plat->irq)
		free_irq(asr_plat->irq, asr_hw);

	gpio_free(asr_hw->wakeup_gpio);
#endif

#ifdef OOB_INTR_ONLY
	if (asr_plat->oob_irq)
		free_irq(asr_plat->oob_irq, asr_hw);

	gpio_free(asr_hw->oob_intr_gpio);
#endif

err_out:
	if (!downloadATE) {
		dev_err(g_asr_para.dev, "ASR: ERROR Could not read interface wlan0 flags: No such device.\n");
	}
	asr_hw->plat = NULL;

	return ret;
}
#endif //CONFIG_ASR_SDIO

#ifdef CONFIG_ASR_USB

#ifdef CONFIG_ASR_KEY_DBG
extern int tx_block_cnt;
extern int tx_unblock_cnt;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
void usb_tx_status_timer_handler(struct timer_list *tl)
{
	struct asr_hw *asr_hw = from_timer(asr_hw, tl, tx_status_timer);
#else
void usb_tx_status_timer_handler(unsigned long data)
{
	struct asr_hw *asr_hw = (struct asr_hw *)data;
#endif
	struct asr_usbdev_info *asr_plat = asr_hw->plat;

	dev_err(asr_hw->dev, "[%s]tx free(%d)(%d %d),tx block(%d %d),rx free(%d),to_os(%d)\n",
		__func__, asr_plat->tx_freecount, asr_plat->tx_low_watermark,
		asr_plat->tx_high_watermark, tx_block_cnt, tx_unblock_cnt,
		asr_plat->rx_freecount, skb_queue_len(&asr_hw->rx_to_os_skb_list));

	tx_block_cnt = tx_unblock_cnt = 0;

	if (tx_status_debug) {
        #if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	    mod_timer(&asr_hw->tx_status_timer, jiffies + msecs_to_jiffies(tx_status_debug * 1000));
        #else		
		asr_hw->tx_status_timer.expires = jiffies + tx_status_debug * (HZ);	//2s
		add_timer(&asr_hw->tx_status_timer);
		#endif
	}
}
#endif

#ifdef ASR_DRV_DEBUG_TIMER
u32 g_usb_rx_cnt, g_usb_tx_cnt, g_tx_failed_cnt, g_rx_failed_cnt, g_tx_all_cnt, g_rx_all_cnt;

void drvdebug_work_func(struct work_struct *work)
{
	struct asr_work_ctx *temp_work = container_of(work, struct asr_work_ctx, real_work);
	struct asr_hw *asr_hw = temp_work->asr_hw;

	if (asr_hw == NULL) {
		return;
	}
	if (g_usb_rx_cnt && g_usb_tx_cnt)
		pr_info("usb:(T:%d/%d R:%d/%d) \n", g_usb_tx_cnt, g_tx_all_cnt, g_usb_rx_cnt, g_rx_all_cnt);
	if (g_tx_failed_cnt || g_rx_failed_cnt)
		pr_info("usb:fail(T:%d R:%d)\n", g_tx_failed_cnt, g_rx_failed_cnt);
	g_usb_rx_cnt = g_usb_tx_cnt = g_tx_all_cnt = g_rx_all_cnt = 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
void drvdebug_timer_handler(struct timer_list *tl)
{
	struct asr_hw *asr_hw = from_timer(asr_hw, tl, drvdebug_timer);
#else
void drvdebug_timer_handler(unsigned long data)
{
	struct asr_hw *asr_hw = (struct asr_hw *)data;
#endif

	asr_hw->drvdebug_work.asr_hw = asr_hw;
	schedule_work(&asr_hw->drvdebug_work.real_work);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	mod_timer(&asr_hw->drvdebug_timer, jiffies + msecs_to_jiffies(ASR_DRVDEBUG_TIMER_OUT));
#else
	del_timer(&asr_hw->drvdebug_timer);
	asr_hw->drvdebug_timer.expires = jiffies + msecs_to_jiffies(ASR_DRVDEBUG_TIMER_OUT);
	add_timer(&asr_hw->drvdebug_timer);
#endif

}
#endif //ASR_DRV_DEBUG_TIMER



int asr_usb_platform_init(struct asr_usbdev_info *asr_plat, void **platform_data)
{
	struct asr_hw *asr_hw;
	int ret = 0;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	int i;
#ifdef CONFIG_FW_TXAGG_PROC
	struct proc_dir_entry *e = NULL;
#endif
	struct wifi_mib *pmib = NULL;
	struct device *dev = asr_plat->dev;
	struct asr_bus *bus_if = NULL;
#ifdef ASR_WIFI_CONFIG_SUPPORT
	struct mm_set_tx_pwr_rate_cfm tx_pwr_cfm;
	struct mm_set_tx_pwr_rate_req tx_pwr_req;
#endif
	int index = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* create a new wiphy for use with cfg80211 */
	wiphy = wiphy_new(&asr_cfg80211_ops, sizeof(struct asr_hw));

	if (!wiphy) {
		dev_err(dev, "Failed to create new wiphy\n");
		ret = -ENOMEM;
		goto err_out;
	}

	asr_hw = wiphy_priv(wiphy);
	asr_hw->wiphy = wiphy;
	asr_hw->plat = asr_plat;	//share with SDIO/USB
	asr_hw->dev = dev;
	asr_hw->mod_params = &asr_module_params;
	asr_hw->tcp_pacing_shift = 7;
	asr_hw->driver_mode = driver_mode;

	/* set device pointer for wiphy */
	set_wiphy_dev(wiphy, asr_hw->dev);

#ifdef ASR_WIFI_CONFIG_SUPPORT
	//parsing config file
	memset(&g_wifi_config, 0, sizeof(g_wifi_config));
	g_wifi_config.pwr_config = false;
	asr_read_wifi_config(asr_hw);
#endif
	//get mac address
	asr_get_mac(asr_hw, asr_hw->mac_addr);
	asr_hw->vif_started = 0;
	asr_hw->adding_sta = false;

#ifdef CFG_SNIFFER_SUPPORT
	asr_hw->monitor_vif_idx = 0xff;
#endif

	for (i = 0; i < NX_VIRT_DEV_MAX + NX_REMOTE_STA_MAX; i++)
		asr_hw->avail_idx_map |= BIT(i);

	asr_hwq_init(asr_hw);

	for (i = 0; i < NX_NB_TXQ; i++) {
		asr_hw->txq[i].idx = TXQ_INACTIVE;
	}

	/* Initialize RoC element pointer to NULL, indicate that RoC can be started */
	asr_hw->roc_elem = NULL;
	/* Cookie can not be 0 */
	asr_hw->roc_cookie_cnt = 1;

	memcpy(wiphy->perm_addr, asr_hw->mac_addr, ETH_ALEN);
	wiphy->mgmt_stypes = asr_default_mgmt_stypes;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 1)
	wiphy->bands[NL80211_BAND_2GHZ] = &asr_band_2GHz;
#else
	wiphy->bands[IEEE80211_BAND_2GHZ] = &asr_band_2GHz;
#endif

	wiphy->interface_modes = BIT(NL80211_IFTYPE_STATION) |
#ifdef CFG_SNIFFER_SUPPORT
	    BIT(NL80211_IFTYPE_MONITOR) |
#endif
	    BIT(NL80211_IFTYPE_AP);

	wiphy->flags |= WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL |
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 0)
	    WIPHY_FLAG_HAS_CHANNEL_SWITCH |
#endif
	    WIPHY_FLAG_4ADDR_STATION | WIPHY_FLAG_4ADDR_AP;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	wiphy->max_num_csa_counters = BCN_MAX_CSA_CPT;
#endif

	wiphy->max_remain_on_channel_duration = asr_hw->mod_params->roc_dur_max;

	wiphy->features |= NL80211_FEATURE_NEED_OBSS_SCAN | NL80211_FEATURE_SK_TX_STATUS |
#ifdef CONFIG_SAE
	    NL80211_FEATURE_SAE |
#endif
	    NL80211_FEATURE_VIF_TXPOWER;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
	wiphy->features |= NL80211_FEATURE_AP_MODE_CHAN_WIDTH_CHANGE;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
	#ifdef CFG_ROAMING
	wiphy->features |= NL80211_FEATURE_TX_POWER_INSERTION;
	wiphy_ext_feature_set(wiphy, NL80211_EXT_FEATURE_RRM);
	#endif
#endif
	wiphy->iface_combinations = asr_combinations;

	wiphy->n_iface_combinations = ARRAY_SIZE(asr_combinations);
	wiphy->reg_notifier = asr_reg_notifier;

	wiphy->signal_type = CFG80211_SIGNAL_TYPE_MBM;

	wiphy->cipher_suites = cipher_suites;
	wiphy->n_cipher_suites = ARRAY_SIZE(cipher_suites) - NB_RESERVED_CIPHER;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	asr_hw->ext_capa[0] = WLAN_EXT_CAPA1_EXT_CHANNEL_SWITCHING;
#endif

	asr_hw->ext_capa[7] = WLAN_EXT_CAPA8_OPMODE_NOTIF;

	wiphy->extended_capabilities = asr_hw->ext_capa;
	wiphy->extended_capabilities_mask = asr_hw->ext_capa;
	wiphy->extended_capabilities_len = ARRAY_SIZE(asr_hw->ext_capa);

	INIT_WORK(&asr_hw->rx_deauth_work.real_work, rx_deauth_work_func);

	for(index = 0; index < NX_REMOTE_STA_MAX; index++) {
		INIT_WORK(&asr_hw->sta_deauth_work[index].real_work, rx_deauth_work_func);
	}

#ifdef ASR_DRV_DEBUG_TIMER
	INIT_WORK(&asr_hw->drvdebug_work.real_work, drvdebug_work_func);
#endif
#ifdef ASR_REDUCE_TCP_ACK
	INIT_WORK(&asr_hw->tcp_ack_work.real_work, tcp_ack_work_func);
	asr_hw->recvd_tcp_ack_count = 0;
	asr_hw->saved_tcp_ack_sdk = NULL;
	asr_hw->first_ack = 1;

#endif
	INIT_WORK(&asr_hw->ate_at_cmd_work.real_work, ate_at_cmd_work_func);

	init_waitqueue_head(&asr_hw->waitq_rx_to_os_thead);
	init_waitqueue_head(&asr_hw->waitq_msgind_task_thead);
	asr_hw->rx_to_os_thread = kthread_run(rx_to_os_thread, asr_hw, "rx_to_os_thread");
	asr_hw->msgind_task_thread = kthread_run(msgind_task_thread, asr_hw, "msgind_thread");
	INIT_LIST_HEAD(&asr_hw->vifs);
	spin_lock_init(&asr_hw->tx_lock);
	spin_lock_init(&asr_hw->cb_lock);
	//spin_lock_init(&asr_hw->tx_agg_env_lock);

	// todolala : instead of use static sbk buf pool, urb alloc skb directly, may modify later.
	//skb_queue_head_init(&asr_hw->rx_sk_list);

	skb_queue_head_init(&asr_hw->rx_to_os_skb_list);	// chain usb rx skb
	#ifdef CFG_OOO_UPLOAD
	skb_queue_head_init(&asr_hw->rx_pending_skb_list);
	#endif
	skb_queue_head_init(&asr_hw->rx_sk_split_list);	// temp use, no need to split skb in usb rx.
	skb_queue_head_init(&asr_hw->msgind_task_skb_list);
	/* Link asr_hw to bus module */
	bus_if = dev_get_drvdata(dev);
	bus_if->asr_hw = asr_hw;

	if (asr_ipc_init(asr_hw))
		goto err_platon;

#ifdef CONFIG_ASR_KEY_DBG
	// tx_status_timer used to detect tx status
	if (tx_status_debug)
	{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		timer_setup(&asr_hw->tx_status_timer, usb_tx_status_timer_handler, 0);
#else
		init_timer(&asr_hw->tx_status_timer);
		asr_hw->tx_status_timer.function = usb_tx_status_timer_handler;
		asr_hw->tx_status_timer.data = (unsigned long)asr_hw;
#endif
	}
#endif

#ifdef ASR_DRV_DEBUG_TIMER
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	timer_setup(&asr_hw->drvdebug_timer, drvdebug_timer_handler, 0);
	mod_timer(&asr_hw->drvdebug_timer, jiffies + msecs_to_jiffies(ASR_DRVDEBUG_TIMER_OUT));
#else
	init_timer(&asr_hw->drvdebug_timer);
	asr_hw->drvdebug_timer.function = drvdebug_timer_handler;
	asr_hw->drvdebug_timer.data = (unsigned long)asr_hw;
	asr_hw->drvdebug_timer.expires = jiffies + msecs_to_jiffies(ASR_DRVDEBUG_TIMER_OUT);
	add_timer(&asr_hw->drvdebug_timer);
#endif
#endif

#ifdef ASR_REDUCE_TCP_ACK
	asr_hw->saved_tcp_ack_sdk = NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	timer_setup(&asr_hw->tcp_ack_timer, tcp_ack_timer_handler, 0);
#else
	init_timer(&asr_hw->tcp_ack_timer);
	asr_hw->tcp_ack_timer.function = tcp_ack_timer_handler;
	asr_hw->tcp_ack_timer.data = (unsigned long)asr_hw;
#endif
	spin_lock_init(&asr_hw->tcp_ack_lock);
#endif

	if (driver_mode == DRIVER_MODE_ATE)
	{

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		timer_setup(&asr_hw->ate_at_cmd_timer, ate_at_cmd_timer_handler, 0);
		mod_timer(&asr_hw->ate_at_cmd_timer, jiffies + msecs_to_jiffies(ASR_ATE_AT_CMD_TIMER_OUT));
#else
		init_timer(&asr_hw->ate_at_cmd_timer);
		asr_hw->ate_at_cmd_timer.function = ate_at_cmd_timer_handler;
		asr_hw->ate_at_cmd_timer.data = (unsigned long)asr_hw;
		asr_hw->ate_at_cmd_timer.expires = jiffies + msecs_to_jiffies(ASR_ATE_AT_CMD_TIMER_OUT);
		add_timer(&asr_hw->ate_at_cmd_timer);
#endif
	//spin_lock_init(&asr_hw->ate_at_cmd_lock);
	}




	if (driver_mode == DRIVER_MODE_ATE) {
		//char at_test_cmd[50] = "get_ate_ver\n";

		asr_ate_init(asr_hw);

		set_bit(ASR_DEV_STARTED, &asr_hw->phy_flags);

		//msleep(20);

		//ate_data_direct_tx_rx(asr_hw, at_test_cmd);

#ifdef ASR_WIFI_CONFIG_SUPPORT
		ate_sync_txpwr_rate_config(asr_hw);
#endif
	}
	//init mib
	//allocating memory for pmib
	pmib = (struct wifi_mib *)kmalloc((sizeof(struct wifi_mib)), GFP_ATOMIC);

	if (!pmib) {
		ret = -ENOMEM;
		dev_err(asr_hw->dev, "Can't kmalloc for wifi_mib (size %d)\n", (int)sizeof(struct wifi_mib));
		goto err_lmac_reqs;
	}
	memset(pmib, 0, sizeof(struct wifi_mib));
	asr_hw->pmib = pmib;

	set_mib_default(asr_hw);

    // init scan related in asr_hw.
	asr_hw->scan_request = NULL;
	asr_hw->scan_vif_index = 0xFF;

	/* Reset FW */
	if ((ret = asr_send_reset(asr_hw)))
		goto err_lmac_reqs;

	if ((ret = asr_send_version_req(asr_hw, &asr_hw->version_cfm)))
		goto err_lmac_reqs;

	asr_set_vers(asr_hw);
#ifndef CONFIG_OLD_USB
// for 1t1r golden fw
	if ((ret = asr_send_fw_softversion_req(asr_hw, &asr_hw->fw_softversion_cfm)))
		goto err_lmac_reqs;
	dev_info(asr_hw->dev, "ASR: fw version(%s)\n", asr_hw->fw_softversion_cfm.fw_softversion);
#endif

	ret = asr_read_mm_info(asr_hw);
	if (ret) {
		goto err_lmac_reqs;
	}

	if (driver_mode == DRIVER_MODE_NORMAL) {
		if ((ret = asr_handle_dynparams(asr_hw, asr_hw->wiphy)))
			goto err_lmac_reqs;
	}

	/* Set parameters to firmware */
	asr_send_me_config_req(asr_hw);

	if ((ret = wiphy_register(wiphy))) {
		wiphy_err(wiphy, "Could not register wiphy device\n");
		goto err_register_wiphy;
	}

	/* Set channel parameters to firmware (must be done after WiPHY registration) */
	asr_send_me_chan_config_req(asr_hw);

#ifdef ASR_WIFI_CONFIG_SUPPORT
		if (g_wifi_config.pwr_config) {
			memset(&tx_pwr_cfm, 0, sizeof(tx_pwr_cfm));
			memcpy(&tx_pwr_req, g_wifi_config.tx_pwr, sizeof(tx_pwr_req));
			asr_send_set_tx_pwr_rate(asr_hw, &tx_pwr_cfm, &tx_pwr_req);
		}
#endif
	*platform_data = asr_hw;

	if ((ret = asr_dbgfs_register(asr_hw, "asr"))) {
		wiphy_err(wiphy, "Failed to register debugfs entries\n");
		goto err_debugfs;
	}

	rtnl_lock();

	/* Add an initial station interface */
	asr_hw->vif_index     = 0xFF;
	asr_hw->ext_vif_index = 0xFF;
	wdev = asr_interface_add(asr_hw, "wlan%d", NL80211_IFTYPE_STATION, NULL);

	rtnl_unlock();

	if (!wdev) {
		wiphy_err(wiphy, "Failed to instantiate a network device\n");
		ret = -ENOMEM;
		goto err_add_interface;
	}

	wiphy_info(wiphy, "New interface create %s\n", wdev->netdev->name);

	// enable cfg interface , default is ap mode.
    if (asr_hw->vif_max_num > 1 && mrole_enable) {
		rtnl_lock();
		/* Add an initial ap interface */
		wdev = asr_interface_add(asr_hw, "asrcfgwlan", NL80211_IFTYPE_AP, NULL);
		rtnl_unlock();

		if (!wdev) {
			wiphy_err(wiphy, "Failed to instantiate cfg network device\n");
			ret = -ENOMEM;
			goto err_add_interface;
		}
		wiphy_info(wiphy, "asr cfg interface create %s\n", wdev->netdev->name);
	}

	if (0)			//(rx_period_timer)
	{
		asr_hw->rx_period_timer.expires = jiffies + rx_period_timer;
		add_timer(&asr_hw->rx_period_timer);
	}
#ifdef CONFIG_ASR_KEY_DBG
	if (tx_status_debug) {
        #if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	    mod_timer(&asr_hw->tx_status_timer, jiffies + msecs_to_jiffies(tx_status_debug * 1000));
        #else
		asr_hw->tx_status_timer.expires = jiffies + tx_status_debug * (HZ);
		add_timer(&asr_hw->tx_status_timer);
		#endif
	}
#endif

#ifdef CONFIG_ASR_PROC
	libasr_proc = proc_mkdir(DRV_PROCNAME, init_net.proc_net);
	if (libasr_proc == NULL) {
		printk("Unable to create proc directory\n");
	}
#ifdef CONFIG_FW_TXAGG_PROC
	libasr_fwtxagg_disable = 0;
	if (libasr_proc) {
		e = proc_create_data("tx_aggr_disable", 0644, libasr_proc, &fwtxagg_disable_proc_fops, asr_hw);

		if (!e) {
			remove_proc_entry(DRV_PROCNAME, init_net.proc_net);
		} else
		    printk(" create proc tx_aggr_disable (0x%x)\n", (unsigned int)(uintptr_t) asr_hw);
	}
#endif /* CONFIG_FW_TXAGG_PROC */
#ifdef CONFIG_TWT
	if (libasr_proc) {
		e = proc_create_data("twt", 0644, libasr_proc, &twt_proc_fops, asr_hw);

		if (!e) {
			remove_proc_entry(DRV_PROCNAME, init_net.proc_net);
		} else
		    printk(" create proc twt (0x%x)\n", (unsigned int)(uintptr_t) asr_hw);
	}
#endif
#endif /* CONFIG_ASR_PROC */

	return 0;

err_add_interface:
err_debugfs:
	wiphy_unregister(asr_hw->wiphy);
err_register_wiphy:
err_lmac_reqs:
	asr_platform_off(asr_hw);
	if (pmib)
		kfree(pmib);
err_platon:
	wiphy_free(wiphy);
#ifdef CONFIG_POWER_SAVE
	if (asr_plat->irq)
		free_irq(asr_plat->irq, asr_hw);

	gpio_free(asr_hw->wakeup_gpio);
#endif
err_out:
	return ret;

}
#endif //CONFIG_ASR_USB

/**
 *
 */
void asr_cfg80211_deinit(struct asr_hw *asr_hw)
{
	int index = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	asr_wdev_unregister(asr_hw);

	flush_work(&asr_hw->rx_deauth_work.real_work);

	for(index = 0; index < NX_REMOTE_STA_MAX; index++) {
		flush_work(&asr_hw->sta_deauth_work[index].real_work);
	}

#ifdef CONFIG_ASR_SDIO
#ifdef ASR_MODULE_RESET_SUPPORT
	flush_work(&asr_hw->dev_restart_work.real_work);
#endif
	if (asr_hw->sdio_reg_buff) {
		devm_kfree(asr_hw->dev, asr_hw->sdio_reg_buff);
		asr_hw->sdio_reg_buff = NULL;
	}
#endif

	asr_dbgfs_unregister(asr_hw);

#ifdef CONFIG_ASR_PROC
	if (libasr_proc) {
#ifdef CONFIG_FW_TXAGG_PROC
		remove_proc_entry("tx_aggr_disable", libasr_proc);
#endif
#ifdef CONFIG_TWT
		remove_proc_entry("twt", libasr_proc);
#endif
		remove_proc_entry(DRV_PROCNAME, init_net.proc_net);
		libasr_proc = NULL;
	}
#endif

	//stop thread del timer
#ifdef CONFIG_ASR_SDIO
	if (asr_hw->main_task_thread) {
		kthread_stop(asr_hw->main_task_thread);
		asr_hw->main_task_thread = NULL;
	}
	hrtimer_cancel(&asr_hw->tx_evt_hrtimer);
	//del_timer(&asr_hw->rx_thread_timer);
	//del_timer(&asr_hw->rx_period_timer);
#endif
	if (asr_hw->msgind_task_thread) {
		kthread_stop(asr_hw->msgind_task_thread);
		asr_hw->msgind_task_thread = NULL;
	}

#ifdef ASR_HEARTBEAT_DETECT
	del_timer_sync(&asr_hw->heartbeat_timer);
#endif
	del_timer_sync(&asr_hw->scan_cmd_timer);

#ifdef ASR_STATS_RATES_TIMER
	del_timer_sync(&asr_hw->statsrates_timer);
#endif

#ifdef CONFIG_ASR_SDIO
	del_timer_sync(&asr_hw->txflow_timer);
#endif
#ifdef ASR_REDUCE_TCP_ACK
	del_timer_sync(&asr_hw->tcp_ack_timer);
#endif

	if (driver_mode == DRIVER_MODE_ATE)
		del_timer_sync(&asr_hw->ate_at_cmd_timer);
#ifdef ASR_DRV_DEBUG_TIMER
	del_timer_sync(&asr_hw->drvdebug_timer);
#endif

	if (asr_hw->rx_to_os_thread) {
		kthread_stop(asr_hw->rx_to_os_thread);
		asr_hw->rx_to_os_thread = NULL;
	}
#ifdef CONFIG_ASR_KEY_DBG
	del_timer(&asr_hw->tx_status_timer);
#endif

	wiphy_unregister(asr_hw->wiphy);
	//driver work in ate mode
	if (driver_mode == DRIVER_MODE_ATE) {

		asr_ate_deinit(asr_hw);
	}
	asr_platform_off(asr_hw);

#ifdef CONFIG_ASR_SDIO
	cancel_work_sync(&asr_hw->datawork);
	if (asr_hw->asr_wq) {
		destroy_workqueue(asr_hw->asr_wq);
		asr_hw->asr_wq = NULL;
	}
#endif
	//stop thread
	//kthread_stop(asr_hw->tx_thread);

#ifdef OOB_INTR_ONLY
    // after oob irq free, stop oob intr thread.
	if (asr_hw->oob_intr_thread) {
		kthread_stop(asr_hw->oob_intr_thread);
		asr_hw->oob_intr_thread = NULL;
	}
#endif

	if (asr_hw->pmib) {
		kfree(asr_hw->pmib);
		asr_hw->pmib = NULL;
	}

	g_asr_para.asr_hw = NULL;
	g_asr_para.dev_reset_start = false;

	if (asr_hw->wiphy) {
		wiphy_free(asr_hw->wiphy);
	}
}

/**
 *
 */
#ifdef CONFIG_NOT_USED_DTS
static void canon_device_release(struct device *dev)
{
	complete(&device_release);
}

struct platform_device asr_platform = {
	.name = "asr-platform",
	//.id = 0,
	.dev = {
		.release = canon_device_release,
		},
};
#endif //CONFIG_NOT_USED_DTS

static int __init asr_mod_init(void)
{
	ASR_DBG(ASR_FN_ENTRY_STR);
	asr_print_version();

	memset(&g_asr_para, 0, sizeof(g_asr_para));
#ifdef CONFIG_ASR_USB
	g_asr_para.dev_driver_remove = true;
#endif
	init_completion(&g_asr_para.reset_complete);
#ifdef CONFIG_NOT_USED_DTS
	platform_device_register(&asr_platform);
#endif
	return asr_platform_register_drv();
}

/**
 *
 */
static void __exit asr_mod_exit(void)
{
	ASR_DBG(ASR_FN_ENTRY_STR);
#ifdef CONFIG_NOT_USED_DTS
	init_completion(&device_release);
	platform_device_unregister(&asr_platform);
#endif
	asr_platform_unregister_drv();
#ifdef CONFIG_NOT_USED_DTS
	wait_for_completion(&device_release);
#endif
}

#ifdef ROCK960
late_initcall(asr_mod_init);
#else
module_init(asr_mod_init);
#endif
module_exit(asr_mod_exit);

module_param(skip_txflow, bool, 0660);
module_param(mrole_enable, bool, 0660);
module_param(p2p_debug, bool, 0660);
module_param(force_bw20, bool, 0660);
module_param(tx_status_debug, int, 0660);
module_param(tx_debug, int, 0660);
module_param(asr_xmit_opt, bool, 0660);
module_param(tx_wait_agger, bool, 0660);
module_param(txlogen, bool, 0660);
module_param(rxlogen, bool, 0660);
module_param(irq_double_edge_en, bool, 0660);
module_param(main_task_clr_irq_en, bool, 0660);
module_param(test_host_re_read, bool, 0660);
module_param(lalalaen, int, 0660);
module_param(dbg_type, int, 0660);
module_param(rx_thread_timer, int, 0660);
module_param(rx_period_timer, int, 0660);
module_param(tx_hr_timer, int, 0660);
module_param(tx_aggr, int, 0660);
module_param(rx_aggr, int, 0660);
module_param(rx_ooo_upload, int, 0660);
module_param(tx_aggr_xmit_thres, int, 0660);
module_param(flow_ctrl_high, int, 0660);
module_param(flow_ctrl_low, int, 0660);
module_param(tx_conserve, int, 0660);
module_param(downloadfw, int, 0660);
module_param(asr_msg_level, int, 0660);
module_param(downloadATE, int, 0660);
module_param(mac_param, charp, 0660);
module_param(driver_mode, int, 0660);
module_param(sdio1thread, bool, 0660);

MODULE_FIRMWARE(ASR_CONFIG_FW_NAME);

MODULE_DESCRIPTION(RW_DRV_DESCRIPTION);
MODULE_VERSION(ASR_VERS_NUM);
MODULE_AUTHOR(RW_DRV_COPYRIGHT " " RW_DRV_AUTHOR);
MODULE_LICENSE("GPL");
;
