// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
 * Authors:  Matteo <duanmt@artinchip.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <linux/fb.h>

#include "tslib.h"

#include <artinchip/sample_base.h>

#define FB_DEV "/dev/fb0"

#define BORDER_WIDTH_MAX	100

int g_fb_fd = -1;
int g_fb_xres = 0;
int g_fb_yres = 0;
int g_fb_len = 0;
unsigned char *g_fb_buf = NULL;

int g_debug = 0;
unsigned int g_border_width = 0;
unsigned int g_jump_thd = 4;
unsigned int g_smp_sum = 0;
unsigned int g_smp_out = 0;
unsigned int g_smp_jump = 0;
double g_elapsed_time = 0.0;

int fb_open(void)
{
	int i, j;
	struct fb_fix_screeninfo fix;
	struct fb_var_screeninfo var;

	g_fb_fd = open(FB_DEV, O_RDWR);
	if (g_fb_fd == -1) {
		ERR("open %s", FB_DEV);
		return -1;
	}

	if (ioctl(g_fb_fd, FBIOGET_FSCREENINFO, &fix) < 0) {
		ERR("ioctl FBIOGET_FSCREENINFO");
		close(g_fb_fd);
		return -1;
	}

	if (ioctl(g_fb_fd, FBIOGET_VSCREENINFO, &var) < 0) {
		ERR("ioctl FBIOGET_VSCREENINFO");
		close(g_fb_fd);
		return -1;
	}

	g_fb_xres = var.xres;
	g_fb_yres = var.yres;
	g_fb_len = fix.smem_len;

	g_fb_buf = mmap(NULL, g_fb_len,
			PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED,
		        g_fb_fd, 0);
	if (g_fb_buf == (unsigned char *)-1) {
		ERR("mmap framebuffer");
		close(g_fb_fd);
		g_fb_fd = -1;
		return -1;
	}
	memset(g_fb_buf, 0, g_fb_len);

	/* Draw a grid, and each cell size: 200*200 */

	for (i = 1; i * 200 < g_fb_yres; i++)
		memset(g_fb_buf + g_fb_xres*4*(200*i - 1), 0x30, g_fb_xres*4);

	for (i = 0; i < g_fb_yres; i++)
		for (j = 1; j * 200 < g_fb_xres; j++)
			memset(g_fb_buf + g_fb_xres*4*i + 200*4*j - 4, 0x30, 4);

	return 0;
}

void fb_close(void)
{
	if (g_fb_buf > 0) {
		memset(g_fb_buf, 0, g_fb_len);
		munmap(g_fb_buf, g_fb_len);
	}
	if (g_fb_fd > 0)
		close(g_fb_fd);
}

void usage(char *app)
{
	printf("Usage: %s [Options], built on %s %s\n", app, __DATE__, __TIME__);
	printf("\t-r, --raw       Use the raw coordinate, default: disable\n");
	printf("\t-d, --debug     Open more debug information, default: disable\n");
	printf("\t-b, --border    The width of border(will ignore), default 0\n");
	printf("\t-j, --jumb-thd  The threshold to determinate jumb sample, default 4\n");
	printf("\t-h, --help\n\n");
}

void update_stats(void)
{
	float out, jump;

	if (!g_smp_sum)
		return;

	out = (float)(g_smp_out*100)/(float)g_smp_sum;
	jump = (float)(g_smp_jump*100)/(float)g_smp_sum;
	printf("Stats: Sum %d, Out %d(%2.2f%%), Jump %d(%2.2f%%), Period %2.2f ms\n\n",
		g_smp_sum, g_smp_out, out, g_smp_jump, jump,
		g_elapsed_time/(double)g_smp_sum);
}

int diff(int m, int n)
{
	if (m == n)
		return 0;

	if (m > n)
		return m - n;
	else
		return n - m;
}

int max(int a, int b)
{
	if (a > b)
		return a;
	else
		return b;
}

void check_jump(int cur_x, int cur_y, int clear)
{
	static int xbuf[3] = {0};
	static int ybuf[3] = {0};
	int found = 0;

	xbuf[2] = xbuf[1];
	xbuf[1] = xbuf[0];
	xbuf[0] = cur_x;
	ybuf[2] = ybuf[1];
	ybuf[1] = ybuf[0];
	ybuf[0] = cur_y;

	if (xbuf[2] || ybuf[2]) {
		int d12 = max(diff(xbuf[2], xbuf[1]), diff(ybuf[2], ybuf[1]));
		int d10 = max(diff(xbuf[1], xbuf[0]), diff(ybuf[1], ybuf[0]));
		int d20 = max(diff(xbuf[2], xbuf[0]), diff(ybuf[2], ybuf[0]));

		if ((d12 > g_jump_thd) && (d10 > g_jump_thd)) {
			g_smp_jump++;
			found = 1;
		}
		else if ((d12 > g_jump_thd) && (d20 > g_jump_thd)) {
			g_smp_jump++;
			found = 1;
		}
		// else if ((d10 > g_jump_thd) && (d20 > g_jump_thd))
		// 	g_smp_jump++;
	}

	if (g_debug && found)
		printf("\t\t\t\t\t\t-> Jump %d\n", g_smp_jump);

	if (clear) {
		memset(xbuf, 0, 3*4);
		memset(ybuf, 0, 3*4);
	}
}

