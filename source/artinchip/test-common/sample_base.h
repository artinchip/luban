// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
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

/* Base data type */

typedef int		s32;
typedef short		s16;
typedef char		s8;
typedef unsigned int	u32;
typedef unsigned short	u16;
typedef unsigned char	u8;
typedef signed int	socket;

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

#ifdef __cplusplus
}
#endif

#endif	// end of _SAMPLES_BASE_H_
