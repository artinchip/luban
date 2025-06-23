/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Zequan Liang <zequan.liang@artinchip.com>
 */

#ifndef _AIC_DEMO_COMP_VIDEO_H
#define _AIC_DEMO_COMP_VIDEO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../ui.h"

lv_obj_t *ui_video_btn_icon_comp_create(lv_obj_t *parent, char *img_path,
                                        int img_width, int img_height);
lv_obj_t *ui_video_play_list_comp_create(lv_obj_t *parent, char *video_name);
lv_obj_t *ui_video_play_mode_comp_create(lv_obj_t *parent, char *mode_name, char *mode_path);
#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
