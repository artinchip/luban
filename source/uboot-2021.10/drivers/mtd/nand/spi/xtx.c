// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022-2025 ArtInChip
 *
 * Authors:
 * keliang.liu <keliang.liu@artinchip.com>
 */

#ifndef __UBOOT__
#include <malloc.h>
#include <linux/device.h>
#include <linux/kernel.h>
#endif
#include <linux/bitops.h>
#include <linux/mtd/spinand.h>

#define SPINAND_MFR_XTX 0x0B

#define XTX_STATUS_ECC_MASK (0xF << 4)
#define XTX_STATUS_ECC_NO_BITFLIPS (0 << 4)
#define XTX_STATUS_ECC_UNCOR_ERROR (0xF << 4)

static SPINAND_OP_VARIANTS(read_cache_variants,
		SPINAND_PAGE_READ_FROM_CACHE_X4_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_X2_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(true, 0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(false, 0, 1, NULL, 0));

static SPINAND_OP_VARIANTS(write_cache_variants,
		SPINAND_PROG_LOAD_X4(true, 0, NULL, 0),
		SPINAND_PROG_LOAD(true, 0, NULL, 0));

static SPINAND_OP_VARIANTS(update_cache_variants,
		SPINAND_PROG_LOAD(false, 0, NULL, 0));

static int xtx_ooblayout_ecc(struct mtd_info *mtd, int section,
			     struct mtd_oob_region *region)
{
	return -ERANGE;
}

static int xtx_ooblayout_free(struct mtd_info *mtd, int section,
			      struct mtd_oob_region *region)
{
	return -ERANGE;
}

static const struct mtd_ooblayout_ops xtx_ooblayout = {
	.ecc = xtx_ooblayout_ecc,
	.rfree = xtx_ooblayout_free,
};

static int xtx_ecc_get_status(struct spinand_device *spinand,
			      u8 status)
{
	switch (status & XTX_STATUS_ECC_MASK) {
	case XTX_STATUS_ECC_NO_BITFLIPS:
		return 0;

	case XTX_STATUS_ECC_UNCOR_ERROR:
		return -EBADMSG;

	case (1 << 4):
	case (2 << 4):
	case (3 << 4):
	case (4 << 4):
	case (5 << 4):
	case (6 << 4):
	case (7 << 4):
	case (8 << 4):
		return ((status & XTX_STATUS_ECC_MASK) >> 4);

	default:
		break;
	}

	return -EINVAL;
}

static const struct spinand_info xtx_spinand_table[] = {
    SPINAND_INFO("XT26G01C",
			 SPINAND_ID(0x11),
			 NAND_MEMORG(1, 2048, 64, 64, 1024, 1, 1, 1),
			 NAND_ECCREQ(8, 528),
			 SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
						  &write_cache_variants,
						  &update_cache_variants),
			 SPINAND_HAS_QE_BIT,
			 SPINAND_ECCINFO(&xtx_ooblayout, xtx_ecc_get_status)),
	SPINAND_INFO("XT26G04C",
			 SPINAND_ID(0x13),
			 NAND_MEMORG(1, 4096, 256, 64, 2048, 1, 1, 1),
			 NAND_ECCREQ(8, 528),
			 SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
						  &write_cache_variants,
						  &update_cache_variants),
			 SPINAND_HAS_QE_BIT,
			 SPINAND_ECCINFO(&xtx_ooblayout, xtx_ecc_get_status)),
};

static int xtx_spinand_init(struct spinand_device *spinand)
{
	return 0;
}

static void xtx_spinand_cleanup(struct spinand_device *spinand)
{

}

static int xtx_spinand_detect(struct spinand_device *spinand)
{
	u8 *id = spinand->id.data;
	int ret;

	if (id[1] != SPINAND_MFR_XTX)
		return 0;

	ret = spinand_match_and_init(spinand, xtx_spinand_table,
				     ARRAY_SIZE(xtx_spinand_table),
				     &id[2]);
	if (ret)
		return ret;

	return 1;
}

static const struct spinand_manufacturer_ops xtx_spinand_manuf_ops = {
    .detect = xtx_spinand_detect,
    .init = xtx_spinand_init,
    .cleanup = xtx_spinand_cleanup};

const struct spinand_manufacturer xtx_spinand_manufacturer = {
    .id = SPINAND_MFR_XTX,
    .name = "xtx",
    .ops = &xtx_spinand_manuf_ops,
};
