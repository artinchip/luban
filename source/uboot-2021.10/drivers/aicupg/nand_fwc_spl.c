// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd
 * Author: Dehuang Wu <dehuang.wu@artinchip.com>
 */
#include <common.h>
#include <command.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <malloc.h>
#include <artinchip/aicupg.h>
#include <mtd.h>
#include <env.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/mtd.h>
#include "nand_fwc_priv.h"
#include "nand_fwc_spl.h"
#include "spi_enc_spl.h"
#include <u-boot/crc.h>

struct aicupg_nand_spl {
	struct mtd_info *mtd;
	u32 spl_blocks[SPL_NAND_IMAGE_BACKUP_NUM];
	u32 rx_size;
	u32 buf_size;
	u8 *image_buf;
	u32 spienc_tweak;
};

static struct aicupg_nand_spl g_nand_spl;

/*
 * FSBL(SPL) candidate block search table
 *
 * This table define 16 candidate block numbers to save FSBL(First Stage Boot
 * Loader).
 *
 * How to use it:
 * - FSBL Programmer
 *   When program FSBL, programmer need to find out first 4 good blocks in this
 *   order, then program FSBL to those blocks, including Page table.
 * - BROM
 *   BROM shall try to read the FSBL in order of block id in this table, and
 *   BROM only try to read FSBL in first good block.
 *
 * Why to do this:
 * - Reason
 *   AIC solution need to support some bad NANDs, those NANDs can't guarantee
 *   first 4 blocks are good, so AIC designed this candidate search table to
 *   increase the find-out good block rate.
 *   This table is fixed, blocks distributes in first 1024 blocks.
 */
static u32 spl_candidate_block_table[SPL_CANDIDATE_BLOCK_NUM] = {
	0,   1,	  2,   3,   202, 32,  312, 296, 142,
	136, 392, 526, 452, 708, 810, 552, 906, 674,
};
/*
 * 0xFF is good block mark value.
 * 0xAC is AIC SPL reserved mark value.
 */
#define SPL_BLOCK_MARK 0xAC
#define IS_BADBLOCK(oob0, oob1)                                                \
	(((oob0) != 0xFF && (oob0) != SPL_BLOCK_MARK) || ((oob1) != 0xFF))

#define IS_SPLBLOCK(oob0, oob1) (((oob0) == SPL_BLOCK_MARK))

#ifdef DEBUG
static void mtd_dump_buf(const u8 *buf, uint len)
{
	int i, j;

	for (i = 0; i < len; ) {
		printf("0x%08x:\t", i);
		for (j = 0; j < 8; j++)
			printf("%02x ", buf[i + j]);
		printf(" ");
		i += 8;
		for (j = 0; j < 8; j++)
			printf("%02x ", buf[i + j]);
		printf("\n");
		i += 8;
	}
}
#endif

