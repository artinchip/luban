/*
 * Copyright (c) 2022-2023, Artinchip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <stddef.h>
#include <sys/mman.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/fb.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <errno.h>
#include "lv_fbdev.h"
#include "mpp_ge.h"
#include "lvgl/lvgl.h"
#include "lv_port_disp.h"

static struct mpp_ge *g_ge = NULL;
static int g_fb = -1;
static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
static int g_buf_num = 1;

char *g_frame_buf[MAX_FRAME_NUM] = { NULL };
unsigned int g_frame_phy[MAX_FRAME_NUM] = { 0 };
int g_draw_buf_fd[2] = { -1 };
char *g_draw_buf[2] = { NULL };

#ifdef USE_DRAW_BUF
#define DMA_HEAP_DEV       "/dev/dma_heap/mpp"
#define DRAW_BUF_STRIDE    ((DRAW_BUF_WIDTH * (LV_COLOR_DEPTH / 8) + 7) & (~7))
#define DRAW_BUF_SWAP_STRIDE    ((DRAW_BUF_HEIGHT * (LV_COLOR_DEPTH / 8) + 7) & (~7))
#define DRAW_BUF_SIZE      (DRAW_BUF_STRIDE * DRAW_BUF_HEIGHT)

static int draw_buf_alloc(int id)
{
    int ret;
    struct dma_heap_allocation_data data = { 0 };
    int heap_fd = open(DMA_HEAP_DEV, O_RDWR);
    if (heap_fd < 0) {
        LV_LOG_ERROR("Failed to open %s, errno: %d[%s]\n",
                     DMA_HEAP_DEV, errno, strerror(errno));
        goto failed;
    }
    data.fd = 0;
    data.len = DRAW_BUF_SIZE;
    data.fd_flags = O_RDWR | O_CLOEXEC;
    data.heap_flags = 0;
    ret = ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &data);
    if (ret < 0) {
        LV_LOG_ERROR("ioctl() failed! errno: %d[%s]\n",
                     errno, strerror(errno));
        goto failed;
    }
    g_draw_buf[id] = mmap(NULL, data.len, PROT_READ|PROT_WRITE, MAP_SHARED, data.fd, 0);
    if (g_draw_buf[id] == MAP_FAILED) {
        LV_LOG_ERROR("mmap() failed! errno: %d[%s]\n",
                     errno, strerror(errno));
        goto failed;
    }
    g_draw_buf_fd[id] = data.fd;

failed:
    if (heap_fd >=0)
        close(heap_fd);

    if (g_draw_buf_fd[id] < 0 && data.fd >= 0) {
        close(data.fd);
        data.fd = -1;
    }
    return 0;
}

static int draw_buf_free(int id)
{
    if (g_draw_buf_fd[id] >= 0) {
        munmap(g_draw_buf[id], DRAW_BUF_SIZE);
        close(g_draw_buf_fd[id]);
        g_draw_buf_fd[id] = -1;
    }
    return 0;
}

#endif

int fbdev_open(void)
{
    char *fbp;

    g_fb = open(FBDEV_PATH, O_RDWR);
    if(g_fb == -1) {
        LV_LOG_ERROR("can't find aic framebuffer device!");
        return -1;
    }
    LV_LOG_INFO("The framebuffer device was opened successfully");

    if (ioctl(g_fb, FBIOGET_FSCREENINFO, &finfo) == -1) {
        LV_LOG_ERROR("Error reading fixed information");
        return -1;
    }

    if (ioctl(g_fb, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        LV_LOG_ERROR("Error reading variable information");
        return -1;
    }
    LV_LOG_INFO("%dx%d, %d", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);

    fbp = (char *)mmap(0, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, g_fb, 0);
    if((intptr_t)fbp == -1) {
        LV_LOG_ERROR("Error: failed to map framebuffer device to memory");
        return -1;
    }

    g_frame_buf[0] = fbp;
    g_frame_phy[0] = finfo.smem_start;

    // double frame buffer
    if (finfo.smem_len >= vinfo.yres * finfo.line_length * 2) {
        g_frame_buf[1] = g_frame_buf[0] + vinfo.yres * finfo.line_length;
        g_frame_phy[1] = g_frame_phy[0] + vinfo.yres * finfo.line_length;
    }

    // triple frame buffer
    if (finfo.smem_len >= vinfo.yres * finfo.line_length * 3) {
        g_frame_buf[2] = g_frame_buf[1] + vinfo.yres * finfo.line_length;
        g_frame_phy[2] = g_frame_phy[1] + vinfo.yres * finfo.line_length;
    }

    // only use one draw buffer right now
    g_buf_num = 1;
#ifdef USE_DRAW_BUF
    int i;
    for (i = 0; i < g_buf_num; i++)
        draw_buf_alloc(i);
#endif

    return g_fb;
}

int fbdev_get_size(int *width, int *height)
{
    *width = vinfo.xres;
    *height = vinfo.yres;
    return 0;
}

enum mpp_pixel_format fbdev_get_fmt(void)
{
    if (vinfo.bits_per_pixel == 32)
        return MPP_FMT_ARGB_8888;
    else if (vinfo.bits_per_pixel == 24)
        return MPP_FMT_RGB_888;
    else if (vinfo.bits_per_pixel == 16)
        return MPP_FMT_RGB_565;

    return MPP_FMT_ARGB_8888;
}

int fbdev_get_bpp(void)
{
    return vinfo.bits_per_pixel;
}

int fbdev_get_pitch(void)
{
    return finfo.line_length;
}

int draw_buf_size(int *width, int *height)
{
#ifdef USE_DRAW_BUF
    *width = DRAW_BUF_WIDTH;
    *height = DRAW_BUF_HEIGHT;
#else
    *width = vinfo.xres;
    *height = vinfo.yres;
#endif
    return 0;
}

enum mpp_pixel_format draw_buf_fmt(void)
{
    if (vinfo.bits_per_pixel == 32)
        return MPP_FMT_ARGB_8888;
    else if (vinfo.bits_per_pixel == 24)
        return MPP_FMT_RGB_888;
    else if (vinfo.bits_per_pixel == 16)
        return MPP_FMT_RGB_565;

    return MPP_FMT_ARGB_8888;
}

int draw_buf_bpp(void)
{
    return vinfo.bits_per_pixel;
}

int draw_buf_pitch(void)
{
#ifdef USE_DRAW_BUF
    if(disp_is_swap())
        return DRAW_BUF_SWAP_STRIDE;
    else
        return DRAW_BUF_STRIDE;
#else
    return finfo.line_length;
#endif
}

void fbdev_close(void)
{
    if (g_fb >= 0) {
        close(g_fb);
        g_fb = -1;
    }

#ifdef USE_DRAW_BUF
    int i;
    for (i = 0; i < g_buf_num; i++)
        draw_buf_free(i);
#endif
}

void ge_open(void)
{
    g_ge = mpp_ge_open();
    if (!g_ge) {
        LV_LOG_ERROR("ge_open fail");
    }
}

struct mpp_ge *get_ge(void)
{
    return g_ge;
}

void ge_close(void)
{
    if (g_ge) {
        mpp_ge_close(g_ge);
        g_ge = NULL;
    }
}
