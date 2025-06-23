/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Zequan Liang <zequan.liang@artinchip.com>
 */

#include "../ui.h"
#include "../model/model_media.h"
#include "../model/lv_ffmpeg.h"

#include <sys/time.h>

#define AIC_DEMO_MUSIC_DIR LVGL_FILE_LIST_PATH(music)

lv_obj_t *ui_music;
static lv_obj_t *ui_music_bg;
static lv_obj_t *ui_back_icon;
static lv_obj_t *ui_next_song_icon;
static lv_obj_t *ui_last_song_icon;
static lv_obj_t *ui_play_song_icon;
static lv_obj_t *ui_pause_song_icon;
static lv_obj_t *ui_back_and_like_bg_0;
static lv_obj_t *ui_back_and_like_bg_1;
static lv_obj_t *ui_like_default_icon;
static lv_obj_t *ui_like_icon;
static lv_obj_t *ui_song_cover;
static lv_obj_t *ui_song_name;
static lv_obj_t *ui_singer_name;
static lv_obj_t *ui_progress_bar;
static lv_obj_t *ui_song_now_time;
static lv_obj_t *ui_song_total_time;

static lv_timer_t *update_play_timer;

static struct media_list *default_list;
static lv_obj_t *default_player;

static struct timeval start, end;
static int elapsed_time_ms;
static int music_status = 0;

static int last_bar_value = 0;

static void ui_anim_free_user_data(lv_anim_t *a)
{
    lv_mem_free(a->user_data);
    a->user_data=NULL;
}

static void ui_anim_cover_set_x(lv_anim_t* a, int32_t v)
{
   ui_anim_user_data_t *usr = (ui_anim_user_data_t *)a->user_data;
   lv_obj_set_style_x(usr->target, v, 0);
}

static void ui_music_cover_anim_start(lv_obj_t * ui_song_cover)
{
    lv_anim_t ui_cover_anim;

    ui_anim_user_data_t *anim_user_data = lv_mem_alloc(sizeof(ui_anim_user_data_t));
    anim_user_data->target = ui_song_cover;
    anim_user_data->val = -1;

    lv_anim_init(&ui_cover_anim);
    lv_anim_set_time(&ui_cover_anim, 300);
    lv_anim_set_user_data(&ui_cover_anim, anim_user_data);
    lv_anim_set_custom_exec_cb(&ui_cover_anim, ui_anim_cover_set_x);
    lv_anim_set_values(&ui_cover_anim, -315, 98);
    lv_anim_set_path_cb(&ui_cover_anim, lv_anim_path_linear);
    lv_anim_set_delay(&ui_cover_anim, 0);
    lv_anim_set_deleted_cb(&ui_cover_anim, ui_anim_free_user_data);
    lv_anim_set_playback_time(&ui_cover_anim, 0);
    lv_anim_set_playback_delay(&ui_cover_anim, 0);
    lv_anim_set_repeat_count(&ui_cover_anim, 0);
    lv_anim_set_repeat_delay(&ui_cover_anim, 0);
    lv_anim_set_early_apply(&ui_cover_anim, false);

    lv_anim_start(&ui_cover_anim);
}

#ifdef USE_HEARTBEAT_ANIM
static void ui_anim_heartbeat_transform_zoom(lv_anim_t* a, int32_t v)
{
   ui_anim_user_data_t *usr = (ui_anim_user_data_t *)a->user_data;

   lv_obj_set_style_transform_zoom(usr->target, v, 0);
}

