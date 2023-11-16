// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 ArtInChip Technology Co., Ltd
 */

#include <common.h>
#include <command.h>
#include <console.h>
#include <malloc.h>
#include <dm.h>
#include <hexdump.h>
#include <asm/io.h>
#include <linux/delay.h>

static int do_mmcspeed_test(struct cmd_tbl *cmdtp, int flag, int argc,
			    char *const argv[])
{
	long start, cur, tm;
	unsigned long addr, startblk, blkcnt;
	unsigned long speed1, speed2, speed3;
	char *pe, cmdbuf[64];
	int ret;

	if (argc != 4)
		return CMD_RET_USAGE;

	addr = ustrtoul(argv[1], &pe, 0);
	startblk = ustrtoul(argv[2], &pe, 0);
	blkcnt = ustrtoul(argv[3], &pe, 0);

	/* Test read speed */
	sprintf(cmdbuf, "mmc read 0x%lx 0x%lx 0x%lx", addr, startblk, blkcnt);
	start = timer_get_us();
	ret = run_command(cmdbuf, 0);
	cur = timer_get_us();
	if (ret)
		return ret;

	tm = (cur - start) / 1000;
	speed1 = (blkcnt * 512 * 1000) / tm;
	speed2 = speed1 / 1024;
	speed3 = speed2 / 1024;
	printf("Read  test time %ld us, speed %lu B/s(%lu KB/s %lu MB/s)\n", tm,
	       speed1, speed2, speed3);

	/* Test write speed */
	sprintf(cmdbuf, "mmc write 0x%lx 0x%lx 0x%lx", addr, startblk, blkcnt);
	start = timer_get_us();
	ret = run_command(cmdbuf, 0);
	cur = timer_get_us();
	if (ret)
		return ret;
	tm = (cur - start) / 1000;
	speed1 = (blkcnt * 512 * 1000) / tm;
	speed2 = speed1 / 1024;
	speed3 = speed2 / 1024;
	printf("Write test time %ld us, speed %lu B/s(%lu KB/s %lu MB/s)\n", tm,
	       speed1, speed2, speed3);

	/* Test erase speed */
	start = timer_get_us();
	sprintf(cmdbuf, "mmc erase 0x%lx 0x%lx", startblk, blkcnt);
	ret = run_command(cmdbuf, 0);
	cur = timer_get_us();
	if (ret)
		return ret;
	tm = (cur - start) / 1000;
	speed1 = (blkcnt * 512 * 1000) / tm;
	speed2 = speed1 / 1024;
	speed3 = speed2 / 1024;
	printf("Erase test time %ld us, speed %lu B/s(%lu KB/s %lu MB/s)\n", tm,
	       speed1, speed2, speed3);

	return 0;
}

static char mmcspeed_help_text[] =
	"mmcspeed addr start_blk blkcnt\n"
	"e.g.:\n"
	"mmcspeed 0x40200000 0x8000 0x800\n";

U_BOOT_CMD(mmcspeed, 4, 0, do_mmcspeed_test, "MMC Speed test command",
	   mmcspeed_help_text);
