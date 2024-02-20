/*
 * Copyright (c) 2022-2023, Artinchip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/fb.h>
#include <video/artinchip_fb.h>
#include "lvgl/lvgl.h"
#include "lvgl/src/core/lv_refr.h"
#include "lv_port_disp.h"
#include "lv_ge2d.h"
#include "mpp_ge.h"
#include "lv_fbdev.h"

static int draw_fps = 0;
static struct mpp_ge *g_ge = NULL;
static lv_disp_draw_buf_t disp_buf;
static lv_disp_drv_t disp_drv;
static int g_fb = -1;
char *buf_next = NULL;
static int g_triple_fb = 0;

#ifdef USE_DRAW_BUF
static int g_fb_num = 1;
static int g_fb_id = 0;
#endif

#define US_PER_SEC      1000000
static double get_time_gap(struct timeval *start, struct timeval *end)
{
    double diff;

    if (end->tv_usec < start->tv_usec) {
        diff = (double)(US_PER_SEC + end->tv_usec - start->tv_usec)/US_PER_SEC;
        diff += end->tv_sec - 1 - start->tv_sec;
    } else {
        diff = (double)(end->tv_usec - start->tv_usec)/US_PER_SEC;
        diff += end->tv_sec - start->tv_sec;
    }

    return diff;
}

static int cal_fps(double gap, int cnt)
{
    return (int)(cnt / gap);
}

static void cal_frame_rate()
{
    static int start_cal = 0;
    static int frame_cnt = 0;
    static struct timeval start, end;
    double interval = 0.5;
    double gap = 0;

    if (start_cal == 0) {
        start_cal = 1;
        gettimeofday(&start, NULL);
    }

    gettimeofday(&end, NULL);
    gap = get_time_gap(&start, &end);
    if (gap >= interval) {
        draw_fps = cal_fps(gap, frame_cnt);
        frame_cnt = 0;
        start_cal = 0;
    } else {
        frame_cnt++;
    }
    return;
}

void sync_disp_buf(lv_disp_drv_t * drv, lv_color_t * color_p, const lv_area_t * area_p)
{
    int32_t ret;
    struct ge_bitblt blt = { 0 };
    lv_coord_t w = lv_area_get_width(area_p);
    lv_coord_t h = lv_area_get_height(area_p);
    enum mpp_pixel_format fmt = draw_buf_fmt();
    int pitch = draw_buf_pitch();
    int buf_w = disp_drv.hor_res;
    int buf_h = disp_drv.ver_res;

#ifdef USE_DRAW_BUF
    blt.src_buf.buf_type = MPP_DMA_BUF_FD;
    if (drv->draw_buf->buf_act == g_draw_buf[0])
        blt.src_buf.fd[0] = g_draw_buf_fd[0];
    else
        blt.src_buf.fd[0] = g_draw_buf_fd[1];
#else
    blt.src_buf.buf_type = MPP_PHY_ADDR;
    if (drv->draw_buf->buf_act == g_frame_buf[0])
        blt.src_buf.phy_addr[0] = g_frame_phy[0];
    else
        blt.src_buf.phy_addr[0] = g_frame_phy[1];
#endif

    blt.src_buf.stride[0] = pitch;
    blt.src_buf.size.width = buf_w;
    blt.src_buf.size.height = buf_h;
    blt.src_buf.crop_en = 1;
    blt.src_buf.crop.x = area_p->x1;
    blt.src_buf.crop.y = area_p->y1;
    blt.src_buf.crop.width = w;
    blt.src_buf.crop.height = h;
    blt.src_buf.format = fmt;

#ifdef USE_DRAW_BUF
    blt.dst_buf.buf_type = MPP_DMA_BUF_FD;
    if (drv->draw_buf->buf_act == g_draw_buf[0])
        blt.dst_buf.fd[0] = g_draw_buf_fd[1];
    else
        blt.dst_buf.fd[0] = g_draw_buf_fd[0];
#else
    blt.dst_buf.buf_type = MPP_PHY_ADDR;
    if (drv->draw_buf->buf_act == g_frame_buf[0])
        blt.dst_buf.phy_addr[0] = g_frame_phy[1];
    else
        blt.dst_buf.phy_addr[0] = g_frame_phy[0];
#endif

    blt.dst_buf.crop_en = 1;
    blt.dst_buf.crop.x = area_p->x1;
    blt.dst_buf.crop.y = area_p->y1;
    blt.dst_buf.crop.width = w;
    blt.dst_buf.crop.height = h;
    blt.dst_buf.stride[0] = pitch;
    blt.dst_buf.size.width = buf_w;
    blt.dst_buf.size.height = buf_h;
    blt.dst_buf.format = fmt;

    ret = mpp_ge_bitblt(g_ge, &blt);
    if (ret < 0) {
        LV_LOG_ERROR("bitblt fail");
        return;
    }

    ret = mpp_ge_emit(g_ge);
    if (ret < 0) {
        LV_LOG_ERROR("emit fail");
        return;
    }

    ret = mpp_ge_sync(g_ge);
    if (ret < 0) {
        LV_LOG_ERROR("sync fail");
        return;
    }

    return;
}

#ifdef USE_DRAW_BUF
void disp_draw_buf(lv_disp_drv_t * drv, lv_color_t * color_p)
{
    int32_t ret;
    int disp_w;
    int disp_h;
    struct ge_bitblt blt = { 0 };
    enum mpp_pixel_format draw_fmt = draw_buf_fmt();
    int draw_pitch = draw_buf_pitch();
    enum mpp_pixel_format disp_fmt = fbdev_get_fmt();
    int disp_pitch = fbdev_get_pitch();
    int draw_w;
    int draw_h;

    if (drv->rotated == LV_DISP_ROT_90 || drv->rotated == LV_DISP_ROT_270) {
        draw_w = drv->ver_res;
        draw_h = drv->hor_res;
    } else  {
        draw_w = drv->hor_res;
        draw_h = drv->ver_res;
    }

    fbdev_get_size(&disp_w, &disp_h);

    blt.src_buf.buf_type = MPP_DMA_BUF_FD;
    if (color_p == (lv_color_t *)g_draw_buf[0])
        blt.src_buf.fd[0] = g_draw_buf_fd[0];
    else
        blt.src_buf.fd[0] = g_draw_buf_fd[1];

    blt.src_buf.stride[0] = draw_pitch;
    blt.src_buf.size.width = draw_w;
    blt.src_buf.size.height = draw_h;
    blt.src_buf.format =  draw_fmt;

    blt.dst_buf.buf_type = MPP_PHY_ADDR;
    blt.dst_buf.phy_addr[0] = g_frame_phy[g_fb_id];
    blt.dst_buf.stride[0] = disp_pitch;
    blt.dst_buf.size.width = disp_w;
    blt.dst_buf.size.height = disp_h;
    blt.dst_buf.format = disp_fmt;

    /* rotation */
    switch (drv->rotated) {
    case LV_DISP_ROT_NONE:
        blt.ctrl.flags = MPP_ROTATION_0;
        break;
    case LV_DISP_ROT_90:
        blt.ctrl.flags = MPP_ROTATION_90;
        break;
    case LV_DISP_ROT_180:
        blt.ctrl.flags = MPP_ROTATION_180;
        break;
    case LV_DISP_ROT_270:
        blt.ctrl.flags = MPP_ROTATION_270;
        break;
    default:
        break;
    };

    ret = mpp_ge_bitblt(g_ge, &blt);
    if (ret < 0) {
        LV_LOG_ERROR("bitblt fail");
        return;
    }

    ret = mpp_ge_emit(g_ge);
    if (ret < 0) {
        LV_LOG_ERROR("emit fail");
        return;
    }

    ret = mpp_ge_sync(g_ge);
    if (ret < 0) {
        LV_LOG_ERROR("sync fail");
        return;
    }

    return;
}
#endif

