// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023 ArtInChip Technology Co., Ltd.
 * Author: weijie.ding <weijie.ding@artinchip.com>
 */


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
#include <string.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/ioctl.h>

int main(void)
{
	int fd;
	struct stat buf;

	system("aplay /usr/share/sounds/alsa/Front_Left.wav | arecord -Dhw:0,1 "
	       "-s 1920 -f S16_LE -c 1 -r 48000 -t raw /tmp/test");

	fd = open("/tmp/test", O_RDONLY);
	if (fd < 0) {
		printf("open file error\n");
		return -1;
	}

	fstat(fd, &buf);

	if (buf.st_size == 1920 * 2) {
		printf("Audio test success\n");
		return 0;
	}
	else {
		printf("Audio test error\n");
		return -1;
	}
}

