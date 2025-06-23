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
#include <signal.h>
#include <errno.h>
#include <artinchip/sample_base.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>

#include "ge_mem.h"

#define DMA_HEAP_DEV	"/dev/dma_heap/reserved"
static int ge_dmabuf_fd = -1;

/* input ge stride requires 8-bytes alignment */
static void ge_cal_stride(enum mpp_pixel_format fmt, int input_width, int stride[])
{
	switch (fmt) {
	case MPP_FMT_ARGB_8888:
	case MPP_FMT_ABGR_8888:
	case MPP_FMT_RGBA_8888:
	case MPP_FMT_BGRA_8888:
	case MPP_FMT_XRGB_8888:
	case MPP_FMT_XBGR_8888:
	case MPP_FMT_RGBX_8888:
	case MPP_FMT_BGRX_8888:
		stride[0] = BYTE_ALIGN((input_width * 4), 8);
		stride[1] = 0;
		stride[2] = 0;
		break;
	case MPP_FMT_ARGB_4444:
	case MPP_FMT_ABGR_4444:
	case MPP_FMT_RGBA_4444:
	case MPP_FMT_BGRA_4444:
	case MPP_FMT_RGB_565:
	case MPP_FMT_BGR_565:
		stride[0] = BYTE_ALIGN((input_width * 2), 8);
		stride[1] = 0;
		stride[2] = 0;
		break;
	case MPP_FMT_ARGB_1555:
	case MPP_FMT_ABGR_1555:
	case MPP_FMT_RGBA_5551:
	case MPP_FMT_BGRA_5551:
	case MPP_FMT_RGB_888:
	case MPP_FMT_BGR_888:
		stride[0] = BYTE_ALIGN((input_width * 3), 8);
		stride[1] = 0;
		stride[2] = 0;
		break;
	case MPP_FMT_YUV420P:
		stride[0]  = BYTE_ALIGN((input_width), 8);
		stride[1] = BYTE_ALIGN((input_width / 2), 8);
		stride[2] = BYTE_ALIGN((input_width / 2), 8);
		break;
	case MPP_FMT_NV21:
	case MPP_FMT_NV12:
		stride[0]  = BYTE_ALIGN((input_width), 8);
		stride[1] = BYTE_ALIGN((input_width), 8);
		stride[2] = 0;
		break;
	case MPP_FMT_YUV422P:
		stride[0]  = BYTE_ALIGN((input_width), 8);
		stride[1] = BYTE_ALIGN((input_width / 2), 8);
		stride[2] = BYTE_ALIGN((input_width / 2), 8);
		break;
	case MPP_FMT_NV16:
	case MPP_FMT_NV61:
		stride[0]  = BYTE_ALIGN((input_width), 8);
		stride[1] = BYTE_ALIGN((input_width), 8);
		stride[2] = 0;
		break;
	case MPP_FMT_YUYV:
	case MPP_FMT_YVYU:
	case MPP_FMT_UYVY:
	case MPP_FMT_VYUY:
		stride[0]  = BYTE_ALIGN((input_width * 2), 8);
		stride[1] = 0;
		stride[2] = 0;
		break;
	case MPP_FMT_YUV400:
		stride[0]  = BYTE_ALIGN((input_width), 8);
		stride[1] = 0;
		stride[2] = 0;
		break;
	case MPP_FMT_YUV444P:
		stride[0]  = BYTE_ALIGN((input_width), 8);
		stride[1] = BYTE_ALIGN((input_width), 8);
		stride[2] = BYTE_ALIGN((input_width), 8);
		break;
	default:
		printf("input format error\n");
		break;
	}

}

