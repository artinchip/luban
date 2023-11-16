// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2013 Google, Inc
 *
 * (C) Copyright 2012
 * Pavel Herrmann <morpheus.ibis@gmail.com>
 */

#include <common.h>
#include <command.h>
#include <mapmem.h>
#include <asm/io.h>
#include <dma.h>

static int do_dma_m2m(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	int ret, len;
	unsigned long src, dst;

	src = simple_strtoul(argv[0], NULL, 16);
	dst = simple_strtoul(argv[1], NULL, 16);
	len = simple_strtoul(argv[2], NULL, 16);

	ret = dma_memcpy((void *)dst, (void *)src, len);
	return ret;
}

static struct cmd_tbl dma_commands[] = {
	U_BOOT_CMD_MKENT(m2m, 3, 1, do_dma_m2m, "m2m src dst len", ""),
};

static int do_dma(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	struct cmd_tbl *sub_cmd;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;
	sub_cmd = find_cmd_tbl(argv[1], dma_commands, ARRAY_SIZE(dma_commands));
	argc -= 2;
	argv += 2;

	if ((!sub_cmd || argc > sub_cmd->maxargs) ||
	    ((sub_cmd->name[0] != 'm') && (argc < 1)))
		return CMD_RET_USAGE;

	ret = sub_cmd->cmd(sub_cmd, flag, argc, argv);

	return cmd_process_error(sub_cmd, ret);
}

U_BOOT_CMD(
	dma,   5,      1,      do_dma,
	"DMA operations",
	"m2m src dst len       Memory to Memory DMA copy"
);
