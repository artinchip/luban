/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#ifndef __8250_ARTINCHIP_H__
#define __8250_ARTINCHIP_H__

#include <linux/types.h>
#include <asm/byteorder.h>

#include "8250.h"

#define	AICUART_DRIVER_NAME		"aic-uart"
#define	AICUART_FIFO_SIZE		256
#define	AICUART_MAX_BRUST		1
#define AICUART_DEFAULT_CLOCK		48000000


#define	AIC_REG_SHIFT_DEFAULT	0x2
/* Offsets for the artinchip specific registers */
#define AIC_REG_UART_USR		0x1f		/* UART Status Register */
#define AIC_REG_UART_TFL		0x20		/* transmit FIFO level */
#define AIC_REG_UART_RFL		0x21		/* Receive FIFO Level */
#define AIC_REG_UART_HSK		0x22		/* DMA Handshake */
#define AIC_REG_UART_HALT		0x29		/* Halt tx register */
#define AIC_REG_UART_RS485		0x30		/* RS485 control register */

/* ArtInChip specific register fields */
#define AIC_UART_MCR_SIRE		0x40
#define AIC_UART_MCR_RS485		0x80
#define AIC_UART_MCR_RS485S		0xC0
#define AIC_UART_MCR_UART		0x00
#define AIC_UART_MCR_FUNC_MASK	0x3F

/* Status Register */
#define AIC_UART_USR_RFF		(BIT(4))
#define AIC_UART_USR_RFNE		(BIT(3))
#define AIC_UART_USR_TFE		(BIT(2))
#define AIC_UART_USR_TFNF		(BIT(1))
#define AIC_UART_USR_BUSY		(BIT(0))
/* Halt Register */
#define AIC_UART_HALT_LCRUP		(BIT(2))
#define AIC_UART_HALT_FORCECFG	(BIT(1))
#define AIC_UART_HALT_HTX		(BIT(0))
/* RS485 Control and Status Register */
#define AIC_UART_RS485_CTL_MODE (BIT(7))
#define AIC_UART_RS485_RXBFA	(BIT(3))
#define AIC_UART_RS485_RXAFA	(BIT(2))

/* Shifter Reg Empty */
#define AIC_IER_SREMPTY_ENABLE	(BIT(5))
#define AIC_IER_RS485_INT_EN	(BIT(4))

#define AIC_IIR_SREMPTY_STATUS	(BIT(4))

#define AIC_HSK_WAIT_CYCLE		0xA5		/* DMA wait cycle */
#define AIC_HSK_HAND_SHAKE		0xE5		/* DMA handshake */

#define AIC_UART_SETTING_TIMEOUT	100000

struct aic8250_port_data {
	int			line;
	struct uart_8250_dma	dma;
	u8			dlf_size;
};

struct aic8250_data {
	struct aic8250_port_data	data;

	int			msr_mask_on;
	int			msr_mask_off;
	struct clk		*clk;
	struct reset_control	*rst;

	unsigned int		uart_16550_compatible:1;
	unsigned int		tx_empty;
	unsigned int		rs485simple;
};

#endif
