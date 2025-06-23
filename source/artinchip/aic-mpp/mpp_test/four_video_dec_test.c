/*
 * Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <qi.xu@artinchip.com>
 *  Desc: four 640x480 video decode test
 */

#define LOG_TAG "dec_test"

#include <stdio.h>
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

#include "dma_allocator.h"
#include "frame_allocator.h"
#include "bit_stream_parser.h"
#include "mpp_decoder.h"
#include "mpp_log.h"

#define DECODER_NUM		(4)
#define FRAME_BUF_NUM		(3)
#define MAX_TEST_FILE           (256)
#define SCREEN_WIDTH            1024
#define SCREEN_HEIGHT           600
#define FRAME_COUNT		30

const char *dev_fb0 = "/dev/fb0";

struct frame_info {
	int fd[3];		// dma-buf fd
	int fd_num;		// number of dma-buf
	int used;		// if the dma-buf of this frame add to de drive
};

struct dec_ctx {
	struct mpp_decoder  *decoder[4];
	struct frame_info frame_info[FRAME_BUF_NUM];	//

	struct bit_stream_parser *parser[4];

	int stream_eos;
	int render_eos;
	int dec_err;
	int cmp_data_err;

	char file_input[MAX_TEST_FILE][1024];	// test file name
	int file_num;				// test file number

	int decode_num;
	int output_format;
	int dma_fd;
	struct mpp_frame frame[FRAME_BUF_NUM];
};

/********************** frame allocator **********************************/
struct ext_frame_allocator {
	struct frame_allocator base;
	struct mpp_frame frame[FRAME_BUF_NUM];
	int frame_idx;
};

#define BUF_STRIDE 1280
#define BUF_HEIGHT 960
static int alloc_frame_buffer(struct frame_allocator *p, struct mpp_frame* frame,
		int width, int height, enum mpp_pixel_format format)
{
	struct ext_frame_allocator* impl = (struct ext_frame_allocator*)p;
	if(format != MPP_FMT_NV12) {
		loge("format error, (%d), we need NV12", format);
	}

	logw("alloc_frame_buffer, idx: %d, w: %d, h: %d", impl->frame_idx, width, height);
	frame->buf.fd[0] = impl->frame[impl->frame_idx].buf.fd[0];
	frame->buf.fd[1] = impl->frame[impl->frame_idx].buf.fd[1];
	frame->buf.fd[2] = impl->frame[impl->frame_idx].buf.fd[2];
	frame->buf.size.width = impl->frame[impl->frame_idx].buf.size.width;
	frame->buf.size.height = impl->frame[impl->frame_idx].buf.size.height;
	frame->buf.stride[0] = impl->frame[impl->frame_idx].buf.stride[0];
	frame->buf.stride[1] = impl->frame[impl->frame_idx].buf.stride[1];
	frame->buf.stride[2] = impl->frame[impl->frame_idx].buf.stride[2];

	impl->frame_idx ++;

	return 0;
}

static int free_frame_buffer(struct frame_allocator *p, struct mpp_frame *frame)
{
	// do nothing
	return 0;
}

static int close_allocator(struct frame_allocator *p)
{
	struct ext_frame_allocator* impl = (struct ext_frame_allocator*)p;

	free(impl);

	return 0;
}

static struct alloc_ops def_ops = {
	.alloc_frame_buffer = alloc_frame_buffer,
	.free_frame_buffer = free_frame_buffer,
	.close_allocator = close_allocator,
};

static struct frame_allocator* open_allocator(struct dec_ctx* ctx)
{
	struct ext_frame_allocator* impl = (struct ext_frame_allocator*)malloc(sizeof(struct ext_frame_allocator));
	if(impl == NULL) {
		return NULL;
	}
	memset(impl, 0, sizeof(struct ext_frame_allocator));

	memcpy(impl->frame, ctx->frame, sizeof(struct mpp_frame)* FRAME_BUF_NUM);
	impl->base.ops = &def_ops;

	return &impl->base;
}

static int alloc_framebuffers(struct dec_ctx* ctx)
{
	int i;
	ctx->dma_fd = dmabuf_device_open();
	for(i=0; i<FRAME_BUF_NUM; i++) {
		ctx->frame[i].id = i;
		ctx->frame[i].buf.buf_type = MPP_DMA_BUF_FD;
		ctx->frame[i].buf.size.height = BUF_HEIGHT;
		ctx->frame[i].buf.size.width = BUF_STRIDE;
		ctx->frame[i].buf.stride[0] = BUF_STRIDE;
		ctx->frame[i].buf.stride[1] = BUF_STRIDE;
		ctx->frame[i].buf.stride[2] = 0;
		ctx->frame[i].buf.format = MPP_FMT_NV12;
		if(mpp_buf_alloc(ctx->dma_fd, &ctx->frame[i].buf))
			return -1;
	}
	return 0;
}

