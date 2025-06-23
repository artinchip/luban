// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2024 ArtInChip Technology Co., Ltd.
 * Authors:  weihui.xu <weihui.xu@artinchip.com>
 */
#include <common.h>
#include <mapmem.h>
#include <clk-uclass.h>
#include <dm.h>
#include <dt-structs.h>
#include <errno.h>
#include <asm/io.h>
#include <dm/device_compat.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <dm/uclass-internal.h>
#include "clk-aic.h"
#include <dt-bindings/clock/artinchip,aic-cmu-v30.h>

/* clk-out */
#define CLK_OUT_REG(x)		((x) * 0x4 + 0x00E0)	/* 0 ~ 3 */
#define CLK_AXI_REG		(0x0100)
#define CLK_AHB_REG		(0x0110)
#define CLK_APB_REG		(0x0120)
#define CLK_CPU0_REG		(0x0180)
#define CLK_CPU1_REG		(0x0190)
#define RST_ENTRY_CPU0_REG	(0x0184)
#define RST_ENTRY_CPU1_REG	(0x0194)
#define CLK_PRE_DIV(x)		((x) * 0x4 + 0x01C0)	/* 0 ~ 7 */

#define CLK_WDOG_REG		(0x020C)
#define CLK_DDR_REG		(0x0210)
#define CLK_DDR_PHY_REG		(0x0214)
#define CLK_DISP0_SCLK_REG	(0x0220)
#define CLK_DISP0_PIX_REG	(0x0224)
#define CLK_DISP1_SCLK_REG	(0x0228)
#define CLK_DISP1_PIX_REG	(0x022C)
#define CLK_AUDIO_SCLK_REG	(0x0230)
#define CLK_DMA0_REG		(0x0400)
#define CLK_DMA1_REG		(0x0404)
#define CLK_DCE_REG		(0x0414)
#define CLK_CE_REG		(0x0418)
#define CLK_USB_DEV_REG		(0x041C)
#define CLK_USBH0_REG		(0x0420)
#define CLK_USBH1_REG		(0x0424)
#define CLK_USB_PHY0_REG	(0x0430)
#define CLK_USB_PHY1_REG	(0x0434)
#define CLK_USB_FS_DR_REG	(0x043C)
#define CLK_GMAC0_REG		(0x0440)
#define CLK_GMAC1_REG		(0x0444)
#define CLK_CANFD_REG		(0x0450)
#define CLK_XSPI_REG		(0x045C)
#define CLK_QSPI_REG(x)		((x) * 0x4 + 0x0460)	/* 0 ~ 3 */
#define CLK_SDMC_REG(x)		((x) * 0x4 + 0x0470)	/* 0 ~ 2 */
#define CLK_PBUS_REG		(0x04A0)
#define CLK_SYSCFG_REG		(0x0800)
#define CLK_SPIENC_REG		(0x0810)
#define CLK_PWMCS_REG		(0x0814)
#define CLK_MTOP_REG		(0x081C)
#define CLK_I2S_REG(x)		((x) * 0x4 + 0x0820)	/* 0 ~ 2 */
#define CLK_SAI_REG		(0x082C)
#define CLK_AUDIO_REG		(0x0830)
#define CLK_GPIO_REG		(0x083C)
#define CLK_UART_REG(x)		((x) * 0x4 + 0x0840)	/* 0 ~ 7 */
#define CLK_LCD0_REG		(0x0880)
#define CLK_LVDS0_REG		(0x0884)
#define CLK_MIPI_DSI_REG	(0x0888)
#define CLK_MIPI_DPHY0_REG	(0x088C)
#define CLK_DVP_REG		(0x0890)
#define CLK_MIPI_CSI_REG	(0x0898)
#define CLK_MIPI_DPHY1_REG	(0x089C)
#define CLK_LCD1_REG		(0x08A0)
#define CLK_LVDS1_REG		(0x08A4)
#define CLK_TVE_REG		(0x08B4)
#define CLK_DE_REG		(0x08C0)
#define CLK_GE_REG		(0x08C4)
#define CLK_VE_REG		(0x08C8)
#define CLK_SID_REG		(0x0904)

