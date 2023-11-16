/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2023 ArtInChip Technology Co.,Ltd
 */

#ifndef _ARTINCHIP_GE_H_
#define _ARTINCHIP_GE_H_

struct ge_ops {
	int (*open)(struct udevice *dev);

	void (*close)(struct udevice *dev);
};

static inline void aic_ge_open_device(struct udevice *dev)
{
	const struct ge_ops *ops = device_get_ops(dev);

	if (ops->open)
		ops->open(dev);
}

static inline void aic_ge_close_device(struct udevice *dev)
{
	const struct ge_ops *ops = device_get_ops(dev);

	if (ops->close)
		ops->close(dev);
}

#endif	/* _ARTINCHIP_GE_H_  */
