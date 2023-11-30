// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2020 ArtInChip Technology Co.,Ltd.
 * Dehuang Wu <dehuang.wu@artinchip.com>
 */

#include <common.h>
#include <generated/autoconf.h>
#include <command.h>
#include <dm.h>
#include <dm/uclass.h>
#include <dm/device-internal.h>
#include <env_internal.h>
#include <linux/stddef.h>
#include <malloc.h>
#include <memalign.h>
#include <search.h>
#include <errno.h>
#include <mtd.h>
#include <env.h>

#ifdef CONFIG_AUTO_CALCULATE_PART_CONFIG
#include <generated/image_cfg_part_config.h>
#endif

#if defined(CONFIG_CMD_SAVEENV) && !defined(CONFIG_SPL_BUILD)
#define CMD_SAVEENV
#endif
#ifndef CONFIG_ENV_RANGE
#define CONFIG_ENV_RANGE	CONFIG_ENV_SIZE
#endif

DECLARE_GLOBAL_DATA_PTR;

static int readenv(struct mtd_info *mtd, size_t offset, u_char *buf)
{
	size_t end = offset + CONFIG_ENV_RANGE;
	size_t rdlen, remain;
	u_char *char_ptr;

	remain = min(mtd->erasesize, (uint32_t)CONFIG_ENV_SIZE);
	char_ptr = buf;

	while ((remain > 0) && (offset < end)) {
		if (mtd_block_isbad(mtd, offset)) {
			offset += mtd->erasesize;
			continue;
		}

		if (mtd_read(mtd, offset, remain, &rdlen, char_ptr))
			return 1;

		offset += rdlen;
		remain -= rdlen;
		char_ptr += rdlen;
	}

	return 0;
}

#if defined(CMD_SAVEENV)
static int writeenv(struct mtd_info *mtd, size_t offset, u_char *buf)
{
	size_t end = offset + CONFIG_ENV_RANGE;
	size_t wrlen, remain;
	u_char *char_ptr;

	remain = min(mtd->erasesize, (uint32_t)CONFIG_ENV_SIZE);
	char_ptr = buf;

	while ((remain > 0) && (offset < end)) {
		if (mtd_block_isbad(mtd, offset)) {
			offset += mtd->erasesize;
			continue;
		}

		if (mtd_write(mtd, offset, remain, &wrlen, char_ptr))
			return 1;

		offset += wrlen;
		remain -= wrlen;
		char_ptr += wrlen;
	}

	return 0;
}
#endif

#if defined(CONFIG_ENV_OFFSET_REDUND)
static int env_spinand_load(void)
{
	struct mtd_info *mtd;
	struct udevice *dev;
	int ret = -1;
	int read1_fail = 0, read2_fail = 0;

	ALLOC_CACHE_ALIGN_BUFFER(char, buf, CONFIG_ENV_SIZE);
	ALLOC_CACHE_ALIGN_BUFFER(char, buf_redund, CONFIG_ENV_SIZE);

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

	read1_fail = readenv(mtd, CONFIG_ENV_OFFSET, (u_char *)buf);
	if (read1_fail)
		pr_info("\n** Unable to read env from %d **\n",
			CONFIG_ENV_OFFSET);
	read2_fail = readenv(mtd, CONFIG_ENV_OFFSET_REDUND,
			     (u_char *)buf_redund);
	if (read2_fail)
		pr_info("\n** Unable to read redundant env from %d **\n",
			CONFIG_ENV_OFFSET_REDUND);
	ret = env_import_redund((char *)buf, read1_fail, (char *)buf_redund,
				read2_fail, H_EXTERNAL);

	return ret;
}

#if defined(CMD_SAVEENV)
static int env_spinand_save(void)
{
	struct mtd_info *mtd;
	struct udevice *dev;
	env_t env_new;
	int ret = -1;
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

	ret = env_export(&env_new);
	if (ret)
		return ret;

	if (gd->env_valid == ENV_VALID) {
		pr_info("Writing to env_redundant mtd... ");
		instr.len = CONFIG_ENV_SIZE;
		instr.addr = CONFIG_ENV_OFFSET_REDUND;
		instr.mtd = mtd;
		pr_info("Erasing ...\n");
		if (mtd_erase(mtd, &instr)) {
			pr_err("SPINAND: erase failed at 0x%08llx\n",
			       instr.addr);
			return 1;
		}

		pr_info("Writing ...\n");
		ret = writeenv(mtd, CONFIG_ENV_OFFSET_REDUND,
			       (u_char *)&env_new);
		if (ret) {
			pr_err("SPINAND: save env failed.\n");
			return 2;
		}
	} else {
		puts("Writing to env mtd... ");
		instr.len = CONFIG_ENV_SIZE;
		instr.addr = CONFIG_ENV_OFFSET;
		instr.mtd = mtd;
		pr_info("Erasing ...\n");
		if (mtd_erase(mtd, &instr)) {
			pr_err("SPINAND: erase failed at 0x%08llx\n",
			       instr.addr);
			return 1;
		}

		pr_info("Writing ...\n");
		ret = writeenv(mtd, CONFIG_ENV_OFFSET, (u_char *)&env_new);
		if (ret) {
			pr_err("SPINAND: save env failed.\n");
			return 2;
		}
	}

	pr_info("Done ...\n");

	gd->env_valid = gd->env_valid == ENV_REDUND ? ENV_VALID : ENV_REDUND;

	return 0;
}
#endif

#else
static int env_spinand_load(void)
{
	struct mtd_info *mtd;
	struct udevice *dev;
	int ret = -1;

	ALLOC_CACHE_ALIGN_BUFFER(char, buf, CONFIG_ENV_SIZE);

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

	ret = readenv(mtd, CONFIG_ENV_OFFSET, (u_char *)buf);
	if (ret) {
		env_set_default("readenv() failed", 0);
		pr_err("read environment from offset 0x%x to 0x%lx failed.\n",
		       CONFIG_ENV_OFFSET, (unsigned long)buf);
		return -EIO;
	}

	ret = env_import(buf, 1, H_EXTERNAL);
	if (!ret)
		gd->env_valid = ENV_VALID;

	return ret;
}

#if defined(CMD_SAVEENV)
static int env_spinand_save(void)
{
	struct mtd_info *mtd;
	struct udevice *dev;
	env_t env_new;
	int ret = -1;
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

	ret = env_export(&env_new);
	if (ret)
		return ret;

	instr.len = CONFIG_ENV_SIZE;
	instr.addr = CONFIG_ENV_OFFSET;
	instr.mtd = mtd;
	pr_info("Erasing ...\n");
	if (mtd_erase(mtd, &instr)) {
		pr_err("SPINAND: erase failed at 0x%08llx\n", instr.addr);
		return 1;
	}

	pr_info("Writing ...\n");
	ret = writeenv(mtd, CONFIG_ENV_OFFSET, (u_char *)&env_new);
	if (ret) {
		pr_err("SPINAND: save env failed.\n");
		return 2;
	}

	return 0;
}
#endif
#endif

U_BOOT_ENV_LOCATION(spinand) = {
	.location	= ENVL_SPINAND,
	ENV_NAME("SPINAND")
	.load		= env_spinand_load,
#if defined(CMD_SAVEENV)
	.save		= env_save_ptr(env_spinand_save),
#endif
};
