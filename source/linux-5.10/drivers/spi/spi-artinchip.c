// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021-2025, Artinchip Technology Co., Ltd
 * Author: Dehuang Wu <dehuang.wu@artinchip.com>
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/bits.h>
#include <linux/clk.h>
#include <linux/pinctrl/consumer.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <linux/reset.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_bitbang.h>
#include <asm/cacheflush.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/spi/spi-mem.h>

#include "../dma/artinchip-dma.h"
#include "internals.h"
#include "spi-artinchip.h"

enum spi_mode_type {
	SINGLE_HALF_DUPLEX_RX,
	SINGLE_HALF_DUPLEX_TX,
	SINGLE_FULL_DUPLEX_RX_TX,
	DUAL_HALF_DUPLEX_RX,
	DUAL_HALF_DUPLEX_TX,
	QUAD_HALF_DUPLEX_RX,
	QUAD_HALF_DUPLEX_TX,
	MODE_TYPE_NULL,
};

#define SINGLE_MODE		0x01010101U
#define DUAL_OUTPUT_MODE	0x01010102U
#define DUAL_IO_MODE		0x01020202U
#define QUAD_OUTPUT_MODE	0x01010104U
#define QUAD_IO_MODE		0x01040404U
#define QPI_MODE		0x04040404U

#define SPI_DUAL_MODE		1
#define SPI_QUAD_MODE		2
#define SPI_SINGLE_MODE		4
#define SPI_DMA_RX		BIT(0)
#define SPI_DMA_TX		BIT(1)
#define SPINOR_OP_READ_1_1_2	0x3b	/* Read data bytes (Dual SPI) */
#define SPINOR_OP_READ_1_1_4	0x6b	/* Read data bytes (Quad SPI) */
#define SPINOR_OP_READ4_1_1_2	0x3c	/* Read data bytes (Dual SPI) */
#define SPINOR_OP_READ4_1_1_4	0x6c	/* Read data bytes (Quad SPI) */

#define RX_SAMP_DLY_AUTO        0
#define RX_SAMP_DLY_NONE        1
#define RX_SAMP_DLY_HALF_CYCLE  2
#define RX_SAMP_DLY_ONE_CYCLE   3

/* The port only used for request DDMA, and maybe different in different SoC */
#define DMA_SPI0		10

struct aic_spi {
	struct device *dev;
	struct spi_controller *ctlr;
	void __iomem *base_addr;
	struct clk *mclk;
	struct reset_control *rst;
	struct dma_chan *dma_rx;
	struct dma_chan *dma_tx;
	dma_addr_t dma_addr_rx;
	dma_addr_t dma_addr_tx;
	enum spi_mode_type mode_type;
	unsigned int irq;
	u32 id;
	char dev_name[48];
	spinlock_t lock;
	bool bit_mode;
	u32 rx_samp_dly;
};

struct aic_spi_platform_data {
	int cs_num;
	char regulator_id[16];
	struct regulator *regulator;
};

static ssize_t aic_spi_status_show(struct device *dev,
				   struct device_attribute *attr, char *buf);

static s32 spi_ctlr_set_cs_num(u32 chipselect, void __iomem *base_addr)
{
	u32 val;

	val = readl(base_addr + SPI_REG_TCR);
	val &= ~TCR_BIT_SS_SEL_MSK;
	val |= (chipselect << TCR_BIT_SS_SEL_OFF);
	writel(val, base_addr + SPI_REG_TCR);

	return 0;
}

static inline void spi_cltr_cfg_tc(u32 mode, void __iomem *base_addr)
{
	u32 val = readl(base_addr + SPI_REG_TCR);

	if (mode & SPI_CPOL)
		val |= TCR_BIT_CPOL;
	else
		val &= ~TCR_BIT_CPOL;

	if (mode & SPI_CPHA)
		val |= TCR_BIT_CPHA;
	else
		val &= ~TCR_BIT_CPHA;

	if (mode & SPI_CS_HIGH)
		val &= ~TCR_BIT_SPOL;
	else
		val |= TCR_BIT_SPOL;

	if (mode & SPI_LSB_FIRST)
		val |= TCR_BIT_FBS;
	else
		val &= ~TCR_BIT_FBS;

	if (mode & SPI_DUMMY_FILL_ONE)
		val |= TCR_BIT_DDB;
	else
		val &= ~TCR_BIT_DDB;

	if (mode & SPI_RECEIVE_ALL_BURST)
		val &= ~TCR_BIT_DHB;
	else
		val |= TCR_BIT_DHB;

#ifndef SUPPORT_SPI_3WIRE_IN_BIT_MODE
	if (mode & SPI_3WIRE) {
		val |= TCR_BIT_3WIRE;
		val |= 0xFF << 16;
	} else
		val &= ~TCR_BIT_3WIRE;
#endif

	val &= ~TCR_BIT_SSCTL;

	writel(val, base_addr + SPI_REG_TCR);
}

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

	/* cdr1 param vs cdr2 param, use the best */
	cdr1_clk = mclk >> cdr1;
	cdr2_clk = mclk / (2 * cdr2 + 1);

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

static void spi_ctlr_set_work_mode(void __iomem *base_addr, int mode)
{
	u32 val;

	val = readl(base_addr + SPI_REG_BTC);
	val &= ~(BTC_BIT_WM_MSK);
	val |= (mode & BTC_BIT_WM_MSK);
	writel(val, base_addr + SPI_REG_BTC);
}

static s32 spi_ctlr_bit_mode_set_cs_num(u32 chipselect, void __iomem *base_addr)
{
	u32 val;

	if (chipselect != 0)
		return -EINVAL;

	val = readl(base_addr + SPI_REG_BTC);
	val &= ~BTC_BIT_SS_SEL_MSK;
	val |= (chipselect << BTC_BIT_SS_SEL_OFS);
	writel(val, base_addr + SPI_REG_BTC);

	return 0;
}

static inline void spi_ctlr_bit_mode_set_clk(u32 spiclk, u32 mclk,
					     void __iomem *base_addr)
{
	u32 div;

	/*
	 * mclk: module source clock
	 * spiclk: expected spi working clock
	 */

	if (spiclk == 0)
		spiclk = 1;
	div = mclk / (2 * spiclk) - 1;

	writel(div, base_addr + SPI_REG_BCLK);
}

static inline void spi_ctlr_bit_mode_start_xfer(void __iomem *base_addr)
{
	u32 val = readl(base_addr + SPI_REG_BTC);

	val |= BTC_BIT_XFER_EN_MSK;
	writel(val, base_addr + SPI_REG_BTC);
}

static inline void spi_ctlr_bit_mode_set_cs(struct aic_spi *aicspi, u32 high)
{
	u32 reg_val = readl(aicspi->base_addr + SPI_REG_BTC);

	if (high)
		reg_val |= BTC_BIT_SS_LEVEL_MSK;
	else
		reg_val &= ~BTC_BIT_SS_LEVEL_MSK;

	writel(reg_val, aicspi->base_addr + SPI_REG_BTC);
}

static inline void spi_ctlr_bit_mode_set_cs_level(struct aic_spi *aicspi,
						  bool level)
{
	u32 reg_val = readl(aicspi->base_addr + SPI_REG_BTC);

	if (level)
		reg_val |= BTC_BIT_SS_LEVEL_MSK;
	else
		reg_val &= ~BTC_BIT_SS_LEVEL_MSK;

	writel(reg_val, aicspi->base_addr + SPI_REG_BTC);
}

static inline void spi_ctlr_bit_mode_set_cs_pol(struct aic_spi *aicspi,
						bool high_active)
{
	u32 reg_val = readl(aicspi->base_addr + SPI_REG_BTC);

	if (high_active)
		reg_val &= ~BTC_BIT_SPOL_MSK;
	else
		reg_val |= BTC_BIT_SPOL_MSK;

	writel(reg_val, aicspi->base_addr + SPI_REG_BTC);
}

static inline void spi_ctlr_bit_mode_set_ss_owner(void __iomem *base_addr,
						  bool soft_ctrl)
{
	u32 val = readl(base_addr + SPI_REG_BTC);

	if (soft_ctrl)
		val |= BTC_BIT_SS_OWNER_MSK;
	else
		val &= ~BTC_BIT_SS_OWNER_MSK;

	writel(val, base_addr + SPI_REG_BTC);
}

static inline bool spi_ctlr_bit_mode_xfer_done(void __iomem *base_addr)
{
	u32 val;

	val = readl(base_addr + SPI_REG_BTC);
	if (val & BTC_BIT_XFER_EN_MSK)
		return false;
	if ((val & BTC_BIT_XFER_COMP_MSK) == 0)
		return false;
	return true;
}

static inline bool spi_ctlr_bit_mode_rxsts_clear(void __iomem *base_addr)
{
	u32 val;

	val = readl(base_addr + SPI_REG_BTC);
	val &= ~(BTC_BIT_RX_BIT_LEN_MSK);
	val |= BTC_BIT_XFER_COMP_MSK;
	writel(val, base_addr + SPI_REG_BTC);
	udelay(1);
	val = readl(base_addr + SPI_REG_BTC);
	if (val & BTC_BIT_XFER_COMP_MSK)
		return false;
	return true;
}

static inline bool spi_ctlr_bit_mode_txsts_clear(void __iomem *base_addr)
{
	u32 val;

	val = readl(base_addr + SPI_REG_BTC);
	val &= ~(BTC_BIT_TX_BIT_LEN_MSK);
	val |= BTC_BIT_XFER_COMP_MSK;
	writel(val, base_addr + SPI_REG_BTC);
	udelay(1);
	val = readl(base_addr + SPI_REG_BTC);
	if (val & BTC_BIT_XFER_COMP_MSK)
		return false;
	return true;
}

