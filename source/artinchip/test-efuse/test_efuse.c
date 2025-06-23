// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2024 ArtInChip Technology Co., Ltd.
 * Authors:  Xiong Hao <hao.xiong@artinchip.com>
 */

#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <asm/types.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

/* Global macro and variables */

#define EFUSE_DEV_PATH "/sys/bus/nvmem/devices/aic-efuse0/nvmem"

struct efuse_info {
	char *name;
	unsigned int start;
	unsigned int len;
};

static struct efuse_info efuse_info_list[] = {
	{ "disread",      0x00, 0x08 },
	{ "diswrite",     0x08, 0x08 },
	{ "chipid",       0x10, 0x10 },
	{ "cali",         0x20, 0x10 },
	{ "brom",         0x30, 0x08 },
	{ "secure",       0x38, 0x08 },
	{ "rotpk",        0x40, 0x10 },
	{ "ssk",          0x50, 0x10 },
	{ "huk",          0x60, 0x10 },
	{ "psk0",         0x70, 0x08 },
	{ "psk1",         0x78, 0x08 },
	{ "psk2",         0x80, 0x08 },
	{ "psk3",         0x88, 0x08 },
	{ "nvcntr",       0x90, 0x10 },
	{ "spienc_key",   0xA0, 0x10 },
	{ "spienc_nonce", 0xB0, 0x08 },
	{ "pnk",          0xB8, 0x08 },
	{ "customer",     0xC0, 0x40 }
};

static struct efuse_info *get_efuse_info(char *name)
{
	int i, cnt;

	cnt = sizeof(efuse_info_list) / sizeof(efuse_info_list[0]);
	for (i = 0; i < cnt; i++) {
		if (strcmp(name, efuse_info_list[i].name) == 0)
			return &efuse_info_list[i];
	}

	return NULL;
}

void hexdump(const char *msg, const uint8_t* data, int len)
{
	int i, plen;
	char msgbuf[32];
	const uint8_t *p;

	memset(msgbuf, ' ', 16);
	msgbuf[16] = '\0';
	strncpy(msgbuf, msg, strlen(msg) > 16 ? 16 : strlen(msg));
	printf("%s", msgbuf);

	memset(msgbuf, ' ', 16);
	p = data;
	do {
		if (len > 16)
			plen = 16;
		else
			plen = len;
		for (i = 0; i < plen; ++i)
			printf("%.2x ", p[i]);
		printf("\n");

		p = p + plen;
		len -= plen;
		if (len > 0)
			printf("%s", msgbuf);
	} while (len > 0);
}

static int usage(char *program)
{
	int i, cnt;

	printf("Compile time: %s %s\n", __DATE__, __TIME__);
	printf("Usage: %s [options]\n", program);

	cnt = sizeof(efuse_info_list) / sizeof(efuse_info_list[0]);
	for (i = 0; i < cnt; i++)
		printf("    %s show %s\t\tPrint %s infomation\n", program,
		       efuse_info_list[i].name, efuse_info_list[i].name);
	return 0;
}

/* Open a device file to be needed. */
static int efuse_show_info(struct efuse_info *info)
{
	char filename[64] = {0};
	unsigned char *data;
	int ret = 0, fd = -1;

	data = malloc(info->len);
	if (!data) {
		printf("malloc data buf failed.\n");
		return -1;
	}

	snprintf(filename, 64, "%s", EFUSE_DEV_PATH);
	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		printf("Failed to open %s errno: %d[%s]\n", filename, errno, strerror(errno));
		ret = -1;
		goto out;
	}

	lseek(fd, info->start, SEEK_SET);
	read(fd, data, info->len);

	hexdump(info->name, data, info->len);

out:
	if (fd)
		close(fd);
	if (data)
		free(data);

	return ret;
}

int main(int argc, char **argv)
{
	struct efuse_info *info = NULL;
	int ret = 0;

	if (argc < 3) {
		usage(argv[0]);
		return -EINVAL;
	}

	info = get_efuse_info(argv[2]);
	if (!info) {
		fprintf(stderr, "not found %s info.\n", argv[1]);
		return -EINVAL;
	}

	if (!strcmp(argv[1], "show")) {
		ret = efuse_show_info(info);
		if (ret) {
			usage(argv[0]);
			return ret;
		}
	} else {
		usage(argv[0]);
	}

	return ret;
}
