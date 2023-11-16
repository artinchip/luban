/******************************************************************************
 *
 * Copyright(c) 2013 - 2017 Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 *****************************************************************************/
/*
 * Description:
 *	This file can be applied to following platforms:
 *	CONFIG_PLATFORM_ARM_SUNXI Series platform
 *
 */

#include <drv_types.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
//#include <linux/sunxi-gpio.h>

#define WIFI_GPIO_ACTIVE(flag) (((flag) & OF_GPIO_ACTIVE_LOW) ? 0 : 1)
#define WIFI_GPIO_INACTIVE(flag) (((flag) & OF_GPIO_ACTIVE_LOW) ? 1 : 0)

//static struct gpio_config wifi_pw_gpio;
static int wifi_pw_gpio = -1;
static enum of_gpio_flags gpio_flags;
#ifdef CONFIG_PLATFORM_ARM_SUNxI
extern int sunxi_usb_disable_hcd(__u32 usbc_no);
extern int sunxi_usb_enable_hcd(__u32 usbc_no);
static int usb_wifi_host = 2;
#endif

int platform_wifi_power_on(void)
{
	int ret = 0;

#ifdef CONFIG_PLATFORM_ARM_SUNxI
 
	int wifiused = 0;
	struct device_node *np = NULL;
	
	np = of_find_node_by_name(NULL, "wifi_para");
	if (!np) {
		pr_err("ERROR! get wifi_para failed, func:%s, line:%d\n",
							__func__, __LINE__);
		return -1;
	}
	ret = of_property_read_u32(np, "wifi_used", &wifiused);
	if (ret) {
		pr_err("get wifi_used is fail, %d\n", ret);
		return -1;
	}
	if(wifiused == 0){
		pr_err("get wifi_used is 0\n");
		return -1;
	}
	

	ret = of_property_read_u32(np, "wifi_usbc_id", &usb_wifi_host);
	if (ret) {
		pr_err("get wifi_usbc_id is fail, %d\n", ret);
		return -1;
	}

	printk("sw_usb_enable_hcd: usbc_num = %d\n", usb_wifi_host);

	wifi_pw_gpio = of_get_named_gpio_flags(np, "wifi_en", 0,&gpio_flags);
	printk("wifi en %d\r\n", wifi_pw_gpio);
	if(wifi_pw_gpio >= 0)
	{
	if (gpio_direction_output(wifi_pw_gpio, 1) != 0)
	{
		pr_err("wifi en gpio set err!");
		return ret;
	}
	gpio_set_value(wifi_pw_gpio, WIFI_GPIO_INACTIVE(gpio_flags));
	msleep(10);
	gpio_set_value(wifi_pw_gpio, WIFI_GPIO_ACTIVE(gpio_flags));
	msleep(100);
	}
	//	sunxi_usb_enable_hcd(usb_wifi_host);
  
#endif /* CONFIG_PLATFORM_ARM_SUNxI */
 
//exit:
	return ret;
}

void platform_wifi_power_off(void)
{
	if(wifi_pw_gpio >= 0)
	gpio_set_value(wifi_pw_gpio, WIFI_GPIO_INACTIVE(gpio_flags));
}