static int free_framebuffers(struct dec_ctx* ctx)
{
	int i;
	for(i=0; i<FRAME_BUF_NUM; i++) {
		mpp_buf_free(&ctx->frame[i].buf);
	}
	dmabuf_free(ctx->dma_fd);
	return 0;
}
/********************** frame allocator end **********************************/

static void print_help(const char* prog)
{
	printf("name: %s\n", prog);
	printf("Compile time: %s\n", __TIME__);
	printf("Usage: mpp_test [options]:\n"
		"\t-i                             input stream file name\n"
		"\t-t                             directory of test files\n"
		"\t-d                             display the picture\n"
		"\t-c                             enable compare output data\n"
		"\t-f                             output pixel format\n"
		"\t-l                             loop time\n"
		"\t-h                             help\n\n"
		"Example1(test single file): mpp_test -i test.264\n"
		"Example2(test some files) : mpp_test -t /usr/data/\n");
}

static long long get_now_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000ll + tv.tv_usec;
}

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
void video_layer_set(int fb0_fd, struct mpp_buf *picture_buf, struct frame_info* frame)
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
		  // rgb format not support scale
		layer.scale_size.width = SCREEN_WIDTH;
		layer.scale_size.height= SCREEN_HEIGHT;
	}
	picture_buf->crop_en = 0;
	picture_buf->crop.x = 0;
	picture_buf->crop.y = 0;
	picture_buf->crop.width = 0;
	picture_buf->crop.height = 0;

	layer.pos.x = 0;
	layer.pos.y = 0;
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

	//* add dmabuf to de driver
	if(!frame->used) {
		for(i=0; i<dmabuf_num; i++) {
			frame->fd[i] = picture_buf->fd[i];
			dmabuf_fd[i].fd = picture_buf->fd[i];
			if (ioctl(fb0_fd, AICFB_ADD_DMABUF, &dmabuf_fd[i]) < 0)
				loge("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");
		}
		frame->used = 1;
		frame->fd_num = dmabuf_num;
	} else {

	}

	logi("width: %d, height %d, stride: %d, %d, crop_en: %d, crop_w: %d, crop_h: %d, format: %d",
		layer.buf.size.width, layer.buf.size.height,
		layer.buf.stride[0], layer.buf.stride[1], layer.buf.crop_en,
		layer.buf.crop.width, layer.buf.crop.height,
		layer.buf.format);
	//* display
	if (ioctl(fb0_fd, AICFB_UPDATE_LAYER_CONFIG, &layer) < 0)
		loge("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");

	//* wait vsync (wait layer config)
	ioctl(fb0_fd, AICFB_WAIT_FOR_VSYNC, NULL);

	logi("wait vsync finish");
}

static void swap(int *a, int *b)
{
	int tmp = *a;
	*a = *b;
	*b = tmp;
}

