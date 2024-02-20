/**
 ******************************************************************************
 *
 * @file asr_txq.c
 *
 * @brief txq related operation
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */

#include "asr_defs.h"
#include "asr_events.h"

/******************************************************************************
 * Utils inline functions
 *****************************************************************************/
const int nx_tid_prio[NX_NB_TID_PER_STA] = {7, 6, 5, 4, 3, 0, 2, 1};

static inline int asr_txq_vif_idx(struct asr_vif *vif, u8 type)
{
    return NX_FIRST_VIF_TXQ_IDX + master_vif_idx(vif) + (type * NX_VIRT_DEV_MAX);
}

struct asr_txq *asr_txq_sta_get(struct asr_sta *sta, u8 tid, int *idx, struct asr_hw *asr_hw)
{
	int id;

	if (tid >= NX_NB_TXQ_PER_STA)
		tid = 0;

	if (is_multicast_sta(asr_hw, sta->sta_idx))
		id = (asr_hw->sta_max_num * NX_NB_TXQ_PER_STA) + sta->vif_idx;
	else
		id = (sta->sta_idx * NX_NB_TXQ_PER_STA) + tid;

	//dev_info(asr_hw->dev, "%s:sta_idx=%d,tid=%d,id=%d\n", __func__, sta->sta_idx, tid, id);

	if (idx)
		*idx = id;

	return &asr_hw->txq[id];
}

struct asr_txq *asr_txq_vif_get(struct asr_vif *vif, u8 type, int *idx)
{
	int id;

	id = vif->asr_hw->sta_max_num * NX_NB_TXQ_PER_STA + master_vif_idx(vif) + (type * vif->asr_hw->vif_max_num);

	if (idx)
		*idx = id;

	return &vif->asr_hw->txq[id];
}

static inline struct asr_sta *asr_txq_2_sta(struct asr_txq *txq)
{
	return txq->sta;
}

/******************************************************************************
 * Init/Deinit functions
 *****************************************************************************/
/**
 * asr_txq_init - Initialize a TX queue
 *
 * @txq: TX queue to be initialized
 * @idx: TX queue index
 * @status: TX queue initial status
 * @hwq: Associated HW queue
 * @ndev: Net device this queue belongs to
 *        (may be null for non netdev txq)
 *
 * Each queue is initialized with the credit of @NX_TXQ_INITIAL_CREDITS.
 */
static void asr_txq_init(struct asr_hw *asr_hw, struct asr_txq *txq, int idx, u8 status,
			 struct asr_hwq *hwq, struct asr_sta *sta, struct net_device *ndev)
{
	int i;

	txq->idx = idx;
	txq->status = status;
	txq->credits = NX_TXQ_INITIAL_CREDITS;
	txq->pkt_sent = 0;
	skb_queue_head_init(&txq->sk_list);
	txq->last_retry_skb = NULL;
	txq->nb_retry = 0;
	txq->hwq = hwq;
	txq->sta = sta;
	for (i = 0; i < CONFIG_USER_MAX; i++)
		txq->pkt_pushed[i] = 0;

	txq->ps_id = LEGACY_PS_ID;
	txq->push_limit = 0;
	if (idx < asr_hw->sta_max_num * NX_NB_TXQ_PER_STA) {
		int sta_idx = sta->sta_idx;
		int tid = idx - (sta_idx * NX_NB_TXQ_PER_STA);
		if (tid < NX_NB_TID_PER_STA)
			txq->ndev_idx = NX_STA_NDEV_IDX(tid, sta_idx);
		else
			txq->ndev_idx = NDEV_NO_TXQ;
	} else if (idx < asr_hw->sta_max_num * NX_NB_TXQ_PER_STA + asr_hw->vif_max_num) {
		txq->ndev_idx = NX_NB_TID_PER_STA * asr_hw->sta_max_num;
	} else {
		txq->ndev_idx = NDEV_NO_TXQ;
	}
	txq->ndev = ndev;
}

/**
 * asr_txq_flush - Flush all buffers queued for a TXQ
 *
 * @asr_hw: main driver data
 * @txq: txq to flush
 */
