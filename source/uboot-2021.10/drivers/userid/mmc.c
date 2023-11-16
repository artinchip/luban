// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2023 ArtInChip Technology Co.,Ltd.
 */

#include <common.h>
#include <dm.h>
#include <log.h>
#include <part.h>
#include <spl.h>
#include <linux/compiler.h>
#include <errno.h>
#include <asm/u-boot.h>
#include <errno.h>
#include <malloc.h>
#include <memalign.h>
#include <mmc.h>
#include <image.h>
#include <userid.h>
#include "userid_internal.h"

static int mmc_userid_load(void)
{
	struct userid_storage_header head;
	static struct mmc *mmc;
	struct blk_desc *mmc_blk;
	int ret = -1, offset, cnt;
	u8 *buf;

	mmc = find_mmc_device(0);
	if (!mmc)
		return ret;

	mmc_blk = mmc_get_blk_desc(mmc);

	buf = memalign(ARCH_DMA_MINALIGN, CONFIG_USERID_SIZE);
	if (!buf) {
		ret = -ENOMEM;
		goto err;
	}

	offset = CONFIG_USERID_OFFSET / mmc_blk->blksz;
	cnt = 1;
	ret = blk_dread(mmc_blk, offset, cnt, buf);
	if (ret != cnt) {
		pr_err("read userid from lba offset 0x%x failed.\n", offset);
		ret = -EIO;
		goto err;
	}

	memcpy(&head, buf, sizeof(head));
	if (head.magic != USERID_HEADER_MAGIC)
		goto err;

	offset += 1;
	cnt = CONFIG_USERID_SIZE / mmc_blk->blksz - 1;
	ret = blk_dread(mmc_blk, offset, cnt, (buf + mmc_blk->blksz));
	if (ret != cnt) {
		pr_err("read userid from lba offset 0x%x failed.\n", offset);
		ret = -EIO;
		goto err;
	}

	ret = userid_import(buf);

err:
	if (buf)
		free(buf);
	return ret;
}

#ifndef CONFIG_SPL_BUILD
static int mmc_userid_save(void)
{
	static struct mmc *mmc;
	struct blk_desc *mmc_blk;
	int ret = -1;
	u32 len, offset, cnt;
	u8 *buf;

	mmc = find_mmc_device(0);
	if (!mmc)
		return ret;

	mmc_blk = mmc_get_blk_desc(mmc);

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

	pr_info("Writing ...\n");
	offset = CONFIG_USERID_OFFSET / mmc_blk->blksz;
	cnt = DIV_ROUND_UP(len, mmc_blk->blksz);
	ret = blk_dwrite(mmc_blk, offset, cnt, buf);
	if (ret != cnt) {
		pr_err("mmc: save userid failed.\n");
		ret = -1;
		goto err;
	}

err:
	if (buf)
		free(buf);
	return 0;
}
#endif

U_BOOT_USERID_LOCATION(mmc) = {
	.name	= "mmc",
	.load	= mmc_userid_load,
#ifndef CONFIG_SPL_BUILD
	.save	= mmc_userid_save,
#endif
};
