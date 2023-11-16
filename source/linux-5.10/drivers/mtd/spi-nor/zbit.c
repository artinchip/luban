// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 ArtInChip
 *
 */

#include <linux/mtd/spi-nor.h>

#include "core.h"

static const struct flash_info zbit_parts[] = {
	/* ZBIT Semi  */
	{ "ZB25VQ128", INFO(0x5e4018, 0, 64 * 1024, 256,
			    SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ) },
};

const struct spi_nor_manufacturer spi_nor_zbit = {
	.name = "ZBIT",
	.parts = zbit_parts,
	.nparts = ARRAY_SIZE(zbit_parts),
};

