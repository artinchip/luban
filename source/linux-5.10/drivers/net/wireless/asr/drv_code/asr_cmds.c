/**
 ****************************************************************************************
 *
 * @file asr_cmds.c
 *
 * @brief Handles commands from LMAC FW
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <linux/list.h>

#include "asr_cmds.h"
#include "asr_defs.h"
#include "asr_msg_tx.h"
#include "asr_strs.h"
#include "asr_events.h"
#include "ipc_host.h"
#include "asr_irqs.h"

static void cmd_mgr_drain(struct asr_cmd_mgr *cmd_mgr);

/**
 *
 */
static void cmd_dump(const struct asr_cmd *cmd)
{
	dev_err(g_asr_para.dev, "tkn[%d]  flags:%04x  result:%3d  cmd:%4d-%-24s - reqcfm(%4d-%-s)\n",
		cmd->tkn, cmd->flags, cmd->result, cmd->id, ASR_ID2STR(cmd->id),
		cmd->reqid, cmd->reqid != (lmac_msg_id_t) - 1 ? ASR_ID2STR(cmd->reqid) : "none");
}

/**
 *
 */
static void cmd_complete(struct asr_cmd_mgr *cmd_mgr, struct asr_cmd *cmd)
{
	lockdep_assert_held(&cmd_mgr->lock);

	cmd->flags |= ASR_CMD_FLAG_DONE;

	if (ASR_CMD_WAIT_COMPLETE(cmd->flags)) {
		cmd->result = 0;
		complete(&cmd->complete);
	}

}

void cmd_queue_crash_handle(struct asr_hw *asr_hw, const char *func, u32 line, u32 reason)
{
	struct asr_cmd_mgr *cmd_mgr = NULL;

	if (asr_hw == NULL) {
		return;
	}

	if (test_bit(ASR_DEV_PRE_RESTARTING, &asr_hw->phy_flags) || test_bit(ASR_DEV_RESTARTING, &asr_hw->phy_flags)) {
		dev_info(asr_hw->dev, "%s:phy_flags=0X%X\n", __func__, (unsigned int)asr_hw->phy_flags);
		return;
	}

	set_bit(ASR_DEV_PRE_RESTARTING, &asr_hw->phy_flags);

	cmd_mgr = &asr_hw->cmd_mgr;

#ifdef CONFIG_ASR_SDIO
	dump_sdio_info(asr_hw, func, line);
#endif

	spin_lock_bh(&cmd_mgr->lock);
	cmd_mgr->state = ASR_CMD_MGR_STATE_CRASHED;
	spin_unlock_bh(&cmd_mgr->lock);

	//send dev restart event
	msleep(10);
	cmd_mgr_drain(cmd_mgr);

#ifdef CONFIG_ASR_SDIO
#ifdef ASR_MODULE_RESET_SUPPORT
	asr_hw->dev_restart_work.asr_hw = asr_hw;
	asr_hw->dev_restart_work.parm1 = reason;
	schedule_work(&asr_hw->dev_restart_work.real_work);
#endif
#endif
}

/**
 *
 */
