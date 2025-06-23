/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Zequan Liang <zequan.liang@artinchip.com>
 */

#ifndef _AIC_DEMO_COMP_HOME_H
#define _AIC_DEMO_COMP_HOME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../ui.h"

lv_obj_t *ui_home_app_icon_comp_create(lv_obj_t *parent, const char *img_path);
lv_obj_t *ui_home_app_label_comp_create(lv_obj_t *parent, const char *text);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
