/**
 ****************************************************************************************
 *
 * @file asr_txq.h
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ****************************************************************************************
 */
#ifndef _ASR_TXQ_H_
#define _ASR_TXQ_H_

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/ieee80211.h>

/**
 * Fullmac TXQ configuration:
 *  - STA: 1 TXQ per TID (limited to 8)
 *         1 txq for bufferable MGT frames
 *  - VIF: 1 tXQ for Multi/Broadcast +
 *         1 TXQ for MGT for unknown STAs or non-bufferable MGT frames
 *  - 1 TXQ for offchannel transmissions
 *
 *
 * Txq mapping looks like
 * for NX_REMOTE_STA_MAX=10 and NX_VIRT_DEV_MAX=4
 *
 * | TXQ | NDEV_ID | VIF |   STA |  TID | HWQ |
 * |-----+---------+-----+-------+------+-----|-
 * |   0 |       0 |     |     0 |    0 |   1 | 9 TXQ per STA
 * |   1 |       1 |     |     0 |    1 |   0 | (8 data + 1 mgmt)
 * |   2 |       2 |     |     0 |    2 |   0 |
 * |   3 |       3 |     |     0 |    3 |   1 |
 * |   4 |       4 |     |     0 |    4 |   2 |
 * |   5 |       5 |     |     0 |    5 |   2 |
 * |   6 |       6 |     |     0 |    6 |   3 |
 * |   7 |       7 |     |     0 |    7 |   3 |
 * |   8 |     N/A |     |     0 | MGMT |   3 |
 * |-----+---------+-----+-------+------+-----|-
 * | ... |         |     |       |      |     | Same for all STAs
 * |-----+---------+-----+-------+------+-----|-
 * |  90 |      80 |   0 | BC/MC |    0 | 1/4 | 1 TXQ for BC/MC per VIF
 * | ... |         |     |       |      |     |
 * |  93 |      80 |   3 | BC/MC |    0 | 1/4 |
 * |-----+---------+-----+-------+------+-----|-
 * |  94 |     N/A |   0 |   N/A | MGMT |   3 | 1 TXQ for unknown STA per VIF
 * | ... |         |     |       |      |     |
 * |  97 |     N/A |   3 |   N/A | MGMT |   3 |
 * |-----+---------+-----+-------+------+-----|-
 * |  98 |     N/A |     |   N/A | MGMT |   3 | 1 TXQ for offchannel frame
 */
#define NX_NB_TID_PER_STA 8
#define NX_NB_TXQ_PER_STA (NX_NB_TID_PER_STA + 1)
#define NX_NB_TXQ_PER_VIF 2
#define NX_NB_TXQ ((NX_NB_TXQ_PER_STA * NX_REMOTE_STA_MAX) +    \
                   (NX_NB_TXQ_PER_VIF * NX_VIRT_DEV_MAX) + 1)

#define NX_FIRST_VIF_TXQ_IDX (NX_REMOTE_STA_MAX * NX_NB_TXQ_PER_STA)
#define NX_FIRST_BCMC_TXQ_IDX  NX_FIRST_VIF_TXQ_IDX
#define NX_FIRST_UNK_TXQ_IDX  (NX_FIRST_BCMC_TXQ_IDX + NX_VIRT_DEV_MAX)

#define NX_OFF_CHAN_TXQ_IDX (NX_FIRST_VIF_TXQ_IDX +                     \
                             (NX_VIRT_DEV_MAX * NX_NB_TXQ_PER_VIF))
#define NX_BCMC_TXQ_TYPE 0
#define NX_UNK_TXQ_TYPE  1

/**
 * Each data TXQ is a netdev queue. TXQ to send MGT are not data TXQ as
 * they did not recieved buffer from netdev interface.
 * Need to allocate the maximum case.
 * AP : all STAs + 1 BC/MC
 */
#define NX_NB_NDEV_TXQ ((NX_NB_TID_PER_STA * NX_REMOTE_STA_MAX) + 1 )
//#define NX_BCMC_TXQ_NDEV_IDX (NX_NB_TID_PER_STA * NX_REMOTE_STA_MAX)
#define NX_STA_NDEV_IDX(tid, sta_idx) ((tid) + (sta_idx) * NX_NB_TID_PER_STA)
#define NDEV_NO_TXQ 0xffff
#if (NX_NB_NDEV_TXQ >= NDEV_NO_TXQ)
#error("Need to increase struct asr_txq->ndev_idx size")
#endif

