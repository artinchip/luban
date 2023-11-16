// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#include <linux/clk.h>
#include <linux/if_vlan.h>
#include <linux/phylink.h>
#include <linux/pci.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/net_tstamp.h>
#include <linux/reset.h>
#include <net/page_pool.h>

#include "aicmac.h"
#include "aicmac_dma.h"
#include "aicmac_dma_desc.h"

int aicmac_dma_ring_init(void *des, dma_addr_t phy_addr, unsigned int size,
			 unsigned int extend_desc)
{
	return 0;
}

int aicmac_dma_ring_jumbo_frm(void *p, struct sk_buff *skb, int csum)
{
	struct aicmac_tx_queue *tx_q = (struct aicmac_tx_queue *)p;
	unsigned int nopaged_len = skb_headlen(skb);
	struct aicmac_priv *priv = tx_q->priv_data;
	unsigned int entry = tx_q->cur_tx;
	unsigned int bmax, len, des2;
	struct dma_desc *desc;

	desc = (struct dma_desc *)(tx_q->dma_etx + entry);

	if (priv->plat->hw_cap.enh_desc)
		bmax = BUF_SIZE_8KiB;
	else
		bmax = BUF_SIZE_2KiB;

	len = nopaged_len - bmax;

	if (nopaged_len > BUF_SIZE_8KiB) {
		des2 = dma_map_single(priv->device, skb->data, bmax,
				      DMA_TO_DEVICE);
		desc->des2 = cpu_to_le32(des2);
		if (dma_mapping_error(priv->device, des2))
			return -1;

		tx_q->tx_skbuff_dma[entry].buf = des2;
		tx_q->tx_skbuff_dma[entry].len = bmax;
		tx_q->tx_skbuff_dma[entry].is_jumbo = true;

		desc->des3 = cpu_to_le32(des2 + BUF_SIZE_4KiB);
		aicmac_dma_desc_prepare_tx_desc(desc, 1, bmax, csum,
						AICMAC_RING_MODE, 0, false,
						skb->len);
		tx_q->tx_skbuff[entry] = NULL;
		entry = AICMAC_GET_ENTRY(entry, DMA_TX_SIZE);

		desc = (struct dma_desc *)(tx_q->dma_etx + entry);

		des2 = dma_map_single(priv->device, skb->data + bmax, len,
				      DMA_TO_DEVICE);
		desc->des2 = cpu_to_le32(des2);
		if (dma_mapping_error(priv->device, des2))
			return -1;

		tx_q->tx_skbuff_dma[entry].buf = des2;
		tx_q->tx_skbuff_dma[entry].len = len;
		tx_q->tx_skbuff_dma[entry].is_jumbo = true;

		desc->des3 = cpu_to_le32(des2 + BUF_SIZE_4KiB);
		aicmac_dma_desc_prepare_tx_desc(desc, 0, len, csum,
						AICMAC_RING_MODE, 1,
						!skb_is_nonlinear(skb),
						skb->len);
	} else {
		des2 = dma_map_single(priv->device, skb->data,
				      nopaged_len, DMA_TO_DEVICE);
		desc->des2 = cpu_to_le32(des2);
		if (dma_mapping_error(priv->device, des2))
			return -1;
		tx_q->tx_skbuff_dma[entry].buf = des2;
		tx_q->tx_skbuff_dma[entry].len = nopaged_len;
		tx_q->tx_skbuff_dma[entry].is_jumbo = true;
		desc->des3 = cpu_to_le32(des2 + BUF_SIZE_4KiB);
		aicmac_dma_desc_prepare_tx_desc(desc, 1, nopaged_len, csum,
						AICMAC_RING_MODE, 0,
						!skb_is_nonlinear(skb),
						skb->len);
	}

	tx_q->cur_tx = entry;

	return entry;
}

unsigned int aicmac_dma_ring_is_jumbo_frm(int len, int enh_desc)
{
	unsigned int ret = 0;

	if (len >= BUF_SIZE_4KiB)
		ret = 1;

	return ret;
}

void aicmac_dma_ring_refill_desc3(void *queue_ptr, struct dma_desc *p)
{
	struct aicmac_rx_queue *rx_q = queue_ptr;
	struct aicmac_priv *priv = rx_q->priv_data;

	if (priv->plat->dma_data->dma_buf_sz == BUF_SIZE_16KiB)
		p->des3 = cpu_to_le32(le32_to_cpu(p->des2) + BUF_SIZE_8KiB);
}

void aicmac_dma_ring_init_desc3(struct dma_desc *p)
{
	p->des3 = cpu_to_le32(le32_to_cpu(p->des2) + BUF_SIZE_8KiB);
}

void aicmac_dma_ring_clean_desc3(void *priv_ptr, struct dma_desc *p)
{
	struct aicmac_tx_queue *tx_q = (struct aicmac_tx_queue *)priv_ptr;
	struct aicmac_priv *priv = tx_q->priv_data;
	unsigned int entry = tx_q->dirty_tx;

	if (unlikely(tx_q->tx_skbuff_dma[entry].is_jumbo ||
		     (tx_q->tx_skbuff_dma[entry].last_segment &&
		      !priv->extend_desc && priv->plat->ptp_data->hwts_tx_en)))
		p->des3 = 0;
}

int aicmac_dma_ring_set_16kib_bfsize(int mtu)
{
	int ret = 0;

	if (unlikely(mtu > BUF_SIZE_8KiB))
		ret = BUF_SIZE_16KiB;

	return ret;
}

int aicmac_dma_ring_set_bfsize(int mtu, int bufsize)
{
	int ret = bufsize;

	if (mtu >= BUF_SIZE_8KiB)
		ret = BUF_SIZE_16KiB;
	else if (mtu >= BUF_SIZE_4KiB)
		ret = BUF_SIZE_8KiB;
	else if (mtu >= BUF_SIZE_2KiB)
		ret = BUF_SIZE_4KiB;
	else if (mtu > DEFAULT_BUFSIZE)
		ret = BUF_SIZE_2KiB;
	else
		ret = DEFAULT_BUFSIZE;

	return ret;
}
