/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#ifndef _AICMAC_H_
#define _AICMAC_H_

#include <linux/clk.h>
#include <linux/if_vlan.h>
#include <linux/phylink.h>
#include <linux/pci.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/net_tstamp.h>
#include <linux/reset.h>
#include <net/page_pool.h>

#include "aicmac_platform.h"

#define DRV_MODULE_VERSION		"20211010"

#define PAUSE_TIME			0xffff
#define FLOW_OFF			0
#define FLOW_RX				1
#define FLOW_TX				2
#define FLOW_AUTO			(FLOW_TX | FLOW_RX)
#define TC_DEFAULT			256
#define	DEFAULT_BUFSIZE			1536

#define AICMAC_CHAIN_MODE		0x1
#define AICMAC_RING_MODE		0x2

#define MTL_MAX_RX_QUEUES		8
#define MTL_MAX_TX_QUEUES		8

#define AICMAC_RSS_HASH_KEY_SIZE	40
#define AICMAC_RSS_MAX_TABLE_SIZE	256

#define BUF_SIZE_16KiB			16368
#define BUF_SIZE_8KiB			8188
#define BUF_SIZE_4KiB			4096
#define BUF_SIZE_2KiB			2048
#define	DEFAULT_BUFSIZE			1536
#define	AICMAC_RX_COPYBREAK		256

#define AICMAC_DEFAULT_LIT_LS		0x3E8
#define AICMAC_DEFAULT_TWT_LS		0x1E

#define LINK_STATE_UP			1
#define LINK_STATE_DOWN			0

struct aicmac_priv {
	struct aicmac_platform_data *plat;
	struct aicmac_resources *resource;

	struct net_device *dev;
	struct device *device;

	struct workqueue_struct *wq;
	struct work_struct service_task;

	unsigned long service_state;

	u32 tx_coal_frames;
	u32 tx_coal_timer;
	u32 rx_coal_frames;

	struct mutex lock;	//global lock
	int link_state;

	int extend_desc;
	int msg_enable;
	int mode;
	u32 adv_ts;
};

#endif
