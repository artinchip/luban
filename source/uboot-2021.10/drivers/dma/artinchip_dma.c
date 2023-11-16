// SPDX-License-Identifier: GPL-2.0+
/*
 * Direct Memory Access ArtInChip driver
 *
 * Copyright (C) 2020 ArtInChip Technology Co.,Ltd.
 */

#include <common.h>
#include <cpu_func.h>
#include <dm.h>
#include <dm/read.h>
#include <dm/device_compat.h>
#include <dma-uclass.h>
#include <memalign.h>
#include <dma.h>
#include <dt-structs.h>
#include <errno.h>
#include <clk.h>
#include <reset.h>
#include <asm/io.h>
#include <linux/iopoll.h>

#define AIC_DMA_PHY_CH_MASK              0xFFFF0000
#define AIC_DMA_PHY_CH_OFF               16
#define AIC_DMA_PORT_MASK                0x0000FFFF
#define AIC_DMA_PORT_OFF                 0

#define AIC_DMA_PHY_CH_CNT               8
#define AIC_DMA_PORT_CNT                 24
#define AIC_DMA_BUF_SIZE                 1024
#define AIC_DMA_TIMEOUT_US               10000000

#define DMA_BURST_1                      (0)
#define DMA_BURST_4                      (1)
#define DMA_BURST_8                      (2)
#define DMA_BURST_16                     (3)
#define DMA_BIT_WIDTH_8                  (0)
#define DMA_BIT_WIDTH_16                 (1)
#define DMA_BIT_WIDTH_32                 (2)
#define DMA_BIT_WIDTH_64                 (3)
#define LINEAR_MODE                      0
#define IO_MODE                          1
#define DMA_DESC_NULL                    ((u32)0xFFFFF800)

#define DMA_REG_IRQ_EN(ud)               (ud->base + 0x00)
#define DMA_REG_IRQ_STS(ud)              (ud->base + 0x10)
#define DMA_REG_STATUS(ud)               (ud->base + 0x30)

#define CH_OFFSET                        (0x40)
#define CH_BASE(ud)                      (ud->base + 0x100)
#define DMA_REG_CH_EN(ud, ch)            (CH_BASE(ud) + (ch) * CH_OFFSET + 0x00)
#define DMA_REG_CH_PAUSE(ud, ch)         (CH_BASE(ud) + (ch) * CH_OFFSET + 0x04)
#define DMA_REG_CH_DESC_ADDR(ud, ch)     (CH_BASE(ud) + (ch) * CH_OFFSET + 0x08)
#define DMA_REG_CH_CUR_CFG(ud, ch)       (CH_BASE(ud) + (ch) * CH_OFFSET + 0x0c)
#define DMA_REG_CH_CUR_SADDR(ud, ch)     (CH_BASE(ud) + (ch) * CH_OFFSET + 0x10)
#define DMA_REG_CH_CUR_DADDR(ud, ch)     (CH_BASE(ud) + (ch) * CH_OFFSET + 0x14)
#define DMA_REG_CH_BCNT_LEFT(ud, ch)     (CH_BASE(ud) + (ch) * CH_OFFSET + 0x18)
#define DMA_REG_CH_PARAM(ud, ch)         (CH_BASE(ud) + (ch) * CH_OFFSET + 0x1c)
#define DMA_REG_CH_MODE(ud, ch)          (CH_BASE(ud) + (ch) * CH_OFFSET + 0x28)
#define DMA_REG_CH_FORMER(ud, ch)        (CH_BASE(ud) + (ch) * CH_OFFSET + 0x2c)
#define DMA_REG_CH_PACKAGE(ud, ch)       (CH_BASE(ud) + (ch) * CH_OFFSET + 0x30)

#define SRC_WAIT_DST_WAIT_MODE           (0 << 2)
#define SRC_HANDSHAKE_DST_WAIT_MODE      (1 << 2)
#define SRC_WAIT_DST_HANDSHAKE_MODE      (2 << 2)
#define SRC_HANDSHAKE_DST_HANDSHAKE_MODE (3 << 2)

struct aic_dma_dev;

