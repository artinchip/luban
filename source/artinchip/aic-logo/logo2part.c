// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2023 Artinchip Technology Co., Ltd.
 * Authors:  Huahui <huahui.mai@artinchip.com>
 */

#include <artinchip/sample_base.h>
#include <errno.h>
#include <mtd/mtd-user.h>

#include "image.h"

/* Global macro and variables */

#define MTD_DEV "/dev/mtd"

/* cmd-line flags */
#define FLAG_NONE		0x00
#define FLAG_LIST		(0x1 << 1)
#define FLAG_SAVE		(0x1 << 2)
#define FLAG_IMAGE		(0x1 << 3)

#define DEV_TYPE_NONE		0
#define DEV_TYPE_BLK		1
#define DEV_TYPE_MTD		2

static const char sopts[] = "i:p:slu";
static const struct option lopts[] = {
	{"input",	  required_argument, NULL, 'i'},
	{"part",	  required_argument, NULL, 'p'},
	{"list",		no_argument, NULL, 'l'},
	{"save",		no_argument, NULL, 's'},
	{"usage",		no_argument, NULL, 'u'},
	{0, 0, 0, 0}
};

struct file_object {
	int fd;
	unsigned int type;
	unsigned int fsize;
	unsigned char *buf;
	struct mtd_info_user mtdinfo;
};

void usage(char *program)
{
	printf("Usage: %s [options]: \n", program);
	printf("\t -i, --input\t\tneed a file \n");
	printf("\t -p, --part\t\tpart device \n");
	printf("\t -l, --list\t\tlist logo part image \n");
	printf("\t -s, --save\t\tsave logo.itb file \n");
	printf("\t -u, --usage \n");
	printf("\n");
	printf("Example: %s -i boot_logo.png -p /dev/mtd7 \n", program);
}

static unsigned int part_type(const char *path)
{
	unsigned int type = DEV_TYPE_NONE;

	if (!strncmp(path, MTD_DEV, strlen(MTD_DEV)))
		type = DEV_TYPE_MTD;
	else if (strlen(path) > 0)
		type = DEV_TYPE_BLK;

	return type;
}

static int part_read(struct file_object *part)
{
	unsigned char *buf;
	int ret = 0;

	buf = malloc(part->fsize);
	if (!buf) {
		ERR("malloc buf failed\n");
		return -ENOMEM;
	}
	part->buf = buf;

	ret = read(part->fd, buf, part->fsize);
	if (ret != part->fsize) {
		ERR("read part failed, ret(%d)\n", ret);
		part->buf = NULL;
		free(buf);
		return -EIO;
	}

	return 0;
}

struct file_object *part_open(const char *path)
{
	struct file_object *part;
	int ret;

	part = malloc(sizeof(struct file_object));
	if (!part) {
		ERR("malloc part object failed\n");
		return NULL;
	}
	memset(part, 0, sizeof(*part));

	part->type = part_type(path);
	if (part->type == DEV_TYPE_NONE) {
		ERR("device path is not correct\n");
		goto out;
	}

	part->fd = open(path, O_RDWR);
	if (part->fd < 0) {
		ERR("Failed to open %s\n", path);
		goto out;
	}

	if (part->type == DEV_TYPE_MTD) {
		ret = ioctl(part->fd, MEMGETINFO, &part->mtdinfo);
		if (ret < 0 || (part->mtdinfo.type != MTD_NORFLASH &&
				part->mtdinfo.type != MTD_NANDFLASH)) {
			ERR("ioctl MEMGETINFO failed\n");
			goto out;
		}

		part->fsize = part->mtdinfo.size;
	} else {
		part->fsize = lseek(part->fd, 0, SEEK_END);
		if (part->fsize < 0) {
			ERR("get mmc part size failed\n");
			goto out;
		}

		lseek(part->fd, 0, SEEK_SET);
	}

	ret = part_read(part);
	if (ret)
		goto out;

	return part;
out:
	if (part->fd > 0)
		close(part->fd);
	if (part)
		free(part);
	return NULL;
}

static void part_close(struct file_object *part)
{
	if (!part)
		return;

	if (part->fd > 0)
		close(part->fd);

	if (part->buf)
		free(part->buf);
}

struct file_object *image_open(const char *name)
{
	struct file_object *image;
	struct stat imagestat;

	image = malloc(sizeof(struct file_object));
	if (!image) {
		ERR("malloc image object failed\n");
		return NULL;
	}
	memset(image, 0x0, sizeof(*image));

	image->fd = open(name, O_RDWR);
	if (image->fd < 0) {
		ERR("Failed to open %s. errno: %d[%s]\n",
			name, errno, strerror(errno));
		goto out;
	}

