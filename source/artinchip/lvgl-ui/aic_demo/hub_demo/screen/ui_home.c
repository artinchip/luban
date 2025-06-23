/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors: Zequan Liang <zequan.liang@artinchip.com>
 */

#include "../ui.h"

#include "../model/model_base.h"

lv_obj_t *ui_home;
static lv_obj_t *ui_tabview;
static lv_obj_t *ui_tabview_0;
static lv_obj_t *ui_home_bg;

static lv_obj_t *ui_home_dashboard_icon;
static lv_obj_t *ui_home_music_icon;
static lv_obj_t *ui_home_video_icon;
static lv_obj_t *ui_home_kitchen_icon;
static lv_obj_t *ui_home_zjinnova_icon;
static lv_obj_t *ui_home_carbit_icon;
static lv_obj_t *ui_home_msn_icon;

static lv_obj_t *ui_home_dashboard_label;
static lv_obj_t *ui_home_music_label;
static lv_obj_t *ui_home_video_label;
static lv_obj_t *ui_home_kitchen_label;
static lv_obj_t *ui_home_zjinnova_label;
static lv_obj_t *ui_home_carbit_label;
static lv_obj_t *ui_home_msn_label;

static lv_timer_t *ui_home_timer;
static lv_obj_t *ui_home_time_label;

uint32_t SWITCH_HOME_EVENT;

static void ui_home_custom_event_register()
{
    if (SWITCH_HOME_EVENT != 0) {
        SWITCH_HOME_EVENT = lv_event_register_id();
    }
}

/* simulate timer function */
static void ui_home_timer_cb(lv_timer_t *tmr)
{
    static int minutes = 0, seconds = 0;

    seconds++;
    if (seconds == 60) {
        seconds = 0;
        minutes++;
    }

    if (minutes == 60) {
        minutes = 0;
    }

    if (minutes < 10 && seconds < 10) {
        lv_label_set_text_fmt(ui_home_time_label, "0%d:0%d", minutes, seconds);
    } else if (minutes > 10 && seconds < 10) {
        lv_label_set_text_fmt(ui_home_time_label, "%d:0%d", minutes, seconds);
    } else if (minutes < 10 && seconds > 10) {
        lv_label_set_text_fmt(ui_home_time_label, "0%d:%d", minutes, seconds);
    } else if (minutes > 10 && seconds > 10) {
        lv_label_set_text_fmt(ui_home_time_label, "%d:%d", minutes, seconds);
    }
}