/* clk-core system */
#define CLK_I2C_REG(x)		((x) * 0x4 + 0x0960)	/* 0 ~ 2 */
#define CLK_XPWM_REG(x)		((x) * 0x4 + 0x0990)	/* 0 ~ 3 */
#define CLK_ADCIM_REG		(0x09A0)
#define CLK_GPAI_REG		(0x09A4)
#define CLK_RTP_REG		(0x09A8)
#define CLK_THS_REG		(0x09AC)
#define CLK_CIR_REG		(0x09B0)
#define CLK_CMP_REG		(0x09E4)
#define CLK_GPT_REG(x)		((x) * 0x4 + 0x0A00)	/* 0 ~ 5 */

/* clk-dpll */
#define CLK_DPLL_CFG0_REG(x)	((x) * 0x10 + 0x0E00)	/* 0 ~ 4 */
#define CLK_DPLL_CFG1_REG(x)	((x) * 0x10 + 0x0E04)	/* 0 ~ 4 */
#define CLK_DPLL_CFG2_REG(x)	((x) * 0x10 + 0x0E08)	/* 0 ~ 4 */
#define CLK_DPLL_STS_REG(x)	((x) * 0x10 + 0x0E0C)	/* 0 ~ 4 */

/* clk-pll_int */
#define CLK_PLL_INT_CFG_REG(x)	((x) * 0x10 + 0x0F00)	/* 0 ~ 1 */
#define CLK_PLL_INT_STS_REG(x)	((x) * 0x10 + 0x0F0C)	/* 0 ~ 1 */

/* clk-pll_fra */
#define CLK_PLL_FRA_GEN_REG(x)	((x) * 0x10 + 0x0F80)	/* 0 ~ 2 */
#define CLK_PLL_FRA_CFG_REG(x)	((x) * 0x10 + 0x0F84)	/* 0 ~ 2 */
#define CLK_PLL_FRA_SDM_REG(x)	((x) * 0x10 + 0x0F88)	/* 0 ~ 2 */
#define CLK_PLL_FRA_STS_REG(x)	((x) * 0x10 + 0x0F8C)	/* 0 ~ 2 */

#ifdef CONFIG_DEBUG_ON_FPGA_BOARD_ARTINCHIP
#define FPGA_MMCM2_CFG_REG	(0x0FE0)
#define FPGA_MMCM2_STS_REG	(0x0FE4)
#endif

#define PLL_COM_REG		(0x0FF0)
#define PLL_IN_REG		(0x0FF4)
#define CMU_AFE_VER_REG		(0x0FF8)
#define CMU_VER_REG		(0x0FFC)
#define CLK_NULL_REG		(0x0FFC)

/* Real-Time system clk */
#define CLK_R_AXI_REG		(0x0100)
#define CLK_R_AHB_REG		(0x0110)
#define CLK_R_CPU_REG		(0x0180)
#define CLK_R_PRE_DIV_REG	(0x01C0)
#define CLK_R_WDOG_REG		(0x020C)
#define CLK_R_DMA_REG		(0x0400)
#define CLK_R_CANFD0_REG	(0x0450)
#define CLK_R_CANFD1_REG	(0x0454)
#define CLK_R_SPI_REG		(0x0460)
#define CLK_R_GPIO_REG		(0x083C)
#define CLK_R_UART0_REG		(0x0840)
#define CLK_R_UART1_REG		(0x0844)
#define CLK_R_RTC_REG		(0x0908)
#define CLK_R_GTC_REG		(0x090C)
#define CLK_R_I2C0_REG		(0x0960)
#define CLK_R_I2C1_REG		(0x0964)
#define CLK_R_XPWM0_REG		(0x0990)
#define CLK_R_XPWM1_REG		(0x0994)
#define CLK_R_XPWM2_REG		(0x0998)
#define CLK_R_XPWM3_REG		(0x099C)
#define CLK_R_CIR_REG		(0x09B0)
#define CLK_R_DPLL_CFG0_REG	(0x0E00)
#define CLK_R_DPLL_CFG1_REG	(0x0E04)
#define CLK_R_DPLL_CFG2_REG	(0x0E08)
#define CLK_R_DPLL_STS_REG	(0x0E0C)
#define CLK_R_PLL_INT_CFG_REG	(0x0F00)
#define CLK_R_PLL_INT_STS_REG	(0x0FF4)

struct aic_clk_plat {
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	struct dtd_artinchip_aic_cmu_v3_0 dtplat;
#endif
};

