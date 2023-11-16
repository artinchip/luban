/*
 * Copyright (c) 2022, ArtInChip Technology Co., Ltd
 * Dehuang Wu <dehuang.wu@artinchip.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <dm/device.h>
#include <dm/device_compat.h>
#include <dt-structs.h>
#include <mapmem.h>
#include <spi.h>
#include <spi-mem.h>
#include <dma.h>
#include <errno.h>
#include <fdt_support.h>
#include <reset.h>
#include <wait_bit.h>
#include <asm/bitops.h>
#include <asm/gpio.h>
#include <asm/cache.h>
#include <asm/io.h>
#include <linux/iopoll.h>
#include <hexdump.h>

DECLARE_GLOBAL_DATA_PTR;

/* AIC spi registers */
#define SPI_REG_VER(priv)       (priv->base + 0x00)
#define SPI_REG_GCR(priv)       (priv->base + 0x04)
#define SPI_REG_TCR(priv)       (priv->base + 0x08)
#define SPI_REG_ICR(priv)       (priv->base + 0x10)
#define SPI_REG_ISR(priv)       (priv->base + 0x14)
#define SPI_REG_FCR(priv)       (priv->base + 0x18)
#define SPI_REG_FSR(priv)       (priv->base + 0x1c)
#define SPI_REG_WCR(priv)       (priv->base + 0x20)
#define SPI_REG_CCR(priv)       (priv->base + 0x24)
#define SPI_REG_BCR(priv)       (priv->base + 0x30)
#define SPI_REG_MTC(priv)       (priv->base + 0x34)
#define SPI_REG_BCC(priv)       (priv->base + 0x38)
#define SPI_REG_DCR(priv)       (priv->base + 0x88)
#define SPI_REG_TXD(priv)       (priv->base + 0x200)
#define SPI_REG_RXD(priv)       (priv->base + 0x300)

#define GCR_BIT_EN              BIT(0)
#define GCR_BIT_MODE            BIT(1)
#define GCR_BIT_PAUSE           BIT(7)
#define GCR_BIT_SRST            BIT(31)

#define TCR_BIT_CPHA            BIT(0)
#define TCR_BIT_CPOL            BIT(1)
#define TCR_BIT_SPOL            BIT(2)
#define TCR_BIT_SSCTL           BIT(3)
#define TCR_BIT_SS_OWNER        BIT(6)
#define TCR_BIT_SS_LEVEL        BIT(7)
#define TCR_BIT_DHB             BIT(8)
#define TCR_BIT_DDB             BIT(9)
#define TCR_BIT_RPSM            BIT(10)
#define TCR_BIT_SDC             BIT(11)
#define TCR_BIT_FBS             BIT(12)
#define TCR_BIT_XCH             BIT(31)
#define TCR_BIT_SS_SEL_OFF      (4)
#define TCR_BIT_SS_SEL_MSK      GENMASK(5, 4)

#define FCR_BIT_RX_LEVEL_MSK    GENMASK(7, 0)
#define FCR_BIT_RX_DMA_EN       BIT(8)
#define FCR_BIT_RX_RST          BIT(15)
#define FCR_BIT_TX_LEVEL_MSK    GENMASK(23, 16)
#define FCR_BIT_TX_DMA_EN       BIT(24)
#define FCR_BIT_TX_RST          BIT(31)
#define FCR_BIT_DMA_EN_MSK      (FCR_BIT_TX_DMA_EN | FCR_BIT_RX_DMA_EN)

#define FSR_BIT_RX_CNT_MSK      GENMASK(7,  0)
#define FSR_BIT_RB_CNT_MSK      GENMASK(14, 12)
#define FSR_BIT_RB_WR_EN        BIT(15)
#define FSR_BIT_TX_CNT_MSK      GENMASK(23, 16)
#define FSR_BIT_TB_CNT_MSK      GENMASK(30, 28)
#define FSR_BIT_TB_WR_EN        BIT(31)
#define FSR_BIT_RXCNT_OFF       (0)
#define FSR_BIT_TXCNT_OFF       (16)

#define ICR_BIT_RX_RDY          BIT(0)
#define ICR_BIT_RX_EMP          BIT(1)
#define ICR_BIT_RX_FULL         BIT(2)
#define ICR_BIT_TX_ERQ          BIT(4)
#define ICR_BIT_TX_EMP          BIT(5)
#define ICR_BIT_TX_FULL         BIT(6)
#define ICR_BIT_RX_OVF          BIT(8)
#define ICR_BIT_RX_UDR          BIT(9)
#define ICR_BIT_TX_OVF          BIT(10)
#define ICR_BIT_TX_UDR          BIT(11)
#define ICR_BIT_TC              BIT(12)
#define ICR_BIT_SSI             BIT(13)
#define ICR_BIT_ERRS            (ICR_BIT_TX_OVF | ICR_BIT_RX_UDR | ICR_BIT_RX_OVF)
#define ICR_BIT_ALL_MSK         (0x77 | (0x3f << 8))

