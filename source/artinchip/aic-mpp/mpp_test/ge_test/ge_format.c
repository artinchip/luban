/*
 * Copyright (C) 2022-2023 ArtinChip Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  ZeQuan Liang <zequan.liang@artinchip.com>
 */

#include <stdlib.h>

#include <signal.h>
#include <sys/time.h>
#include <linux/fb.h>
#include <artinchip/sample_base.h>
#include <video/artinchip_fb.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>

#include "mpp_ge.h"

#define FB_DEV		"/dev/fb0"
#define DMA_HEAP_DEV	"/dev/dma_heap/reserved"
#define SINGER_32BIT_IMAGE	DATADIR"/singer_alpha.bmp"

#define BYTE_ALIGN(x, byte) (((x) + ((byte) - 1))&(~((byte) - 1)))

/* stride in different formats */
#define RGB_STRIDE    		0
#define YUV_STRIDE_Y  		0
#define YUV_STRIDE_U_ADD_V 	1
#define YUV_STRIDE_U_OR_V 	1
#define YUV_STRIDE_YUV 		0	/* stride = y + u + v */

/* height in different formats */
#define RGB_HEIGHT		0
#define YUV_HEIGHT_Y  		0
#define YUV_HEIGHT_U_OR_V 	1
#define YUV_HEIGHT_YUV 		0

#define PLANE_1		1
#define PLANE_2 	2
#define PLANE_3 	3

#define RGB_LENGTH 	0
#define YUV_LENGTH_Y    0
#define YUV_LENGTH_UV   1

/* screen display area selection */
#define SRC_DISP_REGION 0
#define DST_DISP_REGION 1

/* format conversion type */
#define RGB_TO_RGB 	0
#define RGB_TO_YUV 	1
#define YUV_TO_YUV 	2
#define YUV_TO_RGB 	3

/* str to format table struct */
struct StrToFormat {
	char *str;
	int format;
};

/* bmp header format */
#pragma pack(push, 1)
struct BmpHeader {
	unsigned short type;
	unsigned int size;
	unsigned short reserved1;
	unsigned short reserved2;
	unsigned int offset;

	unsigned int dib_size;
	int width;
	int height;
	unsigned short planes;
	unsigned short bit_count;
	unsigned int compression;
	unsigned int size_image;
	int x_meter;
	int y_meter;
	unsigned int clr_used;
	unsigned int clr_important;
};
#pragma pack(pop)

static int g_screen_w = 0;
static int g_screen_h = 0;
static int g_fb_fd = 0;
static int g_fb_len = 0;
static int g_fb_stride = 0;
static unsigned int g_fb_format = 0;
static unsigned int g_fb_phy = 0;
static unsigned char *g_fb_buf = NULL;
static int table_size = 0;
struct StrToFormat *format_table = NULL;
struct BmpHeader bmp_header = {0};

static void usage(char *app)
{
	printf("Usage: %s [Options], built on %s %s\n", app, __DATE__, __TIME__);
	printf("\t-m, --mode, select format conversion type (default RgbToRgb), please use -h or --help to list the selectable types\n");
	printf("\t-s, --scale, select dst scale (default 1), scale from 1/16 to 16\n");

	printf("\t-u, --usage\n");
	printf("\t-h, --help to see more\n\n");
}

static void help(void)
{
	printf("\r--The src layer is displayed on the left side of the screen.\n");
	printf("\r--The dst layer is displayed on the right side of the screen.\n");
	printf("\r--Format conversion supports RgbToRgb, YuvToYuv, YuvToRgb and RgbToYuv.\n");
	printf("\r--The mode parameter supports uppercase and lowercase letters. such as -m RgbToRgb and -m RGBTORGB\n\n");

	printf("\r--RGB format supports: \n");
	printf("\r--ARGB8888, ABGR8888, RGBA8888, BGRA8888\n");
	printf("\r--XRGB8888, XBGR8888, RGBX8888, BGRX8888\n");
	printf("\r--ARGB4444, ABGR4444, RGBA4444, BGRA4444\n");
	printf("\r--ARGB1555, ABGR1555, RGBA5551, BGRA5551\n");
	printf("\r--RGB888,   BGR888,   RGB565,   BGR565\n\n");

	printf("\r--YUV format supports: \n");
	printf("\r--YUV420, NV12, NV21\n");
	printf("\r--YUV422, NV16, NV61 , YUYV, YVYU, UYVY, VYUY\n");
	printf("\r--YUV400, YUV444\n");

	printf("\r--Please use parameter -u or --usage to list other parameters.\n\n");
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
		ERR("ioctl FBIOGET_VSCREENINFO");
		close(g_fb_fd);
		return -1;
	}

	g_screen_w = var.xres;
	g_screen_h = var.yres;
	g_fb_len = fix.smem_len;
	g_fb_phy = fix.smem_start;
	g_fb_stride = layer.buf.stride[0];
	g_fb_format = layer.buf.format;

	g_fb_buf = mmap(NULL, g_fb_len,
			PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED,
		        g_fb_fd, 0);
	if (g_fb_buf == MAP_FAILED) {
		ERR("mmap framebuffer");
		close(g_fb_fd);
		g_fb_fd = -1;
		g_fb_buf = NULL;
		return -1;
	}

	memset(g_fb_buf, 0, g_fb_len);

	return 0;
}

