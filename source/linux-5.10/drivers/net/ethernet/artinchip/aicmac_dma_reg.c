// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#include <linux/io.h>
#include <linux/iopoll.h>

#include "aicmac_dma_reg.h"
#include "aicmac_dma_desc.h"

void aicmac_dma_reg_init_intr(void __iomem *ioaddr)
{
	writel(DMA_INTR_DEFAULT_MASK, ioaddr + DMA0_INTR_ENA);
}

void aicmac_dma_reg_init(void __iomem *ioaddr, struct aicmac_dma_data *dma_data,
			 int atds)
{
	u32 value = readl(ioaddr + DMA_DMA0_CONFIG);
	int txpbl = dma_data->txpbl ?: dma_data->pbl;
	int rxpbl = dma_data->rxpbl ?: dma_data->pbl;

	if (dma_data->pblx8)
		value |= DMA_CONFIG_MAXPBL;
	value |= DMA_CONFIG_USP;
	value &= ~(DMA_CONFIG_PBL_MASK | DMA_CONFIG_RPBL_MASK);
	value |= (txpbl << DMA_CONFIG_PBL_SHIFT);
	value |= (rxpbl << DMA_CONFIG_RPBL_SHIFT);
	value |= DMA_CONFIG_TX_PRI;

	if (dma_data->fixed_burst)
		value |= DMA_CONFIG_FB;

	if (dma_data->mixed_burst)
		value |= DMA_CONFIG_MB;

	if (atds)
		value |= DMA_CONFIG_ATDS;

	if (dma_data->aal)
		value |= DMA_CONFIG_AAL;

	writel(value, ioaddr + DMA_DMA0_CONFIG);

	aicmac_dma_reg_init_intr(ioaddr);
}

void aicmac_dma_reg_init_rx_chain(void __iomem *ioaddr, dma_addr_t dma_rx_phy)
{
	writel(lower_32_bits(dma_rx_phy), ioaddr + DMA0_RCV_BASE_ADDR);
}

void aicmac_dma_reg_init_tx_chain(void __iomem *ioaddr, dma_addr_t dma_tx_phy)
{
	writel(lower_32_bits(dma_tx_phy), ioaddr + DMA0_TX_BASE_ADDR);
}

void aicmac_dma_reg_operation_mode_rx(void __iomem *ioaddr, int mode,
				      u32 channel, int fifosz, u8 qmode)
{
	u32 csr6 = readl(ioaddr + DMA_DMA0_RX_CONTROL);

	if (mode == SF_DMA_MODE) {
		pr_info("GMAC: enable RX store and forward mode\n");
		csr6 |= DMA_CONTROL_RSF;
	} else {
		pr_info("GMAC: disable RX SF mode (threshold %d)\n", mode);
		csr6 &= ~DMA_CONTROL_RSF;
		csr6 &= DMA_CONTROL_TC_RX_MASK;
		if (mode <= 32)
			csr6 |= DMA_CONTROL_RTC_32;
		else if (mode <= 64)
			csr6 |= DMA_CONTROL_RTC_64;
		else if (mode <= 96)
			csr6 |= DMA_CONTROL_RTC_96;
		else
			csr6 |= DMA_CONTROL_RTC_128;
	}

	writel(csr6, ioaddr + DMA_DMA0_RX_CONTROL);
}

void aicmac_dma_reg_operation_mode_tx(void __iomem *ioaddr, int mode,
				      u32 channel, int fifosz, u8 qmode)
{
	u32 csr6 = readl(ioaddr + DMA_DMA0_TX_CONTROL);

	if (mode == SF_DMA_MODE) {
		pr_info("GMAC: enable TX store and forward mode\n");
		csr6 |= DMA_CONTROL_TSF;
		csr6 |= DMA_CONTROL_OSF;
	} else {
		pr_info("GMAC: disabling TX SF (threshold %d)\n", mode);
		csr6 &= ~DMA_CONTROL_TSF;
		csr6 &= DMA_CONTROL_TC_TX_MASK;
		if (mode <= 32)
			csr6 |= DMA_CONTROL_TTC_32;
		else if (mode <= 64)
			csr6 |= DMA_CONTROL_TTC_64;
		else if (mode <= 128)
			csr6 |= DMA_CONTROL_TTC_128;
		else if (mode <= 192)
			csr6 |= DMA_CONTROL_TTC_192;
		else
			csr6 |= DMA_CONTROL_TTC_256;
	}

	writel(csr6, ioaddr + DMA_DMA0_TX_CONTROL);
}

