/*
* Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
*
*  author: <qi.xu@artinchip.com>
*  Desc:
*/

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <video/artinchip_fb.h>

#include "mpp_video.h"
#include "mpp_log.h"

#define FRAME_BUF_NUM		(10)

struct frame_info {
	int fd[3];		// dma-buf fd
	int fd_num;		// number of dma-buf
	int used;		// if the dma-buf of this frame add to de drive
};

struct mpp_video {
	struct mpp_decoder* dec;
	int render_eos;
	struct frame_info frame_info[FRAME_BUF_NUM];
	int dec_err;

	int disp_pos_x;
	int disp_pos_y;
	int disp_width;
	int disp_height;
	int stop;
	pthread_t render_thread_id;
	pthread_t decode_thread_id;
};

static long long get_now_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000ll + tv.tv_usec;
}

/********************* timer *****************************/

struct av_timer {
	int64_t start_time_us; // us
	int64_t system_time_us; // us
};

struct av_timer* av_timer_create()
{
	struct av_timer* timer = (struct av_timer*)malloc(sizeof(struct av_timer));
	if (timer == NULL) {
		return NULL;
	}
	memset(timer, 0, sizeof(struct av_timer));

	return timer;
}

void av_timer_destroy(struct av_timer* timer)
{
	free(timer);
}

int av_timer_set_time(struct av_timer* timer, int64_t t)
{
	timer->start_time_us = t;
	timer->system_time_us = get_now_us();
	return 0;
}

int64_t av_timer_get_time(struct av_timer* timer)
{
	int64_t current_time = get_now_us();
	int64_t pass_time = current_time - timer->system_time_us;

	pass_time += timer->start_time_us;
	timer->start_time_us = pass_time;
	timer->system_time_us = current_time;

	return pass_time;
}
/*********************************************************/

int set_fb_layer_alpha(int fb0_fd, int val)
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

#define MIN(a, b) ((a) < (b) ? (a) : (b))
void video_layer_set(int fb0_fd, struct mpp_buf *picture_buf, struct frame_info* frame, struct mpp_video* p)
{
	struct aicfb_layer_data layer = {0};
	int dmabuf_num = 0;
	struct dma_buf_info dmabuf_fd[3];
	int i;

	if (fb0_fd < 0)
		return;

	layer.layer_id = AICFB_LAYER_TYPE_VIDEO;
	layer.enable = 1;
	if (picture_buf->format == MPP_FMT_YUV420P || picture_buf->format == MPP_FMT_NV12
	  || picture_buf->format == MPP_FMT_NV21) {
		layer.scale_size.width = p->disp_width;
		layer.scale_size.height= p->disp_height;
	}

	layer.pos.x = p->disp_pos_x;
	layer.pos.y = p->disp_pos_y;
	memcpy(&layer.buf, picture_buf, sizeof(struct mpp_buf));

	if (picture_buf->format == MPP_FMT_ARGB_8888) {
		dmabuf_num = 1;
	} else if (picture_buf->format == MPP_FMT_RGBA_8888) {
		dmabuf_num = 1;
	} else if (picture_buf->format == MPP_FMT_RGB_888) {
		dmabuf_num = 1;
	} else if (picture_buf->format == MPP_FMT_YUV420P) {
		dmabuf_num = 3;
	} else if (picture_buf->format == MPP_FMT_NV12 || picture_buf->format == MPP_FMT_NV21) {
		dmabuf_num = 2;
	} else if (picture_buf->format == MPP_FMT_YUV444P) {
		dmabuf_num = 3;
	} else if (picture_buf->format == MPP_FMT_YUV422P) {
		dmabuf_num = 3;
	} else if (picture_buf->format == MPP_FMT_YUV400) {
		dmabuf_num = 1;
	} else {
		loge("no support picture foramt %d, default argb8888", picture_buf->format);
	}

	// add dmabuf to de driver
	if(!frame->used) {
		for(i=0; i<dmabuf_num; i++) {
			frame->fd[i] = picture_buf->fd[i];
			dmabuf_fd[i].fd = picture_buf->fd[i];
			if (ioctl(fb0_fd, AICFB_ADD_DMABUF, &dmabuf_fd[i]) < 0)
				loge("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");
		}
		frame->used = 1;
		frame->fd_num = dmabuf_num;
	}

	logi("width: %d, height %d, stride: %d, %d, crop_en: %d, crop_w: %d, crop_h: %d",
		layer.buf.size.width, layer.buf.size.height,
		layer.buf.stride[0], layer.buf.stride[1], layer.buf.crop_en,
		layer.buf.crop.width, layer.buf.crop.height);
	// display
	if (ioctl(fb0_fd, AICFB_UPDATE_LAYER_CONFIG, &layer) < 0)
		loge("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");

	// wait vsync (wait layer config)
	ioctl(fb0_fd, AICFB_WAIT_FOR_VSYNC, NULL);
}

