// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, Artinchip Technology Co., Ltd
 */
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/devfreq.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/pm_opp.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

struct aic_bus {
	struct devfreq_dev_profile profile;
	struct devfreq *devfreq;
	struct clk *clk;
	struct clk *cpu_clk;
	unsigned long curr_freq;
};

static int aic_bus_target(struct device *dev, unsigned long *freq, u32 flags)
{
	struct aic_bus *priv = dev_get_drvdata(dev);
	struct dev_pm_opp *new_opp;
	int ret = 0;

	new_opp = devfreq_recommended_opp(dev, freq, flags);
	if (IS_ERR(new_opp)) {
		ret = PTR_ERR(new_opp);
		dev_err(dev, "failed to get recommended opp: %d\n", ret);
		return ret;
	}
	dev_pm_opp_put(new_opp);

	priv->curr_freq = *freq;
	return dev_pm_opp_set_rate(dev, *freq);
}

static int aic_bus_get_cur_freq(struct device *dev, unsigned long *freq)
{
	struct aic_bus *priv = dev_get_drvdata(dev);

	*freq = clk_get_rate(priv->clk);
	return 0;
}

static int aic_bus_get_dev_status(struct device *dev,
					struct devfreq_dev_status *stat)
{
	unsigned long cpu_freq;
	struct aic_bus *priv = dev_get_drvdata(dev);

	cpu_freq = clk_get_rate(priv->cpu_clk);
	/*
	 * If the cpu frequency is not less than 300MHz,
	 * then set the bus frequency to the maximum frequency 240MHz.
	 */
	if (cpu_freq >= 300000000) {
		stat->total_time = 0;
	} else {
		if (priv->curr_freq == 100000000) {
			stat->busy_time = 22;
			stat->total_time = 25;
		} else {
			stat->busy_time = 11;
			stat->total_time = 30;
		}
	}

	return 0;
}

static void aic_bus_exit(struct device *dev)
{
	dev_pm_opp_of_remove_table(dev);
}

static int aic_bus_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct aic_bus *priv;
	const char *gov = DEVFREQ_GOV_SIMPLE_ONDEMAND;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(priv->clk)) {
		ret = PTR_ERR(priv->clk);
		dev_err(dev, "failed to fetch clk: %d\n", ret);
		return ret;
	}

	priv->cpu_clk = __clk_lookup("cpu");
	if (IS_ERR(priv->cpu_clk)) {
		ret = PTR_ERR(priv->cpu_clk);
		dev_err(dev, "cann't get cpu_src1 clock\n");
		return ret;
	}

	platform_set_drvdata(pdev, priv);

	ret = dev_pm_opp_of_add_table(dev);
	if (ret < 0) {
		dev_err(dev, "failed to get OPP table\n");
		return ret;
	}

	priv->profile.polling_ms = 200;
	priv->profile.target = aic_bus_target;
	priv->profile.get_dev_status = aic_bus_get_dev_status;
	priv->profile.exit = aic_bus_exit;
	priv->profile.get_cur_freq = aic_bus_get_cur_freq;
	priv->profile.initial_freq = clk_get_rate(priv->clk);
	priv->curr_freq = priv->profile.initial_freq;
	priv->devfreq = devm_devfreq_add_device(dev, &priv->profile,
						gov, NULL);
	if (IS_ERR(priv->devfreq)) {
		ret = PTR_ERR(priv->devfreq);
		dev_err(dev, "failed to add devfreq device: %d\n", ret);
		goto err;
	}

	return 0;

err:
	dev_pm_opp_of_remove_table(dev);
	return ret;
}

static const struct of_device_id aic_bus_of_match[] = {
	{
		.compatible = "artinchip,bus",
	},
	{}
};
MODULE_DEVICE_TABLE(of, aic_bus_of_match);

static struct platform_driver aic_bus_platdrv = {
	.probe		= aic_bus_probe,
	.driver = {
		.name	= "aic-bus-devfreq",
		.of_match_table = of_match_ptr(aic_bus_of_match),
	},
};
module_platform_driver(aic_bus_platdrv);

MODULE_DESCRIPTION("ArtInChip bus frequency scaling driver");
MODULE_AUTHOR("dwj <weijie.ding@artinchip.com>");
MODULE_LICENSE("GPL");

