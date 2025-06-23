// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 ArtInChip
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

#define SPINAND_MFR_UMTEK			0x52

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

static int gss01sqa_ooblayout_ecc(struct mtd_info *mtd, int section,
				  struct mtd_oob_region *region)
{
	return -ERANGE;
}

static int gss01sqa_ooblayout_free(struct mtd_info *mtd, int section,
				   struct mtd_oob_region *region)
{
	return -ERANGE;
}

static const struct mtd_ooblayout_ops gss01sqa_ooblayout = {
	.ecc = gss01sqa_ooblayout_ecc,
	.rfree = gss01sqa_ooblayout_free,
};

static int gss01sqa_select_target(struct spinand_device *spinand,
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

static const struct spinand_info umtek_spinand_table[] = {
	SPINAND_INFO("GSS01GAK1",
		     SPINAND_ID(0xBA),
		     NAND_MEMORG(1, 2048, 64, 64, 1024, 1, 1, 1),
		     NAND_ECCREQ(1, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&gss01sqa_ooblayout, NULL),
		     SPINAND_SELECT_TARGET(gss01sqa_select_target)),
};

static int umtek_spinand_init(struct spinand_device *spinand)
{
	pr_info("umtek %s \n", __func__);

	return 0;
}

static void umtek_spinand_cleanup(struct spinand_device *spinand)
{
	pr_info("umtek %s \n", __func__);
}

static int umtek_spinand_detect(struct spinand_device *spinand)
{
	u8 *id = spinand->id.data;
	int ret;

	if (id[1] != SPINAND_MFR_UMTEK)
		return 0;

	ret = spinand_match_and_init(spinand, umtek_spinand_table,
				     ARRAY_SIZE(umtek_spinand_table),
				     &id[2]);
	if (ret)
		return ret;

	return 1;
}

static const struct spinand_manufacturer_ops umtek_spinand_manuf_ops = {
	.detect = umtek_spinand_detect,
	.init = umtek_spinand_init,
	.cleanup = umtek_spinand_cleanup
};

const struct spinand_manufacturer umtek_spinand_manufacturer = {
	.id = SPINAND_MFR_UMTEK,
	.name = "UnitedMemory",
	.ops = &umtek_spinand_manuf_ops,
};

