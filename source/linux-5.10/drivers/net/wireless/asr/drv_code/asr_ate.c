/**
 ****************************************************************************************
 *
 * @file asr_ate.c
 *
 * @brief RX function definitions
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ****************************************************************************************
 */
#include <linux/version.h>
#include "asr_defs.h"
#include "asr_sdio.h"
#include "asr_ate.h"

//===========================define param start================================

extern int _atoi(char *s, int base);

#ifdef ASR_WIFI_CONFIG_SUPPORT
extern struct asr_wifi_config g_wifi_config;
#endif
//===========================define param end================================

//===========================global param start================================
static struct asr_ate_info g_ate_info;

#ifdef ASR_WIFI_CONFIG_SUPPORT
char *rate_buff[WIFI_CONFIG_MAX_PWR_NUM] = {
	"1M",
	"2M",
	"5.5M",
	"11M",
	"6M",
	"9M",
	"12M",
	"18M",
	"24M",
	"36M",
	"48M",
	"54M",
	"MCS0",
	"MCS1",
	"MCS2",
	"MCS3",
	"MCS4",
	"MCS5",
	"MCS6",
	"MCS7",
	"HT40_MCS0",
	"HT40_MCS1",
	"HT40_MCS2",
	"HT40_MCS3",
	"HT40_MCS4",
	"HT40_MCS5",
	"HT40_MCS6",
	"HT40_MCS7",
};
#endif
//===========================global param end================================

static inline struct lmac_msg *asr_ate_msg_zalloc(lmac_msg_id_t const id,
						  lmac_task_id_t const dest_id,
						  lmac_task_id_t const src_id, u16 const param_len)
{
	struct lmac_msg *msg;
	gfp_t flags;
	uint32_t tx_data_end_token = 0xAECDBFCA;

	if (in_softirq())
		flags = GFP_ATOMIC;
	else
		flags = GFP_KERNEL;

	msg = (struct lmac_msg *)kzalloc(sizeof(struct lmac_msg) + param_len + SDIO_BLOCK_SIZE, flags);
	if (msg == NULL) {
		dev_err(g_asr_para.dev, "%s: msg allocation failed\n", __func__);
		return NULL;
	}

	msg->id = id;
	msg->dest_id = dest_id;
	msg->src_id = src_id;
	msg->param_len = param_len;

	memcpy((u8 *) msg + sizeof(struct lmac_msg) + param_len, &tx_data_end_token, 4);

	return msg;
}

static int asr_send_ate_msg(struct asr_hw *asr_hw, struct lmac_msg *msg, int reqcfm, lmac_msg_id_t reqid, void *cfm)
{
	struct asr_cmd cmd;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	memset(&cmd, 0, sizeof(cmd));

	if (!test_bit(ASR_DEV_STARTED, &asr_hw->phy_flags)) {
		dev_err(asr_hw->dev, "%s: bypassing (restart must have failed)\n", __func__);

		return -EBUSY;
	}

	cmd.result = -EINTR;
	cmd.id = msg->id;
	cmd.reqid = reqid;
	cmd.a2e_msg = msg;
	cmd.e2a_msg = cfm;

	if (!reqcfm)
		cmd.flags = ASR_CMD_FLAG_NONBLOCK;
	if (reqcfm)
		cmd.flags |= ASR_CMD_FLAG_REQ_CFM;

	ret = asr_hw->cmd_mgr.queue(&asr_hw->cmd_mgr, &cmd);

	return ret;
}

