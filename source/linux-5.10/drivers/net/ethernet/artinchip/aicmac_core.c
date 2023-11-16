// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#include <linux/clk.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/crc32.h>
#include <linux/mii.h>
#include <linux/if.h>
#include <linux/if_vlan.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/prefetch.h>
#include <linux/net_tstamp.h>
#include <linux/pinctrl/consumer.h>

#include "aicmac.h"
#include "aicmac_core.h"
#include "aicmac_ethtool.h"
#include "aicmac_platform.h"
#include "aicmac_dma.h"
#include "aicmac_dma_desc.h"
#include "aicmac_dma_ring.h"
#include "aicmac_dma_chain.h"
#include "aicmac_dma_reg.h"
#include "aicmac_mac.h"
#include "aicmac_gmac_reg.h"
#include "aicmac_1588.h"
#include "aicmac_napi.h"
#include "aicmac_util.h"

static int tc = TC_DEFAULT;
static int buf_sz = DEFAULT_BUFSIZE;

#define AICMAC_ALIGN(x) ALIGN(ALIGN(x, SMP_CACHE_BYTES), 16)
#define AICMAC_COAL_TIMER(x) (jiffies + usecs_to_jiffies(x))

static int aicmac_set_features(struct net_device *dev,
			       netdev_features_t features)
{
	struct aicmac_priv *priv = netdev_priv(dev);

	return aicmac_mac_reg_rx_ipc_enable(priv->plat->mac_data);
}

static void aicmac_service_event_schedule(struct aicmac_priv *priv)
{
	if (!test_bit(AICMAC_DOWN, &priv->service_state) &&
	    !test_and_set_bit(AICMAC_SERVICE_SCHED, &priv->service_state))
		queue_work(priv->wq, &priv->service_task);
}

static void aicmac_global_err(struct aicmac_priv *priv)
{
	netif_carrier_off(priv->dev);
	set_bit(AICMAC_RESET_REQUESTED, &priv->service_state);
	aicmac_service_event_schedule(priv);
}

static inline u32 aicmac_tx_avail(struct aicmac_priv *priv, u32 queue)
{
	struct aicmac_tx_queue *tx_q = &priv->plat->tx_queue[queue];
	u32 avail;

	if (tx_q->dirty_tx > tx_q->cur_tx)
		avail = tx_q->dirty_tx - tx_q->cur_tx - 1;
	else
		avail = DMA_TX_SIZE - tx_q->cur_tx + tx_q->dirty_tx - 1;

	return avail;
}

static int aicmac_tx_clean(struct aicmac_priv *priv, int budget, u32 queue)
{
	struct aicmac_tx_queue *tx_q = &priv->plat->tx_queue[queue];
	unsigned int bytes_compl = 0, pkts_compl = 0;
	unsigned int entry, count = 0;

	__netif_tx_lock_bh(netdev_get_tx_queue(priv->dev, queue));

	entry = tx_q->dirty_tx;
	while ((entry != tx_q->cur_tx) && (count < budget)) {
		struct sk_buff *skb = tx_q->tx_skbuff[entry];
		struct dma_desc *p = NULL;
		int status;

		p = (struct dma_desc *)(tx_q->dma_etx + entry);

		status = aicmac_dma_desc_get_tx_status(p,
						       priv->resource->ioaddr);
		/* Check if the descriptor is owned by the DMA */
		if (unlikely(status & tx_dma_own))
			break;

		count++;

		/* Make sure descriptor fields are read after reading
		 * the own bit.
		 */
		dma_rmb();

		/* Just consider the last segment and ...*/
		if (likely(!(status & tx_not_ls)))
			aicmac_1588_get_tx_hwtstamp(priv, p, skb);

		if (likely(tx_q->tx_skbuff_dma[entry].buf)) {
			if (tx_q->tx_skbuff_dma[entry].map_as_page)
				dma_unmap_page(priv->device,
					       tx_q->tx_skbuff_dma[entry].buf,
					       tx_q->tx_skbuff_dma[entry].len,
					       DMA_TO_DEVICE);
			else
				dma_unmap_single(priv->device,
						 tx_q->tx_skbuff_dma[entry].buf,
						 tx_q->tx_skbuff_dma[entry].len,
						 DMA_TO_DEVICE);
			tx_q->tx_skbuff_dma[entry].buf = 0;
			tx_q->tx_skbuff_dma[entry].len = 0;
			tx_q->tx_skbuff_dma[entry].map_as_page = false;
		}

		if (priv->mode == AICMAC_CHAIN_MODE)
			aicmac_dma_chain_clean_desc3(priv, p);
		else
			aicmac_dma_ring_clean_desc3(tx_q, p);

		tx_q->tx_skbuff_dma[entry].last_segment = false;
		tx_q->tx_skbuff_dma[entry].is_jumbo = false;

		if (skb) {
			pkts_compl++;
			bytes_compl += skb->len;
			dev_consume_skb_any(skb);
			tx_q->tx_skbuff[entry] = NULL;
		}

		aicmac_dma_desc_release_tx_desc(p, priv->mode);

		entry = AICMAC_GET_ENTRY(entry, DMA_TX_SIZE);
	}
	tx_q->dirty_tx = entry;

	netdev_tx_completed_queue(netdev_get_tx_queue(priv->dev, queue),
				  pkts_compl, bytes_compl);

	if (unlikely(netif_tx_queue_stopped(netdev_get_tx_queue(priv->dev,
								queue))) &&
	    aicmac_tx_avail(priv, queue) > AICMAC_TX_THRESH) {
		netif_dbg(priv, tx_done, priv->dev, "%s: restart transmit\n",
			  __func__);
		netif_tx_wake_queue(netdev_get_tx_queue(priv->dev, queue));
	}

	/* We still have pending packets, let's call for a new scheduling */
	if (tx_q->dirty_tx != tx_q->cur_tx)
		mod_timer(&tx_q->txtimer, AICMAC_COAL_TIMER(10));

	__netif_tx_unlock_bh(netdev_get_tx_queue(priv->dev, queue));

	return count;
}

static inline u32 aicmac_rx_dirty(struct aicmac_priv *priv, u32 queue)
{
	struct aicmac_rx_queue *rx_q = &priv->plat->rx_queue[queue];
	u32 dirty;

	if (rx_q->dirty_rx <= rx_q->cur_rx)
		dirty = rx_q->cur_rx - rx_q->dirty_rx;
	else
		dirty = DMA_RX_SIZE - rx_q->dirty_rx + rx_q->cur_rx;

	return dirty;
}

