// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, Artinchip Technology Co., Ltd
 */

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/types.h>
#include <dt-bindings/clock/artinchip,aic-cmu.h>
#include "clk-aic.h"

#define PLL_INT0_GEN_REG	(0x0000)
#define PLL_INT1_GEN_REG	(0x0004)
#define PLL_FRA0_GEN_REG	(0x0020)
#define PLL_FRA1_GEN_REG	(0x0024)
#define PLL_FRA2_GEN_REG	(0x0028)
#define PLL_INT0_CFG_REG	(0x0040)
#define PLL_INT1_CFG_REG	(0x0044)
#define PLL_FRA0_CFG_REG	(0x0060)
#define PLL_FRA1_CFG_REG	(0x0064)
#define PLL_FRA2_CFG_REG	(0x0068)
#define PLL_FRA0_SDM_REG	(0x0080)
#define PLL_FRA1_SDM_REG	(0x0084)
#define PLL_FRA2_SDM_REG	(0x0088)
#define PLL_COM_REG		(0x00A0)
#define PLL_IN_REG		(0x00A4)
#define CLK_OUT0_REG		(0x00E0)
#define CLK_OUT1_REG		(0x00E4)
#define CLK_OUT2_REG		(0x00E8)
#define CLK_OUT3_REG		(0x00EC)
#define CLK_AXI0_REG		(0x0100)
#define CLK_AHB0_REG		(0x0110)
#define CLK_APB0_REG		(0x0120)
#define CLK_APB1_REG		(0x0124)
#define CLK_CPU_REG		(0x0200)
#define CLK_DM_REG		(0x0204)
#ifdef CONFIG_CLK_ARTINCHIP_V10
#define CLK_WDOG_REG		(0x020C)
#endif
#define CLK_DDR_REG		(0x0210)
#define CLK_DISP_REG		(0x0220)
#define CLK_DMA_REG		(0x0410)
#define CLK_CE_REG		(0x0418)
#define CLK_USBD_REG		(0x041C)
#define CLK_USBH0_REG		(0x0420)
#define CLK_USBH1_REG		(0x0424)
#define CLK_USB_PHY0_REG	(0x0430)
#define CLK_USB_PHY1_REG	(0x0434)
#define CLK_GMAC0_REG		(0x0440)
#define CLK_GMAC1_REG		(0x0444)
#define CLK_SPI0_REG		(0x0460)
#define CLK_SPI1_REG		(0x0464)
#define CLK_SPI2_REG		(0x0468)
#define CLK_SPI3_REG		(0x046C)
#define CLK_SDMC0_REG		(0x0470)
#define CLK_SDMC1_REG		(0x0474)
#define CLK_SDMC2_REG		(0x0478)
#define CLK_PBUS_REG		(0x04A0)
#define CLK_SYSCFG_REG		(0x0800)
#ifdef CONFIG_CLK_ARTINCHIP_V01
#define CLK_RTC_REG		(0x0804)
#endif
#define CLK_SPIENC_REG		(0x0810)
#define CLK_PWMCS_REG		(0x0814)
#define CLK_PSADC_REG		(0x0818)
#define CLK_MTOP_REG		(0x081C)
#define CLK_I2S0_REG		(0x0820)
#define CLK_I2S1_REG		(0x0824)
#define CLK_CODEC_REG		(0x0830)
#ifdef CONFIG_CLK_ARTINCHIP_V10
#define CLK_GPIO_REG		(0x083C)
#define CLK_UART0_REG		(0x0840)
#define CLK_UART1_REG		(0x0844)
#define CLK_UART2_REG		(0x0848)
#define CLK_UART3_REG		(0x084C)
#define CLK_UART4_REG		(0x0850)
#define CLK_UART5_REG		(0x0854)
#define CLK_UART6_REG		(0x0858)
#define CLK_UART7_REG		(0x085C)
#endif
#define CLK_RGB_REG		(0x0880)
#define CLK_LVDS_REG		(0x0884)
#define CLK_MIPID_REG		(0x0888)
#define CLK_DVP_REG		(0x0890)
#define CLK_DE_REG		(0x08C0)
#define CLK_GE_REG		(0x08C4)
#define CLK_VE_REG		(0x08C8)
#ifdef CONFIG_CLK_ARTINCHIP_V01
#define CLK_WDOG_REG		(0x0900)
#endif
#define CLK_SID_REG		(0x0904)
#ifdef CONFIG_CLK_ARTINCHIP_V10
#define CLK_RTC_REG		(0x0908)
#define CLK_GTC_REG		(0x090C)
#endif
#ifdef CONFIG_CLK_ARTINCHIP_V01
#define CLK_GTC_REG		(0x0908)
#define CLK_GPIO_REG		(0x091C)
#define CLK_UART0_REG		(0x0920)
#define CLK_UART1_REG		(0x0924)
#define CLK_UART2_REG		(0x0928)
#define CLK_UART3_REG		(0x092C)
#define CLK_UART4_REG		(0x0930)
#define CLK_UART5_REG		(0x0934)
#define CLK_UART6_REG		(0x0938)
#define CLK_UART7_REG		(0x093C)
#endif
#define CLK_I2C0_REG		(0x0960)
#define CLK_I2C1_REG		(0x0964)
#define CLK_I2C2_REG		(0x0968)
#define CLK_I2C3_REG		(0x096C)
#define CLK_CAN0_REG		(0x0980)
#define CLK_CAN1_REG		(0x0984)
#define CLK_PWM_REG		(0x0990)
#define CLK_ADCIM_REG		(0x09A0)
#define CLK_GPAI_REG		(0x09A4)
#define CLK_RTP_REG		(0x09A8)
#define CLK_TSEN_REG		(0x09AC)
#define CLK_CIR_REG		(0x09B0)
#define CLK_VER_REG		(0x0FFC)


