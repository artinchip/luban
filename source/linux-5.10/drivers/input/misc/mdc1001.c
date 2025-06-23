// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2024 ArtInChip Technology Co., Ltd.
 */
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/jiffies.h>
#include <linux/interrupt.h>
#include <linux/of_device.h>
#include <linux/delay.h>

#define MDC1001_DRIVER_NAME	"mdc1001_input"
#define MDC1001_REG_LENGTH	1
#define MDC1001_CMD_LENGTH	2
#define MDC1001_HIGH_ID		0x58
#define MDC1001_LOW_ID		0x5B
#define VALUE_OF_ADDR048	0x2F
#define MDC1001_MAX_VAL		2047
/* MDC1001 REG */
#define MDC1001_PID1		0x00
#define MDC1001_PID2		0x01
#define MDC1001_MOT_STATUS	0x02
#define MDC1001_DELTA_X		0x03
#define MDC1001_DELTA_Y		0x04
#define MDC1001_OP_MODE		0x05
#define MDC1001_CONFIG1		0x06
#define MDC1001_WP		0x09
#define MDC1001_SLEEP_CFG	0x0A
#define MDC1001_HIBER_CFG	0x0B
#define MDC1001_RES_Y		0x0D
#define MDC1001_RES_X		0x0E
#define MDC1001_DELTA_XY_HI	0x12
#define MDC1001_IQA		0x13
#define MDC1001_SHUTTER		0x14
#define MDC1001_FRAME_AVG	0x17
#define MDC1001_CONFIG2		0x19
#define MDC1001_HARDWARE_ID	0x1B
#define MDC1001_LDP_CTRL	0x1F
#define MDC1001_BANK_SW		0x3C
#define MDC1001_COMM_MODE	0x3E
#define MDC1001_LDP		0x48
#define MDC1001_SENSOR_COLUMN	0x60
#define MDC1001_COL_CNT		0x68
#define MDC1001_READ_PROJECT	0x6D
#define MDC1001_2A3_MOTION	0x73
#define MDC1001_TRB_OSCH	0x7D

struct mdc1001_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
};

static int mdc1001_read(struct i2c_client *client,
			const char *command, char *buf, int length, u32 wait_time)
{
	int ret;
	/* lock i2c read transfer source*/
	struct mutex i2c_read_lock;

	mutex_init(&i2c_read_lock);
	mutex_lock(&i2c_read_lock);

	ret = i2c_master_send(client, command, MDC1001_REG_LENGTH);
	if (ret != MDC1001_REG_LENGTH) {
		ret = ret < 0 ? ret : -EIO;
		dev_err(&client->dev, "failed to write register: error %d\n", ret);
		goto out;
	}

	if (wait_time)
		usleep_range(wait_time, wait_time + 1000);

	ret = i2c_master_recv(client, buf, length);
	if (ret != length) {
		ret = ret < 0 ? ret : -EIO;
		dev_err(&client->dev, "failed to read register: error %d\n", ret);
		goto out;
	}

	ret = 0;
out:
	mutex_unlock(&i2c_read_lock);
	return ret;
}

static int mdc1001_write(struct i2c_client *client, const char *command)
{
	int ret;
	/* lock i2c write transfer source*/
	struct mutex i2c_write_lock;

	mutex_init(&i2c_write_lock);
	mutex_lock(&i2c_write_lock);

	ret = i2c_master_send(client, command, MDC1001_CMD_LENGTH);
	if (ret != MDC1001_CMD_LENGTH) {
		ret = ret < 0 ? ret : -EIO;
		dev_err(&client->dev, "failed to write register: error %d\n", ret);
	}

	mutex_unlock(&i2c_write_lock);
	return ret;
}

static void mdc1001_poll(struct input_dev *input)
{
	int ret = 0;
	s16 delta_x, dx_high;
	u8 cmd, mot_status = 0x00;
	s8 low_buf[2], high_buf[2];
	struct i2c_client *client = input_get_drvdata(input);

	if (!client)
		dev_err(&client->dev, "poll client is null\n");

	cmd = MDC1001_MOT_STATUS;
	ret = mdc1001_read(client, &cmd, &mot_status, 1, 0);
	if (ret < 0)
		return;

	if ((mot_status & 0x80) != 0x80)
		return;

	cmd = MDC1001_DELTA_X;
	ret = mdc1001_read(client, &cmd, low_buf, sizeof(low_buf), 0);
	if (ret < 0)
		return;

	cmd = MDC1001_DELTA_XY_HI;
	ret = mdc1001_read(client, &cmd, high_buf, sizeof(high_buf), 0);
	if (ret < 0)
		return;

	dx_high = (high_buf[0] << 4) & 0x0f00;
	if (dx_high & 0x0800)
		dx_high |= 0xf000;

	delta_x = dx_high | (int16_t)low_buf[0];
	delta_x = (delta_x == MDC1001_MAX_VAL + 1) ? 0 : delta_x;

	if (delta_x == MDC1001_MAX_VAL) {
		dev_err(&client->dev, "dx/dy overflowed!\n");
		cmd = MDC1001_MOT_STATUS;
		mdc1001_read(client, &cmd, low_buf, 1, 0);
	}

	input_report_rel(input, REL_X, delta_x);

	input_sync(input);
}