static int spi_ctlr_bit_mode_read(struct aic_spi *aicspi, unsigned char *rx_buf,
				  unsigned int rx_len, unsigned long speed_hz,
				  unsigned char bits_per_word)
{
	int dolen, remain, i;
	u32 val, rxbits;
	unsigned char *p;

	val = readl(aicspi->base_addr + SPI_REG_BTC);
	val |= BTC_BIT_XFER_COMP_MSK;
	writel(val, aicspi->base_addr + SPI_REG_BTC);

	p = rx_buf;
	remain = rx_len;
	while (remain) {
		rxbits = 0;
		dolen = remain;
		if (dolen > 4)
			dolen = 4;

		/* Configre rx length and start transfer */
		val = readl(aicspi->base_addr + SPI_REG_BTC);
		val |= bits_per_word << BTC_BIT_RX_BIT_LEN_OFS;
		val |= BTC_BIT_XFER_EN_MSK;
		writel(val, aicspi->base_addr + SPI_REG_BTC);

		while (!spi_ctlr_bit_mode_xfer_done(aicspi->base_addr))
			;
		while (!spi_ctlr_bit_mode_rxsts_clear(aicspi->base_addr))
			;

		/* Read rx bits */
		rxbits = readl(aicspi->base_addr + SPI_REG_RBR);
		for (i = 0; i < dolen; i++)
			p[i] = (rxbits >> (i * 8)) & 0xFF;
		p += dolen;
		remain -= dolen;
	}

	if (remain == 0)
		spi_finalize_current_transfer(aicspi->ctlr);

	return rx_len;
}

static int spi_ctlr_bit_mode_write(struct aic_spi *aicspi,
				   unsigned char *tx_buf, unsigned int tx_len,
				   unsigned long speed_hz,
				   unsigned char bits_per_word)
{
	int dolen, remain, i;
	u32 val, txbits;
	unsigned char *p;

	val = readl(aicspi->base_addr + SPI_REG_BTC);
	val |= BTC_BIT_XFER_COMP_MSK;
	writel(val, aicspi->base_addr + SPI_REG_BTC);

	p = tx_buf;
	remain = tx_len;
	while (remain) {
		txbits = 0;
		dolen = remain;
		if (dolen > 4)
			dolen = 4;
		/* Prepare and write tx bits */
		for (i = 0; i < dolen; i++)
			txbits |= p[i] << (i * 8);
		writel(txbits, aicspi->base_addr + SPI_REG_TBR);

		/* Configure tx length and start transfer */
		val = readl(aicspi->base_addr + SPI_REG_BTC);
		val |= bits_per_word << BTC_BIT_TX_BIT_LEN_OFS;
		val |= BTC_BIT_XFER_EN_MSK;
		writel(val, aicspi->base_addr + SPI_REG_BTC);

		while (!spi_ctlr_bit_mode_xfer_done(aicspi->base_addr))
			;
		while (!spi_ctlr_bit_mode_txsts_clear(aicspi->base_addr))
			;
		p += dolen;
		remain -= dolen;
	}

	if (remain == 0)
		spi_finalize_current_transfer(aicspi->ctlr);

	return tx_len;
}

static int aic_spi_bit_mode_transfer_one(struct spi_controller *ctlr,
					 struct spi_device *spi,
					 struct spi_transfer *t)
{
	struct aic_spi *aicspi = spi_controller_get_devdata(spi->controller);
	void __iomem *base_addr = aicspi->base_addr;
	u8 *tx_buf = (u8 *)t->tx_buf;
	u8 *rx_buf = (u8 *)t->rx_buf;
	u8 bits_per_word = t->bits_per_word;

	dev_dbg(aicspi->dev,
		"spi-%d: begin bit transfer, txbuf 0x%lx, rxbuf 0x%lx, len %d\n",
		spi->controller->bus_num, (unsigned long)tx_buf,
		(unsigned long)rx_buf, t->len);

	if ((!t->tx_buf && !t->rx_buf) || !t->len) {
		dev_err(aicspi->dev, "invalid bit mode parameters\n");
		return -EINVAL;
	}

	spi_ctlr_bit_mode_set_clk(t->speed_hz, clk_get_rate(aicspi->mclk),
				  base_addr);

	if (t->tx_buf)
		spi_ctlr_bit_mode_write(aicspi, (unsigned char *)t->tx_buf,
					t->len, t->speed_hz, bits_per_word);
	if (t->rx_buf)
		spi_ctlr_bit_mode_read(aicspi, t->rx_buf, t->len, t->speed_hz,
				       bits_per_word);

	return 1; /* In progress */
}


static inline void spi_ctlr_set_clk(u32 spiclk, u32 mclk, void __iomem *base_addr)
{
	u32 val, cdr, div;

	/*
	 * mclk: module source clock
	 * spiclk: expected spi working clock
	 *
	 * Need to calculate divider parameter to generate spiclk in spi
	 * controller
	 */
	cdr = spi_get_best_div_param(spiclk, mclk, &div);

	val = readl(base_addr + SPI_REG_CCR);
	if (cdr == 0) {
		val &= ~(CCR_BIT_CDR1_MSK | CCR_BIT_DRS);
		val |= (div << CCR_BIT_CDR1_OFF);
	} else {
		val &= ~CCR_BIT_CDR2_MSK;
		val |= (div | CCR_BIT_DRS);
	}

	writel(val, base_addr + SPI_REG_CCR);
}

static inline void spi_ctlr_start_xfer(void __iomem *base_addr)
{
	u32 val = readl(base_addr + SPI_REG_TCR);

	val |= TCR_BIT_XCH;
	writel(val, base_addr + SPI_REG_TCR);
}

static inline void spi_ctlr_enable_bus(void __iomem *base_addr)
{
	u32 val = readl(base_addr + SPI_REG_GCR);

	val |= GCR_BIT_EN;
	writel(val, base_addr + SPI_REG_GCR);
}

static inline void spi_ctlr_disable_bus(void __iomem *base_addr)
{
	u32 val = readl(base_addr + SPI_REG_GCR);

	val &= ~GCR_BIT_EN;
	writel(val, base_addr + SPI_REG_GCR);
}

static inline void spi_ctlr_set_master_mode(void __iomem *base_addr)
{
	u32 val = readl(base_addr + SPI_REG_GCR);

	val |= GCR_BIT_MODE;
	writel(val, base_addr + SPI_REG_GCR);
}

static inline void spi_ctlr_enable_tp(void __iomem *base_addr)
{
	u32 val = readl(base_addr + SPI_REG_GCR);

	val |= GCR_BIT_PAUSE;
	writel(val, base_addr + SPI_REG_GCR);
}

static inline void spi_ctlr_set_cs_level(struct aic_spi *aicspi, bool level)
{
	u32 reg_val = readl(aicspi->base_addr + SPI_REG_TCR);

	if (level)
		reg_val |= TCR_BIT_SS_LEVEL;
	else
		reg_val &= ~TCR_BIT_SS_LEVEL;

	writel(reg_val, aicspi->base_addr + SPI_REG_TCR);
}

static inline void spi_ctlr_set_cs_enable(struct aic_spi *aicspi, bool enable)
{
	u32 reg_val = readl(aicspi->base_addr + SPI_REG_TCR);
	bool high_active, level;

	high_active = !(reg_val & TCR_BIT_SPOL);
	if (high_active)
		level = enable;
	else
		level = !enable;

	if (level)
		reg_val |= TCR_BIT_SS_LEVEL;
	else
		reg_val &= ~TCR_BIT_SS_LEVEL;

	writel(reg_val, aicspi->base_addr + SPI_REG_TCR);
}

static inline void spi_ctlr_set_cs_pol(struct aic_spi *aicspi, bool high_active)
{
	u32 reg_val = readl(aicspi->base_addr + SPI_REG_TCR);

	if (high_active)
		reg_val &= ~TCR_BIT_SPOL;
	else
		reg_val |= TCR_BIT_SPOL;

	writel(reg_val, aicspi->base_addr + SPI_REG_TCR);
}

static inline void spi_ctlr_soft_reset(void __iomem *base_addr)
{
	u32 val = readl(base_addr + SPI_REG_GCR);

	val |= GCR_BIT_SRST;
	writel(val, base_addr + SPI_REG_GCR);
}

static inline void spi_ctlr_irq_enable(u32 bitmap, void __iomem *base_addr)
{
	u32 val = readl(base_addr + SPI_REG_ICR);

	bitmap &= ICR_BIT_ALL_MSK;
	val |= bitmap;
	writel(val, base_addr + SPI_REG_ICR);
}

static inline void spi_ctlr_irq_disable(u32 bitmap, void __iomem *base_addr)
{
	u32 val = readl(base_addr + SPI_REG_ICR);

	bitmap &= ICR_BIT_ALL_MSK;
	val &= ~bitmap;
	writel(val, base_addr + SPI_REG_ICR);
}

static inline void spi_ctlr_dma_tx_enable(void __iomem *base_addr)
{
	u32 val = readl(base_addr + SPI_REG_FCR);

	val |= FCR_BIT_TX_DMA_EN;
	writel(val, base_addr + SPI_REG_FCR);
}
static inline void spi_ctlr_dma_rx_enable(void __iomem *base_addr)
{
	u32 val = readl(base_addr + SPI_REG_FCR);

	val |= FCR_BIT_RX_DMA_EN;
	writel(val, base_addr + SPI_REG_FCR);
}