static void fb_close(void)
{
	if (!g_fb_buf)
		munmap(g_fb_buf, g_fb_len);

	if (g_fb_fd > 0)
		close(g_fb_fd);
}

static int ori_bmp_open(const char *bmp_path)
{
	int ori_fd = -1;

	ori_fd = open(bmp_path, O_RDWR);
	if (ori_fd < 0) {
		ERR("Failed to open %s, errno: %d[%s]\n",
			bmp_path, errno, strerror(errno));
		return -1;
	}

	if (read(ori_fd, &bmp_header, sizeof(struct BmpHeader)) !=
			sizeof(struct BmpHeader)) {
			ERR("read bmp file error \n");
		return -1;
	}

	return ori_fd;
}

static void ori_bmp_close(int ori_fd)
{
	if (ori_fd > 0)
		close(ori_fd);
}

static int str_to_mode(char *str)
{
	static struct StrToMode {
		char *str;
		int mode;
	} str_mode[] = {
		{"RgbToRgb", RGB_TO_RGB},
		{"RgbToYuv", RGB_TO_YUV},
		{"YuvToYuv", YUV_TO_YUV},
		{"YuvToRgb", YUV_TO_RGB},
	};

	const int str_mode_size = sizeof(str_mode) / sizeof(str_mode[0]);
	for (int i = 0; i < str_mode_size; i++) {
		if (!strncasecmp(str, str_mode[i].str, strlen(str_mode[i].str)))
			return str_mode[i].mode;
	}

	return -1;
}

static int str_to_format(char *str)
{
	int i = 0;
	static struct StrToFormat table[] = {
		{"argb8888", MPP_FMT_ARGB_8888},
		{"abgr8888", MPP_FMT_ABGR_8888},
		{"rgba8888", MPP_FMT_RGBA_8888},
		{"bgra8888", MPP_FMT_BGRA_8888},
		{"xrgb8888", MPP_FMT_XRGB_8888},
		{"xbgr8888", MPP_FMT_XBGR_8888},
		{"rgbx8888", MPP_FMT_RGBX_8888},
		{"bgrx8888", MPP_FMT_BGRX_8888},

		{"argb4444", MPP_FMT_ARGB_4444},
		{"abgr4444", MPP_FMT_ABGR_4444},
		{"rgba4444", MPP_FMT_RGBA_4444},
		{"bgra4444", MPP_FMT_BGRA_4444},
		{"rgb565", MPP_FMT_RGB_565},
		{"bgr565", MPP_FMT_BGR_565},

		{"argb1555", MPP_FMT_ARGB_1555},
		{"abgr1555", MPP_FMT_ABGR_1555},
		{"rgba5551", MPP_FMT_RGBA_5551},
		{"bgra5551", MPP_FMT_BGRA_5551},
		{"rgb888", MPP_FMT_RGB_888},
		{"bgr888", MPP_FMT_BGR_888},

		{"yuv420", MPP_FMT_YUV420P},
		{"nv12", MPP_FMT_NV12},
		{"nv21", MPP_FMT_NV21},

		{"yuv422", MPP_FMT_YUV422P},
		{"nv16", MPP_FMT_NV16},
		{"nv61", MPP_FMT_NV61},

		{"yuyv", MPP_FMT_YUYV},
		{"yvyu", MPP_FMT_YVYU},
		{"uyvy", MPP_FMT_UYVY},
		{"vyuy", MPP_FMT_VYUY},

		{"yuv400", MPP_FMT_YUV400},
		{"yuv444", MPP_FMT_YUV444P},
	};

	format_table = &(*table);
	table_size = sizeof(table) / sizeof(table[0]);
	for (i = 0; i < table_size; i++) {
		if (!strncasecmp(str, table[i].str, strlen(table[i].str)))
			return table[i].format;
	}

	return -1;
}

