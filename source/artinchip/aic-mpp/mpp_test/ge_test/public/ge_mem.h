/*
 * Copyright (c) 2023-2024, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  ZeQuan Liang <zequan.liang@artinchip.com>
 */

#ifndef GE_MEM_CAL_H
#define GE_MEM_CAL_H

#include "mpp_ge.h"

#define BYTE_ALIGN(x, byte) (((x) + ((byte) - 1))&(~((byte) - 1)))

struct ge_buf {
	struct mpp_buf buf;
};

int ge_dmabuf_open(void);
void ge_dmabuf_close(void);

struct ge_buf * ge_buf_malloc(int width, int height, enum mpp_pixel_format fmt);
void ge_buf_free(struct ge_buf * buffer);
void ge_buf_clean_dcache(struct ge_buf * buffer);

void ge_buf_argb8888_printf(char *map_mem, struct ge_buf *g_buf);
#endif
