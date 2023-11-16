/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 ArtInChip Technology Co., Ltd
 */
#ifndef __SPI_NAND_MANUFACTURER_H__
#define __SPI_NAND_MANUFACTURER_H__

#include <linux/mtd/spinand.h>

int spinand_manufacturer_detect(struct spinand_device *spinand);
int spinand_manufacturer_init(struct spinand_device *spinand);
void spinand_manufacturer_cleanup(struct spinand_device *spinand);
#endif
