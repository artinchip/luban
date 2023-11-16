/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Device Tree support for Artinchip SoCs
 *
 * Copyright (C) 2021 Artinchip Technology Co., Ltd
 */

#ifndef __DT_BINDINGS_CLOCK_AIC_CMU_H
#define __DT_BINDINGS_CLOCK_AIC_CMU_H

	/* Fixed rate clock */
#define	CLK_DUMMY		0
#define	CLK_OSC24M		1
#define	CLK_OSC32K		2
#define	CLK_RC1M		3
	/* PLL clock */
#define	CLK_PLL_INT0		4
#define	CLK_PLL_INT1		5
#define	CLK_PLL_FRA0		6
#define	CLK_PLL_FRA1		7
#define	CLK_PLL_FRA2		8
	/* system clock */
#define	CLK_AXI0		9
#define	CLK_AHB0		10
#define	CLK_APB0		11
#define	CLK_APB1		12
#define	CLK_CPU			13
	/* Peripheral clock */
#define	CLK_DMA			14
#define	CLK_CE			15
#define	CLK_USBD		16
#define	CLK_USBH0		17
#define	CLK_USBH1		18
#define	CLK_USB_PHY0	19
#define	CLK_USB_PHY1	20
#define	CLK_GMAC0		21
#define	CLK_GMAC1		22
#define	CLK_SPI0		23
#define	CLK_SPI1		24
#define CLK_SPI2		25
#define CLK_SPI3		26
#define	CLK_SDMC0		27
#define	CLK_SDMC1		28
#define	CLK_SDMC2		29
#define	CLK_SYSCFG		30
#define	CLK_RTC			31
#define	CLK_SPIENC		32
#define	CLK_I2S0		33
#define	CLK_I2S1		34
#define	CLK_CODEC		35
#define	CLK_RGB			36
#define	CLK_DBI			36
#define	CLK_LVDS		37
#define	CLK_MIPIDSI		38
#define	CLK_DE			39
#define	CLK_GE			40
#define	CLK_VE			41
#define	CLK_WDOG		42
#define	CLK_SID			43
#define	CLK_GTC			44
#define	CLK_GPIO		45
#define	CLK_UART0		46
#define	CLK_UART1		47
#define	CLK_UART2		48
#define	CLK_UART3		49
#define	CLK_UART4		50
#define	CLK_UART5		51
#define	CLK_UART6		52
#define	CLK_UART7		53
#define	CLK_I2C0		54
#define	CLK_I2C1		55
#define	CLK_I2C2		56
#define	CLK_I2C3		57
#define	CLK_CAN0		58
#define	CLK_CAN1		59
#define	CLK_PWM			60
#define	CLK_ADCIM		61
#define	CLK_GPAI		62
#define	CLK_RTP			63
#define	CLK_TSEN		64
#define	CLK_CIR			65
#define	CLK_DVP			66
#define	CLK_PBUS		67
#define	CLK_MTOP			68
#define CLK_DM			69
#define CLK_PWMCS		70
#define CLK_PSADC		71
#define CLK_DDR			72
	/* Display clock */
#define	CLK_PIX			73
#define	CLK_SCLK		74
	/* Output clock */
#define	CLK_OUT0		75
#define	CLK_OUT1		76
#define	CLK_OUT2		77
#define	CLK_OUT3		78
#define	AIC_CLK_END		79

#endif