void asr_txq_flush(struct asr_hw *asr_hw, struct asr_txq *txq)
{
	struct sk_buff *skb;
	while ((skb = skb_dequeue(&txq->sk_list)) != NULL) {
		dev_kfree_skb_any(skb);
	}
}

/**
 * asr_txq_deinit - De-initialize a TX queue
 *
 * @asr_hw: Driver main data
 * @txq: TX queue to be de-initialized
 * Any buffer stuck in a queue will be freed.
 */
void asr_txq_deinit(struct asr_hw *asr_hw, struct asr_txq *txq)
{
	spin_lock_bh(&asr_hw->tx_lock);
	asr_txq_del_from_hw_list(txq);
	txq->idx = TXQ_INACTIVE;
	spin_unlock_bh(&asr_hw->tx_lock);

	asr_txq_flush(asr_hw, txq);
}

/**
 * asr_txq_vif_init - Initialize all TXQ linked to a vif
 *
 * @asr_hw: main driver data
 * @asr_vif: Pointer on VIF
 * @status: Intial txq status
 *
 * Softmac : 1 VIF TXQ per HWQ
 *
 * Fullmac : 1 VIF TXQ for BC/MC
 *           1 VIF TXQ for MGMT to unknown STA
 */
void asr_txq_vif_init(struct asr_hw *asr_hw, struct asr_vif *asr_vif, u8 status)
{
	struct asr_txq *txq;
	int idx;

	txq = asr_txq_vif_get(asr_vif, NX_BCMC_TXQ_TYPE, &idx);
	asr_txq_init(asr_hw, txq, idx, status, &asr_hw->hwq[ASR_HWQ_BE],
		     &asr_hw->sta_table[asr_vif->ap.bcmc_index], asr_vif->ndev);

	txq = asr_txq_vif_get(asr_vif, NX_UNK_TXQ_TYPE, &idx);
	asr_txq_init(asr_hw, txq, idx, status, &asr_hw->hwq[ASR_HWQ_VO], NULL, asr_vif->ndev);
}

/**
 * asr_txq_vif_deinit - Deinitialize all TXQ linked to a vif
 *
 * @asr_hw: main driver data
 * @asr_vif: Pointer on VIF
 */
void asr_txq_vif_deinit(struct asr_hw *asr_hw, struct asr_vif *asr_vif)
{
	struct asr_txq *txq;

	txq = asr_txq_vif_get(asr_vif, NX_BCMC_TXQ_TYPE, NULL);
	asr_txq_deinit(asr_hw, txq);

	txq = asr_txq_vif_get(asr_vif, NX_UNK_TXQ_TYPE, NULL);
	asr_txq_deinit(asr_hw, txq);
}

/**
 * asr_txq_sta_init - Initialize TX queues for a STA
 *
 * @asr_hw: Main driver data
 * @asr_sta: STA for which tx queues need to be initialized
 * @status: Intial txq status
 *
 * This function initialize all the TXQ associated to a STA.
 * Softmac : 1 TXQ per TID
 *
 * Fullmac : 1 TXQ per TID (limited to 8)
 *           1 TXQ for MGMT
 */
void asr_txq_sta_init(struct asr_hw *asr_hw, struct asr_sta *asr_sta, u8 status)
{
	struct asr_txq *txq;
	int tid, idx;

	struct asr_vif *asr_vif = asr_hw->vif_table[asr_sta->vif_idx];

	txq = asr_txq_sta_get(asr_sta, 0, &idx, asr_hw);
	for (tid = 0; tid < NX_NB_TXQ_PER_STA; tid++, txq++, idx++) {
		asr_txq_init(asr_hw, txq, idx, status, &asr_hw->hwq[asr_tid2hwq[tid]], asr_sta, asr_vif->ndev);
		txq->ps_id = asr_sta->uapsd_tids & (1 << tid) ? UAPSD_ID : LEGACY_PS_ID;
	}
}

/**
 * asr_txq_sta_deinit - Deinitialize TX queues for a STA
 *
 * @asr_hw: Main driver data
 * @asr_sta: STA for which tx queues need to be deinitialized
 */