static struct aic_fixed_rate clk_fixed_rates[] = {
	/*	 id           rate     */
	CLK_FIXED_RATE(CLK_24M,		24000000),
	CLK_FIXED_RATE(CLK_32K,		32768),
};

static struct aic_pll clk_plls[] = {
	CLK_PLL(CLK_PLL_FRA0, CLK_PLL_FRA_GEN_REG(0), CLK_PLL_FRA_CFG_REG(0),
		CLK_PLL_FRA_SDM_REG(0), AIC_PLL_FRA),
	CLK_PLL(CLK_PLL_FRA1, CLK_PLL_FRA_GEN_REG(1), CLK_PLL_FRA_CFG_REG(1),
		CLK_PLL_FRA_SDM_REG(1), AIC_PLL_FRA),
	CLK_PLL(CLK_PLL_FRA2, CLK_PLL_FRA_GEN_REG(2), CLK_PLL_FRA_CFG_REG(2),
		CLK_PLL_FRA_SDM_REG(2), AIC_PLL_FRA),
	CLK_PLL(CLK_PLL_FRA2, CLK_PLL_FRA_GEN_REG(2), CLK_PLL_FRA_CFG_REG(2),
		CLK_PLL_FRA_SDM_REG(2), AIC_PLL_FRA),

	CLK_PLL(CLK_PLL_INT0, CLK_PLL_INT_CFG_REG(0), 0, 0, AIC_PLL_INT),
	CLK_PLL(CLK_PLL_INT1, CLK_PLL_INT_CFG_REG(1), 0, 0, AIC_PLL_INT),
};

/* muti parent */
static u32 cpu_src_sels[] = {CLK_24M, CLK_PLL_FRA0, CLK_PLL_INT1};
static u32 aud_src_sels[] = {CLK_PLL_FRA1, CLK_PLL_INT1};
static u32 bus_src_sels[] = {CLK_24M, CLK_PLL_INT1};
CLK_CPU_ATTR(csys_cpu_attr, 0xac, 24, 8, 13, 12);

static struct aic_sys_clk clk_syss[] = {
	CLK_SYS_CPU(CLK_CPU0, CLK_CPU0_REG, cpu_src_sels,
		    ARRAY_SIZE(cpu_src_sels), 8, 1, 0, 4, &csys_cpu_attr),

	CLK_SYS_CPU(CLK_CPU1, CLK_CPU1_REG, cpu_src_sels,
		    ARRAY_SIZE(cpu_src_sels), 8, 1, 0, 4, &csys_cpu_attr),

	CLK_SYS_BUS(CLK_AHB, CLK_AHB_REG, bus_src_sels,
		    ARRAY_SIZE(bus_src_sels), 8, 1, 0, 5),

	CLK_SYS_BUS(CLK_AXI, CLK_AXI_REG, bus_src_sels,
		    ARRAY_SIZE(bus_src_sels), 8, 1, 0, 5),

	CLK_SYS_BUS(CLK_AUD_SCLK, CLK_AUDIO_SCLK_REG, aud_src_sels,
		    ARRAY_SIZE(aud_src_sels), 8, 1, 0, 5),
};

/* fixed parent */
static struct aic_periph_clk clk_periphs[] = {
	/*              id               parent           reg           */
	CLK_PERIPH(CLK_APB, CLK_AHB, CLK_APB_REG, -1, -1, 0, 3),

