/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: OMX_VencComponent
*/
#include <pthread.h>
#include <sys/prctl.h>
#include <malloc.h>
#include <string.h>
#include <stddef.h>

#include "OMX_Types.h"
#include "OMX_Core.h"
#include "OMX_CoreExt1.h"
#include "OMX_Component.h"

#include "mpp_log.h"
#include "mpp_list.h"
#include "mpp_mem.h"
#include "aic_message.h"
#include "aic_muxer.h"
#include "dma_allocator.h"
#include "mpp_encoder.h"

OMX_ERRORTYPE OMX_VencComponentInit(OMX_IN	OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE OMX_VencComponentDeInit(OMX_IN	OMX_HANDLETYPE hComponent);

#define VENC_PACKET_ONE_TIME_CREATE_NUM  16
#define VENC_PACKET_NUM_MAX 64
#define VENC_FRAME_ONE_TIME_CREATE_NUM 8
#define VENC_FRAME_NUM_MAX 32

typedef struct VENC_IN_FRAME {
	struct mpp_frame   sFrameInfo;
	struct mpp_list  sList;
}VENC_IN_FRAME;

typedef struct VENC_OUT_PACKET {
	struct aic_av_packet sPkt;
	int nDMAFd;
	void *nDMAMapPhyAddr;
	int nDMASize;
	int nQuality;
	struct mpp_list  sList;
}VENC_OUT_PACKET;

typedef struct VENC_CODER_CONFIG {
	struct aic_av_video_stream sVideoStream;
	int nQuality;// 0- 100
}VENC_CODER_CONFIG;

typedef struct VENC_DATA_TYPE {
	OMX_STATETYPE state;
	pthread_mutex_t sStateLock;
	OMX_CALLBACKTYPE *pCallbacks;
	OMX_PTR pAppData;
	OMX_HANDLETYPE hSelf;
	OMX_PORT_PARAM_TYPE sPortParam;

	OMX_PARAM_PORTDEFINITIONTYPE sInPortDef;
	OMX_PARAM_PORTDEFINITIONTYPE sOutPortDef;

	OMX_PARAM_BUFFERSUPPLIERTYPE sInBufSupplier;
	OMX_PARAM_BUFFERSUPPLIERTYPE sOutBufSupplier;
	OMX_PORT_TUNNELEDINFO sInPortTunneledInfo;
	OMX_PORT_TUNNELEDINFO sOutPortTunneledInfo;

	pthread_t threadId;
	struct aic_message_queue       sMsgQue;

	struct aic_av_video_stream sVideoStream;
	int nQuality;// 0- 100
	OMX_S32 nStreamEndFlag;
	OMX_S32 nDecodeEndFlag;
	OMX_S32 nFrameEndFlag;

	OMX_S32 nFlags;
	enum mpp_codec_type eCodeType;

	OMX_S32 nInFrameNodeNum;
	struct mpp_list sInEmptyFrame;
	struct mpp_list sInReadyFrame;
	struct mpp_list sInProcessedFrame;
	pthread_mutex_t sInFrameLock;

	OMX_S32 nOutPktNodeNum;
	struct mpp_list sOutEmptyPkt;
	struct mpp_list sOutReadyPkt;
	struct mpp_list sOutProcessingPkt;
	pthread_mutex_t sOutPktLock;

	int nDMADevice;
	OMX_U32 nReceivePacktOkNum;
	OMX_U32 nReceivePacktFailNum;
	OMX_U32 nSendBackFrameErrorNum;
	OMX_U32 nSendBackFrameOkNum;

}VENC_DATA_TYPE;
