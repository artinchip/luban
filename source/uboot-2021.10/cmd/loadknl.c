// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd
 */

#include <common.h>
#include <command.h>
#include <console.h>
#include <malloc.h>
#include <linux/libfdt.h>
#include <image.h>
#include <bootstage.h>
#include <memalign.h>
#include <mmc.h>
#include <part.h>
#ifdef CONFIG_ARCH_ARTINCHIP
#include <asm/arch/boot_param.h>
#endif

#define LOADKNL_ARGS_MAX 5
#define CMD_MAX_SIZE 128
#define HEAD_DATA_SIZE 0x1000

static unsigned long get_kernel_itb_size(u8 *head)
{
	unsigned long siz;
	int ret;

	ret = fdt_check_header(head);
	if (ret) {
		printf("Error: Not a valid itb image.\n");
		return 0;
	}
	siz = fdt_totalsize(head);
	pr_debug("%s, size %ld\n", __func__, siz);
	return siz;
}

#ifdef CONFIG_CMD_MMC
/*
 * Load kernel image from partition
 * e.g.:
 * mmc 0 kernel 0x81000000
 */
static int load_from_mmc_partition(int argc, char *const argv[])
{
	char *part, *addr_str;
	unsigned long addr, imgsize, bcnt, start, time;
	struct blk_desc *desc;
	struct disk_partition part_info;
	struct mmc *mmc;
	int devnum, ret;
	u32 n;

	ret = 0;
	if (argc != 4)
		return CMD_RET_USAGE;

	devnum = simple_strtoull(argv[1], NULL, 0);
	part = argv[2];
	addr_str = argv[3];
	addr = simple_strtoull(addr_str, NULL, 0);

	ret = blk_get_device_by_str(argv[0], argv[1], &desc);
	if (ret < 0) {
		pr_err("%s, line %d: failed to get blk dev.\n", __func__, __LINE__);
		return CMD_RET_FAILURE;
	}
	ret = part_get_info_by_name(desc, part, &part_info);
	if (ret < 0) {
		pr_err("%s, line %d: failed to get part.\n", __func__, __LINE__);
		return CMD_RET_FAILURE;
	}

	start = part_info.start;

	mmc = find_mmc_device(devnum);
	if (!mmc) {
		printf("no mmc device at slot %x\n", devnum);
		return CMD_RET_FAILURE;
	}
	if (mmc_init(mmc)) {
		printf("Failed to init mmc\n");
		return CMD_RET_FAILURE;
	}
	/* Read first LBA to get itb head */
	bcnt = 1;
	n = blk_dread(desc, start, bcnt, (void *)addr);
	if (n != bcnt) {
		printf("Failed to read first data of kernel partition\n");
		return CMD_RET_FAILURE;
	}

	imgsize = get_kernel_itb_size((u8 *)addr);
	if (imgsize <= part_info.blksz) {
		/* If it is not itb image, read the whole partition */
		bcnt = part_info.size;
	} else {
		imgsize -= part_info.blksz;
		/* Round up to blksz alignment */
		bcnt = (imgsize + part_info.blksz - 1) / part_info.blksz;
	}

	start++;
	addr += part_info.blksz;

	time = get_timer(0);
	n = blk_dread(desc, start, bcnt, (void *)addr);
	if (n != bcnt) {
		printf("Failed to read the rest of kernel partition\n");
		return CMD_RET_FAILURE;
	}
	time = get_timer(time);

	if (bcnt && time) {
		unsigned long rdsiz, n, d, speed_int, speed_pnt;

		rdsiz = bcnt * part_info.blksz;
		n = rdsiz * 1000;
		d = time * 1024 * 1024;
		speed_int =  n / d;
		speed_pnt = n * 100 / d - speed_int * 100;
		pr_info("Read kernel speed (size %lu time %lu ms) %lu.%lu MB/s\n",
			rdsiz, time, speed_int, speed_pnt);
	}

	return 0;
}
#endif

#ifdef CONFIG_CMD_MTD
/*
 * e.g.:
 * mtd kernel 0x81000000
 */