	CLK_PERIPH(CLK_DMA0, CLK_APB, CLK_DMA0_REG, 21, -1, 0, 0),
	CLK_PERIPH(CLK_DMA1, CLK_APB, CLK_DMA0_REG, 21, -1, 0, 0),
	CLK_PERIPH(CLK_CE, CLK_PLL_INT1, CLK_CE_REG, 22, 16, 0, 3),
	CLK_PERIPH(CLK_SYSCFG, CLK_24M, CLK_SYSCFG_REG, 20, -1, 0, 0),
#if !defined(CONFIG_SPL_BUILD)
	CLK_PERIPH(CLK_USBH0, CLK_AHB, CLK_USBH0_REG, 21, -1, 0, 0),
	CLK_PERIPH(CLK_USBH1, CLK_AHB, CLK_USBH1_REG, 21, -1, 0, 0),
	CLK_PERIPH(CLK_USB_PHY0, CLK_24M, CLK_USB_PHY0_REG, -1, 16, 0, 0),
	CLK_PERIPH(CLK_USB_PHY1, CLK_24M, CLK_USB_PHY1_REG, -1, 16, 0, 0),
#endif
	CLK_PERIPH(CLK_QSPI0, CLK_PLL_FRA0, CLK_QSPI_REG(0), 12, -1, 0, 3),
	CLK_PERIPH(CLK_QSPI1, CLK_PLL_FRA0, CLK_QSPI_REG(1), 12, -1, 0, 3),
#if !defined(CONFIG_SPL_BUILD)
	CLK_PERIPH(CLK_QSPI2, CLK_PLL_FRA0, CLK_QSPI_REG(2), 21, 16, 0, 3),
	CLK_PERIPH(CLK_QSPI3, CLK_PLL_FRA0, CLK_QSPI_REG(3), 21, 16, 0, 3),
#endif
	CLK_PERIPH(CLK_SDMC0, CLK_PLL_FRA0, CLK_SDMC_REG(0), 21, 16, 0, 3),
	CLK_PERIPH(CLK_SDMC1, CLK_PLL_FRA0, CLK_SDMC_REG(1), 21, 16, 0, 3),
	CLK_PERIPH(CLK_SDMC2, CLK_PLL_FRA0, CLK_SDMC_REG(2), 21, 16, 0, 3),
	CLK_PERIPH(CLK_SPIENC, CLK_AHB, CLK_SPIENC_REG, 21, -1, 0, 0),
#if !defined(CONFIG_SPL_BUILD)
	CLK_PERIPH(CLK_I2S0, CLK_AUD_SCLK, CLK_I2S_REG(0), 21, 16, 0, 5),
	CLK_PERIPH(CLK_I2S1, CLK_AUD_SCLK, CLK_I2S_REG(1), 21, 16, 0, 5),
	CLK_PERIPH(CLK_I2S2, CLK_AUD_SCLK, CLK_I2S_REG(2), 21, 16, 0, 5),
	CLK_PERIPH(CLK_AUDIO, CLK_AUD_SCLK, CLK_AUDIO_REG, 20, 16, 0, 5),
#endif
	CLK_PERIPH(CLK_LCD0, CLK_PLL_FRA2, CLK_LCD0_REG, 20, 16, 0, 0),
	CLK_PERIPH(CLK_LVDS0, CLK_PLL_FRA2, CLK_LVDS0_REG, 20, 16, 0, 0),
	CLK_PERIPH(CLK_DE, CLK_PLL_INT1, CLK_DE_REG, 20, 16, 0, 3),
	CLK_PERIPH(CLK_WDOG, CLK_24M, CLK_WDOG_REG, 20, 16, 0, 0),
	CLK_PERIPH(CLK_SID, CLK_24M, CLK_SID_REG, 20, 16, 0, 0),
	CLK_PERIPH(CLK_UART0, CLK_PLL_INT1, CLK_UART_REG(0), 20, 16, 0, 3),
	CLK_PERIPH(CLK_UART1, CLK_PLL_INT1, CLK_UART_REG(1), 20, 16, 0, 3),
	CLK_PERIPH(CLK_UART2, CLK_PLL_INT1, CLK_UART_REG(2), 20, 16, 0, 3),
	CLK_PERIPH(CLK_UART3, CLK_PLL_INT1, CLK_UART_REG(3), 20, 16, 0, 3),
	CLK_PERIPH(CLK_UART4, CLK_PLL_INT1, CLK_UART_REG(4), 20, 16, 0, 3),
	CLK_PERIPH(CLK_UART5, CLK_PLL_INT1, CLK_UART_REG(5), 20, 16, 0, 3),
	CLK_PERIPH(CLK_UART6, CLK_PLL_INT1, CLK_UART_REG(6), 20, 16, 0, 3),
	CLK_PERIPH(CLK_UART7, CLK_PLL_INT1, CLK_UART_REG(7), 20, 16, 0, 3),
#if !defined(CONFIG_SPL_BUILD)
	CLK_PERIPH(CLK_I2C0, CLK_APB, CLK_I2C_REG(0), 20, -1, 0, 0),
	CLK_PERIPH(CLK_I2C1, CLK_APB, CLK_I2C_REG(1), 20, -1, 0, 0),
	CLK_PERIPH(CLK_I2C2, CLK_APB, CLK_I2C_REG(2), 20, -1, 0, 0),
	CLK_PERIPH(CLK_ADCIM, CLK_PLL_INT1, CLK_ADCIM_REG, 20, 16, 0, 5),
	CLK_PERIPH(CLK_GPAI, CLK_APB, CLK_GPAI_REG, 20, -1, 0, 0),
	CLK_PERIPH(CLK_THS, CLK_APB, CLK_THS_REG, 20, -1, 0, 0),
#endif
};

