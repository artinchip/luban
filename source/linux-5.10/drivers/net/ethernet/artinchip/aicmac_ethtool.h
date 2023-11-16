/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */
#ifndef _AICMAC_ETHTOOL_H_
#define _AICMAC_ETHTOOL_H_

#define REG_SPACE_SIZE		0x1060

void aicmac_ethtool_init(void *priv_ptr);
void aicmac_set_ethtool_ops(struct net_device *netdev);

#endif
