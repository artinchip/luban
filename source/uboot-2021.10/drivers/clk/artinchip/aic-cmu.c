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
#include <dt-bindings/clock/artinchip,aic-cmu.h>

#define PLL_INT0_GEN_REG (0x0000)
#define PLL_INT1_GEN_REG (0x0004)
#define PLL_FRA0_GEN_REG (0x0020)
#define PLL_FRA1_GEN_REG (0x0024)
#define PLL_FRA2_GEN_REG (0x0028)
#define PLL_INT0_CFG_REG (0x0040)
#define PLL_INT1_CFG_REG (0x0044)
#define PLL_FRA0_CFG_REG (0x0060)
#define PLL_FRA1_CFG_REG (0x0064)
#define PLL_FRA2_CFG_REG (0x0068)
#define PLL_FRA0_SDM_REG (0x0080)
#define PLL_FRA1_SDM_REG (0x0084)
#define PLL_FRA2_SDM_REG (0x0088)
#define PLL_COM_REG      (0x00A0)
#define PLL_IN_REG       (0x00A4)
#define CLK_OUT0_REG     (0x00E0)
#define CLK_OUT1_REG     (0x00E4)
#define CLK_OUT2_REG     (0x00E8)
#define CLK_OUT3_REG     (0x00EC)
#define CLK_AXI0_REG     (0x0100)
#define CLK_AHB0_REG     (0x0110)
#define CLK_APB0_REG     (0x0120)
#define CLK_APB1_REG     (0x0124)
#define CLK_CPU_REG      (0x0200)
#define CLK_DM_REG	 (0x0204)
#define CLK_DISP_REG	 (0x0220)
#define CLK_DMA_REG      (0x0410)
#define CLK_CE_REG       (0x0418)
#define CLK_USBD_REG     (0x041C)
#define CLK_USBH0_REG    (0x0420)
#define CLK_USBH1_REG    (0x0424)
#define CLK_USBPHY0_REG  (0x0430)
#define CLK_USBPHY1_REG  (0x0434)
#define CLK_GMAC0_REG    (0x0440)
#define CLK_GMAC1_REG    (0x0444)
#define CLK_SPI0_REG     (0x0460)
#define CLK_SPI1_REG     (0x0464)
#define CLK_SPI2_REG	 (0x0468)
#define CLK_SPI3_REG	 (0x046C)
#define CLK_SDMC0_REG    (0x0470)
#define CLK_SDMC1_REG    (0x0474)
#define CLK_SDMC2_REG    (0x0478)
#define CLK_PBUS_REG     (0x04A0)
#define CLK_SYSCFG_REG   (0x0800)
#ifdef CONFIG_CLK_ARTINCHIP_V0_1
#define CLK_RTC_REG      (0x0804)
#else
#define CLK_RTC_REG      (0x0908)
#endif
#define CLK_SPIENC_REG   (0x0810)
#define CLK_PWMCS_REG	 (0x0814)
#define CLK_PSADC_REG	 (0x0818)
#define CLK_MTOP_REG	 (0x081C)
#define CLK_I2S0_REG     (0x0820)
#define CLK_I2S1_REG     (0x0824)
#define CLK_CODEC_REG    (0x0830)
#define CLK_RGB_REG      (0x0880)
#define CLK_LVDS_REG     (0x0884)
#define CLK_MIPIDSI_REG  (0x0888)
#define CLK_DE_REG       (0x08c0)
#define CLK_GE_REG       (0x08c4)
#define CLK_VE_REG       (0x08c8)
#ifdef CONFIG_CLK_ARTINCHIP_V0_1
#define CLK_WDOG_REG     (0x0900)
#else
#define CLK_WDOG_REG     (0x020C)
#endif
#define CLK_SID_REG      (0x0904)
#ifdef CONFIG_CLK_ARTINCHIP_V0_1
#define CLK_GTC_REG      (0x0908)
#define CLK_GPIO_REG     (0x091C)
#define CLK_UART0_REG    (0x0920)
#define CLK_UART1_REG    (0x0924)
#define CLK_UART2_REG    (0x0928)
#define CLK_UART3_REG    (0x092C)
#define CLK_UART4_REG    (0x0930)
#define CLK_UART5_REG    (0x0934)
#define CLK_UART6_REG    (0x0938)
#define CLK_UART7_REG    (0x093C)
#else
#define CLK_GTC_REG      (0x090C)
#define CLK_GPIO_REG     (0x083C)
#define CLK_UART0_REG    (0x0840)
#define CLK_UART1_REG    (0x0844)
#define CLK_UART2_REG    (0x0848)
#define CLK_UART3_REG    (0x084C)
#define CLK_UART4_REG    (0x0850)
#define CLK_UART5_REG    (0x0854)
#define CLK_UART6_REG    (0x0858)
#define CLK_UART7_REG    (0x085C)
#endif
#define CLK_I2C0_REG     (0x0960)
#define CLK_I2C1_REG     (0x0964)
#define CLK_I2C2_REG     (0x0968)
#define CLK_I2C3_REG     (0x096C)
#define CLK_CAN0_REG     (0x0980)
#define CLK_CAN1_REG     (0x0984)
#define CLK_PWM_REG      (0x0990)
#define CLK_ADCIM_REG    (0x09A0)
#define CLK_GPAI_REG     (0x09A4)
#define CLK_RTP_REG      (0x09A8)
#define CLK_TSEN_REG     (0x09AC)
#define CLK_CIR_REG     (0x09B0)