static s32 get_good_blocks_for_spl(struct mtd_info *mtd, u32 *spl_blocks,
				   u32 *blkmark, u32 num)
{
	struct mtd_oob_ops ops;
	struct nand_device *nand = mtd_to_nanddev(mtd);
	ulong bad_block_mark_flag_for_test = 0;
	char *env_bad_bock_mark_flag;
	loff_t offs;
	s32 ret, i, blkidx, cnt;
	u8 buf[4];

	ret = 0;
	cnt = 0;

	/*
	 * bad_block_mark_flag in ENV is for test
	 *
	 * if bad_block_mark_flag = 0xFF, means first 8 candidate blocks is
	 * marked as bad block.
	 */
	env_bad_bock_mark_flag = env_get("bad_block_mark_flag");
	if (env_bad_bock_mark_flag)
		bad_block_mark_flag_for_test =
			hextoul(env_bad_bock_mark_flag, NULL);

	memset(&ops, 0, sizeof(struct mtd_oob_ops));
	ops.ooblen = 2;
	ops.oobbuf = buf;
	ops.mode = MTD_OPS_RAW;

	for (i = 0; i < num; i++)
		spl_blocks[i] = SPL_INVALID_BLOCK_IDX;

	for (i = 0; i < SPL_CANDIDATE_BLOCK_NUM; i++) {
		blkidx = spl_candidate_block_table[i];

		/* For multi-plane device, don't use block 1 & 3, because
		 * boot rom cannot read data from block 1 and 3
		 */
		if ((nand->memorg.planes_per_lun != 1) &&
		    (blkidx == 1 || blkidx == 3))
			continue;
		offs = mtd->erasesize * blkidx;
		ret = mtd_read_oob(mtd, offs, &ops);
		if (ret) {
			pr_err("Read OOB from block %d failed. ret = %d\n", blkidx, ret);
			continue;
		}
		if (IS_BADBLOCK(buf[0], buf[1])) {
			pr_err("Block %d is bad 0x%x 0x%x.\n", blkidx, buf[0],
			       buf[1]);
			continue;
		}
		if (bad_block_mark_flag_for_test & (1 << i)) {
			pr_err("Block %d is marked as bad for test.\n", blkidx);
			continue;
		}
		if (IS_SPLBLOCK(buf[0], buf[1]))
			/* Block already marked as SPL block */
			blkmark[cnt] = 1;
		else
			blkmark[cnt] = 0;

		/* Found one good block */
		pr_info("Found good block %d\n", blkidx);
		spl_blocks[cnt++] = blkidx;
		if (cnt == num)
			break;
	}

	if (cnt == 0)
		ret = -1;

	return ret;
}

static s32 mark_image_block_as_reserved(struct mtd_info *mtd, u32 blkidx)
{
	struct nand_device *nand = mtd_to_nanddev(mtd);
	struct mtd_oob_ops ops;
	struct nand_pos pos;
	loff_t offs;
	s32 ret;
	u32 entry;
	u8 buf[4];

	memset(&ops, 0, sizeof(struct mtd_oob_ops));
	/* Mark SPL reserve at OOB */
	buf[0] = SPL_BLOCK_MARK;
	ops.ooblen = 1;
	ops.oobbuf = buf;

	ret = 0;
	offs = mtd->erasesize * blkidx;

	ret = mtd_write_oob(mtd, offs, &ops);
	if (ret) {
		pr_err("Mark blk %d as bad failed.\n", blkidx);
		return ret;
	}
	nanddev_offs_to_pos(nand, offs, &pos);
	entry = nanddev_bbt_pos_to_entry(nand, &pos);
	ret = nanddev_bbt_set_block_status(nand, entry,
					   NAND_BBT_BLOCK_FACTORY_BAD);

	if (ret)
		pr_err("Set blk %d bad status failed.\n", blkidx);
	pr_info("Set blk %d bad status OK.\n", blkidx);
	return ret;
}

/*
 * Mark SPL reserve blocks, so that UBI and other partition  won't use it
 */
s32 nand_fwc_spl_reserve_blocks(void)
{
	u32 spl_blocks[SPL_NAND_IMAGE_BACKUP_NUM];
	u32 block_mark[SPL_NAND_IMAGE_BACKUP_NUM];
	struct mtd_info *mtd;
	s32 ret, i;

	mtd = get_mtd_device(NULL, 0);
	if (IS_ERR_OR_NULL(mtd)) {
		pr_err("There is no mtd device.\n");
		return -1;
	}
	ret = get_good_blocks_for_spl(mtd, spl_blocks, block_mark,
				      SPL_NAND_IMAGE_BACKUP_NUM);

	if (ret) {
		pr_err("Get SPL blocks failed.\n");
		return ret;
	}

	for (i = 0; i < SPL_NAND_IMAGE_BACKUP_NUM; i++) {
		if (spl_blocks[i] != SPL_INVALID_BLOCK_IDX) {
			if (spl_blocks[i] < 4) /* Don't mark first 4 blocks */
				continue;
			if (block_mark[i])
				continue;
			/* Not marked yet, mark it */
			ret = mark_image_block_as_reserved(mtd, spl_blocks[i]);
			if (ret) {
				pr_err("Mark SPL block failed.\n");
				return -1;
			}
		}
	}

	return 0;
}

