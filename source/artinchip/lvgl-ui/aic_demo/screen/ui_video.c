/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  ArtInChip
 */

#include "../ui.h"
#include "../model/model_media.h"
#include "../components/ui_comp_video.h"

#include <sys/time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/fb.h>
#include <video/artinchip_fb.h>
#include "../model/lv_ffmpeg.h"

#define VIDEO_PLAY_MODE_LIST_LOOP         1
#define VIDEO_STATUS_PLAYING              2
#define VIDEO_STATUS_PAUSE                3

#define DISP_ICON_TIME                    5000
#define AIC_DEMO_VIDEO_DIR LVGL_FILE_LIST_PATH(video)

lv_obj_t *ui_video;

static lv_obj_t *video_name;
static lv_obj_t *play;
static lv_obj_t *play_list;
static lv_obj_t *play_mode;
static lv_obj_t *back;

static lv_obj_t *play_list_group;
static lv_obj_t *play_mode_group;
static lv_obj_t *loading_group;

static lv_timer_t *ui_play_loop_timer;

static struct media_list *default_list;
static lv_obj_t *default_player;
static int start_player = 0;
static int play_loop_mode = 0;
static int video_status = VIDEO_STATUS_PAUSE;
static int play_btn_status = 0;

/* only used in list replay */
static struct timeval start, end;
static int elapsed_time_ms;

static void ui_anim_free_user_data(lv_anim_t *a)
{
    lv_mem_free(a->user_data);
    a->user_data=NULL;
    lv_obj_invalidate(ui_video);
}

static void ui_anim_sidebar_set_x(lv_anim_t* a, int32_t v)
{
   ui_anim_user_data_t *usr = (ui_anim_user_data_t *)a->user_data;
   lv_obj_set_style_x(usr->target, v, 0);
}

static void ui_video_sidebar_anim_start(lv_obj_t * sidebar, int show)
{
    lv_anim_t ui_sidebar_anim;
    ui_anim_user_data_t *anim_user_data = lv_mem_alloc(sizeof(ui_anim_user_data_t));
    anim_user_data->target = sidebar;
    anim_user_data->val = -1;

    lv_anim_init(&ui_sidebar_anim);
    lv_anim_set_time(&ui_sidebar_anim, 400);
    lv_anim_set_user_data(&ui_sidebar_anim, anim_user_data);
    lv_anim_set_custom_exec_cb(&ui_sidebar_anim, ui_anim_sidebar_set_x);
    lv_anim_set_path_cb(&ui_sidebar_anim, lv_anim_path_linear);
    lv_anim_set_delay(&ui_sidebar_anim, 0);
    lv_anim_set_deleted_cb(&ui_sidebar_anim, ui_anim_free_user_data);
    lv_anim_set_playback_time(&ui_sidebar_anim, 0);
    lv_anim_set_playback_delay(&ui_sidebar_anim, 0);
    lv_anim_set_repeat_count(&ui_sidebar_anim, 0);
    lv_anim_set_repeat_delay(&ui_sidebar_anim, 0);
    lv_anim_set_early_apply(&ui_sidebar_anim, false);

    if (show) {
        lv_anim_set_values(&ui_sidebar_anim, 1024, (1024 - 300));
    } else {
        lv_anim_set_values(&ui_sidebar_anim, (1024 - 300), (1024));
    }

    lv_anim_start(&ui_sidebar_anim);
}

