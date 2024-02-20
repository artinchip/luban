/**
 ****************************************************************************************
 *
 * @file asr_cmds.h
 *
 * @brief uwifi cmds header
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef _ASR_CMDS_H_
#define _ASR_CMDS_H_

#include <linux/spinlock.h>
#include <linux/completion.h>
#include "lmac_msg.h"
#include "ipc_shared.h"

#define ASR_80211_CMD_TIMEOUT_MS    500
#define ASR_80211_CMD_TIMEOUT_RETRY 4

#define ASR_CMD_FLAG_NONBLOCK      BIT(0)
#define ASR_CMD_FLAG_REQ_CFM       BIT(1)
#define ASR_CMD_FLAG_WAIT_PUSH     BIT(2)
#define ASR_CMD_FLAG_WAIT_ACK      BIT(3)
#define ASR_CMD_FLAG_WAIT_CFM      BIT(4)
#define ASR_CMD_FLAG_DONE          BIT(5)
/* ATM IPC design makes it possible to get the CFM before the ACK,
 * otherwise this could have simply been a state enum */
#define ASR_CMD_WAIT_COMPLETE(flags) \
    (!(flags & ASR_CMD_FLAG_WAIT_CFM))

#define ASR_CMD_MAX_QUEUED         8

struct asr_hw;
struct asr_cmd;
typedef int (*msg_cb_fct) (struct asr_hw * asr_hw, struct asr_cmd * cmd, struct ipc_e2a_msg * msg);

enum asr_cmd_mgr_state {
	ASR_CMD_MGR_STATE_DEINIT,
	ASR_CMD_MGR_STATE_INITED,
	ASR_CMD_MGR_STATE_CRASHED,
};

struct asr_cmd {
	struct list_head list;
	lmac_msg_id_t id;
	lmac_msg_id_t reqid;
	struct lmac_msg *a2e_msg;
	char *e2a_msg;
	u32 tkn;
	u16 flags;

	struct completion complete;
	u32 result;
};

struct asr_cmd_mgr {
	enum asr_cmd_mgr_state state;
	spinlock_t lock;
	u32 next_tkn;
	u32 queue_sz;
	u32 max_queue_sz;

	struct list_head cmds;

	int (*queue) (struct asr_cmd_mgr *, struct asr_cmd *);
	int (*msgind) (struct asr_cmd_mgr *, struct ipc_e2a_msg *, msg_cb_fct);
	void (*print) (struct asr_cmd_mgr *);
	void (*drain) (struct asr_cmd_mgr *);
};

void asr_cmd_mgr_init(struct asr_cmd_mgr *cmd_mgr);
void asr_cmd_mgr_deinit(struct asr_cmd_mgr *cmd_mgr);
void cmd_queue_crash_handle(struct asr_hw *asr_hw, const char *func, u32 line, u32 reason);

#endif /* _ASR_CMDS_H_ */
