/*
 * Copyright (C) 2022-2023 Artinchip Technology Co., Ltd.
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#include <signal.h>
#include <sys/time.h>
#include <linux/fb.h>
#include <artinchip/sample_base.h>
#include <video/artinchip_fb.h>
#include "mpp_ge.h"

#define FB_DEV "/dev/fb0"
#define URANDOM_DEV "/dev/urandom"

static int g_urandom_fd = 0;
static int g_screen_w = 0;
static int g_screen_h = 0;
static int g_fb_fd = 0;
static int g_fb_len = 0;
static int g_fb_stride = 0;
static unsigned int g_fb_format = 0;
static unsigned int g_fb_phy = 0;
unsigned char *g_fb_buf = NULL;

static int urandom_open(void)
{
	g_urandom_fd = open(URANDOM_DEV, O_RDWR);
	if (g_urandom_fd == -1) {
		ERR("open %s", URANDOM_DEV);
		return -1;
	}
	return 0;
}

static void urandom_close(void)
{
	if (g_urandom_fd > 0)
		close(g_urandom_fd);
}

static int fb_open(void)
{
	struct fb_fix_screeninfo fix;
	struct fb_var_screeninfo var;
	struct aicfb_layer_data layer;

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

	if(ioctl(g_fb_fd, AICFB_GET_FB_LAYER_CONFIG, &layer) < 0) {
		ERR("ioctl  AICFB_GET_LAYER_CONFIG\n");
		close(g_fb_fd);
		return -1;
	}

	g_screen_w = var.xres;
	g_screen_h = var.yres;
	g_fb_len = fix.smem_len;
	g_fb_phy = fix.smem_start;
	g_fb_stride = fix.line_length;
	g_fb_format = layer.buf.format;

	g_fb_buf = mmap(NULL, g_fb_len,
			PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED,
		        g_fb_fd, 0);
	if (g_fb_buf == (unsigned char *)-1) {
		ERR("mmap framebuffer");
		close(g_fb_fd);
		g_fb_fd = -1;
		g_fb_buf = NULL;
		return -1;
	}

	return 0;
}

static void fb_close(void)
{
	if (!g_fb_buf) {
		munmap(g_fb_buf, g_fb_len);
	}
	if (g_fb_fd > 0)
		close(g_fb_fd);
}

static void ge_set_fillrect(struct ge_fillrect *fill)
{
	unsigned int value[5] = {0};
	unsigned int x, y, width, height;

	do {
		read(g_urandom_fd, value, sizeof(value));

		x = value[1] % g_screen_w;
		y = value[2] % g_screen_h;
		width = value[3] % (g_screen_w - x);
		height = value[4] % (g_screen_h - y);

	} while (!width || !height);

	fill->type = GE_NO_GRADIENT;
	fill->start_color = value[0];
	fill->end_color = 0;
	fill->dst_buf.buf_type = MPP_PHY_ADDR;
	fill->dst_buf.phy_addr[0] = g_fb_phy;
	fill->dst_buf.stride[0] = g_fb_stride;
	fill->dst_buf.size.width = g_screen_w;
	fill->dst_buf.size.height = g_screen_h;
	fill->dst_buf.format = g_fb_format;
	fill->ctrl.flags = 0;

	fill->dst_buf.crop_en = 1;
	fill->dst_buf.crop.x = x;
	fill->dst_buf.crop.y = y;
	fill->dst_buf.crop.width = width;
	fill->dst_buf.crop.height = height;
}

static void usage(char *app)
{
	printf("Usage: %s [Options], built on %s %s\n", app, __DATE__, __TIME__);
	printf("\t-n, --number  the number of rectangles to fill (default 1000)\n");
	printf("\t-u, --usage\n\n");
}

int main(int argc, char **argv)
{
	struct mpp_ge *ge = NULL;
	int ret = 0, i = 0;
	int num = 1000;
	struct ge_fillrect fill;

	const struct option lopts[] = {
		{ "number",	     required_argument, NULL, 'n'},
		{ "usage",           no_argument,       NULL, 'u'},
	};

	while ((ret = getopt_long(argc, argv, "n:u", lopts, NULL)) != -1) {
		switch (ret) {
		case 'u':
			usage(argv[0]);
			goto EXIT;
		case 'n':
			num = str2int(optarg);
			break;
		default:
			ERR("Invalid parameter: %#x\n", ret);
			goto EXIT;
		}
	}

	ge = mpp_ge_open();
	if (!ge) {
		ERR("open ge device\n");
		exit(1);
	}

	if (fb_open()) {
		fb_close();
		mpp_ge_close(ge);
		ERR("fb_open\n");
		exit(1);
	}

	if (urandom_open()) {
		fb_close();
		mpp_ge_close(ge);
		ERR("urandom_open()\n");
		exit(1);
	}

	memset(&fill, 0, sizeof(struct ge_fillrect));

	do {
		ge_set_fillrect(&fill);

		ret =  mpp_ge_fillrect(ge, &fill);
		if (ret) {
			ERR("fill task:%d\n", ret);
			goto EXIT;

		}

		ret = mpp_ge_emit(ge);
		if (ret) {
			ERR("emit task failed:%d\n", ret);
			goto EXIT;
		}

		ret = mpp_ge_sync(ge);
		if (ret) {
			ERR("sync task failed:%d\n", ret);
			goto EXIT;
		}
		usleep(10000);

		i++;
	} while(i < num);

EXIT:
	if (ge)
		mpp_ge_close(ge);

	fb_close();
	urandom_close();
	return ret;
}