#define ISR_BIT_RX_RDY          BIT(0)
#define ISR_BIT_RX_EMP          BIT(1)
#define ISR_BIT_RX_FULL         BIT(2)
#define ISR_BIT_TX_RDY          BIT(4)
#define ISR_BIT_TX_EMP          BIT(5)
#define ISR_BIT_TX_FULL         BIT(6)
#define ISR_BIT_RX_OVF          BIT(8)
#define ISR_BIT_RX_UDR          BIT(9)
#define ISR_BIT_TX_OVF          BIT(10)
#define ISR_BIT_TX_UDR          BIT(11)
#define ISR_BIT_TC              BIT(12)
#define ISR_BIT_SSI             BIT(13)
#define ISR_BIT_ERRS            (ISR_BIT_TX_OVF | ISR_BIT_RX_UDR | ISR_BIT_RX_OVF)
#define ISR_BIT_ALL_MSK         (0x77 | (0x3f << 8))

#define WCR_BIT_WCC_MSK         GENMASK(15, 0)
#define WCR_BIT_SWC_MSK         GENMASK(19, 16)

#define CCR_BIT_CDR2_OFF        (0)
#define CCR_BIT_CDR1_OFF        (8)
#define CCR_BIT_CDR2_MSK        GENMASK(7,  0)
#define CCR_BIT_CDR1_MSK        GENMASK(11, 8)
#define CCR_BIT_DRS             BIT(12)

#define BCR_BIT_CNT_MSK		GENMASK(23, 0)
#define MTC_BIT_CNT_MSK		GENMASK(23, 0)

#define BCC_BIT_STC_MSK         GENMASK(23, 0)
#define BCC_BIT_DBC_MSK         GENMASK(27, 24)
#define BCC_BIT_DUAL_MODE       BIT(28)
#define BCC_BIT_QUAD_MODE       BIT(29)

#define AIC_SPI_CS(cs)          (((cs) << TCR_BIT_SS_SEL_OFF) & TCR_BIT_SS_SEL_MSK)
#define AIC_SPI_MAX_XFER_SIZE   0xffffff
#define AIC_SPI_BURST_CNT(cnt)  ((cnt)&AIC_SPI_MAX_XFER_SIZE)
#define AIC_SPI_XMIT_CNT(cnt)   ((cnt)&AIC_SPI_MAX_XFER_SIZE)
#define SPI_FIFO_DEPTH          64
#define AIC_SPI_RX_WL           0x20
#define AIC_SPI_TX_WL           0x20

#define AIC_SPI_MAX_RATE        144000000
#define AIC_SPI_MIN_RATE        3000
#define AIC_SPI_DEFAULT_RATE    24000000
#define AIC_SPI_TIMEOUT_US      1000000

#define SPI_DUAL_MODE           1
#define SPI_QUAD_MODE           2
#define SPI_SINGLE_MODE         4

#define SPINAND_PAGE_READ_FROM_CACHE_DUALIO_OPCODE 0xbb
#define SPINAND_PAGE_READ_FROM_CACHE_QUADIO_OPCODE 0xeb

struct aic_spi_data {
	u32 fifo_depth;
	bool has_soft_reset;
	bool has_burst_ctl;
};

struct aic_spi_platdata {
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	struct dtd_artinchip_aic_spi dtplat;
#endif
	struct aic_spi_data *spi_data;
	void __iomem *base;
	u32 max_hz;
};

struct aic_spi_priv {
	struct aic_spi_data *spi_data;
	struct clk clk;
	struct reset_ctl reset;
	void __iomem *base;
	u32 freq;
	u32 mode;

#ifdef CONFIG_ARTINCHIP_DMA
	/* DMA */
	struct dma rx_dma;
	struct dma tx_dma;
#endif

	u8 *tx_buf;
	u8 *rx_buf;
};

static void aic_spi_set_cs(struct udevice *bus, u8 cs, bool enable)
{
	struct aic_spi_priv *priv = dev_get_priv(bus);
	u32 reg;

	reg = readl(SPI_REG_TCR(priv));

	reg &= ~TCR_BIT_SS_SEL_MSK;
	reg |= AIC_SPI_CS(cs);

	if (enable)
		reg &= ~TCR_BIT_SS_LEVEL;
	else
		reg |= TCR_BIT_SS_LEVEL;

	writel(reg, SPI_REG_TCR(priv));
}

static inline int aic_spi_set_clock(struct udevice *dev, bool enable)
{
	int ret = 0;
#if !defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_CLK_ARTINCHIP)
	struct aic_spi_priv *priv = dev_get_priv(dev);

	dev_dbg(dev, "enable %d\n", enable);
	if (!enable) {
		clk_disable(&priv->clk);
		if (reset_valid(&priv->reset))
			reset_assert(&priv->reset);
		return 0;
	}

	ret = clk_enable(&priv->clk);
	if (ret) {
		dev_err(dev, "failed to enable clock (ret=%d)\n", ret);
		return ret;
	}

	udelay(500);
	clk_set_rate(&priv->clk, priv->freq);
	if (reset_valid(&priv->reset)) {
		ret = reset_deassert(&priv->reset);
		if (ret) {
			dev_err(dev, "failed to deassert reset\n");
			goto err;
		}
	}
	return 0;

err:
	clk_disable(&priv->clk);
#endif
	return ret;
}

