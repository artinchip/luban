// SPDX-License-Identifier: GPL-2.0-only
/*
 * Syscfg driver of Artinchip SoC
 *
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
 * Authors:  Matteo <duanmt@artinchip.com>
 */
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/reset.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/delay.h>

/* Register of Syscfg */
#ifdef CONFIG_DEBUG_ON_FPGA_BOARD_ARTINCHIP
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
	FPGA_DISP_CLK_RGB_SERIAL		 = 0,
	FPGA_DISP_CLK_RGB_PARALLEL		 = 1,
	FPGA_DISP_CLK_LVDS_SINGLE		 = 2,
	FPGA_DISP_CLK_LVDS_DUAL			 = 3,
	FPGA_DISP_CLK_MIPI_4LANE_RGB888		 = 4,
	FPGA_DISP_CLK_MIPI_4LANE_RGB666		 = 5,
	FPGA_DISP_CLK_MIPI_4LANE_RGB565		 = 6,
	FPGA_DISP_CLK_MIPI_3LANE_RGB888		 = 7,
	FPGA_DISP_CLK_MIPI_2LANE_RGB888		 = 8,
	FPGA_DISP_CLK_MIPI_1LANE_RGB888		 = 9,
	FPGA_DISP_CLK_I8080_24BIT		 = 10,
	FPGA_DISP_CLK_I8080_18BIT		 = 11,
	FPGA_DISP_CLK_I8080_16BIT_666_1		 = 12,
	FPGA_DISP_CLK_I8080_16BIT_666_2		 = 13,
	FPGA_DISP_CLK_I8080_16BIT_565		 = 14,
	FPGA_DISP_CLK_I8080_9BIT		 = 15,
	FPGA_DISP_CLK_I8080_8BIT_666		 = 16,
	FPGA_DISP_CLK_I8080_8BIT_565		 = 17,
	FPGA_DISP_CLK_SPI_4LINE_RGB888_OR_RGB666 = 18,
	FPGA_DISP_CLK_SPI_4LINE_RG565		 = 19,
	FPGA_DISP_CLK_SPI_3LINE_RGB888_OR_RGB666 = 20,
	FPGA_DISP_CLK_SPI_3LINE_RG565		 = 21,
	FPGA_DISP_CLK_SPI_4SDA_RGB888_OR_RGB666  = 22,
	FPGA_DISP_CLK_SPI_4SDA_RGB565		 = 23,
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
	FPGA_MMCM_DADDR_CLKOUT4_CTL0		= 0x10,
	FPGA_MMCM_DADDR_CLKOUT4_CTL1		= 0x11,
	FPGA_MMCM_DADDR_CLKOUT5_CTL0		= 0x06,
	FPGA_MMCM_DADDR_CLKOUT5_CTL1		= 0x07,
	FPGA_MMCM_DADDR_CLKOUT6_CTL0		= 0x12,
	FPGA_MMCM_DADDR_CLKOUT6_CTL1		= 0x13,
	FPGA_MMCM_DADDR_VCO_M_CTL0		= 0x14,
	FPGA_MMCM_DADDR_VCO_M_CTL1		= 0x15,
};

enum fpga_gmac_clk_t {
	FPGA_GMAC_CLK_25M			= 0,
	FPGA_GMAC_CLK_125M			= 1,
};

#endif

#define SYSCFG_LDO_CFG		0x20
#define SYSCFG_USB0_CFG		0x40C
#define SYSCFG_GMAC0_CFG	0x410
#define SYSCFG_GMAC1_CFG	0x414

#define SYSCFG_USB0_HOST_MODE			0
#define SYSCFG_USB0_DEVICE_MODE			1

#define SYSCFG_LDO_CFG_IBIAS_EN_SHIFT		16
#define SYSCFG_LDO_CFG_IBIAS_EN_MASK		GENMASK(17, 16)
#define SYSCFG_GMAC_REFCLK_INV   		BIT(29)
#define SYSCFG_GMAC_RXDLY_SEL_SHIFT		18
#define SYSCFG_GMAC_RXDLY_SEL_MASK		GENMASK(22, 18)
#define SYSCFG_GMAC_TXDLY_SEL_SHIFT		12
#define SYSCFG_GMAC_TXDLY_SEL_MASK		GENMASK(16, 12)
#define SYSCFG_GMAC_SW_TXCLK_DIV2_SHIFT		8
#define SYSCFG_GMAC_SW_TXCLK_DIV2_MASK		GENMASK(11, 8)
#define SYSCFG_GMAC_SW_TXCLK_DIV1_SHIFT		4
#define SYSCFG_GMAC_SW_TXCLK_DIV1_MASK		GENMASK(7, 4)
#define SYSCFG_GMAC_SW_TXCLK_DIV_EN		BIT(2)
#define SYSCFG_GMAC_RMII_EXTCLK_SEL		BIT(1)
#define SYSCFG_GMAC_PHY_RGMII_1000M		BIT(0)

