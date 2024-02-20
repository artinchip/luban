/**
 ******************************************************************************
 *
 * @file asr_tx.h
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */
#ifndef _ASR_TX_H_
#define _ASR_TX_H_

#include <linux/version.h>
#include <linux/ieee80211.h>
#include <net/cfg80211.h>
#include <linux/netdevice.h>
#include "ipc_shared.h"
#include "asr_txq.h"
#include "hal_desc.h"

#define ASR_HWQ_BK                     0
#define ASR_HWQ_BE                     1
#define ASR_HWQ_VI                     2
#define ASR_HWQ_VO                     3
#define ASR_HWQ_BCMC                   4
#define ASR_HWQ_NB                     NX_TXQ_CNT
#define ASR_HWQ_ALL_ACS (ASR_HWQ_BK | ASR_HWQ_BE | ASR_HWQ_VI | ASR_HWQ_VO)
#define ASR_HWQ_ALL_ACS_BIT ( BIT(ASR_HWQ_BK) | BIT(ASR_HWQ_BE) |    \
                               BIT(ASR_HWQ_VI) | BIT(ASR_HWQ_VO) )

#define ASR_TX_LIFETIME_MS             150

#define ASR_SWTXHDR_ALIGN_SZ           4
#define ASR_SWTXHDR_ALIGN_MSK (ASR_SWTXHDR_ALIGN_SZ - 1)
#define ASR_SWTXHDR_ALIGN_PADS(x) \
                    ((ASR_SWTXHDR_ALIGN_SZ - ((x) & ASR_SWTXHDR_ALIGN_MSK)) \
                     & ASR_SWTXHDR_ALIGN_MSK)
#if ASR_SWTXHDR_ALIGN_SZ & ASR_SWTXHDR_ALIGN_MSK
#error bad ASR_SWTXHDR_ALIGN_SZ
#endif

#ifdef CONFIG_ASR595X
#define HOST_PAD_LEN 62           // (2+60) , 60 is max mac extend hdr len(include wapi)
#else
#define HOST_PAD_LEN 4
#endif		
#define HOST_PAD_USB_LEN 4	
#define AMSDU_PADDING(x) ((4 - ((x) & 0x3)) & 0x3)

#define TXU_CNTRL_RETRY        BIT(0)
#define TXU_CNTRL_MORE_DATA    BIT(2)
#define TXU_CNTRL_MGMT         BIT(3)
#define TXU_CNTRL_MGMT_NO_CCK  BIT(4)
#define TXU_CNTRL_AMSDU        BIT(6)
#define TXU_CNTRL_MGMT_ROBUST  BIT(7)
#define TXU_CNTRL_USE_4ADDR    BIT(8)
#define TXU_CNTRL_EOSP         BIT(9)
#define TXU_CNTRL_MESH_FWD     BIT(10)
#define TXU_CNTRL_TDLS         BIT(11)

/// This frame is postponed internally because of PS. (only for AP)
#define TXU_CNTRL_POSTPONE_PS   BIT(12)
/// This frame is probably respond to probe request from other channels
#define TXU_CNTRL_LOWEST_RATE   BIT(13)
/// Internal flag indicating that this packet should use the trial rate as first or second rate
#define TXU_CNTRL_RC_TRIAL      BIT(14)

// new add for mrole
#define TXU_CNTRL_DROP          BIT(15)

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 10, 0)
#define IEEE80211_NUM_TIDS		16
#endif

extern const int asr_tid2hwq[IEEE80211_NUM_TIDS];

/**
 * struct asr_amsdu_txhdr - Structure added in skb headroom (instead of
 * asr_txhdr) for amsdu subframe buffer (except for the first subframe
 * that has a normal asr_txhdr)
 *
 * @list     List of other amsdu subframe (asr_sw_txhdr.amsdu.hdrs)
 * @map_len  Length to be downloaded for this subframe
 * @dma_addr Buffer address form embedded point of view
 * @skb      skb
 * @pad      padding added before this subframe
 *           (only use when amsdu must be dismantled)
 */
struct asr_amsdu_txhdr {
	struct list_head list;
	size_t map_len;
	dma_addr_t dma_addr;
	struct sk_buff *skb;
	u16 pad;
};

/**
 * struct asr_amsdu - Structure to manage creation of an A-MSDU, updated
 * only In the first subframe of an A-MSDU
 *
 * @hdrs List of subframe of asr_amsdu_txhdr
 * @len  Current size for this A-MDSU (doesn't take padding into account)
 *       0 means that no amsdu is in progress
 * @nb   Number of subframe in the amsdu
 * @pad  Padding to add before adding a new subframe
 */
struct asr_amsdu {
	struct list_head hdrs;
	u16 len;
	u8 nb;
	u8 pad;
};

/**
 * struct asr_sw_txhdr - Software part of tx header
 *
 * @asr_sta sta to which this buffer is addressed
 * @asr_vif vif that send the buffer
 * @txq pointer to TXQ used to send the buffer
 * @hw_queue Index of the HWQ used to push the buffer.
 *           May be different than txq->hwq->id on confirmation.
 * @frame_len Size of the frame (doesn't not include mac header)
 *            (Only used to update stat, can't we use skb->len instead ?)
 * @headroom Headroom added in skb to add asr_txhdr
 *           (Only used to remove it before freeing skb, is it needed ?)
 * @amsdu Description of amsdu whose first subframe is this buffer
 *        (amsdu.nb = 0 means this buffer is not part of amsdu)
 * @skb skb received from transmission
 * @map_len  Length mapped for DMA (only asr_hw_txhdr and data are mapped)
 * @dma_addr DMA address after mapping
 * @desc Buffer description that will be copied in shared mem for FW
 */