static int aic_spi_claim_bus(struct udevice *dev)
{
	struct aic_spi_priv *priv = dev_get_priv(dev->parent);

	setbits_le32(SPI_REG_GCR(priv),
		     GCR_BIT_EN | GCR_BIT_MODE | GCR_BIT_PAUSE);

	if (priv->spi_data->has_soft_reset)
		setbits_le32(SPI_REG_GCR(priv), GCR_BIT_SRST);

	setbits_le32(SPI_REG_TCR(priv), TCR_BIT_SS_OWNER);

#ifdef CONFIG_ARTINCHIP_DMA
	/* enable dma rx channel */
	dma_enable(&priv->rx_dma);

	/* enable dma tx channel */
	dma_enable(&priv->tx_dma);
#endif
	return 0;
}

static bool aic_spi_in_single_mode(struct aic_spi_priv *priv)
{
	u32 val;

	val = readl(SPI_REG_BCC(priv));
	if ((val & BCC_BIT_QUAD_MODE) || (val & BCC_BIT_DUAL_MODE))
		return false;

	return true;
}

static void aic_spi_reset_fifo(struct aic_spi_priv *priv)
{
	u32 val = readl(SPI_REG_FCR(priv));

	val |= (FCR_BIT_RX_RST | FCR_BIT_TX_RST);

	/* Set the trigger level of RxFIFO/TxFIFO. */
	val &= ~(FCR_BIT_RX_LEVEL_MSK | FCR_BIT_TX_LEVEL_MSK);
	val |= (AIC_SPI_TX_WL << 16) | AIC_SPI_RX_WL;
	writel(val, SPI_REG_FCR(priv));
}

static void aic_spi_set_xfer_cnt(struct aic_spi_priv *priv, u32 tx_len,
				 u32 rx_len, u32 single_len, u32 dummy_cnt)
{
	u32 reg_val = readl(SPI_REG_BCR(priv));

	pr_debug("xfer cnt: tx %d, rx %d, dummy %d\n", tx_len, rx_len, dummy_cnt);

	/* Total transfer length */
	reg_val &= ~BCR_BIT_CNT_MSK;
	reg_val |= (BCR_BIT_CNT_MSK & (tx_len + rx_len + dummy_cnt));
	writel(reg_val, SPI_REG_BCR(priv));

	/* Total send data length */
	reg_val = readl(SPI_REG_MTC(priv));
	reg_val &= ~MTC_BIT_CNT_MSK;
	reg_val |= (MTC_BIT_CNT_MSK & tx_len);
	writel(reg_val, SPI_REG_MTC(priv));

	/*
	 * Special configuration:
	 * # Data length need to send in single mode
	 *   - If the transfer is already single mode, the STC length should be
	 *     the same with total send data length
	 *   - If the transfer is DUAL/QUAD mode, the STC specifies the length
	 *     that need to send out in single mode, usually for cmd,addr,...
	 * # Dummy count for DUAL/QUAD mode
	 *   - DBC specifies how many dummy burst should be sent before receive
	 *     data in DUAL/QUAD mode
	 */
	reg_val = readl(SPI_REG_BCC(priv));
	reg_val &= ~BCC_BIT_STC_MSK;
	reg_val |= (BCC_BIT_STC_MSK & single_len);
	reg_val &= ~(0xf << 24);
	reg_val |= (dummy_cnt << 24);
	writel(reg_val, SPI_REG_BCC(priv));
}

static int aic_spi_release_bus(struct udevice *dev)
{
	struct aic_spi_priv *priv = dev_get_priv(dev->parent);

	clrbits_le32(SPI_REG_GCR(priv), GCR_BIT_EN);

#ifdef CONFIG_ARTINCHIP_DMA
	/* disable dma rx channel */
	dma_disable(&priv->rx_dma);

	/* disable dma tx channel */
	dma_disable(&priv->tx_dma);
#endif
	return 0;
}

