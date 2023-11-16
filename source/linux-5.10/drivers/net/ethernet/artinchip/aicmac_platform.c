// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/of_device.h>
#include <linux/of_mdio.h>
#include <linux/reset.h>

#include "aicmac.h"
#include "aicmac_util.h"
#include "aicmac_platform.h"
#include "aicmac_core.h"
#include "aicmac_macaddr.h"

struct aicmac_platform_data *
aicmac_platform_init_data(struct platform_device *pdev)
{
	struct aicmac_platform_data *plat = devm_kzalloc(&pdev->dev,
		sizeof(struct aicmac_platform_data), GFP_KERNEL);
	struct device_node *np = pdev->dev.of_node;

	plat->phy_data = aicmac_phy_init_data(pdev, np);

	plat->mac_data = aicmac_mac_init_data(pdev, np);

	plat->mdio_data = aicmac_mdio_init_data(pdev, np);

	plat->dma_data = aicmac_dma_init_data(pdev, np);

	plat->napi_data = aicmac_napi_init_data(pdev, np);

	plat->ptp_data = aicmac_1588_init_data(pdev, np);

	return plat;
}

void aicmac_platform_get_mac_addr(struct device *dev, struct device_node *np,
				  struct aicmac_resources *aicmac_res)
{
	const char *mac = of_get_mac_address(np);
	int bus_id = of_alias_get_id(np, AICMAC_OF_ALIAS_NAME);

	if (!IS_ERR_OR_NULL(mac) && is_valid_ether_addr(mac))
		memcpy(aicmac_res->mac, mac, MAX_ADDR_LEN);
	else {
		aicmac_get_mac_addr(dev, bus_id, aicmac_res->mac);
	}

	aicmac_print_mac_addr(aicmac_res->mac);
}

void aicmac_platform_remove_config_dt(struct platform_device *pdev,
				      struct aicmac_platform_data *plat)
{
	of_node_put(plat->phy_data->phy_node);
	of_node_put(plat->mdio_data->mdio_node);
}

void aicmac_platform_init_ioirq(struct aicmac_resources *aicmac_res,
				struct aicmac_platform_data *plat)
{
	plat->mac_data->pcsr = aicmac_res->ioaddr;
}

struct aicmac_platform_data *
aicmac_platform_get_config(struct platform_device *pdev,
			   struct aicmac_resources *aicmac_res)
{
	struct device_node *np = pdev->dev.of_node;
	struct aicmac_platform_data *plat;

	plat = aicmac_platform_init_data(pdev);

	aicmac_platform_get_mac_addr(&pdev->dev, np, aicmac_res);

	//copy or calculator ioaddr to different module
	aicmac_platform_init_ioirq(aicmac_res, plat);

	plat->bus_id = of_alias_get_id(np, AICMAC_OF_ALIAS_NAME);

	if (plat->bus_id < 0)
		plat->bus_id = 0;

	plat->rx_queues_to_use = 1;
	plat->tx_queues_to_use = 1;

	plat->rx_queues_cfg[0].mode_to_use = MTL_QUEUE_DCB;
	plat->tx_queues_cfg[0].mode_to_use = MTL_QUEUE_DCB;

	plat->aicmac_clk = devm_clk_get(&pdev->dev, AICMAC_RESOURCE_NAME);
	if (IS_ERR(plat->aicmac_clk)) {
		dev_warn(&pdev->dev, "Cannot get CSR clock\n");
		plat->aicmac_clk = NULL;
		return ERR_PTR(-EPROBE_DEFER);
	}

	plat->clk_csr = AICMAC_CSR_DEFAULT;
	clk_set_rate(plat->aicmac_clk, CSR_F_50M);
	clk_prepare_enable(plat->aicmac_clk);

	plat->pclk = devm_clk_get(&pdev->dev, AICMAC_PCLK_NAME);
	if (IS_ERR(plat->pclk)) {
		if (PTR_ERR(plat->pclk) == -EPROBE_DEFER)
			goto error_pclk_get;

		plat->pclk = NULL;
	}
	clk_prepare_enable(plat->pclk);

	aicmac_1588_init_clk(pdev, plat->ptp_data, plat->aicmac_clk);

