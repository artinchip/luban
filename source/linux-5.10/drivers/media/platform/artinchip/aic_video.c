// SPDX-License-Identifier: GPL-2.0-only
/*
 * The video-device part of ArtInChip DVP controller driver.
 *
 * Copyright (C) 2020-2022 ArtInChip Technology Co., Ltd.
 * Authors:  Matteo <duanmt@artinchip.com>
 */

#include <linux/device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/clk.h>

#include <media/v4l2-ioctl.h>
#include <media/v4l2-mc.h>
#include <media/videobuf2-v4l2.h>

#include "aic_dvp.h"

#define DVP_DEFAULT_WIDTH	640
#define DVP_DEFAULT_HEIGHT	480

static const struct {
	u32 mbus;
	enum dvp_input_yuv_seq dvp;
} aic_dvp_in_fmt[] = {
	{MEDIA_BUS_FMT_YUYV8_2X8, DVP_YUV_DATA_SEQ_YUYV},
	{MEDIA_BUS_FMT_YVYU8_2X8, DVP_YUV_DATA_SEQ_YVYU},
	{MEDIA_BUS_FMT_UYVY8_2X8, DVP_YUV_DATA_SEQ_UYVY},
	{MEDIA_BUS_FMT_VYUY8_2X8, DVP_YUV_DATA_SEQ_VYUY},
};

static const struct {
	u32 pixelformat;
	enum dvp_output dvp;
} aic_dvp_out_fmt[] = {
	{V4L2_PIX_FMT_NV16, DVP_OUT_YUV422_COMBINED_NV16},
	{V4L2_PIX_FMT_NV12, DVP_OUT_YUV420_COMBINED_NV12},
	// TODO: Add RAW_PASSTHROUGH
};

static int aic_dvp_out_fmt_valid(u32 pixelformat)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(aic_dvp_out_fmt); i++) {
		if (aic_dvp_out_fmt[i].pixelformat == pixelformat)
			return aic_dvp_out_fmt[i].dvp;
	}
	pr_err("%s() - Invalid pixelformat: %#x\n", __func__, pixelformat);
	return -1;
}

static int aic_dvp_in_fmt_valid(u32 mbus)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(aic_dvp_in_fmt); i++) {
		if (aic_dvp_in_fmt[i].mbus == mbus)
			return aic_dvp_in_fmt[i].dvp;
	}

	pr_err("%s() - Invalid mbus: %#x\n", __func__, mbus);
	return -1;
}

static int aic_dvp_querycap(struct file *file, void *priv,
			      struct v4l2_capability *cap)
{
	struct aic_dvp *dvp = video_drvdata(file);

	strscpy(cap->driver, KBUILD_MODNAME, sizeof(cap->driver));
	strscpy(cap->card, AIC_DVP_NAME, sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s",
		 dev_name(dvp->dev));

	return 0;
}

static int aic_dvp_enum_input(struct file *file, void *priv,
				struct v4l2_input *inp)
{
	if (inp->index != 0)
		return -EINVAL;

	inp->type = V4L2_INPUT_TYPE_CAMERA;
	strscpy(inp->name, "Camera", sizeof(inp->name));

	return 0;
}

static int aic_dvp_g_input(struct file *file, void *fh,
			     unsigned int *i)
{
	*i = 0;

	return 0;
}

static int aic_dvp_s_input(struct file *file, void *fh,
			     unsigned int i)
{
	if (i != 0)
		return -EINVAL;

	return 0;
}

static void _aic_dvp_try_fmt(struct aic_dvp *dvp,
			       struct v4l2_pix_format_mplane *pix)
{
	int ret;
	unsigned int i;

	ret = aic_dvp_out_fmt_valid(pix->pixelformat);
	if (ret < 0)
		pix->pixelformat = aic_dvp_out_fmt[0].pixelformat;

	pix->field = V4L2_FIELD_NONE;
	pix->colorspace = V4L2_COLORSPACE_SRGB;
	pix->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(pix->colorspace);
	pix->ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(pix->colorspace);
	pix->quantization = V4L2_MAP_QUANTIZATION_DEFAULT(true,
				pix->colorspace, pix->ycbcr_enc);
	pix->num_planes = DVP_MAX_PLANE;

	memset(pix->reserved, 0, sizeof(pix->reserved));

	for (i = 0; i < DVP_MAX_PLANE; i++) {
		pix->plane_fmt[i].bytesperline = ALIGN(pix->width, 8);
		pix->plane_fmt[i].sizeimage =
			ALIGN(pix->plane_fmt[i].bytesperline * pix->height, 8);
		if ((ret == DVP_OUT_YUV420_COMBINED_NV12) && (i > 0))
			pix->plane_fmt[i].sizeimage >>= 1;

		memset(pix->plane_fmt[i].reserved, 0,
		       sizeof(pix->plane_fmt[i].reserved));
	}
}