#ifdef CONFIG_ARTINCHIP_DMA
static int aic_spi_xfer_dma(struct udevice *dev, unsigned int bitlen,
			    const void *dout, void *din, unsigned long flags)
{
	u32 len = bitlen / 8, val, tx_len, rx_len, single_len;
	struct dm_spi_slave_plat *slave_plat;
	struct udevice *bus = dev->parent;
	struct aic_spi_priv *priv;
	int ret = 0;

	tx_len = 0;
	rx_len = 0;
	single_len = 0;
	slave_plat = dev_get_parent_plat(dev);
	priv = dev_get_priv(bus);
	priv->tx_buf = (u8 *)dout;
	priv->rx_buf = (u8 *)din;

	if (bitlen % 8) {
		dev_err(bus, "%s: non byte-aligned SPI transfer.\n", __func__);
		return -ENAVAIL;
	}

	if (flags & SPI_XFER_BEGIN)
		aic_spi_set_cs(bus, slave_plat->cs, true);

	aic_spi_reset_fifo(priv);

	if (priv->tx_buf) {
		val = readl(SPI_REG_FCR(priv));
		val |= (FCR_BIT_TX_DMA_EN);
		writel(val, SPI_REG_FCR(priv));

		tx_len = len;
		if (aic_spi_in_single_mode(priv))
			single_len = tx_len;
		aic_spi_set_xfer_cnt(priv, tx_len, 0, single_len, 0);
		setbits_le32(SPI_REG_TCR(priv), TCR_BIT_XCH);

		ret = dma_send(&priv->tx_dma, (void *)priv->tx_buf, len,
			       (void *)SPI_REG_TXD(priv));
		if (ret < 0)
			goto send_err;
		ret = readl_poll_timeout(SPI_REG_ISR(priv), val,
					 (val & ISR_BIT_TC),
					 AIC_SPI_TIMEOUT_US);
send_err:
		if (ret < 0) {
			dev_err(bus, "%s: Timeout to send data\n", __func__);
			clrbits_le32(SPI_REG_FCR(priv), FCR_BIT_TX_DMA_EN);
			aic_spi_set_cs(bus, slave_plat->cs, false);
			return ret;
		}
		/* Write 1 to clear */
		setbits_le32(SPI_REG_ISR(priv), ISR_BIT_TC);
		clrbits_le32(SPI_REG_FCR(priv), FCR_BIT_TX_DMA_EN);
	}

	if (priv->rx_buf) {
		val = readl(SPI_REG_FCR(priv));
		val |= (FCR_BIT_RX_DMA_EN);
		writel(val, SPI_REG_FCR(priv));

		rx_len = len;
		aic_spi_set_xfer_cnt(priv, 0, rx_len, 0, 0);
		setbits_le32(SPI_REG_TCR(priv), TCR_BIT_XCH);
		// DMA recieve
		dma_prepare_rcv_buf(&priv->rx_dma, (void *)priv->rx_buf, len);
		ret = dma_receive(&priv->rx_dma, (void **)&priv->rx_buf,
				  (void *)SPI_REG_RXD(priv));
		if (ret < 0)
			goto recv_err;

		ret = readl_poll_timeout(SPI_REG_ISR(priv), val,
					 (val & ISR_BIT_TC),
					 AIC_SPI_TIMEOUT_US);
recv_err:
		if (ret < 0) {
			dev_err(bus, "%s: Timeout to recv data\n", __func__);
			clrbits_le32(SPI_REG_FCR(priv), FCR_BIT_RX_DMA_EN);
			aic_spi_set_cs(bus, slave_plat->cs, false);
			return ret;
		}
		/* Write 1 to clear */
		setbits_le32(SPI_REG_ISR(priv), ISR_BIT_TC);
		clrbits_le32(SPI_REG_FCR(priv), FCR_BIT_RX_DMA_EN);
	}

	if (flags & SPI_XFER_END)
		aic_spi_set_cs(bus, slave_plat->cs, false);

	return ret;
}
#endif

static u32 aic_spi_txfifo_query(struct aic_spi_priv *priv)
{
	u32 val = (FSR_BIT_TX_CNT_MSK & readl(SPI_REG_FSR(priv)));

	val >>= FSR_BIT_TXCNT_OFF;
	return val;
}

static u32 aic_spi_rxfifo_query(struct aic_spi_priv *priv)
{
	u32 val = (FSR_BIT_RX_CNT_MSK & readl(SPI_REG_FSR(priv)));

	val >>= FSR_BIT_RXCNT_OFF;
	return val;
}

static int aic_spi_fifo_write(struct aic_spi_priv *priv, u8 *tx_buf, u32 len)
{
	unsigned int poll_time = 0x7ffffff;
	unsigned int dolen, fifo_len;

	while (len) {
		while (aic_spi_txfifo_query(priv) > (SPI_FIFO_DEPTH / 2))
			;
		fifo_len = SPI_FIFO_DEPTH - aic_spi_txfifo_query(priv);
		dolen = min(fifo_len, len);
		while (dolen) {
			writeb(*tx_buf++, SPI_REG_TXD(priv));
			len--;
			dolen--;
		}
	}

	poll_time = 0x7ffffff;
	while (aic_spi_txfifo_query(priv) && (--poll_time > 0))
		;
	if (poll_time <= 0) {
		pr_err("cpu transfer data time out!\n");
		return -1;
	}

	return 0;
}

static int aic_spi_fifo_read(struct aic_spi_priv *priv, u8 *rx_buf, u32 len)
{
	unsigned int poll_time = 0x7ffffff;
	unsigned int dolen;

	while (len) {
		dolen = aic_spi_rxfifo_query(priv);
		if (dolen == 0) {
			poll_time--;
			if (poll_time == 0)
				break;
			continue;
		}
		while (dolen) {
			*rx_buf++ = readb(SPI_REG_RXD(priv));
			--dolen;
			--len;
		}
	}
	if (poll_time == 0) {
		pr_err("cpu receive data time out!\n");
		return -1;
	}

	return 0;
}