struct dma_cfg {
	u16 port        : 5; /* bit[4:0] */
	u16 res0        : 1;
	u16 burst_size  : 2; /* bit[7:6] */
	u16 addr_mode   : 1; /* bit[8] */
	u16 bit_width   : 2; /* bit[10:9] */
	u16 res1        : 5;
};

/* DMA descriptor */
struct aic_dma_desc {
	struct dma_cfg src_port;
	struct dma_cfg sink_port;
	u32    src_addr;
	u32    sink_addr;
	u32    byte_cnt;
	u32    wait_cyc: 8;
	u32    res0:     24;
	u32    next;
};

#define DMA_CFG(p, burst, mode, width)                                         \
	{                                                                      \
		.port = p,                                                     \
		.res0 = 0,                                                     \
		.burst_size = burst,                                           \
		.addr_mode = mode,                                             \
		.bit_width = width,                                            \
		.res1 = 0,                                                     \
	}

static struct dma_cfg dma_port_cfg[] = {
	DMA_CFG(0, DMA_BURST_16, LINEAR_MODE, DMA_BIT_WIDTH_32), /* SRAM */
	DMA_CFG(1, DMA_BURST_8, LINEAR_MODE, DMA_BIT_WIDTH_32), /* DRAM */
	DMA_CFG(2, DMA_BURST_16, IO_MODE, DMA_BIT_WIDTH_32), /* UNDEFINED */
	DMA_CFG(3, DMA_BURST_16, IO_MODE, DMA_BIT_WIDTH_32), /* UNDEFINED */
	DMA_CFG(4, DMA_BURST_16, IO_MODE, DMA_BIT_WIDTH_32), /* UNDEFINED */
	DMA_CFG(5, DMA_BURST_16, IO_MODE, DMA_BIT_WIDTH_32), /* UNDEFINED */
	DMA_CFG(6, DMA_BURST_16, IO_MODE, DMA_BIT_WIDTH_32), /* UNDEFINED */
	DMA_CFG(7, DMA_BURST_16, IO_MODE, DMA_BIT_WIDTH_32), /* UNDEFINED */
	DMA_CFG(8, DMA_BURST_16, IO_MODE, DMA_BIT_WIDTH_32), /* UNDEFINED */
	DMA_CFG(9, DMA_BURST_16, IO_MODE, DMA_BIT_WIDTH_32), /* UNDEFINED */
	DMA_CFG(10, DMA_BURST_8, IO_MODE, DMA_BIT_WIDTH_32), /* SPI0 */
	DMA_CFG(11, DMA_BURST_8, IO_MODE, DMA_BIT_WIDTH_32), /* SPI1 */
#ifndef CONFIG_SPL_BUILD
	DMA_CFG(12, DMA_BURST_1, IO_MODE, DMA_BIT_WIDTH_32), /* I2S0 */
	DMA_CFG(13, DMA_BURST_1, IO_MODE, DMA_BIT_WIDTH_32), /* I2S1 */
	DMA_CFG(14, DMA_BURST_1, IO_MODE, DMA_BIT_WIDTH_32), /* DMIC */
	DMA_CFG(15, DMA_BURST_1, IO_MODE, DMA_BIT_WIDTH_32), /* UNDEFINED */
	DMA_CFG(16, DMA_BURST_1, IO_MODE, DMA_BIT_WIDTH_32), /* UART0 */
	DMA_CFG(17, DMA_BURST_1, IO_MODE, DMA_BIT_WIDTH_32), /* UART1 */
	DMA_CFG(18, DMA_BURST_1, IO_MODE, DMA_BIT_WIDTH_32), /* UART2 */
	DMA_CFG(19, DMA_BURST_1, IO_MODE, DMA_BIT_WIDTH_32), /* UART3 */
	DMA_CFG(20, DMA_BURST_1, IO_MODE, DMA_BIT_WIDTH_32), /* UART4 */
	DMA_CFG(21, DMA_BURST_1, IO_MODE, DMA_BIT_WIDTH_32), /* UART5 */
	DMA_CFG(22, DMA_BURST_1, IO_MODE, DMA_BIT_WIDTH_32), /* UART6 */
	DMA_CFG(23, DMA_BURST_1, IO_MODE, DMA_BIT_WIDTH_32), /* UART7 */
#endif
};

