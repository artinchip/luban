/*
 * Copyright (C) 2022-2023 Artinchip Technology Co., Ltd.
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
#include "aic_dec.h"

#define IMG_CACHE_NUM 10

#if LV_USE_LOG
static void lv_user_log(const char *buf)
{
    printf("%s\n", buf);
}
#endif /* LV_USE_LOG */

int main(void)
{
#if LV_USE_LOG
    lv_log_register_print_cb(lv_user_log);
#endif /* LV_USE_LOG */

    /*LittlevGL init*/
    lv_init();

#if LV_IMG_CACHE_DEF_SIZE == 1
    lv_img_cache_set_size(IMG_CACHE_NUM);
#endif

    aic_dec_create();

    lv_port_disp_init();
    lv_port_indev_init();

    /*Create a Demo*/
#if LV_USE_DEMO_MUSIC == 1
    void lv_demo_music(void);
    lv_demo_music();
#else
    void base_ui_init();
    base_ui_init();
#endif

    /*Handle LitlevGL tasks (tickless mode)*/
    while (1) {
        lv_timer_handler();
        usleep(1000);
    }

    return 0;
}

/*Set in lv_conf.h as `LV_TICK_CUSTOM_SYS_TIME_EXPR`*/
uint32_t custom_tick_get(void)
{
    static uint64_t start_ms = 0;
    if (start_ms == 0) {
        struct timeval tv_start;
        gettimeofday(&tv_start, NULL);
        start_ms = (tv_start.tv_sec * 1000000 + tv_start.tv_usec) / 1000;
    }

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    uint64_t now_ms;
    now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;

    uint32_t time_ms = now_ms - start_ms;
    return time_ms;
}
