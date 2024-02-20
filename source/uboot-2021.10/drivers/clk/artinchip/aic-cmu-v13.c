// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2020 ArtInChip Inc.
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
#include <dt-bindings/clock/artinchip,aic-cmu-v13.h>

/* clk-pll_fra */
#define CLK_PLL_FRA_GEN_REG(x)	((x) * 0x4 + 0x0020)
#define CLK_PLL_FRA_CFG_REG(x)	((x) * 0x4 + 0x0060)
#define CLK_PLL_FRA_SDM_REG(x)	((x) * 0x4 + 0x0080)
#define CLK_PLL_COM_REG		(0x00A0)
/* clk-out */
#define CLK_OUT_REG(x)		((x) * 0x4 + 0x00E0)
/* clk-bus */
#define CLK_APB0_REG		(0x0120)
#define CLK_APB2_REG		(0x0128)
/* clk-security subsys */
#define CLK_SESS_CPU_REG	(0x0180)
#define CLK_SE_WDOG_REG		(0x018C)
#define CLK_SE_DMA_REG		(0x0190)
#define CLK_SE_CE_REG		(0x0194)
#define CLK_SE_SPI_REG		(0x0198)
#define CLK_SE_SID_REG		(0x01A0)
#define CLK_SE_I2C_REG		(0x01A4)
#define CLK_SE_GPIO_REG		(0x01A8)
#define CLK_SE_SPIENC_REG	(0x01AC)
/* clk-scanner subsys */
#define CLK_SCSS_CPU_REG	(0x01F0)
#define CLK_SC_WDOG_REG		(0x01FC)
/* clk-core system */
#define CLK_CORE_CPU_REG	(0x0200)
#define CLK_CORE_WDOG_REG	(0x020C)
#define CLK_DDR_REG		(0x0210)
#define CLK_DISP_REG		(0x0220)
#define CLK_DMA0_REG		(0x0410)
#define CLK_DMA1_REG		(0x0414)
#define CLK_DMA2_REG		(0x0418)
#define CLK_DCE_REG		(0x041C)
#define CLK_USBH0_REG		(0x0420)
#define CLK_USBH1_REG		(0x0424)
#define CLK_USB_PHY0_REG	(0x0430)
#define CLK_USB_PHY1_REG	(0x0434)
#define CLK_SPI0_REG		(0x045C)
#define CLK_SPI1_REG		(0x0460)
#define CLK_SPI2_REG		(0x0464)
#define CLK_SPI3_REG		(0x0468)
#define CLK_SPI4_REG		(0x046C)
#define CLK_SDMC0_REG		(0x0470)
#define CLK_SDMC1_REG		(0x0474)
#define CLK_SYSCFG_REG		(0x0800)
#define CLK_SPIENC_REG		(0x0810)
#define CLK_MTOP_REG		(0x081C)
#define CLK_I2S0_REG		(0x0820)
#define CLK_AUDIO_REG		(0x0830)
#define CLK_UART0_REG		(0x0840)
#define CLK_UART1_REG		(0x0844)
#define CLK_UART2_REG		(0x0848)
#define CLK_UART3_REG		(0x084C)
#define CLK_UART4_REG		(0x0850)
#define CLK_UART5_REG		(0x0854)
#define CLK_LCD_REG		(0x0880)
#define CLK_LVDS_REG		(0x0884)
#define CLK_DVP_REG		(0x0890)
#define CLK_DE_REG		(0x08C0)
#define CLK_SID_REG		(0x0904)
#define CLK_I2C0_REG		(0x0960)
#define CLK_I2C1_REG		(0x0964)
#define CLK_I2C2_REG		(0x0968)
#define CLK_I2C3_REG		(0x096C)
#define CLK_I2C4_REG		(0x0970)
#define CLK_ADCIM_REG		(0x09A0)
#define CLK_GPAI_REG		(0x09A4)
#define CLK_THS_REG		(0x09AC)
#define CLK_SCAN_REG		(0x0C00)
#define CLK_PRINT_REG		(0x0C08)
#define CLK_IMG_ROT_REG		(0x0C10)
#define CLK_IMG_HT_REG		(0x0C14)
#define CLK_IMG_CRS0_REG	(0x0C18)
#define CLK_IMG_CRS1_REG	(0x0C1C)
#define CLK_IMG_PP0_REG		(0x0C20)
#define CLK_IMG_PP1_REG		(0x0C24)
#define CLK_IMG_EHC0_REG	(0x0C28)
#define CLK_IMG_EHC1_REG	(0x0C2C)
#define CLK_USM_SCALE_REG	(0x0C30)
#define CLK_IMG_JBIG0_REG	(0x0C38)
#define CLK_IMG_JBIG1_REG	(0x0C3C)
#define CLK_IMG_JPEG0_REG	(0x0C40)
#define CLK_IMG_JPEG1_REG	(0x0C44)
#define CLK_XPWM_SDFM_REG	(0x0240)
#define CLK_PWM_SDFM_REG 	(0x0244)
#define CLK_CAP_SDFM_REG	(0x0248)
#define CLK_XPWM_REG(x)		((x) * 0x4 + 0x0B00) /* 0 ~ 7 */
#define CLK_PWM_REG(x)		((x) * 0x4 + 0x0B40) /* 0 ~ 15 */
#define CLK_CAP_REG(x)		((x) * 0x4 + 0x0B80) /* 0 ~ 4 */
#define CLK_GPT_REG(x)		((x) * 0x4 + 0x0BC0) /* 0 ~ 9 */
#define CLK_QSPI_REG(x)		((x) * 0x4 + 0x0460) /* 0 ~ 4 */
#define CLK_NULL_REG		(0x0FFC)
#define CLK_VER_REG		(0x0FFC)
/* clk-spss system */
#define SP_PLL_FRA_GEN_REG(x)	((x) * 0x4 + 0x0000)
#define SP_PLL_FRA_CFG_REG(x)	((x) * 0x4 + 0x0008)
#define SP_PLL_FRA_SDM_REG(x)	((x) * 0x4 + 0x0010)
#define SP_PLL_COM_REG		(0x0018)
#define SP_PLL_IN_REG		(0x001C)
#define CLK_SP_OUT_REG		(0x0020)
#define CLK_SP_AHB_REG		(0x0030)
#define CLK_SP_APB0_REG		(0x0034)
#define CLK_SP_CPU_REG		(0x0038)
#define CLK_SP_WDT_REG		(0x0040)
#define CLK_SP_USBD_REG		(0x0044)
#define CLK_SP_USB_PHY0_REG	(0x0048)
#define CLK_SP_GMAC_REG		(0x004C)
#define CLK_SP_GPIO_REG		(0x0050)
#define CLK_SP_UART0_REG	(0x0054)
#define CLK_SP_UART1_REG	(0x0058)
#define CLK_SP_RTC_REG		(0x005C)
#define CLK_SP_GTC_REG		(0x0060)
#define CLK_SP_I2C_REG		(0x0064)
#define CLK_SP_GMAC_CFG_REG	(0x0084)
#define PM_VDD11		(0x0070)
#define PM_USBD_REXT		(0x0080)
#define PM_BAK			(0x0080)


