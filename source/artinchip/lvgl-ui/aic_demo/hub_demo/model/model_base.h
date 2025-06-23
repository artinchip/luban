/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Zequan Liang <zequan.liang@artinchip.com>
 */

#ifndef _AIC_DEMO_MODEL_BASE_H
#define _AIC_DEMO_MODEL_BASE_H

#ifdef __cplusplus
extern "C" {
#endif

int get_fb_draw_fps(void);
float get_mem_usage(void); /* unit:  % */
float get_cpu_usage(void); /* unit: MB */

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
