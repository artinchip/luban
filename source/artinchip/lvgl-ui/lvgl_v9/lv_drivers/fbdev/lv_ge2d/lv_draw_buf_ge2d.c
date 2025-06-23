/*
 * Copyright (C) 2024-2025 ArtInChip Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#include "lv_draw_ge2d.h"

#if LV_USE_DRAW_GE2D

void lv_draw_buf_ge2d_init_handlers(void)
{
    lv_draw_buf_handlers_t * handlers = lv_draw_buf_get_handlers();

    handlers->invalidate_cache_cb = NULL;
}

#endif /*LV_USE_DRAW_GE2D*/
