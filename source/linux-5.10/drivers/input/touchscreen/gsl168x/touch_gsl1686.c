/* 
 * gsl1686 touch screen driver
 *
 * Copyright (c) 2018 
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/input/touchscreen.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/of_device.h>
#include <asm/unaligned.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include "init-input.h"
#include <linux/pinctrl/consumer.h>
#include "gsl1686.h"
#include <linux/delay.h>

#define MSG_NAME	"gsl1686"

#define GSL_DATA_REG		0x80
#define GSL_STATUS_REG		0xe0
#define GSL_PAGE_REG		0xf0

#define PRESS_MAX    		255
#define MAX_FINGERS 		1
#define MAX_CONTACTS 		10
#define DMA_TRANS_LEN		0x20

#define X_INDEX 				6
#define Y_INDEX					4
#define ID_INDEX				7

struct gsl_ts_data {
	u8 x_index;
	u8 y_index;
	u8 z_index;
	u8 id_index;
	u8 touch_index;
	u8 data_reg;
	u8 status_reg;
	u8 data_size;
	u8 touch_bytes;
	u8 update_data;
	u8 touch_meta_data;
	u8 finger_size;
};


struct gsl_ts_data devices = 
{
	.x_index = 6,
	.y_index = 4,
	.z_index = 5,
	.id_index = 7,
	.touch_index = 0,
	.data_reg = GSL_DATA_REG,
	.status_reg = GSL_STATUS_REG,
	.data_size = 0,
	.touch_bytes = 0x04,
	.update_data = 4,
	.touch_meta_data = 4,
	.finger_size = 0x70
};

struct ts_event {
	u16	x;
	u16	y;
};

struct ts_data {
	struct input_dev	*input_dev;
	struct ts_event		event;
	struct work_struct 	pen_event_work;
	struct workqueue_struct *ts_workqueue;	
	struct delayed_work  poll_work;
	struct workqueue_struct *ts_poll_workqueue;
	struct semaphore tp_cs;	
};


static struct i2c_client *this_client;
static char i2c_lock_flag = 0;

static struct ctp_config_info config_info =
{
	.input_type = CTP_TYPE,
	.int_number = 0,
};

static u32 debug_mask = 0xff;

enum
{
	DEBUG_INIT = 1U << 0,
	DEBUG_SUSPEND = 1U << 1,
	DEBUG_INT_INFO = 1U << 2,
	DEBUG_X_Y_INFO = 1U << 3,
	DEBUG_KEY_INFO = 1U << 4,
	DEBUG_WAKEUP_INFO = 1U << 5,
	DEBUG_OTHERS_INFO = 1U << 6,
};

#define dprintk(level_mask,fmt,arg...)    if(unlikely(debug_mask & level_mask)) \
        printk("***CTP***"fmt, ## arg)
module_param_named(debug_mask,debug_mask,int,S_IRUGO | S_IWUSR | S_IWGRP);


///////////////////////////////////////////////
//specific tp related macro: need be configured for specific tp
#define CTP_IRQ_MODE			(IRQF_TRIGGER_RISING)
#define CTP_NAME				MSG_NAME

/* Addresses to scan */
static const unsigned short normal_i2c[2] = {0x40,I2C_CLIENT_END};

static int init_gsl168x(void);
static void ts_release(void);

static int ts_i2c_txdata(char *txdata, int length)
{
	int ret;

	struct i2c_msg msgs[] = 
	{
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= length,
			.buf	= txdata,
		},
	};

	ret = i2c_transfer(this_client->adapter, msgs, 1);
	if (ret < 0)
		pr_info("msg %s i2c write error: %d\n", __func__, ret);
	else
		ret = 0;
	return ret;
}

static int ts_i2c_rxdata(char *rxdata, int length)
{
	int ret;

	struct i2c_msg msgs[] = 
	{
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= rxdata,
		},
		{
			.addr	= this_client->addr,
			.flags	= I2C_M_RD,
			.len	= length,
			.buf	= rxdata,
		},
	};

	ret = i2c_transfer(this_client->adapter, msgs, 2);
	if (ret < 0)
		pr_info("msg %s i2c read error: %d\n", __func__, ret);
	else
		ret = 0;
	return ret;
}

