// SPDX-License-Identifier: GPL-2.0
/*
 * Artinchip EHCI driver
 *
 * Copyright (C) 2020 ARTINCHIP – All Rights Reserved
 *
 * Author: Matteo <duanmt@artinchip.com>
 *
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/usb/ehci_pdriver.h>
#include <linux/pinctrl/consumer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include "ehci.h"

#define USB_MAX_CLKS_RSTS 2

struct aic_ehci_platform_priv {
	struct clk *clks[USB_MAX_CLKS_RSTS];
	struct clk *clk48;
	struct reset_control *rst[USB_MAX_CLKS_RSTS];
	struct reset_control *pwr;
	struct phy *phy;
};

#define DRIVER_DESC "Artinchip EHCI driver"

#define hcd_to_ehci_priv(h) \
	((struct aic_ehci_platform_priv *)hcd_to_ehci(h)->priv)

static const char hcd_name[] = "ehci-aic";

#define EHCI_CAPS_SIZE 0x10
#define AHB2STBUS_INSREG01 (EHCI_CAPS_SIZE + 0x84)

extern void syscfg_usb_phy0_sw_host(int sw);

#ifndef CONFIG_DEBUG_ON_FPGA_BOARD_ARTINCHIP
#define USB_HOST_CTRL_REG_OFFSE 0x800
#define ULPI_BYPASS_EN 0x1
#define PHY_TYPE_ULPI 0
#define PHY_TYPE_UTMI 1

static void aic_ehci_set_phy_type(struct usb_hcd *hcd, int phy_type)
{
	u32 val = 0;

	val = readl(hcd->regs + USB_HOST_CTRL_REG_OFFSE);

	if (phy_type == PHY_TYPE_ULPI) {
		writel(val & ~ULPI_BYPASS_EN,
		       hcd->regs + USB_HOST_CTRL_REG_OFFSE);
	} else {
		writel(val | ULPI_BYPASS_EN,
		       hcd->regs + USB_HOST_CTRL_REG_OFFSE);
	}
}
#endif

static int aic_ehci_platform_reset(struct usb_hcd *hcd)
{
	struct platform_device *pdev = to_platform_device(hcd->self.controller);
	struct usb_ehci_pdata *pdata = dev_get_platdata(&pdev->dev);
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	u32 threshold;

	/* Set EHCI packet buffer IN/OUT threshold (in DWORDs) */
#ifdef CONFIG_USB_EHCI_HCD_AIC
	/* Must increase the OUT threshold to avoid underrun. (FIFO size - 4) */
	threshold = 32 | (127 << 16);
#else
	/* According the databook. (FIFO size / 4) */
	threshold = 32 | (32 << 16);
#endif
	writel(threshold, hcd->regs + AHB2STBUS_INSREG01);

	ehci->caps = hcd->regs + pdata->caps_offset;
	return ehci_setup(hcd);
}

static int aic_ehci_platform_power_on(struct platform_device *dev)
{
	struct usb_hcd *hcd = platform_get_drvdata(dev);
	struct aic_ehci_platform_priv *priv = hcd_to_ehci_priv(hcd);
	unsigned int gpio;
	int i, ret;

	if (of_property_read_bool(dev->dev.of_node,
				  "artinchip,sw_usb_phy0")) {
		syscfg_usb_phy0_sw_host(1);
		dev_info(&dev->dev, "switch usb_phy0 to host.");

		gpio = of_get_named_gpio(dev->dev.of_node, "vbus-en-gpios", 0);
		if (gpio_is_valid(gpio)) {
			ret = gpio_direction_output(gpio, 1);
			if (ret)
				goto err_assert_power;
		}

		dev_info(&dev->dev, "enable host vbus.");
	}

	//ret = reset_control_deassert(priv->pwr);
	//if (ret)
	//	return ret;

	for (i = 0; i < USB_MAX_CLKS_RSTS && priv->rst[i]; i++) {
		ret = reset_control_deassert(priv->rst[i]);
		if (ret)
			goto err_assert_power;
	}

	/* some SoCs don't have a dedicated 48Mhz clock, but those that do
	 * need the rate to be explicitly set
	 */
	if (priv->clk48) {
		ret = clk_set_rate(priv->clk48, 48000000);
		if (ret)
			goto err_assert_reset;
	}

	for (i = 0; i < USB_MAX_CLKS_RSTS && priv->clks[i]; i++) {
		ret = clk_prepare_enable(priv->clks[i]);
		if (ret)
			goto err_disable_clks;
	}

#ifndef CONFIG_USB_EHCI_HCD_AIC
	ret = phy_init(priv->phy);
	if (ret)
		goto err_disable_clks;

	ret = phy_power_on(priv->phy);
	if (ret)
		goto err_exit_phy;
#endif

	return 0;

#ifndef CONFIG_USB_EHCI_HCD_AIC
err_exit_phy:
	phy_exit(priv->phy);
#endif
err_disable_clks:
	while (--i >= 0)
		clk_disable_unprepare(priv->clks[i]);
err_assert_reset:
	for (i = 0; i < USB_MAX_CLKS_RSTS && priv->rst[i]; i++)
		reset_control_assert(priv->rst[i]);
err_assert_power:
//	reset_control_assert(priv->pwr);

	return ret;
}