struct syscfg_dev {
	void __iomem *regs;
	struct device *dev;
	struct clk *clk;
	struct reset_control *rst;
};
static struct syscfg_dev *g_syscfg;

void syscfg_usb_phy0_sw_host(int sw)
{
	if (sw)
		writel(SYSCFG_USB0_HOST_MODE,
		       g_syscfg->regs + SYSCFG_USB0_CFG);
	else
		writel(SYSCFG_USB0_DEVICE_MODE,
		       g_syscfg->regs + SYSCFG_USB0_CFG);
}
EXPORT_SYMBOL_GPL(syscfg_usb_phy0_sw_host);

#ifdef CONFIG_DEBUG_ON_FPGA_BOARD_ARTINCHIP

static DEFINE_SPINLOCK(user_lock);

static s32 syscfg_fpga_drp_wr(void __iomem *regs, u8 addr, u16 data)
{
	void __iomem *ctl_reg = regs + SYSCFG_MMCM2_CTL;

	writel(SYSCFG_MMCM2_CTL_DRP_RESET, ctl_reg);
	usleep_range(10, 20);

	writel((data << SYSCFG_MMCM2_CTL_DRP_DIN_SHIFT)
		| (addr << SYSCFG_MMCM2_CTL_DRP_DADDR_SHIFT)
		| SYSCFG_MMCM2_CTL_DRP_DWE | SYSCFG_MMCM2_CTL_DRP_START
		| SYSCFG_MMCM2_CTL_DRP_RESET, ctl_reg);

	while (readl(ctl_reg) & SYSCFG_MMCM2_CTL_DRP_START)
		;
	usleep_range(10, 20);

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

int syscfg_fpga_cfg_vco(u32 freq)
{
	u8  cntr;
	u16 data;
	struct syscfg_dev *syscfg = g_syscfg;

	cntr = freq / 2;

	if ((freq % 2) == 0)
		data = 1 << 12 | cntr << 6 | cntr;
	else
		data = 1 << 12 | cntr << 6 | (cntr + 1);

	if (cntr > 0) {
		syscfg_fpga_drp_wr(syscfg->regs,
					FPGA_MMCM_DADDR_VCO_M_CTL0, data);
		if (syscfg_fpga_drp_rd(syscfg->regs, FPGA_MMCM_DADDR_VCO_M_CTL0)
				!= data)
			return -1;
	} else {
		syscfg_fpga_drp_wr(syscfg->regs,
					FPGA_MMCM_DADDR_VCO_M_CTL1, 0x40);
	}

	return 0;
}

int syscfg_fpga_de_clk_sel_by_div(u8 sclk, u8 pixclk)
{
	u8  cntr;
	u16 data;
	struct syscfg_dev *syscfg = g_syscfg;

	cntr = sclk / 2;
	data = (1 << 12) | (cntr << 6) | cntr;

	spin_lock(&user_lock);
	if (cntr > 0) {
		syscfg_fpga_drp_wr(syscfg->regs,
					FPGA_MMCM_DADDR_CLKOUT2_CTL0, data);
		if (syscfg_fpga_drp_rd(syscfg->regs, FPGA_MMCM_DADDR_CLKOUT2_CTL0)
					!= data) {
			dev_err(syscfg->dev, "Failed to set clkout2\n");
			spin_unlock(&user_lock);
			return -1;
		}
	} else {
		syscfg_fpga_drp_wr(syscfg->regs,
					FPGA_MMCM_DADDR_CLKOUT2_CTL1, 0x40);
	}

	cntr = pixclk / 2;
	data = (1 << 12) | (cntr << 6) | cntr;

	if (cntr > 0) {
		syscfg_fpga_drp_wr(syscfg->regs,
					FPGA_MMCM_DADDR_CLKOUT3_CTL0, data);
		if (syscfg_fpga_drp_rd(syscfg->regs, FPGA_MMCM_DADDR_CLKOUT3_CTL0)
					!= data) {
			dev_err(syscfg->dev, "Failed to set clkout3\n");
			spin_unlock(&user_lock);
			return -1;
		}
	} else {
		syscfg_fpga_drp_wr(syscfg->regs,
					FPGA_MMCM_DADDR_CLKOUT3_CTL1, 0x40);
	}

	spin_unlock(&user_lock);
	return 0;
}
EXPORT_SYMBOL_GPL(syscfg_fpga_de_clk_sel_by_div);

int syscfg_fpga_de_clk_sel(enum fpga_disp_clk type)
{
	u8 sclk_div[] = {10,  8,  4,  4,  4,  4,  4,  4,  4,  4,  3,  3,  1,
		3,  6,   6,   4,   6,  1,   2,   1,  2,  4,  6};
	u8 pixclk_div[] = {120, 32, 28, 14, 24, 18, 16, 32, 48, 96, 30, 30, 15,
		60, 60, 120, 120, 120, 96, 128, 108, 72, 96, 96};

	if (type > 17)
		syscfg_fpga_cfg_vco(3);

	return syscfg_fpga_de_clk_sel_by_div(sclk_div[type], pixclk_div[type]);
}
EXPORT_SYMBOL_GPL(syscfg_fpga_de_clk_sel);

void syscfg_fpga_lcd_io_set(u32 val)
{
	spin_lock(&user_lock);
	writel(val, g_syscfg->regs + SYSCFG_LCD_IO_CFG);
	spin_unlock(&user_lock);
}
EXPORT_SYMBOL_GPL(syscfg_fpga_lcd_io_set);

void syscfg_fpga_gmac_clk_sel(u32 id)
{
	u8 fpga_mmcm2_div_gmac_clk[] = { 40,  8};
	struct syscfg_dev *syscfg = g_syscfg;
	u8  cntr;
	u16 data;

	cntr = fpga_mmcm2_div_gmac_clk[id] / 2;
	data = 1 << 12 | cntr << 6 | cntr;

	syscfg_fpga_drp_wr(syscfg->regs, FPGA_MMCM_DADDR_CLKOUT0_CTL0, data);
	if (syscfg_fpga_drp_rd(syscfg->regs, FPGA_MMCM_DADDR_CLKOUT0_CTL0)
			      != data)
		return;

	syscfg_fpga_drp_wr(syscfg->regs, FPGA_MMCM_DADDR_CLKOUT1_CTL0, data);
	if (syscfg_fpga_drp_rd(syscfg->regs, FPGA_MMCM_DADDR_CLKOUT1_CTL0)
			      != data)
		return;
}
EXPORT_SYMBOL_GPL(syscfg_fpga_gmac_clk_sel);
#endif

static int syscfg_usb_init(struct syscfg_dev *syscfg)
{
	// TODO: read some parameters in usb dts, and set it to syscfg register
	return 0;
}

int syscfg_ldo_init(u32 data)
{
	struct syscfg_dev *syscfg = g_syscfg;
	void __iomem *ctl_reg = syscfg->regs + SYSCFG_LDO_CFG;

	u32 val = readl(ctl_reg);

	val &= ~SYSCFG_LDO_CFG_IBIAS_EN_MASK;
	val |= data << SYSCFG_LDO_CFG_IBIAS_EN_SHIFT;

	writel(val, ctl_reg);
	return 0;
}
EXPORT_SYMBOL_GPL(syscfg_ldo_init);

static int syscfg_gmac_init(struct syscfg_dev *syscfg, int ch)
{
	struct device_node *np0, *np1, *np;
	void __iomem *cfg_reg = ch ? (syscfg->regs + SYSCFG_GMAC1_CFG)
				: (syscfg->regs + SYSCFG_GMAC0_CFG);
	s8 name[12] = "ethernet";
	phy_interface_t phy_mode;
	int tx_delay, rx_delay;
	s32 cfg;

	np0 = of_find_node_by_name(NULL, name);
	if (!np0) {
		dev_err(syscfg->dev, "Can't find np0 in dts\n");
		return -1;
	}
	if (ch) {
		np1 = of_find_node_by_name(np0, name);
		of_node_put(np0);
		if (!np1) {
			dev_err(syscfg->dev, "Can't find np0 in dts\n");
			return -1;
		}
		np = np1;
	} else {
		np = np0;
	}

	cfg = readl(cfg_reg);

	if (of_get_phy_mode(np, &phy_mode)) {
		dev_info(syscfg->dev, "Can't find phy-mode\n");
	} else {
		if (phy_mode == PHY_INTERFACE_MODE_GMII || (phy_interface_mode_is_rgmii(phy_mode)))
			cfg |= SYSCFG_GMAC_PHY_RGMII_1000M;
		else
			cfg &= ~SYSCFG_GMAC_PHY_RGMII_1000M;
	}

	if (of_property_read_bool(np, "aic,use_extclk"))
		cfg |= SYSCFG_GMAC_RMII_EXTCLK_SEL;

	if (!of_property_read_u32(np, "aic,tx-delay", &tx_delay))
		cfg |= (tx_delay << SYSCFG_GMAC_TXDLY_SEL_SHIFT);

	if (!of_property_read_u32(np, "aic,rx-delay", &rx_delay))
		cfg |= (rx_delay << SYSCFG_GMAC_RXDLY_SEL_SHIFT);

	writel(cfg, cfg_reg);

	of_node_put(np);

	return 0;
}

static int syscfg_probe(struct platform_device *pdev)
{
	int ret;
	struct syscfg_dev *syscfg;

	syscfg = devm_kzalloc(&pdev->dev,
				sizeof(struct syscfg_dev), GFP_KERNEL);
	if (!syscfg)
		return -ENOMEM;
	syscfg->dev = &pdev->dev;

	syscfg->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(syscfg->regs))
		return PTR_ERR(syscfg->regs);

	syscfg->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(syscfg->clk)) {
		dev_err(&pdev->dev, "no clock defined\n");
		return PTR_ERR(syscfg->clk);
	}

	ret = clk_prepare_enable(syscfg->clk);
	if (ret)
		return ret;

	syscfg->rst = devm_reset_control_get_optional_shared(&pdev->dev, NULL);
	if (IS_ERR(syscfg->rst)) {
		ret = PTR_ERR(syscfg->rst);
		goto disable_clk;
	}
	reset_control_deassert(syscfg->rst);

	g_syscfg = syscfg;
	syscfg_usb_init(syscfg);
	syscfg_gmac_init(syscfg, 0);
	syscfg_gmac_init(syscfg, 1);

	/* Enable lvds link1 */
	syscfg_ldo_init(0x3);