struct aic_dma_dev {
	struct device *dev;
	void __iomem *base;
	struct reset_ctl reset;
	struct clk clk;
	u32 phy_ch;
	uchar	*rx_buf;
	size_t	rx_len;
};

static int aic_dma_phy_request(struct aic_dma_dev *ud)
{
	int i;

	if ((ud->phy_ch & 0xFF) == 0xFF)
		return -EBUSY;

	for (i = 0; i < AIC_DMA_PHY_CH_CNT; i++) {
		if ((ud->phy_ch & (1 << i)) == 0) {
			ud->phy_ch |= (1 << i);
			return i;
		}
	}

	return -EBUSY;
}

static int aic_dma_phy_release(struct aic_dma_dev *ud, int id)
{
	ud->phy_ch &= ~(1 << id);
	return 0;
}

static int aic_dma_request(struct dma *dma)
{
	struct aic_dma_dev *ud = dev_get_priv(dma->dev);
	int phy_ch = 0;

	if ((dma->id & AIC_DMA_PHY_CH_MASK) != 0)
		return -EINVAL;
	if (dma->id >= AIC_DMA_PORT_CNT)
		return -EINVAL;

	phy_ch = aic_dma_phy_request(ud);
	if (phy_ch < 0)
		return -EBUSY;

	/*
	 * DMA ID in DTS is the DMA port id for AIC chip, driver still need to
	 * allocate dma channel for it when user program request one DMA.
	 *
	 * Here set the physical dma channel to the upper bit field of the DMA
	 * ID.
	 */
	dma->id |= (phy_ch << AIC_DMA_PHY_CH_OFF);
	debug("%s(dma id=0x%lx)\n", __func__, dma->id);

	return 0;
}

static int aic_dma_free(struct dma *dma)
{
	struct aic_dma_dev *ud = dev_get_priv(dma->dev);
	int port, phy_ch;

	port = (dma->id & AIC_DMA_PORT_MASK);
	phy_ch = (dma->id >> AIC_DMA_PHY_CH_OFF);
	if (port >= AIC_DMA_PORT_CNT)
		return -EINVAL;

	debug("%s(dma id=0x%lx)\n", __func__, dma->id);
	aic_dma_phy_release(ud, phy_ch);
	ud->rx_buf = NULL;
	ud->rx_len = 0;

	return 0;
}

static u32 aic_dma_get_mode(struct aic_dma_desc *desc)
{
	u16 saddr_mode, daddr_mode;

	saddr_mode = desc->src_port.addr_mode;
	daddr_mode = desc->sink_port.addr_mode;

	if ((saddr_mode == LINEAR_MODE) && (daddr_mode == LINEAR_MODE))
		return SRC_WAIT_DST_WAIT_MODE;
	if ((saddr_mode == LINEAR_MODE) && (daddr_mode == IO_MODE))
		return SRC_WAIT_DST_HANDSHAKE_MODE;
	if ((saddr_mode == IO_MODE) && (daddr_mode == LINEAR_MODE))
		return SRC_HANDSHAKE_DST_WAIT_MODE;
	if ((saddr_mode == IO_MODE) && (daddr_mode == IO_MODE))
		return SRC_HANDSHAKE_DST_HANDSHAKE_MODE;
	return SRC_WAIT_DST_WAIT_MODE;
}

static void aic_dma_start(struct aic_dma_dev *ud, int phy_ch,
			  struct aic_dma_desc *desc)
{
	u32 val;

	writel(0, DMA_REG_CH_EN(ud, phy_ch));
	val = readl(DMA_REG_IRQ_STS(ud));
	val |= 0xF << (phy_ch * 4);
	writel(val, DMA_REG_IRQ_STS(ud));

	do {
		writel((unsigned long)desc, DMA_REG_CH_DESC_ADDR(ud, phy_ch));
	} while ((unsigned long)desc != readl(DMA_REG_CH_DESC_ADDR(ud, phy_ch)));

	val = aic_dma_get_mode(desc);
	writel(val, DMA_REG_CH_MODE(ud, phy_ch));
	writel(1, DMA_REG_CH_EN(ud, phy_ch));
	writel(0, DMA_REG_CH_PAUSE(ud, phy_ch));
}