void asr_txq_sta_deinit(struct asr_hw *asr_hw, struct asr_sta *asr_sta)
{
	struct asr_txq *txq;
	int i;

	txq = asr_txq_sta_get(asr_sta, 0, NULL, asr_hw);

	for (i = 0; i < NX_NB_TXQ_PER_STA; i++, txq++) {
		asr_txq_deinit(asr_hw, txq);
	}
}

/**
 * asr_init_unk_txq - Initialize TX queue for the transmission on a offchannel
 *
 * @vif: Interface for which the queue has to be initialized
 *
 * NOTE: Offchannel txq is only active for the duration of the ROC
 */
void asr_txq_offchan_init(struct asr_vif *asr_vif)
{
	struct asr_hw *asr_hw = asr_vif->asr_hw;
	struct asr_txq *txq;

	txq = &asr_hw->txq[asr_hw->sta_max_num * NX_NB_TXQ_PER_STA + asr_hw->vif_max_num * NX_NB_TXQ_PER_VIF];
	asr_txq_init(asr_hw, txq, asr_hw->sta_max_num * NX_NB_TXQ_PER_STA +
		     asr_hw->vif_max_num * NX_NB_TXQ_PER_VIF, ASR_TXQ_STOP_CHAN, &asr_hw->hwq[ASR_HWQ_VO], NULL,
		     asr_vif->ndev);
}

/**
 * asr_deinit_offchan_txq - Deinitialize TX queue for offchannel
 *
 * @vif: Interface that manages the STA
 *
 * This function deintialize txq for one STA.
 * Any buffer stuck in a queue will be freed.
 */
void asr_txq_offchan_deinit(struct asr_vif *asr_vif)
{
	struct asr_txq *txq;

	txq = &asr_vif->asr_hw->txq[asr_vif->asr_hw->sta_max_num * NX_NB_TXQ_PER_STA +
				    asr_vif->asr_hw->vif_max_num * NX_NB_TXQ_PER_VIF];
	asr_txq_deinit(asr_vif->asr_hw, txq);
}

/******************************************************************************
 * Start/Stop functions
 *****************************************************************************/
/**
 * asr_txq_add_to_hw_list - Add TX queue to a HW queue schedule list.
 *
 * @txq: TX queue to add
 *
 * Add the TX queue if not already present in the HW queue list.
 * To be called with tx_lock hold
 */
void asr_txq_add_to_hw_list(struct asr_txq *txq)
{
	if (!(txq->status & ASR_TXQ_IN_HWQ_LIST)) {
		txq->status |= ASR_TXQ_IN_HWQ_LIST;
		list_add_tail(&txq->sched_list, &txq->hwq->list);
	}
}

/**
 * asr_txq_del_from_hw_list - Delete TX queue from a HW queue schedule list.
 *
 * @txq: TX queue to delete
 *
 * Remove the TX queue from the HW queue list if present.
 * To be called with tx_lock hold
 */
void asr_txq_del_from_hw_list(struct asr_txq *txq)
{
	if (txq->status & ASR_TXQ_IN_HWQ_LIST) {
		txq->status &= ~ASR_TXQ_IN_HWQ_LIST;
		list_del(&txq->sched_list);
	}
}

/**
 * asr_txq_start - Try to Start one TX queue
 *
 * @txq: TX queue to start
 * @reason: reason why the TX queue is started (among ASR_TXQ_STOP_xxx)
 *
 * Re-start the TX queue for one reason.
 * If after this the txq is no longer stopped and some buffers are ready,
 * the TX queue is also added to HW queue list.
 * To be called with tx_lock hold
 */
void asr_txq_start(struct asr_txq *txq, u16 reason)
{
	BUG_ON(txq == NULL);
	if (txq->idx != TXQ_INACTIVE && (txq->status & reason)) {
		//trace_txq_start(txq, reason);
		txq->status &= ~reason;
		if (!asr_txq_is_stopped(txq) && !skb_queue_empty(&txq->sk_list)) {
			asr_txq_add_to_hw_list(txq);
		}
	}
}

/**
 * asr_txq_stop - Stop one TX queue
 *
 * @txq: TX queue to stop
 * @reason: reason why the TX queue is stopped (among ASR_TXQ_STOP_xxx)
 *
 * Stop the TX queue. It will remove the TX queue from HW queue list
 * To be called with tx_lock hold
 */
