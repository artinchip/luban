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
#include <linux/usb/otg.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/ehci_pdriver.h>
#include <linux/pinctrl/consumer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/device.h>
#include <linux/irq.h>
#include <linux/debugfs.h>
#include <linux/workqueue.h>

struct aic_otg {
	struct platform_device *pdev;
	struct usb_phy phy;
	struct usb_otg otg;
	struct delayed_work work;
	struct gpio_desc *gpio_dp_sw;
	struct gpio_desc *gpio_vbus_en;
	struct gpio_desc *gpio_id;
	int irq_id;
	int mode;
	unsigned char host_on;
	unsigned char host1_on;
	unsigned char gadget_on;
	unsigned char gpio_init;
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
static void aic_otg_update_mode(struct aic_otg *d);
static void aic_otg_set_mode_by_id(struct aic_otg *d);
static int aic_otg_get_id_val(struct aic_otg *d);
static int aic_otg_id_detect_en(struct aic_otg *d);
static void aic_otg_id_detect_dis(struct aic_otg *d);
static void aic_otg_gpio_init(struct aic_otg *d);

/* B-device start SRP */
static int aic_otg_start_srp(struct usb_otg *otg)
{
	if (!otg || otg->state != OTG_STATE_B_IDLE)
		return -ENODEV;

	return 0;
}

/* A_host suspend will call this function to start hnp */
static int aic_otg_start_hnp(struct usb_otg *otg)
{
	if (!otg)
		return -ENODEV;

	return 0;
}

static int aic_otg_set_vbus(struct usb_otg *otg, bool on)
{
	struct aic_otg *d = container_of(otg, struct aic_otg, otg);

	if (d->gpio_vbus_en && !IS_ERR(d->gpio_vbus_en)) {
		gpiod_set_value(d->gpio_vbus_en, on);
		dev_info(&d->pdev->dev, "set vbus %s\n", on ? "on" : "off");
	}

	if (d->gpio_dp_sw && !IS_ERR(d->gpio_dp_sw)) {
		gpiod_set_value(d->gpio_dp_sw, on);
		dev_info(&d->pdev->dev, "switch dp to %s\n",
			 on ? "host" : "device");
	}

	return 0;
}

static int aic_otg_set_host(struct usb_otg *otg, struct usb_bus *host)
{
	struct aic_otg *d = container_of(otg, struct aic_otg, otg);

	otg->host = host;

	if (host) {
		d->host_on = 1;
		aic_otg_gpio_init(d);
	}

	return 0;
}

static int aic_otg_set_host1(struct usb_otg *otg, struct usb_bus *host)
{
	struct aic_otg *d = container_of(otg, struct aic_otg, otg);

	otg->host1 = host;

	if (host) {
		d->host1_on = 1;
		aic_otg_gpio_init(d);
	}

	return 0;
}

static int aic_otg_set_peripheral(struct usb_otg *otg,
				  struct usb_gadget *gadget)
{
	struct aic_otg *d = container_of(otg, struct aic_otg, otg);

	otg->gadget = gadget;

	if (gadget) {
		d->gadget_on = 1;
		aic_otg_gpio_init(d);
	} else {
		d->gadget_on = 0;
	}

