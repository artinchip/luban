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
#include <video/artinchip_ge.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>

#include "mpp_ge.h"
#include "mpp_decoder.h"
#include "mpp_log.h"

static int g_screen_w = 0;
static int g_screen_h = 0;
static int g_fb_len = 0;
static int g_fb_stride = 0;
static unsigned int g_fb_format = 0;
static unsigned int g_fb_phy = 0;
unsigned char *g_fb_buf = NULL;

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
	layer.scale_size.width = MPP_MIN(1024, picture_buf->size.width);
	layer.scale_size.height= MPP_MIN(600, picture_buf->size.height);

	logi("stride: %d, crop_en: %d", picture_buf->stride[0], picture_buf->crop_en);
	logi("%d: %d, %d: %d", picture_buf->crop.x, picture_buf->crop.y, picture_buf->crop.width, picture_buf->crop.height);

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

static int fb_open(void)
{
	struct fb_fix_screeninfo fix;
	struct fb_var_screeninfo var;
	struct aicfb_layer_data layer;

	int fb_fd;
	fb_fd = open("/dev/fb0", O_RDWR);
	if (fb_fd == -1) {
		loge("open fb fail");
		return -1;
	}

	if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &fix) < 0) {
		loge("ioctl FBIOGET_FSCREENINFO");
		close(fb_fd);
		return -1;
	}

	if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &var) < 0) {
		loge("ioctl FBIOGET_VSCREENINFO");
		close(fb_fd);
		return -1;
	}

	if(ioctl(fb_fd, AICFB_GET_FB_LAYER_CONFIG, &layer) < 0) {
		loge("ioctl FBIOGET_VSCREENINFO");
		close(fb_fd);
		return -1;
	}

	g_screen_w = var.xres;
	g_screen_h = var.yres;
	g_fb_len = fix.smem_len;
	g_fb_phy = fix.smem_start;
	g_fb_stride = fix.line_length;
	g_fb_format = layer.buf.format;

	logi("screen_w = %d, screen_h = %d, stride = %d, format = %d\n",
			var.xres, var.yres, g_fb_stride, g_fb_format);
	return fb_fd;
}

static int render_frame(struct mpp_frame *frame)
{
	int ret = 0;

	int fb_fd = fb_open();

	if (frame->buf.format == MPP_FMT_ARGB_8888 || frame->buf.format == MPP_FMT_ABGR_8888 ||
	    frame->buf.format == MPP_FMT_RGBA_8888 || frame->buf.format == MPP_FMT_BGRA_8888) {
		//* 1. if the pixels have alpha channel, we need enable pixel alpha blending in GE.
		//     the data flow: VE -> GE -> DE.
		logi("alpha channel, we need pixel alpha blending");

		struct ge_bitblt blt = {0};
		memset(&blt, 0, sizeof(struct ge_bitblt));
		/* source buffer */
		memcpy(&blt.src_buf, &frame->buf, sizeof(struct mpp_frame));

		struct mpp_ge *ge = mpp_ge_open();
		if (!ge) {
			loge("ge open fail\n");
			return -1;
		}

		/* dstination buffer */
		blt.dst_buf.buf_type = MPP_PHY_ADDR;
		blt.dst_buf.phy_addr[0] = g_fb_phy;
		blt.dst_buf.stride[0] = g_fb_stride;
		blt.dst_buf.size.width = g_screen_w;
		blt.dst_buf.size.height = g_screen_h;
		blt.dst_buf.format = g_fb_format;

		blt.dst_buf.crop_en = 1;
		blt.dst_buf.crop.x = 0;
		blt.dst_buf.crop.y = 0;
		blt.dst_buf.crop.width = frame->buf.size.width;
		blt.dst_buf.crop.height = frame->buf.size.height;
		blt.ctrl.alpha_en = 1;

		ret =  mpp_ge_bitblt(ge, &blt);
		if (ret < 0){
			loge("ge bitblt fail\n");
		}

		ret = mpp_ge_emit(ge);
		if (ret < 0){
			loge("ge emit fail\n");
		}

		ret = mpp_ge_sync(ge);
		if (ret < 0) {
			loge("ge sync fail\n");
		}

		if (ge)
			mpp_ge_close(ge);
	} else {
		//* 2. data flow: VE -> DE
		set_fb_layer_alpha(fb_fd, 0);
		video_layer_set(fb_fd, &frame->buf);
	}

	close(fb_fd);

	return 0;
}

int main(int argc, char **argv)
{
	int ret = 0;
	int opt;
	int file_len;
	FILE* fp = NULL;
	char* ptr = NULL;
	int type = MPP_CODEC_VIDEO_DECODER_MJPEG;

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
	render_frame(&frame);

	//* 9. return this frame
	mpp_decoder_put_frame(dec, &frame);

	//* 10. destroy mpp_decoder
	mpp_decoder_destory(dec);

out:
	if(fp)
		fclose(fp);
	return ret;
}
