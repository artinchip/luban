// SPDX-License-Identifier: GPL-2.0+
/*
 * Save kernel device tree snapshot to spinand/spinor/eMMC falcon part
 *
 * Authors:
 * Copyright (c) 2022, ArtInChip Technology Co., Ltd
 * Author: Xuan Wen <xuan.wen@artinchip.com>
 */

#include <common.h>
#include <command.h>
#include <asm/arch/boot_param.h>

#define FALCON_SAVE_MTD							\
	"if test ${boot_os} != yes; then "				\
		"spl export fdt ${knl_addr}; "				\
		"mtd erase falcon; "					\
		"mtd write falcon ${fdtargsaddr} 0 ${fdtargslen}; "	\
		"setenv boot_os yes; "					\
		"saveenv; "						\
	"fi; "

#define FALCON_SAVE_MMC							\
	"if test ${boot_os} != yes; then "				\
		"spl export fdt ${knl_addr}; "				\
		"part start mmc ${boot_devnum} falcon falcon_lba; "	\
		"setexpr fdtargscnt ${fdtargslen} / 0x200; "		\
		"mmc dev ${boot_devnum}; "				\
		"mmc write ${fdtargsaddr} ${falcon_lba} ${fdtargscnt}; "\
		"setenv boot_os yes; "					\
		"saveenv; "						\
	"fi; "

#ifdef CONFIG_SPL_OS_BOOT
static int do_falcon_save(struct cmd_tbl *cmdtp, int flag,
			  int argc, char *const argv[])
{
	int ret = 0;
	enum boot_device bd;
	char *ramboot;

	/* Don't perform falcon save if it is ramboot */
	ramboot = env_get("ramboot");
	if (ramboot)
		return ret;

	bd = aic_get_boot_device();
	switch (bd) {
	case BD_SDMC0:
	case BD_SDMC1:
		ret = run_command(FALCON_SAVE_MMC, 0);
		break;
	case BD_SPINOR:
	case BD_SPINAND:
		ret = run_command(FALCON_SAVE_MTD, 0);
		break;
	default:
		break;
	}

	return ret;
}
#else
static int do_falcon_save(struct cmd_tbl *cmdtp, int flag,
			  int argc, char *const argv[])
{
	return CMD_RET_SUCCESS;
}
#endif

U_BOOT_CMD(falcon_save,	1,	0,	do_falcon_save,
	   "Save kernel device tree snapshot to spinand/spinor/eMMC falcon part",
	   "\n"
);

