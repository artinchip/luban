// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
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
#include <kcapi.h>
#include "util.h"
#include "app.h"

static struct subcmd_cfg cmd_list[] = {
	{ "gen-test-data", NULL, TYPE_UTIL, 0, 0 },
	{ "aes-128-ecb", "ecb(aes)", TYPE_SK, KEY_128, 0 },
	{ "aes-192-ecb", "ecb(aes)", TYPE_SK, KEY_192, 0 },
	{ "aes-256-ecb", "ecb(aes)", TYPE_SK, KEY_256, 0 },
	{ "aes-128-cbc", "cbc(aes)", TYPE_SK, KEY_128, BLK_128 },
	{ "aes-192-cbc", "cbc(aes)", TYPE_SK, KEY_192, BLK_128 },
	{ "aes-256-cbc", "cbc(aes)", TYPE_SK, KEY_256, BLK_128 },
	{ "aes-128-ctr", "ctr(aes)", TYPE_SK, KEY_128, BLK_128 },
	{ "aes-192-ctr", "ctr(aes)", TYPE_SK, KEY_192, BLK_128 },
	{ "aes-256-ctr", "ctr(aes)", TYPE_SK, KEY_256, BLK_128 },
	{ "aes-128-cts", "cts(aes)", TYPE_SK, KEY_128, BLK_128 },
	{ "aes-256-cts", "cts(aes)", TYPE_SK, KEY_256, BLK_128 },
	{ "aes-128-xts", "xts(aes)", TYPE_SK, KEY_256, BLK_128 },
	{ "aes-256-xts", "xts(aes)", TYPE_SK, KEY_512, BLK_128 },
	{ "ssk-aes-128-ecb", "ssk-protected(ecb(aes))", TYPE_SK, KEY_128, 0 },
	{ "ssk-aes-256-ecb", "ssk-protected(ecb(aes))", TYPE_SK, KEY_256, 0 },
	{ "ssk-aes-128-cbc", "ssk-protected(cbc(aes))", TYPE_SK, KEY_128, BLK_128 },
	{ "ssk-aes-256-cbc", "ssk-protected(cbc(aes))", TYPE_SK, KEY_256, BLK_128 },
	{ "huk-aes-128-ecb", "huk-protected(ecb(aes))", TYPE_SK, KEY_128, 0 },
	{ "huk-aes-256-ecb", "huk-protected(ecb(aes))", TYPE_SK, KEY_256, 0 },
	{ "huk-aes-128-cbc", "huk-protected(cbc(aes))", TYPE_SK, KEY_128, BLK_128 },
	{ "huk-aes-256-cbc", "huk-protected(cbc(aes))", TYPE_SK, KEY_256, BLK_128 },
	{ "huk-aes-128-cts", "huk-protected(cts(aes))", TYPE_SK, KEY_128, BLK_128 },
	{ "huk-aes-256-cts", "huk-protected(cts(aes))", TYPE_SK, KEY_256, BLK_128 },
	{ "huk-aes-128-xts", "huk-protected(xts(aes))", TYPE_SK, KEY_256, BLK_128 },
	{ "huk-aes-256-xts", "huk-protected(xts(aes))", TYPE_SK, KEY_512, BLK_128 },
	{ "des-ecb", "ecb(des)", TYPE_SK, KEY_64, 0 },
	{ "des-cbc", "cbc(des)", TYPE_SK, KEY_64, BLK_64 },
	{ "des-ede3", "ecb(des3_ede)", TYPE_SK, KEY_192, 0 },
	{ "des-ede3-cbc", "cbc(des3_ede)", TYPE_SK, KEY_192, BLK_64 },
	{ "rsa", "rsa", TYPE_AK, 0, 0},
	{ "pnk-rsa", "pnk-protected(rsa)", TYPE_AK, 0, 0},
	{ "psk0-rsa", "psk0-protected(rsa)", TYPE_AK, 0, 0},
	{ "psk1-rsa", "psk1-protected(rsa)", TYPE_AK, 0, 0},
	{ "psk2-rsa", "psk2-protected(rsa)", TYPE_AK, 0, 0},
	{ "psk3-rsa", "psk3-protected(rsa)", TYPE_AK, 0, 0},
	{ "md5", "md5", TYPE_MD, 0, 0},
	{ "sha1", "sha1", TYPE_MD, 0, 0},
	{ "sha224", "sha224", TYPE_MD, 0, 0},
	{ "sha256", "sha256", TYPE_MD, 0, 0},
	{ "sha384", "sha384", TYPE_MD, 0, 0},
	{ "sha512", "sha512", TYPE_MD, 0, 0},
	{ "hmac-sha1", "hmac(sha1)", TYPE_MD, 1, 0},
	{ "hmac-sha256", "hmac(sha256)", TYPE_MD, 1, 0},
};

static struct subcmd_cfg *get_subcmd_cfg(char *name)
{
	int i, cnt;

	cnt = sizeof(cmd_list) / sizeof(cmd_list[0]);
	for (i = 0; i < cnt; i++) {
		if (strcmp(name, cmd_list[i].cmd_name) == 0)
			return &cmd_list[i];
	}

	return NULL;
}

static void show_subcmd(char *name)
{
	int i, cnt;

	printf("Usage:\n");
	printf("  %s <subcmd> <options>\n", name);
	printf("  subcmd list:\n");

	cnt = sizeof(cmd_list) / sizeof(cmd_list[0]);
	for (i = 0; i < cnt; i++)
		printf("    %s\n", cmd_list[i].cmd_name);
}

int main(int argc, char *argv[])
{
	struct subcmd_cfg *cmd;
	char *app_name;
	int ret = 0;

	app_name = basename(argv[0]);
	if (argc < 2) {
		show_subcmd(app_name);
		return -EINVAL;
	}

	cmd = get_subcmd_cfg(argv[1]);
	if (!cmd) {
		fprintf(stderr, "%s is not supported.\n", argv[1]);
		return -EINVAL;
	}

	cmd->app_name = app_name;
	if (cmd->type == TYPE_UTIL)
		ret = util_command(cmd, argc - 1, &argv[1]);
	if (cmd->type == TYPE_SK)
		ret = skcipher_command(cmd, argc - 1, &argv[1]);
	if (cmd->type == TYPE_AK)
		ret = akcipher_command(cmd, argc - 1, &argv[1]);
	if (cmd->type == TYPE_MD)
		ret = digest_command(cmd, argc - 1, &argv[1]);
	return ret;
}
