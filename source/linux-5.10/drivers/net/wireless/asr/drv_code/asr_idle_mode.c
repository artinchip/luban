/**
 ******************************************************************************
 *
 * @file asr_idle_mode.c
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */
#if (defined CFG_SNIFFER_SUPPORT || defined CFG_CUS_FRAME)
#include "asr_idle_mode.h"

struct asr_txq *asr_txq_vif_get_idle_mode(struct asr_hw *asr_hw, uint8_t ac, int *idx)
{
	if (idx)
		*idx = ac;

	return &asr_hw->txq[ac];
}

void asr_txq_init_idle_mode(struct asr_txq *txq, int idx, uint8_t status, struct asr_hwq *hwq, int tid)
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
	for (i = 0; i < CONFIG_USER_MAX; i++)
		txq->pkt_pushed[i] = 0;

	//txq->baw.agg_on = false;
}

/* called after idle mode add interface */
void asr_txq_vif_init_idle_mode(struct asr_hw *asr_hw, uint8_t status)
{
	int i;
	int idx;
	struct asr_txq *txq;
	txq = asr_txq_vif_get_idle_mode(asr_hw, 0, &idx);
	for (i = 0; i < NX_NB_TXQ_MAX_IDLE_MODE; i++, idx++, txq++) {
		asr_txq_init_idle_mode(txq, idx, status, &asr_hw->hwq[i], -1);
	}
}

/**
 * asr_txq_sta_deinit - Deinitialize TX queues for a STA
 *
 * @asr_hw: Main driver data
 * @asr_sta: STA for which tx queues need to be deinitialized
 */
void asr_txq_vif_deinit_idle_mode(struct asr_hw *asr_hw, struct asr_vif *asr_vif)
{
	int i;
	int idx;
	struct asr_txq *txq;
	txq = asr_txq_vif_get_idle_mode(asr_hw, 0, &idx);

	for (i = 0; i < NX_NB_TXQ_MAX_IDLE_MODE; i++, txq++) {
		asr_txq_deinit(asr_hw, txq);
	}
}

/*start after channel switch*/
void asr_txq_vif_start_idle_mode(struct asr_vif *asr_vif, u16 reason, struct asr_hw *asr_hw)
{
	struct asr_txq *txq;
	int i;

	spin_lock_bh(&asr_hw->tx_lock);

	txq = asr_txq_vif_get_idle_mode(asr_hw, 0, NULL);
	for (i = 0; i < NX_NB_TXQ_MAX_IDLE_MODE; i++, txq++) {
		asr_txq_start(txq, reason);
	}

	spin_unlock_bh(&asr_hw->tx_lock);
}

/*stop before channel switch*/
void asr_txq_vif_stop_idle_mode(struct asr_vif *asr_vif, uint16_t reason, struct asr_hw *asr_hw)
{
	struct asr_txq *txq;
	int i;

	spin_lock_bh(&asr_hw->tx_lock);

	txq = asr_txq_vif_get_idle_mode(asr_hw, 0, NULL);
	for (i = 0; i < NX_NB_TXQ_MAX_IDLE_MODE; i++, txq++) {
		asr_txq_stop(txq, reason);
	}

	spin_unlock_bh(&asr_hw->tx_lock);
}

/**
 * netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb,
 *                               struct net_device *dev);
 *    Called when a packet needs to be transmitted.
 *    Must return NETDEV_TX_OK , NETDEV_TX_BUSY.
 *        (can also return NETDEV_TX_LOCKED if NETIF_F_LLTX)
 *
 *  - Initialize the desciptor for this pkt (stored in skb before data)
 *  - Push the pkt in the corresponding Txq
 *  - If possible (i.e. credit available and not in PS) the pkt is pushed
 *    to fw
 */
void asr_start_xmit_idle(uint8_t * frame, uint32_t len)
{

}

void asr_tx_dev_custom_mgmtframe(uint8_t * pframe, uint32_t len)
{
	asr_start_xmit_idle(pframe, len);
}
#endif
