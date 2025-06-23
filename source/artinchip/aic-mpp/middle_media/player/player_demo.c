/*
 * Copyright (C) 2020-2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: player_demo
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
#include <video/artinchip_fb.h>
#include <sys/ioctl.h>
#define LOG_DEBUG

#include "mpp_dec_type.h"
#include "mpp_list.h"
#include "mpp_log.h"
#include "mpp_mem.h"
#include "aic_message.h"
#include "aic_player.h"

#define PLAYER_DEMO_FILE_MAX_NUM 128
#define PLAYER_DEMO_FILE_PATH_MAX_LEN 256
#define BUFFER_LEN 32
#define SUPPORT_FILE_SUFFIX_TYPE_NUM 8

struct file_list {
	char file_path[PLAYER_DEMO_FILE_MAX_NUM][PLAYER_DEMO_FILE_PATH_MAX_LEN];
	int file_num;
};

struct video_player_ctx {
	struct aic_player *player;
	struct file_list  files;
	struct aic_capture_info	capture_info;
	int volume;
	int loop_time;
	int alpha_value;
	int player_end;
	struct av_media_info media_info;
};

char *file_suffix[SUPPORT_FILE_SUFFIX_TYPE_NUM] = {".h264", ".264", ".mp4", ".mp3", ".avi", ".mkv", ".ts", ".flv"};


static void print_help(const char* prog)
{
	printf("name: %s\n", prog);
	printf("Compile time: %s\n", __TIME__);
	printf("Usage: player_demo [options]:\n"
		"\t-i                             input stream file name\n"
		"\t-t                             directory of test files\n"
		"\t-l                             loop time\n"
		"\t-c                             save capture file path,default /mnt/video/capture.jpg \n"
		"\t-W                             capture widht\n"
		"\t-H                             capture height\n"
		"\t-a                             set ui aplha\n"
		"\t-h                             help\n\n"
		"Example1(test single file for 1 time): player_demo -i /mnt/video/test.mp4 \n"
		"Example2(test single file for 3 times): player_demo -i /mnt/video/test.mp4 -l 3 \n"
		"Example3(test dir for 1 time ) : player_demo -t /mnt/video \n"
		"Example4(test dir for 3 times ): player_demo -t /mnt/video -l 3 \n"
		"---------------------------------------------------------------------------------------\n"
		"-------------------------------control key while playing-------------------------------\n"
		"---------------------------------------------------------------------------------------\n"
		"('d' + Enter): play next \n"
		"('u' + Enter): play previous \n"
		"('p' + Enter): pause/play \n"
		"('+' + Enter): volum+5 \n"
		"('-' + Enter): volum-5 \n"
		"('m' + Enter): enter/eixt mute \n"
		"('c' + Enter): capture pic,firstly,please pause and then capture \n");
}

static int read_dir(char* path, struct file_list *files)
{
	char* ptr = NULL;
	struct dirent* dir_file;
	int i = 0;
	DIR* dir = opendir(path);
	if (dir == NULL) {
		loge("read dir failed");
		return -1;
	}

	while((dir_file = readdir(dir))) {
		if (strcmp(dir_file->d_name, ".") == 0 || strcmp(dir_file->d_name, "..") == 0)
			continue;

		ptr = strrchr(dir_file->d_name, '.');
		if (ptr == NULL)
			continue;
		for (i = 0; i < SUPPORT_FILE_SUFFIX_TYPE_NUM; i++) {
			if (strcmp(ptr, file_suffix[i]) == 0) {
				break;
			}
		}
		if (i == SUPPORT_FILE_SUFFIX_TYPE_NUM)
			continue;

		logd("name: %s", dir_file->d_name);
		strcpy(files->file_path[files->file_num], path);
		strcat(files->file_path[files->file_num], "/");
		strcat(files->file_path[files->file_num], dir_file->d_name);
		logd("i: %d, filename: %s", files->file_num, files->file_path[files->file_num]);
		files->file_num ++;

		if (files->file_num >= PLAYER_DEMO_FILE_MAX_NUM)
			break;
	}
	return 0;
}

s32 event_handle(void* app_data,s32 event,s32 data1,s32 data2)
{
	int ret = 0;
	struct video_player_ctx *ctx = (struct video_player_ctx *)app_data;
	switch (event) {
		case AIC_PLAYER_EVENT_PLAY_END:
			ctx->player_end = 1;
			logd("g_player_end\n");
			break;
		case AIC_PLAYER_EVENT_PLAY_TIME:
			break;
		default:
			break;
	}
	return ret;
}

static int set_volume(struct video_player_ctx *player_ctx,int volume)
{
	struct video_player_ctx *ctx = player_ctx;
	if (volume < 0) {
		volume = 0;
	} else if (volume > 100) {
		volume = 100;
	}
	logd("volume:%d\n",volume);
	return aic_player_set_volum(ctx->player,volume);
}

static int do_seek(struct video_player_ctx *player_ctx,int forward)
{
	s64 pos;
	struct video_player_ctx *ctx = player_ctx;

	pos = aic_player_get_play_time(ctx->player);
	if (pos == -1) {
		loge("aic_player_get_play_time error!!!!\n");
		return -1;
	}
	if (forward == 1) {
		pos += 8*1000*1000;//+8s
	} else {
		pos -= 8*1000*1000;//-8s
	}

	if (pos < 0) {
		pos = 0;
	} else if (pos < ctx->media_info.duration) {

	} else {
		pos = ctx->media_info.duration;
	}

	if (aic_player_seek(ctx->player,pos) != 0) {
		loge("aic_player_seek error!!!!\n");
		return -1;
	}
	logd("aic_player_seek ok\n");
	return 0;
}

static int do_rotation(struct video_player_ctx *player_ctx)
{
	static int index = 0;
	int rotation = MPP_ROTATION_0;
	struct video_player_ctx *ctx = player_ctx;

	if (index % 4 == 0) {
		rotation = MPP_ROTATION_90;
		logd("*********MPP_ROTATION_90***************\n");
	} else if(index % 4 == 1) {
		rotation = MPP_ROTATION_180;
		logd("*********MPP_ROTATION_180***************\n");
	} else if(index % 4 == 2) {
		rotation = MPP_ROTATION_270;
		logd("*********MPP_ROTATION_270***************\n");
	} else if(index % 4 == 3) {
		rotation = MPP_ROTATION_0;
		logd("*********MPP_ROTATION_0***************\n");
	}
	aic_player_set_rotation(ctx->player,rotation);
	index++;
	return 0;
}

static int start_play(struct video_player_ctx *player_ctx)
{
	int ret = -1;
	static struct av_media_info media_info;
	struct mpp_size screen_size;
	struct mpp_rect disp_rect;
	struct video_player_ctx *ctx = player_ctx;

	ret = aic_player_start(ctx->player);
	if (ret != 0) {
		loge("aic_player_start error!!!!\n");
		return -1;
	}
	logd("aic_player_start ok\n");

	ret =  aic_player_get_media_info(ctx->player,&media_info);
	if (ret != 0) {
		loge("aic_player_get_media_info error!!!!\n");
		return -1;
	}
	ctx->media_info = media_info;
	logd("aic_player_get_media_info duration:%"PRId64",file_size:%"PRId64"\n",media_info.duration,media_info.file_size);

	logd("has_audio:%d,has_video:%d,"
		"width:%d,height:%d,\n"
		"bits_per_sample:%d,nb_channel:%d,sample_rate:%d\n"
		,media_info.has_audio
		,media_info.has_video
		,media_info.video_stream.width
		,media_info.video_stream.height
		,media_info.audio_stream.bits_per_sample
		,media_info.audio_stream.nb_channel
		,media_info.audio_stream.sample_rate);

	if (media_info.has_video) {
		ret = aic_player_get_screen_size(ctx->player, &screen_size);
		if (ret != 0) {
			loge("aic_player_get_screen_size error!!!!\n");
			return -1;
		}
		logd("screen_width:%d,screen_height:%d\n",screen_size.width,screen_size.height);
		disp_rect.x = 324;
		disp_rect.y = 50;
		disp_rect.width = 600;
		disp_rect.height = 500;
		ret = aic_player_set_disp_rect(ctx->player, &disp_rect);//attention:disp not exceed screen_size
		if (ret != 0) {
			loge("aic_player_set_disp_rect error\n");
			return -1;
		}
		logd("aic_player_set_disp_rect  ok\n");
	}

	if (media_info.has_audio) {
		ret = set_volume(ctx,ctx->volume);
		if (ret != 0) {
			loge("set_volume error!!!!\n");
			return -1;
		}
	}

	ret = aic_player_play(ctx->player);
	if (ret != 0) {
		loge("aic_player_play error!!!!\n");
		return -1;
	}
	logd("aic_player_play ok\n");

	return 0;
}

static int parse_options(struct video_player_ctx *player_ctx,int cnt,char**options)
{
	int argc = cnt;
	char **argv = options;
	struct video_player_ctx *ctx = player_ctx;
	int opt;

	if (!ctx || argc == 0 || !argv) {
		loge("para error !!!");
		return -1;
	}

	//set default capture_info
	ctx->capture_info.file_path = mpp_alloc(PLAYER_DEMO_FILE_PATH_MAX_LEN);
	if (!ctx->capture_info.file_path) {
		loge("mpp_alloc error !!!");
		return -1;
	}
	memset(ctx->capture_info.file_path,0x00,PLAYER_DEMO_FILE_PATH_MAX_LEN);
	strcpy((char *)ctx->capture_info.file_path,(char *)"/mnt/video/capture.jpg");
	ctx->capture_info.width = 1024;
	ctx->capture_info.height = 600;
	ctx->capture_info.quality = 90;
	//set default alpha_value loop_time  volume
	ctx->alpha_value = 0;
	ctx->loop_time = 1;
	ctx->volume = 65;
	ctx->player_end = 0;

	while (1) {
		opt = getopt(argc, argv, "i:t:l:c:W:H:q:a:h");
		if (opt == -1) {
			break;
		}
		switch (opt) {
		case 'i':
			strcpy(ctx->files.file_path[0], optarg);
			ctx->files.file_num = 1;
			logd("file path: %s", ctx->files.file_path[0]);
			break;
		case 'l':
			ctx->loop_time = atoi(optarg);
			break;
		case 't':
			read_dir(optarg, &ctx->files);
			break;
		case 'c':
			memset(ctx->capture_info.file_path,0x00,PLAYER_DEMO_FILE_PATH_MAX_LEN);
			strncpy((char*)ctx->capture_info.file_path,(char*)optarg,PLAYER_DEMO_FILE_PATH_MAX_LEN);
			logd("file path: %s", ctx->capture_info.file_path);
			break;
		case 'W':
			ctx->capture_info.width = atoi(optarg);
			break;
		case 'H':
			ctx->capture_info.height = atoi(optarg);
			break;
		case 'q':
			ctx->capture_info.quality = atoi(optarg);
			break;
		case 'a':
			ctx->alpha_value = atoi(optarg);
			printf("alpha_value:%d\n",ctx->alpha_value);
			break;
		case 'h':
			print_help(argv[0]);
			if (ctx->capture_info.file_path) {
				mpp_free(ctx->capture_info.file_path);
				ctx->capture_info.file_path = NULL;
			}
			return -1;
		default:
			break;
		}
	}
	if (ctx->files.file_num == 0) {
		print_help(argv[0]);
		if (ctx->capture_info.file_path) {
			mpp_free(ctx->capture_info.file_path);
			ctx->capture_info.file_path = NULL;
		}
		return -1;
	}
	return 0;
}

static int process_command(struct video_player_ctx *player_ctx,char cmd)
{
	struct video_player_ctx *ctx = player_ctx;
	int ret = 0;
	if (cmd == 0x20) {// pause
		ret = aic_player_pause(ctx->player);
	} else if (cmd == '-') {
		ctx->volume -= 5;
		ret = set_volume(ctx,ctx->volume);
	} else if (cmd == '+') {
		ctx->volume += 5;
		ret = set_volume(ctx,ctx->volume);
	} else if (cmd == 'm') {
		ret = aic_player_set_mute(ctx->player);
	} else if (cmd == 'c') {
		ret = aic_player_capture(ctx->player,&ctx->capture_info);
	} else if (cmd == 'f') {
		ret = do_seek(ctx,1);//+8s
	} else if (cmd == 'b') {
		ret = do_seek(ctx,0);//-8s
	} else if (cmd == 'z') {
		ret = aic_player_seek(ctx->player,0);
	} else if (cmd == 'r') {
		ret = do_rotation(ctx);
	}
	return ret;
}

int main(int argc,char*argv[])
{
	int ret = 0;
	int i = 0;
	int j = 0;
	char buffer[BUFFER_LEN];
	int flag;
	static int fd_dev;
	struct aicfb_alpha_config alpha_bak = {0};
	struct aicfb_alpha_config alpha = {0};
	struct video_player_ctx *ctx = NULL;

	ctx = mpp_alloc(sizeof(struct video_player_ctx));
	if (!ctx) {
		loge("mpp_alloc fail!!!");
		return -1;
	}
	memset(ctx,0x00,sizeof(struct video_player_ctx));

	if (parse_options(ctx,argc,argv)) {
		loge("parse_options fail!!!\n");
		goto _EXIT_;
	}

	fd_dev = open("/dev/fb0", O_RDWR);
	if (fd_dev < 0) {
		loge("open /dev/fb0 fail!!!\n");
		goto _EXIT_;
	}
	//stroe alpha
	alpha_bak.layer_id = AICFB_LAYER_TYPE_UI;
	ioctl(fd_dev, AICFB_GET_ALPHA_CONFIG, &alpha_bak);
	//set alpha
	alpha.layer_id = AICFB_LAYER_TYPE_UI;
	alpha.enable = 1;
	alpha.mode = 1;
	alpha.value = ctx->alpha_value;
	ioctl(fd_dev, AICFB_UPDATE_ALPHA_CONFIG, &alpha);

	ctx->player = aic_player_create(NULL);
	if (ctx->player == NULL) {
		loge("aic_player_create fail!!!\n");
		goto _EXIT_;
	}
	aic_player_set_event_callback(ctx->player,ctx,event_handle);

	flag = fcntl(STDIN_FILENO,F_GETFL);
	flag |= O_NONBLOCK;
	fcntl(STDIN_FILENO,F_SETFL,flag);

	for(i = 0;i < ctx->loop_time; i++) {
		for(j = 0; j < ctx->files.file_num; j++) {
			logd("loop:%d,index:%d,path:%s\n",i,j,ctx->files.file_path[j]);
			ctx->player_end = 0;
			if (aic_player_set_uri(ctx->player,ctx->files.file_path[j])) {
				loge("aic_player_prepare error!!!!\n");
				aic_player_stop(ctx->player);
				continue;
			}

			if (aic_player_prepare_sync(ctx->player)) {
				loge("aic_player_prepare error!!!!\n");
				aic_player_stop(ctx->player);
				continue;
			}

			if (start_play(ctx) != 0) {
				loge("start_play error!!!!\n");
				aic_player_stop(ctx->player);
				continue;
			}

			while(1) {
				if (ctx->player_end == 1) {
					logd("play file:%s end!!!!\n",ctx->files.file_path[j]);
					ret = aic_player_stop(ctx->player);
					ctx->player_end = 0;
					break;
				}
				if (read(STDIN_FILENO, buffer, BUFFER_LEN) > 0) {
					if (buffer[0] == 'u') {// up
						j -= 2;
						j = (j < -1)?(-1):(j);
						aic_player_stop(ctx->player);
						break;
					} else if (buffer[0] == 'd') {// down
						aic_player_stop(ctx->player);
						break;
					} else if (buffer[0] == 'e') {// end
						aic_player_stop(ctx->player);
						goto _EXIT_;
					}
					process_command(ctx,buffer[0]);
				} else {
					usleep(1000*1000);
				}
			}
		}
	}

_EXIT_:
	if (ctx->player) {
		aic_player_destroy(ctx->player);
		ctx->player = NULL;
	}
	if (ctx->capture_info.file_path) {
		mpp_free(ctx->capture_info.file_path);
		ctx->capture_info.file_path = NULL;
	}
	if (fd_dev > 0) {
		//restore alpha
		ioctl(fd_dev, AICFB_UPDATE_ALPHA_CONFIG, &alpha_bak);
		close(fd_dev);
	}
	mpp_free(ctx);
	ctx = NULL;
	return ret;
}