	if (fstat (image->fd, &imagestat) < 0) {
		ERR("image get fstat failed\n");
		goto out;
	}
	image->fsize = imagestat.st_size;
	DBG("open(%s) %d, size %d\n",
		name, image->fd, image->fsize);

	image->buf = mmap(NULL, image->fsize,
		PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED,
		image->fd, 0);
	if (image->buf == MAP_FAILED) {
		ERR("mmap image failed, error: %d[%s]\n",
			errno, strerror(errno));
		goto out;
	}
	return image;

out:
	if (image->fd > 0)
		close(image->fd);

	if (image)
		free(image);
	return NULL;
}

static void image_close(struct file_object *image)
{
	if (!image)
		return;

	if (image->fd > 0)
		close(image->fd);

	if (image->buf)
		munmap(image->buf, image->fsize);
}

static void itb_file_save(struct file_object *part)
{
	FILE* fp_out;

	fp_out= fopen("logo.itb", "wb");
	fwrite(part->buf, 1, fdt_totalsize(part->buf), fp_out);
	fclose(fp_out);
}

int fix_itb_file(struct file_object *part, struct file_object *image)
{
	const char *name = "boot";
	int noffset, ret;
	size_t data_size;
	const void *data;
	int ovcopylen;
	struct fdt_property *prop;
	unsigned char *dst, *src;
	unsigned char *end = part->buf + fdt_totalsize(part->buf);

	/* Get specific logo image offset */
	noffset = fit_image_get_node(part->buf, name);
	if (noffset < 0) {
		ERR("Failed to get %s node\n", name);
		return noffset;
	}

	/* Load specific logo image */
	ret = fit_image_get_data_and_size(part->buf, noffset, &data, &data_size);
	if (ret < 0) {
		ERR("Failed to get data\n");
		return ret;
	}

	/* Update data size */
	prop = fdt_get_property_w(part->buf, noffset, "data", NULL);
	if (!prop) {
		ERR("get data property failed\n");
		return -FDT_ERR_INTERNAL;
	}
	prop->len = cpu_to_fdt32(image->fsize);

	ovcopylen = FDT_TAGALIGN(image->fsize) - FDT_TAGALIGN(data_size);
	if (ovcopylen > 0) {
		src = (unsigned char *)data;
		dst = (unsigned char *)data + ovcopylen;
	} else {
		src = (unsigned char *)data + FDT_TAGALIGN(data_size);
		dst = src + ovcopylen;
	}

	memmove(dst, src, end - src);
	memcpy((void *)data, image->buf, image->fsize);

	fdt_set_totalsize(part->buf, fdt_totalsize(part->buf) + ovcopylen);
	fdt_set_size_dt_struct(part->buf, fdt_size_dt_struct(part->buf) + ovcopylen);
	fdt_set_off_dt_strings(part->buf, fdt_off_dt_strings(part->buf) + ovcopylen);
	return 0;
}

int itb_to_part(struct file_object *part)
{
	size_t size;
	int ret;

	if (part->type == DEV_TYPE_MTD) {
		struct erase_info_user erase;

		/* erase all */
		erase.start = 0;
		erase.length = part->mtdinfo.size;

		if (ioctl(part->fd, MEMERASE, &erase) < 0) {
			ERR("While erasing blocks 0x%.8x-0x%.8x on mtd7\n",
				(unsigned int) erase.start,
				(unsigned int) (erase.start + erase.length));
			return -EIO;
		}
	}

	size = fdt_totalsize(part->buf);
	lseek(part->fd, 0, SEEK_SET);

	ret = write(part->fd, part->buf, size);
	if (ret != size) {
		ERR("write new itb failed\n");
		return -EIO;
	}

	return 0;
}

int main(int argc, char **argv)
{
	int c = 0, flags = FLAG_NONE;
	struct file_object *part = NULL;
	struct file_object *image = NULL;

	while ((c = getopt_long(argc, argv, sopts, lopts, NULL)) != -1) {
		switch (c) {
		case 'i':
			image = image_open(optarg);
			if (image)
				flags |= FLAG_IMAGE;
			break;
		case 'p':
			part = part_open(optarg);
			break;
		case 'l':
			flags |= FLAG_LIST;
			break;
		case 's':
			flags |= FLAG_SAVE;
			break;
		case 'u':
			usage(argv[0]);
			return 0;
		default:
			usage(argv[0]);
			return 0;
		}
	}

	if (!part) {
		ERR("device path is not correct\n");
		goto out;
	}

	if (flags & FLAG_LIST)
		fit_print_contents(part->buf);

	if (flags & FLAG_IMAGE && !fix_itb_file(part, image))
		itb_to_part(part);

	if (flags & FLAG_SAVE)
		itb_file_save(part);

out:
	part_close(part);
	image_close(image);
	return 0;
}