static struct aic_disp_clk clk_disps[] = {
	CLK_DISP(CLK_SCLK0, CLK_PLL_FRA2, CLK_DISP0_SCLK_REG, 0, 6, 0, 0, 0, 0, 0, 0),
	CLK_DISP(CLK_PIX0, CLK_SCLK0, CLK_DISP0_PIX_REG, 0, 0, 0, 8, 0, 0, 12, 1),
	CLK_DISP(CLK_SCLK1, CLK_PLL_FRA2, CLK_DISP1_SCLK_REG, 0, 6, 0, 0, 0, 0, 0, 0),
	CLK_DISP(CLK_PIX1, CLK_SCLK1, CLK_DISP1_PIX_REG, 0, 0, 0, 8, 0, 0, 12, 1),
};

static u32 outclk_src_sels[] = {CLK_PLL_INT1, CLK_PLL_FRA1, CLK_PLL_FRA2};

static struct aic_clk_out clk_output[] = {
	/*      id                reg	     */
	AIC_CLK_OUT(CLK_OUT0, CLK_OUT_REG(0)),
	AIC_CLK_OUT(CLK_OUT1, CLK_OUT_REG(1)),
	AIC_CLK_OUT(CLK_OUT2, CLK_OUT_REG(2)),
	AIC_CLK_OUT(CLK_OUT3, CLK_OUT_REG(3)),
};

struct aic_clk_tree aic_clktree = {
	.fixed_rate_base = CLK_24M,
	.fixed_rate_cnt = ARRAY_SIZE(clk_fixed_rates),

	.pll_base = CLK_PLL_FRA0,
	.pll_cnt = ARRAY_SIZE(clk_plls),

	.system_base = CLK_CPU0,
	.system_cnt = ARRAY_SIZE(clk_syss),

	.periph_base = CLK_SYSCFG,
	.periph_cnt = ARRAY_SIZE(clk_periphs),

	.disp_base = CLK_PIX0,
	.disp_cnt = ARRAY_SIZE(clk_disps),

	.clkout_base = CLK_OUT0,
	.clkout_cnt = ARRAY_SIZE(clk_output),

	.fixed_rate = clk_fixed_rates,
	.plls = clk_plls,
	.system = clk_syss,
	.periph = clk_periphs,
	.disp = clk_disps,
	.clkout = clk_output,
};

