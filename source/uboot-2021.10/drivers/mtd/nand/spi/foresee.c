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

#define SPINAND_MFR_FORESEE			0xCD

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

static int f35sqa_ooblayout_ecc(struct mtd_info *mtd, int section,
				  struct mtd_oob_region *region)
{
	return -ERANGE;
}

static int f35sqa_ooblayout_free(struct mtd_info *mtd, int section,
				   struct mtd_oob_region *region)
{
	return -ERANGE;
}

static const struct mtd_ooblayout_ops f35sqa_ooblayout = {
	.ecc = f35sqa_ooblayout_ecc,
	.rfree = f35sqa_ooblayout_free,
};

static int f35sqa_select_target(struct spinand_device *spinand,
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

static const struct spinand_info foresee_spinand_table[] = {
	SPINAND_INFO("F35SQA002G", 0x72,
		     NAND_MEMORG(1, 2048, 64, 64, 2048, 1, 1, 1),
		     NAND_ECCREQ(1, 528),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&f35sqa_ooblayout, NULL),
		     SPINAND_SELECT_TARGET(f35sqa_select_target)),
	SPINAND_INFO("F35SQA001G", 0x71,
		     NAND_MEMORG(1, 2048, 64, 64, 1024, 1, 1, 1),
		     NAND_ECCREQ(1, 528),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&f35sqa_ooblayout, NULL),
		     SPINAND_SELECT_TARGET(f35sqa_select_target)),
	SPINAND_INFO("F35SQA512M", 0x70,
		     NAND_MEMORG(1, 2048, 64, 64, 512, 1, 1, 1),
		     NAND_ECCREQ(1, 528),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&f35sqa_ooblayout, NULL),
		     SPINAND_SELECT_TARGET(f35sqa_select_target)),
};

static int foresee_spinand_init(struct spinand_device *spinand)
{
	pr_info("Foresee %s \n", __func__);

	return 0;
}

static void foresee_spinand_cleanup(struct spinand_device *spinand)
{
	pr_info("Foresee %s \n", __func__);
}

static int foresee_spinand_detect(struct spinand_device *spinand)
{
	u8 *id = spinand->id.data;
	int ret;

	if (id[1] != SPINAND_MFR_FORESEE)
		return 0;

	ret = spinand_match_and_init(spinand, foresee_spinand_table,
				     ARRAY_SIZE(foresee_spinand_table),
				     id[2]);
	if (ret)
		return ret;

	return 1;
}

static const struct spinand_manufacturer_ops foresee_spinand_manuf_ops = {
	.detect = foresee_spinand_detect,
	.init = foresee_spinand_init,
	.cleanup = foresee_spinand_cleanup
};

const struct spinand_manufacturer foresee_spinand_manufacturer = {
	.id = SPINAND_MFR_FORESEE,
	.name = "Foresee",
	.ops = &foresee_spinand_manuf_ops,
};

