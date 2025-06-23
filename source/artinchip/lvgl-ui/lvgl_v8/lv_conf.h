/*
 * Copyright (C) 2022-2024, ArtInchip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  ArtInChip
 */

#ifndef LV_CONF_H
#define LV_CONF_H


// using LV_AIC_COLOR_SCREEN_TRANSP instead of LV_COLOR_SCREEN_TRANSP
#define LV_AIC_COLOR_SCREEN_TRANSP 1

#define  LV_SUPPORT_SET_IMAGE_STRIDE 1
#define LV_USE_FLEX       1
#define LV_USE_DEMO_MUSIC 0
#define LV_USE_DEMO_BENCHMARK 0
#define LV_BUILD_EXAMPLES 1

#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_MONTSERRAT_36 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_12 1

#define LV_USE_LOG 1
#if LV_USE_LOG
    /*How important log should be added:
    *LV_LOG_LEVEL_TRACE       A lot of logs to give detailed information
    *LV_LOG_LEVEL_INFO        Log important events
    *LV_LOG_LEVEL_WARN        Log if something unwanted happened but didn't cause a problem
    *LV_LOG_LEVEL_ERROR       Only critical issue, when the system may fail
    *LV_LOG_LEVEL_USER        Only logs added by the user
    *LV_LOG_LEVEL_NONE        Do not log anything*/
    #define LV_LOG_LEVEL LV_LOG_LEVEL_WARN

    /*1: Print the log with 'printf';
    *0: User need to register a callback with `lv_log_register_print_cb()`*/
    #define LV_LOG_PRINTF 0

    /*Enable/disable LV_LOG_TRACE in modules that produces a huge number of logs*/
    #define LV_LOG_TRACE_MEM        1
    #define LV_LOG_TRACE_TIMER      1
    #define LV_LOG_TRACE_INDEV      1
    #define LV_LOG_TRACE_DISP_REFR  1
    #define LV_LOG_TRACE_EVENT      1
    #define LV_LOG_TRACE_OBJ_CREATE 1
    #define LV_LOG_TRACE_LAYOUT     1
    #define LV_LOG_TRACE_ANIM       1
#endif  /*LV_USE_LOG*/

#define LV_USE_PERF_MONITOR         0
#define LV_COLOR_DEPTH              32
#define LV_IMG_CACHE_DEF_SIZE 1
#define LV_USE_MEM_MONITOR 0
#define LV_INDEV_DEF_READ_PERIOD 10
#define LV_DISP_DEF_REFR_PERIOD 10

#define LV_TICK_CUSTOM 1
#if LV_TICK_CUSTOM
    #define LV_TICK_CUSTOM_INCLUDE <aic_ui.h>
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR (custom_tick_get()) /*system time in ms*/
#endif   /*LV_TICK_CUSTOM*/

#define LV_USE_FS_POSIX 1
#if LV_USE_FS_POSIX
    #define LV_FS_POSIX_LETTER 'L'
    #define LV_FS_POSIX_PATH ""
    #define LV_FS_POSIX_CACHE_SIZE 0
#endif

#define LV_MEM_CUSTOM 1
#if LV_MEM_CUSTOM == 1
    #define LV_MEM_CUSTOM_INCLUDE <stdlib.h>   /*Header for the dynamic memory function*/
    #define LV_MEM_CUSTOM_ALLOC   malloc
    #define LV_MEM_CUSTOM_FREE    free
    #define LV_MEM_CUSTOM_REALLOC realloc
#endif     /*LV_MEM_CUSTOM*/

#define LV_SPRINTF_CUSTOM 1
#if LV_SPRINTF_CUSTOM
    #define LV_SPRINTF_INCLUDE <stdio.h>
    #define lv_snprintf  snprintf
    #define lv_vsnprintf vsnprintf
    #define LV_SPRINTF_USE_FLOAT 0
#endif  /*LV_SPRINTF_CUSTOM*/

#define LV_USE_FREETYPE 0
#if LV_USE_FREETYPE
    // Memory used by FreeType to cache characters [bytes]
    #define LV_FREETYPE_CACHE_SIZE (256 * 1024)
    #if LV_FREETYPE_CACHE_SIZE >= 0
        // 1: bitmap cache use the sbit cache, 0:bitmap cache use the image cache.
        // sbit cache:it is much more memory efficient for small bitmaps(font size < 256)
        // if font size >= 256, must be configured as image cache */
        #define LV_FREETYPE_SBIT_CACHE 0
        // Maximum number of opened FT_Face/FT_Size objects managed by this cache instance.
        // (0:use system defaults)
        #define LV_FREETYPE_CACHE_FT_FACES 0
        #define LV_FREETYPE_CACHE_FT_SIZES 0
    #endif
#endif

// only used by ArtInChip
#define LV_CACHE_IMG_NUM 15

// should at the end of lv_conf.h
#if defined(LV_USE_CONF_CUSTOM)
#include "lv_conf_custom.h"
#endif

#endif // LV_CONF_H
