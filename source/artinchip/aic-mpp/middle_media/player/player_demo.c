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
#include "aic_message.h"
#include "aic_player.h"

#define PLAYER_DEMO_FILE_MAX_NUM 128
#define PLAYER_DEMO_FILE_PATH_MAX_LEN 256
#define BUFFER_LEN 32

static int g_player_end = 0;
static int g_demuxer_detected_flag = 0;
static int g_sync_flag = AIC_PLAYER_PREPARE_SYNC;
static struct av_media_info g_media_info;
struct file_list {
	char file_path[PLAYER_DEMO_FILE_MAX_NUM][PLAYER_DEMO_FILE_PATH_MAX_LEN];
	int file_num;
};

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
		"\t-h                             help\n\n"
		"Example1(test single file for 1 time): player_demo -i /mnt/video/test.mp4 \n"
		"Example2(test single file for 3 times): player_demo -i /mnt/video/test.mp4 -l 3 \n"
		"Example3(test some files for 1 time ) : player_demo -t /mnt/video \n"
		"Example4(test some file for 3 times ): player_demo -i /mnt/video -l 3 \n"
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

		if (strcmp(ptr, ".h264") && strcmp(ptr, ".264") && strcmp(ptr, ".mp4") )
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

s32 event_handle( void* app_data,s32 event,s32 data1,s32 data2)
{
	int ret = 0;
	switch(event) {
		case AIC_PLAYER_EVENT_PLAY_END:
			g_player_end = 1;
			logd("g_player_end\n");
			break;
		case AIC_PLAYER_EVENT_PLAY_TIME:
			break;
		case AIC_PLAYER_EVENT_DEMUXER_FORMAT_DETECTED:
			if (AIC_PLAYER_PREPARE_ASYNC == g_sync_flag) {
				g_demuxer_detected_flag = 1;
				logd("AIC_PLAYER_EVENT_DEMUXER_FORMAT_DETECTED\n");
			}
			break;

		case AIC_PLAYER_EVENT_DEMUXER_FORMAT_NOT_DETECTED:
			if (AIC_PLAYER_PREPARE_ASYNC == g_sync_flag) {
				logd("AIC_PLAYER_EVENT_DEMUXER_FORMAT_NOT_DETECTED\n");
				logd("cur file format not detected,play next file!!!!!!\n");
				g_player_end = 1;
			}

			break;
		default:
			break;
	}
	return ret;
}

static int set_volume(struct aic_player *player,int volume)
{
	if (volume < 0) {
		volume = 0;
	}
	else if (volume < 101) {

	}
	else {
		volume = 100;
	}
	logd("volume:%d\n",volume);
	return aic_player_set_volum(player,volume);
}

static int do_seek(struct aic_player *player,int forward)
{
	s64 pos;
	pos = aic_player_get_play_time(player);
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
	} else if (pos < g_media_info.duration) {

	} else {
		pos = g_media_info.duration;
	}

	if (aic_player_seek(player,pos) != 0) {
		loge("aic_player_seek error!!!!\n");
		return -1;
	}
	logd("aic_player_seek ok\n");
	return 0;
}

static int do_rotation(struct aic_player *player)
{
	static int index = 0;
	int rotation = MPP_ROTATION_0;

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
	aic_player_set_rotation(player,rotation);
	index++;
	return 0;
}

static int start_play(struct aic_player *player,int volume)
{
	int ret = -1;
	static struct av_media_info media_info;
	struct mpp_size screen_size;
	struct mpp_rect disp_rect;

	ret = aic_player_start(player);
	if (ret != 0) {
		loge("aic_player_start error!!!!\n");
		return -1;
	}
	logd("aic_player_start ok\n");

	ret =  aic_player_get_media_info(player,&media_info);
	if (ret != 0) {
		loge("aic_player_get_media_info error!!!!\n");
		return -1;
	}
	g_media_info = media_info;
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
		ret = aic_player_get_screen_size(player, &screen_size);
		if (ret != 0) {
			loge("aic_player_get_screen_size error!!!!\n");
			return -1;
		}
		logd("screen_width:%d,screen_height:%d\n",screen_size.width,screen_size.height);
		disp_rect.x = 324;
		disp_rect.y = 50;
		disp_rect.width = 600;
		disp_rect.height = 500;
		ret = aic_player_set_disp_rect(player, &disp_rect);//attention:disp not exceed screen_size
		if (ret != 0) {
			loge("aic_player_set_disp_rect error\n");
			return -1;
		}
		logd("aic_player_set_disp_rect  ok\n");
	}

	if (media_info.has_audio) {
		ret = set_volume(player,volume);
		if (ret != 0) {
			loge("set_volume error!!!!\n");
			return -1;
		}
	}

	ret = aic_player_play(player);
	if (ret != 0) {
		loge("aic_player_play error!!!!\n");
		return -1;
	}
	logd("aic_player_play ok\n");

	return 0;
}

