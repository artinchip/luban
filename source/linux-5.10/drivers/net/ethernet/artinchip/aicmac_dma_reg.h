/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#ifndef _AICMAC_DMA_REG_H_
#define _AICMAC_DMA_REG_H_

#include "aicmac_dma.h"

/*#define REGISTER_NAME		address	SHIFT	VALUE */

/* ------------- 0x0004 DMA0 Configuration  -------------------------*/
#define DMA_DMA0_CONFIG		0x00000004
#define DMA_DMA1_CONFIG		0x00000008
/* TX prioirty, 1: tx , 0: rx */
#define DMA_CONFIG_TX_PRI			0x00000008
/* Alternate Descriptor Size */
#define DMA_CONFIG_ATDS				0x00000200
#define DMA_CONFIG_MAXPBL			0x00010000
#define DMA_CONFIG_USP				0x02000000
/* Programmable Burst Len */
#define DMA_CONFIG_PBL_MASK			0x0000fc00
#define DMA_CONFIG_PBL_SHIFT		10
/* Rx-Programmable Burst Len */
#define DMA_CONFIG_RPBL_MASK			0x01f80000
#define DMA_CONFIG_RPBL_SHIFT		19
/* Fixed burst */
#define DMA_CONFIG_FB				0x00020000
/* Mixed burst */
#define DMA_CONFIG_MB				0x00040000
/* AAL Address-Aligned Beats */
#define DMA_CONFIG_AAL				0x04000000

/* ------------- 0x000C DMA0 Status Register 寄存器 ------------------*/
#define DMA0_INTR_STATUS	0x0000000C	/* DMA0 Status Register */
#define DMA1_INTR_STATUS	0x00000014	/* DMA0 Status Register */
/* GMAC LPI interrupt */
#define DMA_INTR_STATUS_GLPII			0x40000000
/* PMT interrupt */
#define DMA_INTR_STATUS_GPI			0x10000000
/* MMC interrupt */
#define DMA_INTR_STATUS_GMI			0x08000000
/* GMAC Line interface int */
#define DMA_INTR_STATUS_GLI			0x04000000
/* Error Bits Mask */
#define DMA_INTR_STATUS_EB_MASK			0x03800000
/* Error Bits - TX Abort */
#define DMA_INTR_STATUS_EB_TX_ABORT		0x00080000
/* Error Bits - RX Abort */
#define DMA_INTR_STATUS_EB_RX_ABORT		0x00100000
/* Transmit Process State */
#define DMA_INTR_STATUS_TS_MASK			0x00700000
#define DMA_INTR_STATUS_TS_SHIFT	20
/* Receive Process State */
#define DMA_INTR_STATUS_RS_MASK			0x000e0000
#define DMA_INTR_STATUS_RS_SHIFT	17
/* Normal Interrupt Summary */
#define DMA_INTR_STATUS_NIS			0x00010000
/* Abnormal Interrupt Summary */
#define DMA_INTR_STATUS_AIS			0x00008000
/* Early Receive Interrupt */
#define DMA_INTR_STATUS_ERI			0x00004000
/* Fatal Bus Error Interrupt */
#define DMA_INTR_STATUS_FBI			0x00002000
/* Early Transmit Interrupt */
#define DMA_INTR_STATUS_ETI			0x00000400
/* Receive Watchdog Timeout */
#define DMA_INTR_STATUS_RWT			0x00000200
/* Receive Process Stopped */
#define DMA_INTR_STATUS_RPS			0x00000100
/* Receive Buffer Unavailable */
#define DMA_INTR_STATUS_RU			0x00000080
/* Receive Interrupt */
#define DMA_INTR_STATUS_RI			0x00000040
/* Transmit Underflow */
#define DMA_INTR_STATUS_UNF			0x00000020
/* Receive Overflow */
#define DMA_INTR_STATUS_OVF			0x00000010
/* Transmit Jabber Timeout */
#define DMA_INTR_STATUS_TJT			0x00000008
/* Transmit Buffer Unavailable */
#define DMA_INTR_STATUS_TU			0x00000004
/* Transmit Process Stopped */
#define DMA_INTR_STATUS_TPS			0x00000002
/* Transmit Interrupt */
#define DMA_INTR_STATUS_TI			0x00000001

/* ------------- 0x0010 DMA0 Interrupt Enable -------------------------*/
#define DMA0_INTR_ENA		0x00000010	/* Interrupt Enable */
/* ------------- 0x0018 DMA1 Interrupt Enable -------------------------*/
#define DMA1_INTR_ENA		0x00000018	/* Interrupt Enable */
/* DMA Normal interrupt */
#define DMA_INTR_ENA_NIE			0x00010000
/* Transmit Interrupt */
#define DMA_INTR_ENA_TIE			0x00000001
/* Transmit Buffer Unavailable */
#define DMA_INTR_ENA_TUE			0x00000004
/* Receive Interrupt */
#define DMA_INTR_ENA_RIE			0x00000040
/* Early Receive */
#define DMA_INTR_ENA_ERE			0x00004000

#define DMA_INTR_NORMAL				\
		(DMA_INTR_ENA_NIE | DMA_INTR_ENA_RIE | DMA_INTR_ENA_TIE)

