// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2020-2023 ArtInChip Inc.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <linux/dmapool.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_dma.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "virt-dma.h"

#define dma_dbg			pr_debug

#ifdef CONFIG_ARTINCHIP_DDMA
/* Reserved the last two channels for dedicated API,
 * which is faster than DMAEngine process.
 */
#define AIC_DDMA_CH_NUM		2
#define AIC_DDMA_CH0_NO(t, n)	((t) - (n))
#define AIC_DDMA_TIMEOUT	msecs_to_jiffies(10)
#else
#define AIC_DDMA_CH_NUM		0
#endif

/*
 * define DMA register list
 */
#define DMA_IRQ_EN_REG		(0x00)
#define DMA_IRQ_STA_REG		(0x10)
/* for each DMA channel */
#define DMA_CH_EN_REG		(0x00)
#define DMA_CH_PAUSE_REG	(0x04)
#define DMA_CH_TASK_REG		(0x08)
#define DMA_CH_CFG_REG		(0x0C)
#define DMA_CH_SRC_REG		(0x10)
#define DMA_CH_SINK_REG		(0x14)
#define DMA_CH_LEFT_REG		(0x18)
#define DMA_CH_MODE_REG		(0x28)
#define DMA_CH_STA		(0x30)

/*
 * define macro for access register for specific channel
 */
#define DMA_IRQ_HALF_TASK	BIT(0)
#define DMA_IRQ_ONE_TASK	BIT(1)
#define DMA_IRQ_ALL_TASK	BIT(2)
#define DMA_IRQ_CH_WIDTH	(4)
#define DMA_IRQ_SHIFT(ch)	(DMA_IRQ_CH_WIDTH * (ch))
#define DMA_IRQ_MASK(ch)	(GENMASK(2, 0) << DMA_IRQ_SHIFT(ch))

#define AIC_DMA_BUS_WIDTH                                                  \
	(BIT(DMA_SLAVE_BUSWIDTH_1_BYTE) | BIT(DMA_SLAVE_BUSWIDTH_2_BYTES) | \
	 BIT(DMA_SLAVE_BUSWIDTH_4_BYTES) | BIT(DMA_SLAVE_BUSWIDTH_8_BYTES))

/*
 * define bit index in channel configuration register
 */
#define DST_WIDTH_BITSHIFT	25
#define DST_ADDR_BITSHIFT	24
#define DST_BURST_BITSHIFT	22
#define DST_PORT_BITSHIFT	16
#define SRC_WIDTH_BITSHIFT	9
#define SRC_ADDR_BITSHIFT	8
#define SRC_BURST_BITSHIFT	6
#define SRC_PORT_BITSHIFT	0

#define ADDR_LINEAR_MODE	0
#define ADDR_FIXED_MODE		1

#define DMA_WAIT_MODE		0
#define DMA_HANDSHAKE_MODE	1
#define DMA_DST_MODE_SHIFT	3
#define DMA_SRC_MODE_SHIFT	2
#define DMA_S_WAIT_D_HANDSHAKE	(DMA_HANDSHAKE_MODE << DMA_DST_MODE_SHIFT)
#define DMA_S_HANDSHAKE_D_WAIT	(DMA_HANDSHAKE_MODE << DMA_SRC_MODE_SHIFT)
#define DMA_S_WAIT_D_WAIT	(DMA_WAIT_MODE)

#define DRAM_DRQ_PORT		1

#define DMA_LINK_END_FLAG	0xfffff800

/*
 * The description structure for one DMA task node
 */
struct aic_dma_task {
	u32 cfg;	/* DMA transfer configuration */
	u32 src;	/* source address of one transfer package */
	u32 dst;	/* destination address of one transfer package */
	u32 len;	/* data length of one transfer package */
	u32 delay;	/* time delay for period transfer */
	u32 p_next;	/* next task node for DMA controller */
	u32 mode;	/* the negotiation mode */

	/*
	 * virtual list for driver maintain package list,
	 * not used by DMA controller
	 */
	struct aic_dma_task *v_next;

};

struct aic_desc {
	dma_addr_t plink;
	struct virt_dma_desc vd;
	struct aic_dma_task *vlink;
};

/*
 * DMA physical channel description
 */
struct aic_pchan {
	u32 id; /* DMA channel number */
	void __iomem *base; /* DMA channel control registers */
	struct aic_vchan *vchan; /* virtual channel info */
};

/*
 * DMA virtual channel description
 */
struct aic_vchan {
	u8 port; /* DRQ port number */
	u8 irq_type; /* IRQ types */
	bool cyclic; /* flag to mark if cyclic transfer one package */
	struct aic_pchan *pchan; /* physical DMA channel */
	struct aic_desc *desc; /* current transfer */

	/* parameter for dmaengine */
	struct virt_dma_chan vc;
	struct dma_slave_config cfg;
	enum dma_status status;
};

/*
 * DMA controller description for SoC serial
 */
struct aic_dma_inf {
	u8 nr_chans; /* count of DMA physical channels */
	u8 nr_ports; /* count of DMA DRQ prots */
	u8 nr_vchans; /* total valid transfer types */

	u32 burst_length; /* burst length capacity */
	u32 addr_widths; /* address width support capacity */
};