static int aic_dvp_try_fmt_vid_cap(struct file *file, void *priv,
				     struct v4l2_format *f)
{
	struct aic_dvp *dvp = video_drvdata(file);

	_aic_dvp_try_fmt(dvp, &f->fmt.pix_mp);

	return 0;
}

static int aic_dvp_s_fmt_vid_cap(struct file *file, void *priv,
				   struct v4l2_format *f)
{
	int i;
	struct aic_dvp *dvp = video_drvdata(file);

	_aic_dvp_try_fmt(dvp, &f->fmt.pix_mp);
	dvp->fmt = f->fmt.pix_mp;

	/* Save the configuration for DVP controller */
	dvp->cfg.output = aic_dvp_out_fmt_valid(dvp->fmt.pixelformat);
	dvp->cfg.width = dvp->fmt.width;
	dvp->cfg.height = dvp->fmt.height;
	for (i = 0; i < DVP_MAX_PLANE; i++) {
		dvp->cfg.stride[i] = dvp->fmt.plane_fmt[i].bytesperline;
		dvp->cfg.sizeimage[i] = dvp->fmt.plane_fmt[i].sizeimage;
	}

	return 0;
}

static int aic_dvp_g_fmt_vid_cap(struct file *file, void *priv,
				   struct v4l2_format *f)
{
	struct aic_dvp *dvp = video_drvdata(file);

	f->fmt.pix_mp = dvp->fmt;

	return 0;
}

static int aic_dvp_enum_fmt_vid_cap(struct file *file, void *priv,
				      struct v4l2_fmtdesc *f)
{
	if (f->index >= ARRAY_SIZE(aic_dvp_out_fmt))
		return -EINVAL;

	f->pixelformat = aic_dvp_out_fmt[f->index].pixelformat;

	return 0;
}

static const struct v4l2_ioctl_ops aic_dvp_ioctl_ops = {
	.vidioc_querycap		= aic_dvp_querycap,

	.vidioc_enum_fmt_vid_cap	= aic_dvp_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap_mplane	= aic_dvp_g_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap_mplane	= aic_dvp_s_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap_mplane	= aic_dvp_try_fmt_vid_cap,

	.vidioc_enum_input		= aic_dvp_enum_input,
	.vidioc_g_input			= aic_dvp_g_input,
	.vidioc_s_input			= aic_dvp_s_input,

	.vidioc_reqbufs			= vb2_ioctl_reqbufs,
	.vidioc_create_bufs		= vb2_ioctl_create_bufs,
	.vidioc_querybuf		= vb2_ioctl_querybuf,
	.vidioc_qbuf			= vb2_ioctl_qbuf,
	.vidioc_dqbuf			= vb2_ioctl_dqbuf,
	.vidioc_expbuf			= vb2_ioctl_expbuf,
	.vidioc_prepare_buf		= vb2_ioctl_prepare_buf,
	.vidioc_streamon		= vb2_ioctl_streamon,
	.vidioc_streamoff		= vb2_ioctl_streamoff,
};

static int aic_dvp_open(struct file *file)
{
	struct aic_dvp *dvp = video_drvdata(file);
	int ret;

	ret = mutex_lock_interruptible(&dvp->lock);
	if (ret)
		return ret;

	ret = pm_runtime_get_sync(dvp->dev);
	if (ret < 0)
		goto err_pm_put;

	ret = v4l2_pipeline_pm_get(&dvp->vdev.entity);
	if (ret)
		goto err_pm_put;

	ret = v4l2_fh_open(file);
	if (ret)
		goto err_pipeline_pm_put;

	clk_set_rate(dvp->clk, dvp->clk_rate);
	clk_prepare_enable(dvp->clk);
	reset_control_deassert(dvp->rst);
	aic_dvp_enable(dvp, 1);

	mutex_unlock(&dvp->lock);

	return 0;

err_pipeline_pm_put:
	v4l2_pipeline_pm_put(&dvp->vdev.entity);

err_pm_put:
	pm_runtime_put(dvp->dev);
	mutex_unlock(&dvp->lock);

	return ret;
}

static int aic_dvp_release(struct file *file)
{
	struct aic_dvp *dvp = video_drvdata(file);

	mutex_lock(&dvp->lock);

	aic_dvp_enable(dvp, 0);
	reset_control_assert(dvp->rst);
	clk_disable_unprepare(dvp->clk);

	_vb2_fop_release(file, NULL);
	v4l2_pipeline_pm_put(&dvp->vdev.entity);
	pm_runtime_put(dvp->dev);

	mutex_unlock(&dvp->lock);

	return 0;
}

static const struct v4l2_file_operations aic_dvp_fops = {
	.owner		= THIS_MODULE,
	.open		= aic_dvp_open,
	.release	= aic_dvp_release,
	.unlocked_ioctl	= video_ioctl2,
	.read		= vb2_fop_read,
	.write		= vb2_fop_write,
	.poll		= vb2_fop_poll,
	.mmap		= vb2_fop_mmap,
};