struct aic_clk_plat {
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	struct dtd_artinchip_aic_cmu_v1_0 dtplat;
#endif
};

static struct aic_fixed_rate clk_fixed_rates[] = {
	/*                 id           rate     */
	CLK_FIXED_RATE(CLK_DUMMY,       0),
	CLK_FIXED_RATE(CLK_OSC24M,      24000000),
	CLK_FIXED_RATE(CLK_OSC32K,      32768),
	CLK_FIXED_RATE(CLK_RC1M,        1000000),
};

static struct aic_pll clk_plls[] = {
	CLK_PLL(CLK_PLL_INT0, PLL_INT0_GEN_REG, PLL_INT0_CFG_REG,
		0, AIC_PLL_INT),
	CLK_PLL(CLK_PLL_INT1, PLL_INT1_GEN_REG, PLL_INT1_CFG_REG,
		0, AIC_PLL_INT),
	CLK_PLL(CLK_PLL_FRA0, PLL_FRA0_GEN_REG, PLL_FRA0_CFG_REG,
		PLL_FRA0_SDM_REG, AIC_PLL_SDM),
	CLK_PLL(CLK_PLL_FRA1, PLL_FRA1_GEN_REG, PLL_FRA1_CFG_REG,
		PLL_FRA1_SDM_REG, AIC_PLL_FRA),
	CLK_PLL_VIDEO(CLK_PLL_FRA2, PLL_FRA2_GEN_REG, PLL_FRA2_CFG_REG,
		PLL_FRA2_SDM_REG, AIC_PLL_SDM, 360000000, 1200000000),
};


static u32 axi0_src_sels[] = {CLK_OSC24M, CLK_PLL_INT1};
static u32 ahb0_src_sels[] = {CLK_OSC24M, CLK_PLL_INT1};
static u32 apb0_src_sels[] = {CLK_OSC24M, CLK_PLL_INT1};
#ifdef CONFIG_CLK_ARTINCHIP_V0_1
static u32 apb1_src_sels[] = {CLK_OSC24M, CLK_PLL_INT1};
#else
static u32 apb1_src_sels[] = {CLK_OSC24M};
#endif

#ifdef CONFIG_CLK_ARTINCHIP_V0_1
static u32 cpu_src_sels[] = {CLK_OSC24M, CLK_PLL_INT0, CLK_DUMMY, CLK_OSC32K};
#else
static u32 cpu_src_sels[] = {CLK_OSC24M, CLK_PLL_INT0};
#endif