int main(int argc,char*argv[])
{
	int ret = 0;
	int i = 0;
	int j = 0;
	char buffer[BUFFER_LEN];
	int flag;
	int opt;
	int loop_time = 1;
	struct file_list  files;
	struct aic_player *player = NULL;
	int volume = 50;
	struct aic_capture_info   capture_info;
	char file_path[255] = {"/mnt/video/capture.jpg"} ;

	//default capture_info
	capture_info.file_path = (s8 *)file_path;
	capture_info.width = 1024;
	capture_info.height = 600;
	capture_info.quality = 90;
	memset(&files,0x00,sizeof(struct file_list));
	while (1) {
		opt = getopt(argc, argv, "i:t:l:c:W:H:q:h");
		if (opt == -1) {
			break;
		}
		switch (opt) {
		case 'i':
			strcpy(files.file_path[0], optarg);
			files.file_num = 1;
			logd("file path: %s", files.file_path[0]);
			break;
		case 'l':
			loop_time = atoi(optarg);
			break;
		case 't':
			read_dir(optarg, &files);
			break;
		case 'c':
			memset(file_path,0x00,sizeof(file_path));
			strncpy(file_path, optarg,sizeof(file_path)-1);
			logd("file path: %s", file_path);
			break;
		case 'W':
			capture_info.width = atoi(optarg);
			break;
		case 'H':
			capture_info.height = atoi(optarg);
			break;
		case 'q':
			capture_info.quality = atoi(optarg);
			break;
		case 'h':
			print_help(argv[0]);
			break;
		default:
			break;
		}
	}

	if (files.file_num == 0) {
		print_help(argv[0]);
		return -1;
	}

	player = aic_player_create(NULL);
	if (player == NULL) {
		loge("aic_player_create fail!!!\n");
		return -1;
	}

	aic_player_set_event_callback(player,player,event_handle);
	g_sync_flag = AIC_PLAYER_PREPARE_SYNC;
	flag = fcntl(STDIN_FILENO,F_GETFL);
	flag |= O_NONBLOCK;
	fcntl(STDIN_FILENO,F_SETFL,flag);

	for(i = 0;i < loop_time; i++) {
		for(j = 0; j < files.file_num; j++) {
			aic_player_set_uri(player,files.file_path[j]);
			if (g_sync_flag == AIC_PLAYER_PREPARE_ASYNC) {
				ret = aic_player_prepare_async(player);
			} else {
				ret = aic_player_prepare_sync(player);
			}
			if (ret) {
				loge("aic_player_prepare error!!!!\n");
				g_player_end = 1;
				goto _NEXT_FILE_;
			}


			if (g_sync_flag == AIC_PLAYER_PREPARE_SYNC) {
				if (start_play(player,volume) != 0) {
					g_player_end = 1;
					goto _NEXT_FILE_;
				}
			}

			while(1)
			 {
	_NEXT_FILE_:
				if (g_player_end == 1) {
					logd("play file:%s end!!!!\n",files.file_path[j]);
					ret = aic_player_stop(player);
					g_player_end = 0;
					break;
				}

				if (g_sync_flag == AIC_PLAYER_PREPARE_ASYNC && g_demuxer_detected_flag == 1) {
					g_demuxer_detected_flag = 0;
					if (start_play(player,volume) != 0) {
						g_player_end = 1;
						goto _NEXT_FILE_;
					}
				}

				if (read(STDIN_FILENO, buffer, BUFFER_LEN) > 0) {
					if (buffer[0] == 0x20) {// pause
						logd("*********enter pause ***************\n");
						aic_player_pause(player);
					} else if (buffer[0] == 'd') {//stop cur, star next
						logd("*********enter down ***************\n");
						aic_player_stop(player);
						break;
					} else if (buffer[0] == 'u') {//stop cur, star pre
						logd("*********enter up j:%d***************\n",j);
						aic_player_stop(player);
						j -= 2;
						j = (j < -1)?(-1):(j);
						break;
					} else if (buffer[0] == '-') {
						logd("*********enter volume--**************\n");
						volume -= 5;
						set_volume(player,volume);
					} else if (buffer[0] == '+') {
						logd("*********enter volume++***************\n");
						volume += 5;
						set_volume(player,volume);
					} else if (buffer[0] == 'm') {
						logd("*********enter/exit mute***************\n");
							aic_player_set_mute(player);
					} else if (buffer[0] == 'c') {
						logd("*********capture***************\n");
						if (aic_player_capture(player,&capture_info) == 0) {
							logd("*********aic_player_capture ok***************\n");
						} else {
							loge("*********aic_player_capture fail ***************\n");
						}
					} else if (buffer[0] == 'f') {
						logd("*********forward***************\n");
						do_seek(player,1);//+8s
					} else if (buffer[0] == 'b') {
						logd("*********back***************\n");
						do_seek(player,0);//-8s
					} else if (buffer[0] == 'z') {
						if (aic_player_seek(player,0) != 0) {
							loge("aic_player_seek error!!!!\n");
						} else {
							logd("aic_player_seek ok\n");
						}
					} else if (buffer[0] == 'r') {
						do_rotation(player);
					} else if(buffer[0] == 's') {//set display rect

					} else if(buffer[0] == 'e') {
						aic_player_stop(player);
						goto _EXIT0_;
					} else {

					}
				} else {
					usleep(1000*1000);
				}
			}
		}
	}
_EXIT0_:
	aic_player_destroy(player);
	return ret;
}