static inline void spi_ctlr_dma_irq_disable(u32 bitmap, void __iomem *base_addr)
{
	u32 val = readl(base_addr + SPI_REG_FCR);

	bitmap &= FCR_BIT_DMA_EN_MSK;
	val &= ~bitmap;
	writel(val, base_addr + SPI_REG_FCR);
}

static inline u32 spi_ctlr_pending_irq_query(void __iomem *base_addr)
{
	return (ISR_BIT_ALL_MSK & readl(base_addr + SPI_REG_ISR));
}

static inline void spi_ctlr_pending_irq_clr(u32 pending_bit,
					    void __iomem *base_addr)
{
	pending_bit &= ISR_BIT_ALL_MSK;
	writel(pending_bit, base_addr + SPI_REG_ISR);
}

static inline u32 spi_ctlr_txfifo_query(void __iomem *base_addr)
{
	u32 val = (FSR_BIT_TX_CNT_MSK & readl(base_addr + SPI_REG_FSR));

	val >>= FSR_BIT_TXCNT_OFF;
	return val;
}

static inline u32 spi_ctlr_rxfifo_query(void __iomem *base_addr)
{
	u32 val = (FSR_BIT_RX_CNT_MSK & readl(base_addr + SPI_REG_FSR));

	val >>= FSR_BIT_RXCNT_OFF;
	return val;
}

static inline void spi_ctlr_reset_fifo(void __iomem *base_addr)
{
	u32 val = readl(base_addr + SPI_REG_FCR);

	val |= (FCR_BIT_RX_RST | FCR_BIT_TX_RST);

	/* Set the trigger level of RxFIFO/TxFIFO. */
	val &= ~(FCR_BIT_RX_LEVEL_MSK | FCR_BIT_TX_LEVEL_MSK);
	val |= (0x10 << 16) | 0x30;
	writel(val, base_addr + SPI_REG_FCR);
}

static inline void spi_ctlr_set_xfer_cnt(void __iomem *base_addr, u32 tx_len,
				  u32 rx_len, u32 single_len, u32 dummy_cnt)
{
	u32 val = readl(base_addr + SPI_REG_BCR);

	/* Total transfer length */
	val &= ~BCR_BIT_CNT_MSK;
	val |= (BCR_BIT_CNT_MSK & (tx_len + rx_len + dummy_cnt));
	writel(val, base_addr + SPI_REG_BCR);

	/* Total send data length */
	val = readl(base_addr + SPI_REG_MTC);
	val &= ~MTC_BIT_CNT_MSK;
	val |= (MTC_BIT_CNT_MSK & tx_len);
	writel(val, base_addr + SPI_REG_MTC);

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
	val = readl(base_addr + SPI_REG_BCC);
	val &= ~BCC_BIT_STC_MSK;
	val |= (BCC_BIT_STC_MSK & single_len);
	val &= ~(0xf << 24);
	val |= (dummy_cnt << 24);
	writel(val, base_addr + SPI_REG_BCC);
}

static inline void spi_ctlr_set_ss_owner(void __iomem *base_addr,
					 bool soft_ctrl)
{
	u32 val = readl(base_addr + SPI_REG_TCR);

	if (soft_ctrl)
		val |= TCR_BIT_SS_OWNER;
	else
		val &= ~TCR_BIT_SS_OWNER;

	writel(val, base_addr + SPI_REG_TCR);
}

static inline void spi_ctlr_set_receive_all_burst(void __iomem *base_addr)
{
	u32 val = readl(base_addr + SPI_REG_TCR);

	val &= ~TCR_BIT_DHB;
	writel(val, base_addr + SPI_REG_TCR);
}

static inline void spi_ctlr_dual_disable(void __iomem *base_addr)
{
	u32 val = readl(base_addr + SPI_REG_BCC);

	val &= ~BCC_BIT_DUAL_MODE;
	writel(val, base_addr + SPI_REG_BCC);
}

static inline void spi_ctlr_dual_enable(void __iomem *base_addr)
{
	u32 val = readl(base_addr + SPI_REG_BCC);

	val |= BCC_BIT_DUAL_MODE;
	writel(val, base_addr + SPI_REG_BCC);
}

static inline void spi_ctlr_quad_disable(void __iomem *base_addr)
{
	u32 val = readl(base_addr + SPI_REG_BCC);

	val &= ~BCC_BIT_QUAD_MODE;
	writel(val, base_addr + SPI_REG_BCC);
}

static inline void spi_ctlr_set_tx_delay_mode(void __iomem *base_addr, bool en)
{
	u32 val = readl(base_addr + SPI_REG_TCR);

	if (en)
		val |= (TCR_BIT_SDDM);
	else
		val &= ~(TCR_BIT_SDDM);
	writel(val, base_addr + SPI_REG_TCR);
}

static inline void spi_ctlr_set_rx_delay_mode(void __iomem *base_addr, int mode)
{
	u32 val = readl(base_addr + SPI_REG_TCR);

	val &= ~TCR_RX_SAMPDLY_MSK;
	if (mode == RX_SAMP_DLY_NONE) {
		val |= TCR_RX_SAMPDLY_NONE;
	} else if (mode == RX_SAMP_DLY_HALF_CYCLE) {
		val |= TCR_RX_SAMPDLY_HALF;
	} else {
		val |= TCR_RX_SAMPDLY_ONE;
	}
	writel(val, base_addr + SPI_REG_TCR);
}

static inline void spi_ctlr_quad_enable(void __iomem  *base_addr)
{
	u32 val = readl(base_addr + SPI_REG_BCC);

	val |= BCC_BIT_QUAD_MODE;
	writel(val, base_addr + SPI_REG_BCC);
}

static int spi_regulator_request(struct aic_spi_platform_data *pdata)
{
	struct regulator *regu = NULL;

	if (pdata->regulator != NULL)
		return 0;

	/* Consider "n*" as nocare. Support "none", "nocare", "null", "" etc. */
	if ((pdata->regulator_id[0] == 'n') || (pdata->regulator_id[0] == 0))
		return 0;

	regu = regulator_get(NULL, pdata->regulator_id);
	if (IS_ERR(regu))
		return -1;
	pdata->regulator = regu;
	return 0;
}

static void spi_regulator_release(struct aic_spi_platform_data *pdata)
{
	if (pdata->regulator == NULL)
		return;

	regulator_put(pdata->regulator);
	pdata->regulator = NULL;
}

static int spi_regulator_enable(struct aic_spi_platform_data *pdata)
{
	if (pdata->regulator == NULL)
		return 0;

	if (regulator_enable(pdata->regulator) != 0)
		return -1;
	return 0;
}

static int spi_regulator_disable(struct aic_spi_platform_data *pdata)
{
	if (pdata->regulator == NULL)
		return 0;

	if (regulator_disable(pdata->regulator) != 0)
		return -1;
	return 0;
}

static void aic_spi_dma_cb_rx(void *data)
{
	struct aic_spi *aicspi = (struct aic_spi *)data;

	spi_ctlr_dma_irq_disable(FCR_BIT_RX_DMA_EN, aicspi->base_addr);
	spi_finalize_current_transfer(aicspi->ctlr);
}

static void aic_spi_dma_cb_tx(void *data)
{
	/* do nothing */
}

static int aic_spi_dma_rx_cfg(struct aic_spi *aicspi, struct scatterlist *sgl,
			      unsigned int sg_len)
{
	struct dma_async_tx_descriptor *dma_desc = NULL;
	struct dma_slave_config dma_conf = {0};

	dma_conf.direction = DMA_DEV_TO_MEM;
	dma_conf.src_addr = aicspi->dma_addr_rx;
	dma_conf.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	dma_conf.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	dma_conf.src_maxburst = 1;
	dma_conf.dst_maxburst = 1;
	dmaengine_slave_config(aicspi->dma_rx, &dma_conf);

	dma_desc = dmaengine_prep_slave_sg(aicspi->dma_rx, sgl, sg_len,
					   dma_conf.direction,
					   DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!dma_desc) {
		dev_err(aicspi->dev, "spi-%d prepare slave sg failed.\n",
			aicspi->ctlr->bus_num);
		return -EINVAL;
	}

	dma_desc->callback = aic_spi_dma_cb_rx;
	dma_desc->callback_param = (void *)aicspi;
#ifdef CONFIG_ARTINCHIP_DDMA
	if (aicspi->id > 0)
#endif
		dmaengine_submit(dma_desc);

	return 0;
}

static int aic_spi_dma_tx_cfg(struct aic_spi *aicspi, struct scatterlist *sgl,
			      unsigned int sg_len)
{
	struct dma_async_tx_descriptor *dma_desc = NULL;
	struct dma_slave_config dma_conf = {0};

	dma_conf.direction = DMA_MEM_TO_DEV;
	dma_conf.dst_addr = aicspi->dma_addr_tx;
	dma_conf.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	dma_conf.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	dma_conf.src_maxburst = 1;
	dma_conf.dst_maxburst = 1;
	dmaengine_slave_config(aicspi->dma_tx, &dma_conf);

	dma_desc = dmaengine_prep_slave_sg(aicspi->dma_tx, sgl, sg_len,
					   dma_conf.direction,
					   DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!dma_desc) {
		dev_err(aicspi->dev, "spi-%d prepare slave sg failed.\n",
			aicspi->ctlr->bus_num);
		return -EINVAL;
	}

	dma_desc->callback = aic_spi_dma_cb_tx;
	dma_desc->callback_param = (void *)aicspi;
#ifdef CONFIG_ARTINCHIP_DDMA
	if (aicspi->id > 0)
#endif
		dmaengine_submit(dma_desc);
	return 0;
}