static struct aic_sys_clk clk_syss[] = {
	CLK_SYS_BUS(CLK_AXI0, CLK_AXI0_REG, axi0_src_sels,
		ARRAY_SIZE(axi0_src_sels), 8, 1, 0, 5),
	CLK_SYS_BUS(CLK_AHB0, CLK_AHB0_REG, ahb0_src_sels,
		ARRAY_SIZE(ahb0_src_sels), 8, 1, 0, 5),
	CLK_SYS_BUS(CLK_APB0, CLK_APB0_REG, apb0_src_sels,
		ARRAY_SIZE(apb0_src_sels), 8, 1, 0, 5),
#ifdef CONFIG_CLK_ARTINCHIP_V0_1
	CLK_SYS_BUS(CLK_APB1, CLK_APB1_REG, apb1_src_sels,
		ARRAY_SIZE(apb1_src_sels), 8, 1, 0, 5),
	CLK_SYS_BUS(CLK_CPU, CLK_CPU_REG, cpu_src_sels,
		ARRAY_SIZE(cpu_src_sels), 8, 2, 0, 5),

#else
	CLK_SYS_BUS(CLK_APB1, CLK_APB1_REG, apb1_src_sels,
		ARRAY_SIZE(apb1_src_sels), 8, 1, 0, 0),
	CLK_SYS_BUS(CLK_CPU, CLK_CPU_REG, cpu_src_sels,
		ARRAY_SIZE(cpu_src_sels), 8, 1, 0, 5),
#endif
};

