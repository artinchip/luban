/*
 * Copyright (c) 2013-2015 liming@allwinnertech.com
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include "init-input.h"
#include <linux/pinctrl/consumer.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>


/***************************CTP************************************/

/**
 * sunxi_ctp_startup() - get config info from sys_config.fex file.
 *@ctp_type:  sensor type
 * return value:
 *                    = 0; success;
 *                    < 0; err
 */
int input_sensor_startup(enum input_sensor_type *ctp_type)
{
	int ret = -1;
	struct ctp_config_info *data = container_of(ctp_type, struct ctp_config_info, input_type);
	struct device_node *np = NULL;

	np = of_find_node_by_name(NULL, "ctp");
	if (!np)
	{
		pr_err("ERROR! get ctp_para failed, func:%s, line:%d\n", __func__, __LINE__);
		goto devicetree_get_item_err;
	}

	if (!of_device_is_available(np)) 
	{
		pr_err("%s: ctp is not used\n", __func__);
		goto devicetree_get_item_err;
	}
	else
		data->ctp_used = 1;

	ret = of_property_read_u32(np, "ctp_twi_id", &data->twi_id);
	if (ret) 
	{
		pr_err("get twi_id is fail, %d\n", ret);
		goto devicetree_get_item_err;
	}
	printk("ctp_twi_id is %d\r\n", data->twi_id);

	data->wakeup_gpio = of_get_named_gpio(np, "ctp_wakeup", 0);
	printk("wavkeup_gpio %d\r\n", data->wakeup_gpio);
	
	data->irq_gpio = of_get_named_gpio(np, "ctp_int_port", 0);
	printk("ctp_int_port %d\r\n", data->irq_gpio);
	
	ret = of_property_read_u32(np, "ctp_screen_max_x", &data->screen_max_x);
	if (ret)
		pr_err("get ctp_screen_max_x is fail, %d\n", ret);

	ret = of_property_read_u32(np, "ctp_screen_max_y", &data->screen_max_y);
	if (ret)
		pr_err("get screen_max_y is fail, %d\n", ret);

	ret = of_property_read_u32(np, "ctp_revert_x_flag", &data->revert_x_flag);
	if (ret)
		pr_err("get revert_x_flag is fail, %d\n", ret);

	ret = of_property_read_u32(np, "ctp_revert_y_flag", &data->revert_y_flag);
	if (ret)
		pr_err("get revert_y_flag is fail, %d\n", ret);

	ret = of_property_read_u32(np, "ctp_exchange_x_y_flag", &data->exchange_x_y_flag);
	if (ret)
		pr_err("get ctp_exchange_x_y_flag is fail, %d\n", ret);


	data->convert_reslution = 0;
	ret = of_property_read_u32(np, "ctp_convert_reslution", &data->convert_reslution);
	if (ret)
		pr_err("get convert_reslution is fail, %d\n", ret);

		
	printk("screen %d %d %d %d %d\r\n", data->screen_max_x, data->screen_max_y, data->revert_x_flag, data->revert_y_flag, data->exchange_x_y_flag);
	return 0;

devicetree_get_item_err:
	pr_notice("=========script_get_item_err============\n");
	return ret;
}

/**
 * sunxi_ctp_free() - free ctp related resource
 * @ctp_type:sensor type
 */
void input_sensor_free(enum input_sensor_type *ctp_type)
{
//	struct ctp_config_info *data = container_of(ctp_type, struct ctp_config_info, input_type);

}

/**
 * sunxi_ctp_init - initialize platform related resource
 * @ctp_type:sensor type
 * return value: 0 : success
 *               -EIO :  i/o err.
 *
 */
int input_sensor_init(enum input_sensor_type *ctp_type)
{
	int ret = -1;
	struct ctp_config_info *data = container_of(ctp_type, struct ctp_config_info, input_type);

	if (gpio_direction_output(data->wakeup_gpio, 1) != 0)
	{
		pr_err("wakeup gpio set err!");
		return ret;
	}
	if (gpio_direction_input(data->irq_gpio) != 0)
	{
		pr_err("wakeup gpio set err!");
		return ret;
	}

	ret = 0;
	return ret;
}
/*************************CTP END************************************/

/**
 * input_set_int_enable() - input set irq enable
 * Input_type:sensor type
 *      enable:
 * return value: 0 : success
 *               -EIO :  i/o err.
 */
int input_set_int_enable(enum input_sensor_type *input_type, u32 enable)
{
	int ret = -1;
	u32 irq_number = 0;
	struct ctp_config_info *data = NULL;

	switch (*input_type) 
	{
		case CTP_TYPE:
			data = container_of(input_type, struct ctp_config_info, input_type);
			irq_number = gpio_to_irq(data->irq_gpio);
			//printk("irqno = 0x%x \n",irq_number);
			break;
		default:
			break;
	}

	if ((enable != 0) && (enable != 1))
		return ret;

	if (enable == 1)
		enable_irq(irq_number);
	else
		disable_irq_nosync(irq_number);

	return 0;
}

/**
 * input_free_int - input free irq
 * Input_type:sensor type
 * return value: 0 : success
 *               -EIO :  i/o err.
 */
int input_free_int(enum input_sensor_type *input_type, void *para)
{
	int irq_number = 0;
	struct ctp_config_info *data = NULL;
	struct device *dev = NULL;

	switch (*input_type)
	{
		case CTP_TYPE:
			data = container_of(input_type, struct ctp_config_info, input_type);
			irq_number = gpio_to_irq(data->irq_gpio);
			printk("irqno = 0x%x \n",irq_number);
			dev = ((struct ctp_config_info *)data)->dev;
			break;
		default:
			break;
	}

	free_irq(irq_number, para);

	return 0;
}

/**
 * input_request_int() - input request irq
 * Input:
 *	type:
 *      handle:
 *      trig_gype:
 *      para:
 * return value: 0 : success
 *               -EIO :  i/o err.
 *
 */
int input_request_int(enum input_sensor_type *input_type, irq_handler_t handle,
			unsigned long trig_type, void *para)
{
	int ret = -1;
	int irq_number = 0;

	struct ctp_config_info *data = NULL;
	struct device *dev = NULL;

	switch (*input_type) 
	{
		case CTP_TYPE:
			data = container_of(input_type, struct ctp_config_info, input_type);
			irq_number = gpio_to_irq(data->irq_gpio);
			printk("irqno = 0x%d %d\n",data->irq_gpio, irq_number);
			if (IS_ERR_VALUE((unsigned long)irq_number)) 
			{
				pr_warn("map gpio to virq failed, errno = %d\n", irq_number);
				return -EINVAL;
			}
			pr_info("input_request_int irq_number %d\n",irq_number);
			dev = data->dev;
			break;
		default:
			break;
	}

	irq_set_irq_type(irq_number, trig_type); //ÏÂ½µÑØ´¥·¢
	/* request virq, set virq type to high level trigger */
	ret =  request_irq(irq_number, handle, trig_type, "ctp_isr", para);
	if (IS_ERR_VALUE((unsigned long)ret)) 
	{
		pr_warn("request virq %d failed, errno = %d\n", irq_number, ret);
		return -EINVAL;
	}

	return 0;
}

