/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Zequan Liang <zequan.liang@artinchip.com>
 */

#ifndef _TK_AIC_GRAPHIC_BUFFER_H
#define _TK_AIC_GRAPHIC_BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef WITH_AIC_G2D
#include "base/bitmap.h"

ret_t aic_graphic_buffer_create_for_bitmap(bitmap_t* bitmap);

#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _TK_AIC_GRAPHIC_BUFFER_H */
