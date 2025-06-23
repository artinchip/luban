/*
 * Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <qi.xu@artinchip.com>
 *  Desc: video decode demo
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
#include "bit_stream_parser.h"
#include "mpp_decoder.h"
#include "mpp_encoder.h"
#include "mpp_log.h"

#define FRAME_BUF_NUM		(18)
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
	struct mpp_decoder  *decoder;
	struct frame_info frame_info[FRAME_BUF_NUM];	//

	struct bit_stream_parser *parser;

	int stream_eos;
	int render_eos;
	int dec_err;
	int cmp_data_err;

	char file_input[MAX_TEST_FILE][1024];	// test file name
	int file_num;				// test file number

	int output_format;
	int cmp_en;
	int display_en;
	int save_data;
	FILE* fp_yuv;				// compare yuv
	FILE* fp_save;				// hw decode save yuv
	FILE* fp_result;			// test result (pass/fail)
};

static void print_help(const char* prog)
{
	printf("name: %s\n", prog);
	printf("Compile time: %s\n", __TIME__);
	printf("Usage: mpp_test [options]:\n"
		"\t-i                             input stream file name\n"
		"\t-t                             directory of test files\n"
		"\t-d                             enable display error picture\n"
		"\t-c                             enable compare output data\n"
		"\t-f                             output pixel format\n"
		"\t-l                             loop time\n"
		"\t-s                             save output data\n"
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

	logi("width: %d, height %d, stride: %d, %d, crop_en: %d, crop_w: %d, crop_h: %d",
		layer.buf.size.width, layer.buf.size.height,
		layer.buf.stride[0], layer.buf.stride[1], layer.buf.crop_en,
		layer.buf.crop.width, layer.buf.crop.height);
	//* display
	if (ioctl(fb0_fd, AICFB_UPDATE_LAYER_CONFIG, &layer) < 0)
		loge("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");

	//* wait vsync (wait layer config)
	ioctl(fb0_fd, AICFB_WAIT_FOR_VSYNC, NULL);
}

static int send_data(struct dec_ctx *data, unsigned char* buf, int buf_size)
{
	int ret = 0;
	struct mpp_packet packet;
	memset(&packet, 0, sizeof(struct mpp_packet));

	// get an empty packet
	do {
		if(data->dec_err) {
			loge("decode error, break now");
			return -1;
		}

		ret = mpp_decoder_get_packet(data->decoder, &packet, buf_size);
		//logd("mpp_dec_get_packet ret: %x", ret);
		if (ret == 0) {
			break;
		}
		usleep(1000);
	} while (1);

	memcpy(packet.data, buf, buf_size);
	packet.size = buf_size;

	if (data->stream_eos)
		packet.flag |= PACKET_FLAG_EOS;

	ret = mpp_decoder_put_packet(data->decoder, &packet);
	logd("mpp_dec_put_packet ret %d", ret);

	return ret;
}

static int cmp_data(struct dec_ctx *data, FILE* fp, struct mpp_buf* video, FILE* fp_save)
{
	int i, j;
	int ret = 0;
	int data_size[3] = {0, 0, 0};
	unsigned char* hw_data[3] = {0};
	int comp = 3;

	if(video->format == MPP_FMT_YUV420P) {
		comp = 3;
		data_size[0] = video->size.height * video->stride[0];
		data_size[1] = data_size[2] = data_size[0]/4;
	} else if(video->format == MPP_FMT_NV12 || video->format == MPP_FMT_NV21) {
		comp = 2;
		data_size[0] = video->size.height * video->stride[0];
		data_size[1] =  data_size[0]/2;
	} else if(video->format == MPP_FMT_YUV444P) {
		comp = 3;
		data_size[0] = video->size.height * video->stride[0];
		data_size[1] = data_size[2] = data_size[0];
	} else if(video->format == MPP_FMT_YUV422P) {
		comp = 3;
		data_size[0] = video->size.height * video->stride[0];
		data_size[1] = data_size[2] = data_size[0]/2;
	} else if(video->format == MPP_FMT_RGBA_8888 || video->format == MPP_FMT_BGRA_8888
		|| video->format == MPP_FMT_ARGB_8888 || video->format == MPP_FMT_ABGR_8888) {
		comp = 1;
		data_size[0] = video->size.height * video->stride[0];
	} else if(video->format == MPP_FMT_RGB_888 || video->format == MPP_FMT_BGR_888) {
		comp = 1;
		data_size[0] = video->size.height * video->stride[0];
	} else if(video->format == MPP_FMT_RGB_565 || video->format == MPP_FMT_BGR_565) {
		comp = 1;
		data_size[0] = video->size.height * video->stride[0];
	}

	logd("data_size: %d %d %d, height: %d, stride: %d, format: %d",
		data_size[0], data_size[1], data_size[2],
		video->size.height, video->stride[0], video->format);

	// mmap dmabuf to virtual space and save the frame yuv data
	for(i=0; i<comp; i++) {
		hw_data[i] = mmap(NULL, data_size[i], PROT_READ, MAP_SHARED, video->fd[i], 0);
		if (hw_data[i] == MAP_FAILED) {
			loge("dmabuf alloc mmap failed!");
			return -1;
		}
		if(fp_save)
			fwrite(hw_data[i], 1, data_size[i], fp_save);
	}

	unsigned char* buf[3] = {0};
	if(fp && data->cmp_en) {
		// read data from compare file
		for(i=0; i<comp; i++) {
			buf[i] = (unsigned char*)malloc(data_size[i]);
			fread(buf[i], 1, data_size[i], fp);
		}

		// compare hw decode data
		for(i=0; i<comp; i++) {
			for(j=0; j<data_size[i]; j++) {
				if(abs(buf[i][j] - hw_data[i][j]) < 2)
					continue;

				ret = -1;
				loge("comp(%d) error, pos: %d, sw: %x, hw: %x",
					i, j,  buf[i][j], hw_data[i][j]);
				goto out;
			}
		}
		logi("compare data success");
	} else {
		logw("not compare data");
		ret = -1;
	}

out:
	// unmap dmabuf
	for(i=0; i<comp; i++) {
		munmap(hw_data[i], data_size[i]);
	}

	if(buf[0]) free(buf[0]);
	if(buf[1]) free(buf[1]);
	if(buf[2]) free(buf[2]);

	return ret;
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

	int first = 0;

	//* 1. open fb0
	fb0_fd = open(dev_fb0, O_RDWR);
	if (fb0_fd < 0) {
		logw("open fb0 failed!");
	}

	time = get_now_us();
	//* 2. render frame until eos
	while(!data->render_eos) {
		memset(&frame[cur_frame_id], 0, sizeof(struct mpp_frame));

		if(data->dec_err)
			break;

		//* 2.1 get frame
		ret = mpp_decoder_get_frame(data->decoder, &frame[cur_frame_id]);
		if(ret == DEC_NO_RENDER_FRAME || ret == DEC_ERR_FM_NOT_CREATE
		|| ret == DEC_NO_EMPTY_FRAME) {
			usleep(10000);
			continue;
		} else if(ret) {
			logw("mpp_dec_get_frame error, ret: %x", ret);
			data->dec_err = 1;
			break;
		}

		time = get_now_us() - time;
		duration_time += time;
		disp_frame_cnt += 1;
		data->render_eos = frame[cur_frame_id].flags & FRAME_FLAG_EOS;
		logi("decode_get_frame successful: frame id %d, number %d, flag: %d",
			frame[cur_frame_id].id, frame_num, frame[cur_frame_id].flags);

		if (frame[cur_frame_id].flags & FRAME_FLAG_ERROR) {
			loge("frame error");
		}
		pic_buffer = &frame[cur_frame_id].buf;
		//* 2.2 compare data
		if((data->cmp_en || data->fp_save) && cmp_data(data, data->fp_yuv, pic_buffer, data->fp_save))
			data->cmp_data_err = 1;

		if(!first) {
			int len = 0;
			int buf_len = 1024*1024;
			int dma_fd = dmabuf_device_open();
			int jpeg_data_fd = dmabuf_alloc(dma_fd, buf_len);
			unsigned char* jpeg_vir_addr = dmabuf_mmap(jpeg_data_fd, buf_len);
			mpp_encode_jpeg(&frame[cur_frame_id], 90, jpeg_data_fd, buf_len, &len);
			logi("encode jpeg len: %d", len);
			FILE* fp_jpeg = fopen("/save.jpg", "wb");
			fwrite(jpeg_vir_addr, 1, len, fp_jpeg);
			fclose(fp_jpeg);
			dmabuf_munmap(jpeg_vir_addr, buf_len);
			dmabuf_free(jpeg_data_fd);
			dmabuf_device_close(dma_fd);
			first ++;
		}

		//* 2.3 disp frame;
		if(data->display_en || !(frame[cur_frame_id].flags & FRAME_FLAG_ERROR)) {
			set_fb_layer_alpha(fb0_fd, 10);
			video_layer_set(fb0_fd, pic_buffer, &data->frame_info[frame[cur_frame_id].id]);
		}

		//* 2.4 return the last frame
		if(frame_num) {
			ret = mpp_decoder_put_frame(data->decoder, &frame[last_frame_id]);
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
		usleep(30000);
	}

	//* put the last frame when eos
	mpp_decoder_put_frame(data->decoder, &frame[last_frame_id]);

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

	return NULL;
}

void* decode_thread(void *p)
{
	struct dec_ctx *data = (struct dec_ctx*)p;
	int ret = 0;

	int dec_num = 0;
	//while(!dec_num) {
	while(!data->render_eos) {
		ret = mpp_decoder_decode(data->decoder);
		if(ret == DEC_NO_READY_PACKET || ret == DEC_NO_EMPTY_FRAME) {
			usleep(1000);
			continue;
		} else if( ret ) {
			logw("decode ret: %x", ret);
			//data->dec_err = 1;
			//break;
		}
		dec_num ++;
		usleep(1000);
	}

	return NULL;
}

int dec_decode(struct dec_ctx *data, char* filename)
{
	int ret;
	int file_fd;
	unsigned char *buf = NULL;
	size_t buf_size = 0;
	pthread_t render_thread_id;
	pthread_t decode_thread_id;
	int dec_type = 0;
	char yuv_file_name[1024];

	logd("dec_test start");

	if (filename) {
		char* ptr = strrchr(filename, '.');
		if (!strcmp(ptr, ".h264") || !strcmp(ptr, ".264")) {
			dec_type = MPP_CODEC_VIDEO_DECODER_H264;
		} else if (!strcmp(ptr, ".jpg")) {
			dec_type = MPP_CODEC_VIDEO_DECODER_MJPEG;
		} else if (!strcmp(ptr, ".png")) {
			dec_type = MPP_CODEC_VIDEO_DECODER_PNG;
		}
		logi("file type: 0x%02X", dec_type);

		strcpy(yuv_file_name, filename);
		ptr = strrchr(yuv_file_name, '.');
		ptr[1] = 'y';
		ptr[2] = 'u';
		ptr[3] = 'v';
		ptr[4] = '\0';
		logi("yuv file name: %s", yuv_file_name);
		data->fp_yuv = fopen(yuv_file_name, "rb");
		if(data->fp_yuv == NULL)
			logi("dec_data.fp_yuv open failed, erron(%d)", errno);
	}

	//* 1. read data
	file_fd = open(filename, O_RDONLY);
	if (file_fd < 0) {
		loge("failed to open input file %s", filename);
		ret = -1;
		goto out;
	}
	buf_size = lseek(file_fd, 0, SEEK_END);
	lseek(file_fd, 0, SEEK_SET);

	//* 2. create and init mpp_decoder
	data->decoder = mpp_decoder_create(dec_type);
	if (!data->decoder) {
		loge("mpp_dec_create failed");
		ret = -1;
		goto out;
	}

	struct decode_config config;
	if(dec_type == MPP_CODEC_VIDEO_DECODER_PNG || dec_type == MPP_CODEC_VIDEO_DECODER_MJPEG)
		config.bitstream_buffer_size = (buf_size + 1023) & (~1023);
	else
		config.bitstream_buffer_size = 1024*1024;
	config.extra_frame_num = 1;
	config.packet_count = 10;
	config.pix_fmt = data->output_format;
	if(dec_type == MPP_CODEC_VIDEO_DECODER_PNG)
		config.pix_fmt = MPP_FMT_ARGB_8888;
	ret = mpp_decoder_init(data->decoder, &config);
	if (ret) {
		logd("%p mpp_dec_init type %d failed", data->decoder, dec_type);
		goto out;
	}

	//* 3. create decode thread
	pthread_create(&decode_thread_id, NULL, decode_thread, data);

	//* 4. create render thread
	pthread_create(&render_thread_id, NULL, render_thread, data);

	//* 5. send data
	if (dec_type == MPP_CODEC_VIDEO_DECODER_H264) {
		data->parser = bs_create(file_fd);
		struct mpp_packet packet;
		memset(&packet, 0, sizeof(struct mpp_packet));

		while((packet.flag & PACKET_FLAG_EOS) == 0) {
			memset(&packet, 0, sizeof(struct mpp_packet));
			bs_prefetch(data->parser, &packet);
			logi("bs_prefetch, size: %d", packet.size);

			// get an empty packet
			do {
				if(data->dec_err) {
					loge("decode error, break now");
					return -1;
				}

				ret = mpp_decoder_get_packet(data->decoder, &packet, packet.size);
				//logd("mpp_dec_get_packet ret: %x", ret);
				if (ret == 0) {
					break;
				}
				usleep(1000);
			} while (1);


			bs_read(data->parser, &packet);
			unsigned char* buf = (unsigned char*)packet.data;
			logd("packet: %p, size: %d, %x %x %x %x", packet.data, packet.size, buf[0], buf[1],
			        buf[2], buf[3]);

			ret = mpp_decoder_put_packet(data->decoder, &packet);
		}

		bs_close(data->parser);
	} else {
		buf = (unsigned char *)malloc(buf_size);
		if (!buf) {
			loge("malloc buf failed");
			ret = -1;
			goto out;
		}

		if(read(file_fd, buf, buf_size) <= 0) {
			loge("read data error");
			data->stream_eos = 1;
			ret = -1;
			goto out;
		}

		data->stream_eos = 1;
		ret = send_data(data, buf, buf_size);
	}

	pthread_join(decode_thread_id, NULL);
	pthread_join(render_thread_id, NULL);
	if(data->cmp_data_err)
		ret = -1;

out:
	if(data->fp_yuv == NULL) {
		fprintf(data->fp_result, "%s: not compare data\n", filename);
	} else if(ret < 0) {
		fprintf(data->fp_result, "%s: fail\n", filename);
	} else {
		fprintf(data->fp_result, "%s: pass\n", filename);
	}
	fflush(data->fp_result);

	if (data->decoder) {
		mpp_decoder_destory(data->decoder);
		data->decoder = NULL;
	}

	if(data->fp_yuv) {
		fclose(data->fp_yuv);
		data->fp_yuv = NULL;
	}

	if (buf)
		free(buf);
	if(file_fd)
		close(file_fd);

	return ret;
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

		if (strcmp(ptr, ".h264") && strcmp(ptr, ".264") && strcmp(ptr, ".png") && strcmp(ptr, ".jpg"))
			continue;

		logi("name: %s", dir_file->d_name);
		strcpy(dec_data->file_input[dec_data->file_num], path);
		strcat(dec_data->file_input[dec_data->file_num], dir_file->d_name);
		logi("i: %d, filename: %s", dec_data->file_num, dec_data->file_input[dec_data->file_num]);
		dec_data->file_num ++;

		if(dec_data->file_num >= MAX_TEST_FILE)
			break;
	}

	return 0;
}

int main(int argc, char **argv)
{
	int ret = 0;
	int i, j;
	int opt;
	int loop_time = 1;

	struct dec_ctx dec_data;
	memset(&dec_data, 0, sizeof(struct dec_ctx));
	dec_data.output_format = MPP_FMT_YUV420P;

	while (1) {
		opt = getopt(argc, argv, "i:t:f:l:dhsc");
		if (opt == -1) {
			break;
		}
		switch (opt) {
		case 'i':
			strcpy(dec_data.file_input[0], optarg);
			dec_data.file_num = 1;
			logd("file path: %s", dec_data.file_input[0]);

			break;
		case 'f':
			if (!strcmp(optarg, "nv12")) {
				logi("output format nv12");
				dec_data.output_format = MPP_FMT_NV12;
			} else if(!strcmp(optarg, "nv21")) {
				logi("output format nv21");
				dec_data.output_format = MPP_FMT_NV21;
			} else if(!strcmp(optarg, "yuv420")) {
				logi("output format yuv420");
				dec_data.output_format = MPP_FMT_YUV420P;
			}
			break;
		case 'l':
			loop_time = atoi(optarg);
			break;
		case 't':
			read_dir(optarg, &dec_data);
			break;
		case 'd':
			dec_data.display_en = 1;
			break;
		case 'c':
			dec_data.cmp_en = 1;
			break;
		case 's':
			dec_data.save_data = 1;
			break;
		case 'h':
			print_help(argv[0]);
		default:
			goto out;
		}
	}

	if(dec_data.file_num == 0) {
		print_help(argv[0]);
		ret = -1;
		goto out;
	}

	dec_data.fp_result = fopen("result.txt", "wb");
	if(dec_data.fp_result) {
		logw("file result open failed");
	}

	if(dec_data.save_data) {
		dec_data.fp_save = fopen("save.bin", "wb");
		if(dec_data.fp_save == NULL)
			loge("dec_data.fp_save open failed, erron(%d)", errno);
	}

	for(j=0; j<loop_time; j++) {
		logi("loop: %d", j);
		for(i=0; i<dec_data.file_num; i++) {
			dec_data.render_eos = 0;
			dec_data.stream_eos = 0;
			dec_data.cmp_data_err = 0;
			dec_data.dec_err = 0;

			memset(dec_data.frame_info, 0, sizeof(struct frame_info)*FRAME_BUF_NUM);
			ret = dec_decode(&dec_data, dec_data.file_input[i]);
			if (0 == ret)
				logi("test successful!");
			else
				logw("test failed! ret %d", ret);
		}
	}

out:
	if(dec_data.fp_save)
		fclose(dec_data.fp_save);
	if(dec_data.fp_result)
		fclose(dec_data.fp_result);

	return ret;
}