void fw2buf(u8 *buf, const u32 *fw)
{
	u32 *u32_buf = (u32 *)buf;
	*u32_buf = *fw;
}


static int  gsl_load_fw(void)
{
	struct fw_data *ptr_fw;
	u8 buf[DMA_TRANS_LEN*4 + 1] = {0};
//	u8 send_flag = 1;
//	u8 *cur = buf + 1;
	u32 source_line = 0;
	u32 source_len = 0;
	int ret;
	
	ptr_fw = (struct fw_data*)GSLX680_FW;	
	source_len = ARRAY_SIZE(GSLX680_FW);
	printk("=============gsl_load_fw start============== %x\r\n", source_len);  

	for (source_line = 0; source_line < source_len; source_line++) 
	{
//		printk("load %d %d %02x %02x %02x %02x %02x\r\n", source_line, verify_line, buf[0], buf[1], buf[2], buf[3], buf[4]);
		buf[0] = GSLX680_FW[source_line].offset;
		memcpy(&buf[1], &GSLX680_FW[source_line].val, 4);
//		if(buf[0] == 0xf0)
//		{
//			msleep(1);
//			ret = ts_i2c_txdata(buf,2);
//			msleep(1);
//		}
//		else
		{
			ret = ts_i2c_txdata(buf,5);
		}
		if(ret)
		{
			printk("gsl_load_fw I2C_Write fail \r\n");
			return -1;
		}
	}
	
 	printk("=============gsl_load_fw END==============\r\n");   
	return 0;
}

static int startup_chip(void)
{
	u8 buf[2];

	buf[0] = 0xe0;
	buf[1] = 0x00;
	if(ts_i2c_txdata(buf,2))
	{
		printk("gsl_load_fw I2C_Write fail \r\n");
		return -1;
	}
	
#ifdef GSL_NOID_VERSION
	gsl_DataInit((  unsigned int  *)(&gsl_config_data_id[0]));
#endif
	mdelay(10);

	return 0;
}

static int reset_chip(void)
{
	u8 buf[5] = {0x00};

	buf[0] = 0xe0;
	buf[1] = 0x88;//tmp;
	if(ts_i2c_txdata(buf,2))
	{
		printk("reset_chip I2C_Write fail \r\n");
		return -1;
	}

	mdelay(5);

	buf[0] = 0xe4;
	buf[1] = 0x04;
	if(ts_i2c_txdata(buf,2))
	{
		printk("reset_chip I2C_Write fail \r\n");
		return -1;
	}
	mdelay(10);

	memset(buf,0,5);
	buf[0] = 0xbc;
	if(ts_i2c_txdata(buf,5))
	{
		printk("reset_chip I2C_Write fail \r\n");
		return -1;
	}
	
	mdelay(10);

	return 0;
}

static int gls_config(void)
{
	u8 data[2]={0};

	data[0] = 0xf0;

	if(ts_i2c_rxdata(data,1))
	{
		printk("gls_config I2C_Read fail \r\n");
		return -1;
	}
	
	printk("@@@@@@@@@@@@@@@@F0=%d @@@@@@@@@@@@ \r\n",data[0]);

	//msleep(2);
	data[0] = 0xf0;
	data[1] = 0x12;
	
	if(ts_i2c_txdata(data,2))
	{
		printk("gls_config I2C_Write fail \r\n");
		return -1;
	}

	//msleep(2);
	data[0] = 0xf0;
	if(ts_i2c_rxdata(data,1))
	{
		printk("gls_config I2C_Read 0xf0 fail \r\n");
		return -1;
	}
	printk("@@@@@@@@@@@@@@@@F0=%d @@@@@@@@@@@@ \r\n",data[0]);

	return 0;
}