static void Debug_format(int format)
{
	int i = 0;

	for(i = 0; i < table_size; i++) {
		if (format == format_table[i].format)
			printf("now format : %s\n", format_table[i].str);
	}
}

static int format_to_stride(int format, int width, int *stride)
{
	int plane = -1;

	switch (format) {
	case MPP_FMT_ARGB_8888:
	case MPP_FMT_ABGR_8888:
	case MPP_FMT_RGBA_8888:
	case MPP_FMT_BGRA_8888:
	case MPP_FMT_XRGB_8888:
	case MPP_FMT_XBGR_8888:
	case MPP_FMT_RGBX_8888:
	case MPP_FMT_BGRX_8888:
		plane = PLANE_1;
		stride[RGB_STRIDE] = BYTE_ALIGN((width * 4), 8);
		break;
	case MPP_FMT_ARGB_4444:
	case MPP_FMT_ABGR_4444:
	case MPP_FMT_RGBA_4444:
	case MPP_FMT_BGRA_4444:
	case MPP_FMT_RGB_565:
	case MPP_FMT_BGR_565:
		plane = PLANE_1;
		stride[RGB_STRIDE] = BYTE_ALIGN((width * 2), 8);
		break;
	case MPP_FMT_ARGB_1555:
	case MPP_FMT_ABGR_1555:
	case MPP_FMT_RGBA_5551:
	case MPP_FMT_BGRA_5551:
	case MPP_FMT_RGB_888:
	case MPP_FMT_BGR_888:
		plane = PLANE_1;
		stride[RGB_STRIDE] = BYTE_ALIGN((width * 3), 8);
		break;
	case MPP_FMT_YUV420P:
		plane = PLANE_3;
		stride[YUV_STRIDE_Y]  = BYTE_ALIGN((width), 8);
		stride[YUV_STRIDE_U_OR_V] = BYTE_ALIGN((width / 2), 8);
		break;
	case MPP_FMT_NV21:
	case MPP_FMT_NV12:
		plane = PLANE_2;
		stride[YUV_STRIDE_Y]  = BYTE_ALIGN((width), 8);
		stride[YUV_STRIDE_U_ADD_V] = BYTE_ALIGN((width), 8);
		break;
	case MPP_FMT_YUV422P:
		plane = PLANE_3;
		stride[YUV_STRIDE_Y]  = BYTE_ALIGN((width), 8);
		stride[YUV_STRIDE_U_OR_V] = BYTE_ALIGN((width / 2), 8);
		break;
	case MPP_FMT_NV16:
	case MPP_FMT_NV61:
		plane = PLANE_2;
		stride[YUV_STRIDE_Y]  = BYTE_ALIGN((width), 8);
		stride[YUV_STRIDE_U_ADD_V] = BYTE_ALIGN((width), 8);
		break;
	case MPP_FMT_YUYV:
	case MPP_FMT_YVYU:
	case MPP_FMT_UYVY:
	case MPP_FMT_VYUY:
		plane = PLANE_1;
		stride[YUV_STRIDE_YUV]  = BYTE_ALIGN((width * 2), 8);
		break;
	case MPP_FMT_YUV400:
		plane = PLANE_1;
		stride[YUV_STRIDE_Y]  = BYTE_ALIGN((width), 8);
		stride[YUV_STRIDE_U_OR_V] = 0;
		break;
	case MPP_FMT_YUV444P:
		plane = PLANE_3;
		stride[YUV_STRIDE_Y]  = BYTE_ALIGN((width), 8);
		stride[YUV_STRIDE_U_OR_V] = BYTE_ALIGN((width), 8);
		break;
	default:
		ERR("Set stride error, format = %d\n", format);
		break;
	}

	return plane;
}