void asr_txq_stop(struct asr_txq *txq, u16 reason)
{
	BUG_ON(txq == NULL);
	if (txq->idx != TXQ_INACTIVE) {
		//trace_txq_stop(txq, reason);
		txq->status |= reason;
		asr_txq_del_from_hw_list(txq);
	}
}

/**
 * asr_txq_sta_start - Start all the TX queue linked to a STA
 *
 * @sta: STA whose TX queues must be re-started
 * @reason: Reason why the TX queue are restarted (among ASR_TXQ_STOP_xxx)
 * @asr_hw: Driver main data
 *
 * This function will re-start all the TX queues of the STA for the reason
 * specified. It can be :
 * - ASR_TXQ_STOP_STA_PS: the STA is no longer in power save mode
 * - ASR_TXQ_STOP_VIF_PS: the VIF is in power save mode (p2p absence)
 * - ASR_TXQ_STOP_CHAN: the STA's VIF is now on the current active channel
 *
 * Any TX queue with buffer ready and not Stopped for other reasons, will be
 * added to the HW queue list
 * To be called with tx_lock hold
 */
void asr_txq_sta_start(struct asr_sta *asr_sta, u16 reason, struct asr_hw *asr_hw)
{
	struct asr_txq *txq;
	int i;
	int nb_txq;

	if (asr_sta == NULL) {
		dev_err(asr_hw->dev, "%s:error asr_sta is null\n", __func__);
		return;
	}
	//trace_txq_sta_start(asr_sta->sta_idx);

	txq = asr_txq_sta_get(asr_sta, 0, NULL, asr_hw);

	if (is_multicast_sta(asr_hw, asr_sta->sta_idx))
		nb_txq = 1;
	else
		nb_txq = NX_NB_TXQ_PER_STA;

	for (i = 0; i < nb_txq; i++, txq++)
		asr_txq_start(txq, reason);
}

/**
 * asr_stop_sta_txq - Stop all the TX queue linked to a STA
 *
 * @sta: STA whose TX queues must be stopped
 * @reason: Reason why the TX queue are stopped (among ASR_TX_STOP_xxx)
 * @asr_hw: Driver main data
 *
 * This function will stop all the TX queues of the STA for the reason
 * specified. It can be :
 * - ASR_TXQ_STOP_STA_PS: the STA is in power save mode
 * - ASR_TXQ_STOP_VIF_PS: the VIF is in power save mode (p2p absence)
 * - ASR_TXQ_STOP_CHAN: the STA's VIF is not on the current active channel
 *
 * Any TX queue present in a HW queue list will be removed from this list.
 * To be called with tx_lock hold
 */
void asr_txq_sta_stop(struct asr_sta *asr_sta, u16 reason, struct asr_hw *asr_hw)
{
	struct asr_txq *txq;
	int i;
	int nb_txq;

	if (!asr_sta)
		return;

	//trace_txq_sta_stop(asr_sta->sta_idx);

	txq = asr_txq_sta_get(asr_sta, 0, NULL, asr_hw);

	if (is_multicast_sta(asr_hw, asr_sta->sta_idx))
		nb_txq = 1;
	else
		nb_txq = NX_NB_TXQ_PER_STA;

	for (i = 0; i < nb_txq; i++, txq++)
		asr_txq_stop(txq, reason);
}

static inline
    void asr_txq_vif_for_each_sta(struct asr_hw *asr_hw,
				  struct asr_vif *asr_vif,
				  void (*f) (struct asr_sta *, u16, struct asr_hw *), u16 reason)
{

	switch (ASR_VIF_TYPE(asr_vif)) {
	case NL80211_IFTYPE_STATION:
	case NL80211_IFTYPE_P2P_CLIENT:
		{
			f(asr_vif->sta.ap, reason, asr_hw);
			break;
		}
	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_P2P_GO:
		{
			struct asr_sta *sta;
			list_for_each_entry(sta, &asr_vif->ap.sta_list, list) {
				f(sta, reason, asr_hw);
			}
			break;
		}
	default:
		BUG();
		break;
	}
}

