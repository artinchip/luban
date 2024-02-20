// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Device Tree support for ArtInChip SoCs
 *
 * Copyright (c) 2020 ArtInChip Inc.
 */
#include <linux/mfd/syscon.h>
#include <linux/mod_devicetable.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <linux/io.h>
#include <dt-bindings/reset/artinchip,aic-reset.h>

#ifdef CONFIG_CLK_ARTINCHIP_V10
#define WDOG_CLK_CTRL_REG		(0x020C)
#endif
#define DMA_CLK_CTRL_REG		(0x0410)
#define CE_CLK_CTRL_REG			(0x0418)
#define USBD_CLK_CTRL_REG		(0x041C)
#define USBH0_CLK_CTRL_REG		(0x0420)
#define USBH1_CLK_CTRL_REG		(0x0424)
#define USBPHY0_CLK_CTRL_REG		(0x0430)
#define USBPHY1_CLK_CTRL_REG		(0x0434)
#define GMAC0_CLK_CTRL_REG		(0x0440)
#define GMAC1_CLK_CTRL_REG		(0x0444)
#define SPI0_CLK_CTRL_REG		(0x0460)
#define SPI1_CLK_CTRL_REG		(0x0464)
#define SPI2_CLK_CTRL_REG		(0x0468)
#define SPI3_CLK_CTRL_REG		(0x046C)
#define SDMC0_CLK_CTRL_REG		(0x0470)
#define SDMC1_CLK_CTRL_REG		(0x0474)
#define SDMC2_CLK_CTRL_REG		(0x0478)
#define PBUS_CLK_CTRL_REG		(0x04A0)
#define SYSCON_CLK_CTRL_REG		(0x0800)
#ifdef CONFIG_CLK_ARTINCHIP_V01
#define RTC_CLK_CTRL_REG		(0x0804)
#endif
#define SPIENC_CLK_CTRL_REG		(0x0810)
#define PWMCS_CLK_CTRL_REG		(0x0814)
#define PSADC_CLK_CTRL_REG		(0x0818)
#define MTOP_CLK_CTRL_REG		(0x081C)
#define I2S0_CLK_CTRL_REG		(0x0820)
#define I2S1_CLK_CTRL_REG		(0x0824)
#define CODEC_CLK_CTRL_REG		(0x0830)
#ifdef CONFIG_CLK_ARTINCHIP_V10
#define GPIO_CLK_CTRL_REG		(0x083C)
#define UART0_CLK_CTRL_REG		(0x0840)
#define UART1_CLK_CTRL_REG		(0x0844)
#define UART2_CLK_CTRL_REG		(0x0848)
#define UART3_CLK_CTRL_REG		(0x084C)
#define UART4_CLK_CTRL_REG		(0x0850)
#define UART5_CLK_CTRL_REG		(0x0854)
#define UART6_CLK_CTRL_REG		(0x0858)
#define UART7_CLK_CTRL_REG		(0x085C)
#endif
#define RGB_CLK_CTRL_REG		(0x0880)
#define LVDS_CLK_CTRL_REG		(0x0884)
#define MIPID_CLK_CTRL_REG		(0x0888)
#define DVP_CLK_CTRL_REG		(0x0890)
#define DE_CLK_CTRL_REG			(0x08c0)
#define GE_CLK_CTRL_REG			(0x08c4)
#define VE_CLK_CTRL_REG			(0x08c8)
#ifdef CONFIG_CLK_ARTINCHIP_V01
#define WDOG_CLK_CTRL_REG		(0x0900)
#endif
#define SID_CLK_CTRL_REG		(0x0904)
#ifdef CONFIG_CLK_ARTINCHIP_V10
#define RTC_CLK_CTRL_REG		(0x0908)
#define GTC_CLK_CTRL_REG		(0x090C)
#endif
#ifdef CONFIG_CLK_ARTINCHIP_V01
#define GTC_CLK_CTRL_REG		(0x0908)
#define GPIO_CLK_CTRL_REG		(0x091C)
#define UART0_CLK_CTRL_REG		(0x0920)
#define UART1_CLK_CTRL_REG		(0x0924)
#define UART2_CLK_CTRL_REG		(0x0928)
#define UART3_CLK_CTRL_REG		(0x092C)
#define UART4_CLK_CTRL_REG		(0x0930)
#define UART5_CLK_CTRL_REG		(0x0934)
#define UART6_CLK_CTRL_REG		(0x0938)
#define UART7_CLK_CTRL_REG		(0x093C)
#endif
#define I2C0_CLK_CTRL_REG		(0x0960)
#define I2C1_CLK_CTRL_REG		(0x0964)
#define I2C2_CLK_CTRL_REG		(0x0968)
#define I2C3_CLK_CTRL_REG		(0x096C)
#define CAN0_CLK_CTRL_REG		(0x0980)
#define CAN1_CLK_CTRL_REG		(0x0984)
#define PWM_CLK_CTRL_REG		(0x0990)
#define ADCIM_CLK_CTRL_REG		(0x09A0)
#define GPAI_CLK_CTRL_REG		(0x09A4)
#define RTP_CLK_CTRL_REG		(0x09A8)
#define TSEN_CLK_CTRL_REG		(0x09AC)
#define CIR_CLK_CTRL_REG		(0x09B0)


