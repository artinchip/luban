/*
 * Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <qi.xu@artinchip.com>
 *  Desc: ve decode + ge blit + de
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <getopt.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <video/artinchip_fb.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>

#include "mpp_mem.h"
#include "mpp_decoder.h"
#include "mpp_log.h"
#include "mpp_ge.h"

#define FB_DEV		"/dev/fb0"

static int g_screen_w = 0;
static int g_screen_h = 0;
static int g_fb_fd = 0;
static int g_fb_len = 0;
static int g_fb_stride = 0;
static unsigned int g_fb_format = 0;
static unsigned int g_fb_phy = 0;
unsigned char *g_fb_buf = NULL;

#define MAX_FILES_NUM           (128)

struct context {
	char file_input[MAX_FILES_NUM][1024];	// test file name
	int file_num;				// test file number
};

static void print_help(char* prog)
{
	printf("%s\n", prog);
	printf("Compile time: %s\n", __TIME__);
	printf("Usage:\n\n"
		"Options:\n"
		"\t-t                             directory of test files\n"
		"\t-l                             loop time\n"
		"\t-h                             help\n\n"
		"End:\n");
}

static int get_file_size(FILE *fp)
{
	fseek(fp, 0, SEEK_END);
	int len = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	logi("file length: %d", len);

	return len;
}

static int fb_open(void)
{
	struct fb_fix_screeninfo fix;
	struct fb_var_screeninfo var;
	struct aicfb_layer_data layer;

	g_fb_fd = open(FB_DEV, O_RDWR);
	if (g_fb_fd == -1) {
		loge("open %s", FB_DEV);
		return -1;
	}

	if (ioctl(g_fb_fd, FBIOGET_FSCREENINFO, &fix) < 0) {
		loge("ioctl FBIOGET_FSCREENINFO");
		close(g_fb_fd);
		return -1;
	}

	if (ioctl(g_fb_fd, FBIOGET_VSCREENINFO, &var) < 0) {
		loge("ioctl FBIOGET_VSCREENINFO");
		close(g_fb_fd);
		return -1;
	}

	if(ioctl(g_fb_fd, AICFB_GET_FB_LAYER_CONFIG, &layer) < 0) {
		loge("ioctl FBIOGET_VSCREENINFO");
		close(g_fb_fd);
		return -1;
	}

	g_screen_w = var.xres;
	g_screen_h = var.yres;
	g_fb_len = fix.smem_len;
	g_fb_phy = fix.smem_start;
	g_fb_stride = fix.line_length;
	g_fb_format = layer.buf.format;

	logd("screen_w = %d, screen_h = %d, stride = %d, format = %d\n",
			var.xres, var.yres, g_fb_stride, g_fb_format);
	return 0;
}

static void fb_close(void)
{
	if (g_fb_fd > 0)
		close(g_fb_fd);
}

static int render_frame(struct mpp_frame *frame)
{
	int ret = 0;
	struct ge_bitblt blt = {0};
	struct mpp_ge *ge = mpp_ge_open();
	if (!ge) {
		loge("ge open fail\n");
		return -1;
	}

	fb_open();

	memset(&blt, 0, sizeof(struct ge_bitblt));
	/* source buffer */
	memcpy(&blt.src_buf, &frame->buf, sizeof(struct mpp_frame));

	/* dstination buffer */
	blt.dst_buf.buf_type = MPP_PHY_ADDR;
	blt.dst_buf.phy_addr[0] = g_fb_phy;
	blt.dst_buf.stride[0] = g_fb_stride;
	blt.dst_buf.size.width = g_screen_w;
	blt.dst_buf.size.height = g_screen_h;
	blt.dst_buf.format = g_fb_format;

	blt.ctrl.flags = MPP_FLIP_H;

	blt.dst_buf.crop_en = 1;
	blt.dst_buf.crop.x = 0;
	blt.dst_buf.crop.y = 0;
	blt.dst_buf.crop.width = frame->buf.size.width;
	blt.dst_buf.crop.height = frame->buf.size.height;

	ret =  mpp_ge_bitblt(ge, &blt);
	if (ret < 0) {
		loge("ge bitblt fail\n");
	}

	ret = mpp_ge_emit(ge);
	if (ret < 0) {
		loge("ge emit fail\n");
	}

	ret = mpp_ge_sync(ge);
	if (ret < 0) {
		loge("ge sync fail\n");
	}

	fb_close();
	if (ge)
		mpp_ge_close(ge);

	return 0;
}

