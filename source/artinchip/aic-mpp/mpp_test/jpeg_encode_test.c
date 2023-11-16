/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <qi.xu@artinchip.com>
*  Desc: jpeg encoder demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

#include "dma_allocator.h"
#include "mpp_encoder.h"
#include "mpp_log.h"

static void print_help(void)
{
	printf("Usage: dec_test [OPTIONS] [SLICES PATH]\n\n"
		"Options:\n"
		" -i                             input stream file name\n"
		" -q				 set quality (value range: 0~100)\n"
		" -w				 width of input yuv data\n"
		" -g				 height of input yuv data\n"
		" -h                             help\n\n"
		"End:\n");
}

int main(int argc, char **argv)
{
	int i;
	int opt;
	char file_name[1024];
	int quality = 90;

	int width = 176;
	int height = 144;

	while (1) {
		opt = getopt(argc, argv, "i:q:w:g:h");
		if (opt == -1) {
			break;
		}
		switch (opt) {
		case 'i':
			logd("file path: %s", optarg);
			strcpy(file_name, optarg);
			break;
		case 'w':
			width = atoi(optarg);
			break;
		case 'g':
			height = atoi(optarg);
			break;
		case 'q':
			quality = atoi(optarg);
			break;
		case 'h':
		default:
			print_help();
			return -1;
		}
	}

	FILE* fp = fopen(file_name, "rb");
	int dmabuf_fd[3];
	unsigned char* vir_addr[3];
	int size[3] = {width*height, width*height/4, width*height/4};
	int dma_fd = dmabuf_device_open();

	for (i=0; i<3; i++) {
		dmabuf_fd[i] = dmabuf_alloc(dma_fd, size[i]);
		vir_addr[i] = dmabuf_mmap(dmabuf_fd[i], size[i]);
		fread(vir_addr[i], 1, size[i], fp);

		dmabuf_sync(dmabuf_fd[i], CACHE_CLEAN);
		dmabuf_munmap(vir_addr[i], size[i]);
	}
	fclose(fp);

	struct mpp_frame frame;
	memset(&frame, 0, sizeof(struct mpp_frame));
	frame.buf.fd[0] = dmabuf_fd[0];
	frame.buf.fd[1] = dmabuf_fd[1];
	frame.buf.fd[2] = dmabuf_fd[2];
	frame.buf.size.width = width;
	frame.buf.size.height = height;
	frame.buf.stride[0] = width;
	frame.buf.stride[1] = frame.buf.stride[2] = width/2;
	frame.buf.format = MPP_FMT_YUV420P;

	int len = 0;
	int buf_len = width * height * 4/5 * quality / 100;
	int jpeg_data_fd = dmabuf_alloc(dma_fd, buf_len);
	unsigned char* jpeg_vir_addr = dmabuf_mmap(jpeg_data_fd, buf_len);

	if (mpp_encode_jpeg(&frame, quality, jpeg_data_fd, buf_len, &len) < 0) {
		loge("encode failed");
		goto out;
	}

	logi("jpeg encode len: %d", len);
	FILE* fp_save = fopen("/save.jpg", "wb");
	fwrite(jpeg_vir_addr, 1, len, fp_save);
	fclose(fp_save);

out:
	dmabuf_munmap(jpeg_vir_addr, buf_len);
	dmabuf_free(jpeg_data_fd);
	dmabuf_free(dmabuf_fd[0]);
	dmabuf_free(dmabuf_fd[1]);
	dmabuf_free(dmabuf_fd[2]);
	dmabuf_device_close(dma_fd);

	return 0;
}
