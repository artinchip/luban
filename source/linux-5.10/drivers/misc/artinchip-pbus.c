// SPDX-License-Identifier: GPL-2.0-only
/*
 * PBUS driver of Artinchip SoC
 *
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
 * Authors:  Matteo <duanmt@artinchip.com>
 */
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/reset.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/types.h>

/* Register of PBUS */
#define PBUS_CFG0	0x000
#define PBUS_CFG1	0x004
#define PBUS_CFG2	0x008
#define PBUS_VERSION	0xFFC

#define PBUS_OUTENABLE_POL		BIT(11)
#define PBUS_WRENABLE_POL		BIT(10)
#define PBUS_ADDRVALID_POL		BIT(9)
#define PBUS_CS_POL			BIT(8)
#define PBUS_BUSCLK_POL			BIT(5)
#define PBUS_BUSCLK_OUTENABLE		BIT(4)
#define PBUS_BUSCLK_DIV_SHIFT		0
#define PBUS_BUSCLK_DIV_MASK		GENMASK(1, 0)

#define PBUS_WRDATA_HOLDTIME_SHIFT	28
#define PBUS_WRDATA_HOLDTIME_MASK	GENMASK(31, 28)
#define PBUS_WRDATA_DELAYTIME_SHIFT	24
#define PBUS_WRDATA_DELAYTIME_MASK	GENMASK(27, 24)
#define PBUS_ADDR_HOLDTIME_SHIFT	20
#define PBUS_ADDR_HOLDTIME_MASK		GENMASK(23, 20)
#define PBUS_ADDR_DELAYTIME_SHIFT	16
#define PBUS_ADDR_DELAYTIME_MASK	GENMASK(19, 16)
#define PBUS_CS_HOLDTIME_SHIFT		8
#define PBUS_CS_HOLDTIME_MASK		GENMASK(12, 8)
#define PBUS_CS_DELAYTIME_SHIFT		0
#define PBUS_CS_DELAYTIME_MASK		GENMASK(4, 0)

#define PBUS_OUTENABLE_HOLDTIME_SHIFT	20
#define PBUS_OUTENABLE_HOLDTIME_MASK	GENMASK(23, 20)
#define PBUS_OUTENABLE_DELAYTIME_SHIFT	16
#define PBUS_OUTENABLE_DELAYTIME_MASK	GENMASK(19, 16)
#define PBUS_WRRD_HOLDTIME_SHIFT	12
#define PBUS_WRRD_HOLDTIME_MASK		GENMASK(15, 12)
#define PBUS_WRRD_DELAYTIME_SHIFT	8
#define PBUS_WRRD_DELAYTIME_MASK	GENMASK(11, 8)
#define PBUS_ADDRVALID_HOLDTIME_SHIFT	4
#define PBUS_ADDRVALID_HOLDTIME_MASK	GENMASK(7, 4)
#define PBUS_ADDRVALID_DELAYTIME_SHIFT	0
#define PBUS_ADDRVALID_DELAYTIME_MASK	GENMASK(3, 0)

#define PBUS_SET_BIT(val, bit, n)	do { \
			if (bit) \
				(val) |= (n); \
			else \
				(val) &= ~(n); \
		} while (0)
#define PBUS_SET_BITS(val, bits, NAME)	do { \
			(val) &= ~NAME##_MASK; \
			(val) |= (bits) << NAME##_SHIFT; \
		} while (0)
#define PBUS_GET_BIT(val, n)	 ((val) & (n) ? 1 : 0)
#define PBUS_GET_BITS(val, NAME) ((u32)(((val) & NAME##_MASK) >> NAME##_SHIFT))

struct pbus_dev {
	void __iomem *base;
	struct platform_device *pdev;
	struct attribute_group attrs;
	struct clk *clk;
	struct reset_control *rst;
};

