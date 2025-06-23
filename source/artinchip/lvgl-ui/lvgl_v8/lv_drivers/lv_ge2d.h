/*
 * Copyright (C) 2022-2023 ArtinChip Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#ifndef LV_GE2D_H
#define LV_GE2D_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/src/misc/lv_color.h"
#include "lvgl/src/hal/lv_hal_disp.h"
#include "lvgl/src/draw/sw/lv_draw_sw.h"

/* pixel number */
#ifndef LV_GE_FILL_SIZE_LIMIT
#define LV_GE_FILL_SIZE_LIMIT 60000
#endif

#ifndef LV_GE_FILL_OPA_SIZE_LIMIT
#define LV_GE_FILL_OPA_SIZE_LIMIT 30000
#endif

typedef lv_draw_sw_ctx_t lv_draw_aic_ctx_t;

struct _lv_disp_drv_t;

void lv_draw_aic_ctx_init(struct _lv_disp_drv_t * drv, lv_draw_ctx_t * draw_ctx);

void lv_draw_aic_ctx_deinit(struct _lv_disp_drv_t * drv, lv_draw_ctx_t * draw_ctx);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_GE2D_H*/
