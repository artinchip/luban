/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021, Artinchip Technology Co., Ltd
 * Author: Dehuang Wu <dehuang.wu@artinchip.com>
 */

#include <linux/regulator/consumer.h>
#include <linux/bits.h>

#ifndef _AIC_SPI_H_
#define _AIC_SPI_H_

#define SPI_FIFO_DEPTH		64
#define SPI_MAX_FREQUENCY	133000000
#define AIC_SPI_MAX_XFER_SIZE	0xffffff

/* SPI Registers offset */
#define SPI_REG_VER		(0x00)
#define SPI_REG_GCR		(0x04)
#define SPI_REG_TCR		(0x08)
#define SPI_REG_ICR		(0x10)
#define SPI_REG_ISR		(0x14)
#define SPI_REG_FCR		(0x18)
#define SPI_REG_FSR		(0x1C)
#define SPI_REG_WCR		(0x20)
#define SPI_REG_CCR		(0x24)
#define SPI_REG_BCR		(0x30)
#define SPI_REG_MTC		(0x34)
#define SPI_REG_BCC		(0x38)
#define SPI_REG_BTC		(0x40)
#define SPI_REG_BCLK		(0x44)
#define SPI_REG_TBR		(0x48)
#define SPI_REG_RBR		(0x4C)
#define SPI_REG_DCR		(0x88)
#define SPI_REG_TXDATA		(0x200)
#define SPI_REG_RXDATA		(0x300)

/* SPI Global Control Register Bit Fields & Masks */
#define GCR_BIT_EN		BIT(0)
#define GCR_BIT_MODE		BIT(1)
#define GCR_BIT_PAUSE		BIT(7)
#define GCR_BIT_SRST		BIT(31)

/* SPI Transfer Control Register Bit Fields & Masks */
#define TCR_BIT_CPHA		BIT(0)
#define TCR_BIT_CPOL		BIT(1)
#define TCR_BIT_SPOL		BIT(2)
#define TCR_BIT_SSCTL		BIT(3)
#define TCR_BIT_SS_OWNER	BIT(6)
#define TCR_BIT_SS_LEVEL	BIT(7)
#define TCR_BIT_DHB		BIT(8)
#define TCR_BIT_DDB		BIT(9)
#define TCR_BIT_RPSM		BIT(10)
#define TCR_BIT_RXINDLY_EN      BIT(11)
#define TCR_BIT_FBS		BIT(12)
#define TCR_BIT_RXDLY_DIS       BIT(13)
#define TCR_BIT_SDDM		BIT(14)
#define TCR_BIT_3WIRE		BIT(25)
#define TCR_BIT_XCH		BIT(31)
#define TCR_BIT_SS_SEL_OFF	(4)
#define TCR_BIT_SS_SEL_MSK	GENMASK(5, 4)

#define TCR_RX_SAMPDLY_MSK      (TCR_BIT_RXINDLY_EN | TCR_BIT_RXDLY_DIS)
#define TCR_RX_SAMPDLY_NONE     (TCR_BIT_RXDLY_DIS)
#define TCR_RX_SAMPDLY_HALF     (0)
#define TCR_RX_SAMPDLY_ONE      (TCR_BIT_RXINDLY_EN)

/* SPI Interrupt Control Register Bit Fields & Masks */
#define ICR_BIT_RX_RDY		BIT(0)
#define ICR_BIT_RX_EMP		BIT(1)
#define ICR_BIT_RX_FULL		BIT(2)
#define ICR_BIT_TX_ERQ		BIT(4)
#define ICR_BIT_TX_EMP		BIT(5)
#define ICR_BIT_TX_FULL		BIT(6)
#define ICR_BIT_RX_OVF		BIT(8)
#define ICR_BIT_RX_UDR		BIT(9)
#define ICR_BIT_TX_OVF		BIT(10)
#define ICR_BIT_TX_UDR		BIT(11)
#define ICR_BIT_TC		BIT(12)
#define ICR_BIT_SSI		BIT(13)
#define ICR_BIT_ERRS		(ICR_BIT_TX_OVF | ICR_BIT_RX_UDR | \
				 ICR_BIT_RX_OVF)
#define ICR_BIT_ALL_MSK		(0x77 | (0x3f << 8))

/* SPI Interrupt Status Register Bit Fields & Masks */
#define ISR_BIT_RX_RDY		BIT(0)
#define ISR_BIT_RX_EMP		BIT(1)
#define ISR_BIT_RX_FULL		BIT(2)
#define ISR_BIT_TX_RDY		BIT(4)
#define ISR_BIT_TX_EMP		BIT(5)
#define ISR_BIT_TX_FULL		BIT(6)
#define ISR_BIT_RX_OVF		BIT(8)
#define ISR_BIT_RX_UDR		BIT(9)
#define ISR_BIT_TX_OVF		BIT(10)
#define ISR_BIT_TX_UDR		BIT(11)
#define ISR_BIT_TC		BIT(12)
#define ISR_BIT_SSI		BIT(13)
#define ISR_BIT_ERRS		(ISR_BIT_TX_OVF | ISR_BIT_RX_UDR | \
				 ISR_BIT_RX_OVF)
