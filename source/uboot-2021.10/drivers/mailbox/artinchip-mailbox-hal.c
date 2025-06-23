/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Mailbox driver of ArtInChip SoC Uboot
 *
 * Copyright (C) 2020-2024 ArtInChip Technology Co., Ltd.
 * Authors:  weihui.xu <weihui.xu@artinchip.com>
 */
#include <dm.h>
#include <asm/io.h>
#include <dm/device_compat.h>
#include "artinchip-mailbox-hal.h"

#define MBOX_REG_CTL(base)	((base) + 0x00)
#define MBOX_REG_FIFO_CSR(base)	((base) + 0x04)
#define MBOX_REG_IRQ_EN(base)	((base) + 0x08)
#define MBOX_REG_IRQ_STAS(base)	((base) + 0x0C)
#define MBOX_REG_WMSG(base)	((base) + 0x10)
#define MBOX_REG_WCOMP(base)	((base) + 0x14)
#define MBOX_REG_RMSG(base)	((base) + 0x18)
#define MBOX_REG_RCOMP(base)	((base) + 0x1C)
#define MBOX_REG_VERSION(base)	((base) + 0xFFC)

#define MBOX_CTL_CMP_NO_IRQ	BIT(1)
#define MBOX_CTL_DBG_MODE	BIT(0)

#define MBOX_FCSR_RX_RST	BIT(31)
#define MBOX_FCSR_RX_LVL_SHIFT	24
#define MBOX_FCSR_RX_LVL_MASK	GENMASK(29, 24)
#define MBOX_FCSR_RX_CNT_SHIFT	16
#define MBOX_FCSR_RX_CNT_MASK	GENMASK(23, 16)
#define MBOX_FCSR_TX_RST	BIT(15)
#define MBOX_FCSR_TX_OF_RO	BIT(14) /* Read only when overflow */
#define MBOX_FCSR_TX_LVL_SHIFT	8
#define MBOX_FCSR_TX_LVL_MASK	GENMASK(13, 8)
#define MBOX_FCSR_TX_CNT_SHIFT	0
#define MBOX_FCSR_TX_CNT_MASK	GENMASK(7, 0)

#define MBOX_IRQ_M_TX_CMP	BIT(11)
#define MBOX_IRQ_RX_UF		BIT(10)	/* underflow */
#define MBOX_IRQ_RX_FULL	BIT(9)
#define MBOX_IRQ_RX_EMPTY	BIT(8)
#define MBOX_IRQ_R_RX_CMP	BIT(3)
#define MBOX_IRQ_TX_OF		BIT(2)	/* overflow */
#define MBOX_IRQ_TX_FULL	BIT(1)
#define MBOX_IRQ_TX_EMPTY	BIT(0)
#define MBOX_IRQ_CARED		(MBOX_IRQ_M_TX_CMP | MBOX_IRQ_RX_UF | \
				MBOX_IRQ_RX_FULL | MBOX_IRQ_R_RX_CMP | \
				MBOX_IRQ_TX_OF)

#define writel_clrbits(mask, addr)	writel(readl(addr) & ~(mask), addr)
#define writel_clrbit(bit, addr)	writel_clrbits(bit, addr)
#define writel_bits(val, mask, shift, addr) \
	({ \
		if (val) \
			writel((readl(addr) & ~(mask)) | ((val) << (shift)), \
				addr); \
		else \
			writel_clrbits(mask, addr); \
	})
#define writel_bit(bit, addr)		writel(readl(addr) | bit, addr)
#define readl_bits(mask, shift, addr)	((readl(addr) & (mask)) >> (shift))
#define readl_bit(bit, addr)		((readl(addr) & bit) ? 1 : 0)

void aic_mbox_dbg_mode(void __iomem *base)
{
	writel_bit(MBOX_CTL_DBG_MODE, MBOX_REG_CTL(base));
}

void aic_mbox_cmp_mode(void __iomem *base, u32 no_irq)
{
	if (no_irq)
		writel_bit(MBOX_CTL_CMP_NO_IRQ, MBOX_REG_CTL(base));
	else
		writel_clrbit(MBOX_CTL_CMP_NO_IRQ, MBOX_REG_CTL(base));
}

void aic_mbox_rxfifo_rst(void __iomem *base)
{
	writel_clrbit(MBOX_FCSR_RX_RST, MBOX_REG_FIFO_CSR(base));
}