struct aic_dma_dev {
	void __iomem *base;
	int irq;
	u32 num_pchans;
	u32 num_vchans;
	u32 max_request;

	struct clk *clk;
	struct reset_control *reset;

	spinlock_t lock;
	struct dma_pool *pool;
	struct aic_pchan *pchans;
	struct aic_vchan *vchans;
	const struct aic_dma_inf *dma_inf;
	struct dma_device slave;
};

#ifdef CONFIG_ARTINCHIP_DDMA
static struct aic_dma_dev *g_ddma_dev;
#endif

static struct device *chan2dev(struct dma_chan *chan)
{
	return &chan->dev->device;
}

static inline struct aic_dma_dev *to_aic_dma_dev(struct dma_device *d)
{
	return container_of(d, struct aic_dma_dev, slave);
}

static inline struct aic_vchan *to_aic_vchan(struct dma_chan *chan)
{
	return container_of(chan, struct aic_vchan, vc.chan);
}

static inline struct aic_desc *to_aic_desc(struct dma_async_tx_descriptor *tx)
{
	return container_of(tx, struct aic_desc, vd.tx);
}

static void *aic_dma_link_add(struct aic_dma_task *prev,
			      struct aic_dma_task *next,
			      dma_addr_t next_phy, struct aic_desc *txd)
{
	if ((!prev && !txd) || !next)
		return NULL;

	if (!prev) {
		txd->plink = next_phy;
		txd->vlink = next;
	} else {
		prev->p_next = next_phy;
		prev->v_next = next;
	}

	next->p_next = DMA_LINK_END_FLAG;
	next->v_next = NULL;

	return next;
}

static size_t aic_get_chan_size(struct aic_pchan *pchan)
{
	struct aic_desc *txd = pchan->vchan->desc;
	struct aic_dma_task *task;
	size_t bytes;
	dma_addr_t pos;

	pos = readl(pchan->base + DMA_CH_TASK_REG);
	bytes = readl(pchan->base + DMA_CH_LEFT_REG);

	if (pos == DMA_LINK_END_FLAG)
		return bytes;

	for (task = txd->vlink; task; task = task->v_next) {
		if (task->p_next == pos) {
			for (task = task->v_next; task; task = task->v_next)
				bytes += task->len;
			break;
		}
	}

	return bytes;
}

static inline s8 convert_burst(u32 maxburst)
{
	switch (maxburst) {
	case 1:
		return 0;
	case 4:
		return 1;
	case 8:
		return 2;
	case 16:
		return 3;
	default:
		return -EINVAL;
	}
}

static inline s8 convert_buswidth(enum dma_slave_buswidth addr_width)
{
	switch (addr_width) {
	case DMA_SLAVE_BUSWIDTH_1_BYTE:
	case DMA_SLAVE_BUSWIDTH_2_BYTES:
	case DMA_SLAVE_BUSWIDTH_4_BYTES:
	case DMA_SLAVE_BUSWIDTH_8_BYTES:
		return ilog2(addr_width);
	default:
		return -EINVAL;
	}
}

static int aic_set_burst(struct aic_dma_dev *sdev,
			 struct dma_slave_config *sconfig,
			 enum dma_transfer_direction direction, u32 *p_cfg)
{
	enum dma_slave_buswidth src_addr_width, dst_addr_width;
	u32 src_maxburst, dst_maxburst;
	s8 src_width, dst_width, src_burst, dst_burst;

	src_addr_width = sconfig->src_addr_width;
	dst_addr_width = sconfig->dst_addr_width;
	src_maxburst = sconfig->src_maxburst;
	dst_maxburst = sconfig->dst_maxburst;

	switch (direction) {
	case DMA_MEM_TO_DEV:
		if (src_addr_width == DMA_SLAVE_BUSWIDTH_UNDEFINED)
			src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		src_maxburst = src_maxburst ? src_maxburst : 8;
		break;
	case DMA_DEV_TO_MEM:
		if (dst_addr_width == DMA_SLAVE_BUSWIDTH_UNDEFINED)
			dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		dst_maxburst = dst_maxburst ? dst_maxburst : 8;
		break;
	default:
		return -EINVAL;
	}

	if (!(BIT(src_addr_width) & sdev->slave.src_addr_widths))
		return -EINVAL;
	if (!(BIT(dst_addr_width) & sdev->slave.dst_addr_widths))
		return -EINVAL;
	if (!(BIT(src_maxburst) & sdev->dma_inf->burst_length))
		return -EINVAL;
	if (!(BIT(dst_maxburst) & sdev->dma_inf->burst_length))
		return -EINVAL;

	src_width = convert_buswidth(src_addr_width);
	if (src_width < 0)
		return -EINVAL;
	dst_width = convert_buswidth(dst_addr_width);
	if (dst_width < 0)
		return -EINVAL;
	dst_burst = convert_burst(dst_maxburst);
	if (dst_burst < 0)
		return -EINVAL;
	src_burst = convert_burst(src_maxburst);
	if (src_burst < 0)
		return -EINVAL;

	*p_cfg = (src_width << SRC_WIDTH_BITSHIFT) |
		 (dst_width << DST_WIDTH_BITSHIFT) |
		 (src_burst << SRC_BURST_BITSHIFT) |
		 (dst_burst << DST_BURST_BITSHIFT);

