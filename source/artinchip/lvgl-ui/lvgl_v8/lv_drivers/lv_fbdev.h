/*
 * Copyright (C) 2022-2024, ArtInchip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef LV_FBDEV_H
#define LV_FBDEV_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include <video/mpp_types.h>

/*
  1. if define USE_DRAW_BUF, lvgl will alloc draw buffer instead of sharing framebuffer
  2. draw with/height can be different with framebuffer
  3. draw buffer can be scaled/rotated and output to framebuffer
  4. draw buffer can be rotated 0/90/180/270 degree
     (1) the default rotation of your display when it is initialized can be using the rotated flag.
         the avaliable options are LV_DISP_ROT_NONE,LV_DISP_ROT_90,LV_DISP_ROT_180 or LV_DISP_ROT_270
     (2) display rotaiton can alse be changed at runtime using lv_disp_set_roation(disp, rot) API.

*/

#if defined(LV_DISPLAY_ROTATE_EN)
#ifndef USE_DRAW_BUF
#define USE_DRAW_BUF
#endif // USE_DRAW_BUF

#ifndef DRAW_BUF_WIDTH
#define DRAW_BUF_WIDTH fbdev_get_screen_width()
#endif // DRAW_BUF_WIDTH

#ifndef DRAW_BUF_HEIGHT
#define DRAW_BUF_HEIGHT fbdev_get_screen_height()
#endif // DRAW_BUF_HEIGHT

#endif // LV_DISPLAY_ROTATE_EN

#ifndef FBDEV_PATH
#define FBDEV_PATH  "/dev/fb0"
#endif

#define MAX_FRAME_NUM 3
extern char *g_frame_buf[MAX_FRAME_NUM];
extern unsigned int g_frame_phy[MAX_FRAME_NUM];
extern int g_draw_buf_fd[2];
extern char *g_draw_buf[2];

struct mpp_ge;

int fbdev_open(void);

void fbdev_close(void);

int fbdev_get_size(int *width, int *height);

enum mpp_pixel_format fbdev_get_fmt(void);

int fbdev_get_bpp(void);

int fbdev_get_pitch(void);

#ifdef USE_CACHE_FRAMEBUFFER
void fbdev_buf_sync(unsigned char *buf);
#endif

int draw_buf_size(int *width, int *height);

unsigned int fbdev_get_screen_width();

unsigned int fbdev_get_screen_height();
enum mpp_pixel_format draw_buf_fmt(void);

int draw_buf_bpp(void);

int draw_buf_pitch(void);

void ge_open(void);

struct mpp_ge *get_ge(void);

void ge_close(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_FBDEV_H*/
