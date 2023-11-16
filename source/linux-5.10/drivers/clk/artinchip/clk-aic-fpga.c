// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, Artinchip Technology Co., Ltd
 */
#include <dt-bindings/clock/artinchip,aic-cmu.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/types.h>
#include "clk-aic.h"

#ifdef CONFIG_DEBUG_ON_FPGA_BOARD_ARTINCHIP
#define CLOCK1_FREQ 48000000
#define CLOCK2_FREQ 100000000
#define CLOCK3_FREQ 24576000
#define CLOCK4_FREQ 62500000
#define CLOCK5_FREQ 48000000
#define CLOCK6_FREQ 60000000
#define CLOCK_120M  120000000
#define CLOCK_100M  100000000
#define CLOCK_72M   72000000
#define CLOCK_60M   60000000
#define CLOCK_50M   50000000
#define CLOCK_30M   30000000
#define CLOCK_24M   24000000
#define CLOCK_12M   12000000
#define CLOCK_1M    1000000
#define CLOCK_32K   32768

const unsigned long fpga_board_rate[] = {
	[CLK_OSC24M]    = CLOCK_24M,
	[CLK_OSC32K]    = CLOCK_32K,
	[CLK_RC1M]      = CLOCK_1M,
	[CLK_PLL_INT0]  = CLOCK_24M,
	[CLK_PLL_INT1]  = CLOCK_24M,
	[CLK_PLL_FRA0]  = CLOCK_24M,
	[CLK_PLL_FRA1]  = CLOCK_24M,
	[CLK_PLL_FRA2]  = CLOCK_24M,
	[CLK_AXI0_SRC1] = CLOCK4_FREQ,
	[CLK_AHB0_SRC1] = CLOCK_60M,
	[CLK_APB0_SRC1] = CLOCK_30M,
	[CLK_APB1_SRC1] = CLOCK_24M,
	[CLK_CPU_SRC1]  = CLOCK_100M,
	[CLK_AXI0]      = CLOCK4_FREQ,
	[CLK_AHB0]      = CLOCK_24M,
	[CLK_APB0]      = CLOCK_24M,
	[CLK_APB1]      = CLOCK_24M,
	[CLK_CPU]       = CLOCK_100M,
	[CLK_DMA]       = CLOCK_60M,
	[CLK_CE]        = CLOCK_72M,
	[CLK_USBD]      = CLOCK_60M,
	[CLK_USBH0]     = CLOCK_60M,
	[CLK_USBH1]     = CLOCK_60M,
	[CLK_USB_PHY0]  = CLOCK_60M,
	[CLK_USB_PHY1]  = CLOCK_60M,
	[CLK_GMAC0]     = CLOCK_50M,
	[CLK_GMAC1]     = CLOCK_50M,
	[CLK_SPI0]      = CLOCK_24M,
	[CLK_SPI1]      = CLOCK_24M,
	[CLK_SDMC0]     = CLOCK1_FREQ,
	[CLK_SDMC1]     = CLOCK1_FREQ,
	[CLK_SDMC2]     = CLOCK1_FREQ,
	[CLK_SYSCFG]    = CLOCK_24M,
	[CLK_RTC]       = CLOCK_1M,
	[CLK_I2S0]      = CLOCK3_FREQ,
	[CLK_I2S1]      = CLOCK3_FREQ,
	[CLK_CODEC]     = CLOCK3_FREQ,
	[CLK_RGB]       = CLOCK_100M,
	[CLK_LVDS]      = CLOCK_100M,
	[CLK_MIPIDSI]   = CLOCK_100M,
	[CLK_DE]        = CLOCK_72M,
	[CLK_GE]       = CLOCK_72M,
	[CLK_VE]        = CLOCK_72M,
	[CLK_WDOG]      = CLOCK_1M,
	[CLK_SID]       = CLOCK_24M,
	[CLK_GTC]       = CLOCK_24M,
	[CLK_GPIO]      = CLOCK_24M,
#ifdef CONFIG_CLK_ARTINCHIP_V01
	[CLK_UART0]     = CLOCK_24M,
	[CLK_UART1]     = CLOCK_24M,
	[CLK_UART2]     = CLOCK_24M,
	[CLK_UART3]     = CLOCK_24M,
	[CLK_UART4]     = CLOCK_24M,
	[CLK_UART5]     = CLOCK_24M,
	[CLK_UART6]     = CLOCK_24M,
	[CLK_UART7]     = CLOCK_24M,
#else
	[CLK_UART0]     = CLOCK_60M,
	[CLK_UART1]     = CLOCK_60M,
	[CLK_UART2]     = CLOCK_60M,
	[CLK_UART3]     = CLOCK_60M,
	[CLK_UART4]     = CLOCK_60M,
	[CLK_UART5]     = CLOCK_60M,
	[CLK_UART6]     = CLOCK_60M,
	[CLK_UART7]     = CLOCK_60M,
#endif
	[CLK_TWI0]      = CLOCK_24M,
	[CLK_TWI1]      = CLOCK_24M,
	[CLK_TWI2]      = CLOCK_24M,
	[CLK_TWI3]      = CLOCK_24M,
	[CLK_CAN0]      = CLOCK_24M,
	[CLK_CAN1]      = CLOCK_24M,
	[CLK_PWM]       = CLOCK_24M,
	[CLK_ADCIM]     = CLOCK_24M,
	[CLK_GPAI]      = CLOCK_24M,
	[CLK_RTP]       = CLOCK_24M,
	[CLK_TSEN]      = CLOCK_24M,
	[CLK_CIR]       = CLOCK_24M,
	[CLK_OUT0]      = CLOCK_24M,
	[CLK_OUT1]      = CLOCK_24M,
	[CLK_OUT2]      = CLOCK_24M,
	[CLK_OUT3]      = CLOCK_24M,
};
#endif