#if CONFIG_IS_ENABLED(OF_PLATDATA)
static void aic_pll_init_platdata(struct udevice *dev)
{
	struct aic_clk_plat *plat = dev_get_plat(dev);
	struct dtd_artinchip_aic_cmu_v3_0 *dtplat = &plat->dtplat;
	struct aic_clk_priv *priv = dev_get_priv(dev);
	struct clk clk;

	priv->base = map_sysmem(dtplat->reg[0], dtplat->reg[1]);
	clk.dev = dev;

	clk.id = CLK_PLL_FRA0;
	clk_set_rate(&clk, dtplat->pll_frac0);

	clk.id = CLK_PLL_FRA1;
	clk_set_rate(&clk, dtplat->pll_frac1);

	clk.id = CLK_PLL_FRA2;
	clk_set_rate(&clk, dtplat->pll_frac2);

	clk.id = CLK_PLL_INT0;
	clk_set_rate(&clk, dtplat->pll_int0);

	clk.id = CLK_PLL_INT1;
	clk_set_rate(&clk, dtplat->pll_int1);
}
#else
static void aic_pll_init_ofdata(struct udevice *dev)
{
	struct aic_clk_priv *priv = dev_get_priv(dev);
	int ret;
	u32 value, freq;
	struct clk clk;

	priv->base = dev_read_addr_ptr(dev);

	ret = dev_read_u32(dev, "pll-frac0", &value);
	if (!ret) {
		clk.id = CLK_PLL_FRA0;
		clk.dev = dev;
		freq = clk_get_rate(&clk);
		if (freq != value)
			clk_set_rate(&clk, value);
		clk_enable(&clk);
	}

	ret = dev_read_u32(dev, "pll-frac1", &value);
	if (!ret) {
		clk.id = CLK_PLL_FRA1;
		clk.dev = dev;
		freq = clk_get_rate(&clk);
		if (freq != value)
			clk_set_rate(&clk, value);
		clk_enable(&clk);
	}

	ret = dev_read_u32(dev, "pll-frac2", &value);
	if (!ret) {
		clk.id = CLK_PLL_FRA2;
		clk.dev = dev;
		freq = clk_get_rate(&clk);
		if (freq != value)
			clk_set_rate(&clk, value);
		clk_enable(&clk);
	}

	ret = dev_read_u32(dev, "pll-int0", &value);
	if (!ret) {
		clk.id = CLK_PLL_INT0;
		clk.dev = dev;
		freq = clk_get_rate(&clk);
		if (freq != value)
			clk_set_rate(&clk, value);
		clk_enable(&clk);
	}

	ret = dev_read_u32(dev, "pll-int1", &value);
	if (!ret) {
		clk.id = CLK_PLL_INT1;
		clk.dev = dev;
		freq = clk_get_rate(&clk);
		if (freq != value)
			clk_set_rate(&clk, value);
		clk_enable(&clk);
	}
}

static void aic_system_clock_init(struct udevice *dev)
{
	int ret;
	u32 value, freq;
	struct clk clk, pclk;

	/* Set ahb bus clock */
	ret = dev_read_u32(dev, "ahb", &value);
	if (!ret) {
		pclk.dev = dev;
		clk.id = CLK_AHB;
		clk.dev = dev;
		if (value == 24000000) {
			pclk.id = CLK_24M;
		} else {
			pclk.id = CLK_PLL_INT1;
			freq = clk_get_rate(&clk);
			if (freq != value)
				clk_set_rate(&clk, value);
		}
		clk_set_parent(&clk, &pclk);
	}

	/* Set apb bus clock */
	ret = dev_read_u32(dev, "apb", &value);
	if (!ret) {
		pclk.dev = dev;
		clk.id = CLK_APB;
		clk.dev = dev;
		if (value == 24000000) {
			pclk.id = CLK_24M;
		} else {
			pclk.id = CLK_AHB;
			freq = clk_get_rate(&clk);
			if (freq != value)
				clk_set_rate(&clk, value);
		}
		clk_set_parent(&clk, &pclk);
	}

		/* Set apb bus clock */
	ret = dev_read_u32(dev, "axi", &value);
	if (!ret) {
		pclk.dev = dev;
		clk.id = CLK_APB;
		clk.dev = dev;
		if (value == 24000000) {
			pclk.id = CLK_24M;
		} else {
			pclk.id = CLK_PLL_INT1;
			freq = clk_get_rate(&clk);
			if (freq != value)
				clk_set_rate(&clk, value);
		}
		clk_set_parent(&clk, &pclk);
	}

	/* Set CPU parent */
	pclk.dev = dev;
	pclk.id = CLK_PLL_INT1;
	clk.id = CLK_CPU0;
	clk.dev = dev;
	clk_set_parent(&clk, &pclk);

}
#endif

static int aic_clk_probe(struct udevice *dev)
{
	aic_clk_common_init(dev, &aic_clktree);
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	aic_pll_init_platdata(dev);
#else
	aic_pll_init_ofdata(dev);
#endif
	aic_system_clock_init(dev);

	dev_info(dev, "%s done\n", __func__);
	return 0;
}

static const struct udevice_id aic_clk_ids[] = {
	{ .compatible = "artinchip,aic-cmu-v3.0", },
	{ }
};

U_BOOT_DRIVER(artinchip_cmu) = {
	.name		= "artinchip_aic_cmu_v3_0",
	.id		= UCLASS_CLK,
	.of_match	= aic_clk_ids,
	.probe		= aic_clk_probe,
	.priv_auto	= sizeof(struct aic_clk_priv),
	.plat_auto	= sizeof(struct aic_clk_plat),
	.ops		= &artinchip_clk_ops,
};

