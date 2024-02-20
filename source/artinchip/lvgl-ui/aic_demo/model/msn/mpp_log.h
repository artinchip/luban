/*
* Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
*
*  author: artinchip
*  Desc: log module
*/

#ifndef MPP_LOG_H
#define MPP_LOG_H

#include <stdio.h>
#include <sys/time.h>

enum log_level {
	LOGL_ERROR = 0,
	LOGL_WARNING,
	LOGL_INFO,
	LOGL_DEBUG,
	LOGL_VERBOSE,

	LOGL_COUNT,

	LOGL_DEFAULT = 	LOGL_ERROR,
	LOGL_FORCE_DEBUG = 0x10,
};

#ifdef LOG_DEBUG
#define _LOG_DEBUG	LOGL_FORCE_DEBUG
#else
#define _LOG_DEBUG	0
#endif

#ifndef LOG_TAG
#define LOG_TAG "aic_mpp"
#endif

#define TAG_ERROR	"error  "
#define TAG_WARNING	"warning"
#define TAG_INFO	"info   "
#define TAG_DEBUG	"debug  "
#define TAG_VERBOSE	"verbose"

#define mpp_log(level, tag, fmt, arg...) ({ \
	int _l = level; \
	if (((_LOG_DEBUG != 0) && (_l <= LOGL_DEBUG)) || \
	    (_l <= LOGL_DEFAULT)) \
		printf("%s: %s <%s:%d>: "fmt"\n", tag, LOG_TAG, __FUNCTION__, __LINE__, ##arg); \
	})

#define loge(fmt, arg...) mpp_log(LOGL_ERROR, TAG_ERROR, "\033[40;31m"fmt"\033[0m", ##arg)
#define logw(fmt, arg...) mpp_log(LOGL_WARNING, TAG_WARNING, "\033[40;33m"fmt"\033[0m", ##arg)
#define logi(fmt, arg...) mpp_log(LOGL_INFO, TAG_INFO, "\033[40;32m"fmt"\033[0m", ##arg)
#define logd(fmt, arg...) mpp_log(LOGL_DEBUG, TAG_DEBUG, fmt, ##arg)
#define logv(fmt, arg...) mpp_log(LOGL_VERBOSE, TAG_VERBOSE, fmt, ##arg)

#define mpp_assert(cond) do { \
	if (!(cond)) { \
		loge("Assertion failed!"); \
	} \
} while (0)

#define time_start(tag) struct timeval time_##tag##_start; gettimeofday(&time_##tag##_start, NULL)
#define time_end(tag) struct timeval time_##tag##_end;gettimeofday(&time_##tag##_end, NULL);\
			fprintf(stderr, #tag " time: %ld us\n",\
			((time_##tag##_end.tv_sec - time_##tag##_start.tv_sec)*1000000) +\
			(time_##tag##_end.tv_usec - time_##tag##_start.tv_usec))

#define MPP_ABS(x,y) ((x>y)?(x-y):(y-x))

#endif
