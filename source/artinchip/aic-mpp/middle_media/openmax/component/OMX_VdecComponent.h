/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: OMX_VdecComponent
*/

#ifndef _OMX_VDEC_COMPONENT_H_
#define _OMX_VDEC_COMPONENT_H_
#include <pthread.h>
#include <sys/prctl.h>
#include <malloc.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
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
#include "mpp_dec_type.h"


OMX_ERRORTYPE OMX_VdecComponentDeInit(
		OMX_IN	OMX_HANDLETYPE hComponent);

OMX_ERRORTYPE OMX_VdecComponentInit(
		OMX_IN	OMX_HANDLETYPE hComponent);



typedef struct VDEC_OUT_FRAME {
	struct mpp_frame   sFrameInfo;
	struct mpp_list  sList;
}VDEC_OUT_FRAME;


typedef struct VDEC_IN_PACKET {
	struct OMX_BUFFERHEADERTYPE   sBuff;
	struct mpp_list  sList;
}VDEC_IN_PACKET;




#define VDEC_PACKET_ONE_TIME_CREATE_NUM  16
#define VDEC_PACKET_NUM_MAX 64

#define VDEC_FRAME_ONE_TIME_CREATE_NUM 8
#define VDEC_FRAME_NUM_MAX 32

#define VDEC_INPORT_STREAM_END_FLAG 0x01 //inprot stream end
#define VDEC_DECODER_CONSUME_ALL_INPORT_STREAM_FLAG 0x02 // decoder consume all inport stream
#define VDEC_GET_ALL_FRAME_FREOM_DECODER_FLAG  0x04 // get all frame from decoder to readylist
#define VDEC_OUTPORT_SEND_ALL_FRAME_FLAG  0x08 // consume all frame in readylist


typedef struct VDEC_DATA_TYPE {
	OMX_STATETYPE state;
	pthread_mutex_t stateLock;
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

	OMX_U32 nReceivePacktOkNum;
	OMX_U32 nReceivePacktFailNum;
	OMX_U32 nPutPacktToDecoderOkNum;
	OMX_U32 nPutPacktToDecoderFailNum;
	OMX_U32 nGiveBackPacktOkNum;
	OMX_U32 nGiveBackPacktFailNum;

	OMX_U32 nGetFrameFromDecoderNum;
	OMX_U32 nDropFrameFromDecoderNum;
	OMX_U32 nLeftReadyFrameWhenCompoentExitNum;
	OMX_U32 nSendFrameOkNum;
	OMX_U32 nSendFrameErrorNum;
	OMX_U32 nSendBackFrameOkNum;
	OMX_S32 nSendBackFrameErrorNum;

	OMX_S32 nStreamEndFlag;
	OMX_S32 nDecodeEndFlag;
	OMX_S32 nFrameEndFlag;

	OMX_S32 nFlags;


	struct mpp_decoder *pDecoder;
	struct decode_config sDecoderConfig;
	enum mpp_codec_type eCodeType;

	OMX_S32 		nInPktNodeNum;
	struct mpp_list sInEmptyPkt;
	struct mpp_list sInReadyPkt;
	struct mpp_list sInProcessedPkt;
	pthread_mutex_t sInPktLock;
	OMX_S32 		nOutFrameNodeNum;
	struct mpp_list sOutEmptyFrame;
	struct mpp_list sOutReadyFrame;
	struct mpp_list sOutProcessingFrame;
	pthread_mutex_t sOutFrameLock;

	OMX_S8 nWaitForReadyPkt;
	OMX_S8 nWaitForEmptyFrame;


}VDEC_DATA_TYPE;

#endif