static int clr_reg(void)
{
	u8 write_buf[4]	= {0};
	
	write_buf[0] = 0xe0;
	write_buf[1] = 0x88;
	
	if(ts_i2c_txdata(write_buf,2))
	{
		printk("clr_reg I2C_Write 0xe0 fail \r\n");
		return -1;
	}

	mdelay(20);

	write_buf[0] = 0x80;
	write_buf[1] = 0x03;
	if(ts_i2c_txdata(write_buf,2))
	{
		printk("clr_reg I2C_Write 0x80 fail \r\n");
		return -1;
	}
	mdelay(5); 

	write_buf[0] = 0xe4;
	write_buf[1] = 0x04;
	if(ts_i2c_txdata(write_buf,2))
	{
		printk("clr_reg I2C_Write 0xe4 fail \r\n");
		return -1;
	}
	mdelay(5);
	
	write_buf[0] = 0xe0;
	write_buf[1] = 0x00;
	if(ts_i2c_txdata(write_buf,2))
	{
		printk("clr_reg I2C_Write 0xe0 fail \r\n");
		return -1;
	}
	mdelay(20);
	
	return 0;
}

static void check_mem_data(void)
{
	u8 read_buf[4]  = {0};
//	int i;
	
	msleep(30);
//	while(1)
	read_buf[0] = 0xb0;
	if(ts_i2c_rxdata(read_buf,4))
	{
		printk("gls_config I2C_Read 0xf0 fail \r\n");
		return;
	}
#if 0
	{
	for(i = 0; i < 4; i++)
	{
		read_buf[i] = 0xb0 + i;
		if(ts_i2c_rxdata(&read_buf[i],1))
		{
			printk("gls_config I2C_Read 0xf0 fail \r\n");
			return;
		}
	}
	}
#endif
	if (read_buf[0] != 0x5a || read_buf[1] != 0x5a || read_buf[2] != 0x5a || read_buf[3] != 0x5a)
	{
		printk("#########check mem read 0xb0 = %x %x %x %x #########\n", read_buf[0], read_buf[1], read_buf[2], read_buf[3]);
		init_gsl168x();
	}
}


int init_gsl168x(void)
{
	if(gls_config())
	{
		printk("Init_Gsl168x  fail \r\n");
		return -1;
	}
	
	if(clr_reg())
	{
		printk("clr_reg  fail \r\n");
		return -1;
	}

	if(reset_chip())
	{
		printk("reset_chip  fail \r\n");
		return -1;
	}
	if(gsl_load_fw())
	{
		printk("gsl_load_fw  fail \r\n");
		return -1;
	}
	if(startup_chip())
	{
		printk("startup_chip  fail \r\n");
		return -1;
	}
	if(reset_chip())
	{
		printk("reset_chip  fail \r\n");
		return -1;
	}
	if(startup_chip())
	{
		printk("startup_chip  fail \r\n");
		return -1;
	}
	
	mdelay(100);
	
	//init_chip();
	check_mem_data();

	return 0;
}

static int ctp_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;

	printk("ctp_detect\r\n");
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;

	if(config_info.twi_id == adapter->nr)
	{
		dprintk(DEBUG_INIT,"%s: addr= %x\n",__func__,client->addr);
		strlcpy(info->type, CTP_NAME, I2C_NAME_SIZE);
		return 0;
	}
	else
	{
		return -ENODEV;
	}
}

static void ctp_print_info(struct ctp_config_info info,int debug_level)
{
	if(debug_level == DEBUG_INIT)
	{
		dprintk(DEBUG_INIT,"info.ctp_used:%d\n",info.ctp_used);
		dprintk(DEBUG_INIT,"info.twi_id:%d\n",info.twi_id);
		dprintk(DEBUG_INIT,"info.screen_max_x:%d\n",info.screen_max_x);
		dprintk(DEBUG_INIT,"info.screen_max_y:%d\n",info.screen_max_y);
		dprintk(DEBUG_INIT,"info.revert_x_flag:%d\n",info.revert_x_flag);
		dprintk(DEBUG_INIT,"info.revert_y_flag:%d\n",info.revert_y_flag);
		dprintk(DEBUG_INIT,"info.exchange_x_y_flag:%d\n",info.exchange_x_y_flag);
//		dprintk(DEBUG_INIT,"info.irq_gpio_number:%d\n",info.irq_gpio.gpio);
//		dprintk(DEBUG_INIT,"info.wakeup_gpio_number:%d\n",info.wakeup_gpio.gpio);
	}
}