static int format_to_height(int format, int height, int *plane_height)
{
	int plane = -1;

	if ((format >= MPP_FMT_ARGB_8888) && (format <= MPP_FMT_BGRA_4444)) {
		plane = PLANE_1;
		plane_height[RGB_HEIGHT] = height;
		return plane;
	}

	switch (format) {
	case MPP_FMT_YUV420P:
		plane = PLANE_3;
		plane_height[YUV_HEIGHT_Y]  = BYTE_ALIGN((height), 2);
		plane_height[YUV_HEIGHT_U_OR_V] = BYTE_ALIGN((height / 2), 2);
		break;
	case MPP_FMT_NV21:
	case MPP_FMT_NV12:
		plane = PLANE_2;
		plane_height[YUV_HEIGHT_Y]  = BYTE_ALIGN((height), 2);
		plane_height[YUV_HEIGHT_U_OR_V] = BYTE_ALIGN((height / 2), 2);
		break;
	case MPP_FMT_YUV422P:
		plane = PLANE_3;
		plane_height[YUV_HEIGHT_Y]  = BYTE_ALIGN(height, 2);
		plane_height[YUV_HEIGHT_U_OR_V] = BYTE_ALIGN(height, 2);
		break;
	case MPP_FMT_NV16:
	case MPP_FMT_NV61:
		plane = PLANE_2;
		plane_height[YUV_HEIGHT_Y]  = BYTE_ALIGN(height, 2);
		plane_height[YUV_HEIGHT_U_OR_V] = BYTE_ALIGN(height, 2);
		break;
	case MPP_FMT_YUYV:
	case MPP_FMT_YVYU:
	case MPP_FMT_UYVY:
	case MPP_FMT_VYUY:
		plane = PLANE_1;
		plane_height[YUV_HEIGHT_YUV]  = BYTE_ALIGN(height, 2);
		break;
	case MPP_FMT_YUV400:
		plane = PLANE_1;
		plane_height[YUV_HEIGHT_Y]  = BYTE_ALIGN((height), 2);
		plane_height[YUV_HEIGHT_U_OR_V] = 0;
		break;
	case MPP_FMT_YUV444P:
		plane = PLANE_3;
		plane_height[YUV_HEIGHT_Y]  = BYTE_ALIGN(height, 2);
		plane_height[YUV_HEIGHT_U_OR_V] = BYTE_ALIGN(height, 2);
		break;
	default:
		ERR("Set height error, format = %d\n", format);
		break;
	}

	return plane;
}

static int format_to_bmp_data_length(int format, int width, int *data_length)
{
	int plane = -1;

	switch (format) {
	case MPP_FMT_ARGB_8888:
	case MPP_FMT_ABGR_8888:
	case MPP_FMT_RGBA_8888:
	case MPP_FMT_BGRA_8888:
	case MPP_FMT_XRGB_8888:
	case MPP_FMT_XBGR_8888:
	case MPP_FMT_RGBX_8888:
	case MPP_FMT_BGRX_8888:
		plane = PLANE_1;
		data_length[RGB_LENGTH] = BYTE_ALIGN((width * 4), 4);
		break;
	case MPP_FMT_ARGB_4444:
	case MPP_FMT_ABGR_4444:
	case MPP_FMT_RGBA_4444:
	case MPP_FMT_BGRA_4444:
	case MPP_FMT_RGB_565:
	case MPP_FMT_BGR_565:
		plane = PLANE_1;
		data_length[RGB_LENGTH] = BYTE_ALIGN(width * 2, 4);
		break;
	case MPP_FMT_ARGB_1555:
	case MPP_FMT_ABGR_1555:
	case MPP_FMT_RGBA_5551:
	case MPP_FMT_BGRA_5551:
	case MPP_FMT_RGB_888:
	case MPP_FMT_BGR_888:
		plane = PLANE_1;
		data_length[RGB_LENGTH] = BYTE_ALIGN((width * 3), 4);
		break;
	default:
		ERR("Set data_length error, format = %d\n", format);
		break;
	}

	return plane;
}

/* apply dmabuf according to scale */
static int dmabuf_request_one(int fd, int format, int *dma_fd, float scale)
{
	int i = 0;
	int plane = 0;
	int input_width = 0;
	int input_height = 0;
	int stride[2] = {0};
	int height[2] = {0};
	struct dma_heap_allocation_data data = {0};

	input_width = (int)(abs(bmp_header.width) * scale);
	input_height = (int)(abs(bmp_header.height) * scale);
	plane = format_to_stride(format, input_width, stride);
	if (plane < 0) {
		ERR("format_to_stride failed!\n");
		return -1;
	}

	plane = format_to_height(format, input_height, height);
	if (plane < 0) {
		ERR("format_to_height failed!\n");
		return -1;
	}

	/* apply for format according to the format */
	for (i = 0; i < plane; i++)
	{
		if ((plane == PLANE_3) && i == PLANE_3 - 1)
			data.len = stride[i - 1] * height[i - 1];
		else
			data.len = stride[i] * height[i];

		data.fd = 0;
		data.fd_flags = O_RDWR | O_CLOEXEC;
		data.heap_flags = 0;

		if (ioctl(fd, DMA_HEAP_IOCTL_ALLOC, &data) < 0) {
			ERR("ioctl() failed! errno: %d[%s]\n", errno, strerror(errno));
			return -1;
		}

		dma_fd[i] = data.fd;
	}

	return 0;
}