#ifdef CONFIG_DEBUG_ON_FPGA_BOARD_ARTINCHIP
	syscfg_fpga_de_clk_sel(FPGA_DISP_CLK_LVDS_SINGLE);
	//If use GMAC, set to 125M
	syscfg_fpga_gmac_clk_sel(FPGA_GMAC_CLK_25M);
#endif

	dev_info(&pdev->dev, "Artinchip Syscfg Loaded\n");
	return 0;

	reset_control_assert(syscfg->rst);
disable_clk:
	clk_disable_unprepare(syscfg->clk);
	return ret;
}

static int syscfg_remove(struct platform_device *pdev)
{
	struct syscfg_dev *syscfg = platform_get_drvdata(pdev);

	reset_control_assert(syscfg->rst);
	clk_disable_unprepare(syscfg->clk);
	return 0;
}

static const struct of_device_id aic_syscfg_dt_ids[] = {
	{.compatible = "artinchip,aic-syscfg"},
	{}
};
MODULE_DEVICE_TABLE(of, aic_syscfg_dt_ids);

static struct platform_driver syscfg_driver = {
	.driver		= {
		.name		= "syscfg",
		.of_match_table	= of_match_ptr(aic_syscfg_dt_ids),
	},
	.probe		= syscfg_probe,
	.remove		= syscfg_remove,
};

/* Must init Syscfg before USB/GMAC, so use postcore_initcall() */
static int __init syscfg_init(void)
{
	return platform_driver_register(&syscfg_driver);
}
postcore_initcall(syscfg_init);

MODULE_AUTHOR("Matteo <duanmt@artinchip.com>");
MODULE_DESCRIPTION("Syscfg driver of Artinchip SoC");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:syscfg");
