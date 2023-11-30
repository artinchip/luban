/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: OMX_VdecComponent tunneld  OMX_VideoRenderComponent demo
*/

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define LOG_DEBUG
#include "cjson/cJSON.h"
#include "mpp_list.h"
#include "mpp_log.h"
#include "dma_allocator.h"
#include "aic_recorder.h"

char* read_file(const char *filename) {
    FILE *file = NULL;
    long length = 0;
    char *content = NULL;
    size_t read_chars = 0;

    /* open in read binary mode */
    file = fopen(filename, "rb");
    if (file == NULL)
    {
        goto cleanup;
    }

    /* get the length */
    if (fseek(file, 0, SEEK_END) != 0)
    {
        goto cleanup;
    }
    length = ftell(file);
    if (length < 0)
    {
        goto cleanup;
    }
    if (fseek(file, 0, SEEK_SET) != 0)
    {
        goto cleanup;
    }

    /* allocate content buffer */
    content = (char*)malloc((size_t)length + sizeof(""));
    if (content == NULL)
    {
        goto cleanup;
    }

    /* read the file into memory */
    read_chars = fread(content, sizeof(char), (size_t)length, file);
    if ((long)read_chars != length)
    {
        free(content);
        content = NULL;
        goto cleanup;
    }
    content[read_chars] = '\0';


cleanup:
    if (file != NULL)
    {
        fclose(file);
    }

    return content;
}

static cJSON *parse_file(const char *filename)
{
    cJSON *parsed = NULL;
    char *content = read_file(filename);

    parsed = cJSON_Parse(content);

    if (content != NULL)
    {
        free(content);
    }

    return parsed;
}

struct video_input_frame {
	struct mpp_list list;
	struct mpp_frame frame;
	unsigned char* vir_addr[3];
	int size[3];
};

struct recorder_context {
	struct aic_recorder *recorder;
	FILE *video_in_fp;
	FILE *audio_in_fp;
	char video_in_file_path[256];
	char audio_in_file_path[256];
	char output_file_path[256];
	struct aic_recorder_config config;
	//video
	int dma_device;
	struct video_input_frame * frame_node;
	struct mpp_list video_input_idle_list;
	struct mpp_list video_input_ready_list;
	struct mpp_list video_input_processing_list;

	pthread_cond_t video_input_empty_cond;
	int wait_video_input_empty_flag;
	int video_input_empty_cond_init_ok;

	pthread_mutex_t video_input_lock;
	pthread_t video_thread_id;
	int  video_thread_exit;
	//audio
	pthread_t audio_thread_id;
	int  audio_thread_exit;

};

#define MAX_FRAME_NUM 3

static int  g_recorder_flag;

s32 event_handle(void* app_data,s32 event,s32 data1,s32 data2)
{
	int ret = 0;
	struct recorder_context *recorder_cxt = (struct recorder_context *)app_data;
	struct video_input_frame *frame_node = NULL;
	static int file_index = 0;
	char file_path[512] = {0};
	switch(event) {
		case AIC_RECORDER_EVENT_NEED_NEXT_FILE:
			//set recorder file name
			snprintf(file_path,sizeof(file_path),"%s-%d.mp4",recorder_cxt->output_file_path,file_index++);
			aic_recorder_set_output_file_path(recorder_cxt->recorder,file_path);
			break;
		case AIC_RECORDER_EVENT_COMPLETE:
			g_recorder_flag = 1;
			break;
		case AIC_RECORDER_EVENT_NO_SPACE:
			break;
		case AIC_RECORDER_EVENT_RELEASE_VIDEO_BUFFER: {
			pthread_mutex_lock(&recorder_cxt->video_input_lock);
			if (mpp_list_empty(&recorder_cxt->video_input_processing_list)) {
				loge("AIC_RECORDER_EVENT_RELEASE_VIDEO_BUFFER error");
				pthread_mutex_unlock(&recorder_cxt->video_input_lock);
				break;
			}
			frame_node = mpp_list_first_entry(&recorder_cxt->video_input_processing_list,struct video_input_frame,list);
			mpp_list_del(&frame_node->list);
			mpp_list_add_tail(&frame_node->list,&recorder_cxt->video_input_idle_list);
			if (recorder_cxt->wait_video_input_empty_flag == 1) {
				recorder_cxt->wait_video_input_empty_flag = 0;
				pthread_cond_signal(&recorder_cxt->video_input_empty_cond);
			}
			pthread_mutex_unlock(&recorder_cxt->video_input_lock);
			break;
		}

		default:
			break;
	}
	return ret;
}