/* DMA Abnormal interrupt */
/* Abnormal Summary */
#define DMA_INTR_ENA_AIE			0x00008000
/* Fatal Bus Error */
#define DMA_INTR_ENA_FBE			0x00002000
/* Early Transmit */
#define DMA_INTR_ENA_ETE			0x00000400
/* Receive Watchdog */
#define DMA_INTR_ENA_RWE			0x00000200
/* Receive Stopped */
#define DMA_INTR_ENA_RSE			0x00000100
/* Receive Buffer Unavailable */
#define DMA_INTR_ENA_RUE			0x00000080
/* Tx Underflow */
#define DMA_INTR_ENA_UNE			0x00000020
/* Receive Overflow */
#define DMA_INTR_ENA_OVE			0x00000010
/* Transmit Jabber */
#define DMA_INTR_ENA_TJE			0x00000008
/* Transmit Stopped */
#define DMA_INTR_ENA_TSE			0x00000002

#define DMA_INTR_ABNORMAL			(DMA_INTR_ENA_AIE | \
						 DMA_INTR_ENA_FBE | \
						 DMA_INTR_ENA_UNE)

#define DMA_INTR_DEFAULT_MASK			(DMA_INTR_NORMAL | \
						 DMA_INTR_ABNORMAL)
#define DMA_INTR_DEFAULT_RX			(DMA_INTR_ENA_RIE | \
						 DMA_INTR_ENA_NIE)
#define DMA_INTR_DEFAULT_TX			(DMA_INTR_ENA_TIE | \
						 DMA_INTR_ENA_NIE)

/* ------------- 0x0024 TX DMA0 CONTROL 寄存器 ---------------------------*/
#define DMA_DMA0_TX_CONTROL	0x00000024
#define DMA_DMA1_TX_CONTROL	0x0000002C
/* Start/Stop Transmission */
#define DMA_CONTROL_ST				0x00000001
/* Flush transmit FIFO */
#define DMA_CONTROL_FTF				0x00000010
/* TSF: Transmit  Store and Forward */
#define DMA_CONTROL_TSF				0x00000020
/* OFS: Operate on second frame */
#define DMA_CONTROL_OSF				0x00000040
/* TX Poll Demand */
#define DMA_CONTROL_TX_POLL			0x00000080
#define DMA_CONTROL_TC_TX_MASK			0xfffffff1

enum ttc_control {
	DMA_CONTROL_TTC_64 = 0x00000000,	//0000
	DMA_CONTROL_TTC_128 = 0x00000002,	//0010
	DMA_CONTROL_TTC_192 = 0x00000004,	//0100
	DMA_CONTROL_TTC_256 = 0x00000006,	//0110
	DMA_CONTROL_TTC_40 = 0x00000008,	//1000
	DMA_CONTROL_TTC_32 = 0x0000000A,	//1010
	DMA_CONTROL_TTC_24 = 0x0000000C,	//1100
	DMA_CONTROL_TTC_16 = 0x0000000E,	//1110
};

/* ------------- 0x0028 RX DMA0 CONTROL 寄存器 ---------------------------*/
#define DMA_DMA0_RX_CONTROL	0x00000028
#define DMA_DMA1_RX_CONTROL	0x00000030
/* Start/Stop Receive */
#define DMA_CONTROL_SR				0x00000001
/* Receive Store and Forward */
#define DMA_CONTROL_RSF				0x00000008
/* RTC: Receive Threshold Control */
#define DMA_CONTROL_TC_RX_MASK			0xfffffff6

enum rtc_control {
	DMA_CONTROL_RTC_64 = 0x00000000,	//000
	DMA_CONTROL_RTC_32 = 0x00000002,	//010
	DMA_CONTROL_RTC_96 = 0x00000004,	//100
	DMA_CONTROL_RTC_128 = 0x00000006,	//110
};

/* ------------- 0x00B0 TXDMA0 Descriptor List Address寄存器 ------------*/
#define DMA0_TX_BASE_ADDR	0x000000B0	/* Transmit List Base */
#define DMA1_TX_BASE_ADDR	0x000000D0	/* Transmit List Base */

/* ------------- 0x00B4 RXDMA0 Descriptor List Address 寄存器 ------------*/
#define DMA0_RCV_BASE_ADDR	0x000000B4	/* Receive List Base */
#define DMA1_RCV_BASE_ADDR	0x000000D4	/* Receive List Base */

void aicmac_dma_reg_init(void __iomem *ioaddr,
			 struct aicmac_dma_data *dma_data, int atds);
void aicmac_dma_reg_init_rx_chain(void __iomem *ioaddr, dma_addr_t dma_rx_phy);
void aicmac_dma_reg_init_tx_chain(void __iomem *ioaddr, dma_addr_t dma_tx_phy);
u32 aicmac_dma_reg_configure_fc(u32 csr6, int rxfifosz);
void aicmac_dma_reg_operation_mode_rx(void __iomem *ioaddr, int mode,
				      u32 channel, int fifosz, u8 qmode);
void aicmac_dma_reg_operation_mode_tx(void __iomem *ioaddr, int mode,
				      u32 channel, int fifosz, u8 qmode);
void aicmac_dma_reg_flush_tx_fifo(void __iomem *ioaddr);
void aicmac_dma_reg_stop_rx(void __iomem *ioaddr, u32 chan);
void aicmac_dma_reg_start_rx(void __iomem *ioaddr, u32 chan);
void aicmac_dma_reg_stop_tx(void __iomem *ioaddr, u32 chan);
void aicmac_dma_reg_start_tx(void __iomem *ioaddr, u32 chan);
void aicmac_dma_reg_disable_irq(void __iomem *ioaddr, u32 chan, bool rx,
				bool tx);
void aicmac_dma_reg_enable_irq(void __iomem *ioaddr, u32 chan, bool rx,
			       bool tx);
void aicmac_dma_reg_enable_transmission(void __iomem *ioaddr);
int aicmac_dma_reg_interrupt_status(void __iomem *ioaddr, u32 chan);
#endif
