/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Zequan Liang <zequan.liang@artinchip.com>
 */

/*
 *                              File structure description
 *
 * ./asserts          --> the stored resources files include images and fonts used by UI.
 * ./components       --> reusable custom UI components managed on a per-screen basic.
 * ./font             --> storage of font resource files
 * ./model            --> encapsulation of modules other then the UI, used for appropriate application invocation.
 * ./screen           --> UI implementation written on a per-screen page basic.
 * ./task             --> concrete implementation of application logic.
 * ./ui.c             --> application entry function.
 * ./ui_event.h       --> declaration of used-defined events.
 * ../main.c          --> lvgl entry function.
 * ../lvgl            --> lvgl source code.
 * ../aic_ui.h        --> aic lvgl related global macro definitions
 * ../lv_conf.h       --> lvgl related global macro definitions
 * ../CMakeLists.txt  --> build project files
 */

#include "ui.h"

lv_obj_t *ui_initial_action;

void fade_in_animation(lv_obj_t *target_obj, int delay) {;}
void ui_event_initial_action(lv_event_t * e) {;}

void ui_init(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(disp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), false, LV_FONT_DEFAULT);
    lv_disp_set_theme(disp, theme);

    ui_initial_action = lv_obj_create(NULL);
    lv_obj_add_event_cb(ui_initial_action, ui_event_initial_action, LV_EVENT_ALL, NULL);

    ui_default_init();

    lv_scr_load_anim(ui_default, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
}