static int aic_spi_xfer_fifo(struct udevice *dev, unsigned int bitlen,
			     const void *dout, void *din, unsigned long flags)
{
	u32 val, len = bitlen / 8, tx_len, rx_len, single_len;
	struct dm_spi_slave_plat *slave_plat;
	struct udevice *bus = dev->parent;
	struct aic_spi_priv *priv;
	int ret = 0;
	tx_len = 0;
	rx_len = 0;
	single_len = 0;

	slave_plat = dev_get_parent_plat(dev);
	priv = dev_get_priv(bus);
	priv->tx_buf = (u8 *)dout;
	priv->rx_buf = (u8 *)din;

	if (bitlen % 8) {
		dev_err(bus, "%s: non byte-aligned SPI transfer.\n", __func__);
		return -EINVAL;
	}

	if (flags & SPI_XFER_BEGIN)
		aic_spi_set_cs(bus, slave_plat->cs, true);

	/* Reset FIFOs */
	aic_spi_reset_fifo(priv);

	if (priv->tx_buf) {
		tx_len = len;
		if (aic_spi_in_single_mode(priv))
			single_len = tx_len;
		aic_spi_set_xfer_cnt(priv, tx_len, 0, single_len, 0);
		/* Start transfer */
		setbits_le32(SPI_REG_TCR(priv), TCR_BIT_XCH);
		ret = aic_spi_fifo_write(priv, priv->tx_buf, len);
		if (ret) {
			ret = -EIO;
			goto out;
		}
		ret = readl_poll_timeout(SPI_REG_ISR(priv), val,
					 (val & ISR_BIT_TC),
					 AIC_SPI_TIMEOUT_US);
		if (ret < 0) {
			dev_err(bus, "%s: Timeout to send data\n", __func__);
			goto out;
		}

		setbits_le32(SPI_REG_ISR(priv), ISR_BIT_TX_EMP);
		setbits_le32(SPI_REG_ISR(priv), ISR_BIT_TX_FULL);
		setbits_le32(SPI_REG_ISR(priv), ISR_BIT_TX_RDY);
		setbits_le32(SPI_REG_ISR(priv), ISR_BIT_TC);
	}

	if (priv->rx_buf) {
		rx_len = len;
		aic_spi_set_xfer_cnt(priv, 0, rx_len, 0, 0);
		/* Start transfer */
		setbits_le32(SPI_REG_TCR(priv), TCR_BIT_XCH);

		ret = aic_spi_fifo_read(priv, priv->rx_buf, len);
		if (ret) {
			ret = -EIO;
			goto out;
		}
		ret = readl_poll_timeout(SPI_REG_ISR(priv), val,
					 (val & ISR_BIT_TC),
					 AIC_SPI_TIMEOUT_US);
		if (ret < 0) {
			dev_err(bus, "%s: Timeout to recv data\n", __func__);
			goto out;
		}

		setbits_le32(SPI_REG_ISR(priv), ISR_BIT_TC);
		setbits_le32(SPI_REG_ISR(priv), ISR_BIT_RX_EMP);
		setbits_le32(SPI_REG_ISR(priv), ISR_BIT_RX_FULL);
		setbits_le32(SPI_REG_ISR(priv), ISR_BIT_RX_RDY);
	}

out:
	if (flags & SPI_XFER_END)
		aic_spi_set_cs(bus, slave_plat->cs, false);

	return ret;
}

static int aic_spi_xfer(struct udevice *dev, unsigned int bitlen,
			const void *dout, void *din, unsigned long flags)
{
	u32 len = bitlen / 8;
	u32 addr_align = 0;
	u32 len_unalign = 0;

	/* Half-duplex only */
	if (din && dout) {
		dev_err(dev, "Half-duplex support only\n");
		return -EINVAL;
	}

#ifdef CONFIG_ARTINCHIP_DMA
	len_unalign = len % (SPI_FIFO_DEPTH / 2);

	if (dout)
		addr_align = (((unsigned long)dout) & (ARCH_DMA_MINALIGN - 1))
				== 0;
	else
		addr_align = (((unsigned long)din) & (ARCH_DMA_MINALIGN - 1))
				== 0;

	if ((len >= SPI_FIFO_DEPTH) && (len >= ARCH_DMA_MINALIGN) &&
	     addr_align && !len_unalign)
		return aic_spi_xfer_dma(dev, bitlen, dout, din, flags);
	else
		return aic_spi_xfer_fifo(dev, bitlen, dout, din, flags);
#else
	(void)len;
	(void)addr_align;
	(void)len_unalign;
	return aic_spi_xfer_fifo(dev, bitlen, dout, din, flags);
#endif
}