static s32 nand_spl_get_good_blocks(struct aicupg_nand_spl *spl)
{
	u32 block_mark[SPL_NAND_IMAGE_BACKUP_NUM];
	s32 ret;

	ret = get_good_blocks_for_spl(spl->mtd, spl->spl_blocks, block_mark,
				      SPL_NAND_IMAGE_BACKUP_NUM);
	if (ret) {
		pr_err("Get blocks for SPL failed.\n");
		return -1;
	}

	return ret;
}

static s32 nand_spl_erase_image_blocks(struct aicupg_nand_spl *spl)
{
	struct erase_info erase_op = {};
	loff_t offs;
	u32 blkidx;
	s32 ret, i;

	ret = 0;
	for (i = 0; i < SPL_NAND_IMAGE_BACKUP_NUM; i++) {
		blkidx = spl->spl_blocks[i];
		if (blkidx == SPL_INVALID_BLOCK_IDX)
			continue;
		offs = spl->mtd->erasesize * blkidx;
		erase_op.mtd = spl->mtd;
		erase_op.addr = offs;
		erase_op.len = spl->mtd->erasesize;
		erase_op.scrub = true;
		ret = mtd_erase(spl->mtd, &erase_op);
		if (ret) {
			pr_err("Erase block %d failed.\n", blkidx);
			return ret;
		}
	}
	return ret;
}

static u32 calc_page_checksum(u8 *start, u32 size)
{
	u8 *p;
	u32 i, val, sum, rest, cnt;

	p = start;
	i = 0;
	sum = 0;
	cnt = size >> 2;

	for (i = 0; i < cnt; i++) {
		p = &start[i * 4];
		val = (p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0];
		sum += val;
	}

	/* Calculate not 32 bit aligned part */
	rest = size - (cnt << 2);
	p = &start[cnt * 4];
	val = 0;
	for (i = 0; i < rest; i++)
		val += (p[i] << (i * 8));
	sum += val;

	return sum;
}

static s32 spl_build_page_table(struct aicupg_nand_spl *spl,
				struct nand_page_table *pt)
{
	u32 pcnt, pgidx, data_size, i, sumval, pa, blkidx;
	u32 page_per_blk, page_in_blk;
	u8 *page_data, *p, *end;
	u32 slice_size;
	s32 ret = 0;

	memset(pt, 0xFF, sizeof(*pt));
	pt->head.magic[0] = 'A';
	pt->head.magic[1] = 'I';
	pt->head.magic[2] = 'C';
	pt->head.magic[3] = 'P';

	pr_debug("%s, going to generate page table.\n", __func__);
	slice_size = spl->mtd->writesize;
	pt->head.page_size = PAGE_SIZE_2KB;
	if (spl->mtd->writesize == 4096)
		pt->head.page_size = PAGE_SIZE_4KB;

	page_per_blk = spl->mtd->erasesize / spl->mtd->writesize;
	page_data = malloc(PAGE_MAX_SIZE);
	if (!page_data)
		return -1;

	pcnt = ROUNDUP(spl->buf_size, slice_size) / slice_size;
	pt->head.entry_cnt = pcnt + 1;

	if (pt->head.entry_cnt > 2 * PAGE_TABLE_MAX_ENTRY) {
		pr_err("Error, SPL is too large entry cnt %d, max %d.\n",
		       pt->head.entry_cnt, 2 * PAGE_TABLE_MAX_ENTRY);
		ret = -1;
		goto out;
	}

	p = spl->image_buf;
	end = spl->image_buf + spl->buf_size;