static void aicmac_tx_timer_arm(struct aicmac_priv *priv, u32 queue)
{
	struct aicmac_tx_queue *tx_q = &priv->plat->tx_queue[queue];

	mod_timer(&tx_q->txtimer, AICMAC_COAL_TIMER(priv->tx_coal_timer));
}

static void aicmac_tx_timer(struct timer_list *t)
{
	struct aicmac_tx_queue *tx_q = from_timer(tx_q, t, txtimer);
	struct aicmac_priv *priv = tx_q->priv_data;
	struct aicmac_channel *ch;
	unsigned long flags;

	ch = &priv->plat->channel[tx_q->queue_index];

	if (likely(napi_schedule_prep(&ch->tx_napi))) {
		spin_lock_irqsave(&ch->lock, flags);
		aicmac_dma_reg_disable_irq(priv->resource->ioaddr, ch->index, 0,
					   1);
		spin_unlock_irqrestore(&ch->lock, flags);
		__napi_schedule(&ch->tx_napi);
	}
}

static void aicmac_tx_err(struct aicmac_priv *priv, u32 chan)
{
	struct aicmac_tx_queue *tx_q = &priv->plat->tx_queue[chan];
	int i;

	netif_tx_stop_queue(netdev_get_tx_queue(priv->dev, chan));
	aicmac_dma_reg_stop_tx(priv, chan);
	aicmac_dma_free_tx_skbufs(priv, chan);

	for (i = 0; i < DMA_TX_SIZE; i++)
		aicmac_dma_desc_init_tx_desc(&tx_q->dma_etx[i].basic,
					     priv->mode,
					     (i == DMA_TX_SIZE - 1));

	tx_q->dirty_tx = 0;
	tx_q->cur_tx = 0;
	tx_q->mss = 0;
	netdev_tx_reset_queue(netdev_get_tx_queue(priv->dev, chan));
	aicmac_dma_reg_start_tx(priv->resource->ioaddr, chan);

	priv->dev->stats.tx_errors++;
	netif_tx_wake_queue(netdev_get_tx_queue(priv->dev, chan));
}

static void aicmac_tx_timeout(struct net_device *dev, unsigned int txqueue)
{
	struct aicmac_priv *priv = netdev_priv(dev);

	aicmac_global_err(priv);
}

static inline void aicmac_rx_refill(struct aicmac_priv *priv, u32 queue)
{
	struct aicmac_rx_queue *rx_q = &priv->plat->rx_queue[queue];
	int len, dirty = aicmac_rx_dirty(priv, queue);
	unsigned int entry = rx_q->dirty_rx;

	len = DIV_ROUND_UP(priv->plat->dma_data->dma_buf_sz, PAGE_SIZE) *
	      PAGE_SIZE;
	while (dirty-- > 0) {
		struct aicmac_rx_buffer *buf = &rx_q->buf_pool[entry];
		struct dma_desc *p;
		bool use_rx_wd;

		p = (struct dma_desc *)(rx_q->dma_erx + entry);

		if (!buf->page) {
			buf->page = page_pool_dev_alloc_pages(rx_q->page_pool);
			if (!buf->page)
				break;
		}

		if (!buf->sec_page) {
			buf->sec_page =
				page_pool_dev_alloc_pages(rx_q->page_pool);
			if (!buf->sec_page)
				break;

			buf->sec_addr = page_pool_get_dma_addr(buf->sec_page);

			dma_sync_single_for_device(priv->device, buf->sec_addr,
						   len, DMA_FROM_DEVICE);
		}

		buf->addr = page_pool_get_dma_addr(buf->page);

		/* Sync whole allocation to device. This will invalidate old
		 * data.
		 */
		dma_sync_single_for_device(priv->device, buf->addr, len,
					   DMA_FROM_DEVICE);

		aicmac_dma_desc_set_addr(p, buf->addr);
		aicmac_dma_desc_set_sec_addr(p, buf->sec_addr);
		if (priv->mode == AICMAC_CHAIN_MODE)
			aicmac_dma_chain_refill_desc3(rx_q, p);
		else
			aicmac_dma_ring_refill_desc3(rx_q, p);

		rx_q->rx_count_frames++;
		rx_q->rx_count_frames += priv->rx_coal_frames;
		if (rx_q->rx_count_frames > priv->rx_coal_frames)
			rx_q->rx_count_frames = 0;
		use_rx_wd = rx_q->rx_count_frames;

		dma_wmb();

		aicmac_dma_desc_set_rx_owner(p, use_rx_wd);

		entry = AICMAC_GET_ENTRY(entry, DMA_RX_SIZE);
	}
	rx_q->dirty_rx = entry;
	rx_q->rx_tail_addr =
		rx_q->dma_rx_phy + (rx_q->dirty_rx * sizeof(struct dma_desc));
}

