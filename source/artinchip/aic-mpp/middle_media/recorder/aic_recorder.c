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
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>

#include "OMX_Core.h"
#include "OMX_CoreExt1.h"
#include "mpp_dec_type.h"
#include "mpp_log.h"
#include "mpp_mem.h"
#include "mpp_dec_type.h"
#include "aic_muxer.h"
#include "aic_recorder.h"

#define AIC_RECORDER_STATE_IDLE 0
#define AIC_RECORDER_STATE_INITIALIZED 1
#define AIC_RECORDER_STATE_RECORDING 2
#define AIC_RECORDER_STATE_STOPPED 3

#define  wait_state(\
		hComponent,\
		des_state)\
		 {\
			OMX_STATETYPE state;\
			while (1) {\
				OMX_GetState(hComponent, &state);\
				if (state == des_state) {\
					break;\
				} else {\
					usleep(1000);\
				} \
			} \
		} 				/* Macro End */

struct aic_recorder {
	OMX_HANDLETYPE muxer_handle;
	OMX_HANDLETYPE venc_handle;
	OMX_HANDLETYPE aenc_handle;
	struct aic_recorder_config config;
	event_handler event_handle;
	void* app_data;
	char uri[512];
	int state;
};

static OMX_ERRORTYPE component_event_handler (
	OMX_HANDLETYPE hComponent,
	OMX_PTR pAppData,
	OMX_EVENTTYPE eEvent,
	OMX_U32 Data1,
	OMX_U32 Data2,
	OMX_PTR pEventData)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	struct aic_recorder *recorder = (struct aic_recorder *)pAppData;

	switch((OMX_S32)eEvent) {
		case OMX_EventMuxerNeedNextFile:
			recorder->event_handle(recorder->app_data,AIC_RECORDER_EVENT_NEED_NEXT_FILE,0,0);
			break;
		default:
			break;
	}
	return eError;
}

OMX_ERRORTYPE empty_buffer_done(
	OMX_OUT OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_PTR pAppData,
	OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;

	OMX_U64 tmp = (OMX_U64)pBuffer->pOutputPortPrivate;
	struct aic_recorder *recorder = (struct aic_recorder *)pAppData;
	if (hComponent == recorder->venc_handle) {
		recorder->event_handle(recorder->app_data,AIC_RECORDER_EVENT_RELEASE_VIDEO_BUFFER,(OMX_U32)tmp,0);
	}
	return eError;
}

static OMX_CALLBACKTYPE component_event_callbacks = {
	.EventHandler    = component_event_handler,
	.EmptyBufferDone = empty_buffer_done,
	.FillBufferDone  = NULL
};

struct aic_recorder *aic_recorder_create(void)
{
	OMX_ERRORTYPE eError;
	struct aic_recorder * recorder = mpp_alloc(sizeof(struct aic_recorder));
	if (recorder == NULL) {
		loge("mpp_alloc aic_recorder error\n");
		return NULL;
	}
	memset(recorder,0x00,sizeof(struct aic_recorder));
	eError = OMX_Init();
	if (eError != OMX_ErrorNone) {
		loge("OMX_init error!!!\n");
		mpp_free(recorder);
		return NULL;
	}
	recorder->state = AIC_RECORDER_STATE_IDLE;
	return recorder;
}

s32 aic_recorder_destroy(struct aic_recorder *recorder)
{
	OMX_Deinit();
	mpp_free(recorder);
	return 0;
}

s32 aic_recorder_set_event_callback(struct aic_recorder *recorder,void* app_data,event_handler event_handle)
{
	recorder->event_handle = event_handle;
	recorder->app_data = app_data;
	return 0;
}

