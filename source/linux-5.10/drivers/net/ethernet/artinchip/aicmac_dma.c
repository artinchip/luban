// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include "aicmac.h"
#include "aicmac_dma.h"
#include "aicmac_util.h"
#include "aicmac_dma_reg.h"
#include "aicmac_dma_desc.h"
#include "aicmac_dma_ring.h"
#include "aicmac_dma_chain.h"

struct aicmac_dma_data *aicmac_dma_init_data(struct platform_device *pdev,
					     struct device_node *np)
{
	struct aicmac_dma_data *dma_data = devm_kzalloc(&pdev->dev,
			sizeof(struct aicmac_dma_data), GFP_KERNEL);

	dma_data->pbl = DEFAULT_DMA_PBL;
	dma_data->txpbl = DEFAULT_DMA_PBL;
	dma_data->rxpbl = DEFAULT_DMA_PBL;
	dma_data->pblx8 = true; //8*PBL

	dma_data->aal = true;
	dma_data->fixed_burst = true;
	dma_data->mixed_burst = false;

	dma_data->tx_coe = AICMAC_RX_COE_TYPE2;
	dma_data->rx_coe = AICMAC_RX_COE_TYPE2;

	dma_data->tx_fifo_size = 2048;
	dma_data->rx_fifo_size = 2048;

	return dma_data;
}

int aicmac_dma_init(void *priv_ptr)
{
	struct aicmac_priv *priv = priv_ptr;
	struct aicmac_dma_data *dma_data = priv->plat->dma_data;
	int ret = 0;

	/* TXCOE doesn't work in thresh DMA mode */
	dma_data->tx_coe = priv->plat->hw_cap.tx_coe;
	dma_data->rx_coe = priv->plat->hw_cap.rx_coe;

	if (priv->plat->hw_cap.addr64) {
		ret = dma_set_mask_and_coherent(priv->device,
				DMA_BIT_MASK(priv->plat->hw_cap.addr64));
		if (!ret) {
			dev_info(priv->device, "Using %d bits DMA width\n",
				 priv->plat->hw_cap.addr64);
		} else {
			ret = dma_set_mask_and_coherent(priv->device,
							DMA_BIT_MASK(32));
			if (ret) {
				dev_err(priv->device,
					"Failed to set DMA Mask\n");
				return ret;
			}

			priv->plat->hw_cap.addr64 = 32;
		}
	}

	return ret;
}

static void aicmac_dma_display_rings(struct aicmac_priv *priv)
{
	/* Display RX ring */
	aicmac_display_all_rings("rx", priv, 0);

	/* Display TX ring */
	aicmac_display_all_rings("tx", priv, 1);
}

static void aicmac_dma_free_rx_buffer(struct aicmac_priv *priv, u32 queue,
				      int i)
{
	struct aicmac_rx_queue *rx_q = &priv->plat->rx_queue[queue];

	kfree_skb(rx_q->rx_skbuff[i]);
	dma_unmap_single(priv->device, rx_q->rx_skbuff_dma[i],
			 priv->plat->dma_data->dma_buf_sz, DMA_FROM_DEVICE);
}

static void aicmac_dma_free_rx_skbufs(struct aicmac_priv *priv, u32 queue)
{
	int i;

	for (i = 0; i < DMA_RX_SIZE; i++)
		aicmac_dma_free_rx_buffer(priv, queue, i);
}

static void aicmac_dma_free_rx_desc_resources(struct aicmac_priv *priv)
{
	u32 rx_count = priv->plat->rx_queues_to_use;
	u32 queue;

	/* Free RX queue resources */
	for (queue = 0; queue < rx_count; queue++) {
		struct aicmac_rx_queue *rx_q = &priv->plat->rx_queue[queue];

		/* Release the DMA RX socket buffers */
		aicmac_dma_free_rx_skbufs(priv, queue);

		/* Free DMA regions of consistent memory previously allocated */
		dma_free_coherent(priv->device,
			DMA_RX_SIZE * sizeof(struct dma_extended_desc),
			rx_q->dma_erx, rx_q->dma_rx_phy);

		kfree(rx_q->rx_skbuff_dma);
		kfree(rx_q->rx_skbuff);
	}
}