	return 0;
}

static void aic_dma_free_chan_resources(struct dma_chan *chan)
{
	struct aic_vchan *vchan = to_aic_vchan(chan);

	vchan_free_chan_resources(&vchan->vc);
}

static struct dma_async_tx_descriptor *
aic_dma_prep_dma_memcpy(struct dma_chan *chan, dma_addr_t dest, dma_addr_t src,
			size_t len, unsigned long flags)
{
	struct aic_dma_dev *sdev = to_aic_dma_dev(chan->device);
	struct aic_vchan *vchan = to_aic_vchan(chan);
	struct aic_dma_task *v_task;
	struct aic_desc *txd;
	dma_addr_t plink;

	if (!len) {
		dev_err(chan2dev(chan), "%s len is 0\n", __func__);
		return NULL;
	}

	txd = kzalloc(sizeof(*txd), GFP_NOWAIT);
	if (!txd) {
		dev_err(chan2dev(chan), "%s malloc failed\n", __func__);
		return NULL;
	}

	v_task = dma_pool_alloc(sdev->pool, GFP_NOWAIT, &plink);
	if (!v_task) {
		dev_err(chan2dev(chan), "Failed to alloc link memory\n");
		goto err_txd_free;
	}

	v_task->src = src;
	v_task->dst = dest;
	v_task->len = len;
	v_task->cfg = (ADDR_LINEAR_MODE << DST_ADDR_BITSHIFT) |
		     (ADDR_LINEAR_MODE << SRC_ADDR_BITSHIFT) |
		     (3 << DST_BURST_BITSHIFT) | (3 << SRC_BURST_BITSHIFT) |
		     (2 << DST_WIDTH_BITSHIFT) | (2 << SRC_WIDTH_BITSHIFT) |
		     (DRAM_DRQ_PORT << DST_PORT_BITSHIFT) |
		     (DRAM_DRQ_PORT << SRC_PORT_BITSHIFT);
	v_task->mode = DMA_S_WAIT_D_WAIT;

	aic_dma_link_add(NULL, v_task, plink, txd);

	return vchan_tx_prep(&vchan->vc, &txd->vd, flags);

err_txd_free:
	kfree(txd);
	return NULL;
}

static struct dma_async_tx_descriptor *
aic_dma_prep_slave_sg(struct dma_chan *chan, struct scatterlist *sgl,
		      unsigned int sg_len, enum dma_transfer_direction dir,
		      unsigned long flags, void *context)
{
	struct aic_dma_dev *sdev = to_aic_dma_dev(chan->device);
	struct aic_vchan *vchan = to_aic_vchan(chan);
	struct dma_slave_config *sconfig = &vchan->cfg;
	struct aic_dma_task *task, *prev = NULL;
	struct aic_desc *txd;
	struct scatterlist *sg;
	dma_addr_t plink;
	u32 ch_cfg;
	int i, ret;

	if (!sgl)
		return NULL;

	ret = aic_set_burst(sdev, sconfig, dir, &ch_cfg);
	if (ret) {
		dev_err(chan2dev(chan), "Invalid DMA configuration\n");
		return NULL;
	}

	txd = kzalloc(sizeof(*txd), GFP_NOWAIT);
	if (!txd)
		return NULL;

	for_each_sg (sgl, sg, sg_len, i) {
		task = dma_pool_alloc(sdev->pool, GFP_NOWAIT, &plink);
		if (!task) {
			dev_err(chan2dev(chan), "DMA pool alloc failed!\n");
			goto err_link_free;
		}

		task->len = sg_dma_len(sg);

		if (dir == DMA_MEM_TO_DEV) {
			task->src = sg_dma_address(sg);
			task->dst = sconfig->dst_addr;
			task->cfg = ch_cfg;
			task->cfg |= (DRAM_DRQ_PORT << SRC_PORT_BITSHIFT) |
				      (vchan->port << DST_PORT_BITSHIFT) |
				      (ADDR_LINEAR_MODE << SRC_ADDR_BITSHIFT) |
				      (ADDR_FIXED_MODE << DST_ADDR_BITSHIFT);
			task->mode = DMA_S_WAIT_D_HANDSHAKE;
		} else {
			task->src = sconfig->src_addr;
			task->dst = sg_dma_address(sg);
			task->cfg = ch_cfg;
			task->cfg |= (DRAM_DRQ_PORT << DST_PORT_BITSHIFT) |
				      (vchan->port << SRC_PORT_BITSHIFT) |
				      (ADDR_LINEAR_MODE << DST_ADDR_BITSHIFT) |
				      (ADDR_FIXED_MODE << SRC_ADDR_BITSHIFT);
			task->mode = DMA_S_HANDSHAKE_D_WAIT;
		}
		task->delay = 0x40;

		prev = aic_dma_link_add(prev, task, plink, txd);
	}

#ifdef CONFIG_ARTINCHIP_DDMA
	if (vchan->pchan && (vchan->pchan->id >=
		AIC_DDMA_CH0_NO(sdev->num_pchans, AIC_DDMA_CH_NUM))) {
		txd->vd.tx.chan = chan;
		list_add_tail(&txd->vd.node, &vchan->vc.desc_issued);
		return &txd->vd.tx;
	}
#endif
	return vchan_tx_prep(&vchan->vc, &txd->vd, flags);

err_link_free:
	for (prev = txd->vlink; prev; prev = prev->v_next)
		dma_pool_free(sdev->pool, prev, virt_to_phys(prev));
	kfree(txd);
	return NULL;
}

