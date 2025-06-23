// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 ArtInChip Technology Co., Ltd
 */

#include <common.h>
#include <command.h>
#include <console.h>
#include <malloc.h>
#include <env.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <asm/arch/usb_detect.h>
#include <userid.h>
#include <dt-bindings/gpio/gpio.h>

#include <dm.h>
#include <fdtdec.h>
#include <fdt_support.h>
#include <asm/global_data.h>

DECLARE_GLOBAL_DATA_PTR;

__weak int aic_upg_mode_detect(void)
{
	return 0;
}
__weak void aic_upg_flag_clear(void)
{
}

__weak void aic_upg_succ_cnt(void)
{
}

#ifndef CONFIG_SPL_BUILD /* In U-Boot */
static int check_sd_fat32_upg(void)
{
	char *p;

	p = env_get("boot_device");
	if (!p)
		return 0;
	if (!strcmp(p, "fat"))
		return 1;
	return 0;
}

static int check_usb_upg(void)
{
	char *p;

	p = env_get("boot_device");
	if (!p)
		return 0;
	if (!strcmp(p, "usb"))
		return 1;

	if (aic_upg_mode_detect())
		return 1;
	return 0;
}

#ifdef CONFIG_USERID_SUPPORT
static int usb_burn_userid_mode(void)
{
	int ret;
	u32 flag = 0;

	ret = userid_read("lock", 0, (void *)&flag, 4);

	if (ret <= 0) {
		/* userid lock is not exist */
		return 1;
	}
	if (flag == 0) {
		/* userid is in unlocked state */
		return 1;
	}
	/* userid is in locked state, don't goto burn userid mode agian */
	return 0;
}
#endif

static int do_upgrade_detect(struct cmd_tbl *cmdtp, int flag, int argc,
			     char *const argv[])
{
	int ret = 0;

	env_set("upg_type", "");
	if (check_usb_upg()) {
		aic_upg_flag_clear();
		ret = 1;
		env_set("upg_type", "usb");
		goto out;
	}

	if (check_sd_fat32_upg()) {
		ret = 1;
		env_set("upg_type", "sdcard");
		goto out;
	}

#ifdef CONFIG_UPDATE_UDISK_FATFS_ARTINCHIP
	if (usb_host_udisk_connection_check()) {
		ret = 1;
		env_set("upg_type", "udisk");
		goto out;
	}
#endif
#ifdef CONFIG_USERID_SUPPORT
	if (usb_burn_userid_mode()) {
		ret = 1;
		env_set("upg_type", "usb");
		env_set("upg_mode", "userid");
		goto out;
	}
#endif
#ifdef CONFIG_AICUPG_FORCE_USBUPG_SUPPORT
	/* Force USB upgrading mode, jump into USB loop and checking
	 * This checking should behind of USERID checking
	 */
	{
		ret = 1;
		env_set("upg_type", "usb");
		env_set("upg_mode", "force");
		goto out;
	}
#endif

out:
	return ret;
}

U_BOOT_CMD(upg_detect, 1, 0, do_upgrade_detect,
	"ArtInChip upgrade detect command",
	"Return code:\n"
	"  - 0: No need to enter upgrade mode\n"
	"  - 1: Need to enter upgrade mode\n"
);

#endif
