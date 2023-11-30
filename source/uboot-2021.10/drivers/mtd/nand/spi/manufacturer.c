// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016-2017 Micron Technology, Inc.
 *
 * Authors:
 *	Peter Pan <peterpandong@micron.com>
 *	Boris Brezillon <boris.brezillon@bootlin.com>
 * Copyright (c) 2023, ArtInChip Technology Co., Ltd
 * Author: Dehuang Wu <dehuang.wu@artinchip.com>
 */

#define pr_fmt(fmt)	"spi-nand: " fmt

#include <common.h>
#include <errno.h>
#include <watchdog.h>
#include <spi.h>
#include <spi-mem.h>
#include <dm/device_compat.h>
#include <dm/devres.h>
#include <linux/bitops.h>
#include <linux/bug.h>
#include <linux/mtd/spinand.h>
#include "manufacturer.h"

static const struct spinand_manufacturer *spinand_manufacturers[] = {
#ifdef CONFIG_SPI_NAND_GIGADEVICE
	&gigadevice_spinand_manufacturer,
#endif
#ifdef CONFIG_SPI_NAND_MACRONIX
	&macronix_spinand_manufacturer,
#endif
#ifdef CONFIG_SPI_NAND_MICRON
	&micron_spinand_manufacturer,
#endif
#ifdef CONFIG_SPI_NAND_TOSHIBA
	&toshiba_spinand_manufacturer,
#endif
#ifdef CONFIG_SPI_NAND_WINBOND
	&winbond_spinand_manufacturer,
#endif
#ifdef CONFIG_SPI_NAND_FMSH
	&fmsh_spinand_manufacturer,
#endif
#ifdef CONFIG_SPI_NAND_FORESEE
	&foresee_spinand_manufacturer,
#endif
#ifdef CONFIG_SPI_NAND_ZBIT
	&zbit_spinand_manufacturer,
#endif
#ifdef CONFIG_SPI_NAND_ELITE
	&elite_spinand_manufacturer,
#endif
#ifdef CONFIG_SPI_NAND_ESMT
	&esmt_spinand_manufacturer,
#endif
#ifdef CONFIG_SPI_NAND_UMTEK
	&umtek_spinand_manufacturer,
#endif
#ifdef CONFIG_SPI_NAND_BYTE
	&byte_spinand_manufacturer,
#endif
};

int spinand_manufacturer_detect(struct spinand_device *spinand)
{
	unsigned int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(spinand_manufacturers); i++) {
		ret = spinand_manufacturers[i]->ops->detect(spinand);
		if (ret > 0) {
			spinand->manufacturer = spinand_manufacturers[i];
			return 0;
		} else if (ret < 0) {
			continue;
		}
	}

	return -ENOTSUPP;
}

int spinand_manufacturer_init(struct spinand_device *spinand)
{
	if (spinand->manufacturer->ops->init)
		return spinand->manufacturer->ops->init(spinand);

	return 0;
}

void spinand_manufacturer_cleanup(struct spinand_device *spinand)
{
	/* Release manufacturer private data */
	if (spinand->manufacturer->ops->cleanup)
		return spinand->manufacturer->ops->cleanup(spinand);
}