static void aic_dma_stop(struct aic_dma_dev *ud, int phy_ch)
{
	u32 val;

	val = readl(DMA_REG_CH_EN(ud, phy_ch));
	if (val == 0)
		return;
	writel(1, DMA_REG_CH_PAUSE(ud, phy_ch));
	writel(0, DMA_REG_CH_EN(ud, phy_ch));
	writel(0, DMA_REG_CH_PAUSE(ud, phy_ch));
	writel(DMA_DESC_NULL, DMA_REG_CH_DESC_ADDR(ud, phy_ch));
	val = readl(DMA_REG_IRQ_STS(ud));
	val |= 0xF << (phy_ch * 4);
	writel(val, DMA_REG_IRQ_STS(ud));
}

static int aic_dma_enable(struct dma *dma)
{
	int port;

	port = (dma->id & AIC_DMA_PORT_MASK);
	if (port >= AIC_DMA_PORT_CNT)
		return -EINVAL;

	/* Do nothing */
	return 0;
}

static int aic_dma_disable(struct dma *dma)
{
	struct aic_dma_dev *ud = dev_get_priv(dma->dev);
	int port, phy_ch;

	port = (dma->id & AIC_DMA_PORT_MASK);
	phy_ch = (dma->id >> AIC_DMA_PHY_CH_OFF);
	if (port >= AIC_DMA_PORT_CNT)
		return -EINVAL;

	aic_dma_stop(ud, phy_ch);
	return 0;
}

#define pr_reg(addr) debug("0x%lx: 0x%x\n", (unsigned long)addr, (u32)readl(addr))
void aic_dma_dump(struct aic_dma_dev *ud)
{
	int i;

	debug("DMA Regs:\n");
	pr_reg(DMA_REG_IRQ_EN(ud));
	pr_reg(DMA_REG_IRQ_STS(ud));
	pr_reg(DMA_REG_STATUS(ud));
	for (i = 0; i < 8; i++) {
		pr_reg(DMA_REG_CH_EN(ud, i));
		pr_reg(DMA_REG_CH_PAUSE(ud, i));
		pr_reg(DMA_REG_CH_DESC_ADDR(ud, i));
		pr_reg(DMA_REG_CH_CUR_CFG(ud, i));
		pr_reg(DMA_REG_CH_CUR_SADDR(ud, i));
		pr_reg(DMA_REG_CH_CUR_DADDR(ud, i));
		pr_reg(DMA_REG_CH_BCNT_LEFT(ud, i));
		pr_reg(DMA_REG_CH_PARAM(ud, i));
		pr_reg(DMA_REG_CH_MODE(ud, i));
		pr_reg(DMA_REG_CH_FORMER(ud, i));
		pr_reg(DMA_REG_CH_PACKAGE(ud, i));
	}
}

static void aic_dma_desc_flush(struct aic_dma_desc *desc)
{
	u32 start, stop;

	start = (unsigned long)desc;
	stop = ALIGN(start + sizeof(struct aic_dma_desc), ARCH_DMA_MINALIGN);
	flush_dcache_range(start, stop);
}

#ifndef CONFIG_SPL_BUILD
static int aic_dma_send(struct dma *dma, void *src, size_t len, void *port_addr)
{
	struct aic_dma_dev *ud = dev_get_priv(dma->dev);
	struct aic_dma_desc *desc;
	s32 port, phy_ch, ret;
	u32 val, start, stop;

	port = (dma->id & AIC_DMA_PORT_MASK);
	phy_ch = (dma->id >> AIC_DMA_PHY_CH_OFF);
	if (port >= AIC_DMA_PORT_CNT)
		return -EINVAL;
	if (!src || !port_addr)
		return -EINVAL;

	debug("send port :%d, phy ch %d\n", port, phy_ch);
	desc = malloc_cache_aligned(sizeof(struct aic_dma_desc));
	if (desc == NULL)
		return -ENOMEM;
	memset(desc, 0, sizeof(struct aic_dma_desc));

	start = (unsigned long)src;
	stop = ALIGN(start + len, ARCH_DMA_MINALIGN);
	flush_dcache_range(start, stop);

	/* MEM to DEV */
	desc->src_port = dma_port_cfg[1]; /* Source port DRAM */
	desc->sink_port = dma_port_cfg[port];
	desc->wait_cyc = 0x10;
	desc->src_addr = (unsigned long)src;
	desc->sink_addr = (unsigned long)port_addr; /* IO FIFO Address */
	desc->byte_cnt = len;
	desc->next = DMA_DESC_NULL;
	aic_dma_desc_flush(desc);

	aic_dma_start(ud, phy_ch, desc);
	ret = readl_poll_timeout(DMA_REG_STATUS(ud), val,
				 ((val & (1 << phy_ch)) == 0),
				 AIC_DMA_TIMEOUT_US);

	if (desc)
		free(desc);
	if (ret)
		return -EFAULT;
	return len;
}
#endif

