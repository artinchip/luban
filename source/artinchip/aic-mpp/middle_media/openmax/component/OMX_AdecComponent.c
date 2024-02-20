/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: OMX_AdecComponent
*/

#include "OMX_AdecComponent.h"

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

#define OMX_AdecListEmpty(list,mutex)\
({\
	int ret = 0;\
	aic_pthread_mutex_lock(&mutex);\
	ret = mpp_list_empty(list);\
	aic_pthread_mutex_unlock(&mutex);\
	(ret);\
})

#define OMX_ADEC_PRINT_FRAME_NUM (30)

static OMX_ERRORTYPE OMX_AdecSendCommand(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_COMMANDTYPE Cmd,
		OMX_IN	OMX_U32 nParam1,
		OMX_IN	OMX_PTR pCmdData);

static OMX_ERRORTYPE OMX_AdecGetParameter(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nParamIndex,
		OMX_INOUT OMX_PTR pComponentParameterStructure);

static OMX_ERRORTYPE OMX_AdecSetParameter(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_IN	OMX_PTR pComponentParameterStructure);

static OMX_ERRORTYPE OMX_AdecGetConfig(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_INOUT OMX_PTR pComponentConfigStructure);

static OMX_ERRORTYPE OMX_AdecSetConfig(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_IN	OMX_PTR pComponentConfigStructure);

static OMX_ERRORTYPE OMX_AdecGetState(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_OUT OMX_STATETYPE* pState);

static OMX_ERRORTYPE OMX_AdecComponentTunnelRequest(
	OMX_IN	OMX_HANDLETYPE hComp,
	OMX_IN	OMX_U32 nPort,
	OMX_IN	OMX_HANDLETYPE hTunneledComp,
	OMX_IN	OMX_U32 nTunneledPort,
	OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup);

static OMX_ERRORTYPE OMX_AdecEmptyThisBuffer(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_BUFFERHEADERTYPE* pBuffer);

static OMX_ERRORTYPE OMX_AdecFillThisBuffer(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_BUFFERHEADERTYPE* pBuffer);

static OMX_ERRORTYPE OMX_AdecSetCallbacks(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_CALLBACKTYPE* pCallbacks,
		OMX_IN	OMX_PTR pAppData);

static int OMX_AdecGiveBackAllPackets(ADEC_DATA_TYPE *pAdecDataType);
static int OMX_AdecGiveBackAllFramesToDecoder(ADEC_DATA_TYPE *pAdecDataType);