static void ui_home_time_init(lv_obj_t *parent)
{
    ui_home_time_label = lv_label_create(parent);
    lv_obj_set_size(ui_home_time_label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_pos(ui_home_time_label, 24, 45);
    lv_label_set_text(ui_home_time_label, "00:00");
    lv_obj_set_style_text_color(ui_home_time_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_home_time_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_home_time_label, &ui_font_Title, 0);

    ui_home_timer = lv_timer_create(ui_home_timer_cb, 1000, NULL);
}

static void enter_dashboard_ui(lv_event_t *e)
{
    lv_event_code_t code = (lv_event_code_t)lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        lv_timer_del(ui_home_timer);
        lv_obj_clean(ui_home);
        ui_dashboard_init();
        lv_scr_load_anim(ui_dashboard, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
    }
}

static void enter_music_ui(lv_event_t *e)
{
    lv_event_code_t code = (lv_event_code_t)lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        lv_timer_del(ui_home_timer);
        lv_obj_clean(ui_home);
        ui_music_init();
        lv_scr_load_anim(ui_music, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
    }
}

static void enter_video_ui(lv_event_t *e)
{
    lv_event_code_t code = (lv_event_code_t)lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        lv_timer_del(ui_home_timer);
        lv_obj_clean(ui_home);
        ui_video_init();
        lv_scr_load_anim(ui_video, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
    }
}

static void enter_kitchen_ui(lv_event_t *e)
{
    lv_event_code_t code = (lv_event_code_t)lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {;}
}

static void enter_zjinnova_ui(lv_event_t *e)
{
    lv_event_code_t code = (lv_event_code_t)lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {;}
}

static void enter_carbit_ui(lv_event_t *e)
{
    lv_event_code_t code = (lv_event_code_t)lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {;}
}

static void enter_msn(lv_event_t *e)
{
#ifdef USE_MSNLINK
    lv_event_code_t code = (lv_event_code_t)lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        lv_timer_del(ui_home_timer);
        lv_obj_clean(ui_home);
        ui_msn_init();
        lv_scr_load_anim(ui_msn, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
    }
#endif
}

static void ui_home_dashboard_init(lv_obj_t *parent)
{
    ui_home_dashboard_icon = ui_home_app_icon_comp_create(parent, LVGL_PATH(home/dashborad.png));
    lv_obj_set_pos(ui_home_dashboard_icon, 232, 139);

    ui_home_dashboard_label = ui_home_app_label_comp_create(parent, "dashboard");
    lv_obj_set_pos(ui_home_dashboard_label, 217, 242);

    lv_obj_add_event_cb(ui_home_dashboard_icon, enter_dashboard_ui, LV_EVENT_PRESSED, NULL);
}

static void ui_home_music_init(lv_obj_t *parent)
{
    ui_home_music_icon = ui_home_app_icon_comp_create(parent, LVGL_PATH(home/music.png));
    lv_obj_set_pos(ui_home_music_icon, 426, 139);

    ui_home_music_label = ui_home_app_label_comp_create(parent, "music");
    lv_obj_set_pos(ui_home_music_label, 437, 242);

    lv_obj_add_event_cb(ui_home_music_icon, enter_music_ui, LV_EVENT_PRESSED, NULL);
}

static void ui_home_video_init(lv_obj_t *parent)
{
    ui_home_video_icon = ui_home_app_icon_comp_create(parent, LVGL_PATH(home/video.png));
    lv_obj_set_pos(ui_home_video_icon, 629, 139);

    ui_home_video_label = ui_home_app_label_comp_create(parent, "video");
    lv_obj_set_pos(ui_home_video_label, 645, 242);

    lv_obj_add_event_cb(ui_home_video_icon, enter_video_ui, LV_EVENT_PRESSED, NULL);
}

static void ui_home_kitchen_init(lv_obj_t *parent)
{
    ui_home_kitchen_icon = ui_home_app_icon_comp_create(parent, LVGL_PATH(home/kitchen.png));
    lv_obj_set_pos(ui_home_kitchen_icon, 823, 139);

    ui_home_kitchen_label = ui_home_app_label_comp_create(parent, "kitchen");
    lv_obj_set_pos(ui_home_kitchen_label, 825, 242);

    lv_obj_add_event_cb(ui_home_kitchen_icon, enter_kitchen_ui, LV_EVENT_PRESSED, NULL);
}

static void ui_home_zjinnova_init(lv_obj_t *parent)
{
    ui_home_zjinnova_icon = ui_home_app_icon_comp_create(parent, LVGL_PATH(home/zjinnova.png));
    lv_obj_set_pos(ui_home_zjinnova_icon, 232, 327);

    ui_home_zjinnova_label = ui_home_app_label_comp_create(parent, "zjinnova");
    lv_obj_set_pos(ui_home_zjinnova_label, 232, 430);

    lv_obj_add_event_cb(ui_home_zjinnova_icon, enter_zjinnova_ui, LV_EVENT_PRESSED, NULL);
}

static void ui_home_carbit_init(lv_obj_t *parent)
{
    ui_home_carbit_icon = ui_home_app_icon_comp_create(parent, LVGL_PATH(home/carbit.png));
    lv_obj_set_pos(ui_home_carbit_icon, 426, 327);

    ui_home_carbit_label = ui_home_app_label_comp_create(parent, "carbit");
    lv_obj_set_pos(ui_home_carbit_label, 439, 430);

    lv_obj_add_event_cb(ui_home_carbit_icon, enter_carbit_ui, LV_EVENT_PRESSED, NULL);
}

static void ui_home_msn(lv_obj_t *parent)
{
    ui_home_msn_icon = ui_home_app_icon_comp_create(parent, LVGL_PATH(home/msn_link.png));
    lv_obj_set_pos(ui_home_msn_icon, 629, 327);

    ui_home_msn_label = ui_home_app_label_comp_create(parent, "msn");
    lv_obj_set_pos(ui_home_msn_label, 650, 430);

    lv_obj_add_event_cb(ui_home_msn_icon, enter_msn, LV_EVENT_PRESSED, NULL);
}

static void ui_home_app_init(lv_obj_t *parent)
{
    ui_home_dashboard_init(parent);
    ui_home_music_init(parent);
    ui_home_video_init(parent);
    ui_home_kitchen_init(parent);
    ui_home_zjinnova_init(parent);
    ui_home_carbit_init(parent);
    ui_home_msn(parent);
}

void ui_home_init(void)
{
    ui_home = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_home, LV_OBJ_FLAG_SCROLLABLE);
    ui_home_custom_event_register();

    ui_home_bg = lv_img_create(ui_home);
    lv_img_set_src(ui_home_bg, LVGL_PATH(home/home_bg.jpg));
    lv_obj_set_pos(ui_home_bg, 0, 0);
    lv_obj_set_width(ui_home_bg, 1024);
    lv_obj_set_height(ui_home_bg, 600);
    lv_obj_set_align(ui_home_bg, LV_ALIGN_TOP_LEFT);
    lv_obj_add_flag(ui_home_bg, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_home_bg, LV_OBJ_FLAG_SCROLLABLE);

    ui_tabview = lv_tabview_create(ui_home, LV_DIR_TOP, 0);
    lv_obj_set_pos(ui_tabview, 0, 0);
    lv_obj_set_style_bg_opa(ui_tabview, LV_OPA_0, 0);

    ui_tabview_0 = lv_tabview_add_tab(ui_tabview, "Interface 0");
    lv_obj_set_pos(ui_tabview_0, 0, 0);
    lv_obj_set_style_bg_opa(ui_tabview_0, LV_OPA_0, 0);

    ui_home_time_init(ui_tabview_0);

    ui_home_app_init(ui_tabview_0);
}