static void aicmac_dma_free_tx_desc_resources(struct aicmac_priv *priv)
{
	u32 tx_count = priv->plat->tx_queues_to_use;
	u32 queue;

	/* Free TX queue resources */
	for (queue = 0; queue < tx_count; queue++) {
		struct aicmac_tx_queue *tx_q = &priv->plat->tx_queue[queue];

		/* Release the DMA TX socket buffers */
		aicmac_dma_free_tx_skbufs(priv, queue);

		/* Free DMA regions of consistent memory previously allocated */
		dma_free_coherent(priv->device,
			DMA_TX_SIZE * sizeof(struct dma_extended_desc),
			tx_q->dma_etx, tx_q->dma_tx_phy);

		kfree(tx_q->tx_skbuff_dma);
		kfree(tx_q->tx_skbuff);
	}
}

void aicmac_dma_free_desc_resources(void *priv_ptr)
{
	struct aicmac_priv *priv = priv_ptr;
	/* Release the DMA RX socket buffers */
	aicmac_dma_free_rx_desc_resources(priv);

	/* Release the DMA TX socket buffers */
	aicmac_dma_free_tx_desc_resources(priv);
}

static void aicmac_dma_free_tx_buffer(struct aicmac_priv *priv, u32 queue,
				      int i)
{
	struct aicmac_tx_queue *tx_q = &priv->plat->tx_queue[queue];

	if (tx_q->tx_skbuff_dma[i].buf) {
		if (tx_q->tx_skbuff_dma[i].map_as_page)
			dma_unmap_page(priv->device, tx_q->tx_skbuff_dma[i].buf,
				       tx_q->tx_skbuff_dma[i].len,
				       DMA_TO_DEVICE);
		else
			dma_unmap_single(priv->device,
					 tx_q->tx_skbuff_dma[i].buf,
					 tx_q->tx_skbuff_dma[i].len,
					 DMA_TO_DEVICE);
	}

	if (tx_q->tx_skbuff[i]) {
		dev_kfree_skb_any(tx_q->tx_skbuff[i]);
		tx_q->tx_skbuff[i] = NULL;
		tx_q->tx_skbuff_dma[i].buf = 0;
		tx_q->tx_skbuff_dma[i].map_as_page = false;
	}
}

void aicmac_dma_free_tx_skbufs(void *priv_ptr, u32 queue)
{
	struct aicmac_priv *priv = priv_ptr;
	int i;

	for (i = 0; i < DMA_TX_SIZE; i++)
		aicmac_dma_free_tx_buffer(priv, queue, i);
}

static int aicmac_dma_alloc_dma_rx_desc_resources(struct aicmac_priv *priv)
{
	u32 rx_count = priv->plat->rx_queues_to_use;
	int ret = -ENOMEM;
	u32 queue;
	struct aicmac_rx_queue *rx_q;

	/* RX queues buffers and DMA */
	for (queue = 0; queue < rx_count; queue++) {
		rx_q = &priv->plat->rx_queue[queue];

		/* allocate memory for RX descriptors */
		rx_q->dma_erx = dma_alloc_coherent(priv->device,
			DMA_RX_SIZE * sizeof(struct dma_extended_desc),
			&rx_q->dma_rx_phy, GFP_KERNEL);
		if (!rx_q->dma_erx)
			goto dma_error;

		/* allocate memory for RX skbuff array */
		rx_q->rx_skbuff_dma = kmalloc_array(DMA_RX_SIZE,
						sizeof(dma_addr_t), GFP_KERNEL);
		if (!rx_q->rx_skbuff_dma) {
			ret = -ENOMEM;
			goto err_rx_skbuff_dma;
		}

		rx_q->rx_skbuff = kmalloc_array(DMA_RX_SIZE,
					   sizeof(struct sk_buff *), GFP_KERNEL);
		if (!rx_q->rx_skbuff) {
			goto err_rx_skbuff;
			ret = -ENOMEM;
		}
	}

	return 0;

err_rx_skbuff:
	kfree(rx_q->rx_skbuff_dma);

err_rx_skbuff_dma:
	dma_free_coherent(priv->device,
			  DMA_RX_SIZE * sizeof(struct dma_extended_desc),
			  rx_q->dma_erx, rx_q->dma_rx_phy);

dma_error:
	// todo: Is it incorrect to release resources repeatedly?
	aicmac_dma_free_rx_desc_resources(priv);

	return ret;
}