void* render_thread(void *p)
{
	int fb0_fd = -1;
	int cur_frame_id = 0;
	int last_frame_id = 1;
	struct mpp_frame frame[2];
	struct mpp_buf *pic_buffer = NULL;
	int frame_num = 0;
	int ret;
	int i, j;
	long long time = 0;
	long long duration_time = 0;
	int disp_frame_cnt = 0;
	long long total_duration_time = 0;
	int total_disp_frame_cnt = 0;
	struct dec_ctx *data = (struct dec_ctx*)p;
	int display_num = 0;

	//* 1. open fb0
	fb0_fd = open(dev_fb0, O_RDWR);
	if (fb0_fd < 0) {
		logw("open fb0 failed!");
	}

	time = get_now_us();
	//* 2. render frame until eos
	while(!data->render_eos) {
		memset(&frame[cur_frame_id], 0, sizeof(struct mpp_frame));
		logw("display_num: %d", display_num);

		if(data->dec_err)
			break;

		if(display_num > 85) {
			logw("render exit, display_num: %d", display_num);
			data->render_eos = 1;
			break;
		}

		//* 2.1 get frame
		for (i=0; i<DECODER_NUM; i++) {
			while(1) {
				ret = mpp_decoder_get_frame(data->decoder[i], &frame[cur_frame_id]);
				if(ret == DEC_NO_RENDER_FRAME || ret == DEC_ERR_FM_NOT_CREATE
					|| ret == DEC_NO_EMPTY_FRAME || ret == DEC_ERR_FM_NOT_CREATE) {
					logw("get frame (%d), ret: %d", i, ret);
					usleep(10000);
					continue;
				} else if(ret) {
					loge("mpp_dec_get_frame error, ret: %x", ret);
					data->dec_err = 1;
					break;
				} else {
					logd("get frame success");
					break;
				}
			}
			logd("decoder(%d) get frame success", i);
		}

		time = get_now_us() - time;
		duration_time += time;
		disp_frame_cnt += 1;
		data->render_eos = frame[cur_frame_id].flags & FRAME_FLAG_EOS;
		logi("decode_get_frame successful: frame id %d, number %d, flag: %d, pts: %lld",
			frame[cur_frame_id].id, frame_num, frame[cur_frame_id].flags, frame[cur_frame_id].pts);

		pic_buffer = &frame[cur_frame_id].buf;
		//* 2.2 compare data
		// save_data(data, pic_buffer, fp_save);

		//* 2.3 disp frame;
		set_fb_layer_alpha(fb0_fd, 10);
		video_layer_set(fb0_fd, pic_buffer, &data->frame_info[frame[cur_frame_id].id]);

		//* 2.4 return the last frame
		if(frame_num) {
			for (i=0; i<DECODER_NUM; i++) {
				ret = mpp_decoder_put_frame(data->decoder[i], &frame[last_frame_id]);
			}
		}

		swap(&cur_frame_id, &last_frame_id);

		if(disp_frame_cnt > FRAME_COUNT) {
			float fps, avg_fps;
			total_disp_frame_cnt += disp_frame_cnt;
			total_duration_time += duration_time;
			fps = (float)(duration_time / 1000.0f);
			fps = (disp_frame_cnt * 1000) / fps;
			avg_fps = (float)(total_duration_time / 1000.0f);
			avg_fps = (total_disp_frame_cnt * 1000) / avg_fps;
			logi("decode speed info: fps: %.2f, avg_fps: %.2f", fps, avg_fps);
			duration_time = 0;
			disp_frame_cnt = 0;
		}
		time = get_now_us();
		frame_num++;
		display_num ++;
	}

	//* put the last frame when eos
	for (i=0; i<DECODER_NUM; i++) {
		mpp_decoder_put_frame(data->decoder[i], &frame[last_frame_id]);
	}

	//* disable layer
	struct aicfb_layer_data layer = {0};
	layer.enable = 0;
	if (ioctl(fb0_fd, AICFB_UPDATE_LAYER_CONFIG, &layer) < 0)
		loge("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");

	//* remove all dmabuf from de driver
	for(i=0; i<FRAME_BUF_NUM; i++) {
		if(data->frame_info[i].used == 0)
			continue;

		for(j=0; j<data->frame_info[i].fd_num; j++) {
			if (ioctl(fb0_fd, AICFB_RM_DMABUF, &data->frame_info[i].fd[j]) < 0)
				loge("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");
		}
	}

	if (fb0_fd >= 0)
		close(fb0_fd);

	loge("render thread exit");

	return NULL;
}

void* decode_thread(void *p)
{
	struct dec_ctx *data = (struct dec_ctx*)p;
	int ret = 0;
	int i;

	while(!data->render_eos) {
		for (i=0; i<DECODER_NUM; i++) {

			while(1) {
				logi("decode: %d, render_eos: %d", i, data->render_eos);
				if(data->render_eos)
					goto out;
				ret = mpp_decoder_decode(data->decoder[i]);
				if(ret == DEC_NO_READY_PACKET || ret == DEC_NO_EMPTY_FRAME) {
					usleep(1000);
					continue;
				} else if( ret ) {
					loge("decode ret: %x", ret);
					data->dec_err = 1;
					break;
				}

				logw("decoder %d finish one frame", i);
				break;
			}
		}
		logw("all of decoders finish one frame ");
		data->decode_num ++;
	}

out:
	loge("decode thread exit");
	return NULL;
}

