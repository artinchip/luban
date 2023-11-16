// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 ArtInChip
 *
 * Authors:
 *      keliang.liu <keliang.liu@artinchip.com>
 */

#include <linux/mtd/spi-nor.h>

#include "core.h"

static const struct flash_info fmsh_parts[] = {
	/* FudanMicro (Shanghai Fudan Microelectronics Group Company.) */
	{ "FM25Q128", INFO(0x1c4018, 0, 64 * 1024, 256,
			    SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ) },
	{ "FM25Q64", INFO(0x1c4018, 0, 64 * 1024, 128,
			     SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ) },
};

const struct spi_nor_manufacturer spi_nor_fmsh = {
	.name = "FudanMicro",
	.parts = fmsh_parts,
	.nparts = ARRAY_SIZE(fmsh_parts),
};

