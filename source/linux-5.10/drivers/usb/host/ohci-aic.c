// SPDX-License-Identifier: GPL-2.0
/*
 * Artinchip OHCI driver
 *
 * Copyright (C) 2020 ARTINCHIP - All Rights Reserved
 *
 *
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/hrtimer.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/usb/ohci_pdriver.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/usb/otg.h>

#include "ohci.h"

#define USB_MAX_CLKS 3

struct aic_ohci_platform_priv {
	struct clk *clks[USB_MAX_CLKS];
	struct clk *clk48;
	struct reset_control *rst;
	struct reset_control *pwr;
	struct phy *phy;
};

#define DRIVER_DESC "OHCI Artinchip driver"

#define hcd_to_ohci_priv(h) \
	((struct aic_ohci_platform_priv *)hcd_to_ohci(h)->priv)

static const char hcd_name[] = "ohci-aic";

static int aic_ohci_platform_power_on(struct platform_device *dev)
{
	struct usb_hcd *hcd = platform_get_drvdata(dev);
	struct aic_ohci_platform_priv *priv = hcd_to_ohci_priv(hcd);
	int clk, ret;

	ret = reset_control_deassert(priv->pwr);
	if (ret)
		return ret;

	ret = reset_control_deassert(priv->rst);
	if (ret)
		goto err_assert_power;

	/* some SoCs don't have a dedicated 48Mhz clock, but those that do
	 * need the rate to be explicitly set
	 */
	if (priv->clk48) {
		ret = clk_set_rate(priv->clk48, 48000000);
		if (ret)
			goto err_assert_reset;
	}

	for (clk = 0; clk < USB_MAX_CLKS && priv->clks[clk]; clk++) {
		ret = clk_prepare_enable(priv->clks[clk]);
		if (ret)
			goto err_disable_clks;
	}

	ret = phy_init(priv->phy);
	if (ret)
		goto err_disable_clks;

	ret = phy_power_on(priv->phy);
	if (ret)
		goto err_exit_phy;

	return 0;

err_exit_phy:
	phy_exit(priv->phy);
err_disable_clks:
	while (--clk >= 0)
		clk_disable_unprepare(priv->clks[clk]);
err_assert_reset:
	reset_control_assert(priv->rst);
err_assert_power:
	reset_control_assert(priv->pwr);

	return ret;
}

static void aic_ohci_platform_power_off(struct platform_device *dev)
{
	struct usb_hcd *hcd = platform_get_drvdata(dev);
	struct aic_ohci_platform_priv *priv = hcd_to_ohci_priv(hcd);

	int clk;

	reset_control_assert(priv->pwr);

	reset_control_assert(priv->rst);

	phy_power_off(priv->phy);

	phy_exit(priv->phy);

	for (clk = USB_MAX_CLKS - 1; clk >= 0; clk--)
		if (priv->clks[clk])
			clk_disable_unprepare(priv->clks[clk]);
}

static struct hc_driver __read_mostly ohci_platform_hc_driver;

static const struct ohci_driver_overrides platform_overrides __initconst = {
	.product_desc =		"Aic OHCI controller",
	.extra_priv_size =	sizeof(struct aic_ohci_platform_priv),
};

static struct usb_ohci_pdata ohci_platform_defaults = {
	.power_on =		aic_ohci_platform_power_on,
	.power_suspend =	aic_ohci_platform_power_off,
	.power_off =		aic_ohci_platform_power_off,
};

