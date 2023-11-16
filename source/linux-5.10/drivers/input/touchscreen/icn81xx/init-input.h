/*
 * Copyright (c) 2013-2015 liming@allwinnertech.com
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */
#ifndef _INIT_INPUT_H
#define _INIT_INPUT_H

#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
//#include <linux/sunxi-gpio.h>

typedef u32 (*gpio_int_handle)(void *para);

enum input_sensor_type 
{
	CTP_TYPE,
};

struct ctp_config_info 
{
	enum input_sensor_type input_type;
	int ctp_used;
	u32 twi_id;
	int screen_max_x;
	int screen_max_y;
	int revert_x_flag;
	int revert_y_flag;
	int exchange_x_y_flag;
	int convert_reslution;
	u32 int_number;
	int irq_gpio;
	int wakeup_gpio;
	struct device *dev;
};

int input_sensor_startup(enum input_sensor_type *input_type);
void input_sensor_free(enum input_sensor_type *input_type);
int input_sensor_init(enum input_sensor_type *input_type);
int input_request_int(enum input_sensor_type *input_type, irq_handler_t handle,
			unsigned long trig_type, void *para);
int input_free_int(enum input_sensor_type *input_type, void *para);
int input_set_int_enable(enum input_sensor_type *input_type, u32 enable);
int input_set_power_enable(enum input_sensor_type *input_type, u32 enable);
#endif
