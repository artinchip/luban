// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 ArtInChip Technology Co., Ltd.
 * Authors:  Matteo <duanmt@artinchip.com>
 */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/io.h>
//#include <asm/system.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/string.h>
#include <linux/list.h>
#include <asm/delay.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>

#define DEFAULT_WIDTH		720
#define DEFAULT_HEIGHT		576
#define DEFAULT_FRAMERATE	25

#define DEFAULT_VIN_CH		0
#define DEFAULT_V4L2_CODE	MEDIA_BUS_FMT_UYVY8_2X8

#define DRV_NAME		"gm7150"

/* Default format configuration of GM7150 */
#define GM7150_DFT_WIDTH        PAL_WIDTH
#define GM7150_DFT_HEIGHT       PAL_HEIGHT
#define GM7150_DFT_BUS_TYPE     MEDIA_BUS_BT656
#define GM7150_DFT_CODE         MEDIA_BUS_FMT_UYVY8_2X8

#define GM7150_CHIP_ID          0x7150

static int test;

struct gm7150_v4l2_dev {
	struct i2c_client *i2c;
	struct gpio_desc *pwdn_gpio;

	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_fwnode_endpoint ep; /* the parsed DT endpoint info */
	struct v4l2_mbus_framefmt fmt;
	struct v4l2_fract frame_interval;

	int cur_fr;

	/* lock to protect all members below */
	struct mutex lock;
	bool streaming;
};

void gm7150_write_reg(struct i2c_client *client,
		      u8 reg_addr,
		      u8 value)
{
	unsigned char buf[2];

	buf[0] = reg_addr;
	buf[1] = value;

	i2c_master_send(client, buf, 2);
}

u8 gm7150_read_reg(struct i2c_client *client, u8 reg_addr, u8 *value)
{
	static struct i2c_msg msg[2] = {{0}};
	unsigned char         buffer[2] = {0};
	int                   ret = 0;

	buffer[0] = reg_addr & 0xFF;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = buffer;

	msg[1].addr = client->addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = buffer;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret != 2) {
		pr_err("%s() %d - i2c_transfer error, ret=%d.\n", __func__, __LINE__, ret);
		return -1;
	}
	*value = buffer[0];

	return 0;
}

static void gm7150_power_on(struct gm7150_v4l2_dev *sensor)
{
	if (IS_ERR_OR_NULL(sensor->pwdn_gpio))
		return;

	msleep(30);
	gpiod_set_value_cansleep(sensor->pwdn_gpio, 0);
	msleep(10);
	gpiod_set_value_cansleep(sensor->pwdn_gpio, 1);
	msleep(30);
}

static void gm7150_select_ch(struct gm7150_v4l2_dev *sensor, u32 ch)
{
	if (ch)
		gm7150_write_reg(sensor->i2c, 0x00, 0x02);
	else
		gm7150_write_reg(sensor->i2c, 0x00, 0x00);
}

static void gm7150_out_bt656(struct gm7150_v4l2_dev *sensor)
{
	gm7150_write_reg(sensor->i2c, 0x03, 0x0D);
	gm7150_write_reg(sensor->i2c, 0x11, 0x04);
	gm7150_write_reg(sensor->i2c, 0x12, 0x00);
	gm7150_write_reg(sensor->i2c, 0x13, 0x04);
	gm7150_write_reg(sensor->i2c, 0x14, 0x00);
	gm7150_write_reg(sensor->i2c, 0xA0, 0x55);
	gm7150_write_reg(sensor->i2c, 0xA1, 0xAA);
	gm7150_write_reg(sensor->i2c, 0x69, 0x40);
	gm7150_write_reg(sensor->i2c, 0x6D, 0x90);
}