static int spi_ctlr_cfg_single(struct aic_spi *aicspi, struct spi_transfer *t)
{
	unsigned long flags = 0;

	pr_debug("%s, xfer len %d\n", __func__, t->len);
	spin_lock_irqsave(&aicspi->lock, flags);
	spi_ctlr_dual_disable(aicspi->base_addr);
	spi_ctlr_quad_disable(aicspi->base_addr);
	if (t->tx_buf && t->rx_buf) {
		/* full duplex */
		spi_ctlr_set_receive_all_burst(aicspi->base_addr);
		spi_ctlr_set_xfer_cnt(aicspi->base_addr, t->len, 0, t->len, 0);
		aicspi->mode_type = SINGLE_FULL_DUPLEX_RX_TX;
	} else if (t->tx_buf) {
		/* half duplex transmit(single mode) */
		spi_ctlr_set_xfer_cnt(aicspi->base_addr, t->len, 0, t->len, 0);
		aicspi->mode_type = SINGLE_HALF_DUPLEX_TX;
	} else if (t->rx_buf) {
		/* half duplex receive(single mode) */
		spi_ctlr_set_xfer_cnt(aicspi->base_addr, 0, t->len, 0, 0);
		aicspi->mode_type = SINGLE_HALF_DUPLEX_RX;
	}
	spin_unlock_irqrestore(&aicspi->lock, flags);

	return 0;
}

static int spi_ctlr_cfg_dual(struct aic_spi *aicspi, struct spi_transfer *t)
{
	unsigned long flags = 0;

	pr_debug("%s, xfer len %d\n", __func__, t->len);
	spin_lock_irqsave(&aicspi->lock, flags);
	spi_ctlr_dual_enable(aicspi->base_addr);
	if (t->tx_buf) {
		spi_ctlr_set_xfer_cnt(aicspi->base_addr, t->len, 0, 0, 0);
		aicspi->mode_type = DUAL_HALF_DUPLEX_TX;
	} else if (t->rx_buf) {
		spi_ctlr_set_xfer_cnt(aicspi->base_addr, 0, t->len, 0, 0);
		aicspi->mode_type = DUAL_HALF_DUPLEX_RX;
	}
	spin_unlock_irqrestore(&aicspi->lock, flags);

	return 0;
}

static int spi_ctlr_cfg_quad(struct aic_spi *aicspi, struct spi_transfer *t)
{
	unsigned long flags = 0;

	pr_debug("%s, xfer len %d\n", __func__, t->len);
	spin_lock_irqsave(&aicspi->lock, flags);
	spi_ctlr_quad_enable(aicspi->base_addr);
	if (t->tx_buf) {
		spi_ctlr_set_xfer_cnt(aicspi->base_addr, t->len, 0, 0, 0);
		aicspi->mode_type = QUAD_HALF_DUPLEX_TX;
	} else if (t->rx_buf) {
		spi_ctlr_set_xfer_cnt(aicspi->base_addr, 0, t->len, 0, 0);
		aicspi->mode_type = QUAD_HALF_DUPLEX_RX;
	}
	spin_unlock_irqrestore(&aicspi->lock, flags);

	return 0;
}

static void spi_ctlr_set_rx_delay(struct aic_spi *aicspi, u32 speed_hz)
{
	void __iomem *base_addr = aicspi->base_addr;
	int rxdlymode = RX_SAMP_DLY_NONE;

	if (aicspi->rx_samp_dly == RX_SAMP_DLY_AUTO) {
		if (speed_hz <= 24000000)
			rxdlymode = RX_SAMP_DLY_NONE;
		else if (speed_hz <= 60000000)
			rxdlymode = RX_SAMP_DLY_HALF_CYCLE;
		else
			rxdlymode = RX_SAMP_DLY_ONE_CYCLE;
	} else if (aicspi->rx_samp_dly == RX_SAMP_DLY_NONE) {
		rxdlymode = RX_SAMP_DLY_NONE;
	} else if (aicspi->rx_samp_dly == RX_SAMP_DLY_HALF_CYCLE) {
		rxdlymode = RX_SAMP_DLY_HALF_CYCLE;
	} else {
		rxdlymode = RX_SAMP_DLY_ONE_CYCLE;
	}
	spi_ctlr_set_rx_delay_mode(base_addr, rxdlymode);
}

static int spi_ctlr_cfg_xfer(struct aic_spi *aicspi, struct spi_device *spi,
			     struct spi_transfer *t)
{
	void __iomem *base_addr = aicspi->base_addr;

	if (aicspi->mode_type != MODE_TYPE_NULL)
		return -EINVAL;

	/* DUPLEX_TX_RX mode, use TX setting */
	if (t->tx_buf) {
		if ((spi->mode & SPI_TX_DUAL) && (t->tx_nbits == 2))
			spi_ctlr_cfg_dual(aicspi, t);
		else if ((spi->mode & SPI_TX_QUAD) && (t->tx_nbits == 4))
			spi_ctlr_cfg_quad(aicspi, t);
		else
			spi_ctlr_cfg_single(aicspi, t);
	} else if (t->rx_buf) {
		if ((spi->mode & SPI_RX_DUAL) && (t->rx_nbits == 2))
			spi_ctlr_cfg_dual(aicspi, t);
		else if ((spi->mode & SPI_RX_QUAD) && (t->rx_nbits == 4))
			spi_ctlr_cfg_quad(aicspi, t);
		else
			spi_ctlr_cfg_single(aicspi, t);
	}
	spi_ctlr_set_rx_delay(aicspi, t->speed_hz);
	spi_ctlr_set_clk(t->speed_hz, clk_get_rate(aicspi->mclk), base_addr);
	return 0;
}

static bool spi_check_timeout_one_seconed(unsigned long jiffies0)
{
	unsigned long timeout;

	timeout = jiffies0 + msecs_to_jiffies(1000);

	return time_after(jiffies, timeout);
}

#define check_timeout_1s(a)								\
		do {									\
			if (spi_check_timeout_one_seconed(a)) {				\
				dev_err(aicspi->dev, "%d cpu transfer data time out!\n", __LINE__);	\
				return -1;						\
			}								\
		} while (0)

static int spi_ctlr_fifo_read(struct aic_spi *aicspi, unsigned char *rx_buf,
			      unsigned int rx_len)
{
	void __iomem *base_addr = aicspi->base_addr;
	unsigned long jiffies0;
	unsigned int dolen;

	while (rx_len) {
		jiffies0 = jiffies;
		while (!spi_ctlr_rxfifo_query(base_addr))
			check_timeout_1s(jiffies0);

		dolen = spi_ctlr_rxfifo_query(base_addr);
		while (dolen) {
			*rx_buf++ = readb(base_addr + SPI_REG_RXDATA);
			--dolen;
			--rx_len;
		}
	}

	return 0;
}

static int spi_ctlr_fifo_write(struct aic_spi *aicspi,
			       const unsigned char *tx_buf, unsigned int tx_len)
{
	void __iomem *base_addr = aicspi->base_addr;
	unsigned long flags = 0;
	unsigned long jiffies0;
	unsigned int fifo_len;
	unsigned int dolen;


	spin_lock_irqsave(&aicspi->lock, flags);

	while (tx_len) {
		jiffies0 = jiffies;
		while (spi_ctlr_txfifo_query(base_addr) > (SPI_FIFO_DEPTH / 2))
			check_timeout_1s(jiffies0);

		fifo_len = SPI_FIFO_DEPTH - spi_ctlr_txfifo_query(base_addr);
		dolen = min(fifo_len, tx_len);
		while (dolen) {
			writeb(*tx_buf++, base_addr + SPI_REG_TXDATA);
			tx_len--;
			dolen--;
		}
	}

	spin_unlock_irqrestore(&aicspi->lock, flags);

	jiffies0 = jiffies;
	while (spi_ctlr_txfifo_query(base_addr))
		check_timeout_1s(jiffies0);

	return 0;
}



static int spi_ctlr_fifo_write_read(struct aic_spi *aicspi,
			       const unsigned char *tx_buf, unsigned char *rx_buf, unsigned int len)
{
	void __iomem *base_addr = aicspi->base_addr;
	unsigned int dolen, temp_cnt;
	unsigned long flags = 0;
	unsigned long jiffies0;
	unsigned int free_len;

	spin_lock_irqsave(&aicspi->lock, flags);

	while (len) {
		jiffies0 = jiffies;
		while (spi_ctlr_txfifo_query(base_addr) > (SPI_FIFO_DEPTH / 2))
			check_timeout_1s(jiffies0);

		free_len = SPI_FIFO_DEPTH - spi_ctlr_txfifo_query(base_addr);
		dolen = min(free_len, len);
		temp_cnt = dolen;
		while (temp_cnt) {
			writeb(*tx_buf++, base_addr + SPI_REG_TXDATA);
			temp_cnt--;
		}

		temp_cnt = dolen;
		jiffies0 = jiffies;
		while (spi_ctlr_rxfifo_query(base_addr) != dolen)
			check_timeout_1s(jiffies0);

		while (temp_cnt) {
			*rx_buf++ = readb(base_addr + SPI_REG_RXDATA);
			temp_cnt--;
			len--;
		}
	}

	spin_unlock_irqrestore(&aicspi->lock, flags);
	jiffies0 = jiffies;
	while (spi_ctlr_txfifo_query(base_addr))
		check_timeout_1s(jiffies0);

	return 0;
}