#define PARENT(_parent) ((const char *[]) { _parent })

/* For PLL clock */
#define PLL(_id, _type, _name, _parent, _gen, _fra, _sdm, _flag)	\
{									\
	.id		=	_id,					\
	.type		=	_type,					\
	.name		=	_name,					\
	.parent_names	=	_parent,				\
	.num_parents	=	ARRAY_SIZE(_parent),			\
	.offset_gen	=	_gen,					\
	.offset_fra	=	_fra,					\
	.offset_sdm	=	_sdm,					\
	.flags		=	_flag,					\
	.func		=	aic_clk_hw_pll,				\
}
#define PLL_INT(_id, _name, _parent, _gen, _flag)			\
		PLL(_id, AIC_PLL_INT, _name, _parent, _gen, 0, 0, _flag)
#define PLL_FRA(_id, _name, _parent, _gen, _fra, _sdm, _flag)		\
		PLL(_id, AIC_PLL_FRA, _name, _parent, _gen, _fra, _sdm, _flag)
#define PLL_SDM(_id, _name, _parent, _gen, _fra, _sdm, _flag)		\
		PLL(_id, AIC_PLL_SDM, _name, _parent, _gen, _fra, _sdm, _flag)

#define PLL_SDM_VIDEO(_id, _name, _parent, _gen, _fra, _sdm, _flag,	\
		_min_rate, _max_rate)					\
{									\
	.id		=	_id,					\
	.type		=	AIC_PLL_SDM,				\
	.name		=	_name,					\
	.parent_names	=	_parent,				\
	.num_parents	=	ARRAY_SIZE(_parent),			\
	.offset_gen	=	_gen,					\
	.offset_fra	=	_fra,					\
	.offset_sdm	=	_sdm,					\
	.flags		=	_flag,					\
	.min_rate	=	_min_rate,				\
	.max_rate	=	_max_rate,				\
	.func		=	aic_clk_hw_pll,				\
}

/* For clocks fixed parent */
#define FPCLK(_id, _name, _parent, _reg, _bus, _mod, _div, _width)	\
{									\
	.id		=	_id,					\
	.type		=	AIC_FPCLK_NORMAL,			\
	.fact_mult	=	0,					\
	.fact_div	=	0,					\
	.name		=	_name,					\
	.parent_names	=	_parent,				\
	.num_parents	=	ARRAY_SIZE(_parent),			\
	.offset_reg	=	_reg,					\
	.bus_gate_bit	=	_bus,					\
	.mod_gate_bit	=	_mod,					\
	.div_bit	=	_div,					\
	.div_width	=	_width,					\
	.func		=	aic_clk_hw_fixed_parent,		\
}

