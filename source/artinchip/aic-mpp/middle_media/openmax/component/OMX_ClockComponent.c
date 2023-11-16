/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: OMX_ClockComponent
*/

#include "OMX_ClockComponent.h"

static OMX_ERRORTYPE OMX_ClockSendCommand(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_COMMANDTYPE Cmd,
		OMX_IN	OMX_U32 nParam1,
		OMX_IN	OMX_PTR pCmdData);

static OMX_ERRORTYPE OMX_ClockGetParameter(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nParamIndex,
		OMX_INOUT OMX_PTR pComponentParameterStructure);

static OMX_ERRORTYPE OMX_ClockSetParameter(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_IN	OMX_PTR pComponentParameterStructure);

static OMX_ERRORTYPE OMX_ClockGetConfig(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_INOUT OMX_PTR pComponentConfigStructure);

static OMX_ERRORTYPE OMX_ClockSetConfig(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_IN	OMX_PTR pComponentConfigStructure);

static OMX_ERRORTYPE OMX_ClockGetState(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_OUT OMX_STATETYPE* pState);

static OMX_ERRORTYPE OMX_ClockComponentTunnelRequest(
	OMX_IN	OMX_HANDLETYPE hComp,
	OMX_IN	OMX_U32 nPort,
	OMX_IN	OMX_HANDLETYPE hTunneledComp,
	OMX_IN	OMX_U32 nTunneledPort,
	OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup);

static OMX_ERRORTYPE OMX_ClockEmptyThisBuffer(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_BUFFERHEADERTYPE* pBuffer);

static OMX_ERRORTYPE OMX_ClockFillThisBuffer(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_BUFFERHEADERTYPE* pBuffer);

static OMX_ERRORTYPE OMX_ClockSetCallbacks(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_CALLBACKTYPE* pCallbacks,
		OMX_IN	OMX_PTR pAppData);

static OMX_ERRORTYPE OMX_ClockOMX_IndexConfigTimePosition(
		OMX_HANDLETYPE hComponent,
		OMX_TIME_CONFIG_TIMESTAMPTYPE *pTimeStamp);

