// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020-2025 ArtInChip Technology Co., Ltd.
 * Authors:  Matteo <duanmt@artinchip.com>
 */
#ifndef _SAMPLES_BASE_H_
#define _SAMPLES_BASE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#define DEBUG
//#undef DEBUG

#ifdef DEBUG

#define DBG(string, args...) \
		do { \
			printf("%s()%d - ", __func__, __LINE__); \
			printf(string, ##args); \
		} while(0)
#define ERR(string, args...) \
		do { \
			printf("[ERROR] %s()%d - ", __func__, __LINE__); \
			printf(string, ##args); \
		} while(0)

#else

#define DBG(string, args...)  \
		do { \
		} while(0)
#define ERR(string, args...)   \
		do { \
		} while(0)

#endif

#define US_PER_SEC      1000000

/* Base data type */

typedef int		s32;
typedef short		s16;
typedef char		s8;
typedef unsigned int	u32;
typedef unsigned short	u16;
typedef unsigned char	u8;

/* Struct */

/* Functions */

static inline long long int str2int(char *_str)
{
	if (_str == NULL) {
		ERR("The string is empty!\n");
		return -1;
	}

	if (strncmp(_str, "0x", 2))
		return atoi(_str);
	else
		return strtoll(_str, NULL, 16);
}

static double timeval_diff(struct timeval *start, struct timeval *end)
{
	double diff;

	if (end->tv_usec < start->tv_usec) {
		diff = (double)(US_PER_SEC + end->tv_usec - start->tv_usec) / US_PER_SEC;
		diff += end->tv_sec - 1 - start->tv_sec;
	} else {
		diff = (double)(end->tv_usec - start->tv_usec) / US_PER_SEC;
		diff += end->tv_sec - start->tv_sec;
	}
	return diff;
}

static void show_fps(char *mod, struct timeval *start, struct timeval *end, int cnt)
{
	double diff = timeval_diff(start, end);

	printf("%s frame rate: %.1f, frame %d / %.1f seconds\n",
		   mod, (double)cnt / diff, cnt, diff);
}

#ifdef __cplusplus
}
#endif

#endif	// end of _SAMPLES_BASE_H_