#if !defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_CLK_ARTINCHIP)
static u32 spi_get_best_div_param(u32 spiclk, u32 mclk, u32 *div)
{
	u32 cdr1_clk, cdr2_clk, cdr1_clkt, cdr2_clkt;
	s32 cdr2, cdr1;

	/* Get the best cdr1 param if going to use cdr1 */
	cdr1 = 0;
	while ((mclk >> cdr1) > spiclk)
		cdr1++;
	if (cdr1 > 0xF)
		cdr1 = 0xF;

	/* Get the best cdr1 value around bus clk*/
	cdr1_clk = mclk >> cdr1;
	if (cdr1 > 0 && (cdr1_clk < spiclk)) {
		cdr1_clkt = mclk >> (cdr1 - 1);
		if ((cdr1_clkt - spiclk) < (spiclk - cdr1_clk))
			cdr1--;
	}

	/* Get the best cdr2 param if going to use cdr2 */
	cdr2 = (s32)(mclk / (spiclk * 2)) - 1;
	if (cdr2 < 0)
		cdr2 = 0;
	if (cdr2 > 0xFF)
		cdr2 = 0xFF;

	cdr2_clk = mclk / (2 * cdr2 + 1);
	if ((cdr2 < 0xFF) && (cdr2_clk > spiclk)) {
		cdr2_clkt = mclk / (2 * (cdr2 + 1) + 1);
		if ((spiclk - cdr2_clkt) < (cdr2_clk - spiclk))
			cdr2++;
	}

	cdr1_clk = mclk >> cdr1;
	cdr2_clk = mclk / (2 * cdr2 + 1);

	/* cdr1 param vs cdr2 param, use the best */
	if (cdr1_clk == spiclk) {
		*div = cdr1;
		return 0;
	} else if (cdr2_clk == spiclk) {
		*div = cdr2;
		return 1;
	} else if ((cdr2_clk < spiclk) && (cdr1_clk < spiclk)) {
		/* Two clks less than expect clk, use the larger one */
		if (cdr2_clk > cdr1_clk) {
			*div = cdr2;
			return 1;
		}
		*div = cdr1;
		return 0;
	}
	/*
	 * 1. Two clks great than expect clk, use least one
	 * 2. There is one clk less than expect clk, use it
	 */
	if (cdr2_clk < cdr1_clk) {
		*div = cdr2;
		return 1;
	}
	*div = cdr1;
	return 0;
}
#endif

static int aic_spi_set_speed(struct udevice *dev, uint speed)
{
#if !defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_CLK_ARTINCHIP)
	struct aic_spi_platdata *plat = dev_get_plat(dev);
	struct aic_spi_priv *priv = dev_get_priv(dev);
	u32 reg, cdr, div;

	dev_info(dev, "speed %d\n", speed);
	if (speed > plat->max_hz)
		speed = plat->max_hz;

	if (speed < AIC_SPI_MIN_RATE)
		speed = AIC_SPI_MIN_RATE;

	cdr = spi_get_best_div_param(speed, plat->max_hz, &div);

	reg = readl(SPI_REG_CCR(priv));
	if (cdr == 0) {
		reg &= ~(CCR_BIT_CDR1_MSK | CCR_BIT_DRS);
		reg |= (div << CCR_BIT_CDR1_OFF);
	} else {
		reg &= ~CCR_BIT_CDR2_MSK;
		reg  |= (div | CCR_BIT_DRS);
	}
	priv->freq = speed;
	writel(reg, SPI_REG_CCR(priv));
#endif

	return 0;
}

static void aic_spi_config_bus_mode(struct aic_spi_priv *priv, u32 busmode)
{
	u32 val;

	val = readl(SPI_REG_BCC(priv));

	if (busmode == SPI_QUAD_MODE)
		val |= BCC_BIT_QUAD_MODE;
	else
		val &= ~BCC_BIT_QUAD_MODE;

	if (busmode == SPI_DUAL_MODE)
		val |= BCC_BIT_DUAL_MODE;
	else
		val &= ~BCC_BIT_DUAL_MODE;

	writel(val, SPI_REG_BCC(priv));
}

static int aic_spi_set_mode(struct udevice *dev, uint mode)
{
	struct aic_spi_priv *priv = dev_get_priv(dev);
	u32 reg, val, busmode;

	dev_info(dev, "mode = 0x%x\n", mode);
	reg = readl(SPI_REG_TCR(priv));
	reg &= ~(TCR_BIT_CPOL | TCR_BIT_CPHA);

	if (mode & SPI_CPOL)
		reg |= TCR_BIT_CPOL;

	if (mode & SPI_CPHA)
		reg |= TCR_BIT_CPHA;

	if (mode & SPI_CS_HIGH)
		reg &= ~TCR_BIT_SPOL;
	else
		reg |= TCR_BIT_SPOL;
	/*
	 * Should set this bit to discard unused RX burst, otherwise unused
	 * data will full fill RXFIFO when sending data.
	 */
	reg |= TCR_BIT_DHB;
	writel(reg, SPI_REG_TCR(priv));

	val = (mode & (SPI_TX_DUAL | SPI_RX_DUAL));
	if ((val > 0) && val != (SPI_TX_DUAL | SPI_RX_DUAL)) {
		dev_warn(dev, "TX RX bus width should be the same.\n");
		mode &= ~(SPI_TX_DUAL | SPI_RX_DUAL);
	}

	val = (mode & (SPI_TX_QUAD | SPI_RX_QUAD));
	if ((val > 0) && val != (SPI_TX_QUAD | SPI_RX_QUAD)) {
		dev_warn(dev, "TX RX bus width should be the same.\n");
		mode &= ~(SPI_TX_QUAD | SPI_RX_QUAD);
	}

	priv->mode = mode;

	if (mode & SPI_RX_QUAD)
		busmode = SPI_QUAD_MODE;
	else if (mode & SPI_RX_DUAL)
		busmode = SPI_DUAL_MODE;
	else
		busmode = SPI_SINGLE_MODE;

	aic_spi_config_bus_mode(priv, busmode);

	return 0;
}

