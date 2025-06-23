// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 ArtInChip Technology Co.,Ltd
 */

#include <common.h>
#include <dm/device.h>
#include <asm/cache.h>
#include <command.h>
#include <time.h>
#include <artinchip_ve.h>

static int do_hwunzip(struct cmd_tbl *cmdtp, int flag, int argc,
		    char *const argv[])
{
	unsigned long src, dst;
	unsigned long dst_len = ~0UL, img_len = ~0UL;
	unsigned long int start, delta;

	src = hextoul(argv[1], NULL);
	img_len = hextoul(argv[2], NULL);
	dst = hextoul(argv[3], NULL);
	dst_len = hextoul(argv[4], NULL);

	struct udevice *dev;
	int ret;
	ret = gunzip_init_device(&dev);
	if (ret) {
		printf("artinchip gunzip init failed\n");
		return -1;
	}
	start = get_timer(0);
	ret = gunzip_decompress(dev, (void *)dst, dst_len, (void *)src,
				&img_len);
	delta = get_timer(start);
	if (ret == 0)
		printf("Decrompess OK, raw image length %lu, time %lu ms\n",
		       img_len, delta);
	return 0;
}

U_BOOT_CMD(
	hwunzip,	5,	1,	do_hwunzip,
	"hardware unzip a memory region",
	"srcaddr srcsize dstaddr dstsize"
);
