// SPDX-License-Identifier: GPL-2.0+
/*
 * SPI Bus Encryption driver for ArtInChip SPI Enc device
 *
 * Copyright (c) 2021, ArtInChip Technology Co., Ltd
 * Dehuang Wu <dehuang.wu@artinchip.com>
 */

#include <common.h>
#include <asm/io.h>
#include <dm.h>
#include <dm/device.h>
#include <dm/device_compat.h>
#include <clk.h>
#include <reset.h>
#include <misc.h>
#include <artinchip/aic_spienc.h>

#define SPIE_REG_CTL              0x00
#define SPIE_REG_ICR              0x04
#define SPIE_REG_ISR              0x08
#define SPIE_REG_KCNT             0x0C
#define SPIE_REG_OCNT             0x10
#define SPIE_REG_ADDR             0x14
#define SPIE_REG_TWEAK            0x18
#define SPIE_REG_CPOS             0x1C
#define SPIE_REG_CLEN             0x20

#define SPIE_START_OFF            0
#define SPIE_SPI_SEL_OFF          12

#define SPIE_START_MSK            (0x1 << SPIE_START_OFF)
#define SPIE_SPI_SEL_MSK          (0x3 << SPIE_SPI_SEL_OFF)

#define SPIE_INTR_KEY_GEN_MSK     (1 << 0)
#define SPIE_INTR_ENC_DEC_FIN_MSK (1 << 1)
#define SPIE_INTR_ALL_EMP_MSK     (1 << 2)
#define SPIE_INTR_HALF_EMP_MSK    (1 << 3)
#define SPIE_INTR_KEY_UDF_MSK     (1 << 4)
#define SPIE_INTR_KEY_OVF_MSK     (1 << 5)
#define SPIE_INTR_ALL_MSK         (0x3F)

#define SPI_CTLR_0                0
#define SPI_CTLR_1                1
#define SPI_CTLR_INVAL            0xFF

struct aic_spienc_platdata {
	void __iomem *base;
	struct clk clk;
	struct reset_ctl reset;
	u32 tweak;
	u32 tweak_sel;
	u32 bypass;
};

static int aic_spienc_attach_bus(struct udevice *dev, u32 bus)
{
	struct aic_spienc_platdata *plat = dev_get_plat(dev);
	u32 val;

	val = readl(plat->base + SPIE_REG_CTL);
	val &= ~SPIE_SPI_SEL_MSK;

	/* Attach SPI Bus */
	switch (bus) {
	case SPI_CTLR_0:
		val |= (1 << SPIE_SPI_SEL_OFF);
		writel(val, (plat->base + SPIE_REG_CTL));
		break;
	case SPI_CTLR_1:
		val |= (2 << SPIE_SPI_SEL_OFF);
		writel(val, (plat->base + SPIE_REG_CTL));
		break;
	case SPI_CTLR_INVAL:
		/* Clear SPI SEL to zero. */
		val |= (0 << SPIE_SPI_SEL_OFF);
		writel(val, (plat->base + SPIE_REG_CTL));
		break;
	default:
		val |= (0 << SPIE_SPI_SEL_OFF);
		writel(val, (plat->base + SPIE_REG_CTL));
		dev_err(dev, "Wrong SPI Controller ID In DTS\n");
		return -EINVAL;
	}

	return 0;
}