static struct aic_periph_clk clk_periphs[] = {
	/*                      id	        parent          reg           */
	AIC_CLK_PERIPH(CLK_DMA,             CLK_AHB0,       CLK_DMA_REG),
	AIC_CLK_PERIPH(CLK_CE,              CLK_PLL_INT1,   CLK_CE_REG),
#if !defined(CONFIG_SPL_BUILD)
	AIC_CLK_PERIPH(CLK_USBD,            CLK_AHB0,     CLK_USBD_REG),
	AIC_CLK_PERIPH(CLK_USBH0,           CLK_AHB0,     CLK_USBH0_REG),
	AIC_CLK_PERIPH(CLK_USBH1,           CLK_AHB0,     CLK_USBH1_REG),
	AIC_CLK_PERIPH(CLK_USB_PHY0,        CLK_OSC24M,     CLK_USBPHY0_REG),
	AIC_CLK_PERIPH(CLK_USB_PHY1,        CLK_OSC24M,     CLK_USBPHY1_REG),
	AIC_CLK_PERIPH(CLK_GMAC0,           CLK_PLL_INT1,       CLK_GMAC0_REG),
	AIC_CLK_PERIPH(CLK_GMAC1,           CLK_PLL_INT1,       CLK_GMAC1_REG),
#endif
	AIC_CLK_PERIPH(CLK_SPI0,            CLK_PLL_FRA0,   CLK_SPI0_REG),
	AIC_CLK_PERIPH(CLK_SPI1,            CLK_PLL_FRA0,   CLK_SPI1_REG),
#if !defined(CONFIG_SPL_BUILD)
	AIC_CLK_PERIPH(CLK_SPI2,            CLK_PLL_FRA0,   CLK_SPI2_REG),
	AIC_CLK_PERIPH(CLK_SPI3,            CLK_PLL_FRA0,   CLK_SPI3_REG),
#endif
	AIC_CLK_PERIPH(CLK_SDMC0,           CLK_PLL_FRA0,   CLK_SDMC0_REG),
	AIC_CLK_PERIPH(CLK_SDMC1,           CLK_PLL_FRA0,   CLK_SDMC1_REG),
#if !defined(CONFIG_SPL_BUILD)
	AIC_CLK_PERIPH(CLK_SDMC2,           CLK_PLL_FRA0,   CLK_SDMC2_REG),
	AIC_CLK_PERIPH(CLK_PBUS,            CLK_AHB0,       CLK_PBUS_REG),
	AIC_CLK_PERIPH(CLK_SYSCFG,          CLK_OSC24M,       CLK_SYSCFG_REG),
#endif
#ifdef CONFIG_CLK_ARTINCHIP_V0_1
	AIC_CLK_PERIPH(CLK_RTC,             CLK_APB0,       CLK_RTC_REG),
#else
	AIC_CLK_PERIPH(CLK_RTC,             CLK_OSC32K,       CLK_RTC_REG),
#endif
	AIC_CLK_PERIPH(CLK_SPIENC,          CLK_AHB0,   CLK_SPIENC_REG),
#if !defined(CONFIG_SPL_BUILD)
	AIC_CLK_PERIPH(CLK_PWMCS,          CLK_PLL_INT1,   CLK_PWMCS_REG),
	AIC_CLK_PERIPH(CLK_PSADC,          CLK_OSC24M,   CLK_PSADC_REG),
	AIC_CLK_PERIPH(CLK_I2S0,            CLK_PLL_FRA1,   CLK_I2S0_REG),
	AIC_CLK_PERIPH(CLK_I2S1,            CLK_PLL_FRA1,   CLK_I2S1_REG),
	AIC_CLK_PERIPH(CLK_CODEC,           CLK_PLL_FRA1,   CLK_CODEC_REG),
#endif
	AIC_CLK_PERIPH(CLK_RGB,             CLK_SCLK,	    CLK_RGB_REG),
	AIC_CLK_PERIPH(CLK_LVDS,            CLK_SCLK,	    CLK_LVDS_REG),
	AIC_CLK_PERIPH(CLK_MIPIDSI,         CLK_SCLK,	    CLK_MIPIDSI_REG),
	AIC_CLK_PERIPH(CLK_DE,              CLK_PLL_INT1,   CLK_DE_REG),
	AIC_CLK_PERIPH(CLK_GE,              CLK_PLL_INT1,   CLK_GE_REG),
	AIC_CLK_PERIPH(CLK_VE,              CLK_PLL_INT1,   CLK_VE_REG),
	AIC_CLK_PERIPH(CLK_PWM,             CLK_PLL_INT1,   CLK_PWM_REG),
#ifdef CONFIG_CLK_ARTINCHIP_V0_1
	AIC_CLK_PERIPH(CLK_WDOG,            CLK_APB1,       CLK_WDOG_REG),
#else
	AIC_CLK_PERIPH(CLK_WDOG,            CLK_OSC32K,     CLK_WDOG_REG),
#endif
	AIC_CLK_PERIPH(CLK_SID,             CLK_OSC24M,     CLK_SID_REG),
	AIC_CLK_PERIPH(CLK_GTC,             CLK_APB1,       CLK_GTC_REG),
#ifdef CONFIG_CLK_ARTINCHIP_V0_1
	AIC_CLK_PERIPH(CLK_GPIO,            CLK_APB1,       CLK_GPIO_REG),
	AIC_CLK_PERIPH(CLK_UART0,           CLK_APB1,       CLK_UART0_REG),
	AIC_CLK_PERIPH(CLK_UART1,           CLK_APB1,       CLK_UART1_REG),
	AIC_CLK_PERIPH(CLK_UART2,           CLK_APB1,       CLK_UART2_REG),
	AIC_CLK_PERIPH(CLK_UART3,           CLK_APB1,       CLK_UART3_REG),
	AIC_CLK_PERIPH(CLK_UART4,           CLK_APB1,       CLK_UART4_REG),
	AIC_CLK_PERIPH(CLK_UART5,           CLK_APB1,       CLK_UART5_REG),
	AIC_CLK_PERIPH(CLK_UART6,           CLK_APB1,       CLK_UART6_REG),
	AIC_CLK_PERIPH(CLK_UART7,           CLK_APB1,       CLK_UART7_REG),
#else
	AIC_CLK_PERIPH(CLK_GPIO,            CLK_APB0,       CLK_GPIO_REG),
	AIC_CLK_PERIPH(CLK_UART0,           CLK_PLL_INT1,   CLK_UART0_REG),
	AIC_CLK_PERIPH(CLK_UART1,           CLK_PLL_INT1,   CLK_UART1_REG),
	AIC_CLK_PERIPH(CLK_UART2,           CLK_PLL_INT1,   CLK_UART2_REG),
	AIC_CLK_PERIPH(CLK_UART3,           CLK_PLL_INT1,   CLK_UART3_REG),
	AIC_CLK_PERIPH(CLK_UART4,           CLK_PLL_INT1,   CLK_UART4_REG),
	AIC_CLK_PERIPH(CLK_UART5,           CLK_PLL_INT1,   CLK_UART5_REG),
	AIC_CLK_PERIPH(CLK_UART6,           CLK_PLL_INT1,   CLK_UART6_REG),
	AIC_CLK_PERIPH(CLK_UART7,           CLK_PLL_INT1,   CLK_UART7_REG),
#endif
#if !defined(CONFIG_SPL_BUILD)
	AIC_CLK_PERIPH(CLK_I2C0,            CLK_APB1,       CLK_I2C0_REG),
	AIC_CLK_PERIPH(CLK_I2C1,            CLK_APB1,       CLK_I2C1_REG),
	AIC_CLK_PERIPH(CLK_I2C2,            CLK_APB1,       CLK_I2C2_REG),
	AIC_CLK_PERIPH(CLK_I2C3,            CLK_APB1,       CLK_I2C3_REG),
	AIC_CLK_PERIPH(CLK_CAN0,            CLK_APB1,       CLK_CAN0_REG),
	AIC_CLK_PERIPH(CLK_CAN1,            CLK_APB1,       CLK_CAN1_REG),
	AIC_CLK_PERIPH(CLK_ADCIM,           CLK_OSC24M,     CLK_ADCIM_REG),
	AIC_CLK_PERIPH(CLK_GPAI,            CLK_APB1,       CLK_GPAI_REG),
	AIC_CLK_PERIPH(CLK_RTP,             CLK_APB1,       CLK_RTP_REG),
	AIC_CLK_PERIPH(CLK_TSEN,            CLK_APB1,       CLK_TSEN_REG),
	AIC_CLK_PERIPH(CLK_CIR,             CLK_APB1,       CLK_CIR_REG),
#endif
};