static int aic_dma_prepare_rcv_buf(struct dma *dma, void *dst, size_t size)
{
	struct aic_dma_dev *ud = dev_get_priv(dma->dev);

	ud->rx_buf = dst;
	ud->rx_len = size;

	debug("%s, rx buf 0x%lx, len %ld\n", __func__, (unsigned long)dst,
	      (unsigned long)size);
	return 0;
}

static int aic_dma_receive(struct dma *dma, void **dst, void *port_addr)
{
	struct aic_dma_dev *ud = dev_get_priv(dma->dev);
	struct aic_dma_desc *desc;
	s32 port, phy_ch, ret;
	u32 val, start, stop;

	port = (dma->id & AIC_DMA_PORT_MASK);
	phy_ch = (dma->id >> AIC_DMA_PHY_CH_OFF);

	debug("recv port :%d, phy ch %d\n", port, phy_ch);
	if (port >= AIC_DMA_PORT_CNT)
		return -EINVAL;
	if (!dst || !port_addr)
		return -EINVAL;

	if (!ud->rx_len || !ud->rx_buf)
		return 0;

	desc = malloc_cache_aligned(sizeof(struct aic_dma_desc));
	if (desc == NULL)
		return -ENOMEM;
	memset(desc, 0, sizeof(struct aic_dma_desc));

	/* DEV to MEM */
	desc->src_port = dma_port_cfg[port];
	desc->sink_port = dma_port_cfg[1];
	desc->wait_cyc = 0x40;
	desc->src_addr = (unsigned long)port_addr; /* IO FIFO Address */
	desc->sink_addr = (unsigned long)ud->rx_buf;
	desc->byte_cnt = (u32)ud->rx_len;
	desc->next = DMA_DESC_NULL;
	aic_dma_desc_flush(desc);

	start = (unsigned long)ud->rx_buf;
	stop = ALIGN(start + ud->rx_len, ARCH_DMA_MINALIGN);
	/* Invalidate the area, so no writeback into the RAM races with DMA */
	invalidate_dcache_range(start, stop);

	aic_dma_start(ud, phy_ch, desc);
	ret = readl_poll_timeout(DMA_REG_STATUS(ud), val,
				 ((val & (1 << phy_ch)) == 0),
				 AIC_DMA_TIMEOUT_US);
	if (ret) {
		ret = -EIO;
		goto out;
	}

	invalidate_dcache_range(start, stop);
	*dst = ud->rx_buf;
	ret = (s32)ud->rx_len;

	ud->rx_buf = NULL;
	ud->rx_len = 0;
out:
	if (desc)
		free(desc);
	return ret;
}

