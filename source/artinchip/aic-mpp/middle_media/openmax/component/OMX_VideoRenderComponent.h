
/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: OMX_DemuxerComponent
*/


#ifndef _OMX_VIDEO_RENDER_COMPONENT_H_
#define _OMX_VIDEO_RENDER_COMPONENT_H_
#include <pthread.h>
#include <sys/prctl.h>
#include <malloc.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <inttypes.h>

#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "OMX_Types.h"
#include "OMX_Core.h"
#include "OMX_CoreExt1.h"
#include "OMX_Component.h"

#include "mpp_log.h"
#include "mpp_list.h"
#include "mpp_mem.h"
#include "aic_message.h"
#include "mpp_decoder.h"
#include "aic_render.h"

#include "dma_allocator.h"
#include "mpp_encoder.h"
#include "mpp_ge.h"

OMX_ERRORTYPE OMX_VideoRenderComponentDeInit(
		OMX_IN	OMX_HANDLETYPE hComponent);

OMX_ERRORTYPE OMX_VideoRenderComponentInit(
		OMX_IN	OMX_HANDLETYPE hComponent);


typedef struct VIDEO_RENDER_IN_FRAME {
	struct mpp_frame   sFrameInfo;
	struct mpp_list  sList;
}VIDEO_RENDER_IN_FRAME;

typedef struct VIDEO_RENDER_DMA_FD {
	int   fd[3];
	struct mpp_list  sList;
}VIDEO_RENDER_DMA_FD;

#define VIDEO_RENDER_FRAME_NUM_MAX 16
#define VIDEO_RENDER_FRAME_ONE_TIME_CREATE_NUM 4

#define VIDEO_RENDER_INPORT_FRAME_END_FLAG  0x01 //inprot stream end

#define VIDEO_RENDER_INPORT_SEND_ALL_FRAME_FLAG  0x02 // consume all frame in readylist


typedef struct VIDEO_RENDER_DATA_TYPE {
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
	OMX_S32 nVideoRenderInitFlag;
	struct aic_video_render * render;
	//now default nLayerId = 0 and nDevId = 0
	OMX_S32 nLayerId;
	OMX_S32 nDevId;
	struct mpp_rect sDisRect;
	OMX_S32 nDisRectChange;

	OMX_S32 nFrameEndFlag;
	OMX_S32 nFlags;

	OMX_S32 nReceiveFrameNum;
	OMX_S32 nLeftReadyFrameWhenCompoentExitNum;
	OMX_S32 nShowFrameOkNum;
	OMX_S32 nShowFrameFailNum;
	OMX_S32 nGiveBackFrameFailNum;
	OMX_S32 nGiveBackFrameOkNum;
	OMX_S32 nDropFrameNum;

	OMX_S32 		nInFrameNodeNum;
	struct mpp_list sInEmptyFrame;
	struct mpp_list sInReadyFrame;
	struct mpp_list sInProcessedFrmae;
	pthread_mutex_t sInFrameLock;

	OMX_TICKS sFisrtShowPts; //us
	OMX_TICKS sWallTimeBase;
	OMX_TICKS sPauseTimePoint;
	OMX_TICKS sPauseTimeDurtion;

	OMX_TICKS sPreFramePts;


	OMX_S32 nDumpIndex;

	OMX_TIME_CLOCKSTATE eClockState;

	pthread_mutex_t sWaitReayFrameLock;
	OMX_S32 nWaitReayFrameFlag;

	//rotation
	OMX_S32 nInitRotationParam; // -1-init fail,0-not init,1-init ok
	OMX_S32 nRotationAngleChange;
	OMX_S32 nRotationAngle;
	OMX_S32 nDMAFd;
	struct mpp_frame sRotationFrames[2];
	struct mpp_frame *pCurDisplayFrame;
	struct mpp_ge *sGeHandle;
	struct mpp_list sDMAFd;
	OMX_U32 nRotationIndex;
}VIDEO_RENDER_DATA_TYPE;


#endif


