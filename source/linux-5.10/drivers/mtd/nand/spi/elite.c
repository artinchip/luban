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

#define SPINAND_MFR_ELITE			0x2C

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

static int f50l2g_ooblayout_ecc(struct mtd_info *mtd, int section,
				  struct mtd_oob_region *region)
{
	return -ERANGE;
}

static int f50l2g_ooblayout_free(struct mtd_info *mtd, int section,
				   struct mtd_oob_region *region)
{
	return -ERANGE;
}

static const struct mtd_ooblayout_ops f50l2g_ooblayout = {
	.ecc = f50l2g_ooblayout_ecc,
	.free = f50l2g_ooblayout_free,
};

static int f50l2g_ecc_get_status(struct spinand_device *spinand,
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

static const struct spinand_info elite_spinand_table[] = {
	SPINAND_INFO("F50L2G",
		SPINAND_ID(SPINAND_READID_METHOD_OPCODE_DUMMY, 0x24),
		NAND_MEMORG(1, 2048, 128, 64, 2048, 40, 2, 1, 1),
		NAND_ECCREQ(8, 512),
		SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
			&write_cache_variants,
			&update_cache_variants),
			SPINAND_HAS_QE_BIT,
			SPINAND_ECCINFO(&f50l2g_ooblayout,
			f50l2g_ecc_get_status)),
};

static int elite_spinand_init(struct spinand_device *spinand)
{
	pr_info("Elite %s \n", __func__);
	return 0;
}

static void elite_spinand_cleanup(struct spinand_device *spinand)
{
	pr_info("Elite %s \n", __func__);
}

static const struct spinand_manufacturer_ops elite_spinand_manuf_ops = {
	.init = elite_spinand_init,
	.cleanup = elite_spinand_cleanup
};

const struct spinand_manufacturer elite_spinand_manufacturer = {
	.id = SPINAND_MFR_ELITE,
	.name = "Elite",
	.chips = elite_spinand_table,
	.nchips = ARRAY_SIZE(elite_spinand_table),
	.ops = &elite_spinand_manuf_ops,
};