static void* video_thread(void* pThreadData) {
	struct recorder_context *recorder_cxt = (struct recorder_context *)pThreadData;
	struct video_input_frame *frame_node;
	int frame_index = 0;
	int frame_duration = 25;
	int i = 0;

	while (!recorder_cxt->video_thread_exit) {
		pthread_mutex_lock(&recorder_cxt->video_input_lock);
		while (!mpp_list_empty(&recorder_cxt->video_input_ready_list)) {
			frame_node = mpp_list_first_entry(&recorder_cxt->video_input_ready_list,struct video_input_frame,list);
			aic_recorder_write_video_frame(recorder_cxt->recorder,&frame_node->frame);
			mpp_list_del(&frame_node->list);
			mpp_list_add_tail(&frame_node->list,&recorder_cxt->video_input_processing_list);
		}

		if (mpp_list_empty(&recorder_cxt->video_input_idle_list)) {
			recorder_cxt->wait_video_input_empty_flag = 1;
			pthread_cond_wait(&recorder_cxt->video_input_empty_cond,&recorder_cxt->video_input_lock);
		}

		frame_node = mpp_list_first_entry(&recorder_cxt->video_input_idle_list,struct video_input_frame,list);
		for (i = 0;i < 3;i++) {
			if (fread(frame_node->vir_addr[i],1,frame_node->size[i],recorder_cxt->video_in_fp) != frame_node->size[i]) {
				logd("read to file end");
				pthread_mutex_unlock(&recorder_cxt->video_input_lock);
				g_recorder_flag = 1;
				goto _EXIT;
			}
			dmabuf_sync(frame_node->frame.buf.fd[i], CACHE_CLEAN);
		}
		usleep(20*1000);
		frame_node->frame.pts = frame_index++ * frame_duration;
		mpp_list_del(&frame_node->list);
		mpp_list_add_tail(&frame_node->list,&recorder_cxt->video_input_ready_list);
		pthread_mutex_unlock(&recorder_cxt->video_input_lock);
	}
_EXIT:
	return (void*)0;
}

static void* audio_thread(void* pThreadData)
{
	return (void*)0;
}

int init_video_frame(struct recorder_context *recorder_cxt)
{
	int i = 0,j = 0;
	struct video_input_frame *frame_node;
	int width =0;
	int height = 0;
	int size[3];
	int stride[3];
	if (!recorder_cxt) {
		return -1;
	}

	width = recorder_cxt->config.video_config.in_width;
	height = recorder_cxt->config.video_config.in_height;
	recorder_cxt->dma_device = dmabuf_device_open();
	mpp_list_init(&recorder_cxt->video_input_idle_list);
	mpp_list_init(&recorder_cxt->video_input_ready_list);
	mpp_list_init(&recorder_cxt->video_input_processing_list);
	recorder_cxt->frame_node = malloc(MAX_FRAME_NUM * sizeof(struct video_input_frame));
	if (!recorder_cxt->frame_node) {
		return -1;
	}
	memset(recorder_cxt->frame_node,0x00,MAX_FRAME_NUM * sizeof(struct video_input_frame));
	for (i = 0; i < 3; i++) {
		size[i] = (i==0)?(width * height):(width * height/4);
		stride[i] = (i==0)?(width):(width/2);
	}
	frame_node = recorder_cxt->frame_node;
	for (i = 0; i < MAX_FRAME_NUM; i++) {
		for (j = 0; j < 3;j++) {
			frame_node[i].frame.buf.fd[j] = dmabuf_alloc(recorder_cxt->dma_device,size[j]);
			frame_node[i].vir_addr[j] = dmabuf_mmap(frame_node[i].frame.buf.fd[j], size[j]);
			frame_node[i].size[j] = size[j];
			frame_node[i].frame.buf.stride[j] = stride[j];
		}
		frame_node[i].frame.buf.buf_type = MPP_DMA_BUF_FD;
		frame_node[i].frame.buf.size.width = width;
		frame_node[i].frame.buf.size.height = height;
		frame_node[i].frame.buf.format = MPP_FMT_YUV420P;
		mpp_list_add_tail(&frame_node[i].list,&recorder_cxt->video_input_idle_list);
	}

	return 0;
}