static int asr_send_ate_data(struct asr_hw *asr_hw, char *ate_msg, u32 ate_msg_len)
{
	struct lmac_msg *msg = NULL;
	char *msg_data = NULL;
	struct sk_buff *skb = NULL;
	int ret = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	if (asr_hw->driver_mode != DRIVER_MODE_ATE) {
		dev_info(asr_hw->dev, "%s: drop ate msg in driver_mode(%d)\n", __func__, asr_hw->driver_mode);
		return 0;
	}

	if (ate_msg_len >= ASR_ATE_BUFF_SIZE) {

		ate_msg_len = ASR_ATE_BUFF_SIZE - 1;
	}

	/* RESET REQ has no parameter */
	msg = asr_ate_msg_zalloc(ATE_TYPE_MSG, TASK_MM, DRV_TASK_ID, ate_msg_len);
	if (!msg)
		return -ENOMEM;

	//mutex_lock(&asr_hw->tx_msg_mutex);

	spin_lock_bh(&g_ate_info.lock);
	while (!skb_queue_empty(&g_ate_info.rx_msg_list)) {
		skb = __skb_dequeue(&g_ate_info.rx_msg_list);
		if (skb) {
			dev_info(asr_hw->dev, "%s: clear rx:%s\n", __func__, skb->data);

			memset(skb->data, 0, ASR_ATE_BUFF_SIZE);
			// Add the sk buffer structure in the table of rx buffer
			skb_queue_tail(&g_ate_info.rx_msg_empty_list, skb);
			skb = NULL;
		}
	}
	spin_unlock_bh(&g_ate_info.lock);

	dev_info(asr_hw->dev, "%s: ate cmd len=%u,%s\n", __func__, ate_msg_len, ate_msg);

	msg_data = (char *)msg->param;

	memcpy(msg_data, ate_msg, ate_msg_len);

	ret = asr_send_ate_msg(asr_hw, msg, 0, 0, NULL);

	//mutex_unlock(&asr_hw->tx_msg_mutex);

	kfree(msg);
	return ret;
}

/**
 *
 */
void asr_rx_ate_handle(struct asr_hw *asr_hw, struct ipc_e2a_msg *msg)
{
	struct sk_buff *skb = NULL;
	u32 data_len = 0;

	if ((asr_hw == NULL) || (msg == NULL)) {
		return;
	}

	dev_info(asr_hw->dev, "%s: len=%u,%s\n", __func__, msg->param_len, (char *)(msg->param));

	if (!g_ate_info.send_cmd_flag) {
		return;
	}

	spin_lock_bh(&g_ate_info.lock);
	if (!skb_queue_empty(&g_ate_info.rx_msg_empty_list)
	    && (skb = skb_dequeue(&g_ate_info.rx_msg_empty_list))) {

		data_len = msg->param_len;
		if (data_len >= ASR_ATE_BUFF_SIZE) {
			data_len = ASR_ATE_BUFF_SIZE - 1;
		}
		memset(skb->data, 0, ASR_ATE_BUFF_SIZE);
		memcpy(skb->data, msg->param, data_len);
		skb->data[data_len] = 0;
		skb_queue_tail(&g_ate_info.rx_msg_list, skb);

		complete(&g_ate_info.rx_msg_complete);
	} else {
		dev_info(asr_hw->dev, "%s: Warning, drop.\n", __func__);
	}
	spin_unlock_bh(&g_ate_info.lock);
}

static int ate_direct_tx_rx(struct asr_hw *asr_hw, char *cmd_buff,unsigned char *data, u32 wait_ms)
{
	int ret = -1;
	int len = 0;
	struct sk_buff *skb = NULL;

	//dev_info(asr_hw->dev, "%s: str=%s,cmd=%s\n", __func__, data, cmd_buff);

	ret = asr_send_ate_data(asr_hw, cmd_buff, strlen(cmd_buff) + 1);
	if (ret < 0) {
		dev_err(asr_hw->dev, "%s: fail\n", __func__);
		return ret;
	}

	g_ate_info.send_cmd_flag = true;

	if (!wait_for_completion_timeout(&g_ate_info.rx_msg_complete, wait_ms)) {
		dev_info(asr_hw->dev, "%s: msg timeout\n", __func__);
		g_ate_info.send_cmd_flag = false;
		return 0;
	}

	g_ate_info.send_cmd_flag = false;

	spin_lock_bh(&g_ate_info.lock);
	while (!skb_queue_empty(&g_ate_info.rx_msg_list)) {
		skb = __skb_dequeue(&g_ate_info.rx_msg_list);
		if (skb) {
			dev_info(asr_hw->dev, "%s: rsp:%s\n", __func__, skb->data);
			memset(asr_hw->mod_params->ate_at_cmd,0,ATE_AT_CMD_LEN);
			len = strlen(skb->data);
			memcpy((char *)data,"rsp:",4);
			memcpy((char *)(data+4), skb->data, len);
			memset(skb->data, 0, ATE_AT_CMD_LEN);
			// Add the sk buffer structure in the table of rx buffer
			skb_queue_tail(&g_ate_info.rx_msg_empty_list, skb);
		}
	}
	spin_unlock_bh(&g_ate_info.lock);

	if (!skb) {
		memset(data, 0, 4);
		len = 0;
	}

	return len;
}

