/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: OMX_MuxerComponent
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

OMX_ERRORTYPE OMX_MuxerComponentDeInit(
		OMX_IN	OMX_HANDLETYPE hComponent);

OMX_ERRORTYPE OMX_MuxerComponentInit(
		OMX_IN	OMX_HANDLETYPE hComponent);

#define MUX_PACKET_NUM_MAX  64

typedef struct MUXER_IN_PACKET {
	struct aic_av_packet  pkt;
	struct mpp_list  sList;
}MUXER_IN_PACKET;

typedef struct MUXER_DATA_TYPE {
	OMX_STATETYPE state;
	pthread_mutex_t sStateLock;
	OMX_CALLBACKTYPE *pCallbacks;
	OMX_PTR pAppData;
	OMX_HANDLETYPE hSelf;
	OMX_PORT_PARAM_TYPE sPortParam;
	OMX_PARAM_PORTDEFINITIONTYPE sInPortDef[2];
	OMX_PARAM_BUFFERSUPPLIERTYPE sInBufSupplier[2];
	OMX_PORT_TUNNELEDINFO sInPortTunneledInfo[2];

	pthread_t threadId;
	struct aic_message_queue       sMsgQue;
	struct aic_av_media_info sMediaInfo;
	OMX_PARAM_CONTENTURITYPE *pContentUri;

	struct aic_muxer *pMuxer;
	struct mpp_list sInEmptyPkt;
	struct mpp_list sInReadyPkt;
	struct mpp_list sInProcessedPkt;
	pthread_mutex_t sInPktLock;
	int nInPktNodeNum;

	int nMaxDuration;
	int nFileNum;
	int nMuxerType;

	OMX_U32 nReceiveFrameNum;
	OMX_U32 nCurFileHasWriteFrameNum;

}MUXER_DATA_TYPE;