static void fbdev_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t *color_p)
{
    lv_disp_t * disp = _lv_refr_get_disp_refreshing();
    lv_disp_draw_buf_t * draw_buf = lv_disp_get_draw_buf(disp);

    if (!disp->driver->direct_mode || draw_buf->flushing_last) {
        struct fb_var_screeninfo var = {0};

        if (ioctl(g_fb, FBIOGET_VSCREENINFO, &var) < 0) {
            LV_LOG_WARN("ioctl FBIOGET_VSCREENINFO");
            return;
        }

#ifdef USE_DRAW_BUF
        int disp_w;
        int disp_h;

        fbdev_get_size(&disp_w, &disp_h);

        var.xoffset = 0;
        var.yoffset = g_fb_id * disp_h;
        // copy draw buf to display buffer
        disp_draw_buf(drv, color_p);
        g_fb_id++;
        if (g_fb_id >= g_fb_num)
            g_fb_id = 0;
#else
        // swap triple frame buffer
        if (buf_next) {
            if (draw_buf->buf1 == color_p)
                draw_buf->buf1 = (lv_color_t *)buf_next;
            else
                draw_buf->buf2 = (lv_color_t *)buf_next;

            draw_buf->buf_act = (lv_color_t *)buf_next;
            buf_next = (char *)color_p;
        }

        if (color_p == (lv_color_t *)g_frame_buf[0]) {
            var.xoffset = 0;
            var.yoffset = 0;
        } else if (color_p == (lv_color_t *)g_frame_buf[2]) {
            var.xoffset = 0;
            var.yoffset = disp_drv.ver_res * 2;
        } else {
            var.xoffset = 0;
            var.yoffset = disp_drv.ver_res;
        }
#endif
        if (ioctl(g_fb, FBIOPAN_DISPLAY, &var) == 0) {
            if (!g_triple_fb) {
                int zero = 0;
                if (ioctl(g_fb, AICFB_WAIT_FOR_VSYNC, &zero) < 0) {
                    LV_LOG_WARN("ioctl AICFB_WAIT_FOR_VSYNC fail");
                    return;
                }
            }
        } else {
            LV_LOG_WARN("pan display err");
        }

#ifndef USE_DRAW_BUF
        if (drv->direct_mode == 1) {
            for (int i = 0; i < disp->inv_p; i++) {
                if (disp->inv_area_joined[i] == 0) {
                    sync_disp_buf(drv, color_p, &disp->inv_areas[i]);
                }
            }
        }
#endif

        cal_frame_rate();
        lv_disp_flush_ready(drv);
    } else {
        lv_disp_flush_ready(drv);
    }
}