struct aic_reset_signal {
	u32		offset;
	u32		bit;
};

struct aic_reset_variant {
	const struct aic_reset_signal *signals;
	u32 signals_num;
	struct reset_control_ops ops;
};

struct aic_reset {
	struct reset_controller_dev rcdev;
	void __iomem	*reg_base;
	const struct aic_reset_signal *signals;
};

static const struct aic_reset_signal aic_reset_signals[RESET_NUMBER] = {
	[RESET_DMA]		= { DMA_CLK_CTRL_REG, BIT(13) },
	[RESET_CE]		= { CE_CLK_CTRL_REG, BIT(13) },
	[RESET_USBD]	= { USBD_CLK_CTRL_REG, BIT(13) },
	[RESET_USBH0]	= { USBH0_CLK_CTRL_REG, BIT(13) },
	[RESET_USBH1]	= { USBH1_CLK_CTRL_REG, BIT(13) },
	[RESET_USBPHY0] = { USBPHY0_CLK_CTRL_REG, BIT(13) },
	[RESET_USBPHY1] = { USBPHY1_CLK_CTRL_REG, BIT(13) },
	[RESET_GMAC0]	= { GMAC0_CLK_CTRL_REG, BIT(13) },
	[RESET_GMAC1]	= { GMAC1_CLK_CTRL_REG, BIT(13) },
	[RESET_SPI0]	= { SPI0_CLK_CTRL_REG, BIT(13) },
	[RESET_SPI1]	= { SPI1_CLK_CTRL_REG, BIT(13) },
	[RESET_SPI2]	= { SPI2_CLK_CTRL_REG, BIT(13) },
	[RESET_SPI3]	= { SPI3_CLK_CTRL_REG, BIT(13) },
	[RESET_SDMC0]	= { SDMC0_CLK_CTRL_REG, BIT(13) },
	[RESET_SDMC1]	= { SDMC1_CLK_CTRL_REG, BIT(13) },
	[RESET_SDMC2]	= { SDMC2_CLK_CTRL_REG, BIT(13) },
	[RESET_PBUS]	= { PBUS_CLK_CTRL_REG, BIT(13) },
	[RESET_SYSCFG]  = { SYSCON_CLK_CTRL_REG, BIT(13) },
	[RESET_RTC]     = { RTC_CLK_CTRL_REG, BIT(13) },
	[RESET_SPIENC]	= { SPIENC_CLK_CTRL_REG, BIT(13) },
	[RESET_I2S0]	= { I2S0_CLK_CTRL_REG, BIT(13) },
	[RESET_I2S1]	= { I2S1_CLK_CTRL_REG, BIT(13) },
	[RESET_CODEC]	= { CODEC_CLK_CTRL_REG, BIT(13) },
	[RESET_RGB]		= { RGB_CLK_CTRL_REG, BIT(13) },
	[RESET_LVDS]	= { LVDS_CLK_CTRL_REG, BIT(13) },
	[RESET_MIPIDSI]	= { MIPID_CLK_CTRL_REG, BIT(13) },
	[RESET_DE]		= { DE_CLK_CTRL_REG, BIT(13) },
	[RESET_GE]		= { GE_CLK_CTRL_REG, BIT(13) },
	[RESET_VE]		= { VE_CLK_CTRL_REG, BIT(13) },
	[RESET_WDOG]    = { WDOG_CLK_CTRL_REG, BIT(13) },
	[RESET_SID]     = { SID_CLK_CTRL_REG, BIT(13) },
	[RESET_GTC]     = { GTC_CLK_CTRL_REG, BIT(13) },
	[RESET_GPIO]	= { GPIO_CLK_CTRL_REG, BIT(13) },
	[RESET_UART0]	= { UART0_CLK_CTRL_REG, BIT(13) },
	[RESET_UART1]	= { UART1_CLK_CTRL_REG, BIT(13) },
	[RESET_UART2]	= { UART2_CLK_CTRL_REG, BIT(13) },
	[RESET_UART3]	= { UART3_CLK_CTRL_REG, BIT(13) },
	[RESET_UART4]	= { UART4_CLK_CTRL_REG, BIT(13) },
	[RESET_UART5]	= { UART5_CLK_CTRL_REG, BIT(13) },
	[RESET_UART6]	= { UART6_CLK_CTRL_REG, BIT(13) },
	[RESET_UART7]	= { UART7_CLK_CTRL_REG, BIT(13) },
	[RESET_I2C0]	= { I2C0_CLK_CTRL_REG, BIT(13) },
	[RESET_I2C1]	= { I2C1_CLK_CTRL_REG, BIT(13) },
	[RESET_I2C2]	= { I2C2_CLK_CTRL_REG, BIT(13) },
	[RESET_I2C3]	= { I2C3_CLK_CTRL_REG, BIT(13) },
	[RESET_CAN0]	= { CAN0_CLK_CTRL_REG, BIT(13) },
	[RESET_CAN1]	= { CAN1_CLK_CTRL_REG, BIT(13) },
	[RESET_PWM]		= { PWM_CLK_CTRL_REG, BIT(13) },
	[RESET_ADCIM]   = { ADCIM_CLK_CTRL_REG, BIT(13) },
	[RESET_GPAI]	= { GPAI_CLK_CTRL_REG, BIT(13) },
	[RESET_RTP]     = { RTP_CLK_CTRL_REG, BIT(13) },
	[RESET_TSEN]	= { TSEN_CLK_CTRL_REG, BIT(13) },
	[RESET_CIR]	= { CIR_CLK_CTRL_REG, BIT(13) },
	[RESET_DVP]     = { DVP_CLK_CTRL_REG, BIT(13)},
	[RESET_MTOP]	= { MTOP_CLK_CTRL_REG, BIT(13)},
	[RESET_PSADC]	= { PSADC_CLK_CTRL_REG, BIT(13)},
	[RESET_PWMCS]	= { PWMCS_CLK_CTRL_REG, BIT(13)},
};

