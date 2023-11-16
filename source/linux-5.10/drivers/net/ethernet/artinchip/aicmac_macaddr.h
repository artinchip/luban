/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#ifndef _AICMAC_MACADDR_H_
#define _AICMAC_MACADDR_H_

void aicmac_get_mac_addr(struct device *dev, int bus_id, unsigned char *addr);

#endif