static void ge_cal_height(enum mpp_pixel_format fmt, int input_height, int height[])
{
	if ((fmt >= MPP_FMT_ARGB_8888) && (fmt <= MPP_FMT_BGRA_4444)) {
		height[0] = input_height;
		height[1] = 0;
		height[2] = 0;
		return;
	}

	switch (fmt) {
	case MPP_FMT_YUV420P:
		height[0]  = BYTE_ALIGN((input_height), 2);
		height[1] = BYTE_ALIGN((input_height / 2), 2);
		height[2] = BYTE_ALIGN((input_height / 2), 2);
		break;
	case MPP_FMT_NV21:
	case MPP_FMT_NV12:
		height[0] = BYTE_ALIGN((input_height), 2);
		height[1] = BYTE_ALIGN((input_height / 2), 2);
		height[2] = 0;
		break;
	case MPP_FMT_YUV422P:
		height[0] = BYTE_ALIGN(input_height, 2);
		height[1] = BYTE_ALIGN(input_height, 2);
		height[2] = BYTE_ALIGN(input_height, 2);
		break;
	case MPP_FMT_NV16:
	case MPP_FMT_NV61:
		height[0] = BYTE_ALIGN(input_height, 2);
		height[1] = BYTE_ALIGN(input_height, 2);
		height[2] = 0;
		break;
	case MPP_FMT_YUYV:
	case MPP_FMT_YVYU:
	case MPP_FMT_UYVY:
	case MPP_FMT_VYUY:
		height[0] = BYTE_ALIGN(input_height, 2);
		height[1] = 0;
		height[2] = 0;
		break;
	case MPP_FMT_YUV400:
		height[0] = BYTE_ALIGN((input_height), 2);
		height[1] = 0;
		height[2] = 0;
		break;
	case MPP_FMT_YUV444P:
		height[0] = BYTE_ALIGN(input_height, 2);
		height[1] = BYTE_ALIGN(input_height, 2);
		height[2] = BYTE_ALIGN(input_height, 2);
		break;
	default:
		printf("input format error\n");
		break;
	}

}

int ge_dmabuf_open(void)
{
	ge_dmabuf_fd = open(DMA_HEAP_DEV, O_RDWR);
	if (ge_dmabuf_fd < 0) {
		printf("failed to open %s, errno: %d[%s]\n",
			DMA_HEAP_DEV, errno, strerror(errno));
		return -1;
	}

	return 0;
}

void ge_dmabuf_close(void)
{
	if (ge_dmabuf_fd > 0)
		close(ge_dmabuf_fd);
	ge_dmabuf_fd = -1;
}

struct ge_buf * ge_buf_malloc(int width, int height, enum mpp_pixel_format fmt)
{
	int i = 0;
	int align_height[3] = {0};

	struct dma_heap_allocation_data data;
	struct ge_buf * buffer;

	if (ge_dmabuf_fd < 0) {
		printf("dmabuf fd < 0, please open ge_dmabuf_open first\n");
		return NULL;
	}

	buffer = (struct ge_buf *)malloc(sizeof(struct ge_buf));
	if (!buffer)
		return NULL;

	memset(buffer, 0, sizeof(struct ge_buf));

	ge_cal_stride(fmt, width, (int *)buffer->buf.stride);
	ge_cal_height(fmt, height, align_height);

	buffer->buf.buf_type = MPP_DMA_BUF_FD;
	buffer->buf.size.width = width;
	buffer->buf.size.height = height;
	buffer->buf.format = fmt;

	for (i = 0; i < 3 && buffer->buf.stride[i] != 0; i++) {
		if (buffer->buf.stride[i] * align_height[i] == 0)
			continue;

		memset(&data, 0, sizeof(struct dma_heap_allocation_data));
		data.len = buffer->buf.stride[i] * align_height[i];
		data.fd = 0;
		data.fd_flags = O_RDWR | O_CLOEXEC;
		data.heap_flags = 0;

		if (ioctl(ge_dmabuf_fd, DMA_HEAP_IOCTL_ALLOC, &data) < 0) {
			printf("ioctl() failed! errno: %d[%s]\n", errno, strerror(errno));
			return NULL;
		}
		buffer->buf.fd[i] = data.fd;
	}

	return buffer;
}

void ge_buf_free(struct ge_buf * buffer)
{
	if (!buffer)
		return;
	int i;

	for (i = 0; i < 3; i++) {
		if (buffer->buf.fd[i] > 0)
			close(buffer->buf.fd[i]);
	}
}

void ge_buf_clean_dcache(struct ge_buf * buffer)
{
	return;
}

void ge_buf_argb8888_printf(char *map_mem, struct ge_buf *g_buf)
{
	if (!map_mem || !g_buf)
		return;

	for (int i = 0; i < g_buf->buf.size.height; i++) {
		for (int j = 0; j < g_buf->buf.stride[0]; j += 4) {
			printf("%08x ", *(unsigned int *)(map_mem + j + i * g_buf->buf.stride[0]));
		}
		printf("\n");
	}
}
