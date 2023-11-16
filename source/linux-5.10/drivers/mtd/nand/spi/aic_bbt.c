// SPDX-License-Identifier: GPL-2.0
/*
 * Use bbt partition to manage bad blocks for fast boot in nand
 *
 * Authors:
 * Copyright (c) 2022, ArtInChip Technology Co., Ltd
 * Author: Xuan Wen <xuan.wen@artinchip.com>
 */

#define pr_fmt(fmt)	"spi-nand: " fmt

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/mtd/spinand.h>
#include <linux/string.h>
#include <linux/spi/spi.h>

/* check nand bad block data in spinand badblock part */
static int check_nand_part_bad_block(struct bad_block_ctl *ctl)
{
	u8 cmp_sum = 0;
	int i;
	int ret = 0;

	if (ctl->start != 0xaa55 || ctl->end != 0x55aa) {
		pr_info("ctl->start = 0x%x, ctl->end = 0x%x\n",
			ctl->start, ctl->end);
		return 1;
	}

	if (ctl->len % 512) {
		pr_info("ctl->len = 0x%x\n", ctl->len);
		return 1;
	}

	for (i = 0; i < ctl->len; i++)
		cmp_sum += ctl->data[i];

	ret = cmp_sum == ctl->sum ? 0 : 1;
	if (ret)
		pr_info("cmp_sum = 0x%x, sum = 0x%x\n",
			cmp_sum, ctl->sum);

	return ret;
}

static int get_nand_part_bad_block(struct mtd_info *mtd, u8 *buf)
{
	loff_t offset = CONFIG_NAND_BBT_OFFSET;
	size_t end = offset + CONFIG_NAND_BBT_RANGE;
	size_t rdlen, remain;
	u_char *char_ptr;

	remain = sizeof(struct bad_block_ctl);
	char_ptr = buf;

	while ((remain > 0) && (offset < end)) {
		if (mtd_block_isbad(mtd, offset)) {
			offset += mtd->erasesize;
			continue;
		}

		mtd_read(mtd, offset, remain, &rdlen, char_ptr);
		if (remain != rdlen) {
			pr_err("remain=0x%lx,rdlen=0x%lx\n", remain, rdlen);
			return 1;
		}

		offset += rdlen;
		remain -= rdlen;
		char_ptr += rdlen;
	}

	if (offset >= end) {
		pr_err("can't find good block in bbt part!\n");
		return 1;
	}

	return 0;
}

static int set_nand_part_bad_block(struct mtd_info *mtd,
				   struct bad_block_ctl *bad_ctl)
{
	loff_t offset = CONFIG_NAND_BBT_OFFSET;
	size_t end = offset + CONFIG_NAND_BBT_RANGE;
	size_t wrlen, remain;
	u_char *char_ptr;
	struct erase_info instr;
	u64 i;

	memset(&instr, 0, sizeof(struct erase_info));

	instr.len = mtd->erasesize;

	for (i = CONFIG_NAND_BBT_OFFSET; i < end; i = i + mtd->erasesize) {
		instr.addr = i;

		if (mtd_erase(mtd, &instr)) {
			pr_err("SPINAND: erase failed at 0x%08llx\n",
			       instr.addr);
			return 1;
		}
	}

	remain = sizeof(struct bad_block_ctl);
	char_ptr = (u_char *)bad_ctl;

	while ((remain > 0) && (offset < end)) {
		if (mtd_block_isbad(mtd, offset)) {
			offset += mtd->erasesize;
			continue;
		}

		mtd_write(mtd, offset, remain, &wrlen, char_ptr);
		if (remain != wrlen) {
			pr_err("remain=0x%lx, rdlen=0x%lx\n", remain, wrlen);
			return 1;
		}

		offset += wrlen;
		remain -= wrlen;
		char_ptr += wrlen;
	}

	if (offset >= end) {
		pr_err("can't find good block in bbt part!\n");
		return 1;
	}

	pr_info("data in bbt part update success!\n");
	return 0;
}

static void sum_nand_part_bad_block(struct bad_block_ctl *ctl)
{
	int i;

	ctl->sum = 0;

	for (i = 0; i < ctl->len; i++)
		ctl->sum += ctl->data[i];

	pr_info("nand bbt:ctl->sum = %d\n", ctl->sum);
}

static int update_nand_part_bad_block(struct bad_block_ctl *bad_ctl,
				      loff_t index)
{
	u8 marker = 0;

	if (index >= bad_ctl->len) {
		pr_err("data out off len!\n");
		return 1;
	}

	if (bad_ctl->data[index] == marker) {
		pr_err("the new bad block is already bad!\n");
		return 1;
	}

	bad_ctl->data[index] = marker;

	sum_nand_part_bad_block(bad_ctl);

	return 0;
}

void aic_nand_bbt_markbad(struct mtd_info *mtd, loff_t ofs)
{
	struct spinand_device *spinand = mtd_to_spinand(mtd);
	int ret;

	if (spinand->enable_bbt_ctl != 1)
		return;

	ret = check_nand_part_bad_block(spinand->bbt_ctl);
	if (ret) {
		pr_err("data in bbt part check failed!\n");
		return;
	}

	ret = update_nand_part_bad_block(spinand->bbt_ctl,
					 ofs / mtd->erasesize);
	if (ret)
		return;

	set_nand_part_bad_block(mtd, spinand->bbt_ctl);
}

static void sync_bbt(struct spinand_device *spinand,
		     struct bad_block_ctl *bad_block)
{
	struct nand_device *nand = spinand_to_nand(spinand);
	int i;

	for (i = 0; i < bad_block->len; i++)
		nanddev_bbt_set_block_status(nand, i, bad_block->data[i]);
}

void aic_nand_bbt_init(struct spinand_device *spinand)
{
	struct mtd_info *mtd = spinand_to_mtd(spinand);
	int ret;

	spinand->bbt_ctl = kmalloc(sizeof(*spinand->bbt_ctl), GFP_KERNEL);
	if (!spinand->bbt_ctl)
		return;

	memset(spinand->bbt_ctl, 0, sizeof(*spinand->bbt_ctl));

	ret = get_nand_part_bad_block(mtd, (u8 *)spinand->bbt_ctl);
	if (ret) {
		pr_err("SPI NAND read failed.\n");
		return;
	}

	ret = check_nand_part_bad_block(spinand->bbt_ctl);
	if (!ret) {
		spinand->enable_bbt_ctl = 1;
		pr_info("enable nand bbt\n");
		sync_bbt(spinand, spinand->bbt_ctl);
	}
}
