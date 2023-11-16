#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_USE_PERF_MONITOR         0
#define LV_COLOR_DEPTH              32
#define LV_IMG_CACHE_DEF_SIZE 1
#define LV_USE_MEM_MONITOR 0
#define LV_INDEV_DEF_READ_PERIOD 10
#define LV_DISP_DEF_REFR_PERIOD 10

#define LV_TICK_CUSTOM 1
#if LV_TICK_CUSTOM
    #define LV_TICK_CUSTOM_INCLUDE <stdint.h>
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
#endif

#define LV_SPRINTF_CUSTOM 1
#if LV_SPRINTF_CUSTOM
    #define LV_SPRINTF_INCLUDE <stdio.h>
    #define lv_snprintf  snprintf
    #define lv_vsnprintf vsnprintf
	#define LV_SPRINTF_USE_FLOAT 0
#endif  /*LV_SPRINTF_CUSTOM*/
