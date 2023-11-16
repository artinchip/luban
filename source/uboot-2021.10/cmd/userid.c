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
#include <artinchip/aicupg.h>
#include <config_parse.h>
#include <dm/uclass.h>
#include <hexdump.h>
#include <userid.h>

static int do_userid_list(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	int i, cnt, ret;
	char name[256];

	cnt = userid_get_count();
	printf("ID count %d\n", cnt);
	for (i = 0; i < cnt; i++) {
		memset(name, 0, 256);
		ret = userid_get_name(i, name, 255);
		printf("  %s\n", name);
		if (ret <= 0)
			continue;
	}
	return 0;
}

static int do_userid_init(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	int ret;

	ret = userid_init();
	if (ret)
		printf("UserID initialization failed.\n");

	return 0;
}

static int do_userid_dump(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	unsigned long size, offset;
	char *name, *pe;
	u8 *p;
	int rdcnt;

	if (argc != 4 && argc != 2)
		return CMD_RET_USAGE;
	name = argv[1];
	if (argc == 2) {
		offset = 0;
		size = userid_get_data_length(name);
	} else {
		offset = ustrtoul(argv[2], &pe, 0);
		size = ustrtoul(argv[3], &pe, 0);
	}

	p = malloc(size);
	rdcnt = userid_read(name, offset, p, size);
	if (rdcnt > 0)
		print_hex_dump("", DUMP_PREFIX_OFFSET, 16, 1, p, rdcnt, true);
	else
		pr_err("Read failed. ret = %d\n", rdcnt);
	free(p);
	return 0;
}

static int do_userid_size(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	unsigned long addr;
	char *name, *pe;
	int size;
	u32 *p;

	if (argc != 3 && argc != 2)
		return CMD_RET_USAGE;
	name = argv[1];
	size = userid_get_data_length(name);
	if (size < 0)
		size = 0;
	if (argc == 2) {
		printf("%d\n", size);
	} else {
		addr = ustrtoul(argv[2], &pe, 0);
		p = (u32 *)addr;
		*p = size;
	}

	return 0;
}

static int do_userid_remove(struct cmd_tbl *cmdtp, int flag, int argc,
			    char *const argv[])
{
	int ret;
	char *name;

	if ((argc != 2))
		return CMD_RET_USAGE;
	name = argv[1];
	ret = userid_remove(name);
	if (ret) {
		pr_err("Failed to remove %s\n", name);
		return -1;
	}
	return 0;
}

static int do_userid_import(struct cmd_tbl *cmdtp, int flag, int argc,
			    char *const argv[])
{
	unsigned long addr;
	char *pe;
	int ret;

	if ((argc != 2))
		return CMD_RET_USAGE;
	addr = ustrtoul(argv[1], &pe, 0);
	ret = userid_import((void *)addr);
	if (ret) {
		pr_err("Failed to import from 0x%lx\n", addr);
		return -1;
	}
	return 0;
}

static int do_userid_export(struct cmd_tbl *cmdtp, int flag, int argc,
			    char *const argv[])
{
	unsigned long addr;
	char *pe;
	int ret;

	if (argc != 2)
		return CMD_RET_USAGE;
	addr = ustrtoul(argv[1], &pe, 0);
	ret = userid_export((void *)addr);
	if (ret <= 0) {
		pr_err("Failed to export to 0x%lx\n", addr);
		return -1;
	}
	return 0;
}

static int do_userid_read(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	unsigned long addr, size, offset;
	char *name, *pe;
	int rdcnt;

	if (argc < 5)
		return CMD_RET_USAGE;

	name = argv[1];
	offset = ustrtoul(argv[2], &pe, 0);
	size = ustrtoul(argv[3], &pe, 0);
	addr = ustrtoul(argv[4], &pe, 0);

	rdcnt = userid_read(name, offset, (void *)addr, size);
	if (rdcnt <= 0) {
		pr_err("Read %s failed.\n", name);
		return -1;
	}
	return 0;
}

