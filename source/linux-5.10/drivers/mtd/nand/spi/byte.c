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

#define SPINAND_MFR_BYTE 0x68

static SPINAND_OP_VARIANTS(
	read_cache_variants,
	SPINAND_PAGE_READ_FROM_CACHE_QUADIO_OP(0, 1, NULL, 0),
	SPINAND_PAGE_READ_FROM_CACHE_X4_OP(0, 1, NULL, 0),
	SPINAND_PAGE_READ_FROM_CACHE_DUALIO_OP(0, 1, NULL, 0),
	SPINAND_PAGE_READ_FROM_CACHE_X2_OP(0, 1, NULL, 0),
	SPINAND_PAGE_READ_FROM_CACHE_OP(true, 0, 1, NULL, 0),
	SPINAND_PAGE_READ_FROM_CACHE_OP(false, 0, 1, NULL, 0));

static SPINAND_OP_VARIANTS(write_cache_variants,
			   SPINAND_PROG_LOAD_X4(true, 0, NULL, 0),
			   SPINAND_PROG_LOAD(true, 0, NULL, 0));

static SPINAND_OP_VARIANTS(update_cache_variants,
			   SPINAND_PROG_LOAD_X4(false, 0, NULL, 0),
			   SPINAND_PROG_LOAD(false, 0, NULL, 0));

static int by5f1gq5_ooblayout_ecc(struct mtd_info *mtd, int section,
				  struct mtd_oob_region *region)
{
	return -ERANGE;
}

static int by5f1gq5_ooblayout_free(struct mtd_info *mtd, int section,
				   struct mtd_oob_region *region)
{
	return -ERANGE;
}

static const struct mtd_ooblayout_ops by5f1gq5_ooblayout = {
	.ecc = by5f1gq5_ooblayout_ecc,
	.free = by5f1gq5_ooblayout_free,
};

static int by5f1gq5_ecc_get_status(struct spinand_device *spinand, u8 status)
{
	switch (status & STATUS_ECC_MASK) {
	case STATUS_ECC_NO_BITFLIPS:
		return 0;

	case STATUS_ECC_UNCOR_ERROR:
		return -EBADMSG;

	case STATUS_ECC_HAS_BITFLIPS:
		return ((status & STATUS_ECC_MASK) >> 4);

	default:
		break;
	}

	return -EINVAL;
}

static const struct spinand_info byte_spinand_table[] = {
	SPINAND_INFO("BY5F1GQ5UAYIG",
		     SPINAND_ID(SPINAND_READID_METHOD_OPCODE_DUMMY, 0x1B),
		     NAND_MEMORG(1, 2048, 128, 64, 1024, 64, 1, 1, 1),
		     NAND_ECCREQ(8, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&by5f1gq5_ooblayout,
				     by5f1gq5_ecc_get_status)),
};

static int byte_spinand_init(struct spinand_device *spinand)
{
	pr_info("BYTe %s \n", __func__);
	return 0;
}

static void byte_spinand_cleanup(struct spinand_device *spinand)
{
	pr_info("BYTe %s \n", __func__);
}

static const struct spinand_manufacturer_ops byte_spinand_manuf_ops = {
	.init = byte_spinand_init,
	.cleanup = byte_spinand_cleanup
};

const struct spinand_manufacturer byte_spinand_manufacturer = {
	.id = SPINAND_MFR_BYTE,
	.name = "BYTe",
	.chips = byte_spinand_table,
	.nchips = ARRAY_SIZE(byte_spinand_table),
	.ops = &byte_spinand_manuf_ops,
};
