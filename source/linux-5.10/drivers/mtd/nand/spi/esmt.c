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

#define SPINAND_MFR_ESMT			0xC8

static SPINAND_OP_VARIANTS(read_cache_variants,
		SPINAND_PAGE_READ_FROM_CACHE_X4_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_X2_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(true, 0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(false, 0, 1, NULL, 0));

static SPINAND_OP_VARIANTS(write_cache_variants,
		SPINAND_PROG_LOAD_X4(true, 0, NULL, 0),
		SPINAND_PROG_LOAD(true, 0, NULL, 0));

static SPINAND_OP_VARIANTS(update_cache_variants,
		SPINAND_PROG_LOAD_X4(false, 0, NULL, 0),
		SPINAND_PROG_LOAD(false, 0, NULL, 0));

static int f50l1g_ooblayout_ecc(struct mtd_info *mtd, int section,
				  struct mtd_oob_region *region)
{
	return -ERANGE;
}

static int f50l1g_ooblayout_free(struct mtd_info *mtd, int section,
				   struct mtd_oob_region *region)
{
	return -ERANGE;
}

static const struct mtd_ooblayout_ops f50l1g_ooblayout = {
	.ecc = f50l1g_ooblayout_ecc,
	.free = f50l1g_ooblayout_free,
};

static int f50l1g_ecc_get_status(struct spinand_device *spinand,
				      u8 status)
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

static const struct spinand_info esmt_spinand_table[] = {
	SPINAND_INFO("F50L1G",
		SPINAND_ID(SPINAND_READID_METHOD_OPCODE_DUMMY, 0x01),
		NAND_MEMORG(1, 2048, 64, 64, 1024, 40, 1, 1, 1),
		NAND_ECCREQ(1, 512),
		SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
			&write_cache_variants,
			&update_cache_variants),
			SPINAND_HAS_QE_BIT,
			SPINAND_ECCINFO(&f50l1g_ooblayout,
			f50l1g_ecc_get_status)),
};

static int esmt_spinand_init(struct spinand_device *spinand)
{
	pr_info("Esmt %s \n", __func__);
	return 0;
}

static void esmt_spinand_cleanup(struct spinand_device *spinand)
{
	pr_info("Esmt %s \n", __func__);
}

static const struct spinand_manufacturer_ops esmt_spinand_manuf_ops = {
	.init = esmt_spinand_init,
	.cleanup = esmt_spinand_cleanup
};

const struct spinand_manufacturer esmt_spinand_manufacturer = {
	.id = SPINAND_MFR_ESMT,
	.name = "Esmt",
	.chips = esmt_spinand_table,
	.nchips = ARRAY_SIZE(esmt_spinand_table),
	.ops = &esmt_spinand_manuf_ops,
};