static int aic_spi_check_buswidth(struct spi_slave *slave, u8 buswidth, bool tx)
{
	u32 mode = slave->mode;

	switch (buswidth) {
	case 1:
		return 0;

	case 2:
		if ((tx && (mode & (SPI_TX_DUAL | SPI_TX_QUAD))) ||
		    (!tx && (mode & (SPI_RX_DUAL | SPI_RX_QUAD))))
			return 0;

		break;

	case 4:
		if ((tx && (mode & SPI_TX_QUAD)) ||
		    (!tx && (mode & SPI_RX_QUAD)))
			return 0;

		break;

	default:
		break;
	}

	return -ENOTSUPP;
}

static int aic_spi_check_opcode(u8 opcode)
{
	int ret = 0;

	/*
	 * ArtInChip SPI controller not support DUAL IO, QUAD IO
	 */
	switch (opcode) {
	case SPINAND_PAGE_READ_FROM_CACHE_DUALIO_OPCODE:
	case SPINAND_PAGE_READ_FROM_CACHE_QUADIO_OPCODE:
		ret = -ENOTSUPP;
		break;
	}
	return ret;
}

static bool aic_spi_mem_supports_op(struct spi_slave *slave,
				    const struct spi_mem_op *op)
{
	if (aic_spi_check_opcode(op->cmd.opcode))
		return false;

	if (aic_spi_check_buswidth(slave, op->cmd.buswidth, true))
		return false;

	if (op->addr.nbytes &&
	    aic_spi_check_buswidth(slave, op->addr.buswidth, true))
		return false;

	if (op->dummy.nbytes &&
	    aic_spi_check_buswidth(slave, op->dummy.buswidth, true))
		return false;

	if (op->data.nbytes &&
	    aic_spi_check_buswidth(slave, op->data.buswidth,
				   op->data.dir == SPI_MEM_DATA_OUT))
		return false;

	return true;
}

static int aic_spi_mem_exec_op(struct spi_slave *slave,
			       const struct spi_mem_op *op)
{
	struct udevice *dev = slave->dev->parent;
	struct aic_spi_priv *priv = dev_get_priv(dev);
	u32 flag, busmode, pos = 0;
	const u8 *tx_buf = NULL;
	int ret, i, op_len;
	u8 *rx_buf = NULL;

	if (op->data.nbytes) {
		if (op->data.dir == SPI_MEM_DATA_IN)
			rx_buf = op->data.buf.in;
		else
			tx_buf = op->data.buf.out;
	}

	op_len = op->cmd.nbytes + op->addr.nbytes + op->dummy.nbytes;

	/*
	 * Avoid using malloc() here so that we can use this code in SPL where
	 * simple malloc may be used. That implementation does not allow free()
	 * so repeated calls to this code can exhaust the space.
	 *
	 * The value of op_len is small, since it does not include the actual
	 * data being sent, only the op-code and address. In fact, it should be
	 * possible to just use a small fixed value here instead of op_len.
	 */
	u8 op_buf[op_len];

	op_buf[pos++] = op->cmd.opcode;

	if (op->addr.nbytes) {
		for (i = 0; i < op->addr.nbytes; i++)
			op_buf[pos + i] =
				op->addr.val >> (8 * (op->addr.nbytes - i - 1));

		pos += op->addr.nbytes;
	}

	if (op->dummy.nbytes)
		memset(op_buf + pos, 0xff, op->dummy.nbytes);

	/* 1st transfer: opcode + address + dummy cycles */
	flag = SPI_XFER_BEGIN;
	/* Make sure to set END bit if no tx or rx data messages follow */
	if (!tx_buf && !rx_buf)
		flag |= SPI_XFER_END;

	/* Command phase, should run in single mode */
	aic_spi_config_bus_mode(priv, SPI_SINGLE_MODE);
	ret = aic_spi_xfer(slave->dev, op_len * 8, op_buf, NULL, flag);
	if (ret)
		return ret;

	/* Data phase, check bus width */
	if (op->data.buswidth == 4)
		busmode = SPI_QUAD_MODE;
	else if (op->data.buswidth == 2)
		busmode = SPI_DUAL_MODE;
	else
		busmode = SPI_SINGLE_MODE;

	aic_spi_config_bus_mode(priv, busmode);

	/* 2nd transfer: rx or tx data path */
	if (tx_buf || rx_buf)
		ret = aic_spi_xfer(slave->dev, op->data.nbytes * 8, tx_buf,
				   rx_buf, SPI_XFER_END);

	return ret;
}

