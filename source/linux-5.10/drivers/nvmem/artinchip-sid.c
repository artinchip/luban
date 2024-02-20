// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2022, Artinchip Technology Co., Ltd
 */

#include <linux/device.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/nvmem-provider.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/reset.h>

#define SID_REG_CTL	(0x0000)
#define SID_REG_ADDR	(0x0004)
#define SID_REG_WDATA	(0x0008)
#define SID_REG_RDATA	(0x000C)
#define SID_REG_TIMING	(0x0010)
#define SID_REG_SRAM	(0x200)

#define SID_STATUS_OFS	(8)
#define SID_STATUS_MSK	(0xF << SID_STATUS_OFS)
#define SID_STATUS_IDLE	(0x2)

struct aic_sid {
	void __iomem *base;
};

static int aic_sid_read(void *context, unsigned int offset, void *data,
			size_t bytes)
{
	struct aic_sid *sid = context;
	u32 val, cnt, pos;
	u8 *pb, *ps, *pe;

	pr_debug("sid read: offset %d bytes %lu\n", offset, bytes);

	ps = (u8 *)data;
	pe = ps + bytes;

	val = readl(sid->base + SID_REG_CTL);
	if (((val & SID_STATUS_MSK) >> SID_STATUS_OFS) == SID_STATUS_IDLE) {
		/* Read from shadown register */
		pos = (u32)offset;
		cnt = DIV_ROUND_UP(bytes, 4);
		while (cnt > 0) {
			val = readl(sid->base + SID_REG_SRAM + (pos & (~0x3)));
			pb = (u8 *)&val;
			/* Seek to start offset in u32 */
			if (pos % 4)
				pb += (pos % 4);
			/* Copy value by byte */
			do {
				*ps = *pb;
				ps++;
				pb++;
				pos++;
			} while (pos % 4);
			cnt--;
			if (ps >= pe)
				break;
		}
	} else {
		pr_err("Error, SID is not ready.\n");
		return -EFAULT;
	}

	return 0;
}

static int aic_sid_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct resource *res;
	struct nvmem_config *nvmem_cfg;
	struct nvmem_device *nvmem;
	struct aic_sid *sid;
	struct clk *clk;
	struct reset_control *rstc;
	int ret;
	u32 timing = 0;

	sid = devm_kzalloc(dev, sizeof(*sid), GFP_KERNEL);
	if (!sid)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	sid->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(sid->base))
		return PTR_ERR(sid->base);

	clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "clocks not configured!\n");
		return PTR_ERR(clk);
	}

	rstc = devm_reset_control_get(&pdev->dev, NULL);
	if (IS_ERR(rstc)) {
		dev_err(&pdev->dev, "resets not configured!\n");
		return PTR_ERR(rstc);
	}

	ret = clk_prepare_enable(clk);
	if (ret) {
		dev_err(&pdev->dev, "enable clk failed!\n");
		return -EFAULT;
	}

	ret = reset_control_deassert(rstc);
	if (ret) {
		dev_err(&pdev->dev, "reset dassert failed!\n");
		return -EFAULT;
	}

	nvmem_cfg = devm_kzalloc(dev, sizeof(*nvmem_cfg), GFP_KERNEL);
	if (!nvmem_cfg)
		return -ENOMEM;

	nvmem_cfg->dev = dev;
	nvmem_cfg->name = "aic-efuse";
	nvmem_cfg->read_only = true;
	nvmem_cfg->size = resource_size(res);
	nvmem_cfg->word_size = 1;
	nvmem_cfg->stride = 4;
	nvmem_cfg->priv = sid;
	nvmem_cfg->reg_read = aic_sid_read;
	nvmem = devm_nvmem_register(dev, nvmem_cfg);
	if (IS_ERR(nvmem))
		return PTR_ERR(nvmem);

	platform_set_drvdata(pdev, nvmem);

	if (of_property_read_u32(np, "aic,timing", &timing)) {
		dev_info(dev, "Can't parse timing value\n");
	} else {
		writel(timing, sid->base + SID_REG_TIMING);
	}

	return PTR_ERR_OR_ZERO(nvmem);
}

static const struct of_device_id aic_sid_of_match[] = {
	{ .compatible = "artinchip,aic-sid-v1.0" },
	{/* sentinel */},
};
MODULE_DEVICE_TABLE(of, aic_sid_of_match);

static struct platform_driver aic_sid_driver = {
	.probe = aic_sid_probe,
	.driver = {
		.name = "artinchip-sid",
		.of_match_table = aic_sid_of_match,
	},
};

static int __init sid_init(void)
{
	return platform_driver_register(&aic_sid_driver);
}
subsys_initcall(sid_init);

static void __exit sid_exit(void)
{
	return platform_driver_unregister(&aic_sid_driver);
}
module_exit(sid_exit);

MODULE_AUTHOR("Dehuang Wu <dehuang.wu@artinchip.com>");
MODULE_DESCRIPTION("Artinchip SID Driver");
MODULE_LICENSE("GPL");
