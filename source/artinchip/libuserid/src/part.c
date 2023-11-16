/*
 * Copyright (C) 2020-2023 Artinchip Technology Co., Ltd.
 * Authors:  Wu Dehuang <dehuang.wu@artinchip.com>
 */
#include <unistd.h>
#include <stdint.h>
#include <sys/uio.h>
#include <asm/types.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/user.h>
#include <time.h>
#include <linux/random.h>
#include <limits.h>
#include <sys/types.h>
#include <zlib.h>
#include <mtd/mtd-user.h>
#include "list.h"
#include "internal.h"

#define DEV_TYPE_NONE 0
#define DEV_TYPE_BLK 1
#define DEV_TYPE_MTD 2

#define DEVICE_MTD_NAME "/dev/mtd"

struct part_fd {
	int fd;
	int type;
	struct mtd_info_user mtdinfo;
};

static int userid_part_type(char const *path)
{
	int type = DEV_TYPE_NONE;

	if (!strncmp(path, DEVICE_MTD_NAME, strlen(DEVICE_MTD_NAME)))
		type = DEV_TYPE_MTD;
	else if (strlen(path) > 0)
		type = DEV_TYPE_BLK;

	return type;
}

static ssize_t mmc_read(struct part_fd *part, loff_t offset, void *buf,
			size_t count)
{
	lseek(part->fd, offset, SEEK_SET);
	return read(part->fd, buf, count);
}

static ssize_t nor_read(struct part_fd *part, loff_t offset, void *buf,
			size_t count)
{
	lseek(part->fd, offset, SEEK_SET);
	return read(part->fd, buf, count);
}

static ssize_t nand_read(struct part_fd *part, loff_t offset, void *buf,
			 size_t count)
{
	size_t rdsize, dosize;
	unsigned char *p;
	int bad;

	rdsize = count;
	p = buf;
	while (rdsize > 0) {
		bad = ioctl(part->fd, MEMGETBADBLOCK, &offset);
		if (bad < 0)
			return -EIO;
		if (bad > 0) {
			offset += part->mtdinfo.erasesize;
			continue;
		}
		lseek(part->fd, offset, SEEK_SET);
		dosize = rdsize;
		if (rdsize >= part->mtdinfo.erasesize)
			dosize = part->mtdinfo.erasesize;
		if (read(part->fd, p, dosize) != dosize)
			return -EIO;
		p += dosize;
		offset += dosize;
		rdsize -= dosize;
	}
	return count;
}

static ssize_t part_read(struct part_fd *part, loff_t offset, void *buf,
			 size_t count)
{
	if (part->type == DEV_TYPE_BLK)
		return mmc_read(part, offset, buf, count);
	if (part->type == DEV_TYPE_MTD && part->mtdinfo.type == MTD_NORFLASH)
		return nor_read(part, offset, buf, count);
	if (part->type == DEV_TYPE_MTD && part->mtdinfo.type == MTD_NANDFLASH)
		return nand_read(part, offset, buf, count);
	return -1;
}