static struct aic_reset *to_aic_reset(struct reset_controller_dev *rcdev)
{
	return container_of(rcdev, struct aic_reset, rcdev);
}

static void aic_reset_set(struct reset_controller_dev *rcdev,
		u32 id, bool assert)
{
	unsigned int value;
	struct aic_reset *aicreset = to_aic_reset(rcdev);

	value = readl(aicreset->reg_base + aicreset->signals[id].offset);
	if (assert == true)
		value &= ~aicreset->signals[id].bit;
	else
		value |= aicreset->signals[id].bit;
	writel(value, aicreset->reg_base + aicreset->signals[id].offset);
}

static int aic_reset_assert(struct reset_controller_dev *rcdev,
		unsigned long id)
{
	aic_reset_set(rcdev, id, true);
	return 0;
}

static int aic_reset_deassert(struct reset_controller_dev *rcdev,
			       unsigned long id)
{
	aic_reset_set(rcdev, id, false);
	return 0;
}

static int aic_reset_status(struct reset_controller_dev *rcdev,
				unsigned long id)
{
	unsigned int value;
	struct aic_reset *aicreset = to_aic_reset(rcdev);

	value = readl(aicreset->reg_base + aicreset->signals[id].offset);
	return !(value & BIT(13));
}

static const struct aic_reset_variant aic_reset_data = {
	.signals = aic_reset_signals,
	.signals_num = ARRAY_SIZE(aic_reset_signals),
	.ops = {
		.assert   = aic_reset_assert,
		.deassert = aic_reset_deassert,
		.status = aic_reset_status,
	},
};


static int aic_reset_probe(struct platform_device *pdev)
{
	struct aic_reset *aicreset;
	struct device *dev = &pdev->dev;
	const struct aic_reset_variant *variant;

	variant = of_device_get_match_data(dev);
	aicreset = devm_kzalloc(dev, sizeof(*aicreset), GFP_KERNEL);
	if (!aicreset)
		return -ENOMEM;

	if (!dev->of_node) {
		pr_err("%s(%d): of node is not exist!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	aicreset->reg_base = of_iomap(dev->of_node, 0);
	if (!aicreset->reg_base) {
		pr_err("%s: could not map reset region\n", __func__);
		return -ENOMEM;
	}

	aicreset->signals = variant->signals;
	aicreset->rcdev.owner     = THIS_MODULE;
	aicreset->rcdev.nr_resets = variant->signals_num;
	aicreset->rcdev.ops       = &variant->ops;
	aicreset->rcdev.of_node   = dev->of_node;

	return devm_reset_controller_register(dev, &aicreset->rcdev);
}

static const struct of_device_id aic_reset_dt_ids[] = {
	{ .compatible = "artinchip,aic-reset-v1.0", .data = &aic_reset_data },
	{ /* sentinel */ },
};

static struct platform_driver aic_reset_driver = {
	.probe	= aic_reset_probe,
	.driver = {
		.name		= "aic-reset",
		.of_match_table	= aic_reset_dt_ids,
	},
};

static int __init aic_reset_init(void)
{
	return platform_driver_register(&aic_reset_driver);
}
postcore_initcall(aic_reset_init);

