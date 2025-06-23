// SPDX-License-Identifier: GPL-2.0-only
/*
 * The main part of ArtInChip DVP controller driver.
 *
 * Copyright (C) 2020-2022 ArtInChip Technology Co., Ltd.
 * Authors:  Matteo <duanmt@artinchip.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/videodev2.h>

#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-mediabus.h>

#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>

#include "aic_dvp.h"

static const struct media_entity_operations aic_dvp_video_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static ssize_t buflist_show(struct device *dev,
				 struct device_attribute *devattr,
				 char *buf)
{
	int i;
	struct aic_dvp *dvp = dev_get_drvdata(dev);
	struct vb2_buffer *vb;
	static char *str[] = {"Dequeued", "In_request", "Preparing", "Queued",
			"Active", "Done", "Error"};

	sprintf(buf, "In dvp->buf_list, the current buf in list:\n");
	for (i = 0; i < DVP_MAX_BUF; i++) {
		if (dvp->vbuf[i] == NULL) {
			sprintf(buf, "%s[%d]: Empty\n", buf, i);
			continue;
		}

		sprintf(buf, "%s[%d]: index %d, sequence %d\n", buf,
			i, dvp->vbuf[i]->vb2_buf.index,
			dvp->vbuf[i]->sequence);
	}

	sprintf(buf, "%s\nIn V4L2 Q-buf list:\n", buf);
	if (list_empty(&dvp->queue.queued_list)) {
		sprintf(buf, "%sEmpty\n", buf);
	} else {
		list_for_each_entry(vb, &dvp->queue.queued_list, queued_entry)
			sprintf(buf, "%s[%d], State: %s\n", buf,
				vb->index, str[vb->state]);
	}

	sprintf(buf, "%s\nIn V4L2 DQ-buf list:\n", buf);
	if (list_empty(&dvp->queue.done_list)) {
		sprintf(buf, "%sEmpty\n", buf);
	} else {
		list_for_each_entry(vb, &dvp->queue.done_list, done_entry)
			sprintf(buf, "%s[%d], State: %s\n", buf,
				vb->index, str[vb->state]);
	}

	return strlen(buf);
}
static DEVICE_ATTR_RO(buflist);

static struct attribute *aic_dvp_attr[] = {
	&dev_attr_buflist.attr,
	NULL
};

static int aic_dvp_notify_bound(struct v4l2_async_notifier *notifier,
				  struct v4l2_subdev *subdev,
				  struct v4l2_async_subdev *asd)
{
	struct aic_dvp *dvp = container_of(notifier, struct aic_dvp,
					     notifier);

	dvp->src_subdev = subdev;
	dvp->src_pad = media_entity_get_fwnode_pad(&subdev->entity,
						   subdev->fwnode,
						   MEDIA_PAD_FL_SOURCE);
	if (dvp->src_pad < 0) {
		dev_err(dvp->dev, "Couldn't find output pad for subdev %s\n",
			subdev->name);
		return dvp->src_pad;
	}

	dev_dbg(dvp->dev, "Bound %s pad: %d\n", subdev->name, dvp->src_pad);
	return 0;
}

static int aic_dvp_notify_complete(struct v4l2_async_notifier *notifier)
{
	struct aic_dvp *dvp = container_of(notifier, struct aic_dvp,
					     notifier);
	struct v4l2_subdev *subdev = &dvp->subdev;
	struct video_device *vdev = &dvp->vdev;
	int ret;

	ret = v4l2_device_register_subdev(&dvp->v4l2, subdev);
	if (ret < 0)
		return ret;

	ret = aic_dvp_video_register(dvp);
	if (ret < 0)
		return ret;

	ret = media_device_register(&dvp->mdev);
	if (ret)
		return ret;

	/* Create link from subdev to main device */
	ret = media_create_pad_link(&subdev->entity, DVP_SUBDEV_SOURCE,
				    &vdev->entity, 0,
				    MEDIA_LNK_FL_ENABLED |
				    MEDIA_LNK_FL_IMMUTABLE);
	if (ret)
		goto err_clean_media;

	ret = media_create_pad_link(&dvp->src_subdev->entity, dvp->src_pad,
				    &subdev->entity, DVP_SUBDEV_SINK,
				    MEDIA_LNK_FL_ENABLED |
				    MEDIA_LNK_FL_IMMUTABLE);
	if (ret)
		goto err_clean_media;

	ret = v4l2_device_register_subdev_nodes(&dvp->v4l2);
	if (ret < 0)
		goto err_clean_media;

	return 0;

