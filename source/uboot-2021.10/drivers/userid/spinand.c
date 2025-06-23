// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2023 ArtInChip Technology Co.,Ltd.
 */

#include <common.h>
#include <generated/autoconf.h>
#include <command.h>
#include <dm.h>
#include <dm/uclass.h>
#include <dm/device-internal.h>
#include <linux/stddef.h>
#include <malloc.h>
#include <memalign.h>
#include <errno.h>
#include <mtd.h>
#include <userid.h>
#include "userid_internal.h"

#ifdef CONFIG_AUTO_CALCULATE_PART_CONFIG
#include <generated/image_cfg_part_config.h>
#endif

#ifndef CONFIG_SPL_BUILD

static bool mtd_is_aligned_with_block_size(struct mtd_info *mtd, u64 size)
{
	return !do_div(size, mtd->erasesize);
}

static int read_userid(struct mtd_info *mtd, size_t offset, u8 *buf,
		       size_t size)
{
	size_t end = offset + size;
	size_t rdlen, remain;
	u8 *ptr;

	remain = min(mtd->erasesize, (uint32_t)CONFIG_USERID_SIZE);
	ptr = buf;

	while ((remain > 0) && (offset < end)) {
		if (mtd_is_aligned_with_block_size(mtd, offset) &&
		    mtd_block_isbad(mtd, offset)) {
			offset += mtd->erasesize;
			continue;
		}

		if (mtd_read(mtd, offset, remain, &rdlen, ptr))
			return 1;

		offset += rdlen;
		remain -= rdlen;
		ptr += rdlen;
	}

	return 0;
}

static int spinand_userid_load(void)
{
	struct userid_storage_header head;
	struct mtd_info *mtd;
	struct udevice *dev;
	size_t offset, size;
	int ret = -1;
	u8 *buf;

	ret = uclass_first_device(UCLASS_MTD, &dev);
	if (ret && !dev) {
		pr_err("Find MTD device failed.\n");
		return -ENODEV;
	}

	device_probe(dev);
	mtd = get_mtd_device(NULL, 0);
	if (!mtd) {
		pr_err("Get SPI NAND mtd device failed.\n");
		return -ENODEV;
	}

	buf = memalign(ARCH_DMA_MINALIGN, CONFIG_USERID_SIZE);
	if (!buf) {
		ret = -ENOMEM;
		goto err;
	}

	offset = CONFIG_USERID_OFFSET;
	size = mtd->writesize;
	ret = read_userid(mtd, offset, buf, size);
	if (ret) {
		pr_err("read userid from offset 0x%lx failed.\n", offset);
		ret = -EIO;
		goto err;
	}

	memcpy(&head, buf, sizeof(head));
	if (head.magic != USERID_HEADER_MAGIC)
		goto err;

	offset += mtd->writesize;
	size = CONFIG_USERID_SIZE - mtd->writesize;
	ret = read_userid(mtd, offset, (buf + mtd->writesize), size);
	if (ret) {
		pr_err("read userid from offset 0x%lx failed.\n", offset);
		ret = -EIO;
		goto err;
	}

	ret = userid_import(buf);

err:
	if (buf)
		free(buf);
	return ret;
}

static int write_userid(struct mtd_info *mtd, size_t offset, u8 *buf, u32 len)
{
	size_t end = offset + CONFIG_USERID_SIZE;
	size_t wrlen, remain;
	u8 *ptr;

	remain = len;
	ptr = buf;

	while ((remain > 0) && (offset < end)) {
		if (mtd_block_isbad(mtd, offset)) {
			offset += mtd->erasesize;
			continue;
		}

		if (mtd_write(mtd, offset, remain, &wrlen, ptr))
			return 1;

		offset += wrlen;
		remain -= wrlen;
		ptr += wrlen;
	}

	return 0;
}

static int spinand_userid_save(void)
{
	struct mtd_info *mtd;
	struct udevice *dev;
	int ret = -1;
	u32 len;
	u8 *buf;
	struct erase_info instr = {
		.callback	= NULL,
	};

	ret = uclass_first_device(UCLASS_MTD, &dev);
	if (ret && !dev) {
		pr_err("Find MTD device failed.\n");
		return -ENODEV;
	}

	device_probe(dev);
	mtd = get_mtd_device(NULL, 0);
	if (!mtd) {
		pr_err("Get SPI NAND mtd device failed.\n");
		return -ENODEV;
	}

	buf = memalign(ARCH_DMA_MINALIGN, CONFIG_USERID_SIZE);
	if (!buf) {
		pr_err("Failed to malloc buffer.\n");
		ret = -ENOMEM;
		goto err;
	}
	memset(buf, 0xff, CONFIG_USERID_SIZE);
	ret = userid_export(buf);
	if (ret <= 0) {
		pr_err("Failed to export userid to buffer.\n");
		goto err;
	}
	len = ret;

	instr.len = CONFIG_USERID_SIZE;
	instr.addr = CONFIG_USERID_OFFSET;
	instr.mtd = mtd;
	pr_info("Erasing ...\n");
	if (mtd_erase(mtd, &instr)) {
		pr_err("SPINAND: erase failed at 0x%08llx\n", instr.addr);
		ret = 1;
		goto err;
	}

	pr_info("Writing ...\n");
	ret = write_userid(mtd, CONFIG_USERID_OFFSET, buf, len);
	if (ret) {
		pr_err("SPINAND: save userid failed.\n");
		ret = 2;
		goto err;
	}

err:
	if (buf)
		free(buf);
	return 0;
}

U_BOOT_USERID_LOCATION(spinand) = {
	.name	= "SPINAND",
	.load	= spinand_userid_load,
	.save	= spinand_userid_save,
};
#endif
