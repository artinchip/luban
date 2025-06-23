/*
 * Copyright (C) 2022-2023 ArtinChip Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#include "../ui.h"
#include "aic_ui.h"
#include "../model/model_base.h"

lv_obj_t *ui_default;
static lv_style_t back_style_pressed;
static lv_obj_t *title;
static lv_obj_t *bg_logo;
static lv_obj_t *ui_speed;
static lv_obj_t *fps_title;
static lv_obj_t *cpu_title;
static lv_obj_t *mem_title;
static lv_obj_t *fps_info;
static lv_obj_t *cpu_info;
static lv_obj_t *mem_info;
static lv_timer_t *perf_stats_timer;
static lv_timer_t *point_rotate_timer;
static lv_anim_t anim;
static int rot_angle = 255;

static void enter_home_ui(lv_event_t *e)
{
    lv_event_code_t code = (lv_event_code_t)lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        lv_anim_del(&anim, NULL);
        lv_timer_del(perf_stats_timer);
        lv_timer_del(point_rotate_timer);

        lv_obj_clean(ui_default);
        ui_home_init();
        lv_scr_load_anim(ui_home, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
    }
}

static void timer_callback(lv_timer_t *tmr)
{
    char data_str[128];
    float cpu_usage = 0.0;
    float mem_usage = 0.0;
    int fps = 0;
    (void)tmr;

    /* frame rate */
    fps = get_fb_draw_fps();
    snprintf(data_str, 128, "%2d fps", fps);
    lv_label_set_text(fps_info, data_str);

    /* cpu usage */
    cpu_usage = get_cpu_usage();
    snprintf(data_str, 128, "%.2f%%\n", cpu_usage);
    lv_label_set_text(cpu_info, data_str);

    /* mem usage */
    mem_usage = get_mem_usage();
    snprintf(data_str, 128, "%.2fMB\n", mem_usage);
    lv_label_set_text(mem_info, data_str);
}

static int angle2speed(int angle)
{
    float speed;
    float ratio;
    float range;

    range = (360 - 255 + 105);

    if (angle >= 255 && angle < 360)
        speed = (float)(angle - 255);
    else
        speed = (float)(angle + 360 - 255);

    if (speed > 0) {
        ratio = 160.0 / range;
        speed = speed * ratio;
    }

    return (int)speed;
}

static void point_callback(lv_timer_t *tmr)
{
    char speed_str[8];

    (void)tmr;

    int speed = angle2speed(rot_angle);
    snprintf(speed_str, 8, "%d", speed);
    lv_label_set_text(ui_speed, speed_str);
}

static void anim_set_angle(void *var, int32_t v)
{
    static int rot_direct;

    if (rot_direct == 0)
        rot_angle += 1;
    else
        rot_angle -= 1;

    if (rot_angle >= 360)
        rot_angle = 0;

    if (rot_angle < 0)
        rot_angle = 359;

    if (rot_direct == 0 && rot_angle == 106) {
        rot_angle = 104;
        rot_direct = 1;
    }

    if (rot_direct == 1 && rot_angle == 254) {
        rot_angle = 253;
        rot_direct = 0;
    }

    lv_img_set_angle(var, rot_angle * 10);
}

static void point_aimation(lv_obj_t *obj)
{
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_values(&anim, 0, 3600);
    lv_anim_set_time(&anim, 1000);

    //lv_anim_set_delay(&anim, delay + 0);
    lv_anim_set_playback_delay(&anim, 0);
    lv_anim_set_playback_time(&anim, 0);
    lv_anim_set_repeat_delay(&anim, 0);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&anim, lv_anim_path_linear);
    lv_anim_set_exec_cb(&anim, anim_set_angle);
    lv_anim_start(&anim);
}