int ate_data_direct_tx_rx(struct asr_hw *asr_hw, unsigned char *data)
{
	char cmd_buff[ATE_AT_CMD_LEN]={0};
	char *pdata = data, *presult = NULL;

	ASR_DBG(ASR_FN_ENTRY_STR);


	while (*pdata) {
		presult = strchr(pdata, ',');
		if (presult) {
			*presult = ' ';
			pdata = presult + 1;

			presult = NULL;
		} else {
			break;
		}
	}

	strlcpy(cmd_buff, data, sizeof(cmd_buff));
	memset(asr_hw->mod_params->ate_at_cmd,0,ATE_AT_CMD_LEN);

	return ate_direct_tx_rx(asr_hw, cmd_buff,data, msecs_to_jiffies(ASR_ATE_MSG_TIMEOUT));
}

#ifdef ASR_WIFI_CONFIG_SUPPORT
void ate_sync_txpwr_rate_config(struct asr_hw *asr_hw)
{
	int index = 0;
	char at_test_cmd[50] = "";

	//set tx pwr
	if (g_wifi_config.pwr_config) {

		for (index = 0; index < WIFI_CONFIG_MAX_PWR_NUM; index++) {
			memset(at_test_cmd, 0, sizeof(at_test_cmd));
			snprintf(at_test_cmd, sizeof(at_test_cmd), "wifi_pwr_rate_offset %s %d",
				 rate_buff[index], g_wifi_config.tx_pwr[index]);
			ate_data_direct_tx_rx(asr_hw, at_test_cmd);
		}
	}
}
#endif

int asr_ate_init(struct asr_hw *asr_hw)
{
	struct sk_buff *skb = NULL;
	int index = 0;

	ASR_DBG(ASR_FN_ENTRY_STR);

	memset(&g_ate_info, 0, sizeof(g_ate_info));
	init_completion(&g_ate_info.rx_msg_complete);
	skb_queue_head_init(&g_ate_info.rx_msg_empty_list);
	skb_queue_head_init(&g_ate_info.rx_msg_list);
	mutex_init(&g_ate_info.tx_msg_mutex);
	spin_lock_init(&g_ate_info.lock);

	dev_info(asr_hw->dev, "%s:ALLOC %d msg\n", __func__, ASR_ATE_BUFF_NUM);

	spin_lock_bh(&g_ate_info.lock);
	for (index = 0; index < ASR_ATE_BUFF_NUM; index++) {

		// Allocate a new sk buff
		if (asr_rxbuff_alloc(asr_hw, ASR_ATE_BUFF_SIZE, &skb)) {
			dev_err(asr_hw->dev, "%s:%d: MEM ALLOC FAILED\n", __func__, __LINE__);
			return -1;
		}

		memset(skb->data, 0, ASR_ATE_BUFF_SIZE);
		// Add the sk buffer structure in the table of rx buffer
		skb_queue_tail(&g_ate_info.rx_msg_empty_list, skb);
	}
	spin_unlock_bh(&g_ate_info.lock);

	return 0;
}

int asr_ate_deinit(struct asr_hw *asr_hw)
{
	struct sk_buff *skb = NULL;

	ASR_DBG(ASR_FN_ENTRY_STR);

	dev_info(asr_hw->dev, "%s\n", __func__);

	spin_lock_bh(&g_ate_info.lock);
	while (!skb_queue_empty(&g_ate_info.rx_msg_empty_list)) {
		skb = __skb_dequeue(&g_ate_info.rx_msg_empty_list);
		if (skb) {
			dev_kfree_skb(skb);
			skb = NULL;
		}
	}

	while (!skb_queue_empty(&g_ate_info.rx_msg_list)) {
		skb = __skb_dequeue(&g_ate_info.rx_msg_list);
		if (skb) {
			dev_kfree_skb(skb);
			skb = NULL;
		}
	}
	spin_unlock_bh(&g_ate_info.lock);

	memset(&g_ate_info, 0, sizeof(g_ate_info));

	return 0;
}
