// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2022-2023 ArtinChip Technology Co., Ltd.
 * Authors:  Zequan.liang <zequan.liang@artinchip.com>
 */

#include "artinchip/sample_base.h"
#include <getopt.h>
#include <sys/stat.h>

#define SDCARD_DEV		 0
#define USB_DEV			 1

#define NO_FIND_BLOCK_DEV        0
#define FIND_BLOCK_DEV           1
#define FIND_SDCARD_DEV          2
#define FIND_USB_DEV             3

#define SDCARD_TEST_PATH 	"/mnt/sdcard"
#define USB_TEST_PATH		"/mnt/usb"

int execute_command(const char *command)
{
	FILE *fp = NULL;
	char buffer[256];
	int err = 0;
	int ret;

	fp = popen(command, "r");
	if (fp == NULL) {
		printf("Failed to open subprocess\n");
		err = -1;
		goto CLOSE;
	}

	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		printf("%s", buffer);
	}

CLOSE:
	ret = pclose(fp);
	switch (ret) {
	case 1:
		printf("command is empty\n");
		return -1;
	case -1:
		printf("Faild to create child process\n");
		return -1;
	case 0X7F00:
		printf("Command error unable to execute\n");
		return -1;
	default:
		break;
	}

	return err;
}

int mount_block_dev(const char *command)
{
	FILE *fp = NULL;
	char buffer[256];
	int ret = 0;
	int err = 0;

	fp = popen(command, "r");
	if (fp == NULL) {
		printf("Failed to open subprocess\n");
		err = -1;
		goto CLOSE;
	}

	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		if (strstr(buffer, "No such file or directory")) {
			err = -1;
			break;
		}
	}

	if (err == -1) {
		printf("mount failure: command is = %s\n", command);
	}

CLOSE:
	ret = pclose(fp);
	switch (ret) {
	case 1:
		printf("command is empty\n");
		return -1;
	case -1:
		printf("Faild to create child process\n");
		return -1;
	case 0X7F00:
		printf("Command error unable to execute\n");
		return -1;
	default:
		break;
	}

	if (err == -1)
		return -1;

	return err;
}

int obtain_block_dev(char *dev, int type, int usb_num)
{
	FILE *fp = NULL;
	char line[256] = {0};
	char device[512] = {0};
	int ret = 0;
	int back_status = -1;
	/*
	 * ls /sys/class/block/
	 * mmcblk0    mmcblk0p1  sda        sda1    sdb        sdb1
	 */
	fp = popen("ls /sys/class/block/", "r");
	if (fp == NULL) {
		printf("Failed to open subprocess\n");
		goto CLOSE;
	}

	while (fgets(line, sizeof(line), fp) != NULL) {
		strcat(device, line);
	}

	if (type == SDCARD_DEV) {
		char *mmc = strstr(device, "mmcblk0");
		if (mmc) {
			char *partition = strstr(mmc + 7, "mmcblk0p1");
			if (partition) {
				snprintf(dev, strlen("dev/mmcblk0p1") + 1, "dev/%s", partition);
			} else {
				snprintf(dev, strlen("dev/mmcblk0") + 1, "dev/%s", mmc);
			}
			back_status = 0;
			goto CLOSE;
		}
	}  else if (type == USB_DEV) {
		char *usb_blk = NULL;
		char *usb_blk_num = NULL;
		if (usb_num == 0) {
			usb_blk = "sda";
			usb_blk_num = "sda1";
		} else if (usb_num == 1) {
			usb_blk = "sdb";
			usb_blk_num = "sdb1";
		}
		char *usb = strstr(device, usb_blk);
		if (usb) {
			char *partition = strstr(usb + 3, usb_blk_num);
			if (partition) {
				snprintf(dev, strlen("dev/sdx1") + 1, "dev/%s", partition);
			} else {
				snprintf(dev, strlen("dev/sdx") + 1, "dev/%s", usb);
			}
			back_status = 0;
			goto CLOSE;
		}
	}
CLOSE:
	ret = pclose(fp);
	switch (ret) {
	case 1:
		printf("command is empty\n");
		return -1;
	case -1:
		printf("Faild to create child process\n");
		return -1;
	case 0X7F00:
		printf("Command error unable to execute\n");
		return -1;
	default:
		break;
	}

	return back_status;
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
	FILE *file = NULL;
	int ret = 0;
	int i = 0;
	int success_num = 0;
	char dev_block[50] = {0};
	char command[256] = {0};
	char filenames[3][50] = {0};
	char contents[3][200] = {
		"This is the content of file0.\nIt contains multiple lines.\nLorem ipsum dolor sit amet, consectetur adipiscing elit.\n",
		"This is the content of file1.\nIt contains special characters like @#$%%^&*().\nSuspendisse ornare nibh id consequat lobortis.\n",
		"This is the content of file2.\nIt contains numbers: 12345.\nNullam aliquam massa vitae justo cursus auctor.\n"
	};
	char content[100] = {0};

	int type = SDCARD_DEV;
	int circle = 0;
	int usb_num = 0;
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
			usb_num = str2int(optarg);
			break;
		default:
			printf("Invalid parameter: %#x\n", ret);
			return 0;
		}
	}

	for (i = 0; i < 3; i++) {
		char str[50] = {0};
		if (type == SDCARD_DEV)
			snprintf(str, sizeof(str), "%s/file%d.txt", SDCARD_TEST_PATH, i);
		else
			snprintf(str, sizeof(str), "%s/file%d.txt", USB_TEST_PATH, i);
		strncpy(filenames[i], str, strlen(str));
	}

	memset(command, 0, sizeof(command));
	strcat(command, "mkdir ");
	if (type == SDCARD_DEV)
		strcat(command, SDCARD_TEST_PATH);
	else
		strcat(command, USB_TEST_PATH);
	ret = execute_command(command);
	if (ret < 0) {
		return 0;
	}

	ret = obtain_block_dev(dev_block, type, usb_num);
	if (ret < 0) {
		printf("can't find block dev, dev_block = %s\n", dev_block);
		return 0;
	}

	printf("dev_block name is = %s\n", dev_block);
	/* example: mount -t vfat /dev/mmcblk1 /mnt/sdcard */
	memset(command, 0, sizeof(command));
	strcpy(command, "mount -t vfat ");
	strcat(command, dev_block);
	strcat(command, " ");

	if (type == SDCARD_DEV)
		strcat(command, SDCARD_TEST_PATH);
	else
		strcat(command, USB_TEST_PATH);
	ret = mount_block_dev(command);
	if (ret < 0) {
		return 0;
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
		} else {
			failed_time++;
			printf("storage block dev test failed, times = %lld\n", failed_time);
		}
		if (circle) {
			usleep(100000);
			success_num = 0;
		}
	} while (circle);

	memset(command, 0, sizeof(command));
	strcat(command, "umount ");
	if (type == SDCARD_DEV)
		strcat(command, SDCARD_TEST_PATH);
	else
		strcat(command, USB_TEST_PATH);
	ret = execute_command(command);
	if (ret < 0) {
		return 0;
	}

	memset(command, 0, sizeof(command));
	strcat(command, "rmdir ");
	if (type == SDCARD_DEV)
		strcat(command, SDCARD_TEST_PATH);
	else
		strcat(command, USB_TEST_PATH);
	ret = execute_command(command);
	if (ret < 0) {
		return 0;
	}

	return 0;
}