static ssize_t status_show(struct device *dev, struct device_attribute *devattr,
			   char *buf)
{
	u32 cfg0, cfg1, cfg2, ver;
	struct pbus_dev *pbus = dev_get_drvdata(dev);
	void __iomem *base = pbus->base;

	cfg0 = readl(base + PBUS_CFG0);
	cfg1 = readl(base + PBUS_CFG1);
	cfg2 = readl(base + PBUS_CFG2);
	ver  = readl(base + PBUS_VERSION);

	return sprintf(buf, "In PBUS V%d.%02d:\n"
		"Bus clk: Div %d, Out enable %d, Pol %d\n"
		"POL: CS %d, Addr valid %d, Write enable %d, Out enable %d\n\n"
		"            Hold time    Delay time\n"
		"   WR data: %-12d %-9d\n"
		"      Addr: %-12d %-9d\n"
		"        CS: %-12d %-9d\n"
		"Out enable: %-12d %-9d\n"
		"Write&Read: %-12d %-9d\n"
		"Addr Valid: %-12d %-9d\n",
		ver >> 8, ver & 0xff,
		(u32)(cfg0 & PBUS_BUSCLK_DIV_MASK),
		PBUS_GET_BIT(cfg0, PBUS_BUSCLK_OUTENABLE),
		PBUS_GET_BIT(cfg0, PBUS_BUSCLK_POL),
		PBUS_GET_BIT(cfg0, PBUS_CS_POL),
		PBUS_GET_BIT(cfg0, PBUS_ADDRVALID_POL),
		PBUS_GET_BIT(cfg0, PBUS_WRENABLE_POL),
		PBUS_GET_BIT(cfg0, PBUS_OUTENABLE_POL),
		PBUS_GET_BITS(cfg1, PBUS_WRDATA_HOLDTIME),
		PBUS_GET_BITS(cfg1, PBUS_WRDATA_DELAYTIME),
		PBUS_GET_BITS(cfg1, PBUS_ADDR_HOLDTIME),
		PBUS_GET_BITS(cfg1, PBUS_ADDR_DELAYTIME),
		PBUS_GET_BITS(cfg1, PBUS_CS_HOLDTIME),
		PBUS_GET_BITS(cfg1, PBUS_CS_DELAYTIME),
		PBUS_GET_BITS(cfg2, PBUS_OUTENABLE_HOLDTIME),
		PBUS_GET_BITS(cfg2, PBUS_OUTENABLE_DELAYTIME),
		PBUS_GET_BITS(cfg2, PBUS_WRRD_HOLDTIME),
		PBUS_GET_BITS(cfg2, PBUS_WRRD_DELAYTIME),
		PBUS_GET_BITS(cfg2, PBUS_ADDRVALID_HOLDTIME),
		PBUS_GET_BITS(cfg2, PBUS_ADDRVALID_DELAYTIME));
}
static DEVICE_ATTR_RO(status);

static struct attribute *pbus_attr[] = {
	&dev_attr_status.attr,
	NULL
};

static void pbus_set_cfg0(struct device *dev, void __iomem *base)
{
	bool tmp = false;
	u32 div, val = readl(base + PBUS_CFG0);

	tmp = device_property_read_bool(dev, "aic,outenable-pol-highactive");
	PBUS_SET_BIT(val, tmp, PBUS_OUTENABLE_POL);

	tmp = device_property_read_bool(dev, "aic,wrenable-pol-highactive");
	PBUS_SET_BIT(val, tmp, PBUS_WRENABLE_POL);

	tmp = device_property_read_bool(dev, "aic,addrvalid-pol-highactive");
	PBUS_SET_BIT(val, tmp, PBUS_ADDRVALID_POL);

	tmp = device_property_read_bool(dev, "aic,cs-pol-highactive");
	PBUS_SET_BIT(val, tmp, PBUS_CS_POL);

	tmp = device_property_read_bool(dev, "aic,busclk-pol-riseedge");
	PBUS_SET_BIT(val, tmp, PBUS_BUSCLK_POL);

	tmp = device_property_read_bool(dev, "aic,busclk-outenable");
	PBUS_SET_BIT(val, tmp, PBUS_BUSCLK_OUTENABLE);

	if (!device_property_read_u32(dev, "aic,busclk-div", &div) && div < 4)
		PBUS_SET_BITS(val, div, PBUS_BUSCLK_DIV);

	writel(val, base + PBUS_CFG0);
	dev_dbg(dev, "Set CFG0: %#x\n", val);
}

static void pbus_set_cfg1(struct device *dev, void __iomem *base)
{
	u32 tmp = 0;
	u32 val = readl(base + PBUS_CFG1);

	if (!device_property_read_u32(dev, "aic,wrdata-holdtime", &tmp))
		PBUS_SET_BITS(val, tmp, PBUS_WRDATA_HOLDTIME);

	if (!device_property_read_u32(dev, "aic,wrdata-delaytime", &tmp))
		PBUS_SET_BITS(val, tmp, PBUS_WRDATA_DELAYTIME);

	if (!device_property_read_u32(dev, "aic,addr-holdtime", &tmp))
		PBUS_SET_BITS(val, tmp, PBUS_ADDR_HOLDTIME);

	if (!device_property_read_u32(dev, "aic,addr-delaytime", &tmp))
		PBUS_SET_BITS(val, tmp, PBUS_ADDR_DELAYTIME);

	if (!device_property_read_u32(dev, "aic,cs-holdtime", &tmp))
		PBUS_SET_BITS(val, tmp, PBUS_CS_HOLDTIME);

	if (!device_property_read_u32(dev, "aic,cs-delaytime", &tmp))
		PBUS_SET_BITS(val, tmp, PBUS_CS_DELAYTIME);

	writel(val, base + PBUS_CFG1);
	dev_dbg(dev, "Set CFG1: %#x\n", val);
}