void* render_thread(void *arg)
{
	int fb0_fd = -1;
	int cur_frame_id = 0;
	int last_frame_id = 1;
	struct mpp_frame frame[2];
	struct mpp_buf *pic_buffer = NULL;
	int frame_num = 0;
	int ret;
	int i, j;
	struct mpp_video* p = (struct mpp_video*)arg;

	struct av_timer* timer = av_timer_create();

	// 1. open fb0
	fb0_fd = open("/dev/fb0", O_RDWR);
	if (fb0_fd < 0) {
		logw("open fb0 failed!");
	}

	// 2. render frame until eos
	while(!p->render_eos) {
		memset(&frame[cur_frame_id], 0, sizeof(struct mpp_frame));

		if(p->dec_err || p->stop)
			break;

		// 2.1 get frame
		ret = mpp_decoder_get_frame(p->dec, &frame[cur_frame_id]);
		if(ret == DEC_NO_RENDER_FRAME || ret == DEC_ERR_FM_NOT_CREATE
			|| ret == DEC_NO_EMPTY_FRAME) {
			usleep(10000);
			continue;
		} else if(ret) {
			logw("mpp_dec_get_frame error, ret: %x", ret);
			p->dec_err = 1;
			break;
		}

		p->render_eos = frame[cur_frame_id].flags & FRAME_FLAG_EOS;
		logi("decode_get_frame successful: frame id %d, number %d, flag: %d",
			frame[cur_frame_id].id, frame_num, frame[cur_frame_id].flags);

		if (frame[cur_frame_id].flags & FRAME_FLAG_ERROR) {
			loge("frame error");
		}
		pic_buffer = &frame[cur_frame_id].buf;

		// set first start time
		if (0 == frame_num) {
			av_timer_set_time(timer, frame[cur_frame_id].pts);
		}

		int64_t diff_time = frame[cur_frame_id].pts - av_timer_get_time(timer);
		if (diff_time > 10000) {
			loge("diff_time: %lld ms, wait", (long long)diff_time/1000);
			usleep(diff_time);
		}

		// 2.3 disp frame;
		if(!(frame[cur_frame_id].flags & FRAME_FLAG_ERROR)) {
			set_fb_layer_alpha(fb0_fd, 0);
			video_layer_set(fb0_fd, pic_buffer, &p->frame_info[frame[cur_frame_id].id], p);
		}

		// 2.4 return the last frame
		if (frame_num) {
			ret = mpp_decoder_put_frame(p->dec, &frame[last_frame_id]);
		}

		int tmp = cur_frame_id;
		cur_frame_id = last_frame_id;
		last_frame_id = tmp;

		frame_num++;
	}

	// put the last frame when eos
	mpp_decoder_put_frame(p->dec, &frame[last_frame_id]);

	// disable layer
	struct aicfb_layer_data layer = {0};
	layer.enable = 0;
	if (ioctl(fb0_fd, AICFB_UPDATE_LAYER_CONFIG, &layer) < 0)
		loge("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");

	// remove all dmabuf from de driver
	for (i=0; i<FRAME_BUF_NUM; i++) {
		if(p->frame_info[i].used == 0)
			continue;

		for(j=0; j<p->frame_info[i].fd_num; j++) {
			if (ioctl(fb0_fd, AICFB_RM_DMABUF, &p->frame_info[i].fd[j]) < 0)
				loge("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");
		}
	}

	if (fb0_fd >= 0)
		close(fb0_fd);

	av_timer_destroy(timer);

	return NULL;
}

void* decode_thread(void *p)
{
	struct mpp_video* data = (struct mpp_video*)p;
	int ret = 0;

	while (!data->render_eos) {
		if (data->stop)
			break;

		ret = mpp_decoder_decode(data->dec);
		if (ret == DEC_NO_READY_PACKET || ret == DEC_NO_EMPTY_FRAME) {
			usleep(1000);
			continue;
		} else if( ret ) {
			logw("decode ret: %x", ret);
			//data->dec_err = 1;
			//break;
		}
		usleep(10000);
	}

	return NULL;
}

struct mpp_video* mpp_video_create()
{
	struct mpp_video* impl = (struct mpp_video*)malloc(sizeof(struct mpp_video));
	if (NULL == impl) {
		return NULL;
	}
	memset(impl, 0, sizeof(struct mpp_video));

	return impl;
}

int mpp_video_init(struct mpp_video* p, enum mpp_codec_type type, struct decode_config *config)
{
	p->dec = mpp_decoder_create(type);
	mpp_decoder_init(p->dec, config);
	return 0;
}

int mpp_video_start(struct mpp_video* p)
{
	pthread_create(&p->decode_thread_id, NULL, decode_thread, p);
	pthread_create(&p->render_thread_id, NULL, render_thread, p);
	return 0;
}

int mpp_video_set_disp_window(struct mpp_video* p, struct mpp_rect* disp_window)
{
	p->disp_pos_x = disp_window->x;
	p->disp_pos_y = disp_window->y;
	p->disp_height = disp_window->height;
	p->disp_width = disp_window->width;
	return 0;
}

int mpp_video_send_packet(struct mpp_video* p, struct mpp_packet* input_packet, int timeout_ms)
{
#define SLEEP_TIME_MS (10)

	int times = (timeout_ms <= 0) ? 1 : (timeout_ms / SLEEP_TIME_MS);
	int i;

	struct mpp_packet packet;
	for (i=0; i<times; i++) {
		if (mpp_decoder_get_packet(p->dec, &packet, input_packet->size) == 0) {
			memcpy(packet.data, input_packet->data, input_packet->size);
			packet.size = input_packet->size;
			packet.flag = input_packet->flag;
			packet.pts = input_packet->pts;
			mpp_decoder_put_packet(p->dec, &packet);
			logi("send packet success");
			return 0;
		}
		usleep(SLEEP_TIME_MS * 1000);
	}

	logw("send packet timeout");
	return -1;
}

int mpp_video_stop(struct mpp_video* p)
{
	p->stop = 1;
	pthread_join(p->decode_thread_id, NULL);
	pthread_join(p->render_thread_id, NULL);
	return 0;
}

void mpp_video_destroy(struct mpp_video* p)
{
	mpp_decoder_destory(p->dec);
	free(p);
}