static int aicmac_dma_alloc_dma_tx_desc_resources(struct aicmac_priv *priv)
{
	u32 tx_count = priv->plat->tx_queues_to_use;
	int ret = -ENOMEM;
	u32 queue;

	/* TX queues buffers and DMA */
	for (queue = 0; queue < tx_count; queue++) {
		struct aicmac_tx_queue *tx_q = &priv->plat->tx_queue[queue];

		tx_q->queue_index = queue;
		tx_q->priv_data = priv;

		tx_q->tx_skbuff_dma = kcalloc(DMA_TX_SIZE,
				sizeof(*tx_q->tx_skbuff_dma), GFP_KERNEL);
		if (!tx_q->tx_skbuff_dma)
			goto err_dma;

		tx_q->tx_skbuff = kcalloc(DMA_TX_SIZE, sizeof(struct sk_buff *),
					  GFP_KERNEL);
		if (!tx_q->tx_skbuff)
			goto err_dma;

		tx_q->dma_etx = dma_alloc_coherent(priv->device,
			DMA_TX_SIZE * sizeof(struct dma_extended_desc),
			&tx_q->dma_tx_phy, GFP_KERNEL);
		if (!tx_q->dma_etx)
			goto err_dma;
	}

	return 0;

err_dma:
	aicmac_dma_free_tx_desc_resources(priv);

	return ret;
}

int aicmac_dma_alloc_dma_desc_resources(void *priv_ptr)
{
	struct aicmac_priv *priv = priv_ptr;

	/* RX Allocation */
	int ret = aicmac_dma_alloc_dma_rx_desc_resources(priv);

	if (ret)
		return ret;

	ret = aicmac_dma_alloc_dma_tx_desc_resources(priv);

	return ret;
}

static void aicmac_dma_clear_rx_descriptors(struct aicmac_priv *priv, u32 queue)
{
	struct aicmac_rx_queue *rx_q = &priv->plat->rx_queue[queue];
	int i;
	int use_riwt = !priv->plat->mac_data->riwt_off;

	/* Clear the RX descriptors */
	for (i = 0; i < DMA_RX_SIZE; i++)
		aicmac_dma_desc_init_rx_desc(&rx_q->dma_erx[i].basic,
			use_riwt, priv->mode, (i == DMA_RX_SIZE - 1),
			priv->plat->dma_data->dma_buf_sz);
}

static void aicmac_dma_clear_tx_descriptors(struct aicmac_priv *priv, u32 queue)
{
	struct aicmac_tx_queue *tx_q = &priv->plat->tx_queue[queue];
	int i;

	/* Clear the TX descriptors */
	for (i = 0; i < DMA_TX_SIZE; i++)
		aicmac_dma_desc_init_tx_desc(&tx_q->dma_etx[i].basic,
				priv->mode, (i == DMA_TX_SIZE - 1));
}

static void aicmac_dma_clear_descriptors(struct aicmac_priv *priv)
{
	u32 rx_queue_cnt = priv->plat->rx_queues_to_use;
	u32 tx_queue_cnt = priv->plat->tx_queues_to_use;
	u32 queue;

	/* Clear the RX descriptors */
	for (queue = 0; queue < rx_queue_cnt; queue++)
		aicmac_dma_clear_rx_descriptors(priv, queue);

	/* Clear the TX descriptors */
	for (queue = 0; queue < tx_queue_cnt; queue++)
		aicmac_dma_clear_tx_descriptors(priv, queue);
}

static int aicmac_dma_init_rx_buffers(struct aicmac_priv *priv,
				      struct dma_desc *p, int i, gfp_t flags,
				      u32 queue)
{
	struct aicmac_rx_queue *rx_q = &priv->plat->rx_queue[queue];
	struct sk_buff *skb;
	unsigned int dma_buf_sz = priv->plat->dma_data->dma_buf_sz;

	skb = __netdev_alloc_skb_ip_align(priv->dev, dma_buf_sz, GFP_KERNEL);
	if (!skb)
		return -ENOMEM;

	rx_q->rx_skbuff[i] = skb;
	rx_q->rx_skbuff_dma[i] = dma_map_single(priv->device, skb->data,
						dma_buf_sz, DMA_FROM_DEVICE);

	if (dma_mapping_error(priv->device, rx_q->rx_skbuff_dma[i])) {
		netdev_err(priv->dev, "%s: DMA mapping error\n", __func__);
		dev_kfree_skb_any(skb);
		return -EINVAL;
	}

	aicmac_dma_desc_set_addr(p, rx_q->rx_skbuff_dma[i]);
	if (priv->plat->dma_data->dma_buf_sz == BUF_SIZE_16KiB) {
		if (priv->mode == AICMAC_CHAIN_MODE)
			aicmac_dma_chain_init_desc3(p);
		else
			aicmac_dma_ring_init_desc3(p);
	}

	return 0;
}

