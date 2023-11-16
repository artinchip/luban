// SPDX-License-Identifier: GPL-2.0
/*
 * Artinchip OTG driver
 *
 * Copyright (C) 2020 ARTINCHIP - All Rights Reserved
 *
 *
 */
// #define DEBUG
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
#include <linux/device.h>
#include <linux/irq.h>
#include <linux/debugfs.h>

struct aic_otg_drv_data {
	struct platform_device *pdev;
	unsigned int gpio_vbus_sw;
	unsigned int gpio_id;
	int irq_id;
	int mode;
	bool id_en;
};

#define DRIVER_DESC	"Artinchip OTG driver"
#define DRV_NAME	"aic-otg"

#define MODE_HOST	0
#define MODE_DEVICE	1
#define MODE_MSK	(MODE_HOST | MODE_DEVICE)
#define MODE_FORCE_FLG	0x100

static char *mode_str[] = {
	"host",
	"device",
};

extern void syscfg_usb_phy0_sw_host(int sw);
static void aic_otg_set_mode(struct aic_otg_drv_data *d, int mode);
static void aic_otg_set_mode_by_id(struct aic_otg_drv_data *d);
static int aic_otg_get_id_val(struct aic_otg_drv_data *d);
static int aic_otg_id_detect_en(struct aic_otg_drv_data *d);
static void aic_otg_id_detect_dis(struct aic_otg_drv_data *d);

static ssize_t otg_mode_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	struct aic_otg_drv_data *d = dev_get_drvdata(dev);
	bool force = d->mode & MODE_FORCE_FLG;
	int mode = d->mode & MODE_MSK;
	int val = 0;
	ssize_t count = 0;

	if (force) {
		count = snprintf(buf, PAGE_SIZE, "%s\n", mode_str[mode]);
	} else {
		val = aic_otg_get_id_val(d);
		count = snprintf(buf, PAGE_SIZE,
				 "auto (current mode: %s, id pin: %d)\n",
				 mode_str[mode], val);
	}

	return count;
}

static ssize_t otg_mode_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct aic_otg_drv_data *d = dev_get_drvdata(dev);
	int ret = 0;

	if (!strncmp(buf, "host", 4)) {
		aic_otg_id_detect_dis(d);
		d->mode = MODE_FORCE_FLG | MODE_HOST;
		aic_otg_set_mode(d, MODE_HOST);
	} else if (!strncmp(buf, "device", 6)) {
		aic_otg_id_detect_dis(d);
		d->mode = MODE_FORCE_FLG | MODE_DEVICE;
		aic_otg_set_mode(d, MODE_DEVICE);
	} else if (!strncmp(buf, "auto", 4)) {
		d->mode = 0;
		ret = aic_otg_id_detect_en(d);
		if (ret)
			return ret;
	} else {
		return -EINVAL;
	}

	return count;
}

static DEVICE_ATTR(otg_mode, 0644, otg_mode_show, otg_mode_store);

static void aic_otg_set_mode(struct aic_otg_drv_data *d, int mode)
{
	if (mode == MODE_DEVICE) {
		syscfg_usb_phy0_sw_host(0);
		if (gpio_is_valid(d->gpio_vbus_sw))
			gpio_set_value(d->gpio_vbus_sw, 0);
		pr_info("%s: id pin = high, switch to Device mode.\n",
			DRV_NAME);
	} else {
		syscfg_usb_phy0_sw_host(1);
		if (gpio_is_valid(d->gpio_vbus_sw))
			gpio_set_value(d->gpio_vbus_sw, 1);
		pr_info("%s: id pin = low, switch to Host mode.\n",
			DRV_NAME);
	}
}

static void aic_otg_set_mode_by_id(struct aic_otg_drv_data *d)
{
	int new_mode = 0;
	int val = 0;

	if (d->mode & MODE_FORCE_FLG)
		return;

	val = gpio_get_value(d->gpio_id);
	/* id pin = high */
	if (val)
		new_mode = MODE_DEVICE;
	/* id pin = low */
	else
		new_mode = MODE_HOST;

	dev_dbg(&d->pdev->dev, "%s: val = %d, new_mode = %d, d->mode = %d.\n",
		__func__, val, new_mode, d->mode);

	aic_otg_set_mode(d, new_mode);
	d->mode = new_mode;
}

static int aic_otg_get_id_val(struct aic_otg_drv_data *d)
{
	return gpio_get_value(d->gpio_id);
}

