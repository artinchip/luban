/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: OMX_AudioRenderComponent
*/

#include "OMX_AudioRenderComponent.h"

#define  aic_pthread_mutex_lock(mutex)\
{\
	/*loge("before lock\n");*/\
	pthread_mutex_lock(mutex);\
	/*loge("after lock\n");*/\
}

#define aic_pthread_mutex_unlock(mutex)\
{\
	/*loge("before unlock\n");*/\
	pthread_mutex_unlock(mutex);\
	/*loge("after unlock\n");*/\
}

#define OMX_AudioRenderListEmpty(list,mutex)\
({\
	int ret = 0;\
	aic_pthread_mutex_lock(&mutex);\
	ret = mpp_list_empty(list);\
	aic_pthread_mutex_unlock(&mutex);\
	(ret);\
})

#define OMX_AUDIO_RENDER_PRINT_FRAME_NUM  (30)

static OMX_ERRORTYPE OMX_AudioRenderSendCommand(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_COMMANDTYPE Cmd,
		OMX_IN	OMX_U32 nParam1,
		OMX_IN	OMX_PTR pCmdData);

static OMX_ERRORTYPE OMX_AudioRenderGetParameter(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nParamIndex,
		OMX_INOUT OMX_PTR pComponentParameterStructure);

static OMX_ERRORTYPE OMX_AudioRenderSetParameter(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_IN	OMX_PTR pComponentParameterStructure);

static OMX_ERRORTYPE OMX_AudioRenderGetConfig(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_INOUT OMX_PTR pComponentConfigStructure);

static OMX_ERRORTYPE OMX_AudioRenderSetConfig(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_IN	OMX_PTR pComponentConfigStructure);

static OMX_ERRORTYPE OMX_AudioRenderGetState(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_OUT OMX_STATETYPE* pState);

static OMX_ERRORTYPE OMX_AudioRenderComponentTunnelRequest(
	OMX_IN	OMX_HANDLETYPE hComp,
	OMX_IN	OMX_U32 nPort,
	OMX_IN	OMX_HANDLETYPE hTunneledComp,
	OMX_IN	OMX_U32 nTunneledPort,
	OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup);

static OMX_ERRORTYPE OMX_AudioRenderEmptyThisBuffer(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_BUFFERHEADERTYPE* pBuffer);

static OMX_ERRORTYPE OMX_AudioRenderFillThisBuffer(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_BUFFERHEADERTYPE* pBuffer);

static OMX_ERRORTYPE OMX_AudioRenderSetCallbacks(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_CALLBACKTYPE* pCallbacks,
		OMX_IN	OMX_PTR pAppData);

