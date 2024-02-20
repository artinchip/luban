// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2024 ArtInChip Technology Co., Ltd.
 * Author: weijie.ding <weijie.ding@artinchip.com>
 */
#ifndef GOERTZEL_H_
#define GOERTZEL_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <errno.h>
#include <getopt.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <malloc.h>
#include <stdbool.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>
#include <locale.h>
#include <alsa/asoundlib.h>
#include <assert.h>
#include <poll.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <endian.h>

double logarithm(unsigned int i, double temp, double base, double num,
		 unsigned int acc);
double log10(double x);
int goertzel_test(void);

#endif

