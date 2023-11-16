/*
* Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
*
*  author: <qi.xu@artinchip.com>
*  Desc: picture random error test
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <pthread.h>
#include <errno.h>

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

static void print_help(char* prog)
{
	printf("%s\n", prog);
	printf("Compile time: %s\n", __TIME__);
	printf("Usage: dec_test [OPTIONS] [SLICES PATH]\n\n"
		"Options:\n"
		"\t-i                             input stream file name\n"
		"\t-l                             loop time\n"
		"\t-h                             help\n\n"
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
	layer.scale_size.width = 1024;
	layer.scale_size.height= 600;

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

	//* display this picture 2 seconds
	usleep(1000000);

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
	set_fb_layer_alpha(fb0_fd, 0);
	video_layer_set(fb0_fd, &frame->buf);

	if (fb0_fd >= 0)
		close(fb0_fd);
	return 0;
}

int main(int argc, char **argv)
{
	int ret = 0;
	int opt;
	int file_len;
	unsigned char* buffer = NULL;
	int i;
	int loop_time = 1;
	FILE* fp = NULL;
	char* ptr = NULL;
	int type = MPP_CODEC_VIDEO_DECODER_MJPEG;

	while (1) {
		opt = getopt(argc, argv, "i:l:h");
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
		case 'l':
			loop_time = atoi(optarg);
			break;
		case 'h':
		default:
			print_help(argv[0]);
			return -1;
		}
	}

	if(fp == NULL) {
		print_help(argv[0]);
		return -1;
	}

	file_len = get_file_size(fp);
	buffer = (unsigned char*)malloc(file_len);
	fread(buffer, 1, file_len, fp);

	for(i=0; i<loop_time; i++) {
		//* 1. create mpp_decoder
		struct mpp_decoder* dec = mpp_decoder_create(type);

		struct decode_config config;
		config.bitstream_buffer_size = (file_len + 1023) & (~1023);
		config.extra_frame_num = 0;
		config.packet_count = 1;

		// JPEG not supprt YUV2RGB
		if(type == MPP_CODEC_VIDEO_DECODER_MJPEG)
			config.pix_fmt = MPP_FMT_YUV420P;
		else if(type == MPP_CODEC_VIDEO_DECODER_PNG)
			config.pix_fmt = MPP_FMT_ARGB_8888;

		//* 2. init mpp_decoder
		mpp_decoder_init(dec, &config);

		file_len -= i;
		logi("file_len: %d", file_len);
		//* 3. get an empty packet from mpp_decoder
		struct mpp_packet packet;
		memset(&packet, 0, sizeof(struct mpp_packet));
		mpp_decoder_get_packet(dec, &packet, file_len);

		int err_pos = rand() % file_len;
		err_pos = err_pos < 100 ? 100 : err_pos;
		unsigned char tmp = buffer[err_pos];
		int val = rand() % 255;
		buffer[err_pos] = val;

		//* 4. copy data to packet
		memcpy(packet.data, buffer, file_len);
		packet.size = file_len;
		packet.flag = PACKET_FLAG_EOS;

		buffer[err_pos] = tmp;

		logi("err pos: %d, val: %d", err_pos, val);

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
		render_frame(&frame);

		//* 9. return this frame
		mpp_decoder_put_frame(dec, &frame);

		//* 10. destroy mpp_decoder
		mpp_decoder_destory(dec);
	}


out:
	if(buffer)
		free(buffer);
	if(fp)
		fclose(fp);
	return ret;
}
