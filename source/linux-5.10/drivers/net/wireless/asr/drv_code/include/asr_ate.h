/**
 ****************************************************************************************
 *
 * @file asr_ate.h
 *
 * @brief RX function declarations
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef _ASR_ATE_H_
#define _ASR_ATE_H_

#define ASR_ATE_BUFF_NUM     5
#define ASR_ATE_BUFF_SIZE    512
#define ASR_ATE_MSG_TIMEOUT  20000

struct asr_ate_info {
	struct sk_buff_head rx_msg_empty_list;
	struct sk_buff_head rx_msg_list;
	struct completion rx_msg_complete;
	struct mutex tx_msg_mutex;
	spinlock_t lock;
	bool send_cmd_flag;
};

enum ate_msg_type {
	ATE_TYPE_MSG = 0x8a,
};

void asr_rx_ate_handle(struct asr_hw *asr_hw, struct ipc_e2a_msg *msg);
int ate_data_direct_tx_rx(struct asr_hw *asr_hw, unsigned char *data);
#ifdef ASR_WIFI_CONFIG_SUPPORT
void ate_sync_txpwr_rate_config(struct asr_hw *asr_hw);
#endif
int asr_ate_init(struct asr_hw *asr_hw);
int asr_ate_deinit(struct asr_hw *asr_hw);

#endif /* _ASR_ATE_H_ */
