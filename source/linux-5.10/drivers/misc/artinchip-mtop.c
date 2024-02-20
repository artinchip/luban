// SPDX-License-Identifier: GPL-2.0-only
/*
 * MTOP(Bandwidth Monitor) driver of Artinchip SoC
 *
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
 * Authors:  Weijie Ding <weijie.ding@artinchip.com>
 */
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/pm_runtime.h>

#define MTOP_CTL			0x0000
#define MTOP_TIME_CNT			0x0004
#define MTOP_IRQ_CTL			0x0008
#define MTOP_IRQ_STA			0x000C
#define MTOP_AXI_WCNT(p)		(0x0100 + p * 0x20)
#define MTOP_AXI_RCNT(p)		(0x0104 + p * 0x20)
#define MTOP_AHB_WCNT			0x0200
#define MTOP_AHB_RCNT			0x0204

#define MTOP_TRIG			BIT(29)
#define MTOP_MODE			BIT(28)
#define MTOP_EN				BIT(0)
#define MTOP_VERSION			0xFFC

struct mtop_dev {
	struct attribute_group attrs;
	void __iomem	*base;
	struct clk *clk;
	struct reset_control *rst;
	u32 cpu_wr;
	u32 cpu_rd;
	u32 dma_wr;
	u32 dma_rd;
	u32 de_wr;
	u32 de_rd;
	/* ge-ve-dvp */
	u32 gvd_wr;
	u32 gvd_rd;
	u32 ahb_wr;
	u32 ahb_rd;
};

static irqreturn_t aic_mtop_irq(int irqno, void *dev_id)
{
	u32 irq_sta;
	struct mtop_dev *mtop = dev_id;

	irq_sta = readl(mtop->base + MTOP_IRQ_STA);

	if (irq_sta & 0x1) {
		mtop->cpu_wr = readl(mtop->base + MTOP_AXI_WCNT(0));
		mtop->cpu_rd = readl(mtop->base + MTOP_AXI_RCNT(0));
		mtop->dma_wr = readl(mtop->base + MTOP_AXI_WCNT(1));
		mtop->dma_rd = readl(mtop->base + MTOP_AXI_RCNT(1));
		mtop->de_wr = readl(mtop->base + MTOP_AXI_WCNT(2));
		mtop->de_rd = readl(mtop->base + MTOP_AXI_RCNT(2));
		mtop->gvd_wr = readl(mtop->base + MTOP_AXI_WCNT(3));
		mtop->gvd_rd = readl(mtop->base + MTOP_AXI_RCNT(3));
		mtop->ahb_wr = readl(mtop->base + MTOP_AHB_WCNT);
		mtop->ahb_rd = readl(mtop->base + MTOP_AHB_RCNT);
		/* clear IRQ flag */
		writel(1, mtop->base + MTOP_IRQ_STA);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static ssize_t cpu_rd_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct mtop_dev *mtop = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mtop->cpu_rd);
}

static ssize_t cpu_wr_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct mtop_dev *mtop = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mtop->cpu_wr);
}


static ssize_t dma_rd_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct mtop_dev *mtop = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mtop->dma_rd);
}

static ssize_t dma_wr_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct mtop_dev *mtop = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mtop->dma_wr);
}


static ssize_t de_rd_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct mtop_dev *mtop = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mtop->de_rd);
}

static ssize_t de_wr_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct mtop_dev *mtop = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mtop->de_wr);
}


static ssize_t gvd_rd_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct mtop_dev *mtop = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mtop->gvd_rd);
}

static ssize_t gvd_wr_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct mtop_dev *mtop = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mtop->gvd_wr);
}

static ssize_t ahb_rd_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct mtop_dev *mtop = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mtop->ahb_rd);
}

static ssize_t ahb_wr_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct mtop_dev *mtop = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mtop->ahb_wr);
}


static ssize_t enable_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t size)
{
	unsigned int val;
	unsigned int reg_value;
	struct mtop_dev *mtop = dev_get_drvdata(dev);
	int res = kstrtouint(buf, 10, &val);

	if (res < 0)
		return res;

	if (val != 0 && val != 1)
		return -EINVAL;

	if (val == 1 && pm_runtime_status_suspended(dev))
		pm_runtime_get_sync(dev);

	reg_value = readl(mtop->base + MTOP_CTL);
	reg_value &= ~MTOP_EN;
	reg_value |= val;
	writel(reg_value, mtop->base + MTOP_CTL);

	if (val == 0 && !pm_runtime_status_suspended(dev))
		pm_runtime_put(dev);

	return size;
}

static ssize_t set_period_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t size)
{
	struct mtop_dev *mtop = dev_get_drvdata(dev);
	unsigned int sec, freq;
	int res;

	res = kstrtouint(buf, 10, &sec);
	if (res < 0)
		return res;

	pm_runtime_get_sync(dev);
	freq = clk_get_rate(mtop->clk);
	/* Configure the period count, triggering interrupt every sec seconds */
	writel(sec * freq - 1, mtop->base + MTOP_TIME_CNT);
	pm_runtime_put(dev);

	return size;
}

static ssize_t version_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct mtop_dev *mtop = dev_get_drvdata(dev);
	void __iomem *base = mtop->base;
	int version = readl(base + MTOP_VERSION);

	return sprintf(buf, "%d.%d\n", version >> 8, version & 0xff);
}