/* For clocks fixed parent, fixed factor */
#define FPCLK_FIXED_FACTOR(_id, _name, _parent, _reg, _bus, _mod, _mult, _div)\
{									\
	.id		=	_id,					\
	.type		=	AIC_FPCLK_FIXED_FACTOR,			\
	.fact_mult	=	_mult,					\
	.fact_div	=	_div,					\
	.name		=	_name,					\
	.parent_names	=	_parent,				\
	.num_parents	=	ARRAY_SIZE(_parent),			\
	.offset_reg	=	_reg,					\
	.bus_gate_bit	=	_bus,					\
	.mod_gate_bit	=	_mod,					\
	.func		=	aic_clk_hw_fixed_parent,		\
}

/* For clocks that has multi-parent source */
#define MPCLK(_id, _name, _parent, _reg, _gate, _mux, _muxw, _div0, _div0w)\
{									\
	.id		=	_id,					\
	.name		=	_name,					\
	.parent_names	=	_parent,				\
	.num_parents	=	ARRAY_SIZE(_parent),			\
	.offset_reg	=	_reg,					\
	.gate_bit	=	_gate,					\
	.mux_bit	=	_mux,					\
	.mux_width	=	_muxw,					\
	.div0_bit	=	_div0,					\
	.div0_width	=	_div0w,					\
	.func		=	aic_clk_hw_multi_parent,		\
}

/* For display clock */
#define DISPCLK(_id, _name, _parent, _reg, _divn, _nwidth, \
		_divm, _mwidth, _divl, _lwidth, _pix_divsel,\
		_pix_divsel_width, _flag)\
{\
	.id = _id,\
	.name = _name,\
	.parent_names = _parent,\
	.num_parents = ARRAY_SIZE(_parent),\
	.offset_reg = _reg,\
	.divn_bit = _divn,\
	.divn_width = _nwidth,\
	.divm_bit = _divm,\
	.divm_width = _mwidth,\
	.divl_bit = _divl,\
	.divl_width = _lwidth,\
	.pix_divsel_bit = _pix_divsel,\
	.pix_divsel_width = _pix_divsel_width,\
	.flags = _flag,\
	.func = aic_clk_hw_disp,\
}

static struct clk_hw **hws;
static struct clk_hw_onecell_data *clk_hw_data;

#ifdef CONFIG_CLK_ARTINCHIP_V01
static const char *const apb1_src_sels[] = {
	"osc24m", "pll_int1",
};

static const char *const cpu_src_sels[] = {
	"osc24m", "pll_int0", "dummy", "osc32k",
};

static const char *const outclk_src_sels[] = {
	"pll_int0", "pll_int1", "pll_fra0", "pll_fra1",
	"pll_fra2", "osc24m",   "osc32k",   "rc1m",
};
#else
static const char *const cpu_src_sels[] = {
	"osc24m", "pll_int0",
};

static const char *const outclk_src_sels[] = {
	"pll_int1", "pll_fra1", "pll_fra2",
};
#endif

static const char *const ahb0_src_sels[] = {
	"osc24m", "pll_int1",
};

static const char *const apb0_src_sels[] = {
	"osc24m", "pll_int1",
};

static const char *const axi0_src_sels[] = {
	"osc24m", "pll_int1",
};

static const char *const dm_src_sels[] = {
	"osc24m", "pll_int0"
};

static const struct pll_clk_cfg aic_clk_pll_cfg[] = {
	PLL_INT(CLK_PLL_INT0, "pll_int0", PARENT("osc24m"),
		PLL_INT0_GEN_REG, 0),
	PLL_INT(CLK_PLL_INT1, "pll_int1", PARENT("osc24m"),
		PLL_INT1_GEN_REG, 0),
	PLL_SDM(CLK_PLL_FRA0, "pll_fra0", PARENT("osc24m"),
		PLL_FRA0_GEN_REG, PLL_FRA0_CFG_REG,
		PLL_FRA0_SDM_REG, CLK_IGNORE_UNUSED),
	PLL_FRA(CLK_PLL_FRA1, "pll_fra1", PARENT("osc24m"),
		PLL_FRA1_GEN_REG, PLL_FRA1_CFG_REG, PLL_FRA1_SDM_REG, 0),
	PLL_SDM_VIDEO(CLK_PLL_FRA2, "pll_fra2", PARENT("osc24m"),
		PLL_FRA2_GEN_REG, PLL_FRA2_CFG_REG, PLL_FRA2_SDM_REG, 0,
		360000000, 1200000000),
};