static int aic_spi_dma_rx_start(struct aic_spi *aicspi, struct scatterlist *sgl,
				unsigned int sg_len)
{
	int ret = 0;

	spi_ctlr_dma_rx_enable(aicspi->base_addr);
	ret = aic_spi_dma_rx_cfg(aicspi, sgl, sg_len);
	if (ret < 0)
		return ret;
	dma_async_issue_pending(aicspi->dma_rx);

	return ret;
}

static int aic_spi_dma_tx_start(struct aic_spi *aicspi, struct scatterlist *sgl,
				unsigned int sg_len)
{
	int ret = 0;

	spi_ctlr_dma_tx_enable(aicspi->base_addr);
	ret = aic_spi_dma_tx_cfg(aicspi, sgl, sg_len);
	if (ret < 0)
		return ret;
	dma_async_issue_pending(aicspi->dma_tx);

	return ret;
}

static bool aic_spi_use_dma(struct aic_spi *aicspi, const void *tx_buf,
			    void *rx_buf, unsigned int len)
{
	/* Data length with DMA, should be 4 byte aligned. */
	if ((len <= SPI_FIFO_DEPTH) || (len % 4))
		return false;

	/* Buffers for DMA transactions must be 8 Byte aligned */
	if ((unsigned long)tx_buf % 8 != 0 || (unsigned long)rx_buf % 8 != 0)
		return false;

	if (!aicspi->dma_rx || !aicspi->dma_tx)
		return false;

	return true;
}
static bool aic_spi_can_dma(struct spi_controller *ctlr, struct spi_device *spi,
			    struct spi_transfer *t)
{
	struct aic_spi *aicspi = spi_controller_get_devdata(spi->master);

	return aic_spi_use_dma(aicspi, t->tx_buf, t->rx_buf, t->len);
}

static int aic_spi_data_xfer(struct spi_device *spi, struct spi_transfer *t)
{
	struct aic_spi *aicspi = spi_controller_get_devdata(spi->master);
	void __iomem *base_addr = aicspi->base_addr;
	unsigned int tx_len = t->len;
	unsigned int rx_len = t->len;

	switch (aicspi->mode_type) {
	case SINGLE_HALF_DUPLEX_RX:
	case DUAL_HALF_DUPLEX_RX:
	case QUAD_HALF_DUPLEX_RX:
	{
		if (aic_spi_use_dma(aicspi, t->tx_buf, t->rx_buf, t->len)) {
			dev_dbg(aicspi->dev, "rx -> by dma, len %d\n", rx_len);
			spi_ctlr_irq_disable(ICR_BIT_TC, base_addr);
			aic_spi_dma_rx_start(aicspi, t->rx_sg.sgl, t->rx_sg.nents);
			spi_ctlr_start_xfer(base_addr);
		} else {
			dev_dbg(aicspi->dev, "rx -> by fifo, len %d\n", rx_len);
			spi_ctlr_start_xfer(base_addr);
			spi_ctlr_fifo_read(aicspi, t->rx_buf, t->len);
		}
		break;
	}
	case SINGLE_HALF_DUPLEX_TX:
	case DUAL_HALF_DUPLEX_TX:
	case QUAD_HALF_DUPLEX_TX:
	{
		if (aic_spi_use_dma(aicspi, t->tx_buf, t->rx_buf, t->len)) {
			dev_dbg(aicspi->dev, "tx -> by dma, len %d\n", tx_len);
			spi_ctlr_start_xfer(base_addr);
			aic_spi_dma_tx_start(aicspi, t->tx_sg.sgl, t->tx_sg.nents);
		} else {
			dev_dbg(aicspi->dev, "tx -> by fifo, len %d\n", tx_len);
			spi_ctlr_start_xfer(base_addr);
			spi_ctlr_fifo_write(aicspi, t->tx_buf, t->len);
		}
		break;
	}
	case SINGLE_FULL_DUPLEX_RX_TX:
	{
		if ((rx_len == 0) || (tx_len == 0))
			return -EINVAL;
		if (aic_spi_use_dma(aicspi, t->tx_buf, t->rx_buf, t->len)) {
			dev_dbg(aicspi->dev, "rx and tx -> by dma\n");
			spi_ctlr_irq_disable(ICR_BIT_TC, base_addr);
			aic_spi_dma_tx_start(aicspi, t->tx_sg.sgl, t->tx_sg.nents);
			spi_ctlr_start_xfer(base_addr);
			aic_spi_dma_rx_start(aicspi, t->rx_sg.sgl, t->rx_sg.nents);
		} else {
			dev_dbg(aicspi->dev, "rx and tx -> by fifo\n");
			spi_ctlr_start_xfer(base_addr);
			spi_ctlr_fifo_write_read(aicspi, t->tx_buf, t->rx_buf, t->len);
		}
		break;
	}
	default:
		return -1;
	}

	return 0;
}

static irqreturn_t aic_spi_handle_irq(int irq, void *dev_id)
{
	struct aic_spi *aicspi = (struct aic_spi *)dev_id;
	void __iomem *base_addr = aicspi->base_addr;
	unsigned int status = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&aicspi->lock, flags);
	status = spi_ctlr_pending_irq_query(base_addr);
	spi_ctlr_pending_irq_clr(status, base_addr);
	dev_dbg(aicspi->dev, "spi-%d: irq status = %x\n", aicspi->ctlr->bus_num,
		status);

	/* master mode, Transfer Complete Interrupt */
	if (status & ISR_BIT_TC) {
		dev_dbg(aicspi->dev, "spi-%d: SPI TC comes\n",
			aicspi->ctlr->bus_num);
		spi_ctlr_irq_disable(ISR_BIT_TC | ISR_BIT_ERRS, base_addr);

		/* the callback of tx dma done is slower disable TF_DREQ_EN now */
		spi_ctlr_dma_irq_disable(FCR_BIT_TX_DMA_EN, aicspi->base_addr);

		spi_finalize_current_transfer(aicspi->ctlr);
		spin_unlock_irqrestore(&aicspi->lock, flags);
		return IRQ_HANDLED;
	} else if (status & ISR_BIT_ERRS) {
		dev_err(aicspi->dev, "spi-%d: SPI ERR 0x%x comes\n",
			aicspi->ctlr->bus_num, status);
		spi_ctlr_irq_disable(ISR_BIT_TC | ISR_BIT_ERRS, base_addr);
		spi_ctlr_soft_reset(base_addr);
		spi_finalize_current_transfer(aicspi->ctlr);
		spin_unlock_irqrestore(&aicspi->lock, flags);
		return IRQ_HANDLED;
	}
	dev_dbg(aicspi->dev, "spi-%d: SPI interrupt comes\n",
		aicspi->ctlr->bus_num);
	spin_unlock_irqrestore(&aicspi->lock, flags);

	return IRQ_NONE;
}

static size_t aic_spi_max_transfer_size(struct spi_device *spi)
{
	return AIC_SPI_MAX_XFER_SIZE - 1;
}

static int aic_spi_byte_mode_transfer_one(struct spi_controller *ctlr,
					  struct spi_device *spi,
					  struct spi_transfer *t)
{
	struct aic_spi *aicspi = spi_controller_get_devdata(spi->controller);
	void __iomem *base_addr = aicspi->base_addr;
	u8 *tx_buf = (u8 *)t->tx_buf;
	u8 *rx_buf = (u8 *)t->rx_buf;

	dev_dbg(aicspi->dev,
		"spi-%d: begin transfer, txbuf 0x%lx, rxbuf 0x%lx, len %d\n",
		spi->controller->bus_num, (unsigned long)tx_buf,
		(unsigned long)rx_buf, t->len);

	if ((!t->tx_buf && !t->rx_buf) || !t->len) {
		dev_err(aicspi->dev, "invalid parameters\n");
		return -EINVAL;
	}

	spi_ctlr_pending_irq_clr(ISR_BIT_ALL_MSK, base_addr);
	spi_ctlr_dma_irq_disable(FCR_BIT_DMA_EN_MSK, base_addr);
	spi_ctlr_reset_fifo(base_addr);

	if (spi_ctlr_cfg_xfer(aicspi, spi, t)) {
		dev_err(aicspi->dev, "transfer configuration failed.\n");
		return -EINVAL;
	}

	spi_ctlr_irq_enable(ICR_BIT_TC | ICR_BIT_ERRS, base_addr);

	aic_spi_data_xfer(spi, t);

	if (aicspi->mode_type != MODE_TYPE_NULL)
		aicspi->mode_type = MODE_TYPE_NULL;

	return 1; /* In progress */
}

static int aic_spi_transfer_one(struct spi_controller *ctlr,
				struct spi_device *spi,
				struct spi_transfer *t)
{
	struct aic_spi *aicspi = spi_controller_get_devdata(spi->controller);

	if (aicspi->bit_mode)
		return aic_spi_bit_mode_transfer_one(ctlr, spi, t);
	else
		return aic_spi_byte_mode_transfer_one(ctlr, spi, t);
}

static void aic_spi_set_cs(struct spi_device *spi, bool high)
{
	struct aic_spi *aicspi = spi_controller_get_devdata(spi->controller);

	if (aicspi->bit_mode)
		spi_ctlr_bit_mode_set_cs_level(aicspi, high);
	else
		spi_ctlr_set_cs_level(aicspi, high);
}

