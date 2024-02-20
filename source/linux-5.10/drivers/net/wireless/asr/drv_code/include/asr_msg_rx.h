/**
 ****************************************************************************************
 *
 * @file asr_msg_rx.h
 *
 * @brief RX function declarations
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef _ASR_MSG_RX_H_
#define _ASR_MSG_RX_H_

int asr_rx_sm_disconnect_ind(struct asr_hw *asr_hw, struct asr_cmd *cmd, struct ipc_e2a_msg *msg);
void asr_rx_handle_msg(struct asr_hw *asr_hw, struct ipc_e2a_msg *msg);

#endif /* _ASR_MSG_RX_H_ */