static int draw_ori_dmabuf(int *dmabuf_fd, int ori_fd, int format, float scale)
{
	int i = 0;
	int j = 0;
	int plane = 0;
	int len = 0;
	int line_length = 0;
	int data_length_now = 0;
	int input_width = 0;
	int input_height = 0;
	int stride[2] = {0};
	int height[2] = {0};
	int data_length[2] = {0};
	unsigned char *dmabuf = NULL;
	unsigned char *buf = NULL;

	if (bmp_header.type != 0x4D42) {
		ERR("Not a Bmp file\n");
		return -1;
	}

	input_width = (int)(abs(bmp_header.width) * scale);
	input_height = (int)(abs(bmp_header.height) * scale);
	plane = format_to_stride(format, input_width, stride);
	if (plane < 0) {
		ERR("format_to_stride failed!\n");
		return -1;
	}

	plane = format_to_height(format, input_height, height);
	if (plane < 0) {
		ERR("format_to_height failed!\n");
		return -1;
	}

	plane = format_to_bmp_data_length(format, input_width, data_length);
	if (plane < 0) {
		ERR("format_to_bmp_data_length failed!\n");
		return -1;
	}

	for (i = 0; i < plane; i++)
	{
		if ((plane == PLANE_3) && i == PLANE_3 - 1) {
			len = stride[i - 1] * height[i - 1];
			line_length = stride[i - 1];
			data_length_now = data_length[i - 1];
		} else {
			len = stride[i] * height[i];
			line_length = stride[i];
			data_length_now = data_length[i];
		}

		dmabuf = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, dmabuf_fd[i], 0);
		if (dmabuf == MAP_FAILED) {
			ERR("mmap() failed! errno: %d[%s]\n", errno, strerror(errno));
			return -1;
		}

		/* read from bottom to top */
		if (bmp_header.height < 0){
			buf = dmabuf;
			for (j = 0; j < input_height; j++) {
				if ((read(ori_fd, buf, data_length_now) != data_length_now)) {
					ERR("read(%d) . errno: %d[%s]\n", data_length_now,
						errno, strerror(errno));
					return -1;
				}
				buf += line_length;
			}
		/* read from top to bottom */
		} else {
			for (j = input_height -1; j >= 0; j--) {
				buf = dmabuf + (input_height + j) * line_length;
				if ((read(ori_fd, buf, data_length_now) != data_length_now)) {
					ERR("read(%d) . errno: %d[%s]\n", data_length_now,
						errno, strerror(errno));
					return -1;
				}
			}
		}

		munmap(dmabuf, len);
	}

	return 0;
}

static void sent_ge_dma_fd(struct mpp_ge *ge, int *dma_fd)
{
	int i = 0;

	for (i = 0; i < 3; i++)
	{
		if (dma_fd[i] > 0)
			mpp_ge_add_dmabuf(ge, dma_fd[i]);
	}
}

static void rm_ge_dma_fd(struct mpp_ge *ge, int *dma_fd)
{
	int i = 0;

	for (i = 0; i < 3; i++)
	{
		if (dma_fd[i] > 0)
			mpp_ge_rm_dmabuf(ge, dma_fd[i]);
	}
}

static void close_dma_fd(int *dma_fd)
{
	int i = 0;

	for (i = 0; i < 3; i++)
	{
		if (dma_fd[i] > 0) {
			close(dma_fd[i]);
			dma_fd[i] = -1;
		}
	}
}

static int ge_bitlet_set(struct ge_bitblt *blt, const int *src_dma_fd, const int *dst_dma_fd,
			 int src_format, int dst_format, float scale)
{
	int i = 0;
	int plane = 0;
	int stride[2] = {0};
	int input_width = 0;
	int input_height = 0;

	memset(blt, 0, sizeof(struct ge_bitblt));

	/* src layer settings*/
	input_width = (int)(abs(bmp_header.width) * 1);
	input_height = (int)(abs(bmp_header.height) * 1);
	plane = format_to_stride(src_format, input_width, stride);
	if (plane < 0) {
		ERR("format_to_stride failed!\n");
		return -1;
	}

	for (i = 0; i < plane; i++)
	{
		blt->src_buf.fd[i] = src_dma_fd[i];
		if ((plane == PLANE_3) && (i == PLANE_3 - 1)) {
			continue;
		}
		blt->src_buf.stride[i] = stride[i];
	}

	blt->src_buf.buf_type = MPP_DMA_BUF_FD;
	blt->src_buf.size.width = input_width;
	blt->src_buf.size.height = input_height;
	blt->src_buf.format = src_format;
	blt->src_buf.crop_en = 0;
	blt->src_buf.flags = 0;

	/* dst layer settings */
	input_width = (int)(abs(bmp_header.width) * scale);
	input_height = (int)(abs(bmp_header.height) * scale);
	plane = format_to_stride(dst_format, input_width, stride);
	if (plane < 0) {
		ERR("format_to_stride failed!\n");
		return -1;
	}

	for (i = 0; i < plane; i++)
	{
		blt->dst_buf.fd[i] = dst_dma_fd[i];
		if ((plane == PLANE_3) && (i == PLANE_3 - 1))
			continue;
		blt->dst_buf.stride[i] = stride[i];
	}

	blt->dst_buf.buf_type = MPP_DMA_BUF_FD;
	blt->dst_buf.size.width = input_width;
	blt->dst_buf.size.height = input_height;
	blt->dst_buf.format = dst_format;
	blt->dst_buf.flags = 0;

	/* supports scale from 1/16 to 16 */
	if (scale != 1)
	{
		if ((scale > 16) || (scale < (1/16)))
		{
			ERR("scale invalid: %f\n", scale);
			return -1;
		}

		blt->dst_buf.crop_en = 1;
		blt->dst_buf.crop.x = 0;
		blt->dst_buf.crop.y = 0;
		blt->dst_buf.crop.width = input_width;
		blt->dst_buf.crop.height = input_height;
	} else {
		blt->dst_buf.crop_en = 0;
	}

	return 0;
}

