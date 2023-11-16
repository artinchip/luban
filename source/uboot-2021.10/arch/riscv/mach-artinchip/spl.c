// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ArtInChip Technology Co.,Ltd
 * Author: Dehuang Wu <dehuang.wu@artinchip.com>
 */

#include <common.h>
#include <cpu_func.h>
#include <dm.h>
#include <log.h>
#include <clk.h>
#include <blk.h>
#include <image.h>
#include <spl.h>
#include <hang.h>
#include <init.h>
#include <debug_uart.h>
#include <asm/arch/boot_param.h>

void __weak spl_board_init(void)
{
	pr_warn("Please implement spl_board_init for your board.\n");
}

u32 spl_boot_device(void)
{
	u32 bootdev = BOOT_DEVICE_NONE;
	enum boot_device bd;

	bd = aic_get_boot_device();
	switch (bd) {
	case BD_USB:
		bootdev = BOOT_DEVICE_RAM;
		break;
	case BD_SDMC0:
		bootdev = BOOT_DEVICE_MMC1;
		break;
	case BD_SDMC1:
	case BD_SDFAT32:
		bootdev = BOOT_DEVICE_MMC2;
		break;
	case BD_SPINOR:
		bootdev = BOOT_DEVICE_SPI;
		break;
	case BD_SPINAND:
		bootdev = BOOT_DEVICE_SPI;
		break;
	default:
		bootdev = BOOT_DEVICE_NONE;
		log_err("Boot device: Unknown, %d\n", (int)bd);
		break;
	}

	return bootdev;
}

u32 spl_mmc_boot_mode(const u32 boot_device)
{
	enum boot_device bd;

	bd = aic_get_boot_device();
	if (bd == BD_SDFAT32)
		return MMCSD_MODE_FS;

	return MMCSD_MODE_RAW;
}