static struct dma_async_tx_descriptor *
aic_dma_prep_dma_cyclic(struct dma_chan *chan, dma_addr_t buf_addr,
			size_t buf_len, size_t period_len,
			enum dma_transfer_direction dir, unsigned long flags)
{
	struct aic_dma_dev *sdev = to_aic_dma_dev(chan->device);
	struct aic_vchan *vchan = to_aic_vchan(chan);
	struct dma_slave_config *sconfig = &vchan->cfg;
	struct aic_dma_task *task, *prev = NULL;
	struct aic_desc *txd;
	dma_addr_t plink;
	u32 ch_cfg;
	unsigned int i, periods = buf_len / period_len;
	int ret;

	ret = aic_set_burst(sdev, sconfig, dir, &ch_cfg);
	if (ret) {
		dev_err(chan2dev(chan), "Invalid DMA configuration\n");
		return NULL;
	}

	txd = kzalloc(sizeof(*txd), GFP_NOWAIT);
	if (!txd) {
		dev_err(chan2dev(chan), "%s malloc failed\n", __func__);
		return NULL;
	}

	for (i = 0; i < periods; i++) {
		task = dma_pool_alloc(sdev->pool, GFP_NOWAIT, &plink);
		if (!task) {
			dev_err(chan2dev(chan), "Failed to alloc memory\n");
			goto err_link_free;
		}

		task->len = period_len;

		if (dir == DMA_MEM_TO_DEV) {
			task->src = buf_addr + period_len * i;
			task->dst = sconfig->dst_addr;
			task->cfg = ch_cfg;
			task->cfg |= (DRAM_DRQ_PORT << SRC_PORT_BITSHIFT) |
				      (vchan->port << DST_PORT_BITSHIFT) |
				      (ADDR_LINEAR_MODE << SRC_ADDR_BITSHIFT) |
				      (ADDR_FIXED_MODE << DST_ADDR_BITSHIFT);
			task->mode = DMA_S_WAIT_D_HANDSHAKE;
		} else {
			task->src = sconfig->src_addr;
			task->dst = buf_addr + period_len * i;
			task->cfg = ch_cfg;
			task->cfg |= (DRAM_DRQ_PORT << DST_PORT_BITSHIFT) |
				      (vchan->port << SRC_PORT_BITSHIFT) |
				      (ADDR_LINEAR_MODE << DST_ADDR_BITSHIFT) |
				      (ADDR_FIXED_MODE << SRC_ADDR_BITSHIFT);
			task->mode = DMA_S_HANDSHAKE_D_WAIT;
		}

		prev = aic_dma_link_add(prev, task, plink, txd);
	}

	/* build a cyclic list */
	prev->p_next = txd->plink;

	vchan->cyclic = true;

	return vchan_tx_prep(&vchan->vc, &txd->vd, flags);

err_link_free:
	for (prev = txd->vlink; prev; prev = prev->v_next)
		dma_pool_free(sdev->pool, prev, virt_to_phys(prev));
	kfree(txd);
	return NULL;
}

static int aic_dma_config(struct dma_chan *chan,
			  struct dma_slave_config *config)
{
	struct aic_vchan *vchan = to_aic_vchan(chan);

	memcpy(&vchan->cfg, config, sizeof(*config));

	return 0;
}

static int aic_dma_pause(struct dma_chan *chan)
{
	struct aic_vchan *vchan = to_aic_vchan(chan);
	struct aic_pchan *pchan = vchan->pchan;

	if (pchan) {
		/* pause the physical channel */
		writel(0x01, pchan->base + DMA_CH_PAUSE_REG);
		vchan->status = DMA_PAUSED;
	}

	return 0;
}

static int aic_dma_resume(struct dma_chan *chan)
{
	struct aic_vchan *vchan = to_aic_vchan(chan);
	struct aic_pchan *pchan = vchan->pchan;

	if (pchan) {
		writel(0, pchan->base + DMA_CH_PAUSE_REG);
		vchan->status = DMA_IN_PROGRESS;
	}

	return 0;
}

static enum dma_status aic_dma_tx_status(struct dma_chan *chan,
					 dma_cookie_t cookie,
					 struct dma_tx_state *state)
{
	struct aic_vchan *vchan = to_aic_vchan(chan);
	struct aic_pchan *pchan = vchan->pchan;
	struct aic_dma_task *task;
	struct virt_dma_desc *vd;
	struct aic_desc *txd;
	enum dma_status ret;
	unsigned long flags;
	size_t bytes = 0;

	ret = dma_cookie_status(chan, cookie, state);
	if (ret == DMA_COMPLETE || !state)
		return ret;

	spin_lock_irqsave(&vchan->vc.lock, flags);

	vd = vchan_find_desc(&vchan->vc, cookie);
	txd = to_aic_desc(&vd->tx);

	if (vd) {
		for (task = txd->vlink; task != NULL; task = task->v_next)
			bytes += task->len;
	} else if (!pchan || !vchan->desc) {
		bytes = 0;
	} else {
		bytes = aic_get_chan_size(pchan);
	}