static int ctp_get_system_config(void)
{   
    ctp_print_info(config_info,DEBUG_INIT);
   
    if((config_info.screen_max_x == 0) || (config_info.screen_max_y == 0))
	{
        printk("%s:read config error!\n",__func__);
        return -1;
    }
    return 0;
}

/**
 * ctp_wakeup - function
 *
 */
int ctp_wakeup(int status,int ms)
{
	dprintk(DEBUG_INIT,"***CTP*** %s:status:%d,ms = %d\n",__func__,status,ms);

	if (status == 0)
	{
		if(ms == 0) 
		{
			gpio_set_value(config_info.wakeup_gpio, 0);
		}
		else 
		{
			gpio_set_value(config_info.wakeup_gpio, 0);
			msleep(ms);
			gpio_set_value(config_info.wakeup_gpio, 1);
		}
	}
	if (status == 1) 
	{
		if(ms == 0)
		{
			gpio_set_value(config_info.wakeup_gpio, 1);
		}
		else 
		{
			gpio_set_value(config_info.wakeup_gpio, 1);
			msleep(ms);
			gpio_set_value(config_info.wakeup_gpio, 0);
		}
	}
	msleep(20);

	return 0;
}

static void ts_release(void)
{
	struct ts_data *data = i2c_get_clientdata(this_client);

	input_report_abs(data->input_dev, ABS_PRESSURE, 0);
	input_report_key(data->input_dev, BTN_TOUCH, 0);
	input_sync(data->input_dev);
	return;
}

static u16 join_bytes(u8 a, u8 b)
{
	u16 ab = 0;
	ab = ab | a;
	ab = ab << 8 | b;
	return ab;
}