	return 0;
}

static void aic_otg_start_host(struct aic_otg *d, unsigned char on)
{
	struct usb_otg *otg = &d->otg;
	struct usb_hcd *hcd;

	if (!otg->host && !otg->host1)
		return;

	dev_info(&d->pdev->dev, "%s host\n", on ? "start" : "stop");

	if (on)
		otg_set_vbus(otg, 1);
	else
		otg_set_vbus(otg, 0);

	if (otg->host && on != d->host_on) {
		d->host_on = on;
		hcd = bus_to_hcd(otg->host);

		if (on) {
			usb_add_hcd(hcd, hcd->irq, IRQF_SHARED);
			device_wakeup_enable(hcd->self.controller);
		} else {
			usb_remove_hcd(hcd);
		}
	}

	if (otg->host1 && on != d->host1_on) {
		d->host1_on = on;
		hcd = bus_to_hcd(otg->host1);

		if (on) {
			usb_add_hcd(hcd, hcd->irq, IRQF_SHARED);
			device_wakeup_enable(hcd->self.controller);
		} else {
			usb_remove_hcd(hcd);
		}
	}
}

static void aic_otg_start_peripheral(struct aic_otg *d, unsigned char on)
{
	struct usb_otg *otg = &d->otg;

	if (!otg->gadget || on == d->gadget_on)
		return;

	d->gadget_on = on;

	dev_info(&d->pdev->dev, "%s gadget\n", on ? "start" : "stop");

	if (on)
		usb_gadget_vbus_connect(otg->gadget);
	else
		usb_gadget_vbus_disconnect(otg->gadget);
}

static void aic_otg_work(struct work_struct *work)
{
	struct aic_otg *d;

	d = container_of(to_delayed_work(work), struct aic_otg, work);

	if ((d->mode & MODE_MSK) == MODE_DEVICE) {
		aic_otg_start_host(d, 0);
		syscfg_usb_phy0_sw_host(0);
		aic_otg_start_peripheral(d, 1);

	} else {
		aic_otg_start_peripheral(d, 0);
		syscfg_usb_phy0_sw_host(1);
		aic_otg_start_host(d, 1);
	}
}

static ssize_t otg_mode_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	struct aic_otg *d = dev_get_drvdata(dev);
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
	struct aic_otg *d = dev_get_drvdata(dev);
	int ret = 0;