	spin_unlock_irqrestore(&vchan->vc.lock, flags);

	dma_set_residue(state, bytes);

	return ret;
}

static int aic_dma_terminate_all(struct dma_chan *chan)
{
	struct aic_vchan *vchan = to_aic_vchan(chan);
	struct aic_pchan *pchan = vchan->pchan;
	unsigned long flags;
	LIST_HEAD(head);

	spin_lock_irqsave(&vchan->vc.lock, flags);

	if (vchan->cyclic) {
		vchan->cyclic = false;
		if (pchan && vchan->desc) {
			struct virt_dma_desc *vd = &vchan->desc->vd;
			struct virt_dma_chan *vc = &vchan->vc;

			list_add_tail(&vd->node, &vc->desc_completed);
		}
	}

	vchan_get_all_descriptors(&vchan->vc, &head);

	if (pchan) {
		writel(0x00, pchan->base + DMA_CH_PAUSE_REG);
		writel(0x00, pchan->base + DMA_CH_EN_REG);

		vchan->pchan = NULL;
		vchan->desc = NULL;
		pchan->vchan = NULL;
	}

	spin_unlock_irqrestore(&vchan->vc.lock, flags);

	vchan_dma_desc_free_list(&vchan->vc, &head);

	return 0;
}

static void aic_dma_free_desc(struct virt_dma_desc *vd)
{
	struct aic_desc *txd = to_aic_desc(&vd->tx);
	struct aic_dma_dev *sdev = to_aic_dma_dev(vd->tx.chan->device);
	struct aic_dma_task *task, *v_next;
	dma_addr_t plink, p_next;

	if (unlikely(!txd))
		return;

	plink = txd->plink;
	task = txd->vlink;

	while (task) {
		v_next = task->v_next;
		p_next = task->p_next;

		dma_pool_free(sdev->pool, task, plink);

		task = v_next;
		plink = p_next;
	}

	kfree(txd);
}

static struct dma_chan *aic_dma_of_xlate(struct of_phandle_args *dma_spec,
					 struct of_dma *ofdma)
{
	struct aic_dma_dev *sdev = ofdma->of_dma_data;
	struct aic_vchan *vchan;
	struct dma_chan *chan;
	u8 port = dma_spec->args[0];

	if (port >= sdev->max_request) {
		dev_err(sdev->slave.dev, "dmas config error!\n");
		return NULL;
	}

	chan = dma_get_any_slave_channel(&sdev->slave);
	if (!chan) {
		dev_err(sdev->slave.dev, "try allocate DMA chan failed!\n");
		return NULL;
	}

	vchan = to_aic_vchan(chan);
	vchan->port = port;

	return chan;
}

static int aic_dma_start_desc(struct aic_vchan *vchan,
			      struct virt_dma_desc *desc)
{
	struct aic_dma_dev *sdev = to_aic_dma_dev(vchan->vc.chan.device);
	struct aic_pchan *pchan = vchan->pchan;

	if (unlikely(readl(pchan->base + DMA_CH_EN_REG)))  {
		dev_err(sdev->slave.dev, "ch%d is busy already!", pchan->id);
		return -EBUSY;
	}

	list_del(&desc->node);

	vchan->desc = to_aic_desc(&desc->tx);
	WARN_ON(!vchan->desc);
	vchan->status = DMA_IN_PROGRESS;

	vchan->irq_type = vchan->cyclic ? DMA_IRQ_ONE_TASK : DMA_IRQ_ALL_TASK;

#ifdef CONFIG_ARTINCHIP_DDMA
	/* DDMA channel don't need enable IRQ */
	if (pchan->id < AIC_DDMA_CH0_NO(sdev->num_pchans, AIC_DDMA_CH_NUM))
#endif
		writel_bits(vchan->irq_type, DMA_IRQ_MASK(pchan->id),
			    DMA_IRQ_SHIFT(pchan->id),
			    sdev->base + DMA_IRQ_EN_REG);

	writel(vchan->desc->vlink->mode, pchan->base + DMA_CH_MODE_REG);
	writel(vchan->desc->plink, pchan->base + DMA_CH_TASK_REG);
	writel(0x01, pchan->base + DMA_CH_EN_REG);
	writel(0x00, pchan->base + DMA_CH_PAUSE_REG);
	dma_dbg("%s() Start ch%d[%s]: IRQ %#x, mode %#x, len %d\n", __func__,
		pchan->id, vchan->vc.chan.dbg_client_name,
		vchan->irq_type, vchan->desc->vlink->mode,
		vchan->desc->vlink->len);
	return 0;
}

