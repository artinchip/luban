// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd
 * Dehuang Wu <dehuang.wu@artinchip.com>
 */

#include <common.h>
#include <mapmem.h>
#include <dm.h>
#include <errno.h>
#include <reset-uclass.h>
#include <dt-structs.h>
#include <asm/io.h>
#include <dm/lists.h>
#include <linux/log2.h>
#include <dt-bindings/reset/artinchip,aic-reset-v30.h>
#include "reset-artinchip-common.h"

struct aic_reset_plat {
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	struct dtd_artinchip_aic_reset_v3_0 dtplat;
#endif
};

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

/* PRCM Real-Time system clk */
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
#define RESET_DESC(_id, _reg)	{.id = _id, .bit = 24, .reg = _reg}

static struct artinchip_reset	rest_info[] = {
	/*              id                  reg         */
	RESET_DESC(RESET_CPU0,		CLK_CPU0_REG),
	RESET_DESC(RESET_CPU1,		CLK_CPU1_REG),
	RESET_DESC(RESET_WDOG,		CLK_WDOG_REG),
	RESET_DESC(RESET_DDR,		CLK_DDR_REG),
	RESET_DESC(RESET_DMA0,		CLK_DMA0_REG),
	RESET_DESC(RESET_DMA1,		CLK_DMA1_REG),
	RESET_DESC(RESET_DCE,		CLK_DCE_REG),
	RESET_DESC(RESET_CE,		CLK_CE_REG),
	RESET_DESC(RESET_USB_DEV,	CLK_USB_DEV_REG),
	RESET_DESC(RESET_USBH0,		CLK_USBH0_REG),
	RESET_DESC(RESET_USBH1,		CLK_USBH1_REG),
	RESET_DESC(RESET_USB_PHY0,	CLK_USB_PHY0_REG),
	RESET_DESC(RESET_USB_PHY1,	CLK_USB_PHY1_REG),
	RESET_DESC(RESET_USB_FS_DR,	CLK_USB_FS_DR_REG),
	RESET_DESC(RESET_GMAC0,		CLK_GMAC0_REG),
	RESET_DESC(RESET_GMAC1,		CLK_GMAC1_REG),
	RESET_DESC(RESET_CANFD,		CLK_CANFD_REG),
	RESET_DESC(RESET_XSPI,		CLK_XSPI_REG),
	RESET_DESC(RESET_QSPI0,		CLK_QSPI_REG(0)),
	RESET_DESC(RESET_QSPI1,		CLK_QSPI_REG(1)),
	RESET_DESC(RESET_QSPI2,		CLK_QSPI_REG(2)),
	RESET_DESC(RESET_QSPI3,		CLK_QSPI_REG(3)),
	RESET_DESC(RESET_SDMC0,		CLK_SDMC_REG(0)),
	RESET_DESC(RESET_SDMC1,		CLK_SDMC_REG(1)),
	RESET_DESC(RESET_SDMC2,		CLK_SDMC_REG(2)),
	RESET_DESC(RESET_PBUS,		CLK_PBUS_REG),
	RESET_DESC(RESET_SPIENC,	CLK_SPIENC_REG),
	RESET_DESC(RESET_PWMCS,		CLK_PWMCS_REG),
	RESET_DESC(RESET_MTOP,		CLK_MTOP_REG),
	RESET_DESC(RESET_I2S0,		CLK_I2S_REG(0)),
	RESET_DESC(RESET_I2S1,		CLK_I2S_REG(1)),
	RESET_DESC(RESET_I2S2,		CLK_I2S_REG(2)),
	RESET_DESC(RESET_SAI,		CLK_SAI_REG),
	RESET_DESC(RESET_AUDIO,		CLK_AUDIO_REG),
	RESET_DESC(RESET_GPIO,		CLK_GPIO_REG),
	RESET_DESC(RESET_UART0,		CLK_UART_REG(0)),
	RESET_DESC(RESET_UART1,		CLK_UART_REG(1)),
	RESET_DESC(RESET_UART2,		CLK_UART_REG(2)),
	RESET_DESC(RESET_UART3,		CLK_UART_REG(3)),
	RESET_DESC(RESET_UART4,		CLK_UART_REG(4)),
	RESET_DESC(RESET_UART5,		CLK_UART_REG(5)),
	RESET_DESC(RESET_UART6,		CLK_UART_REG(6)),
	RESET_DESC(RESET_UART7,		CLK_UART_REG(7)),
	RESET_DESC(RESET_LCD0,		CLK_LCD0_REG),
	RESET_DESC(RESET_LVDS0,		CLK_LVDS0_REG),
	RESET_DESC(RESET_MIPI_DSI,	CLK_MIPI_DSI_REG),
	RESET_DESC(RESET_MIPI_DPHY0,	CLK_MIPI_DPHY0_REG),
	RESET_DESC(RESET_DVP,		CLK_DVP_REG),
	RESET_DESC(RESET_MIPI_CSI,	CLK_MIPI_CSI_REG),
	RESET_DESC(RESET_MIPI_DPHY1,	CLK_MIPI_DPHY1_REG),
	RESET_DESC(RESET_LCD1,		CLK_LCD1_REG),
	RESET_DESC(RESET_LVDS1,		CLK_LVDS1_REG),
	RESET_DESC(RESET_TVE,		CLK_TVE_REG),
	RESET_DESC(RESET_DE,		CLK_DE_REG),
	RESET_DESC(RESET_GE,		CLK_GE_REG),
	RESET_DESC(RESET_VE,		CLK_VE_REG),
	RESET_DESC(RESET_I2C0,		CLK_I2C_REG(0)),
	RESET_DESC(RESET_I2C1,		CLK_I2C_REG(1)),
	RESET_DESC(RESET_I2C2,		CLK_I2C_REG(2)),
	RESET_DESC(RESET_XPWM0,		CLK_XPWM_REG(0)),
	RESET_DESC(RESET_XPWM1,		CLK_XPWM_REG(1)),
	RESET_DESC(RESET_XPWM2,		CLK_XPWM_REG(2)),
	RESET_DESC(RESET_XPWM3,		CLK_XPWM_REG(3)),
	RESET_DESC(RESET_ADCIM,		CLK_ADCIM_REG),
	RESET_DESC(RESET_GPAI,		CLK_GPAI_REG),
	RESET_DESC(RESET_RTP,		CLK_RTP_REG),
	RESET_DESC(RESET_THS,		CLK_THS_REG),
	{.id = RESET_CIR, .bit = 13, .reg = CLK_CIR_REG},
	RESET_DESC(RESET_CMP,		CLK_CMP_REG),
	RESET_DESC(RESET_GPT0,		CLK_GPT_REG(0)),
	RESET_DESC(RESET_GPT1,		CLK_GPT_REG(1)),
	RESET_DESC(RESET_GPT2,		CLK_GPT_REG(2)),
	RESET_DESC(RESET_GPT3,		CLK_GPT_REG(3)),
	RESET_DESC(RESET_GPT4,		CLK_GPT_REG(4)),
	RESET_DESC(RESET_GPT5,		CLK_GPT_REG(5)),
	/*PRCM Real_Time system*/
	RESET_DESC(RESET_R_CPU,		CLK_R_CPU_REG),
	RESET_DESC(RESET_R_WDOG,	CLK_R_WDOG_REG),
	RESET_DESC(RESET_R_DMA,		CLK_R_DMA_REG),
	RESET_DESC(RESET_R_CANFD0,	CLK_R_CANFD0_REG),
	RESET_DESC(RESET_R_CANFD1,	CLK_R_CANFD1_REG),
	RESET_DESC(RESET_R_SPI,		CLK_R_SPI_REG),
	RESET_DESC(RESET_R_GPIO,	CLK_R_GPIO_REG),
	RESET_DESC(RESET_R_UART0,	CLK_R_UART0_REG),
	RESET_DESC(RESET_R_UART1,	CLK_R_UART1_REG),
	RESET_DESC(RESET_R_GTC,		CLK_R_GTC_REG),
	RESET_DESC(RESET_R_I2C0,	CLK_R_I2C0_REG),
	RESET_DESC(RESET_R_I2C1,	CLK_R_I2C1_REG),
	RESET_DESC(RESET_R_XPWM0,	CLK_R_XPWM0_REG),
	RESET_DESC(RESET_R_XPWM1,	CLK_R_XPWM1_REG),
	RESET_DESC(RESET_R_XPWM2,	CLK_R_XPWM2_REG),
	RESET_DESC(RESET_R_XPWM3,	CLK_R_XPWM3_REG),
	RESET_DESC(RESET_R_CIR,		CLK_R_CIR_REG),
};