void aicmac_dma_reg_flush_tx_fifo(void __iomem *ioaddr)
{
	u32 csr6 = readl(ioaddr + DMA_DMA0_TX_CONTROL);

	writel((csr6 | DMA_CONTROL_FTF), ioaddr + DMA_DMA0_TX_CONTROL);

	do {
	} while ((readl(ioaddr + DMA_DMA0_TX_CONTROL) & DMA_CONTROL_FTF));
}

void aicmac_dma_reg_enable_transmission(void __iomem *ioaddr)
{
	u32 value = readl(ioaddr + DMA_DMA0_TX_CONTROL);

	value |= DMA_CONTROL_TX_POLL;
	writel(value, ioaddr + DMA_DMA0_TX_CONTROL);
}

void aicmac_dma_reg_enable_irq(void __iomem *ioaddr, u32 chan, bool rx, bool tx)
{
	u32 value = readl(ioaddr + DMA0_INTR_ENA);

	if (rx)
		value |= DMA_INTR_DEFAULT_RX;
	if (tx)
		value |= DMA_INTR_DEFAULT_TX;

	writel(value, ioaddr + DMA0_INTR_ENA);
}

void aicmac_dma_reg_disable_irq(void __iomem *ioaddr, u32 chan, bool rx,
				bool tx)
{
	u32 value = readl(ioaddr + DMA0_INTR_ENA);

	if (rx)
		value &= ~DMA_INTR_DEFAULT_RX;
	if (tx)
		value &= ~DMA_INTR_DEFAULT_TX;

	writel(value, ioaddr + DMA0_INTR_ENA);
}

void aicmac_dma_reg_start_tx(void __iomem *ioaddr, u32 chan)
{
	u32 value = readl(ioaddr + DMA_DMA0_TX_CONTROL);

	value |= DMA_CONTROL_ST;
	writel(value, ioaddr + DMA_DMA0_TX_CONTROL);
}

void aicmac_dma_reg_stop_tx(void __iomem *ioaddr, u32 chan)
{
	u32 value = readl(ioaddr + DMA_DMA0_TX_CONTROL);

	value &= ~DMA_CONTROL_ST;
	writel(value, ioaddr + DMA_DMA0_TX_CONTROL);
}

void aicmac_dma_reg_start_rx(void __iomem *ioaddr, u32 chan)
{
	u32 value = readl(ioaddr + DMA_DMA0_RX_CONTROL);

	value |= DMA_CONTROL_SR;
	writel(value, ioaddr + DMA_DMA0_RX_CONTROL);
}

void aicmac_dma_reg_stop_rx(void __iomem *ioaddr, u32 chan)
{
	u32 value = readl(ioaddr + DMA_DMA0_RX_CONTROL);

	value &= ~DMA_CONTROL_SR;
	writel(value, ioaddr + DMA_DMA0_RX_CONTROL);
}

int aicmac_dma_reg_interrupt_status(void __iomem *ioaddr, u32 chan)
{
	int ret = 0;
	u32 intr_status = readl(ioaddr + DMA0_INTR_STATUS);

	/* ABNORMAL interrupts */
	if (unlikely(intr_status & DMA_INTR_STATUS_AIS)) {
		if (unlikely(intr_status & DMA_INTR_STATUS_UNF))
			ret = tx_hard_error_bump_tc;
		if (unlikely(intr_status & DMA_INTR_STATUS_TPS))
			ret = tx_hard_error;
		if (unlikely(intr_status & DMA_INTR_STATUS_FBI))
			ret = tx_hard_error;
	}
	/* TX/RX NORMAL interrupts */
	if (likely(intr_status & DMA_INTR_STATUS_NIS)) {
		if (likely(intr_status & DMA_INTR_STATUS_RI)) {
			u32 value = readl(ioaddr + DMA0_INTR_ENA);
			/* to schedule NAPI on real RIE event. */
			if (likely(value & DMA_INTR_ENA_RIE))
				ret |= handle_rx;
		}
		if (likely(intr_status & DMA_INTR_STATUS_TI))
			ret |= handle_tx;
	}

	/* Clear the interrupt by writing a logic 1 to the CSR5[15-0] */
	writel((intr_status & 0x1ffff), ioaddr + DMA0_INTR_STATUS);

	return ret;
}
