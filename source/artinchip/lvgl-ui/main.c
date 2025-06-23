/*
 * Copyright (C) 2022-2024 ArtInChip Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <sys/time.h>
#include <sched.h>
#include <assert.h>
#include "lvgl/lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "aic_ui.h"

uint32_t custom_tick_get(void)
{
    static uint64_t start_ms = 0;
    if (start_ms == 0) {
        struct timespec tv_start;
        clock_gettime(CLOCK_MONOTONIC, &tv_start);
        start_ms = tv_start.tv_sec * 1000 + tv_start.tv_nsec / 1000000;
    }

    struct timespec tv_now;
    clock_gettime(CLOCK_MONOTONIC, &tv_now);
    uint64_t now_ms;
    now_ms = tv_now.tv_sec * 1000 + tv_now.tv_nsec / 1000000;

    uint32_t time_ms = (uint32_t)(now_ms - start_ms);
    return time_ms;
}

#if LV_USE_LOG
#if LVGL_VERSION_MAJOR == 8
static void lv_user_log(const char *buf)
{
    printf("%s\n", buf);
}
#else
static void lv_user_log(lv_log_level_t level, const char *buf)
{
    (void)level;
    printf("%s\n", buf);
}
#endif
#endif /* LV_USE_LOG */

int main(void)
{
#if LV_USE_LOG
    lv_log_register_print_cb(lv_user_log);
#endif /* LV_USE_LOG */

    /*LittlevGL init*/
    lv_init();

    lv_port_disp_init();
    lv_port_indev_init();

#if LV_IMG_CACHE_DEF_SIZE == 1
    lv_img_cache_set_size(LV_CACHE_IMG_NUM);
#endif

    /*Create a Demo*/
#if LV_USE_DEMO_MUSIC == 1
    void lv_demo_music(void);
    lv_demo_music();
#else
    void ui_init(void);
    ui_init();
#endif

#if LVGL_VERSION_MAJOR == 9
    lv_tick_set_cb(&custom_tick_get);
#endif

    /*Handle LitlevGL tasks (tickless mode)*/
    while (1) {
        lv_timer_handler();
        usleep(1000);
    }

    return 0;
}