double timeval2ms(struct timeval *tv)
{
	return (double)(tv->tv_sec*1000 + (double)tv->tv_usec/1000);
}

void smp_elapsed_time(struct timeval *tv, int up)
{
	static struct timeval start_tv = {0};

	if (!up) {
		if (start_tv.tv_sec == 0)
			memcpy(&start_tv, tv, sizeof(struct timeval));
		return;
	}

	g_elapsed_time += timeval2ms(tv) - timeval2ms(&start_tv);
	start_tv.tv_sec = 0;
}

int main(int argc, char **argv)
{
	struct tsdev *ts = NULL;
	int ret = 0;
	char raw = 0;
	const struct option lopts[] = {
		{ "raw",      no_argument,       NULL, 'r'},
		{ "debug",    no_argument,       NULL, 'd'},
		{ "border",   required_argument, NULL, 'b'},
		{ "jumb-thd", required_argument, NULL, 'j'},
		{ "help",     no_argument,       NULL, 'h'},
	};

	while ((ret = getopt_long(argc, argv, "rhdb:j:", lopts, NULL)) != -1) {
		switch (ret) {
		case 'h':
			usage(argv[0]);
			goto draw_out;

		case 'r':
			raw = 1;
			DBG("Will use the raw coordinate\n");
			continue;

		case 'd':
			g_debug = 1;
			continue;

		case 'b':
			ret = str2int(optarg);
			if (ret > BORDER_WIDTH_MAX) {
				ERR("The border width %d is too big!\n", ret);
				continue;
			}
			g_border_width = ret;
			continue;

		case 'j':
			ret = str2int(optarg);
			if (ret > BORDER_WIDTH_MAX) {
				ERR("The jump threshold %d is too big\n", ret);
				continue;
			}
			g_jump_thd = ret;
			continue;

		default:
			ERR("Invalid parameter: %#x\n", ret);
			goto draw_out;
		}
	}

	ts = ts_setup(NULL, 0);
	if (!ts) {
		ERR("ts_setup");
		exit(1);
	}

	if (fb_open()) {
		fb_close();
		ts_close(ts);
		exit(1);
	}
	printf("FB res: X %d, Y %d，Size %d, Border %d, Jump thd %d\n",
		g_fb_xres, g_fb_yres, g_fb_len, g_border_width, g_jump_thd);

	while (1) {
		struct ts_sample samp;
		unsigned int position = 0, norm_x, norm_y;

		if (raw)
			ret = ts_read_raw(ts, &samp, 1);
		else
			ret = ts_read(ts, &samp, 1);
		if (ret < 0) {
			ERR("Failed to ts_read()");
			ret = -1;
			goto draw_out;
		}

		if (ret != 1)
			continue;

		g_smp_sum++;
		smp_elapsed_time(&samp.tv, !samp.pressure);

		if (raw) {
			norm_x = samp.x * g_fb_xres / 4096;
			norm_y = samp.y * g_fb_yres / 4096;
			if (g_debug)
				printf("%ld.%06ld: X - %4d(%3d) Y - %4d(%3d) P - %4d\n",
				       samp.tv.tv_sec, samp.tv.tv_usec,
				       samp.x, norm_x, samp.y, norm_y,
				       samp.pressure);
		}
		else {
			norm_x = samp.x;
			norm_y = samp.y;
			if (g_debug)
				printf("%ld.%06ld: X - %4d Y - %4d P - %4d\n",
				       samp.tv.tv_sec, samp.tv.tv_usec,
				       samp.x, samp.y, samp.pressure);
		}

		if (samp.x < g_border_width || samp.y < g_border_width
			|| samp.x > (g_fb_xres - g_border_width)
			|| samp.y > (g_fb_yres - g_border_width)) {
			g_smp_out++;
			if (g_debug)
				printf("\t\t\t\t\t\t-> Outside %d\n", g_smp_out);
			continue;
		}

		check_jump(samp.x, samp.y, !samp.pressure);
		if (samp.pressure == 0) {
			update_stats();
			continue;
		}

		position = norm_y * g_fb_xres * 4 + norm_x * 4;
		if (position < g_fb_len)
			memset(g_fb_buf + position, 0xFF, 4);
		else
			ERR("Invalid position: %d\n", position);
	}

draw_out:
	if (ts)
		ts_close(ts);
	fb_close();
	return ret;
}