/* stop netdev queue when number of queued buffers if greater than this  */
#define ASR_NDEV_FLOW_CTRL_STOP    100
/* restart netdev queue when number of queued buffers is lower than this */
#define ASR_NDEV_FLOW_CTRL_RESTART 50

#define TXQ_INACTIVE 0xffff
#if (NX_NB_TXQ >= TXQ_INACTIVE)
#error("Need to increase struct asr_txq->idx size")
#endif

#define NX_TXQ_INITIAL_CREDITS 4

/**
 * struct asr_hwq - Structure used to save information relative to
 *                   an AC TX queue (aka HW queue)
 * @list: List of TXQ, that have buffers ready for this HWQ
 * @credits: available credit for the queue (i.e. nb of buffers that
 *           can be pushed to FW )
 * @id Id of the HWQ among ASR_HWQ_....
 * @size size of the queue
 * @need_processing Indicate if hwq should be processed
 * @len number of packet ready to be pushed to fw for this HW queue
 * @len_stop threshold to stop mac80211(i.e. netdev) queues. Stop queue when
 *           driver has more than @len_stop packets ready.
 * @len_start threshold to wake mac8011 queues. Wake queue when driver has
 *            less than @len_start packets ready.
 */
struct asr_hwq {
	struct list_head list;
	u8 credits[CONFIG_USER_MAX];
	u8 size;
	u8 id;
};

/**
 * enum asr_push_flags - Flags of pushed buffer
 *
 * @ASR_PUSH_RETRY Pushing a buffer for retry
 * @ASR_PUSH_IMMEDIATE Pushing a buffer without queuing it first
 */
enum asr_push_flags {
	ASR_PUSH_RETRY = BIT(0),
	ASR_PUSH_IMMEDIATE = BIT(1),
};

/**
 * enum asr_txq_flags - TXQ status flag
 *
 * @ASR_TXQ_IN_HWQ_LIST The queue is scheduled for transmission
 * @ASR_TXQ_STOP_FULL No more credits for the queue
 * @ASR_TXQ_STOP_CSA CSA is in progress
 * @ASR_TXQ_STOP_STA_PS Destiniation sta is currently in power save mode
 * @ASR_TXQ_STOP_VIF_PS Vif owning this queue is currently in power save mode
 * @ASR_TXQ_STOP_CHAN Channel of this queue is not the current active channel
 * @ASR_TXQ_STOP_MU_POS TXQ is stopped waiting for all the buffers pushed to
 *                       fw to be confirmed
 * @ASR_TXQ_STOP All possible reason to have a txq stopped
 * @ASR_TXQ_NDEV_FLOW_CTRL associated netdev queue is currently stopped.
 *                          Note: when a TXQ is flowctrl it is NOT stopped
 */
enum asr_txq_flags {
	ASR_TXQ_IN_HWQ_LIST = BIT(0),
	ASR_TXQ_STOP_FULL = BIT(1),
	ASR_TXQ_STOP_CSA = BIT(2),
	ASR_TXQ_STOP_STA_PS = BIT(3),
	ASR_TXQ_STOP_VIF_PS = BIT(4),
	ASR_TXQ_STOP_CHAN = BIT(5),
#ifdef CONFIG_TWT
	ASR_TXQ_STOP_TWT = BIT(6),	// sta mode, txq is stopped when outside of TWT SP.
#else
	ASR_TXQ_STOP_MU_POS = BIT(6),	// MU not support.
#endif
	ASR_TXQ_STOP = (ASR_TXQ_STOP_FULL | ASR_TXQ_STOP_CSA |
			ASR_TXQ_STOP_STA_PS | ASR_TXQ_STOP_VIF_PS | ASR_TXQ_STOP_CHAN
#ifdef CONFIG_TWT
			| ASR_TXQ_STOP_TWT
#endif
	    ),
	ASR_TXQ_NDEV_FLOW_CTRL = BIT(7),
};

