/*
 * Copyright (C) 2024-2025 ArtInChip Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#ifndef LV_MPP_DEC_H
#define LV_MPP_DEC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

#ifndef LV_USE_MPP_DEC
#define LV_USE_MPP_DEC 1
#endif

#ifndef LV_CACHE_IMG_NUM_LIMIT
#define LV_CACHE_IMG_NUM_LIMIT 1
#endif

#ifndef LV_CACHE_IMG_NUM
#define LV_CACHE_IMG_NUM 8
#endif

#ifndef MPP_JPEG_DEC_OUT_SIZE_LIMIT_ENABLE
#define MPP_JPEG_DEC_OUT_SIZE_LIMIT_ENABLE
#endif

#ifndef MPP_JPEG_DEC_MAX_OUT_WIDTH
#define MPP_JPEG_DEC_MAX_OUT_WIDTH  2048
#endif

#ifndef MPP_JPEG_DEC_MAX_OUT_HEIGHT
#define MPP_JPEG_DEC_MAX_OUT_HEIGHT 2048
#endif

typedef struct {
    lv_draw_buf_t decoded;
    struct mpp_buf dec_buf;
    uint8_t *data[3];
    int size[3];
    bool cached;
} mpp_decoder_data_t;

static inline int jpeg_width_limit(int width)
{
    int r = 0;

    while(width > MPP_JPEG_DEC_MAX_OUT_WIDTH) {
        width = width >> 1;
        r++;

        if (r == 3) {
            break;
        }
    }
    return r;
}

static inline int jpeg_height_limit(int height)
{
    int r = 0;

    while(height > MPP_JPEG_DEC_MAX_OUT_HEIGHT) {
        height = height >> 1;
        r++;

        if (r == 3) {
            break;
        }
    }
    return r;
}

static inline int jpeg_size_limit(int w, int h)
{
    return LV_MAX(jpeg_width_limit(w), jpeg_height_limit(h));
}

void lv_mpp_dec_init(void);

void lv_mpp_dec_deinit(void);

bool lv_drop_one_cached_image();

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif //LV_MPP_DEC_H
