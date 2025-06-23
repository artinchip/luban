/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Zequan Liang <zequan.liang@artinchip.com>
 */
#ifdef USE_MSNLINK
#include "../ui.h"
#include "../model/msn/msn_slink.h"

#define SLINK_UNCONNECTED  0
#define SLINK_CONNECTING   1
#define SLINK_CONNECTED    2

lv_obj_t *ui_msn;
static lv_obj_t *ui_back_btn;
static lv_obj_t *ui_link_btn;
static lv_obj_t *ui_link_roller;
static lv_obj_t *ui_msn_bg_default;
static lv_obj_t *ui_msn_bg_black;
static lv_obj_t *ui_connect_status;

static lv_timer_t *ui_connect_timer;

static struct msn_slink *link;

static int last_connect_status = -1;

static void check_connection_status(lv_timer_t *tmr)
{
    static int connect_status = 0;

    connect_status = msn_slink_get_status(link);
    if (last_connect_status != connect_status) {
        if (connect_status == SLINK_UNCONNECTED) {
            msn_slink_stop(link);
            lv_obj_clear_flag(ui_msn_bg_default, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_link_roller, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_link_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_connect_status, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_msn_bg_black, LV_OBJ_FLAG_HIDDEN);
            lv_img_set_src(ui_link_btn, LVGL_PATH(msn/link_orange.png));
            lv_label_set_text(ui_connect_status, "Waiting for connection...");
        } else if (connect_status == SLINK_CONNECTING) {
            lv_label_set_text(ui_connect_status, "Connecting...");
        } else {
            lv_obj_add_flag(ui_msn_bg_default, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_link_roller, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_link_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_msn_bg_default, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_connect_status, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_msn_bg_black, LV_OBJ_FLAG_HIDDEN);
        }
    }

    last_connect_status = connect_status;
}

static void back_home_cb(lv_event_t *e)
{
    lv_event_code_t code = (lv_event_code_t)lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        msn_slink_stop(link);
        msn_slink_delete(link);

        lv_timer_del(ui_connect_timer);
        lv_obj_clean(ui_msn);

        ui_home_init();
        lv_scr_load_anim(ui_home, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
    }
}

static void start_link_cb(lv_event_t *e)
{
    lv_event_code_t code = (lv_event_code_t)lv_event_get_code(e);

    int connect_status = 0;
    if (code == LV_EVENT_PRESSED) {
        connect_status = msn_slink_get_status(link);
        if (connect_status == SLINK_UNCONNECTED) {
            lv_img_set_src(ui_link_btn, LVGL_PATH(msn/link_grey.png));
            msn_slink_start(link);
        }
    }
}

static LinkType link_name_corresponds_to_link_type(char *link_name)
{
    struct link_table {
        char *name;
        LinkType type;
    };

    struct link_table table[] = {
        {"airplay",      LINK_TYPE_AIRPLAY},
        {"carplay",      LINK_TYPE_CARPLAY},
        {"androidauto",  LINK_TYPE_ANDROIDAUTO},
        {"androidlink",  LINK_TYPE_ANDROIDLINK},
        {"carlife",      LINK_TYPE_CARLIFE},
        {"dlna",         LINK_TYPE_DLNA},
        {"hicar",        LINK_TYPE_HICAR},
    };

    int num = sizeof(table) / sizeof(table[0]);

    for (int i = 0; i < num; i++) {
        if (strncmp(link_name, table[i].name, strlen(table[i].name)) == 0) {
            return table[i].type;
        }
    }

    printf("link_name_corresponds_to_link_type error, name = %s\n", link_name);
    return -1;
}

static void select_link_cb(lv_event_t *e)
{
    lv_event_code_t code = (lv_event_code_t)lv_event_get_code(e);
    lv_obj_t *roller = lv_event_get_target(e);
    char buf[128] = {0};
    int link_type = -1;

    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_roller_get_selected_str(roller, buf, sizeof(buf));
        link_type = link_name_corresponds_to_link_type(buf);
        msn_slink_modify_connect_type(link, link_type);
    }
}