err_clean_media:
	media_device_unregister(&dvp->mdev);

	return ret;
}

static const struct v4l2_async_notifier_operations aic_dvp_notify_ops = {
	.bound		= aic_dvp_notify_bound,
	.complete	= aic_dvp_notify_complete,
};

static int aic_dvp_bustype2input(int bustype)
{
	if (bustype == V4L2_MBUS_PARALLEL)
		return DVP_IN_YUV422;
	if (bustype == V4L2_MBUS_BT656)
		return DVP_IN_BT656;

	pr_err("%s() - Invalid bustype %d\n", __func__, bustype);
	return DVP_IN_YUV422;
}

static int aic_dvp_notifier_init(struct aic_dvp *dvp)
{
	struct v4l2_fwnode_endpoint vep = {
		.bus_type = V4L2_MBUS_UNKNOWN,
	};
	struct fwnode_handle *ep;
	int ret;

	v4l2_async_notifier_init(&dvp->notifier);

	ep = fwnode_graph_get_endpoint_by_id(dev_fwnode(dvp->dev), 0, 0,
					     FWNODE_GRAPH_ENDPOINT_NEXT);
	if (!ep)
		return -EINVAL;

	ret = v4l2_fwnode_endpoint_parse(ep, &vep);
	if (ret)
		goto out;

	dvp->cfg.field_active = 1;
	fwnode_property_read_u32(ep, "aic,field-active",
				 &dvp->cfg.field_active);
	if (fwnode_property_read_bool(ep, "aic,interlaced"))
		dvp->cfg.field = V4L2_FIELD_INTERLACED;

	dvp->bus = vep.bus.parallel;
	dvp->cfg.input = aic_dvp_bustype2input(vep.bus_type);

	ret = v4l2_async_notifier_add_fwnode_remote_subdev(&dvp->notifier,
							   ep, &dvp->asd);
	if (ret)
		goto out;

	dvp->notifier.ops = &aic_dvp_notify_ops;

out:
	fwnode_handle_put(ep);
	return ret;
}

static int aic_dvp_parse_dt(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct aic_dvp *dvp = platform_get_drvdata(pdev);

	dvp->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(dvp->regs))
		return PTR_ERR(dvp->regs);

	dvp->irq = platform_get_irq(pdev, 0);
	if (dvp->irq < 0)
		return dvp->irq;

	dvp->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(dvp->clk)) {
		dev_err(dev, "Couldn't get DVP clock\n");
		return PTR_ERR(dvp->clk);
	}

	ret = of_property_read_u32(dev->of_node, "clock-rate", &dvp->clk_rate);
	if (ret) {
		dev_err(dev, "Can't parse clock-rate\n");
		return ret;
	}

	dvp->rst = devm_reset_control_get(dev, NULL);
	if (IS_ERR(dvp->rst)) {
		dev_err(dev, "Couldn't get DVP reset\n");
		return PTR_ERR(dvp->rst);
	}

	return 0;
}

static int aic_dvp_sysfs_create(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(aic_dvp_attr); i++) {
		if (!aic_dvp_attr[i])
			break;

		if (sysfs_add_file_to_group(&dev->kobj, aic_dvp_attr[i], NULL)
			< 0) {
			dev_err(dev, "Failed to create %s.\n",
				aic_dvp_attr[i]->name);
			return -1;
		}
	}

	return 0;
}