static void ui_music_heartbeat_anim_start(lv_obj_t * ui_like_icon)
{
    ui_anim_user_data_t *anim_user_data = lv_mem_alloc(sizeof(ui_anim_user_data_t));
    anim_user_data->target = ui_like_icon;
    anim_user_data->val = -1;

    lv_anim_t ui_heartbeat_anim;
    lv_anim_init(&ui_heartbeat_anim);
    lv_anim_set_time(&ui_heartbeat_anim, 500);
    lv_anim_set_user_data(&ui_heartbeat_anim, anim_user_data);
    lv_anim_set_custom_exec_cb(&ui_heartbeat_anim, ui_anim_heartbeat_transform_zoom);
    lv_anim_set_values(&ui_heartbeat_anim, 256, 256 + 64);
    lv_anim_set_path_cb(&ui_heartbeat_anim, lv_anim_path_linear);
    lv_anim_set_delay(&ui_heartbeat_anim, 0);
    lv_anim_set_deleted_cb(&ui_heartbeat_anim, ui_anim_free_user_data);
    lv_anim_set_playback_time(&ui_heartbeat_anim, 250);
    lv_anim_set_playback_delay(&ui_heartbeat_anim, 0);
    lv_anim_set_repeat_count(&ui_heartbeat_anim, 0);
    lv_anim_set_repeat_delay(&ui_heartbeat_anim, 0);
    lv_anim_set_early_apply(&ui_heartbeat_anim, false);

    lv_anim_start(&ui_heartbeat_anim);
}
#endif

static void ui_anim_set_bar_value(lv_anim_t* a, int32_t v)
{
   ui_anim_user_data_t *usr = (ui_anim_user_data_t *)a->user_data;
   lv_bar_set_value(usr->target, v, LV_ANIM_OFF);
}

static void ui_progress_bar_anim_start(lv_obj_t * progress_bar, int start_value, int end_value) {
    ui_anim_user_data_t *anim_user_data = lv_mem_alloc(sizeof(ui_anim_user_data_t));
    anim_user_data->target = progress_bar;
    anim_user_data->val = -1;

    lv_anim_t ui_progress_bar_anim;
    lv_anim_init(&ui_progress_bar_anim);
    lv_anim_set_time(&ui_progress_bar_anim, 100);
    lv_anim_set_user_data(&ui_progress_bar_anim, anim_user_data);
    lv_anim_set_custom_exec_cb(&ui_progress_bar_anim, ui_anim_set_bar_value);
    lv_anim_set_values(&ui_progress_bar_anim, start_value, end_value);
    lv_anim_set_path_cb(&ui_progress_bar_anim, lv_anim_path_linear);
    lv_anim_set_delay(&ui_progress_bar_anim, 0);
    lv_anim_set_deleted_cb(&ui_progress_bar_anim, ui_anim_free_user_data);
    lv_anim_set_playback_time(&ui_progress_bar_anim, 0);
    lv_anim_set_playback_delay(&ui_progress_bar_anim, 0);
    lv_anim_set_repeat_count(&ui_progress_bar_anim, 0);
    lv_anim_set_repeat_delay(&ui_progress_bar_anim, 0);
    lv_anim_set_early_apply(&ui_progress_bar_anim, false);

    lv_anim_start(&ui_progress_bar_anim);
}