static irqreturn_t aic_otg_irq(int irq, void *data)
{
	struct platform_device *pdev = data;
	struct aic_otg_drv_data *d = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "Interrupt hadnle: %s.\n", __func__);

	/* sleep unbonuce */

	aic_otg_set_mode_by_id(d);

	return IRQ_HANDLED;
}

static int aic_otg_id_detect_en(struct aic_otg_drv_data *d)
{
	int ret;

	if (!gpio_is_valid(d->gpio_id))
		return -EINVAL;

	if (d->id_en)
		return 0;
	d->id_en = true;

	gpio_direction_input(d->gpio_id);
	gpio_set_debounce(d->gpio_id, 10000);

	aic_otg_set_mode_by_id(d);

	d->irq_id = gpio_to_irq(d->gpio_id);
	dev_dbg(&d->pdev->dev, "%s: d->irq_id = %d.\n",
		__func__, d->irq_id);

	ret = devm_request_irq(&d->pdev->dev, d->irq_id, aic_otg_irq,
			       IRQ_TYPE_EDGE_BOTH, DRV_NAME, d->pdev);

	return 0;
}

static void aic_otg_id_detect_dis(struct aic_otg_drv_data *d)
{
	if (!gpio_is_valid(d->gpio_id))
		return;

	if (!d->id_en)
		return;
	d->id_en = false;

	disable_irq(d->irq_id);
	devm_free_irq(&d->pdev->dev, d->irq_id, d->pdev);
}

static int aic_otg_platform_probe(struct platform_device *pdev)
{
	struct aic_otg_drv_data *d;
	unsigned int gpio = 0;
	int ret = 0;
	const char *mod;
	bool force = false;

	d = kzalloc(sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;
	platform_set_drvdata(pdev, d);
	d->pdev = pdev;

	gpio = of_get_named_gpio(pdev->dev.of_node, "vbus-en-gpios", 0);
	d->gpio_vbus_sw = gpio;

	/* Get force mode define from dts
	 * Force Mode	: field 'otg-mode' need defined as 'host' or 'device'
	 * Auto Mode	: field 'otg-mode' defined ad 'auto' or absent
	 */
	ret = of_property_read_string(pdev->dev.of_node, "otg-mode", &mod);
	if (!ret) {
		if (!strncmp(mod, "host", 4)) {
			force = true;
			d->mode = MODE_FORCE_FLG | MODE_HOST;
			aic_otg_set_mode(d, MODE_HOST);
		} else if (!strncmp(mod, "device", 6)) {
			force = true;
			d->mode = MODE_FORCE_FLG | MODE_DEVICE;
			aic_otg_set_mode(d, MODE_DEVICE);
		} else if (!strncmp(mod, "auto", 4)) {
			force = false;
		} else {
			force = false;
		}
	}

	/* Get id pins gpio
	 * Auto Mode	: must define id pins
	 * Force Mode	: id pins define is not necessary
	 */
	gpio = of_get_named_gpio(pdev->dev.of_node, "id-gpios", 0);
	if (!gpio_is_valid(gpio) && !force) {
		ret = -EINVAL;
		goto err_free_data;
	}
	d->gpio_id = gpio;

	/* Auto mode enable id pin gpio */
	if (!force)
		aic_otg_id_detect_en(d);

	device_create_file(&pdev->dev, &dev_attr_otg_mode);

	return 0;

err_free_data:
	if (d)
		kfree(d);
	return ret;
}

static int aic_otg_platform_remove(struct platform_device *pdev)
{
	struct aic_otg_drv_data *d = platform_get_drvdata(pdev);

	device_remove_file(&pdev->dev, &dev_attr_otg_mode);

	aic_otg_id_detect_dis(d);
	if (d)
		kfree(d);
	return 0;
}

static const struct of_device_id aic_otg_ids[] = {
	{ .compatible = "artinchip,aic-otg-v1.0", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, aic_otg_ids);

static struct platform_driver otg_platform_driver = {
	.probe		= aic_otg_platform_probe,
	.remove		= aic_otg_platform_remove,
	.driver		= {
		.name	= DRV_NAME,
		.of_match_table = aic_otg_ids,
	}
};

static int __init otg_platform_init(void)
{
	if (usb_disabled())
		return -ENODEV;

	pr_info(DRV_NAME ": " DRIVER_DESC "\n");

	return platform_driver_register(&otg_platform_driver);
}
module_init(otg_platform_init);

static void __exit otg_platform_cleanup(void)
{
	platform_driver_unregister(&otg_platform_driver);
}
module_exit(otg_platform_cleanup);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