static int aicmac_rx(struct aicmac_priv *priv, int limit, u32 queue)
{
	struct aicmac_rx_queue *rx_q = &priv->plat->rx_queue[queue];
	struct aicmac_channel *ch = &priv->plat->channel[queue];
	unsigned int count = 0, error = 0, len = 0;
	int status = 0, coe = 0;
	unsigned int next_entry = rx_q->cur_rx;
	struct sk_buff *skb = NULL;

	while (count < limit) {
		unsigned int hlen = 0, prev_len = 0;
		enum pkt_hash_types hash_type;
		struct aicmac_rx_buffer *buf;
		struct dma_desc *np, *p;
		unsigned int sec_len;
		int entry;
		u32 hash;

		if (!count && rx_q->state_saved) {
			skb = rx_q->state.skb;
			error = rx_q->state.error;
			len = rx_q->state.len;
		} else {
			rx_q->state_saved = false;
			skb = NULL;
			error = 0;
			len = 0;
		}

		if (count >= limit)
			break;

read_again:
		sec_len = 0;
		entry = next_entry;
		buf = &rx_q->buf_pool[entry];

		p = (struct dma_desc *)(rx_q->dma_erx + entry);

		/* read the status of the incoming frame */
		status = aicmac_dma_desc_get_rx_status(p,
						       priv->resource->ioaddr);
		/* check if managed by the DMA otherwise go ahead */
		if (unlikely(status & dma_own))
			break;

		rx_q->cur_rx = AICMAC_GET_ENTRY(rx_q->cur_rx, DMA_RX_SIZE);
		next_entry = rx_q->cur_rx;

		np = (struct dma_desc *)(rx_q->dma_erx + next_entry);

		prefetch(np);
		prefetch(page_address(buf->page));

		if (unlikely(status == discard_frame)) {
			page_pool_recycle_direct(rx_q->page_pool, buf->page);
			buf->page = NULL;
			error = 1;
			if (!priv->plat->ptp_data->hwts_rx_en)
				priv->dev->stats.rx_errors++;
		}

		if (unlikely(error && (status & rx_not_ls)))
			goto read_again;
		if (unlikely(error)) {
			dev_kfree_skb(skb);
			count++;
			continue;
		}

		/* Buffer is good. Go on. */

		if (likely(status & rx_not_ls)) {
			len += priv->plat->dma_data->dma_buf_sz;
		} else {
			prev_len = len;
			len = aicmac_dma_desc_get_rx_frame_len(p, coe);
		}

		if (!skb) {
			if (hlen > 0) {
				sec_len = len;
				if (!(status & rx_not_ls))
					sec_len = sec_len - hlen;
				len = hlen;

				prefetch(page_address(buf->sec_page));
			}

			skb = napi_alloc_skb(&ch->rx_napi, len);
			if (!skb) {
				count++;
				continue;
			}

			dma_sync_single_for_cpu(priv->device, buf->addr, len,
						DMA_FROM_DEVICE);
			skb_copy_to_linear_data(skb, page_address(buf->page),
						len);
			skb_put(skb, len);

			page_pool_recycle_direct(rx_q->page_pool, buf->page);
			buf->page = NULL;
		} else {
			unsigned int buf_len = len - prev_len;

			if (likely(status & rx_not_ls))
				buf_len = priv->plat->dma_data->dma_buf_sz;

			dma_sync_single_for_cpu(priv->device, buf->addr,
						buf_len, DMA_FROM_DEVICE);
			skb_add_rx_frag(skb, skb_shinfo(skb)->nr_frags,
					buf->page, 0, buf_len,
					priv->plat->dma_data->dma_buf_sz);

			/* Data payload appended into SKB */
			page_pool_release_page(rx_q->page_pool, buf->page);
			buf->page = NULL;
		}

		if (sec_len > 0) {
			dma_sync_single_for_cpu(priv->device, buf->sec_addr,
						sec_len, DMA_FROM_DEVICE);
			skb_add_rx_frag(skb, skb_shinfo(skb)->nr_frags,
					buf->sec_page, 0, sec_len,
					priv->plat->dma_data->dma_buf_sz);

			len += sec_len;

			/* Data payload appended into SKB */
			page_pool_release_page(rx_q->page_pool, buf->sec_page);
			buf->sec_page = NULL;
		}

		if (likely(status & rx_not_ls))
			goto read_again;

		/* Got entire packet into SKB. Finish it. */

		aicmac_1588_get_rx_hwtstamp(priv, p, np, skb);
		skb->protocol = eth_type_trans(skb, priv->dev);

		if (unlikely(!coe))
			skb_checksum_none_assert(skb);
		else
			skb->ip_summed = CHECKSUM_UNNECESSARY;

		skb_set_hash(skb, hash, hash_type);

		skb_record_rx_queue(skb, queue);
		napi_gro_receive(&ch->rx_napi, skb);

		priv->dev->stats.rx_packets++;
		priv->dev->stats.rx_bytes += len;
		count++;
	}

	if (status & rx_not_ls) {
		rx_q->state_saved = true;
		rx_q->state.skb = skb;
		rx_q->state.error = error;
		rx_q->state.len = len;
	}

	aicmac_rx_refill(priv, queue);
	return count;
}

static int aicmac_napi_poll_rx(struct napi_struct *napi, int budget)
{
	struct aicmac_channel *ch =
		container_of(napi, struct aicmac_channel, rx_napi);
	struct aicmac_priv *priv = ch->priv_data;
	u32 chan = ch->index;
	unsigned long flags;
	int work_done;

	work_done = aicmac_rx(priv, budget, chan);
	if (work_done < budget && napi_complete_done(napi, work_done)) {
		spin_lock_irqsave(&ch->lock, flags);
		aicmac_dma_reg_enable_irq(priv->resource->ioaddr, chan, 1, 0);
		spin_unlock_irqrestore(&ch->lock, flags);
	}

	return work_done;
}

static int aicmac_napi_poll_tx(struct napi_struct *napi, int budget)
{
	struct aicmac_channel *ch =
		container_of(napi, struct aicmac_channel, tx_napi);
	struct aicmac_priv *priv = ch->priv_data;
	u32 chan = ch->index;
	int work_done;

	work_done = aicmac_tx_clean(priv, DMA_TX_SIZE, chan);
	work_done = min(work_done, budget);

	if (work_done < budget && napi_complete_done(napi, work_done)) {
		unsigned long flags;

		spin_lock_irqsave(&ch->lock, flags);
		aicmac_dma_reg_enable_irq(priv->resource->ioaddr, chan, 0, 1);
		spin_unlock_irqrestore(&ch->lock, flags);
	}

	return work_done;
}

static void aicmac_reset_subtask(struct aicmac_priv *priv)
{
	if (!test_and_clear_bit(AICMAC_RESET_REQUESTED, &priv->service_state))
		return;
	if (test_bit(AICMAC_DOWN, &priv->service_state))
		return;

	netdev_err(priv->dev, "Reset adapter.\n");

	rtnl_lock();
	netif_trans_update(priv->dev);
	while (test_and_set_bit(AICMAC_RESETTING, &priv->service_state))
		usleep_range(1000, 2000);

	set_bit(AICMAC_DOWN, &priv->service_state);
	dev_close(priv->dev);
	dev_open(priv->dev, NULL);
	clear_bit(AICMAC_DOWN, &priv->service_state);
	clear_bit(AICMAC_RESETTING, &priv->service_state);
	rtnl_unlock();
}

static void aicmac_service_task(struct work_struct *work)
{
	struct aicmac_priv *priv =
		container_of(work, struct aicmac_priv, service_task);

	aicmac_reset_subtask(priv);
	clear_bit(AICMAC_SERVICE_SCHED, &priv->service_state);
}