static void aic_ehci_platform_power_off(struct platform_device *dev)
{
	struct usb_hcd *hcd = platform_get_drvdata(dev);
	struct aic_ehci_platform_priv *priv = hcd_to_ehci_priv(hcd);
	int i;

	// reset_control_assert(priv->pwr);

	for (i = 0; i < USB_MAX_CLKS_RSTS && priv->rst[i]; i++)
		reset_control_assert(priv->rst[i]);

#ifndef CONFIG_USB_EHCI_HCD_AIC
	phy_power_off(priv->phy);

	phy_exit(priv->phy);
#endif

	for (i = USB_MAX_CLKS_RSTS - 1; i >= 0 && priv->clks[i]; i--)
		clk_disable_unprepare(priv->clks[i]);
}

static struct hc_driver __read_mostly ehci_platform_hc_driver;

static const struct ehci_driver_overrides platform_overrides __initconst = {
	.reset =		aic_ehci_platform_reset,
	.extra_priv_size =	sizeof(struct aic_ehci_platform_priv),
};

static struct usb_ehci_pdata ehci_platform_defaults = {
	.power_on      = aic_ehci_platform_power_on,
	.power_suspend = aic_ehci_platform_power_off,
	.power_off     = aic_ehci_platform_power_off,
};

static int aic_ehci_platform_probe(struct platform_device *dev)
{
	struct usb_hcd *hcd;
	struct resource *res_mem;
	struct usb_ehci_pdata *pdata = &ehci_platform_defaults;
	struct aic_ehci_platform_priv *priv;
	int err, irq, i = 0;

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

	hcd = usb_create_hcd(&ehci_platform_hc_driver, &dev->dev,
			     dev_name(&dev->dev));
	if (!hcd)
		return -ENOMEM;

	platform_set_drvdata(dev, hcd);
	dev->dev.platform_data = pdata;
	priv = hcd_to_ehci_priv(hcd);

#ifndef CONFIG_USB_EHCI_HCD_AIC
	priv->phy = devm_phy_get(&dev->dev, "usb");
	if (IS_ERR(priv->phy)) {
		err = PTR_ERR(priv->phy);
		goto err_put_hcd;
	}
#endif

	for (i = 0; i < USB_MAX_CLKS_RSTS; i++) {
		priv->clks[i] = of_clk_get(dev->dev.of_node, i);
		if (IS_ERR(priv->clks[i])) {
			err = PTR_ERR(priv->clks[i]);
			if (err == -EPROBE_DEFER)
				goto err_put_clks;
			priv->clks[i] = NULL;
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

	for (i = 0; i < USB_MAX_CLKS_RSTS; i++) {
		if (i == 1)
			priv->rst[i] = devm_reset_control_get_shared_by_index(&dev->dev, i);
		else
			priv->rst[i] = devm_reset_control_get_by_index(&dev->dev, i);
		if (IS_ERR(priv->rst[i])) {
			err = PTR_ERR(priv->rst[i]);
			if (err == -EPROBE_DEFER)
				goto err_put_clks;
			priv->rst[i] = NULL;
		}
	}

	if (pdata->power_on) {
		err = pdata->power_on(dev);
		if (err < 0)
			goto err_put_clks;
	}

	hcd->rsrc_start = res_mem->start;
	hcd->rsrc_len = resource_size(res_mem);

	hcd->regs = devm_ioremap_resource(&dev->dev, res_mem);
	if (IS_ERR(hcd->regs)) {
		err = PTR_ERR(hcd->regs);
		goto err_put_clks;
	}

#ifndef CONFIG_DEBUG_ON_FPGA_BOARD_ARTINCHIP
	aic_ehci_set_phy_type(hcd, PHY_TYPE_UTMI);
#endif

	err = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (err)
		goto err_put_clks;

	device_wakeup_enable(hcd->self.controller);
	platform_set_drvdata(dev, hcd);

	return err;

err_put_clks:
	while (--i >= 0)
		clk_put(priv->clks[i]);
#ifndef CONFIG_USB_EHCI_HCD_AIC
err_put_hcd:
#endif
	if (pdata == &ehci_platform_defaults)
		dev->dev.platform_data = NULL;

	usb_put_hcd(hcd);

	return err;
}

static int aic_ehci_platform_remove(struct platform_device *dev)
{
	struct usb_hcd *hcd = platform_get_drvdata(dev);
	struct usb_ehci_pdata *pdata = dev_get_platdata(&dev->dev);
	struct aic_ehci_platform_priv *priv = hcd_to_ehci_priv(hcd);
	int clk;

	usb_remove_hcd(hcd);

	if (pdata->power_off)
		pdata->power_off(dev);

	for (clk = 0; clk < USB_MAX_CLKS_RSTS && priv->clks[clk]; clk++)
		clk_put(priv->clks[clk]);

	usb_put_hcd(hcd);

	if (pdata == &ehci_platform_defaults)
		dev->dev.platform_data = NULL;

	return 0;
}

#ifdef CONFIG_PM_SLEEP

static int aic_ehci_suspend(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct usb_ehci_pdata *pdata = dev_get_platdata(dev);
	struct platform_device *pdev = to_platform_device(dev);
	bool do_wakeup = device_may_wakeup(dev);
	int ret;

	ret = ehci_suspend(hcd, do_wakeup);
	if (ret)
		return ret;

	if (pdata->power_suspend)
		pdata->power_suspend(pdev);

	pinctrl_pm_select_sleep_state(dev);

	return ret;
}

static int aic_ehci_resume(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct usb_ehci_pdata *pdata = dev_get_platdata(dev);
	struct platform_device *pdev = to_platform_device(dev);
	int err;

	pinctrl_pm_select_default_state(dev);

	if (pdata->power_on) {
		err = pdata->power_on(pdev);
		if (err < 0)
			return err;
	}

	ehci_resume(hcd, false);
	return 0;
}

static SIMPLE_DEV_PM_OPS(aic_ehci_pm_ops, aic_ehci_suspend, aic_ehci_resume);

#endif /* CONFIG_PM_SLEEP */

static const struct of_device_id aic_ehci_ids[] = {
	{ .compatible = "artinchip,aic-usbh-v1.0", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, aic_ehci_ids);

static struct platform_driver ehci_platform_driver = {
	.probe		= aic_ehci_platform_probe,
	.remove		= aic_ehci_platform_remove,
	.shutdown	= usb_hcd_platform_shutdown,
	.driver		= {
		.name	= "aic-ehci",
#ifdef CONFIG_PM_SLEEP
		.pm	= &aic_ehci_pm_ops,
#endif
		.of_match_table = aic_ehci_ids,
	}
};

static int __init ehci_platform_init(void)
{
	if (usb_disabled())
		return -ENODEV;

	pr_info("%s: " DRIVER_DESC "\n", hcd_name);

	ehci_init_driver(&ehci_platform_hc_driver, &platform_overrides);
	return platform_driver_register(&ehci_platform_driver);
}
module_init(ehci_platform_init);

static void __exit ehci_platform_cleanup(void)
{
	platform_driver_unregister(&ehci_platform_driver);
}
module_exit(ehci_platform_cleanup);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR(" Matteo <duanmt@artinchip.com>");
MODULE_LICENSE("GPL");