struct aic_clk_plat {
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	struct dtd_artinchip_aic_cmu_v1_0 dtplat;
#endif
};

static struct aic_fixed_rate clk_fixed_rates[] = {
	/*                 id           rate     */
	CLK_FIXED_RATE(CLK_24M,		24000000),
	CLK_FIXED_RATE(CLK_32K,		32768),
	CLK_FIXED_RATE(CLK_AXI_AHB,	24000000),
};

static struct aic_pll clk_plls[] = {
	CLK_PLL(CLK_PLL_FRA0, CLK_PLL_FRA_GEN_REG(0), CLK_PLL_FRA_CFG_REG(0),
		CLK_PLL_FRA_SDM_REG(0), AIC_PLL_FRA),
	CLK_PLL(CLK_PLL_FRA1, CLK_PLL_FRA_GEN_REG(1), CLK_PLL_FRA_CFG_REG(1),
		CLK_PLL_FRA_SDM_REG(1), AIC_PLL_FRA),
	CLK_PLL(CLK_PLL_FRA2, CLK_PLL_FRA_GEN_REG(2), CLK_PLL_FRA_CFG_REG(2),
		CLK_PLL_FRA_SDM_REG(2), AIC_PLL_FRA),
	CLK_PLL(CLK_PLL_FRA3, CLK_PLL_FRA_GEN_REG(3), CLK_PLL_FRA_CFG_REG(3),
		CLK_PLL_FRA_SDM_REG(3), AIC_PLL_FRA),
	CLK_PLL_VIDEO(CLK_PLL_FRA4, CLK_PLL_FRA_GEN_REG(4), CLK_PLL_FRA_CFG_REG(4),
		      CLK_PLL_FRA_SDM_REG(4), AIC_PLL_SDM, 360000000, 1200000000),
	CLK_PLL(CLK_PLL_FRA5, CLK_PLL_FRA_GEN_REG(5), CLK_PLL_FRA_CFG_REG(5),
		CLK_PLL_FRA_SDM_REG(5), AIC_PLL_FRA),
	CLK_PLL(CLK_PLL_FRA6, CLK_PLL_FRA_GEN_REG(6), CLK_PLL_FRA_CFG_REG(6),
		CLK_PLL_FRA_SDM_REG(6), AIC_PLL_FRA),
	CLK_PLL(CLK_PLL_FRA7, CLK_PLL_FRA_GEN_REG(7), CLK_PLL_FRA_CFG_REG(7),
		CLK_PLL_FRA_SDM_REG(7), AIC_PLL_FRA),
};