static void aic_dma_xfer(struct aic_dma_dev *sdev, struct aic_vchan *vchan)
{
	struct aic_pchan *pchan = vchan->pchan;
	struct virt_dma_desc *desc = vchan_next_desc(&vchan->vc);
	unsigned int i, found = 0;
	unsigned long flags;

	spin_lock_irqsave(&sdev->lock, flags);
	if (!desc) {
		/* no request available, free the physical channel */
		dma_dbg("%s() - Free ch%d[%s]\n", __func__,
			pchan->id, vchan->vc.chan.dbg_client_name);
		vchan->desc = NULL;
		vchan->pchan = NULL;
		pchan->vchan = NULL;
		spin_unlock_irqrestore(&sdev->lock, flags);
		return;
	}

	if (pchan) {
		dma_dbg("%s() - ch%d[%s] maybe has next desc\n", __func__,
			pchan->id, vchan->vc.chan.dbg_client_name);
		aic_dma_start_desc(vchan, desc);
		spin_unlock_irqrestore(&sdev->lock, flags);
		return;
	}

	/* Need allocate physical channel for the vchan */
	for (i = 0; i < sdev->num_pchans; i++) {
		pchan = &sdev->pchans[i];

		/* If the pchan is busy */
		if (pchan->vchan)
			continue;

		dma_dbg("%s() - Allocate ch%d for %s\n", __func__,
			pchan->id, vchan->vc.chan.dbg_client_name);

		pchan->vchan = vchan;
		vchan->pchan = pchan;
		aic_dma_start_desc(vchan, desc);
		found = 1;
		break;
	}
	spin_unlock_irqrestore(&sdev->lock, flags);

	if (!found)
		dev_err(sdev->slave.dev, "No DMA chan is available!\n");
}

static void aic_dma_issue_pending(struct dma_chan *chan)
{
	struct aic_dma_dev *sdev = to_aic_dma_dev(chan->device);
	struct aic_vchan *vchan = to_aic_vchan(chan);
	unsigned long flags;

	dma_dbg("%s() - issue %s\n", __func__, chan->dbg_client_name);
	spin_lock_irqsave(&vchan->vc.lock, flags);

	if (vchan_issue_pending(&vchan->vc)) {
		if (!vchan->desc)
			aic_dma_xfer(sdev, vchan);
		else
			dev_info(sdev->slave.dev, "%s has desc already!\n",
				 chan->dbg_client_name);
	}

	spin_unlock_irqrestore(&vchan->vc.lock, flags);
}

static inline void aic_kill_tasklet(struct aic_dma_dev *sdev)
{
	writel(0, sdev->base + DMA_IRQ_EN_REG);
	devm_free_irq(sdev->slave.dev, sdev->irq, sdev);
}

static inline void aic_dma_free(struct aic_dma_dev *sdev)
{
	int i;

	for (i = 0; i < sdev->num_vchans - AIC_DDMA_CH_NUM; i++) {
		struct aic_vchan *vchan = &sdev->vchans[i];

		list_del(&vchan->vc.chan.device_node);
		tasklet_kill(&vchan->vc.task);
	}
}

#ifdef CONFIG_ARTINCHIP_DDMA

struct dma_chan *aic_ddma_request_chan(struct device *dev, u32 port)
{
	struct aic_dma_dev *sdev = g_ddma_dev;
	int i, ch = AIC_DDMA_CH0_NO(sdev->num_vchans, AIC_DDMA_CH_NUM);
	struct aic_vchan *vchan = &sdev->vchans[ch];

	if (!dev) {
		dev_err(sdev->slave.dev, "Invalid parameters\n");
		return NULL;
	}

	for (i = 0; i < AIC_DDMA_CH_NUM; i++, vchan++) {
		if (vchan->status != DMA_IN_PROGRESS) {
			struct dma_chan *chan = &vchan->vc.chan;

			chan->slave = dev;
			chan->dbg_client_name = (char *)dev->kobj.name;
			chan->device->privatecnt++;
			vchan->port = port;
			vchan->status = DMA_IN_PROGRESS; /* Means it's used */
			dma_dbg("Alloc DDMA ch%d for [%d]%s\n", i, port,
				chan->dbg_client_name);
			return chan;
		}
	}

	dev_err(sdev->slave.dev, "No DDMA channel available\n");
	return NULL;
}
EXPORT_SYMBOL(aic_ddma_request_chan);

void aic_ddma_release_chan(struct dma_chan *chan)
{
	struct aic_dma_dev *sdev = g_ddma_dev;
	int i, ch = AIC_DDMA_CH0_NO(sdev->num_vchans, AIC_DDMA_CH_NUM);
	struct aic_vchan *vchan = &sdev->vchans[ch];

	if (!chan) {
		dev_err(sdev->slave.dev, "Invalid parameters\n");
		return;
	}

	for (i = 0; i < AIC_DDMA_CH_NUM; i++, vchan++) {
		if (&vchan->vc.chan != chan)
			continue;

		if (vchan->status != DMA_IN_PROGRESS) {
			chan->slave = NULL;
			chan->dbg_client_name = NULL;
			chan->device->privatecnt--;
			vchan->port = 0;
			vchan->status = DMA_COMPLETE; /* Means it's free */
			return;
		}
	}

	dev_err(sdev->slave.dev, "Invalid DDMA channel\n");
}
EXPORT_SYMBOL(aic_ddma_release_chan);

int aic_ddma_transfer(struct dma_chan *chan)
{
	struct aic_dma_dev *sdev = g_ddma_dev;
	struct aic_vchan *vchan = to_aic_vchan(chan);
	struct virt_dma_desc *desc = NULL;

	aic_dma_xfer(sdev, vchan);
	while (readl_bit(BIT(vchan->pchan->id), sdev->base + DMA_CH_STA))
		;

	desc = &vchan->desc->vd;
	if (desc->tx.callback)
		desc->tx.callback(desc->tx.callback_param);
	aic_dma_free_desc(desc);
	return 0;
}
EXPORT_SYMBOL(aic_ddma_transfer);