	/* The following code looks weird, the explaination:
	 * 1. The original design is use 4 blocks to store 4 backup of SPL,
	 *    but the SPL code size is larger than the expectation (More than 126KB)
	 *    so the new solution is use 4 blocks to store 1 backup of SPL
	 * 2. Boot ROM only care the first part of SPL, so the caculate first 101 page
	 *    data's checksum is enough
	 * 3. If the SPL data is more than 101 pages, the page address will be stored
	 *    to original backup1's place, corresponding checksum will be stored to
	 *    original backup2's place.
	 */
	/* The first page is used to store page table */
	for (pgidx = 1; pgidx < pcnt + 1; pgidx++) {
		if ((p + slice_size) <= end)
			data_size = slice_size;
		else
			data_size = (unsigned long)end - (unsigned long)p;

		memset(page_data, 0xFF, slice_size);
		memcpy(page_data, p, data_size);
		p += data_size;

		sumval = calc_page_checksum(page_data, slice_size);

		if (pgidx > 2 * PAGE_TABLE_MAX_ENTRY) {
			pr_err("Error, SPL is too large.\n");
			ret = -1;
			goto out;
		}

		/* Calculate where the data is stored */
		i = pgidx / page_per_blk;
		page_in_blk = pgidx % page_per_blk;
		blkidx = spl->spl_blocks[i];
		if (blkidx == SPL_INVALID_BLOCK_IDX) {
			pr_info("%s, block id is invalid\n", __func__);
			ret = -1;
			goto out;
		}
		pa = (blkidx << 6) + page_in_blk;
		if (pgidx < PAGE_TABLE_MAX_ENTRY) {
			pt->entry[pgidx].pageaddr[0] = pa;
			pt->entry[pgidx].checksum = ~sumval;
		} else {
			/* Calculate where the info is stored */
			pt->entry[pgidx % PAGE_TABLE_MAX_ENTRY].pageaddr[1] =
				pa;
			pt->entry[pgidx % PAGE_TABLE_MAX_ENTRY].pageaddr[2] =
				~sumval;
		}
	}

	pgidx = 0;
	blkidx = spl->spl_blocks[0];
	pa = (blkidx << 6) + pgidx;
	pt->entry[0].pageaddr[0] = pa;
	pt->entry[0].checksum = 0;
	memset(page_data, 0xFF, PAGE_TABLE_USE_SIZE);
	memcpy(page_data, pt, sizeof(*pt));

	sumval = calc_page_checksum(page_data, PAGE_TABLE_USE_SIZE);
	pt->entry[0].checksum = ~sumval;
	ret = 0;

out:
	free(page_data);
	return ret;
}

/*
 * Write SPL image to flash blocks
 */
static s32 nand_fwc_spl_program(struct fwc_info *fwc,
				struct aicupg_nand_spl *spl)
{
	u8 *page_data = NULL, *rd_page_data = NULL, *p, *end;
	struct nand_page_table *pt = NULL;
	u32 data_size, blkidx, blkcnt, pa, pgidx, slice_size;
	size_t retlen;
	loff_t offs;
	s32 ret = -1, i, calc_len;
	u32 trans_crc;
	u32 partition_crc;
	u32 trans_size;
	u32 page_per_blk;

	pt = malloc(PAGE_TABLE_USE_SIZE);
	page_data = malloc(PAGE_MAX_SIZE);
	rd_page_data = malloc(PAGE_MAX_SIZE);
	if (!page_data || !rd_page_data || !pt) {
		ret = -1;
		goto out;
	}

	ret = spl_build_page_table(spl, pt);
	if (ret) {
		pr_err("Generate page table failed.\n");
		ret = -1;
		goto out;
	}
	slice_size = spl->mtd->writesize;
	page_per_blk = spl->mtd->erasesize / spl->mtd->writesize;