static int load_from_mtd_partition(int argc, char *const argv[])
{
	char *cmdbuf = NULL, *part, *addr_str;
	unsigned long addr, size, start, time;
	int ret;

	if (argc != 3)
		return CMD_RET_USAGE;

	part = argv[1];
	addr_str = argv[2];
	addr = simple_strtoull(addr_str, NULL, 0);

	cmdbuf = (char *)malloc(CMD_MAX_SIZE);
	if (!cmdbuf) {
		pr_err("%s: failed to malloc buffer.\n", __func__);
		return CMD_RET_FAILURE;
	}

	/* Read first 4KB to get the total length */
	start = 0;
	snprintf(cmdbuf, CMD_MAX_SIZE, "mtd read %s %s 0x%lx 0x%x", part,
		 addr_str, start, HEAD_DATA_SIZE);

	ret = run_command(cmdbuf, 0);
	if (ret) {
		printf("Failed to read first data of kernel partition\n");
		goto out;
	}

	size = get_kernel_itb_size((u8 *)addr) - HEAD_DATA_SIZE;
	if (size) {
		/* Round up to 4KB alignment */
		if (size & 0xFFF)
			size = (((size >> 12) + 1) << 12);

		/* Read the rest data of kernel image */
		start = HEAD_DATA_SIZE;
		snprintf(cmdbuf, CMD_MAX_SIZE, "mtd read %s 0x%lx 0x%lx 0x%lx",
			 part, addr + 0x1000, start, size);
	} else {
		/*
		 * Unknown size, read the whole partition
		 */
		snprintf(cmdbuf, CMD_MAX_SIZE, "mtd read 0x%lx %s", addr, part);
	}

	time = get_timer(0);
	ret = run_command(cmdbuf, 0);
	if (ret) {
		printf("Failed to read the rest of kernel partition\n");
		goto out;
	}
	time = get_timer(time);
	if (size && time) {
		unsigned long rdsiz, n, d, speed_int, speed_pnt;

		rdsiz = size - start;
		n = rdsiz * 1000;
		d = time * 1024 * 1024;
		speed_int =  n / d;
		speed_pnt = n * 100 / d - speed_int * 100;
		pr_info("Read kernel speed (size %lu time %lu ms) %lu.%lu MB/s\n",
			rdsiz, time, speed_int, speed_pnt);
	}
out:
	if (cmdbuf)
		free(cmdbuf);
	return ret;
}
#endif

/*
 * load kernel from specific partition, and auto-detect the kernel size
 */
static int do_loadknl(struct cmd_tbl *cmdtp, int flag, int argc,
		      char *const argv[])
{
	int ret = CMD_RET_USAGE;
	char *ramboot;


	if (argc > LOADKNL_ARGS_MAX)
		return CMD_RET_USAGE;

	if (argv[1] == NULL)
		return CMD_RET_USAGE;

	/* Don't load from device if it is ramboot,
	 * In ramboot mode, kernel image already download from host to DDR
	 */
	ramboot = env_get("ramboot");
	if (ramboot)
		return 0;

#ifdef CONFIG_ARTINCHIP_DEBUG_BOOT_TIME
	u32 *p = (u32 *)BOOT_TIME_UBOOT_LOAD_KERNEL_START;
	*p = aic_timer_get_us();
#endif

#ifdef CONFIG_CMD_MMC
	if (!strcmp(argv[1], "mmc"))
		ret = load_from_mmc_partition(argc - 1, &argv[1]);
#endif
#ifdef CONFIG_CMD_MTD
	if (!strcmp(argv[1], "mtd"))
		ret = load_from_mtd_partition(argc - 1, &argv[1]);
#endif

#ifdef CONFIG_ARTINCHIP_DEBUG_BOOT_TIME
	p = (u32 *)BOOT_TIME_UBOOT_LOAD_KERNEL_DONE;
	*p = aic_timer_get_us();
#endif
	if (ret)
		printf("Load kernel from %s failed.\n", argv[1]);
	return ret;
}

U_BOOT_CMD(loadknl, LOADKNL_ARGS_MAX, 0, do_loadknl,
	"load kernel itb image from partition",
	"[devtype] [partition] [address]\n"
	"  - devtype: should be mmc, mtd\n"
	"  - address: memory to store kernel\n"
	"e.g.\n"
	"loadknl mmc ${boot_devnum} kernel ${knl_addr}\n"
	"loadknl mtd kernel ${knl_addr}\n"
);
