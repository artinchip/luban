/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#ifndef _AICMAC_DMA_RING_H_
#define _AICMAC_DMA_RING_H_

#include <linux/skbuff.h>

#include "aicmac_dma_desc.h"

int aicmac_dma_ring_init(void *des, dma_addr_t phy_addr, unsigned int size,
			 unsigned int extend_desc);
int aicmac_dma_ring_jumbo_frm(void *p, struct sk_buff *skb, int csum);
unsigned int aicmac_dma_ring_is_jumbo_frm(int len, int enh_desc);
void aicmac_dma_ring_refill_desc3(void *queue_ptr, struct dma_desc *p);
void aicmac_dma_ring_init_desc3(struct dma_desc *p);
void aicmac_dma_ring_clean_desc3(void *priv_ptr, struct dma_desc *p);
int aicmac_dma_ring_set_16kib_bfsize(int mtu);
int aicmac_dma_ring_set_bfsize(int mtu, int bufsize);
#endif