static int aic_spi_setup(struct spi_device *spi)
{
	struct aic_spi *aicspi = spi_controller_get_devdata(spi->master);
	unsigned long flags;
	u32 val;

	/* Only support 8 bits per word */
	if (spi->bits_per_word != 8) {
		dev_err(aicspi->dev, "bits_per_word is not supportted.\n");
		return -EINVAL;
	}

	if (spi->max_speed_hz > SPI_MAX_FREQUENCY) {
		dev_err(aicspi->dev, "max_speed_hz is too large %d.\n",
			spi->max_speed_hz);
		return -EINVAL;
	}

	val = (spi->mode & (SPI_TX_DUAL | SPI_RX_DUAL));
	if ((val > 0) && val != (SPI_TX_DUAL | SPI_RX_DUAL)) {
		dev_warn(aicspi->dev, "TX RX bus width should be the same.\n");
		spi->mode &= ~(SPI_TX_DUAL | SPI_RX_DUAL);
	}

	val = (spi->mode & (SPI_TX_QUAD | SPI_RX_QUAD));
	if ((val > 0) && val != (SPI_TX_QUAD | SPI_RX_QUAD)) {
		dev_warn(aicspi->dev, "TX RX bus width should be the same.\n");
		spi->mode &= ~(SPI_TX_QUAD | SPI_RX_QUAD);
	}

#ifdef SUPPORT_SPI_3WIRE_IN_BIT_MODE
	if (spi->mode & SPI_3WIRE)
		aicspi->bit_mode = true;
	else
		aicspi->bit_mode = false;
#endif
	spin_lock_irqsave(&aicspi->lock, flags);

	if (aicspi->bit_mode) {
		spi_ctlr_set_work_mode(aicspi->base_addr, BTC_BIT_WM_BIT_3WIRE);
		/* set chip select number */
		spi_ctlr_bit_mode_set_cs_num(spi->chip_select,
					     aicspi->base_addr);
		if (spi->mode & SPI_CS_HIGH)
			spi_ctlr_bit_mode_set_cs_pol(aicspi, true);
		else
			spi_ctlr_bit_mode_set_cs_pol(aicspi, false);
	} else {
		spi_ctlr_set_work_mode(aicspi->base_addr, BTC_BIT_WM_BYTE);
		spi_cltr_cfg_tc(spi->mode, aicspi->base_addr);
		/* set chip select number */
		spi_ctlr_set_cs_num(spi->chip_select, aicspi->base_addr);
		if (spi->mode & SPI_CS_HIGH)
			spi_ctlr_set_cs_pol(aicspi, true);
		else
			spi_ctlr_set_cs_pol(aicspi, false);
	}
	spin_unlock_irqrestore(&aicspi->lock, flags);

	return 0;
}

static int aic_spi_clk_init(struct aic_spi *aicspi, u32 src_clk)
{
	long rate = 0;

	aicspi->mclk = of_clk_get(aicspi->dev->of_node, 0);
	if (IS_ERR_OR_NULL(aicspi->mclk)) {
		dev_err(aicspi->dev, "spi-%d Unable to acquire module clock\n",
			aicspi->ctlr->bus_num);
		return -1;
	}

	rate = clk_round_rate(aicspi->mclk, src_clk);
	if (clk_set_rate(aicspi->mclk, rate)) {
		dev_err(aicspi->dev, "spi-%d spi clk_set_rate failed\n",
			aicspi->ctlr->bus_num);
		return -1;
	}

	dev_dbg(aicspi->dev, "spi-%d mclk %u\n", aicspi->ctlr->bus_num,
		(unsigned int)clk_get_rate(aicspi->mclk));

	if (clk_prepare_enable(aicspi->mclk)) {
		dev_err(aicspi->dev, "spi-%d enable module clock failed.\n",
			aicspi->ctlr->bus_num);
		return -EBUSY;
	}

	return clk_get_rate(aicspi->mclk);
}

static int aic_spi_clk_exit(struct aic_spi *aicspi)
{
	if (IS_ERR_OR_NULL(aicspi->mclk)) {
		dev_err(aicspi->dev, "spi-%d SPI mclk handle is invalid.\n",
			aicspi->ctlr->bus_num);
		return -1;
	}

	clk_disable_unprepare(aicspi->mclk);
	clk_put(aicspi->mclk);
	aicspi->mclk = NULL;
	return 0;
}

static int aic_spi_hw_init(struct aic_spi *aicspi,
			   struct aic_spi_platform_data *pdata)
{
	void __iomem *base_addr = aicspi->base_addr;
	u32 src_clk = 0;
	int ret;

	if (spi_regulator_request(pdata) < 0) {
		dev_err(aicspi->dev, "spi-%d request regulator failed!\n",
			aicspi->ctlr->bus_num);
		return -1;
	}
	spi_regulator_enable(pdata);

	ret = of_property_read_u32(aicspi->dev->of_node, "spi-max-frequency",
				   &src_clk);
	if (ret) {
		dev_err(aicspi->dev, "spi-%d Get spi-max-frequency failed\n",
			aicspi->ctlr->bus_num);
		return -1;
	}

	src_clk = aic_spi_clk_init(aicspi, src_clk);
	if (src_clk < 0) {
		dev_err(aicspi->dev, "spi-%d aic_spi_clk_init(%s) failed.\n",
			aicspi->ctlr->bus_num, aicspi->dev_name);
		return -1;
	}

	aicspi->rst = devm_reset_control_get_exclusive(aicspi->dev, NULL);
	if (!IS_ERR(aicspi->rst)) {
		reset_control_assert(aicspi->rst);
		udelay(2);
		reset_control_deassert(aicspi->rst);
	}

	spi_ctlr_enable_bus(base_addr);
	spi_ctlr_set_cs_num(0, base_addr);
	spi_ctlr_set_master_mode(base_addr);
	spi_ctlr_set_clk(24000000, src_clk, base_addr);
	spi_cltr_cfg_tc(SPI_MODE_0, base_addr);
	spi_ctlr_set_tx_delay_mode(base_addr, true);
	spi_ctlr_enable_tp(base_addr);
	spi_ctlr_set_ss_owner(base_addr, true);
	spi_ctlr_bit_mode_set_ss_owner(aicspi->base_addr, true);
	spi_ctlr_reset_fifo(base_addr);

	return 0;
}

static int aic_spi_hw_exit(struct aic_spi *aicspi,
			   struct aic_spi_platform_data *pdata)
{
	spi_ctlr_disable_bus(aicspi->base_addr);
	aic_spi_clk_exit(aicspi);
	spi_regulator_disable(pdata);
	spi_regulator_release(pdata);

	return 0;
}

static ssize_t aic_spi_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct aic_spi_platform_data *pdata;
	struct platform_device *pdev;

	pdev = container_of(dev, struct platform_device, dev);
	pdata = dev->platform_data;

	return snprintf(buf, PAGE_SIZE,
			"pdev->id   = %d\n"
			"pdev->name = %s\n"
			"pdev->num_resources = %u\n"
			"pdev->resource.mem = [%pa, %pa]\n"
			"pdev->resource.irq = %pa\n"
			"pdev->dev.platform_data.cs_num    = %d\n"
			"pdev->dev.platform_data.regulator = 0x%p\n"
			"pdev->dev.platform_data.regulator_id = %s\n",
			pdev->id, pdev->name, pdev->num_resources,
			&pdev->resource[0].start, &pdev->resource[0].end,
			&pdev->resource[1].start, pdata->cs_num,
			pdata->regulator, pdata->regulator_id);
}
static struct device_attribute aic_spi_info_attr =
	__ATTR(info, 0444, aic_spi_info_show, NULL);

static ssize_t aic_spi_status_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct spi_controller *ctlr;
	struct aic_spi *aicspi;
	char const *spi_mode[] = { "Single mode, half duplex read",
				   "Single mode, half duplex write",
				   "Single mode, full duplex read and write",
				   "Dual mode, half duplex read",
				   "Dual mode, half duplex write",
				   "Quad mode, half duplex read",
				   "Quad mode, half duplex write",
				   "Null" };

	ctlr = spi_controller_get(platform_get_drvdata(pdev));
	aicspi = spi_controller_get_devdata(ctlr);

	if (ctlr == NULL)
		return snprintf(buf, PAGE_SIZE, "%s\n", "controller is NULL!");

	return snprintf(buf, PAGE_SIZE,
			"ctlr->bus_num = %d\n"
			"ctlr->num_chipselect = %d\n"
			"ctlr->dma_alignment  = %d\n"
			"ctlr->mode_bits = %d\n"
			"ctlr->flags = 0x%x, ->bus_lock_flag = 0x%x\n"
			"ctlr->busy = %d, ->running = %d, ->rt = %d\n"
			"aicspi->mode_type = %d [%s]\n"
			"aicspi->irq = %d [%s]\n"
			"aicspi->base_addr = 0x%lx, the SPI control register:\n"
			"[VER] 0x%02x = 0x%08x, [GCR] 0x%02x = 0x%08x\n"
			"[TCR] 0x%02x = 0x%08x, [ICR] 0x%02x = 0x%08x\n"
			"[ISR] 0x%02x = 0x%08x, [FCR] 0x%02x = 0x%08x\n"
			"[FSR] 0x%02x = 0x%08x, [WCR] 0x%02x = 0x%08x\n"
			"[CCR] 0x%02x = 0x%08x, [BCR] 0x%02x = 0x%08x\n"
			"[MTC] 0x%02x = 0x%08x, [BCC] 0x%02x = 0x%08x\n"
			"[DMA] 0x%02x = 0x%08x\n",
			ctlr->bus_num, ctlr->num_chipselect,
			ctlr->dma_alignment, ctlr->mode_bits, ctlr->flags,
			ctlr->bus_lock_flag, ctlr->busy, ctlr->running,
			ctlr->rt, aicspi->mode_type,
			spi_mode[aicspi->mode_type], aicspi->irq,
			aicspi->dev_name, (unsigned long)aicspi->base_addr,
			SPI_REG_VER, readl(aicspi->base_addr + SPI_REG_VER),
			SPI_REG_GCR, readl(aicspi->base_addr + SPI_REG_GCR),
			SPI_REG_TCR, readl(aicspi->base_addr + SPI_REG_TCR),
			SPI_REG_ICR, readl(aicspi->base_addr + SPI_REG_ICR),
			SPI_REG_ISR, readl(aicspi->base_addr + SPI_REG_ISR),

			SPI_REG_FCR, readl(aicspi->base_addr + SPI_REG_FCR),
			SPI_REG_FSR, readl(aicspi->base_addr + SPI_REG_FSR),
			SPI_REG_WCR, readl(aicspi->base_addr + SPI_REG_WCR),
			SPI_REG_CCR, readl(aicspi->base_addr + SPI_REG_CCR),
			SPI_REG_BCR, readl(aicspi->base_addr + SPI_REG_BCR),

			SPI_REG_MTC, readl(aicspi->base_addr + SPI_REG_MTC),
			SPI_REG_BCC, readl(aicspi->base_addr + SPI_REG_BCC),
			SPI_REG_DCR, readl(aicspi->base_addr + SPI_REG_DCR));
}