static int aic_ohci_platform_probe(struct platform_device *dev)
{
	struct usb_hcd *hcd;
	struct resource *res_mem;
	struct usb_ohci_pdata *pdata = &ohci_platform_defaults;
	struct aic_ohci_platform_priv *priv;
	struct ohci_hcd *ohci;
	int err, irq;
#ifndef CONFIG_USB_OHCI_HCD_AIC
	int clk = 0;
#endif

	if (usb_disabled())
		return -ENODEV;

	irq = platform_get_irq(dev, 0);
	if (irq < 0)
		return irq;

	res_mem = platform_get_resource(dev, IORESOURCE_MEM, 0);
	if (!res_mem) {
		dev_err(&dev->dev, "no memory resource provided");
		return -ENXIO;
	}

	hcd = usb_create_hcd(&ohci_platform_hc_driver, &dev->dev,
			     dev_name(&dev->dev));
	if (!hcd)
		return -ENOMEM;

	platform_set_drvdata(dev, hcd);
	dev->dev.platform_data = pdata;
	priv = hcd_to_ohci_priv(hcd);
	ohci = hcd_to_ohci(hcd);

	if (dev->dev.of_node) {
		if (of_property_read_bool(dev->dev.of_node, "big-endian-regs"))
			ohci->flags |= OHCI_QUIRK_BE_MMIO;

		if (of_property_read_bool(dev->dev.of_node, "big-endian-desc"))
			ohci->flags |= OHCI_QUIRK_BE_DESC;

		if (of_property_read_bool(dev->dev.of_node, "big-endian"))
			ohci->flags |= OHCI_QUIRK_BE_MMIO | OHCI_QUIRK_BE_DESC;

		if (of_property_read_bool(dev->dev.of_node, "no-big-frame-no"))
			ohci->flags |= OHCI_QUIRK_FRAME_NO;

		if (of_property_read_bool(dev->dev.of_node,
					  "remote-wakeup-connected"))
			ohci->hc_control = OHCI_CTRL_RWC;

		of_property_read_u32(dev->dev.of_node, "num-ports",
				     &ohci->num_ports);
	}

#ifndef CONFIG_USB_OHCI_HCD_AIC
	priv->phy = devm_phy_get(&dev->dev, "usb");
	if (IS_ERR(priv->phy)) {
		err = PTR_ERR(priv->phy);
		goto err_put_hcd;
	}

	for (clk = 0; clk < USB_MAX_CLKS; clk++) {
		priv->clks[clk] = of_clk_get(dev->dev.of_node, clk);
		if (IS_ERR(priv->clks[clk])) {
			err = PTR_ERR(priv->clks[clk]);
			if (err == -EPROBE_DEFER)
				goto err_put_clks;
			priv->clks[clk] = NULL;
			break;
		}
	}

	/* some SoCs don't have a dedicated 48Mhz clock, but those that
	 * do need the rate to be explicitly set
	 */
	priv->clk48 = devm_clk_get(&dev->dev, "clk48");
	if (IS_ERR(priv->clk48)) {
		dev_info(&dev->dev, "48MHz clk not found\n");
		priv->clk48 = NULL;
	}

	priv->pwr =
		devm_reset_control_get_optional_shared(&dev->dev, "power");
	if (IS_ERR(priv->pwr)) {
		err = PTR_ERR(priv->pwr);
		goto err_put_clks;
	}

	priv->rst =
		devm_reset_control_get_optional_shared(&dev->dev, "softreset");
	if (IS_ERR(priv->rst)) {
		err = PTR_ERR(priv->rst);
		goto err_put_clks;
	}
#endif

	if (pdata->power_on) {
		err = pdata->power_on(dev);
		if (err < 0)
			goto err_power;
	}

	hcd->rsrc_start = res_mem->start;
	hcd->rsrc_len = resource_size(res_mem);

	hcd->regs = devm_ioremap_resource(&dev->dev, res_mem);
	if (IS_ERR(hcd->regs)) {
		err = PTR_ERR(hcd->regs);
		goto err_power;
	}

#ifdef	CONFIG_USB_OTG
	if (of_property_read_bool(dev->dev.of_node, "aic,otg-support")) {
		struct ohci_hcd *ohci = hcd_to_ohci(hcd);

		hcd->usb_phy = usb_get_phy(USB_PHY_TYPE_USB2);
		if (!IS_ERR_OR_NULL(hcd->usb_phy)) {
			err = otg_set_host1(hcd->usb_phy->otg,
					    &ohci_to_hcd(ohci)->self);
			dev_dbg(hcd->self.controller, "init %s phy, status %d\n",
				hcd->usb_phy->label, err);
			if (err) {
				usb_put_phy(hcd->usb_phy);
				goto err_power;
			}
		} else {
			dev_err(&dev->dev, "can't find phy\n");
			err = -ENODEV;
			goto err_power;
		}
		hcd->skip_phy_initialization = 1;
	}
#endif

	err = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (err)
		goto err_power;

	device_wakeup_enable(hcd->self.controller);

	platform_set_drvdata(dev, hcd);

	return err;

err_power:
	if (pdata->power_off)
		pdata->power_off(dev);
#ifndef CONFIG_USB_OHCI_HCD_AIC
err_put_clks:
	while (--clk >= 0)
		clk_put(priv->clks[clk]);
err_put_hcd:
#endif
	if (pdata == &ohci_platform_defaults)
		dev->dev.platform_data = NULL;

	usb_put_hcd(hcd);

	return err;
}