#endif

static irqreturn_t aic_dma_interrupt(int irq, void *dev_id)
{
	int i;
	u32 status;
	struct aic_dma_dev *sdev = dev_id;
	struct aic_pchan *pchan = sdev->pchans;
	struct aic_vchan *vchan;

	/* get DMA IRQ pending */
	status = readl(sdev->base + DMA_IRQ_STA_REG);
	if (!status) {
		/* none IRQ trigger */
		return IRQ_NONE;
	}

	/* clear IRQ pending */
	writel(status, sdev->base + DMA_IRQ_STA_REG);
	dma_dbg("IRQ status: %#x\n", status);

	/* process IRQ for every DMA channel */
	for (i = 0; i < sdev->num_pchans && status; i++, pchan++) {
		if (!(status & DMA_IRQ_MASK(i)))
			continue;

		vchan = pchan->vchan;
#ifdef CONFIG_ARTINCHIP_DDMA
		/* The DDMA channel use pooling mode instead of IRQ */
		if (i >= AIC_DDMA_CH0_NO(sdev->num_pchans, AIC_DDMA_CH_NUM))
			continue;
#endif

		if (unlikely(!vchan || !vchan->desc)) {
			dev_warn(sdev->slave.dev, "ch%d has no task (%#x)\n",
				 pchan->id, status);
			continue;
		}

		/* Only care the IRQ type wanted */
		if (!((status >> DMA_IRQ_SHIFT(pchan->id)) & vchan->irq_type))
			continue;

		if (vchan->cyclic) {
			vchan_cyclic_callback(&vchan->desc->vd);
		} else {
			spin_lock(&vchan->vc.lock);
			vchan_cookie_complete(&vchan->desc->vd);

			spin_lock(&sdev->lock);
			vchan->status = DMA_COMPLETE;
			vchan->desc = NULL;
			spin_unlock(&sdev->lock);

			/* Try the next descriptor(if available) */
			aic_dma_xfer(sdev, vchan);
			spin_unlock(&vchan->vc.lock);
		}
	}

	return IRQ_HANDLED;
}