static void gm7150_cur_status(struct gm7150_v4l2_dev *sensor)
{
	u8 val = 0, fmt = 0;
	char *formats[] = {"Reserved", "NTSC BT.601", "Reserved", "PAL BT.601",
			   "Reserved", "(M)PAL BT.601", "Reserved", "PAL-N BT.601",
			   "Reserved", "NTSC 4.43 BT.601", "Reserved", "Reserved",
			   "Reserved", "Reserved", "Reserved", "Reserved"};

	gm7150_read_reg(sensor->i2c, 0x88, &val);
	dev_info(&sensor->i2c->dev, "Reg 0x%02x: 0x%02x. Input signal is %s\n",
		 0x88, val, (val & 0x6) == 0x6 ? "valid" : "invalid");

	gm7150_read_reg(sensor->i2c, 0x8C, &val);
	fmt = val & 0xF;
	dev_info(&sensor->i2c->dev, "Reg 0x%02x: 0x%02x. Input format: %s\n",
		 0x8C, val, formats[fmt]);

	if (fmt == 0x1 || fmt == 9)
		sensor->fmt.height = 480; // NTSC_HEIGHT;
}

static int __init gm7150_module_init(struct gm7150_v4l2_dev *sensor)
{
	u8 id_h = 0, id_l = 0;

	if (gm7150_read_reg(sensor->i2c, 0x80, &id_h) ||
	    gm7150_read_reg(sensor->i2c, 0x81, &id_l))
		return -1;

	if ((id_h << 8 | id_l) != GM7150_CHIP_ID) {
		dev_err(&sensor->i2c->dev, "Invalid chip ID: %02x %02x\n", id_h, id_l);
		return -1;
	}
	gm7150_select_ch(sensor, DEFAULT_VIN_CH);
	gm7150_out_bt656(sensor);
	gm7150_cur_status(sensor);

	dev_info(&sensor->i2c->dev, "GM7150 Driver Loaded!\n");
	return 0;
}

/******************************************************************************
 * V4L2 API of GM7150
 ******************************************************************************/

static inline struct gm7150_v4l2_dev *to_gm7150_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct gm7150_v4l2_dev, sd);
}

static int gm7150_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct gm7150_v4l2_dev *sensor = to_gm7150_dev(sd);

	fi->interval = sensor->frame_interval;
	return 0;
}

static int gm7150_s_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct gm7150_v4l2_dev *sensor = to_gm7150_dev(sd);

	if (fi->pad != 0)
		return -EINVAL;

	if (sensor->streaming)
		return -EBUSY;

	dev_dbg(&sensor->i2c->dev, "Set FR %d-%d\n",
		fi->interval.numerator, fi->interval.denominator);
	// sensor->frame_interval = fi->interval;
	return 0;
}

static int gm7150_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad != 0)
		return -EINVAL;

	code->code = DEFAULT_V4L2_CODE;
	return 0;
}

static int gm7150_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct gm7150_v4l2_dev *sensor = to_gm7150_dev(sd);

	dev_dbg(&sensor->i2c->dev, "Streaming %s\n", enable ? "On" : "Off");
	sensor->streaming = enable;

	if (test) {
		if (enable) {
			dev_info(&sensor->i2c->dev, "Enter test mode\n");
			gm7150_write_reg(0, 0x2A, 0x3C);
		} else {
			dev_info(&sensor->i2c->dev, "Exit test mode\n");
			gm7150_write_reg(0, 0x2A, 0x30);
		}
	}

	return 0;
}

static int gm7150_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *format)
{
	struct gm7150_v4l2_dev *sensor = to_gm7150_dev(sd);

	if (format->pad != 0)
		return -EINVAL;

	format->format = sensor->fmt;
	return 0;
}

static int gm7150_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *format)
{
	struct gm7150_v4l2_dev *sensor = to_gm7150_dev(sd);
	struct v4l2_mbus_framefmt *fmt = &format->format;

	if (format->pad != 0)
		return -EINVAL;

	if (sensor->streaming)
		return -EBUSY;

	dev_dbg(&sensor->i2c->dev,
		"Set format: code %#x, colorspace %#x, %d x %d\n",
		fmt->code, fmt->colorspace, fmt->width, fmt->height);
	return 0;
}

