/*
 * Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <qi.xu@artinchip.com>
 *  Desc: jpeg/png decode demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <pthread.h>
#include <errno.h>
#include <dirent.h>

#include <linux/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#include <video/artinchip_fb.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>

#include "mpp_decoder.h"
#include "mpp_log.h"

#define MAX_TEST_FILE 200
struct dec_ctx {
	struct mpp_decoder  *decoder;

	char file_input[MAX_TEST_FILE][1024];	// test file name
	int file_num;				// test file number
};

static void print_help(void)
{
	printf("Usage: pic_animation [OPTIONS] [SLICES PATH]\n\n"
		"Options:\n"
		" -d                             directory of input png files\n"
		" -h                             help\n\n"
		"End:\n");
}

static int get_file_size(FILE* fp)
{
	int len = 0;
	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	return len;
}

static int set_fb_layer_alpha(int fb0_fd, int val)
{
	int ret = 0;
	struct aicfb_alpha_config alpha = {0};

	if (fb0_fd < 0)
		return -1;

	alpha.layer_id = 1;
	alpha.enable = 1;
	alpha.mode = 1;
	alpha.value = val;
	ret = ioctl(fb0_fd, AICFB_UPDATE_ALPHA_CONFIG, &alpha);
	if (ret < 0)
		loge("fb ioctl() AICFB_UPDATE_ALPHA_CONFIG failed!");

	return ret;
}

static void video_layer_set(int fb0_fd, struct mpp_buf *picture_buf)
{
	struct aicfb_layer_data layer = {0};
	int dmabuf_num = 0;
	struct dma_buf_info dmabuf_fd[3];
	int i;

	if (fb0_fd < 0)
		return;

	layer.layer_id = 0;
	layer.enable = 1;
	layer.scale_size.width = picture_buf->size.width;
	layer.scale_size.height= picture_buf->size.height;

	layer.pos.x = 0;
	layer.pos.y = 0;
	layer.buf = *picture_buf;

	if (picture_buf->format == MPP_FMT_ARGB_8888) {
		dmabuf_num = 1;
	} else if (picture_buf->format == MPP_FMT_RGBA_8888) {
		dmabuf_num = 1;
	} else if (picture_buf->format == MPP_FMT_RGB_888) {
		dmabuf_num = 1;
	} else if (picture_buf->format == MPP_FMT_YUV420P) {
		dmabuf_num = 3;
	} else if (picture_buf->format == MPP_FMT_YUV444P) {
		dmabuf_num = 3;
	} else if (picture_buf->format == MPP_FMT_YUV422P) {
		dmabuf_num = 3;
	} else if (picture_buf->format == MPP_FMT_YUV400) {
		dmabuf_num = 1;
	} else {
		loge("no support picture foramt %d, default argb8888", picture_buf->format);
	}

	//* add dmabuf to de driver
	for(i=0; i<dmabuf_num; i++) {
		dmabuf_fd[i].fd = picture_buf->fd[i];
		if (ioctl(fb0_fd, AICFB_ADD_DMABUF, &dmabuf_fd[i]) < 0)
			loge("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");
	}

	//* update layer config (it is async interface)
	if (ioctl(fb0_fd, AICFB_UPDATE_LAYER_CONFIG, &layer) < 0)
		loge("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");

	//* wait vsync (wait layer config)
	ioctl(fb0_fd, AICFB_WAIT_FOR_VSYNC, NULL);

	//* remove dmabuf to de driver
	for(i=0; i<dmabuf_num; i++) {
		if (ioctl(fb0_fd, AICFB_RM_DMABUF, &dmabuf_fd[i]) < 0)
			loge("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");
	}
}

static int render_frame(struct mpp_frame* frame, int fb0_fd)
{
	struct fb_fix_screeninfo finfo;

	if (ioctl(fb0_fd, FBIOGET_FSCREENINFO, &finfo) == -1) {
		loge("read fixed information failed!");
		close(fb0_fd);
		return -1;
	}

	set_fb_layer_alpha(fb0_fd, 0);
	video_layer_set(fb0_fd, &frame->buf);

	return 0;
}

static int read_dir(char* path, struct dec_ctx* dec_data)
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

		if (strcmp(ptr, ".png") && strcmp(ptr, ".jpg"))
			continue;

		logi("name: %s", dir_file->d_name);
		logi("path: %s", path);
		strcpy(dec_data->file_input[dec_data->file_num], path);
		logi("strcpy");
		usleep(20);
		strcat(dec_data->file_input[dec_data->file_num], dir_file->d_name);
		usleep(20);
		logi("i: %d, filename: %s", dec_data->file_num, dec_data->file_input[dec_data->file_num]);
		dec_data->file_num ++;

		if(dec_data->file_num >= MAX_TEST_FILE)
			break;
	}

	return 0;
}

static long long get_now_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000ll + tv.tv_usec;
}

int main(int argc, char **argv)
{
	int ret = 0;
	int i;
	int opt;
	int dec_type;
	int file_len;
	int fb0_fd;
	FILE* fp = NULL;
	struct dec_ctx *dec_data = NULL;
	dec_data = malloc(sizeof(struct dec_ctx));
	memset(dec_data, 0, sizeof(struct dec_ctx));

	if (argc < 2) {
		print_help();
		goto out;
	}

	while (1) {
		opt = getopt(argc, argv, "d:h");
		if (opt == -1) {
			break;
		}
		switch (opt) {
		case 'd':
			read_dir(optarg, dec_data);

			break;
		case 'h':
		default:
			print_help();
			goto out;
		}
	}

	fb0_fd = open("/dev/fb0", O_RDWR);
	if (fb0_fd < 0) {
		logw("open fb0 failed!");
		goto out;
	}

	long long time_start = get_now_us();
	for(i=0; i<dec_data->file_num; i++) {
		char* ptr = strrchr(dec_data->file_input[i], '.');
		if (!strcmp(ptr, ".jpg")) {
			dec_type = MPP_CODEC_VIDEO_DECODER_MJPEG;
		} else if (!strcmp(ptr, ".png")) {
			dec_type = MPP_CODEC_VIDEO_DECODER_PNG;
		}

		fp = fopen(dec_data->file_input[i], "rb");
		if (fp == NULL) {
			loge("open file failed");
			goto out;
		}
		file_len = get_file_size(fp);

		//* 1. create mpp_decoder
		struct mpp_decoder* dec = mpp_decoder_create(dec_type);

		struct decode_config config;
		config.bitstream_buffer_size = 512*1024;
		config.extra_frame_num = 0;
		config.packet_count = 1;

		// JPEG not supprt YUV2RGB
		if(dec_type == MPP_CODEC_VIDEO_DECODER_MJPEG)
			config.pix_fmt = MPP_FMT_YUV420P;
		else if(dec_type == MPP_CODEC_VIDEO_DECODER_PNG)
			config.pix_fmt = MPP_FMT_ARGB_8888;

		//* 2. init mpp_decoder
		mpp_decoder_init(dec, &config);

		//* 3. get an empty packet from mpp_decoder
		struct mpp_packet packet;
		memset(&packet, 0, sizeof(struct mpp_packet));
		mpp_decoder_get_packet(dec, &packet, file_len);

		//* 4. copy data to packet
		fread(packet.data, 1, file_len, fp);
		packet.size = file_len;
		packet.flag = PACKET_FLAG_EOS;

		//* 5. put the packet to mpp_decoder
		mpp_decoder_put_packet(dec, &packet);

		//* 6. decode
		ret = mpp_decoder_decode(dec);
		if(ret < 0) {
			loge("decode error");
			goto out;
		}

		//* 7. get a decoded frame
		struct mpp_frame frame;
		memset(&frame, 0, sizeof(struct mpp_frame));
		mpp_decoder_get_frame(dec, &frame);

		//* 8. render this frame
		render_frame(&frame, fb0_fd);

		//* 9. return this frame
		mpp_decoder_put_frame(dec, &frame);

		//* 10. destroy mpp_decoder
		mpp_decoder_destory(dec);
	}

	long long time_end = get_now_us();
	logw("time: %lld", time_end - time_start);
	float fps = (long long)dec_data->file_num * 1000000LL/ (time_end - time_start);
	logw("speed: fps %.2f", fps);

	//* disable layer
	struct aicfb_layer_data layer = {0};
	layer.enable = 0;
	if(ioctl(fb0_fd, AICFB_UPDATE_LAYER_CONFIG, &layer) < 0)
		loge("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");

out:
	if (dec_data)
		free(dec_data);
	if (fb0_fd)
		close(fb0_fd);
	if (fp)
		fclose(fp);
	return ret;
}
