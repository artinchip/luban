/*
 * Copyright (C) 2024, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_USE_LOG 1
#if LV_USE_LOG

    /*How important log should be added:
    *LV_LOG_LEVEL_TRACE       A lot of logs to give detailed information
    *LV_LOG_LEVEL_INFO        Log important events
    *LV_LOG_LEVEL_WARN        Log if something unwanted happened but didn't cause a problem
    *LV_LOG_LEVEL_ERROR       Only critical issue, when the system may fail
    *LV_LOG_LEVEL_USER        Only logs added by the user
    *LV_LOG_LEVEL_NONE        Do not log anything*/
    #define LV_LOG_LEVEL LV_LOG_LEVEL_ERROR

    /*1: Print the log with 'printf';
    *0: User need to register a callback with `lv_log_register_print_cb()`*/
    #define LV_LOG_PRINTF 1

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

#define LV_COLOR_DEPTH          32

#define LV_USE_MEM_MONITOR 0
#define LV_USE_PERF_MONITOR 1
#define LV_USE_SYSMON   1
#define LV_INDEV_DEF_READ_PERIOD 10

#define LV_USE_STDLIB_MALLOC    LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING    LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_CLIB

#define LV_USE_FS_POSIX 1
#if LV_USE_FS_POSIX
    #define LV_FS_POSIX_LETTER 'L'     /*Set an upper cased letter on which the drive will accessible (e.g. 'A')*/
    #define LV_FS_POSIX_PATH ""         /*Set the working directory. File/directory paths will be appended to it.*/
    #define LV_FS_POSIX_CACHE_SIZE 0    /*>0 to cache this number of bytes in lv_fs_read()*/
#endif

/*FreeType library*/
#define LV_USE_FREETYPE 0
#if LV_USE_FREETYPE
    /*Let FreeType to use LVGL memory and file porting*/
    #define LV_FREETYPE_USE_LVGL_PORT 0

    /*Cache count of the glyphs in FreeType. It means the number of glyphs that can be cached.
     *The higher the value, the more memory will be used.*/
    #define LV_FREETYPE_CACHE_FT_GLYPH_CNT 256
#endif

#define LV_USE_GIF 1

#define LV_CACHE_DEF_SIZE 10 * 1024 * 1024

#define LV_IMAGE_HEADER_CACHE_DEF_CNT  20

#define LV_DEF_REFR_PERIOD 10

#define LV_USE_PARALLEL_DRAW_DEBUG 0

#define LV_DRAW_BUF_STRIDE_ALIGN 8

#define LV_DRAW_BUF_ALIGN 64

#define LV_USE_MONKEY 1

#define LV_USE_OS LV_OS_PTHREAD

#define LV_IMG_CACHE_DEF_SIZE 1

// only used by ArtInChip
#define LV_CACHE_IMG_NUM 15

// should at the end of lv_conf.h
#if defined(LV_USE_CONF_CUSTOM)
#include "lv_conf_custom.h"
#endif

#endif // LV_CONF_H