/* muti parent */
static u32 cpu_src_sels[] = {CLK_24M, CLK_PLL_FRA0};
static u32 bus_src_sels[] = {CLK_24M, CLK_PLL_FRA1};
CLK_CPU_ATTR(csys_cpu_attr, 0xac, 24, 8, 13, 12);

static struct aic_sys_clk clk_syss[] = {
	CLK_SYS_CPU(CLK_CORE_CPU, CLK_CORE_CPU_REG, cpu_src_sels,
		    ARRAY_SIZE(cpu_src_sels), 8, 1, 0, 4, &csys_cpu_attr),
	CLK_SYS_BUS(CLK_APB0, CLK_APB0_REG, bus_src_sels,
		    ARRAY_SIZE(bus_src_sels), 8, 1, 0, 5),
	CLK_SYS_BUS(CLK_APB2, CLK_APB2_REG, bus_src_sels,
		    ARRAY_SIZE(bus_src_sels), 8, 1, 0, 5),
};

static struct aic_periph_clk clk_periphs[] = {
	/*              id               parent           reg           */
	CLK_PERIPH(CLK_DMA0, CLK_24M, CLK_DMA0_REG, 12, 0, 0, 0),
	CLK_PERIPH(CLK_SE_CE, CLK_PLL_FRA1, CLK_SE_CE_REG, 12, 0, 0, 0),
	CLK_PERIPH(CLK_SYSCFG, CLK_24M, CLK_SYSCFG_REG, 12, 0, 0, 0),
#if !defined(CONFIG_SPL_BUILD)
	CLK_PERIPH(CLK_USBH0, CLK_24M, CLK_USBH0_REG, 12, 0, 0, 0),
	CLK_PERIPH(CLK_USBH1, CLK_24M, CLK_USBH1_REG, 12, 0, 0, 0),
	CLK_PERIPH(CLK_USB_PHY0, CLK_24M, CLK_USB_PHY0_REG, 0, 8, 0, 0),
	CLK_PERIPH(CLK_USB_PHY1, CLK_24M, CLK_USB_PHY1_REG, 0, 8, 0, 0),
#endif
	CLK_PERIPH(CLK_SPI0, CLK_PLL_FRA2, CLK_SPI0_REG, 12, 8, 0, 4),
	CLK_PERIPH(CLK_SPI1, CLK_PLL_FRA2, CLK_SPI1_REG, 12, 8, 0, 4),
#if !defined(CONFIG_SPL_BUILD)
	CLK_PERIPH(CLK_SPI2, CLK_PLL_FRA2, CLK_SPI2_REG, 12, 8, 0, 4),
	CLK_PERIPH(CLK_SPI3, CLK_PLL_FRA2, CLK_SPI3_REG, 12, 8, 0, 4),
	CLK_PERIPH(CLK_SPI4, CLK_PLL_FRA2, CLK_SPI4_REG, 12, 8, 0, 4),
#endif
	CLK_PERIPH(CLK_SDMC0, CLK_PLL_FRA2, CLK_SDMC0_REG, 12, 8, 0, 5),
	CLK_PERIPH(CLK_SDMC1, CLK_PLL_FRA2, CLK_SDMC1_REG, 12, 8, 0, 5),
	CLK_PERIPH(CLK_SPIENC, CLK_AXI_AHB, CLK_SPIENC_REG, 12, 8, 0, 0),
#if !defined(CONFIG_SPL_BUILD)
	CLK_PERIPH(CLK_I2S0, CLK_PLL_FRA3, CLK_I2S0_REG, 12, 8, 0, 4),
	CLK_PERIPH(CLK_AUDIO, CLK_PLL_FRA3, CLK_AUDIO_REG, 12, 8, 0, 4),
#endif
	CLK_PERIPH(CLK_LCD, CLK_SCLK, CLK_LCD_REG, 12, 8, 0, 0),
	CLK_PERIPH(CLK_LVDS, CLK_SCLK, CLK_LVDS_REG, 12, 8, 0, 0),
	CLK_PERIPH(CLK_DE, CLK_PLL_FRA1, CLK_DE_REG, 12, 8, 0, 4),
	CLK_PERIPH(CLK_CORE_WDOG, CLK_32K, CLK_CORE_WDOG_REG, 12, 8, 0, 0),
	CLK_PERIPH(CLK_SID, CLK_24M, CLK_SID_REG, 12, 8, 0, 0),
	CLK_PERIPH(CLK_UART0, CLK_PLL_FRA1, CLK_UART0_REG, 12, 8, 0, 4),
	CLK_PERIPH(CLK_UART1, CLK_PLL_FRA1, CLK_UART1_REG, 12, 8, 0, 4),
	CLK_PERIPH(CLK_UART2, CLK_PLL_FRA1, CLK_UART2_REG, 12, 8, 0, 4),
	CLK_PERIPH(CLK_UART3, CLK_PLL_FRA1, CLK_UART3_REG, 12, 8, 0, 4),
	CLK_PERIPH(CLK_UART4, CLK_PLL_FRA1, CLK_UART4_REG, 12, 8, 0, 4),
	CLK_PERIPH(CLK_UART5, CLK_PLL_FRA1, CLK_UART5_REG, 12, 8, 0, 4),
#if !defined(CONFIG_SPL_BUILD)
	CLK_PERIPH(CLK_I2C0, CLK_24M, CLK_I2C0_REG, 12, -1, 0, 0),
	CLK_PERIPH(CLK_I2C1, CLK_24M, CLK_I2C1_REG, 12, -1, 0, 0),
	CLK_PERIPH(CLK_I2C2, CLK_24M, CLK_I2C2_REG, 12, -1, 0, 0),
	CLK_PERIPH(CLK_I2C3, CLK_24M, CLK_I2C3_REG, 12, -1, 0, 0),
	CLK_PERIPH(CLK_I2C4, CLK_24M, CLK_I2C4_REG, 12, -1, 0, 0),
	CLK_PERIPH(CLK_ADCIM, CLK_APB2, CLK_ADCIM_REG, 12, -1, 0, 0),
	CLK_PERIPH(CLK_GPAI, CLK_APB2, CLK_GPAI_REG, 12, -1, 0, 0),
	CLK_PERIPH(CLK_THS, CLK_APB2, CLK_THS_REG, 12, -1, 0, 0),
#endif
};

