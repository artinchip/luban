/*
 * Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <qi.xu@artinchip.com>
 *  Desc: png test (use libpng)
 */

#include <stdio.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <sys/time.h>
#include "png.h"

#define LOG_TAG "pngtest"
#define TAG_ERROR	"error  "
#define TAG_WARNING	"warning"
#define TAG_INFO	"info   "
#define TAG_DEBUG	"debug  "
#define TAG_VERBOSE	"verbose"

#define mpp_log(level, tag, fmt, arg...) ({ \
		printf("%s: %s <%s:%d>: "fmt"\n", tag, LOG_TAG, __FUNCTION__, __LINE__, ##arg); \
	})

#define loge(fmt, arg...) mpp_log(LOGL_ERROR, TAG_ERROR, "\033[40;31m"fmt"\033[0m", ##arg)
#define logw(fmt, arg...) mpp_log(LOGL_WARNING, TAG_WARNING, "\033[40;33m"fmt"\033[0m", ##arg)
#define logi(fmt, arg...) mpp_log(LOGL_INFO, TAG_INFO, "\033[40;32m"fmt"\033[0m", ##arg)
#define logd(fmt, arg...) mpp_log(LOGL_DEBUG, TAG_DEBUG, fmt, ##arg)
#define logv(fmt, arg...) mpp_log(LOGL_VERBOSE, TAG_VERBOSE, fmt, ##arg)

#define __TIC__(tag) struct timeval time_##tag##_start; gettimeofday(&time_##tag##_start, NULL)
#define __TOC__(tag) struct timeval time_##tag##_end;gettimeofday(&time_##tag##_end, NULL);\
			fprintf(stderr, #tag " time: %ld us\n",\
			((time_##tag##_end.tv_sec - time_##tag##_start.tv_sec)*1000000) +\
			(time_##tag##_end.tv_usec - time_##tag##_start.tv_usec))

int decode_png(FILE *fp)
{
	int i;
	png_structp 	png_ptr;
	png_infop	info_ptr;
	FILE* fp_out = NULL;
	int channels, color_type;
	int width, height, bit_depth;

	logi("png version: %s", PNG_LIBPNG_VER_STRING);
	//* 1. init struct
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	info_ptr = png_create_info_struct(png_ptr);

	//* 2. set error
	setjmp(png_jmpbuf(png_ptr));
	rewind(fp);

	//* 3. band
	png_init_io(png_ptr, fp);

	//* 4. read png info
	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_EXPAND, 0);
	channels = png_get_channels(png_ptr, info_ptr);
	color_type = png_get_color_type(png_ptr, info_ptr);
	bit_depth = png_get_bit_depth(png_ptr, info_ptr);
	width = png_get_image_width(png_ptr, info_ptr);
	height = png_get_image_height(png_ptr, info_ptr);
	logi("channels = %d color_type = %d bit_depth = %d width = %d height = %d",
		channels, color_type, bit_depth, width, height);

	//* 5. save rgb data
	png_bytepp row_pointers;
	row_pointers = png_get_rows(png_ptr, info_ptr);
	if (channels == 4 || color_type == PNG_COLOR_TYPE_RGB_ALPHA) {
		logw("rgba");
	} else if (channels == 3 || color_type == PNG_COLOR_TYPE_RGB) {
		logw("rgb");
	}
	else
		return -1;

	fp_out = fopen("out.rgb", "wb");
	for (i = 0; i < height; i++)
		fwrite(row_pointers[i], 1, width * channels, fp_out);
	fclose(fp_out);

	//* 6. destroy
	png_destroy_read_struct(&png_ptr, &info_ptr, 0);
}

static void print_help(void)
{
	printf("Usage: png_test [options]:\n"
		"   -i                             input png file name\n"
		"   -h                             help\n\n"
		"Example: png_test -i test.png\n");
}

int main(int argc, char *argv[])
{
	int opt;
	FILE* fp = NULL;

	if(argc < 2) {
		print_help();
		return -1;
	}

	while (1) {
		opt = getopt(argc, argv, "i:h");
		if (opt == -1) {
			break;
		}
		switch (opt) {
		case 'i':
			fp = fopen(optarg, "rb");
			if(fp == NULL) {
				loge("open png(%s) failed", optarg);
				return -1;
			}
			break;
		case 'h':
		default:
			print_help();
			return -1;
		}
	}

	if(fp == NULL) {
		print_help();
		return -1;
	}

	__TIC__(decode_png);
	decode_png(fp);
	__TOC__(decode_png);

	fclose(fp);
	return 0;
}
