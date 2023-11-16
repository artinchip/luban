// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
 * Authors:  Wu Dehuang <dehuang.wu@artinchip.com>
 */

// #define HOST_TOOL
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <asm/types.h>
#include <sys/socket.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>
#include <sys/user.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/syscall.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifndef HOST_TOOL
#include "app.h"
#endif

static void gen_test_data_usage(void)
{
	printf(" --help Show this help.\n");
	printf(" --size Test data size.\n");
	printf(" --out  Test data output file name.\n");
}

static int gen_test_data(int argc, char *argv[])
{
	int c = 0;
	FILE *fp;
	char *fname = NULL;
	int opt_index = 0;
	unsigned long i, size = 0;

	static struct option cmd_opts[] = {
		{ "help", no_argument, 0, 'h' },
		{ "size", required_argument, 0, 's' },
		{ "out", required_argument, 0, 'o' },
		{ 0, 0, 0, 0 }
	};

	while (1) {
		c = getopt_long_only(argc, argv, "h", cmd_opts, &opt_index);
		if (c < 0)
			break;

		switch (c) {
		case 's': /* size */
			size = strtoul(optarg, NULL, 10);
			break;
		case 'o': /* out */
			fname = optarg;
			break;
		case 'h':
			gen_test_data_usage();
			return 0;
		default:
			return -EINVAL;
		}
	}

	if (!size  || !fname) {
		gen_test_data_usage();
		return -1;
	}

	fp = fopen(fname, "wb");
	if (!fp) {
		fprintf(stderr, "Failed to open %s\n", fname);
		return -EINVAL;
	}

	c = 0;
	for (i = 0; i < size; i++) {
		fwrite(&c, 1, 1, fp);
		c++;
		c = c % 256;
	}

	fclose(fp);
	return 0;
}

#ifndef HOST_TOOL
int util_command(struct subcmd_cfg *cmd, int argc, char *argv[])
{
	if (!strcmp(cmd->cmd_name, "gen-test-data"))
		return gen_test_data(argc, argv);
	return -1;
}
#else
int main(int argc, char *argv[])
{
	return gen_test_data(argc, argv);
}
#endif
