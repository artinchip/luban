/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Zequan Liang <zequan.liang@artinchip.com>
 */

#include "ui_comp_video.h"

lv_obj_t *ui_video_btn_icon_comp_create(lv_obj_t *parent, char *img_path,
                                        int img_width, int img_height)
{
    lv_obj_t * btn_icon;
    btn_icon =  lv_img_create(parent);
    lv_img_set_src(btn_icon, img_path);
    lv_obj_set_pos(btn_icon, 0, 0);
    lv_obj_set_size(btn_icon, img_width, img_height);
    lv_obj_set_align(btn_icon, LV_ALIGN_TOP_LEFT);
    lv_obj_add_flag(btn_icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(btn_icon, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(btn_icon, LV_OBJ_FLAG_SCROLLABLE);

    return btn_icon;
}

lv_obj_t *ui_video_play_list_comp_create(lv_obj_t *parent, char *video_name)
{
    lv_obj_t *play_list_video;
    play_list_video =  lv_obj_create(parent);
    lv_obj_remove_style_all(play_list_video);
    lv_obj_clear_flag(play_list_video, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(play_list_video, LV_OPA_0, 0);
    lv_obj_set_size(play_list_video, 300, 40);

    lv_obj_t *play_list_btn =  lv_img_create(play_list_video);
    lv_obj_set_pos(play_list_btn, 10, 0);
    lv_obj_set_align(play_list_btn, LV_ALIGN_TOP_LEFT);
    lv_img_set_src(play_list_btn, LVGL_PATH(video/video.png));
    lv_obj_set_size(play_list_btn, 29, 26);

    lv_obj_t *play_list_text = lv_label_create(play_list_video);
    lv_label_set_long_mode(play_list_text, LV_LABEL_LONG_SCROLL);
    lv_obj_set_width(play_list_text, 270);
    lv_obj_set_height(play_list_text, LV_SIZE_CONTENT);
    lv_obj_set_align(play_list_text, LV_ALIGN_TOP_MID);
    lv_obj_set_pos(play_list_text, 40, 0);
    lv_label_set_text(play_list_text, video_name);
    lv_obj_set_style_text_color(play_list_text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(play_list_text, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(play_list_text, &lv_font_montserrat_24, LV_PART_MAIN| LV_STATE_DEFAULT);

    return play_list_video;
}

lv_obj_t *ui_video_play_mode_comp_create(lv_obj_t *parent, char *mode_name, char *mode_path)
{
    lv_obj_t *play_mode;
    play_mode =  lv_obj_create(parent);
    lv_obj_remove_style_all(play_mode);
    lv_obj_clear_flag(play_mode, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(play_mode, LV_OPA_0, 0);
    lv_obj_set_size(play_mode, 300, 40);

    lv_obj_t *play_mode_btn =  lv_img_create(play_mode);
    lv_obj_set_pos(play_mode_btn, 10, 0);
    lv_obj_set_align(play_mode_btn, LV_ALIGN_TOP_LEFT);
    lv_img_set_src(play_mode_btn, mode_path);
    lv_obj_set_size(play_mode_btn, 30, 30);

    lv_obj_t *play_mode_text = lv_label_create(play_mode);
    lv_label_set_long_mode(play_mode_text, LV_LABEL_LONG_SCROLL);
    lv_obj_set_width(play_mode_text, 270);
    lv_obj_set_height(play_mode_text, LV_SIZE_CONTENT);
    lv_obj_set_align(play_mode_text, LV_ALIGN_TOP_MID);
    lv_obj_set_pos(play_mode_text, 40, 0);
    lv_label_set_text(play_mode_text, mode_name);
    lv_obj_set_style_text_color(play_mode_text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(play_mode_text, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(play_mode_text, &lv_font_montserrat_24, LV_PART_MAIN| LV_STATE_DEFAULT);

    return play_mode;
}