int aicmac_core_destroy(struct aicmac_priv *priv)
{
	struct net_device *ndev = priv->dev;
	struct aicmac_platform_data *plat = priv->plat;

	aicmac_dma_stop_all_dma(priv);

	aicmac_mac_reg_enable_mac(priv->resource->ioaddr, false);

	netif_carrier_off(ndev);
	unregister_netdev(ndev);

	phylink_destroy(plat->phy_data->phylink);
	if (priv->plat->aicmac_rst)
		reset_control_assert(plat->aicmac_rst);
	clk_disable_unprepare(plat->pclk);
	clk_disable_unprepare(plat->aicmac_clk);
	if (plat->mac_data->pcs != AICMAC_PCS_RGMII)
		aicmac_mdio_unregister(ndev);
	destroy_workqueue(priv->wq);
	mutex_destroy(&priv->lock);

	return 0;
}

static int aicmac_hw_setup(struct net_device *dev)
{
	struct aicmac_priv *priv = netdev_priv(dev);
	int speed = SPEED_100;
	int ret;

	ret = aicmac_dma_init_engine(priv);
	if (ret < 0) {
		netdev_err(priv->dev, "%s: DMA engine initialization failed\n",
			   __func__);
		return ret;
	}

	/* Copy the MAC addr into the HW  */
	aicmac_mac_reg_set_umac_addr(priv->plat->mac_data, dev->dev_addr, 0);

	speed = priv->plat->mac_data->mac_port_sel_speed;

	if (speed == SPEED_10 || speed == SPEED_100 ||
	    speed == SPEED_1000) {
		priv->plat->mac_data->ps = speed;
	} else {
		dev_warn(priv->device, "invalid port speed\n");
		priv->plat->mac_data->ps = SPEED_100;
	}

	/* Initialize the MAC Core */
	aicmac_mac_reg_core_init(priv->plat->mac_data, dev);

	ret = aicmac_mac_reg_rx_ipc_enable(priv->plat->mac_data);
	if (!ret) {
		netdev_warn(priv->dev, "RX IPC Checksum Offload disabled\n");
		priv->plat->dma_data->rx_coe = AICMAC_RX_COE_NONE;
	}

	/* Enable the MAC Rx/Tx */
	aicmac_mac_reg_enable_mac(priv->resource->ioaddr, true);

	/* Set the HW DMA mode and the COE */
	aicmac_dma_operation_mode(priv);

	ret = clk_prepare_enable(priv->plat->ptp_data->clk_ptp_ref);
	if (ret < 0)
		netdev_warn(priv->dev,
			    "failed to enable PTP reference clock: %d\n", ret);

	aicmac_1588_register(priv);

	/* Configure real RX and TX queues */
	netif_set_real_num_rx_queues(priv->dev, priv->plat->rx_queues_to_use);
	netif_set_real_num_tx_queues(priv->dev, priv->plat->tx_queues_to_use);
	/* Start the ball rolling... */
	aicmac_dma_start_all_dma(priv);

	return 0;
}

static void aicmac_hw_teardown(struct net_device *dev)
{
	struct aicmac_priv *priv = netdev_priv(dev);

	clk_disable_unprepare(priv->plat->ptp_data->clk_ptp_ref);
}

/*init mitigation options*/
static void aicmac_init_coalesce(struct aicmac_priv *priv)
{
	u32 tx_channel_count = priv->plat->tx_queues_to_use;
	u32 chan;

	priv->tx_coal_frames = AICMAC_TX_FRAMES;
	priv->tx_coal_timer = AICMAC_COAL_TX_TIMER;
	priv->rx_coal_frames = AICMAC_RX_FRAMES;

	for (chan = 0; chan < tx_channel_count; chan++) {
		struct aicmac_tx_queue *tx_q = &priv->plat->tx_queue[chan];

		timer_setup(&tx_q->txtimer, aicmac_tx_timer, 0);
	}
}

static int aicmac_napi_check(struct aicmac_priv *priv, u32 chan)
{
	int status =
		aicmac_dma_reg_interrupt_status(priv->resource->ioaddr, chan);
	struct aicmac_channel *ch = &priv->plat->channel[chan];
	unsigned long flags;

	if ((status & handle_rx) && chan < priv->plat->rx_queues_to_use) {
		if (napi_schedule_prep(&ch->rx_napi)) {
			spin_lock_irqsave(&ch->lock, flags);
			aicmac_dma_reg_disable_irq(priv->resource->ioaddr, chan,
						   1, 0);
			spin_unlock_irqrestore(&ch->lock, flags);
			__napi_schedule(&ch->rx_napi);
		}
	}

	if ((status & handle_tx) && chan < priv->plat->tx_queues_to_use) {
		if (napi_schedule_prep(&ch->tx_napi)) {
			spin_lock_irqsave(&ch->lock, flags);
			aicmac_dma_reg_disable_irq(priv->resource->ioaddr, chan,
						   0, 1);
			spin_unlock_irqrestore(&ch->lock, flags);
			__napi_schedule(&ch->tx_napi);
		}
	}

	return status;
}

static irqreturn_t aicmac_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = (struct net_device *)dev_id;
	struct aicmac_priv *priv = netdev_priv(dev);

	u32 tx_channel_count = priv->plat->tx_queues_to_use;
	u32 rx_channel_count = priv->plat->rx_queues_to_use;
	u32 channels_to_check = tx_channel_count > rx_channel_count ?
					tx_channel_count :
					rx_channel_count;
	u32 chan;
	int status[max_t(u32, MTL_MAX_TX_QUEUES, MTL_MAX_RX_QUEUES)];

	/* Check if adapter is up */
	if (test_bit(AICMAC_DOWN, &priv->service_state))
		return IRQ_HANDLED;

	if (priv->link_state == LINK_STATE_UP)
		netif_carrier_on(dev);
	else
		netif_carrier_off(dev);

	/* Make sure we never check beyond our status buffer. */
	if (WARN_ON_ONCE(channels_to_check > ARRAY_SIZE(status)))
		channels_to_check = ARRAY_SIZE(status);

	for (chan = 0; chan < channels_to_check; chan++)
		status[chan] = aicmac_napi_check(priv, chan);

	for (chan = 0; chan < tx_channel_count; chan++) {
		if (unlikely(status[chan] & tx_hard_error_bump_tc)) {
			/* Try to bump up the dma threshold on this failure */
			if (tc <= 256) {
				tc += 64;
				if (priv->plat->dma_data->force_thresh_dma_mode)
					aicmac_dma_set_operation_mode(priv, tc,
						tc, chan);
				else
					aicmac_dma_set_operation_mode(priv,
						tc, SF_DMA_MODE, chan);
			}
		} else if (unlikely(status[chan] == tx_hard_error)) {
			aicmac_tx_err(priv, chan);
		}
	}

	return IRQ_HANDLED;
}

