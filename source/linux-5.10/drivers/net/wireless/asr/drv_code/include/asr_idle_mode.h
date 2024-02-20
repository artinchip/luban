/**
 ******************************************************************************
 *
 * @file asr_idle_mode.h
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */
#ifndef _ASR_IDLE_MODE_H_
#define _ASR_IDLE_MODE_H_

#include "asr_defs.h"
#include "asr_txq.h"

#define  CUS_MGMT_FRAME  300

#define NX_NB_TXQ_MAX_IDLE_MODE NX_TXQ_CNT

void asr_tx_dev_custom_mgmtframe(uint8_t * pframe, uint32_t len);

void asr_txq_vif_init_idle_mode(struct asr_hw *asr_hw, uint8_t status);
void asr_txq_vif_deinit_idle_mode(struct asr_hw *asr_hw, struct asr_vif *asr_vif);
void asr_txq_vif_start_idle_mode(struct asr_vif *asr_vif, uint16_t reason, struct asr_hw *asr_hw);
void asr_txq_vif_stop_idle_mode(struct asr_vif *asr_vif, uint16_t reason, struct asr_hw *asr_hw);

struct asr_txq *asr_txq_vif_get_idle_mode(struct asr_hw *asr_hw, uint8_t ac, int *idx);

#endif /* _ASR_IDLE_MODE_H_ */