/**
 * asr_txq_vif_start - START TX queues of all STA associated to the vif
 *                      and vif's TXQ
 *
 * @vif: Interface to start
 * @reason: Start reason (ASR_TXQ_STOP_CHAN or ASR_TXQ_STOP_VIF_PS)
 * @asr_hw: Driver main data
 *
 * Iterate over all the STA associated to the vif and re-start them for the
 * reason @reason
 * Take tx_lock
 */

bool check_vif_block_flags(struct asr_vif *asr_vif)
{
    if (NULL == asr_vif)
        return true;
    else
    return (test_bit(ASR_DEV_STA_OUT_TWTSP,   &asr_vif->dev_flags) ||
	       test_bit(ASR_DEV_TXQ_STOP_CSA,    &asr_vif->dev_flags) ||
	       test_bit(ASR_DEV_TXQ_STOP_VIF_PS, &asr_vif->dev_flags) ||
	       test_bit(ASR_DEV_TXQ_STOP_CHAN,   &asr_vif->dev_flags));
}

extern bool asr_main_process_running(struct asr_hw * asr_hw);

void asr_txq_vif_start(struct asr_vif *asr_vif, u16 reason, struct asr_hw *asr_hw)
{
	struct asr_txq *txq;

	spin_lock_bh(&asr_hw->tx_lock);

    // sdio mode not used.
	asr_txq_vif_for_each_sta(asr_hw, asr_vif, asr_txq_sta_start, reason);
	txq = asr_txq_vif_get(asr_vif, NX_BCMC_TXQ_TYPE, NULL);
	asr_txq_start(txq, reason);
	txq = asr_txq_vif_get(asr_vif, NX_UNK_TXQ_TYPE, NULL);
	asr_txq_start(txq, reason);

	// sdio mode used: clr drv flag for tx task.
#ifdef CONFIG_TWT
    if (ASR_TXQ_STOP_TWT == reason) {
	    clear_bit(ASR_DEV_STA_OUT_TWTSP,&asr_vif->dev_flags);
	}
#endif
    if (ASR_TXQ_STOP_CSA == reason) {
	    clear_bit(ASR_DEV_TXQ_STOP_CSA,&asr_vif->dev_flags);
	}

    if (ASR_TXQ_STOP_CHAN == reason) {
	    clear_bit(ASR_DEV_TXQ_STOP_CHAN,&asr_vif->dev_flags);
	}

    if (ASR_TXQ_STOP_VIF_PS == reason) {
	    clear_bit(ASR_DEV_TXQ_STOP_VIF_PS,&asr_vif->dev_flags);
	}

	spin_unlock_bh(&asr_hw->tx_lock);
#ifdef CONFIG_ASR_SDIO
    // add tx task trigger.
    if ( (check_vif_block_flags(asr_vif) == false) ) {
		if (!asr_main_process_running(asr_hw)) {
			set_bit(ASR_FLAG_MAIN_TASK_BIT, &asr_hw->ulFlag);
			wake_up_interruptible(&asr_hw->waitq_main_task_thead);

            // add log.
			//netdev_err(asr_vif->ndev, "[vif%d][%d] tri tx task \r\n",asr_vif->vif_index, asr_vif->wdev.iftype);
		}
    }
#endif
}

/**
 * asr_txq_vif_stop - STOP TX queues of all STA associated to the vif
 *
 * @vif: Interface to stop
 * @arg: Stop reason (ASR_TXQ_STOP_CHAN or ASR_TXQ_STOP_VIF_PS)
 * @asr_hw: Driver main data
 *
 * Iterate over all the STA associated to the vif and stop them for the
 * reason ASR_TXQ_STOP_CHAN or ASR_TXQ_STOP_VIF_PS
 * Take tx_lock
 */