static void aicmac_enable_all_queues(struct aicmac_priv *priv)
{
	u32 rx_queues_cnt = priv->plat->rx_queues_to_use;
	u32 tx_queues_cnt = priv->plat->tx_queues_to_use;
	u32 maxq = max(rx_queues_cnt, tx_queues_cnt);
	u32 queue;

	for (queue = 0; queue < maxq; queue++) {
		struct aicmac_channel *ch = &priv->plat->channel[queue];

		if (queue < rx_queues_cnt)
			napi_enable(&ch->rx_napi);
		if (queue < tx_queues_cnt)
			napi_enable(&ch->tx_napi);
	}
}

static void aicmac_disable_all_queues(struct aicmac_priv *priv)
{
	u32 rx_queues_cnt = priv->plat->rx_queues_to_use;
	u32 tx_queues_cnt = priv->plat->tx_queues_to_use;
	u32 maxq = max(rx_queues_cnt, tx_queues_cnt);
	u32 queue;

	for (queue = 0; queue < maxq; queue++) {
		struct aicmac_channel *ch = &priv->plat->channel[queue];

		if (queue < rx_queues_cnt)
			napi_disable(&ch->rx_napi);
		if (queue < tx_queues_cnt)
			napi_disable(&ch->tx_napi);
	}
}

static void aicmac_stop_all_queues(struct aicmac_priv *priv)
{
	u32 tx_queues_cnt = priv->plat->tx_queues_to_use;
	u32 queue;

	for (queue = 0; queue < tx_queues_cnt; queue++)
		netif_tx_stop_queue(netdev_get_tx_queue(priv->dev, queue));
}

static int aicmac_open(struct net_device *dev)
{
	struct aicmac_priv *priv = netdev_priv(dev);
	int bfsize = 0;
	u32 chan;
	int ret;

	if (priv->plat->phy_gpio) {
		aicmac_phy_reset(priv->plat->phy_gpio);
	}

	ret = aicmac_phy_connect(dev);
	if (ret) {
		netdev_err(priv->dev, "%s: Cannot attach to PHY (error: %d)\n",
			   __func__, ret);
		return ret;
	}

	ret = aicmac_mac_reg_reset(priv->resource->ioaddr);
	if (ret < 0) {
		netdev_err(priv->dev, "%s: mac reset failed\n", __func__);
		return ret;
	}

	msleep(50);
	aicmac_mac_reg_core_init(priv->plat->mac_data, priv->dev);

	if (priv->mode == AICMAC_CHAIN_MODE)
		bfsize = aicmac_dma_chain_set_16kib_bfsize(dev->mtu);
	else
		bfsize = aicmac_dma_ring_set_16kib_bfsize(dev->mtu);

	if (bfsize < 0)
		bfsize = 0;

	if (bfsize < BUF_SIZE_16KiB) {
		if (priv->mode == AICMAC_CHAIN_MODE)
			bfsize = aicmac_dma_chain_set_bfsize(dev->mtu,
				priv->plat->dma_data->dma_buf_sz);
		else
			bfsize = aicmac_dma_ring_set_bfsize(dev->mtu,
				priv->plat->dma_data->dma_buf_sz);
	}

	priv->plat->dma_data->dma_buf_sz = bfsize;
	buf_sz = bfsize;

	ret = aicmac_dma_alloc_dma_desc_resources(priv);
	if (ret < 0) {
		netdev_err(priv->dev,
			   "%s: DMA descriptors allocation failed.\n",
			   __func__);
		goto dma_desc_error;
	}

	ret = aicmac_dma_init_desc_rings(dev, GFP_KERNEL);
	if (ret < 0) {
		netdev_err(priv->dev,
			   "%s: DMA descriptors initialization failed.\n",
			   __func__);
		goto init_error;
	}

	msleep(50);
	ret = aicmac_hw_setup(dev);
	if (ret < 0) {
		netdev_err(priv->dev, "%s: Hw setup failed.\n", __func__);
		goto init_error;
	}
	aicmac_init_coalesce(priv);

	phylink_start(priv->plat->phy_data->phylink);

	ret = request_irq(dev->irq, aicmac_interrupt, IRQF_SHARED, dev->name,
			  dev);
	if (unlikely(ret < 0)) {
		netdev_err(priv->dev,
			   "%s: ERROR: allocating the IRQ %d (error: %d)\n",
			   __func__, dev->irq, ret);
		goto irq_error;
	}

	/* Request the Wake IRQ in case of another line is used for WoL */
	if (priv->resource->wol_irq > 0 &&
	    priv->resource->wol_irq != dev->irq) {
		ret = request_irq(priv->resource->wol_irq, aicmac_interrupt,
				  IRQF_SHARED, dev->name, dev);
		if (unlikely(ret < 0)) {
			netdev_err(priv->dev,
				"%s: ERROR: allocating the WoL IRQ %d (%d)\n",
				__func__, priv->resource->wol_irq, ret);
			goto wolirq_error;
		}
	}

	/* Request the IRQ lines */
	if (priv->resource->lpi_irq > 0) {
		ret = request_irq(priv->resource->lpi_irq, aicmac_interrupt,
				  IRQF_SHARED, dev->name, dev);
		if (unlikely(ret < 0)) {
			netdev_err(priv->dev,
				"%s: ERROR: allocating the LPI IRQ %d (%d)\n",
				__func__, priv->resource->lpi_irq, ret);
			goto lpiirq_error;
		}
	}

	aicmac_enable_all_queues(priv);
	netif_tx_start_all_queues(priv->dev);

#ifdef CONFIG_ARTINCHIP_GMAC_DEBUG
	aicmac_print_reg("open", priv->resource->ioaddr, AICMAC_GMAC_REGS_NUM);
#endif

	return 0;

lpiirq_error:
	if (priv->resource->wol_irq != dev->irq)
		free_irq(priv->resource->wol_irq, dev);
wolirq_error:
	free_irq(dev->irq, dev);
irq_error:
	phylink_stop(priv->plat->phy_data->phylink);

	for (chan = 0; chan < priv->plat->tx_queues_to_use; chan++)
		del_timer_sync(&priv->plat->tx_queue[chan].txtimer);

	aicmac_hw_teardown(dev);
init_error:
	aicmac_dma_free_desc_resources(priv);
dma_desc_error:
	phylink_disconnect_phy(priv->plat->phy_data->phylink);

	return ret;
}