static struct aic_disp_clk clk_disps[] = {
	CLK_DISP(CLK_SCLK, CLK_PLL_FRA2, CLK_DISP_REG, 0, 3, 0, 0, 0, 0, 0, 0),
	CLK_DISP(CLK_PIX, CLK_SCLK, CLK_DISP_REG, 0, 0, 4, 5, 10, 2, 12, 2),
};

#ifdef CONFIG_CLK_ARTINCHIP_V0_1
static u32 outclk_src_sels[] = {CLK_PLL_INT0, CLK_PLL_INT1, CLK_PLL_FRA0,
				CLK_PLL_FRA1, CLK_PLL_FRA2, CLK_OSC24M,
				CLK_OSC32K,   CLK_RC1M};
#else
static u32 outclk_src_sels[] = {CLK_PLL_INT1, CLK_PLL_FRA1, CLK_PLL_FRA2};
#endif

static struct aic_clk_out clk_output[] = {
	/*      id                reg	     */
	AIC_CLK_OUT(CLK_OUT0, CLK_OUT0_REG),
	AIC_CLK_OUT(CLK_OUT1, CLK_OUT1_REG),
	AIC_CLK_OUT(CLK_OUT2, CLK_OUT2_REG),
	AIC_CLK_OUT(CLK_OUT3, CLK_OUT3_REG),
};

struct aic_clk_tree aic_clktree = {
	.fixed_rate_base = CLK_DUMMY,
	.fixed_rate_cnt = ARRAY_SIZE(clk_fixed_rates),

	.pll_base = CLK_PLL_INT0,
	.pll_cnt = ARRAY_SIZE(clk_plls),

	.system_base = CLK_AXI0,
	.system_cnt = ARRAY_SIZE(clk_syss),

	.periph_base = CLK_DMA,
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
	struct dtd_artinchip_aic_cmu_v1_0 *dtplat = &plat->dtplat;
	struct aic_clk_priv *priv = dev_get_priv(dev);
	struct clk clk;

	priv->base = map_sysmem(dtplat->reg[0], dtplat->reg[1]);
	clk.dev = dev;

	clk.id = CLK_PLL_INT0;
	clk_set_rate(&clk, dtplat->pll_int0);

	clk.id = CLK_PLL_INT1;
	clk_set_rate(&clk, dtplat->pll_int1);

	clk.id = CLK_PLL_FRA0;
	clk_set_rate(&clk, dtplat->pll_frac0);

	clk.id = CLK_PLL_FRA1;
	clk_set_rate(&clk, dtplat->pll_frac1);

	clk.id = CLK_PLL_FRA2;
	clk_set_rate(&clk, dtplat->pll_frac2);
}
#else
static void aic_pll_init_ofdata(struct udevice *dev)
{
	struct aic_clk_priv *priv = dev_get_priv(dev);
	int ret;
	u32 value, freq;
	struct clk clk;

	priv->base = dev_read_addr_ptr(dev);

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
}

