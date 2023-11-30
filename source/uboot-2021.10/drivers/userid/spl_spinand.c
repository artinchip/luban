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
#include <linux/mtd/spinand.h>
#include <userid.h>
#include <artinchip_spinand.h>
#include "userid_internal.h"

#ifdef CONFIG_AUTO_CALCULATE_PART_CONFIG
#include <generated/image_cfg_part_config.h>
#endif

#if defined(CONFIG_SPL_BUILD) && defined(CONFIG_SPL_SPI_NAND_TINY)

static bool aligned_with_block_size(u64 erasesize, u64 size)
{
	return !do_div(size, erasesize);
}

static int read_userid(struct spinand_device *spinand, size_t offset, u8 *buf,
		       size_t size)
{
	struct nand_device *nand;
	size_t end = offset + size;
	size_t rdlen, remain;
	u8 *ptr;

	nand = spinand_to_nand(spinand);
	remain = size;
	ptr = buf;

	while ((remain > 0) && (offset < end)) {
		if (aligned_with_block_size(nand->info->erasesize, offset) &&
		    spinand_block_isbad(spinand, offset)) {
			offset += nand->info->erasesize;
			continue;
		}

		if (spinand_read(spinand, offset, remain, &rdlen, ptr))
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
	struct spinand_device *spinand;
	struct nand_device *nand;
	size_t offset, size, head_len;
	int ret = -1;
	u8 *buf;

	spinand = spl_spinand_init();
	if (IS_ERR_OR_NULL(spinand)) {
		pr_err("Tiny SPI NAND init failed., ret = %ld\n", PTR_ERR(spinand));
		return -ENODEV;
	}

	nand = spinand_to_nand(spinand);

	buf = memalign(ARCH_DMA_MINALIGN, CONFIG_USERID_SIZE);
	if (!buf) {
		ret = -ENOMEM;
		goto err;
	}

	offset = CONFIG_USERID_OFFSET;
	head_len = 4096;
	size = head_len;
	ret = read_userid(spinand, offset, buf, size);
	if (ret) {
		pr_err("read userid from offset 0x%lx failed.\n", offset);
		ret = -EIO;
		goto err;
	}

	memcpy(&head, buf, sizeof(head));
	if (head.magic != USERID_HEADER_MAGIC)
		goto err;

	if ((head.total_length + 8) > head_len) {
		offset += head_len;
		size = CONFIG_USERID_SIZE - head_len;
		ret = read_userid(spinand, offset, (buf + head_len), size);
		if (ret) {
			pr_err("read userid from offset 0x%lx failed.\n", offset);
			ret = -EIO;
			goto err;
		}
	}

	ret = userid_import(buf);

err:
	if (buf)
		free(buf);
	return ret;
}

U_BOOT_USERID_LOCATION(spinand) = {
	.name	= "SPINAND",
	.load	= spinand_userid_load,
};
#endif