static OMX_ERRORTYPE OMX_AdecSendCommand(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_COMMANDTYPE Cmd,
		OMX_IN	OMX_U32 nParam1,
		OMX_IN	OMX_PTR pCmdData)
{
	ADEC_DATA_TYPE *pAdecDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	struct aic_message sMsg;
	pAdecDataType = (ADEC_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	sMsg.message_id = Cmd;
	sMsg.param = nParam1;
	sMsg.data_size = 0;

	//now not use always NULL
	if (pCmdData != NULL) {
		sMsg.data = pCmdData;
		sMsg.data_size = strlen((char*)pCmdData);
	}

	aic_msg_put(&pAdecDataType->sMsgQue, &sMsg);
	return eError;
}

static void* OMX_AdecComponentThread(void* pThreadData);

static OMX_ERRORTYPE OMX_AdecGetParameter(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nParamIndex,
		OMX_INOUT OMX_PTR pComponentParameterStructure)
{
	ADEC_DATA_TYPE *pAdecDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;

	pAdecDataType = (ADEC_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

	switch (nParamIndex) {
		case OMX_IndexParamPortDefinition: {
			OMX_PARAM_PORTDEFINITIONTYPE *port = (OMX_PARAM_PORTDEFINITIONTYPE*)pComponentParameterStructure;
			if (port->nPortIndex == ADEC_PORT_IN_INDEX) {
				memcpy(port,&pAdecDataType->sInPortDef,sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
			} else if (port->nPortIndex == ADEC_PORT_OUT_INDEX) {
				memcpy(port,&pAdecDataType->sOutPortDef,sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
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
				sBufferSupplier->eBufferSupplier = pAdecDataType->sInBufSupplier.eBufferSupplier;
			} else if (sBufferSupplier->nPortIndex == 1) {
				sBufferSupplier->eBufferSupplier = pAdecDataType->sOutBufSupplier.eBufferSupplier;
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

static OMX_S32 OMX_AdecAudioFormatTrans(enum aic_audio_codec_type *eDesType,OMX_AUDIO_CODINGTYPE *eSrcType)
{
	OMX_S32 ret = 0;
	if (eDesType== NULL || eSrcType == NULL) {
		loge("bad params!!!!\n");
		return -1;
	}
	if (*eSrcType == OMX_AUDIO_CodingMP3) {
		*eDesType = MPP_CODEC_AUDIO_DECODER_MP3;
	#ifdef AAC_DECODER
	} else if (*eSrcType == OMX_AUDIO_CodingAAC) {
		*eDesType = MPP_CODEC_AUDIO_DECODER_AAC;
	#endif
	} else {
		loge("unsupport codec!!!!\n");
		ret = -1;
	}
	return ret;
}

static OMX_ERRORTYPE OMX_AdecSetParameter(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nParamIndex,
		OMX_IN	OMX_PTR pComponentParameterStructure)
{
	ADEC_DATA_TYPE *pAdecDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_U32 index;
	OMX_S32 nIndex = (OMX_S32)nParamIndex;
	enum  aic_audio_codec_type eCodecType;
	OMX_PARAM_FRAMEEND *sFrameEnd;
	pAdecDataType = (ADEC_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	switch (nIndex) {
		case OMX_IndexParamAudioPortFormat: {
			OMX_AUDIO_PARAM_PORTFORMATTYPE *sPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)pComponentParameterStructure;
			index = sPortFormat->nPortIndex;
			if (index == ADEC_PORT_IN_INDEX) {
				pAdecDataType->sInPortDef.format.audio.eEncoding = sPortFormat->eEncoding;
				logw("eEncoding:%d\n",pAdecDataType->sInPortDef.format.audio.eEncoding);
				if (OMX_AdecAudioFormatTrans(&eCodecType,&sPortFormat->eEncoding) != 0) {
					eError = OMX_ErrorUnsupportedSetting;
					loge("OMX_ErrorUnsupportedSetting\n");
					break;
				}
				pAdecDataType->eCodeType = eCodecType;
				logw("eCodeType:%d\n",pAdecDataType->eCodeType);
				/*need to define extened OMX_Index or decide by inner*/
				pAdecDataType->sDecoderConfig.packet_buffer_size = 16*1024;
				pAdecDataType->sDecoderConfig.packet_count = 8;
				pAdecDataType->sDecoderConfig.frame_count = 16;

			} else if (index == ADEC_PORT_OUT_INDEX) {
				logw("now no need to set out port param\n");
			} else {
				loge("OMX_ErrorBadParameter\n");
			}
			break;
		}
		case OMX_IndexParamPortDefinition: {
			OMX_PARAM_PORTDEFINITIONTYPE *port = (OMX_PARAM_PORTDEFINITIONTYPE*)pComponentParameterStructure;
			index = port->nPortIndex;
			if (index == ADEC_PORT_IN_INDEX) {
				pAdecDataType->sInPortDef.format.audio.eEncoding = port->format.audio.eEncoding;
				logw("eEncoding:%d\n",pAdecDataType->sInPortDef.format.audio.eEncoding);
				if (OMX_AdecAudioFormatTrans(&eCodecType,&port->format.audio.eEncoding) != 0) {
					eError = OMX_ErrorUnsupportedSetting;
					loge("OMX_ErrorUnsupportedSetting\n");
					break;
				}

				/*need to convert */
				pAdecDataType->eCodeType = eCodecType;
				logw("eCompressionFormat:%d\n",pAdecDataType->eCodeType);
				/*need to define extened OMX_Index or decide by inner*/
				pAdecDataType->sDecoderConfig.packet_buffer_size = 16*1024;
				pAdecDataType->sDecoderConfig.packet_count = 8;
				pAdecDataType->sDecoderConfig.frame_count = 16;

			} else if (index == ADEC_PORT_OUT_INDEX) {
					logw("now no need to set out port param\n");
			} else {
				loge("OMX_ErrorBadParameter\n");
				eError = OMX_ErrorBadParameter;
			}
		}
			break;
		case OMX_IndexParamVideoAvc:
			break;
		case OMX_IndexParamVideoProfileLevelQuerySupported:
			break;
		case OMX_IndexParamVideoProfileLevelCurrent:
			break;
		case OMX_IndexParamVideoMpeg4:
			break;
		case OMX_IndexVendorStreamFrameEnd:
			sFrameEnd = (OMX_PARAM_FRAMEEND*)pComponentParameterStructure;
			if (sFrameEnd->bFrameEnd == OMX_TRUE) {
				pAdecDataType->nStreamEndFlag = OMX_TRUE;
				logi("setup nStreamEndFlag\n");
			} else {
				pAdecDataType->nStreamEndFlag = OMX_FALSE;
				pAdecDataType->nDecodeEndFlag = OMX_FALSE;
				pAdecDataType->nFrameEndFlag = OMX_FALSE;
				logi("cancel nStreamEndFlag\n");
			}
			break;
		default:
		 break;
	}
	return eError;
}

static OMX_ERRORTYPE OMX_AdecGetConfig(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_INOUT OMX_PTR pComponentConfigStructure)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;

	return eError;

}

static OMX_ERRORTYPE OMX_AdecSetConfig(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_IN	OMX_PTR pComponentConfigStructure)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_S32 nParamIndex = (OMX_S32)nIndex;
	ADEC_DATA_TYPE* pAdecDataType = (ADEC_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	switch (nParamIndex) {
	case OMX_IndexConfigTimePosition:
		// 1 clear input buffer
		OMX_AdecGiveBackAllPackets(pAdecDataType);
		// 2 clear output buffer
		OMX_AdecGiveBackAllFramesToDecoder(pAdecDataType);
		// 3 clear flag
		pAdecDataType->nFlags = 0;
		pAdecDataType->nPutPacktToDecoderOkNum = 0;
		pAdecDataType->nGetFrameFromDecoderNum = 0;
		// 4 clear decoder buff
		aic_audio_decoder_reset(pAdecDataType->pDecoder);
		break;
	case OMX_IndexConfigTimeSeekMode:
		break;
	default:
		break;
	}
	return eError;

}

static OMX_ERRORTYPE OMX_AdecGetState(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_OUT OMX_STATETYPE* pState)
{
	ADEC_DATA_TYPE* pAdecDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	pAdecDataType = (ADEC_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

	aic_pthread_mutex_lock(&pAdecDataType->stateLock);
	*pState = pAdecDataType->state;
	aic_pthread_mutex_unlock(&pAdecDataType->stateLock);

	return eError;

}

static OMX_ERRORTYPE OMX_AdecComponentTunnelRequest(
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
	ADEC_DATA_TYPE* pAdecDataType;
	pAdecDataType = (ADEC_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComp)->pComponentPrivate);
	if (pAdecDataType->state != OMX_StateLoaded)
	 {
		loge("Component is not in OMX_StateLoaded,it is in%d,it can not tunnel\n",pAdecDataType->state);
		return OMX_ErrorInvalidState;
	}
	if (nPort == ADEC_PORT_IN_INDEX) {
		pPort = &pAdecDataType->sInPortDef;
		pTunneledInfo = &pAdecDataType->sInPortTunneledInfo;
		pBufSupplier = &pAdecDataType->sInBufSupplier;
	} else if (nPort == ADEC_PORT_OUT_INDEX) {
		pPort = &pAdecDataType->sOutPortDef;
		pTunneledInfo = &pAdecDataType->sOutPortTunneledInfo;
		pBufSupplier = &pAdecDataType->sOutBufSupplier;
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

static OMX_ERRORTYPE OMX_AdecEmptyThisBuffer(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	ADEC_DATA_TYPE* pAdecDataType;
	ADEC_IN_PACKET *pktNode;
	int ret = 0;
	struct aic_message sMsg;
	static int rate = 0;
	static struct timespec pev = {0,0} ,cur = {0,0};
	pAdecDataType = (ADEC_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

	aic_pthread_mutex_lock(&pAdecDataType->stateLock);
	if (pAdecDataType->state != OMX_StateExecuting) {
		logw("component is not in OMX_StateExecuting,it is in [%d]!!!\n",pAdecDataType->state);
		aic_pthread_mutex_unlock(&pAdecDataType->stateLock);
		return OMX_ErrorIncorrectStateOperation;
	}
	//aic_pthread_mutex_unlock(&pAdecDataType->stateLock);

	if (pAdecDataType->sInPortTunneledInfo.nTunneledFlag) {
		if (pAdecDataType->sInBufSupplier.eBufferSupplier == OMX_BufferSupplyOutput) {
			if (OMX_AdecListEmpty(&pAdecDataType->sInEmptyPkt,pAdecDataType->sInPktLock)) {
				if (pAdecDataType->nInPktNodeNum + 1> ADEC_PACKET_NUM_MAX) {
					loge("empty node has aready increase to max [%d]!!!\n",pAdecDataType->nInPktNodeNum);
					eError = OMX_ErrorInsufficientResources;
					pAdecDataType->nReceivePacktFailNum++;
					//return  eError;
					goto _EXIT;
				} else {
					int i;
					logw("no empty node,need to extend!!!\n");
					for(i =0 ; i < ADEC_PACKET_ONE_TIME_CREATE_NUM; i++) {
						ADEC_IN_PACKET *pPktNode = (ADEC_IN_PACKET*)mpp_alloc(sizeof(ADEC_IN_PACKET));
						if (NULL == pPktNode) {
							break;
						}
						memset(pPktNode,0x00,sizeof(ADEC_IN_PACKET));
						aic_pthread_mutex_lock(&pAdecDataType->sInPktLock);
						mpp_list_add_tail(&pPktNode->sList, &pAdecDataType->sInEmptyPkt);
						aic_pthread_mutex_unlock(&pAdecDataType->sInPktLock);
						pAdecDataType->nInPktNodeNum++;
					}
					if (i == 0) {
						loge("mpp_alloc empty video node fail\n");
						eError = OMX_ErrorInsufficientResources;
						pAdecDataType->nReceivePacktFailNum++;
						//return  eError;
						goto _EXIT;
					}
				}
			}

			struct mpp_packet pkt;
			pkt.size = pBuffer->nFilledLen;
			ret = aic_audio_decoder_get_packet(pAdecDataType->pDecoder, &pkt, pkt.size);
			if (ret != 0) {
				//loge("get pkt from decoder error ret:%d!!!\n",ret);
				if (ret == DEC_NO_EMPTY_PACKET) {
					pAdecDataType->nReceivePacktFailNum++;
					//return  OMX_ErrorOverflow;
					eError = OMX_ErrorOverflow;
					goto _EXIT;
				} else {
					pAdecDataType->nReceivePacktFailNum++;
					//return OMX_ErrorInsufficientResources;
					eError = OMX_ErrorInsufficientResources;
					goto _EXIT;
				}
			}
			memcpy(pkt.data,pBuffer->pBuffer,pkt.size);
			pkt.flag =pBuffer->nFlags;
			pkt.pts = pBuffer->nTimeStamp;
			//mpp_decoder_get_packet is ok then mpp_decoder_put_packet is also ok
			aic_audio_decoder_put_packet(pAdecDataType->pDecoder, &pkt);

			rate += pkt.size;
			if (pev.tv_sec == 0) {
				clock_gettime(CLOCK_REALTIME,&pev);
			} else {
				long diff;
				clock_gettime(CLOCK_REALTIME,&cur);
				diff = (cur.tv_sec - pev.tv_sec)*1000*1000 + (cur.tv_nsec - pev.tv_nsec)/1000;
				if (diff > 1*1000*1000) {
					logi(" vbv rate:%d,diff:%ld \n",rate*8,diff);
					rate = 0;
					pev = cur;
				}
			}

			aic_pthread_mutex_lock(&pAdecDataType->sInPktLock);
			pktNode = mpp_list_first_entry(&pAdecDataType->sInEmptyPkt, ADEC_IN_PACKET, sList);
			pktNode->sBuff.nOutputPortIndex = pBuffer->nOutputPortIndex;
			pktNode->sBuff.pBuffer = pBuffer->pBuffer;
			pktNode->sBuff.nFilledLen = pBuffer->nFilledLen;
			pktNode->sBuff.nTimeStamp = pBuffer->nTimeStamp;
			pktNode->sBuff.nFlags = pBuffer->nFlags;
			mpp_list_del(&pktNode->sList);
			mpp_list_add_tail(&pktNode->sList, &pAdecDataType->sInProcessedPkt);
			if (pAdecDataType->nWaitForReadyPkt == 1) {
				sMsg.message_id = OMX_CommandNops;
				sMsg.data_size = 0;
				aic_msg_put(&pAdecDataType->sMsgQue, &sMsg);
				pAdecDataType->nWaitForReadyPkt = 0;
			}
			if (pktNode->sBuff.nFlags & PACKET_FLAG_EOS) {
				pAdecDataType->nFlags |= ADEC_INPORT_STREAM_END_FLAG;
				//pVdecDataType->nStreamEndFlag = OMX_TRUE;
					printf("[%s:%d]:StreamEndFlag\n",__FUNCTION__,__LINE__);
			}
			pAdecDataType->nReceivePacktOkNum++;
			logd("pAdecDataType->nReceivePacktOkNum:%d\n",pAdecDataType->nReceivePacktOkNum);
			aic_pthread_mutex_unlock(&pAdecDataType->sInPktLock);
		} else if (pAdecDataType->sInBufSupplier.eBufferSupplier == OMX_BufferSupplyInput) {
			eError = OMX_ErrorNotImplemented;
			logw("OMX_ErrorNotImplemented\n");
		} else {
			eError = OMX_ErrorNotImplemented;
			logw("OMX_ErrorNotImplemented\n");
		}
	} else {
		struct mpp_packet pkt;
		pkt.size = pBuffer->nFilledLen;
		ret = aic_audio_decoder_get_packet(pAdecDataType->pDecoder, &pkt, pkt.size);
		if (ret != 0) {
			//loge("get pkt from decoder error ret:%d!!!\n",ret);
			if (ret == DEC_NO_EMPTY_PACKET) {
				pAdecDataType->nReceivePacktFailNum++;
				//return  OMX_ErrorOverflow;
				eError = OMX_ErrorOverflow;
				goto _EXIT;
			} else {
				pAdecDataType->nReceivePacktFailNum++;
				//return OMX_ErrorInsufficientResources;
				eError = OMX_ErrorInsufficientResources;
				goto _EXIT;
			}
		} else {
			//logw("get pkt from decoder ok!!!\n");
		}
		memcpy(pkt.data,pBuffer->pBuffer,pkt.size);
		pkt.flag =pBuffer->nFlags;
		pkt.pts = pBuffer->nTimeStamp;
		//mpp_decoder_get_packet is ok then mpp_decoder_put_packet is also ok
		aic_audio_decoder_put_packet(pAdecDataType->pDecoder, &pkt);
		if (pkt.flag & PACKET_FLAG_EOS) {
		pAdecDataType->nFlags |= ADEC_INPORT_STREAM_END_FLAG;
		//pAdecDataType->nStreamEndFlag = OMX_TRUE;
			logi("StreamEndFlag!!!\n");
		}
		pAdecDataType->nReceivePacktOkNum++;
		pAdecDataType->nGiveBackPacktOkNum++;
	}

_EXIT:
	aic_pthread_mutex_unlock(&pAdecDataType->stateLock);
	return eError;
}

static OMX_ERRORTYPE OMX_AdecFillThisBuffer(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	ADEC_DATA_TYPE* pAdecDataType;
	struct aic_audio_frame *pFrame;
	ADEC_OUT_FRAME *pFrameNode1 = NULL,*pFrameNode2 = NULL;
	OMX_BOOL bMatch = OMX_FALSE;
	OMX_S32 ret;
	pAdecDataType = (ADEC_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	if (pBuffer->nOutputPortIndex != ADEC_PORT_OUT_INDEX) {
		loge("port not match\n");
		return OMX_ErrorBadParameter;
	}

	if (!OMX_AdecListEmpty(&pAdecDataType->sOutProcessingFrame,pAdecDataType->sOutFrameLock)) {
		pFrame = (struct aic_audio_frame *)pBuffer->pBuffer;
		aic_pthread_mutex_lock(&pAdecDataType->sOutFrameLock);
		mpp_list_for_each_entry_safe(pFrameNode1,pFrameNode2,&pAdecDataType->sOutProcessingFrame,sList) {
			if (pFrameNode1->sFrameInfo.data == pFrame->data) {
				bMatch = OMX_TRUE;
				break;
			}
		}
		if (bMatch) {//give frame back to decoder
			struct aic_message sMsg;
			ret = aic_audio_decoder_put_frame(pAdecDataType->pDecoder, &pFrameNode1->sFrameInfo);
			if (ret != 0) {// how to do
				loge("mpp_decoder_put_frame error!!!!\n");
			}
			// now, no matter whether is back to decoder ok,move pFrameNode1 to sOutEmptyFrame
			pAdecDataType->nSendBackFrameOkNum++;
			mpp_list_del(&pFrameNode1->sList);
			mpp_list_add_tail(&pFrameNode1->sList, &pAdecDataType->sOutEmptyFrame);
			if (pAdecDataType->nWaitForEmptyFrame ==1) {
				sMsg.message_id = OMX_CommandNops;
				sMsg.data_size = 0;
				aic_msg_put(&pAdecDataType->sMsgQue, &sMsg);
				pAdecDataType->nWaitForEmptyFrame = 0;
			}
			logd("pAdecDataType->nReceivePacktOkNum:%d\n",pAdecDataType->nReceivePacktOkNum);
		} else {
			pAdecDataType->nSendBackFrameErrorNum++;
			loge("frame not match!!!\n");
			eError =  OMX_ErrorBadParameter;
		}
		logi("pAdecDataType->nSendBackFrameOkNum:%d,pAdecDataType->nSendBackFrameErrorNum:%d\n"
			,pAdecDataType->nSendBackFrameOkNum
			,pAdecDataType->nSendBackFrameErrorNum);
		aic_pthread_mutex_unlock(&pAdecDataType->sOutFrameLock);
	} else {
		logw("no frame need to back \n");
		eError =  OMX_ErrorBadParameter;
	}
	return eError;
}

static OMX_ERRORTYPE OMX_AdecSetCallbacks(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_CALLBACKTYPE* pCallbacks,
		OMX_IN	OMX_PTR pAppData)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	ADEC_DATA_TYPE* pAdecDataType;
	pAdecDataType = (ADEC_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	pAdecDataType->pCallbacks = pCallbacks;
	pAdecDataType->pAppData = pAppData;
	return eError;
}

OMX_ERRORTYPE OMX_AdecComponentDeInit(
		OMX_IN	OMX_HANDLETYPE hComponent)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_COMPONENTTYPE *pComp;
	ADEC_DATA_TYPE *pAdecDataType;
	ADEC_IN_PACKET *pPktNode = NULL,*pPktNode1 = NULL;
	ADEC_OUT_FRAME *pFrameNode = NULL,*pFrameNode1 = NULL;
	pComp = (OMX_COMPONENTTYPE *)hComponent;
	struct aic_message sMsg;
	pAdecDataType = (ADEC_DATA_TYPE *)pComp->pComponentPrivate;

	aic_pthread_mutex_lock(&pAdecDataType->stateLock);
	if (pAdecDataType->state != OMX_StateLoaded) {
		loge("compoent is in %d,but not in OMX_StateLoaded(1),can not FreeHandle.\n",pAdecDataType->state);
		aic_pthread_mutex_unlock(&pAdecDataType->stateLock);
		return OMX_ErrorIncorrectStateOperation;
	}
	aic_pthread_mutex_unlock(&pAdecDataType->stateLock);

	sMsg.message_id = OMX_CommandStop;
	sMsg.data_size = 0;
	aic_msg_put(&pAdecDataType->sMsgQue, &sMsg);
	pthread_join(pAdecDataType->threadId, (void*)&eError);

	aic_pthread_mutex_lock(&pAdecDataType->sInPktLock);
	if (!mpp_list_empty(&pAdecDataType->sInEmptyPkt)) {
			mpp_list_for_each_entry_safe(pPktNode, pPktNode1, &pAdecDataType->sInEmptyPkt, sList) {
				mpp_list_del(&pPktNode->sList);
				mpp_free(pPktNode);
		}
	}

	if (!mpp_list_empty(&pAdecDataType->sInReadyPkt)) {
			mpp_list_for_each_entry_safe(pPktNode, pPktNode1, &pAdecDataType->sInReadyPkt, sList) {
				mpp_list_del(&pPktNode->sList);
				mpp_free(pPktNode);
		}
	}

	aic_pthread_mutex_unlock(&pAdecDataType->sInPktLock);

	aic_pthread_mutex_lock(&pAdecDataType->sOutFrameLock);
	if (!mpp_list_empty(&pAdecDataType->sOutEmptyFrame)) {
			mpp_list_for_each_entry_safe(pFrameNode, pFrameNode1, &pAdecDataType->sOutEmptyFrame, sList) {
				mpp_list_del(&pFrameNode->sList);
				mpp_free(pFrameNode);
		}
	}
	if (!mpp_list_empty(&pAdecDataType->sOutReadyFrame)) {
			mpp_list_for_each_entry_safe(pFrameNode, pFrameNode1, &pAdecDataType->sOutReadyFrame, sList) {
				mpp_list_del(&pFrameNode->sList);
				mpp_free(pFrameNode);
		}
	}

	if (!mpp_list_empty(&pAdecDataType->sOutProcessingFrame)) {
			mpp_list_for_each_entry_safe(pFrameNode, pFrameNode1, &pAdecDataType->sOutProcessingFrame, sList) {
				mpp_list_del(&pFrameNode->sList);
				mpp_free(pFrameNode);
		}
	}
	aic_pthread_mutex_unlock(&pAdecDataType->sOutFrameLock);

	pthread_mutex_destroy(&pAdecDataType->sInPktLock);
	pthread_mutex_destroy(&pAdecDataType->sOutFrameLock);
	pthread_mutex_destroy(&pAdecDataType->stateLock);

	aic_msg_destroy(&pAdecDataType->sMsgQue);

	if (pAdecDataType->pDecoder) {
		aic_audio_decoder_destroy(pAdecDataType->pDecoder);
		pAdecDataType->pDecoder = NULL;
	}

	mpp_free(pAdecDataType);
	pAdecDataType = NULL;

	logi("OMX_VideoRenderComponentDeInit\n");
	return eError;
}

OMX_ERRORTYPE OMX_AdecComponentInit(
		OMX_IN	OMX_HANDLETYPE hComponent)
{
	OMX_COMPONENTTYPE *pComp;
	ADEC_DATA_TYPE *pAdecDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_U32 err;
	OMX_U32 i;

	logi("OMX_AdecComponentInit....\n");

	pComp = (OMX_COMPONENTTYPE *)hComponent;

	pAdecDataType = (ADEC_DATA_TYPE *)mpp_alloc(sizeof(ADEC_DATA_TYPE));

	if (NULL == pAdecDataType) {
		loge("mpp_alloc(sizeof(ADEC_DATA_TYPE) fail!\n");
		eError = OMX_ErrorInsufficientResources;
		goto _EXIT1;
	}

	memset(pAdecDataType, 0x0, sizeof(ADEC_DATA_TYPE));
	pComp->pComponentPrivate	= (void*) pAdecDataType;
	pAdecDataType->state 		= OMX_StateLoaded;
	pAdecDataType->hSelf 		= pComp;

	pComp->SetCallbacks 		= OMX_AdecSetCallbacks;
	pComp->SendCommand			= OMX_AdecSendCommand;
	pComp->GetState 			= OMX_AdecGetState;
	pComp->GetParameter 		= OMX_AdecGetParameter;
	pComp->SetParameter 		= OMX_AdecSetParameter;
	pComp->GetConfig			= OMX_AdecGetConfig;
	pComp->SetConfig			= OMX_AdecSetConfig;
	pComp->ComponentTunnelRequest = OMX_AdecComponentTunnelRequest;
	pComp->ComponentDeInit		= OMX_AdecComponentDeInit;
	pComp->FillThisBuffer		= OMX_AdecFillThisBuffer;
	pComp->EmptyThisBuffer		= OMX_AdecEmptyThisBuffer;

	pAdecDataType->sPortParam.nPorts = 2;
	pAdecDataType->sPortParam.nStartPortNumber = 0x0;

	pAdecDataType->sInPortDef.nPortIndex = ADEC_PORT_IN_INDEX;
	pAdecDataType->sInPortDef.bPopulated = OMX_TRUE;
	pAdecDataType->sInPortDef.bEnabled	= OMX_TRUE;
	pAdecDataType->sInPortDef.eDomain = OMX_PortDomainAudio;
	pAdecDataType->sInPortDef.eDir = OMX_DirInput;

	pAdecDataType->sOutPortDef.nPortIndex = ADEC_PORT_OUT_INDEX;
	pAdecDataType->sOutPortDef.bPopulated = OMX_TRUE;
	pAdecDataType->sOutPortDef.bEnabled = OMX_TRUE;
	pAdecDataType->sOutPortDef.eDomain = OMX_PortDomainAudio;
	pAdecDataType->sOutPortDef.eDir = OMX_DirOutput;

	pAdecDataType->sInPortTunneledInfo.nPortIndex = ADEC_PORT_IN_INDEX;
	pAdecDataType->sInPortTunneledInfo.pSelfComp = hComponent;
	pAdecDataType->sOutPortTunneledInfo.nPortIndex = ADEC_PORT_OUT_INDEX;
	pAdecDataType->sOutPortTunneledInfo.pSelfComp = hComponent;

	//now  demux  support buffers ,later modify
	pAdecDataType->sInBufSupplier.nPortIndex = ADEC_PORT_IN_INDEX;
	pAdecDataType->sInBufSupplier.eBufferSupplier = OMX_BufferSupplyOutput;
	pAdecDataType->nInPktNodeNum = 0;
	mpp_list_init(&pAdecDataType->sInEmptyPkt);
	mpp_list_init(&pAdecDataType->sInReadyPkt);
	mpp_list_init(&pAdecDataType->sInProcessedPkt);
	pthread_mutex_init(&pAdecDataType->sInPktLock, NULL);
	for(i =0 ; i < ADEC_PACKET_ONE_TIME_CREATE_NUM; i++) {
		ADEC_IN_PACKET *pPktNode = (ADEC_IN_PACKET*)mpp_alloc(sizeof(ADEC_IN_PACKET));
		if (NULL == pPktNode) {
			break;
		}
		memset(pPktNode,0x00,sizeof(ADEC_IN_PACKET));
		mpp_list_add_tail(&pPktNode->sList, &pAdecDataType->sInEmptyPkt);
		pAdecDataType->nInPktNodeNum++;
	}
	if (pAdecDataType->nInPktNodeNum == 0) {
		loge("mpp_alloc empty video node fail\n");
		eError = OMX_ErrorInsufficientResources;
		goto _EXIT2;
	}

	// vdec support out pot buffer
	pAdecDataType->sOutBufSupplier.nPortIndex = ADEC_PORT_OUT_INDEX;
	pAdecDataType->sOutBufSupplier.eBufferSupplier = OMX_BufferSupplyOutput;
	pAdecDataType->nOutFrameNodeNum = 0;
	mpp_list_init(&pAdecDataType->sOutEmptyFrame);
	mpp_list_init(&pAdecDataType->sOutReadyFrame);
	mpp_list_init(&pAdecDataType->sOutProcessingFrame);
	pthread_mutex_init(&pAdecDataType->sOutFrameLock, NULL);
	for(i =0 ; i < ADEC_FRAME_ONE_TIME_CREATE_NUM; i++) {
		ADEC_OUT_FRAME *pFrameNode = (ADEC_OUT_FRAME*)mpp_alloc(sizeof(ADEC_OUT_FRAME));
		if (NULL == pFrameNode) {
			break;
		}
		memset(pFrameNode,0x00,sizeof(ADEC_OUT_FRAME));
		mpp_list_add_tail(&pFrameNode->sList, &pAdecDataType->sOutEmptyFrame);
		pAdecDataType->nOutFrameNodeNum++;
	}
	if (pAdecDataType->nOutFrameNodeNum == 0) {
		loge("mpp_alloc empty video node fail\n");
		eError = OMX_ErrorInsufficientResources;
		goto _EXIT3;
	}

	if (aic_msg_create(&pAdecDataType->sMsgQue)<0)
	 {
		loge("aic_msg_create fail!\n");
		eError = OMX_ErrorInsufficientResources;
		goto _EXIT4;
	}

	pthread_mutex_init(&pAdecDataType->stateLock, NULL);
	// Create the component thread
	err = pthread_create(&pAdecDataType->threadId, NULL, OMX_AdecComponentThread, pAdecDataType);
	if (err || !pAdecDataType->threadId)
	 {
		loge("pthread_create fail!\n");
		eError = OMX_ErrorInsufficientResources;
		goto _EXIT5;
	}

	logi("OMX_AdecComponentInit OK \n");
	return eError;

_EXIT5:
	aic_msg_destroy(&pAdecDataType->sMsgQue);
	pthread_mutex_destroy(&pAdecDataType->stateLock);

_EXIT4:
	if (!mpp_list_empty(&pAdecDataType->sInEmptyPkt)) {
		ADEC_IN_PACKET *pPktNode = NULL,*pPktNode1 = NULL;
		mpp_list_for_each_entry_safe(pPktNode, pPktNode1, &pAdecDataType->sInEmptyPkt, sList) {
			mpp_list_del(&pPktNode->sList);
			mpp_free(pPktNode);
		}
	}

_EXIT3:
	if (!mpp_list_empty(&pAdecDataType->sOutEmptyFrame)) {
		ADEC_OUT_FRAME *pFrameNode = NULL,*pFrameNode1 = NULL;
		mpp_list_for_each_entry_safe(pFrameNode, pFrameNode1, &pAdecDataType->sOutEmptyFrame, sList) {
			mpp_list_del(&pFrameNode->sList);
			mpp_free(pFrameNode);
		}
	}

_EXIT2:
	if (pAdecDataType) {
		mpp_free(pAdecDataType);
		pAdecDataType = NULL;
	}

_EXIT1:
	return eError;
}

static int OMX_AdecGiveBackAllPackets(ADEC_DATA_TYPE *pAdecDataType)
{
	int ret = 0;
	//move sInReadyPkt to sInProcessedPkt
	logi("Before OMX_AdecComponentThread exit,move node in sInReadyFrame to sInProcessedFrmae\n");
	if (!OMX_AdecListEmpty(&pAdecDataType->sInReadyPkt,pAdecDataType->sInPktLock)) {
		logi("sInReadyFrame is not empty\n");
		ADEC_IN_PACKET *pktNode1,*pktNode2;
		aic_pthread_mutex_lock(&pAdecDataType->sInPktLock);
		mpp_list_for_each_entry_safe(pktNode1, pktNode2, &pAdecDataType->sInReadyPkt, sList) {
			pAdecDataType->nLeftReadyFrameWhenCompoentExitNum++;
			mpp_list_del(&pktNode1->sList);
			mpp_list_add_tail(&pktNode1->sList, &pAdecDataType->sInProcessedPkt);
		}
		aic_pthread_mutex_unlock(&pAdecDataType->sInPktLock);
	}

	// give back all in port pkts
	logi("Before OMX_AdecComponentThread exit,it must give back all in port pkts\n");
	if (!OMX_AdecListEmpty(&pAdecDataType->sInProcessedPkt,pAdecDataType->sInPktLock)) {
		logi("sInProcessedPkt is not empty\n");
		ADEC_IN_PACKET *pktNode1 = NULL;
		while(!OMX_AdecListEmpty(&pAdecDataType->sInProcessedPkt,pAdecDataType->sInPktLock)) {
			aic_pthread_mutex_lock(&pAdecDataType->sInPktLock);
			pktNode1 = mpp_list_first_entry(&pAdecDataType->sInProcessedPkt, ADEC_IN_PACKET, sList);
			aic_pthread_mutex_unlock(&pAdecDataType->sInPktLock);
			ret = 0;
			if (pAdecDataType->sInPortTunneledInfo.nTunneledFlag) {
				ret = OMX_FillThisBuffer(pAdecDataType->sInPortTunneledInfo.pTunneledComp,&pktNode1->sBuff);
				if (ret != 0) { // how to do ,deal with problem by TunneledComp, do nothing here
					logw("OMX_FillThisBuffer error \n");
				}
			} else {
				if (pAdecDataType->pCallbacks != NULL && pAdecDataType->pCallbacks->EmptyBufferDone!= NULL) {
					ret = pAdecDataType->pCallbacks->EmptyBufferDone(pAdecDataType->hSelf,pAdecDataType->pAppData,&pktNode1->sBuff);
					if (ret != 0) {// how to do ,deal with problem by app, do nothing here
						logw("EmptyBufferDone error \n");
					}
				}
			}
			if (ret == 0) {
				pAdecDataType->nGiveBackPacktOkNum++;
			} else {
				pAdecDataType->nGiveBackPacktFailNum++;
				continue;// must give back ok ,so retry to give back
			}
			aic_pthread_mutex_lock(&pAdecDataType->sInPktLock);
			mpp_list_del(&pktNode1->sList);
			mpp_list_add_tail(&pktNode1->sList, &pAdecDataType->sInEmptyPkt);
			logi("pAdecDataType->nGiveBackPacktOkNum:%d,pAdecDataType->nGiveBackPacktFailNum:%d\n"
				,pAdecDataType->nGiveBackPacktOkNum
				,pAdecDataType->nGiveBackPacktFailNum);
			aic_pthread_mutex_unlock(&pAdecDataType->sInPktLock);
		}
	}
	return 0;
}

static int OMX_AdecGiveBackAllFramesToDecoder(ADEC_DATA_TYPE *pAdecDataType)
{
	int ret = 0;
	//wait for all out port frames from other component or app to back
	logi("Before OMX_AdecComponentThread exit,it must wait for sOutProcessingFrame empty\n");
	while(!OMX_AdecListEmpty(&pAdecDataType->sOutProcessingFrame,pAdecDataType->sOutFrameLock)) {
		usleep(1000);
	}

	//give back all frames in sOutReadyFrame to decoder
	logi("Before OMX_AdecComponentThread exit,it must give back all frames in sOutReadyFrame to decoder\n");

	if (!OMX_AdecListEmpty(&pAdecDataType->sOutReadyFrame,pAdecDataType->sOutFrameLock)) {
		logi("sOutReadyFrame is not empty\n");
		ADEC_OUT_FRAME *pFrameNode1,*pFrameNode2;
		aic_pthread_mutex_lock(&pAdecDataType->sOutFrameLock);
		mpp_list_for_each_entry_safe(pFrameNode1,pFrameNode2,&pAdecDataType->sOutReadyFrame,sList) {
			ret = aic_audio_decoder_put_frame(pAdecDataType->pDecoder, &pFrameNode1->sFrameInfo);
			pAdecDataType->nLeftReadyFrameWhenCompoentExitNum++;
			if (ret != 0) {// how to do
				loge("mpp_decoder_put_frame error!!!!\n");
			} else {

			}
			logd("pts:%ld:\n",pFrameNode1->sFrameInfo.pts);
			// now, no matter whether is back to decoder ok,move pFrameNode1 to sOutEmptyFrame
			mpp_list_del(&pFrameNode1->sList);
			mpp_list_add_tail(&pFrameNode1->sList, &pAdecDataType->sOutEmptyFrame);
		}
		aic_pthread_mutex_unlock(&pAdecDataType->sOutFrameLock);
	}
	return 0;
}

static void OMX_AdecEventNotify(
		ADEC_DATA_TYPE * pAdecDataType,
		OMX_EVENTTYPE event,
		OMX_U32 nData1,
		OMX_U32 nData2,
		OMX_PTR pEventData)
{
	if (pAdecDataType && pAdecDataType->pCallbacks && pAdecDataType->pCallbacks->EventHandler) {
		pAdecDataType->pCallbacks->EventHandler(
					pAdecDataType->hSelf,
					pAdecDataType->pAppData,event,
					nData1, nData2, pEventData);
	}
}

static void OMX_AdecStateChangeToInvalid(ADEC_DATA_TYPE * pAdecDataType)
{
	pAdecDataType->state = OMX_StateInvalid;
	OMX_AdecEventNotify(pAdecDataType
						,OMX_EventError
						,OMX_ErrorInvalidState,0,NULL);
	OMX_AdecEventNotify(pAdecDataType
						,OMX_EventCmdComplete
						,OMX_CommandStateSet
						,pAdecDataType->state,NULL);

}

static void OMX_AdecStateChangeLoaded(ADEC_DATA_TYPE * pAdecDataType)
{
	if (pAdecDataType->state == OMX_StateIdle) {
		OMX_AdecGiveBackAllPackets(pAdecDataType);
		OMX_AdecGiveBackAllFramesToDecoder(pAdecDataType);
	} else if (pAdecDataType->state == OMX_StateExecuting) {

	} else if (pAdecDataType->state == OMX_StatePause) {

	} else {
		OMX_AdecEventNotify(pAdecDataType
							,OMX_EventError
							,OMX_ErrorIncorrectStateTransition
							,pAdecDataType->state,NULL);
		loge("OMX_ErrorIncorrectStateTransition\n");
		return;
	}
	pAdecDataType->state = OMX_StateLoaded;
	OMX_AdecEventNotify(pAdecDataType
						,OMX_EventCmdComplete
						,OMX_CommandStateSet
						,pAdecDataType->state,NULL);

}
static void OMX_AdecStateChangeToIdle(ADEC_DATA_TYPE * pAdecDataType)
{
	int ret;
	if (pAdecDataType->state == OMX_StateLoaded) {
		//create decoder
		if (pAdecDataType->pDecoder == NULL) {
			pAdecDataType->pDecoder = aic_audio_decoder_create(pAdecDataType->eCodeType);
			if (pAdecDataType->pDecoder == NULL) {
				loge("mpp_decoder_create fail!!!!\n ");
				OMX_AdecEventNotify(pAdecDataType
							,OMX_EventError
							,OMX_ErrorIncorrectStateTransition
							,pAdecDataType->state,NULL);
				loge("OMX_ErrorIncorrectStateTransition\n");
				return;
			}
			logi("aic_audio_decoder_create ok!\n ");

			ret = aic_audio_decoder_init(pAdecDataType->pDecoder, &pAdecDataType->sDecoderConfig);
			if (ret) {
				loge("mpp_decoder_init %d failed\n", pAdecDataType->eCodeType);
				aic_audio_decoder_destroy(pAdecDataType->pDecoder);
				pAdecDataType->pDecoder = NULL;
				OMX_AdecEventNotify(pAdecDataType
							,OMX_EventError
							,OMX_ErrorIncorrectStateTransition
							,pAdecDataType->state,NULL);
				loge("OMX_ErrorIncorrectStateTransition\n");
				return;
			}
			logi("aic_audio_decoder_init ok!\n ");
		}
	} else if (pAdecDataType->state == OMX_StatePause) {

	} else if (pAdecDataType->state == OMX_StateExecuting) {

	} else {
		OMX_AdecEventNotify(pAdecDataType
					,OMX_EventError
					,OMX_ErrorIncorrectStateTransition
					,pAdecDataType->state,NULL);
		loge("OMX_ErrorIncorrectStateTransition\n");
		return;
	}
	pAdecDataType->state = OMX_StateIdle;
	OMX_AdecEventNotify(pAdecDataType
						,OMX_EventCmdComplete
						,OMX_CommandStateSet
						,pAdecDataType->state,NULL);

}
static void OMX_AdecStateChangeToExcuting(ADEC_DATA_TYPE * pAdecDataType)
{
	if (pAdecDataType->state == OMX_StateLoaded) {
		OMX_AdecEventNotify(pAdecDataType
					,OMX_EventError
					,OMX_ErrorIncorrectStateTransition
					,pAdecDataType->state,NULL);
		loge("OMX_ErrorIncorrectStateTransition\n");
		return;

	} else if (pAdecDataType->state == OMX_StateIdle) {

	} else if (pAdecDataType->state == OMX_StatePause) {

	} else {
		OMX_AdecEventNotify(pAdecDataType
					,OMX_EventError
					,OMX_ErrorIncorrectStateTransition
					,pAdecDataType->state,NULL);
		loge("OMX_ErrorIncorrectStateTransition\n");
		return;
	}
	pAdecDataType->state = OMX_StateExecuting;
	OMX_AdecEventNotify(pAdecDataType
						,OMX_EventCmdComplete
						,OMX_CommandStateSet
						,pAdecDataType->state,NULL);

}
static void OMX_AdecStateChangeToPause(ADEC_DATA_TYPE * pAdecDataType)
{
	if (pAdecDataType->state == OMX_StateLoaded) {

	} else if (pAdecDataType->state == OMX_StateIdle) {

	} else if (pAdecDataType->state == OMX_StateExecuting) {

	} else {
		OMX_AdecEventNotify(pAdecDataType
					,OMX_EventError
					,OMX_ErrorIncorrectStateTransition
					,pAdecDataType->state,NULL);
		loge("OMX_ErrorIncorrectStateTransition\n");
		return;
	}

	pAdecDataType->state = OMX_StatePause;
	OMX_AdecEventNotify(pAdecDataType
						,OMX_EventCmdComplete
						,OMX_CommandStateSet
						,pAdecDataType->state,NULL);

}

static void* OMX_AdecComponentThread(void* pThreadData)
{
	struct aic_message message;
	OMX_S32 nCmd;		//OMX_COMMANDTYPE
	OMX_S32 nCmdData;	//OMX_STATETYPE
	ADEC_DATA_TYPE* pAdecDataType = (ADEC_DATA_TYPE*)pThreadData;
	OMX_S32 ret;
	OMX_S32 dec_ret;
	ADEC_OUT_FRAME *pFrameNode;
	struct aic_audio_frame   sFrame;
	OMX_S32 bNotifyFrameEnd = 0;
	//prctl(PR_SET_NAME,(u32)"Adec");

	while(1) {
_AIC_MSG_GET_:
		if (aic_msg_get(&pAdecDataType->sMsgQue, &message) == 0) {
			nCmd = message.message_id;
			nCmdData = message.param;
			logw("nCmd:%d, nCmdData:%d\n",nCmd,nCmdData);
			if (OMX_CommandStateSet == nCmd) {
				aic_pthread_mutex_lock(&pAdecDataType->stateLock);
				if (pAdecDataType->state == (OMX_STATETYPE)(nCmdData)) {
					OMX_AdecEventNotify(pAdecDataType,OMX_EventError,OMX_ErrorSameState,0,NULL);
					aic_pthread_mutex_unlock(&pAdecDataType->stateLock);
					continue;
				}
				switch(nCmdData) {
					case OMX_StateInvalid:
						OMX_AdecStateChangeToInvalid(pAdecDataType);
						break;
					case OMX_StateLoaded:
						OMX_AdecStateChangeLoaded(pAdecDataType);
						break;
					case OMX_StateIdle:
						OMX_AdecStateChangeToIdle(pAdecDataType);
						break;
					case OMX_StateExecuting:
						OMX_AdecStateChangeToExcuting(pAdecDataType);
						break;
					case OMX_StatePause:
						OMX_AdecStateChangeToPause(pAdecDataType);
						break;
					default:
						break;
				}
				aic_pthread_mutex_unlock(&pAdecDataType->stateLock);
			} else if (OMX_CommandFlush == nCmd) {

			} else if (OMX_CommandPortDisable == nCmd) {

			} else if (OMX_CommandPortEnable == nCmd) {

			} else if (OMX_CommandMarkBuffer == nCmd) {

			} else if (OMX_CommandStop == nCmd) {
				logi("OMX_AdecComponentThread ready to exit!!!\n");
				goto _EXIT;
			} else {

			}
		}

		if (pAdecDataType->state != OMX_StateExecuting) {
			//usleep(1000);
			aic_msg_wait_new_msg(&pAdecDataType->sMsgQue, 0);
			continue;
		}

		if (pAdecDataType->nFlags & ADEC_OUTPORT_SEND_ALL_FRAME_FLAG) {
			if (!bNotifyFrameEnd) {
				//notify app decoder end
				OMX_AdecEventNotify(pAdecDataType,OMX_EventBufferFlag,0,0,NULL);
				/*
				//notify tunneld component decoder end
				if (pAdecDataType->sOutPortTunneledInfo.nTunneledFlag) {
					OMX_PARAM_FRAMEEND sFrameEnd;
					sFrameEnd.bFrameEnd = OMX_TRUE;
					OMX_SetParameter(pAdecDataType->sOutPortTunneledInfo.pTunneledComp, OMX_IndexVendorStreamFrameEnd,&sFrameEnd);
				}
				*/
				bNotifyFrameEnd = 1;
			}
			//usleep(1000);
			aic_msg_wait_new_msg(&pAdecDataType->sMsgQue, 0);
			continue;
		}
		bNotifyFrameEnd = 0;

		//give back packet to demux
		if ((pAdecDataType->sInPortTunneledInfo.nTunneledFlag)
			&& (!OMX_AdecListEmpty(&pAdecDataType->sInProcessedPkt,pAdecDataType->sInPktLock))) {
			ADEC_IN_PACKET *pktNode1 = NULL;
			while(!OMX_AdecListEmpty(&pAdecDataType->sInProcessedPkt,pAdecDataType->sInPktLock)) {
				aic_pthread_mutex_lock(&pAdecDataType->sInPktLock);
				pktNode1 = mpp_list_first_entry(&pAdecDataType->sInProcessedPkt, ADEC_IN_PACKET, sList);
				aic_pthread_mutex_unlock(&pAdecDataType->sInPktLock);
				ret = OMX_FillThisBuffer(pAdecDataType->sInPortTunneledInfo.pTunneledComp,&pktNode1->sBuff);
				if (ret == 0) {
					aic_pthread_mutex_lock(&pAdecDataType->sInPktLock);
					mpp_list_del(&pktNode1->sList);
					mpp_list_add_tail(&pktNode1->sList, &pAdecDataType->sInEmptyPkt);
					pAdecDataType->nGiveBackPacktOkNum++;
					logi("pAdecDataType->nGiveBackPacktOkNum:%d,pAdecDataType->nGiveBackPacktFailNum:%d\n"
						,pAdecDataType->nGiveBackPacktOkNum
						,pAdecDataType->nGiveBackPacktFailNum);
					aic_pthread_mutex_unlock(&pAdecDataType->sInPktLock);
				} else {
					//loge("OMX_FillThisBuffer error \n");
					pAdecDataType->nGiveBackPacktFailNum++;
					logi("pAdecDataType->nGiveBackPacktOkNum:%d,pAdecDataType->nGiveBackPacktFailNum:%d\n"
						,pAdecDataType->nGiveBackPacktOkNum
						,pAdecDataType->nGiveBackPacktFailNum);
					break;
				}
			}
		}

		aic_pthread_mutex_lock(&pAdecDataType->sInPktLock);
		aic_pthread_mutex_lock(&pAdecDataType->sOutFrameLock);
		dec_ret = aic_audio_decoder_decode(pAdecDataType->pDecoder);
		if (dec_ret == DEC_OK) {
			logd("mpp_decoder_decode ok!!!\n");
		} else if (dec_ret == DEC_NO_READY_PACKET) {
			pAdecDataType->nWaitForReadyPkt = 1;
		} else if (dec_ret == DEC_NO_EMPTY_FRAME) {
			pAdecDataType->nWaitForEmptyFrame = 1;
		} else if (dec_ret == DEC_NO_RENDER_FRAME) {
			logd("aic_audio_decoder_decode other error ret:%d \n",dec_ret);
		} else {
			logd("aic_audio_decoder_decode other error ret:%d \n",dec_ret);
		}
		aic_pthread_mutex_unlock(&pAdecDataType->sOutFrameLock);
		aic_pthread_mutex_unlock(&pAdecDataType->sInPktLock);


		do {
			OMX_S32 result = 0;
			OMX_BUFFERHEADERTYPE sBuffHead;

			ret = aic_audio_decoder_get_frame(pAdecDataType->pDecoder,&sFrame);
			if (ret != DEC_OK) {
				logd("mpp_decoder_get_frame other error ret:%d \n",ret);
				break;
			}
			if (OMX_AdecListEmpty(&pAdecDataType->sOutEmptyFrame,pAdecDataType->sOutFrameLock)) {
				ADEC_OUT_FRAME *pFrameNode = (ADEC_OUT_FRAME*)mpp_alloc(sizeof(ADEC_OUT_FRAME));
				if (NULL == pFrameNode) {
					loge("mpp_alloc error \n");
					aic_audio_decoder_put_frame(pAdecDataType->pDecoder, &sFrame);
					goto _AIC_MSG_GET_;
				}
				memset(pFrameNode,0x00,sizeof(ADEC_OUT_FRAME));
				aic_pthread_mutex_lock(&pAdecDataType->sOutFrameLock);
				mpp_list_add_tail(&pFrameNode->sList, &pAdecDataType->sOutEmptyFrame);
				aic_pthread_mutex_unlock(&pAdecDataType->sOutFrameLock);
				pAdecDataType->nOutFrameNodeNum++;
			}
			sBuffHead.nOutputPortIndex = ADEC_PORT_OUT_INDEX;
			sBuffHead.pBuffer = (OMX_U8 *)&sFrame;
			if (pAdecDataType->sOutPortTunneledInfo.nTunneledFlag) {
				sBuffHead.nInputPortIndex = pAdecDataType->sOutPortTunneledInfo.nTunnelPortIndex;
				result = OMX_EmptyThisBuffer(pAdecDataType->sOutPortTunneledInfo.pTunneledComp,&sBuffHead);
			} else {
				if (pAdecDataType->pCallbacks != NULL && pAdecDataType->pCallbacks->FillBufferDone != NULL) {
					result = pAdecDataType->pCallbacks->FillBufferDone(pAdecDataType->hSelf,pAdecDataType->pAppData,&sBuffHead);
				}
			}
			if (result == 0) {
				aic_pthread_mutex_lock(&pAdecDataType->sOutFrameLock);
				pFrameNode = mpp_list_first_entry(&pAdecDataType->sOutEmptyFrame, ADEC_OUT_FRAME, sList);
				pFrameNode->sFrameInfo = sFrame;
				mpp_list_del(&pFrameNode->sList);
				mpp_list_add_tail(&pFrameNode->sList, &pAdecDataType->sOutProcessingFrame);
				aic_pthread_mutex_unlock(&pAdecDataType->sOutFrameLock);
				if (pFrameNode->sFrameInfo.flag & FRAME_FLAG_EOS) {
					printf("[%s:%d] nFrameEndFlag",__FUNCTION__,__LINE__);
					pAdecDataType->nFlags |= ADEC_OUTPORT_SEND_ALL_FRAME_FLAG;
					if (pAdecDataType->pCallbacks && pAdecDataType->pCallbacks->EventHandler)  {
						pAdecDataType->pCallbacks->EventHandler(pAdecDataType->hSelf,pAdecDataType->pAppData,OMX_EventBufferFlag,0, 0,NULL);
					}
				}
				pAdecDataType->nSendFrameOkNum++;
			} else {
				//this may drop last frame,so it must deal with this case
				if (sFrame.flag & FRAME_FLAG_EOS) {
					printf("[%s:%d]frame end!!!\n",__FUNCTION__,__LINE__);
					pAdecDataType->nFlags |= ADEC_OUTPORT_SEND_ALL_FRAME_FLAG;
					if (pAdecDataType->pCallbacks && pAdecDataType->pCallbacks->EventHandler)  {
						pAdecDataType->pCallbacks->EventHandler(pAdecDataType->hSelf,pAdecDataType->pAppData,OMX_EventBufferFlag,0, 0,NULL);
					}
				}
				ret = aic_audio_decoder_put_frame(pAdecDataType->pDecoder, &sFrame);
				if (ret != 0) {// how to do
					loge("mpp_decoder_put_frame error!!!!\n");
					//ASSERT();
				}
				pAdecDataType->nSendFrameErrorNum++;
			}
		} while(1);

        if (dec_ret == DEC_NO_READY_PACKET) {
            aic_pthread_mutex_lock(&pAdecDataType->sInPktLock);
            if (!(pAdecDataType->nFlags & ADEC_INPORT_STREAM_END_FLAG)) {
                aic_pthread_mutex_unlock(&pAdecDataType->sInPktLock);
                aic_msg_wait_new_msg(&pAdecDataType->sMsgQue, 0);
            } else {
                aic_pthread_mutex_unlock(&pAdecDataType->sInPktLock);
                aic_msg_wait_new_msg(&pAdecDataType->sMsgQue, 5*1000);
            }
        } else if (dec_ret == DEC_NO_EMPTY_FRAME) {
            if (!(pAdecDataType->nFlags & ADEC_OUTPORT_SEND_ALL_FRAME_FLAG)) {
                aic_msg_wait_new_msg(&pAdecDataType->sMsgQue, 0);
            }
        } else {
            aic_msg_wait_new_msg(&pAdecDataType->sMsgQue, 5*1000);
        }
		// send frame back to decoder in OMX_AdecFillThisBuffer
	}

_EXIT:
	logi("in port:nReceivePacktOkNum:%d,"\
		"nReceivePacktFailNum:%d,"\
		"nPutPacktToDecoderOkNum:%d,"\
		"nPutPacktToDecoderFailNum:%d,"\
		"nGiveBackPacktOkNum:%d"\
		"nGiveBackPacktFailNum:%d\n"
		,pAdecDataType->nReceivePacktOkNum
		,pAdecDataType->nReceivePacktFailNum
		,pAdecDataType->nPutPacktToDecoderOkNum
		,pAdecDataType->nPutPacktToDecoderFailNum
		,pAdecDataType->nGiveBackPacktOkNum
		,pAdecDataType->nGiveBackPacktFailNum);

	logi("out port:nGetFrameFromDecoderNum:%d,"\
		"nDropFrameFromDecoderNum:%d,"\
		"nSendFrameOkNum:%d,"\
		"nSendFrameErrorNum:%d,"\
		"nLeftReadyFrameWhenCompoentExitNum:%d\n"
		,pAdecDataType->nGetFrameFromDecoderNum
		,pAdecDataType->nDropFrameFromDecoderNum
		,pAdecDataType->nSendFrameOkNum
		,pAdecDataType->nSendFrameErrorNum
		,pAdecDataType->nLeftReadyFrameWhenCompoentExitNum);
	logi("OMX_AdecComponentThread EXIT\n");
	return (void*)OMX_ErrorNone;
}