/**
 * struct asr_txq - Structure used to save information relative to
 *                   a RA/TID TX queue
 *
 * @idx: Unique txq idx. Set to TXQ_INACTIVE if txq is not used.
 * @status: bitfield of @asr_txq_flags.
 * @credits: available credit for the queue (i.e. nb of buffers that
 *           can be pushed to FW).
 * @pkt_sent: number of consecutive pkt sent without leaving HW queue list
 * @pkt_pushed: number of pkt currently pending for transmission confirmation
 * @sched_list: list node for HW queue schedule list (asr_hwq.list)
 * @sk_list: list of buffers to push to fw
 * @last_retry_skb: pointer on the last skb in @sk_list that is a retry.
 *                  (retry skb are stored at the beginning of the list)
 *                  NULL if no retry skb is queued in @sk_list
 * @nb_retry: Number of retry packet queued.
 * @hwq: Pointer on the associated HW queue.
 *
 * SOFTMAC specific:
 * @baw: Block Ack window information
 * @amsdu_anchor: pointer to asr_sw_txhdr of the first subframe of the A-MSDU.
 *                NULL if no A-MSDU frame is in construction
 * @amsdu_ht_len_cap:
 * @amsdu_vht_len_cap:
 * @tid:
 *
 * FULLMAC specific
 * @ps_id: Index to use for Power save mode (LEGACY or UAPSD)
 * @push_limit: number of packet to push before removing the txq from hwq list.
 *              (we always have push_limit < skb_queue_len(sk_list))
 * @ndev_idx: txq idx from netdev point of view (0xFF for non netdev queue)
 * @ndev: pointer to ndev of the corresponding vif
 * @amsdu: pointer to asr_sw_txhdr of the first subframe of the A-MSDU.
 *         NULL if no A-MSDU frame is in construction
 * @amsdu_len: Maximum size allowed for an A-MSDU. 0 means A-MSDU not allowed
 */
struct asr_txq {
	u16 idx;
	u8 status;
	s8 credits;
	u8 pkt_sent;
	u8 pkt_pushed[CONFIG_USER_MAX];
	struct list_head sched_list;
	struct sk_buff_head sk_list;
	struct sk_buff *last_retry_skb;
	struct asr_hwq *hwq;
	int nb_retry;
	struct asr_sta *sta;
	u8 ps_id;
	u8 push_limit;
	u16 ndev_idx;
	struct net_device *ndev;
};

struct asr_sta;
struct asr_vif;
struct asr_hw;
struct asr_sw_txhdr;

#define ASR_TXQ_GROUP_ID(txq) 0
#define ASR_TXQ_POS_ID(txq)   0

static inline bool asr_txq_is_stopped(struct asr_txq *txq)
{
	return (txq->status & ASR_TXQ_STOP);
}

static inline bool asr_txq_is_full(struct asr_txq *txq)
{
	return (txq->status & ASR_TXQ_STOP_FULL);
}

static inline bool asr_txq_is_scheduled(struct asr_txq *txq)
{
	return (txq->status & ASR_TXQ_IN_HWQ_LIST);
}

/*
 * if
 * - txq is not stopped
 * - hwq has credits
 * - there is no buffer queued
 * then a buffer can be immediately pushed without having to queue it first
 */
static inline bool asr_txq_is_ready_for_push(struct asr_txq *txq)
{
	return (!asr_txq_is_stopped(txq) &&
		txq->hwq->credits[ASR_TXQ_POS_ID(txq)] > 0 && skb_queue_empty(&txq->sk_list));
}

/**
 * foreach_sta_txq - Macro to iterate over all TXQ of a STA in increasing
 *                   TID order
 *
 * @sta: pointer to asr_sta
 * @txq: pointer to asr_txq updated with the next TXQ at each iteration
 * @tid: int updated with the TXQ tid at each iteration
 * @asr_hw: main driver data
 */

#define foreach_sta_txq(sta, txq, tid, asr_hw)                          \
    for (tid = 0, txq = asr_txq_sta_get(sta, 0, NULL,asr_hw);               \
         tid < (is_multicast_sta(asr_hw,sta->sta_idx) ? 1 : NX_NB_TXQ_PER_STA); \
         tid++, txq++)