DEVICE_ATTR_RO(cpu_rd);
DEVICE_ATTR_RO(cpu_wr);
DEVICE_ATTR_RO(dma_rd);
DEVICE_ATTR_RO(dma_wr);
DEVICE_ATTR_RO(de_rd);
DEVICE_ATTR_RO(de_wr);
DEVICE_ATTR_RO(gvd_rd);
DEVICE_ATTR_RO(gvd_wr);
DEVICE_ATTR_RO(ahb_rd);
DEVICE_ATTR_RO(ahb_wr);
DEVICE_ATTR_RO(version);
DEVICE_ATTR_WO(enable);
DEVICE_ATTR_WO(set_period);

static struct attribute *mtop_attrs[] = {
	&dev_attr_version.attr,
	&dev_attr_cpu_rd.attr,
	&dev_attr_cpu_wr.attr,
	&dev_attr_dma_rd.attr,
	&dev_attr_dma_wr.attr,
	&dev_attr_de_rd.attr,
	&dev_attr_de_wr.attr,
	&dev_attr_gvd_rd.attr,
	&dev_attr_gvd_wr.attr,
	&dev_attr_ahb_rd.attr,
	&dev_attr_ahb_wr.attr,
	&dev_attr_enable.attr,
	&dev_attr_set_period.attr,
	NULL,
};

static struct attribute_group mtop_attr_group = {
	.name = "mtop",
	.attrs = mtop_attrs,
};

static int aic_mtop_probe(struct platform_device *pdev)
{
	struct mtop_dev *mtop;
	struct resource *res;
	int irq, ret;

	mtop = devm_kzalloc(&pdev->dev, sizeof(struct mtop_dev), GFP_KERNEL);
	if (!mtop)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mtop->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(mtop->base))
		return PTR_ERR(mtop->base);

	mtop->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(mtop->clk)) {
		dev_err(&pdev->dev, "no clock defined\n");
		return PTR_ERR(mtop->clk);
	}

	/* Enable MTOP module clock */
	ret = clk_prepare_enable(mtop->clk);
	if (ret)
		return ret;

	mtop->rst = devm_reset_control_get_exclusive(&pdev->dev, NULL);
	if (IS_ERR(mtop->rst)) {
		ret = PTR_ERR(mtop->rst);
		goto disable_clk;
	}

	ret = reset_control_deassert(mtop->rst);
	if (ret)
		goto disable_clk;

	platform_set_drvdata(pdev, mtop);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "Failed to get irq\n");
		goto disable_rst;
	}

	ret = devm_request_irq(&pdev->dev, irq, aic_mtop_irq, 0,
			       dev_name(&pdev->dev), mtop);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request irq\n");
		goto disable_rst;
	}

	/* Enable interrupt */
	writel(0x1, mtop->base + MTOP_IRQ_CTL);

	pm_runtime_enable(&pdev->dev);
	pm_runtime_set_active(&pdev->dev);

	ret = sysfs_create_group(&pdev->dev.kobj, &mtop_attr_group);
	if (ret)
		return ret;

	dev_info(&pdev->dev, "MTOP Module Initialized!\n");
	return 0;

disable_rst:
	reset_control_assert(mtop->rst);
disable_clk:
	clk_disable_unprepare(mtop->clk);

	return ret;
}

static int aic_mtop_remove(struct platform_device *pdev)
{
	struct mtop_dev *mtop = platform_get_drvdata(pdev);

	/* Disable interrupt */
	writel(0, mtop->base + MTOP_IRQ_CTL);

	/* Disable MTOP */
	writel(0, mtop->base + MTOP_CTL);

	sysfs_remove_group(&pdev->dev.kobj, &mtop_attr_group);
	pm_runtime_disable(&pdev->dev);

	return 0;
}

#ifdef CONFIG_PM
static int aic_mtop_runtime_suspend(struct device *dev)
{
	struct mtop_dev *mtop = dev_get_drvdata(dev);

	clk_disable_unprepare(mtop->clk);
	return 0;
}

static int aic_mtop_runtime_resume(struct device *dev)
{
	struct mtop_dev *mtop = dev_get_drvdata(dev);

	return clk_prepare_enable(mtop->clk);
}

static const struct dev_pm_ops mtop_pm_ops = {
	SET_RUNTIME_PM_OPS(aic_mtop_runtime_suspend,
			aic_mtop_runtime_resume, NULL)
};

#define MTOP_DEV_PM_OPS	(&mtop_pm_ops)
#else
#define MTOP_DEV_PM_OPS	NULL
#endif

static const struct of_device_id aic_mtop_match[] = {
	{
		.compatible = "artinchip,aic-mtop",
	},
	{}
};

MODULE_DEVICE_TABLE(of, aic_mtop_match);

static struct platform_driver aic_mtop_driver = {
	.probe = aic_mtop_probe,
	.remove = aic_mtop_remove,
	.driver = {
		.name = "aic-mtop",
		.of_match_table = aic_mtop_match,
		.pm = MTOP_DEV_PM_OPS,
	},
};

module_platform_driver(aic_mtop_driver);

MODULE_DESCRIPTION("ArtInChip MTOP driver");
MODULE_AUTHOR("dwj <weijie.ding@artinchip.com>");
MODULE_LICENSE("GPL");