static int mdc1001_hw_init(struct i2c_client *client)
{
	int ret = 0, read_id = 0;
	u8 buf[2] = {0}, cmd;

	buf[0] = MDC1001_WP;
	buf[1] = 0x5A;
	mdc1001_write(client, buf);

	buf[0] = MDC1001_BANK_SW;
	buf[1] = 0xAC;
	mdc1001_write(client, buf);

	buf[0] = MDC1001_WP;
	buf[1] = 0xA5;
	mdc1001_write(client, buf);

	buf[0] = MDC1001_READ_PROJECT;
	buf[1] = 0x39;
	mdc1001_write(client, buf);

	cmd = MDC1001_PID1;
	mdc1001_read(client, &cmd, buf, 2, 0);

	if (buf[0] == MDC1001_HIGH_ID && buf[1] == MDC1001_LOW_ID)
		read_id = 1;

	if (read_id) {
		buf[0] = MDC1001_CONFIG2;
		buf[1] = 0x04;
		mdc1001_write(client, buf);
		buf[0] = MDC1001_COMM_MODE;
		buf[1] = 0x73;
		mdc1001_write(client, buf);
		buf[0] = MDC1001_LDP;
		buf[1] = VALUE_OF_ADDR048;
		mdc1001_write(client, buf);
		buf[0] = MDC1001_LDP_CTRL;
		buf[1] = 0xFE;
		mdc1001_write(client, buf);
		buf[0] = MDC1001_2A3_MOTION;
		buf[1] = 0x08;
		mdc1001_write(client, buf);
		buf[0] = MDC1001_COL_CNT;
		buf[1] = 0x67;
		mdc1001_write(client, buf);
		buf[0] = MDC1001_TRB_OSCH;
		buf[1] = 0x60;
		mdc1001_write(client, buf);
		buf[0] = MDC1001_SENSOR_COLUMN;
		buf[1] = 0x00;
		mdc1001_write(client, buf);
		buf[0] = MDC1001_RES_Y;
		buf[1] = 0x20;
		mdc1001_write(client, buf);
		buf[0] = MDC1001_RES_X;
		buf[1] = 0x20;
		mdc1001_write(client, buf);
		buf[0] = MDC1001_CONFIG2;
		buf[1] = 0x1C;
		mdc1001_write(client, buf);
	} else {
		dev_err(&client->dev, "mdc1001 init failed!\r\n");
		ret = -1;
	}

	return ret;
}

static int mdc1001_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int error;
	u8 buf[2] = {0}, cmd;
	struct mdc1001_data *data;
	struct i2c_adapter *adap = client->adapter;

	if (!i2c_check_functionality(adap, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2C not support this device!\n");
		return -ENODEV;
	}

	cmd = MDC1001_HARDWARE_ID;
	mdc1001_read(client, &cmd, buf, 1, 0);

	switch (buf[0]) {
	case 0xA1:
	case 0xB1:
	case 0xC1:
		mdc1001_hw_init(client);
		break;
	default:
		dev_err(&client->dev, "ots sensor not support\n");
		break;
	}

	data = devm_kzalloc(&client->dev, sizeof(struct mdc1001_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->client = client;

	data->input_dev = devm_input_allocate_device(&data->client->dev);
	if (!data->input_dev) {
		error = -ENOMEM;
		goto err_free_data;
	}

	data->input_dev->name = MDC1001_DRIVER_NAME;
	data->input_dev->id.bustype = BUS_I2C;
	data->input_dev->dev.parent = &client->dev;

	input_set_capability(data->input_dev, EV_REL, REL_X);
	i2c_set_clientdata(client, data);
	input_set_drvdata(data->input_dev, client);

	error = input_setup_polling(data->input_dev, mdc1001_poll);
	if (error) {
		dev_err(&client->dev, "failed to set up polling\n");
		goto err_free_input_dev;
	}

	/* Set the polling inerval to 15ms */
	input_set_poll_interval(data->input_dev, 15);
	/* Set the max polling inerval to 100ms */
	input_set_max_poll_interval(data->input_dev, 100);

	error = input_register_device(data->input_dev);
	if (error) {
		dev_err(&client->dev, "failed to register mdc1001\n");
		goto err_free_input_dev;
	}

	return 0;

err_free_input_dev:
	input_free_device(data->input_dev);
err_free_data:
	devm_kfree(&client->dev, data);
	return error;
}

static int mdc1001_remove(struct i2c_client *client)
{
	struct mdc1001_data *data = i2c_get_clientdata(client);

	input_unregister_device(data->input_dev);

	return 0;
}

static const struct i2c_device_id mdc1001_id[] = {
	{ MDC1001_DRIVER_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mdc1001_id);

static const struct of_device_id mdc1001_dt_ids[] = {
	{ .compatible = "mixo,mdc1001", },
	{ }
};
MODULE_DEVICE_TABLE(of, mdc1001_dt_ids);

static struct i2c_driver mdc1001_driver = {
	.driver = {
		.name = MDC1001_DRIVER_NAME,
		.of_match_table = mdc1001_dt_ids,
	},
	.probe = mdc1001_probe,
	.remove = mdc1001_remove,
	.id_table = mdc1001_id,
};
module_i2c_driver(mdc1001_driver);

MODULE_AUTHOR("hjh <jiehua.huang@artinchip.com>");
MODULE_DESCRIPTION("MDC1001 Optical Tracking Sensor Driver");
MODULE_LICENSE("GPL");
