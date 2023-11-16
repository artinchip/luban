/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#ifndef _AICMAC_NAPI_H_
#define _AICMAC_NAPI_H_

#include <linux/netdevice.h>

#define JUMBO_LEN		9000
#define AICMAC_NAPI_DEBUG	-1
#define AICMAC_NAPI_MSG_LEVEL	(NETIF_MSG_DRV | NETIF_MSG_PROBE | \
				 NETIF_MSG_LINK | NETIF_MSG_IFUP | \
				 NETIF_MSG_IFDOWN | NETIF_MSG_TIMER)

struct aicmac_napi_data {
};

struct aicmac_napi_data *aicmac_napi_init_data(struct platform_device *pdev,
					       struct device_node *np);
int aicmac_napi_init(void *priv_ptr);
#endif
