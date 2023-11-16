// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 ArtInChip
 *
 * Authors:
 *	keliang.liu <keliang.liu@artinchip.com>
 */

#ifndef __UBOOT__
#include <malloc.h>
#include <linux/device.h>
#include <linux/kernel.h>
#endif
#include <linux/bitops.h>
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
	.rfree = f50l1g_ooblayout_free,
};

static int f50l1g_select_target(struct spinand_device *spinand,
				  unsigned int target)
{
	struct spi_mem_op op = SPI_MEM_OP(SPI_MEM_OP_CMD(0xc2, 1),
					  SPI_MEM_OP_NO_ADDR,
					  SPI_MEM_OP_NO_DUMMY,
					  SPI_MEM_OP_DATA_OUT(1,
							spinand->scratchbuf,
							1));

	*spinand->scratchbuf = target;
	return spi_mem_exec_op(spinand->slave, &op);
}

static const struct spinand_info elite_spinand_table[] = {
	SPINAND_INFO("F50L2G", 0x24,
		NAND_MEMORG(1, 2048, 128, 64, 2048, 2, 1, 1),
		NAND_ECCREQ(8, 512),
		SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
			&write_cache_variants,
			&update_cache_variants),
			SPINAND_HAS_QE_BIT,
			SPINAND_ECCINFO(&f50l1g_ooblayout, NULL),
			SPINAND_SELECT_TARGET(f50l1g_select_target)),
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

static int elite_spinand_detect(struct spinand_device *spinand)
{
	u8 *id = spinand->id.data;
	int ret;

	if (id[1] != SPINAND_MFR_ELITE)
		return 0;

	ret = spinand_match_and_init(spinand, elite_spinand_table,
				     ARRAY_SIZE(elite_spinand_table),
				     id[2]);
	if (ret)
		return ret;

	return 1;
}

static const struct spinand_manufacturer_ops elite_spinand_manuf_ops = {
	.detect = elite_spinand_detect,
	.init = elite_spinand_init,
	.cleanup = elite_spinand_cleanup
};

const struct spinand_manufacturer elite_spinand_manufacturer = {
	.id = SPINAND_MFR_ELITE,
	.name = "Elite",
	.ops = &elite_spinand_manuf_ops,
};
