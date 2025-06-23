/*
 * Copyright (c) 2023-2024, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  ZeQuan Liang <zequan.liang@artinchip.com>
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "bmp.h"
#include "ge_mem.h"

static int cal_bmp_stride(struct bmp_header *bmp_header)
{
	int bytes_per_pixel = 0;

	if (bmp_header->width <= 0 || bmp_header->bit_count <= 0)
		return -1;

	bytes_per_pixel = bmp_header->bit_count / 8;

	return BYTE_ALIGN((bytes_per_pixel * bmp_header->width), 4);
}

static int cal_stride(struct bmp_header *bmp_header)
{
	int bytes_per_pixel = 0;

	if (bmp_header->width <= 0 || bmp_header->bit_count <= 0)
		return -1;

	bytes_per_pixel = bmp_header->bit_count / 8;

	return BYTE_ALIGN((bytes_per_pixel * bmp_header->width), 8);
}

static enum bmp_pixel_format bmp_get_ori_fmt(struct bmp_header *bmp_header)
{
	if (!bmp_header) {
		printf("bmp_header err\n");
		return UNKNOWN_FORMAT;
	}

	switch (bmp_header->bit_count) {
		case 1: {
			return MONOCHROME_FORMAT;
		}
		case 4: {
			if (bmp_header->compression == 0) {
				return INDEXED_4BIT_FORMAT;
			} else if (bmp_header->compression == 2) {
				return RLE_4BIT_FORMAT;
			}
			break;
		}
		case 8: {
			if (bmp_header->compression == 0) {
				return INDEXED_8BIT_FORMAT;
			} else if (bmp_header->compression == 1) {
				return RLE_8BIT_FORMAT;
			} else if (bmp_header->compression == 2) {
				return BITFIELDS_FORMAT;
			}
			break;
		}
		case 16: {
			if (bmp_header->compression == 0) {
				return RGB_565_FORMAT;
			} else if (bmp_header->compression == 3) {
				return BITFIELDS_FORMAT;
			}
			break;
		}
		case 24: {
			if (bmp_header->compression == 0) {
				return BGR_FORMAT;
			}
			break;
		}
		case 32: {
			if (bmp_header->compression == 0) {
				return BGRA_FORMAT;
			} else if (bmp_header->compression == 3) {
				return ARGB_FORMAT;
			}
			break;
		}
		default: {
			printf("format not supported\n");
			return UNKNOWN_FORMAT;
		}
	}

	printf("format not supported\n");
	return UNKNOWN_FORMAT;
}

enum mpp_pixel_format bmp_get_fmt(struct bmp_header *bmp_header)
{
	enum bmp_pixel_format bmp_ori_fmt = 0;
	enum mpp_pixel_format mpp_fmt = 0;

	bmp_ori_fmt = bmp_get_ori_fmt(bmp_header);
	if (bmp_ori_fmt != RGB_565_FORMAT && bmp_ori_fmt != BGR_FORMAT &&
		bmp_ori_fmt != BGRA_FORMAT && bmp_ori_fmt != ARGB_FORMAT) {
		printf("don't support bmp fmt\n");
		return -1;
	}

	/* change bmp fmt, enable it to display correctly */
	switch (bmp_ori_fmt)
	{
	case BGR_FORMAT:
		mpp_fmt = MPP_FMT_RGB_888;
		break;
	case RGB_565_FORMAT:
		mpp_fmt = MPP_FMT_RGB_565;
		break;
	/* MPP fmt is litte-endian */
	case BGRA_FORMAT:
		mpp_fmt = MPP_FMT_ARGB_8888;
		break;
	case ARGB_FORMAT:
		mpp_fmt = MPP_FMT_RGBA_8888;
		break;
	default:
		break;
	}

	return mpp_fmt;
}

int bmp_open(char * path, struct bmp_header *bmp_header)
{
	int fd = 0;

	fd = open(path, O_RDONLY);

	if (fd < 0) {
		printf("open bmp fail\n");
		return -1;
	}

	if (read(fd, bmp_header, sizeof(struct bmp_header)) != sizeof(struct bmp_header)) {
		printf("read bmp file error \n");
		return -1;
	}

	return fd;
}

void bmp_close(int fd)
{
	if (fd > 0)
		close(fd);
}

/* read bmp image to the buffers */
int bmp_read(int bmp_fd, void *buffer, struct bmp_header *bmp_header)
{
	int i = 0;
	int stride = 0;
	int bmp_stride = 0;
	int height = 0;
	unsigned char *buf = NULL;

	if (bmp_header->type != 0x4D42) {
		printf("not a Bmp file\n");
		return -1;
	}

	if (bmp_fd < 0) {
		printf("bmp_fd err\n");
		return -1;
	}

	bmp_stride = cal_bmp_stride(bmp_header);
	if (bmp_stride < 0) {
		printf("cal bmp stride err\n");
		return -1;
	}

	stride = cal_stride(bmp_header);
	if (stride < 0) {
		printf("cal stride err\n");
		return -1;
	}

	height = abs(bmp_header->height);
	/* read image from bottom to top */
	if (bmp_header->height < 0) {
		buf = buffer;
		for (i = 0; i < height; i++) {
			if (read(bmp_fd, buf, bmp_stride) != bmp_stride) {
				printf("read bmp to buffer error\n");
				return -1;
			}
			buf += stride;
		}
	/* read image from top to bottom */
	} else {
		for (i = height - 1; i >= 0; i--) {
			buf = buffer + (i * stride);
			if (read(bmp_fd, buf, bmp_stride) != bmp_stride) {
				printf("read bmp to buffer error\n");
				return -1;
			}
		}
	}

	return 0;
}
