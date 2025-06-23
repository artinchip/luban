// SPDX-License-Identifier: GPL-2.0-only
/*
 * Mailbox driver of ArtInChip SoC Uboot
 *
 * Copyright (C) 2024 ArtInChip Technology Co., Ltd.
 * Authors:  weihui.xu <weihui.xu@artinchip.com>
 */
#include <dm.h>
#include <malloc.h>
#include <common.h>
#include <dm/device.h>
#include <dm/devres.h>
#include <dm/lists.h>
#include <dm/device_compat.h>
#include "artinchip-mailbox-hal.h"

#define AIC_MBOX_NAME		"aic-mbox-char"
#define MSG_MAX_WORD_LEN	32

static mbox_cb_t g_mbox_cb;

static int aic_mbox_recv(struct mbox_chan *chan, void *data)
{
	u32 cnt;
	static u32 index;
	struct aic_mbox *mbox = dev_get_priv(chan->dev);
	mbox->recv_buf = (u32 *)data;
	u32 sta = aic_mbox_int_sta(mbox->regs);

	/* dev_dbg(chan->dev, "IRQ status: 0x%x\n", sta); */
	if (!aic_mbox_int_sta_is_cared(sta))
		goto err;

	if (aic_mbox_int_sta_is_rx_cmp(sta)) {
		dev_dbg(chan->dev, "Peer had received data.\n");
		mbox->last_tx_done = 1;
	}

	if (aic_mbox_int_sta_is_rx_uf(sta)) {
		dev_err(chan->dev, "RxFIFO is underflow!\n");
		aic_mbox_rxfifo_rst(mbox->regs);
		goto err;
	}

	if (aic_mbox_int_sta_is_rx_full(sta)
	    | aic_mbox_int_sta_is_tx_cmp(sta)) {
		cnt = aic_mbox_rxfifo_cnt(mbox->regs);

		if (cnt > 1) {
			dev_dbg(chan->dev, "RxFIFO discard %d data.\n", cnt - 1);
			aic_mbox_rd(mbox->regs, &mbox->recv_buf[index], cnt - 1);
			index += cnt - 1;
		}

		if (aic_mbox_int_sta_is_tx_cmp(sta)) {
			mbox->recv_buf[index] = aic_mbox_rd_cmp(mbox->regs);
			dev_dbg(chan->dev, "Recv len %d Word, index:%d\n", cnt, index);
			index++;

			if (g_mbox_cb) {
				g_mbox_cb(mbox->recv_buf, index);
				mbox->last_tx_done = 1;
			}

			index = 0;
		}
		goto handled;
	}

	if (aic_mbox_int_sta_is_tx_of(sta)) {
		dev_err(chan->dev, "mbox: TxFIFO is overflow!\n");
		goto err;
	}

	if (aic_mbox_int_sta_is_tx_full(sta)) {
		dev_dbg(chan->dev, "mbox: TxFIFO is full!\n");
		goto err;
	}

err:
	aic_mbox_int_clr(mbox->regs, sta);
	return -ENODATA;

handled:
	aic_mbox_int_clr(mbox->regs, sta);
	return 0;
}

void hal_mbox_set_cb(mbox_cb_t cb)
{
	if (!cb)
		return;

	g_mbox_cb = cb;
}

static void aic_mbox_shutdown(struct mbox_chan *chan)
{
	struct aic_mbox *mbox = dev_get_priv(chan->dev);

	aic_mbox_int_all_en(mbox->regs, 0);
	aic_mbox_rxfifo_rst(mbox->regs);

	dev_dbg(chan->dev, "Shutdown\n");
}

static int aic_mbox_of_xlate(struct mbox_chan *chan,
			     struct ofnode_phandle_args *args)
{
	struct aic_mbox *mbox = dev_get_priv(chan->dev);

	if (args->args_count != 1) {
		dev_dbg(chan->dev, "Invaild args_count: %d\n", args->args_count);
		return -EINVAL;
	}
	mbox->regs = (void __iomem *)dev_read_addr(chan->dev);

	chan->con_priv = mbox;
	chan->id = args->args[0];

	return 0;
}

static int aic_mbox_send(struct mbox_chan *chan, const void *data)
{
	struct aic_mbox *mbox = dev_get_priv(chan->dev);
	struct aic_rpmsg *msg = (struct aic_rpmsg *)data;
	u32 sta = aic_mbox_int_sta(mbox->regs);
	u32 ret = 0;

	if (aic_mbox_int_sta_is_rx_cmp(sta)) {
		dev_dbg(chan->dev, "Peer had received data\n");
		mbox->last_tx_done = 1;
	}

	if (!mbox->last_tx_done) {
		dev_err(chan->dev, "The mailbox is busy now! %d\n", aic_mbox_txfifo_cnt(mbox->regs));
		return -ENOENT;
	}

	if (!msg->len || msg->len > MSG_MAX_WORD_LEN) {
		dev_err(chan->dev, "Error in message length!\n");
		return -ENOENT;
	}

	ret = aic_mbox_wr(mbox->regs, (u32 *)msg, msg->len - 1);
	if (!ret) {
		dev_err(chan->dev, "The mailbox send fail!\n");
		return -ENOENT;
	}

	aic_mbox_wr_cmp(mbox->regs, msg->data[msg->len - 1]);
	mbox->last_tx_done = 0;

	return 0;
}

/**
 * aic_mbox_rfree() - Free the mailbox channel
 * @chan:	Channel Pointer
 */
static int aic_mbox_rfree(struct mbox_chan *chan)
{
	dev_dbg(chan->dev, "%s(chan=%p)\n", __func__, chan);

	aic_mbox_shutdown(chan);

	return 0;
}

static const struct mbox_ops aic_mbox_ops = {
	.of_xlate = aic_mbox_of_xlate,
	.rfree = aic_mbox_rfree,
	.send = aic_mbox_send,
	.recv = aic_mbox_recv,
};

static int aic_mbox_probe(struct udevice *dev)
{
	struct aic_mbox *mbox = dev_get_priv(dev);

	mbox->regs = (void __iomem *)dev_read_addr(dev);
	if (!mbox->regs)
		return -ENOENT;

	aic_mbox_int_all_en(mbox->regs, 0);
#ifdef CONFIG_AIC_MBOX_DBG_MODE
	aic_mbox_dbg_mode(mbox->regs);
#endif
	mbox->last_tx_done = 1;
	return 0;
}

static const struct udevice_id aic_mbox_ids[] = {
	{ .compatible = "artinchip,aic-mbox-char-v1.0", },
	{},
};

U_BOOT_DRIVER(artinchip_mbox) = {
	.name = AIC_MBOX_NAME,
	.id = UCLASS_MAILBOX,
	.of_match = aic_mbox_ids,
	.probe = aic_mbox_probe,
	.flags = DM_FLAG_PRE_RELOC,
	.priv_auto = sizeof(struct aic_mbox),
	.ops = &aic_mbox_ops,
};