static OMX_ERRORTYPE OMX_AudioRenderSendCommand(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_COMMANDTYPE Cmd,
		OMX_IN	OMX_U32 nParam1,
		OMX_IN	OMX_PTR pCmdData)
{
	AUDIO_RENDER_DATA_TYPE *pAudioRenderDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	struct aic_message sMsg;
	pAudioRenderDataType = (AUDIO_RENDER_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	sMsg.message_id = Cmd;
	sMsg.param = nParam1;
	sMsg.data_size = 0;

	//now not use always NULL
	if (pCmdData != NULL) {
		sMsg.data = pCmdData;
		sMsg.data_size = strlen((char*)pCmdData);
	}

	aic_msg_put(&pAudioRenderDataType->sMsgQue, &sMsg);
	return eError;
}

static void* OMX_AudioRenderComponentThread(void* pThreadData);

static int  OMX_AudioRenderGiveBackAllFrames(AUDIO_RENDER_DATA_TYPE * pAudioRenderDataType);

static OMX_ERRORTYPE OMX_AudioRenderGetParameter(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nParamIndex,
		OMX_INOUT OMX_PTR pComponentParameterStructure)
{
	AUDIO_RENDER_DATA_TYPE *pAudioRenderDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;

	pAudioRenderDataType = (AUDIO_RENDER_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

	switch ((OMX_S32)nParamIndex) {
		case OMX_IndexParamPortDefinition:
			break;
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
			if (sBufferSupplier->nPortIndex == 0) {
				sBufferSupplier->eBufferSupplier = pAudioRenderDataType->sInBufSupplier[AUDIO_RENDER_PORT_IN_AUDIO_INDEX].eBufferSupplier;
			} else {
				loge("error nPortIndex\n");
				eError = OMX_ErrorBadPortIndex;
			}
			break;
		}
		case OMX_IndexVendorAudioRenderVolume: {
			OMX_S32 vol = pAudioRenderDataType->render->get_volume(pAudioRenderDataType->render);
			((OMX_PARAM_AUDIO_VOLUME *)pComponentParameterStructure)->nVolume = vol;

		break;
		}
		default:
			eError = OMX_ErrorNotImplemented;
		 break;
	}

	return eError;
}

static OMX_ERRORTYPE OMX_AudioRenderSetParameter(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nParamIndex,
		OMX_IN	OMX_PTR pComponentParameterStructure)
{
	AUDIO_RENDER_DATA_TYPE *pAudioRenderDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_PARAM_FRAMEEND *sFrameEnd;
	pAudioRenderDataType = (AUDIO_RENDER_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	if (pComponentParameterStructure == NULL) {
		loge("param error!!!\n");
		return OMX_ErrorBadParameter;
	}
	switch ((OMX_S32)nParamIndex) {
		case OMX_IndexParamPortDefinition:
			break;
		case OMX_IndexParamVideoPortFormat:
			break;
		case OMX_IndexVendorStreamFrameEnd:
			sFrameEnd = (OMX_PARAM_FRAMEEND*)pComponentParameterStructure;
			if (sFrameEnd->bFrameEnd == OMX_TRUE) {
				pAudioRenderDataType->nFrameEndFlag = OMX_TRUE;
				logi("setup nFrameEndFlag\n");
			} else {
				pAudioRenderDataType->nFrameEndFlag = OMX_FALSE;
				logi("cancel nFrameEndFlag\n");
			}
			break;
		case OMX_IndexParamVideoMpeg4:
			break;
		case OMX_IndexVendorAudioRenderVolume: {
			OMX_S32 vol = ((OMX_PARAM_AUDIO_VOLUME *)pComponentParameterStructure)->nVolume;
			if (vol < 0) {
				pAudioRenderDataType->nVolume = 0;
			} else if (vol < 101) {
				pAudioRenderDataType->nVolume = vol;
			} else {
				pAudioRenderDataType->nVolume = 100;
			}
			pAudioRenderDataType->nVolumeChange = 1;

			logd("nVolume:%d,change:%d\n",pAudioRenderDataType->nVolume,pAudioRenderDataType->nVolumeChange);

			break;
		}
		default:
		 break;
	}
	return eError;
}

static OMX_ERRORTYPE OMX_AudioRenderGetConfig(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_INOUT OMX_PTR pComponentConfigStructure)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;

	return eError;

}

static OMX_ERRORTYPE OMX_AudioRenderSetConfig(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_IN	OMX_PTR pComponentConfigStructure)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	AUDIO_RENDER_DATA_TYPE* pAudioRenderDataType = (AUDIO_RENDER_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

	switch ((OMX_S32)nIndex) {
	case OMX_IndexConfigTimePosition:
		//1 clear input buffer list
		OMX_AudioRenderGiveBackAllFrames(pAudioRenderDataType);
		//2 reset flag
		pAudioRenderDataType->nFrameFisrtShowFlag = OMX_TRUE;
		pAudioRenderDataType->nFlags = 0;
		pAudioRenderDataType->eClockState = OMX_TIME_ClockStateWaitingForStartTime;
		// 3 reset render
		//aic_audio_render_reset(pAudioRenderDataType->render);
		aic_audio_render_clear_cache(pAudioRenderDataType->render);

		break;
	case OMX_IndexConfigTimeSeekMode:
		break;
	case OMX_IndexConfigTimeClockState: {
		OMX_TIME_CONFIG_CLOCKSTATETYPE* state = (OMX_TIME_CONFIG_CLOCKSTATETYPE *)pComponentConfigStructure;
		pAudioRenderDataType->eClockState = state->eState;
		printf("[%s:%d] pAudioRenderDataType->eClockState:%d\n",__FUNCTION__,__LINE__,pAudioRenderDataType->eClockState);
		break;
	}
	case OMX_IndexVendorAudioRenderInit: {
		int ret = 0;
		if (!pAudioRenderDataType->render) {
			 ret = aic_audio_render_create(&pAudioRenderDataType->render);
			if (ret) {
				eError = OMX_ErrorInsufficientResources;
				logd("aic_audio_render_create error!!!!\n");
				break;
			}
			ret = pAudioRenderDataType->render->init(pAudioRenderDataType->render,pAudioRenderDataType->nDevId);
			if (!ret) {
				logd("pAudioRenderDataType->render->init ok\n");
				pAudioRenderDataType->nAudioRenderInitFlag = 1;
			} else {
				loge("pAudioRenderDataType->render->init fail\n");
				eError = OMX_ErrorInsufficientResources;
			}
		}
		break;
	}
	default:
		break;
	}
	return eError;

}

static OMX_ERRORTYPE OMX_AudioRenderGetState(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_OUT OMX_STATETYPE* pState)
{
	AUDIO_RENDER_DATA_TYPE* pAudioRenderDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	pAudioRenderDataType = (AUDIO_RENDER_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

	aic_pthread_mutex_lock(&pAudioRenderDataType->stateLock);
	*pState = pAudioRenderDataType->state;
	aic_pthread_mutex_unlock(&pAudioRenderDataType->stateLock);

	return eError;

}

static OMX_ERRORTYPE OMX_AudioRenderComponentTunnelRequest(
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
	AUDIO_RENDER_DATA_TYPE* pAudioRenderDataType;
	pAudioRenderDataType = (AUDIO_RENDER_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComp)->pComponentPrivate);
	if (pAudioRenderDataType->state != OMX_StateLoaded)
	 {
		loge("Component is not in OMX_StateLoaded,it is in%d,it can not tunnel\n",pAudioRenderDataType->state);
		return OMX_ErrorInvalidState;
	}

	if (nPort == AUDIO_RENDER_PORT_IN_AUDIO_INDEX) {
		pPort = &pAudioRenderDataType->sInPortDef[AUDIO_RENDER_PORT_IN_AUDIO_INDEX];
		pTunneledInfo = &pAudioRenderDataType->sInPortTunneledInfo[AUDIO_RENDER_PORT_IN_AUDIO_INDEX];
		pBufSupplier = &pAudioRenderDataType->sInBufSupplier[AUDIO_RENDER_PORT_IN_AUDIO_INDEX];
	} else if (nPort == AUDIO_RENDER_PORT_IN_CLOCK_INDEX) {
		pPort = &pAudioRenderDataType->sInPortDef[AUDIO_RENDER_PORT_IN_CLOCK_INDEX];
		pTunneledInfo = &pAudioRenderDataType->sInPortTunneledInfo[AUDIO_RENDER_PORT_IN_CLOCK_INDEX];
		pBufSupplier = &pAudioRenderDataType->sInBufSupplier[AUDIO_RENDER_PORT_IN_CLOCK_INDEX];
	} else {
		loge("component can not find port:%d\n",nPort);
		return OMX_ErrorBadParameter;
	}

	// cancle setup tunnel
	if (NULL == hTunneledComp && 0 == nTunneledPort && NULL == pTunnelSetup) {
			pTunneledInfo->nTunneledFlag = OMX_FALSE;
			pTunneledInfo->nTunnelPortIndex = nTunneledPort;
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
		if (pTunnelSetup->eSupplier == OMX_BufferSupplyMax)
		 {
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

static OMX_ERRORTYPE OMX_AudioRenderEmptyThisBuffer(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	AUDIO_RENDER_DATA_TYPE* pAudioRenderDataType;
	AUDIO_RENDER_IN_FRAME *pFrame;
	struct aic_message sMsg;
	pAudioRenderDataType = (AUDIO_RENDER_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

	aic_pthread_mutex_lock(&pAudioRenderDataType->stateLock);
	if (pAudioRenderDataType->state != OMX_StateExecuting) {
		logw("component is not in OMX_StateExecuting,it is in [%d]!!!\n",pAudioRenderDataType->state);
		aic_pthread_mutex_unlock(&pAudioRenderDataType->stateLock);
		return OMX_ErrorIncorrectStateOperation;
	}
	//aic_pthread_mutex_unlock(&pAudioRenderDataType->stateLock);

	// if (OMX_AudioRenderListEmpty(&pAudioRenderDataType->sInEmptyFrame,pAudioRenderDataType->sInFrameLock)) {
	// 	if (pAudioRenderDataType->nInFrameNodeNum + 1> AUDIO_RENDER_FRAME_NUM_MAX) {
	// 		loge("empty node has aready increase to max [%d]!!!\n",pAudioRenderDataType->nInFrameNodeNum);
	// 		eError = OMX_ErrorInsufficientResources;
	// 		return  eError;
	// 	 } else {
	// 		int i;
	// 		logw("no empty node,need to extend!!!\n");
	// 		for(i =0 ; i < AUDIO_RENDER_FRAME_ONE_TIME_CREATE_NUM; i++ ) {
	// 			AUDIO_RENDER_IN_FRAME *pFrameNode = (AUDIO_RENDER_IN_FRAME*)mpp_alloc(sizeof(AUDIO_RENDER_IN_FRAME));
	// 			if (NULL == pFrameNode) {
	// 				break;
	// 			 }
	// 			memset(pFrameNode,0x00,sizeof(AUDIO_RENDER_IN_FRAME));
	// 			aic_pthread_mutex_lock(&pAudioRenderDataType->sInFrameLock);
	// 			mpp_list_add_tail(&pFrameNode->sList, &pAudioRenderDataType->sInEmptyFrame);
	// 			aic_pthread_mutex_unlock(&pAudioRenderDataType->sInFrameLock);
	// 			pAudioRenderDataType->nInFrameNodeNum++;
	// 		 }
	// 		if (i == 0) {
	// 			loge("mpp_alloc empty Audio node fail\n");
	// 			eError = OMX_ErrorInsufficientResources;
	// 			return  eError;
	// 		 }
	// 	 }
	//  }

	if (OMX_AudioRenderListEmpty(&pAudioRenderDataType->sInEmptyFrame,pAudioRenderDataType->sInFrameLock)) {
		AUDIO_RENDER_IN_FRAME *pFrameNode = (AUDIO_RENDER_IN_FRAME*)mpp_alloc(sizeof(AUDIO_RENDER_IN_FRAME));
		if (NULL == pFrameNode) {
			loge("OMX_ErrorInsufficientResources\n");
			aic_pthread_mutex_unlock(&pAudioRenderDataType->stateLock);
			return OMX_ErrorInsufficientResources;
		 }
		memset(pFrameNode,0x00,sizeof(AUDIO_RENDER_IN_FRAME));
		aic_pthread_mutex_lock(&pAudioRenderDataType->sInFrameLock);
		mpp_list_add_tail(&pFrameNode->sList, &pAudioRenderDataType->sInEmptyFrame);
		aic_pthread_mutex_unlock(&pAudioRenderDataType->sInFrameLock);
		pAudioRenderDataType->nInFrameNodeNum++;
	 }


	if (pAudioRenderDataType->sInPortTunneledInfo[AUDIO_RENDER_PORT_IN_AUDIO_INDEX].nTunneledFlag) {// now Tunneled and non-Tunneled are same
		aic_pthread_mutex_lock(&pAudioRenderDataType->sInFrameLock);
		pFrame = mpp_list_first_entry(&pAudioRenderDataType->sInEmptyFrame, AUDIO_RENDER_IN_FRAME, sList);
		memcpy(&pFrame->sFrameInfo,pBuffer->pBuffer,sizeof(struct aic_audio_frame));
		mpp_list_del(&pFrame->sList);
		mpp_list_add_tail(&pFrame->sList, &pAudioRenderDataType->sInReadyFrame);
		if (pAudioRenderDataType->nWaitReayFrameFlag) {
			sMsg.message_id = OMX_CommandNops;
			sMsg.data_size = 0;
			aic_msg_put(&pAudioRenderDataType->sMsgQue, &sMsg);
			pAudioRenderDataType->nWaitReayFrameFlag = 0;
		}
		pAudioRenderDataType->nReceiveFrameNum++;
		logd("nReceiveFrameNum:%"PRId32"\n",pAudioRenderDataType->nReceiveFrameNum);
		aic_pthread_mutex_unlock(&pAudioRenderDataType->sInFrameLock);
	 } else { // now Tunneled and non-Tunneled are same
		aic_pthread_mutex_lock(&pAudioRenderDataType->sInFrameLock);
		pFrame = mpp_list_first_entry(&pAudioRenderDataType->sInEmptyFrame, AUDIO_RENDER_IN_FRAME, sList);
		memcpy(&pFrame->sFrameInfo,pBuffer->pBuffer,sizeof(struct aic_audio_frame));
		mpp_list_del(&pFrame->sList);
		mpp_list_add_tail(&pFrame->sList, &pAudioRenderDataType->sInReadyFrame);
		if (pAudioRenderDataType->nWaitReayFrameFlag) {
			sMsg.message_id = OMX_CommandNops;
			sMsg.data_size = 0;
			aic_msg_put(&pAudioRenderDataType->sMsgQue, &sMsg);
			pAudioRenderDataType->nWaitReayFrameFlag = 0;
		}
		pAudioRenderDataType->nReceiveFrameNum++;
		logd("nReceiveFrameNum:%"PRId32"\n",pAudioRenderDataType->nReceiveFrameNum);
		aic_pthread_mutex_unlock(&pAudioRenderDataType->sInFrameLock);
	 }

	aic_pthread_mutex_unlock(&pAudioRenderDataType->stateLock);
	return eError;

#if 0
	pFrame->sFrameInfo.flags = pFrame1->sFrameInfo.flags;
	pFrame->sFrameInfo.id = pFrame1->sFrameInfo.id;
	pFrame->sFrameInfo.pts = pFrame1->sFrameInfo.pts;
	pFrame->sFrameInfo.buf.buf_type = pFrame1->sFrameInfo.buf.buf_type;
	pFrame->sFrameInfo.buf.crop.x = pFrame1->sFrameInfo.buf.crop.x;
	pFrame->sFrameInfo.buf.crop.y = pFrame1->sFrameInfo.buf.crop.y;
	pFrame->sFrameInfo.buf.crop.width = pFrame1->sFrameInfo.buf.crop.width;
	pFrame->sFrameInfo.buf.crop.height = pFrame1->sFrameInfo.buf.crop.height;
	pFrame->sFrameInfo.buf.crop_en = pFrame1->sFrameInfo.buf.crop_en;
	pFrame->sFrameInfo.buf.phy_addr[0] = pFrame1->sFrameInfo.buf.phy_addr[0];
	pFrame->sFrameInfo.buf.phy_addr[1] = pFrame1->sFrameInfo.buf.phy_addr[1];
	pFrame->sFrameInfo.buf.phy_addr[2] = pFrame1->sFrameInfo.buf.phy_addr[2];
	pFrame->sFrameInfo.buf.fd[0] = pFrame1->sFrameInfo.buf.fd[0];
	pFrame->sFrameInfo.buf.fd[1] = pFrame1->sFrameInfo.buf.fd[1];
	pFrame->sFrameInfo.buf.fd[2] = pFrame1->sFrameInfo.buf.fd[2];
	pFrame->sFrameInfo.buf.flags = pFrame1->sFrameInfo.buf.flags;
	pFrame->sFrameInfo.buf.format = pFrame1->sFrameInfo.buf.format;
	pFrame->sFrameInfo.buf.size.width = pFrame1->sFrameInfo.buf.size.width;
	pFrame->sFrameInfo.buf.size.height = pFrame1->sFrameInfo.buf.size.height;
	pFrame->sFrameInfo.buf.stride[0] = pFrame1->sFrameInfo.buf.stride[0];
	pFrame->sFrameInfo.buf.stride[1] = pFrame1->sFrameInfo.buf.stride[1];
	pFrame->sFrameInfo.buf.stride[2] = pFrame1->sFrameInfo.buf.stride[2];
#endif

}

static OMX_ERRORTYPE OMX_AudioRenderFillThisBuffer(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;

	return eError;
}

static OMX_ERRORTYPE OMX_AudioRenderSetCallbacks(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_CALLBACKTYPE* pCallbacks,
		OMX_IN	OMX_PTR pAppData)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	AUDIO_RENDER_DATA_TYPE* pAudioRenderDataType;
	pAudioRenderDataType = (AUDIO_RENDER_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	pAudioRenderDataType->pCallbacks = pCallbacks;
	pAudioRenderDataType->pAppData = pAppData;
	return eError;
}

OMX_ERRORTYPE OMX_AudioRenderComponentDeInit(
		OMX_IN	OMX_HANDLETYPE hComponent)  {
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_COMPONENTTYPE *pComp;
	AUDIO_RENDER_DATA_TYPE *pAudioRenderDataType;
	AUDIO_RENDER_IN_FRAME	*pFrameNode = NULL,*pFrameNode1 = NULL;
	pComp = (OMX_COMPONENTTYPE *)hComponent;
	struct aic_message sMsg;
	pAudioRenderDataType = (AUDIO_RENDER_DATA_TYPE *)pComp->pComponentPrivate;

	aic_pthread_mutex_lock(&pAudioRenderDataType->stateLock);
	if (pAudioRenderDataType->state != OMX_StateLoaded) {
		loge("compoent is in %d,but not in OMX_StateLoaded(1),can ont FreeHandle.\n",pAudioRenderDataType->state);
		aic_pthread_mutex_unlock(&pAudioRenderDataType->stateLock);
		return OMX_ErrorIncorrectStateOperation;
	}
	aic_pthread_mutex_unlock(&pAudioRenderDataType->stateLock);

	sMsg.message_id = OMX_CommandStop;
	sMsg.data_size = 0;
	aic_msg_put(&pAudioRenderDataType->sMsgQue, &sMsg);
	pthread_join(pAudioRenderDataType->threadId, (void*)&eError);

	aic_pthread_mutex_lock(&pAudioRenderDataType->sInFrameLock);
	if (!mpp_list_empty(&pAudioRenderDataType->sInEmptyFrame)) {
			mpp_list_for_each_entry_safe(pFrameNode, pFrameNode1, &pAudioRenderDataType->sInEmptyFrame, sList) {
				mpp_list_del(&pFrameNode->sList);
				mpp_free(pFrameNode);
		}
	}

	if (!mpp_list_empty(&pAudioRenderDataType->sInReadyFrame)) {
			mpp_list_for_each_entry_safe(pFrameNode, pFrameNode1, &pAudioRenderDataType->sInReadyFrame, sList) {
				mpp_list_del(&pFrameNode->sList);
				mpp_free(pFrameNode);
		}
	}
	if (!mpp_list_empty(&pAudioRenderDataType->sInProcessedFrmae)) {
			mpp_list_for_each_entry_safe(pFrameNode, pFrameNode1, &pAudioRenderDataType->sInProcessedFrmae, sList) {
				mpp_list_del(&pFrameNode->sList);
				mpp_free(pFrameNode);
		}
	}
	aic_pthread_mutex_unlock(&pAudioRenderDataType->sInFrameLock);

	pthread_mutex_destroy(&pAudioRenderDataType->sInFrameLock);
	pthread_mutex_destroy(&pAudioRenderDataType->stateLock);

	aic_msg_destroy(&pAudioRenderDataType->sMsgQue);

	if (pAudioRenderDataType->render)  {
		aic_audio_render_destroy(pAudioRenderDataType->render);
		pAudioRenderDataType->render = NULL;
	}

	mpp_free(pAudioRenderDataType);
	pAudioRenderDataType = NULL;

	logi("OMX_AudioRenderComponentDeInit\n");
	return eError;
}

OMX_ERRORTYPE OMX_AudioRenderComponentInit(
		OMX_IN	OMX_HANDLETYPE hComponent)
{
	OMX_COMPONENTTYPE *pComp;
	AUDIO_RENDER_DATA_TYPE *pAudioRenderDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_U32 err;
	OMX_U32 i;

	logi("OMX_AudioRenderComponentInit....\n");

	pComp = (OMX_COMPONENTTYPE *)hComponent;

	pAudioRenderDataType = (AUDIO_RENDER_DATA_TYPE *)mpp_alloc(sizeof(AUDIO_RENDER_DATA_TYPE));

	if (NULL == pAudioRenderDataType)  {
		loge("mpp_alloc(sizeof(AUDIO_RENDER_DATA_TYPE) fail!\n");
		eError = OMX_ErrorInsufficientResources;
		goto _EXIT1;
	}

	memset(pAudioRenderDataType, 0x0, sizeof(AUDIO_RENDER_DATA_TYPE));
	pComp->pComponentPrivate	= (void*) pAudioRenderDataType;
	pAudioRenderDataType->nFrameFisrtShowFlag = OMX_TRUE;
	pAudioRenderDataType->state 		= OMX_StateLoaded;
	pAudioRenderDataType->hSelf 		= pComp;

	pComp->SetCallbacks 		= OMX_AudioRenderSetCallbacks;
	pComp->SendCommand			= OMX_AudioRenderSendCommand;
	pComp->GetState 			= OMX_AudioRenderGetState;
	pComp->GetParameter 		= OMX_AudioRenderGetParameter;
	pComp->SetParameter 		= OMX_AudioRenderSetParameter;
	pComp->GetConfig			= OMX_AudioRenderGetConfig;
	pComp->SetConfig			= OMX_AudioRenderSetConfig;
	pComp->ComponentTunnelRequest = OMX_AudioRenderComponentTunnelRequest;
	pComp->ComponentDeInit		= OMX_AudioRenderComponentDeInit;
	pComp->FillThisBuffer		= OMX_AudioRenderFillThisBuffer;
	pComp->EmptyThisBuffer		= OMX_AudioRenderEmptyThisBuffer;

	pAudioRenderDataType->sPortParam.nPorts = 2;
	pAudioRenderDataType->sPortParam.nStartPortNumber = 0x0;

	pAudioRenderDataType->sInPortDef[AUDIO_RENDER_PORT_IN_AUDIO_INDEX].nPortIndex = AUDIO_RENDER_PORT_IN_AUDIO_INDEX;
	pAudioRenderDataType->sInPortDef[AUDIO_RENDER_PORT_IN_AUDIO_INDEX].bPopulated = OMX_TRUE;
	pAudioRenderDataType->sInPortDef[AUDIO_RENDER_PORT_IN_AUDIO_INDEX].bEnabled	= OMX_TRUE;
	pAudioRenderDataType->sInPortDef[AUDIO_RENDER_PORT_IN_AUDIO_INDEX].eDomain = OMX_PortDomainAudio;
	pAudioRenderDataType->sInPortDef[AUDIO_RENDER_PORT_IN_AUDIO_INDEX].eDir = OMX_DirInput;
	pAudioRenderDataType->sInBufSupplier[AUDIO_RENDER_PORT_IN_AUDIO_INDEX].nPortIndex = AUDIO_RENDER_PORT_IN_AUDIO_INDEX;
	pAudioRenderDataType->sInBufSupplier[AUDIO_RENDER_PORT_IN_AUDIO_INDEX].eBufferSupplier = OMX_BufferSupplyOutput;

	pAudioRenderDataType->sInPortDef[AUDIO_RENDER_PORT_IN_CLOCK_INDEX].nPortIndex = AUDIO_RENDER_PORT_IN_CLOCK_INDEX;
	pAudioRenderDataType->sInPortDef[AUDIO_RENDER_PORT_IN_CLOCK_INDEX].bPopulated = OMX_TRUE;
	pAudioRenderDataType->sInPortDef[AUDIO_RENDER_PORT_IN_CLOCK_INDEX].bEnabled	= OMX_TRUE;
	pAudioRenderDataType->sInPortDef[AUDIO_RENDER_PORT_IN_CLOCK_INDEX].eDomain = OMX_PortDomainOther;
	pAudioRenderDataType->sInPortDef[AUDIO_RENDER_PORT_IN_CLOCK_INDEX].eDir = OMX_DirInput;
	pAudioRenderDataType->sInBufSupplier[AUDIO_RENDER_PORT_IN_CLOCK_INDEX].nPortIndex = AUDIO_RENDER_PORT_IN_CLOCK_INDEX;
	pAudioRenderDataType->sInBufSupplier[AUDIO_RENDER_PORT_IN_CLOCK_INDEX].eBufferSupplier = OMX_BufferSupplyOutput;

	pAudioRenderDataType->nInFrameNodeNum = 0;
	mpp_list_init(&pAudioRenderDataType->sInEmptyFrame);
	mpp_list_init(&pAudioRenderDataType->sInReadyFrame);
	mpp_list_init(&pAudioRenderDataType->sInProcessedFrmae);
	pthread_mutex_init(&pAudioRenderDataType->sInFrameLock, NULL);
	for(i =0 ; i < AUDIO_RENDER_FRAME_ONE_TIME_CREATE_NUM; i++) {
		AUDIO_RENDER_IN_FRAME *pFrameNode = (AUDIO_RENDER_IN_FRAME*)mpp_alloc(sizeof(AUDIO_RENDER_IN_FRAME));
		if (NULL == pFrameNode) {
			break;
		}
		memset(pFrameNode,0x00,sizeof(AUDIO_RENDER_IN_FRAME));
		mpp_list_add_tail(&pFrameNode->sList, &pAudioRenderDataType->sInEmptyFrame);
		pAudioRenderDataType->nInFrameNodeNum++;
	}
	if (pAudioRenderDataType->nInFrameNodeNum == 0) {
		loge("mpp_alloc empty Audio node fail\n");
		eError = OMX_ErrorInsufficientResources;
		goto _EXIT2;
	}

	if (aic_msg_create(&pAudioRenderDataType->sMsgQue)<0)
	 {
		loge("aic_msg_create fail!\n");
		eError = OMX_ErrorInsufficientResources;
		goto _EXIT4;
	}

	pAudioRenderDataType->eClockState = OMX_TIME_ClockStateStopped;

	pthread_mutex_init(&pAudioRenderDataType->stateLock, NULL);
	// Create the component thread
	err = pthread_create(&pAudioRenderDataType->threadId, NULL, OMX_AudioRenderComponentThread, pAudioRenderDataType);
	if (err || !pAudioRenderDataType->threadId) {
		loge("pthread_create fail!\n");
		eError = OMX_ErrorInsufficientResources;
		goto _EXIT5;
	}

	logi("OMX_AudioRenderComponentInit OK\n");
#ifdef AUDIO_RENDRE_DUMP_ENABLE
    pAudioRenderDataType->pDumpAudioFilePath = (OMX_S8*)AUDIO_RENDRE_DUMP_FILEPATH;
    pAudioRenderDataType->nDumpAudioFd = 0;
#endif

	return eError;

_EXIT5:
	aic_msg_destroy(&pAudioRenderDataType->sMsgQue);
	pthread_mutex_destroy(&pAudioRenderDataType->stateLock);

_EXIT4:
	if (!mpp_list_empty(&pAudioRenderDataType->sInEmptyFrame)) {
		AUDIO_RENDER_IN_FRAME	*pFrameNode = NULL,*pFrameNode1 = NULL;
		mpp_list_for_each_entry_safe(pFrameNode, pFrameNode1, &pAudioRenderDataType->sInEmptyFrame, sList) {
			mpp_list_del(&pFrameNode->sList);
			mpp_free(pFrameNode);
		}
	}

_EXIT2:
	if (pAudioRenderDataType) {
		mpp_free(pAudioRenderDataType);
		pAudioRenderDataType = NULL;
	}

_EXIT1:
	return eError;
}

static int  OMX_AudioRenderGiveBackAllFrames(AUDIO_RENDER_DATA_TYPE * pAudioRenderDataType)
{
	int ret = 0;
	// 1 move ready node to using list
	logi("Before OMX_AudioRenderComponentThread exit,move node in sInReadyFrame to sInProcessedFrmae\n");
	if (!OMX_AudioRenderListEmpty(&pAudioRenderDataType->sInReadyFrame,pAudioRenderDataType->sInFrameLock)) {
		logi("sInReadyFrame is not empty\n");
		AUDIO_RENDER_IN_FRAME *pFrameNode,*pFrameNode1;
		aic_pthread_mutex_lock(&pAudioRenderDataType->sInFrameLock);
		mpp_list_for_each_entry_safe(pFrameNode, pFrameNode1, &pAudioRenderDataType->sInReadyFrame, sList) {
			pAudioRenderDataType->nLeftReadyFrameWhenCompoentExitNum++;
			mpp_list_del(&pFrameNode->sList);
			mpp_list_add_tail(&pFrameNode->sList, &pAudioRenderDataType->sInProcessedFrmae);
		}
		aic_pthread_mutex_unlock(&pAudioRenderDataType->sInFrameLock);
	}

	// 2 give back frames in processed list to adec or app
	logi("Before OMX_AudioRenderComponentThread exit,give all frames back to Adec\n");
	if (!OMX_AudioRenderListEmpty(&pAudioRenderDataType->sInProcessedFrmae,pAudioRenderDataType->sInFrameLock)) {
		logi("sInProcessedFrmae is not empty\n");
		AUDIO_RENDER_IN_FRAME *pFrameNode = NULL;
		OMX_BUFFERHEADERTYPE sBuffHead;
		while(!OMX_AudioRenderListEmpty(&pAudioRenderDataType->sInProcessedFrmae,pAudioRenderDataType->sInFrameLock)) {
			aic_pthread_mutex_lock(&pAudioRenderDataType->sInFrameLock);
			pFrameNode = mpp_list_first_entry(&pAudioRenderDataType->sInProcessedFrmae, AUDIO_RENDER_IN_FRAME, sList);
			aic_pthread_mutex_unlock(&pAudioRenderDataType->sInFrameLock);
			ret =-1;
			if (pAudioRenderDataType->sInPortTunneledInfo[AUDIO_RENDER_PORT_IN_AUDIO_INDEX].nTunneledFlag) {
				sBuffHead.nInputPortIndex = AUDIO_RENDER_PORT_IN_AUDIO_INDEX;
				sBuffHead.nOutputPortIndex = pAudioRenderDataType->sInPortTunneledInfo[AUDIO_RENDER_PORT_IN_AUDIO_INDEX].nTunnelPortIndex;
				sBuffHead.pBuffer = (OMX_U8 *)&pFrameNode->sFrameInfo;
				ret = OMX_FillThisBuffer(pAudioRenderDataType->sInPortTunneledInfo[AUDIO_RENDER_PORT_IN_AUDIO_INDEX].pTunneledComp,&sBuffHead);
			} else {
				if (pAudioRenderDataType->pCallbacks != NULL && pAudioRenderDataType->pCallbacks->EmptyBufferDone!= NULL) {
					sBuffHead.pBuffer =  (OMX_U8 *)&pFrameNode->sFrameInfo;
					ret = pAudioRenderDataType->pCallbacks->EmptyBufferDone(pAudioRenderDataType->hSelf,pAudioRenderDataType->pAppData,&sBuffHead);
				}
			}

			if (ret == 0) {
				logd("give back frame to vdec ok");
				pAudioRenderDataType->nGiveBackFrameOkNum++;
			} else {
				loge("give back frame to vdec fail\n");
				pAudioRenderDataType->nGiveBackFrameFailNum++;
				continue;// must give back ok ,so retry to give back
			}
			logi("nGiveBackFrameOkNum:%d,nGiveBackFrameFailNum:%d\n"
				,pAudioRenderDataType->nGiveBackFrameOkNum
				,pAudioRenderDataType->nGiveBackFrameFailNum);

			aic_pthread_mutex_lock(&pAudioRenderDataType->sInFrameLock);
			mpp_list_del(&pFrameNode->sList);
			mpp_list_add_tail(&pFrameNode->sList, &pAudioRenderDataType->sInEmptyFrame);
			aic_pthread_mutex_unlock(&pAudioRenderDataType->sInFrameLock);
		}
	}
	return 0;
}

static void OMX_AudioRenderEventNotify(
		AUDIO_RENDER_DATA_TYPE * pAudioRenderDataType,
		OMX_EVENTTYPE event,
		OMX_U32 nData1,
		OMX_U32 nData2,
		OMX_PTR pEventData)
{
	if (pAudioRenderDataType && pAudioRenderDataType->pCallbacks && pAudioRenderDataType->pCallbacks->EventHandler)  {
		pAudioRenderDataType->pCallbacks->EventHandler(
					pAudioRenderDataType->hSelf,
					pAudioRenderDataType->pAppData,event,
					nData1, nData2, pEventData);
	}
}

static void OMX_AudioRenderStateChangeToInvalid(AUDIO_RENDER_DATA_TYPE * pAudioRenderDataType)
{
	pAudioRenderDataType->state = OMX_StateInvalid;
	OMX_AudioRenderEventNotify(pAudioRenderDataType
						,OMX_EventError
						,OMX_ErrorInvalidState,0,NULL);
	OMX_AudioRenderEventNotify(pAudioRenderDataType
						,OMX_EventCmdComplete
						,OMX_CommandStateSet
						,pAudioRenderDataType->state,NULL);

}
static void OMX_AudioRenderStateChangeToLoaded(AUDIO_RENDER_DATA_TYPE * pAudioRenderDataType)
{
	if (pAudioRenderDataType->state == OMX_StateIdle) {
		OMX_AudioRenderGiveBackAllFrames(pAudioRenderDataType);
	} else if (pAudioRenderDataType->state == OMX_StateExecuting) {

	} else if (pAudioRenderDataType->state == OMX_StatePause) {

	} else  {
		OMX_AudioRenderEventNotify(pAudioRenderDataType
							,OMX_EventError
							,OMX_ErrorIncorrectStateTransition
							, pAudioRenderDataType->state,NULL);
		loge("OMX_ErrorIncorrectStateTransition\n");
		return;
	}
	pAudioRenderDataType->state = OMX_StateLoaded;
	OMX_AudioRenderEventNotify(pAudioRenderDataType
						,OMX_EventCmdComplete
						,OMX_CommandStateSet
						, pAudioRenderDataType->state,NULL);
}
static void OMXAudioRenderStateChangeToIdle(AUDIO_RENDER_DATA_TYPE * pAudioRenderDataType)
{
	int ret = 0;
	if (pAudioRenderDataType->state == OMX_StateLoaded) {
		//create Audio_handle
	if (!pAudioRenderDataType->render) {
		 ret = aic_audio_render_create(&pAudioRenderDataType->render);
	}

	if (ret != 0) {
		loge("aic_create_Audio_render fail\n");
		OMX_AudioRenderEventNotify(pAudioRenderDataType
							,OMX_EventError
							,OMX_ErrorIncorrectStateTransition
							, pAudioRenderDataType->state,NULL);

		loge("OMX_ErrorIncorrectStateTransition\n");
		return;
	}

	} else if (pAudioRenderDataType->state == OMX_StatePause) {

	} else if (pAudioRenderDataType->state == OMX_StateExecuting) {

	} else {
		OMX_AudioRenderEventNotify(pAudioRenderDataType
							,OMX_EventError
							,OMX_ErrorIncorrectStateTransition
							, pAudioRenderDataType->state,NULL);
		loge("OMX_ErrorIncorrectStateTransition\n");
		return;
	}
	pAudioRenderDataType->state = OMX_StateIdle;
	OMX_AudioRenderEventNotify(pAudioRenderDataType
						,OMX_EventCmdComplete
						,OMX_CommandStateSet
						, pAudioRenderDataType->state,NULL);

}
static void OMX_AudioRenderStateChangeToExcuting(AUDIO_RENDER_DATA_TYPE * pAudioRenderDataType)
{
	if (pAudioRenderDataType->state == OMX_StateLoaded) {
		OMX_AudioRenderEventNotify(pAudioRenderDataType
							,OMX_EventError
							,OMX_ErrorIncorrectStateTransition
							, pAudioRenderDataType->state,NULL);
		loge("OMX_ErrorIncorrectStateTransition\n");
		return;
	} else if (pAudioRenderDataType->state == OMX_StateIdle) {

	} else if (pAudioRenderDataType->state == OMX_StatePause) {
		aic_audio_render_pause(pAudioRenderDataType->render);
	} else {
		OMX_AudioRenderEventNotify(pAudioRenderDataType
							,OMX_EventError
							,OMX_ErrorIncorrectStateTransition
							, pAudioRenderDataType->state,NULL);
		loge("OMX_ErrorIncorrectStateTransition\n");
		return;
	}
	pAudioRenderDataType->state = OMX_StateExecuting;
	OMX_AudioRenderEventNotify(pAudioRenderDataType
						,OMX_EventCmdComplete
						,OMX_CommandStateSet
						, pAudioRenderDataType->state,NULL);

}
static void OMX_AudioRenderStateChangeToPause(AUDIO_RENDER_DATA_TYPE * pAudioRenderDataType)
{
	if (pAudioRenderDataType->state == OMX_StateLoaded) {

	} else if (pAudioRenderDataType->state == OMX_StateIdle) {

	} else if (pAudioRenderDataType->state == OMX_StateExecuting) {
		aic_audio_render_pause(pAudioRenderDataType->render);
	} else {
		OMX_AudioRenderEventNotify(pAudioRenderDataType
							,OMX_EventError
							,OMX_ErrorIncorrectStateTransition
							, pAudioRenderDataType->state,NULL);
		loge("OMX_ErrorIncorrectStateTransition\n");
		return;

	}
	pAudioRenderDataType->state = OMX_StatePause;
	OMX_AudioRenderEventNotify(pAudioRenderDataType
						,OMX_EventCmdComplete
						,OMX_CommandStateSet
						, pAudioRenderDataType->state,NULL);

}

// need to define in OMX_Core_Ext1.h
#define CORRECT_REF_CLOCK_TIME (1*1000*1000)

static int OMX_ProcessAudioSync(AUDIO_RENDER_DATA_TYPE* pAudioRenderDataType,struct aic_audio_frame *pFrameInfo)
{
	OMX_TICKS sDiffTime;
	OMX_TICKS sRealAudioTime;
	OMX_TICKS sAudioCacheDuration;
	OMX_TIME_CONFIG_TIMESTAMPTYPE sTimeStamp;
	OMX_U32 data1,data2;

	OMX_PORT_TUNNELEDINFO *pTunneldClock = &pAudioRenderDataType->sInPortTunneledInfo[AUDIO_RENDER_PORT_IN_CLOCK_INDEX];
	sAudioCacheDuration = pAudioRenderDataType->render->get_cached_time(pAudioRenderDataType->render);
	sRealAudioTime = pFrameInfo->pts - sAudioCacheDuration;
	if (pTunneldClock->nTunneledFlag) {
		OMX_GetConfig(pTunneldClock->pTunneledComp, OMX_IndexConfigTimeCurrentMediaTime,&sTimeStamp);
		sDiffTime = sTimeStamp.nTimestamp - pAudioRenderDataType->sPreCorrectMediaTime;
		// correct ref clock per 10s
		if (sDiffTime > CORRECT_REF_CLOCK_TIME) { //correct ref time
			sTimeStamp.nTimestamp = sRealAudioTime;
			pAudioRenderDataType->sPreCorrectMediaTime = sTimeStamp.nTimestamp;
			OMX_SetConfig(pTunneldClock->pTunneledComp,OMX_IndexConfigTimeCurrentAudioReference,&sTimeStamp);
			logd("OMX_IndexConfigTimeCurrentAudioReference:%ld,sDiffTime:%ld\n",sTimeStamp.nTimestamp,sDiffTime);
		}
	}
	data1 = (sRealAudioTime >> 32) & 0x00000000ffffffff;
	data2 = sRealAudioTime & 0x00000000ffffffff;
	OMX_AudioRenderEventNotify(pAudioRenderDataType,OMX_EventAudioRenderPts,data1,data2,NULL);
	return 0;
}

#ifdef AUDIO_RENDRE_DUMP_ENABLE
static int OMX_AudioRenderDump(AUDIO_RENDER_DATA_TYPE* pAudioRenderDataType,char * data,int len,int end)
{
    if (pAudioRenderDataType->nDumpAudioFd == 0) {
        s32 fd;
        fd = open((char *)pAudioRenderDataType->pDumpAudioFilePath, O_RDWR|O_CREAT);
        if (fd < 0) {
            loge("open %s failed!!!!!\n",pAudioRenderDataType->pDumpAudioFilePath);
            return -1;
        } else {
            pAudioRenderDataType->nDumpAudioFd = fd;
        }
    }
    write(pAudioRenderDataType->nDumpAudioFd,data,len);
    if (end == 1) {
        close(pAudioRenderDataType->nDumpAudioFd);
        pAudioRenderDataType->nDumpAudioFd = 0;
    }

    return 0;
}
#endif


static int OMX_AudioGiveBackFrames(AUDIO_RENDER_DATA_TYPE* pAudioRenderDataType)
{
	int ret = 0;
	AUDIO_RENDER_IN_FRAME *pFrameNode;
	OMX_PORT_TUNNELEDINFO *pTunneldAudio;
	pTunneldAudio = &pAudioRenderDataType->sInPortTunneledInfo[AUDIO_RENDER_PORT_IN_AUDIO_INDEX];

	if (!OMX_AudioRenderListEmpty(&pAudioRenderDataType->sInProcessedFrmae,pAudioRenderDataType->sInFrameLock)) {
		OMX_BUFFERHEADERTYPE sBuffHead;
		while(!OMX_AudioRenderListEmpty(&pAudioRenderDataType->sInProcessedFrmae,pAudioRenderDataType->sInFrameLock)) {
			aic_pthread_mutex_lock(&pAudioRenderDataType->sInFrameLock);
			pFrameNode = mpp_list_first_entry(&pAudioRenderDataType->sInProcessedFrmae, AUDIO_RENDER_IN_FRAME, sList);
			aic_pthread_mutex_unlock(&pAudioRenderDataType->sInFrameLock);
			ret = -1;
			if (pTunneldAudio->nTunneledFlag) {
				sBuffHead.pBuffer = (OMX_U8 *)&pFrameNode->sFrameInfo;
				sBuffHead.nInputPortIndex = AUDIO_RENDER_PORT_IN_AUDIO_INDEX;
				sBuffHead.nOutputPortIndex = pTunneldAudio->nTunnelPortIndex;
				ret = OMX_FillThisBuffer(pTunneldAudio->pTunneledComp,&sBuffHead);
				} else {
				sBuffHead.pBuffer = (OMX_U8 *)&pFrameNode->sFrameInfo;
				if (pAudioRenderDataType->pCallbacks->EmptyBufferDone) {
					ret = pAudioRenderDataType->pCallbacks->EmptyBufferDone(pAudioRenderDataType->hSelf,
											pAudioRenderDataType->pAppData,&sBuffHead);
					}
				}

			if (ret == 0) { // how to do
				//loge("give back frame to adec ok");
				pAudioRenderDataType->nGiveBackFrameOkNum++;
				} else {
				logw("give back frame to adec fail\n");
				pAudioRenderDataType->nGiveBackFrameFailNum++;
				}
			logi("nGiveBackFrameOkNum:%d,nGiveBackFrameFailNum:%d\n"
				,pAudioRenderDataType->nGiveBackFrameOkNum
				,pAudioRenderDataType->nGiveBackFrameFailNum);

			if (ret == 0) {
				aic_pthread_mutex_lock(&pAudioRenderDataType->sInFrameLock);
				mpp_list_del(&pFrameNode->sList);
				mpp_list_add_tail(&pFrameNode->sList, &pAudioRenderDataType->sInEmptyFrame);
				aic_pthread_mutex_unlock(&pAudioRenderDataType->sInFrameLock);
				} else { // how to do ,do nothing or move to empty list,now move to  empty list
				//mpp_list_del(&pFrameNode->sList);
				//mpp_list_add_tail(&pFrameNode->sList, &pAudioRenderDataType->sInEmptyFrame);
				break;
				}
			}
		} else {
		//loge("no processed frame!!!!\n");
		}
	return ret;
}

static void* OMX_AudioRenderComponentThread(void* pThreadData)
{
	struct aic_message message;
	OMX_S32 nCmd;		//OMX_COMMANDTYPE
	OMX_S32 nCmdData;	//OMX_STATETYPE
	AUDIO_RENDER_DATA_TYPE* pAudioRenderDataType = (AUDIO_RENDER_DATA_TYPE*)pThreadData;
	OMX_S32 ret;
	AUDIO_RENDER_IN_FRAME *pFrameNode;
	OMX_S32 bNotifyFrameEnd = 0;
	//prctl(PR_SET_NAME,(u32)"AudioRender");
	OMX_PORT_TUNNELEDINFO *pTunneldClock;
	pTunneldClock = &pAudioRenderDataType->sInPortTunneledInfo[AUDIO_RENDER_PORT_IN_CLOCK_INDEX];
	while(1) {
_AIC_MSG_GET_:
		if  (aic_msg_get(&pAudioRenderDataType->sMsgQue, &message) == 0) {
			nCmd = message.message_id;
			nCmdData = message.param;
			logi("nCmd:%d, nCmdData:%d\n",nCmd,nCmdData);
			if (OMX_CommandStateSet == nCmd) {
				aic_pthread_mutex_lock(&pAudioRenderDataType->stateLock);
				if (pAudioRenderDataType->state == (OMX_STATETYPE)(nCmdData)) {
					logi("OMX_ErrorSameState\n");
					OMX_AudioRenderEventNotify(pAudioRenderDataType
						,OMX_EventError
						,OMX_ErrorSameState,0,NULL);
					aic_pthread_mutex_unlock(&pAudioRenderDataType->stateLock);
					continue;
				 }
				switch(nCmdData) {
					case OMX_StateInvalid:
						OMX_AudioRenderStateChangeToInvalid(pAudioRenderDataType);
						break;
					case OMX_StateLoaded:
						OMX_AudioRenderStateChangeToLoaded(pAudioRenderDataType);
						break;
					case OMX_StateIdle:
						OMXAudioRenderStateChangeToIdle(pAudioRenderDataType);
						break;
					case OMX_StateExecuting:
						OMX_AudioRenderStateChangeToExcuting(pAudioRenderDataType);
						break;
					case OMX_StatePause:
						OMX_AudioRenderStateChangeToPause(pAudioRenderDataType);
						break;
					default:
						break;
				 }
				aic_pthread_mutex_unlock(&pAudioRenderDataType->stateLock);
			} else if (OMX_CommandFlush == nCmd) {

			} else if (OMX_CommandPortDisable == nCmd) {

			} else if (OMX_CommandPortEnable == nCmd) {

			} else if (OMX_CommandMarkBuffer == nCmd) {

			} else if (OMX_CommandStop == nCmd) {
				logi("OMX_AudioRenderComponentThread ready to exit!!!\n");
				goto _EXIT;
			} else {

			}
		}

		if (pAudioRenderDataType->state != OMX_StateExecuting) {
			//usleep(1000);
			aic_msg_wait_new_msg(&pAudioRenderDataType->sMsgQue, 0);
			continue;
		}
		if (pAudioRenderDataType->nFlags & AUDIO_RENDER_INPORT_SEND_ALL_FRAME_FLAG) {
			if (!bNotifyFrameEnd) {
				OMX_AudioRenderEventNotify(pAudioRenderDataType,OMX_EventBufferFlag,0,0,NULL);
				bNotifyFrameEnd = 1;
			}
			//usleep(1000);
			aic_msg_wait_new_msg(&pAudioRenderDataType->sMsgQue, 0);
			continue;
		}

		bNotifyFrameEnd = 0;

		OMX_AudioGiveBackFrames(pAudioRenderDataType);

		aic_pthread_mutex_lock(&pAudioRenderDataType->sInFrameLock);
		if (mpp_list_empty(&pAudioRenderDataType->sInReadyFrame)) {
			struct timespec before = {0},after = {0};
			long diff;

			pAudioRenderDataType->nWaitReayFrameFlag = 1;
			aic_pthread_mutex_unlock(&pAudioRenderDataType->sInFrameLock);

			clock_gettime(CLOCK_REALTIME,&before);
			aic_msg_wait_new_msg(&pAudioRenderDataType->sMsgQue, AUDIO_RENDER_WAIT_FRAME_INTERVAL);
			clock_gettime(CLOCK_REALTIME,&after);
			diff = (after.tv_sec - before.tv_sec)*1000*1000 + (after.tv_nsec - before.tv_nsec)/1000;
			if (diff > AUDIO_RENDER_WAIT_FRAME_MAX_TIME) {
				printf("[%s:%d]:%ld\n",__FUNCTION__,__LINE__,diff);
				pAudioRenderDataType->nFlags  |= AUDIO_RENDER_INPORT_SEND_ALL_FRAME_FLAG;
			}
			goto _AIC_MSG_GET_;
		}
		aic_pthread_mutex_unlock(&pAudioRenderDataType->sInFrameLock);

		while(!OMX_AudioRenderListEmpty(&pAudioRenderDataType->sInReadyFrame,pAudioRenderDataType->sInFrameLock)) {
			aic_pthread_mutex_lock(&pAudioRenderDataType->sInFrameLock);
			pFrameNode = mpp_list_first_entry(&pAudioRenderDataType->sInReadyFrame, AUDIO_RENDER_IN_FRAME, sList);
			aic_pthread_mutex_unlock(&pAudioRenderDataType->sInFrameLock);

			if (pAudioRenderDataType->nFrameFisrtShowFlag) {
				struct aic_audio_render_attr ao_attr;
				if (!pAudioRenderDataType->nAudioRenderInitFlag) {
					pAudioRenderDataType->render->init(pAudioRenderDataType->render,pAudioRenderDataType->nDevId);
					pAudioRenderDataType->nAudioRenderInitFlag = 1;
				 }

				ao_attr.bits_per_sample = pFrameNode->sFrameInfo.bits_per_sample;
				ao_attr.channels = pFrameNode->sFrameInfo.channels;
				ao_attr.sample_rate = pFrameNode->sFrameInfo.sample_rate;
				ao_attr.smples_per_frame = 32*1024;/*need to define a member of struct*/
				pAudioRenderDataType->sAudioRenderArrt = ao_attr;
				pAudioRenderDataType->render->set_attr(pAudioRenderDataType->render,&ao_attr);
				if (pAudioRenderDataType->nVolumeChange) {
					if (pAudioRenderDataType->render->set_volume(pAudioRenderDataType->render,pAudioRenderDataType->nVolume) == 0) {
						pAudioRenderDataType->nVolumeChange = 0;
					} else {
						loge("set_volume error\n");
					}
				} else {
						pAudioRenderDataType->nVolume = pAudioRenderDataType->render->get_volume(pAudioRenderDataType->render);
						logd("nVolume :%d\n",pAudioRenderDataType->nVolume);
				}

				if (pTunneldClock->nTunneledFlag) { // set clock start time
					OMX_TIME_CONFIG_TIMESTAMPTYPE sTimeStamp;
					sTimeStamp.nPortIndex = pTunneldClock->nTunnelPortIndex;
					sTimeStamp.nTimestamp = pFrameNode->sFrameInfo.pts;
					pAudioRenderDataType->sPreFramePts = pFrameNode->sFrameInfo.pts;
					pAudioRenderDataType->sFirstFramePts = pFrameNode->sFrameInfo.pts;
					OMX_SetConfig(pTunneldClock->pTunneledComp,OMX_IndexConfigTimeClientStartTime, &sTimeStamp);
					pAudioRenderDataType->sPreCorrectMediaTime = pFrameNode->sFrameInfo.pts;
					// whether need to wait????
					// printf("[%s:%d]wait video start time\n",__FUNCTION__,__LINE__);
					// while(pAudioRenderDataType->eClockState != OMX_TIME_ClockStateRunning) {
					// 		usleep(1*1000);
					//  }
					if (pAudioRenderDataType->eClockState != OMX_TIME_ClockStateRunning) {
						aic_msg_wait_new_msg(&pAudioRenderDataType->sMsgQue, 1*1000);
						goto _AIC_MSG_GET_;
					}
					printf("[%s:%d]video start time arrive\n",__FUNCTION__,__LINE__);
				 }

				printf("[%s:%d]bits_per_sample:%d,channels:%d,sample_rate:%d,pts:%" PRId64 "\n"
					,__FUNCTION__,__LINE__
					,pFrameNode->sFrameInfo.bits_per_sample
					,pFrameNode->sFrameInfo.channels
					,pFrameNode->sFrameInfo.sample_rate
					,pFrameNode->sFrameInfo.pts);

				ret = 0;
				if (pFrameNode->sFrameInfo.size > 0) {// frame size maybe 0.
					ret = pAudioRenderDataType->render->rend(pAudioRenderDataType->render,pFrameNode->sFrameInfo.data,pFrameNode->sFrameInfo.size);
				 }
				if (ret == 0) {
					aic_pthread_mutex_lock(&pAudioRenderDataType->sInFrameLock);
					mpp_list_del(&pFrameNode->sList);
					mpp_list_add_tail(&pFrameNode->sList, &pAudioRenderDataType->sInProcessedFrmae);
					aic_pthread_mutex_unlock(&pAudioRenderDataType->sInFrameLock);
					pAudioRenderDataType->nShowFrameOkNum++;
					pAudioRenderDataType->nFrameFisrtShowFlag = OMX_FALSE;
					OMX_AudioRenderEventNotify(pAudioRenderDataType,OMX_EventAudioRenderFirstFrame,0,0,NULL);
					#ifdef AUDIO_RENDRE_DUMP_ENABLE
					 {
						int end = 0;
						if (pFrameNode->sFrameInfo.flag & FRAME_FLAG_EOS) {
							end = 1;
							printf("[%s:%d]audio stream end \n",__FUNCTION__,__LINE__);
						 }
						OMX_AudioRenderDump(pAudioRenderDataType,pFrameNode->sFrameInfo.data,pFrameNode->sFrameInfo.size,end);
					 }
					#endif
				 } else {// how to do ,video can deal with  same success,drop this frame,but audio can not drop it.
					loge("first frame error,there is something wrong!!!!\n");
					//mpp_list_del(&pFrameNode->sList);
					//mpp_list_add_tail(&pFrameNode->sList, &pAudioRenderDataType->sInProcessedFrmae);
					pAudioRenderDataType->nShowFrameFailNum++;
				 }
				logd("nReceiveFrameNum:%d,nShowFrameOkNum:%d,nShowFrameFailNum:%d\n"
					,pAudioRenderDataType->nReceiveFrameNum
					,pAudioRenderDataType->nShowFrameOkNum
					,pAudioRenderDataType->nShowFrameFailNum);
			 } else {// not first frame
				static struct aic_audio_frame pre_frame =  {0};
				u64 diff_pts = pFrameNode->sFrameInfo.pts - pre_frame.pts;
				if (diff_pts > 27*1000) {
					logd("[pre:%ld,cur:%ld,diff:%ld,pre_id:%d,cur_id:%d]",pre_frame.pts,pFrameNode->sFrameInfo.pts,diff_pts,pre_frame.id,pFrameNode->sFrameInfo.id);
				 }
				pre_frame.id = pFrameNode->sFrameInfo.id;
				pre_frame.pts = pFrameNode->sFrameInfo.pts;

				if (pAudioRenderDataType->nVolumeChange) {
					if (pAudioRenderDataType->render->set_volume(pAudioRenderDataType->render,pAudioRenderDataType->nVolume) == 0) {
						pAudioRenderDataType->nVolumeChange = 0;
					} else {
						loge("set_volume error\n");
					}
				}
				OMX_ProcessAudioSync(pAudioRenderDataType,&pFrameNode->sFrameInfo);
				ret = 0;
				if (pFrameNode->sFrameInfo.size > 0) {// last frame size maybe 0.
					ret = pAudioRenderDataType->render->rend(pAudioRenderDataType->render,pFrameNode->sFrameInfo.data,pFrameNode->sFrameInfo.size);
					//nPktSize = pFrameNode->sFrameInfo.size;
				 }
				if (ret == 0) {
					aic_pthread_mutex_lock(&pAudioRenderDataType->sInFrameLock);
					mpp_list_del(&pFrameNode->sList);
					mpp_list_add_tail(&pFrameNode->sList, &pAudioRenderDataType->sInProcessedFrmae);
					aic_pthread_mutex_unlock(&pAudioRenderDataType->sInFrameLock);
					pAudioRenderDataType->nShowFrameOkNum++;
					//loge("nShowFrameOkNum:%d\n",pAudioRenderDataType->nShowFrameOkNum);
					#ifdef AUDIO_RENDRE_DUMP_ENABLE
					 {
						int end = 0;
						if (pFrameNode->sFrameInfo.flag & FRAME_FLAG_EOS) {
							end = 1;
							printf("[%s:%d]audio stream end \n",__FUNCTION__,__LINE__);
						 }
						OMX_AudioRenderDump(pAudioRenderDataType,pFrameNode->sFrameInfo.data,pFrameNode->sFrameInfo.size,end);
					 }
					#endif
					if (pFrameNode->sFrameInfo.flag & FRAME_FLAG_EOS) {
						pAudioRenderDataType->nFlags  |= AUDIO_RENDER_INPORT_SEND_ALL_FRAME_FLAG;
						//pAudioRenderDataType->nFrameEndFlag = OMX_TRUE;
						printf("[%s:%d]receive nFrameEndFlag\n",__FUNCTION__,__LINE__);
					 }

					 {
						static int nRenderFrameNum = 0;
						static struct timespec pre =  {0} ,cur =  {0};
						long diff;
						int nCnt = 0;

						aic_pthread_mutex_lock(&pAudioRenderDataType->sInFrameLock);
						mpp_list_for_each_entry(pFrameNode, &pAudioRenderDataType->sInReadyFrame, sList) {
							nCnt++;
						 }
						aic_pthread_mutex_unlock(&pAudioRenderDataType->sInFrameLock);


						nRenderFrameNum++;
						clock_gettime(CLOCK_REALTIME,&cur);
						diff = (cur.tv_sec - pre.tv_sec)*1000*1000 + (cur.tv_nsec - pre.tv_nsec)/1000;
						if (diff > 1*1000*1000) {
							logd("[%s:%d]:%ld:%d:%d\n",__FUNCTION__,__LINE__,diff,nRenderFrameNum,nCnt);
							pre = cur;
							nRenderFrameNum = 0;
						 }
					 }
				 } else {
					loge("frame erro!!!!\n");
					pAudioRenderDataType->nShowFrameFailNum++;

					break;
				 }
				logd("nReceiveFrameNum:%d,nShowFrameOkNum:%d,nShowFrameFailNum:%d\n"
					,pAudioRenderDataType->nReceiveFrameNum
					,pAudioRenderDataType->nShowFrameOkNum
					,pAudioRenderDataType->nShowFrameFailNum);
			 }
		 }
	 }
_EXIT:
	logi("nReceiveFrameNum:%d,"\
			"nLeftReadyFrameWhenCompoentExitNum:%d,"\
			"nShowFrameOkNum:%d,"\
			"nShowFrameFailNum:%d,"\
			"nGiveBackFrameOkNum:%d,"\
			"nGiveBackFrameFailNum:%d\n"
			,pAudioRenderDataType->nReceiveFrameNum
			,pAudioRenderDataType->nLeftReadyFrameWhenCompoentExitNum
			,pAudioRenderDataType->nShowFrameOkNum
			,pAudioRenderDataType->nShowFrameFailNum
			,pAudioRenderDataType->nGiveBackFrameOkNum
			,pAudioRenderDataType->nGiveBackFrameFailNum);
	logi("OMX_AudioRenderComponentThread EXIT\n");
	return (void*)OMX_ErrorNone;
}

