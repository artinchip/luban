/*
 * Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <qi.xu@artinchip.com>
 *  Desc: video decode demo
 */

#define LOG_TAG "mpp_video_test"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>

#include "bit_stream_parser.h"
#include "mpp_video.h"
#include "mpp_log.h"


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

int main(int argc, char **argv)
{
	int ret = 0;
	int opt;
	char file_name[128];

	while (1) {
		opt = getopt(argc, argv, "i:h");
		if (opt == -1) {
			break;
		}
		switch (opt) {
		case 'i':
			strcpy(file_name, optarg);

			break;

		case 'h':
			print_help(argv[0]);
			return -1;
		}
	}

	// 1. read data
	int file_fd = open(file_name, O_RDONLY);
	if (file_fd < 0) {
		loge("failed to open input file %s", file_name);
		return -1;
	}
	lseek(file_fd, 0, SEEK_SET);

	struct mpp_video* video = mpp_video_create();

	struct decode_config config;
	config.bitstream_buffer_size = 1024*1024;
	config.extra_frame_num = 1;
	config.packet_count = 10;
	config.pix_fmt = MPP_FMT_YUV420P;
	mpp_video_init(video, MPP_CODEC_VIDEO_DECODER_H264, &config);

	struct mpp_rect disp_win;
	disp_win.x = 100;
	disp_win.y = 200;
	disp_win.width = 320;
	disp_win.height = 240;
	mpp_video_set_disp_window(video, &disp_win);
	mpp_video_start(video);

	struct bit_stream_parser*  parser = bs_create(file_fd);

	struct mpp_packet packet;
	memset(&packet, 0, sizeof(struct mpp_packet));
	packet.data = malloc(1024*1024);
	while((packet.flag & PACKET_FLAG_EOS) == 0) {
		bs_prefetch(parser, &packet);
		bs_read(parser, &packet);
		unsigned char* buf = (unsigned char*)packet.data;
		logd("packet: %p, size: %d, %x %x %x %x", packet.data, packet.size, buf[0], buf[1],
			buf[2], buf[3]);

		while (1) {
			ret = mpp_video_send_packet(video, &packet, 2000);
			if (!ret)
				break;

			loge("send packet timeout");
		}
	}

	bs_close(parser);
	free(packet.data);

	mpp_video_stop(video);
	mpp_video_destroy(video);

	return ret;
}
