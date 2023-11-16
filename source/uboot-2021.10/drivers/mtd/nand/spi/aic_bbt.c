// SPDX-License-Identifier: GPL-2.0
/*
 * Use bbt partition to manage bad blocks for fast boot in nand
 *
 * Authors:
 * Copyright (c) 2022, ArtInChip Technology Co., Ltd
 * Author: Xuan Wen <xuan.wen@artinchip.com>
 */

#include <common.h>
#include <bootstage.h>
#include <image.h>
#include <asm/cache.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <command.h>
#include <console.h>
#include <env.h>
#include <log.h>
#include <linux/err.h>
#include <mtd.h>
#include <dm.h>
#include <dm/uclass.h>
#include <dm/device-internal.h>
#include <artinchip_spinand.h>

/* check nand bad block data in spinand bbt part */
static int check_nand_part_bad_block(struct bad_block_ctl *ctl)
{
	u8 cmp_sum = 0;
	int i;
	int ret = 0;

	if (ctl->start != 0xaa55 || ctl->end != 0x55aa) {
		pr_info("bbt:ctl->start = 0x%x, ctl->end = 0x%x\n",
			ctl->start, ctl->end);
		return 1;
	}

	if (ctl->len % 512) {
		pr_info("bbt:ctl->len = 0x%x\n", ctl->len);
		return 1;
	}

	for (i = 0; i < ctl->len; i++)
		cmp_sum += ctl->data[i];

	ret = cmp_sum == ctl->sum ? 0 : 1;
	if (ret)
		pr_info("bbt:cmp_sum = 0x%x, sum = 0x%x\n",
			cmp_sum, ctl->sum);

	return ret;
}

static void sync_bbt(struct spinand_device *spinand,
		     struct bad_block_ctl *bad_block)
{
	struct nand_device *nand = spinand_to_nand(spinand);
	int i;

	for (i = 0; i < bad_block->len; i++)
		nanddev_bbt_set_block_status(nand, i, bad_block->data[i]);
}

#ifdef CONFIG_SPL_BUILD
void spl_nand_bbt_init(struct spinand_device *spinand)
{
	struct nand_device *nand = spinand_to_nand(spinand);
	unsigned long offset, remain;
	size_t end;
	size_t rdlen, ret;
	struct bad_block_ctl *bad_block;

	bad_block = kmalloc(sizeof(*bad_block), GFP_KERNEL);
	if (!bad_block)
		return;

	memset(bad_block, 0, sizeof(struct bad_block_ctl));

	offset = CONFIG_NAND_BBT_OFFSET;
	remain = sizeof(struct bad_block_ctl);
	end = offset + CONFIG_NAND_BBT_RANGE;

	while (spinand_block_isbad(spinand, offset))
		offset += nand->info->erasesize;

	if (offset >= end) {
		pr_err("can't find good block for bbt");
		return;
	}

	spl_spi_nand_read(spinand, offset, remain, &rdlen, (void *)bad_block);
	if (remain != rdlen) {
		pr_err("Tiny SPI NAND read failed.\n");
		return;
	}

	ret = check_nand_part_bad_block(bad_block);
	if (!ret) {
		pr_notice("enable nand bbt\n");
		sync_bbt(spinand, bad_block);
	}
}
#else
static void parse_mtd_device_bad_block(struct bad_block_ctl *bbt_buf)
{
	int i;

	for (i = 0; i < bbt_buf->len; i++)
		bbt_buf->sum += bbt_buf->data[i];
}

static void get_mtd_device_bad_block(struct mtd_info *mtd)
{
	struct spinand_device *spinand = mtd_to_spinand(mtd);
	u64 offs = 0;

	aic_clear_nand_bbt();
	spinand->bbt_buf->len = mtd->size / mtd->erasesize;

	while (offs < mtd->size) {
		mtd_block_isbad(mtd, offs);
		offs += mtd->erasesize;
	}

	parse_mtd_device_bad_block(spinand->bbt_buf);
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
		pr_err("can't find good block for bbt\n");
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
	struct erase_info instr = {.callback = NULL,};

	instr.len = CONFIG_NAND_BBT_RANGE;
	instr.addr = CONFIG_NAND_BBT_OFFSET;
	instr.mtd = mtd;

	if (mtd_erase(mtd, &instr)) {
		pr_err("SPINAND: erase failed at 0x%08llx\n", instr.addr);
		return 1;
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
		pr_err("can't find good block in bbt\n");
		return 1;
	}

	return 0;
}

static void update_nand_part_bad_block(struct bad_block_ctl *ctl,
				       struct bad_block_ctl *bad_ctl)
{
	char *buf = (char *)bad_ctl->data;
	char *cmp_buf = (char *)ctl->data;

	bad_ctl->start = 0xaa55;
	bad_ctl->len = ctl->len;
	bad_ctl->sum = ctl->sum;

	memcpy(buf, cmp_buf, bad_ctl->len);

	bad_ctl->end = 0x55aa;
}

/*
 * return:0 is equal
 */
static int cmp_mtd_and_bad_block(struct bad_block_ctl *ctl,
				 struct bad_block_ctl *bad_ctl)
{
	char *buf;
	char *cmp_buf;
	int ret = 1;

