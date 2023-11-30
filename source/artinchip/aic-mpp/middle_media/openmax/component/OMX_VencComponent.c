/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: OMX_VencComponent
*/

#include "OMX_VencComponent.h"

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

#define OMX_VencListEmpty(list,mutex)\
({\
	int ret = 0;\
	aic_pthread_mutex_lock(&mutex);\
	ret = mpp_list_empty(list);\
	aic_pthread_mutex_unlock(&mutex);\
	(ret);\
})

static void* OMX_VencComponentThread(void* pThreadData);

static OMX_ERRORTYPE OMX_VencSendCommand(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_COMMANDTYPE Cmd,
		OMX_IN	OMX_U32 nParam1,
		OMX_IN	OMX_PTR pCmdData)
{
	VENC_DATA_TYPE *pVencDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	struct aic_message sMsg;
	pVencDataType = (VENC_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	sMsg.message_id = Cmd;
	sMsg.param = nParam1;
	sMsg.data_size = 0;

	//now not use always NULL
	if (pCmdData != NULL) {
		sMsg.data = pCmdData;
		sMsg.data_size = strlen((char*)pCmdData);
	}

	aic_msg_put(&pVencDataType->sMsgQue, &sMsg);
	return eError;
}

static OMX_ERRORTYPE OMX_VencGetParameter(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nParamIndex,
		OMX_INOUT OMX_PTR pComponentParameterStructure)
{
	VENC_DATA_TYPE *pVencDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;

	pVencDataType = (VENC_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

	switch (nParamIndex) {
		case OMX_IndexParamPortDefinition: {
			OMX_PARAM_PORTDEFINITIONTYPE *port = (OMX_PARAM_PORTDEFINITIONTYPE*)pComponentParameterStructure;
			if (port->nPortIndex == VDEC_PORT_IN_INDEX) {
				memcpy(port,&pVencDataType->sInPortDef,sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
			} else if (port->nPortIndex == VDEC_PORT_OUT_INDEX) {
				memcpy(port,&pVencDataType->sOutPortDef,sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
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
			if (sBufferSupplier->nPortIndex == 0) {
				sBufferSupplier->eBufferSupplier = pVencDataType->sInBufSupplier.eBufferSupplier;
			} else if (sBufferSupplier->nPortIndex == 1) {
				sBufferSupplier->eBufferSupplier = pVencDataType->sOutBufSupplier.eBufferSupplier;
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

static OMX_ERRORTYPE OMX_VencSetParameter(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nParamIndex,
		OMX_IN	OMX_PTR pComponentParameterStructure)
{
	VENC_DATA_TYPE *pVencDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_S32 nIndex = (OMX_S32)nParamIndex;
	pVencDataType = (VENC_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	switch (nIndex) {
	case OMX_IndexParamPortDefinition: {//width height
	OMX_PARAM_PORTDEFINITIONTYPE *pPortDefine = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;
		pVencDataType->sVideoStream.width = pPortDefine->format.video.nFrameWidth;
		pVencDataType->sVideoStream.height = pPortDefine->format.video.nFrameHeight;
		loge("width:%d,height:%d",pVencDataType->sVideoStream.width,pVencDataType->sVideoStream.height);
		break;
	}
	case OMX_IndexParamQFactor:  {//quality
		OMX_IMAGE_PARAM_QFACTORTYPE *Q = (OMX_IMAGE_PARAM_QFACTORTYPE *)pComponentParameterStructure;
		pVencDataType->nQuality = Q->nQFactor;
		loge("nQuality:%d",pVencDataType->nQuality);
		loge("");
		break;
	}
	default:
		 break;
	}
	return eError;
}

static OMX_ERRORTYPE OMX_VencGetConfig(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_INOUT OMX_PTR pComponentConfigStructure)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	return eError;
}

static OMX_ERRORTYPE OMX_VencSetConfig(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_IN	OMX_PTR pComponentConfigStructure)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	return eError;
}

static OMX_ERRORTYPE OMX_VencGetState(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_OUT OMX_STATETYPE* pState)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	VENC_DATA_TYPE* pVencDataType;
	pVencDataType = (VENC_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

	aic_pthread_mutex_lock(&pVencDataType->sStateLock);
	*pState = pVencDataType->state;
	aic_pthread_mutex_unlock(&pVencDataType->sStateLock);

	return eError;
}

static OMX_ERRORTYPE OMX_VencComponentTunnelRequest(
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
	VENC_DATA_TYPE* pVencDataType;
	pVencDataType = (VENC_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComp)->pComponentPrivate);
	if (pVencDataType->state != OMX_StateLoaded)
	 {
		loge("Component is not in OMX_StateLoaded,it is in%d,it can not tunnel\n",pVencDataType->state);
		return OMX_ErrorInvalidState;
	}
	if (nPort == VENC_PORT_IN_INDEX) {
		pPort = &pVencDataType->sInPortDef;
		pTunneledInfo = &pVencDataType->sInPortTunneledInfo;
		pBufSupplier = &pVencDataType->sInBufSupplier;
	} else if (nPort == VENC_PORT_OUT_INDEX) {
		pPort = &pVencDataType->sOutPortDef;
		pTunneledInfo = &pVencDataType->sOutPortTunneledInfo;
		pBufSupplier = &pVencDataType->sOutBufSupplier;
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

static OMX_ERRORTYPE OMX_VencEmptyThisBuffer(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	VENC_DATA_TYPE* pVencDataType;
	VENC_IN_FRAME *pFrameNode;
	struct aic_message sMsg;
	struct mpp_frame * pFrame = NULL;
	pVencDataType = (VENC_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

	aic_pthread_mutex_lock(&pVencDataType->sStateLock);
	if (pVencDataType->state != OMX_StateExecuting) {
		logw("component is not in OMX_StateExecuting,it is in [%d]!!!\n",pVencDataType->state);
		aic_pthread_mutex_unlock(&pVencDataType->sStateLock);
		return OMX_ErrorIncorrectStateOperation;
	}

	if (pVencDataType->sInPortTunneledInfo.nTunneledFlag) {
		if (pVencDataType->sInBufSupplier.eBufferSupplier != OMX_BufferSupplyOutput) {
			logw("OMX_ErrorNotImplemented\n");
			aic_pthread_mutex_unlock(&pVencDataType->sStateLock);
			return OMX_ErrorNotImplemented;
		}
	}

	if (OMX_VencListEmpty(&pVencDataType->sInEmptyFrame,pVencDataType->sInFrameLock)) {
		VENC_IN_FRAME *pFrameNode = (VENC_IN_FRAME*)mpp_alloc(sizeof(VENC_IN_FRAME));
		if (NULL == pFrameNode) {
			loge("OMX_ErrorInsufficientResources\n");
			aic_pthread_mutex_unlock(&pVencDataType->sStateLock);
			return OMX_ErrorInsufficientResources;
		}
		memset(pFrameNode,0x00,sizeof(VENC_IN_FRAME));
		aic_pthread_mutex_lock(&pVencDataType->sInFrameLock);
		mpp_list_add_tail(&pFrameNode->sList, &pVencDataType->sInEmptyFrame);
		aic_pthread_mutex_unlock(&pVencDataType->sInFrameLock);
		pVencDataType->nInFrameNodeNum++;
	}

	aic_pthread_mutex_lock(&pVencDataType->sInFrameLock);
	pFrameNode = mpp_list_first_entry(&pVencDataType->sInEmptyFrame, VENC_IN_FRAME, sList);
	pFrame = (struct mpp_frame *)pBuffer->pOutputPortPrivate;
	pFrameNode->sFrameInfo = *pFrame;
	mpp_list_del(&pFrameNode->sList);
	mpp_list_add_tail(&pFrameNode->sList, &pVencDataType->sInReadyFrame);
	aic_pthread_mutex_unlock(&pVencDataType->sInFrameLock);

	sMsg.message_id = OMX_CommandNops;
	sMsg.data_size = 0;
	aic_msg_put(&pVencDataType->sMsgQue, &sMsg);

	pVencDataType->nReceivePacktOkNum++;
	//loge("pVencDataType->nReceivePacktOkNum:%d\n",pVencDataType->nReceivePacktOkNum);
	aic_pthread_mutex_unlock(&pVencDataType->sStateLock);
	return eError;
}

static OMX_ERRORTYPE OMX_VencFillThisBuffer(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	VENC_DATA_TYPE* pVencDataType;
	struct aic_av_packet *pPkt;
	VENC_OUT_PACKET *pPktNode1 = NULL,*pPktNode2 = NULL;
	OMX_BOOL bMatch = OMX_FALSE;
	pVencDataType = (VENC_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

	if (pBuffer->nOutputPortIndex != VENC_PORT_OUT_INDEX) {
		loge("port not match\n");
		return OMX_ErrorBadParameter;
	}

	if (!OMX_VencListEmpty(&pVencDataType->sOutProcessingPkt,pVencDataType->sOutPktLock)) {
		pPkt = (struct aic_av_packet *)pBuffer->pOutputPortPrivate;
		aic_pthread_mutex_lock(&pVencDataType->sOutPktLock);
		mpp_list_for_each_entry_safe(pPktNode1,pPktNode2,&pVencDataType->sOutProcessingPkt,sList) {
			if (pPktNode1->sPkt.data == pPkt->data) {
					bMatch = OMX_TRUE;
					break;
				}
		}
		logi("bMatch:%d\n",bMatch);
		if (bMatch) {//give frame back to decoder
			struct aic_message sMsg;

			mpp_list_del(&pPktNode1->sList);
			mpp_list_add_tail(&pPktNode1->sList, &pVencDataType->sOutEmptyPkt);

			sMsg.message_id = OMX_CommandNops;
			sMsg.data_size = 0;
			aic_msg_put(&pVencDataType->sMsgQue, &sMsg);
		} else {
			loge("frame not match!!!\n");
			eError =  OMX_ErrorBadParameter;
			pVencDataType->nSendBackFrameErrorNum++;
		}
		logi("pVencDataType->nSendBackFrameOkNum:%d,pVencDataType->nSendBackFrameErrorNum:%d"
			,pVencDataType->nSendBackFrameOkNum
			,pVencDataType->nSendBackFrameErrorNum);
		aic_pthread_mutex_unlock(&pVencDataType->sOutPktLock);
	} else {
		logw("no frame need to back \n");
		eError =  OMX_ErrorBadParameter;
	}

	return eError;
}

static OMX_ERRORTYPE OMX_VencSetCallbacks(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_CALLBACKTYPE* pCallbacks,
		OMX_IN	OMX_PTR pAppData)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	VENC_DATA_TYPE* pVencDataType;
	pVencDataType = (VENC_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	pVencDataType->pCallbacks = pCallbacks;
	pVencDataType->pAppData = pAppData;
	return eError;
}

OMX_ERRORTYPE OMX_VencComponentDeInit(OMX_IN	OMX_HANDLETYPE hComponent)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_COMPONENTTYPE *pComp;
	VENC_DATA_TYPE *pVencDataType;
	VENC_OUT_PACKET *pPktNode = NULL,*pPktNode1 = NULL;
	VENC_IN_FRAME *pFrameNode = NULL,*pFrameNode1 = NULL;
	pComp = (OMX_COMPONENTTYPE *)hComponent;
	struct aic_message sMsg;
	pVencDataType = (VENC_DATA_TYPE *)pComp->pComponentPrivate;

	aic_pthread_mutex_lock(&pVencDataType->sStateLock);
	if (pVencDataType->state != OMX_StateLoaded) {
		logw("compoent is in %d,but not in OMX_StateLoaded(1),can not FreeHandle.\n",pVencDataType->state);
		aic_pthread_mutex_unlock(&pVencDataType->sStateLock);
		return OMX_ErrorIncorrectStateOperation;
	}
	aic_pthread_mutex_unlock(&pVencDataType->sStateLock);

	sMsg.message_id = OMX_CommandStop;
	sMsg.data_size = 0;
	aic_msg_put(&pVencDataType->sMsgQue, &sMsg);
	pthread_join(pVencDataType->threadId, (void*)&eError);

	aic_pthread_mutex_lock(&pVencDataType->sInFrameLock);
	if (!mpp_list_empty(&pVencDataType->sInEmptyFrame)) {
			mpp_list_for_each_entry_safe(pFrameNode, pFrameNode1, &pVencDataType->sInEmptyFrame, sList) {
				mpp_list_del(&pFrameNode->sList);
				mpp_free(pFrameNode);
		}
	}

	if (!mpp_list_empty(&pVencDataType->sInReadyFrame)) {
			mpp_list_for_each_entry_safe(pFrameNode, pFrameNode1, &pVencDataType->sInReadyFrame, sList) {
				mpp_list_del(&pFrameNode->sList);
				mpp_free(pPktNode);
		}
	}

	aic_pthread_mutex_unlock(&pVencDataType->sInFrameLock);

	aic_pthread_mutex_lock(&pVencDataType->sOutPktLock);
	if (!mpp_list_empty(&pVencDataType->sOutEmptyPkt)) {
			mpp_list_for_each_entry_safe(pPktNode, pPktNode1, &pVencDataType->sOutEmptyPkt, sList) {
				mpp_list_del(&pPktNode->sList);
				if (pPktNode->nDMAFd > 0) {
					dmabuf_munmap(pPktNode->nDMAMapPhyAddr, pPktNode->nDMASize);
					dmabuf_free(pPktNode->nDMAFd);
				}
				mpp_free(pPktNode);
		}
	}
	if (!mpp_list_empty(&pVencDataType->sOutReadyPkt)) {
			mpp_list_for_each_entry_safe(pPktNode, pPktNode1, &pVencDataType->sOutReadyPkt, sList) {
				mpp_list_del(&pPktNode->sList);
				if (pPktNode->nDMAFd > 0) {
					dmabuf_munmap(pPktNode->nDMAMapPhyAddr, pPktNode->nDMASize);
					dmabuf_free(pPktNode->nDMAFd);
				}
				mpp_free(pPktNode);
		}
	}

	if (!mpp_list_empty(&pVencDataType->sOutProcessingPkt)) {
			mpp_list_for_each_entry_safe(pPktNode, pPktNode1, &pVencDataType->sOutProcessingPkt, sList) {
				mpp_list_del(&pPktNode->sList);
				if (pPktNode->nDMAFd > 0) {
					dmabuf_munmap(pPktNode->nDMAMapPhyAddr, pPktNode->nDMASize);
					dmabuf_free(pPktNode->nDMAFd);
				}
				mpp_free(pPktNode);
		}
	}
	aic_pthread_mutex_unlock(&pVencDataType->sOutPktLock);

	pthread_mutex_destroy(&pVencDataType->sInFrameLock);
	pthread_mutex_destroy(&pVencDataType->sOutPktLock);
	pthread_mutex_destroy(&pVencDataType->sStateLock);

	aic_msg_destroy(&pVencDataType->sMsgQue);

	mpp_free(pVencDataType);
	pVencDataType = NULL;

	logw("OMX_VideoRenderComponentDeInit\n");
	return eError;
}

OMX_ERRORTYPE OMX_VencComponentInit(OMX_IN	OMX_HANDLETYPE hComponent)
{
	OMX_COMPONENTTYPE *pComp;
	VENC_DATA_TYPE *pVencDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_U32 err;
	OMX_U32 i;
	OMX_S8 nMsgCreateOk = 0;
	OMX_S8 nInFrameLockInitOk = 0;
	OMX_S8 nOutPktLockInitOk = 0;
	OMX_S8 nSateLockInitOk = 0;

	logw("OMX_VencComponentInit....");

	pComp = (OMX_COMPONENTTYPE *)hComponent;

	pVencDataType = (VENC_DATA_TYPE *)mpp_alloc(sizeof(VENC_DATA_TYPE));

	if (NULL == pVencDataType)  {
		loge("mpp_alloc(sizeof(VENC_DATA_TYPE) fail!");
		return OMX_ErrorInsufficientResources;
	}

	memset(pVencDataType, 0x0, sizeof(VENC_DATA_TYPE));
	pComp->pComponentPrivate	= (void*) pVencDataType;
	pVencDataType->state 		= OMX_StateLoaded;
	pVencDataType->hSelf 		= pComp;

	pComp->SetCallbacks 		= OMX_VencSetCallbacks;
	pComp->SendCommand			= OMX_VencSendCommand;
	pComp->GetState 			= OMX_VencGetState;
	pComp->GetParameter 		= OMX_VencGetParameter;
	pComp->SetParameter 		= OMX_VencSetParameter;
	pComp->GetConfig			= OMX_VencGetConfig;
	pComp->SetConfig			= OMX_VencSetConfig;
	pComp->ComponentTunnelRequest = OMX_VencComponentTunnelRequest;
	pComp->ComponentDeInit		= OMX_VencComponentDeInit;
	pComp->FillThisBuffer		= OMX_VencFillThisBuffer;
	pComp->EmptyThisBuffer		= OMX_VencEmptyThisBuffer;

	pVencDataType->sPortParam.nPorts = 2;
	pVencDataType->sPortParam.nStartPortNumber = 0x0;

	pVencDataType->sInPortDef.nPortIndex = VENC_PORT_IN_INDEX;
	pVencDataType->sInPortDef.bPopulated = OMX_TRUE;
	pVencDataType->sInPortDef.bEnabled	= OMX_TRUE;
	pVencDataType->sInPortDef.eDomain = OMX_PortDomainVideo;
	pVencDataType->sInPortDef.eDir = OMX_DirInput;

	pVencDataType->sOutPortDef.nPortIndex = VENC_PORT_OUT_INDEX;
	pVencDataType->sOutPortDef.bPopulated = OMX_TRUE;
	pVencDataType->sOutPortDef.bEnabled = OMX_TRUE;
	pVencDataType->sOutPortDef.eDomain = OMX_PortDomainVideo;
	pVencDataType->sOutPortDef.eDir = OMX_DirOutput;

	pVencDataType->sInPortTunneledInfo.nPortIndex = VENC_PORT_IN_INDEX;
	pVencDataType->sInPortTunneledInfo.pSelfComp = hComponent;
	pVencDataType->sOutPortTunneledInfo.nPortIndex = VENC_PORT_OUT_INDEX;
	pVencDataType->sOutPortTunneledInfo.pSelfComp = hComponent;

	pVencDataType->sInBufSupplier.nPortIndex = VENC_PORT_IN_INDEX;
	pVencDataType->sInBufSupplier.eBufferSupplier = OMX_BufferSupplyOutput;
	// venc support out pot buffer
	pVencDataType->sOutBufSupplier.nPortIndex = VENC_PORT_OUT_INDEX;
	pVencDataType->sOutBufSupplier.eBufferSupplier = OMX_BufferSupplyOutput;

	pVencDataType->nOutPktNodeNum = 0;
	mpp_list_init(&pVencDataType->sInEmptyFrame);
	mpp_list_init(&pVencDataType->sInReadyFrame);
	mpp_list_init(&pVencDataType->sInProcessedFrame);
	for(i =0 ; i < VENC_PACKET_ONE_TIME_CREATE_NUM; i++) {
		VENC_IN_FRAME *pFrameNode = (VENC_IN_FRAME*)mpp_alloc(sizeof(VENC_IN_FRAME));
		if (NULL == pFrameNode) {
			break;
		}
		memset(pFrameNode,0x00,sizeof(VENC_IN_FRAME));
		mpp_list_add_tail(&pFrameNode->sList, &pVencDataType->sInEmptyFrame);
		pVencDataType->nInFrameNodeNum++;
	}
	if (pVencDataType->nInFrameNodeNum == 0) {
		loge("mpp_alloc empty video node fail\n");
		eError = OMX_ErrorInsufficientResources;
		goto _EXIT;
	}

	pVencDataType->nOutPktNodeNum = 0;
	mpp_list_init(&pVencDataType->sOutEmptyPkt);
	mpp_list_init(&pVencDataType->sOutReadyPkt);
	mpp_list_init(&pVencDataType->sOutProcessingPkt);

	for(i =0 ; i < VENC_FRAME_ONE_TIME_CREATE_NUM; i++) {
		VENC_OUT_PACKET *pPktNode = (VENC_OUT_PACKET*)mpp_alloc(sizeof(VENC_OUT_PACKET));
		if (NULL == pPktNode) {
			break;
		}
		memset(pPktNode,0x00,sizeof(VENC_OUT_PACKET));
		pPktNode->nDMAFd = -1;
		mpp_list_add_tail(&pPktNode->sList, &pVencDataType->sOutEmptyPkt);
		pVencDataType->nOutPktNodeNum++;
	}
	if (pVencDataType->nOutPktNodeNum == 0) {
		loge("mpp_alloc empty video node fail\n");
		eError = OMX_ErrorInsufficientResources;
		goto _EXIT;
	}

	pthread_mutex_init(&pVencDataType->sInFrameLock, NULL);
	pthread_mutex_init(&pVencDataType->sOutPktLock, NULL);
	pthread_mutex_init(&pVencDataType->sStateLock, NULL);

	if (aic_msg_create(&pVencDataType->sMsgQue)<0)
	 {
		loge("aic_msg_create fail!\n");
		eError = OMX_ErrorInsufficientResources;
		goto _EXIT;
	}
	nMsgCreateOk = 1;
	if (pthread_mutex_init(&pVencDataType->sInFrameLock, NULL)) {
		loge("pthread_mutex_init fail!\n");
		goto _EXIT;
	}
	nInFrameLockInitOk = 1;
	if (pthread_mutex_init(&pVencDataType->sOutPktLock, NULL)) {
		loge("pthread_mutex_init fail!\n");
		goto _EXIT;
	}
	nOutPktLockInitOk = 1;
	if (pthread_mutex_init(&pVencDataType->sStateLock, NULL)) {
		loge("pthread_mutex_init fail!\n");
		goto _EXIT;
	}
	nSateLockInitOk = 1;

	pVencDataType->nDMADevice = dmabuf_device_open();

	if (pVencDataType->nDMADevice < 0) {
		goto _EXIT;
	}

	// Create the component thread
	err = pthread_create(&pVencDataType->threadId, NULL, OMX_VencComponentThread, pVencDataType);
	if (err || !pVencDataType->threadId) {
		loge("pthread_create fail!");
		eError = OMX_ErrorInsufficientResources;
		goto _EXIT;
	}

	return eError;

_EXIT:
	if (pVencDataType->nDMADevice > 0) {
		dmabuf_device_close(pVencDataType->nDMADevice);
	}
	if (nSateLockInitOk) {
		pthread_mutex_destroy(&pVencDataType->sStateLock);
	}
	if (nOutPktLockInitOk) {
		pthread_mutex_destroy(&pVencDataType->sOutPktLock);
	}
	if (nInFrameLockInitOk) {
		pthread_mutex_destroy(&pVencDataType->sInFrameLock);
	}
	if (nMsgCreateOk) {
		aic_msg_destroy(&pVencDataType->sMsgQue);
	}
	if (!mpp_list_empty(&pVencDataType->sOutEmptyPkt)) {
		VENC_OUT_PACKET *pPktNode = NULL,*pPktNode1 = NULL;
		mpp_list_for_each_entry_safe(pPktNode, pPktNode1, &pVencDataType->sOutEmptyPkt, sList) {
			mpp_list_del(&pPktNode->sList);
			mpp_free(pPktNode);
		}
	}
	if (!mpp_list_empty(&pVencDataType->sInEmptyFrame)) {
		VENC_IN_FRAME *pFrameNode = NULL,*pFrameNode1 = NULL;
		mpp_list_for_each_entry_safe(pFrameNode, pFrameNode1, &pVencDataType->sInEmptyFrame, sList) {
			mpp_list_del(&pFrameNode->sList);
			mpp_free(pFrameNode);
		}
	}
	if (pVencDataType) {
		mpp_free(pVencDataType);
		pVencDataType = NULL;
	}
	return eError;
}

static void OMX_VencEventNotify(
		VENC_DATA_TYPE * pVencDataType,
		OMX_EVENTTYPE event,
		OMX_U32 nData1,
		OMX_U32 nData2,
		OMX_PTR pEventData)
{
	if (pVencDataType && pVencDataType->pCallbacks && pVencDataType->pCallbacks->EventHandler)  {
		pVencDataType->pCallbacks->EventHandler(
					pVencDataType->hSelf,
					pVencDataType->pAppData,event,
					nData1, nData2, pEventData);
	}

}

static void OMX_VencStateChangeToInvalid(VENC_DATA_TYPE * pVencDataType)
{
	pVencDataType->state = OMX_StateInvalid;
	OMX_VencEventNotify(pVencDataType
						,OMX_EventError
						,OMX_ErrorInvalidState,0,NULL);
	OMX_VencEventNotify(pVencDataType
						,OMX_EventCmdComplete
						,OMX_CommandStateSet
						,pVencDataType->state,NULL);
}

static void OMX_VencStateChangeToIdle(VENC_DATA_TYPE * pVencDataType)
{
	if (OMX_StateLoaded == pVencDataType->state) {

	} else if (OMX_StateExecuting == pVencDataType->state) {

	} else if (OMX_StatePause == pVencDataType->state) {

	} else  {
		OMX_VencEventNotify(pVencDataType
						,OMX_EventError
						,OMX_ErrorIncorrectStateTransition
						,pVencDataType->state,NULL);
		loge("OMX_ErrorIncorrectStateTransition\n");
		return;
	}

	pVencDataType->state = OMX_StateIdle;
	OMX_VencEventNotify(pVencDataType
						,OMX_EventCmdComplete
						,OMX_CommandStateSet
						,pVencDataType->state,NULL);
}

static void OMX_VencStateChangeToLoaded(VENC_DATA_TYPE * pVencDataType)
{
	if (OMX_StateIdle == pVencDataType->state) {
		// //wait for	all out port packet from other component or app to back.
		// logi("Before OMX_VencComponentThread exit,it must wait for sOutAudioProcessingPkt empty\n");
		// while (!OMX_VencListEmpty(&pVencDataType->sOutAudioProcessingPkt,pVencDataType->sAudioPktLock)) {
		// 	usleep(1000);
		// }
		// logi("Before OMX_VencComponentThread exit,it must wait for sOutVideoProcessingPkt empty\n");
		// while (!OMX_VencListEmpty(&pVencDataType->sOutVideoProcessingPkt,pVencDataType->sVideoPktLock)) {
		// 	usleep(1000);
		// }
		logi("OMX_VencStateChangeToLoaded\n");

		pVencDataType->state = OMX_StateLoaded;
		OMX_VencEventNotify(pVencDataType
						,OMX_EventCmdComplete
						,OMX_CommandStateSet
						,pVencDataType->state,NULL);
	} else {
		OMX_VencEventNotify(pVencDataType
						,OMX_EventError
						,OMX_ErrorIncorrectStateTransition
						, pVencDataType->state,NULL);
		loge("OMX_ErrorIncorrectStateTransition\n");
	}

}

static void OMX_VencStateChangeToExecuting(VENC_DATA_TYPE * pVencDataType)
{
	if (OMX_StateIdle == pVencDataType->state) {
		// if (NULL == pVencDataType->pParser) {
		// 	OMX_VencEventNotify(pVencDataType
		// 					,OMX_EventError
		// 					,OMX_ErrorIncorrectStateTransition
		// 					,pVencDataType->state,NULL);
		// 	loge("pVencDataType->pParser is not created,please set param OMX_IndexParamContentURI!!!!!\n");
		// 	return;
		// }
	} else if (OMX_StatePause == pVencDataType->state) {
	//
	} else {
		OMX_VencEventNotify(pVencDataType
						,OMX_EventError
						,OMX_ErrorIncorrectStateTransition
						,pVencDataType->state,NULL);
		loge("OMX_ErrorIncorrectStateTransition\n");
		return;
	}
	pVencDataType->state = OMX_StateExecuting;

}

static void OMX_VencStateChangeToPause(VENC_DATA_TYPE * pVencDataType)
{
	if (OMX_StateExecuting == pVencDataType->state) {

	} else {
		OMX_VencEventNotify(pVencDataType
						,OMX_EventError
						,OMX_ErrorIncorrectStateTransition
						,pVencDataType->state,NULL);
		logd("OMX_ErrorIncorrectStateTransition\n");
		return;
	}
	pVencDataType->state = OMX_StatePause;

}

static void* OMX_VencComponentThread(void* pThreadData)
{
	struct aic_message message;
	OMX_S32 nCmd;		//OMX_COMMANDTYPE
	OMX_S32 nCmdData;	//OMX_STATETYPE
	VENC_DATA_TYPE* pVencDataType = (VENC_DATA_TYPE*)pThreadData;
	OMX_S32 ret;
	VENC_IN_FRAME *pFrameNode = NULL;
	VENC_OUT_PACKET *pPktNode = NULL;

	while (1) {
_AIC_MSG_GET_:
		if (aic_msg_get(&pVencDataType->sMsgQue, &message) == 0) {
			nCmd = message.message_id;
			nCmdData = message.param;
			logi("nCmd:%d, nCmdData:%d\n",nCmd,nCmdData);
 			if (OMX_CommandStateSet == nCmd) {
				aic_pthread_mutex_lock(&pVencDataType->sStateLock);
				if (pVencDataType->state == (OMX_STATETYPE)(nCmdData)) {
					OMX_VencEventNotify(pVencDataType,OMX_EventError,OMX_ErrorSameState,0,NULL);
					aic_pthread_mutex_unlock(&pVencDataType->sStateLock);
					continue;
				}
				switch((OMX_STATETYPE)(nCmdData)) {
				case OMX_StateInvalid:
					OMX_VencStateChangeToInvalid(pVencDataType);
					break;
				case OMX_StateLoaded://idel->loaded means stop
					OMX_VencStateChangeToLoaded(pVencDataType);
					break;
				case OMX_StateIdle:
					OMX_VencStateChangeToIdle(pVencDataType);
					break;
				case OMX_StateExecuting:
					OMX_VencStateChangeToExecuting(pVencDataType);
					break;
				case OMX_StatePause:
					OMX_VencStateChangeToPause(pVencDataType);
					break;
				case OMX_StateWaitForResources:
					break;
				default:
					break;
				}
				aic_pthread_mutex_unlock(&pVencDataType->sStateLock);
 			} else if (OMX_CommandFlush == nCmd) {

			} else if (OMX_CommandPortDisable == nCmd) {

			} else if (OMX_CommandPortEnable == nCmd) {

			} else if (OMX_CommandMarkBuffer == nCmd) {

			} else if (OMX_CommandStop == nCmd) {
				logi("OMX_VencComponentThread ready to exit!!!\n");
				goto _EXIT;
			} else if (OMX_CommandNops == nCmd) {

			} else {

			}
		}

		if (pVencDataType->state != OMX_StateExecuting) {
			aic_msg_wait_new_msg(&pVencDataType->sMsgQue, 0);
			continue;
		}

		// return frame
		while (!OMX_VencListEmpty(&pVencDataType->sInProcessedFrame,pVencDataType->sInFrameLock)) {
			OMX_BUFFERHEADERTYPE sBuffHead;
			OMX_PORT_TUNNELEDINFO *pTunneldInfo;

			aic_pthread_mutex_lock(&pVencDataType->sInFrameLock);
			pFrameNode = mpp_list_first_entry(&pVencDataType->sInProcessedFrame,VENC_IN_FRAME,sList);
			aic_pthread_mutex_unlock(&pVencDataType->sInFrameLock);
			pTunneldInfo = &pVencDataType->sInPortTunneledInfo;
			sBuffHead.nOutputPortIndex = VENC_PORT_OUT_INDEX;
			sBuffHead.nInputPortIndex = MUX_PORT_VIDEO_INDEX;
			sBuffHead.pOutputPortPrivate = (OMX_U8 *)&pFrameNode->sFrameInfo;
			ret = 0;
			if (pTunneldInfo->nTunneledFlag) {
				ret = OMX_FillThisBuffer(pTunneldInfo->pTunneledComp,&sBuffHead);
			} else {
				if (pVencDataType->pCallbacks != NULL && pVencDataType->pCallbacks->EmptyBufferDone != NULL) {
						ret = pVencDataType->pCallbacks->EmptyBufferDone(pVencDataType->hSelf,pVencDataType->pAppData,&sBuffHead);
					}
			}
			if (ret != 0) {
				loge("give back frame to venc fail\n");
				break;
			}
			aic_pthread_mutex_lock(&pVencDataType->sInFrameLock);
			mpp_list_del(&pFrameNode->sList);
			mpp_list_add_tail(&pFrameNode->sList, &pVencDataType->sInEmptyFrame);
			aic_pthread_mutex_unlock(&pVencDataType->sInFrameLock);
			logd("give back frame to venc ok");
		}

		if (OMX_VencListEmpty(&pVencDataType->sInReadyFrame,pVencDataType->sInFrameLock)) {
			aic_msg_wait_new_msg(&pVencDataType->sMsgQue, 0);
			continue;
		}

		while (!OMX_VencListEmpty(&pVencDataType->sInReadyFrame,pVencDataType->sInFrameLock)) {
			if (OMX_VencListEmpty(&pVencDataType->sOutEmptyPkt,pVencDataType->sOutPktLock)) {
				pPktNode = (VENC_OUT_PACKET*)mpp_alloc(sizeof(VENC_OUT_PACKET));
				if (NULL == pPktNode) {
					loge("mpp_alloc error");
					goto _AIC_MSG_GET_;
				}
				memset(pPktNode,0x00,sizeof(VENC_OUT_PACKET));
				pPktNode->nDMAFd = -1;
				aic_pthread_mutex_lock(&pVencDataType->sOutPktLock);
				mpp_list_add_tail(&pPktNode->sList, &pVencDataType->sOutEmptyPkt);
				aic_pthread_mutex_unlock(&pVencDataType->sOutPktLock);
				pVencDataType->nOutPktNodeNum++;
			}
			pFrameNode = mpp_list_first_entry(&pVencDataType->sInReadyFrame, VENC_IN_FRAME, sList);
			pPktNode = mpp_list_first_entry(&pVencDataType->sOutEmptyPkt,VENC_OUT_PACKET,sList);
			if (pPktNode->nDMAFd == -1) {
				int width = pVencDataType->sVideoStream.width;
				int height = pVencDataType->sVideoStream.height;
				int quality =  pVencDataType->nQuality;
				pPktNode->nDMASize = width * height * 4/5 * quality / 100;
				pPktNode->nDMAFd = dmabuf_alloc(pVencDataType->nDMADevice, pPktNode->nDMASize);
				if (pPktNode->nDMAFd < 0) {
					loge("nDMADevice:%d,nDMAMapPhyAddr:%p,nDMAFd:%d,nDMASize:%d",pVencDataType->nDMADevice,pPktNode->nDMAMapPhyAddr,pPktNode->nDMAFd,pPktNode->nDMASize);
					loge("dmabuf_alloc failed");
					goto _AIC_MSG_GET_;
				}
				pPktNode->nDMAMapPhyAddr = pPktNode->sPkt.data = dmabuf_mmap(pPktNode->nDMAFd, pPktNode->nDMASize);
			}
			if (mpp_encode_jpeg(&pFrameNode->sFrameInfo, pVencDataType->nQuality, pPktNode->nDMAFd,  pPktNode->nDMASize, &pPktNode->sPkt.size) < 0) {
				loge("encode failed");
				goto _AIC_MSG_GET_;
			}
			pPktNode->sPkt.dts = pPktNode->sPkt.pts = pFrameNode->sFrameInfo.pts;
			OMX_BUFFERHEADERTYPE sBuffHead;
			sBuffHead.nOutputPortIndex = ADEC_PORT_OUT_INDEX;
			sBuffHead.pOutputPortPrivate = (OMX_U8 *)&pPktNode->sPkt;
			ret = 0;
			if (pVencDataType->sOutPortTunneledInfo.nTunneledFlag) {
				sBuffHead.nInputPortIndex = pVencDataType->sOutPortTunneledInfo.nTunnelPortIndex;
				ret = OMX_EmptyThisBuffer(pVencDataType->sOutPortTunneledInfo.pTunneledComp,&sBuffHead);
			} else {
				if (pVencDataType->pCallbacks != NULL && pVencDataType->pCallbacks->FillBufferDone != NULL) {
						ret = pVencDataType->pCallbacks->FillBufferDone(pVencDataType->hSelf,pVencDataType->pAppData,&sBuffHead);
					}
			}
			if (ret != 0) {
				logw("OMX_EmptyThisBuffer or FillBufferDone error");
				goto _AIC_MSG_GET_;
			}
			// push Inframe to processed list
			aic_pthread_mutex_lock(&pVencDataType->sInFrameLock);
			mpp_list_del(&pFrameNode->sList);
			mpp_list_add_tail(&pFrameNode->sList,&pVencDataType->sInProcessedFrame);
			aic_pthread_mutex_unlock(&pVencDataType->sInFrameLock);
			// push sOutPkt to processing list
			aic_pthread_mutex_lock(&pVencDataType->sOutPktLock);
			mpp_list_del(&pPktNode->sList);
			mpp_list_add_tail(&pPktNode->sList, &pVencDataType->sOutProcessingPkt);
			aic_pthread_mutex_unlock(&pVencDataType->sOutPktLock);
		}
	}
_EXIT:
	return (void*)OMX_ErrorNone;

}
