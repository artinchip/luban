/*
 * Copyright (C) 2024-2025, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#include <stdio.h>
#include <stdbool.h>
#include <getopt.h>
#include <string.h>
#include <sys/time.h>
#include <stddef.h>
#include <sys/mman.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <video/artinchip_fb.h>
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_ge2d/lv_draw_ge2d.h"
#include "lv_mpp_dec/lv_mpp_dec.h"
#include "frame_allocator.h"
#include "dma_allocator.h"

#define ALIGN_8B(x) (((x) + (7)) & ~(7))

#define NS_PER_SEC      1000000000
static double get_time_gap(struct timespec *start, struct timespec *end)
{
    double diff;

    if (end->tv_nsec < start->tv_nsec) {
        diff = (double)(NS_PER_SEC + end->tv_nsec - start->tv_nsec) / NS_PER_SEC;
        diff += end->tv_sec - 1 - start->tv_sec;
    } else {
        diff = (double)(end->tv_nsec - start->tv_nsec) / NS_PER_SEC;
        diff += end->tv_sec - start->tv_sec;
    }

    return diff;
}

static int cal_fps(double gap, int cnt)
{
    return (int)(cnt / gap);
}

static void display_cal_frame_rate(aic_disp_t *aic_disp)
{
#if DISP_SHOW_FPS == 1
    static int start_cal = 0;
    static int frame_cnt = 0;
    static struct timespec start, end;
    double interval = 0.5;
    double gap = 0;

    if (start_cal == 0) {
        start_cal = 1;
        clock_gettime(CLOCK_MONOTONIC, &start);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    gap = get_time_gap(&start, &end);
    if (gap >= interval) {
        aic_disp->fps = cal_fps(gap, frame_cnt);
        frame_cnt = 0;
        start_cal = 0;
    } else {
        frame_cnt++;
    }
#else
    (void)aic_disp;
#endif
    return;
}

static inline void *get_fb_buf_by_id(aic_disp_t *aic_disp, int id)
{
    if (id == 1)
        return (void *)(aic_disp->buf_mapped + aic_disp->fb_size);
    else
        return (void *)aic_disp->buf_mapped;
}

static inline void wait_sync_ready(aic_disp_t *aic_disp)
{
    while (aic_disp->sync_ready == false) {
        if (aic_disp->exit_status)
            break;

        lv_thread_sync_wait(&aic_disp->sync_notify);
    }
}

static inline void disp_do_blit(aic_disp_t *aic_disp, lv_display_t *disp, lv_draw_buf_t *disp_buf)
{
    void *dest_buf = get_fb_buf_by_id(aic_disp, aic_disp->buf_id);
    int32_t hor_res = lv_display_get_horizontal_resolution(disp);
    int32_t ver_res = lv_display_get_vertical_resolution(disp);
    lv_color_format_t cf = lv_display_get_color_format(disp);
    int32_t src_stride = lv_draw_buf_width_to_stride(hor_res, cf);
    int32_t dst_stride = (int32_t)aic_disp->finfo.line_length;
    lv_display_rotation_t rotation = lv_display_get_rotation(disp);
#if LV_USE_DRAW_GE2D
    lv_draw_ge2d_rotate(disp_buf->data, dest_buf, hor_res, ver_res,
                        src_stride, dst_stride, rotation, cf);
#else
    lv_draw_sw_rotate(disp_buf->data, dest_buf, hor_res, ver_res,
                      src_stride, dst_stride, rotation, cf);
#endif
}

static inline void disp_draw_buf_flush(aic_disp_t *aic_disp, lv_display_t *disp, lv_draw_buf_t *disp_buf)
{
    if (aic_disp->double_buf) {
        wait_sync_ready(aic_disp);
        aic_disp->sync_ready = false;
        disp_do_blit(aic_disp, disp, disp_buf);

        struct fb_var_screeninfo var;
        memcpy(&var, &aic_disp->vinfo, sizeof(struct fb_var_screeninfo));

        if(aic_disp->buf_id == 0) {
            var.xoffset = 0;
            var.yoffset = 0;
        } else {
            var.xoffset = 0;
            var.yoffset = aic_disp->vinfo.yres;
        }

        // send framebuffer to display
        ioctl(aic_disp->fb, FBIOPAN_DISPLAY, &var);

        aic_disp->flush_act = true;
        lv_thread_sync_signal(&aic_disp->sync);
    } else {
        // wait vsync
        int zero = 0;
        ioctl(aic_disp->fb, AICFB_WAIT_FOR_VSYNC, &zero);
        disp_do_blit(aic_disp, disp, disp_buf);
    }
}

static void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    aic_disp_t *aic_disp = (aic_disp_t *)lv_display_get_user_data(disp);

    lv_draw_buf_t *disp_buf = lv_display_get_buf_active(disp);
    (void)px_map;

    if (lv_disp_flush_is_last(disp)) {
        if (disp_buf->data == aic_disp->buf) {
            if (aic_disp->double_buf)
                aic_disp->buf_id  =  aic_disp->buf_id > 0 ? 0 : 1;
            else
                aic_disp->buf_id = 0;

            disp_draw_buf_flush(aic_disp, disp, disp_buf);
        } else {
            struct fb_var_screeninfo var;
            memcpy(&var, &aic_disp->vinfo, sizeof(struct fb_var_screeninfo));

            if(disp_buf->data == aic_disp->buf_mapped) {
                var.xoffset = 0;
                var.yoffset = 0;
            } else {
                var.xoffset = 0;
                var.yoffset = aic_disp->vinfo.yres;
            }

            // send framebuffer to display
            ioctl(aic_disp->fb, FBIOPAN_DISPLAY, &var);

            // wait vsync
            int zero = 0;
            if (ioctl(aic_disp->fb, AICFB_WAIT_FOR_VSYNC, &zero) < 0) {
                LV_LOG_WARN("ioctl AICFB_WAIT_FOR_VSYNC fail");
                return;
            }
        }
        display_cal_frame_rate(aic_disp);
    }

    lv_display_flush_ready(disp);
}

static lv_color_format_t lv_display_fmt(int pixel_bits)
{
    lv_color_format_t fmt = LV_COLOR_FORMAT_ARGB8888;
    switch(pixel_bits) {
        case 16:
            fmt = LV_COLOR_FORMAT_RGB565;
            break;
        case 24:
            fmt = LV_COLOR_FORMAT_RGB888;
            break;
        case 32:
            fmt = LV_COLOR_FORMAT_ARGB8888;
            break;
        default:
            LV_LOG_ERROR("unsupported pixel bits:%d", pixel_bits);
            break;
    }
    return fmt;
}

static void aic_display_thread(void *ptr)
{
    aic_disp_t *aic_disp = (aic_disp_t *)ptr;

    while(1) {
        while (aic_disp->flush_act == false) {
            if(aic_disp->exit_status)
                break;

            lv_thread_sync_wait(&aic_disp->sync);
        }

        aic_disp->flush_act = false;
        
        // wait vsync
        int zero = 0;
        ioctl(aic_disp->fb, AICFB_WAIT_FOR_VSYNC, &zero);

        aic_disp->sync_ready = true;
        lv_thread_sync_signal(&aic_disp->sync_notify);

        if (aic_disp->exit_status) {
            LV_LOG_INFO("Ready to exit aic display thread.");
            break;
        }
    }

    lv_thread_sync_delete(&aic_disp->sync);
    lv_thread_sync_delete(&aic_disp->sync_notify);
    LV_LOG_INFO("Exit aic display thread.");
}

static uint8_t *create_draw_buf(aic_disp_t *aic_disp, int w, int h, lv_color_format_t cf)
{
    int bpp = lv_color_format_get_bpp(cf) / 8;
    int buf_size = ALIGN_8B(w) * ALIGN_8B(h) * bpp;
    int dma_fd = dmabuf_device_open();
    aic_disp->fd = dmabuf_alloc(dma_fd, buf_size);
    if (aic_disp->fd < 0) {
        goto alloc_error;
    } else {
        aic_disp->buf = (uint8_t *)dmabuf_mmap(aic_disp->fd, buf_size);
        if (aic_disp->buf == MAP_FAILED) {
            goto alloc_error;
        }
    }

    dmabuf_device_close(dma_fd);
    aic_disp->buf_size = buf_size;
    return aic_disp->buf;
alloc_error:
    dmabuf_device_close(dma_fd);

    if (aic_disp->buf != MAP_FAILED && aic_disp->buf) {
        dmabuf_munmap((unsigned char*)aic_disp->buf, aic_disp->buf_size);
        aic_disp->buf = NULL;
        dmabuf_free(aic_disp->fd);
    }

    return NULL;
}

void lv_port_disp_init(void)
{
    void *buf1;
    void *buf2;
    aic_disp_t *aic_disp;
    int fb_size;
    int width;
    int height;
    int stride;
    lv_color_format_t cf;

#if LV_USE_MPP_DEC
    lv_mpp_dec_init();
#endif

#if LV_USE_DRAW_GE2D
    lv_draw_ge2d_init();
#endif

    aic_disp = (aic_disp_t *)lv_malloc_zeroed(sizeof(aic_disp_t));
    if (!aic_disp) {
        LV_LOG_ERROR("malloc aic display failed");
        return;
    }

    aic_disp->fb = open(FBDEV_PATH, O_RDWR);
    if (aic_disp->fb > 0) {
        if (ioctl(aic_disp->fb, FBIOGET_FSCREENINFO, &aic_disp->finfo) == -1) {
            LV_LOG_ERROR("Error reading fixed information");
            close(aic_disp->fb);
            lv_free(aic_disp);
            return;
        }

        if (ioctl(aic_disp->fb, FBIOGET_VSCREENINFO, &aic_disp->vinfo) == -1) {
            LV_LOG_ERROR("Error reading variable information");
            close(aic_disp->fb);
            lv_free(aic_disp);
            return;
        }

        uint8_t *buf_mapped = (uint8_t *)mmap(0, aic_disp->finfo.smem_len,
                            PROT_READ | PROT_WRITE, MAP_SHARED, aic_disp->fb, 0);
        if (buf_mapped == MAP_FAILED) {
            LV_LOG_ERROR("Error: failed to map framebuffer device to memory");
            close(aic_disp->fb);
            lv_free(aic_disp);
            return;
        }

        aic_disp->buf_mapped = buf_mapped;
        width = aic_disp->vinfo.xres;
        height = aic_disp->vinfo.yres;
        stride = aic_disp->finfo.line_length;
        fb_size = height * stride;
        aic_disp->fb_size = fb_size;

        if (aic_disp->finfo.smem_len >= fb_size * 2) {
            buf1 = (void *)(buf_mapped + fb_size);
            buf2 = (void *)buf_mapped;
            aic_disp->double_buf = true;
        } else {
            buf1 = (void *)buf_mapped;
            buf2 = NULL;
        }
    } else {
        LV_LOG_ERROR("get device fb info failed");
        close(aic_disp->fb);
        lv_free(aic_disp);
        return;
    }

    cf = lv_display_fmt(aic_disp->vinfo.bits_per_pixel);

    aic_disp->rotate_en = false;
    aic_disp->rotate_degree = 0;

#if defined(LV_DISPLAY_ROTATE_EN)
    aic_disp->rotate_en = true;
#endif

#if defined(LV_ROTATE_DEGREE)
    aic_disp->rotate_degree = LV_ROTATE_DEGREE / 90;
#endif

    if (aic_disp->rotate_en) {
        if (!create_draw_buf(aic_disp, width, height, cf)) {
            LV_LOG_ERROR("create draw buffer failed");
            close(aic_disp->fb);
            lv_free(aic_disp);
            return;
        } else {
            buf1 = (void *)aic_disp->buf;
            buf2 = NULL;
            aic_disp->buf_id = 0;
        }
    }

    lv_display_t *disp = lv_display_create(width, height);
    lv_display_set_color_format(disp, cf);
    lv_display_set_flush_cb(disp, disp_flush);
    lv_display_set_user_data(disp, aic_disp);
    lv_display_set_buffers(disp, buf1, buf2, fb_size, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_rotation(disp, aic_disp->rotate_degree);

    if (aic_disp->rotate_en && aic_disp->double_buf) {
        aic_disp->sync_ready = true;
        lv_thread_sync_init(&aic_disp->sync);
        lv_thread_sync_init(&aic_disp->sync_notify);
        lv_thread_init(&aic_disp->thread, LV_THREAD_PRIO_HIGH, aic_display_thread, 8 * 1024, aic_disp);
    }
}
