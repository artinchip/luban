/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Zequan Liang <zequan.liang@artinchip.com>
 */

#ifndef _AIC_DEMO_UI_H
#define _AIC_DEMO_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/lvgl.h"
#include "ui_events.h"
#include "components/ui_comp_home.h"
#include "components/ui_comp_video.h"
#include "aic_ui.h"

void fade_in_animation(lv_obj_t *TargetObject, int delay);
#define MSN_TEST

typedef struct _ui_anim_user_data_t {
    lv_obj_t *target;
    lv_img_dsc_t **imgset;
    int32_t imgset_size;
    int32_t val;
} ui_anim_user_data_t;

extern lv_obj_t *ui_initial_action;
extern lv_obj_t *ui_default;
extern lv_obj_t *ui_home;
extern lv_obj_t *ui_dashboard;
extern lv_obj_t *ui_music;
extern lv_obj_t *ui_msn;
extern lv_obj_t *ui_video;

void ui_default_init(void);
void ui_home_init(void);
void ui_dashboard_init(void);
void ui_music_init(void);
void ui_msn_init(void);
void ui_video_init(void);

LV_FONT_DECLARE(ui_font_Big);
LV_FONT_DECLARE(ui_font_Title);
LV_FONT_DECLARE(ui_font_H1);

void ui_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
