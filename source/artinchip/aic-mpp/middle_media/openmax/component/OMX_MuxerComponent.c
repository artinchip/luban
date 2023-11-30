/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: OMX_MuxerComponent
*/
#include "OMX_MuxerComponent.h"

#define  aic_pthread_mutex_lock(mutex)\
{\
	pthread_mutex_lock(mutex);\
}

#define aic_pthread_mutex_unlock(mutex)\
{\
	pthread_mutex_unlock(mutex);\
}

#define OMX_MuxerListEmpty(list,mutex)\
({\
	int ret = 0;\
	aic_pthread_mutex_lock(&mutex);\
	ret = mpp_list_empty(list);\
	aic_pthread_mutex_unlock(&mutex);\
	(ret);\
})

static void* OMX_MuxerComponentThread(void* pThreadData);

static OMX_ERRORTYPE OMX_MuxerSendCommand(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_COMMANDTYPE Cmd,
		OMX_IN	OMX_U32 nParam1,
		OMX_IN	OMX_PTR pCmdData)
{
	MUXER_DATA_TYPE *pMuxerDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	struct aic_message sMsg;
	pMuxerDataType = (MUXER_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	sMsg.message_id = Cmd;
	sMsg.param = nParam1;
	sMsg.data_size = 0;

	//now not use always NULL
	if (pCmdData != NULL) {
		sMsg.data = pCmdData;
		sMsg.data_size = strlen((char*)pCmdData);
	}

	aic_msg_put(&pMuxerDataType->sMsgQue, &sMsg);
	return eError;
}

static OMX_ERRORTYPE OMX_MuxerGetParameter(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nParamIndex,
		OMX_INOUT OMX_PTR pComponentParameterStructure)
{
	MUXER_DATA_TYPE *pMuxerDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	pMuxerDataType = (MUXER_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

	switch (nParamIndex) {
		case OMX_IndexParamPortDefinition: {
			OMX_PARAM_PORTDEFINITIONTYPE *port = (OMX_PARAM_PORTDEFINITIONTYPE*)pComponentParameterStructure;
			if (port->nPortIndex == MUX_PORT_VIDEO_INDEX) {
				memcpy(port,&pMuxerDataType->sInPortDef[MUX_PORT_VIDEO_INDEX],sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
			} else if (port->nPortIndex == MUX_PORT_AUDIO_INDEX) {
				memcpy(port,&pMuxerDataType->sInPortDef[MUX_PORT_AUDIO_INDEX],sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
			} else {
				eError = OMX_ErrorBadParameter;
			}
			break;
		}
		case OMX_IndexParamVideoAvc:
			break;
		case OMX_IndexParamVideoProfileLevelQuerySupported:
			break;
		case OMX_IndexParamVideoPortFormat:
			break;
		case OMX_IndexParamVideoProfileLevelCurrent:
			break;
		case OMX_IndexParamVideoMpeg4:
			break;
		case OMX_IndexParamCompBufferSupplier: {
			OMX_PARAM_BUFFERSUPPLIERTYPE *sBufferSupplier = (OMX_PARAM_BUFFERSUPPLIERTYPE*)pComponentParameterStructure;
			if (sBufferSupplier->nPortIndex == MUX_PORT_VIDEO_INDEX) {
				sBufferSupplier->eBufferSupplier = pMuxerDataType->sInBufSupplier[MUX_PORT_VIDEO_INDEX].eBufferSupplier;
			} else if (sBufferSupplier->nPortIndex == MUX_PORT_AUDIO_INDEX) {
				sBufferSupplier->eBufferSupplier = pMuxerDataType->sInBufSupplier[MUX_PORT_AUDIO_INDEX].eBufferSupplier;
			} else {
				loge("error nPortIndex\n");
				eError = OMX_ErrorBadPortIndex;
			}
			break;
		}
		default:
			eError = OMX_ErrorNotImplemented;
		 break;
	}

	return eError;
}

static OMX_ERRORTYPE OMX_MuxerSetParameter(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_IN	OMX_PTR pComponentParameterStructure)
{
	MUXER_DATA_TYPE *pMuxerDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_S32 index = (OMX_S32)nIndex;
	// enum mpp_codec_type eCodecType;
	// enum mpp_pixel_format ePixFormat;
	pMuxerDataType = (MUXER_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	switch (index) {
	case OMX_IndexParamPortDefinition: {
		//1 video codec and video info
		OMX_PARAM_PORTDEFINITIONTYPE *sPortDefine = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;
		if (sPortDefine->nPortIndex == MUX_PORT_VIDEO_INDEX) {
			pMuxerDataType->sMediaInfo.has_video = 1;
			if (sPortDefine->format.video.eCompressionFormat != OMX_VIDEO_CodingMJPEG) {
				loge("now only support OMX_VIDEO_CodingMJPEG");
				break;
			}
			pMuxerDataType->sMediaInfo.video_stream.codec_type = MPP_CODEC_VIDEO_DECODER_MJPEG;
			pMuxerDataType->sMediaInfo.video_stream.width = sPortDefine->format.video.nFrameHeight;
			pMuxerDataType->sMediaInfo.video_stream.height = sPortDefine->format.video.nFrameHeight;
			pMuxerDataType->sMediaInfo.video_stream.frame_rate = sPortDefine->format.video.xFramerate;
			pMuxerDataType->sMediaInfo.video_stream.bit_rate = sPortDefine->format.video.nBitrate;
		} else if ((sPortDefine->nPortIndex == MUX_PORT_AUDIO_INDEX)) {
			pMuxerDataType->sMediaInfo.has_audio = 1;
			pMuxerDataType->sMediaInfo.audio_stream.codec_type = sPortDefine->format.audio.eEncoding;
		}
		//2 audio codec
		break;
	}
	case OMX_IndexParamAudioPcm:
		// audio info
		break;
	case OMX_IndexParamAudioAac:
		// audio info
		break;
	case OMX_IndexParamAudioMp3: {
		// audio info
		OMX_AUDIO_PARAM_MP3TYPE * mp3 = (OMX_AUDIO_PARAM_MP3TYPE *)pComponentParameterStructure;
		pMuxerDataType->sMediaInfo.audio_stream.bit_rate = mp3->nBitRate;
		pMuxerDataType->sMediaInfo.audio_stream.nb_channel = mp3->nChannels;
		pMuxerDataType->sMediaInfo.audio_stream.sample_rate = mp3->nSampleRate;
		pMuxerDataType->sMediaInfo.audio_stream.bits_per_sample = 16;
		break;
	}
	case OMX_IndexParamContentURI: {
		// file path
		OMX_PARAM_CONTENTURITYPE * pContentURI = (OMX_PARAM_CONTENTURITYPE *)pComponentParameterStructure;
		pMuxerDataType->pContentUri =(OMX_PARAM_CONTENTURITYPE *)mpp_alloc(pContentURI->nSize);
		memcpy(pMuxerDataType->pContentUri,pContentURI,pContentURI->nSize);
		break;
	}
	case OMX_IndexVendorMuxerRecorderFileInfo: {
		OMX_PARAM_RECORDERFILEINFO *pRecorderFileInfo = (OMX_PARAM_RECORDERFILEINFO *)pComponentParameterStructure;
		if (pRecorderFileInfo->nMuxerType != AIC_MUXER_TYPE_MP4) {
			loge("not suport muxer type, now only support mp4");
			eError = OMX_ErrorBadParameter;
			break;
		}
		pMuxerDataType->nMaxDuration = pRecorderFileInfo->nDuration;
		pMuxerDataType->nMuxerType = pRecorderFileInfo->nMuxerType;
		pMuxerDataType->nFileNum = pRecorderFileInfo->nFileNum;
 		break;
	}
	default:
		 break;
	}

	return eError;
}

static OMX_ERRORTYPE OMX_MuxerGetConfig(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_INOUT OMX_PTR pComponentConfigStructure)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	return eError;
}

static OMX_ERRORTYPE OMX_MuxerSetConfig(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_IN	OMX_PTR pComponentConfigStructure)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	return eError;
}

static OMX_ERRORTYPE OMX_MuxerGetState(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_OUT OMX_STATETYPE* pState)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	MUXER_DATA_TYPE* pMuxerDataType;
	pMuxerDataType = (MUXER_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

	aic_pthread_mutex_lock(&pMuxerDataType->sStateLock);
	*pState = pMuxerDataType->state;
	aic_pthread_mutex_unlock(&pMuxerDataType->sStateLock);
	return eError;
}

static OMX_ERRORTYPE OMX_MuxerComponentTunnelRequest(
	OMX_IN	OMX_HANDLETYPE hComp,
	OMX_IN	OMX_U32 nPort,
	OMX_IN	OMX_HANDLETYPE hTunneledComp,
	OMX_IN	OMX_U32 nTunneledPort,
	OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_PARAM_PORTDEFINITIONTYPE *pPort;
	OMX_PORT_TUNNELEDINFO *pTunneledInfo;
	OMX_PARAM_BUFFERSUPPLIERTYPE *pBufSupplier;
	MUXER_DATA_TYPE* pMuxerDataType;
	pMuxerDataType = (MUXER_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComp)->pComponentPrivate);
	if (pMuxerDataType->state != OMX_StateLoaded)
	 {
		loge("Component is not in OMX_StateLoaded,it is in%d,it can not tunnel\n",pMuxerDataType->state);
		return OMX_ErrorInvalidState;
	}
	if (nPort == MUX_PORT_AUDIO_INDEX) {
		pPort = &pMuxerDataType->sInPortDef[MUX_PORT_AUDIO_INDEX];
		pTunneledInfo = &pMuxerDataType->sInPortTunneledInfo[MUX_PORT_AUDIO_INDEX];
		pBufSupplier = &pMuxerDataType->sInBufSupplier[MUX_PORT_AUDIO_INDEX];
	} else if (nPort == MUX_PORT_VIDEO_INDEX) {
		pPort = &pMuxerDataType->sInPortDef[MUX_PORT_VIDEO_INDEX];
		pTunneledInfo = &pMuxerDataType->sInPortTunneledInfo[MUX_PORT_VIDEO_INDEX];
		pBufSupplier = &pMuxerDataType->sInBufSupplier[MUX_PORT_VIDEO_INDEX];
	} else {
		loge("component can not find port :%d\n",nPort);
		return OMX_ErrorBadParameter;
	}

	// cancle setup tunnel
	if (NULL == hTunneledComp && 0 == nTunneledPort && NULL == pTunnelSetup) {
			pTunneledInfo->nTunneledFlag = OMX_FALSE;
			pTunneledInfo->nTunnelPortIndex	= nTunneledPort;
			pTunneledInfo->pTunneledComp = hTunneledComp;
		return OMX_ErrorNone;
	}

	if (pPort->eDir == OMX_DirOutput) {
		pTunneledInfo->nTunnelPortIndex = nTunneledPort;
		pTunneledInfo->pTunneledComp = hTunneledComp;
		pTunneledInfo->nTunneledFlag = OMX_TRUE;
		pTunnelSetup->nTunnelFlags = 0;
		pTunnelSetup->eSupplier = pBufSupplier->eBufferSupplier;
	} else if (pPort->eDir == OMX_DirInput) {
		OMX_PARAM_PORTDEFINITIONTYPE sTunneledPort;
		OMX_PARAM_BUFFERSUPPLIERTYPE sBuffSupplier;
		sTunneledPort.nPortIndex = nTunneledPort;
		sBuffSupplier.nPortIndex = nTunneledPort;
		if (pTunnelSetup->eSupplier == OMX_BufferSupplyMax) {
			loge("both ports are input.\n");
			return OMX_ErrorPortsNotCompatible;
		}
		OMX_GetParameter(hTunneledComp, OMX_IndexParamPortDefinition,&sTunneledPort);
		if (pPort->eDomain != sTunneledPort.eDomain) {
			loge("ports domain are not compatible: %d %d.\n",
			pPort->eDomain, sTunneledPort.eDomain);
			return OMX_ErrorPortsNotCompatible;
		}
		if (sTunneledPort.eDir != OMX_DirOutput) {
			loge("both ports are input.\n");
			return OMX_ErrorPortsNotCompatible;
		}

		//negotiate buffer supplier
		OMX_GetParameter(hTunneledComp, OMX_IndexParamCompBufferSupplier,&sBuffSupplier);
		if (sBuffSupplier.eBufferSupplier != pTunnelSetup->eSupplier) {
			loge("out_port and in_port supplier are different,please check code!!!!\n");
			return OMX_ErrorPortsNotCompatible;
		}
		pTunneledInfo->nTunnelPortIndex = nTunneledPort;
		pTunneledInfo->pTunneledComp = hTunneledComp;
		pTunneledInfo->nTunneledFlag = OMX_TRUE;
		pBufSupplier->eBufferSupplier = pTunnelSetup->eSupplier;
	} else {
		loge("port is neither output nor input.\n");
		return OMX_ErrorPortsNotCompatible;
	}
	return eError;
}

static OMX_ERRORTYPE OMX_MuxerEmptyThisBuffer(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	MUXER_DATA_TYPE* pMuxerDataType;
	MUXER_IN_PACKET *pktNode;
	struct aic_message sMsg;
	int nPortId;
	pMuxerDataType = (MUXER_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

	nPortId = pBuffer->nOutputPortIndex;
	if (nPortId != MUX_PORT_AUDIO_INDEX && nPortId != MUX_PORT_VIDEO_INDEX) {
		loge("OMX_ErrorBadParameter\n");
		return OMX_ErrorBadParameter;
	}

	aic_pthread_mutex_lock(&pMuxerDataType->sStateLock);
	if (pMuxerDataType->state != OMX_StateExecuting) {
		logw("component is not in OMX_StateExecuting,it is in [%d]!!!\n",pMuxerDataType->state);
		aic_pthread_mutex_unlock(&pMuxerDataType->sStateLock);
		return OMX_ErrorIncorrectStateOperation;
	}

	if (OMX_MuxerListEmpty(&pMuxerDataType->sInEmptyPkt,pMuxerDataType->sInPktLock)) {
		MUXER_IN_PACKET *pktNode = (MUXER_IN_PACKET*)mpp_alloc(sizeof(MUXER_IN_PACKET));
		if (NULL == pktNode) {
			loge("OMX_ErrorInsufficientResources\n");
			aic_pthread_mutex_unlock(&pMuxerDataType->sStateLock);
			return OMX_ErrorInsufficientResources;
		}
		memset(pktNode,0x00,sizeof(MUXER_IN_PACKET));
		aic_pthread_mutex_lock(&pMuxerDataType->sInPktLock);
		mpp_list_add_tail(&pktNode->sList, &pMuxerDataType->sInEmptyPkt);
		aic_pthread_mutex_unlock(&pMuxerDataType->sInPktLock);
		pMuxerDataType->nInPktNodeNum++;
	}

	if (pMuxerDataType->sInPortTunneledInfo[nPortId].nTunneledFlag) {// now Tunneled and non-Tunneled are same
		aic_pthread_mutex_lock(&pMuxerDataType->sInPktLock);
		pktNode = mpp_list_first_entry(&pMuxerDataType->sInEmptyPkt, MUXER_IN_PACKET, sList);
		memcpy(&pktNode->pkt,pBuffer->pOutputPortPrivate,sizeof(struct aic_av_packet));
		mpp_list_del(&pktNode->sList);
		mpp_list_add_tail(&pktNode->sList, &pMuxerDataType->sInReadyPkt);
		pMuxerDataType->nReceiveFrameNum++;
		//loge("nReceiveFrameNum:%d\n",pMuxerDataType->nReceiveFrameNum);
		aic_pthread_mutex_unlock(&pMuxerDataType->sInPktLock);

	} else { // now Tunneled and non-Tunneled are same
		aic_pthread_mutex_lock(&pMuxerDataType->sInPktLock);
		pktNode = mpp_list_first_entry(&pMuxerDataType->sInEmptyPkt, MUXER_IN_PACKET, sList);
		memcpy(&pktNode->pkt,pBuffer->pOutputPortPrivate,sizeof(struct aic_av_packet));
		mpp_list_del(&pktNode->sList);
		mpp_list_add_tail(&pktNode->sList, &pMuxerDataType->sInReadyPkt);
		pMuxerDataType->nReceiveFrameNum++;
		logi("nReceiveFrameNum:%d\n",pMuxerDataType->nReceiveFrameNum);
		aic_pthread_mutex_unlock(&pMuxerDataType->sInPktLock);
	}

	// aic_pthread_mutex_lock(&pMuxerDataType->sWaitReayFrameLock);
	// if (pMuxerDataType->nWaitReayFrameFlag) {
	 	sMsg.message_id = OMX_CommandNops;
	 	sMsg.data_size = 0;
	 	aic_msg_put(&pMuxerDataType->sMsgQue, &sMsg);
	// 	pMuxerDataType->nWaitReayFrameFlag = 0;
	// }
	// aic_pthread_mutex_unlock(&pMuxerDataType->sWaitReayFrameLock);
	aic_pthread_mutex_unlock(&pMuxerDataType->sStateLock);
	return eError;
}

static OMX_ERRORTYPE OMX_MuxerFillThisBuffer(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	return eError;
}

static OMX_ERRORTYPE OMX_MuxerSetCallbacks(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_CALLBACKTYPE* pCallbacks,
		OMX_IN	OMX_PTR pAppData)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	MUXER_DATA_TYPE* pMuxerDataType;
	pMuxerDataType = (MUXER_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	pMuxerDataType->pCallbacks = pCallbacks;
	pMuxerDataType->pAppData = pAppData;
	return eError;
}

OMX_ERRORTYPE OMX_MuxerComponentDeInit(OMX_IN	OMX_HANDLETYPE hComponent)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_COMPONENTTYPE *pComp;
	MUXER_DATA_TYPE *pMuxerDataType;
	pComp = (OMX_COMPONENTTYPE *)hComponent;
	struct aic_message sMsg;

	pMuxerDataType = (MUXER_DATA_TYPE *)pComp->pComponentPrivate;
	aic_pthread_mutex_lock(&pMuxerDataType->sStateLock);
	if (pMuxerDataType->state != OMX_StateLoaded) {
		logd("compoent is in %d,but not in OMX_StateLoaded(1),can ont FreeHandle.\n",pMuxerDataType->state);
		aic_pthread_mutex_unlock(&pMuxerDataType->sStateLock);
		return OMX_ErrorInvalidState;
	}
	aic_pthread_mutex_unlock(&pMuxerDataType->sStateLock);
	sMsg.message_id = OMX_CommandStop;
	sMsg.data_size = 0;
	aic_msg_put(&pMuxerDataType->sMsgQue, &sMsg);
	pthread_join(pMuxerDataType->threadId, (void*)&eError);

	aic_pthread_mutex_lock(&pMuxerDataType->sInPktLock);
	if (!mpp_list_empty(&pMuxerDataType->sInEmptyPkt)) {
		MUXER_IN_PACKET  *pPktNode = NULL,*pPktNode1 = NULL;
		mpp_list_for_each_entry_safe(pPktNode, pPktNode1, &pMuxerDataType->sInEmptyPkt, sList) {
			mpp_list_del(&pPktNode->sList);
			mpp_free(pPktNode);
		}
	}
	aic_pthread_mutex_unlock(&pMuxerDataType->sInPktLock);
	pthread_mutex_destroy(&pMuxerDataType->sInPktLock);
	pthread_mutex_destroy(&pMuxerDataType->sStateLock);
	aic_msg_destroy(&pMuxerDataType->sMsgQue);
	if (pMuxerDataType->pMuxer) {
		aic_muxer_destroy(pMuxerDataType->pMuxer);
		pMuxerDataType->pMuxer = NULL;
	}
	mpp_free(pMuxerDataType);
	pMuxerDataType = NULL;
	logd("OMX_MuxerComponentDeInit\n");
	return eError;
}

OMX_ERRORTYPE OMX_MuxerComponentInit(OMX_IN	OMX_HANDLETYPE hComponent)
{
	OMX_COMPONENTTYPE *pComp;
	MUXER_DATA_TYPE *pMuxerDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_U32 err;
	OMX_U32 i;
	OMX_U32 cnt = 0;
	OMX_PARAM_PORTDEFINITIONTYPE *pAudioPort,*pVideoPort;
	OMX_PARAM_BUFFERSUPPLIERTYPE *pAudioBufSupplier,*pVideoBufSupplier;

	OMX_S8 nMsgCreateOk = 0;
	OMX_S8 nInPktLockInitOk = 0;
	OMX_S8 nSateLockInitOk = 0;

	logd("OMX_MuxerComponentInit....");

	pComp = (OMX_COMPONENTTYPE *)hComponent;

	pMuxerDataType = (MUXER_DATA_TYPE *)mpp_alloc(sizeof(MUXER_DATA_TYPE));

	if (NULL == pMuxerDataType)  {
		loge("mpp_alloc(sizeof(MuxerDATATYPE) fail!\n");
		return OMX_ErrorInsufficientResources;
	}

	memset(pMuxerDataType, 0x0, sizeof(MUXER_DATA_TYPE));
	pComp->pComponentPrivate	= (void*) pMuxerDataType;
	pMuxerDataType->state			= OMX_StateLoaded;
	pMuxerDataType->hSelf			= pComp;

	pComp->SetCallbacks 		= OMX_MuxerSetCallbacks;
	pComp->SendCommand			= OMX_MuxerSendCommand;
	pComp->GetState 			= OMX_MuxerGetState;
	pComp->GetParameter  		= OMX_MuxerGetParameter;
	pComp->SetParameter 		= OMX_MuxerSetParameter;
	pComp->GetConfig			= OMX_MuxerGetConfig;
	pComp->SetConfig			= OMX_MuxerSetConfig;
	pComp->ComponentTunnelRequest = OMX_MuxerComponentTunnelRequest;
	pComp->ComponentDeInit		= OMX_MuxerComponentDeInit;
	pComp->FillThisBuffer		= OMX_MuxerFillThisBuffer;
	pComp->EmptyThisBuffer		= OMX_MuxerEmptyThisBuffer;

	pMuxerDataType->sPortParam.nPorts = 2;
	pMuxerDataType->sPortParam.nStartPortNumber = 0x0;
	pAudioPort = &pMuxerDataType->sInPortDef[MUX_PORT_AUDIO_INDEX];
	pVideoPort = &pMuxerDataType->sInPortDef[MUX_PORT_VIDEO_INDEX];
	pAudioBufSupplier = &pMuxerDataType->sInBufSupplier[MUX_PORT_AUDIO_INDEX];
	pVideoBufSupplier = &pMuxerDataType->sInBufSupplier[MUX_PORT_VIDEO_INDEX];

	pAudioPort->nPortIndex = MUX_PORT_AUDIO_INDEX;
	pAudioPort->bPopulated = OMX_TRUE;
	pAudioPort->bEnabled = OMX_TRUE;
	pAudioPort->eDomain = OMX_PortDomainAudio;
	pAudioPort->eDir = OMX_DirInput;

	pVideoPort->nPortIndex = MUX_PORT_VIDEO_INDEX;
	pVideoPort->bPopulated = OMX_TRUE;
	pVideoPort->bEnabled = OMX_TRUE;
	pVideoPort->eDomain = OMX_PortDomainVideo;
	pVideoPort->eDir = OMX_DirInput;

	pAudioBufSupplier->nPortIndex = MUX_PORT_AUDIO_INDEX;
	pAudioBufSupplier->eBufferSupplier = OMX_BufferSupplyOutput;
	pVideoBufSupplier->nPortIndex = MUX_PORT_VIDEO_INDEX;
	pVideoBufSupplier->eBufferSupplier = OMX_BufferSupplyOutput;

	mpp_list_init(&pMuxerDataType->sInEmptyPkt);
	mpp_list_init(&pMuxerDataType->sInReadyPkt);
	mpp_list_init(&pMuxerDataType->sInProcessedPkt);

	cnt = 0;
	for(i =0 ; i < MUX_PACKET_NUM_MAX; i++) {
		MUXER_IN_PACKET *pPktNode = (MUXER_IN_PACKET*)mpp_alloc(sizeof(MUXER_IN_PACKET));
		if (NULL == pPktNode) {
			break;
		}
		memset(pPktNode,0x00,sizeof(MUXER_IN_PACKET));
		mpp_list_add_tail(&pPktNode->sList, &pMuxerDataType->sInEmptyPkt);
		cnt++;
	}
	if (cnt == 0) {
		loge("mpp_alloc empty video node fail\n");
		eError = OMX_ErrorInsufficientResources;
		goto _EXIT;
	}

	if (aic_msg_create(&pMuxerDataType->sMsgQue)<0)
	 {
		loge("aic_msg_create fail!\n");
		eError = OMX_ErrorInsufficientResources;
		goto _EXIT;
	}
	nMsgCreateOk = 1;
	if (pthread_mutex_init(&pMuxerDataType->sInPktLock, NULL)) {
		loge("pthread_mutex_init fail!\n");
		goto _EXIT;
	}
	nInPktLockInitOk = 1;
	if (pthread_mutex_init(&pMuxerDataType->sStateLock, NULL)) {
		loge("pthread_mutex_init fail!\n");
		goto _EXIT;
	}
	nSateLockInitOk = 1;

	// Create the component thread
	err = pthread_create(&pMuxerDataType->threadId, NULL, OMX_MuxerComponentThread, pMuxerDataType);
	if (err || !pMuxerDataType->threadId) {
		loge("pthread_create fail!\n");
		eError = OMX_ErrorInsufficientResources;
		goto _EXIT;
	}
	return eError;

_EXIT:
	if (nSateLockInitOk)
		pthread_mutex_destroy(&pMuxerDataType->sStateLock);
	if (nInPktLockInitOk)
		pthread_mutex_destroy(&pMuxerDataType->sInPktLock);
	if (nMsgCreateOk)
		aic_msg_destroy(&pMuxerDataType->sMsgQue);
	if (!mpp_list_empty(&pMuxerDataType->sInEmptyPkt)) {
		MUXER_IN_PACKET *pPktNode=NULL,*pPktNode1=NULL;
		mpp_list_for_each_entry_safe(pPktNode, pPktNode1, &pMuxerDataType->sInEmptyPkt, sList) {
			mpp_list_del(&pPktNode->sList);
			mpp_free(pPktNode);
		}
	}
	if (pMuxerDataType) {
		mpp_free(pMuxerDataType);
		pMuxerDataType = NULL;
	}
	return eError;
}
static void OMX_MuxerEventNotify(
		MUXER_DATA_TYPE * pMuxerDataType,
		OMX_EVENTTYPE event,
		OMX_U32 nData1,
		OMX_U32 nData2,
		OMX_PTR pEventData)
{
	if (pMuxerDataType && pMuxerDataType->pCallbacks && pMuxerDataType->pCallbacks->EventHandler)  {
		pMuxerDataType->pCallbacks->EventHandler(
					pMuxerDataType->hSelf,
					pMuxerDataType->pAppData,event,
					nData1, nData2, pEventData);
	}

}

static void OMX_MuxerStateChangeToInvalid(MUXER_DATA_TYPE * pMuxerDataType)
{
	pMuxerDataType->state = OMX_StateInvalid;
	OMX_MuxerEventNotify(pMuxerDataType
						,OMX_EventError
						,OMX_ErrorInvalidState,0,NULL);
	OMX_MuxerEventNotify(pMuxerDataType
						,OMX_EventCmdComplete
						,OMX_CommandStateSet
						,pMuxerDataType->state,NULL);
}

static void OMX_MuxerStateChangeToIdle(MUXER_DATA_TYPE * pMuxerDataType)
{

	if (OMX_StateLoaded == pMuxerDataType->state) {

	} else if (OMX_StateExecuting == pMuxerDataType->state) {
		if (pMuxerDataType->pMuxer) {
			MUXER_IN_PACKET *pPktNode = NULL;
			int ret = 0;
			int num = 0;
			while (!OMX_MuxerListEmpty(&pMuxerDataType->sInReadyPkt,pMuxerDataType->sInPktLock)) {
				aic_pthread_mutex_lock(&pMuxerDataType->sInPktLock);
				pPktNode = mpp_list_first_entry(&pMuxerDataType->sInReadyPkt,MUXER_IN_PACKET,sList);
				aic_pthread_mutex_unlock(&pMuxerDataType->sInPktLock);
				ret = aic_muxer_write_packet(pMuxerDataType->pMuxer,&pPktNode->pkt);
				if (ret == 0) {
					aic_pthread_mutex_lock(&pMuxerDataType->sInPktLock);
					mpp_list_del(&pPktNode->sList);
					mpp_list_add_tail(&pPktNode->sList,&pMuxerDataType->sInProcessedPkt);
					aic_pthread_mutex_unlock(&pMuxerDataType->sInPktLock);
					num++;
					pMuxerDataType->nCurFileHasWriteFrameNum++;
				} else if (ret == -2) {//AIC_NO_SPACE
					loge("AIC_NO_SPACE\n");
				} else {
					loge("ohter error\n");
				}
			}
			loge("num:%d,nCurFileHasWriteFrameNum:%d\n",num,pMuxerDataType->nCurFileHasWriteFrameNum);

			aic_muxer_write_trailer(pMuxerDataType->pMuxer);
			aic_muxer_destroy(pMuxerDataType->pMuxer);
			pMuxerDataType->pMuxer = NULL;
		}

		while(!OMX_MuxerListEmpty(&pMuxerDataType->sInProcessedPkt,pMuxerDataType->sInPktLock)) {
			int ret = 0;
			MUXER_IN_PACKET *pPktNode = NULL;
			OMX_BUFFERHEADERTYPE sBuffHead;
			OMX_PORT_TUNNELEDINFO *pTunneldInfo = NULL;

			aic_pthread_mutex_lock(&pMuxerDataType->sInPktLock);
			pPktNode = mpp_list_first_entry(&pMuxerDataType->sInProcessedPkt,MUXER_IN_PACKET,sList);
			aic_pthread_mutex_unlock(&pMuxerDataType->sInPktLock);
			if (pPktNode->pkt.type == MPP_MEDIA_TYPE_VIDEO) {
				pTunneldInfo = &pMuxerDataType->sInPortTunneledInfo[MUX_PORT_VIDEO_INDEX];
				sBuffHead.nOutputPortIndex = VENC_PORT_OUT_INDEX;
				sBuffHead.nInputPortIndex = MUX_PORT_VIDEO_INDEX;
				sBuffHead.pOutputPortPrivate = (OMX_U8 *)&pPktNode->pkt;
			} else if (pPktNode->pkt.type == MPP_MEDIA_TYPE_AUDIO) {
				pTunneldInfo = &pMuxerDataType->sInPortTunneledInfo[MUX_PORT_AUDIO_INDEX];
				sBuffHead.nOutputPortIndex = VENC_PORT_OUT_INDEX;
				sBuffHead.nInputPortIndex = MUX_PORT_AUDIO_INDEX;
				sBuffHead.pOutputPortPrivate = (OMX_U8 *)&pPktNode->pkt;
			} else {
				loge("pkt.type error %d",pPktNode->pkt.type);
			}

			if (pTunneldInfo->nTunneledFlag) {
				ret = OMX_FillThisBuffer(pTunneldInfo->pTunneledComp,&sBuffHead);
			} else {
				if (pMuxerDataType->pCallbacks != NULL && pMuxerDataType->pCallbacks->EmptyBufferDone != NULL) {
						ret = pMuxerDataType->pCallbacks->EmptyBufferDone(pMuxerDataType->hSelf,pMuxerDataType->pAppData,&sBuffHead);
					}
			}
			if (ret == 0) {
				aic_pthread_mutex_lock(&pMuxerDataType->sInPktLock);
				mpp_list_del(&pPktNode->sList);
				mpp_list_add_tail(&pPktNode->sList, &pMuxerDataType->sInEmptyPkt);
				aic_pthread_mutex_unlock(&pMuxerDataType->sInPktLock);
				logd("give back frame to venc ok");
			} else {
				loge("give back frame to venc fail");
			}

 		}

	} else if (OMX_StatePause == pMuxerDataType->state) {

	} else  {
		OMX_MuxerEventNotify(pMuxerDataType
						,OMX_EventError
						,OMX_ErrorIncorrectStateTransition
						,pMuxerDataType->state,NULL);
		loge("OMX_ErrorIncorrectStateTransition\n");
		return;
	}
	pMuxerDataType->state = OMX_StateIdle;
	OMX_MuxerEventNotify(pMuxerDataType
						,OMX_EventCmdComplete
						,OMX_CommandStateSet
						,pMuxerDataType->state,NULL);
}

static void OMX_MuxerStateChangeToLoaded(MUXER_DATA_TYPE * pMuxerDataType)
{
	if (OMX_StateIdle == pMuxerDataType->state) {
		//wait for	all out port packet from other component or app to back.
		// logi("Before OMX_MuxerComponentThread exit,it must wait for sOutAudioProcessingPkt empty\n");
		// while(!OMX_MuxerListEmpty(&pMuxerDataType->sOutAudioProcessingPkt,pMuxerDataType->sAudioPktLock)) {
		// 	usleep(1000);
		// }
		// logi("Before OMX_MuxerComponentThread exit,it must wait for sOutVideoProcessingPkt empty\n");
		// while(!OMX_MuxerListEmpty(&pMuxerDataType->sOutVideoProcessingPkt,pMuxerDataType->sVideoPktLock)) {
		// 	usleep(1000);
		// }
		// logi("OMX_MuxerStateChangeToLoaded\n");

		pMuxerDataType->state = OMX_StateLoaded;
		OMX_MuxerEventNotify(pMuxerDataType
						,OMX_EventCmdComplete
						,OMX_CommandStateSet
						,pMuxerDataType->state,NULL);
	} else {
		OMX_MuxerEventNotify(pMuxerDataType
						,OMX_EventError
						,OMX_ErrorIncorrectStateTransition
						, pMuxerDataType->state,NULL);
		loge("OMX_ErrorIncorrectStateTransition\n");
	}

}

static void OMX_MuxerStateChangeToExecuting(MUXER_DATA_TYPE * pMuxerDataType)
{
	if (OMX_StateIdle == pMuxerDataType->state) {
		// if (NULL == pMuxerDataType->pParser) {
		// 	OMX_MuxerEventNotify(pMuxerDataType
		// 					,OMX_EventError
		// 					,OMX_ErrorIncorrectStateTransition
		// 					,pMuxerDataType->state,NULL);
		// 	loge("pMuxerDataType->pParser is not created,please set param OMX_IndexParamContentURI!!!!!\n");
		// 	return;
		// }
	} else if (OMX_StatePause == pMuxerDataType->state) {
	//
	} else {
		OMX_MuxerEventNotify(pMuxerDataType
						,OMX_EventError
						,OMX_ErrorIncorrectStateTransition
						,pMuxerDataType->state,NULL);
		loge("OMX_ErrorIncorrectStateTransition\n");
		return;
	}
	pMuxerDataType->state = OMX_StateExecuting;

}

static void OMX_MuxerStateChangeToPause(MUXER_DATA_TYPE * pMuxerDataType)
{
	if (OMX_StateExecuting == pMuxerDataType->state) {

	} else {
		OMX_MuxerEventNotify(pMuxerDataType
						,OMX_EventError
						,OMX_ErrorIncorrectStateTransition
						,pMuxerDataType->state,NULL);
		logd("OMX_ErrorIncorrectStateTransition\n");
		return;
	}
	pMuxerDataType->state = OMX_StatePause;

}

static void* OMX_MuxerComponentThread(void* pThreadData)
{
	struct aic_message message;
	OMX_S32 nCmd;		//OMX_COMMANDTYPE
	OMX_S32 nCmdData;	//OMX_STATETYPE
	MUXER_DATA_TYPE* pMuxerDataType = (MUXER_DATA_TYPE*)pThreadData;
	OMX_S32 ret;
	MUXER_IN_PACKET *pPktNode;
	int nCreateNewFileFlag = 1;
	int nFileCount = 0;
	int nVideoDuration = 0;
	int nAudioDuration = 0;
	int64_t nFirsrtVideoPktPts = -1;
	int64_t nFirsrtAudioPktPts = -1;

	while(1) {
_AIC_MSG_GET_:
		if (aic_msg_get(&pMuxerDataType->sMsgQue, &message) == 0) {
			nCmd = message.message_id;
			nCmdData = message.param;
			logi("nCmd:%d, nCmdData:%d\n",nCmd,nCmdData);
 			if (OMX_CommandStateSet == nCmd) {
				aic_pthread_mutex_lock(&pMuxerDataType->sStateLock);
				if (pMuxerDataType->state == (OMX_STATETYPE)(nCmdData)) {
					OMX_MuxerEventNotify(pMuxerDataType,OMX_EventError,OMX_ErrorSameState,0,NULL);
					aic_pthread_mutex_unlock(&pMuxerDataType->sStateLock);
					continue;
				}
				switch((OMX_STATETYPE)(nCmdData)) {
				case OMX_StateInvalid:
					OMX_MuxerStateChangeToInvalid(pMuxerDataType);
					break;
				case OMX_StateLoaded://idel->loaded means stop
					OMX_MuxerStateChangeToLoaded(pMuxerDataType);
					break;
				case OMX_StateIdle:
					OMX_MuxerStateChangeToIdle(pMuxerDataType);
					break;
				case OMX_StateExecuting:
					OMX_MuxerStateChangeToExecuting(pMuxerDataType);
					break;
				case OMX_StatePause:
					OMX_MuxerStateChangeToPause(pMuxerDataType);
					break;
				case OMX_StateWaitForResources:
					break;
				default:
					break;
				}
				aic_pthread_mutex_unlock(&pMuxerDataType->sStateLock);
 			} else if (OMX_CommandFlush == nCmd) {

			} else if (OMX_CommandPortDisable == nCmd) {

			} else if (OMX_CommandPortEnable == nCmd) {

			} else if (OMX_CommandMarkBuffer == nCmd) {

			} else if (OMX_CommandStop == nCmd) {
				logi("OMX_MuxerComponentThread ready to exit!!!\n");
				goto _EXIT;
			} else if (OMX_CommandNops == nCmd) {

			} else {

			}
		}

		if (pMuxerDataType->state != OMX_StateExecuting) {
			aic_msg_wait_new_msg(&pMuxerDataType->sMsgQue, 0);
			continue;
		}

		// process data
		if (nCreateNewFileFlag) {
			//close current file
			if (pMuxerDataType->pMuxer) {
				aic_muxer_write_trailer(pMuxerDataType->pMuxer);
				aic_muxer_destroy(pMuxerDataType->pMuxer);
				loge("nCurFileHasWriteFrameNum:%d\n",pMuxerDataType->nCurFileHasWriteFrameNum);

			}
			pMuxerDataType->nCurFileHasWriteFrameNum = 0;
			//notity need next file
			OMX_MuxerEventNotify(pMuxerDataType,OMX_EventMuxerNeedNextFile,0,0,NULL);

			// open next file
			if (aic_muxer_create(pMuxerDataType->pContentUri->contentURI,&pMuxerDataType->pMuxer,pMuxerDataType->nMuxerType) != 0 || !pMuxerDataType->pMuxer) {
				loge("aic_muxer_create error");
				goto _AIC_MSG_GET_;// EXIT
			}
			if (aic_muxer_init(pMuxerDataType->pMuxer,&pMuxerDataType->sMediaInfo) != 0) {
				loge("aic_muxer_init error");
				goto _AIC_MSG_GET_;//EXIT
			}
			if (aic_muxer_write_header(pMuxerDataType->pMuxer) != 0) {
				loge("aic_muxer_write_header error");
				goto _AIC_MSG_GET_;//EXIT
			}
			nCreateNewFileFlag = 0;
			nFirsrtVideoPktPts = -1;
			nFirsrtAudioPktPts = -1;
		}

		// return pkt to encoder
		while(!OMX_MuxerListEmpty(&pMuxerDataType->sInProcessedPkt,pMuxerDataType->sInPktLock)) {
			MUXER_IN_PACKET *pPktNode;
			OMX_BUFFERHEADERTYPE sBuffHead;
			OMX_PORT_TUNNELEDINFO *pTunneldInfo = NULL;
			ret = 0;
			aic_pthread_mutex_lock(&pMuxerDataType->sInPktLock);
			pPktNode = mpp_list_first_entry(&pMuxerDataType->sInProcessedPkt,MUXER_IN_PACKET,sList);
			aic_pthread_mutex_unlock(&pMuxerDataType->sInPktLock);
			if (pPktNode->pkt.type == MPP_MEDIA_TYPE_VIDEO) {
				pTunneldInfo = &pMuxerDataType->sInPortTunneledInfo[MUX_PORT_VIDEO_INDEX];
				sBuffHead.nOutputPortIndex = VENC_PORT_OUT_INDEX;
				sBuffHead.nInputPortIndex = MUX_PORT_VIDEO_INDEX;
				sBuffHead.pOutputPortPrivate = (OMX_U8 *)&pPktNode->pkt;
			} else if (pPktNode->pkt.type == MPP_MEDIA_TYPE_AUDIO) {
				pTunneldInfo = &pMuxerDataType->sInPortTunneledInfo[MUX_PORT_AUDIO_INDEX];
				sBuffHead.nOutputPortIndex = VENC_PORT_OUT_INDEX;
				sBuffHead.nInputPortIndex = MUX_PORT_AUDIO_INDEX;
				sBuffHead.pOutputPortPrivate = (OMX_U8 *)&pPktNode->pkt;
			} else {
				loge("pkt.type error %d",pPktNode->pkt.type);
			}

			if (pTunneldInfo->nTunneledFlag) {
				ret = OMX_FillThisBuffer(pTunneldInfo->pTunneledComp,&sBuffHead);
			} else {
				if (pMuxerDataType->pCallbacks != NULL && pMuxerDataType->pCallbacks->FillBufferDone != NULL) {
						ret = pMuxerDataType->pCallbacks->FillBufferDone(pMuxerDataType->hSelf,pMuxerDataType->pAppData,&sBuffHead);
					}
			}

			if (ret == 0) {
				aic_pthread_mutex_lock(&pMuxerDataType->sInPktLock);
				mpp_list_del(&pPktNode->sList);
				mpp_list_add_tail(&pPktNode->sList, &pMuxerDataType->sInEmptyPkt);
				aic_pthread_mutex_unlock(&pMuxerDataType->sInPktLock);
				logd("give back frame to venc ok");
			} else { // how to do ,do nothing or move to empty list,now move to  empty list
				//loge("give back frame to venc fail\n");
				break;
			}
 		}

		if (OMX_MuxerListEmpty(&pMuxerDataType->sInReadyPkt,pMuxerDataType->sInPktLock)) {
			aic_msg_wait_new_msg(&pMuxerDataType->sMsgQue, 0);
			continue;
		}

		aic_pthread_mutex_lock(&pMuxerDataType->sInPktLock);
		pPktNode = mpp_list_first_entry(&pMuxerDataType->sInReadyPkt,MUXER_IN_PACKET,sList);
		aic_pthread_mutex_unlock(&pMuxerDataType->sInPktLock);

		if (pPktNode->pkt.type == MPP_MEDIA_TYPE_VIDEO) {
			if (nFirsrtVideoPktPts == -1) {
				nFirsrtVideoPktPts = pPktNode->pkt.pts;
			}
			nVideoDuration = pPktNode->pkt.pts - nFirsrtVideoPktPts;

		} else if (pPktNode->pkt.type == MPP_MEDIA_TYPE_AUDIO) {
			if (nFirsrtAudioPktPts == -1) {
				nFirsrtAudioPktPts = pPktNode->pkt.pts;
			}
			nAudioDuration = pPktNode->pkt.pts - nFirsrtAudioPktPts;
		}

		if (pMuxerDataType->sMediaInfo.has_video && pMuxerDataType->sMediaInfo.has_audio) {
			if (nVideoDuration > pMuxerDataType->nMaxDuration && nAudioDuration > pMuxerDataType->nMaxDuration) {
				nCreateNewFileFlag = 1;
				nFileCount++;
			}
		} else if (pMuxerDataType->sMediaInfo.has_video && !pMuxerDataType->sMediaInfo.has_audio) {
			if (nVideoDuration > pMuxerDataType->nMaxDuration) {
				nCreateNewFileFlag = 1;
				nFileCount++;
			}
		} else if (!pMuxerDataType->sMediaInfo.has_video && pMuxerDataType->sMediaInfo.has_audio) {
			if (nAudioDuration > pMuxerDataType->nMaxDuration) {
				nCreateNewFileFlag = 1;
				nFileCount++;
			}
		}

		ret = aic_muxer_write_packet(pMuxerDataType->pMuxer,&pPktNode->pkt);
		if (ret == 0) {
			aic_pthread_mutex_lock(&pMuxerDataType->sInPktLock);
			mpp_list_del(&pPktNode->sList);
			mpp_list_add_tail(&pPktNode->sList,&pMuxerDataType->sInProcessedPkt);
			aic_pthread_mutex_unlock(&pMuxerDataType->sInPktLock);
			pMuxerDataType->nCurFileHasWriteFrameNum++;
		} else if (ret == -2) {//AIC_NO_SPACE
			loge("AIC_NO_SPACE\n");
			goto _AIC_MSG_GET_;
		} else {
			loge("ohter error\n");
		}

	}
_EXIT:
	return (void*)OMX_ErrorNone;
}

