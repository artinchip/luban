// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 ArtInChip
 *
 * Authors:
 *	keliang.liu <keliang.liu@artinchip.com>
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/mtd/spinand.h>

#define SPINAND_MFR_XTX			0x0B

#define XTX_STATUS_ECC_MASK  (0xF << 4)
#define XTX_STATUS_ECC_NO_BITFLIPS (0 << 4)
#define XTX_STATUS_ECC_UNCOR_ERROR (0xF << 4)

static SPINAND_OP_VARIANTS(read_cache_variants,
		SPINAND_PAGE_READ_FROM_CACHE_X4_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_X2_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(true, 0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(false, 0, 1, NULL, 0));

static SPINAND_OP_VARIANTS(write_cache_variants,
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
	.free = xtx_ooblayout_free,
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
		return 8 << 4;
	default:
		break;
	}
	return -EINVAL;
}

static const struct spinand_info xtx_spinand_table[] = {
	SPINAND_INFO("XT26G01C",
		SPINAND_ID(SPINAND_READID_METHOD_OPCODE_DUMMY, 0x11),
		NAND_MEMORG(1, 2048, 64, 64, 1024, 40, 1, 1, 1),
		NAND_ECCREQ(8, 528),
		SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
			&write_cache_variants,
			&update_cache_variants),
			SPINAND_HAS_QE_BIT,
			SPINAND_ECCINFO(&xtx_ooblayout,
			xtx_ecc_get_status)),
};

static int xtx_spinand_init(struct spinand_device *spinand)
{
	return 0;
}

static void xtx_spinand_cleanup(struct spinand_device *spinand)
{

}

static const struct spinand_manufacturer_ops xtx_spinand_manuf_ops = {
	.init = xtx_spinand_init,
	.cleanup = xtx_spinand_cleanup
};

const struct spinand_manufacturer xtx_spinand_manufacturer = {
	.id = SPINAND_MFR_XTX,
	.name = "xtx",
	.chips = xtx_spinand_table,
	.nchips = ARRAY_SIZE(xtx_spinand_table),
	.ops = &xtx_spinand_manuf_ops,
};
