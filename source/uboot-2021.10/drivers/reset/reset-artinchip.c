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
#include <dt-bindings/reset/artinchip,aic-reset.h>
#include "reset-artinchip-common.h"

struct aic_reset_plat {
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	struct dtd_artinchip_aic_reset_v1_0 dtplat;
#endif
};

#define CLK_DMA_REG             (0x0410)
#define CLK_CE_REG              (0x0418)
#define CLK_USBD_REG            (0x041C)
#define CLK_USBH0_REG           (0x0420)
#define CLK_USBH1_REG           (0x0424)
#define CLK_USBPHY0_REG         (0x0430)
#define CLK_USBPHY1_REG         (0x0434)
#define CLK_GMAC0_REG           (0x0440)
#define CLK_GMAC1_REG           (0x0444)
#define CLK_SPI0_REG            (0x0460)
#define CLK_SPI1_REG            (0x0464)
#define CLK_SPI2_REG		(0x0468)
#define CLK_SPI3_REG		(0x046C)
#define CLK_SDMC0_REG           (0x0470)
#define CLK_SDMC1_REG           (0x0474)
#define CLK_SDMC2_REG           (0x0478)
#define CLK_PBUS_REG            (0x04A0)
#define CLK_SYSCFG_REG          (0x0800)
#define CLK_RTC_REG             (0x0908)
#define CLK_SPIENC_REG          (0x0810)
#define CLK_PWMCS_REG		(0x0814)
#define CLK_PSADC_REG		(0x0818)
#define CLK_MTOP_REG		(0x081C)
#define CLK_I2S0_REG            (0x0820)
#define CLK_I2S1_REG            (0x0824)
#define CLK_CODEC_REG           (0x0830)
#define CLK_RGB_REG             (0x0880)
#define CLK_LVDS_REG            (0x0884)
#define CLK_MIPIDSI_REG         (0x0888)
#define CLK_DE_REG              (0x08c0)
#define CLK_GE_REG              (0x08c4)
#define CLK_VE_REG              (0x08c8)
#define CLK_WDOG_REG            (0x020C)
#define CLK_SID_REG             (0x0904)
#define CLK_GTC_REG             (0x090C)
#define CLK_GPIO_REG            (0x083C)
#define CLK_UART0_REG           (0x0840)
#define CLK_UART1_REG           (0x0844)
#define CLK_UART2_REG           (0x0848)
#define CLK_UART3_REG           (0x084C)
#define CLK_UART4_REG           (0x0850)
#define CLK_UART5_REG           (0x0854)
#define CLK_UART6_REG           (0x0858)
#define CLK_UART7_REG           (0x085C)
#define CLK_I2C0_REG            (0x0960)
#define CLK_I2C1_REG            (0x0964)
#define CLK_I2C2_REG            (0x0968)
#define CLK_I2C3_REG            (0x096C)
#define CLK_CAN0_REG            (0x0980)
#define CLK_CAN1_REG            (0x0984)
#define CLK_PWM_REG             (0x0990)
#define CLK_ADCIM_REG           (0x09A0)
#define CLK_GPAI_REG            (0x09A4)
#define CLK_RTP_REG             (0x09A8)
#define CLK_TSEN_REG            (0x09AC)
#define CLK_CIR_REG             (0x09B0)

