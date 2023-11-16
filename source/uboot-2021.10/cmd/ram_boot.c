// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2023 ArtInChip Technology Co., Ltd
 */

#include <common.h>
#include <command.h>
#include <console.h>
#include <malloc.h>
#include <g_dnl.h>
#include <usb.h>
#include <env.h>
#include <artinchip/aicupg.h>
#include <artinchip/artinchip_fb.h>
#include <asm/arch/boot_param.h>
#include <config_parse.h>
#include <dm/uclass.h>


static int do_ram_boot(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	int ret = CMD_RET_USAGE;
	char addrstr[32], cmdstr[32], *p;
	unsigned long addr;

	if (argc > 2) {
		if (strcmp("spi-nand", argv[2]) == 0) {
			env_set("boot_device", "nand");
			sprintf(cmdstr, "run nand_boot");
		}
		if (strcmp("spi-nor", argv[2]) == 0) {
			env_set("boot_device", "nor");
			sprintf(cmdstr, "run nor_boot");
		}
		if (strcmp("mmc", argv[2]) == 0) {
			env_set("boot_device", "mmc");
			sprintf(cmdstr, "run mmc_boot");
		}
	}
	if (argc > 3)
		env_set("boot_devnum", argv[3]);

	addr = ustrtoul((char *)argv[1], &p, 0);
	sprintf(addrstr, "0x%lx", addr);
	env_set("knl_addr", addrstr);
	env_set("ramboot", "yes");

	printf("ram_boot for debug.\n");
	ret = run_command(cmdstr, 0);
	return ret;
}

U_BOOT_CMD(ram_boot, 4, 0, do_ram_boot,
	"Boot kernel from RAM",
	"ram_boot <addr> [boot device] [device id]\n"
);
