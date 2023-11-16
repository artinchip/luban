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

int obtain_block_dev(char *dev, int type)
{
	FILE *fp = NULL;
	char buffer[256] = {0};
	char block_dev[5][100] = {0};
	int block_num = 0;
	int find_dev = NO_FIND_BLOCK_DEV;
	int i = 0;
	int ret = 0;
	int err = 0;
	int len = 0;
	char *ptr;

	/*
	 * fdisk -l format:
	 * Disk /dev/mmcblk0: 30 GB, 31927042048 bytes, 62357504 sectors
	 * 3881 cylinders, 255 heads, 63 sectors/track
         * Units: sectors of 1 * 512 = 512 bytes
         * Device       Boot StartCHS    EndCHS        StartLBA     EndLBA    Sectors  Size Id Type
	 * /dev/mmcblk0p1    0,2,3       1023,254,63        128   62357503   62357376 29.7G  c Win95 FAT32 (LBA)
	 * Disk /dev/sda: 7680 MB, 8053063680 bytes, 15728640 sectors
	 * 1022 cylinders, 248 heads, 62 sectors/track
	 *
	 * Units: sectors of 1 * 512 = 512 bytes
	 * Device  Boot StartCHS    EndCHS        StartLBA     EndLBA    Sectors  Size Id Type
	 * /dev/sda1 73 97,115,32   107,121,32  1948285285 3650263507 1701978223  811G 6e Unknown
	 */
	fp = popen("fdisk -l", "r");
	if (fp == NULL) {
		printf("Failed to open subprocess\n");
		err = -1;
		goto CLOSE;
	}

	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		/* locate next line */
		if (strstr(buffer, "Device")) {
			find_dev = FIND_BLOCK_DEV;
			continue;
		} else if (strstr(buffer, "Disk")) {
			find_dev = FIND_BLOCK_DEV;
		}

		/* save dev name example: /dev/mmcblk0p1 and Disk /dev/sda 7680 MB, 8053063680 bytes, 15728640 sectors ... */
		if (find_dev == FIND_BLOCK_DEV) {
			strncpy(block_dev[block_num], buffer, 20);
			block_num++;
			find_dev = NO_FIND_BLOCK_DEV;
			if (block_num >= 5)
				break;
		}
	}

	/* roughly determine the type of storage block device */
	for (i = 0; i < block_num; i++) {
		if (type == SDCARD_DEV) {
			if (strstr(block_dev[i], "Disk /dev/mmcblk")) {
				continue;
			}

			if (strstr(block_dev[i], "/dev/mmcblk")) {
				find_dev = FIND_SDCARD_DEV;
				len = strlen("/dev/mmcblk") + 3;
				strncpy(dev, block_dev[i], len);
			}
		} else {
			if (strstr(block_dev[i], "Disk /dev/sda")) {
				find_dev = FIND_USB_DEV;
				len = strlen("/dev/sda") + 1;
				ptr = strchr(block_dev[i], ' ');
				strncpy(dev, ptr + 1, len);
				if (dev[len - 1] == ':')
					dev[len - 1] = '\0';
			} else if (strstr(block_dev[i], "/dev/sda")) {
				find_dev = FIND_USB_DEV;
				len = strlen("/dev/sda") + 1;
				strncpy(dev, block_dev[i], len);
			}
		}

		if (find_dev == FIND_SDCARD_DEV || find_dev == FIND_USB_DEV) {
			break;
		}
	}

	if (find_dev == NO_FIND_BLOCK_DEV) {
		printf("Can't find block dev\n");
		err = -1;
		goto CLOSE;
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

static void usage(char *app)
{
	printf("Usage: %s [Options], built on %s %s\n", app, __DATE__, __TIME__);
	printf("\t-t, --type, select storage block device types, types can use usb or sdcard (default type is sdcard)\n");
	printf("\t-c, --circle, loop test\n\n");

	printf("\texample: test_blkdev -t sdcard -c\n");
	printf("\t-u, --usage\n\n");
}

int main(int argc, char **argv) {
	FILE *file = NULL;
	int ret = 0;
	int i = 0;
	int success_num = 0;
	char dev_block[40] = {0};
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
	const char sopts[] = "uct:";
	static const struct option lopts[] = {
		{"usage",   no_argument, 	NULL, 'u'},
		{"circle",  no_argument, 	NULL, 'c'},
		{"type",    required_argument,  NULL, 't'},
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

	ret = obtain_block_dev(dev_block, type);
	if (ret < 0 || strlen(dev_block) == 0) {
		return 0;
	}

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

			printf("Content of %s:\n", filenames[i]);
			while (fgets(content, sizeof(content), file) != NULL) {
				if (strncmp(contents[i], content, strlen(content)) == 0)
					success_num++;
				printf("%s", content);
			}

			printf("\n");
			fclose(file);
		}

		/* Check file conditions */
		for (i = 0; i < 3; i++) {
			file = fopen(filenames[i], "r");
			if (file != NULL) {
				fseek(file, 0, SEEK_END);
				long size = ftell(file);
				if (size > 0) {
					printf("%s is not empty. Size: %ld bytes.\n", filenames[i], size);
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
				printf("File deleted %s\n", filenames[i]);
				success_num++;
			} else {
				printf("Unable to delete the file: %s\n", filenames[i]);
			}
		}

		if (success_num == 9) {
			printf("storage block dev test success\n");
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
