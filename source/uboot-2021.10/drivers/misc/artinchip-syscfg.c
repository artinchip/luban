// SPDX-License-Identifier: GPL-2.0-only
/*
 * Syscfg driver of ArtInChip SoC
 *
 * Copyright (C) 2020-2021 ArtInChip Technology Co., Ltd.
 * Authors:  Huahui Mai <huahui.mai@artinchip.com>
 */

#include <common.h>
#include <dm.h>
#include <misc.h>
#include <clk.h>
#include <reset.h>
#include <dm/ofnode.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <linux/delay.h>

DECLARE_GLOBAL_DATA_PTR;

/* Register of Syscfg */
#ifdef CONFIG_FPGA_BOARD_ARTINCHIP
#define SYSCFG_MMCM2_CTL	0xF0
#define SYSCFG_MMCM2_STA	0xF4
#define SYSCFG_LCD_IO_CFG	0xF8
#define SYSCFG_PHY_CFG		0xFC

#define SYSCFG_MMCM2_CTL_DRP_DIN_SHIFT		16
#define SYSCFG_MMCM2_CTL_DRP_DADDR_SHIFT	9
#define SYSCFG_MMCM2_CTL_DRP_DADDR_MASK		GENMASK(15, 9)
#define SYSCFG_MMCM2_CTL_DRP_DWE		BIT(8)
#define SYSCFG_MMCM2_CTL_DRP_START		BIT(4)
#define SYSCFG_MMCM2_CTL_DRP_RESET		BIT(0)

#define SYSCFG_MMCM2_STA_DRP_DOUT_SHIFT		16
#define SYSCFG_MMCM2_STA_DRP_DOUT_MASK		GENMASK(31, 16)

enum fpga_disp_clk {
	FPGA_DISP_CLK_RGB_SERIAL		= 0,
	FPGA_DISP_CLK_RGB_PARALLEL		= 1,
	FPGA_DISP_CLK_LVDS_SINGLE		= 2,
	FPGA_DISP_CLK_LVDS_DUAL			= 3,
	FPGA_DISP_CLK_MIPI_4LANE_RGB888		= 4,
	FPGA_DISP_CLK_MIPI_4LANE_RGB666		= 5,
	FPGA_DISP_CLK_MIPI_4LANE_RGB565		= 6,
};

enum fpga_mmcm_daddr {
	FPGA_MMCM_DADDR_CLKOUT0_CTL0		= 0x8,
	FPGA_MMCM_DADDR_CLKOUT0_CTL1		= 0x9,
	FPGA_MMCM_DADDR_CLKOUT1_CTL0		= 0xA,
	FPGA_MMCM_DADDR_CLKOUT1_CTL1		= 0xB,
	FPGA_MMCM_DADDR_CLKOUT2_CTL0		= 0xC,
	FPGA_MMCM_DADDR_CLKOUT2_CTL1		= 0xD,
	FPGA_MMCM_DADDR_CLKOUT3_CTL0		= 0xE,
	FPGA_MMCM_DADDR_CLKOUT3_CTL1		= 0xF,
};
#endif

#define SYSCFG_USB0_CFG		0x40C
#define SYSCFG_GMAC0_CFG	0x410
#define SYSCFG_GMAC1_CFG	0x414

#define SYSCFG_USB0_HOST_MODE			0
#define SYSCFG_USB0_DEVICE_MODE			1

#define SYSCFG_GMAC_RXDLY_SEL_SHIFT		24
#define SYSCFG_GMAC_RXDLY_SEL_MASK		GENMASK(28, 24)
#define SYSCFG_GMAC_TXDLY_SEL_SHIFT		16
#define SYSCFG_GMAC_TXDLY_SEL_MASK		GENMASK(20, 16)
#define SYSCFG_GMAC_SW_TXCLK_DIV2_SHIFT		12
#define SYSCFG_GMAC_SW_TXCLK_DIV2_MASK		GENMASK(15, 12)
#define SYSCFG_GMAC_SW_TXCLK_DIV1_SHIFT		8
#define SYSCFG_GMAC_SW_TXCLK_DIV1_MASK		GENMASK(11, 8)
#define SYSCFG_GMAC_SW_TXCLK_DIV_EN		BIT(5)
#define SYSCFG_GMAC_RMII_EXTCLK_SEL		BIT(4)
#define SYSCFG_GMAC_PHY_RGMII_1000M		BIT(0)