static int aic_dma_probe(struct platform_device *pdev)
{
	int ret, i;
	struct resource *res;
	struct aic_dma_dev *sdev;

	sdev = devm_kzalloc(&pdev->dev, sizeof(*sdev), GFP_KERNEL);
	if (!sdev) {
		dev_err(&pdev->dev, "malloc failed!\n");
		return -ENOMEM;
	}

	sdev->dma_inf = of_device_get_match_data(&pdev->dev);
	if (!sdev->dma_inf) {
		dev_err(&pdev->dev, "no match data!\n");
		return -ENODEV;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	sdev->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(sdev->base)) {
		dev_err(&pdev->dev, "ioremap failed!\n");
		return PTR_ERR(sdev->base);
	}

	sdev->irq = platform_get_irq(pdev, 0);
	if (sdev->irq < 0) {
		dev_err(&pdev->dev, "interrupts not configured!\n");
		return sdev->irq;
	}
	writel(0, sdev->base + DMA_IRQ_EN_REG);

	sdev->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(sdev->clk)) {
		dev_err(&pdev->dev, "clocks not configured!\n");
		return PTR_ERR(sdev->clk);
	}

	sdev->reset = devm_reset_control_get(&pdev->dev, NULL);
	if (IS_ERR(sdev->reset)) {
		dev_err(&pdev->dev, "resets not configured!\n");
		return PTR_ERR(sdev->reset);
	}

	sdev->pool = dmam_pool_create(dev_name(&pdev->dev), &pdev->dev,
				      sizeof(struct aic_dma_task), 4, 0);
	if (!sdev->pool) {
		dev_err(&pdev->dev, "DMA pool create failed!\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, sdev);
	spin_lock_init(&sdev->lock);

	sdev->slave.dev = &pdev->dev;
	if (of_device_is_compatible(pdev->dev.of_node,
				    "artinchip,aic-dma-v0.1"))
		sdev->slave.copy_align = DMAENGINE_ALIGN_128_BYTES;
	else
		sdev->slave.copy_align = DMAENGINE_ALIGN_8_BYTES;
	sdev->slave.src_addr_widths = AIC_DMA_BUS_WIDTH;
	sdev->slave.dst_addr_widths = AIC_DMA_BUS_WIDTH;
	sdev->slave.directions = BIT(DMA_DEV_TO_MEM) | BIT(DMA_MEM_TO_DEV);
	sdev->slave.residue_granularity = DMA_RESIDUE_GRANULARITY_BURST;

	INIT_LIST_HEAD(&sdev->slave.channels);

	dma_cap_set(DMA_PRIVATE, sdev->slave.cap_mask);
	dma_cap_set(DMA_MEMCPY, sdev->slave.cap_mask);
	dma_cap_set(DMA_SLAVE, sdev->slave.cap_mask);
	dma_cap_set(DMA_CYCLIC, sdev->slave.cap_mask);

	sdev->slave.device_free_chan_resources = aic_dma_free_chan_resources;
	sdev->slave.device_prep_dma_memcpy = aic_dma_prep_dma_memcpy;
	sdev->slave.device_prep_slave_sg = aic_dma_prep_slave_sg;
	sdev->slave.device_prep_dma_cyclic = aic_dma_prep_dma_cyclic;

	sdev->slave.device_config = aic_dma_config;
	sdev->slave.device_pause = aic_dma_pause;
	sdev->slave.device_resume = aic_dma_resume;
	sdev->slave.device_terminate_all = aic_dma_terminate_all;
	sdev->slave.device_tx_status = aic_dma_tx_status;
	sdev->slave.device_issue_pending = aic_dma_issue_pending;

	sdev->num_pchans = sdev->dma_inf->nr_chans;
	sdev->num_vchans = sdev->dma_inf->nr_vchans;
	sdev->max_request = sdev->dma_inf->nr_ports;

	sdev->pchans = devm_kcalloc(&pdev->dev, sdev->num_pchans,
				    sizeof(struct aic_pchan), GFP_KERNEL);
	if (!sdev->pchans) {
		dev_err(&pdev->dev, "malloc failed!\n");
		return -ENOMEM;
	}

	sdev->vchans = devm_kcalloc(&pdev->dev, sdev->num_vchans,
				    sizeof(struct aic_vchan), GFP_KERNEL);
	if (!sdev->vchans) {
		dev_err(&pdev->dev, "malloc failed!\n");
		return -ENOMEM;
	}

	for (i = 0; i < sdev->num_pchans; i++) {
		struct aic_pchan *pchan = &sdev->pchans[i];

		pchan->id = i;
		pchan->base = sdev->base + 0x100 + i * 0x40;
	}

	for (i = 0; i < sdev->num_vchans; i++) {
		struct aic_vchan *vchan = &sdev->vchans[i];

		vchan->vc.desc_free = aic_dma_free_desc;
		/* Reserved the tail channels for dedicated API */
		if (i < sdev->num_vchans - AIC_DDMA_CH_NUM) {
			vchan_init(&vchan->vc, &sdev->slave);
			continue;
		}
#ifdef CONFIG_ARTINCHIP_DDMA
		ret = sdev->num_pchans - (sdev->num_vchans - i);

		sdev->pchans[ret].vchan = vchan;
		vchan->pchan = &sdev->pchans[ret];
		vchan->vc.chan.device = &sdev->slave;
		/* Only use the issued list */
		INIT_LIST_HEAD(&vchan->vc.desc_issued);
#endif
	}

#ifdef CONFIG_ARTINCHIP_DDMA
	g_ddma_dev = sdev;
#endif

	ret = clk_prepare_enable(sdev->clk);
	if (ret) {
		dev_err(&pdev->dev, "enable clk failed!\n");
		goto err_chan_free;
	}

	ret = reset_control_deassert(sdev->reset);
	if (ret) {
		dev_err(&pdev->dev, "reset dassert failed!\n");
		goto err_clk_disable;
	}

	ret = devm_request_irq(&pdev->dev, sdev->irq, aic_dma_interrupt, 0,
			       dev_name(&pdev->dev), sdev);
	if (ret) {
		dev_err(&pdev->dev, "Cannot request IRQ\n");
		goto err_reset_assert;
	}

	ret = dma_async_device_register(&sdev->slave);
	if (ret) {
		dev_warn(&pdev->dev, "register DMA device failed\n");
		goto err_irq_disable;
	}

	ret = of_dma_controller_register(pdev->dev.of_node, aic_dma_of_xlate,
					 sdev);
	if (ret) {
		dev_err(&pdev->dev, "dma register failed!\n");
		goto err_dma_unregister;
	}
	dev_info(&pdev->dev, "ArtInChip DMA loaded");

	return 0;

err_dma_unregister:
	dma_async_device_unregister(&sdev->slave);
err_irq_disable:
	aic_kill_tasklet(sdev);
err_reset_assert:
	reset_control_assert(sdev->reset);
err_clk_disable:
	clk_disable_unprepare(sdev->clk);
err_chan_free:
	aic_dma_free(sdev);
	return ret;
}

static int aic_dma_remove(struct platform_device *pdev)
{
	struct aic_dma_dev *sdev = platform_get_drvdata(pdev);

	of_dma_controller_free(pdev->dev.of_node);
	dma_async_device_unregister(&sdev->slave);

	aic_kill_tasklet(sdev);

	clk_disable_unprepare(sdev->clk);
	reset_control_assert(sdev->reset);

	aic_dma_free(sdev);

	return 0;
}

const struct aic_dma_inf aic_dma_interface = {
	.nr_chans = 8,
	.nr_ports = 24,
	.nr_vchans = 24,
	.burst_length = BIT(1) | BIT(4) | BIT(8) | BIT(16),
};

static const struct of_device_id aic_dma_match[] = {
	{ .compatible = "artinchip,aic-dma-v0.1", .data = &aic_dma_interface },
	{ .compatible = "artinchip,aic-dma-v1.0", .data = &aic_dma_interface },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, aic_dma_match);

static struct platform_driver aic_dma_driver = {
	.probe		= aic_dma_probe,
	.remove		= aic_dma_remove,
	.driver = {
		.name		= "aic-dma",
		.of_match_table	= aic_dma_match,
	},
};
module_platform_driver(aic_dma_driver);
