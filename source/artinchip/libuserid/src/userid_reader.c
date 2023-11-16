/*
 * Copyright (C) 2020-2023 Artinchip Technology Co., Ltd.
 * Authors:  Wu Dehuang <dehuang.wu@artinchip.com>
 */

#include <unistd.h>
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
#include <time.h>
#include <sys/utsname.h>
#include <linux/random.h>
#include <sys/syscall.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <libgen.h>
#include <artinchip/userid.h>

static char userid_help_text[] =
	"ArtInChip UserID example command\n\n"
	"userid help                             : Show this help\n"
	"userid list partition                   : List all items' name\n"
	"                                          e.g.: userid list /dev/mtd2\n"
	"userid dump partition id_name           : Dump data in name\n"
	"                                          e.g.: userid dump /dev/mtd2 sn\n"
	"userid writestr partition id_name str   : Write data to id from input string. Just for test.\n"
	"                                          e.g.: userid writestr /dev/mtd2 sn test_value\n";

static int do_userid_help(int argc, const char *argv[])
{
	printf("%s\n", userid_help_text);
	return 0;
}

static int do_userid_list(int argc, const char *argv[])
{
	int i, cnt, ret;
	char name[256];

	ret = userid_init(argv[1]);
	if (ret)
		return -1;
	cnt = userid_get_count();
	printf("ID count %d\n", cnt);
	for (i = 0; i < cnt; i++) {
		memset(name, 0, 256);
		ret = userid_get_name(i, name, 255);
		printf("  %s\n", name);
		if (ret <= 0)
			continue;
	}

	userid_deinit();
	return 0;
}

static void print_hex(uint8_t *data, uint32_t len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (i && (i % 16 == 0))
			printf("\n");
		printf("%02X ", data[i]);
	}
	printf("\n");
}

static int do_userid_dump(int argc, const char *argv[])
{
	unsigned long size, offset;
	const char *name;
	uint8_t *p;
	int ret, rdcnt;

	if (argc != 3)
		return -EINVAL;
	name = argv[2];

	ret = userid_init(argv[1]);
	if (ret)
		return -1;
	offset = 0;
	size = userid_get_data_length(name);

	p = malloc(size);
	rdcnt = userid_read(name, offset, p, size);
	if (rdcnt > 0)
		print_hex(p, rdcnt);
	else
		fprintf(stdout, "Read failed. ret = %d\n", rdcnt);

	userid_deinit();
	free(p);
	return 0;
}

static int do_userid_writestr(int argc, const char *argv[])
{
	int wrcnt;
	unsigned long size, offset;
	const char *dev, *data, *name;
	int ret;

	if (argc < 4)
		return -EINVAL;

	dev = argv[1];
	name = argv[2];
	data = argv[3];
	offset = 0;
	size = strlen(data);

	ret = userid_init(dev);
	if (ret)
		return -1;
	wrcnt = userid_write(name, offset, (uint8_t *)data, size);
	if (wrcnt != size) {
		fprintf(stderr, "Failed to write userid %s.\n", name);
		return -1;
	}

	ret = userid_save(dev);
	if (ret)
		return -1;

	return 0;
}

int main(int argc, const char *argv[])
{
	if (argc <= 1)
		goto err;
	if (!strcmp(argv[1], "list"))
		do_userid_list(argc - 1, &argv[1]);
	else if (!strcmp(argv[1], "dump"))
		do_userid_dump(argc - 1, &argv[1]);
	else if (!strcmp(argv[1], "writestr"))
		do_userid_writestr(argc - 1, &argv[1]);
	else
		goto err;
	return 0;
err:
	do_userid_help(0, NULL);
	return -1;
}