struct syscfg_dev {
	void __iomem *regs;
	struct udevice *dev;
	struct clk clk;
	struct reset_ctl rst;
};
static struct syscfg_dev *g_syscfg;

#ifdef CONFIG_FPGA_BOARD_ARTINCHIP

static s32 syscfg_fpga_drp_wr(void __iomem *regs, u8 addr, u16 data)
{
	void __iomem *ctl_reg = regs + SYSCFG_MMCM2_CTL;
	u32 cnt;

	writel(SYSCFG_MMCM2_CTL_DRP_RESET, ctl_reg);
	cnt = 20;
	while (cnt--)
		;

	writel((data << SYSCFG_MMCM2_CTL_DRP_DIN_SHIFT)
		| (addr << SYSCFG_MMCM2_CTL_DRP_DADDR_SHIFT)
		| SYSCFG_MMCM2_CTL_DRP_DWE | SYSCFG_MMCM2_CTL_DRP_START
		| SYSCFG_MMCM2_CTL_DRP_RESET, ctl_reg);

	while (readl(ctl_reg) & SYSCFG_MMCM2_CTL_DRP_START)
		;
	cnt = 20;
	while (cnt--)
		;

	writel(readl(ctl_reg) & ~SYSCFG_MMCM2_CTL_DRP_RESET, ctl_reg);
	return 0;
}

static u16 syscfg_fpga_drp_rd(void __iomem *regs, u16 addr)
{
	void __iomem *ctl_reg = regs + SYSCFG_MMCM2_CTL;
	void __iomem *sta_reg = regs + SYSCFG_MMCM2_STA;
	u32 val = readl(ctl_reg);

	val &= ~SYSCFG_MMCM2_CTL_DRP_DWE;
	val &= ~SYSCFG_MMCM2_CTL_DRP_DADDR_MASK;
	val |= (addr << SYSCFG_MMCM2_CTL_DRP_DADDR_SHIFT)
		| SYSCFG_MMCM2_CTL_DRP_START;

	writel(val, ctl_reg);
	while (readl(ctl_reg) & SYSCFG_MMCM2_CTL_DRP_START)
		;

	return readl(sta_reg) >> SYSCFG_MMCM2_STA_DRP_DOUT_SHIFT;
}

