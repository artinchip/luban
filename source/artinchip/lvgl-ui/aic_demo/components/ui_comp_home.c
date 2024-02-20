/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Zequan Liang <zequan.liang@artinchip.com>
 */

#include "ui_comp_home.h"

lv_obj_t *ui_home_app_icon_comp_create(lv_obj_t *parent, const char *img_path)
{
    static lv_style_t icon_style_pressed;
    lv_style_init(&icon_style_pressed);

    /*
     * LVGL version 8.3, reading images from external sources does not support the
     * 'lv_style_set_img_recolor' and will not produce any effects. but the
     * 'lv_style_set_img_opa' can work.
     *
     * lv_style_set_img_recolor(&icon_style_pressed, lv_color_black());
     * lv_style_set_img_opa(&icon_style_pressed, LV_OPA_70);
     */
    lv_style_set_translate_x(&icon_style_pressed, 2);
    lv_style_set_translate_y(&icon_style_pressed, 2);

    lv_obj_t * app_icon;
    app_icon = lv_imgbtn_create(parent);
    lv_imgbtn_set_src(app_icon, LV_IMGBTN_STATE_RELEASED , NULL, img_path, NULL);
    lv_imgbtn_set_src(app_icon, LV_IMGBTN_STATE_PRESSED , NULL, img_path, NULL);
    lv_obj_set_pos(app_icon, 0, 0);
    lv_obj_set_size(app_icon, 97, 93);

    lv_obj_add_style(app_icon, &icon_style_pressed, LV_STATE_PRESSED);

    return app_icon;
}

lv_obj_t *ui_home_app_label_comp_create(lv_obj_t *parent, const char *text)
{
    lv_obj_t *app_label = lv_label_create(parent);
    lv_obj_set_size(app_label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_pos(app_label, 0, 0);
    lv_label_set_text(app_label, text);
    lv_obj_set_style_text_color(app_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(app_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(app_label, &lv_font_montserrat_24, 0);

    return app_label;
}