void asr_txq_vif_stop(struct asr_vif *asr_vif, u16 reason, struct asr_hw *asr_hw)
{
	struct asr_txq *txq;

	//trace_txq_vif_stop(asr_vif->vif_index);

	spin_lock_bh(&asr_hw->tx_lock);

    // sdio mode not used.
	asr_txq_vif_for_each_sta(asr_hw, asr_vif, asr_txq_sta_stop, reason);
	txq = asr_txq_vif_get(asr_vif, NX_BCMC_TXQ_TYPE, NULL);
	asr_txq_stop(txq, reason);
	txq = asr_txq_vif_get(asr_vif, NX_UNK_TXQ_TYPE, NULL);
	asr_txq_stop(txq, reason);

	// sdio mode used: set drv flag for tx task stop.
#ifdef CONFIG_TWT
    if (ASR_TXQ_STOP_TWT == reason) {
	    set_bit(ASR_DEV_STA_OUT_TWTSP,&asr_vif->dev_flags);
	}
#endif
    if (ASR_TXQ_STOP_CSA == reason) {
	    set_bit(ASR_DEV_TXQ_STOP_CSA,&asr_vif->dev_flags);
	}

    if (ASR_TXQ_STOP_CHAN == reason) {
	    set_bit(ASR_DEV_TXQ_STOP_CHAN,&asr_vif->dev_flags);
	}

    if (ASR_TXQ_STOP_VIF_PS == reason) {
	    set_bit(ASR_DEV_TXQ_STOP_VIF_PS,&asr_vif->dev_flags);
	}

	spin_unlock_bh(&asr_hw->tx_lock);
}

/**
 * asr_start_offchan_txq - START TX queue for offchannel frame
 *
 * @asr_hw: Driver main data
 */
void asr_txq_offchan_start(struct asr_hw *asr_hw)
{
	struct asr_txq *txq;

	txq = &asr_hw->txq[asr_hw->sta_max_num * NX_NB_TXQ_PER_STA + asr_hw->vif_max_num * NX_NB_TXQ_PER_VIF];
	spin_lock_bh(&asr_hw->tx_lock);
	asr_txq_start(txq, ASR_TXQ_STOP_CHAN);
	spin_unlock_bh(&asr_hw->tx_lock);
}

/**
 * asr_switch_vif_sta_txq - Associate TXQ linked to a STA to a new vif
 *
 * @sta: STA whose txq must be switched
 * @old_vif: Vif currently associated to the STA (may no longer be active)
 * @new_vif: vif which should be associated to the STA for now on
 *
 * This function will switch the vif (i.e. the netdev) associated to all STA's
 * TXQ. This is used when AP_VLAN interface are created.
 * If one STA is associated to an AP_vlan vif, it will be moved from the master
 * AP vif to the AP_vlan vif.
 * If an AP_vlan vif is removed, then STA will be moved back to mastert AP vif.
 *
 */
void asr_txq_sta_switch_vif(struct asr_sta *sta, struct asr_vif *old_vif, struct asr_vif *new_vif)
{
	struct asr_hw *asr_hw = new_vif->asr_hw;
	struct asr_txq *txq;
	int i;

	/* start TXQ on the new interface, and update ndev field in txq */
	if (!netif_carrier_ok(new_vif->ndev))
		netif_carrier_on(new_vif->ndev);
	txq = asr_txq_sta_get(sta, 0, NULL, asr_hw);
	for (i = 0; i < NX_NB_TID_PER_STA; i++, txq++) {
		txq->ndev = new_vif->ndev;
		netif_wake_subqueue(txq->ndev, txq->ndev_idx);
	}
}

/******************************************************************************
 * HWQ processing
 *****************************************************************************/

static inline s8 asr_txq_get_credits(struct asr_txq *txq)
{
	s8 cred = txq->credits;
	/* if destination is in PS mode, push_limit indicates the maximum
	   number of packet that can be pushed on this txq. */
	if (txq->push_limit && (cred > txq->push_limit)) {
		cred = txq->push_limit;
	}
	return cred;
}

/**
 * asr_hwq_init - Initialize all hwq structures
 *
 * @asr_hw: Driver main data
 *
 */
void asr_hwq_init(struct asr_hw *asr_hw)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(asr_hw->hwq); i++) {
		struct asr_hwq *hwq = &asr_hw->hwq[i];
		hwq->id = i;
		hwq->size = nx_txdesc_cnt[i];
		INIT_LIST_HEAD(&hwq->list);
	}
}
