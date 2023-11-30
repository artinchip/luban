/*
 * Copyright (C) 2022-2023 Artinchip Technology Co., Ltd.
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#ifndef BASE_UI_H
#define BASE_UI_H

#ifdef __cplusplus
extern "C" {
#endif

LV_FONT_DECLARE(ui_font_Big);
LV_FONT_DECLARE(ui_font_Title);
LV_FONT_DECLARE(ui_font_H1);

void base_ui_init();

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif //AIC_UI_H
