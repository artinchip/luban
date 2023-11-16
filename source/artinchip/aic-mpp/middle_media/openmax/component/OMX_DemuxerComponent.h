/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: OMX_DemuxerComponent
*/

#ifndef _OMX_DEMUXER_COMPONENT_H_
#define _OMX_DEMUXER_COMPONENT_H_

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
#include "aic_parser.h"
#include "aic_stream.h"

OMX_ERRORTYPE OMX_DemuxerComponentDeInit(
		OMX_IN	OMX_HANDLETYPE hComponent);

OMX_ERRORTYPE OMX_DemuxerComponentInit(
		OMX_IN	OMX_HANDLETYPE hComponent);

typedef struct DEMUXER_OUT_PACKET {
	OMX_BUFFERHEADERTYPE  sBuff;
	struct mpp_list  sList;
}DEMUXER_OUT_PACKET;

//later,modify accord to real test
//#define DEMUX_AUDIO_PACKET_NUM_MAX  128
#define DEMUX_AUDIO_PACKET_NUM_MAX  64
#define DEMUX_VIDEO_PACKET_NUM_MAX   8

#define DEMUX_SKIP_AUDIO_TRACK 0x01
#define DEMUX_SKIP_VIDEO_TRACK 0x02

typedef struct DEMUXER_DATA_TYPE {
	OMX_STATETYPE state;
	pthread_mutex_t stateLock;
	OMX_CALLBACKTYPE *pCallbacks;
	OMX_PTR pAppData;
	OMX_HANDLETYPE hSelf;
	OMX_PORT_PARAM_TYPE sPortParam;
	/*
		OpenMAX_IL_1_1_2_Specification.pdf 8.9.3
		output port: video and audio
		input port : time or other
	*/
	OMX_PARAM_PORTDEFINITIONTYPE sInPortDef;
	OMX_PARAM_PORTDEFINITIONTYPE sOutPortDef[2];

	OMX_PARAM_BUFFERSUPPLIERTYPE sInBufSupplier;
	OMX_PARAM_BUFFERSUPPLIERTYPE sOutBufSupplier[2];
	OMX_PORT_TUNNELEDINFO sInPortTunneledInfo;
	OMX_PORT_TUNNELEDINFO sOutPortTunneledInfo[2];
	OMX_PARAM_CONTENTURITYPE *pDemuxerChnAttr;//struct demuxer_chn_attr sDemuxerChnAttr;

	OMX_S32   nEos;
	OMX_S32  sActiveStreamIndex[2];
	OMX_PARAM_U32TYPE  sStreamNum[2];
	OMX_AUDIO_PARAM_PORTFORMATTYPE sAudioStream[1];
	OMX_VIDEO_PARAM_PORTFORMATTYPE sVideoStream[1];

	struct aic_parser_av_media_info sMediaInfo;
	pthread_t threadId;
	struct aic_message_queue       sMsgQue;
	struct aic_parser              *pParser;
	struct mpp_list sOutVideoEmptyPkt;
	struct mpp_list sOutVideoReadyPkt;
	struct mpp_list sOutVideoProcessingPkt;
	pthread_mutex_t sVideoPktLock;
	struct mpp_list sOutAudioEmptyPkt;
	struct mpp_list sOutAudioReadyPkt;
	struct mpp_list sOutAudioProcessingPkt;
	pthread_mutex_t sAudioPktLock;

	OMX_U32 nVideoPacketNum;
	OMX_U32 nAudioPacketNum;
	OMX_U32 nSendVideoPacketNum;
	OMX_U32 nSendAudioPacketNum;
	OMX_U32 nLeftReadyVideoPktFrameWhenCompoentExitNum;
	OMX_U32 nLeftReadyAudioPktFrameWhenCompoentExitNum;
	OMX_U32 nSendBackVideoPacketNum;
	OMX_U32 nSendBackAudioPacketNum;

	OMX_S32 nSeekFlag;
	OMX_S32 nNeedPeek;
	OMX_S32 nSkipTrack;
} DEMUXER_DATA_TYPE;

#endif