#if CONFIG_IS_ENABLED(OF_PLATDATA)
static void aic_spi_get_platdata(struct udevice *dev)
{
	struct aic_spi_platdata *plat = dev_get_plat(dev);
	struct dtd_artinchip_aic_spi *dtplat = &plat->dtplat;
	struct aic_spi_priv *priv = dev_get_priv(dev);

	plat->base = map_sysmem(dtplat->reg[0], dtplat->reg[1]);
	plat->max_hz = AIC_SPI_MAX_RATE;
}
#endif

static int aic_spi_probe(struct udevice *bus)
{
	struct aic_spi_platdata *plat = dev_get_plat(bus);
	struct aic_spi_priv *priv = dev_get_priv(bus);
	int ret = 0;

	dev_info(bus, "%s\n", __func__);
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	aic_spi_get_platdata(bus);
#elif CONFIG_IS_ENABLED(OF_CONTROL) && defined(CONFIG_ARTINCHIP_DMA)
	/* get dma channels */
	ret = dma_get_by_name(bus, "tx", &priv->tx_dma);
	if (ret) {
		dev_err(bus, "get dma tx failed.\n");
		return -EINVAL;
	}

	ret = dma_get_by_name(bus, "rx", &priv->rx_dma);
	if (ret) {
		dev_err(bus, "get dma tx failed.\n");
		return -EINVAL;
	}
#endif

#if !defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_CLK_ARTINCHIP)
	ret = clk_get_by_index(bus, 0, &priv->clk);
	if (ret < 0) {
		dev_err(bus, "failed to get clock\n");
		return ret;
	}

	ret = reset_get_by_index(bus, 0, &priv->reset);
	if (ret && ret != -ENOENT) {
		dev_err(bus, "failed to get reset\n");
		return ret;
	}
#endif
	priv->spi_data = plat->spi_data;
	priv->base = plat->base;
	priv->freq = plat->max_hz;

	ret = aic_spi_set_clock(bus, true);
	if (ret) {
		dev_err(bus, "failed to set clock\n");
		return ret;
	}
	/* Set max DMA delay cycle */
	writel(readl(SPI_REG_DCR(priv)) | 0x1F, SPI_REG_DCR(priv));
	/* enable interrupt */
	setbits_le32(SPI_REG_ICR(priv), 0x3fff);

	dev_info(bus, "%s done.\n", __func__);
	return ret;
}

static int aic_spi_ofdata_to_platdata(struct udevice *bus)
{
	struct aic_spi_platdata *plat = dev_get_plat(bus);
#if !CONFIG_IS_ENABLED(OF_PLATDATA)
	int node = dev_of_offset(bus);

	plat->base = (void *)devfdt_get_addr(bus);
	plat->max_hz = fdtdec_get_int(gd->fdt_blob, node, "spi-max-frequency",
				      AIC_SPI_DEFAULT_RATE);
	if (plat->max_hz > AIC_SPI_MAX_RATE)
		plat->max_hz = AIC_SPI_MAX_RATE;
#endif
	plat->spi_data = (struct aic_spi_data *)dev_get_driver_data(bus);

	return 0;
}

static const struct aic_spi_data aic_spi_data = {
	.fifo_depth     = 64,
	.has_soft_reset = true,
	.has_burst_ctl  = true,
};

static const struct spi_controller_mem_ops aic_spi_mem_ops = {
	.supports_op = aic_spi_mem_supports_op,
	.exec_op = aic_spi_mem_exec_op,
};

static const struct dm_spi_ops aic_spi_ops = {
	.claim_bus   = aic_spi_claim_bus,
	.release_bus = aic_spi_release_bus,
	.xfer        = aic_spi_xfer,
	.set_speed   = aic_spi_set_speed,
	.set_mode    = aic_spi_set_mode,
	.mem_ops     = &aic_spi_mem_ops,
};

static const struct udevice_id aic_spi_ids[] = {
	{
		.compatible = "artinchip,aic-spi-v1.0",
		.data       = (ulong)&aic_spi_data,
	},
	{}
};

U_BOOT_DRIVER(aic_spi) = {
	.name                     = "artinchip_aic_spi",
	.id                       = UCLASS_SPI,
	.of_match                 = aic_spi_ids,
	.ops                      = &aic_spi_ops,
	.of_to_plat               = aic_spi_ofdata_to_platdata,
	.plat_auto                = sizeof(struct aic_spi_platdata),
	.priv_auto                = sizeof(struct aic_spi_priv),
	.probe                    = aic_spi_probe,
};