struct asr_sw_txhdr {
	struct txdesc_api desc;
};

/**
 * struct asr_txhdr - Stucture to control transimission of packet
 * (Added in skb headroom)
 *
 * @sw_hdr: Information from driver
 * @cache_guard:
 * @hw_hdr: Information for/from hardware
 */
struct asr_txhdr {
	struct asr_sw_txhdr sw_hdr;
	u8 host_pad[HOST_PAD_LEN];
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
u16 asr_select_queue(struct net_device *dev, struct sk_buff *skb, struct net_device *sb_dev);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
u16 asr_select_queue(struct net_device *dev, struct sk_buff *skb,
		     struct net_device *sb_dev, select_queue_fallback_t fallback);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
u16 asr_select_queue(struct net_device *dev, struct sk_buff *skb, void *accel_priv, select_queue_fallback_t fallback);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
u16 asr_select_queue(struct net_device *dev, struct sk_buff *skb, void *accel_priv);
#else
u16 asr_select_queue(struct net_device *dev, struct sk_buff *skb);
#endif
int asr_start_xmit(struct sk_buff *skb, struct net_device *dev);
int asr_start_mgmt_xmit(struct asr_vif *vif, struct asr_sta *sta,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
			struct cfg80211_mgmt_tx_params *params,
#else
			const u8 * buf, size_t len, bool no_cck,
#endif
			bool offchan, u64 * cookie);

struct asr_hw;
struct asr_sta;

void asr_ps_bh_enable(struct asr_hw *asr_hw, struct asr_sta *sta,
                       bool enable);
void asr_ps_bh_traffic_req(struct asr_hw *asr_hw, struct asr_sta *sta,
                            u16 pkt_req, u8 ps_id);

void asr_set_traffic_status(struct asr_hw *asr_hw, struct asr_sta *sta, bool available, u8 ps_id);

void asr_switch_vif_sta_txq(struct asr_sta *sta, struct asr_vif *old_vif, struct asr_vif *new_vif);

int asr_dbgfs_print_sta(char *buf, size_t size, struct asr_sta *sta, struct asr_hw *asr_hw);
void asr_txq_credit_update(struct asr_hw *asr_hw, int sta_idx, u8 tid, s8 update);
#ifdef CONFIG_ASR_SDIO
int asr_tx_task(struct asr_hw *asr_hw);
bool asr_tx_ring_is_ready_push(struct asr_hw *asr_hw);
bool asr_tx_ring_is_nearly_full(struct asr_hw *asr_hw);
bool asr_start_xmit_agg_sdio(struct asr_hw *asr_hw, struct asr_vif *asr_vif, struct sk_buff *skb);
void asr_tx_agg_buf_reset(struct asr_hw *asr_hw);
void asr_tx_agg_buf_mask_vif(struct asr_hw *asr_hw,struct asr_vif *asr_vif);
void asr_tx_flow_ctrl(struct asr_hw * asr_hw,struct asr_vif *asr_vif,bool check_high_wm);

// new add for tx_opt
void asr_drop_tx_vif_skb(struct asr_hw *asr_hw,struct asr_vif *asr_vif_drop);
void asr_tx_skb_sta_deinit(struct asr_hw *asr_hw,struct asr_sta *asr_sta_drop);
int asr_opt_tx_task(struct asr_hw *asr_hw);

#endif

#define TRAFFIC_VIF_FC_LEVEL       (96)
#define MROLE_VIF_FC_DIV           (128)

#define CFG_VIF_MAX_BYTES       (TX_AGG_BUF_UNIT_CNT * TX_AGG_BUF_UNIT_SIZE * (MROLE_VIF_FC_DIV - TRAFFIC_VIF_FC_LEVEL) / MROLE_VIF_FC_DIV)
#define TRAFFIC_VIF_MAX_BYTES   (TX_AGG_BUF_UNIT_CNT * TX_AGG_BUF_UNIT_SIZE * TRAFFIC_VIF_FC_LEVEL / MROLE_VIF_FC_DIV)
#define UNIQUE_VIF_MAX_BYTES    (TX_AGG_BUF_UNIT_CNT * TX_AGG_BUF_UNIT_SIZE)

#define CFG_VIF_LWM   (CFG_VIF_MAX_BYTES / 5)
#define CFG_VIF_HWM   (CFG_VIF_MAX_BYTES * 4 / 5)

#define TRA_VIF_LWM   (TRAFFIC_VIF_MAX_BYTES / 5)
#define TRA_VIF_HWM   (TRAFFIC_VIF_MAX_BYTES * 4 / 5)

#define UNIQUE_VIF_LWM   (UNIQUE_VIF_MAX_BYTES / 5)
#define UNIQUE_VIF_HWM   (UNIQUE_VIF_MAX_BYTES * 4 / 5)

#endif /* _ASR_TX_H_ */