int ts_read_data(void)
{
	u8 touches;
	int tmp1 = 0;
	struct gsl_touch_info cinfo;
	u32 ix = 0,iy = 0;
	struct ts_data *data = i2c_get_clientdata(this_client);
	struct ts_event *event = &data->event;
	unsigned char buf[32]={0};
	int ret = -1;
	static int downflag = 0;
	int i = 0;

	u8*ptouchdata = &buf[0];
	
#if 1
	for(i = 0; i < 16; i++)
	{
		buf[i] = 0x80 + i;
		if(ts_i2c_rxdata(&buf[i],1))
		{
			pr_info("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
			return -1;
		}
	}
#else
	buf[0] = 0x80;
	ret = ts_i2c_rxdata(buf, 16);
	if (ret < 0) 
	{
		pr_info("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
		return ret;
	}
#endif
	touches = buf[0];
	cinfo.finger_num = touches;
//	printk("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\r\n",
//		buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7],
//		buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
//	printk("-----cinfo.finger_num: %x -----\r\n",touches);
	for(i = 0; i < (touches < MAX_CONTACTS ? touches : MAX_CONTACTS); i ++)
	{
		cinfo.x[i] = join_bytes((ptouchdata[devices.x_index + 4 * i + 1] & 0x0f), ptouchdata[devices.x_index + 4 * i]);
		cinfo.y[i] = join_bytes(ptouchdata[devices.y_index + 4 * i + 1], ptouchdata[devices.y_index + 4 * i ]);
		cinfo.id[i] = (( ptouchdata[devices.id_index  + 4 * i + 1] & 0xf0)>>4);
	}
//	printk("before cinfo.x[0] %d cinfo.y[0] %d \n",cinfo.x[0],cinfo.y[0]);
#ifdef GSL_NOID_VERSION
	gsl_alg_id_main(&cinfo);
	tmp1 = gsl_mask_tiaoping();
	if(tmp1>0&&tmp1<0xffffffff)
	{
		
		buf[0] = 0xf0;
		buf[1] = 0xa;
		buf[2] = 0;
		buf[3] = 0;
		buf[4] = 0;
		ts_i2c_txdata(buf,5);

		buf[0] = 0x08;
		buf[1]=(u8)(tmp1 & 0xff);
		buf[2]=(u8)((tmp1>>8) & 0xff);
		buf[3]=(u8)((tmp1>>16) & 0xff);
		buf[4]=(u8)((tmp1>>24) & 0xff);
		ts_i2c_txdata(buf,5);
	}
#endif
	touches = cinfo.finger_num;
	
	//printk("ix  %d iy %d \n",cinfo.x[0],cinfo.y[0]);
	if((cinfo.y[0] == 0) && (cinfo.x[0] == 0))
	{
		ix = 0;
		iy = 0;
	}
	else if(config_info.exchange_x_y_flag)
	{	
		if(cinfo.x[0] <= config_info.screen_max_y && cinfo.y[0] <= config_info.screen_max_x) 
		{
			ix = cinfo.y[0];
			iy = cinfo.x[0];
		}
		else
		{
			ix = 0;
			iy = 0;
		}
	}
	else
	{
		if(cinfo.x[0] <= config_info.screen_max_x && cinfo.y[0] <= config_info.screen_max_y) 
		{
			ix = cinfo.x[0];
			iy = cinfo.y[0];
		}
		else
		{
			ix = 0;
			iy = 0;
		}
	}
	
	if(config_info.revert_x_flag)
		ix = config_info.screen_max_x - ix;
	
	if(config_info.revert_y_flag)
		iy = config_info.screen_max_y - iy;
//	printk("ix  %d iy %d \n",ix,iy);
	
	
	if(touches == 0)
	{
		if(downflag)
		{
			ts_release();
			downflag = 0;
		}
		return 1;
	}

	memset(event, 0, sizeof(struct ts_event));

	if(ix <= config_info.screen_max_x && iy <= config_info.screen_max_y)
	{	
		event->x = ix;	
		event->y = iy;
		downflag = 1;
	}
	else
	{
		event->x = 0;	
		event->y = 0;
		if(downflag)
		{
			ts_release();
			downflag = 0;
			
		}
		return 1;
	}
	return 0;
}

static void ts_report_singletouch(void)
{
	struct ts_data *data = i2c_get_clientdata(this_client);
	struct ts_event *event = &data->event;
	
	input_report_abs(data->input_dev, ABS_X, event->x);
	input_report_abs(data->input_dev, ABS_Y, event->y);
	input_report_abs(data->input_dev, ABS_PRESSURE, 1);
	input_report_key(data->input_dev, BTN_TOUCH, 1);
	input_sync(data->input_dev);

	return;
}

static void ts_pen_irq_work(struct work_struct *work)
{
	int ret = -1;

	if(i2c_lock_flag != 0)
		goto queue_monitor_work;
	else
		i2c_lock_flag = 1;
	
	ret = ts_read_data();

	if (ret == 0) 
		ts_report_singletouch();
	
	i2c_lock_flag = 0;
	
queue_monitor_work:
	return;
}

static irqreturn_t ts_interrupt(int irq, void *dev_id)
{
	struct ts_data *ts = dev_id;
//	printk("ts_interrupt \r\n");
	queue_work(ts->ts_workqueue, &ts->pen_event_work);
	return IRQ_HANDLED;
}

static int ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ts_data *ts;
	struct input_dev *input_dev;
//	struct device *dev;
//	struct i2c_dev *i2c_dev;
	int err = 0;

	
	printk("====%s begin=====.	\n", __func__);	
	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}


	ts = kzalloc(sizeof(struct ts_data), GFP_KERNEL);
	if (!ts)	{
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}

	this_client = client;
	this_client->addr = client->addr;
	i2c_set_clientdata(client, ts);

	//gsl1686 slave address  0x40
	if(init_gsl168x())	 
	{		
		printk("init_gsl168x fail\n");		
		goto exit_create_singlethread;	
	}	
	
	printk("touch i2c slave address is %x\n",client->addr);
	sema_init(&ts->tp_cs,1);
	INIT_WORK(&ts->pen_event_work, ts_pen_irq_work);
	ts->ts_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
	if (!ts->ts_workqueue)
	{
		err = -ESRCH;
		goto exit_create_singlethread;
	}

	input_dev = input_allocate_device();
	if (!input_dev)
	{
		err = -ENOMEM;
		printk("failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}
	
	ts->input_dev = input_dev;

	set_bit(ABS_X, input_dev->absbit);
	set_bit(ABS_Y, input_dev->absbit);
	set_bit(ABS_PRESSURE, input_dev->absbit);
	set_bit(BTN_TOUCH, input_dev->keybit);
	input_set_abs_params(input_dev, ABS_X, 0, config_info.screen_max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, config_info.screen_max_y, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_PRESSURE, 0, PRESS_MAX, 0 , 0);
	
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);
	
	input_dev->name		= CTP_NAME;
	err = input_register_device(input_dev);
	if (err) 
	{
		printk("ts_probe: failed to register input device: %s\n",dev_name(&client->dev));
		goto exit_input_register_device_failed;
	}
	config_info.dev = &(ts->input_dev->dev);
	err = input_request_int(&(config_info.input_type), ts_interrupt, CTP_IRQ_MODE, ts);
	if (err) 
	{
	  printk("touch screen gsl1686 request irq failed\n");
	   goto exit_irq_request_failed;
	}
	
	printk("ts_probe gsl1686 touchscreen end\n");
	return 0;

exit_irq_request_failed:
exit_input_register_device_failed:
	input_free_device(input_dev);
exit_input_dev_alloc_failed:
	cancel_work_sync(&ts->pen_event_work);
	destroy_workqueue(ts->ts_workqueue);
exit_create_singlethread:
	i2c_set_clientdata(client, NULL);
	kfree(ts);
exit_alloc_data_failed:
exit_check_functionality_failed:
	return err;
}

static int ts_remove(struct i2c_client *client)
{

	struct ts_data *ts = i2c_get_clientdata(client);
	
	printk("==ts_remove=\n");
	input_free_int(&(config_info.input_type),ts);

	input_unregister_device(ts->input_dev);
	input_free_device(ts->input_dev);
	cancel_work_sync(&ts->pen_event_work);
	destroy_workqueue(ts->ts_workqueue);
	kfree(ts);
    
	i2c_set_clientdata(client, NULL);
	
	input_sensor_free(&(config_info.input_type));
	
	return 0;

}

static const struct i2c_device_id ts_id[] = 
{
	{ CTP_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, ts_id);

static struct i2c_driver ts_driver = 
{
	.class = I2C_CLASS_HWMON,
	.probe		= ts_probe,
	.remove		= ts_remove,
	.id_table	= ts_id,
	.driver	= 
    {
		.name	= CTP_NAME,
		.owner	= THIS_MODULE,
	},
	.address_list	= normal_i2c,
};

static int __init ts_init(void)
{ 
	int ret = -1;
	
	if (input_sensor_startup(&config_info.input_type)) 
	{
		printk("%s: ctp_fetch_sysconfig_para err.\n", __func__);
		return -1;
	} 
	else
	{
		ret = input_sensor_init(&config_info.input_type);
		if (0 != ret) 
		{
			printk("%s:ctp_ops.init_platform_resource err. \n", __func__);	  
			return ret;
		}
	}

	if(config_info.ctp_used == 0)
	{
        printk("*** ctp_used set to 0 !\n");
        printk("*** if use ctp,please put the sys_config.fex ctp_used set to 1. \n");
        return 0;
	}

    if(ctp_get_system_config())
	{
        printk("%s:read config fail!\n",__func__);
        return ret;
    }

	ctp_wakeup(0, 20);
	
	ts_driver.detect = ctp_detect;

	ret = i2c_add_driver(&ts_driver);
	printk("i2c_add_driver %d\r\n", ret);
	return ret;
}

static void __exit ts_exit(void)
{
	printk("==ts_exit==\n");
	i2c_del_driver(&ts_driver);
}

module_init(ts_init);
module_exit(ts_exit);
MODULE_AUTHOR("hp");
MODULE_DESCRIPTION("gsl1686 TouchScreen driver");
MODULE_LICENSE("GPL");