	/* How many blocks will be used */
	blkcnt = (pt->head.entry_cnt + page_per_blk - 1) / page_per_blk;
	for (i = 0; i < blkcnt; i++) {
		blkidx = spl->spl_blocks[i];
		if (blkidx < 4) {
			/* First 4 blocks don't mark */
			continue;
		}
		mark_image_block_as_reserved(spl->mtd, blkidx);
	}
	/* Program page table and image data to blocks */
	blkidx = spl->spl_blocks[0];
	if (blkidx == SPL_INVALID_BLOCK_IDX) {
		ret = -1;
		goto out;
	}
	/*  Page table is written to the first page. */
	memset(page_data, 0xFF, PAGE_TABLE_USE_SIZE);
	memcpy(page_data, pt, sizeof(*pt));

	pa = pt->entry[0].pageaddr[0];
	offs = pa * spl->mtd->writesize;
	pr_debug("Write page table to blk %d pa 0x%x., off 0x%x\n", blkidx, pa,
		 (u32)offs);

	ret = mtd_write(spl->mtd, offs, PAGE_TABLE_USE_SIZE, &retlen,
			page_data);
	if (ret) {
		pr_err("Write SPL page %d failed.\n", 0);
		ret = -1;
		goto out;
	}

	/* Write image data to page */
	trans_crc = 0;
	partition_crc = 0;
	trans_size = 0;
	p = spl->image_buf;
	end = p + spl->buf_size;
	for (pgidx = 1; pgidx < pt->head.entry_cnt; pgidx++) {
		i = pgidx / page_per_blk;
		blkidx = spl->spl_blocks[i];
		if ((p + slice_size) <= end)
			data_size = slice_size;
		else
			data_size = (unsigned long)end - (unsigned long)p;
		memset(page_data, 0xFF, slice_size);
		memset(rd_page_data, 0xFF, slice_size);
		memcpy(page_data, p, data_size);

		p += data_size;
		if (pgidx < PAGE_TABLE_MAX_ENTRY)
			pa = pt->entry[pgidx].pageaddr[0];
		else
			pa = pt->entry[pgidx % PAGE_TABLE_MAX_ENTRY].pageaddr[1];
		offs = pa * spl->mtd->writesize;
		pr_debug("Write data to blk %d pa 0x%x., off 0x%x\n", blkidx,
			 pa, (u32)offs);
		ret = mtd_write(spl->mtd, offs, slice_size, &retlen, page_data);
		if (ret) {
			pr_err("Write SPL page %d failed.\n", pgidx);
			ret = -1;
			goto out;
		}
		// Read data to calc crc
		ret = mtd_read(spl->mtd, offs, slice_size, &retlen,
			       rd_page_data);
		if (ret) {
			pr_err("Read SPL page %d failed.\n", pgidx);
			ret = -1;
			goto out;
		}

		if (slice_size - data_size > 0)
			calc_len = fwc->meta.size % slice_size;
		else
			calc_len = slice_size;

		partition_crc = crc32(partition_crc, rd_page_data, calc_len);
#ifdef CONFIG_AICUPG_SINGLE_TRANS_BURN_CRC32_VERIFY
		trans_crc = crc32(trans_crc, page_data, calc_len);
		if (trans_crc != partition_crc) {
			pr_err("calc_len:%d\n", calc_len);
			pr_err("crc err at trans len %u\n", trans_size);
			pr_err("trans crc:0x%x, partition crc:0x%x\n",
			       trans_crc, partition_crc);
			ret = -1;
			goto out;
		}
#endif
		trans_size += data_size;
	}

	fwc->calc_trans_crc = trans_crc;
out:
	if (page_data)
		free(page_data);
	if (pt)
		free(pt);
	if (rd_page_data)
		free(rd_page_data);
	return ret;
}

static s32 verify_page_table(struct aicupg_nand_spl *spl, u32 blkidx,
			     struct nand_page_table *pt, u32 len)
{
	u8 page_data[PAGE_TABLE_USE_SIZE] = {0};
	struct mtd_oob_ops ops;
	struct mtd_info *mtd;
	loff_t offs;
	u32 sumval;
	s32 ret;

