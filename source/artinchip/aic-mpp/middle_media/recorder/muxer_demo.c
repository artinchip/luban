/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: OMX_VdecComponent tunneld  OMX_VideoRenderComponent demo
*/



#include <string.h>
#include <malloc.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <dirent.h>
#include <inttypes.h>

#define LOG_DEBUG

#include "mpp_dec_type.h"
#include "mpp_list.h"
#include "mpp_log.h"
#include "mpp_mem.h"
#include "aic_muxer.h"

static void print_help(const char* prog)
{
	printf("Compile time: %s\n", __TIME__);
	printf("Usage: %s [options]:\n"
		"\t-i                             input file (mjpeg)\n"
		"\t-o                             output file(.mp4) \n"
		"\t-W                             mjpeg widht\n"
		"\t-H                             mjpeg height\n"
		"\t-h                             help\n\n"
		"Example  %s -i /mnt/test.mjpeg  -o /mnt/test.mp4 -W 1280 -H 720 \n",prog,prog);
}

/*
	test file struct
	pkt_size  pkt_data   ...... max_pkt_size
	 4 byte   pkt_size   ......   4 byte
*/

int read_packet(int fd,struct aic_av_packet* pkt)
{
	int len = 0;
	int size = 0;
	static int pkt_index = 0;
	//read data size
	len = read(fd,&size,sizeof(size));
	pkt->size  = size;
	pkt->duration  = 40;
	pkt->pts = pkt->dts = (pkt_index++)*40;
	//read data
	len = read(fd,pkt->data,size);
	//logd("len:%d,size:%d",len,size);

	return len;
}

int read_max_packet_size(int fd)
{
	int size = 0;
	lseek(fd, -4, SEEK_END);
	read(fd,&size,sizeof(size));
	lseek(fd, 0, SEEK_SET);
	return size;
}


int main(int argc,char *argv[])
{
	struct aic_muxer *muxer = NULL;
	struct aic_av_media_info media_info;
	struct aic_av_packet packet;
	int fd = 0;
	int opt;
	char in_path[256] = {0};
	char out_path[256] = {0};
	int width = 0;
	int height = 0;
	int size = 0;

	optind = 0;
	while (1) {
		opt = getopt(argc, argv, "i:o:W:H:h");
		if (opt == -1) {
			break;
		}
		switch (opt) {
		case 'i':
			strcpy(in_path, optarg);
			logd("in_path: %s", in_path);
			break;
		case 'o':
			strcpy(out_path, optarg);
			logd("out_path: %s", out_path);
			break;
		case 'W':
			width = atoi(optarg);
			break;
		case 'H':
			height = atoi(optarg);
			break;
		case 'h':
			print_help(argv[0]);
			break;
		default:
			break;
		}
	}

	if (strlen(out_path) == 0 || strlen(in_path) == 0 || width == 0 || height== 0) {
		loge("param error");
		print_help(argv[0]);
		return -1;
	}

	aic_muxer_create((unsigned char *)out_path, &muxer,AIC_MUXER_TYPE_MP4);
	if (muxer == NULL) {
		loge("aic_muxer_create error");
		return -1;
	}

	fd = open(in_path, O_RDONLY);
	if (fd < 0) {
		loge("open error");
		goto _EXIT;
	}

	size = read_max_packet_size(fd);
	logd("max size:%d",size);
	packet.data = mpp_alloc(size);
	packet.type = MPP_MEDIA_TYPE_VIDEO;
	if(!packet.data) {
		loge("mpp_alloc error");
		goto _EXIT;
	}

	// 1 media_info
	media_info.has_audio = 0;
	media_info.has_video = 1;
	media_info.video_stream.codec_type = MPP_CODEC_VIDEO_DECODER_MJPEG;
	media_info.video_stream.width = width;
	media_info.video_stream.height = height;
	media_info.video_stream.frame_rate = 25;
	media_info.video_stream.extra_data_size = 0;
	media_info.video_stream.extra_data = NULL;
	//media_info.video_stream.bit_rate =

	// 2 init & write header
	aic_muxer_init(muxer,&media_info);
	aic_muxer_write_header(muxer);

	//3  read packet from file
	while(read_packet(fd,&packet) > 0) {
		aic_muxer_write_packet(muxer,&packet);
	}

	//4  write trailer
	aic_muxer_write_trailer(muxer);
	logd("aic_muxer_write_trailer\n");

	//5 destroy
	aic_muxer_destroy(muxer);
	logd("aic_muxer_destroy\n");

	mpp_free(packet.data);
	logd("mpp_free\n");

	close(fd);
	return 0;
_EXIT:
	if (muxer) {
		aic_muxer_destroy(muxer);
	}
	if (fd > 0) {
		close(fd);
	}
}
