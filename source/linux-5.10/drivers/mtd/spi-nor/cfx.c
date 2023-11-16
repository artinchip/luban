// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 ArtInChip
 *
 * Authors:
 *      keliang.liu <keliang.liu@artinchip.com>
 */

#include <linux/mtd/spi-nor.h>

#include "core.h"

static const struct flash_info cfx_parts[] = {
	/* CFX (Zhuhai ChuangFeiXin-Technology.) */
	{ "GM25Q128A", INFO(0xa14018, 0, 64 * 1024, 256,
			    SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ) },
};

const struct spi_nor_manufacturer spi_nor_cfx = {
	.name = "ChuangFeiXin",
	.parts = cfx_parts,
	.nparts = ARRAY_SIZE(cfx_parts),
};