static void hide_video_icon(void)
{
    lv_obj_add_flag(video_name, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(play, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(play_list, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(play_mode, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(back, LV_OBJ_FLAG_HIDDEN);
}

static void show_video_icon(void)
{
    lv_obj_clear_flag(video_name, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(play, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(play_list, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(play_mode, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(back, LV_OBJ_FLAG_HIDDEN);
}

static void control_icon_disp_cb(lv_event_t *e)
{
    lv_event_code_t code = (lv_event_code_t)lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED || play_mode_group == NULL || play_list_group == NULL) {
        return;
    }

    if (lv_obj_get_x(play_mode_group) >= 1000 &&
        lv_obj_get_x(play_list_group) >= 1000) {
        if (lv_obj_has_flag(video_name, LV_OBJ_FLAG_HIDDEN) == true) {
            show_video_icon();
            struct media_info video = {0};
            media_list_get_now_info(default_list, &video);
            lv_label_set_text(video_name, video.name);
        } else {
            hide_video_icon();
        }
    } else {
        /* collapse the sidebar */
        if (lv_obj_get_x(play_mode_group) < 1000) {
            ui_video_sidebar_anim_start(play_mode_group, 0);
        } else if (lv_obj_get_x(play_list_group) < 1000) {
            ui_video_sidebar_anim_start(play_list_group, 0);
        }
        hide_video_icon();
    }
    lv_obj_invalidate(ui_video);
}

static void play_video_cb(lv_event_t *e)
{
    lv_event_code_t code = (lv_event_code_t)lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED) {
        return;
    }

    int *btn_status = (int *)lv_event_get_param(e);
    lv_obj_t *play = lv_event_get_target(e);

    if (btn_status != NULL && *btn_status == 1) {
        if (video_status == VIDEO_STATUS_PLAYING) {
            lv_img_set_src(play, LVGL_PATH(video/pause.png));
        } else {
            lv_img_set_src(play, LVGL_PATH(video/play.png));
        }
        *btn_status = 0;
        return;
    }

    if (video_status == VIDEO_STATUS_PAUSE) {
        video_status = VIDEO_STATUS_PLAYING;
        lv_ffmpeg_player_set_cmd(default_player, LV_FFMPEG_PLAYER_CMD_START);
        lv_img_set_src(play, LVGL_PATH(video/pause.png));
    } else if (video_status == VIDEO_STATUS_PLAYING) {
        video_status = VIDEO_STATUS_PAUSE;
        gettimeofday(&end, NULL);
        double elapsed_time = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
        elapsed_time_ms -= elapsed_time;
        lv_ffmpeg_player_set_cmd(default_player, LV_FFMPEG_PLAYER_CMD_PAUSE);
        lv_img_set_src(play, LVGL_PATH(video/play.png));
        gettimeofday(&start, NULL);
    }
    lv_obj_invalidate(ui_video);
}

static void back_home_cb(lv_event_t *e)
{
    lv_event_code_t code = (lv_event_code_t)lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED) {
        return;
    }

    lv_timer_del(ui_play_loop_timer);

    media_list_destroy(default_list);
    lv_ffmpeg_player_set_cmd(default_player, LV_FFMPEG_PLAYER_CMD_STOP);
    lv_obj_clean(ui_video);

    ui_home_init();
    lv_scr_load_anim(ui_home, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
}

static void show_play_list_cb(lv_event_t *e)
{
    lv_event_code_t code = (lv_event_code_t)lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED) {
        return;
    }

    lv_obj_clear_flag(play_list_group, LV_OBJ_FLAG_HIDDEN);
    int child_cnt = lv_obj_get_child_cnt(play_list_group);

    for (int i = child_cnt - 1; i > 0; i--) {     /* except for "play_list_name" */
        lv_obj_t *play_video = lv_obj_get_child(play_list_group, i);
        lv_obj_t *btn = lv_obj_get_child(play_video, 0);
        lv_obj_t *name = lv_obj_get_child(play_video, 1);

        struct media_info video = {0};
        media_list_get_now_info(default_list, &video);
        char *label_name = lv_label_get_text(name);
        if (strncmp(label_name, video.name, strlen(label_name)) == 0) {
            lv_img_set_src(btn, LVGL_PATH(video/video_pink.png));
            lv_obj_set_style_text_color(name, lv_color_hex(0xED9EDE), LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            lv_img_set_src(btn, LVGL_PATH(video/video.png));
            lv_obj_set_style_text_color(name, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    hide_video_icon();
    ui_video_sidebar_anim_start(play_list_group, 1);
    lv_obj_invalidate(ui_video);
}

static void show_play_mode_cb(lv_event_t *e)
{
    lv_event_code_t code = (lv_event_code_t)lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED) {
        return;
    }

    lv_obj_clear_flag(play_mode_group, LV_OBJ_FLAG_HIDDEN);
    ui_video_sidebar_anim_start(play_mode_group, 1);
    hide_video_icon();
    lv_obj_invalidate(ui_video);
}

static void play_list_videos_set_default(void)
{
    int child_cnt = lv_obj_get_child_cnt(play_list_group);

    /* except for "play_list_name" */
    for (int i = child_cnt - 1; i > 0; i--) {
        lv_obj_t *play_video = lv_obj_get_child(play_list_group, i);
        lv_obj_t *btn = lv_obj_get_child(play_video, 0);
        lv_obj_t *name = lv_obj_get_child(play_video, 1);

        lv_img_set_src(btn, LVGL_PATH(video/video.png));
        lv_obj_set_style_text_color(name, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

static void play_mode_set_default(void)
{
    int child_cnt = lv_obj_get_child_cnt(play_mode_group);

    lv_obj_t *play_mode_one = lv_obj_get_child(play_mode_group, child_cnt - 1 - 2);
    lv_obj_t *play_mode_one_btn = lv_obj_get_child(play_mode_one, 0);
    lv_obj_t *play_mode_one_name = lv_obj_get_child(play_mode_one, 1);

    lv_img_set_src(play_mode_one_btn, LVGL_PATH(video/one.png));
    lv_obj_set_style_text_color(play_mode_one_name, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *play_mode_single_loop = lv_obj_get_child(play_mode_group, child_cnt - 1 - 1);
    lv_obj_t *play_mode_single_btn = lv_obj_get_child(play_mode_single_loop, 0);
    lv_obj_t *play_mode_single_name = lv_obj_get_child(play_mode_single_loop, 1);

    lv_img_set_src(play_mode_single_btn, LVGL_PATH(video/circle_single.png));
    lv_obj_set_style_text_color(play_mode_single_name, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *play_mode_list_loop = lv_obj_get_child(play_mode_group, child_cnt - 1);
    lv_obj_t *play_mode_list_btn = lv_obj_get_child(play_mode_list_loop, 0);
    lv_obj_t *play_mode_list_name = lv_obj_get_child(play_mode_list_loop, 1);

    lv_img_set_src(play_mode_list_btn, LVGL_PATH(video/circle_list.png));
    lv_obj_set_style_text_color(play_mode_list_name, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void select_video_to_play(lv_event_t *e)
{
    lv_event_code_t code = (lv_event_code_t)lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED) {
        return;
    }

    play_list_videos_set_default();

    lv_obj_t *play_video = lv_event_get_target(e);
    lv_obj_t *btn = lv_obj_get_child(play_video, 0);
    lv_obj_t *name = lv_obj_get_child(play_video, 1);

    lv_img_set_src(btn, LVGL_PATH(video/video_pink.png));
    lv_obj_set_style_text_color(name, lv_color_hex(0xED9EDE), LV_PART_MAIN | LV_STATE_DEFAULT);

    char *label_name = lv_label_get_text(name);
    media_list_set_pos(default_list, label_name);
    start_player = 0;
    /* show loading interface */
    lv_obj_clear_flag(loading_group, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(ui_video);
}

static void select_to_play_mode_one_time(lv_event_t *e)
{
    lv_event_code_t code = (lv_event_code_t)lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED) {
        return;
    }

    play_mode_set_default();

    lv_obj_t *play_mode_one = lv_event_get_target(e);
    lv_obj_t *btn = lv_obj_get_child(play_mode_one, 0);
    lv_obj_t *name = lv_obj_get_child(play_mode_one, 1);

    lv_img_set_src(btn, LVGL_PATH(video/one_pink.png));
    lv_obj_set_style_text_color(name, lv_color_hex(0xED9EDE), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_ffmpeg_player_set_auto_restart(default_player, false);
    lv_obj_invalidate(ui_video);
}

static void select_to_play_mode_single(lv_event_t *e)
{
    lv_event_code_t code = (lv_event_code_t)lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED) {
        return;
    }

    play_mode_set_default();

    lv_obj_t *play_mode_single = lv_event_get_target(e);
    lv_obj_t *btn = lv_obj_get_child(play_mode_single, 0);
    lv_obj_t *name = lv_obj_get_child(play_mode_single, 1);

    lv_img_set_src(btn, LVGL_PATH(video/circle_single_pink.png));
    lv_obj_set_style_text_color(name, lv_color_hex(0xED9EDE), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_ffmpeg_player_set_auto_restart(default_player, true);
    lv_obj_invalidate(ui_video);
}

static void select_to_play_mode_list(lv_event_t *e)
{
    lv_event_code_t code = (lv_event_code_t)lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED) {
        return;
    }

    play_mode_set_default();

    lv_obj_t *play_mode_list = lv_event_get_target(e);
    lv_obj_t *btn = lv_obj_get_child(play_mode_list, 0);
    lv_obj_t *name = lv_obj_get_child(play_mode_list, 1);

    lv_img_set_src(btn, LVGL_PATH(video/circle_list_pink.png));
    lv_obj_set_style_text_color(name, lv_color_hex(0xED9EDE), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_ffmpeg_player_set_auto_restart(default_player, false);
    play_loop_mode = VIDEO_PLAY_MODE_LIST_LOOP;
    lv_obj_invalidate(ui_video);
}

static void control_icon_group_init(void)
{
    play = ui_video_btn_icon_comp_create(ui_video, LVGL_PATH(video/pause.png),
                                         142, 146);
    lv_obj_set_pos(play, 0, 0);
    lv_obj_center(play);
    lv_obj_set_style_img_opa(play, LV_OPA_80, 0);
    lv_obj_clear_flag(play, LV_OBJ_FLAG_ADV_HITTEST);

    back = lv_imgbtn_create(ui_video);
    lv_imgbtn_set_src(back, LV_IMGBTN_STATE_RELEASED , NULL, LVGL_PATH(video/back.png), NULL);
    lv_imgbtn_set_src(back, LV_IMGBTN_STATE_PRESSED , NULL, LVGL_PATH(video/back_pink.png), NULL);
    lv_obj_set_pos(back, 28, 20);
    lv_obj_set_size(back, 32, 62);
    lv_obj_set_align(back, LV_ALIGN_TOP_LEFT);
    lv_obj_clear_flag(back, LV_OBJ_FLAG_SCROLLABLE);

    play_list = ui_video_btn_icon_comp_create(ui_video, LVGL_PATH(video/play_list.png),
                     68, 61);
    lv_obj_set_pos(play_list, 855, 10);

    play_mode = ui_video_btn_icon_comp_create(ui_video, LVGL_PATH(video/play_mode_list.png),
                     76, 76);
    lv_obj_set_pos(play_mode, 945, 3);

    struct media_info video = {0};
    media_list_get_now_info(default_list, &video);

    video_name = lv_label_create(ui_video);
    lv_obj_set_size(video_name, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_pos(video_name, 80, 30);
    lv_obj_set_align(video_name, LV_ALIGN_TOP_LEFT);
    lv_label_set_long_mode(video_name, LV_LABEL_LONG_SCROLL);
    lv_label_set_text(video_name, video.name);
    lv_obj_set_style_text_color(video_name, lv_color_hex(0xFFFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(video_name, LV_OPA_100, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(video_name, &lv_font_montserrat_36, LV_PART_MAIN| LV_STATE_DEFAULT);

    lv_obj_add_event_cb(play, play_video_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(back, back_home_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(play_list, show_play_list_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(play_mode, show_play_mode_cb, LV_EVENT_PRESSED, NULL);
}

static void play_list_group_init(void)
{
    play_list_group = lv_obj_create(ui_video);
    lv_obj_remove_style_all(play_list_group);
    lv_obj_set_pos(play_list_group, 724, 0);
    lv_obj_set_size(play_list_group, 300, 600);
    lv_obj_set_align(play_list_group, LV_ALIGN_TOP_LEFT);
    lv_obj_set_style_bg_color(play_list_group, lv_color_hex(0X0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(play_list_group, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_layout(play_list_group, LV_STYLE_FLEX_MAIN_PLACE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(play_list_group, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(play_list_group, LV_DIR_TOP | LV_DIR_BOTTOM | LV_DIR_VER);
    lv_obj_set_scrollbar_mode(play_list_group, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(play_list_group, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_style_base_dir(play_list_group, LV_BASE_DIR_LTR, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(play_list_group, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *play_list_name_container =  lv_obj_create(play_list_group);
    lv_obj_remove_style_all(play_list_name_container);
    lv_obj_clear_flag(play_list_name_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(play_list_name_container, LV_OPA_0, 0);
    lv_obj_set_size(play_list_name_container, 300, 80);

    lv_obj_t *play_list_name = lv_label_create(play_list_name_container);
    lv_obj_set_width(play_list_name, LV_SIZE_CONTENT);
    lv_obj_set_height(play_list_name, LV_SIZE_CONTENT);
    lv_obj_set_align(play_list_name, LV_ALIGN_CENTER);
    lv_obj_set_pos(play_list_name, 0, 0);
    lv_label_set_text(play_list_name, "Play List");
    lv_obj_set_style_text_color(play_list_name, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(play_list_name, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(play_list_name, &lv_font_montserrat_48, LV_PART_MAIN| LV_STATE_DEFAULT);

    for (int i = 0; i < default_list->num; i++) {
        lv_obj_t *play_list_video = ui_video_play_list_comp_create(play_list_group, default_list->info[i].name);
        lv_obj_add_event_cb(play_list_video, select_video_to_play, LV_EVENT_PRESSED, NULL);
    }
}

static void play_mode_group_init(void)
{
    play_mode_group = lv_obj_create(ui_video);
    lv_obj_remove_style_all(play_mode_group);
    lv_obj_set_pos(play_mode_group, 724, 0);
    lv_obj_set_size(play_mode_group, 300, 600);
    lv_obj_set_align(play_mode_group, LV_ALIGN_TOP_LEFT);
    lv_obj_set_style_bg_color(play_mode_group, lv_color_hex(0X0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(play_mode_group, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_layout(play_mode_group, LV_STYLE_FLEX_MAIN_PLACE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(play_mode_group, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(play_mode_group, LV_DIR_TOP | LV_DIR_BOTTOM | LV_DIR_VER);
    lv_obj_set_scrollbar_mode(play_mode_group, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(play_mode_group, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_style_base_dir(play_mode_group, LV_BASE_DIR_LTR, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(play_mode_group, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *play_list_name_container =  lv_obj_create(play_mode_group);
    lv_obj_remove_style_all(play_list_name_container);
    lv_obj_clear_flag(play_list_name_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(play_list_name_container, LV_OPA_0, 0);
    lv_obj_set_size(play_list_name_container, 300, 80);

    lv_obj_t *play_list_name = lv_label_create(play_list_name_container);
    lv_obj_set_width(play_list_name, LV_SIZE_CONTENT);
    lv_obj_set_height(play_list_name, LV_SIZE_CONTENT);
    lv_obj_set_align(play_list_name, LV_ALIGN_CENTER);
    lv_obj_set_pos(play_list_name, 0, 0);
    lv_label_set_text(play_list_name, "Play Mode");
    lv_obj_set_style_text_color(play_list_name, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(play_list_name, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(play_list_name, &lv_font_montserrat_48, LV_PART_MAIN| LV_STATE_DEFAULT);

    lv_obj_t *play_mode_one = ui_video_play_mode_comp_create(play_mode_group,
                   "One time", LVGL_PATH(video/one.png));

    lv_obj_t *play_mode_single = ui_video_play_mode_comp_create(play_mode_group,
                   "Sing loop", LVGL_PATH(video/circle_single.png));

    lv_obj_t *play_mode_list = ui_video_play_mode_comp_create(play_mode_group,
                   "List loop", LVGL_PATH(video/circle_list.png));

    lv_obj_add_event_cb(play_mode_one, select_to_play_mode_one_time, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(play_mode_single, select_to_play_mode_single, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(play_mode_list, select_to_play_mode_list, LV_EVENT_PRESSED, NULL);

    lv_event_send(play_mode_one, LV_EVENT_PRESSED, NULL); /* default play mode is onetime */
}

static void loading_group_init(void)
{
    loading_group =  lv_obj_create(ui_video);
    lv_obj_remove_style_all(loading_group);
    lv_obj_set_style_bg_opa(loading_group, LV_OPA_0, 0);
    lv_obj_set_pos(loading_group, 340, 235);
    lv_obj_set_size(loading_group, 460, 200);
    lv_obj_clear_flag(loading_group, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(loading_group, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *loading_group_icon =  lv_img_create(loading_group);
    lv_obj_set_pos(loading_group_icon, 0, 0);
    lv_obj_set_align(loading_group_icon, LV_ALIGN_TOP_LEFT);
    lv_img_set_src(loading_group_icon, LVGL_PATH(video/loading.png));
    lv_obj_set_size(loading_group_icon, 118, 118);

    lv_obj_t *loading_group_text = lv_label_create(loading_group);
    lv_obj_set_size(loading_group_text, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_pos(loading_group_text, 141, 40);
    lv_obj_set_align(loading_group_text, LV_ALIGN_TOP_LEFT);
    lv_label_set_text(loading_group_text, "Loading...");
    lv_obj_set_style_text_color(loading_group_text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(loading_group_text, LV_OPA_100, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(loading_group_text, &lv_font_montserrat_48, LV_PART_MAIN| LV_STATE_DEFAULT);
}

static void play_control_cb(lv_timer_t *tmr)
{
    struct media_info info = {0};
    double elapsed_time;

    if (start_player == 0) {
        media_list_get_now_info(default_list, &info);
        elapsed_time_ms = info.lengths;

        /* calling lv_ffmpeg_player_set_src function will cause the system to block for a long time */
        lv_ffmpeg_player_set_cmd(default_player, LV_FFMPEG_PLAYER_CMD_STOP);
        lv_ffmpeg_player_set_src(default_player, info.source_path);
        lv_ffmpeg_player_set_cmd(default_player, LV_FFMPEG_PLAYER_CMD_START);
        gettimeofday(&start, NULL);
        video_status = VIDEO_STATUS_PLAYING;
        play_btn_status = 1;
        start_player = 1;
        lv_event_send(play, LV_EVENT_PRESSED, &play_btn_status);
        lv_obj_add_flag(loading_group, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_opa(ui_video, LV_OPA_0, LV_PART_MAIN | LV_STATE_DEFAULT);
        return;
    }

    if (video_status == VIDEO_STATUS_PAUSE) {
        gettimeofday(&start, NULL);
        return;
    }

    gettimeofday(&end, NULL);
    elapsed_time = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
    elapsed_time_ms -= elapsed_time;
    gettimeofday(&start, NULL);
    if (elapsed_time_ms < 0 && start_player == 1) {
        if (play_loop_mode != VIDEO_PLAY_MODE_LIST_LOOP) {
            return;
        }
        int now_media = default_list->now_pos;
        int num = default_list->num;

        now_media++;
        now_media = now_media >= num ? 0 : now_media;
        default_list->now_pos = now_media;
        start_player = 0;
        video_status = VIDEO_STATUS_PAUSE;
        play_btn_status = 1;

        struct media_info video = {0};
        media_list_get_now_info(default_list, &video);
        lv_label_set_text(video_name, video.name);

        lv_event_send(play, LV_EVENT_PRESSED, &play_btn_status);
        lv_obj_set_style_bg_opa(ui_video, LV_OPA_100, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(loading_group, LV_OBJ_FLAG_HIDDEN);
    }
}

static struct media_list* video_list_create()
{
    default_list = media_list_create();

    struct media_info video = {0};
    memset(&video, 0, sizeof(struct media_info));
    strncpy(video.name, "Warriors of Future", strlen("Warriors of Future") + 1);
    snprintf(video.source_path, sizeof(video.source_path), "%s/video/%s", AIC_DEMO_VIDEO_DIR, "warriors_of_future.mp4");
    video.lengths = 29000;
    media_list_add_info(default_list, &video);

    memset(&video, 0, sizeof(struct media_info));
    strncpy(video.name, "Toy Story 4", strlen("Toy Story 4") + 1);
    snprintf(video.source_path, sizeof(video.source_path), "%s/video/%s", AIC_DEMO_VIDEO_DIR, "toy_story_4.mp4");
    video.lengths = 29000;
    media_list_add_info(default_list, &video);

    return default_list;
}

void ui_video_init(void)
{
    start_player = 0;

    ui_video = lv_obj_create(NULL);
    lv_obj_remove_style_all(ui_video);
    lv_obj_set_pos(ui_video, 0, 0);
    lv_obj_set_size(ui_video, 1024, 600);
    lv_obj_clear_flag(ui_video, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_video, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(ui_video, LV_OPA_100, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_video, lv_color_hex(0X0), LV_PART_MAIN | LV_STATE_DEFAULT);

    default_list = video_list_create();
    /* please read the introduction of ../model/lv_ffmpeg.h of using lv_ffmpeg_player_xxx  */
    /* Using ffmpeg can be understood as using lv_obj objects. Currently, it only supports
       modifying the actual playback size of the video by setting the control size
       behavior. It does not support rotation features.
     */
    default_player = lv_ffmpeg_player_create(ui_video);
    lv_obj_set_size(default_player, 1024, 600);
    lv_obj_set_pos(default_player, 0, 0);
    lv_obj_set_align(default_player, LV_ALIGN_TOP_LEFT);
    lv_ffmpeg_player_set_auto_restart(default_player, true);

    control_icon_group_init();
    play_list_group_init();
    play_mode_group_init();
    loading_group_init();

    ui_play_loop_timer = lv_timer_create(play_control_cb, 100, NULL);

    lv_obj_add_event_cb(ui_video, control_icon_disp_cb, LV_EVENT_PRESSED, NULL);
}