static int cmd_mgr_queue(struct asr_cmd_mgr *cmd_mgr, struct asr_cmd *cmd)
{
	struct asr_hw *asr_hw = container_of(cmd_mgr, struct asr_hw, cmd_mgr);
	unsigned long tout;
	int ret = 0;
	u32 rx_retry = 0;
	struct asr_cmd *cur = NULL, *nxt = NULL;

	ASR_DBG(ASR_FN_ENTRY_STR);

	spin_lock_bh(&cmd_mgr->lock);

	if (cmd_mgr->state == ASR_CMD_MGR_STATE_CRASHED) {
		dev_err(asr_hw->dev, "cmd queue crashed\n");
		cmd->result = -EPIPE;
		//kfree(cmd->a2e_msg);
		spin_unlock_bh(&cmd_mgr->lock);
		return -EPIPE;
	}

	if (!list_empty(&cmd_mgr->cmds)) {
		if (cmd_mgr->queue_sz == cmd_mgr->max_queue_sz) {
			dev_err(asr_hw->dev, "Too many cmds (%d) already queued\n", cmd_mgr->max_queue_sz);
			cmd->result = -ENOMEM;
			//kfree(cmd->a2e_msg);
			spin_unlock_bh(&cmd_mgr->lock);
			return -ENOMEM;
		}
	}

	if (cmd->flags & ASR_CMD_FLAG_REQ_CFM)
		cmd->flags |= ASR_CMD_FLAG_WAIT_CFM;

	cmd->tkn = cmd_mgr->next_tkn++;
	cmd->result = -EINTR;

	if (!(cmd->flags & ASR_CMD_FLAG_NONBLOCK))	//block case
		init_completion(&cmd->complete);

	/* first use origin msg arch, use cmd_mgr->lock to protect different thread access
	 * if later find bug, may be add msg to queue,trigger another main thread to process msg
	 */
	list_add_tail(&cmd->list, &cmd_mgr->cmds);
	cmd_mgr->queue_sz++;
	tout = msecs_to_jiffies(ASR_80211_CMD_TIMEOUT_MS);
	spin_unlock_bh(&cmd_mgr->lock);
	//dev_err(asr_hw->dev, "[%s %d] \n",__func__,__LINE__);
	/* send cmd to FW, SDIO
	   if error, return and release msg and cmd
	 */
	ret = ipc_host_msg_push(asr_hw->ipc_env, cmd, sizeof(struct lmac_msg) + cmd->a2e_msg->param_len);
	//kfree(cmd->a2e_msg);

	if (ret)		//has error
	{
		spin_lock_bh(&cmd_mgr->lock);
		list_for_each_entry_safe(cur, nxt, &cmd_mgr->cmds, list) {
			if (cmd == cur) {
				list_del(&cur->list);
				cmd_mgr->queue_sz--;
			}
		}
		spin_unlock_bh(&cmd_mgr->lock);
		dev_err(asr_hw->dev, "[%s %d] ipc_host_msg_push ret=%d\n", __func__, __LINE__, ret);
		//dump_stack();
#ifdef CONFIG_ASR_SDIO
		if (ret != -ECANCELED)
			cmd_queue_crash_handle(asr_hw, __func__, __LINE__, ASR_RESTART_REASON_TXMSG_FAIL);
#endif

		return -EBUSY;
	}
	//dev_err(asr_hw->dev, "[%s %d] \n",__func__,__LINE__);

	if (!(cmd->flags & ASR_CMD_FLAG_NONBLOCK)) {	//block case
		rx_retry = ASR_80211_CMD_TIMEOUT_RETRY;

		while (rx_retry--) {

			if (!wait_for_completion_timeout(&cmd->complete, tout)) {

				if (rx_retry) {

					tout = msecs_to_jiffies(ASR_80211_CMD_TIMEOUT_MS);

					dev_err(asr_hw->dev,
						"%s: rx msg retry(%d),(%d,%d)->(%d,%d)\n",
						__func__, rx_retry, MSG_T(cmd->id), MSG_I(cmd->id)
						, MSG_T(cmd->reqid), MSG_I(cmd->reqid));

#ifdef CONFIG_ASR_SDIO
					queue_work(asr_hw->asr_wq, &asr_hw->datawork);
#endif

				} else {

					dev_err(asr_hw->dev, "%s: cmd timed-out\n", __func__);

					cmd_dump(cmd);

					spin_lock_bh(&cmd_mgr->lock);
					list_for_each_entry_safe(cur, nxt, &cmd_mgr->cmds, list) {
						if (cmd == cur) {
							list_del(&cur->list);
							cmd_mgr->queue_sz--;
						}
					}
					spin_unlock_bh(&cmd_mgr->lock);
#ifdef CONFIG_ASR_SDIO
					cmd_queue_crash_handle(asr_hw, __func__, __LINE__, ASR_RESTART_REASON_CMD_CRASH);
#endif

					return -ETIMEDOUT;
				}

			} else {
				break;
			}
		}

	} else {
		cmd->result = 0;
	}

	spin_lock_bh(&cmd_mgr->lock);
	list_for_each_entry_safe(cur, nxt, &cmd_mgr->cmds, list) {
		if (cmd == cur) {
			list_del(&cur->list);
			cmd_mgr->queue_sz--;
		}
	}
	spin_unlock_bh(&cmd_mgr->lock);

	if (cmd_mgr->state == ASR_CMD_MGR_STATE_CRASHED) {
		return -ETIMEDOUT;
	}
	//dev_err(asr_hw->dev, "[%s %d] \n",__func__,__LINE__);
	return 0;
}
static int cmd_mgr_queue_sync(struct asr_cmd_mgr *cmd_mgr, struct asr_cmd *cmd)
{
	struct asr_hw *asr_hw = container_of(cmd_mgr, struct asr_hw, cmd_mgr);
	int ret;

	mutex_lock(&asr_hw->tx_msg_mutex);
	ret = cmd_mgr_queue(cmd_mgr, cmd);
	mutex_unlock(&asr_hw->tx_msg_mutex);

	return ret;
}

