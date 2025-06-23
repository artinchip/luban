/*
 * Copyright (c) 2022-2025, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/fb.h>
#include "lvgl.h"

#ifndef DISP_SHOW_FPS
#define DISP_SHOW_FPS 1
#endif

#ifndef FBDEV_PATH
#define FBDEV_PATH  "/dev/fb0"
#endif

typedef struct {
    int fb;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    lv_thread_t thread;
    lv_thread_sync_t sync;
    lv_thread_sync_t sync_notify;
    bool exit_status;
    bool sync_ready;
    bool flush_act;
    uint8_t *buf; /* draw buffer */
    int fd; /* draw buffer fd */
    int buf_id;
    bool rotate_en;
    int rotate_degree;
    uint8_t *buf_mapped;
    bool double_buf;
    int  buf_size;
    int fb_size;

    int fps;
} aic_disp_t;

void lv_port_disp_init(void);

static inline int fbdev_draw_fps()
{
#if DISP_SHOW_FPS
    lv_display_t *disp = lv_display_get_default();
    aic_disp_t *aic_disp = (aic_disp_t *)lv_display_get_user_data(disp);
    return aic_disp->fps;
#else
    return 0;
#endif
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_PORT_DISP_H*/
