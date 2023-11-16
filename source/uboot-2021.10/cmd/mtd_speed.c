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
#include <mtd.h>
#include <linux/delay.h>

static int do_mtdspeed_test(struct cmd_tbl *cmdtp, int flag, int argc,
			    char *const argv[])
{
	long start_tm, cur_tm, tm;
	unsigned long i, addr, start, size;
	unsigned long speed1, speed2, speed3;
	char *part, *pe, cmdbuf[64];
	u8 *p;
	int ret;

	if (argc != 5)
		return CMD_RET_USAGE;

	part = argv[1];
	addr = ustrtoul(argv[2], &pe, 0);
	start = ustrtoul(argv[3], &pe, 0);
	size = ustrtoul(argv[4], &pe, 0);

	/* Test write speed */
	p = (u8 *)addr;
	for (i = 0; i < size; i++)
		p[i] = i % 256;

	sprintf(cmdbuf, "mtd write %s 0x%lx 0x%lx 0x%lx", part, addr, start,
		size);
	start_tm = timer_get_us();
	ret = run_command(cmdbuf, 0);
	cur_tm = timer_get_us();
	if (ret)
		return ret;
	tm = (cur_tm - start_tm) / 1000;

	speed1 = (size * 1000) / tm;
	speed2 = speed1 / 1024;
	speed3 = speed2 / 1024;
	printf("Write test time %ld us, speed %lu B/s(%lu KB/s %lu MB/s)\n", tm,
	       speed1, speed2, speed3);

	/* Test read speed */
	sprintf(cmdbuf, "mtd read %s 0x%lx 0x%lx 0x%lx", part, addr, start,
		size);
	start_tm = timer_get_us();
	ret = run_command(cmdbuf, 0);
	cur_tm = timer_get_us();
	if (ret)
		return ret;
	tm = (cur_tm - start_tm) / 1000;

	speed1 = (size * 1000) / tm;
	speed2 = speed1 / 1024;
	speed3 = speed2 / 1024;
	printf("Read  test time %ld ms, speed %lu B/s(%lu KB/s %lu MB/s)\n", tm,
	       speed1, speed2, speed3);

	/* Test Erase speed */
	start_tm = timer_get_us();
	sprintf(cmdbuf, "mtd erase %s 0x%lx 0x%lx", part, start, size);
	ret = run_command(cmdbuf, 0);
	cur_tm = timer_get_us();
	tm = (cur_tm - start_tm) / 1000;

	speed1 = (size * 1000) / tm;
	speed2 = speed1 / 1024;
	speed3 = speed2 / 1024;
	printf("Erase test time %ld ms, speed %lu B/s(%lu KB/s %lu MB/s)\n", tm,
	       speed1, speed2, speed3);
	return 0;
}

static char mtdspeed_help_text[] =
	"mtdspeed part addr start size\n"
	"e.g.:\n"
	"  mtd list\n"
	"  mtdspeed kernel 0x40200000 0x8000 0x800\n";

U_BOOT_CMD(mtdspeed, 5, 0, do_mtdspeed_test, "MTD Speed test command",
	   mtdspeed_help_text);
