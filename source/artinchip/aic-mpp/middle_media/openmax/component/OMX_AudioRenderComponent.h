/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: OMX_AudioRenderComponent
*/

#ifndef _OMX_AUDIO_RENDER_COMPONENT_H_
#define _OMX_AUDIO_RENDER_COMPONENT_H_
#include <pthread.h>
#include <sys/prctl.h>
#include <malloc.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>

#include "OMX_Types.h"
#include "OMX_Core.h"
#include "OMX_CoreExt1.h"
#include "OMX_Component.h"

#include "mpp_log.h"
#include "mpp_list.h"
#include "mpp_mem.h"
#include "aic_message.h"
#include "mpp_decoder.h"
#include "aic_audio_render.h"
#include "aic_audio_decoder.h"

OMX_ERRORTYPE OMX_AudioRenderComponentDeInit(
		OMX_IN	OMX_HANDLETYPE hComponent);

OMX_ERRORTYPE OMX_AudioRenderComponentInit(
		OMX_IN	OMX_HANDLETYPE hComponent);

typedef struct AUDIO_RENDER_IN_FRAME {
	struct aic_audio_frame   sFrameInfo;
	struct mpp_list  sList;
}AUDIO_RENDER_IN_FRAME;

#define AUDIO_RENDER_FRAME_NUM_MAX 16
#define AUDIO_RENDER_FRAME_ONE_TIME_CREATE_NUM 4

#define AUDIO_RENDER_INPORT_FRAME_END_FLAG  0x01 //inprot stream end

#define AUDIO_RENDER_INPORT_SEND_ALL_FRAME_FLAG  0x02 // consume all frame in readylist

//#define AUDIO_RENDRE_DUMP_ENABLE

#ifdef AUDIO_RENDRE_DUMP_ENABLE
#define  AUDIO_RENDRE_DUMP_FILEPATH "/sdcard/audio.pcm"
#endif

typedef struct AUDIO_RENDER_DATA_TYPE {
	OMX_STATETYPE state;
	pthread_mutex_t stateLock;
	OMX_CALLBACKTYPE *pCallbacks;
	OMX_PTR pAppData;
	OMX_HANDLETYPE hSelf;
	OMX_PORT_PARAM_TYPE sPortParam;
	OMX_PARAM_PORTDEFINITIONTYPE sInPortDef[2];
	OMX_PARAM_BUFFERSUPPLIERTYPE sInBufSupplier[2];
	OMX_PORT_TUNNELEDINFO sInPortTunneledInfo[2];

	pthread_t threadId;
	struct aic_message_queue       sMsgQue;

	OMX_S32 nFrameFisrtShowFlag;
	struct aic_audio_render * render;
	struct aic_audio_render_attr sAudioRenderArrt;
	OMX_S32 nAudioRenderInitFlag;
	OMX_S32 nVolume;
	OMX_S32 nVolumeChange;
	OMX_S32 nLayerId;
	OMX_S32 nDevId;
	struct mpp_rect sDisRect;

	OMX_S32 nFrameEndFlag;
	OMX_S32 nFlags;

	OMX_S32 nReceiveFrameNum;
	OMX_S32 nLeftReadyFrameWhenCompoentExitNum;
	OMX_S32 nShowFrameOkNum;
	OMX_S32 nShowFrameFailNum;
	OMX_S32 nGiveBackFrameFailNum;
	OMX_S32 nGiveBackFrameOkNum;

	OMX_S32 		nInFrameNodeNum;
	struct mpp_list sInEmptyFrame;
	struct mpp_list sInReadyFrame;
	struct mpp_list sInProcessedFrmae;
	pthread_mutex_t sInFrameLock;

	OMX_TICKS sPreFramePts;
	OMX_TICKS sFirstFramePts;
	OMX_TICKS sPreCorrectMediaTime;

	OMX_TIME_CLOCKSTATE eClockState;

#ifdef AUDIO_RENDRE_DUMP_ENABLE
	OMX_S32 nDumpAudioFd;
	OMX_S8  *pDumpAudioFilePath;
#endif

	pthread_mutex_t sWaitReayFrameLock;
	OMX_S32 nWaitReayFrameFlag;

}AUDIO_RENDER_DATA_TYPE;
#endif