static struct device_attribute aic_spi_status_attr =
	__ATTR(status, 0444, aic_spi_status_show, NULL);

static void aic_spi_sysfs(struct platform_device *_pdev)
{
	device_create_file(&_pdev->dev, &aic_spi_info_attr);
	device_create_file(&_pdev->dev, &aic_spi_status_attr);
}

bool aic_spi_mem_supports_op(struct spi_mem *mem, const struct spi_mem_op *op)
{
#ifndef CONFIG_SPI_SUPPORT_QUADIO_ARTINCHIP
	if ((op->addr.buswidth == 4) && (op->dummy.buswidth == 4) &&
	    (op->data.buswidth == 4))
		return false;
#endif
	return spi_mem_default_supports_op(mem, op);
}

/*
 *       Standard SPI   Dual Output   Dual I/O   Quad Output   Quad I/O   QPI
 * cmd   1 bit          1 bit         1 bit      1 bit         1 bit      4 bit
 * addr  1 bit          1 bit         2 bit      1 bit         4 bit      4 bit
 * dummy 1 bit          1 bit         2 bit      1 bit         4 bit      4 bit
 * data  1 bit          2 bit         2 bit      4 bit         4 bit      4 bit
 */
static int aic_spi_mem_exec_op(struct spi_mem *mem, const struct spi_mem_op *op)
{
	u32 head_len, tx_total, tx_dlen, tx_single, rx_dlen, use_dma;
	u8 tx_head[16], *rx_buf;
	void __iomem *base_addr;
	struct aic_spi *aicspi;
	const u8 *tx_buf;
	struct sg_table sg;
	unsigned long mode;
	unsigned long long tmo;
	int ret = 0, i;

	aicspi = spi_controller_get_devdata(mem->spi->controller);
	base_addr = aicspi->base_addr;

	head_len = op->cmd.nbytes + op->addr.nbytes + op->dummy.nbytes;

	tx_buf = NULL;
	tx_dlen = 0;
	rx_buf = NULL;
	rx_dlen = 0;
	if (op->data.dir == SPI_MEM_DATA_IN) {
		rx_buf = op->data.buf.in;
		rx_dlen = op->data.nbytes;
	} else {
		tx_buf = op->data.buf.out;
		tx_dlen = op->data.nbytes;
	}

	tx_total = head_len + tx_dlen;
	use_dma = aic_spi_use_dma(aicspi, tx_buf, rx_buf, op->data.nbytes);

	if (op->cmd.nbytes)
		memcpy(tx_head, &op->cmd.opcode, op->cmd.nbytes);
	if (op->addr.nbytes) {
		for (i = 0; i < op->addr.nbytes; i++)
			tx_head[op->cmd.nbytes + i] = op->addr.val >>
					(8 * (op->addr.nbytes - i - 1));
	}
	if (op->dummy.nbytes)
		memset(tx_head + op->cmd.nbytes + op->addr.nbytes, 0, op->dummy.nbytes);

	if (op->cmd.buswidth == 4)
		mode = QPI_MODE;
	else if (op->addr.buswidth == 4)
		mode = QUAD_IO_MODE;
	else if (op->addr.buswidth == 2)
		mode = DUAL_IO_MODE;
	else if (op->data.buswidth == 4)
		mode = QUAD_OUTPUT_MODE;
	else if (op->data.buswidth == 2)
		mode = DUAL_OUTPUT_MODE;
	else
		mode = SINGLE_MODE;

#ifndef CONFIG_SPI_SUPPORT_QUADIO_ARTINCHIP
	if ((mode == QUAD_IO_MODE) || (mode == QPI_MODE))
		return -ENOTSUPP;
#endif
	switch (mode) {
	case SINGLE_MODE:
		dev_dbg(aicspi->dev, "Single mode\n");
		tx_single = tx_total;
		spi_ctlr_dual_disable(base_addr);
		spi_ctlr_quad_disable(base_addr);
		spi_ctlr_set_xfer_cnt(base_addr, tx_total, rx_dlen, tx_single, 0);
		break;
	case DUAL_OUTPUT_MODE:
		dev_dbg(aicspi->dev, "DUAL OUTPUT\n");
		tx_single = head_len;
		spi_ctlr_quad_disable(base_addr);
		spi_ctlr_dual_enable(base_addr);
		spi_ctlr_set_xfer_cnt(base_addr, tx_total, rx_dlen, tx_single, 0);
		break;
	case DUAL_IO_MODE:
		dev_dbg(aicspi->dev, "DUAL I/O\n");
		tx_single = op->cmd.nbytes;
		spi_ctlr_quad_disable(base_addr);
		spi_ctlr_dual_enable(base_addr);
		spi_ctlr_set_xfer_cnt(base_addr, tx_total, rx_dlen, tx_single, 0);
		break;
	case QUAD_OUTPUT_MODE:
		dev_dbg(aicspi->dev, "QUAD OUTPUT\n");
		tx_single = head_len;//op->cmd.nbytes + op->addr.nbytes;
		spi_ctlr_dual_disable(base_addr);
		spi_ctlr_quad_enable(base_addr);
		spi_ctlr_set_xfer_cnt(base_addr, tx_total, rx_dlen, tx_single, 0);
		break;
	case QUAD_IO_MODE:
		dev_dbg(aicspi->dev, "QUAD I/O\n");
		tx_single = op->cmd.nbytes;
		spi_ctlr_dual_disable(base_addr);
		spi_ctlr_quad_enable(base_addr);
		spi_ctlr_set_xfer_cnt(base_addr, tx_total, rx_dlen, tx_single, 0);
		break;
	case QPI_MODE:
		dev_dbg(aicspi->dev, "QPI\n");
		tx_single = 0;
		spi_ctlr_dual_disable(base_addr);
		spi_ctlr_quad_enable(base_addr);
		spi_ctlr_set_xfer_cnt(base_addr, tx_total, rx_dlen, tx_single, 0);
		break;
	}

	spi_ctlr_set_rx_delay(aicspi, mem->spi->max_speed_hz);
	spi_ctlr_set_clk(mem->spi->max_speed_hz, clk_get_rate(aicspi->mclk),
			 base_addr);
	spi_ctlr_set_cs_enable(aicspi, true);
	spi_ctlr_pending_irq_clr(ISR_BIT_ALL_MSK, base_addr);
	reinit_completion(&aicspi->ctlr->xfer_completion);
	spi_ctlr_irq_enable(ICR_BIT_TC | ICR_BIT_ERRS, base_addr);
	spi_ctlr_reset_fifo(base_addr);
	if (use_dma) {
		/* Config DMA first */
		ret = spi_controller_dma_map_mem_op_data(aicspi->ctlr, op, &sg);
		if (ret)
			goto out;

		spi_ctlr_start_xfer(base_addr);
		if (head_len)
			spi_ctlr_fifo_write(aicspi, tx_head, head_len);

		if (rx_buf) {
			/* FIFO mode xfer head, then enalbe dma to xfer data */
			spi_ctlr_irq_disable(ICR_BIT_TC, base_addr);

			spi_ctlr_dma_rx_enable(aicspi->base_addr);
			ret = aic_spi_dma_rx_cfg(aicspi, sg.sgl, sg.nents);
			if (ret < 0)
				goto out;
#ifdef CONFIG_ARTINCHIP_DDMA
			if (aicspi->id == 0)
				aic_ddma_transfer(aicspi->dma_rx);
			else
#endif
				dma_async_issue_pending(aicspi->dma_rx);

		} else {
			spi_ctlr_dma_tx_enable(aicspi->base_addr);
			ret = aic_spi_dma_tx_cfg(aicspi, sg.sgl, sg.nents);
			if (ret < 0)
				goto out;
#ifdef CONFIG_ARTINCHIP_DDMA
			if (aicspi->id == 0)
				aic_ddma_transfer(aicspi->dma_tx);
			else
#endif
				dma_async_issue_pending(aicspi->dma_tx);

		}
	} else {
		spi_ctlr_start_xfer(base_addr);
		if (head_len)
			spi_ctlr_fifo_write(aicspi, tx_head, head_len);
		if (tx_buf)
			spi_ctlr_fifo_write(aicspi, tx_buf, tx_dlen);
		if (rx_buf)
			spi_ctlr_fifo_read(aicspi, rx_buf, rx_dlen);
	}

	tmo = 8LL * (tx_total + rx_dlen) * MSEC_PER_SEC;
	do_div(tmo, mem->spi->max_speed_hz);
	tmo += tmo + 200;
	if (tmo > UINT_MAX)
		tmo = UINT_MAX;
	if (!wait_for_completion_timeout(&aicspi->ctlr->xfer_completion,
					 msecs_to_jiffies(tmo))) {
		dev_err(aicspi->dev, "wait data xfer done timeout.\n");
		ret = -ETIMEDOUT;
		goto out;
	}
out:
	if (use_dma)
		spi_controller_dma_unmap_mem_op_data(aicspi->ctlr, op, &sg);

	spi_ctlr_set_cs_enable(aicspi, false);
	return ret;
}