void ui_default_init(void)
{
    ui_default = lv_obj_create(NULL);
    lv_obj_set_pos(ui_default, 0, 0);
    lv_obj_set_size(ui_default, 1024, 600);
    lv_obj_clear_flag(ui_default, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_default, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(ui_default, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(ui_default, LVGL_PATH(default/global_bg.png), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *img_ck = lv_img_create(ui_default);
    lv_img_set_src(img_ck, LVGL_PATH(default/meter_clk.png));
    lv_obj_set_pos(img_ck, 50, 152);

    lv_obj_t *img_point = lv_img_create(ui_default);
    lv_img_set_src(img_point, LVGL_PATH(default/meter_point.png));
    lv_obj_set_pos(img_point, 192, 200);
    lv_img_set_pivot(img_point, 12, 108);
    lv_img_set_angle(img_point, rot_angle * 10);
    lv_img_set_antialias(img_point, 0);

    lv_obj_t *img_info = lv_img_create(ui_default);
    lv_img_set_src(img_info, LVGL_PATH(default/meter_info.png));
    lv_obj_set_pos(img_info, 340, 125);

    lv_obj_t *exit_info = lv_label_create(ui_default);
    lv_obj_set_width(exit_info, LV_SIZE_CONTENT);
    lv_obj_set_height(exit_info, LV_SIZE_CONTENT);
    lv_obj_align(exit_info, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_label_set_text(exit_info, "click on any area to exit");
    lv_obj_set_style_text_color(exit_info, lv_color_hex(0x00A0EF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(exit_info, LV_OPA_50, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(exit_info, &lv_font_montserrat_36, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_speed = lv_label_create(ui_default);
    lv_obj_set_width(ui_speed, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_speed, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_speed, 192);
    lv_obj_set_y(ui_speed, 380);
    lv_label_set_text(ui_speed, "0");
    lv_obj_set_style_text_color(ui_speed, lv_color_hex(0xF9E09D), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_speed, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_speed, &ui_font_Big, LV_PART_MAIN | LV_STATE_DEFAULT);

    title = lv_label_create(ui_default);
    lv_obj_set_width(title, LV_SIZE_CONTENT);
    lv_obj_set_height(title, LV_SIZE_CONTENT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_pos(title, 0, 10);
    lv_label_set_text(title, "Default");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(title, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(title, &ui_font_Title, LV_PART_MAIN | LV_STATE_DEFAULT);

    bg_logo = lv_label_create(ui_default);
    lv_obj_set_width(bg_logo, LV_SIZE_CONTENT);
    lv_obj_set_height(bg_logo, LV_SIZE_CONTENT);
    lv_obj_align(bg_logo, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_pos(bg_logo, -30, -30);
    lv_label_set_text(bg_logo, "ArtInChip");
    lv_obj_set_style_text_color(bg_logo, lv_color_hex(0x00FFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(bg_logo, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(bg_logo, &ui_font_Big, LV_PART_MAIN | LV_STATE_DEFAULT);

    fps_title = lv_label_create(ui_default);
    lv_obj_set_width(fps_title, LV_SIZE_CONTENT);
    lv_obj_set_height(fps_title, LV_SIZE_CONTENT);
    lv_obj_set_pos(fps_title, 425, 286);
    lv_label_set_text(fps_title, "Frame rate");
    lv_obj_set_style_text_color(fps_title, lv_color_hex(0x00A0EF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(fps_title, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(fps_title, &ui_font_Title, LV_PART_MAIN | LV_STATE_DEFAULT);

    cpu_title = lv_label_create(ui_default);
    lv_obj_set_width(cpu_title, LV_SIZE_CONTENT);
    lv_obj_set_height(cpu_title, LV_SIZE_CONTENT);
    lv_obj_set_pos(cpu_title, 615, 286);
    lv_label_set_text(cpu_title, "CPU usage");
    lv_obj_set_style_text_color(cpu_title, lv_color_hex(0x00A0EF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(cpu_title, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(cpu_title, &ui_font_Title, LV_PART_MAIN | LV_STATE_DEFAULT);

    mem_title = lv_label_create(ui_default);
    lv_obj_set_width(mem_title, LV_SIZE_CONTENT);
    lv_obj_set_height(mem_title, LV_SIZE_CONTENT);
    lv_obj_set_pos(mem_title, 795, 286);
    lv_label_set_text(mem_title, "Mem usage");
    lv_obj_set_style_text_color(mem_title, lv_color_hex(0x00A0EF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(mem_title, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(mem_title, &ui_font_Title, LV_PART_MAIN | LV_STATE_DEFAULT);

    fps_info = lv_label_create(ui_default);
    lv_obj_set_width(fps_info, LV_SIZE_CONTENT);
    lv_obj_set_height(fps_info, LV_SIZE_CONTENT);
    lv_obj_set_pos(fps_info, 464, 345);
    lv_label_set_text(fps_info, "0");
    lv_obj_set_style_text_color(fps_info, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(fps_info, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(fps_info, &ui_font_Big, LV_PART_MAIN | LV_STATE_DEFAULT);

    cpu_info = lv_label_create(ui_default);
    lv_obj_set_width(cpu_info, LV_SIZE_CONTENT);
    lv_obj_set_height(cpu_info, LV_SIZE_CONTENT);
    lv_obj_set_pos(cpu_info, 644, 345);
    lv_label_set_text(cpu_info, "0");
    lv_obj_set_style_text_color(cpu_info, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(cpu_info, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(cpu_info, &ui_font_Big, LV_PART_MAIN | LV_STATE_DEFAULT);

    mem_info = lv_label_create(ui_default);
    lv_obj_set_width(mem_info, LV_SIZE_CONTENT);
    lv_obj_set_height(mem_info, LV_SIZE_CONTENT);
    lv_obj_set_pos(mem_info, 824, 345);
    lv_label_set_text(mem_info, "0");
    lv_obj_set_style_text_color(mem_info, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(mem_info, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(mem_info, &ui_font_Big, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_style_init(&back_style_pressed);
    lv_style_set_bg_color(&back_style_pressed, lv_palette_main(LV_PALETTE_GREY));
    lv_style_set_bg_img_recolor_opa(&back_style_pressed, 50);

    point_aimation(img_point);
    perf_stats_timer = lv_timer_create(timer_callback, 500, 0);
    point_rotate_timer = lv_timer_create(point_callback, 100, 0);

    lv_obj_add_event_cb(ui_default, enter_home_ui, LV_EVENT_CLICKED, NULL);
}