static int cmd_mgr_run_callback(struct asr_hw *asr_hw, struct asr_cmd *cmd, struct ipc_e2a_msg *msg, msg_cb_fct cb)
{
	int res;

	if (!cb)
		return 0;

	spin_lock(&asr_hw->cb_lock);
	res = cb(asr_hw, cmd, msg);
	spin_unlock(&asr_hw->cb_lock);

	return res;
}

/**
 *
 */
static int cmd_mgr_msgind(struct asr_cmd_mgr *cmd_mgr, struct ipc_e2a_msg *msg, msg_cb_fct cb)
{
	struct asr_hw *asr_hw = container_of(cmd_mgr, struct asr_hw, cmd_mgr);
	struct asr_cmd *cmd;
	bool found = false;

	ASR_DBG(ASR_FN_ENTRY_STR);

	spin_lock(&cmd_mgr->lock);
	list_for_each_entry(cmd, &cmd_mgr->cmds, list) {
		if (cmd->reqid == msg->id && (cmd->flags & ASR_CMD_FLAG_WAIT_CFM)) {

			if (!cmd_mgr_run_callback(asr_hw, cmd, msg, cb)) {
				found = true;
				cmd->flags &= ~ASR_CMD_FLAG_WAIT_CFM;

				if (cmd->e2a_msg && msg->param_len)
					memcpy(cmd->e2a_msg, &msg->param, msg->param_len);

				if (ASR_CMD_WAIT_COMPLETE(cmd->flags))
					cmd_complete(cmd_mgr, cmd);

				break;
			}
		}
	}
	spin_unlock(&cmd_mgr->lock);

	if (!found)
		cmd_mgr_run_callback(asr_hw, NULL, msg, cb);

	return 0;
}

/**
 *
 */
static void cmd_mgr_print(struct asr_cmd_mgr *cmd_mgr)
{
	struct asr_cmd *cur;

	spin_lock_bh(&cmd_mgr->lock);
	asr_dbg(INFO, "q_sz/max: %2d / %2d - next tkn: %d\n",
		cmd_mgr->queue_sz, cmd_mgr->max_queue_sz, cmd_mgr->next_tkn);
	list_for_each_entry(cur, &cmd_mgr->cmds, list) {
		cmd_dump(cur);
	}
	spin_unlock_bh(&cmd_mgr->lock);
}

/**
 *
 */
static void cmd_mgr_drain(struct asr_cmd_mgr *cmd_mgr)
{
	struct asr_cmd *cur, *nxt;

	ASR_DBG(ASR_FN_ENTRY_STR);

	spin_lock_bh(&cmd_mgr->lock);
	list_for_each_entry_safe(cur, nxt, &cmd_mgr->cmds, list) {
		list_del(&cur->list);
		cmd_mgr->queue_sz--;
		if (!(cur->flags & ASR_CMD_FLAG_NONBLOCK))	//block case
			complete(&cur->complete);
	}
	spin_unlock_bh(&cmd_mgr->lock);
}

/**
 *
 */
void asr_cmd_mgr_init(struct asr_cmd_mgr *cmd_mgr)
{
	ASR_DBG(ASR_FN_ENTRY_STR);

	INIT_LIST_HEAD(&cmd_mgr->cmds);
	spin_lock_init(&cmd_mgr->lock);
	cmd_mgr->max_queue_sz = ASR_CMD_MAX_QUEUED;
	cmd_mgr->queue = &cmd_mgr_queue_sync;	//asr_send_msg
	cmd_mgr->print = &cmd_mgr_print;
	cmd_mgr->drain = &cmd_mgr_drain;
	cmd_mgr->msgind = &cmd_mgr_msgind;

	dev_info(g_asr_para.dev, "%s\n", __func__);
}

/**
 *
 */
void asr_cmd_mgr_deinit(struct asr_cmd_mgr *cmd_mgr)
{
	cmd_mgr->print(cmd_mgr);
	cmd_mgr->drain(cmd_mgr);
	cmd_mgr->print(cmd_mgr);
	memset(cmd_mgr, 0, sizeof(*cmd_mgr));

	dev_info(g_asr_para.dev, "%s\n", __func__);
}
