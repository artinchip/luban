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
#include "aicmac_dma_chain.h"

int aicmac_dma_chain_init(void *des, dma_addr_t phy_addr, unsigned int size,
			  unsigned int extend_desc)
{
	int i;
	dma_addr_t dma_phy = phy_addr;

	if (extend_desc) {
		struct dma_extended_desc *p = (struct dma_extended_desc *)des;

		for (i = 0; i < (size - 1); i++) {
			dma_phy += sizeof(struct dma_extended_desc);
			p->basic.des3 = cpu_to_le32((unsigned int)dma_phy);
			p++;
		}
		p->basic.des3 = cpu_to_le32((unsigned int)phy_addr);

	} else {
		struct dma_desc *p = (struct dma_desc *)des;

		for (i = 0; i < (size - 1); i++) {
			dma_phy += sizeof(struct dma_desc);
			p->des3 = cpu_to_le32((unsigned int)dma_phy);
			p++;
		}
		p->des3 = cpu_to_le32((unsigned int)phy_addr);
	}

	return 0;
}

int aicmac_dma_chain_jumbo_frm(void *p, struct sk_buff *skb, int csum)
{
	struct aicmac_tx_queue *tx_q = (struct aicmac_tx_queue *)p;
	unsigned int nopaged_len = skb_headlen(skb);
	struct aicmac_priv *priv = tx_q->priv_data;
	unsigned int entry = tx_q->cur_tx;
	unsigned int bmax, des2;
	unsigned int i = 1, len;
	struct dma_desc *desc;

	desc = (struct dma_desc *)(tx_q->dma_etx + entry);

	if (priv->plat->hw_cap.enh_desc)
		bmax = BUF_SIZE_8KiB;
	else
		bmax = BUF_SIZE_2KiB;

	len = nopaged_len - bmax;

	des2 = dma_map_single(priv->device, skb->data, bmax, DMA_TO_DEVICE);
	desc->des2 = cpu_to_le32(des2);
	if (dma_mapping_error(priv->device, des2))
		return -1;
	tx_q->tx_skbuff_dma[entry].buf = des2;
	tx_q->tx_skbuff_dma[entry].len = bmax;
	/* do not close the descriptor and do not set own bit */
	aicmac_dma_desc_prepare_tx_desc(desc, 1, bmax, csum, AICMAC_CHAIN_MODE,
					0, false, skb->len);

	while (len != 0) {
		tx_q->tx_skbuff[entry] = NULL;
		entry = AICMAC_GET_ENTRY(entry, DMA_DEFAULT_TX_SIZE);
		desc = (struct dma_desc *)(tx_q->dma_etx + entry);

		if (len > bmax) {
			des2 = dma_map_single(priv->device,
					      (skb->data + bmax * i), bmax,
					      DMA_TO_DEVICE);
			desc->des2 = cpu_to_le32(des2);
			if (dma_mapping_error(priv->device, des2))
				return -1;
			tx_q->tx_skbuff_dma[entry].buf = des2;
			tx_q->tx_skbuff_dma[entry].len = bmax;
			aicmac_dma_desc_prepare_tx_desc(desc, 0, bmax, csum,
							AICMAC_CHAIN_MODE, 1,
							false, skb->len);
			len -= bmax;
			i++;
		} else {
			des2 = dma_map_single(priv->device,
					      (skb->data + bmax * i), len,
					      DMA_TO_DEVICE);
			desc->des2 = cpu_to_le32(des2);
			if (dma_mapping_error(priv->device, des2))
				return -1;
			tx_q->tx_skbuff_dma[entry].buf = des2;
			tx_q->tx_skbuff_dma[entry].len = len;
			/* last descriptor can be set now */
			aicmac_dma_desc_prepare_tx_desc(desc, 0, len, csum,
							AICMAC_CHAIN_MODE, 1,
							true, skb->len);
			len = 0;
		}
	}

	tx_q->cur_tx = entry;

	return entry;
}

int aicmac_dma_chain_is_jumbo_frm(int len, int enh_desc)
{
	unsigned int ret = 0;

	if ((enh_desc && len > BUF_SIZE_8KiB) ||
	    (!enh_desc && len > BUF_SIZE_2KiB)) {
		ret = 1;
	}

	return ret;
}

void aicmac_dma_chain_refill_desc3(void *priv_ptr, struct dma_desc *p)
{
	struct aicmac_rx_queue *rx_q = (struct aicmac_rx_queue *)priv_ptr;
	struct aicmac_priv *priv = rx_q->priv_data;

	if (priv->plat->ptp_data->hwts_rx_en && !priv->extend_desc)
		p->des3 = cpu_to_le32((unsigned int)(rx_q->dma_rx_phy +
				       (((rx_q->dirty_rx) + 1) %
					DMA_DEFAULT_RX_SIZE) *
					       sizeof(struct dma_desc)));
}

void aicmac_dma_chain_clean_desc3(void *priv_ptr, struct dma_desc *p)
{
	struct aicmac_tx_queue *tx_q = (struct aicmac_tx_queue *)priv_ptr;
	struct aicmac_priv *priv = tx_q->priv_data;
	unsigned int entry = tx_q->dirty_tx;

	if (tx_q->tx_skbuff_dma[entry].last_segment && !priv->extend_desc &&
	    priv->plat->ptp_data->hwts_rx_en)
		p->des3 = cpu_to_le32((unsigned int)((tx_q->dma_tx_phy +
						      ((tx_q->dirty_tx + 1) %
						       DMA_DEFAULT_RX_SIZE)) *
						     sizeof(struct dma_desc)));
}

int aicmac_dma_chain_set_16kib_bfsize(int mtu)
{
	return 0;
}

int aicmac_dma_chain_set_bfsize(int mtu, int bufsize)
{
	return 0;
}

void aicmac_dma_chain_init_desc3(struct dma_desc *p)
{
}

