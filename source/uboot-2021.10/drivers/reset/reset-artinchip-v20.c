// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021-2024 ArtInChip Technology Co., Ltd
 * Authors: Dehuang Wu <dehuang.wu@artinchip.com>
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
#include <dt-bindings/reset/artinchip,aic-reset-v20.h>
#include "reset-artinchip-common.h"

struct aic_reset_plat {
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	struct dtd_artinchip_aic_reset_v2_0 dtplat;
#endif
};

/* artinchip,aic-reset-v2.0 */
#define CLK_SESS_CPU_REG	(0x0180)
#define CLK_SE_WDOG_REG		(0x018C)
#define CLK_SE_DMA_REG		(0x0190)
#define CLK_SE_CE_REG		(0x0194)
#define CLK_SE_SPI_REG		(0x0198)
#define CLK_SE_SID_REG		(0x01A0)
#define CLK_SE_I2C_REG		(0x01A4)
#define CLK_SE_GPIO_REG		(0x01A8)
#define CLK_SE_SPIENC_REG	(0x01AC)
#define CLK_SCSS_CPU_REG	(0x01F0)
#define CLK_SC_WDOG_REG		(0x01FC)
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
#define CLK_PRT_REG		(0x0C00)
#define CLK_PRT_LSU_REG		(0x0C04)
#define CLK_PRT_FUSER_REG	(0x0C08)
#define CLK_PRT_SBP_REG		(0x0C0C)
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
#define CLK_PWM_SDFM_REG	(0x0244)
#define CLK_CAP_SDFM_REG	(0x0248)
#define CLK_XPWM_REG(x)		((x) * 0x4 + 0x0B00) /* 0 ~ 7 */
#define CLK_PWM_REG(x)		((x) * 0x4 + 0x0B40) /* 0 ~ 15 */
#define CLK_CAP_REG(x)		((x) * 0x4 + 0x0B80) /* 0 ~ 4 */
#define CLK_GPT_REG(x)		((x) * 0x4 + 0x0BC0) /* 0 ~ 9 */
#define CLK_QSPI_REG(x)		((x) * 0x4 + 0x0460) /* 0 ~ 4 */
#define CLK_NULL_REG		(0x0FFC)
#define CLK_VER_REG		(0x0FFC)
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
#define RESET_DESC(_id, _reg)	{.id = _id, .bit = 13, .reg = _reg}

