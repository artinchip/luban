/*
 * Copyright (C) 2025, Artinchip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#include "lv_port_indev.h"

#include "lvgl/lvgl.h"
#include "../lvgl/src/display/lv_display_private.h"

#if USE_EVDEV != 0 || USE_BSD_EVDEV
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#if USE_BSD_EVDEV
#include <dev/evdev/input.h>
#else
#include <linux/input.h>
#endif

#if USE_XKB
#include "xkb.h"
#endif /* USE_XKB */

#if USE_TSLIB
#include "tslib.h"
struct tsdev *ts;
#endif /* USE_TSLIB */

#if USE_ENCODER
int encoder_evdev_fd;
int encoder_button = LV_INDEV_STATE_REL;
int encoder_diff = 0;
bool encoder_evdev_set_file(char* dev_name);
#endif

bool evdev_set_file(char* dev_name);
int map(int x, int in_min, int in_max, int out_min, int out_max);

/**********************
 *  STATIC VARIABLES
 **********************/
int evdev_fd = -1;
int evdev_root_x;
int evdev_root_y;
int evdev_button;

int evdev_key_val;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Initialize the evdev interface
 */
void evdev_init(void)
{
    if (!evdev_set_file(EVDEV_NAME)) {
        return;
    }

#if USE_XKB
    xkb_init();
#endif

#if USE_ENCODER
    if (!encoder_evdev_set_file(EVDEV_NAME)) {
        return;
    }
#endif
}
/**
 * reconfigure the device file for evdev
 * @param dev_name set the evdev device filename
 * @return true: the device file set complete
 *         false: the device file doesn't exist current system
 */
bool evdev_set_file(char* dev_name)
{
     if(evdev_fd != -1) {
        close(evdev_fd);
     }
#if USE_TSLIB == 0
#if USE_BSD_EVDEV
     evdev_fd = open(dev_name, O_RDWR | O_NOCTTY);
#else
     evdev_fd = open(dev_name, O_RDWR | O_NOCTTY | O_NDELAY);
#endif

     if(evdev_fd == -1) {
        perror("unable to open evdev interface:");
        return false;
     }

#if USE_BSD_EVDEV
     fcntl(evdev_fd, F_SETFL, O_NONBLOCK);
#else
     fcntl(evdev_fd, F_SETFL, O_ASYNC | O_NONBLOCK);
#endif
#else
     ts = ts_setup(NULL, 1);
     if (!ts) {
        perror("ts_setup");
        return false;
     }
#endif
     evdev_root_x = 0;
     evdev_root_y = 0;
     evdev_key_val = 0;
     evdev_button = LV_INDEV_STATE_REL;

     return true;
}

#if USE_ENCODER
bool encoder_evdev_set_file(char* dev_name)
{
     if(encoder_evdev_fd != -1) {
        close(encoder_evdev_fd);
     }

     encoder_evdev_fd = open(dev_name, O_RDWR | O_NOCTTY | O_NDELAY);
     if(encoder_evdev_fd == -1) {
        perror("unable to open evdev interface:");
        return false;
     }
     fcntl(encoder_evdev_fd, F_SETFL, O_ASYNC | O_NONBLOCK);

     encoder_button = LV_INDEV_STATE_REL;
     encoder_diff = 0;
     return true;
}
#endif
/**
 * Get the current position and state of the evdev
 * @param data store the evdev data here
 */