static struct aic_disp_clk clk_disps[] = {
	CLK_DISP(CLK_SCLK, CLK_PLL_FRA2, CLK_DISP_REG, 0, 3, 0, 0, 0, 0, 0, 0),
	CLK_DISP(CLK_PIX, CLK_SCLK, CLK_DISP_REG, 0, 0, 4, 5, 10, 2, 12, 2),
};

static u32 outclk_src_sels[] = {CLK_PLL_FRA1, CLK_PLL_FRA2,
				CLK_PLL_FRA3, CLK_PLL_FRA4};

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

	.system_base = CLK_CORE_CPU,
	.system_cnt = ARRAY_SIZE(clk_syss),

	.periph_base = CLK_SYSCFG,
	.periph_cnt = ARRAY_SIZE(clk_periphs),

	.disp_base = CLK_PIX,
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
	struct dtd_artinchip_aic_cmu_v1_3 *dtplat = &plat->dtplat;
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

	clk.id = CLK_PLL_FRA3;
	clk_set_rate(&clk, dtplat->pll_frac3);

	clk.id = CLK_PLL_FRA4;
	clk_set_rate(&clk, dtplat->pll_frac4);

	clk.id = CLK_PLL_FRA5;
	clk_set_rate(&clk, dtplat->pll_frac5);

	clk.id = CLK_PLL_FRA6;
	clk_set_rate(&clk, dtplat->pll_frac6);
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

	ret = dev_read_u32(dev, "pll-frac3", &value);
	if (!ret) {
		clk.id = CLK_PLL_FRA3;
		clk.dev = dev;
		freq = clk_get_rate(&clk);
		if (freq != value)
			clk_set_rate(&clk, value);
		clk_enable(&clk);
	}

	ret = dev_read_u32(dev, "pll-frac4", &value);
	if (!ret) {
		clk.id = CLK_PLL_FRA4;
		clk.dev = dev;
		freq = clk_get_rate(&clk);
		if (freq != value)
			clk_set_rate(&clk, value);
		clk_enable(&clk);
	}

	ret = dev_read_u32(dev, "pll-frac5", &value);
	if (!ret) {
		clk.id = CLK_PLL_FRA5;
		clk.dev = dev;
		freq = clk_get_rate(&clk);
		if (freq != value)
			clk_set_rate(&clk, value);
		clk_enable(&clk);
	}

	ret = dev_read_u32(dev, "pll-frac6", &value);
	if (!ret) {
		clk.id = CLK_PLL_FRA6;
		clk.dev = dev;
		freq = clk_get_rate(&clk);
		if (freq != value)
			clk_set_rate(&clk, value);
		clk_enable(&clk);
	}

	ret = dev_read_u32(dev, "pll-frac7", &value);
	if (!ret) {
		clk.id = CLK_PLL_FRA7;
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
	/* Set apb0 bus clock */
	ret = dev_read_u32(dev, "apb0", &value);
	if (!ret) {
		pclk.dev = dev;
		clk.id = CLK_APB0;
		clk.dev = dev;
		if (value == 24000000) {
			pclk.id = CLK_24M;
		} else {
			pclk.id = CLK_PLL_FRA1;
			freq = clk_get_rate(&clk);
			if (freq != value)
				clk_set_rate(&clk, value);
		}
		clk_set_parent(&clk, &pclk);
	}

	/* Set apb2 bus clock */
	ret = dev_read_u32(dev, "apb2", &value);
	if (!ret) {
		pclk.dev = dev;
		clk.id = CLK_APB2;
		clk.dev = dev;
		if (value == 24000000) {
			pclk.id = CLK_24M;
		} else {
			pclk.id = CLK_PLL_FRA1;
			freq = clk_get_rate(&clk);
			if (freq != value)
				clk_set_rate(&clk, value);
		}
		clk_set_parent(&clk, &pclk);
	}

	/* Set CPU parent */
	pclk.dev = dev;
	pclk.id = CLK_PLL_FRA0;
	clk.id = CLK_CORE_CPU;
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
	{ .compatible = "artinchip,aic-cmu-v1.3", },
	{ }
};

U_BOOT_DRIVER(artinchip_cmu) = {
	.name                     = "artinchip_aic_cmu_v1_3",
	.id                       = UCLASS_CLK,
	.of_match                 = aic_clk_ids,
	.probe                    = aic_clk_probe,
	.priv_auto                = sizeof(struct aic_clk_priv),
	.plat_auto                = sizeof(struct aic_clk_plat),
	.ops                      = &artinchip_clk_ops,
};

