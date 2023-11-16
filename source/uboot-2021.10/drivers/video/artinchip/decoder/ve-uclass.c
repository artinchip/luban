// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 ArtInChip Technology Co.,Ltd
 * Author: Hao Xiong <hao.xiong@artinchip.com>
 */

#define LOG_CATEGORY UCLASS_DECODER

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <log.h>
#include <artinchip_ve.h>
#include <malloc.h>
#include <dm/device_compat.h>
#include <asm/global_data.h>
#include <dm/device-internal.h>
#include <dm/uclass-internal.h>
#include <dm/lists.h>
#include <dm/util.h>

DECLARE_GLOBAL_DATA_PTR;

int gunzip_init_device(struct udevice **dev)
{
	int ret;

	ret = uclass_first_device(UCLASS_DECODER, dev);
	if (ret) {
		pr_err("gunzip init device failed\n");
		return ret;
	}

	return ret;
}

int gunzip_decompress(struct udevice *dev, void *dst, int dstlen,
			unsigned char *src, unsigned long *lenp)
{
	const struct decoder_ops *ops;
	int ret;

	ops = device_get_ops(dev);
	if (!ops->init || !ops->decompress_init ||
	    !ops->decompress || !ops->release)
		return -ENOSYS;

	ret = ops->init(dev);
	if (ret) {
		pr_err("decoder init failed\n");
		return -EINVAL;
	}

	ret = ops->decompress_init(dev, dst, dstlen, src, lenp);
	if (ret) {
		pr_err("init failed\n");
		goto out;
	}

	ret = ops->decompress(dev);
	if (ret) {
		pr_err("decompress failed\n");
		goto out;
	}

out:
	ops->release(dev);
	return ret;
}

int aic_png_decode(void *src, unsigned int max_size)
{
	const struct decoder_ops *ops;
	struct udevice *dev;
	int ret;

	ret = uclass_first_device(UCLASS_DECODER, &dev);
	if (ret) {
		pr_err("find decoder device failed\n");
		return ret;
	}

	ops = device_get_ops(dev);
	if (!ops->init || !ops->png_decode || !ops->release)
		return -ENOSYS;

	ret = ops->init(dev);
	if (ret) {
		pr_err("decoder init failed\n");
		return -EINVAL;
	}

	ret = ops->png_decode(dev, src, max_size);
	if (ret)
		pr_err("png decode failed\n");

	ops->release(dev);
	return ret;
}

int aic_jpeg_decode(void *src, unsigned int size)
{
	const struct decoder_ops *ops;
	struct udevice *dev;
	int ret;

	ret = uclass_first_device(UCLASS_DECODER, &dev);
	if (ret) {
		pr_err("find decoder device failed\n");
		return ret;
	}

	ops = device_get_ops(dev);
	if (!ops->init || !ops->jpeg_decode || !ops->release)
		return -ENOSYS;

	ret = ops->init(dev);
	if (ret) {
		pr_err("decoder init failed\n");
		return -EINVAL;
	}

	ret = ops->jpeg_decode(dev, src, size);
	if (ret)
		pr_err("jpeg decode failed\n");

	ops->release(dev);
	return ret;
}

UCLASS_DRIVER(ve) = {
	.id		= UCLASS_DECODER,
	.name		= "ve",
};