void lv_port_disp_init(void)
{
    int width, height;
    void *buf1 = NULL;
    void *buf2 = NULL;

    g_fb = fbdev_open();
    if (g_fb < 0) {
        LV_LOG_ERROR("fbdev_open fail");
        return;
    }

    draw_buf_size(&width, &height);
    ge_open();
    g_ge = get_ge();
    if (!g_ge) {
        LV_LOG_ERROR("ge open fail");
        return;
    }

#ifdef USE_DRAW_BUF
    buf1 = (void *)g_draw_buf[0];
    if (g_frame_buf[1]) {
        g_fb_id = 1;
        g_fb_num = 2;
    }
#ifdef TRIPLE_FRAME_BUF_EN
    if (g_frame_buf[2]) {
        g_fb_id = 1;
        g_fb_num = 3;
    }
#endif // TRIPLE_FRAME_BUF_EN
#else
    buf1 = (void *)g_frame_buf[0];
    buf2 = (void *)g_frame_buf[1];
#endif // USE_DRAW_BUF

    if (!buf2) // single frame buffer
        lv_disp_draw_buf_init(&disp_buf, buf1, 0, width * height);
    else      // double frame buffer
        lv_disp_draw_buf_init(&disp_buf, buf2, buf1, width * height);

    lv_disp_drv_init(&disp_drv);

    /*Set a display buffer*/
    disp_drv.draw_buf = &disp_buf;

    /*Set the resolution of the display*/
    disp_drv.hor_res = width;
    disp_drv.ver_res = height;
    disp_drv.full_refresh = 0;
    disp_drv.direct_mode = 1;
    disp_drv.flush_cb = fbdev_flush;
    disp_drv.draw_ctx_init = lv_draw_aic_ctx_init;
    disp_drv.draw_ctx_deinit = lv_draw_aic_ctx_deinit;
    disp_drv.draw_ctx_size = sizeof(lv_draw_aic_ctx_t);

    /* when define USE_DRAW_BUF, disp_drv.rotated can be
      LV_DISP_ROT_90/LV_DISP_ROT_180/LV_DISP_ROT_270
    */
    //disp_drv.rotated = LV_DISP_ROT_90;
#ifdef TRIPLE_FRAME_BUF_EN
    if (g_frame_buf[2]) {
        disp_drv.full_refresh = 1;
        disp_drv.direct_mode = 0;
        buf_next = g_frame_buf[2];
        g_triple_fb = 1;
    }
#endif

    /*Finally register the driver*/
    lv_disp_drv_register(&disp_drv);
}

void lv_port_disp_exit(void)
{
    if (g_ge) {
        ge_close();
        g_ge = NULL;
    }
    fbdev_close();
}

int fbdev_draw_fps(void)
{
    return (int)draw_fps;
}

int disp_is_swap(void)
{
    if (disp_drv.rotated == LV_DISP_ROT_90 || disp_drv.rotated == LV_DISP_ROT_270)
        return 1;
    else
        return 0;
}