	mtd = spl->mtd;
	offs = mtd->erasesize * blkidx;

	memset(&ops, 0, sizeof(struct mtd_oob_ops));
	ops.len = PAGE_TABLE_USE_SIZE;
	ops.datbuf = page_data;
	//ops.mode = MTD_OPS_AUTO_OOB;

	ret = mtd_read_oob(mtd, offs, &ops);
	if (ret)
		return -1;

	//mtd_dump_buf(page_data, PAGE_TABLE_USE_SIZE);
	memcpy(pt, page_data, len);
	if (strncmp(pt->head.magic, "AICP", 4)) {
		pr_err("Page table magic check failed.\n");
		return -1;
	}
	sumval = calc_page_checksum(page_data, PAGE_TABLE_USE_SIZE);
	if (sumval != 0xFFFFFFFF) {
		pr_err("Page table in block %d checksum 0x%x verify failed.\n",
		       blkidx, sumval);
		return -1;
	}

	pr_debug("Page table verify is OK\n");
	return 0;
}

static s32 verify_image_page(struct aicupg_nand_spl *spl,
			     struct nand_page_table *pt)
{
	u8 *page_data;
	s32 ret = 0, i;
	size_t retlen;
	loff_t offs;
	u32 sumval, cksum;
	u32 pa, slice_size;

	page_data = malloc(PAGE_MAX_SIZE);
	if (!page_data) {
		pr_err("malloc page_data failed.\n");
		return -ENOMEM;
	}

	slice_size = spl->mtd->writesize;

	for (i = 1; i < pt->head.entry_cnt; i++) {
		if (pt->head.entry_cnt < PAGE_TABLE_MAX_ENTRY) {
			pa = pt->entry[i].pageaddr[0];
			cksum = pt->entry[i].checksum;
		} else {
			pa = pt->entry[i % PAGE_TABLE_MAX_ENTRY].pageaddr[1];
			cksum = pt->entry[i % PAGE_TABLE_MAX_ENTRY].pageaddr[2];
		}
		if (pa == SPL_INVALID_PAGE_ADDR)
			continue;
		offs = pa * spl->mtd->writesize;
		ret = mtd_read(spl->mtd, offs, slice_size, &retlen, page_data);
		if (ret) {
			pr_err("Read page %d, pa 0x%x failed. ret %d\n", i, pa,
			       ret);
			ret = -1;
			goto out;
		}
		sumval = calc_page_checksum(page_data, slice_size);
		sumval += cksum;
		if (sumval != 0xFFFFFFFF) {
			pr_err("Page %d, pa 0x%x checksum 0x%x is wrong.\n", i,
			       pa, sumval);
			ret = -1;
			goto out;
		}
	}

	pr_debug("SPL image verify is OK\n");
out:
	if (page_data)
		free(page_data);
	return ret;
}

static s32 nand_fwc_spl_image_verify(struct aicupg_nand_spl *spl)
{
	struct nand_page_table *pt = NULL;
	u32 blkidx;
	s32 ret;

	pt = malloc(PAGE_TABLE_USE_SIZE);
	pr_debug("Verifying SPL image data...\n");
	blkidx = spl->spl_blocks[0];
	if (blkidx == SPL_INVALID_BLOCK_IDX) {
		ret = -1;
		goto out;
	}
	ret = verify_page_table(spl, blkidx, pt, sizeof(*pt));
	if (ret) {
		pr_err("Page table in block %d is bad\n", blkidx);
		ret = -1;
		goto out;
	} else {
		pr_info("Page table in block %d is good\n", blkidx);
	}

	ret = verify_image_page(spl, pt);
out:
#ifdef CONFIG_ARTINCHIP_SPIENC
	spi_enc_tweak_select(AIC_SPIENC_USER_TWEAK);
#endif
	if (pt)
		free(pt);
	return ret;
}