#define RESET_DESC(_id, _reg)	{.id = _id, .bit = 13, .reg = _reg}
static struct artinchip_reset	rest_info[] = {
	/*              id              reg             */
	RESET_DESC(RESET_DMA,           CLK_DMA_REG),
	RESET_DESC(RESET_CE,            CLK_CE_REG),
	RESET_DESC(RESET_USBD,          CLK_USBD_REG),
	RESET_DESC(RESET_USBH0,         CLK_USBH0_REG),
	RESET_DESC(RESET_USBH1,         CLK_USBH1_REG),
	RESET_DESC(RESET_USBPHY0,       CLK_USBPHY0_REG),
	RESET_DESC(RESET_USBPHY1,       CLK_USBPHY1_REG),
	RESET_DESC(RESET_GMAC0,         CLK_GMAC0_REG),
	RESET_DESC(RESET_GMAC1,         CLK_GMAC1_REG),
	RESET_DESC(RESET_SPI0,          CLK_SPI0_REG),
	RESET_DESC(RESET_SPI1,          CLK_SPI1_REG),
	RESET_DESC(RESET_SDMC0,         CLK_SDMC0_REG),
	RESET_DESC(RESET_SDMC1,         CLK_SDMC1_REG),
	RESET_DESC(RESET_SDMC2,         CLK_SDMC2_REG),
	RESET_DESC(RESET_PBUS,          CLK_PBUS_REG),
	RESET_DESC(RESET_SYSCFG,        CLK_SYSCFG_REG),
	RESET_DESC(RESET_RTC,           CLK_RTC_REG),
	RESET_DESC(RESET_SPIENC,        CLK_SPIENC_REG),
	RESET_DESC(RESET_I2S0,          CLK_I2S0_REG),
	RESET_DESC(RESET_I2S1,          CLK_I2S1_REG),
	RESET_DESC(RESET_CODEC,         CLK_CODEC_REG),
	RESET_DESC(RESET_RGB,           CLK_RGB_REG),
	RESET_DESC(RESET_LVDS,          CLK_LVDS_REG),
	RESET_DESC(RESET_MIPIDSI,       CLK_MIPIDSI_REG),
	RESET_DESC(RESET_DE,            CLK_DE_REG),
	RESET_DESC(RESET_GE,            CLK_GE_REG),
	RESET_DESC(RESET_VE,            CLK_VE_REG),
	RESET_DESC(RESET_WDOG,          CLK_WDOG_REG),
	RESET_DESC(RESET_SID,           CLK_SID_REG),
	RESET_DESC(RESET_GTC,           CLK_GTC_REG),
	RESET_DESC(RESET_GPIO,          CLK_GPIO_REG),
	RESET_DESC(RESET_UART0,         CLK_UART0_REG),
	RESET_DESC(RESET_UART1,         CLK_UART1_REG),
	RESET_DESC(RESET_UART2,         CLK_UART2_REG),
	RESET_DESC(RESET_UART3,         CLK_UART3_REG),
	RESET_DESC(RESET_UART4,         CLK_UART4_REG),
	RESET_DESC(RESET_UART5,         CLK_UART5_REG),
	RESET_DESC(RESET_UART6,         CLK_UART6_REG),
	RESET_DESC(RESET_UART7,         CLK_UART7_REG),
	RESET_DESC(RESET_I2C0,          CLK_I2C0_REG),
	RESET_DESC(RESET_I2C1,          CLK_I2C1_REG),
	RESET_DESC(RESET_I2C2,          CLK_I2C2_REG),
	RESET_DESC(RESET_I2C3,          CLK_I2C3_REG),
	RESET_DESC(RESET_CAN0,          CLK_CAN0_REG),
	RESET_DESC(RESET_CAN1,          CLK_CAN1_REG),
	RESET_DESC(RESET_PWM,           CLK_PWM_REG),
	RESET_DESC(RESET_ADCIM,         CLK_ADCIM_REG),
	RESET_DESC(RESET_GPAI,          CLK_GPAI_REG),
	RESET_DESC(RESET_RTP,           CLK_RTP_REG),
	RESET_DESC(RESET_TSEN,          CLK_TSEN_REG),
	RESET_DESC(RESET_CIR,		CLK_CIR_REG)
};

static int aic_reset_probe(struct udevice *dev)
{
	struct artinchip_reset_priv *priv = dev_get_priv(dev);
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	struct aic_reset_plat *plat = dev_get_plat(dev);
	struct dtd_artinchip_aic_reset_v1_0 *dtplat = &plat->dtplat;

	priv->base = map_sysmem(dtplat->reg[0], dtplat->reg[1]);
#else
	priv->base = dev_read_addr_ptr(dev);
#endif
	priv->count = ARRAY_SIZE(rest_info);
	priv->max_id = RESET_NUMBER;
	priv->rests = rest_info;

	return 0;
}

static const struct udevice_id aic_clk_ids[] = {
	{ .compatible = "artinchip,aic-reset-v1.0", },
	{ .compatible = "artinchip,aic-reset-v1.3", },
	{ }
};

static struct reset_ops aic_reset_ops = {
	.request      = artinchip_reset_request,
	.rfree         = artinchip_reset_free,
	.rst_assert   = artinchip_reset_assert,
	.rst_deassert = artinchip_reset_deassert,
};

U_BOOT_DRIVER(aic_reset_v1_0) = {
	.name                     = "artinchip_aic_reset_v1_0",
	.id                       = UCLASS_RESET,
	.of_match                 = aic_clk_ids,
	.ops                      = &aic_reset_ops,
	.probe                    = aic_reset_probe,
	.priv_auto                = sizeof(struct artinchip_reset_priv),
	.plat_auto                = sizeof(struct aic_reset_plat),
};

//U_BOOT_DRIVER(aic_reset_v1_3) = {
//	.name                     = "artinchip_aic_reset_v1_3",
//	.id                       = UCLASS_RESET,
//	.of_match                 = aic_clk_ids,
//	.ops                      = &aic_reset_ops,
//	.probe                    = aic_reset_probe,
//	.priv_auto                = sizeof(struct artinchip_reset_priv),
//	.plat_auto                = sizeof(struct aic_reset_plat),
//};