#define ISR_BIT_ALL_MSK		(0x77 | (0x3f << 8))

/* SPI FIFO Control Register Bit Fields & Masks */
#define FCR_BIT_RX_LEVEL_MSK	GENMASK(7, 0)
#define FCR_BIT_RX_DMA_EN	BIT(8)
#define FCR_BIT_RX_RST		BIT(15)
#define FCR_BIT_TX_LEVEL_MSK	GENMASK(23, 16)
#define FCR_BIT_TX_DMA_EN	BIT(24)
#define FCR_BIT_TX_RST		BIT(31)
#define FCR_BIT_DMA_EN_MSK	(FCR_BIT_TX_DMA_EN | FCR_BIT_RX_DMA_EN)

/* SPI FIFO Status Register Bit Fields & Masks */
#define FSR_BIT_RX_CNT_MSK	GENMASK(7, 0)
#define FSR_BIT_RB_CNT_MSK	GENMASK(14, 12)
#define FSR_BIT_RB_WR_EN	BIT(15)
#define FSR_BIT_TX_CNT_MSK	GENMASK(23, 16)
#define FSR_BIT_TB_CNT_MSK	GENMASK(30, 28)
#define FSR_BIT_TB_WR_EN	BIT(31)
#define FSR_BIT_RXCNT_OFF	(0)
#define FSR_BIT_TXCNT_OFF	(16)

/* SPI Wait Clock Register Bit Fields & Masks */
#define WCR_BIT_WCC_MSK		GENMASK(15, 0)
#define WCR_BIT_SWC_MSK		GENMASK(19, 16)

/* SPI Clock Control Register Bit Fields & Masks */
#define CCR_BIT_CDR2_OFF	(0)
#define CCR_BIT_CDR1_OFF	(8)
#define CCR_BIT_CDR2_MSK	GENMASK(7, 0)
#define CCR_BIT_CDR1_MSK	GENMASK(11, 8)
#define CCR_BIT_DRS		BIT(12)

/* SPI Master Burst Counter Register Bit Fields & Masks */
#define BCR_BIT_CNT_MSK		GENMASK(23, 0)

/* SPI Master Transmit Counter reigster */
#define MTC_BIT_CNT_MSK		GENMASK(23, 0)

/* SPI Master Burst Control Counter reigster Bit Fields & Masks */
#define BCC_BIT_STC_MSK		GENMASK(23, 0)
#define BCC_BIT_DBC_MSK		GENMASK(27, 24)
#define BCC_BIT_DUAL_MODE	BIT(28)
#define BCC_BIT_QUAD_MODE	BIT(29)

#define BTC_BIT_WM_OFS		(0)
#define BTC_BIT_WM_MSK		GENMASK(1, 0)
#define BTC_BIT_WM_BYTE		(0x0)
#define BTC_BIT_WM_BIT_3WIRE	(0x2)
#define BTC_BIT_WM_BIT_STD	(0x3)
#define BTC_BIT_SS_SEL_OFS	(2)
#define BTC_BIT_SS_SEL_MSK	GENMASK(3, 2)
#define BTC_BIT_SPOL_OFS	(5)
#define BTC_BIT_SPOL_MSK	BIT(5)
#define BTC_BIT_SS_OWNER_OFS	(6)
#define BTC_BIT_SS_OWNER_MSK	BIT(6)
#define BTC_BIT_SS_LEVEL_OFS	(7)
#define BTC_BIT_SS_LEVEL_MSK	BIT(7)
#define BTC_BIT_TX_BIT_LEN_OFS	(8)
#define BTC_BIT_TX_BIT_LEN_MSK	GENMASK(13, 8)
#define BTC_BIT_RX_BIT_LEN_OFS	(16)
#define BTC_BIT_RX_BIT_LEN_MSK	GENMASK(21, 16)
#define BTC_BIT_XFER_COMP_OFS	(25)
#define BTC_BIT_XFER_COMP_MSK	BIT(25)
#define BTC_BIT_SAMP_MODE_OFS	(30)
#define BTC_BIT_SAMP_MODE_MSK	BIT(30)
#define BTC_BIT_XFER_EN_OFS	(31)
#define BTC_BIT_XFER_EN_MSK	BIT(31)

#define DCR_BIT_DMA_CYCLE_MSK	GENMASK(4, 0)

/* spi_device mode options, only can be used in AIC controller */
#define SPI_DUMMY_FILL_ONE	BIT(30)
#define SPI_RECEIVE_ALL_BURST	BIT(31)

#define AIC_SPI_DEV_NAME	"spi"
#define AIC_SPI_BUS_MASK(bus)	BIT(bus)

#define AIC_SPI_DMA_RX(ch)	(DMASRC_SPI0_RX + ch)
#define AIC_SPI_DMA_TX(ch)	(DMADST_SPI0_TX + ch)

#define SUPPORT_SPI_3WIRE_IN_BIT_MODE
#endif