int dec_decode(struct dec_ctx *data, char* path)
{
	int ret;
	int file_fd[4];
	int i;
	unsigned char *buf = NULL;
	pthread_t render_thread_id;
	pthread_t decode_thread_id;
	int dec_type = MPP_CODEC_VIDEO_DECODER_H264;
	long long pts = 0;

	alloc_framebuffers(data);

	logd("dec_test start");

	const char filename[4][1024] = {"/data/1.264", "/data/2.264", "/data/3.264", "/data/4.264"};

	struct mpp_dec_output_pos pos[4] = { {640, 0}, {0, 0}, {0, 480}, {640, 480}};
	for (i=0; i<DECODER_NUM; i++) {
		//* 1. read data
		file_fd[i] = open(filename[i], O_RDONLY);
		if (file_fd[i] < 0) {
			loge("failed to open input file %s", filename[i]);
			ret = -1;
			goto out;
		}

		//* 2. create and init mpp_decoder
		data->decoder[i] = mpp_decoder_create(dec_type);
		if (!data->decoder[i]) {
			loge("mpp_dec_create failed, i: %d", i);
			ret = -1;
			goto out;
		}

		struct frame_allocator* allocator = open_allocator(data);
		mpp_decoder_control(data->decoder[i], MPP_DEC_INIT_CMD_SET_EXT_FRAME_ALLOCATOR, (void*)allocator);
		mpp_decoder_control(data->decoder[i], MPP_DEC_INIT_CMD_SET_OUTPUT_POS, (void*)&pos[i]);

		struct decode_config config;
		config.bitstream_buffer_size = 512*1024;
		config.extra_frame_num = 1;
		config.packet_count = 10;
		config.pix_fmt = MPP_FMT_NV12;
		ret = mpp_decoder_init(data->decoder[i], &config);
		if (ret) {
			logd("%p mpp_dec_init type %d failed", data->decoder[i], dec_type);
			goto out;
		}
	}


	//* 3. create decode thread
	pthread_create(&decode_thread_id, NULL, decode_thread, data);

	//* 4. create render thread
	pthread_create(&render_thread_id, NULL, render_thread, data);

	//* 5. send data
	for (i=0; i<DECODER_NUM; i++) {
		data->parser[i] = bs_create(file_fd[i]);
	}

	struct mpp_packet packet;
	memset(&packet, 0, sizeof(struct mpp_packet));

	while((packet.flag & PACKET_FLAG_EOS) == 0) {
		for (i=0; i<DECODER_NUM; i++) {
			memset(&packet, 0, sizeof(struct mpp_packet));
			bs_prefetch(data->parser[i], &packet);
			logi("bs_prefetch, size: %d", packet.size);

			// get an empty packet
			do {
				if(data->dec_err) {
					loge("decode error, break now");
					return -1;
				}

				if(data->render_eos) {
					goto eos;
				}
				ret = mpp_decoder_get_packet(data->decoder[i], &packet, packet.size);
				//logd("mpp_dec_get_packet ret: %x", ret);
				if (ret == 0) {
					break;
				}
				usleep(1000);
			} while (1);

			bs_read(data->parser[i], &packet);
			packet.pts = pts;

			ret = mpp_decoder_put_packet(data->decoder[i], &packet);
		}
		pts += 30000; //us
	}
eos:
	for (i=0; i<DECODER_NUM; i++) {
		bs_close(data->parser[i]);
	}

	pthread_join(decode_thread_id, NULL);
	pthread_join(render_thread_id, NULL);

out:
	for (i=0; i<DECODER_NUM; i++) {
		if (data->decoder[i]) {
			mpp_decoder_destory(data->decoder[i]);
			data->decoder[i] = NULL;
		}
	}
	if (buf)
		free(buf);

	for (i=0; i<DECODER_NUM; i++) {
		if(file_fd[i])
			close(file_fd[i]);
	}
	free_framebuffers(data);

	return ret;
}

int main(int argc, char **argv)
{
	int ret = 0;
	int  j;
	int opt;
	int loop_time = 100;
	char path[1024];

	struct dec_ctx dec_data;
	memset(&dec_data, 0, sizeof(struct dec_ctx));
	dec_data.output_format = MPP_FMT_YUV420P;

	while (1) {
		opt = getopt(argc, argv, "h");
		if (opt == -1) {
			break;
		}
		switch (opt) {
		case 'h':
			print_help(argv[0]);
		default:
			goto out;
		}
	}

	for(j=0; j<loop_time; j++) {
		logi("loop: %d", j);
		dec_data.render_eos = 0;
		dec_data.stream_eos = 0;
		dec_data.cmp_data_err = 0;
		dec_data.dec_err = 0;

		memset(dec_data.frame_info, 0, sizeof(struct frame_info)*FRAME_BUF_NUM);
		ret = dec_decode(&dec_data, path);
	}

out:
	return ret;
}