static struct artinchip_reset	rest_info[] = {
	/*              id                  reg         */
	RESET_DESC(RESET_SE_WDOG,	CLK_SE_WDOG_REG),
	RESET_DESC(RESET_SE_DMA,	CLK_SE_DMA_REG),
	RESET_DESC(RESET_SE_CE,		CLK_SE_CE_REG),
	RESET_DESC(RESET_SE_SPI,	CLK_SE_SPI_REG),
	RESET_DESC(RESET_SE_I2C,	CLK_SE_I2C_REG),
	RESET_DESC(RESET_SE_SPIENC,	CLK_SE_SPIENC_REG),
	RESET_DESC(RESET_SC_WDOG,	CLK_SC_WDOG_REG),
	RESET_DESC(RESET_SC_CPU,	CLK_SCSS_CPU_REG),
	RESET_DESC(RESET_CORE_CPU,	CLK_CORE_CPU_REG),
	RESET_DESC(RESET_CORE_WDOG,	CLK_CORE_WDOG_REG),
	RESET_DESC(RESET_DCE,		CLK_DCE_REG),
	RESET_DESC(RESET_DMA0,		CLK_DMA0_REG),
	RESET_DESC(RESET_DMA1,		CLK_DMA1_REG),
	RESET_DESC(RESET_DMA2,		CLK_DMA2_REG),
	RESET_DESC(RESET_DCE,		CLK_DCE_REG),
	RESET_DESC(RESET_USBH0,		CLK_USBH0_REG),
	RESET_DESC(RESET_USBH1,		CLK_USBH1_REG),
	RESET_DESC(RESET_USB_PHY0,	CLK_USB_PHY0_REG),
	RESET_DESC(RESET_USB_PHY1,	CLK_USB_PHY1_REG),
	RESET_DESC(RESET_SPI0,		CLK_SPI0_REG),
	RESET_DESC(RESET_SPI1,		CLK_SPI1_REG),
	RESET_DESC(RESET_SPI2,		CLK_SPI2_REG),
	RESET_DESC(RESET_SPI3,		CLK_SPI3_REG),
	RESET_DESC(RESET_SPI4,		CLK_SPI4_REG),
	RESET_DESC(RESET_SDMC0,		CLK_SDMC0_REG),
	RESET_DESC(RESET_SDMC1,		CLK_SDMC1_REG),
	RESET_DESC(RESET_MTOP,		CLK_MTOP_REG),
	RESET_DESC(RESET_I2S0,		CLK_I2S0_REG),
	RESET_DESC(RESET_AUDIO,		CLK_AUDIO_REG),
	RESET_DESC(RESET_UART0,		CLK_UART0_REG),
	RESET_DESC(RESET_UART1,		CLK_UART1_REG),
	RESET_DESC(RESET_UART2,		CLK_UART2_REG),
	RESET_DESC(RESET_UART3,		CLK_UART3_REG),
	RESET_DESC(RESET_UART4,		CLK_UART4_REG),
	RESET_DESC(RESET_UART5,		CLK_UART5_REG),
	RESET_DESC(RESET_ADCIM,		CLK_ADCIM_REG),
	RESET_DESC(RESET_GPAI,		CLK_GPAI_REG),
	RESET_DESC(RESET_THS,		CLK_THS_REG),
	RESET_DESC(RESET_LCD,		CLK_LCD_REG),
	RESET_DESC(RESET_LVDS,		CLK_LVDS_REG),
	RESET_DESC(RESET_DVP,		CLK_DVP_REG),
	RESET_DESC(RESET_DE,		CLK_DE_REG),
	RESET_DESC(RESET_I2C0,		CLK_I2C0_REG),
	RESET_DESC(RESET_I2C1,		CLK_I2C1_REG),
	RESET_DESC(RESET_I2C2,		CLK_I2C2_REG),
	RESET_DESC(RESET_I2C3,		CLK_I2C3_REG),
	RESET_DESC(RESET_I2C4,		CLK_I2C4_REG),
	RESET_DESC(RESET_SCAN,		CLK_SCAN_REG),
	RESET_DESC(RESET_PRT,		CLK_PRT_REG),
	RESET_DESC(RESET_PRT,		CLK_PRT_LSU_REG),
	RESET_DESC(RESET_PRT,		CLK_PRT_FUSER_REG),
	RESET_DESC(RESET_PRT,		CLK_PRT_SBP_REG),
	RESET_DESC(RESET_IMG_ROT,	CLK_IMG_ROT_REG),
	RESET_DESC(RESET_IMG_HT,	CLK_IMG_HT_REG),
	RESET_DESC(RESET_IMG_CRS0,	CLK_IMG_CRS0_REG),
	RESET_DESC(RESET_IMG_CRS1,	CLK_IMG_CRS1_REG),
	RESET_DESC(RESET_IMG_PP0,	CLK_IMG_PP0_REG),
	RESET_DESC(RESET_IMG_PP1,	CLK_IMG_PP1_REG),
	RESET_DESC(RESET_IMG_EHC0,	CLK_IMG_EHC0_REG),
	RESET_DESC(RESET_IMG_EHC1,	CLK_IMG_EHC1_REG),
	RESET_DESC(RESET_IMG_USM_SCALE,	CLK_USM_SCALE_REG),
	RESET_DESC(RESET_IMG_JBIG0,	CLK_IMG_JBIG0_REG),
	RESET_DESC(RESET_IMG_JBIG1,	CLK_IMG_JBIG1_REG),
	RESET_DESC(RESET_IMG_JPEG0,	CLK_IMG_JPEG0_REG),
	RESET_DESC(RESET_IMG_JPEG1,	CLK_IMG_JPEG1_REG),
	RESET_DESC(RESET_XPWM0,		CLK_XPWM_REG(0)),
	RESET_DESC(RESET_XPWM1,		CLK_XPWM_REG(1)),
	RESET_DESC(RESET_XPWM2,		CLK_XPWM_REG(2)),
	RESET_DESC(RESET_XPWM3,		CLK_XPWM_REG(3)),
	RESET_DESC(RESET_XPWM4,		CLK_XPWM_REG(4)),
	RESET_DESC(RESET_XPWM5,		CLK_XPWM_REG(5)),
	RESET_DESC(RESET_XPWM6,		CLK_XPWM_REG(6)),
	RESET_DESC(RESET_XPWM7,		CLK_XPWM_REG(7)),
	RESET_DESC(RESET_PWM0,		CLK_PWM_REG(0)),
	RESET_DESC(RESET_PWM1,		CLK_PWM_REG(1)),
	RESET_DESC(RESET_PWM2,		CLK_PWM_REG(2)),
	RESET_DESC(RESET_PWM3,		CLK_PWM_REG(3)),
	RESET_DESC(RESET_PWM4,		CLK_PWM_REG(4)),
	RESET_DESC(RESET_PWM5,		CLK_PWM_REG(5)),
	RESET_DESC(RESET_PWM6,		CLK_PWM_REG(6)),
	RESET_DESC(RESET_PWM7,		CLK_PWM_REG(7)),
	RESET_DESC(RESET_PWM8,		CLK_PWM_REG(8)),
	RESET_DESC(RESET_PWM9,		CLK_PWM_REG(9)),
	RESET_DESC(RESET_PWM10,		CLK_PWM_REG(10)),
	RESET_DESC(RESET_PWM11,		CLK_PWM_REG(11)),
	RESET_DESC(RESET_PWM12,		CLK_PWM_REG(12)),
	RESET_DESC(RESET_PWM13,		CLK_PWM_REG(13)),
	RESET_DESC(RESET_PWM14,		CLK_PWM_REG(14)),
	RESET_DESC(RESET_PWM15,		CLK_PWM_REG(15)),
	RESET_DESC(RESET_CAP0,		CLK_CAP_REG(0)),
	RESET_DESC(RESET_CAP1,		CLK_CAP_REG(1)),
	RESET_DESC(RESET_CAP2,		CLK_CAP_REG(2)),
	RESET_DESC(RESET_CAP3,		CLK_CAP_REG(3)),
	RESET_DESC(RESET_CAP4,		CLK_CAP_REG(4)),
	RESET_DESC(RESET_GPT0,		CLK_GPT_REG(0)),
	RESET_DESC(RESET_GPT1,		CLK_GPT_REG(1)),
	RESET_DESC(RESET_GPT2,		CLK_GPT_REG(2)),
	RESET_DESC(RESET_GPT3,		CLK_GPT_REG(3)),
	RESET_DESC(RESET_GPT4,		CLK_GPT_REG(4)),
	RESET_DESC(RESET_GPT5,		CLK_GPT_REG(5)),
	RESET_DESC(RESET_GPT6,		CLK_GPT_REG(6)),
	RESET_DESC(RESET_GPT7,		CLK_GPT_REG(7)),
	RESET_DESC(RESET_GPT8,		CLK_GPT_REG(8)),
	RESET_DESC(RESET_GPT9,		CLK_GPT_REG(9)),
	RESET_DESC(RESET_SP_USBD,	CLK_SP_USBD_REG),
	RESET_DESC(RESET_SP_USB_PHY0,	CLK_SP_USB_PHY0_REG),
};

static int aic_reset_probe(struct udevice *dev)
{
	struct artinchip_reset_priv *priv = dev_get_priv(dev);
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	struct aic_reset_plat *plat = dev_get_plat(dev);
	struct dtd_artinchip_aic_reset_v2_0 *dtplat = &plat->dtplat;

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
	{ .compatible = "artinchip,aic-reset-v2.0", },
	{ }
};

static struct reset_ops aic_reset_ops = {
	.request      = artinchip_reset_request,
	.rfree        = artinchip_reset_free,
	.rst_assert   = artinchip_reset_assert,
	.rst_deassert = artinchip_reset_deassert,
};

U_BOOT_DRIVER(aic_reset_v2_0) = {
	.name                     = "artinchip_aic_reset_v2_0",
	.id                       = UCLASS_RESET,
	.of_match                 = aic_clk_ids,
	.ops                      = &aic_reset_ops,
	.probe                    = aic_reset_probe,
	.priv_auto                = sizeof(struct artinchip_reset_priv),
	.plat_auto                = sizeof(struct aic_reset_plat),
};
