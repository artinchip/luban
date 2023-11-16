/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016-2017 Micron Technology, Inc.
 *
 *  Authors:
 *	Peter Pan <peterpandong@micron.com>
 *
 * Copyright (c) 2021, ArtInChip Technology Co., Ltd
 * Author: Xiong Hao <xiong.hao@artinchip.com>
 */

#ifndef __ARTINCHIP_SPINAND_H
#define __ARTINCHIP_SPINAND_H

#include <common.h>
#include <spi.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/spinand.h>

struct spinand_device *get_spinand(void);
int spinand_block_isbad(struct spinand_device *spinand, loff_t offs);
int spinand_read(struct spinand_device *spinand, loff_t from, size_t len,
			    size_t *retlen, u_char *buf);
struct spinand_device *spl_spinand_init(void);
int spl_spi_nand_read(struct spinand_device *spinand, size_t from,
		      size_t len, size_t *retlen, u_char *buf);

#endif /* __ARTINCHIP_SPINAND_H */