static const struct fixed_parent_clk_cfg aic_fixed_parent_clk_cfg[] = {
#ifndef CONFIG_CLK_ARTINCHIP_V01
	FPCLK(CLK_APB1, "apb1", PARENT("osc24m"), CLK_APB1_REG, -1, -1, 0, 0),
#endif
	FPCLK(CLK_DMA, "dma", PARENT("ahb0"), CLK_DMA_REG, 12, -1, 0, 0),
	FPCLK(CLK_CE, "ce", PARENT("pll_int1"), CLK_CE_REG, 12, 8, 0, 3),
	FPCLK(CLK_USBD, "usb_dev", PARENT("ahb0"), CLK_USBD_REG, 12, -1, 0, 0),
	FPCLK(CLK_USBH0, "usb_host0", PARENT("ahb0"), CLK_USBH0_REG,
		12, -1, 0, 0),
	FPCLK(CLK_USBH1, "usb_host1", PARENT("ahb0"), CLK_USBH1_REG,
		12, -1, 0, 0),
	FPCLK(CLK_USB_PHY0, "usb_phy0", PARENT("osc24m"), CLK_USB_PHY0_REG,
		-1, 8, 0, 0),
	FPCLK(CLK_USB_PHY1, "usb_phy1", PARENT("osc24m"), CLK_USB_PHY1_REG,
		-1, 8, 0, 0),
	FPCLK(CLK_GMAC0, "gmac0", PARENT("pll_int1"), CLK_GMAC0_REG, 12, 8, 0, 5),
	FPCLK(CLK_GMAC1, "gmac1", PARENT("pll_int1"), CLK_GMAC1_REG, 12, 8, 0, 5),
	FPCLK(CLK_SPI0, "spi0", PARENT("pll_fra0"), CLK_SPI0_REG, 12, 8, 0, 5),
	FPCLK(CLK_SPI1, "spi1", PARENT("pll_fra0"), CLK_SPI1_REG, 12, 8, 0, 5),
	FPCLK(CLK_SPI2, "spi2", PARENT("pll_fra0"), CLK_SPI2_REG, 12, 8, 0, 5),
	FPCLK(CLK_SPI3, "spi3", PARENT("pll_fra0"), CLK_SPI3_REG, 12, 8, 0, 5),
	FPCLK(CLK_SDMC0, "sdmc0", PARENT("pll_fra0"), CLK_SDMC0_REG,
	      12, 8, 0, 5),
	FPCLK(CLK_SDMC1, "sdmc1", PARENT("pll_fra0"), CLK_SDMC1_REG,
	      12, 8, 0, 5),
	FPCLK(CLK_SDMC2, "sdmc2", PARENT("pll_fra0"), CLK_SDMC2_REG,
	      12, 8, 0, 5),
	FPCLK(CLK_PBUS, "pbus", PARENT("ahb0"), CLK_PBUS_REG, 12, 8, 0, 5),
	FPCLK(CLK_SYSCFG, "syscfg", PARENT("osc24m"), CLK_SYSCFG_REG,
		12, -1, 0, 0),
#ifdef CONFIG_CLK_ARTINCHIP_V01
	FPCLK(CLK_RTC, "rtc", PARENT("apb0"), CLK_RTC_REG, 12, -1, 0, 0),
#else
	FPCLK(CLK_RTC, "rtc", PARENT("osc32k"), CLK_RTC_REG, 12, -1, 0, 0),
#endif
	FPCLK(CLK_I2S0, "i2s0", PARENT("pll_fra1"), CLK_I2S0_REG, 12, 8, 0, 5),
	FPCLK(CLK_I2S1, "i2s1", PARENT("pll_fra1"), CLK_I2S1_REG, 12, 8, 0, 5),
	FPCLK(CLK_CODEC, "codec", PARENT("pll_fra1"), CLK_CODEC_REG, 12, 8, 0, 5),
	FPCLK(CLK_DE, "de", PARENT("pll_int1"), CLK_DE_REG, 12, 8, 0, 5),
	FPCLK(CLK_GE, "ge", PARENT("pll_int1"), CLK_GE_REG, 12, 8, 0, 5),
	FPCLK(CLK_VE, "ve", PARENT("pll_int1"), CLK_VE_REG, 12, 8, 0, 5),
#ifdef CONFIG_CLK_ARTINCHIP_V01
	FPCLK(CLK_WDOG, "wdog", PARENT("apb1"), CLK_WDOG_REG, 12, 8, 0, 0),
#else
	FPCLK(CLK_WDOG, "wdog", PARENT("osc24m"), CLK_WDOG_REG, 12, 8, 0, 0),
#endif
	FPCLK(CLK_SID, "sid", PARENT("osc24m"), CLK_SID_REG, 12, 8, 0, 0),
	FPCLK(CLK_GTC, "gtc", PARENT("apb1"), CLK_GTC_REG, 12, -1, 0, 0),
#ifdef CONFIG_CLK_ARTINCHIP_V01
	FPCLK(CLK_GPIO, "gpio", PARENT("apb1"), CLK_GPIO_REG, 12, -1, 0, 0),
	FPCLK(CLK_UART0, "uart0", PARENT("apb1"), CLK_UART0_REG, 12, -1, 0, 0),
	FPCLK(CLK_UART1, "uart1", PARENT("apb1"), CLK_UART1_REG, 12, -1, 0, 0),
	FPCLK(CLK_UART2, "uart2", PARENT("apb1"), CLK_UART2_REG, 12, -1, 0, 0),
	FPCLK(CLK_UART3, "uart3", PARENT("apb1"), CLK_UART3_REG, 12, -1, 0, 0),
	FPCLK(CLK_UART4, "uart4", PARENT("apb1"), CLK_UART4_REG, 12, -1, 0, 0),
	FPCLK(CLK_UART5, "uart5", PARENT("apb1"), CLK_UART5_REG, 12, -1, 0, 0),
	FPCLK(CLK_UART6, "uart6", PARENT("apb1"), CLK_UART6_REG, 12, -1, 0, 0),
	FPCLK(CLK_UART7, "uart7", PARENT("apb1"), CLK_UART7_REG, 12, -1, 0, 0),
#else
	FPCLK(CLK_GPIO, "gpio", PARENT("apb0"), CLK_GPIO_REG, 12, -1, 0, 0),
	FPCLK(CLK_UART0, "uart0", PARENT("pll_int1"), CLK_UART0_REG, 12, 8, 0, 5),
	FPCLK(CLK_UART1, "uart1", PARENT("pll_int1"), CLK_UART1_REG, 12, 8, 0, 5),
	FPCLK(CLK_UART2, "uart2", PARENT("pll_int1"), CLK_UART2_REG, 12, 8, 0, 5),
	FPCLK(CLK_UART3, "uart3", PARENT("pll_int1"), CLK_UART3_REG, 12, 8, 0, 5),
	FPCLK(CLK_UART4, "uart4", PARENT("pll_int1"), CLK_UART4_REG, 12, 8, 0, 5),
	FPCLK(CLK_UART5, "uart5", PARENT("pll_int1"), CLK_UART5_REG, 12, 8, 0, 5),
	FPCLK(CLK_UART6, "uart6", PARENT("pll_int1"), CLK_UART6_REG, 12, 8, 0, 5),
	FPCLK(CLK_UART7, "uart7", PARENT("pll_int1"), CLK_UART7_REG, 12, 8, 0, 5),
#endif
	FPCLK(CLK_I2C0, "i2c0", PARENT("apb1"), CLK_I2C0_REG, 12, -1, 0, 0),
	FPCLK(CLK_I2C1, "i2c1", PARENT("apb1"), CLK_I2C1_REG, 12, -1, 0, 0),
	FPCLK(CLK_I2C2, "i2c2", PARENT("apb1"), CLK_I2C2_REG, 12, -1, 0, 0),
	FPCLK(CLK_I2C3, "i2c3", PARENT("apb1"), CLK_I2C3_REG, 12, -1, 0, 0),
	FPCLK(CLK_CAN0, "can0", PARENT("apb1"), CLK_CAN0_REG, 12, -1, 0, 0),
	FPCLK(CLK_CAN1, "can1", PARENT("apb1"), CLK_CAN1_REG, 12, -1, 0, 0),
	FPCLK(CLK_PWM, "pwm", PARENT("pll_int1"), CLK_PWM_REG, 12, 8, 0, 5),
	FPCLK(CLK_ADCIM, "adcim", PARENT("osc24m"), CLK_ADCIM_REG,
		12, 8, 0, 5),
	FPCLK(CLK_GPAI, "gpai", PARENT("apb1"), CLK_GPAI_REG, 12, -1, 0, 0),
	FPCLK(CLK_RTP, "rtp", PARENT("apb1"), CLK_RTP_REG, 12, -1, 0, 0),
	FPCLK(CLK_TSEN, "tsen", PARENT("apb1"), CLK_TSEN_REG, 12, -1, 0, 0),
	FPCLK(CLK_CIR, "cir", PARENT("apb1"), CLK_CIR_REG, 12, -1, 0, 0),
	FPCLK(CLK_DVP, "dvp", PARENT("pll_int1"), CLK_DVP_REG, 12, 8, 0, 5),
	FPCLK(CLK_MTOP, "mtop", PARENT("apb0"), CLK_MTOP_REG, 12, -1, 0, 0),
	FPCLK(CLK_SPIENC, "spienc", PARENT("ahb0"), CLK_SPIENC_REG,
		12, 8, 0, 0),
	FPCLK(CLK_PWMCS, "pwmcs", PARENT("pll_int1"), CLK_PWMCS_REG,
		12, 8, 0, 5),
	FPCLK(CLK_PSADC, "psadc", PARENT("osc24m"), CLK_PSADC_REG,
		12, 8, 0, 0),
	FPCLK(CLK_RGB, "rgb", PARENT("sclk"), CLK_RGB_REG, 12, 8, 0, 0),
	FPCLK(CLK_LVDS, "lvds", PARENT("sclk"), CLK_LVDS_REG, 12, 8, 0, 0),
	FPCLK(CLK_MIPIDSI, "mipidsi", PARENT("pll_fra2"), CLK_MIPID_REG,
		12, 8, 0, 0),
	FPCLK(CLK_DDR, "ddr", PARENT("pll_fra0"), CLK_DDR_REG, -1, -1, 0, 5),
};

