/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#ifndef _AICMAC_CORE_H_
#define _AICMAC_CORE_H_

#include "aicmac.h"

#define PAUSE_TIME			0xffff
#define AICMAC_COAL_TX_TIMER		1000
#define AICMAC_MAX_COAL_TX_TICK		100000
#define AICMAC_TX_MAX_FRAMES		256
#define AICMAC_TX_FRAMES		25
#define AICMAC_RX_FRAMES		0

#define AICMAC_WQ_NAME			"aicmac_wq"

enum aicmac_state {
	AICMAC_DOWN,
	AICMAC_RESET_REQUESTED,
	AICMAC_RESETTING,
	AICMAC_SERVICE_SCHED,
};

int aicmac_core_init(struct device *device,
		     struct aicmac_platform_data *plat_dat,
		     struct aicmac_resources *res);
int aicmac_core_destroy(struct aicmac_priv *priv);

int aicmac_core_setup_napiop(struct aicmac_priv *priv, struct net_device *dev);
#endif