static OMX_ERRORTYPE OMX_ClockOMX_IndexConfigTimePosition(
		OMX_HANDLETYPE hComponent,
		OMX_TIME_CONFIG_TIMESTAMPTYPE *pTimeStamp)
{
	CLOCK_DATA_TYPE *pClockDataType = (CLOCK_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	pClockDataType->sClockState.eState = OMX_TIME_ClockStateWaitingForStartTime;
	pClockDataType->sClockState.nWaitMask |= (OMX_CLOCKPORT0|OMX_CLOCKPORT1);
	loge("OMX_ClockOMX_IndexConfigTimePosition\n");
	return OMX_ErrorNone;
}

static OMX_ERRORTYPE OMX_ClockGetParameter(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nParamIndex,
		OMX_INOUT OMX_PTR pComponentParameterStructure)
{
	CLOCK_DATA_TYPE *pClockDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;

	pClockDataType = (CLOCK_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

	switch (nParamIndex) {
	case OMX_IndexParamPortDefinition: {
		OMX_PARAM_PORTDEFINITIONTYPE *port = (OMX_PARAM_PORTDEFINITIONTYPE*)pComponentParameterStructure;
		if (port->nPortIndex == CLOCK_PORT_OUT_VIDEO) {
			memcpy(port,&pClockDataType->sOutPortDef[CLOCK_PORT_OUT_VIDEO],sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
		} else if (port->nPortIndex == CLOCK_PORT_OUT_AUDIO) {
			memcpy(port,&pClockDataType->sOutPortDef[CLOCK_PORT_OUT_AUDIO],sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
		} else {
			eError = OMX_ErrorBadParameter;
		}
		break;
	}
	case OMX_IndexParamCompBufferSupplier: {
		OMX_PARAM_BUFFERSUPPLIERTYPE *sBufferSupplier = (OMX_PARAM_BUFFERSUPPLIERTYPE*)pComponentParameterStructure;
		if (sBufferSupplier->nPortIndex == CLOCK_PORT_OUT_VIDEO) {
			sBufferSupplier->eBufferSupplier = pClockDataType->sOutBufSupplier[CLOCK_PORT_OUT_VIDEO].eBufferSupplier;
		} else if (sBufferSupplier->nPortIndex == CLOCK_PORT_OUT_AUDIO) {
			sBufferSupplier->eBufferSupplier = pClockDataType->sOutBufSupplier[CLOCK_PORT_OUT_AUDIO].eBufferSupplier;
		}  else {
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

static OMX_ERRORTYPE OMX_ClockSetParameter(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nParamIndex,
		OMX_IN	OMX_PTR pComponentParameterStructure)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	if (pComponentParameterStructure == NULL) {
		loge("param error!!!\n");
		return OMX_ErrorBadParameter;
	}
	switch (nParamIndex) {
		case OMX_IndexParamPortDefinition:
			break;
		default:
		 break;
	}
	return eError;
}

static OMX_S64 OMX_ClockGetSystemTime()
{
	struct timespec ts =  {0,0};
	OMX_S64 tick = 0;
	clock_gettime(CLOCK_MONOTONIC,&ts);
	tick = ts.tv_sec*1000000 + ts.tv_nsec/1000;
	return tick;
}

static OMX_ERRORTYPE OMX_ClockConfigTimeCurrentAudioReference(OMX_HANDLETYPE hComponent,OMX_TIME_CONFIG_TIMESTAMPTYPE *pTimeStamp)
{
	OMX_S64 nCurMeidaTime;
	OMX_S64 nDiffTime;
	CLOCK_DATA_TYPE *pClockDataType;
	pClockDataType = (CLOCK_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	if (pClockDataType->sClockState.eState != OMX_TIME_ClockStateRunning) {
		loge("clockState are not in OMX_TIME_ClockStateStopped,do not set!!!\n");
		pTimeStamp->nTimestamp = -1;
		return OMX_ErrorUndefined;
	}

	nCurMeidaTime = (OMX_ClockGetSystemTime() - pClockDataType->sWallTimeBase - pClockDataType->sPauseTimeDurtion) + pClockDataType->sRefClockTimeBase;

	nDiffTime = nCurMeidaTime - pTimeStamp->nTimestamp;
	if (nDiffTime > 10*1000 || nDiffTime < -10*1000) {//10ms
		pClockDataType->sRefClockTimeBase = pTimeStamp->nTimestamp;
		pClockDataType->sWallTimeBase = OMX_ClockGetSystemTime();
		pClockDataType->sPauseTimeDurtion = 0;
	}

	return OMX_ErrorNone;
}

static OMX_ERRORTYPE OMX_ClockGetCurrentMediaTime(OMX_HANDLETYPE hComponent,OMX_TIME_CONFIG_TIMESTAMPTYPE *pTimeStamp)
{
	CLOCK_DATA_TYPE *pClockDataType;
	pClockDataType = (CLOCK_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	if (pClockDataType->sClockState.eState != OMX_TIME_ClockStateRunning) {
		//loge("clockState are not in OMX_TIME_ClockStateRunning,do not get media time!!!\n");
		pTimeStamp->nTimestamp = -1;
		return OMX_ErrorUndefined;
	}
	logd("curTime:%ld,sWallTimeBase:%ld,sRefClockTimeBase:%ld\n",OMX_ClockGetSystemTime(),pClockDataType->sWallTimeBase,pClockDataType->sRefClockTimeBase);
	pTimeStamp->nTimestamp = (OMX_ClockGetSystemTime() - pClockDataType->sWallTimeBase - pClockDataType->sPauseTimeDurtion) + pClockDataType->sRefClockTimeBase;
	return OMX_ErrorNone;
}

static OMX_ERRORTYPE OMX_ClockConfigTimeClockState(OMX_HANDLETYPE hComponent,OMX_TIME_CONFIG_CLOCKSTATETYPE *pClockState)
{
	CLOCK_DATA_TYPE *pClockDataType;
	pClockDataType = (CLOCK_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	if (pClockDataType->sClockState.eState != OMX_TIME_ClockStateStopped) {
		loge("clockState are not in OMX_TIME_ClockStateStopped,do not set!!!\n");
		return OMX_ErrorUndefined;
	}
	memcpy(&pClockDataType->sClockState,pClockState,sizeof(OMX_TIME_CONFIG_CLOCKSTATETYPE));
	logd("nWaitMask:0x%x,sClockState:%d\n",pClockDataType->sClockState.nWaitMask,pClockDataType->sClockState.eState);
	//pClockDataType->sClockState.eState = OMX_TIME_ClockStateWaitingForStartTime;

	return OMX_ErrorNone;
}

static OMX_ERRORTYPE OMX_ClockConfigTimeClientStartTime(OMX_HANDLETYPE hComponent,OMX_TIME_CONFIG_TIMESTAMPTYPE *pTimeStamp)
{
	//int i = 0;
	CLOCK_DATA_TYPE *pClockDataType;
	OMX_TICKS minTimeStamp;
	pClockDataType = (CLOCK_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	OMX_PORT_TUNNELEDINFO *pVideoTunneledInfo = &pClockDataType->sOutPortTunneledInfo[CLOCK_PORT_OUT_VIDEO];
	OMX_PORT_TUNNELEDINFO *pAudioTunneledInfo = &pClockDataType->sOutPortTunneledInfo[CLOCK_PORT_OUT_AUDIO];

	if (pClockDataType->sClockState.eState != OMX_TIME_ClockStateWaitingForStartTime) {
		logw("clockState are not in OMX_TIME_ClockStateWaitingForStartTime,do not set!!!\n");
		return OMX_ErrorUndefined;
	}

	if (pClockDataType->sClockState.nWaitMask) {
		//loge("nPortIndex:%d,nTimestamp:%ld\n",pTimeStamp->nPortIndex,pTimeStamp->nTimestamp);
		if (pTimeStamp->nPortIndex == CLOCK_PORT_OUT_VIDEO) {
			pClockDataType->sClockState.nWaitMask &= ~OMX_CLOCKPORT0;
			pClockDataType->sPortStartTime[CLOCK_PORT_OUT_VIDEO] = pTimeStamp->nTimestamp;
			logd("CLOCK_PORT_OUT_VIDEO nWaitMask:0x%x,nTimestamp:%ld\n"
					,pClockDataType->sClockState.nWaitMask,pTimeStamp->nTimestamp);
		} else if (pTimeStamp->nPortIndex == CLOCK_PORT_OUT_AUDIO) {
			pClockDataType->sClockState.nWaitMask &= ~OMX_CLOCKPORT1;
			pClockDataType->sPortStartTime[CLOCK_PORT_OUT_AUDIO] = pTimeStamp->nTimestamp;
			logd("CLOCK_PORT_OUT_AUDIO nWaitMask:0x%x,nTimestamp:%ld\n"
					,pClockDataType->sClockState.nWaitMask,pTimeStamp->nTimestamp);
		} else {
			return OMX_ErrorBadPortIndex;
		}
	}

	if (!pClockDataType->sClockState.nWaitMask) {//all port start time come
		minTimeStamp = pClockDataType->sPortStartTime[0];

		#if 0
		// for(i = 1; i< CLOCK_PORT_NUM_MAX;i++ ) {
		// 	if (pClockDataType->sPortStartTime[i] < minTimeStamp) {
		// 		minTimeStamp = pClockDataType->sPortStartTime[i];
		// 	}
		// }
		#else
		minTimeStamp = pClockDataType->sPortStartTime[CLOCK_PORT_OUT_AUDIO];
		#endif

		pClockDataType->sClockState.nStartTime = minTimeStamp;
		pClockDataType->sRefClockTimeBase = minTimeStamp;
		pClockDataType->sWallTimeBase = OMX_ClockGetSystemTime();
		pClockDataType->sPauseTimeDurtion = 0;
		pClockDataType->sClockState.eState = OMX_TIME_ClockStateRunning;
		printf("[%s:%d] sRefClockTimeBase:%ld,sWallTimeBase:%ld\n"
				,__FUNCTION__,__LINE__,pClockDataType->sRefClockTimeBase,pClockDataType->sWallTimeBase);
		OMX_SetConfig(pVideoTunneledInfo->pTunneledComp, OMX_IndexConfigTimeClockState,&pClockDataType->sClockState);
		OMX_SetConfig(pAudioTunneledInfo->pTunneledComp, OMX_IndexConfigTimeClockState,&pClockDataType->sClockState);
	}
	return OMX_ErrorNone;
}

static OMX_ERRORTYPE OMX_ClockGetConfig(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_INOUT OMX_PTR pComponentConfigStructure)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	if (pComponentConfigStructure == NULL) {
		loge("param error!!!\n");
		return OMX_ErrorBadParameter;
	}
	switch (nIndex) {
	case OMX_IndexConfigTimeCurrentMediaTime:
		eError = OMX_ClockGetCurrentMediaTime(hComponent,(OMX_TIME_CONFIG_TIMESTAMPTYPE*)pComponentConfigStructure);
		break;
	case OMX_IndexConfigTimeCurrentWallTime:
		//OMX_TIME_CONFIG_TIMESTAMPTYPE
		break;
	case OMX_IndexConfigTimeActiveRefClock:
		//OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE
		break;
	case OMX_IndexConfigTimeClockState:
		break;
	default:
	 break;
	}

	return eError;
}

static OMX_ERRORTYPE OMX_ClockSetConfig(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_IN	OMX_PTR pComponentConfigStructure)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	if (pComponentConfigStructure == NULL) {
		loge("param error!!!\n");
		return OMX_ErrorBadParameter;
	}
	switch (nIndex) {
	case OMX_IndexConfigTimeCurrentAudioReference:
		eError = OMX_ClockConfigTimeCurrentAudioReference(hComponent,(OMX_TIME_CONFIG_TIMESTAMPTYPE*)pComponentConfigStructure);
		break;
	case OMX_IndexConfigTimeCurrentVideoReference:
		break;
	case OMX_IndexConfigTimeMediaTimeRequest:
		break;
	case OMX_IndexConfigTimeClientStartTime:
		eError = OMX_ClockConfigTimeClientStartTime(hComponent,(OMX_TIME_CONFIG_TIMESTAMPTYPE*)pComponentConfigStructure);
		break;
	case OMX_IndexConfigTimePosition:// do seek
		eError = OMX_ClockOMX_IndexConfigTimePosition(hComponent,(OMX_TIME_CONFIG_TIMESTAMPTYPE*)pComponentConfigStructure);
		break;
	case OMX_IndexConfigTimeActiveRefClock:
		break;
	case OMX_IndexConfigTimeClockState:
		eError = OMX_ClockConfigTimeClockState(hComponent,(OMX_TIME_CONFIG_CLOCKSTATETYPE *)pComponentConfigStructure);
		break;
	default:
	 break;
	}
	return eError;
}

static OMX_ERRORTYPE OMX_ClockGetState(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_OUT OMX_STATETYPE* pState)
{
	CLOCK_DATA_TYPE* pClockDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	pClockDataType = (CLOCK_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

	pthread_mutex_lock(&pClockDataType->stateLock);
	*pState = pClockDataType->state;
	pthread_mutex_unlock(&pClockDataType->stateLock);
	return eError;
}

static OMX_ERRORTYPE OMX_ClockComponentTunnelRequest(
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
	CLOCK_DATA_TYPE* pClockDataType;
	pClockDataType = (CLOCK_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComp)->pComponentPrivate);
	if (pClockDataType->state != OMX_StateLoaded)
	 {
		loge("Component is not in OMX_StateLoaded,it is in%d,it can not tunnel\n",pClockDataType->state);
		return OMX_ErrorInvalidState;
	}

	if (nPort == CLOCK_PORT_OUT_VIDEO) {

		pPort = &pClockDataType->sOutPortDef[CLOCK_PORT_OUT_VIDEO];
		pTunneledInfo = &pClockDataType->sOutPortTunneledInfo[CLOCK_PORT_OUT_VIDEO];
		pBufSupplier = &pClockDataType->sOutBufSupplier[CLOCK_PORT_OUT_VIDEO];
	} else if (nPort == CLOCK_PORT_OUT_AUDIO) {
		pPort = &pClockDataType->sOutPortDef[CLOCK_PORT_OUT_AUDIO];
		pTunneledInfo = &pClockDataType->sOutPortTunneledInfo[CLOCK_PORT_OUT_AUDIO];
		pBufSupplier = &pClockDataType->sOutBufSupplier[CLOCK_PORT_OUT_AUDIO];
	} else {
		loge("component can not find \n");
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

static OMX_ERRORTYPE OMX_ClockEmptyThisBuffer(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	return eError;
}

static OMX_ERRORTYPE OMX_ClockFillThisBuffer(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;

	return eError;
}

static OMX_ERRORTYPE OMX_ClockSetCallbacks(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_CALLBACKTYPE* pCallbacks,
		OMX_IN	OMX_PTR pAppData)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	CLOCK_DATA_TYPE* pClockDataType;
	pClockDataType = (CLOCK_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	pClockDataType->pCallbacks = pCallbacks;
	pClockDataType->pAppData = pAppData;
	return eError;
}

OMX_ERRORTYPE OMX_ClockComponentDeInit(
		OMX_IN	OMX_HANDLETYPE hComponent)  {
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_COMPONENTTYPE *pComp;
	CLOCK_DATA_TYPE *pClockDataType;
	pComp = (OMX_COMPONENTTYPE *)hComponent;

	pClockDataType = (CLOCK_DATA_TYPE *)pComp->pComponentPrivate;

	pthread_mutex_lock(&pClockDataType->stateLock);
	if (pClockDataType->state != OMX_StateLoaded) {
		loge("compoent is in %d,but not in OMX_StateLoaded(1),can ont FreeHandle.\n",pClockDataType->state);
		pthread_mutex_unlock(&pClockDataType->stateLock);
		return OMX_ErrorIncorrectStateOperation;
	}
	pthread_mutex_unlock(&pClockDataType->stateLock);

	pthread_mutex_destroy(&pClockDataType->stateLock);

	aic_msg_destroy(&pClockDataType->sMsgQue);

	mpp_free(pClockDataType);
	pClockDataType = NULL;

	logi("OMX_ClockComponentDeInit\n");
	return eError;
}

OMX_ERRORTYPE OMX_ClockComponentInit(
		OMX_IN	OMX_HANDLETYPE hComponent)
{
	OMX_COMPONENTTYPE *pComp;
	CLOCK_DATA_TYPE *pClockDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_U32 i;

	logi("OMX_ClockComponentInit....\n");

	pComp = (OMX_COMPONENTTYPE *)hComponent;

	pClockDataType = (CLOCK_DATA_TYPE *)mpp_alloc(sizeof(CLOCK_DATA_TYPE));

	if (NULL == pClockDataType)  {
		loge("mpp_alloc(sizeof(CLOCK_DATA_TYPE) fail!");
		eError = OMX_ErrorInsufficientResources;
		goto _EXIT1;
	}

	memset(pClockDataType, 0x0, sizeof(CLOCK_DATA_TYPE));
	pComp->pComponentPrivate	= (void*) pClockDataType;
	pClockDataType->state 		= OMX_StateLoaded;
	pClockDataType->hSelf 		= pComp;

	pComp->SetCallbacks 		= OMX_ClockSetCallbacks;
	pComp->SendCommand			= OMX_ClockSendCommand;
	pComp->GetState 			= OMX_ClockGetState;
	pComp->GetParameter 		= OMX_ClockGetParameter;
	pComp->SetParameter 		= OMX_ClockSetParameter;
	pComp->GetConfig			= OMX_ClockGetConfig;
	pComp->SetConfig			= OMX_ClockSetConfig;
	pComp->ComponentTunnelRequest = OMX_ClockComponentTunnelRequest;
	pComp->ComponentDeInit		= OMX_ClockComponentDeInit;
	pComp->FillThisBuffer		= OMX_ClockFillThisBuffer;
	pComp->EmptyThisBuffer		= OMX_ClockEmptyThisBuffer;

	pClockDataType->sPortParam.nPorts = 2;
	pClockDataType->sPortParam.nStartPortNumber = 0x0;

	pClockDataType->sOutPortDef[CLOCK_PORT_OUT_VIDEO].nPortIndex = CLOCK_PORT_OUT_VIDEO;
	pClockDataType->sOutPortDef[CLOCK_PORT_OUT_VIDEO].bPopulated = OMX_TRUE;
	pClockDataType->sOutPortDef[CLOCK_PORT_OUT_VIDEO].bEnabled = OMX_TRUE;
	pClockDataType->sOutPortDef[CLOCK_PORT_OUT_VIDEO].eDomain = OMX_PortDomainOther;
	pClockDataType->sOutPortDef[CLOCK_PORT_OUT_VIDEO].eDir = OMX_DirOutput;
	pClockDataType->sOutBufSupplier[CLOCK_PORT_OUT_VIDEO].nPortIndex = CLOCK_PORT_OUT_VIDEO;
	pClockDataType->sOutBufSupplier[CLOCK_PORT_OUT_VIDEO].eBufferSupplier = OMX_BufferSupplyOutput;

	pClockDataType->sOutPortDef[CLOCK_PORT_OUT_AUDIO].nPortIndex = CLOCK_PORT_OUT_AUDIO;
	pClockDataType->sOutPortDef[CLOCK_PORT_OUT_AUDIO].bPopulated = OMX_TRUE;
	pClockDataType->sOutPortDef[CLOCK_PORT_OUT_AUDIO].bEnabled = OMX_TRUE;
	pClockDataType->sOutPortDef[CLOCK_PORT_OUT_AUDIO].eDomain = OMX_PortDomainOther;
	pClockDataType->sOutPortDef[CLOCK_PORT_OUT_AUDIO].eDir = OMX_DirOutput;
	pClockDataType->sOutBufSupplier[CLOCK_PORT_OUT_AUDIO].nPortIndex = CLOCK_PORT_OUT_AUDIO;
	pClockDataType->sOutBufSupplier[CLOCK_PORT_OUT_AUDIO].eBufferSupplier = OMX_BufferSupplyOutput;

	pClockDataType->sClockState.eState = OMX_TIME_ClockStateStopped;
	pClockDataType->sClockState.nStartTime = -1;
	for(i=0; i<CLOCK_PORT_NUM_MAX; i++) {
		pClockDataType->sPortStartTime[i] = -1;
	}
	pClockDataType->sActiveRefClock.eClock = OMX_TIME_RefClockAudio;

	pClockDataType->sPauseTimeDurtion = 0;

	if (aic_msg_create(&pClockDataType->sMsgQue)<0)
	 {
		loge("aic_msg_create fail!");
		eError = OMX_ErrorInsufficientResources;
		goto _EXIT2;
	}

	pthread_mutex_init(&pClockDataType->stateLock, NULL);

	return eError;

_EXIT2:
	if (pClockDataType) {
		mpp_free(pClockDataType);
		pClockDataType = NULL;
	}

_EXIT1:
	return eError;
}

static void OMX_ClockEventNotify(
		CLOCK_DATA_TYPE * pClockDataType,
		OMX_EVENTTYPE event,
		OMX_U32 nData1,
		OMX_U32 nData2,
		OMX_PTR pEventData)
{
	if (pClockDataType && pClockDataType->pCallbacks && pClockDataType->pCallbacks->EventHandler)  {
		pClockDataType->pCallbacks->EventHandler(
					pClockDataType->hSelf,
					pClockDataType->pAppData,event,
					nData1, nData2, pEventData);
	}
}

static void OMX_ClockStateChangeToInvalid(CLOCK_DATA_TYPE * pClockDataType)
{
	pClockDataType->state = OMX_StateInvalid;
	OMX_ClockEventNotify(pClockDataType
						,OMX_EventError
						,OMX_ErrorInvalidState,0,NULL);
	OMX_ClockEventNotify(pClockDataType
						,OMX_EventCmdComplete
						,OMX_CommandStateSet
						,pClockDataType->state,NULL);

}
static void OMX_ClockStateChangeToLoaded(CLOCK_DATA_TYPE * pClockDataType)
{
	if (pClockDataType->state == OMX_StateIdle) {

	} else if (pClockDataType->state == OMX_StateExecuting) {

	} else if (pClockDataType->state == OMX_StatePause) {

	} else  {
		OMX_ClockEventNotify(pClockDataType
							,OMX_EventError
							,OMX_ErrorIncorrectStateTransition
							, pClockDataType->state,NULL);
		loge("OMX_ErrorIncorrectStateTransition\n");
		return;
	}
	pClockDataType->state = OMX_StateLoaded;
	OMX_ClockEventNotify(pClockDataType
						,OMX_EventCmdComplete
						,OMX_CommandStateSet
						, pClockDataType->state,NULL);
}
static void OMXClockStateChangeToIdle(CLOCK_DATA_TYPE * pClockDataType)
{
	if (pClockDataType->state == OMX_StateLoaded) {

	} else if (pClockDataType->state == OMX_StatePause) {

	} else if (pClockDataType->state == OMX_StateExecuting) {

	} else {
		OMX_ClockEventNotify(pClockDataType
							,OMX_EventError
							,OMX_ErrorIncorrectStateTransition
							, pClockDataType->state,NULL);
		loge("OMX_ErrorIncorrectStateTransition\n");
		return;
	}
	pClockDataType->state = OMX_StateIdle;
	OMX_ClockEventNotify(pClockDataType
						,OMX_EventCmdComplete
						,OMX_CommandStateSet
						, pClockDataType->state,NULL);

}
static void OMX_ClockStateChangeToExcuting(CLOCK_DATA_TYPE * pClockDataType)
{

	if (pClockDataType->state == OMX_StateLoaded) {
		OMX_ClockEventNotify(pClockDataType
							,OMX_EventError
							,OMX_ErrorIncorrectStateTransition
							, pClockDataType->state,NULL);
		loge("OMX_ErrorIncorrectStateTransition\n");
		return;
	} else if (pClockDataType->state == OMX_StateIdle) {

	} else if (pClockDataType->state == OMX_StatePause) {
		//OMX_S64 nCurMeidaTime;
		pClockDataType->sPauseTimeDurtion += (OMX_ClockGetSystemTime() - pClockDataType->sPauseTimePoint);
		// loge("OMX_ClockGetSystemTime:%ld,sPauseTimePoint:%ld,sPauseTimeDurtion:%ld,sWallTimeBase:%ld,sRefClockTimeBase:%ld\n"
		// 	,OMX_ClockGetSystemTime()
		// 	,pClockDataType->sPauseTimePoint
		// 	,pClockDataType->sPauseTimeDurtion
		// 	,pClockDataType->sWallTimeBase
		// 	,pClockDataType->sRefClockTimeBase);

		//nCurMeidaTime = (OMX_ClockGetSystemTime() - pClockDataType->sWallTimeBase - pClockDataType->sPauseTimeDurtion) + pClockDataType->sRefClockTimeBase;
		//loge("pClockDataType->sPauseTimeDurtion:%ld,nCurMeidaTime:%ld\n",pClockDataType->sPauseTimeDurtion,nCurMeidaTime);
	} else {
		OMX_ClockEventNotify(pClockDataType
							,OMX_EventError
							,OMX_ErrorIncorrectStateTransition
							, pClockDataType->state,NULL);
		loge("OMX_ErrorIncorrectStateTransition\n");
		return;
	}
	pClockDataType->state = OMX_StateExecuting;
	OMX_ClockEventNotify(pClockDataType
						,OMX_EventCmdComplete
						,OMX_CommandStateSet
						, pClockDataType->state,NULL);

}
static void OMX_ClockStateChangeToPause(CLOCK_DATA_TYPE * pClockDataType)
{
	if (pClockDataType->state == OMX_StateLoaded) {
		OMX_ClockEventNotify(pClockDataType
							,OMX_EventError
							,OMX_ErrorIncorrectStateTransition
							, pClockDataType->state,NULL);
		loge("OMX_ErrorIncorrectStateTransition\n");
		return;

	} else if (pClockDataType->state == OMX_StateIdle) {

	} else if (pClockDataType->state == OMX_StateExecuting) {
		//OMX_S64 nCurMeidaTime;
		// loge("OMX_ClockGetSystemTime:%ld,sPauseTimePoint:%ld,sPauseTimeDurtion:%ld,sWallTimeBase:%ld,sRefClockTimeBase:%ld\n"
		// 	,OMX_ClockGetSystemTime()
		// 	,pClockDataType->sPauseTimePoint
		// 	,pClockDataType->sPauseTimeDurtion
		// 	,pClockDataType->sWallTimeBase
		// 	,pClockDataType->sRefClockTimeBase);
		//nCurMeidaTime = (OMX_ClockGetSystemTime() - pClockDataType->sWallTimeBase - pClockDataType->sPauseTimeDurtion) + pClockDataType->sRefClockTimeBase;
		// loge("OMX_ClockGetSystemTime:%ld,sPauseTimePoint:%ld,sPauseTimeDurtion:%ld,sWallTimeBase:%ld,sRefClockTimeBase:%ld,nCurMeidaTime:%ld\n"
		// 	,OMX_ClockGetSystemTime()
		// 	,pClockDataType->sPauseTimePoint
		// 	,pClockDataType->sPauseTimeDurtion
		// 	,pClockDataType->sWallTimeBase
		// 	,pClockDataType->sRefClockTimeBase
		// 	,nCurMeidaTime);

		pClockDataType->sPauseTimePoint = OMX_ClockGetSystemTime();
	} else {
		OMX_ClockEventNotify(pClockDataType
							,OMX_EventError
							,OMX_ErrorIncorrectStateTransition
							, pClockDataType->state,NULL);
		loge("OMX_ErrorIncorrectStateTransition\n");
		return;

	}
	pClockDataType->state = OMX_StatePause;
	OMX_ClockEventNotify(pClockDataType
						,OMX_EventCmdComplete
						,OMX_CommandStateSet
						, pClockDataType->state,NULL);

}

/*
	there is on need to create a pthread to run this component.
	processing cmd directly in OMX_ClockSendCommand.
*/
static OMX_ERRORTYPE OMX_ClockSendCommand(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_COMMANDTYPE Cmd,
		OMX_IN	OMX_U32 nParam1,
		OMX_IN	OMX_PTR pCmdData)
{
	CLOCK_DATA_TYPE *pClockDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	pClockDataType = (CLOCK_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

	if (OMX_CommandStateSet == Cmd) {
		pthread_mutex_lock(&pClockDataType->stateLock);
		if (pClockDataType->state == (OMX_STATETYPE)(nParam1)) {
			logi("OMX_ErrorSameState\n");
			OMX_ClockEventNotify(pClockDataType
				,OMX_EventError
				,OMX_ErrorSameState,0,NULL);
			pthread_mutex_unlock(&pClockDataType->stateLock);
			return OMX_ErrorSameState;
		}
		switch(nParam1) {
		case OMX_StateInvalid:
			OMX_ClockStateChangeToInvalid(pClockDataType);
			break;
		case OMX_StateLoaded:
			OMX_ClockStateChangeToLoaded(pClockDataType);
			break;
		case OMX_StateIdle:
			OMXClockStateChangeToIdle(pClockDataType);
			break;
		case OMX_StateExecuting:
			OMX_ClockStateChangeToExcuting(pClockDataType);
			break;
		case OMX_StatePause:
			OMX_ClockStateChangeToPause(pClockDataType);
			break;
		default:
			break;
		}
		pthread_mutex_unlock(&pClockDataType->stateLock);
	} else if (OMX_CommandFlush == Cmd) {

	} else if (OMX_CommandPortDisable == Cmd) {

	} else if (OMX_CommandPortEnable == Cmd) {

	} else if (OMX_CommandMarkBuffer == (OMX_S32)Cmd) {

	} else if (OMX_CommandStop == (OMX_S32)Cmd) {
	} else {

	}

	return eError;

}