static const struct multi_parent_clk_cfg aic_multi_parent_clk_cfg[] = {
#ifdef CONFIG_CLK_ARTINCHIP_V01
	MPCLK(CLK_APB1, "apb1", apb1_src_sels, CLK_APB1_REG,
		-1, 8, 1, 0, 5),
#endif
	MPCLK(CLK_CPU, "cpu", cpu_src_sels, CLK_CPU_REG,
		-1, 8, 1, 0, 5),
	MPCLK(CLK_AHB0, "ahb0", ahb0_src_sels, CLK_AHB0_REG,
		-1, 8, 1, 0, 5),
	MPCLK(CLK_APB0, "apb0", apb0_src_sels, CLK_APB0_REG,
		-1, 8, 1, 0, 5),
	MPCLK(CLK_AXI0, "axi0", axi0_src_sels, CLK_AXI0_REG,
		-1, 8, 1, 0, 5),
	MPCLK(CLK_OUT0, "clk_out0", outclk_src_sels, CLK_OUT0_REG,
		16, 12, 3, 0, 8),
	MPCLK(CLK_OUT1, "clk_out1", outclk_src_sels, CLK_OUT1_REG,
		16, 12, 3, 0, 8),
	MPCLK(CLK_OUT2, "clk_out2", outclk_src_sels, CLK_OUT2_REG,
		16, 12, 3, 0, 8),
	MPCLK(CLK_OUT3, "clk_out3", outclk_src_sels, CLK_OUT3_REG,
		16, 12, 3, 0, 8),
	MPCLK(CLK_DM, "dm", dm_src_sels, CLK_DM_REG,
		-1, 8, 1, 0, 5),
};