static int aicmac_dma_init_rx_desc_rings(struct net_device *dev, gfp_t flags)
{
	struct aicmac_priv *priv = netdev_priv(dev);
	u32 rx_count = priv->plat->rx_queues_to_use;
	int ret = -ENOMEM;
	int queue;
	int i;

	/* RX INITIALIZATION */
	netif_dbg(priv, probe, priv->dev,
		  "SKB addresses:\nskb\t\tskb data\tdma data\n");

	for (queue = 0; queue < rx_count; queue++) {
		struct aicmac_rx_queue *rx_q = &priv->plat->rx_queue[queue];

		aicmac_dma_clear_rx_descriptors(priv, queue);

		for (i = 0; i < DMA_RX_SIZE; i++) {
			struct dma_desc *p;

			p = &((rx_q->dma_erx + i)->basic);

			ret = aicmac_dma_init_rx_buffers(priv, p, i, flags,
							 queue);
			if (ret)
				goto err_init_rx_buffers;
		}

		rx_q->cur_rx = 0;
		rx_q->dirty_rx = (unsigned int)(i - DMA_RX_SIZE);

		rx_q->priv_data = priv;

		/* Setup the chained descriptor addresses */
		if (priv->mode == AICMAC_CHAIN_MODE) {
			aicmac_dma_chain_init(rx_q->dma_erx,
				rx_q->dma_rx_phy,
				DMA_RX_SIZE, 1);
		}
	}

	return 0;

err_init_rx_buffers:
	while (queue >= 0) {
		while (--i >= 0)
			aicmac_dma_free_rx_buffer(priv, queue, i);

		if (queue == 0)
			break;

		i = DMA_RX_SIZE;
		queue--;
	}

	return ret;
}

static int aicmac_dma_init_tx_desc_rings(struct net_device *dev)
{
	struct aicmac_priv *priv = netdev_priv(dev);
	u32 tx_queue_cnt = priv->plat->tx_queues_to_use;
	u32 queue;
	int i;

	for (queue = 0; queue < tx_queue_cnt; queue++) {
		struct aicmac_tx_queue *tx_q = &priv->plat->tx_queue[queue];

		/* Setup the chained descriptor addresses */
		if (priv->mode == AICMAC_CHAIN_MODE) {
			aicmac_dma_chain_init(tx_q->dma_etx,
					tx_q->dma_tx_phy,
					DMA_TX_SIZE, 1);
		}

		for (i = 0; i < DMA_TX_SIZE; i++) {
			struct dma_desc *p;

			p = &((tx_q->dma_etx + i)->basic);

			aicmac_dma_desc_clear(p);

			tx_q->tx_skbuff_dma[i].buf = 0;
			tx_q->tx_skbuff_dma[i].map_as_page = false;
			tx_q->tx_skbuff_dma[i].len = 0;
			tx_q->tx_skbuff_dma[i].last_segment = false;
			tx_q->tx_skbuff[i] = NULL;
		}

		tx_q->dirty_tx = 0;
		tx_q->cur_tx = 0;
		tx_q->mss = 0;

		netdev_tx_reset_queue(netdev_get_tx_queue(priv->dev, queue));
	}

	return 0;
}

int aicmac_dma_init_desc_rings(struct net_device *dev, gfp_t flags)
{
	struct aicmac_priv *priv = netdev_priv(dev);
	int ret;

	ret = aicmac_dma_init_rx_desc_rings(dev, flags);
	if (ret)
		return ret;

	ret = aicmac_dma_init_tx_desc_rings(dev);

	aicmac_dma_clear_descriptors(priv);

	if (netif_msg_hw(priv))
		aicmac_dma_display_rings(priv);

	return ret;
}

int aicmac_dma_init_engine(void *priv_ptr)
{
	struct aicmac_priv *priv = priv_ptr;
	u32 rx_channels_count = priv->plat->rx_queues_to_use;
	u32 tx_channels_count = priv->plat->tx_queues_to_use;
	struct aicmac_rx_queue *rx_q;
	struct aicmac_tx_queue *tx_q;
	u32 chan = 0;
	int atds = 0;
	int ret = 0;

	if (!priv->plat->dma_data || !priv->plat->dma_data->pbl) {
		dev_err(priv->device, "Invalid DMA configuration\n");
		return -EINVAL;
	}

	if (priv->extend_desc && priv->mode == AICMAC_RING_MODE)
		atds = 1;

	/* DMA Configuration */
	aicmac_dma_reg_init(priv->resource->ioaddr, priv->plat->dma_data, atds);

	/* DMA RX Channel Configuration */
	for (chan = 0; chan < rx_channels_count; chan++) {
		rx_q = &priv->plat->rx_queue[chan];

		aicmac_dma_reg_init_rx_chain(priv->resource->ioaddr,
					     rx_q->dma_rx_phy);
	}

	/* DMA TX Channel Configuration */
	for (chan = 0; chan < tx_channels_count; chan++) {
		tx_q = &priv->plat->tx_queue[chan];

		aicmac_dma_reg_init_tx_chain(priv->resource->ioaddr,
					     tx_q->dma_tx_phy);

		tx_q->tx_tail_addr = tx_q->dma_tx_phy;
	}

	return ret;
}