static int do_userid_write(struct cmd_tbl *cmdtp, int flag, int argc,
			   char *const argv[])
{
	unsigned long addr, size, offset;
	char *name, *pe;
	int ret;

	if (argc < 5)
		return CMD_RET_USAGE;
	name = argv[1];
	offset = ustrtoul(argv[2], &pe, 0);
	size = ustrtoul(argv[3], &pe, 0);
	addr = ustrtoul(argv[4], &pe, 0);

	ret = userid_write(name, offset, (void *)addr, size);
	if ((unsigned long)ret != size) {
		pr_err("UserID write failed.\n");
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static int do_userid_writehex(struct cmd_tbl *cmdtp, int flag, int argc,
			      char *const argv[])
{
	int i, j, wrcnt;
	unsigned long size, offset;
	u8 *data, buf[64];
	u8 byte[3] = {0x00, 0x00, 0x00};
	char *name, *pe;

	if (argc < 4)
		return CMD_RET_USAGE;

	name = argv[1];
	offset = ustrtoul(argv[2], &pe, 0);
	data = argv[3];
	size = strlen(data) / 2;
	if (strlen(data) % 2) {
		pr_err("HEX string length shold be even.\n");
		return -1;
	}
	if (size > 64) {
		pr_err("Data size is too large.\n");
		return -1;
	}

	/* hex string to hex value */
	for (i = 0, j = 0; i < strlen(data) - 1; i += 2, j += 1) {
		byte[0] = data[i];
		byte[1] = data[i + 1];
		buf[j] = simple_strtol(byte, &pe, 16);
	}

	wrcnt = userid_write(name, offset, buf, size);
	if (wrcnt != size) {
		pr_err("Failed to write userid %s.\n", name);
		return -1;
	}
	return 0;
}

static int do_userid_writestr(struct cmd_tbl *cmdtp, int flag, int argc,
			      char *const argv[])
{
	int wrcnt;
	unsigned long size, offset;
	u8 *data;
	char *name, *pe;

	if (argc < 4)
		return CMD_RET_USAGE;

	name = argv[1];
	offset = ustrtoul(argv[2], &pe, 0);
	data = argv[3];
	size = strlen(data);

	wrcnt = userid_write(name, offset, data, size);
	if (wrcnt != size) {
		pr_err("Failed to write userid %s.\n", name);
		return -1;
	}
	return 0;
}

static int do_userid_save(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	int ret;

	ret = userid_save();
	if (ret)
		printf("Failed to save UserID to storage.\n");

	return 0;
}

static int do_lock(int flag)
{
	int ret;
	u8 lock;

	lock = flag;
	ret = userid_write("lock", 0, &lock, 1);
	if (ret != 1) {
		pr_err("Failed to write userid lock.\n");
		return -1;
	}
	ret = userid_save();
	if (ret)
		printf("Failed to save UserID to storage.\n");
	return 0;
}

static int do_userid_lock(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	return do_lock(1);
}

static int do_userid_unlock(struct cmd_tbl *cmdtp, int flag, int argc,
			    char *const argv[])
{
	return do_lock(0);
}

static int do_userid_help(struct cmd_tbl *cmdtp, int flag, int argc,
			    char *const argv[])
{
	return CMD_RET_USAGE;
}

static char userid_help_text[] =
	"ArtInChip UserID read/write command\n\n"
	"userid help                             : Show this help\n"
	"userid init                             : Initialize UserID\n"
	"userid list                             : List all items' name\n"
	"userid import   addr                    : Import userid from RAM\n"
	"userid export   addr                    : Export userid to RAM\n"
	"userid dump     name offset size        : Dump data in name\n"
	"userid size     name [addr]             : Get the ID's data size\n"
	"userid read     name offset size addr   : Read data in id to RAM address\n"
	"userid write    name offset size addr   : Write data to id from RAM address\n"
	"userid remove   name                    : Remove id item\n"
	"userid writehex name offset data        : Write data to id from input hex string\n"
	"userid writestr name offset data        : Write data to id from input string\n"
	"userid lock                             : Lock userid partition\n"
	"userid unlock                           : Unlock userid partition\n"
	"userid save                             : Save UserID data to storage\n";

U_BOOT_CMD_WITH_SUBCMDS(userid, "ArtInChip UserID command", userid_help_text,
			U_BOOT_SUBCMD_MKENT(help, 1, 0, do_userid_help),
			U_BOOT_SUBCMD_MKENT(init, 1, 0, do_userid_init),
			U_BOOT_SUBCMD_MKENT(list, 1, 0, do_userid_list),
			U_BOOT_SUBCMD_MKENT(dump, 5, 0, do_userid_dump),
			U_BOOT_SUBCMD_MKENT(size, 4, 0, do_userid_size),
			U_BOOT_SUBCMD_MKENT(read, 5, 0, do_userid_read),
			U_BOOT_SUBCMD_MKENT(write, 5, 0, do_userid_write),
			U_BOOT_SUBCMD_MKENT(writehex, 4, 0, do_userid_writehex),
			U_BOOT_SUBCMD_MKENT(writestr, 4, 0, do_userid_writestr),
			U_BOOT_SUBCMD_MKENT(remove, 3, 0, do_userid_remove),
			U_BOOT_SUBCMD_MKENT(import, 4, 0, do_userid_import),
			U_BOOT_SUBCMD_MKENT(export, 4, 0, do_userid_export),
			U_BOOT_SUBCMD_MKENT(lock, 1, 0, do_userid_lock),
			U_BOOT_SUBCMD_MKENT(unlock, 1, 0, do_userid_unlock),
			U_BOOT_SUBCMD_MKENT(save, 1, 0, do_userid_save));