int syscfg_fpga_de_clk_sel_by_div(u8 sclk, u8 pixclk)
{
	u8  cntr;
	u16 data;
	struct syscfg_dev *syscfg = g_syscfg;

	cntr = sclk / 2;
	data = (1 << 12) | (cntr << 6) | cntr;

	syscfg_fpga_drp_wr(syscfg->regs, FPGA_MMCM_DADDR_CLKOUT2_CTL0, data);
	if (syscfg_fpga_drp_rd(syscfg->regs, FPGA_MMCM_DADDR_CLKOUT2_CTL0)
				!= data) {
		debug("Failed to set clkout2\n");
		return -1;
	}

	cntr = pixclk / 2;
	data = (1 << 12) | (cntr << 6) | cntr;

	syscfg_fpga_drp_wr(syscfg->regs, FPGA_MMCM_DADDR_CLKOUT3_CTL0, data);
	if (syscfg_fpga_drp_rd(syscfg->regs, FPGA_MMCM_DADDR_CLKOUT3_CTL0)
				!= data) {
		debug("Failed to set clkout3\n");
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(syscfg_fpga_de_clk_sel_by_div);

int syscfg_fpga_de_clk_sel(enum fpga_disp_clk type)
{
	u8 sclk_div[] = {10,  8,  4,  4,  4,  4,  4};
	u8 pixclk_div[] = {120, 32, 28, 14, 24, 18, 16};

	return syscfg_fpga_de_clk_sel_by_div(sclk_div[type], pixclk_div[type]);
}
EXPORT_SYMBOL_GPL(syscfg_fpga_de_clk_sel);

void syscfg_fpga_lcd_io_set(u32 val)
{
	writel(val, g_syscfg->regs + SYSCFG_LCD_IO_CFG);
}
EXPORT_SYMBOL_GPL(syscfg_fpga_lcd_io_set);

#endif

static int syscfg_usb_init(struct syscfg_dev *syscfg)
{
	// TODO: read some parameters in usb dts, and set it to syscfg register
	return 0;
}

static int syscfg_gmac_init(struct syscfg_dev *syscfg, int ch)
{
	ofnode np0, np1, np;
	void __iomem *cfg_reg = ch ? (syscfg->regs + SYSCFG_GMAC1_CFG)
				: (syscfg->regs + SYSCFG_GMAC0_CFG);
	s32 cfg, ret;
	u32 val;
	const char *str;

	np0 = ofnode_by_compatible(ofnode_null(), "artinchip,aic-gmac");
	if (!ofnode_valid(np0)) {
		debug("Can't find np0 in dts\n");
		return -1;
	}
	if (ch) {
		np1 = ofnode_by_compatible(np0, "artinchip,aic-gmac");
		if (!ofnode_valid(np1)) {
			debug("Can't find np0 in dts\n");
			return -1;
		}
		np = np1;
	} else {
		np = np0;
	}

	cfg = readl(cfg_reg);

	str = ofnode_read_string(np, "phy-mode");
	if (!str) {
		debug("Can't find phy-mode\n");
	} else {
		if (strcmp(str, "rmgii") == 0)
			cfg |= SYSCFG_GMAC_PHY_RGMII_1000M;
		else
			cfg &= ~SYSCFG_GMAC_PHY_RGMII_1000M;
	}

	ret = ofnode_read_u32(np, "max-speed", &val);
	if (ret) {
		debug("Can't find max-speed\n");
	} else {
		if (val == 1000)
			cfg |= SYSCFG_GMAC_PHY_RGMII_1000M;
		else
			cfg &= ~SYSCFG_GMAC_PHY_RGMII_1000M;
	}

	// TODO: read some parameters in gmac0/1 dts, set it to syscfg register
	writel(cfg, cfg_reg);
	return 0;
}

static int syscfg_bind(struct udevice *dev)
{
	struct syscfg_dev *syscfg = dev_get_plat(dev);
	int ret;

	syscfg->dev = dev;
	syscfg->regs = (void *)dev_read_addr(dev);
	if (IS_ERR(syscfg->regs))
		return PTR_ERR(syscfg->regs);

	ret = clk_get_by_index(dev, 0, &syscfg->clk);
	if (ret) {
		debug("no clock defined\n");
		return ret;
	}

	ret = reset_get_by_index(dev, 0, &syscfg->rst);
	if (ret) {
		debug("no reset defined\n");
		goto disable_clk;
	}

	reset_deassert(&syscfg->rst);

	g_syscfg = syscfg;
	syscfg_usb_init(syscfg);
	syscfg_gmac_init(syscfg, 0);
	syscfg_gmac_init(syscfg, 1);
#ifdef CONFIG_FPGA_BOARD_ARTINCHIP
	syscfg_fpga_de_clk_sel(FPGA_DISP_CLK_RGB_PARALLEL);
#endif

	debug("ArtInChip Syscfg Loaded\n");
	return 0;

	reset_assert(&syscfg->rst);
disable_clk:
	clk_disable(&syscfg->clk);
	return ret;
}

static const struct udevice_id aic_syscfg_ids[] = {
	{ .compatible = "artinchip,aic-syscfg" },
	{ }
};

U_BOOT_DRIVER(syscfg) = {
	.name      = "artinchip_syscfg",
	.id        = UCLASS_MISC,
	.of_match  = aic_syscfg_ids,
	.bind      = syscfg_bind,
	.plat_auto = sizeof(struct syscfg_dev),
};