static void aic_system_clock_init(struct udevice *dev)
{
	int ret;
	u32 value, freq;
	struct clk clk, pclk;

	/* Set axi0 bus clock */
	ret = dev_read_u32(dev, "axi0", &value);
	if (!ret) {
		pclk.dev = dev;
		clk.id = CLK_AXI0;
		clk.dev = dev;
		if (value == 24000000) {
			pclk.id = CLK_OSC24M;
		} else {
			pclk.id = CLK_PLL_INT1;
			freq = clk_get_rate(&clk);
			if (freq != value)
				clk_set_rate(&clk, value);
		}
		clk_set_parent(&clk, &pclk);
	}

	/* Set ahb0 bus clock */
	ret = dev_read_u32(dev, "ahb0", &value);
	if (!ret) {
		pclk.dev = dev;
		clk.id = CLK_AHB0;
		clk.dev = dev;
		if (value == 24000000) {
			pclk.id = CLK_OSC24M;
		} else {
			pclk.id = CLK_PLL_INT1;
			freq = clk_get_rate(&clk);
			if (freq != value)
				clk_set_rate(&clk, value);
		}
		clk_set_parent(&clk, &pclk);
	}

	/* Set apb0 bus clock */
	ret = dev_read_u32(dev, "apb0", &value);
	if (!ret) {
		pclk.dev = dev;
		clk.id = CLK_APB0;
		clk.dev = dev;
		if (value == 24000000) {
			pclk.id = CLK_OSC24M;
		} else {
			pclk.id = CLK_PLL_INT1;
			freq = clk_get_rate(&clk);
			if (freq != value)
				clk_set_rate(&clk, value);
		}
		clk_set_parent(&clk, &pclk);
	}

	/* Set apb1 bus clock */
	ret = dev_read_u32(dev, "apb1", &value);
	if (!ret) {
		pclk.dev = dev;
		clk.id = CLK_APB1;
		clk.dev = dev;
		if (value == 24000000) {
			pclk.id = CLK_OSC24M;
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
	pclk.id = CLK_PLL_INT0;
	clk.id = CLK_CPU;
	clk.dev = dev;
	clk_set_parent(&clk, &pclk);

	/* Set clk_out0 frequency */
	ret = dev_read_u32(dev, "clk-out0", &value);
	if (!ret) {
		clk.id = CLK_OUT0;
		clk.dev = dev;
		freq = clk_get_rate(&clk);
		if (freq != value)
			clk_set_rate(&clk, value);
		clk_enable(&clk);
	}

	/* Set clk_out1 frequency */
	ret = dev_read_u32(dev, "clk-out1", &value);
	if (!ret) {
		clk.id = CLK_OUT1;
		clk.dev = dev;
		freq = clk_get_rate(&clk);
		if (freq != value)
			clk_set_rate(&clk, value);
		clk_enable(&clk);
	}

	/* Set clk_out2 frequency */
	ret = dev_read_u32(dev, "clk-out2", &value);
	if (!ret) {
		clk.id = CLK_OUT2;
		clk.dev = dev;
		freq = clk_get_rate(&clk);
		if (freq != value)
			clk_set_rate(&clk, value);
		clk_enable(&clk);
	}

	/* Set clk_out3 frequency */
	ret = dev_read_u32(dev, "clk-out3", &value);
	if (!ret) {
		clk.id = CLK_OUT3;
		clk.dev = dev;
		freq = clk_get_rate(&clk);
		if (freq != value)
			clk_set_rate(&clk, value);
		clk_enable(&clk);
	}
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
	{ .compatible = "artinchip,aic-cmu-v1.0", },
	{ .compatible = "artinchip,aic-cmu-v1.3", },
	{ }
};

U_BOOT_DRIVER(artinchip_cmu) = {
	.name                     = "artinchip_aic_cmu_v1_0",
	//.name                     = "artinchip_aic_cmu_v1_3",
	.id                       = UCLASS_CLK,
	.of_match                 = aic_clk_ids,
	.probe                    = aic_clk_probe,
	.priv_auto                = sizeof(struct aic_clk_priv),
	.plat_auto                = sizeof(struct aic_clk_plat),
	.ops                      = &artinchip_clk_ops,
};