void evdev_read(lv_indev_t * drv, lv_indev_data_t * data)
{
#if USE_TSLIB == 0
    struct input_event in;
    lv_indev_type_t type = lv_indev_get_type(drv);

    while(read(evdev_fd, &in, sizeof(struct input_event)) > 0) {
        if(in.type == EV_REL) {
            if(in.code == REL_X)
#if EVDEV_SWAP_AXES
                evdev_root_y += in.value;
#else
                evdev_root_x += in.value;
#endif
            else if(in.code == REL_Y)
#if EVDEV_SWAP_AXES
                evdev_root_x += in.value;
#else
                evdev_root_y += in.value;
#endif
        } else if(in.type == EV_ABS) {
            if(in.code == ABS_X) {
#if EVDEV_SWAP_AXES
                evdev_root_y = in.value;
#else
                evdev_root_x = in.value;
#endif
            } else if(in.code == ABS_Y) {
#if EVDEV_SWAP_AXES
                evdev_root_x = in.value;
#else
                evdev_root_y = in.value;
#endif
           } else if(in.code == ABS_MT_POSITION_X) {
#if EVDEV_SWAP_AXES
                evdev_root_y = in.value;
#else
                evdev_root_x = in.value;
#endif
           } else if(in.code == ABS_MT_POSITION_Y) {
#if EVDEV_SWAP_AXES
                evdev_root_x = in.value;
#else
                evdev_root_y = in.value;
#endif
           } else if(in.code == ABS_MT_TRACKING_ID) {
                if(in.value == -1)
                    evdev_button = LV_INDEV_STATE_REL;
                else if(in.value == 0)
                    evdev_button = LV_INDEV_STATE_PR;
            }
        } else if(in.type == EV_KEY) {
            if(in.code == BTN_MOUSE || in.code == BTN_TOUCH) {
                if(in.value == 0)
                    evdev_button = LV_INDEV_STATE_REL;
                else if(in.value == 1)
                    evdev_button = LV_INDEV_STATE_PR;
            } else if(type == LV_INDEV_TYPE_KEYPAD) {
#if USE_XKB
                data->key = xkb_process_key(in.code, in.value != 0);
#else
                switch(in.code) {
                    case KEY_BACKSPACE:
                        data->key = LV_KEY_BACKSPACE;
                        break;
                    case KEY_ENTER:
                        data->key = LV_KEY_ENTER;
                        break;
                    case KEY_PREVIOUS:
                        data->key = LV_KEY_PREV;
                        break;
                    case KEY_NEXT:
                        data->key = LV_KEY_NEXT;
                        break;
                    case KEY_UP:
                        data->key = LV_KEY_UP;
                        break;
                    case KEY_LEFT:
                        data->key = LV_KEY_LEFT;
                        break;
                    case KEY_RIGHT:
                        data->key = LV_KEY_RIGHT;
                        break;
                    case KEY_DOWN:
                        data->key = LV_KEY_DOWN;
                        break;
                    case KEY_TAB:
                        data->key = LV_KEY_NEXT;
                        break;
                    default:
                        data->key = 0;
                        break;
                }
#endif /* USE_XKB */
                if (data->key != 0) {
                    /* Only record button state when actual output is produced to prevent widgets from refreshing */
                    data->state = (in.value) ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
                }
                evdev_key_val = data->key;
                evdev_button = data->state;
                return;
            }
        }
    }

    if(type == LV_INDEV_TYPE_KEYPAD) {
        /* No data retrieved */
        data->key = evdev_key_val;
        data->state = evdev_button;
        return;
    }
    if(type != LV_INDEV_TYPE_POINTER)
        return;
#else
    struct ts_sample samp;
    while(ts_read(ts, &samp, 1) == 1) {
        #if EVDEV_SWAP_AXES
            evdev_root_x = samp.y;
            evdev_root_y = samp.x;
        #else
            evdev_root_x = samp.x;
            evdev_root_y = samp.y;
        #endif

        if(samp.pressure == 0)
            evdev_button = LV_INDEV_STATE_REL;
        else
            evdev_button = LV_INDEV_STATE_PR;
    }

#endif
    /*Store the collected data*/

    lv_display_t *disp = lv_indev_get_display(drv);

#if EVDEV_CALIBRATE
    data->point.x = map(evdev_root_x, EVDEV_HOR_MIN, EVDEV_HOR_MAX, 0, disp->hor_res);
    data->point.y = map(evdev_root_y, EVDEV_VER_MIN, EVDEV_VER_MAX, 0, disp->ver_res);
#else
    data->point.x = evdev_root_x;
    data->point.y = evdev_root_y;
#endif

    data->state = evdev_button;

    if(data->point.x < 0)
      data->point.x = 0;
    if(data->point.y < 0)
      data->point.y = 0;
    if(data->point.x >= disp->hor_res)
      data->point.x = disp->hor_res - 1;
    if(data->point.y >= disp->ver_res)
      data->point.y = disp->ver_res - 1;

    return;
}

#if USE_ENCODER
/* Map to -1, 0, 1 */
static int lv_encoder_diff_filtering(int diff)
{
    static int last_diff;
    static int sum;
    const int threshold = 200; /* the higher the threshold, the lower the sensitivity */

    /* last time was inconsistent with this move, start calculating the sum */
    if ((last_diff > 0 && diff > 0) || (last_diff < 0 && diff < 0)) {
        sum += diff;
    } else {
        sum = 0;
    }

    last_diff = diff;
    if (sum >= threshold) {
        sum = 0;
        return 1;
    } else if (sum <= -threshold) {
        sum = 0;
        return -1;
    }
    return 0;
}

void encoder_evdev_read(lv_indev_t * drv, lv_indev_data_t * data)
{
    struct input_event ev;
    lv_indev_type_t type = lv_indev_get_type(drv);

    if(type != LV_INDEV_TYPE_ENCODER)
        return;

    while(read(encoder_evdev_fd, &ev, sizeof(struct input_event)) > 0) {
        if (ev.type == EV_REL && ev.code == REL_X) {
            encoder_diff = lv_encoder_diff_filtering(ev.value);
            printf("encoder_diff = %d, %d\n", ev.value, encoder_diff);
        }
        if (ev.type == EV_KEY) {
            encoder_button = ev.value ? LV_INDEV_STATE_REL : LV_INDEV_STATE_PR;
            printf("encoder_button = %d\n", encoder_button);
        }
    }
    data->state = encoder_button;
    data->enc_diff = encoder_diff;
    encoder_diff = 0;
}
#endif
/**********************
 *   STATIC FUNCTIONS
 **********************/
int map(int x, int in_min, int in_max, int out_min, int out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void lv_port_indev_init(void)
{
    static lv_indev_t *indev_touchpad;

    evdev_init();

    indev_touchpad = lv_indev_create();
#ifndef USE_ENCODER
    lv_indev_set_type(indev_touchpad, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev_touchpad, evdev_read);
#else
    lv_group_t *g = lv_group_create();
    lv_group_set_default(g);
    lv_indev_set_type(indev_touchpad, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(indev_touchpad, encoder_evdev_read);
    lv_indev_set_group(indev_touchpad, g);
#endif
}

#endif
