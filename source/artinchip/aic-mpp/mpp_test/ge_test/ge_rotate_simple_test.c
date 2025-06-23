/*
 * Copyright (c) 2023-2024, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  ZeQuan Liang <zequan.liang@artinchip.com>
 */

#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/mman.h>
#include <math.h>

#include "mpp_ge.h"
#include "./public/bmp.h"
#include "./public/ge_mem.h"

#define PI 3.14159265
#define SIN(x) (sin((x) * PI / 180.0))
#define COS(x) (cos((x) * PI / 180.0))

static void ge_process_rotate_set(struct ge_rotation *rot, struct ge_buf *src_buf, struct ge_buf *dst_buf)
{
	double degree = 0.0;

	memset(rot, 0, sizeof(struct ge_rotation));

	memcpy(&rot->src_buf, &src_buf->buf, sizeof(struct mpp_buf));
	rot->src_rot_center.x = 0;
	rot->src_rot_center.y = 0;

	memcpy(&rot->dst_buf, &dst_buf->buf, sizeof(struct mpp_buf));
	rot->dst_rot_center.x = 0;
	rot->dst_rot_center.y = 0;

	degree = 180.0;
	rot->angle_sin = (int)(SIN(degree) * 4096);
	rot->angle_cos = (int)(COS(degree) * 4096);
}

int main(int argc, char **argv)
{
	int ret = -1;

	int map_size = -1;
	char *map_mem = MAP_FAILED;
	struct mpp_ge *ge = NULL;
	struct ge_buf *src_buf = NULL;
	struct ge_buf *dst_buf = NULL;
	struct ge_rotation rot;

	ge = mpp_ge_open();
	if (!ge) {
		printf("open ge device error\n");
		goto EXIT;
	}

	if (ge_dmabuf_open() < 0) {
		printf("ge_dmabuf_open open error\n");
		goto EXIT;
	}

	src_buf = ge_buf_malloc(10, 10, MPP_FMT_ARGB_8888);
	if (src_buf == NULL) {
		printf("malloc src_buf error\n");
		goto EXIT;
	}
	map_size = src_buf->buf.stride[0] * src_buf->buf.size.height;
	map_mem = (char *)mmap(NULL, map_size,
        PROT_READ | PROT_WRITE, MAP_SHARED, src_buf->buf.fd[0], 0);
	if (map_mem == MAP_FAILED) {
		printf("mmap error\n");
		goto EXIT;
	}
	memset(map_mem, 0x0, map_size);
	for (int col = 0; col < src_buf->buf.stride[0]; col += 4) {
		unsigned int *pixel = (unsigned int *)(map_mem + col + 0 * src_buf->buf.stride[0]);
		*pixel = 0xffffffff;
	}
	munmap(map_mem, map_size);

	dst_buf = ge_buf_malloc(10, 10, MPP_FMT_ARGB_8888);
	if (dst_buf == NULL) {
		printf("malloc dst_buf error\n");
		goto EXIT;
	}

	map_size = dst_buf->buf.stride[0] * dst_buf->buf.size.height;
	map_mem = (char *)mmap(NULL, map_size,
							PROT_READ | PROT_WRITE, MAP_SHARED, dst_buf->buf.fd[0], 0);
	if (map_mem == MAP_FAILED) {
		printf("mmap error\n");
		goto EXIT;
	}
	memset(map_mem, 0x0, map_size);
	for (int col = 0; col < dst_buf->buf.stride[0]; col += 4) {
		unsigned int *pixel = (unsigned int *)(map_mem + col + 1 * dst_buf->buf.stride[0]);
		*pixel = 0xffffffff;
	}

	printf("rotate before\n");
	ge_buf_argb8888_printf(map_mem, dst_buf);

	ge_process_rotate_set(&rot, src_buf, dst_buf);

	mpp_ge_add_dmabuf(ge, src_buf->buf.fd[0]);
	mpp_ge_add_dmabuf(ge, dst_buf->buf.fd[0]);
	ret =  mpp_ge_rotate(ge, &rot);
	if (ret < 0) {
		printf("ge rotate fail\n");
		goto EXIT;
	}

	ret = mpp_ge_emit(ge);
	if (ret < 0) {
		printf("ge emit fail\n");
		goto EXIT;
	}

	ret = mpp_ge_sync(ge);
	if (ret < 0) {
		printf("ge sync fail\n");
	}
	mpp_ge_rm_dmabuf(ge, src_buf->buf.fd[0]);
	mpp_ge_rm_dmabuf(ge, dst_buf->buf.fd[0]);

	printf("rotate after\n");
	ge_buf_argb8888_printf(map_mem, dst_buf);
EXIT:
	if (map_mem != MAP_FAILED)
		munmap(map_mem, map_size);

	if (src_buf)
		ge_buf_free(src_buf);

	if (dst_buf)
		ge_buf_free(dst_buf);

	if (ge)
		mpp_ge_close(ge);

	ge_dmabuf_close();
}