s32 nand_fwc_spl_prepare(struct fwc_info *fwc)
{
	struct aicupg_nand_priv *priv;
	struct aicupg_nand_spl *spl;
	struct mtd_info *mtd;
	s32 ret;

	priv = (struct aicupg_nand_priv *)fwc->priv;

	if (!priv->mtds[0]) {
		pr_err("MTD is NULL\n");
		return -EINVAL;
	}
	/* Should operate the NAND device, not mtd partition */
	mtd = priv->mtds[0]->parent;
	if (!mtd) {
		pr_err("mtd0 %s, mtd 0x%lx\n", priv->mtds[0]->name,
		       (unsigned long)mtd);
		pr_err("%s, MTD device is NULL.\n", __func__);
		return -EINVAL;
	}

	spl = &g_nand_spl;

	if (spl->image_buf)
		free(spl->image_buf);
	memset(spl, 0, sizeof(struct aicupg_nand_spl));
	spl->mtd = mtd;
	spl->buf_size = ROUNDUP(fwc->meta.size, fwc->block_size);

	spl->image_buf = malloc(spl->buf_size);
	if (!spl->image_buf) {
		pr_err("Malloc SPL image buffer(%d) failed.\n",
		       spl->buf_size);
		return -ENOMEM;
	}
	memset(spl->image_buf, 0xFF, spl->buf_size);

	ret = nand_spl_get_good_blocks(spl);
	if (ret) {
		pr_err("No good blocks for SPL.\n");
		return -1;
	}

	ret = nand_spl_erase_image_blocks(spl);
	if (ret) {
		pr_err("spl erase image block failed.\n");
		return -1;
	}
#ifdef CONFIG_ARTINCHIP_SPIENC
	spi_enc_tweak_select(AIC_SPIENC_HW_TWEAK);
#endif
	return 0;
}

/*
 * Only write to RAM buffer, and begin to program NAND blocks when rx is
 * finished.
 */
s32 nand_fwc_spl_writer(struct fwc_info *fwc, u8 *buf, s32 len)
{
	struct aicupg_nand_spl *spl = &g_nand_spl;
	s32 ret;
	u8 *dst;

	if (spl->rx_size + len > spl->buf_size)
		return 0;
	dst = spl->image_buf + spl->rx_size;
	memcpy(dst, buf, len);
	spl->rx_size += len;

	if (spl->rx_size >= fwc->meta.size) {
		/* SPL image rx is finished, start to program */
		ret = nand_fwc_spl_program(fwc, spl);
		if (ret)
			return 0;
		ret = nand_fwc_spl_image_verify(spl);
		if (ret)
			return 0;
	}

	return len;
}


static int do_nand_writespl(struct cmd_tbl *cmdtp, int flag, int argc,
			     char *const argv[])
{
	unsigned long addr, len;
	struct fwc_info fwc;
	struct aicupg_nand_priv *priv;
	int ret = 0;

	if (argc < 3)
		return CMD_RET_USAGE;

	addr = hextoul(argv[1], NULL);
	len = hextoul(argv[2], NULL);

	priv = malloc(sizeof(struct aicupg_nand_priv));
	if (!priv) {
		pr_err("Out of memory, malloc failed.\n");
		ret = -1;
		goto out;
	}
	memset(priv, 0, sizeof(struct aicupg_nand_priv));
	priv->mtds[0] = get_mtd_device(NULL, 1);
	if (!priv->mtds[0]) {
		pr_err("Get MTD partition failed.\n");
		ret = -1;
		goto out;
	}
	fwc.meta.size = len;
	fwc.block_size = priv->mtds[0]->writesize;
	fwc.priv = priv;

	nand_fwc_spl_prepare(&fwc);
	nand_fwc_spl_writer(&fwc, (void *)addr, len);

out:
	if (priv)
		free(priv);
	return 0;
}

U_BOOT_CMD(nand_writespl, 3, 0, do_nand_writespl,
	"ArtInChip write SPL command for NAND",
	"nand_writespl <hex spl ram addr> <hex data len>\n"
);