static int aicmac_release(struct net_device *dev)
{
	struct aicmac_priv *priv = netdev_priv(dev);
	u32 chan;

	if (device_may_wakeup(priv->device))
		phylink_speed_down(priv->plat->phy_data->phylink, false);
	/* Stop and disconnect the PHY */
	phylink_stop(priv->plat->phy_data->phylink);
	phylink_disconnect_phy(priv->plat->phy_data->phylink);

	aicmac_stop_all_queues(priv);

	aicmac_disable_all_queues(priv);

	for (chan = 0; chan < priv->plat->tx_queues_to_use; chan++)
		del_timer_sync(&priv->plat->tx_queue[chan].txtimer);

	/* Free the IRQ lines */
	free_irq(dev->irq, dev);
	if (priv->resource->wol_irq != dev->irq)
		free_irq(priv->resource->wol_irq, dev);
	if (priv->resource->lpi_irq > 0)
		free_irq(priv->resource->lpi_irq, dev);

	/* Stop TX/RX DMA and clear the descriptors */
	aicmac_dma_stop_all_dma(priv);

	/* Release and free the Rx/Tx resources */
	aicmac_dma_free_desc_resources(priv);

	/* Disable the MAC Rx/Tx */
	aicmac_mac_reg_enable_mac(priv->resource->ioaddr, false);

	netif_carrier_off(dev);

	aicmac_1588_destroy(priv);

	return 0;
}

static int aicmac_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct aicmac_priv *priv = netdev_priv(dev);
	unsigned int nopaged_len = skb_headlen(skb);
	int i, csum_insertion = 0, is_jumbo = 0;
	u32 queue = skb_get_queue_mapping(skb);
	int nfrags = skb_shinfo(skb)->nr_frags;
	struct dma_desc *desc, *first;
	struct aicmac_tx_queue *tx_q;
	unsigned int first_entry, tx_packets;
	unsigned int enh_desc;
	dma_addr_t des;
	bool set_ic;
	int entry, first_tx, desc_size;

	tx_q = &priv->plat->tx_queue[queue];
	first_tx = tx_q->cur_tx;

	if (unlikely(aicmac_tx_avail(priv, queue) < nfrags + 1)) {
		if (!netif_tx_queue_stopped(netdev_get_tx_queue(dev, queue))) {
			netif_tx_stop_queue(netdev_get_tx_queue(priv->dev,
								queue));
			/* This is a hard error, log it. */
			netdev_err(priv->dev,
				   "%s: Tx Ring full when queue awake\n",
				   __func__);
		}
		netdev_err(priv->dev, "%s: xmit failed with NETDEV_TX_BUSY\n",
			   __func__);
		return NETDEV_TX_BUSY;
	}

	entry = tx_q->cur_tx;
	first_entry = entry;
	WARN_ON(tx_q->tx_skbuff[first_entry]);

	csum_insertion = (skb->ip_summed == CHECKSUM_PARTIAL);

	desc = (struct dma_desc *)(tx_q->dma_etx + entry);

	first = desc;

	enh_desc = priv->plat->hw_cap.enh_desc;
	/* To program the descriptors according to the size of the frame */
	if (enh_desc) {
		if (priv->mode == AICMAC_CHAIN_MODE)
			is_jumbo = aicmac_dma_ring_is_jumbo_frm(skb->len,
								enh_desc);
		else
			is_jumbo = aicmac_dma_ring_is_jumbo_frm(skb->len,
								enh_desc);
	}

	if (unlikely(is_jumbo)) {
		if (priv->mode == AICMAC_CHAIN_MODE)
			entry = aicmac_dma_chain_jumbo_frm(tx_q, skb,
							   csum_insertion);
		else
			entry = aicmac_dma_ring_jumbo_frm(tx_q, skb,
							  csum_insertion);

		if (unlikely(entry < 0) && (entry != -EINVAL))
			goto dma_map_err;
	}

	for (i = 0; i < nfrags; i++) {
		const skb_frag_t *frag = &skb_shinfo(skb)->frags[i];
		int len = skb_frag_size(frag);
		bool last_segment = (i == (nfrags - 1));

		entry = AICMAC_GET_ENTRY(entry, DMA_TX_SIZE);
		WARN_ON(tx_q->tx_skbuff[entry]);

		desc = (struct dma_desc *)(tx_q->dma_etx + entry);

		des = skb_frag_dma_map(priv->device, frag, 0, len,
				       DMA_TO_DEVICE);
		if (dma_mapping_error(priv->device, des))
			goto dma_map_err; /* should reuse desc w/o issues */

		tx_q->tx_skbuff_dma[entry].buf = des;

		aicmac_dma_desc_set_addr(desc, des);

		tx_q->tx_skbuff_dma[entry].map_as_page = true;
		tx_q->tx_skbuff_dma[entry].len = len;
		tx_q->tx_skbuff_dma[entry].last_segment = last_segment;

		/* Prepare the descriptor and set the own bit too */
		aicmac_dma_desc_prepare_tx_desc(desc, 0, len, csum_insertion,
						priv->mode, 1, last_segment,
						skb->len);
	}

	/* Only the last descriptor gets to point to the skb. */
	tx_q->tx_skbuff[entry] = skb;

	/* According to the coalesce parameter the IC bit for the latest
	 * segment is reset and the timer re-started to clean the tx status.
	 * This approach takes care about the fragments: desc is the first
	 * element in case of no SG.
	 */
	tx_packets = (entry + 1) - first_tx;
	tx_q->tx_count_frames += tx_packets;

	if ((skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP) &&
	    priv->plat->ptp_data->hwts_tx_en)
		set_ic = true;
	else if (!priv->tx_coal_frames)
		set_ic = false;
	else if (tx_packets > priv->tx_coal_frames)
		set_ic = true;
	else if ((tx_q->tx_count_frames % priv->tx_coal_frames) < tx_packets)
		set_ic = true;
	else
		set_ic = false;

	if (set_ic) {
		desc = &tx_q->dma_etx[entry].basic;
		tx_q->tx_count_frames = 0;
		aicmac_dma_desc_set_tx_ic(desc);
	}

	/* We've used all descriptors we need for this skb, however,
	 * advance cur_tx so that it references a fresh descriptor.
	 * ndo_start_xmit will fill this descriptor the next time it's
	 * called and aicmac_tx_clean may clean up to this descriptor.
	 */
	entry = AICMAC_GET_ENTRY(entry, DMA_TX_SIZE);
	tx_q->cur_tx = entry;

	if (netif_msg_pktdata(priv)) {
		void *tx_head;

		netdev_dbg(priv->dev,
			"%s: curr=%d dirty=%d f=%d, e=%d, first=%p, nfrags=%d",
			__func__, tx_q->cur_tx, tx_q->dirty_tx, first_entry,
			entry, first, nfrags);

		tx_head = (void *)tx_q->dma_etx;

		netdev_dbg(priv->dev, ">>> frame to be transmitted: ");
	}

	if (unlikely(aicmac_tx_avail(priv, queue) <= (MAX_SKB_FRAGS + 1)))
		netif_tx_stop_queue(netdev_get_tx_queue(priv->dev, queue));

	dev->stats.tx_bytes += skb->len;
	skb_tx_timestamp(skb);

	/* Ready to fill the first descriptor and set the OWN bit w/o any
	 * problems because all the descriptors are actually ready to be
	 * passed to the DMA engine.
	 */
	if (likely(!is_jumbo)) {
		bool last_segment = (nfrags == 0);

		des = dma_map_single(priv->device, skb->data, nopaged_len,
				     DMA_TO_DEVICE);
		if (dma_mapping_error(priv->device, des))
			goto dma_map_err;

		tx_q->tx_skbuff_dma[first_entry].buf = des;

		aicmac_dma_desc_set_addr(first, des);

		tx_q->tx_skbuff_dma[first_entry].len = nopaged_len;
		tx_q->tx_skbuff_dma[first_entry].last_segment = last_segment;

		/* Prepare the first descriptor setting the OWN bit too */
		aicmac_dma_desc_prepare_tx_desc(first, 1, nopaged_len,
						csum_insertion, priv->mode, 0,
						last_segment, skb->len);
	}

	if (unlikely((skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP) &&
		     priv->plat->ptp_data->hwts_tx_en)) {
		/* declare that device is doing timestamping */
		skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;
		aicmac_dma_desc_enable_tx_timestamp(first);
	}

	aicmac_dma_desc_set_tx_owner(first);

	/* The own bit must be the latest setting done when prepare the
	 * descriptor and then barrier is needed to make sure that
	 * all is coherent before granting the DMA engine.
	 */
	wmb();

	netdev_tx_sent_queue(netdev_get_tx_queue(dev, queue), skb->len);
	aicmac_dma_reg_enable_transmission(priv->resource->ioaddr);

	desc_size = sizeof(struct dma_extended_desc);

	tx_q->tx_tail_addr = tx_q->dma_tx_phy + (tx_q->cur_tx * desc_size);
	aicmac_tx_timer_arm(priv, queue);

	return NETDEV_TX_OK;