int userid_part_load(char const *path, uint8_t **buf)
{
	struct userid_storage_header head;
	struct part_fd part;
	uint32_t total_len;
	uint8_t data[4096];
	int ret;

	if (!path) {
		fprintf(stderr, "device path is not provided.\n");
		return -EINVAL;
	}

	memset(&part, 0, sizeof(part));
	part.type = userid_part_type(path);
	if (part.type == DEV_TYPE_NONE) {
		fprintf(stderr, "device path is not correct.\n");
		return -EINVAL;
	}
	part.fd = open(path, O_RDONLY);
	if (part.fd < 0) {
		fprintf(stderr, "Failed to open %s.\n", path);
		return -EIO;
	}

	if (part.type == DEV_TYPE_MTD) {
		ret = ioctl(part.fd, MEMGETINFO, &part.mtdinfo);
		if (ret < 0 || (part.mtdinfo.type != MTD_NORFLASH &&
				part.mtdinfo.type != MTD_NANDFLASH)) {
			ret = -EBADF;
			goto out;
		}
	}

	ret = 0;
	*buf = NULL;
	ret = part_read(&part, 0, data, 4096);
	if (ret <= 0) {
		fprintf(stderr, "Failed to read data.\n");
		ret = -EIO;
		goto out;
	}

	memcpy(&head, data, sizeof(head));
	if (head.magic != USERID_HEADER_MAGIC) {
		ret = 0;
		goto out;
	}

	total_len = head.total_length + 8;
	if (total_len <= 4096) {
		*buf = malloc(4096);
		if (*buf == 0) {
			ret = 0;
			fprintf(stderr, "Failed to malloc buffer.\n");
			goto out;
		}
		memcpy(*buf, data, 4096);
		ret = total_len;
	} else {
		*buf = malloc(total_len + 8);
		if (*buf == 0) {
			ret = 0;
			fprintf(stderr, "Failed to malloc buffer.\n");
			goto out;
		}
		ret = part_read(&part, 0, *buf, total_len);
		if (ret <= 0) {
			free(*buf);
			*buf = NULL;
			ret = -EIO;
			fprintf(stderr, "Failed to read data.\n");
			goto out;
		}
		ret = total_len;
	}
out:
	close(part.fd);

	return ret;
}

static ssize_t mmc_write(struct part_fd *part, loff_t offset, void *buf,
			 size_t count)
{
	lseek(part->fd, offset, SEEK_SET);
	return write(part->fd, buf, count);
}

static ssize_t nand_nor_write(struct part_fd *part, loff_t offset, void *buf,
			      size_t count)
{
	struct erase_info_user erase;
	size_t rdsize, dosize;
	unsigned char *p;
	int bad;

	rdsize = count;
	p = buf;
	while (rdsize > 0) {
		if (part->mtdinfo.type == MTD_NANDFLASH) {
			bad = ioctl(part->fd, MEMGETBADBLOCK, &offset);
			if (bad < 0)
				return -EIO;
			if (bad > 0) {
				offset += part->mtdinfo.erasesize;
				continue;
			}
		}
		if ((offset % part->mtdinfo.erasesize) == 0) {
			erase.length = part->mtdinfo.erasesize;
			erase.start = offset;

			if (ioctl(part->fd, MEMERASE, &erase) != 0)
				return -EIO;
		}

		lseek(part->fd, offset, SEEK_SET);
		dosize = rdsize;
		if (rdsize >= part->mtdinfo.erasesize)
			dosize = part->mtdinfo.erasesize;
		if (write(part->fd, p, dosize) != dosize)
			return -EIO;
		p += dosize;
		offset += dosize;
		rdsize -= dosize;
	}
	return count;
}

static ssize_t part_write(struct part_fd *part, loff_t offset, void *buf,
			  size_t count)
{
	if (part->type == DEV_TYPE_BLK)
		return mmc_write(part, offset, buf, count);
	if (part->type == DEV_TYPE_MTD)
		return nand_nor_write(part, offset, buf, count);
	return -1;
}


int userid_part_save(char const *path, uint8_t *buf, int len)
{
	struct part_fd part;
	int ret = 0;

	if (!path) {
		fprintf(stderr, "device path is not provided.\n");
		return -EINVAL;
	}

	memset(&part, 0, sizeof(part));
	part.type = userid_part_type(path);
	if (part.type == DEV_TYPE_NONE) {
		fprintf(stderr, "device path is not correct.\n");
		return -EINVAL;
	}
	part.fd = open(path, O_RDWR);
	if (part.fd < 0) {
		fprintf(stderr, "Failed to open %s.\n", path);
		return -EIO;
	}

	if (part.type == DEV_TYPE_MTD) {
		ret = ioctl(part.fd, MEMGETINFO, &part.mtdinfo);
		if (ret < 0 || (part.mtdinfo.type != MTD_NORFLASH &&
				part.mtdinfo.type != MTD_NANDFLASH)) {
			ret = -EBADF;
			goto out;
		}
	}

	ret = part_write(&part, 0, buf, len);
out:
	close(part.fd);
	return ret;
}
