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

#include "mpp_ge.h"
#include "./public/bmp.h"
#include "./public/ge_mem.h"

static void ge_process_fill_set(struct ge_fillrect *fill, struct ge_buf *src_buf)
{
	memset(fill, 0, sizeof(struct ge_fillrect));

	memcpy(&fill->dst_buf, &src_buf->buf, sizeof(struct mpp_buf));

	fill->type = GE_NO_GRADIENT;
	fill->start_color = 0xffffffff;
}

int main(int argc, char **argv)
{
	int ret = -1;
	int map_size = -1;
	char *map_mem = MAP_FAILED;
	struct mpp_ge *ge = NULL;
	struct ge_buf *src_buf = NULL;
	struct ge_fillrect fill;

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

	ge_process_fill_set(&fill, src_buf);

	printf("fill before\n");
	ge_buf_argb8888_printf(map_mem, src_buf);

	mpp_ge_add_dmabuf(ge, src_buf->buf.fd[0]);

	ret =  mpp_ge_fillrect(ge, &fill);
	if (ret < 0) {
		printf("ge fillrect fail\n");
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

	printf("fill after\n");
	ge_buf_argb8888_printf(map_mem, src_buf);
EXIT:
	if (map_mem != MAP_FAILED)
		munmap(map_mem, map_size);

	if (src_buf)
		ge_buf_free(src_buf);

	if (ge)
		mpp_ge_close(ge);

	ge_dmabuf_close();
}
