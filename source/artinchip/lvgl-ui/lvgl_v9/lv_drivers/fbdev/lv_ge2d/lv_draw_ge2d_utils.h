/*
 * Copyright (C) 2024-2025 ArtInChip Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#ifndef LV_DRAW_GE2D_UTILS_H
#define LV_DRAW_GE2D_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <video/mpp_types.h>
#include "lvgl.h"
#include "../../lvgl/src/display/lv_display_private.h"
#include "../lv_port_disp.h"

#define CHECK_FD_VALID

static inline bool disp_buf_check(uint8_t *buf)
{
    lv_display_t *disp = lv_display_get_default();
    if (disp->_static_buf1.data == buf || disp->_static_buf2.data == buf)
        return true;
    else
        return false;
}

static inline unsigned int disp_buf_phy_addr(uint8_t *buf)
{
    lv_display_t *disp = lv_display_get_default();
    aic_disp_t *aic_disp = (aic_disp_t *)lv_display_get_user_data(disp);

    if (aic_disp->buf_mapped == buf) {
        return (unsigned int)aic_disp->finfo.smem_start;
    } else {
        return (unsigned int)(aic_disp->finfo.smem_start + aic_disp->fb_size);
    }
}

static inline int disp_buf_fd(uint8_t *buf)
{
    lv_display_t *disp = lv_display_get_default();
    aic_disp_t *aic_disp = (aic_disp_t *)lv_display_get_user_data(disp);

#ifdef CHECK_FD_VALID
    if (aic_disp->buf != buf) {
        LV_LOG_ERROR("Invalid buf:%p != %p", aic_disp->buf, buf);
    }
#endif
    return aic_disp->fd;
}

static inline enum mpp_buf_type disp_buf_type(void)
{
    lv_display_t *disp = lv_display_get_default();
    aic_disp_t *aic_disp = (aic_disp_t *)lv_display_get_user_data(disp);
    if (aic_disp->fd > 0)
        return MPP_DMA_BUF_FD;
    else
        return MPP_PHY_ADDR;
}

static inline bool ge2d_dst_fmt_supported(lv_color_format_t cf)
{
    bool supported = false;

    switch(cf) {
        case LV_COLOR_FORMAT_RGB565:
        case LV_COLOR_FORMAT_RGB888:
        case LV_COLOR_FORMAT_ARGB8888:
        case LV_COLOR_FORMAT_XRGB8888:
            supported = true;
            break;
        default:
            break;
    }

    return supported;
}

static inline bool ge2d_src_fmt_supported(lv_color_format_t cf)
{
    bool supported = false;

    switch(cf) {
        case LV_COLOR_FORMAT_RGB565:
        case LV_COLOR_FORMAT_RGB888:
        case LV_COLOR_FORMAT_ARGB8888:
        case LV_COLOR_FORMAT_XRGB8888:
        case LV_COLOR_FORMAT_I420:
        case LV_COLOR_FORMAT_I422:
        case LV_COLOR_FORMAT_I444:
        case LV_COLOR_FORMAT_I400:
        case LV_COLOR_FORMAT_RAW:
            supported = true;
            break;
        default:
            break;
    }

    return supported;
}

static inline bool lv_fmt_is_yuv(lv_color_format_t fmt)
{
    if (fmt >= LV_COLOR_FORMAT_YUV_START)
        return true;
    else
        return false;
}

static inline bool lv_fmt_is_mpp_buf(uint32_t flags)
{
    if (flags & LV_IMAGE_FLAGS_USER8)
        return true;
    else
        return false;
}

enum mpp_pixel_format lv_fmt_to_mpp_fmt(lv_color_format_t cf);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_DRAW_GE2D_UTILS_H*/