void aicmac_dma_operation_mode(void *priv_ptr)
{
	struct aicmac_priv *priv = priv_ptr;
	struct aicmac_dma_data *dma_data = priv->plat->dma_data;
	u32 rx_channels_count = priv->plat->rx_queues_to_use;
	u32 tx_channels_count = priv->plat->tx_queues_to_use;
	int rxfifosz = dma_data->rx_fifo_size;
	int txfifosz = dma_data->tx_fifo_size;
	u32 txmode = 0;
	u32 rxmode = 0;
	u32 chan = 0;
	u8 qmode = 0;

	if (rxfifosz == 0)
		rxfifosz = priv->plat->hw_cap.rx_fifo_size;
	if (txfifosz == 0)
		txfifosz = priv->plat->hw_cap.tx_fifo_size;

	/* Adjust for real per queue fifo size */
	rxfifosz /= rx_channels_count;
	txfifosz /= tx_channels_count;

	/* it will Transmit Underflow easily in TX_SF mode */
	txmode = SF_DMA_MODE;

	/* but higher bus utilization rate in RX threshold mode */
	rxmode = TC_DEFAULT;

	/* configure all channels */
	for (chan = 0; chan < rx_channels_count; chan++) {
		qmode = priv->plat->rx_queues_cfg[chan].mode_to_use;

		aicmac_dma_reg_operation_mode_rx(priv->resource->ioaddr, rxmode,
						 chan, rxfifosz, qmode);
	}

	for (chan = 0; chan < tx_channels_count; chan++) {
		qmode = priv->plat->tx_queues_cfg[chan].mode_to_use;

		aicmac_dma_reg_operation_mode_tx(priv->resource->ioaddr, txmode,
						 chan, txfifosz, qmode);
	}
}

void aicmac_dma_set_operation_mode(void *priv_ptr, u32 txmode, u32 rxmode,
				   u32 chan)
{
	struct aicmac_priv *priv = priv_ptr;
	u8 rxqmode = priv->plat->rx_queues_cfg[chan].mode_to_use;
	u8 txqmode = priv->plat->tx_queues_cfg[chan].mode_to_use;
	u32 rx_channels_count = priv->plat->rx_queues_to_use;
	u32 tx_channels_count = priv->plat->tx_queues_to_use;
	int rxfifosz = priv->plat->dma_data->rx_fifo_size;
	int txfifosz = priv->plat->dma_data->tx_fifo_size;

	if (rxfifosz == 0)
		rxfifosz = priv->plat->hw_cap.rx_fifo_size;
	if (txfifosz == 0)
		txfifosz = priv->plat->hw_cap.tx_fifo_size;

	/* Adjust for real per queue fifo size */
	rxfifosz /= rx_channels_count;
	txfifosz /= tx_channels_count;

	aicmac_dma_reg_operation_mode_rx(priv->resource->ioaddr, rxmode, chan,
					 rxfifosz, rxqmode);
	aicmac_dma_reg_operation_mode_tx(priv->resource->ioaddr, txmode, chan,
					 txfifosz, txqmode);
}

void aicmac_dma_start_all_dma(void *priv_ptr)
{
	struct aicmac_priv *priv = priv_ptr;
	u32 rx_channels_count = priv->plat->rx_queues_to_use;
	u32 tx_channels_count = priv->plat->tx_queues_to_use;
	u32 chan = 0;

	for (chan = 0; chan < rx_channels_count; chan++)
		aicmac_dma_reg_start_rx(priv->resource->ioaddr, chan);

	for (chan = 0; chan < tx_channels_count; chan++)
		aicmac_dma_reg_start_tx(priv->resource->ioaddr, chan);
}

void aicmac_dma_stop_all_dma(void *priv_ptr)
{
	struct aicmac_priv *priv = priv_ptr;
	u32 rx_channels_count = priv->plat->rx_queues_to_use;
	u32 tx_channels_count = priv->plat->tx_queues_to_use;
	u32 chan = 0;

	for (chan = 0; chan < rx_channels_count; chan++)
		aicmac_dma_reg_stop_rx(priv->resource->ioaddr, chan);

	for (chan = 0; chan < tx_channels_count; chan++)
		aicmac_dma_reg_stop_tx(priv->resource->ioaddr, chan);
}