static int aic_spienc_ioctl(struct udevice *dev, unsigned long req, void *buf)
{
	struct aic_spienc_platdata *plat = dev_get_plat(dev);
	struct spienc_crypt_cfg *cfg;
	u32 val, *p, tweak;

	p = (u32 *)buf;
	switch (req) {
	case AIC_SPIENC_IOCTL_CRYPT_CFG:
		cfg = (struct spienc_crypt_cfg *)buf;
		if (!cfg)
			return -EINVAL;
		if (plat->tweak_sel == AIC_SPIENC_HW_TWEAK) {
			tweak = 0;
		} else {
			tweak = plat->tweak;
			if (cfg->tweak)
				tweak = cfg->tweak;
		}

		if (plat->bypass)
			aic_spienc_attach_bus(dev, SPI_CTLR_INVAL);
		else
			aic_spienc_attach_bus(dev, cfg->spi_id);
		writel(cfg->addr, (plat->base + SPIE_REG_ADDR));
		writel(cfg->cpos, (plat->base + SPIE_REG_CPOS));
		writel(cfg->clen, (plat->base + SPIE_REG_CLEN));
		writel(tweak, (plat->base + SPIE_REG_TWEAK));

		break;
	case AIC_SPIENC_IOCTL_START:
		writel(SPIE_INTR_ALL_MSK, (plat->base + SPIE_REG_ISR));
		val = readl((plat->base + SPIE_REG_CTL));
		val |= SPIE_START_MSK;
		writel(val, (plat->base + SPIE_REG_CTL));
		break;
	case AIC_SPIENC_IOCTL_STOP:
		val = readl((plat->base + SPIE_REG_CTL));
		val &= ~SPIE_START_MSK;
		writel(val, (plat->base + SPIE_REG_CTL));
		break;
	case AIC_SPIENC_IOCTL_BYPASS:
		if (buf)
			plat->bypass = 1;
		else
			plat->bypass = 0;
		break;
	case AIC_SPIENC_IOCTL_CHECK_EMPTY:
		if (!p)
			return -EINVAL;
		val = readl((plat->base + SPIE_REG_ISR));
		if (val & SPIE_INTR_ALL_EMP_MSK)
			*p = 1;
		else
			*p = 0;
		writel(val, (plat->base + SPIE_REG_ISR));
		break;
	case AIC_SPIENC_IOCTL_TWEAK_SELECT:
		if (buf)
			plat->tweak_sel = AIC_SPIENC_HW_TWEAK;
		else
			plat->tweak_sel = AIC_SPIENC_USER_TWEAK;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static inline int aic_spienc_set_clock(struct udevice *dev, bool enable)
{
	int ret = 0;
#if !defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_CLK_ARTINCHIP)
	struct aic_spienc_platdata *plat = dev_get_plat(dev);

	if (!enable) {
		clk_disable(&plat->clk);
		if (reset_valid(&plat->reset))
			reset_assert(&plat->reset);
		return 0;
	}

	ret = clk_enable(&plat->clk);
	if (ret) {
		dev_err(dev, "failed to enable clock (ret=%d)\n", ret);
		return ret;
	}

	if (reset_valid(&plat->reset)) {
		ret = reset_deassert(&plat->reset);
		if (ret) {
			dev_err(dev, "failed to deassert reset\n");
			goto err;
		}
	}
	return 0;

err:
	clk_disable(&plat->clk);
#endif
	return ret;
}

static int aic_spienc_probe(struct udevice *dev)
{
	struct aic_spienc_platdata *plat = dev_get_plat(dev);
	int ret = 0;

#if !defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_CLK_ARTINCHIP)
	ret = clk_get_by_index(dev, 0, &plat->clk);
	if (ret < 0)
		return ret;

	ret = reset_get_by_index(dev, 0, &plat->reset);
	if (ret && ret != -ENOENT) {
		dev_err(dev, "failed to get reset\n");
		return ret;
	}
#endif

	plat->base = (void *)devfdt_get_addr(dev);

	ret = aic_spienc_set_clock(dev, true);
	plat->tweak = dev_read_u32_default(dev, "aic,spienc-tweak", 0);
	if (plat->tweak)
		writel(plat->tweak, (plat->base + SPIE_REG_TWEAK));

	/* Enable Interrupt */
	writel(SPIE_INTR_ALL_MSK, (plat->base + SPIE_REG_ICR));

	return ret;
}

static const struct misc_ops aic_spienc_ops = {
	.ioctl = aic_spienc_ioctl,
};

static const struct udevice_id aic_spienc_ids[] = {
	{ .compatible = "artinchip,aic-spienc-v1.0" },
	{ }
};

U_BOOT_DRIVER(artinchip_spienc) = {
	.name      = "artinchip_aic_spienc",
	.id        = UCLASS_MISC,
	.ops       = &aic_spienc_ops,
	.of_match  = aic_spienc_ids,
	.probe     = aic_spienc_probe,
	.plat_auto = sizeof(struct aic_spienc_platdata),
};