/**
 * foreach_sta_txq_prio - Macro to iterate over all TXQ of a STA in
 *						  decreasing priority order
 *
 * @sta: pointer to asr_sta
 * @txq: pointer to asr_txq updated with the next TXQ at each iteration
 * @tid: int updated with the TXQ tid at each iteration
 * @i: int updated with ieration count
 * @asr_hw: main driver data
 *
 * Note: For fullmac txq for mgmt frame is skipped
 */
#define foreach_sta_txq_prio(sta, txq, tid, i, asr_hw)                          \
			for (i = 0, tid = nx_tid_prio[0], txq = asr_txq_sta_get(sta, tid,NULL, asr_hw); \
				 i < NX_NB_TID_PER_STA; 												 \
				 i++, tid = nx_tid_prio[i], txq = asr_txq_sta_get(sta, tid,NULL, asr_hw))


/**
 * extract the first @nb_elt of @list and append them to @head
 * It is assume that:
 * - @list contains more that @nb_elt
 * - There is no need to take @list nor @head lock to modify them
 */
static inline void skb_queue_extract(struct sk_buff_head *list, struct sk_buff_head *head, int nb_elt)
{
	int i;
	struct sk_buff *first, *last, *ptr;

	first = ptr = list->next;
	for (i = 0; i < nb_elt; i++) {
		ptr = ptr->next;
	}
	last = ptr->prev;

	/* unlink nb_elt in list */
	list->qlen -= nb_elt;
	list->next = ptr;
	ptr->prev = (struct sk_buff *)list;

	/* append nb_elt at end of head */
	head->qlen += nb_elt;
	last->next = (struct sk_buff *)head;
	head->prev->next = first;
	first->prev = head->prev;
	head->prev = last;
}

struct asr_txq *asr_txq_sta_get(struct asr_sta *sta, u8 tid, int *idx, struct asr_hw *asr_hw);
struct asr_txq *asr_txq_vif_get(struct asr_vif *vif, u8 type, int *idx);

/* return status bits related to the vif */
static inline u8 asr_txq_vif_get_status(struct asr_vif *asr_vif)
{
	struct asr_txq *txq = asr_txq_vif_get(asr_vif, 0, NULL);
	return (txq->status & (ASR_TXQ_STOP_CHAN | ASR_TXQ_STOP_VIF_PS));
}

void asr_txq_deinit(struct asr_hw *asr_hw, struct asr_txq *txq);
void asr_txq_vif_init(struct asr_hw *asr_hw, struct asr_vif *vif, u8 status);
void asr_txq_vif_deinit(struct asr_hw *asr_hw, struct asr_vif *vif);
void asr_txq_sta_init(struct asr_hw *asr_hw, struct asr_sta *asr_sta, u8 status);
void asr_txq_sta_deinit(struct asr_hw *asr_hw, struct asr_sta *asr_sta);
void asr_txq_offchan_init(struct asr_vif *asr_vif);
void asr_txq_offchan_deinit(struct asr_vif *asr_vif);

void asr_txq_add_to_hw_list(struct asr_txq *txq);
void asr_txq_del_from_hw_list(struct asr_txq *txq);
void asr_txq_stop(struct asr_txq *txq, u16 reason);
void asr_txq_start(struct asr_txq *txq, u16 reason);
void asr_txq_vif_start(struct asr_vif *vif, u16 reason, struct asr_hw *asr_hw);
void asr_txq_vif_stop(struct asr_vif *vif, u16 reason, struct asr_hw *asr_hw);

void asr_txq_sta_start(struct asr_sta *sta, u16 reason, struct asr_hw *asr_hw);
void asr_txq_sta_stop(struct asr_sta *sta, u16 reason, struct asr_hw *asr_hw);
void asr_txq_offchan_start(struct asr_hw *asr_hw);
void asr_txq_sta_switch_vif(struct asr_sta *sta, struct asr_vif *old_vif, struct asr_vif *new_vif);

void asr_hwq_init(struct asr_hw *asr_hw);
void asr_hwq_process(struct asr_hw *asr_hw, struct asr_hwq *hwq);
void asr_hwq_process_all(struct asr_hw *asr_hw);

#endif /* _ASR_TXQ_H_ */