s32 aic_recorder_init(struct aic_recorder *recorder,struct aic_recorder_config *recorder_config)
{
	int ret = 0;
	OMX_PARAM_PORTDEFINITIONTYPE port_define;
	OMX_PARAM_RECORDERFILEINFO rec_file_info;
	OMX_IMAGE_PARAM_QFACTORTYPE qfactor;
	if (!recorder || !recorder_config) {
		return -1;
	}
	recorder->config = *recorder_config;
	if (!recorder->config.has_video && !recorder->config.has_audio) {
		loge("para error\n");
		return -1;
	}

	// create muxer
	if (OMX_ErrorNone !=OMX_GetHandle(&recorder->muxer_handle, OMX_COMPONENT_MUXER_NAME,recorder, &component_event_callbacks)) {
		loge("unable to get muxer_handle handle.\n");
		return -1;
	}
	// set muxer para
	rec_file_info.nDuration = recorder->config.file_duration;
	rec_file_info.nFileNum = recorder->config.file_num;
	rec_file_info.nMuxerType = 0;//only support  mp4
	if (OMX_ErrorNone != OMX_SetParameter(recorder->muxer_handle, OMX_IndexVendorMuxerRecorderFileInfo,&rec_file_info)) {
		loge("OMX_SetParameter error.\n");
		ret = -1;
		goto _EXIT;
	}
	// video
	if (recorder->config.has_video) {
		// create venc
		if (OMX_ErrorNone !=OMX_GetHandle(&recorder->venc_handle, OMX_COMPONENT_VENC_NAME,recorder, &component_event_callbacks)) {
			loge("unable to get muxer_handle handle.\n");
			ret = -1;
			goto _EXIT;
		}

		// set muxer in_video_port para
		port_define.nPortIndex = MUX_PORT_VIDEO_INDEX;
		if (OMX_ErrorNone != OMX_GetParameter(recorder->muxer_handle, OMX_IndexParamPortDefinition,&port_define)) {
			loge("OMX_GetParameter error.\n");
			ret = -1;
			goto _EXIT;
		}

		port_define.format.video.nFrameWidth = recorder->config.video_config.out_width;
		port_define.format.video.nFrameHeight = recorder->config.video_config.out_height;
		port_define.format.video.xFramerate = recorder->config.video_config.out_frame_rate;
		port_define.format.video.nBitrate = recorder->config.video_config.out_bit_rate;
		if (recorder->config.video_config.codec_type != MPP_CODEC_VIDEO_DECODER_MJPEG) {
			loge("only support MPP_CODEC_VIDEO_DECODER_MJPEG.\n");
			ret = -1;
			goto _EXIT;
		}
		port_define.format.video.eCompressionFormat = OMX_VIDEO_CodingMJPEG;
		if (OMX_ErrorNone != OMX_SetParameter(recorder->muxer_handle, OMX_IndexParamPortDefinition,&port_define)) {
			loge("OMX_SetParameter error.\n");
			ret = -1;
			goto _EXIT;
		}

		// set venc in_port para
		if (OMX_ErrorNone != OMX_GetParameter(recorder->venc_handle, OMX_IndexParamPortDefinition,&port_define)) {
			loge("OMX_SetParameter error.\n");
			ret = -1;
			goto _EXIT;
		}
		port_define.format.video.nFrameWidth = recorder->config.video_config.out_width;
		port_define.format.video.nFrameHeight = recorder->config.video_config.out_height;
		port_define.format.video.xFramerate = recorder->config.video_config.out_frame_rate;
		port_define.format.video.nBitrate = recorder->config.video_config.out_bit_rate;
		port_define.format.video.eCompressionFormat = OMX_VIDEO_CodingMJPEG;
		if (OMX_ErrorNone != OMX_SetParameter(recorder->venc_handle, OMX_IndexParamPortDefinition,&port_define)) {
			loge("OMX_SetParameter error.\n");
			ret = -1;
			goto _EXIT;
		}
		qfactor.nQFactor = recorder->config.qfactor;
		if (OMX_ErrorNone != OMX_SetParameter(recorder->venc_handle, OMX_IndexParamQFactor,&qfactor)) {
			loge("OMX_SetParameter error.\n");
			ret = -1;
			goto _EXIT;
		}

		// setup tunnel VENC_PORT_OUT_INDEX--->MUX_PORT_VIDEO_INDEX
		if (OMX_ErrorNone !=OMX_SetupTunnel(recorder->venc_handle, VENC_PORT_OUT_INDEX, recorder->muxer_handle, MUX_PORT_VIDEO_INDEX)) {
			loge("OMX_SetupTunnel error.\n");
			ret = -1;
			goto _EXIT;
		}
	}

	// audio
	if (recorder->config.has_audio) {

	}

	if (recorder->muxer_handle) {
		OMX_SendCommand(recorder->muxer_handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
	}
	if (recorder->venc_handle) {
		OMX_SendCommand(recorder->venc_handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
	}
	if (recorder->aenc_handle) {
		OMX_SendCommand(recorder->aenc_handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
	}

	recorder->state = AIC_RECORDER_STATE_INITIALIZED;

	return ret;
_EXIT:
	if (recorder->muxer_handle) {
		OMX_FreeHandle(recorder->muxer_handle);
		recorder->muxer_handle = NULL;
	}
	if (recorder->venc_handle) {
		OMX_FreeHandle(recorder->venc_handle);
		recorder->venc_handle = NULL;
	}
	if (recorder->aenc_handle) {
		OMX_FreeHandle(recorder->aenc_handle);
		recorder->aenc_handle = NULL;
	}
	return ret;
}

s32 aic_recorder_start(struct aic_recorder *recorder)
{

	if (recorder->config.has_video && recorder->venc_handle) {
		OMX_SendCommand(recorder->venc_handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
	}
	if (recorder->config.has_audio && recorder->aenc_handle) {
		OMX_SendCommand(recorder->aenc_handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
	}
	if (recorder->muxer_handle) {
		OMX_SendCommand(recorder->muxer_handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
	}
	recorder->state = AIC_RECORDER_STATE_RECORDING;
	return 0;
}

s32 aic_recorder_stop(struct aic_recorder *recorder)
{

	if (recorder->state  == AIC_RECORDER_STATE_IDLE) {
		printf("%s:%d\n",__FUNCTION__,__LINE__);
		goto _FREE_HANDLE_;
	}

	if (recorder->muxer_handle) {
		OMX_SendCommand(recorder->muxer_handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
		wait_state(recorder->muxer_handle,OMX_StateIdle);
		OMX_SendCommand(recorder->muxer_handle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
		wait_state(recorder->muxer_handle,OMX_StateLoaded);
	}

	if (recorder->config.has_video) {
		if (recorder->venc_handle) {
			OMX_SendCommand(recorder->venc_handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
			wait_state(recorder->venc_handle,OMX_StateIdle);
			OMX_SendCommand(recorder->venc_handle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
			wait_state(recorder->venc_handle,OMX_StateLoaded);
		}
	}

	if (recorder->config.has_video) {
		if (recorder->muxer_handle && recorder->venc_handle) {
			OMX_SetupTunnel(recorder->venc_handle,VENC_PORT_OUT_INDEX,NULL,0);
			OMX_SetupTunnel(NULL,0,recorder->muxer_handle,MUX_PORT_VIDEO_INDEX);
		}
	}

_FREE_HANDLE_:
	if (recorder->muxer_handle) {
		OMX_FreeHandle(recorder->muxer_handle);
		recorder->muxer_handle = NULL;
	}
	if (recorder->venc_handle) {
		OMX_FreeHandle(recorder->venc_handle);
		recorder->venc_handle = NULL;
	}
	if (recorder->aenc_handle) {
		OMX_FreeHandle(recorder->aenc_handle);
		recorder->aenc_handle = NULL;
	}
	return 0;
}

s32 aic_recorder_set_output_file_path(struct aic_recorder *recorder, char *uri)
{
	int bytes;
	OMX_PARAM_CONTENTURITYPE *uri_param;

	if (!recorder || !uri) {
		loge("param  error\n");
		return -1;
	}
	memset(recorder->uri,0x00,sizeof(recorder->uri));
	strncpy(recorder->uri,uri,sizeof(recorder->uri)-1);
	bytes = strlen(recorder->uri);
	uri_param = (OMX_PARAM_CONTENTURITYPE *)mpp_alloc(sizeof(OMX_PARAM_CONTENTURITYPE) + bytes);
	uri_param->nSize = sizeof(OMX_PARAM_CONTENTURITYPE) + bytes;
	strcpy((char *)uri_param->contentURI, recorder->uri);
	OMX_SetParameter(recorder->muxer_handle, OMX_IndexParamContentURI,uri_param);
	mpp_free(uri_param);
	return 0;
}

s32 aic_recorder_set_max_duration(struct aic_recorder *recorder)
{
	return 0;
}

s32 aic_recorder_write_video_frame(struct aic_recorder *recorder, struct mpp_frame *frame)
{
	OMX_BUFFERHEADERTYPE buffer_header;
	if (!recorder->venc_handle) {
		loge("venc_handle==NULL");
	}
	buffer_header.pOutputPortPrivate =  (OMX_U8 *)frame;
	if (OMX_ErrorNone == OMX_EmptyThisBuffer(recorder->venc_handle,&buffer_header)) {
		return -1;
	} else {
		return 0;
	}

}