dma_map_err:
	netdev_err(priv->dev, "Tx DMA map failed\n");
	dev_kfree_skb(skb);
	priv->dev->stats.tx_dropped++;

	return NETDEV_TX_OK;
}

static int aicmac_change_mtu(struct net_device *dev, int new_mtu)
{
	struct aicmac_priv *priv = netdev_priv(dev);
	int txfifosz = priv->plat->dma_data->tx_fifo_size;

	if (txfifosz == 0)
		txfifosz = priv->plat->hw_cap.tx_fifo_size;

	txfifosz /= priv->plat->tx_queues_to_use;

	if (netif_running(dev)) {
		netdev_err(priv->dev, "must be stopped to change its MTU\n");
		return -EBUSY;
	}

	new_mtu = AICMAC_ALIGN(new_mtu);

	/* If condition true, FIFO is too small or MTU too large */
	if (txfifosz < new_mtu || new_mtu > BUF_SIZE_16KiB)
		return -EINVAL;

	dev->mtu = new_mtu;

	netdev_update_features(dev);

	return 0;
}

static netdev_features_t aicmac_fix_features(struct net_device *dev,
					     netdev_features_t features)
{
	struct aicmac_priv *priv = netdev_priv(dev);

	if (priv->plat->dma_data->rx_coe == AICMAC_RX_COE_NONE)
		features &= ~NETIF_F_RXCSUM;

	if (!priv->plat->dma_data->tx_coe)
		features &= ~NETIF_F_CSUM_MASK;

	if (priv->plat->mac_data->bugged_jumbo && dev->mtu > ETH_DATA_LEN)
		features &= ~NETIF_F_CSUM_MASK;

	return features;
}

static void aicmac_set_rx_mode(struct net_device *dev)
{
	struct aicmac_priv *priv = netdev_priv(dev);

	aicmac_mac_reg_set_filter(priv->plat->mac_data, dev);
}

static int aicmac_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct aicmac_priv *priv = netdev_priv(dev);
	int ret = -EOPNOTSUPP;

	if (!netif_running(dev))
		return -EINVAL;

	switch (cmd) {
	case SIOCGMIIPHY:
	case SIOCGMIIREG:
	case SIOCSMIIREG:
		ret = phylink_mii_ioctl(priv->plat->phy_data->phylink, rq, cmd);
		break;
	case SIOCSHWTSTAMP:
		ret = aicmac_1588_hwtstamp_set(dev, rq);
		break;
	case SIOCGHWTSTAMP:
		ret = aicmac_1588_hwtstamp_get(dev, rq);
		break;
	default:
		break;
	}

	return ret;
}

static int aicmac_setup_tc(struct net_device *ndev, enum tc_setup_type type,
			   void *type_data)
{
	int ret = -EOPNOTSUPP;
	return ret;
}

static u16 aicmac_select_queue(struct net_device *dev, struct sk_buff *skb,
			       struct net_device *sb_dev)
{
	if (skb_shinfo(skb)->gso_type & (SKB_GSO_TCPV4 | SKB_GSO_TCPV6))
		return 0;

	return netdev_pick_tx(dev, skb, NULL) % dev->real_num_tx_queues;
}

static int aicmac_set_mac_address(struct net_device *ndev, void *addr)
{
	struct aicmac_priv *priv = netdev_priv(ndev);
	int ret = 0;

	ret = eth_mac_addr(ndev, addr);
	if (ret)
		return ret;

	aicmac_mac_reg_set_umac_addr(priv->plat->mac_data, ndev->dev_addr, 0);

	return ret;
}

static int aicmac_vlan_rx_add_vid(struct net_device *ndev, __be16 proto,
				  u16 vid)
{
	return -EOPNOTSUPP;
}