static void pbus_set_cfg2(struct device *dev, void __iomem *base)
{
	u32 tmp = 0;
	u32 val = readl(base + PBUS_CFG2);

	if (!device_property_read_u32(dev, "aic,outenable-holdtime", &tmp))
		PBUS_SET_BITS(val, tmp, PBUS_OUTENABLE_HOLDTIME);

	if (!device_property_read_u32(dev, "aic,outenable-delaytime", &tmp))
		PBUS_SET_BITS(val, tmp, PBUS_OUTENABLE_DELAYTIME);

	if (!device_property_read_u32(dev, "aic,wrrd-holdtime", &tmp))
		PBUS_SET_BITS(val, tmp, PBUS_WRRD_HOLDTIME);

	if (!device_property_read_u32(dev, "aic,wrrd-delaytime", &tmp))
		PBUS_SET_BITS(val, tmp, PBUS_WRRD_DELAYTIME);

	if (!device_property_read_u32(dev, "aic,addrvalid-holdtime", &tmp))
		PBUS_SET_BITS(val, tmp, PBUS_ADDRVALID_HOLDTIME);

	if (!device_property_read_u32(dev, "aic,addrvalid-delaytime", &tmp))
		PBUS_SET_BITS(val, tmp, PBUS_ADDRVALID_DELAYTIME);

	writel(val, base + PBUS_CFG2);
	dev_dbg(dev, "Set CFG2: %#x\n", val);
}

static int pbus_probe(struct platform_device *pdev)
{
	int ret;
	struct pbus_dev *pbus;

	pbus = devm_kzalloc(&pdev->dev, sizeof(struct pbus_dev), GFP_KERNEL);
	if (!pbus)
		return -ENOMEM;
	pbus->pdev = pdev;

	pbus->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(pbus->base))
		return PTR_ERR(pbus->base);

	pbus->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(pbus->clk)) {
		dev_err(&pdev->dev, "no clock defined\n");
		return PTR_ERR(pbus->clk);
	}

	ret = clk_prepare_enable(pbus->clk);
	if (ret)
		return ret;

	pbus->rst = devm_reset_control_get_optional_shared(&pdev->dev, NULL);
	if (IS_ERR(pbus->rst)) {
		ret = PTR_ERR(pbus->rst);
		goto disable_clk;
	}
	reset_control_deassert(pbus->rst);

	pbus->attrs.attrs = pbus_attr;
	ret = sysfs_create_group(&pdev->dev.kobj, &pbus->attrs);
	if (ret)
		goto disable_rst;

	pbus_set_cfg0(&pdev->dev, pbus->base);
	pbus_set_cfg1(&pdev->dev, pbus->base);
	pbus_set_cfg2(&pdev->dev, pbus->base);

	dev_info(&pdev->dev, "Artinchip PBUS Loaded\n");
	platform_set_drvdata(pdev, pbus);
	return 0;

disable_rst:
	reset_control_assert(pbus->rst);
disable_clk:
	clk_disable_unprepare(pbus->clk);
	return ret;
}

static int pbus_remove(struct platform_device *pdev)
{
	struct pbus_dev *pbus = platform_get_drvdata(pdev);

	reset_control_assert(pbus->rst);
	clk_disable_unprepare(pbus->clk);
	return 0;
}

static const struct of_device_id aic_pbus_dt_ids[] = {
	{.compatible = "artinchip,aic-pbus-v1.0"},
	{}
};
MODULE_DEVICE_TABLE(of, aic_pbus_dt_ids);

static struct platform_driver pbus_driver = {
	.driver		= {
		.name	= "aic-pbus",
		.of_match_table	= of_match_ptr(aic_pbus_dt_ids),
	},
	.probe		= pbus_probe,
	.remove		= pbus_remove,
};

/* Must init PBUS before FLASH device, so use postcore_initcall() */
static int __init pbus_init(void)
{
	return platform_driver_register(&pbus_driver);
}
postcore_initcall(pbus_init);

MODULE_AUTHOR("Matteo <duanmt@artinchip.com>");
MODULE_DESCRIPTION("PBUS driver of Artinchip SoC");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:pbus");