static int ge_bitlet(struct mpp_ge *ge, struct ge_bitblt *blt, const int *src_dma_fd,
		     const int *dst_dma_fd, int src_format, int dst_format, float scale)
{
	int ret = -1;

	ret = ge_bitlet_set(blt, src_dma_fd, dst_dma_fd, src_format, dst_format, scale);
	if (ret < 0) {
		ERR("ge_bitlet set task failed: %d\n", ret);
		return ret;
	}

	ret = mpp_ge_bitblt(ge, blt);
	if (ret) {
		ERR("bitblt task failed: %d\n", ret);
		return ret;
	}

	ret = mpp_ge_emit(ge);
	if (ret) {
		ERR("emit task failed: %d\n", ret);
		return ret;
	}

	ret = mpp_ge_sync(ge);
	if (ret) {
		ERR("sync task failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static int ge_display_set(struct ge_bitblt *blt, const int *dma_fd, int format, int region, float scale)
{
	int i = 0;
	int plane = -1;
	int input_width = 0;
	int input_height = 0;
	int stride[2] = {0};
	int dstx = 0;
	int dsty = 0;

	memset(blt, 0, sizeof(struct ge_bitblt));

	if (region == SRC_DISP_REGION) {
		input_width = (int)(abs(bmp_header.width) * 1);
		input_height = (int)(abs(bmp_header.height) * 1);
	} else {
		input_width = (int)(abs(bmp_header.width) * scale);
		input_height = (int)(abs(bmp_header.height) * scale);
	}

	/* src layer settings*/
	plane = format_to_stride(format, input_width, stride);
	if (plane < 0) {
		ERR("format_to_stride failed!\n");
		return -1;
	}

	for (i = 0; i < plane ; i++)
	{
		blt->src_buf.fd[i] = dma_fd[i];
		if ((plane == PLANE_3) && i == PLANE_3 - 1)
			blt->src_buf.stride[i - 1] = stride[i - 1];
		else
			blt->src_buf.stride[i] = stride[i];
	}

	blt->src_buf.buf_type = MPP_DMA_BUF_FD;
	blt->src_buf.size.width = input_width;
	blt->src_buf.size.height = input_height;
	blt->src_buf.format = format;
	blt->src_buf.crop_en = 0;

	/* dst layer settings */
	blt->dst_buf.buf_type = MPP_PHY_ADDR;
	blt->dst_buf.phy_addr[0] = g_fb_phy;
	blt->dst_buf.stride[0] = g_fb_stride;
	blt->dst_buf.size.width = g_screen_w;
	blt->dst_buf.size.height = g_screen_h;
	blt->dst_buf.format = g_fb_format;

	if (region == SRC_DISP_REGION) {
		dstx = 0;
		dsty = 0;

	} else {
		dstx = input_width / scale;
		dsty = 0;

		if (input_width > g_screen_w - dstx)
			input_width = g_screen_w - dstx;
		if (input_height > g_screen_h)
			input_height = g_screen_h;
	}

	blt->dst_buf.crop_en = 1;
	blt->dst_buf.crop.x = dstx;
	blt->dst_buf.crop.y = dsty;
	blt->dst_buf.crop.width = input_width;
	blt->dst_buf.crop.height = input_height;

	return 0;
}

static int  ge_display(struct mpp_ge *ge, struct ge_bitblt *blt, int *dma_fd,
		       int format, int region, float scale)
{
	int ret = -1;

	ret = ge_display_set(blt, dma_fd, format, region, scale);
	if (ret < 0) {
		ERR("set bitblt task failed: %d\n", ret);
		return ret;
	}

	ret = mpp_ge_bitblt(ge, blt);
	if (ret) {
		ERR("bitblt task failed: %d\n", ret);
		return ret;
	}

	ret = mpp_ge_emit(ge);
	if (ret) {
		ERR("emit task failed: %d\n", ret);
		return ret;
	}

	ret = mpp_ge_sync(ge);
	if (ret) {
		ERR("sync task failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static int ori_to_src(struct mpp_ge *ge, struct ge_bitblt *blt, int *ori_dma_fd,
		      int *src_dma_fd, int src_format, float scale)
{
	int ret = -1;

	ret = ge_bitlet(ge, blt, ori_dma_fd, src_dma_fd, MPP_FMT_ARGB_8888, src_format, scale);
	if (ret < 0) {
		ERR("ge_bitlet task failed:%d\n", ret);
		return ret;
	}
	return 0;
}

static int request_dmabuf_again(struct mpp_ge *ge, int fd, int format, int *dma_fd, float scale)
{
	int ret = 0;

	/* reapply for memory */
	rm_ge_dma_fd(ge, dma_fd);
	close_dma_fd(dma_fd);

	ret = dmabuf_request_one(fd, format, dma_fd, scale);
	if (ret < 0) {
		ERR("dma_fd failed to request dmabuf\n");
		return -1;
	}

	sent_ge_dma_fd(ge, dma_fd);

	return 0;
}

/* full format conversion */
static int format_conversion(struct mpp_ge *ge, struct ge_bitblt *blt,
			     int *ori_dma_fd, int *src_dma_fd, int *dst_dma_fd,
			     int heap_fd, float scale, int mode)
{
	int i = 0;
	int j = 0;
	int ret = 0;
	char *str_src_format = NULL;
	char *str_dst_format = NULL;
	int src_format = 0;
	int dst_format = 0;
	int src_select_region_left = 0;
	int dst_select_region_left = 0;
	int src_select_region_right = 0;
	int dst_select_region_right = 0;

	/* Set region according to mode */
	switch (mode)
	{
	case RGB_TO_RGB:
		str_src_format = "argb8888";
		str_dst_format = "argb8888";

		src_select_region_left = 0;
	 	dst_select_region_left = 0;
		src_select_region_right = 20;
		dst_select_region_right = 20;
		break;
	case RGB_TO_YUV:
		str_src_format = "argb8888";
		str_dst_format = "yuv420";

		src_select_region_left = 0;
	 	dst_select_region_left = 20;
		src_select_region_right = 20;
		dst_select_region_right = 32;
		break;
	case YUV_TO_YUV:
		str_src_format = "yuv420";
		str_dst_format = "yuv420";

		src_select_region_left = 20;
	 	dst_select_region_left = 20;
		src_select_region_right = 32;
		dst_select_region_right = 32;
		break;
	case YUV_TO_RGB:
		str_src_format = "yuv420";
		str_dst_format = "argb8888";

		src_select_region_left = 20;
	 	dst_select_region_left = 0;
		src_select_region_right = 32;
		dst_select_region_right = 20;
		break;
	default:
		ERR("mode invalid, mode = %d\n",mode);
		break;
	}

	src_format = str_to_format(str_src_format);
	if (src_format < 0) {
		ERR("str_to_format invalid\n");
		return -1;
	}

	dst_format = str_to_format(str_dst_format);
	if (dst_format < 0) {
		ERR("dst_format invalid\n");
		return -1;
	}

	for (i = src_select_region_left; i < src_select_region_right; i++) {
		src_format = format_table[i].format;
		printf("\r-----------------------------------------\n");
		printf("\r--Src ");
		Debug_format(src_format);

		ret = request_dmabuf_again(ge, heap_fd, src_format, src_dma_fd, 1);
		if (ret < 0) {
			ERR("request_dmabuf_again execusion error \n");
			return -1;
		}
		/* convert the original image format to input format */
		ret = ori_to_src(ge, blt, ori_dma_fd, src_dma_fd, src_format, 1);
		if (ret) {
			ERR("ori_to_src task failed: %d\n", ret);
			return -1;
		}

		ret = ge_display(ge, blt, src_dma_fd, src_format, SRC_DISP_REGION, scale);
		if (ret < 0) {
			ERR("ge_display task failed:%d\n", ret);
			return -1;
		}

		sleep(1);
		for (j = dst_select_region_left; j < dst_select_region_right; j++) {
			dst_format = format_table[j].format;

			printf("\rDst ");
			Debug_format(dst_format);

			ret = request_dmabuf_again(ge, heap_fd, dst_format, dst_dma_fd, scale);
			if (ret < 0) {
				ERR("request_dmabuf_again execusion error \n");
				return -1;
			}

			ret = ge_bitlet(ge, blt, src_dma_fd, dst_dma_fd, src_format, dst_format, scale);
			if (ret < 0) {
				ERR("ge_bitlet task failed\n");
				return -1;
			}

			ret = ge_display(ge, blt, dst_dma_fd, dst_format, DST_DISP_REGION, scale);
			if (ret < 0) {
				ERR("ge_display task failed\n");
				return -1;
			}
		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	int ret = 0;
	int mode = RGB_TO_RGB;
	int ori_fd = -1;
	int ori_format = 0;
	int heap_fd = -1;
	float scale = 1;
	int ori_dmabuf_fd[3] = {-1};
	int src_dmabuf_fd[3] = {-1};
	int dst_dmabuf_fd[3] = {-1};
	struct mpp_ge *ge = NULL;
	struct ge_bitblt blt = {0};

	/* parameter supports settings */
	const char sopts[] = "uhm:s:";
	const struct option lopts[] = {
		{"usage",	no_argument,       NULL, 'u'},
		{"help",	no_argument,       NULL, 'h'},
		{"mode",	required_argument, NULL, 'm'},
		{"scale",	required_argument, NULL, 's'},
	};

	while ((ret = getopt_long(argc, argv, sopts, lopts, NULL)) != -1) {
		switch (ret) {
		case 'm':
			mode = str_to_mode(optarg);
			if (mode < 0) {
				printf("mode set error, please set against\n");
				goto EXIT;
			}
			break;
		case 's':
			scale = atof(optarg);
			if ((scale > 16) || (scale < (1/16))) {
				printf("scale invalid, please set against\n");
				goto EXIT;
			}
			break;
		case 'u':
			usage(argv[0]);
			goto EXIT;
		case 'h':
			help();
			goto EXIT;
		default:
			ERR("Invalid parameter: %#x\n", ret);
			goto EXIT;
		}
	}

	ge = mpp_ge_open();
	if (!ge) {
		ERR("open ge device\n");
		goto EXIT;
	}

	if (fb_open()) {
		ERR("fb_open error\n");
		goto EXIT;
	}

	heap_fd = open(DMA_HEAP_DEV, O_RDWR);
	if (heap_fd < 0) {
		ERR("failed to open %s, errno: %d[%s]\n",
			DMA_HEAP_DEV, errno, strerror(errno));
		goto EXIT;
	}

	ori_fd = ori_bmp_open(SINGER_32BIT_IMAGE);
	if (ori_fd < 0) {
		ERR("ori_bmp_open error\n");
		goto EXIT;
	}

	ori_format = str_to_format("argb8888");
	if (ori_format < 0) {
		ERR("str_to_format invalid\n");
		goto EXIT;
	}

	ret = dmabuf_request_one(heap_fd, ori_format, ori_dmabuf_fd, 1);
	if (ret < 0) {
		ERR("ori_dmabuf_fd failed to request dmabuf\n");
		goto EXIT;
	}

	ret = draw_ori_dmabuf(ori_dmabuf_fd, ori_fd, ori_format, 1);
	if (ret < 0) {
		ERR("draw_ori_dmabuf task failed\n");
		goto EXIT;
	}

	sent_ge_dma_fd(ge, ori_dmabuf_fd);
	ret = mpp_ge_emit(ge);
	if (ret) {
		ERR("emit task failed:%d\n", ret);
		goto EXIT;
	}

	ret = format_conversion(ge, &blt, ori_dmabuf_fd, src_dmabuf_fd, dst_dmabuf_fd, heap_fd, scale, mode);
	if (ret < 0) {
		ERR("format_conversion task failed:%d\n", ret);
		goto EXIT;
	}

EXIT:
	rm_ge_dma_fd(ge, ori_dmabuf_fd);
	close_dma_fd(ori_dmabuf_fd);

	rm_ge_dma_fd(ge, src_dmabuf_fd);
	close_dma_fd(src_dmabuf_fd);

	rm_ge_dma_fd(ge, dst_dmabuf_fd);
	close_dma_fd(dst_dmabuf_fd);

	if (heap_fd > 0)
		close(heap_fd);

	if (ori_fd > 0)
		ori_bmp_close(ori_fd);

	if (ge)
		mpp_ge_close(ge);

	fb_close();

	return 0;
}