static int aic_reset_probe(struct udevice *dev)
{
	struct artinchip_reset_priv *priv = dev_get_priv(dev);
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	struct aic_reset_plat *plat = dev_get_plat(dev);
	struct dtd_artinchip_aic_reset_v3_0 *dtplat = &plat->dtplat;

	priv->base = map_sysmem(dtplat->reg[0], dtplat->reg[1]);
	priv->cz_base = map_sysmem(dtplat->reg[2], dtplat->reg[3]);
#else
	priv->base = dev_read_addr_ptr(dev);
	priv->cz_base = (void *)(uintptr_t)dev_read_addr_index(dev, 1);
#endif
	priv->count = ARRAY_SIZE(rest_info);
	priv->max_id = RESET_NUMBER;
	priv->rests = rest_info;

	return 0;
}

static const struct udevice_id aic_clk_ids[] = {
	{ .compatible = "artinchip,aic-reset-v3.0", },
	{ }
};

static struct reset_ops aic_reset_ops = {
	.request	= artinchip_reset_request,
	.rfree		= artinchip_reset_free,
	.rst_assert	= artinchip_reset_assert,
	.rst_deassert	= artinchip_reset_deassert,
};

U_BOOT_DRIVER(aic_reset_v3_0) = {
	.name		= "artinchip_aic_reset_v3_0",
	.id		= UCLASS_RESET,
	.of_match	= aic_clk_ids,
	.ops		= &aic_reset_ops,
	.probe		= aic_reset_probe,
	.priv_auto	= sizeof(struct artinchip_reset_priv),
	.plat_auto	= sizeof(struct aic_reset_plat),
};