static void ui_music_bg_init(void)
{
    ui_music_bg = lv_img_create(ui_music);
    lv_img_set_src(ui_music_bg, LVGL_PATH(music/music_bg.jpeg));
    lv_obj_set_pos(ui_music_bg, 0, 0);
    lv_obj_set_width(ui_music_bg, 1024);
    lv_obj_set_height(ui_music_bg, 600);
    lv_obj_set_align(ui_music_bg, LV_ALIGN_TOP_LEFT);
    lv_obj_add_flag(ui_music_bg, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_set_style_bg_color(ui_music_bg, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_music_bg, 10, LV_PART_MAIN);
    lv_obj_clear_flag(ui_music_bg, LV_OBJ_FLAG_SCROLLABLE);
}

static void ui_music_btn_init(void) {
    static lv_style_t style_btn_default;
    lv_style_init(&style_btn_default);
    lv_style_set_shadow_width(&style_btn_default, 40);
    lv_style_set_shadow_ofs_x(&style_btn_default, 0);
    lv_style_set_shadow_ofs_y(&style_btn_default, 4);
    lv_style_set_shadow_spread(&style_btn_default, 0);
    lv_style_set_shadow_color(&style_btn_default, lv_color_hex(0x0));
    lv_style_set_shadow_opa(&style_btn_default, LV_OPA_20);
    lv_style_set_radius(&style_btn_default, LV_RADIUS_CIRCLE);

    static lv_style_t style_btn_pressed;
    lv_style_init(&style_btn_pressed);
    lv_style_set_shadow_width(&style_btn_pressed, 40);
    lv_style_set_shadow_ofs_x(&style_btn_pressed, 0);
    lv_style_set_shadow_ofs_y(&style_btn_pressed, 0);
    lv_style_set_shadow_spread(&style_btn_pressed, 4);
    lv_style_set_shadow_color(&style_btn_pressed, lv_color_hex(0x5792FE));
    lv_style_set_shadow_opa(&style_btn_pressed, LV_OPA_100);
    lv_style_set_radius(&style_btn_pressed, LV_RADIUS_CIRCLE);

    ui_last_song_icon = lv_imgbtn_create(ui_music);
    lv_imgbtn_set_src(ui_last_song_icon, LV_IMGBTN_STATE_RELEASED , NULL, LVGL_PATH(music/last_song.png), NULL);
    lv_imgbtn_set_src(ui_last_song_icon, LV_IMGBTN_STATE_PRESSED , NULL, LVGL_PATH(music/last_song.png), NULL);
    lv_obj_set_pos(ui_last_song_icon, 460, 340);
    lv_obj_set_size(ui_last_song_icon, 75, 75);
    lv_obj_set_align(ui_last_song_icon, LV_ALIGN_TOP_LEFT);
    lv_obj_clear_flag(ui_last_song_icon, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(ui_last_song_icon, &style_btn_default, LV_STATE_DEFAULT);
    lv_obj_add_style(ui_last_song_icon, &style_btn_pressed, LV_STATE_PRESSED);

    ui_play_song_icon = lv_imgbtn_create(ui_music);
    lv_imgbtn_set_src(ui_play_song_icon, LV_IMGBTN_STATE_RELEASED , NULL, LVGL_PATH(music/play_continue.png), NULL);
    lv_imgbtn_set_src(ui_play_song_icon, LV_IMGBTN_STATE_PRESSED , NULL, LVGL_PATH(music/play_continue.png), NULL);
    lv_obj_set_pos(ui_play_song_icon, 560, 315);
    lv_obj_set_size(ui_play_song_icon, 119, 119);
    lv_obj_set_align(ui_play_song_icon, LV_ALIGN_TOP_LEFT);
    lv_obj_clear_flag(ui_play_song_icon, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(ui_play_song_icon, &style_btn_default, LV_STATE_DEFAULT);
    lv_obj_add_style(ui_play_song_icon, &style_btn_pressed, LV_STATE_PRESSED);

    ui_pause_song_icon = lv_imgbtn_create(ui_music);
    lv_imgbtn_set_src(ui_pause_song_icon, LV_IMGBTN_STATE_RELEASED , NULL, LVGL_PATH(music/play_pause.png), NULL);
    lv_imgbtn_set_src(ui_pause_song_icon, LV_IMGBTN_STATE_PRESSED , NULL, LVGL_PATH(music/play_pause.png), NULL);
    lv_obj_set_pos(ui_pause_song_icon, 560, 315);
    lv_obj_set_size(ui_pause_song_icon, 119, 119);
    lv_obj_clear_flag(ui_pause_song_icon, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(ui_pause_song_icon, &style_btn_default, LV_STATE_DEFAULT);
    lv_obj_add_style(ui_pause_song_icon, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_align(ui_pause_song_icon, LV_ALIGN_TOP_LEFT);
    lv_obj_add_flag(ui_pause_song_icon, LV_OBJ_FLAG_HIDDEN);

    ui_next_song_icon = lv_imgbtn_create(ui_music);
    lv_imgbtn_set_src(ui_next_song_icon, LV_IMGBTN_STATE_RELEASED , NULL, LVGL_PATH(music/next_song.png), NULL);
    lv_imgbtn_set_src(ui_next_song_icon, LV_IMGBTN_STATE_PRESSED , NULL, LVGL_PATH(music/next_song.png), NULL);
    lv_obj_set_pos(ui_next_song_icon, 705, 340);
    lv_obj_set_size(ui_next_song_icon, 75, 75);
    lv_obj_clear_flag(ui_next_song_icon, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_align(ui_next_song_icon, LV_ALIGN_TOP_LEFT);
    lv_obj_add_style(ui_next_song_icon, &style_btn_default, LV_STATE_DEFAULT);
    lv_obj_add_style(ui_next_song_icon, &style_btn_pressed, LV_STATE_PRESSED);

    ui_back_and_like_bg_0 = lv_img_create(ui_music);
    lv_img_set_src(ui_back_and_like_bg_0, LVGL_PATH(music/ellipse.png));
    lv_obj_set_pos(ui_back_and_like_bg_0, 38, 43);
    lv_obj_set_size(ui_back_and_like_bg_0, 75, 75);
    lv_obj_set_align(ui_back_and_like_bg_0, LV_ALIGN_TOP_LEFT);
    lv_obj_add_style(ui_back_and_like_bg_0, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_flag(ui_back_and_like_bg_0, LV_OBJ_FLAG_ADV_HITTEST | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_back_and_like_bg_0, LV_OBJ_FLAG_SCROLLABLE);

    ui_back_icon = lv_img_create(ui_back_and_like_bg_0);
    lv_img_set_src(ui_back_icon, LVGL_PATH(music/back.png));
    lv_obj_set_pos(ui_back_icon, 0, 0);
    lv_obj_set_size(ui_back_icon, 34, 36);
    lv_obj_set_align(ui_back_icon, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_back_icon, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_back_icon, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
}

static void ui_music_song_bar_init(struct media_info *info) {
    ui_progress_bar = lv_bar_create(ui_music);
    lv_bar_set_value(ui_progress_bar, 0, LV_ANIM_OFF);
    lv_bar_set_range(ui_progress_bar, 0, info->lengths / 1000);
    lv_obj_clear_flag(ui_progress_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(ui_progress_bar, 777, 16);
    lv_obj_set_pos(ui_progress_bar, 107, 503);
    lv_obj_set_align(ui_progress_bar, LV_ALIGN_TOP_LEFT);
    lv_obj_set_style_bg_color(ui_progress_bar, lv_color_hex(0x0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_progress_bar, LV_OPA_10 / 2, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_progress_bar, lv_color_hex(0x0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_progress_bar, LV_OPA_0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_progress_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_progress_bar, LV_BORDER_SIDE_FULL, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_color(ui_progress_bar, lv_color_hex(0x0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_opa(ui_progress_bar, LV_OPA_0, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(ui_progress_bar, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_outline_pad(ui_progress_bar, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_progress_bar, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_progress_bar, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_progress_bar, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_progress_bar, 0, LV_PART_MAIN| LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(ui_progress_bar, lv_color_hex(0X7C92BA), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_progress_bar, 255, LV_PART_INDICATOR| LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_progress_bar, lv_color_hex(0x6236FF), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_progress_bar, LV_GRAD_DIR_HOR, LV_PART_INDICATOR| LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_progress_bar, 0, LV_PART_INDICATOR| LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_progress_bar, 255, LV_PART_INDICATOR| LV_STATE_DEFAULT);

    lv_obj_set_style_shadow_color(ui_progress_bar, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui_progress_bar, LV_OPA_20, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui_progress_bar, 10, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui_progress_bar, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_ofs_x(ui_progress_bar, -4, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_ofs_y(ui_progress_bar, 2, LV_PART_MAIN| LV_STATE_DEFAULT);
}

static void ui_music_song_cover_init(struct media_info *info)
{
    ui_song_cover = lv_img_create(ui_music);
    lv_img_set_src(ui_song_cover, info->cover_path);
    lv_obj_set_pos(ui_song_cover, 98, 135);
    lv_obj_set_size(ui_song_cover, 323, 323);
    lv_obj_set_align(ui_song_cover, LV_ALIGN_TOP_LEFT);
    lv_obj_clear_flag(ui_song_cover, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
}

static void ui_music_song_like_init()
{
    ui_back_and_like_bg_1 = lv_img_create(ui_music);
    lv_img_set_src(ui_back_and_like_bg_1, LVGL_PATH(music/ellipse.png));
    lv_obj_set_pos(ui_back_and_like_bg_1, 911, 43);
    lv_obj_set_size(ui_back_and_like_bg_1, 75, 75);
    lv_obj_set_align(ui_back_and_like_bg_1, LV_ALIGN_TOP_LEFT);
    lv_obj_add_flag(ui_back_and_like_bg_1, LV_OBJ_FLAG_ADV_HITTEST | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_back_and_like_bg_1, LV_OBJ_FLAG_SCROLLABLE);

    ui_like_default_icon = lv_img_create(ui_back_and_like_bg_1);
    lv_img_set_src(ui_like_default_icon, LVGL_PATH(music/heart_grey.png));
    lv_obj_set_pos(ui_like_default_icon, 0, 0);
    lv_obj_set_size(ui_like_default_icon, 30, 28);
    lv_obj_set_align(ui_like_default_icon, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_like_default_icon, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_like_default_icon, LV_OBJ_FLAG_SCROLLABLE);

    ui_like_icon = lv_img_create(ui_back_and_like_bg_1);
    lv_img_set_src(ui_like_icon, LVGL_PATH(music/heart_red.png));
    lv_obj_set_pos(ui_like_icon, 0, 0);
    lv_obj_set_size(ui_like_icon, 30, 28);
    lv_obj_set_align(ui_like_icon, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_like_icon, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_like_icon, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_clear_flag(ui_like_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_like_default_icon, LV_OBJ_FLAG_HIDDEN);
}

static void ui_music_song_name_init(struct media_info *info)
{
    ui_song_name = lv_label_create(ui_music);
    lv_obj_set_width(ui_song_name, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_song_name, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_song_name, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(ui_song_name, 450, 185);
    lv_label_set_text(ui_song_name, info->name);
    lv_obj_set_style_text_color(ui_song_name, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_song_name, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_song_name, &lv_font_montserrat_48, LV_PART_MAIN| LV_STATE_DEFAULT);
}

static void ui_music_singer_name_init(struct media_info *info)
{
    ui_singer_name = lv_label_create(ui_music);
    lv_obj_set_size(ui_singer_name, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_pos(ui_singer_name, 450, 260);
    lv_obj_set_align(ui_singer_name, LV_ALIGN_TOP_LEFT);
    lv_label_set_text(ui_singer_name, info->author);
    lv_obj_set_style_text_color(ui_singer_name, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_singer_name, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_singer_name, &lv_font_montserrat_24, LV_PART_MAIN| LV_STATE_DEFAULT);
}

static void ui_music_song_time_init(struct media_info *info)
{
    ui_song_now_time = lv_label_create(ui_music);
    lv_obj_set_size(ui_song_now_time, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_pos(ui_song_now_time, 116, 539);
    lv_obj_set_align(ui_song_now_time, LV_ALIGN_TOP_LEFT);
    lv_label_set_text(ui_song_now_time, "00:00");
    lv_obj_set_style_text_color(ui_song_now_time, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_song_now_time, LV_OPA_70, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_song_now_time, &lv_font_montserrat_24, LV_PART_MAIN| LV_STATE_DEFAULT);

    int song_minutes = (info->lengths / 1000) / 60;
    int song_seconds = (info->lengths / 1000) % 60;

    ui_song_total_time = lv_label_create(ui_music);
    lv_obj_set_size(ui_song_total_time, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_pos(ui_song_total_time, 810, 539);
    lv_obj_set_align(ui_song_total_time, LV_ALIGN_TOP_LEFT);
    if (song_minutes < 10 && song_seconds < 10) {
        lv_label_set_text_fmt(ui_song_total_time, "0%d:0%d", song_minutes, song_seconds);
    } else if (song_minutes >= 10 && song_seconds < 10) {
        lv_label_set_text_fmt(ui_song_total_time, "%d:0%d", song_minutes, song_seconds);
    } else if (song_minutes < 10 && song_seconds >= 10) {
        lv_label_set_text_fmt(ui_song_total_time, "0%d:%d", song_minutes, song_seconds);
    } else if (song_minutes >= 10 && song_seconds >= 10) {
        lv_label_set_text_fmt(ui_song_total_time, "%d:%d", song_minutes, song_seconds);
    }

    lv_label_set_text_fmt(ui_song_total_time, "0%d:%d", song_minutes, song_seconds);
    lv_obj_set_style_text_color(ui_song_total_time, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_song_total_time, LV_OPA_70, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_song_total_time, &lv_font_montserrat_24, LV_PART_MAIN| LV_STATE_DEFAULT);
}

static void back_home_cb(lv_event_t *e)
{
    lv_event_code_t code = (lv_event_code_t)lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        media_list_destroy(default_list);
        lv_ffmpeg_player_set_cmd(default_player, LV_FFMPEG_PLAYER_CMD_STOP);
        lv_timer_del(update_play_timer);

        lv_obj_clean(ui_music);
        ui_home_init();
        lv_scr_load_anim(ui_home, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
    }
}

static void ui_song_info_update(struct media_info *info)
{
    int song_minutes = (info->lengths / 1000) / 60;
    int song_seconds = (info->lengths / 1000) % 60;
    /* update song time info */
    lv_label_set_text(ui_song_now_time, "00:00");

    if (song_minutes < 10 && song_seconds < 10) {
        lv_label_set_text_fmt(ui_song_total_time, "0%d:0%d", song_minutes, song_seconds);
    } else if (song_minutes > 10 && song_seconds < 10) {
        lv_label_set_text_fmt(ui_song_total_time, "%d:0%d", song_minutes, song_seconds);
    } else if (song_minutes < 10 && song_seconds >= 10) {
        lv_label_set_text_fmt(ui_song_total_time, "0%d:%d", song_minutes, song_seconds);
    } else if (song_minutes >= 10 && song_seconds >= 10) {
        lv_label_set_text_fmt(ui_song_total_time, "%d:%d", song_minutes, song_seconds);
    }

    /* update song cover info */
    lv_img_set_src(ui_song_cover, info->cover_path);

    /* update song name info */
    lv_label_set_text(ui_song_name, info->name);

    /* update singer name info */
    lv_label_set_text(ui_singer_name, info->author);

    /* update song progress bar */
    lv_bar_set_value(ui_progress_bar, 0, LV_ANIM_OFF);
    lv_bar_set_range(ui_progress_bar, 0, info->lengths / 1000);

    /* update song playing btn */
    lv_obj_add_flag(ui_play_song_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pause_song_icon, LV_OBJ_FLAG_HIDDEN);
}

static void play_next_song(lv_event_t *e)
{
    lv_event_code_t code = (lv_event_code_t)lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        int now_media = default_list->now_pos;
        int num = default_list->num;

        now_media++;
        now_media = now_media >= num ? 0 : now_media;
        default_list->now_pos = now_media;

        struct media_info info = {0};
        media_list_get_now_info(default_list, &info);

        ui_song_info_update(&info);

        lv_ffmpeg_player_set_cmd(default_player, LV_FFMPEG_PLAYER_CMD_STOP);
        lv_ffmpeg_player_set_src(default_player, info.source_path);
        lv_ffmpeg_player_set_cmd(default_player, LV_FFMPEG_PLAYER_CMD_START);

        ui_music_cover_anim_start(ui_song_cover);

        lv_obj_add_flag(ui_play_song_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_pause_song_icon, LV_OBJ_FLAG_HIDDEN);

        gettimeofday(&start, NULL);
        last_bar_value = 0;
        music_status = 0;
        elapsed_time_ms = 0;
    }
}

static void play_last_song(lv_event_t *e)
{
    lv_event_code_t code = (lv_event_code_t)lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        int now_media = default_list->now_pos;
        int num = default_list->num;

        now_media--;
        now_media = now_media < 0 ? num -1 : now_media;
        default_list->now_pos = now_media;

        struct media_info info = {0};
        media_list_get_now_info(default_list, &info);

        ui_song_info_update(&info);

        lv_ffmpeg_player_set_cmd(default_player, LV_FFMPEG_PLAYER_CMD_STOP);
        lv_ffmpeg_player_set_src(default_player, info.source_path);
        lv_ffmpeg_player_set_cmd(default_player, LV_FFMPEG_PLAYER_CMD_START);
        gettimeofday(&start, NULL);

        ui_music_cover_anim_start(ui_song_cover);

        lv_obj_add_flag(ui_play_song_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_pause_song_icon, LV_OBJ_FLAG_HIDDEN);

        last_bar_value = 0;
        music_status = 0;
        elapsed_time_ms = 0;
    }
}

static void play_song(lv_event_t *e) {
    lv_event_code_t code = (lv_event_code_t)lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        struct media_info info = {0};
        media_list_get_now_info(default_list, &info);

        if (elapsed_time_ms == 0) {
            lv_ffmpeg_player_set_cmd(default_player, LV_FFMPEG_PLAYER_CMD_STOP);
            lv_ffmpeg_player_set_src(default_player, info.source_path);
            lv_ffmpeg_player_set_cmd(default_player, LV_FFMPEG_PLAYER_CMD_START);
            gettimeofday(&start, NULL);
        } else {
            lv_ffmpeg_player_set_cmd(default_player, LV_FFMPEG_PLAYER_CMD_START);
        }

        lv_obj_add_flag(ui_play_song_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_pause_song_icon, LV_OBJ_FLAG_HIDDEN);
        music_status = 0;
    }
}

static void pause_song(lv_event_t *e) {
    lv_event_code_t code = (lv_event_code_t)lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        lv_ffmpeg_player_set_cmd(default_player, LV_FFMPEG_PLAYER_CMD_PAUSE);
        lv_obj_clear_flag(ui_play_song_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pause_song_icon, LV_OBJ_FLAG_HIDDEN);
        music_status = 1;
    }
}

static void like_cb(lv_event_t *e) {
    lv_event_code_t code = (lv_event_code_t)lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        static int like = 0;
        if (like == 0) {
            like = 1;
            lv_obj_add_flag(ui_like_default_icon, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_like_icon, LV_OBJ_FLAG_HIDDEN);
        } else {
            like = 0;
            lv_obj_clear_flag(ui_like_default_icon, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_like_icon, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static struct media_list* music_list_create(void)
{
    default_list = media_list_create();

    struct media_info song = {0};
    memset(&song, 0, sizeof(struct media_info));
    strncpy(song.name, "Deep And Serene", strlen("Deep And Serene") + 1);
    strncpy(song.author, "Da Ge Zhao", strlen("Da Ge Zhao") + 1);
    snprintf(song.source_path, sizeof(song.source_path), "%s/music/%s", AIC_DEMO_MUSIC_DIR, "deep_and_serene.mp3");
    snprintf(song.cover_path, sizeof(song.cover_path),"%s", LVGL_PATH(music/song_3_img.png));
    song.lengths = 122000;
    media_list_add_info(default_list, &song);

    memset(&song, 0, sizeof(struct media_info));
    strncpy(song.name, "60 Second", strlen("60 Second") + 1);
    strncpy(song.author, "kingphosphor", strlen("kingphosphor") + 1);
    snprintf(song.source_path, sizeof(song.source_path), "%s/music/%s", AIC_DEMO_MUSIC_DIR, "60_second_music.mp3");
    snprintf(song.cover_path, sizeof(song.cover_path), "%s", LVGL_PATH(music/song_2_img.png));
    song.lengths = 60000;
    media_list_add_info(default_list, &song);

    memset(&song, 0, sizeof(struct media_info));
    strncpy(song.name, "Leisure Time", strlen("Leisure Time") + 1);
    strncpy(song.author, "Da Ge Zhao", strlen("Da Ge Zhao") + 1);
    snprintf(song.source_path, sizeof(song.source_path), "%s/music/%s", AIC_DEMO_MUSIC_DIR, "leisure_time.mp3");
    snprintf(song.cover_path, sizeof(song.cover_path), "%s", LVGL_PATH(music/song_1_img.png));
    song.lengths = 104000;
    media_list_add_info(default_list, &song);

    return default_list;
}

static void update_play_cb(lv_timer_t *tmr)
{
    struct media_info info = {0};
    double elapsed_time;

    if (default_list == NULL) {
        return;
    }

    if (music_status == 1) {
        gettimeofday(&start, NULL);
        return;
    }

    media_list_get_now_info(default_list, &info);
    gettimeofday(&end, NULL);
    elapsed_time = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
    elapsed_time_ms += elapsed_time;
    gettimeofday(&start, NULL);

    if (elapsed_time_ms >= info.lengths) {
        lv_obj_clear_flag(ui_play_song_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pause_song_icon, LV_OBJ_FLAG_HIDDEN);
        elapsed_time_ms = info.lengths;
    }

    int song_minutes = (elapsed_time_ms / 1000) / 60;
    int song_seconds = (elapsed_time_ms / 1000) % 60;
    /* update song time info */
    lv_label_set_text(ui_song_now_time, "0:00");
    if (song_minutes < 10 && song_seconds < 10) {
        lv_label_set_text_fmt(ui_song_now_time, "0%d:0%d", song_minutes, song_seconds);
    } else if (song_minutes > 10 && song_seconds < 10) {
        lv_label_set_text_fmt(ui_song_now_time, "%d:0%d", song_minutes, song_seconds);
    } else if (song_minutes < 10 && song_seconds > 10) {
        lv_label_set_text_fmt(ui_song_now_time, "0%d:%d", song_minutes, song_seconds);
    } else if (song_minutes > 10 && song_seconds > 10) {
        lv_label_set_text_fmt(ui_song_now_time, "%d:%d", song_minutes, song_seconds);
    }

    ui_progress_bar_anim_start(ui_progress_bar, last_bar_value, elapsed_time_ms / 1000);

    last_bar_value = elapsed_time_ms / 1000;
}

void ui_music_init(void)
{
    struct media_info info = {0};

    elapsed_time_ms = 0;
    music_status = 1;
    default_list = music_list_create();
    default_list->now_pos = 2;
    media_list_get_now_info(default_list, &info);

    default_player = lv_ffmpeg_player_create(ui_music);
    lv_obj_set_size(default_player, 0, 0);
    lv_obj_set_pos(default_player, 0, 0);
    lv_obj_set_align(default_player, LV_ALIGN_TOP_LEFT);
    lv_ffmpeg_player_set_auto_restart(default_player, false);

    ui_music = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_music, LV_OBJ_FLAG_SCROLLABLE);

    ui_music_bg_init();
    ui_music_btn_init();
    ui_music_song_like_init();
    ui_music_song_bar_init(&info);
    ui_music_song_cover_init(&info);
    ui_music_song_name_init(&info);
    ui_music_singer_name_init(&info);
    ui_music_song_time_init(&info);

    update_play_timer = lv_timer_create(update_play_cb, 100, NULL);

    lv_obj_add_event_cb(ui_back_and_like_bg_0, back_home_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(ui_next_song_icon, play_next_song, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(ui_last_song_icon, play_last_song, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(ui_play_song_icon, play_song, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(ui_pause_song_icon, pause_song, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(ui_back_and_like_bg_1, like_cb, LV_EVENT_PRESSED, NULL);
}