int deinit_video_frame(struct recorder_context *recorder_cxt)
{
	int i=0,j=0;
	struct video_input_frame *frame_node;
	if (!recorder_cxt->frame_node) {
		return -1;
	}
	frame_node = recorder_cxt->frame_node;
	for (i = 0; i < MAX_FRAME_NUM; i++) {
		for (j = 0; j < 3;j++) {
			if (frame_node[i].frame.buf.fd[i] > 0) {
				dmabuf_munmap(frame_node[i].vir_addr[j] , frame_node[i].size[j]);
				dmabuf_free(frame_node[i].frame.buf.fd[i]);
				frame_node[i].frame.buf.fd[i] = -1;
			}
		}
		frame_node++;
	}
	if (recorder_cxt->dma_device > 0) {
		dmabuf_device_close(recorder_cxt->dma_device);
		recorder_cxt->dma_device = -1;
	}
	if (recorder_cxt->frame_node) {
		free(recorder_cxt->frame_node);
 		recorder_cxt->frame_node = NULL;
	}
	return 0;
}

int parse_config_file(char *config_file,struct recorder_context *recorder_cxt)
{
	int ret = 0;
	cJSON *cjson = NULL;
	cJSON *root = NULL;
	if (!config_file || !recorder_cxt) {
		ret = -1;
		goto _EXIT;
	}
	root = parse_file(config_file);
	if (!root) {
		loge("parse_file error");
		ret = -1;
		goto _EXIT;
	}

	cjson = cJSON_GetObjectItem(root,"video_in_file");
	if (!cjson) {
		loge("no video_in_file error");
		ret = -1;
		goto _EXIT;
	}
	strcpy(recorder_cxt->video_in_file_path,cjson->valuestring);

	cjson = cJSON_GetObjectItem(root,"audio_in_file");
	if (!cjson) {
		strcpy(recorder_cxt->audio_in_file_path, cjson->valuestring);
	}

	cjson = cJSON_GetObjectItem(root,"output_file");
	if (!cjson) {
		loge("no output_file error");
		ret = -1;
		goto _EXIT;
	}
	strcpy(recorder_cxt->output_file_path, cjson->valuestring);

	cjson = cJSON_GetObjectItem(root,"file_duration");
	if (cjson) {
		recorder_cxt->config.file_duration = cjson->valueint*1000;
	}

	cjson = cJSON_GetObjectItem(root,"file_num");
	if (cjson) {
		recorder_cxt->config.file_num = cjson->valueint;
	}

	cjson = cJSON_GetObjectItem(root,"qfactor");
	if (cjson) {
		recorder_cxt->config.qfactor = cjson->valueint;
	}

	cjson = cJSON_GetObjectItem(root,"video");
	if (cjson) {
		int enable = cJSON_GetObjectItem(cjson,"enable")->valueint;
		if (enable == 1) {
			recorder_cxt->config.has_video = 1;
		}
		recorder_cxt->config.video_config.codec_type = cJSON_GetObjectItem(cjson,"codec_type")->valueint;

		loge("codec_type:0x%x",recorder_cxt->config.video_config.codec_type);
		printf("codec_type:0x%x\n",recorder_cxt->config.video_config.codec_type);
		if (recorder_cxt->config.video_config.codec_type != MPP_CODEC_VIDEO_DECODER_MJPEG) {
			ret = -1;
			loge("only support  MPP_CODEC_VIDEO_DECODER_MJPEG");
			g_recorder_flag = 1;
			goto _EXIT;
		}
		recorder_cxt->config.video_config.out_width = cJSON_GetObjectItem(cjson,"out_width")->valueint;
		recorder_cxt->config.video_config.out_height = cJSON_GetObjectItem(cjson,"out_height")->valueint;
		recorder_cxt->config.video_config.out_frame_rate = cJSON_GetObjectItem(cjson,"out_framerate")->valueint;
		recorder_cxt->config.video_config.out_bit_rate = cJSON_GetObjectItem(cjson,"out_bitrate")->valueint;
		recorder_cxt->config.video_config.in_width = cJSON_GetObjectItem(cjson,"in_width")->valueint;
		recorder_cxt->config.video_config.in_height = cJSON_GetObjectItem(cjson,"in_height")->valueint;
		recorder_cxt->config.video_config.in_pix_fomat = cJSON_GetObjectItem(cjson,"in_pix_format")->valueint;
	}

	cjson = cJSON_GetObjectItem(root,"audio");
	if (cjson) {
		int enable = cJSON_GetObjectItem(cjson,"enable")->valueint;
		if (enable == 1) {
			recorder_cxt->config.has_audio = 1;
		}
		recorder_cxt->config.audio_config.codec_type = cJSON_GetObjectItem(cjson,"codec_type")->valueint;
		recorder_cxt->config.audio_config.out_bitrate = cJSON_GetObjectItem(cjson,"out_bitrate")->valueint;
		recorder_cxt->config.audio_config.out_samplerate = cJSON_GetObjectItem(cjson,"out_samplerate")->valueint;
		recorder_cxt->config.audio_config.out_channels = cJSON_GetObjectItem(cjson,"out_channels")->valueint;
		recorder_cxt->config.audio_config.out_bits_per_sample = cJSON_GetObjectItem(cjson,"out_bits_per_sample")->valueint;
		recorder_cxt->config.audio_config.in_samplerate = cJSON_GetObjectItem(cjson,"in_samplerate")->valueint;
		recorder_cxt->config.audio_config.in_channels = cJSON_GetObjectItem(cjson,"in_channels")->valueint;
		recorder_cxt->config.audio_config.in_bits_per_sample = cJSON_GetObjectItem(cjson,"in_bits_per_sample")->valueint;
	}
_EXIT:
	if (root) {
		cJSON_Delete(root);
	}
	return ret;
}