	plat->aicmac_rst =
		devm_reset_control_get(&pdev->dev, AICMAC_RESOURCE_NAME);
	if (IS_ERR(plat->aicmac_rst)) {
		if (PTR_ERR(plat->aicmac_rst) == -EPROBE_DEFER)
			goto error_hw_init;
		plat->aicmac_rst = NULL;
	}

	plat->phy_gpio = devm_gpiod_get_optional(&pdev->dev, "phy-reset", GPIOD_OUT_HIGH);
	if (IS_ERR(plat->phy_gpio))
		goto error_hw_init;
	aicmac_phy_reset(plat->phy_gpio);

	pr_info("%s bus_id:%d \n", __func__, plat->bus_id);

	return plat;

error_hw_init:
	clk_disable_unprepare(plat->pclk);
error_pclk_get:
	clk_disable_unprepare(plat->aicmac_clk);
	aicmac_platform_remove_config_dt(pdev, plat);
	return ERR_PTR(-EPROBE_DEFER);
}

struct aicmac_resources *
aicmac_platform_get_resources(struct platform_device *pdev)
{
	struct aicmac_resources *aicmac_res;

	aicmac_res = devm_kzalloc(&pdev->dev, sizeof(struct aicmac_resources),
				  GFP_KERNEL);
	memset(aicmac_res, 0, sizeof(*aicmac_res));

	aicmac_res->irq = platform_get_irq_byname(pdev, AICMAC_IRQ_NAME);
	if (aicmac_res->irq < 0)
		return ERR_PTR(-EPROBE_DEFER);

	aicmac_res->ioaddr = devm_platform_ioremap_resource(pdev, 0);

	aicmac_res->wol_irq = -1;
	aicmac_res->lpi_irq = -1;

	return aicmac_res;
}

static int aicmac_probe(struct platform_device *pdev)
{
	struct aicmac_platform_data *plat_dat;
	struct aicmac_resources *aicmac_res;
	int ret = -1;

	aicmac_res = aicmac_platform_get_resources(pdev);
	if (IS_ERR(aicmac_res)) {
		pr_err("%s prepare platform resource failed.\n", __func__);
		return ret;
	}

	if (!pdev->dev.of_node) {
		pr_err("%s prepare of_node failed.\n", __func__);
		return ret;
	}

	plat_dat = aicmac_platform_get_config(pdev, aicmac_res);
	if (IS_ERR(plat_dat)) {
		pr_err("%s prepare platform configuration failed.\n", __func__);
		return PTR_ERR(plat_dat);
	}

	ret = aicmac_core_init(&pdev->dev, plat_dat, aicmac_res);
	if (ret) {
		if (pdev->dev.of_node)
			aicmac_platform_remove_config_dt(pdev, plat_dat);
		pr_err("%s core init failed.\n", __func__);
		return ret;
	}

	pr_info("%s success.", __func__);
	return 0;
}

int aicmac_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct aicmac_priv *priv = netdev_priv(ndev);
	struct aicmac_platform_data *plat = priv->plat;
	int ret = 0;

	ret = aicmac_core_destroy(priv);

	aicmac_platform_remove_config_dt(pdev, plat);

	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int aicmac_suspend(struct device *dev)
{
	int ret = 0;

	return ret;
}

static int aicmac_resume(struct device *dev)
{
	int ret = 0;

	return ret;
}
#endif

SIMPLE_DEV_PM_OPS(aicmac_pm_ops, aicmac_suspend, aicmac_resume);

static const struct of_device_id aicmac_match[] = {
	{ .compatible = "artinchip,aic-mac-v1.0" },
	{}
};
MODULE_DEVICE_TABLE(of, aicmac_match);

const struct dev_pm_ops aicmac_pm_ops;
static struct platform_driver aicmac_driver = {
	.probe  = aicmac_probe,
	.remove = aicmac_remove,
	.driver = {
		.name           = AICMAC_RESOURCE_NAME,
		.pm             = &aicmac_pm_ops,
		.of_match_table = of_match_ptr(aicmac_match),
	},
};

module_platform_driver(aicmac_driver);

MODULE_AUTHOR("Keliang Liu");
MODULE_DESCRIPTION("ArtInChip GMAC Driver");
MODULE_ALIAS("platform:" AICMAC_RESOURCE_NAME);
MODULE_LICENSE("GPL");