static int aicmac_vlan_rx_kill_vid(struct net_device *ndev, __be16 proto,
				   u16 vid)
{
	return -EOPNOTSUPP;
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static int aicmac_poll_controller(struct net_device *dev)
{
	int ret = 0;

	disable_irq(dev->irq);
	aicmac_interrupt(dev->irq, dev);
	enable_irq(dev->irq);

	return ret;
}
#endif

static const struct net_device_ops aicmac_netdev_ops = {
	.ndo_open = aicmac_open,
	.ndo_start_xmit = aicmac_xmit,
	.ndo_stop = aicmac_release,
	.ndo_change_mtu = aicmac_change_mtu,
	.ndo_fix_features = aicmac_fix_features,
	.ndo_set_features = aicmac_set_features,
	.ndo_set_rx_mode = aicmac_set_rx_mode,
	.ndo_tx_timeout = aicmac_tx_timeout,
	.ndo_do_ioctl = aicmac_ioctl,
	.ndo_setup_tc = aicmac_setup_tc,
	.ndo_select_queue = aicmac_select_queue,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller = aicmac_poll_controller,
#endif
	.ndo_set_mac_address = aicmac_set_mac_address,
	.ndo_vlan_rx_add_vid = aicmac_vlan_rx_add_vid,
	.ndo_vlan_rx_kill_vid = aicmac_vlan_rx_kill_vid,
};

int aicmac_core_setup_napiop(struct aicmac_priv *priv, struct net_device *dev)
{
	u32 queue, maxq;
	int ret = 0;

	/* Setup channels NAPI */
	maxq = max(priv->plat->rx_queues_to_use, priv->plat->tx_queues_to_use);

	dev->netdev_ops = &aicmac_netdev_ops;

	for (queue = 0; queue < maxq; queue++) {
		struct aicmac_channel *ch = &priv->plat->channel[queue];

		ch->priv_data = priv;
		ch->index = queue;
		spin_lock_init(&ch->lock);

		if (queue < priv->plat->rx_queues_to_use) {
			netif_napi_add(dev, &ch->rx_napi, aicmac_napi_poll_rx,
				       NAPI_POLL_WEIGHT);
		}
		if (queue < priv->plat->tx_queues_to_use) {
			netif_tx_napi_add(dev, &ch->tx_napi,
					  aicmac_napi_poll_tx,
					  NAPI_POLL_WEIGHT);
		}
	}

	return ret;
}

int aicmac_core_init(struct device *device,
		     struct aicmac_platform_data *plat_dat,
		     struct aicmac_resources *res)
{
	struct net_device *ndev = NULL;
	struct aicmac_priv *priv;
	u32 queue, maxq;
	int ret = 0;

	ndev = devm_alloc_etherdev_mqs(device, sizeof(struct aicmac_priv),
				       MTL_MAX_TX_QUEUES, MTL_MAX_RX_QUEUES);
	if (!ndev)
		return -ENOMEM;

	SET_NETDEV_DEV(ndev, device);

	priv = netdev_priv(ndev);
	priv->dev = ndev;
	priv->device = device;

	priv->plat = plat_dat;
	priv->resource = res;
	priv->mode = AICMAC_RING_MODE;
	priv->extend_desc = 1;

	maxq = max(priv->plat->rx_queues_to_use, priv->plat->tx_queues_to_use);

	priv->dev->base_addr = (unsigned long)res->ioaddr;
	priv->dev->irq = res->irq;

	if (!IS_ERR_OR_NULL(res->mac))
		memcpy(priv->dev->dev_addr, res->mac, ETH_ALEN);

	aicmac_set_ethtool_ops(ndev);
	dev_set_drvdata(device, priv->dev);

	/* Allocate workqueue */
	priv->wq = create_singlethread_workqueue(AICMAC_WQ_NAME);
	if (!priv->wq) {
		dev_err(priv->device, "failed to create workqueue\n");
		return -ENOMEM;
	}

	INIT_WORK(&priv->service_task, aicmac_service_task);

	if (priv->plat->aicmac_rst) {
		ret = reset_control_assert(priv->plat->aicmac_rst);
		reset_control_deassert(priv->plat->aicmac_rst);
		if (ret == -EOPNOTSUPP)
			reset_control_reset(priv->plat->aicmac_rst);
	}

	/* Init MAC and get the capabilities */
	ret = aicmac_mac_init(priv);
	if (ret) {
		netdev_err(ndev, "failed to setup mac (%d)\n", ret);
		goto error_mac_init;
	}
	ret = aicmac_dma_init(priv);
	if (ret) {
		netdev_err(ndev, "failed to setup dma (%d)\n", ret);
		goto error_mac_init;
	}

	aicmac_napi_init(priv);

	aicmac_core_setup_napiop(priv, priv->dev);

	mutex_init(&priv->lock);

	/* MDIO bus Registration */
	ret = aicmac_mdio_register(priv);
	if (ret < 0) {
		dev_err(priv->device,
			"%s: MDIO bus (id: %d) registration failed", __func__,
			priv->plat->bus_id);
		goto error_mdio_register;
	}

	ret = aicmac_phy_init(priv);
	if (ret) {
		netdev_err(ndev, "failed to setup phy (%d)\n", ret);
		goto error_phy_setup;
	}

	ret = aicmac_1588_init(priv);
	if (ret) {
		netdev_err(ndev, "failed to setup 1588 (%d)\n", ret);
		goto error_phy_setup;
	}

	ret = register_netdev(ndev);
	if (ret) {
		dev_err(priv->device, "%s: ERROR %i registering the device\n",
			__func__, ret);
		goto error_netdev_register;
	}

	return ret;

error_netdev_register:
	phylink_destroy(priv->plat->phy_data->phylink);
error_phy_setup:
	if (priv->plat->mac_data->pcs != AICMAC_PCS_TBI &&
	    priv->plat->mac_data->pcs != AICMAC_PCS_RTBI)
		aicmac_mdio_unregister(ndev);
error_mdio_register:
	for (queue = 0; queue < maxq; queue++) {
		struct aicmac_channel *ch = &priv->plat->channel[queue];

		if (queue < priv->plat->rx_queues_to_use)
			netif_napi_del(&ch->rx_napi);
		if (queue < priv->plat->tx_queues_to_use)
			netif_napi_del(&ch->tx_napi);
	}
error_mac_init:
	destroy_workqueue(priv->wq);
	return ret;
}
