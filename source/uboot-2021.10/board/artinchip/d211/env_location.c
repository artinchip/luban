// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ArtInChip Technology Co.,Ltd
 */

#include <common.h>
#include <command.h>
#include <asm/global_data.h>
#include <env.h>
#include <env_internal.h>
#include <asm/arch/boot_param.h>
#include <linux/stddef.h>

DECLARE_GLOBAL_DATA_PTR;

enum env_location env_get_location(enum env_operation op, int prio)
{
	enum env_location loc = ENVL_UNKNOWN;
	enum boot_device bd;

	if (prio)
		return ENVL_UNKNOWN;

	bd = aic_get_boot_device();
	switch (bd) {
	case BD_SDMC0:
		debug("Load env from eMMC...\n");
#ifdef CONFIG_ENV_IS_IN_MMC
		loc = ENVL_MMC;
#endif
		break;
	case BD_SDMC1:
		debug("Load env from SD Card...\n");
#ifdef CONFIG_ENV_IS_IN_MMC
		loc = ENVL_MMC;
#endif
		break;
	case BD_SDFAT32:
		debug("Load env from SD Card FAT32...\n");
#ifdef CONFIG_ENV_IS_IN_FAT
		loc = ENVL_FAT;
#endif
		break;
	case BD_SPINOR:
		debug("Load env from SPI NOR...\n");
#ifdef CONFIG_ENV_IS_IN_FLASH
		loc = ENVL_FLASH;
#endif
#ifdef CONFIG_ENV_IS_IN_SPI_FLASH
		loc = ENVL_SPI_FLASH;
#endif
		break;
	case BD_SPINAND:
		debug("Load env from SPI NAND...\n");
#ifdef CONFIG_ENV_IS_IN_UBI
		loc = ENVL_UBI;
#endif
#ifdef CONFIG_ENV_IS_IN_SPINAND
		loc = ENVL_SPINAND;
#endif
		break;
	case BD_USB:
		debug("Load env from RAM...\n");
		loc = ENVL_RAM;
		break;
	default:
		return ENVL_UNKNOWN;
	}
	return loc;
}

#if defined(CONFIG_ENV_IS_IN_RAM) && !defined(CONFIG_SPL_BUILD)
/*
 * It is used by AIC USB upgrading
 *
 * According AIC USB upgrading protocol, during BOOT ROM stage, Host PC
 * download env.bin to RAM, then download U-Boot and run it.
 *
 * Firmware is written to storage device by U-Boot, U-Boot need to read
 * some settings in environment, here add environment location driver to
 * help U-Boot import environment from RAM buffer.
 */
static int env_ram_load(void)
{
	char *buf;

	buf = (char *)CONFIG_ENV_RAM_ADDR;
	return env_import(buf, 1, H_EXTERNAL);
}

U_BOOT_ENV_LOCATION(ram) = {
	ENV_NAME("RAM")
	.location	= ENVL_RAM,
	.load		= env_ram_load,
};
#endif
