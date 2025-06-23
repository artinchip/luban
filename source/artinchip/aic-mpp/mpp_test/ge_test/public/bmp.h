/*
 * Copyright (c) 2023-2024, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  ZeQuan Liang <zequan.liang@artinchip.com>
 */

#ifndef BMP_H_
#define BMP_H_

#include "mpp_ge.h"

#define BYTE_ALIGN(x, byte) (((x) + ((byte) - 1))&(~((byte) - 1)))

#pragma pack(push, 1)
struct bmp_header {
	unsigned short type;
	unsigned int size;
	unsigned short reserved1;
	unsigned short reserved2;
	unsigned int offset;

	unsigned int head_size;
	int width;
	int height;
	unsigned short planes;
	unsigned short bit_count;
	unsigned int compression;
	unsigned int size_image;
	int x_meter;
	int y_meter;
	unsigned int clr_used;
	unsigned int clr_important;
};
#pragma pack(pop)

enum bmp_pixel_format{
	UNKNOWN_FORMAT = 100,
	MONOCHROME_FORMAT,
	INDEXED_4BIT_FORMAT,
	RLE_4BIT_FORMAT,
	INDEXED_8BIT_FORMAT,
	RLE_8BIT_FORMAT,
	BITFIELDS_FORMAT,
	RGB_565_FORMAT = 14,
	BGR_FORMAT = 9,
	/* here is big-endian */
	BGRA_FORMAT = 3,
	ARGB_FORMAT = 0
};

int bmp_open(char * path, struct bmp_header *bmp_header);
int bmp_read(int bmp_fd, void *buffer, struct bmp_header *bmp_header);
enum mpp_pixel_format bmp_get_fmt(struct bmp_header *bmp_header);
void bmp_close(int fd);

#endif