static void aic_spi_release_dma(struct aic_spi *aicspi)
{
#ifdef CONFIG_ARTINCHIP_DDMA
	if (aicspi->id == 0) {
		if (aicspi->dma_rx)
			aic_ddma_release_chan(aicspi->dma_rx);
		if (aicspi->dma_tx)
			aic_ddma_release_chan(aicspi->dma_tx);
		return;
	}
#endif

	if (aicspi->dma_rx)
		dma_release_channel(aicspi->dma_rx);
	if (aicspi->dma_tx)
		dma_release_channel(aicspi->dma_tx);
}

static const struct spi_controller_mem_ops aic_spi_mem_ops = {
	.exec_op = aic_spi_mem_exec_op,
	.supports_op = aic_spi_mem_supports_op,
};

static int aic_spi_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct aic_spi_platform_data *pdata = NULL;
	struct spi_controller *ctlr;
	int ret = 0, err = 0, irq;
	struct resource	*mem_res;
	struct aic_spi *aicspi;
	const char *dly;
	u32 num_cs = 0;

	if (!np) {
		dev_err(&pdev->dev, "failed to get of_node\n");
		return -ENODEV;
	}

	pdev->id = of_alias_get_id(np, "spi");
	if (pdev->id < 0) {
		dev_err(&pdev->dev, "failed to get alias id\n");
		return -EINVAL;
	}

	pdata = kzalloc(sizeof(struct aic_spi_platform_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;
	pdev->dev.platform_data = pdata;

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem_res) {
		dev_err(&pdev->dev, "unable to get spi iomem resource\n");
		ret = -ENXIO;
		goto err0;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "failed to get irq\n");
		ret = -ENXIO;
		goto err0;
	}

	err = of_property_read_u32(pdev->dev.of_node, "num-cs", &num_cs);
	if (err) {
		/* Default: only support 1 spi device on bus */
		pdata->cs_num = 1;
	} else {
		pdata->cs_num = num_cs;
	}

	ctlr = spi_alloc_master(&pdev->dev, sizeof(struct aic_spi));
	if (ctlr == NULL) {
		dev_err(&pdev->dev, "Unable to allocate SPI Master\n");
		ret = -ENOMEM;
		goto err0;
	}

	platform_set_drvdata(pdev, ctlr);
	aicspi = spi_controller_get_devdata(ctlr);
	memset(aicspi, 0, sizeof(struct aic_spi));

	aicspi->id  = pdev->id;
	aicspi->dev = &pdev->dev;
	aicspi->ctlr = ctlr;
	aicspi->irq = irq;

#ifdef CONFIG_ARTINCHIP_DDMA
	if (aicspi->id == 0)
		aicspi->dma_rx = aic_ddma_request_chan(aicspi->dev, DMA_SPI0);
	else
#endif
		aicspi->dma_rx = dma_request_slave_channel(aicspi->dev, "rx");
	if (!aicspi->dma_rx)
		dev_warn(aicspi->dev, "failed to request rx dma channel\n");
	else
		ctlr->dma_rx = aicspi->dma_rx;

#ifdef CONFIG_ARTINCHIP_DDMA
	if (aicspi->id == 0)
		aicspi->dma_tx = aic_ddma_request_chan(aicspi->dev, DMA_SPI0);
	else
#endif
		aicspi->dma_tx = dma_request_slave_channel(aicspi->dev, "tx");
	if (!aicspi->dma_tx)
		dev_warn(aicspi->dev, "failed to request tx dma channel\n");
	else
		ctlr->dma_tx = aicspi->dma_tx;

	dly = NULL;
	device_property_read_string(&pdev->dev, "aic,rx-samp-dly", &dly);
	aicspi->rx_samp_dly = RX_SAMP_DLY_AUTO;
	if (dly  && !strcmp(dly, "none"))
		aicspi->rx_samp_dly = RX_SAMP_DLY_NONE;
	if (dly  && !strcmp(dly, "half"))
		aicspi->rx_samp_dly = RX_SAMP_DLY_HALF_CYCLE;
	if (dly  && !strcmp(dly, "one"))
		aicspi->rx_samp_dly = RX_SAMP_DLY_ONE_CYCLE;

	if (aicspi->dma_tx || aicspi->dma_rx)
		ctlr->can_dma = aic_spi_can_dma;
	aicspi->mode_type = MODE_TYPE_NULL;

	ctlr->dev.of_node = pdev->dev.of_node;
	ctlr->bus_num = pdev->id;
	ctlr->num_chipselect = pdata->cs_num;
	ctlr->setup = aic_spi_setup;
	ctlr->use_gpio_descriptors = true;
	ctlr->set_cs = aic_spi_set_cs;
	ctlr->transfer_one = aic_spi_transfer_one;
	ctlr->mem_ops = &aic_spi_mem_ops;
	ctlr->max_transfer_size = aic_spi_max_transfer_size;
	ctlr->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH | SPI_LSB_FIRST |
			  SPI_TX_DUAL | SPI_TX_QUAD | SPI_RX_DUAL |
			  SPI_RX_QUAD | SPI_3WIRE;

	if (request_mem_region(mem_res->start, resource_size(mem_res),
			       pdev->name) == NULL) {
		dev_err(&pdev->dev, "Req mem region failed\n");
		ret = -ENXIO;
		goto err1;
	}

	aicspi->base_addr = ioremap(mem_res->start, resource_size(mem_res));
	if (aicspi->base_addr == NULL) {
		dev_err(&pdev->dev, "unable to remap IO\n");
		ret = -ENXIO;
		goto err2;
	}

	spin_lock_init(&aicspi->lock);

	snprintf(aicspi->dev_name, sizeof(aicspi->dev_name),
		 AIC_SPI_DEV_NAME "%d", pdev->id);
	err = request_irq(aicspi->irq, aic_spi_handle_irq, 0, aicspi->dev_name,
			  aicspi);
	if (err) {
		dev_err(&pdev->dev, "request irq failed.\n");
		goto err3;
	}

	aicspi->dma_addr_rx = mem_res->start + SPI_REG_RXDATA;
	aicspi->dma_addr_tx = mem_res->start + SPI_REG_TXDATA;

	pdev->dev.init_name = aicspi->dev_name;

	ret = aic_spi_hw_init(aicspi, pdata);
	if (ret != 0) {
		dev_err(&pdev->dev, "spi hw init failed!\n");
		goto err4;
	}

	if (spi_register_controller(ctlr)) {
		dev_err(&pdev->dev, "cannot register spi controller\n");
		ret = -EBUSY;
		goto err5;
	}

	aic_spi_sysfs(pdev);

	dev_info(&pdev->dev, "spi-%d: driver probe done.\n", ctlr->bus_num);
	return 0;

err5:
	aic_spi_hw_exit(aicspi, pdata);
err4:
	free_irq(aicspi->irq, aicspi);
err3:
	iounmap(aicspi->base_addr);
err2:
	release_mem_region(mem_res->start, resource_size(mem_res));
err1:
	aic_spi_release_dma(aicspi);
	platform_set_drvdata(pdev, NULL);
	spi_controller_put(ctlr);
err0:
	kfree(pdata);

	return ret;
}

static int aic_spi_remove(struct platform_device *pdev)
{
	struct spi_controller *ctlr;
	struct resource	*mem_res;
	struct aic_spi *aicspi;

	ctlr = spi_controller_get(platform_get_drvdata(pdev));
	aicspi = spi_controller_get_devdata(ctlr);

	aic_spi_hw_exit(aicspi, pdev->dev.platform_data);
	spi_unregister_controller(ctlr);

	iounmap(aicspi->base_addr);
	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (mem_res != NULL)
		release_mem_region(mem_res->start, resource_size(mem_res));

	aic_spi_release_dma(aicspi);
	platform_set_drvdata(pdev, NULL);
	spi_controller_put(ctlr);
	kfree(pdev->dev.platform_data);

	return 0;
}

#define AIC_SPI_DEV_PM_OPS NULL

static const struct of_device_id aic_spi_match[] = {
	{ .compatible = "artinchip,aic-spi-v1.0", },
	{},
};
MODULE_DEVICE_TABLE(of, aic_spi_match);

static struct platform_driver aic_spi_driver = {
	.probe  = aic_spi_probe,
	.remove = aic_spi_remove,
	.driver = {
		.name           = AIC_SPI_DEV_NAME,
		.owner          = THIS_MODULE,
		.pm             = AIC_SPI_DEV_PM_OPS,
		.of_match_table = aic_spi_match,
	},
};

static int __init aic_spi_init(void)
{
	return platform_driver_register(&aic_spi_driver);
}

static void __exit aic_spi_exit(void)
{
	platform_driver_unregister(&aic_spi_driver);
}

module_init(aic_spi_init);
module_exit(aic_spi_exit);

MODULE_AUTHOR("Dehuang Wu");
MODULE_DESCRIPTION("Artinchip SPI Bus Driver");
MODULE_ALIAS("platform:"AIC_SPI_DEV_NAME);
MODULE_LICENSE("GPL");
