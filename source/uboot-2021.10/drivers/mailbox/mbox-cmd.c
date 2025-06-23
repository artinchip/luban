/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Test command of mailbox driver for ArtInChip SoC Uboot
 *
 * Copyright (C) 2020-2024 ArtInChip Technology Co., Ltd.
 * Authors:  Matteo <duanmt@artinchip.com>
 */

#include <common.h>
#include <command.h>
#include <dm.h>
#include <errno.h>
#include <div64.h>
#include "ctype.h"
#include <asm/io.h>
#include <dm/lists.h>
#include <linux/delay.h>
#include <clk-uclass.h>
#include <dt-structs.h>
#include <dm/device-internal.h>
#include <dm/uclass-internal.h>
#include <mailbox-uclass.h>
#include "artinchip-mailbox-hal.h"

#define MSG_CNT_DEFAULT		10
#define MSG_MAX_LEN		512
#define RPMSG_CMD_DEF(m, c)	((m) << 8 | (c))
#define RPMSG_CMD_DDR_RDY	RPMSG_CMD_DEF('e', 6) /* DDR is ready */

static u32 g_msg_cnt = MSG_CNT_DEFAULT;
static u32 g_msg_len;

static u32 g_send_buf[MSG_MAX_LEN] = {0};
static u32 g_recv_buf[MSG_MAX_LEN] = {0};

static void mbox_msg_generate(struct aic_rpmsg *msg, u32 cnt)
{
	msg->cmd = RPMSG_CMD_DDR_RDY;
	msg->seq = cnt;

	if (g_msg_len) {
		msg->len = g_msg_len / 4 - 1;
		printf("Send cmd 0x%x, seq %d, data len %d Word\n",
			msg->cmd, msg->seq, msg->len);
	} else {
		msg->len = 6;
		snprintf((char *)msg->data, 32, "Hi! I'm CSYS %d", cnt);
		printf("Send cmd 0x%x, seq %d, data len %d Word, data |%s|\n",
			msg->cmd, msg->seq, msg->len, (char *)msg->data);
	}
}

static void test_mbox_recv_cb(u32 *data, u32 len)
{
	struct aic_rpmsg *msg = (struct aic_rpmsg *)data;

	g_recv_buf[msg->len * 4] = 0;
	printf("\nRecv cmd 0x%x, seq %d, data len %d Word, data |%s|\n",
		msg->cmd, msg->seq, msg->len, (char *)msg->data);
}

static int mbox_send_msg(struct cmd_tbl *cmdtp, int flag, int argc,
					char *const argv[])
{
	struct mbox_chan chan;
	struct aic_rpmsg *msg = (struct aic_rpmsg *)g_send_buf;
	int i, ret;

	if (argc > 3 || argc == 1) {
		printf("please input test_mbox send [cnt] [len].\n");
		return 0;
	}

	if (argc >= 2) {
		g_msg_len = dectoul(argv[1], NULL);
		if (argc == 3)
			g_msg_cnt = dectoul(argv[2], NULL);
	}

	ret = uclass_get_device_by_driver(UCLASS_MAILBOX,
			DM_DRIVER_GET(artinchip_mbox), &chan.dev);
	if (ret)
		return ret;

	char *tmp = (char *)&g_send_buf[1];
	for (i = 0; i < g_msg_len; i++)
		tmp[i] = 0x30 + i % 10;

	hal_mbox_set_cb(test_mbox_recv_cb);
	printf("Will send %d message with a length %d Byte\n",
			g_msg_cnt, (g_msg_len / 4 - 1));

	for (i = 0; i < g_msg_cnt; i++) {
		mbox_msg_generate(msg, i);
		ret = mbox_send(&chan, msg);
		if (ret < 0) {
			printf("ret: %d  test_send_mbox send fail!\n", ret);
			return ret;
		}
		memset(g_recv_buf, 0, MSG_MAX_LEN);
		ret = mbox_recv(&chan, g_recv_buf, 5000);	// The timeout parameter is invalid.
		if (ret < 0) {
			printf("ret: %d  test_send_mbox recv fail!\n", ret);
			return ret;
		}
	}

	printf("------------------------------------\n");
	return 0;
}

static char mbox_help_text[] =
	"---------AIC mailbox command----------\n"
	" test_mbox  [len] or [len] [cnt]   send num of infor\n"
	"--------------------------------------\n";

U_BOOT_CMD(test_mbox, 3, 1, mbox_send_msg, "Uboot mailbox", mbox_help_text);