static const struct v4l2_subdev_core_ops gm7150_core_ops = {
	.log_status = v4l2_ctrl_subdev_log_status,
	.subscribe_event = v4l2_ctrl_subdev_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_video_ops gm7150_video_ops = {
	.g_frame_interval = gm7150_g_frame_interval,
	.s_frame_interval = gm7150_s_frame_interval,
	.s_stream = gm7150_s_stream,
};

static const struct v4l2_subdev_pad_ops gm7150_pad_ops = {
	.enum_mbus_code = gm7150_enum_mbus_code,
	.get_fmt = gm7150_get_fmt,
	.set_fmt = gm7150_set_fmt,
};

static const struct v4l2_subdev_ops gm7150_subdev_ops = {
	.core = &gm7150_core_ops,
	.video = &gm7150_video_ops,
	.pad = &gm7150_pad_ops,
};

static int gm7150_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct fwnode_handle *endpoint = NULL;
	struct gm7150_v4l2_dev *sensor = NULL;
	struct v4l2_mbus_framefmt *fmt = NULL;
	int ret = 0;

	sensor = devm_kzalloc(dev, sizeof(struct gm7150_v4l2_dev), GFP_KERNEL);
	if (!sensor)
		return -ENOMEM;

	/* request optional power down pin */
	sensor->pwdn_gpio = devm_gpiod_get_optional(dev, "powerdown", GPIOD_OUT_HIGH);
	if (IS_ERR_OR_NULL(sensor->pwdn_gpio))
		dev_dbg(dev, "Failed to parse powerdown-gpio\n");
	else
		gm7150_power_on(sensor);

	fmt = &sensor->fmt;
	fmt->code = DEFAULT_V4L2_CODE;
	fmt->colorspace = V4L2_COLORSPACE_SRGB;
	fmt->ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(fmt->colorspace);
	fmt->quantization = V4L2_QUANTIZATION_FULL_RANGE;
	fmt->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(fmt->colorspace);
	fmt->width = DEFAULT_WIDTH;
	fmt->height = DEFAULT_HEIGHT;
	fmt->field = V4L2_FIELD_NONE;
	sensor->frame_interval.numerator = 1;
	sensor->frame_interval.denominator = DEFAULT_FRAMERATE;
	sensor->cur_fr = DEFAULT_FRAMERATE;

	sensor->i2c = client;
	ret = gm7150_module_init(sensor);
	if (ret)
		return -EINVAL;

	endpoint = fwnode_graph_get_next_endpoint(dev_fwnode(&client->dev), NULL);
	if (!endpoint) {
		dev_err(dev, "endpoint node not found\n");
		return -EINVAL;
	}

	ret = v4l2_fwnode_endpoint_parse(endpoint, &sensor->ep);
	fwnode_handle_put(endpoint);
	if (ret) {
		dev_err(dev, "Could not parse endpoint\n");
		return ret;
	}

	if (sensor->ep.bus_type != V4L2_MBUS_BT656) {
		dev_err(dev, "Unsupported bus type %d\n", sensor->ep.bus_type);
		return -EINVAL;
	}

	v4l2_i2c_subdev_init(&sensor->sd, client, &gm7150_subdev_ops);

	sensor->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	sensor->pad.flags = MEDIA_PAD_FL_SOURCE;
	sensor->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sensor->sd.entity, 1, &sensor->pad);
	if (ret)
		return ret;

	ret = v4l2_async_register_subdev_sensor_common(&sensor->sd);
	if (ret)
		return ret;

	dev_info(dev, "Register %s to V4L2 device\n", DRV_NAME);

	return 0;
}

static int gm7150_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gm7150_v4l2_dev *sensor = to_gm7150_dev(sd);

	v4l2_async_unregister_subdev(&sensor->sd);
	media_entity_cleanup(&sensor->sd.entity);

	return 0;
}

static const struct i2c_device_id gm7150_id[] = {
	{DRV_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, gm7150_id);

static const struct of_device_id gm7150_dt_ids[] = {
	{ .compatible = "guoteng,gm7150" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, gm7150_dt_ids);

static struct i2c_driver gm7150_i2c_driver = {
	.driver = {
		.name  = DRV_NAME,
		.of_match_table	= gm7150_dt_ids,
	},
	.id_table = gm7150_id,
	.probe_new = gm7150_probe,
	.remove   = gm7150_remove,
};

module_i2c_driver(gm7150_i2c_driver);

MODULE_DESCRIPTION("GuoTeng GM7150 Linux Module");
MODULE_LICENSE("GPL");
