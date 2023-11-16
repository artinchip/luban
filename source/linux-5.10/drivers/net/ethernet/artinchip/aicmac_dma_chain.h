/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */
#ifndef _AICMAC_DMA_CHAIN_H_
#define _AICMAC_DMA_CHAIN_H_

#include <linux/skbuff.h>

#include "aicmac_dma_desc.h"

int aicmac_dma_chain_init(void *des, dma_addr_t phy_addr, unsigned int size,
			  unsigned int extend_desc);
int aicmac_dma_chain_jumbo_frm(void *p, struct sk_buff *skb, int csum);
int aicmac_dma_chain_is_jumbo_frm(int len, int enh_desc);
void aicmac_dma_chain_refill_desc3(void *priv_ptr, struct dma_desc *p);
void aicmac_dma_chain_clean_desc3(void *priv_ptr, struct dma_desc *p);
void aicmac_dma_chain_init_desc3(struct dma_desc *p);
int aicmac_dma_chain_set_16kib_bfsize(int mtu);
int aicmac_dma_chain_set_bfsize(int mtu, int bufsize);

#endif
