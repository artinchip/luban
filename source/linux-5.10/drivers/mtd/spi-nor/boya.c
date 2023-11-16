// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2005, Intec Automation Inc.
 * Copyright (C) 2014, Freescale Semiconductor, Inc.
 */

#include <linux/mtd/spi-nor.h>

#include "core.h"

static const struct flash_info boya_parts[] = {
	{ "by25q128as", INFO(0x684018, 0, 64 * 1024, 256,
			SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ |
			SPI_NOR_HAS_LOCK | SPI_NOR_HAS_TB)
	},
};

const struct spi_nor_manufacturer spi_nor_boya = {
	.name = "boya",
	.parts = boya_parts,
	.nparts = ARRAY_SIZE(boya_parts),
};