static const struct v4l2_mbus_framefmt aic_dvp_pad_fmt_default = {
	.width = DVP_DEFAULT_WIDTH,
	.height = DVP_DEFAULT_HEIGHT,
	.code = MEDIA_BUS_FMT_YUYV8_2X8,
	.field = V4L2_FIELD_NONE,
	.colorspace = V4L2_COLORSPACE_RAW,
	.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT,
	.quantization = V4L2_QUANTIZATION_DEFAULT,
	.xfer_func = V4L2_XFER_FUNC_DEFAULT,
};

static int aic_dvp_subdev_init_cfg(struct v4l2_subdev *subdev,
				     struct v4l2_subdev_pad_config *cfg)
{
	struct v4l2_mbus_framefmt *fmt;

	fmt = v4l2_subdev_get_try_format(subdev, cfg, DVP_SUBDEV_SINK);
	*fmt = aic_dvp_pad_fmt_default;

	return 0;
}

static int aic_dvp_subdev_get_fmt(struct v4l2_subdev *subdev,
				    struct v4l2_subdev_pad_config *cfg,
				    struct v4l2_subdev_format *fmt)
{
	struct aic_dvp *dvp = container_of(subdev, struct aic_dvp, subdev);
	struct v4l2_mbus_framefmt *subdev_fmt;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		subdev_fmt = v4l2_subdev_get_try_format(subdev, cfg, fmt->pad);
	else
		subdev_fmt = &dvp->subdev_fmt;

	fmt->format = *subdev_fmt;
	return 0;
}

static int aic_dvp_subdev_set_fmt(struct v4l2_subdev *subdev,
				    struct v4l2_subdev_pad_config *cfg,
				    struct v4l2_subdev_format *fmt)
{
	struct aic_dvp *dvp = container_of(subdev, struct aic_dvp, subdev);
	struct v4l2_mbus_framefmt *subdev_fmt;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		subdev_fmt = v4l2_subdev_get_try_format(subdev, cfg, fmt->pad);
	else
		subdev_fmt = &dvp->subdev_fmt;

	/* We can only set the format on the sink pad */
	if (fmt->pad == DVP_SUBDEV_SINK) {
		if (aic_dvp_in_fmt_valid(fmt->format.code) < 0)
			return -EINVAL;

		/* It's the sink, only allow changing the frame size */
		subdev_fmt->width = fmt->format.width;
		subdev_fmt->height = fmt->format.height;
		subdev_fmt->code = fmt->format.code;

		dvp->cfg.input_seq = aic_dvp_in_fmt_valid(fmt->format.code);
	}

	fmt->format = *subdev_fmt;

	return 0;
}

static int
aic_dvp_subdev_enum_mbus_code(struct v4l2_subdev *subdev,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_mbus_code_enum *mbus)
{
	if (mbus->index >= ARRAY_SIZE(aic_dvp_in_fmt))
		return -EINVAL;

	mbus->code = aic_dvp_in_fmt[mbus->index].mbus;

	return 0;
}

static const struct v4l2_subdev_pad_ops aic_dvp_subdev_pad_ops = {
	.link_validate	= v4l2_subdev_link_validate_default,
	.init_cfg	= aic_dvp_subdev_init_cfg,
	.get_fmt	= aic_dvp_subdev_get_fmt,
	.set_fmt	= aic_dvp_subdev_set_fmt,
	.enum_mbus_code	= aic_dvp_subdev_enum_mbus_code,
};

const struct v4l2_subdev_ops aic_dvp_subdev_ops = {
	.pad = &aic_dvp_subdev_pad_ops,
};

int aic_dvp_video_register(struct aic_dvp *dvp)
{
	struct video_device *vdev = &dvp->vdev;
	int ret;

	vdev->device_caps = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING;
	vdev->v4l2_dev = &dvp->v4l2;
	vdev->queue = &dvp->queue;
	strscpy(vdev->name, KBUILD_MODNAME, sizeof(vdev->name));
	vdev->release = video_device_release_empty;
	vdev->lock = &dvp->lock;

	/* Set a default format */
	dvp->fmt.pixelformat = aic_dvp_out_fmt[0].pixelformat;
	dvp->fmt.width = DVP_DEFAULT_WIDTH;
	dvp->fmt.height = DVP_DEFAULT_HEIGHT;
	_aic_dvp_try_fmt(dvp, &dvp->fmt);
	dvp->subdev_fmt = aic_dvp_pad_fmt_default;

	vdev->fops = &aic_dvp_fops;
	vdev->ioctl_ops = &aic_dvp_ioctl_ops;
	video_set_drvdata(vdev, dvp);

	ret = video_register_device(&dvp->vdev, VFL_TYPE_VIDEO, -1);
	if (ret)
		return ret;

	dev_info(dvp->dev, "Device registered as %s\n",
		 video_device_node_name(vdev));

	return 0;
}