static const struct disp_clk_cfg aic_clk_disp_mod_cfg[] = {
	DISPCLK(CLK_PIX, "pixclk", PARENT("sclk"), CLK_DISP_REG,
		0, 0, 4, 5, 10, 2, 12, 2, 0),
	DISPCLK(CLK_SCLK, "sclk", PARENT("pll_fra2"), CLK_DISP_REG,
		0, 3, 0, 0, 0, 0, 0, 0, CLK_SET_RATE_PARENT),
};

static void __init aic_clocks_init(struct device_node *np)
{
	const struct fixed_parent_clk_cfg *fpcfg;
	const struct multi_parent_clk_cfg *mpcfg;
	const struct pll_clk_cfg *pllcfg;
	const struct disp_clk_cfg *dispcfg;
	void __iomem *reg_base;
	int i, ret;
	u32 xtal_gm, val;

	clk_hw_data = kzalloc(struct_size(clk_hw_data, hws, AIC_CLK_END),
			      GFP_KERNEL);
	if (WARN_ON(!clk_hw_data))
		return;

	clk_hw_data->num = AIC_CLK_END;
	hws = clk_hw_data->hws;

	reg_base = of_iomap(np, 0);
	if (!reg_base) {
		pr_err("%s: could not map ccu region\n", __func__);
		return;
	}

	/* xtal drive force value */
	ret = of_property_read_u32(np, "aic,xtal_driver", &xtal_gm);
	if (!ret) {
		val = readl(reg_base + PLL_IN_REG);
		val &= ~GENMASK(31, 30);
		val |= xtal_gm << 30;
		writel(val, reg_base + PLL_IN_REG);
	}

	hws[CLK_DUMMY] = aic_clk_hw_fixed("dummy", 0);

	/* Register fixed source clocks */
	hws[CLK_OSC24M] = __clk_get_hw(of_clk_get_by_name(np, "osc24m"));
	hws[CLK_OSC32K] = __clk_get_hw(of_clk_get_by_name(np, "osc32k"));
	hws[CLK_RC1M] = __clk_get_hw(of_clk_get_by_name(np, "rc1m"));

	/* Register PLL */
	for (i = 0; i < ARRAY_SIZE(aic_clk_pll_cfg); i++) {
		pllcfg = &aic_clk_pll_cfg[i];
		hws[pllcfg->id] = pllcfg->func(reg_base, pllcfg);
	}

	/* Register clock that has fixed parent */
	for (i = 0; i < ARRAY_SIZE(aic_fixed_parent_clk_cfg); i++) {
		fpcfg = &aic_fixed_parent_clk_cfg[i];
		hws[fpcfg->id] = fpcfg->func(reg_base, fpcfg);
	}

	/* Register clock that has multi-parent */
	for (i = 0; i < ARRAY_SIZE(aic_multi_parent_clk_cfg); i++) {
		mpcfg = &aic_multi_parent_clk_cfg[i];
		hws[mpcfg->id] = mpcfg->func(reg_base, mpcfg);
	}

	/* Register display modules clock  */
	for (i = 0; i < ARRAY_SIZE(aic_clk_disp_mod_cfg); i++) {
		dispcfg = &aic_clk_disp_mod_cfg[i];
		hws[dispcfg->id] = dispcfg->func(reg_base, dispcfg);
	}

	/* add clock provider */
	of_clk_add_hw_provider(np, of_clk_hw_onecell_get, clk_hw_data);

	/* Set cpu and bus clock parent */
	clk_set_parent(hws[CLK_CPU]->clk, hws[CLK_PLL_INT0]->clk);
	clk_set_parent(hws[CLK_AHB0]->clk, hws[CLK_PLL_INT1]->clk);
	clk_set_parent(hws[CLK_AXI0]->clk, hws[CLK_PLL_INT1]->clk);
	clk_set_parent(hws[CLK_APB0]->clk, hws[CLK_PLL_INT1]->clk);
	clk_set_parent(hws[CLK_APB1]->clk, hws[CLK_OSC24M]->clk);

	/* prepare and enable cpu and bus clock */
	clk_prepare_enable(hws[CLK_CPU]->clk);
	clk_prepare_enable(hws[CLK_AXI0]->clk);
	clk_prepare_enable(hws[CLK_GTC]->clk);

	/* Set clk_out0 */
	if (of_property_read_bool(np, "clk-out0-enable"))
		clk_prepare_enable(hws[CLK_OUT0]->clk);
	/* Set clk_out1 */
	if (of_property_read_bool(np, "clk-out1-enable"))
		clk_prepare_enable(hws[CLK_OUT1]->clk);
	/* Set clk_out2 */
	if (of_property_read_bool(np, "clk-out2-enable"))
		clk_prepare_enable(hws[CLK_OUT2]->clk);
	/* Set clk_out3 */
	if (of_property_read_bool(np, "clk-out3-enable"))
		clk_prepare_enable(hws[CLK_OUT3]->clk);
}

CLK_OF_DECLARE(aic, "artinchip,aic-cmu-v1.0", aic_clocks_init);