void aic_mbox_rxfifo_lvl(void __iomem *base, u32 level)
{
	writel_bits(level, MBOX_FCSR_RX_LVL_MASK, MBOX_FCSR_RX_LVL_SHIFT,
		    MBOX_REG_FIFO_CSR(base));
}

u32 aic_mbox_rxfifo_cnt(void __iomem *base)
{
	return readl_bits(MBOX_FCSR_RX_CNT_MASK, MBOX_FCSR_RX_CNT_SHIFT,
			  MBOX_REG_FIFO_CSR(base));
}

void aic_mbox_txfifo_rst(void __iomem *base)
{
	writel_clrbit(MBOX_FCSR_TX_RST, MBOX_REG_FIFO_CSR(base));
}

void aic_mbox_txfifo_lvl(void __iomem *base, u32 level)
{
	writel_bits(level, MBOX_FCSR_TX_LVL_MASK, MBOX_FCSR_TX_LVL_SHIFT,
		    MBOX_REG_FIFO_CSR(base));
}

u32 aic_mbox_txfifo_cnt(void __iomem *base)
{
	return readl_bits(MBOX_FCSR_TX_CNT_MASK, MBOX_FCSR_TX_CNT_SHIFT,
			  MBOX_REG_FIFO_CSR(base));
}

u32 aic_mbox_int_sta(void __iomem *base)
{
	return readl(MBOX_REG_IRQ_STAS(base));
}

/* Clear all the pending status */
void aic_mbox_int_clr(void __iomem *base, u32 sta)
{
	writel(sta, MBOX_REG_IRQ_STAS(base));
}

u32 aic_mbox_int_sta_is_tx_cmp(u32 sta)
{
	return sta & MBOX_IRQ_M_TX_CMP ? 1 : 0;
}

u32 aic_mbox_int_sta_is_rx_uf(u32 sta)
{
	return sta & MBOX_IRQ_RX_UF ? 1 : 0;
}

u32 aic_mbox_int_sta_is_rx_full(u32 sta)
{
	return sta & MBOX_IRQ_RX_FULL ? 1 : 0;
}

u32 aic_mbox_int_sta_is_rx_empty(u32 sta)
{
	return sta & MBOX_IRQ_RX_EMPTY ? 1 : 0;
}

u32 aic_mbox_int_sta_is_rx_cmp(u32 sta)
{
	return sta & MBOX_IRQ_R_RX_CMP ? 1 : 0;
}

u32 aic_mbox_int_sta_is_tx_of(u32 sta)
{
	return sta & MBOX_IRQ_TX_OF ? 1 : 0;
}

u32 aic_mbox_int_sta_is_tx_full(u32 sta)
{
	return sta & MBOX_IRQ_TX_FULL ? 1 : 0;
}

u32 aic_mbox_int_sta_is_tx_empty(u32 sta)
{
	return sta & MBOX_IRQ_TX_EMPTY ? 1 : 0;
}

u32 aic_mbox_int_sta_is_cared(u32 sta)
{
	return sta & MBOX_IRQ_CARED ? 1 : 0;
}

void aic_mbox_int_en(void __iomem *base, u32 bit)
{
	if (bit)
		writel_bit(bit, MBOX_REG_IRQ_EN(base));
	else
		writel_clrbit(bit, MBOX_REG_IRQ_EN(base));
}

void aic_mbox_int_all_en(void __iomem *base, u32 enable)
{
	if (enable)
		writel(MBOX_IRQ_CARED, MBOX_REG_IRQ_EN(base));
	else
		writel(0, MBOX_REG_IRQ_EN(base));
}

u32 aic_mbox_wr(void __iomem *base, u32 *data, u32 cnt)
{
	u32 i = 0;

	if (!data)
		return 0;

	for (i = 0; (i < cnt) && (i < MBOX_MAX_DAT_LEN - 1); i++)
		writel(data[i], MBOX_REG_WMSG(base));

	return i;
}

void aic_mbox_wr_cmp(void __iomem *base, u32 data)
{
	writel(data, MBOX_REG_WCOMP(base));
}

u32 aic_mbox_rd(void __iomem *base, u32 *data, u32 cnt)
{
	u32 i;

	if (!data)
		return 0;

	for (i = 0; (i < cnt) && (i < MBOX_MAX_DAT_LEN - 1); i++)
		data[i] = readl(MBOX_REG_RMSG(base));

	return i;
}

u32 aic_mbox_rd_cmp(void __iomem *base)
{
	return readl(MBOX_REG_RCOMP(base));
}