#define BUFFER_LEN 32

int main(int argc ,char *argv[])
{
	int ret = 0;
	char buffer[BUFFER_LEN];
	int flag;
	struct recorder_context *recorder_cxt = NULL;

	if(argc < 2) {
		printf("\trecorder_demo usage:recorder_demo recorder_demo.config\n");
		return -1;
	}

	recorder_cxt = malloc(sizeof(struct recorder_context));
	if (!recorder_cxt) {
		loge("malloc error");
		return -1;
	}
	memset(recorder_cxt,0x00,sizeof(struct recorder_context));
	if (parse_config_file(argv[1],recorder_cxt)) {
		loge("parse_config_file error");
		goto _EXIT;
	}

	pthread_cond_init(&recorder_cxt->video_input_empty_cond,NULL);
	recorder_cxt->wait_video_input_empty_flag = 0;
	recorder_cxt->video_input_empty_cond_init_ok = 1;

	recorder_cxt->video_in_fp = fopen(recorder_cxt->video_in_file_path,"rb");
	if (!recorder_cxt->video_in_fp) {
		loge("fopen error");
		goto _EXIT;
	}
	printf("%s:%d\n",__FUNCTION__,__LINE__);
	if (init_video_frame(recorder_cxt)) {
		loge("init_video_frame error");
		goto _EXIT;
	}

	recorder_cxt->recorder = aic_recorder_create();
	if (!recorder_cxt->recorder) {
		loge("aic_recorder_create error");
		goto _EXIT;
	}

	if (aic_recorder_set_event_callback(recorder_cxt->recorder,recorder_cxt,event_handle)) {
		loge("aic_recorder_set_event_callback error");
		goto _EXIT;
	}
	if (aic_recorder_init(recorder_cxt->recorder,&recorder_cxt->config)) {
		loge("aic_recorder_init error");
		goto _EXIT;
	}

	if (aic_recorder_start(recorder_cxt->recorder)) {
		loge("aic_recorder_start error");
		goto _EXIT;
	}

	if (recorder_cxt->config.has_video) {
		loge("recorder_cxt->video_thread_exit:%d",recorder_cxt->video_thread_exit);
		pthread_create(&recorder_cxt->video_thread_id, NULL, video_thread, recorder_cxt);
	}
	if (recorder_cxt->config.has_audio) {
		pthread_create(&recorder_cxt->audio_thread_id, NULL, audio_thread, recorder_cxt);
	}

	flag = fcntl(STDIN_FILENO,F_GETFL);
	flag |= O_NONBLOCK;
	fcntl(STDIN_FILENO,F_SETFL,flag);

	while (!g_recorder_flag) {
		if (read(STDIN_FILENO, buffer, BUFFER_LEN) > 0) {
			if (buffer[0] == 'e') {
				logd("app want to exit");
				recorder_cxt->video_thread_exit = 1;
				break;
			}
		} else {
			usleep(1000);
		}
	}


_EXIT:
	if (recorder_cxt && recorder_cxt->recorder) {
		aic_recorder_stop(recorder_cxt->recorder);
		aic_recorder_destroy(recorder_cxt->recorder);
	}

	if (recorder_cxt->config.has_video && recorder_cxt->video_thread_id) {
		pthread_join(recorder_cxt->video_thread_id,(void*)&ret);
		logd("video_thread_id pthread_join:%d",ret);
	}
	if (recorder_cxt->config.has_audio && recorder_cxt->audio_thread_id) {
		pthread_join(recorder_cxt->audio_thread_id,(void*)&ret);
		logd("audio_thread_id pthread_join:%d",ret);
	}

	if(recorder_cxt->video_input_empty_cond_init_ok) {
		pthread_cond_destroy(&recorder_cxt->video_input_empty_cond);
	}

	if (recorder_cxt) {
		deinit_video_frame(recorder_cxt);
		free(recorder_cxt);
	}

	return ret;
}