static int aic_dvp_probe(struct platform_device *pdev)
{
	struct v4l2_subdev *subdev;
	struct video_device *vdev;
	struct aic_dvp *dvp;
	int ret;

	dvp = devm_kzalloc(&pdev->dev, sizeof(*dvp), GFP_KERNEL);
	if (!dvp)
		return -ENOMEM;

	platform_set_drvdata(pdev, dvp);
	ret = aic_dvp_parse_dt(pdev);
	if (ret < 0)
		return ret;

	dvp->dev = &pdev->dev;
	dvp->ch = 0;
	subdev = &dvp->subdev;
	vdev = &dvp->vdev;

	dvp->mdev.dev = dvp->dev;
	strscpy(dvp->mdev.model, "ArtInChip DVP", sizeof(dvp->mdev.model));
	dvp->mdev.hw_revision = 1;
	media_device_init(&dvp->mdev);
	dvp->v4l2.mdev = &dvp->mdev;

	/* Initialize subdev */
	v4l2_subdev_init(subdev, &aic_dvp_subdev_ops);
	subdev->flags = V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	subdev->entity.function = MEDIA_ENT_F_VID_IF_BRIDGE;
	subdev->entity.ops = &aic_dvp_video_entity_ops;
	subdev->owner = THIS_MODULE;
	snprintf(subdev->name, sizeof(subdev->name), AIC_DVP_NAME "-sd");
	v4l2_set_subdevdata(subdev, dvp);

	dvp->subdev_pads[DVP_SUBDEV_SINK].flags =
		MEDIA_PAD_FL_SINK | MEDIA_PAD_FL_MUST_CONNECT;
	dvp->subdev_pads[DVP_SUBDEV_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&subdev->entity, DVP_SUBDEV_PAD_NUM,
				     dvp->subdev_pads);
	if (ret < 0)
		return ret;

	dvp->vdev_pad.flags = MEDIA_PAD_FL_SINK | MEDIA_PAD_FL_MUST_CONNECT;
	ret = media_entity_pads_init(&vdev->entity, 1, &dvp->vdev_pad);
	if (ret < 0)
		return ret;

	ret = aic_dvp_buf_register(dvp);
	if (ret)
		goto err_clean_pad;

	ret = aic_dvp_notifier_init(dvp);
	if (ret)
		goto err_unregister_media;

	ret = v4l2_async_notifier_register(&dvp->v4l2, &dvp->notifier);
	if (ret) {
		dev_err(dvp->dev, "Couldn't register our notifier.\n");
		goto err_unregister_media;
	}

	if (aic_dvp_sysfs_create(&pdev->dev) < 0)
		goto err_unregister_media;

	pm_runtime_enable(&pdev->dev);
	dev_info(&pdev->dev, "ArtInChip DVP Loaded.\n");
	return 0;

err_unregister_media:
	media_device_unregister(&dvp->mdev);
	aic_dvp_buf_unregister(dvp);

err_clean_pad:
	media_device_cleanup(&dvp->mdev);

	return ret;
}

static int aic_dvp_remove(struct platform_device *pdev)
{
	struct aic_dvp *dvp = platform_get_drvdata(pdev);

	v4l2_async_notifier_unregister(&dvp->notifier);
	v4l2_async_notifier_cleanup(&dvp->notifier);
	media_device_unregister(&dvp->mdev);
	aic_dvp_buf_unregister(dvp);
	media_device_cleanup(&dvp->mdev);

	return 0;
}

static const struct of_device_id aic_dvp_of_match[] = {
	{ .compatible = "artinchip,aic-dvp-v1.0" },
	{ /* Sentinel */ }
};
MODULE_DEVICE_TABLE(of, aic_dvp_of_match);

#ifdef CONFIG_PM

static int aic_dvp_pm_suspend(struct device *dev)
{
	struct aic_dvp *dvp = dev_get_drvdata(dev);

	aic_dvp_enable(dvp, 0);
	if (__clk_is_enabled(dvp->clk))
		clk_disable_unprepare(dvp->clk);
	return 0;
}

static int aic_dvp_pm_resume(struct device *dev)
{
	struct aic_dvp *dvp = dev_get_drvdata(dev);

	clk_set_rate(dvp->clk, dvp->clk_rate);
	if (!__clk_is_enabled(dvp->clk))
		clk_prepare_enable(dvp->clk);
	aic_dvp_enable(dvp, 1);
	return 0;
}

static SIMPLE_DEV_PM_OPS(aic_dvp_pm_ops,
			 aic_dvp_pm_suspend, aic_dvp_pm_resume);
#endif

static struct platform_driver aic_dvp_driver = {
	.probe	= aic_dvp_probe,
	.remove	= aic_dvp_remove,
	.driver	= {
		.name		= AIC_DVP_NAME,
		.of_match_table	= aic_dvp_of_match,
#ifdef CONFIG_PM
		.pm		= &aic_dvp_pm_ops,
#endif
	},
};
module_platform_driver(aic_dvp_driver);

MODULE_DESCRIPTION("ArtInChip DVP driver");
MODULE_AUTHOR("Matteo <duanmt@artinchip.com>");
MODULE_LICENSE("GPL");