#ifndef CONFIG_SPL_BUILD
static int aic_dma_transfer(struct udevice *dev, int direction, void *dst,
			    void *src, size_t len)
{
	struct aic_dma_dev *ud = dev_get_priv(dev);
	struct aic_dma_desc *desc;
	int phy_ch, ret;
	u32 val, start, stop;

	if (direction != DMA_MEM_TO_MEM)
		return -EINVAL;

	desc = malloc_cache_aligned(sizeof(struct aic_dma_desc));
	if (desc == NULL)
		return -ENOMEM;
	memset(desc, 0, sizeof(struct aic_dma_desc));

	start = (unsigned long)src;
	stop = ALIGN(start + len, ARCH_DMA_MINALIGN);
	flush_dcache_range(start, stop);

	phy_ch = aic_dma_phy_request(ud);
	if (phy_ch < 0) {
		ret = -EBUSY;
		goto out;
	}

	/* MEM to MEM */
	desc->src_port = dma_port_cfg[1];
	desc->sink_port = dma_port_cfg[1];
	desc->wait_cyc = 0x40;
	desc->src_addr = (unsigned long)src;
	desc->sink_addr = (unsigned long)dst;
	desc->byte_cnt = len;
	desc->next = DMA_DESC_NULL;
	aic_dma_desc_flush(desc);

	aic_dma_start(ud, phy_ch, desc);
	ret = readl_poll_timeout(DMA_REG_STATUS(ud), val,
				 ((val & (1 << phy_ch)) == 0),
				 AIC_DMA_TIMEOUT_US);
	if (ret) {
		aic_dma_stop(ud, phy_ch);
		ret = -EFAULT;
		goto out;
	}

	aic_dma_stop(ud, phy_ch);

	start = (unsigned long)dst;
	stop = ALIGN(start + len, ARCH_DMA_MINALIGN);
	invalidate_dcache_range(start, stop);
out:
	if (desc)
		free(desc);

	return ret;
}
#endif

static const struct dma_ops aic_dma_ops = {
#ifndef CONFIG_SPL_BUILD
	/* SPL don't support dma memory copy, for code size saving */
	.transfer        = aic_dma_transfer,
#endif
	.request         = aic_dma_request,
	.rfree           = aic_dma_free,
	.enable          = aic_dma_enable,
	.disable         = aic_dma_disable,
#ifndef CONFIG_SPL_BUILD
	/* SPL don't support dma send, for code size saving */
	.send            = aic_dma_send,
#endif
	.receive         = aic_dma_receive,
	.prepare_rcv_buf = aic_dma_prepare_rcv_buf,
};

static int aic_dma_probe(struct udevice *dev)
{
	struct dma_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	struct aic_dma_dev *ud = dev_get_priv(dev);
	int i, ret = 0;

	uc_priv->supported = DMA_SUPPORTS_MEM_TO_MEM |
			     DMA_SUPPORTS_MEM_TO_DEV |
			     DMA_SUPPORTS_DEV_TO_MEM;

	ud->base = dev_read_addr_ptr(dev);

#if !defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_CLK_ARTINCHIP)
	ret = clk_get_by_index(dev, 0, &ud->clk);
	if (ret < 0) {
		dev_err(dev, "failed to get clock\n");
		return ret;
	}

	ret = clk_enable(&ud->clk);
	if (ret < 0) {
		pr_err("error enabling clock\n");
		return ret;
	}

	ret = reset_get_by_index(dev, 0, &ud->reset);
	if (ret && ret != -ENOENT) {
		dev_err(dev, "failed to get reset\n");
		return ret;
	}

	ret = reset_deassert(&ud->reset);
	if (ret < 0) {
		pr_err("error deasserting reset %d\n", i);
		return ret;
	}
#endif

	ud->rx_buf = NULL;
	ud->rx_len = 0;

	for (i = 0; i < AIC_DMA_PHY_CH_CNT; i++) {
		writel(1, DMA_REG_CH_PAUSE(ud, i));
		writel(0x0, DMA_REG_CH_EN(ud, i));
	}

	writel(0x0, DMA_REG_IRQ_EN(ud));
	writel(0xFFFFFFFF, DMA_REG_IRQ_STS(ud));
	writel(0xFFFFFFFF, DMA_REG_IRQ_EN(ud));

	pr_info("%s done.\n", __func__);
	return ret;
}

static const struct udevice_id aic_dma_ids[] = {
	{ .compatible = "artinchip,aic-dma-v0.1" },
	{ .compatible = "artinchip,aic-dma-v1.0" },
	{ }
};

U_BOOT_DRIVER(aic_dma) = {
	.name      = "artinchip_aic_dma",
	.id        = UCLASS_DMA,
	.of_match  = aic_dma_ids,
	.ops       = &aic_dma_ops,
	.probe     = aic_dma_probe,
	.priv_auto = sizeof(struct aic_dma_dev),
};
