// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022-2023 ArtinChip Technology Co., Ltd.
 * Authors:  Zequan.liang <zequan.liang@artinchip.com>
 */

#include "artinchip/sample_base.h"
#include <getopt.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/mount.h>

#define SDCARD_DEV		 0
#define USB_DEV			 1

#define SDCARD_TEST_PATH 	"/mnt/sdcard"
#define USB_TEST_PATH		"/mnt/udisk"

static void umount_blk_dev(char *path)
{
	FILE *fp = fopen("/proc/mounts", "r");

	if (fp == NULL)
		return;

	char line[512];
	while (fgets(line, sizeof(line), fp)) {
		char *dev = strtok(line, " ");
		char *dir = strtok(NULL, " ");
		(void)dev;
		if (dir && strcmp(dir, path) == 0) {
			umount(path);
		}
	}
	fclose(fp);
	return;
}

static int check_dir_exit(char *path)
{
	DIR *dir = opendir(path);
	if (dir) {
		closedir(dir);
		return 0; /* exist */
	} else {
		return -1;
	}
}

static int get_blk_dev(char *dev, int type, int blk_num)
{
	FILE *fp = popen("ls /sys/class/block/", "r");
	if (fp == NULL) {
		fprintf(stderr, "Failed to open subprocess\n");
		return -1;
	}

	char line[64];
	int back_status = -1;
	const char usb_blk_dev[][4] = {
		{"sda"},
		{"sdb"}
	};
	const char mmc_blk_dev[][8] = {
		{"mmcblk0"},
		{"mmcblk1"}
	};

	while (fgets(line, sizeof(line), fp)) {
		// Trim newline character
		line[strcspn(line, "\n")] = '\0';

		// Process based on device type
		if ((type == SDCARD_DEV && strstr(line, mmc_blk_dev[blk_num]) == line) ||
			(type == USB_DEV && (strstr(line, usb_blk_dev[blk_num]) == line))) {
			if (type == SDCARD_DEV) {
				snprintf(dev, 49, "/dev/mmcblk%dp1", blk_num);
			} else if (type == USB_DEV) {
				snprintf(dev, 49, "/dev/sd%c1", (char)('a' + blk_num));
			}

			back_status = 0;
			break; // Found the device, no need to continue loop
		}
	}

	int ret = pclose(fp);
	if (ret != 0) {
	    return back_status;
	}

	return back_status;
}

static int file_create_test(char *blk_path, int circle)
{
	FILE *file = NULL;
	int i;
	int success_num = 0;
	char filenames[3][50] = {0};
	char contents[3][200] = {
		"This is the content of file0.\nIt contains multiple lines.\nLorem ipsum dolor sit amet, consectetur adipiscing elit.\n",
		"This is the content of file1.\nIt contains special characters like @#$%%^&*().\nSuspendisse ornare nibh id consequat lobortis.\n",
		"This is the content of file2.\nIt contains numbers: 12345.\nNullam aliquam massa vitae justo cursus auctor.\n"
	};
	char content[100] = {0};


	for (i = 0; i < 3; i++) {
		snprintf(filenames[i], 49, "%s/file%d.txt", blk_path, i);
	}

	do {
		/* Create files */
		for (i = 0; i < 3; i++) {
			file = fopen(filenames[i], "w");
			if (file == NULL) {
				printf("Unable to create file: %s\n", filenames[i]);
				return 0;
			}
			fclose(file);
		}

		/* Write complex content to files */
		for (i = 0; i < 3; i++) {
			file = fopen(filenames[i], "w");
			if (file == NULL) {
				printf("Unable to open file: %s\n", filenames[i]);
				return 0;
			}

			fprintf(file, "%s", contents[i]);
			fclose(file);
		}

		/* Read and print content from files */
		for (i = 0; i < 3; i++) {
			file = fopen(filenames[i], "r");
			if (file == NULL) {
				printf("Unable to open file: %s\n", filenames[i]);
				return 0;
			}

			while (fgets(content, sizeof(content), file) != NULL) {
				if (strncmp(contents[i], content, strlen(content)) == 0)
					success_num++;
			}

			fclose(file);
		}

		/* Check file conditions */
		for (i = 0; i < 3; i++) {
			file = fopen(filenames[i], "r");
			if (file != NULL) {
				fseek(file, 0, SEEK_END);
				long size = ftell(file);
				if (size > 0) {
					success_num++;
				} else {
					printf("%s is empty.\n", filenames[i]);
				}
				fclose(file);
			}
		}

		/* Delete files */
		for (i = 0; i < 3; i++) {
			if (remove(filenames[i]) == 0) {
				success_num++;
			} else {
				printf("Unable to delete the file: %s\n", filenames[i]);
			}
		}

		long long failed_time = 0;
		if (success_num == 9) {
			printf("storage block dev test success\n");
			return 0;
		} else {
			failed_time++;
			printf("storage block dev test failed, times = %lld\n", failed_time);
			return -1;
		}
		if (circle) {
			usleep(100000);
			success_num = 0;
		}
	} while (circle);

	return -1;
}

static void usage(char *app)
{
	printf("Usage: %s [Options], built on %s %s\n", app, __DATE__, __TIME__);
	printf("\t-t, --type,    select storage block device types, types can use usb or sdcard (default type is sdcard)\n");
	printf("\t-c, --circle,  loop test\n");
	printf("\t-n, --blk_num, block num, (default 0)\n");
	printf("\t-u, --usage\n\n");

	printf("\texample: test_blkdev -t sdcard -c -n 0\n");
}

int main(int argc, char **argv) {
	int ret = 0;
	char dev_block[50] = {0};

	int type = SDCARD_DEV;
	int circle = 0;
	int blk_num = 0;
	char *blk_path = SDCARD_TEST_PATH;
	const char sopts[] = "uct:n:";
	static const struct option lopts[] = {
		{"usage",   no_argument, 	NULL, 'u'},
		{"circle",  no_argument, 	NULL, 'c'},
		{"type",    required_argument,  NULL, 't'},
		{"num",     required_argument,  NULL, 'n'},
		{NULL, 0, 0, 0},
	};

	while ((ret = getopt_long(argc, argv, sopts, lopts, NULL)) != -1) {
		switch (ret) {
		case 't':
			if (strncmp(optarg, "usb", strlen("usb")) == 0) {
				type = USB_DEV;
				break;
			} else if (strncmp(optarg, "sdcard", strlen("sdcard")) == 0) {
				type = SDCARD_DEV;
				break;
			} else {
				printf("Invalid parameter: %#x\n", ret);
				return 0;
			}
			break;
		case 'u':
			usage(argv[0]);
			return 0;
		case 'c':
			circle = 1;
			break;
		case 'n':
			blk_num = str2int(optarg);
			break;
		default:
			printf("Invalid parameter: %#x\n", ret);
			return 0;
		}
	}

	if (type == USB_DEV) {
		blk_path = USB_TEST_PATH;
	}

	if (check_dir_exit(blk_path) != 0) {
		char command[256] = {0};
		snprintf(command, sizeof(command) - 1, "mkdir %s", blk_path);
		system(command);
	}

	umount_blk_dev(blk_path);

	if (get_blk_dev(dev_block, type, blk_num) < 0) {
		printf("can't find block dev\n");
		return 0;
	}
	printf("dev_block name is = %s\n", dev_block);

	if (mount(dev_block, blk_path, "vfat", 0, NULL) != 0) {
		printf("can't mount %s to %s\n", dev_block, blk_path);
		return -1;
	}

	file_create_test(blk_path, circle);

	umount_blk_dev(blk_path);

	return 0;
}