	if (!strncmp(buf, "host", 4)) {
		aic_otg_id_detect_dis(d);
		d->mode = MODE_FORCE_FLG | MODE_HOST;
		aic_otg_update_mode(d);
	} else if (!strncmp(buf, "device", 6)) {
		aic_otg_id_detect_dis(d);
		d->mode = MODE_FORCE_FLG | MODE_DEVICE;
		aic_otg_update_mode(d);
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

static void aic_otg_update_mode(struct aic_otg *d)
{
	cancel_delayed_work(&d->work);
	schedule_delayed_work(&d->work, (HZ / 10));
}

static void aic_otg_set_mode_by_id(struct aic_otg *d)
{
	int new_mode = 0;
	int val = 0;

	if (d->mode & MODE_FORCE_FLG)
		return;

	val = gpiod_get_value(d->gpio_id);
	/* id pin = high */
	if (val)
		new_mode = MODE_DEVICE;
	/* id pin = low */
	else
		new_mode = MODE_HOST;

	dev_dbg(&d->pdev->dev, "%s: val = %d, new_mode = %d, d->mode = %d.\n",
		__func__, val, new_mode, d->mode);

	d->mode = new_mode;
	aic_otg_update_mode(d);
}

static int aic_otg_get_id_val(struct aic_otg *d)
{
	return gpiod_get_value(d->gpio_id);
}

static irqreturn_t aic_otg_irq(int irq, void *data)
{
	struct platform_device *pdev = data;
	struct aic_otg *d = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "Interrupt hadnle: %s.\n", __func__);

	/* sleep unbonuce */

	aic_otg_set_mode_by_id(d);

	return IRQ_HANDLED;
}

static int aic_otg_id_detect_en(struct aic_otg *d)
{
	int ret;

	if (!d->gpio_id || IS_ERR(d->gpio_id))
		return -ENODEV;

	if (d->id_en)
		return 0;
	d->id_en = true;

	gpiod_set_debounce(d->gpio_id, 10000);

	aic_otg_set_mode_by_id(d);

	d->irq_id = gpiod_to_irq(d->gpio_id);
	dev_dbg(&d->pdev->dev, "%s: d->irq_id = %d.\n",
		__func__, d->irq_id);

	ret = devm_request_irq(&d->pdev->dev, d->irq_id, aic_otg_irq,
			       IRQ_TYPE_EDGE_BOTH, DRV_NAME, d->pdev);

	return 0;
}

static void aic_otg_id_detect_dis(struct aic_otg *d)
{
	if (!d->gpio_id || IS_ERR(d->gpio_id))
		return;

	if (!d->id_en)
		return;
	d->id_en = false;

	disable_irq(d->irq_id);
	devm_free_irq(&d->pdev->dev, d->irq_id, d->pdev);
}

static void aic_otg_gpio_init(struct aic_otg *d)
{
	if (!d->otg.host || !d->otg.host1 || !d->otg.gadget)
		return;
	if (d->gpio_init)
		return;

	d->gpio_init = 1;

	d->gpio_dp_sw = devm_gpiod_get_optional(&d->pdev->dev, "dp-sw",
						GPIOD_OUT_HIGH);
	d->gpio_vbus_en = devm_gpiod_get_optional(&d->pdev->dev, "vbus-en",
						  GPIOD_OUT_HIGH);
	d->gpio_id = devm_gpiod_get_optional(&d->pdev->dev, "id",
					     GPIOD_IN);

	dev_dbg(&d->pdev->dev, "d->gpio_vbus_en = 0x%lx\n",
		(unsigned long)d->gpio_vbus_en);
	dev_dbg(&d->pdev->dev, "d->gpio_dp_sw = 0x%lx\n",
		(unsigned long)d->gpio_dp_sw);
	dev_dbg(&d->pdev->dev, "d->gpio_id = 0x%lx\n",
		(unsigned long)d->gpio_id);

	/* Auto mode enable id pin gpio */
	if (!(d->mode & MODE_FORCE_FLG))
		aic_otg_id_detect_en(d);
	else
		aic_otg_update_mode(d);
}

static int aic_otg_platform_probe(struct platform_device *pdev)
{
	struct aic_otg *d;
	int ret = 0;
	const char *mod;

	d = kzalloc(sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;
	platform_set_drvdata(pdev, d);
	d->pdev = pdev;

	INIT_DELAYED_WORK(&d->work, aic_otg_work);

	/* Get force mode define from dts
	 * Force Mode	: field 'otg-mode' need defined as 'host' or 'device'
	 * Auto Mode	: field 'otg-mode' defined ad 'auto' or absent
	 */
	ret = of_property_read_string(pdev->dev.of_node, "otg-mode", &mod);
	if (!ret) {
		if (!strncmp(mod, "host", 4)) {
			d->mode = MODE_FORCE_FLG | MODE_HOST;
		} else if (!strncmp(mod, "device", 6)) {
			d->mode = MODE_FORCE_FLG | MODE_DEVICE;
		} else if (!strncmp(mod, "auto", 4)) {
			d->mode = 0;
		} else {
			d->mode = 0;
		}
	}

	device_create_file(&pdev->dev, &dev_attr_otg_mode);

	/* initialize the otg structure */
	d->phy.label = DRIVER_DESC;
	d->phy.dev = &pdev->dev;
	d->phy.otg = &d->otg;

	d->otg.usb_phy = &d->phy;
	d->otg.set_vbus = aic_otg_set_vbus;
	d->otg.set_host = aic_otg_set_host;
	d->otg.set_host1 = aic_otg_set_host1;
	d->otg.set_peripheral = aic_otg_set_peripheral;
	d->otg.start_hnp = aic_otg_start_hnp;
	d->otg.start_srp = aic_otg_start_srp;

	ret = usb_add_phy(&d->phy, USB_PHY_TYPE_USB2);
	if (ret) {
		dev_err(&pdev->dev, "Unable to register OTG transceiver.\n");
		goto err_free_data;
	}

	return 0;

err_free_data:
	if (d)
		kfree(d);
	return ret;
}

static int aic_otg_platform_remove(struct platform_device *pdev)
{
	struct aic_otg *d = platform_get_drvdata(pdev);

	device_remove_file(&pdev->dev, &dev_attr_otg_mode);

	aic_otg_id_detect_dis(d);

	usb_remove_phy(&d->phy);

	if (d->gpio_dp_sw && !IS_ERR(d->gpio_dp_sw))
		devm_gpiod_put(&pdev->dev, d->gpio_dp_sw);
	if (d->gpio_vbus_en && !IS_ERR(d->gpio_vbus_en))
		devm_gpiod_put(&pdev->dev, d->gpio_vbus_en);
	if (d->gpio_id && !IS_ERR(d->gpio_id))
		devm_gpiod_put(&pdev->dev, d->gpio_id);
	if (d)
		kfree(d);
	return 0;
}

static const struct of_device_id aic_otg_ids[] = {
	{ .compatible = "artinchip,aic-otg-v2.0", },
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
arch_initcall(otg_platform_init);

static void __exit otg_platform_cleanup(void)
{
	platform_driver_unregister(&otg_platform_driver);
}
module_exit(otg_platform_cleanup);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