	if (bad_ctl->len == ctl->len && bad_ctl->sum == ctl->sum) {
		buf = (char *)bad_ctl->data;
		cmp_buf = (char *)ctl->data;

		ret = memcmp(buf, cmp_buf, bad_ctl->len);
		if (ret)
			pr_notice("bad block in bbt part and spinand is diff\n");
		return ret;
	}

	pr_notice("bad_ctl->len = 0x%x, ctl->len = 0x%x\n", bad_ctl->len,
		  ctl->len);
	pr_notice("bad_ctl->sum= 0x%x,ctl->sum= 0x%x\n", bad_ctl->sum,
		  ctl->sum);

	return 1;
}

void aic_clear_nand_bbt(void)
{
	int ret = 0;
	struct mtd_info *mtd;
	struct udevice *dev;
	struct nand_device *nand;

	ret = uclass_first_device(UCLASS_MTD, &dev);
	if (ret && !dev) {
		pr_err("Find MTD device failed.\n");
		return;
	}

	device_probe(dev);

	mtd = get_mtd_device(NULL, 0);
	if (IS_ERR_OR_NULL(mtd)) {
		pr_err("There is no mtd device.\n");
		return;
	}

	nand = mtd_to_nanddev(mtd);
	nanddev_bbt_clean(nand);

	put_mtd_device(mtd);
}

void aic_nand_bbt_init(struct spinand_device *spinand)
{
	struct bad_block_ctl *bbt_ctl;
	struct mtd_info *mtd = spinand_to_mtd(spinand);
	int ret;

	bbt_ctl = kmalloc(sizeof(*bbt_ctl), GFP_KERNEL);
	if (!bbt_ctl)
		return;

	memset(bbt_ctl, 0, sizeof(*bbt_ctl));

	ret = get_nand_part_bad_block(mtd, (u8 *)bbt_ctl);
	if (ret) {
		pr_err("SPI NAND read failed.\n");
		return;
	}

	ret = check_nand_part_bad_block(bbt_ctl);
	if (!ret) {
		pr_notice("enable nand bbt ...\n");
		sync_bbt(spinand, bbt_ctl);
	}
}

static int do_nand_bbt(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	int ret = 0;
	struct bad_block_ctl *bad_block_buf;
	struct mtd_info *mtd;
	struct udevice *dev;
	struct spinand_device *spinand;

	if (argc > 1)
		return CMD_RET_USAGE;

	bad_block_buf = kmalloc(sizeof(*bad_block_buf), GFP_KERNEL);
	if (!bad_block_buf) {
		pr_err("Kmalloc bad block buf failed.\n");
		return CMD_RET_FAILURE;
	}

	memset((char *)bad_block_buf, 0, sizeof(*bad_block_buf));

	ret = uclass_first_device(UCLASS_MTD, &dev);
	if (ret && !dev) {
		pr_err("Find MTD device failed.\n");
		ret = -ENODEV;
		goto get_mtd_device_err;
	}

	device_probe(dev);

	mtd = get_mtd_device(NULL, 0);
	if (IS_ERR_OR_NULL(mtd)) {
		pr_err("There is no mtd device.\n");
		goto get_mtd_device_err;
	}

	spinand = mtd_to_spinand(mtd);

	spinand->bbt_buf = kmalloc(sizeof(*spinand->bbt_buf), GFP_KERNEL);
	if (!spinand->bbt_buf) {
		pr_err("Kmalloc bbt buf failed.\n");
		goto kmalloc_bbt_buf_err;
	}

	memset((char *)spinand->bbt_buf, 0, sizeof(*spinand->bbt_buf));

	/* save bad block information from spinand to buf */
	get_mtd_device_bad_block(mtd);

	ret = get_nand_part_bad_block(mtd, (u8 *)bad_block_buf);
	if (ret)
		goto get_nand_part_err;

	ret = check_nand_part_bad_block(bad_block_buf);
	if (!ret) {
		ret = cmp_mtd_and_bad_block(spinand->bbt_buf, bad_block_buf);
		if (!ret) {
			pr_notice("bbt part data is already up to date!\n");
			goto get_nand_part_err;
		}
		pr_notice("find new bad block in spinand!\n");
	}

	pr_notice("data in bbt part is not valid!\n");

	update_nand_part_bad_block(spinand->bbt_buf, bad_block_buf);

	ret = set_nand_part_bad_block(mtd, bad_block_buf);
	if (ret)
		goto get_nand_part_err;

	memset(bad_block_buf, 0, sizeof(struct bad_block_ctl));

	ret = get_nand_part_bad_block(mtd, (u8 *)bad_block_buf);
	if (ret)
		goto get_nand_part_err;

	ret = check_nand_part_bad_block(bad_block_buf);
	if (!ret) {
		ret = cmp_mtd_and_bad_block(spinand->bbt_buf, bad_block_buf);
		if (!ret) {
			pr_notice("bbt part data is already up to date!\n");
			goto get_nand_part_err;
		}
	}

	pr_err("spinand write or read fail!\n");

get_nand_part_err:
	kfree(spinand->bbt_buf);
kmalloc_bbt_buf_err:
	put_mtd_device(mtd);
get_mtd_device_err:
	kfree(bad_block_buf);

	return ret;
}

U_BOOT_CMD(nand_bbt, 1, 0, do_nand_bbt,
	   "fresh all the bad block information to the nand partition bbt",
	   "\n"
);
#endif
