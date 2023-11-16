/*
* Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
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
#include <dlfcn.h>
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
#include <sys/time.h>

#include <video/artinchip_fb.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>

#include "mpp_decoder.h"
#include "mpp_log.h"

#define MPP_DSO "/usr/local/lib/libmpp_decoder.so"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static void print_help(void)
{
	printf("Usage: dec_test [OPTIONS] [SLICES PATH]\n\n"
		"Options:\n"
		" -i                             input stream file name\n"
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

	if(picture_buf->format == MPP_FMT_ARGB_8888 || picture_buf->format == MPP_FMT_RGBA_8888
	  || picture_buf->format == MPP_FMT_RGB_888) {
		// rgb not support scale, so we need crop if the resolution is too large
		picture_buf->crop_en = 1;
		picture_buf->crop.width = MIN(1024, picture_buf->size.width);
		picture_buf->crop.height = MIN(600, picture_buf->size.height);
	} else {
		layer.scale_size.width = MIN(1024, picture_buf->size.width);
		layer.scale_size.height= MIN(1024, picture_buf->size.height);
	}

	logi("stride: %d", picture_buf->stride[0]);

	layer.pos.x = 0;
	layer.pos.y = 0;
	layer.buf = *picture_buf;

	logi("format: %d", picture_buf->format);
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

	//* display this picture 2 seconds
	usleep(2000000);

	//* disable layer
	layer.enable = 0;
	if(ioctl(fb0_fd, AICFB_UPDATE_LAYER_CONFIG, &layer) < 0)
		loge("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");

	//* remove dmabuf to de driver
	for(i=0; i<dmabuf_num; i++) {
		if (ioctl(fb0_fd, AICFB_RM_DMABUF, &dmabuf_fd[i]) < 0)
			loge("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");
	}
}

static int render_frame(struct mpp_frame* frame)
{
	int fb0_fd = 0;
	struct fb_fix_screeninfo finfo;
	struct mpp_buf *pic_buffer = NULL;
	int ret;

	//* 1. open fb0
	fb0_fd = open("/dev/fb0", O_RDWR);
	if (fb0_fd < 0) {
		logw("open fb0 failed!");
		return -1;
	}

	if (ioctl(fb0_fd, FBIOGET_FSCREENINFO, &finfo) == -1) {
		loge("read fixed information failed!");
		close(fb0_fd);
		return -1;
	}

	//* 2 disp frame;
	set_fb_layer_alpha(fb0_fd, 128);
	video_layer_set(fb0_fd, &frame->buf);

	if (fb0_fd >= 0)
		close(fb0_fd);
	return 0;
}

struct mpp_decoder_ops {
	struct mpp_decoder* (*create_func)(enum mpp_codec_type type);
	void (*destory_func)(struct mpp_decoder* decoder);
	int (*init_func)(struct mpp_decoder *decoder, struct decode_config *config);
	int (*decode_func)(struct mpp_decoder* decoder);
	int (*get_packet_func)(struct mpp_decoder* decoder, struct mpp_packet* packet, int size);
	int (*put_packet_func)(struct mpp_decoder* decoder, struct mpp_packet* packet);
	int (*get_frame_func)(struct mpp_decoder* decoder, struct mpp_frame* frame);
	int (*put_frame_func)(struct mpp_decoder* decoder, struct mpp_frame* frame);
};

int main(int argc, char **argv)
{
	int ret = 0;
	int opt;
	char* file_name;
	int file_len;
	FILE* fp = NULL;
	char* ptr = NULL;
	int type = MPP_CODEC_VIDEO_DECODER_MJPEG;

	struct mpp_decoder_ops ops;
	void* mpp_handle = dlopen(MPP_DSO, RTLD_LAZY);
	if(mpp_handle == NULL) {
		loge("dlopen %s failed", MPP_DSO);
		return -1;
	}

	while (1) {
		opt = getopt(argc, argv, "i:h");
		if (opt == -1) {
			break;
		}
		switch (opt) {
		case 'i':
			logd("file path: %s", optarg);
			if (optarg) {
				ptr = strrchr(optarg, '.');

				if (!strcmp(ptr, ".jpg")) {
					type = MPP_CODEC_VIDEO_DECODER_MJPEG;
				}
				if (!strcmp(ptr, ".png")) {
					type = MPP_CODEC_VIDEO_DECODER_PNG;
				}
				logd("decode type: 0x%02X", type);
			}
			fp = fopen(optarg, "rb");

			break;
		case 'h':
		default:
			print_help();
			return -1;
		}
	}

	if(fp == NULL) {
		loge("please input the right file path");
		return -1;
	}

	file_len = get_file_size(fp);
	ops.create_func = dlsym(mpp_handle, "mpp_decoder_create");
	ops.destory_func = dlsym(mpp_handle, "mpp_decoder_destory");
	ops.decode_func = dlsym(mpp_handle, "mpp_decoder_decode");
	ops.init_func = dlsym(mpp_handle, "mpp_decoder_init");
	ops.get_packet_func = dlsym(mpp_handle, "mpp_decoder_get_packet");
	ops.put_packet_func = dlsym(mpp_handle, "mpp_decoder_put_packet");
	ops.get_frame_func = dlsym(mpp_handle, "mpp_decoder_get_frame");
	ops.put_frame_func = dlsym(mpp_handle, "mpp_decoder_put_frame");

	//* 1. create mpp_decoder
	struct mpp_decoder* dec = ops.create_func(type);

	struct decode_config config;
	config.bitstream_buffer_size = (file_len + 1023) & (~1023);
	config.extra_frame_num = 0;
	config.packet_count = 1;

	// JPEG not supprt YUV2RGB
	if(type == MPP_CODEC_VIDEO_DECODER_MJPEG)
		config.pix_fmt = MPP_FMT_YUV420P;
	else if(type == MPP_CODEC_VIDEO_DECODER_PNG)
		config.pix_fmt = MPP_FMT_RGB_888;

	//* 2. init mpp_decoder
	ops.init_func(dec, &config);

	//* 3. get an empty packet from mpp_decoder
	struct mpp_packet packet;
	memset(&packet, 0, sizeof(struct mpp_packet));
	ops.get_packet_func(dec, &packet, file_len);

	//* 4. copy data to packet
	fread(packet.data, 1, file_len, fp);
	packet.size = file_len;
	packet.flag = PACKET_FLAG_EOS;

	//* 5. put the packet to mpp_decoder
	ops.put_packet_func(dec, &packet);

	//* 6. decode
	time_start(mpp_decoder_decode);
	ret = ops.decode_func(dec);
	if(ret < 0) {
		loge("decode error");
		goto out;
	}
	time_end(mpp_decoder_decode);

	//* 7. get a decoded frame
	struct mpp_frame frame;
	memset(&frame, 0, sizeof(struct mpp_frame));
	ops.get_frame_func(dec, &frame);

	//* 8. render this frame
	render_frame(&frame);

	//* 9. return this frame
	ops.put_frame_func(dec, &frame);

	//* 10. destroy mpp_decoder
	ops.destory_func(dec);

out:
	dlclose(mpp_handle);
	if(fp)
		fclose(fp);
	return ret;
}