static int aic_ohci_platform_remove(struct platform_device *dev)
{
	struct usb_hcd *hcd = platform_get_drvdata(dev);
	struct usb_ohci_pdata *pdata = dev_get_platdata(&dev->dev);
	struct aic_ohci_platform_priv *priv = hcd_to_ohci_priv(hcd);
	int clk;

#ifdef CONFIG_USB_OTG
	if (!IS_ERR_OR_NULL(hcd->usb_phy)) {
		otg_set_host1(hcd->usb_phy->otg, 0);
		usb_put_phy(hcd->usb_phy);
	}
#endif

	usb_remove_hcd(hcd);

	if (pdata->power_off)
		pdata->power_off(dev);

	for (clk = 0; clk < USB_MAX_CLKS && priv->clks[clk]; clk++)
		clk_put(priv->clks[clk]);

	usb_put_hcd(hcd);

	if (pdata == &ohci_platform_defaults)
		dev->dev.platform_data = NULL;

	return 0;
}

#ifdef CONFIG_PM_SLEEP

static int aic_ohci_suspend(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct usb_ohci_pdata *pdata = dev->platform_data;
	struct platform_device *pdev = to_platform_device(dev);
	bool do_wakeup = device_may_wakeup(dev);
	int ret;

	ret = ohci_suspend(hcd, do_wakeup);
	if (ret)
		return ret;

	if (pdata->power_suspend)
		pdata->power_suspend(pdev);

	return ret;
}

static int aic_ohci_resume(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct usb_ohci_pdata *pdata = dev_get_platdata(dev);
	struct platform_device *pdev = to_platform_device(dev);
	int err;

	if (pdata->power_on) {
		err = pdata->power_on(pdev);
		if (err < 0)
			return err;
	}

	ohci_resume(hcd, false);
	return 0;
}

static SIMPLE_DEV_PM_OPS(aic_ohci_pm_ops, aic_ohci_suspend, aic_ohci_resume);

#endif /* CONFIG_PM_SLEEP */

static const struct of_device_id aic_ohci_platform_ids[] = {
	{ .compatible = "artinchip,aic-ohci-v1.0", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, aic_ohci_platform_ids);

static struct platform_driver ohci_platform_driver = {
	.probe		= aic_ohci_platform_probe,
	.remove		= aic_ohci_platform_remove,
	.shutdown	= usb_hcd_platform_shutdown,
	.driver		= {
		.name	= "aic-ohci",
#ifdef CONFIG_PM_SLEEP
		.pm	= &aic_ohci_pm_ops,
#endif
		.of_match_table = aic_ohci_platform_ids,
	}
};

static int __init ohci_platform_init(void)
{
	if (usb_disabled())
		return -ENODEV;

	pr_info("%s: " DRIVER_DESC "\n", hcd_name);

	ohci_init_driver(&ohci_platform_hc_driver, &platform_overrides);
	return platform_driver_register(&ohci_platform_driver);
}
module_init(ohci_platform_init);

static void __exit ohci_platform_cleanup(void)
{
	platform_driver_unregister(&ohci_platform_driver);
}
module_exit(ohci_platform_cleanup);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