void ui_msn_init(void)
{
    last_connect_status = -1;
    link = msn_slink_create(0);

    ui_msn = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_msn, LV_OBJ_FLAG_SCROLLABLE);

    ui_msn_bg_default = lv_img_create(ui_msn);
    lv_img_set_src(ui_msn_bg_default, LVGL_PATH(msn/msn_bg.png));
    lv_obj_set_pos(ui_msn_bg_default, 0, 0);
    lv_obj_set_width(ui_msn_bg_default, 1024);
    lv_obj_set_height(ui_msn_bg_default, 600);
    lv_obj_set_align(ui_msn_bg_default, LV_ALIGN_TOP_LEFT);
    lv_obj_add_flag(ui_msn_bg_default, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_msn_bg_default, LV_OBJ_FLAG_SCROLLABLE);

    ui_msn_bg_black = lv_img_create(ui_msn);
    lv_obj_set_pos(ui_msn_bg_black, 0, 0);
    lv_obj_set_width(ui_msn_bg_black, 1024);
    lv_obj_set_height(ui_msn_bg_black, 600);
    lv_obj_set_style_bg_color(ui_msn_bg_black, lv_color_hex(0x0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_msn_bg_black, LV_OPA_100, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_add_flag(ui_msn_bg_black, LV_OBJ_FLAG_HIDDEN);

    ui_connect_status = lv_label_create(ui_msn);
    lv_obj_set_size(ui_connect_status, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_pos(ui_connect_status, 0, -220);
    lv_obj_set_align(ui_connect_status, LV_ALIGN_CENTER);
    lv_label_set_long_mode(ui_connect_status, LV_LABEL_LONG_SCROLL);
    lv_label_set_text(ui_connect_status, "Waiting for connection...");
    lv_obj_set_style_text_color(ui_connect_status, lv_color_hex(0xFFF0F2F9), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_connect_status, LV_OPA_100, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_connect_status, &lv_font_montserrat_48, LV_PART_MAIN| LV_STATE_DEFAULT);

    static lv_style_t btn_style_pressed;
    lv_style_init(&btn_style_pressed);
    lv_style_set_translate_x(&btn_style_pressed, 4);
    lv_style_set_translate_y(&btn_style_pressed, 4);

    ui_back_btn = lv_imgbtn_create(ui_msn);
    lv_imgbtn_set_src(ui_back_btn, LV_IMGBTN_STATE_RELEASED , NULL, LVGL_PATH(msn/back.png), NULL);
    lv_imgbtn_set_src(ui_back_btn, LV_IMGBTN_STATE_PRESSED , NULL, LVGL_PATH(msn/back.png), NULL);
    lv_obj_set_pos(ui_back_btn, 37, 41);
    lv_obj_set_size(ui_back_btn, 85, 85);
    lv_obj_set_align(ui_back_btn, LV_ALIGN_TOP_LEFT);
    lv_obj_clear_flag(ui_back_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(ui_back_btn, &btn_style_pressed, LV_STATE_PRESSED);

    ui_link_btn =  lv_img_create(ui_msn);
    lv_img_set_src(ui_link_btn, LVGL_PATH(msn/link_orange.png));
    lv_obj_set_pos(ui_link_btn, 63, 232);
    lv_obj_set_size(ui_link_btn, 200, 200);
    lv_obj_set_align(ui_link_btn, LV_ALIGN_TOP_LEFT);
    lv_obj_add_flag(ui_link_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_link_btn, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_link_btn, LV_OBJ_FLAG_SCROLLABLE);

    static lv_style_t style_sel;
    lv_style_init(&style_sel);
    lv_style_set_text_font(&style_sel, &lv_font_montserrat_36);
    lv_style_set_bg_color(&style_sel, lv_color_hex(0xFFFF9497));

    static lv_style_t style_bg;
    lv_style_init(&style_bg);
    lv_style_set_bg_grad_color(&style_bg, lv_color_hex(0xFF4A555F));
    lv_style_set_border_opa(&style_bg, LV_OPA_100);
    lv_style_set_border_width(&style_bg, 3);
    lv_style_set_bg_color(&style_bg, lv_color_hex(0xFFF0F2F9));
    lv_style_set_bg_opa(&style_bg, LV_OPA_100);

    const char *opts = "carlife\n"
                       "carplay\n"
                       "androidauto\n"
                       "androidlink\n"
                       "airplay\n"
                       "dlna\n"
                       "hicar";
    ui_link_roller = lv_roller_create(ui_msn);
    lv_obj_set_pos(ui_link_roller, 0, 0);
    lv_obj_set_size(ui_link_roller, 350, 400);
    lv_obj_set_align(ui_link_roller, LV_ALIGN_CENTER);
    lv_roller_set_options(ui_link_roller, opts, LV_ROLLER_MODE_INFINITE);
    lv_roller_set_visible_row_count(ui_link_roller, 7);
    lv_obj_add_style(ui_link_roller, &style_sel, LV_PART_SELECTED);
    lv_obj_add_style(ui_link_roller, &style_bg, LV_PART_MAIN);

    lv_obj_add_event_cb(ui_link_roller, select_link_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_back_btn, back_home_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(ui_link_btn, start_link_cb, LV_EVENT_PRESSED, NULL);

    ui_connect_timer = lv_timer_create(check_connection_status, 200, NULL);
}
#endif