static int read_dir(char* path, struct context* ctx)
{
	char* ptr = NULL;
	struct dirent* dir_file;
	DIR* dir = opendir(path);
	if(dir == NULL) {
		loge("read dir failed");
		return -1;
	}

	while((dir_file = readdir(dir))) {
		if(strcmp(dir_file->d_name, ".") == 0 || strcmp(dir_file->d_name, "..") == 0)
			continue;

		ptr = strrchr(dir_file->d_name, '.');
		if(ptr == NULL)
			continue;

		if (strcmp(ptr, ".h264") && strcmp(ptr, ".264") && strcmp(ptr, ".png") && strcmp(ptr, ".jpg"))
			continue;

		logi("name: %s", dir_file->d_name);
		strcpy(ctx->file_input[ctx->file_num], path);
		strcat(ctx->file_input[ctx->file_num], dir_file->d_name);
		logi("i: %d, filename: %s", ctx->file_num, ctx->file_input[ctx->file_num]);
		ctx->file_num ++;

		if(ctx->file_num >= MAX_FILES_NUM)
			break;
	}

	return 0;
}

int main(int argc, char **argv)
{
	int ret = 0;
	int i;
	int opt;
	int cnt = 0;
	int file_len = 0;
	FILE* input_fp = 0;
	int loop_time = 1;
	char* ptr = NULL;
	char* pic_path;
	int type = MPP_CODEC_VIDEO_DECODER_MJPEG;
	struct context ctx;
	memset(&ctx, 0, sizeof(struct context));

	while (1) {
		opt = getopt(argc, argv, "t:l:h");
		if (opt == -1) {
			break;
		}
		switch (opt) {
		case 't':
			read_dir(optarg, &ctx);
			break;
		case 'l':
			loop_time = atoi(optarg);
			break;
		case 'h':
		default:
			print_help(argv[0]);
			return -1;
		}
	}

	if(ctx.file_num == 0) {
		print_help(argv[0]);
		return -1;
	}

	for(cnt=0; cnt<loop_time; cnt++) {
		logi("cnt: %d", cnt);

		for(i=0; i<ctx.file_num; i++) {
			pic_path = ctx.file_input[i];
			logi("file name: %s", pic_path);
			ptr = strrchr(pic_path, '.');

			if (!strcmp(ptr, ".jpg")) {
				type = MPP_CODEC_VIDEO_DECODER_MJPEG;
			} else if (!strcmp(ptr, ".png")) {
				type = MPP_CODEC_VIDEO_DECODER_PNG;
			} else if (!strcmp(ptr, ".264")) {
				type = MPP_CODEC_VIDEO_DECODER_H264;
			}
			logd("decode type: 0x%02X", type);

			input_fp = fopen(pic_path, "rb");
			if(input_fp == NULL) {
				loge("open file failed");
				return -1;
			}

			file_len = get_file_size(input_fp);

			//* 1. create mpp_decoder
			struct mpp_decoder* dec = mpp_decoder_create(type);

			struct decode_config config;
			config.bitstream_buffer_size = (file_len + 1023) & (~1023);
			config.extra_frame_num = 0;
			config.packet_count = 1;
			config.pix_fmt = MPP_FMT_ABGR_8888;

			//* 2. init mpp_decoder
			mpp_decoder_init(dec, &config);

			//* 3. get an empty packet from mpp_decoder
			struct mpp_packet packet;
			memset(&packet, 0, sizeof(struct mpp_packet));
			mpp_decoder_get_packet(dec, &packet, file_len);

			//* 4. copy data to packet
			int r_len = fread(packet.data, 1, file_len, input_fp);
			packet.size = r_len;
			packet.flag = PACKET_FLAG_EOS;
			logi("read len: %d", r_len);

			//* 5. put the packet to mpp_decoder
			mpp_decoder_put_packet(dec, &packet);

			//* 6. decode
			//time_start(mpp_decoder_decode);
			ret = mpp_decoder_decode(dec);
			if(ret < 0) {
				loge("decode error");
				mpp_decoder_destory(dec);
			}

			//time_end(mpp_decoder_decode);

			//* 7. get a decoded frame
			struct mpp_frame frame;
			memset(&frame, 0, sizeof(struct mpp_frame));
			mpp_decoder_get_frame(dec, &frame);

			//* 8. render data
			render_frame(&frame);

			//* 9. return this frame
			mpp_decoder_put_frame(dec, &frame);

			//* 10. destroy mpp_decoder
			mpp_decoder_destory(dec);

			if(input_fp)
				fclose(input_fp);

			usleep(30000);
		}
	}
	return 0;
}
