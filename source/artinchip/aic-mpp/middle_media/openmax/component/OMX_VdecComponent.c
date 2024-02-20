/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: OMX_VdecComponent
*/

#include "OMX_VdecComponent.h"

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

#define OMX_VdecListEmpty(list,mutex)\
({\
	int ret = 0;\
	aic_pthread_mutex_lock(&mutex);\
	ret = mpp_list_empty(list);\
	aic_pthread_mutex_unlock(&mutex);\
	(ret);\
})

#define OMX_VDEC_PRINT_FRAME_NUM (30)

static OMX_ERRORTYPE OMX_VdecSendCommand(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_COMMANDTYPE Cmd,
		OMX_IN	OMX_U32 nParam1,
		OMX_IN	OMX_PTR pCmdData);

static OMX_ERRORTYPE OMX_VdecGetParameter(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nParamIndex,
		OMX_INOUT OMX_PTR pComponentParameterStructure);

static OMX_ERRORTYPE OMX_VdecSetParameter(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_IN	OMX_PTR pComponentParameterStructure);

static OMX_ERRORTYPE OMX_VdecGetConfig(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_INOUT OMX_PTR pComponentConfigStructure);

static OMX_ERRORTYPE OMX_VdecSetConfig(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_IN	OMX_PTR pComponentConfigStructure);

static OMX_ERRORTYPE OMX_VdecGetState(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_OUT OMX_STATETYPE* pState);

static OMX_ERRORTYPE OMX_VdecComponentTunnelRequest(
	OMX_IN	OMX_HANDLETYPE hComp,
	OMX_IN	OMX_U32 nPort,
	OMX_IN	OMX_HANDLETYPE hTunneledComp,
	OMX_IN	OMX_U32 nTunneledPort,
	OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup);

static OMX_ERRORTYPE OMX_VdecEmptyThisBuffer(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_BUFFERHEADERTYPE* pBuffer);

static OMX_ERRORTYPE OMX_VdecFillThisBuffer(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_BUFFERHEADERTYPE* pBuffer);

static OMX_ERRORTYPE OMX_VdecSetCallbacks(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_CALLBACKTYPE* pCallbacks,
		OMX_IN	OMX_PTR pAppData);

static int OMX_VdecGiveBackAllPackets(VDEC_DATA_TYPE *pVdecDataType);

static int OMX_VdecGiveBackAllFramesToDecoder(VDEC_DATA_TYPE *pVdecDataType);

static int OMX_VdecSendCommand(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_COMMANDTYPE Cmd,
		OMX_IN	OMX_U32 nParam1,
		OMX_IN	OMX_PTR pCmdData)
{
	VDEC_DATA_TYPE *pVdecDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	struct aic_message sMsg;
	pVdecDataType = (VDEC_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	sMsg.message_id = Cmd;
	sMsg.param = nParam1;
	sMsg.data_size = 0;

	//now not use always NULL
	if (pCmdData != NULL) {
		sMsg.data = pCmdData;
		sMsg.data_size = strlen((char*)pCmdData);
	}

	aic_msg_put(&pVdecDataType->sMsgQue, &sMsg);
	return eError;
}

static void* OMX_VdecComponentThread(void* pThreadData);

static OMX_ERRORTYPE OMX_VdecGetParameter(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nParamIndex,
		OMX_INOUT OMX_PTR pComponentParameterStructure)
{
	VDEC_DATA_TYPE *pVdecDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;

	pVdecDataType = (VDEC_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

	switch (nParamIndex) {
		case OMX_IndexParamPortDefinition: {
			OMX_PARAM_PORTDEFINITIONTYPE *port = (OMX_PARAM_PORTDEFINITIONTYPE*)pComponentParameterStructure;
			if (port->nPortIndex == VDEC_PORT_IN_INDEX) {
				memcpy(port,&pVdecDataType->sInPortDef,sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
			} else if (port->nPortIndex == VDEC_PORT_OUT_INDEX) {
				memcpy(port,&pVdecDataType->sOutPortDef,sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
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
				sBufferSupplier->eBufferSupplier = pVdecDataType->sInBufSupplier.eBufferSupplier;
			} else if (sBufferSupplier->nPortIndex == 1) {
				sBufferSupplier->eBufferSupplier = pVdecDataType->sOutBufSupplier.eBufferSupplier;
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

static OMX_S32 OMX_VdecVideoFormatTrans(enum mpp_codec_type *eDesType,OMX_VIDEO_CODINGTYPE *eSrcType)
{
	OMX_S32 ret = 0;
	if (eDesType== NULL || eSrcType == NULL) {
		loge("bad params!!!!\n");
		return -1;
	}
	if (*eSrcType == OMX_VIDEO_CodingAVC) {
		*eDesType = MPP_CODEC_VIDEO_DECODER_H264;
	} else if (*eSrcType == OMX_VIDEO_CodingMJPEG) {
		*eDesType = MPP_CODEC_VIDEO_DECODER_MJPEG;
	} else {
		loge("unsupport codec!!!!\n");
		ret = -1;
	}
	return ret;
}

static OMX_S32 OMX_VdecVideoPixelFormatTrans(enum mpp_pixel_format *eDesPixFormat,OMX_COLOR_FORMATTYPE *eSrcPixFormat)
{
	OMX_S32 ret = 0;
	if (eDesPixFormat== NULL || eSrcPixFormat == NULL) {
		loge("bad params!!!!\n");
		return -1;
	}
	if (*eSrcPixFormat == OMX_COLOR_FormatYUV420Planar) {
		*eDesPixFormat = MPP_FMT_YUV420P;
	} else if (*eSrcPixFormat == OMX_COLOR_FormatYUV420SemiPlanar) {
		*eDesPixFormat = MPP_FMT_NV12;
	} else if (*eSrcPixFormat == OMX_COLOR_FormatYUV420PackedPlanar) {
		*eDesPixFormat = MPP_FMT_NV21;
	} else {
		*eDesPixFormat = MPP_FMT_YUV420P;
		loge("unsupport pixformat!!!!\n");
		ret = -1;
	}
	return ret;

}

static OMX_ERRORTYPE OMX_VdecSetParameter(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nParamIndex,
		OMX_IN	OMX_PTR pComponentParameterStructure)
{
	VDEC_DATA_TYPE *pVdecDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_U32 index;
	OMX_S32 nIndex = (OMX_S32)nParamIndex;
	enum mpp_codec_type eCodecType;
	enum mpp_pixel_format ePixFormat;
	pVdecDataType = (VDEC_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	switch (nIndex) {
		case OMX_IndexParamVideoPortFormat: {
			OMX_VIDEO_PARAM_PORTFORMATTYPE *sPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE*)pComponentParameterStructure;
			index = sPortFormat->nPortIndex;
			if (index == VDEC_PORT_IN_INDEX) {
				pVdecDataType->sInPortDef.format.video.eCompressionFormat = sPortFormat->eCompressionFormat;
				pVdecDataType->sInPortDef.format.video.eColorFormat = sPortFormat->eColorFormat;
				logw("eCompressionFormat:%d,eColorFormat:%d\n"
				,pVdecDataType->sInPortDef.format.video.eCompressionFormat
				,pVdecDataType->sInPortDef.format.video.eColorFormat);

				if (OMX_VdecVideoFormatTrans(&eCodecType,&sPortFormat->eCompressionFormat) != 0) {
					eError = OMX_ErrorUnsupportedSetting;
					loge("OMX_ErrorUnsupportedSetting\n");
					break;
				}
				if (OMX_VdecVideoPixelFormatTrans(&ePixFormat,&sPortFormat->eColorFormat) != 0) {
					eError = OMX_ErrorUnsupportedSetting;
					loge("OMX_ErrorUnsupportedSetting\n");
					break;
				}
				pVdecDataType->eCodeType = eCodecType;
				pVdecDataType->sDecoderConfig.pix_fmt = ePixFormat;
				logw("eCompressionFormat:%d,eColorFormat:%d\n"
				,pVdecDataType->eCodeType
				,pVdecDataType->sDecoderConfig.pix_fmt);
				/*need to define extened OMX_Index or decide by inner*/
				pVdecDataType->sDecoderConfig.bitstream_buffer_size = 1024*1024;
				pVdecDataType->sDecoderConfig.packet_count = 10;
				pVdecDataType->sDecoderConfig.extra_frame_num = 1;

			} else if (index == VDEC_PORT_OUT_INDEX) {
				logw("now no need to set out port param\n");
				eError = OMX_ErrorUnsupportedSetting;
			} else {
				loge("OMX_ErrorBadParameter\n");
			}
			break;
		}
		case OMX_IndexParamPortDefinition: {
			OMX_PARAM_PORTDEFINITIONTYPE *port = (OMX_PARAM_PORTDEFINITIONTYPE*)pComponentParameterStructure;
			index = port->nPortIndex;
			if (index == VDEC_PORT_IN_INDEX) {
				pVdecDataType->sInPortDef.format.video.eCompressionFormat = port->format.video.eCompressionFormat;
				pVdecDataType->sInPortDef.format.video.eColorFormat = port->format.video.eColorFormat;
				logw("eCompressionFormat:%d,eColorFormat:%d\n"
				,pVdecDataType->sInPortDef.format.video.eCompressionFormat
				,pVdecDataType->sInPortDef.format.video.eColorFormat);

				if (OMX_VdecVideoFormatTrans(&eCodecType,&port->format.video.eCompressionFormat) != 0) {
					eError = OMX_ErrorUnsupportedSetting;
					loge("OMX_ErrorUnsupportedSetting\n");
					break;
				}
				if (OMX_VdecVideoPixelFormatTrans(&ePixFormat,& port->format.video.eColorFormat) != 0) {
					eError = OMX_ErrorUnsupportedSetting;
					loge("OMX_ErrorUnsupportedSetting\n");
					break;
				}

				/*need to convert */
				pVdecDataType->eCodeType = eCodecType;
				pVdecDataType->sDecoderConfig.pix_fmt = ePixFormat;
				logw("eCompressionFormat:%d,eColorFormat:%d\n"
				,pVdecDataType->eCodeType
				,pVdecDataType->sDecoderConfig.pix_fmt);
				/*need to define extened OMX_Index or decide by inner*/
				pVdecDataType->sDecoderConfig.bitstream_buffer_size = 1024*1024;
				pVdecDataType->sDecoderConfig.extra_frame_num = 1;
				pVdecDataType->sDecoderConfig.packet_count = 10;
			} else if (index == VDEC_PORT_OUT_INDEX) {
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
		default:
		 break;
	}
	return eError;
}

static OMX_ERRORTYPE OMX_VdecGetConfig(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_INOUT OMX_PTR pComponentConfigStructure)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;

	return eError;

}

static OMX_ERRORTYPE OMX_VdecSetConfig(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_IN	OMX_PTR pComponentConfigStructure)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_S32 nParamIndex = (OMX_S32)nIndex;
	VDEC_DATA_TYPE* pVdecDataType = (VDEC_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

	switch (nParamIndex) {
	case OMX_IndexConfigTimePosition:
		// 1 clear input buffer
		OMX_VdecGiveBackAllPackets(pVdecDataType);
		// 2 clear output buffer
		OMX_VdecGiveBackAllFramesToDecoder(pVdecDataType);
		// 3 clear flag
		pVdecDataType->nFlags = 0;
		pVdecDataType->nPutPacktToDecoderOkNum = 0;
		pVdecDataType->nGetFrameFromDecoderNum = 0;
		// 4 clear decoder buff
		mpp_decoder_reset(pVdecDataType->pDecoder);

		break;
	case OMX_IndexConfigTimeSeekMode:
		break;
	default:
		break;
	}
	return eError;

}

static OMX_ERRORTYPE OMX_VdecGetState(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_OUT OMX_STATETYPE* pState)
{
	VDEC_DATA_TYPE* pVdecDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	pVdecDataType = (VDEC_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

	aic_pthread_mutex_lock(&pVdecDataType->stateLock);
	*pState = pVdecDataType->state;
	aic_pthread_mutex_unlock(&pVdecDataType->stateLock);

	return eError;

}

static OMX_ERRORTYPE OMX_VdecComponentTunnelRequest(
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
	VDEC_DATA_TYPE* pVdecDataType;
	pVdecDataType = (VDEC_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComp)->pComponentPrivate);
	if (pVdecDataType->state != OMX_StateLoaded)
	 {
		loge("Component is not in OMX_StateLoaded,it is in%d,it can not tunnel\n",pVdecDataType->state);
		return OMX_ErrorInvalidState;
	}
	if (nPort == VDEC_PORT_IN_INDEX) {
		pPort = &pVdecDataType->sInPortDef;
		pTunneledInfo = &pVdecDataType->sInPortTunneledInfo;
		pBufSupplier = &pVdecDataType->sInBufSupplier;
	} else if (nPort == VDEC_PORT_OUT_INDEX) {
		pPort = &pVdecDataType->sOutPortDef;
		pTunneledInfo = &pVdecDataType->sOutPortTunneledInfo;
		pBufSupplier = &pVdecDataType->sOutBufSupplier;
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

static OMX_ERRORTYPE OMX_VdecEmptyThisBuffer(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	VDEC_DATA_TYPE* pVdecDataType;
	VDEC_IN_PACKET *pktNode;
	int ret = 0;
	struct aic_message sMsg;
	static int rate = 0;
	static struct timespec pev =  {0,0} ,cur =  {0,0};
	pVdecDataType = (VDEC_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

	aic_pthread_mutex_lock(&pVdecDataType->stateLock);
	if (pVdecDataType->state != OMX_StateExecuting) {
		logw("component is not in OMX_StateExecuting,it is in [%d]!!!\n",pVdecDataType->state);
		aic_pthread_mutex_unlock(&pVdecDataType->stateLock);
		return OMX_ErrorIncorrectStateOperation;
	}
	//aic_pthread_mutex_unlock(&pVdecDataType->stateLock);

	if (pVdecDataType->sInPortTunneledInfo.nTunneledFlag) {
		if (pVdecDataType->sInBufSupplier.eBufferSupplier == OMX_BufferSupplyOutput) {
			if (OMX_VdecListEmpty(&pVdecDataType->sInEmptyPkt,pVdecDataType->sInPktLock)) {
				if (pVdecDataType->nInPktNodeNum + 1> VDEC_PACKET_NUM_MAX) {
					loge("empty node has aready increase to max [%d]!!!\n",pVdecDataType->nInPktNodeNum);
					eError = OMX_ErrorInsufficientResources;
					pVdecDataType->nReceivePacktFailNum++;
					//return  eError;
					goto _EXIT;
				} else {
					int i;
					logw("no empty node,need to extend!!!\n");
					for(i =0 ; i < VDEC_PACKET_ONE_TIME_CREATE_NUM; i++) {
						VDEC_IN_PACKET *pPktNode = (VDEC_IN_PACKET*)mpp_alloc(sizeof(VDEC_IN_PACKET));
						if (NULL == pPktNode) {
							break;
						}
						memset(pPktNode,0x00,sizeof(VDEC_IN_PACKET));
						aic_pthread_mutex_lock(&pVdecDataType->sInPktLock);
						mpp_list_add_tail(&pPktNode->sList, &pVdecDataType->sInEmptyPkt);
						aic_pthread_mutex_unlock(&pVdecDataType->sInPktLock);
						pVdecDataType->nInPktNodeNum++;
					}
					if (i == 0) {
						loge("mpp_alloc empty video node fail\n");
						eError = OMX_ErrorInsufficientResources;
						pVdecDataType->nReceivePacktFailNum++;
						//return  eError;
						goto _EXIT;
					}
				}
			}

			struct mpp_packet pkt;
			pkt.size = pBuffer->nFilledLen;
			ret = mpp_decoder_get_packet(pVdecDataType->pDecoder, &pkt, pkt.size);
			if (ret != 0) {
				//loge("get pkt from decoder error ret:%d!!!\n",ret);
				if (ret == DEC_NO_EMPTY_PACKET) {
					pVdecDataType->nReceivePacktFailNum++;
					//return  OMX_ErrorOverflow;
					eError = OMX_ErrorOverflow;
					goto _EXIT;
				} else {
					pVdecDataType->nReceivePacktFailNum++;
					//return OMX_ErrorInsufficientResources;
					eError = OMX_ErrorInsufficientResources;
					goto _EXIT;
				}
			}
			memcpy(pkt.data,pBuffer->pBuffer,pkt.size);
			pkt.flag =pBuffer->nFlags;
			pkt.pts = pBuffer->nTimeStamp;
			//mpp_decoder_get_packet is ok then mpp_decoder_put_packet is also ok
			mpp_decoder_put_packet(pVdecDataType->pDecoder, &pkt);

			rate += pkt.size;
			if (pev.tv_sec == 0) {
				clock_gettime(CLOCK_REALTIME,&pev);
			} else {
				long diff;
				clock_gettime(CLOCK_REALTIME,&cur);
				diff = (cur.tv_sec - pev.tv_sec)*1000*1000 + (cur.tv_nsec - pev.tv_nsec)/1000;
				if (diff > 42*1000) {
					logi(" vbv rate:%d,diff:%ld \n",rate*8,diff);
					rate = 0;
					pev = cur;
				}
			}

			aic_pthread_mutex_lock(&pVdecDataType->sInPktLock);
			pktNode = mpp_list_first_entry(&pVdecDataType->sInEmptyPkt, VDEC_IN_PACKET, sList);
			pktNode->sBuff.nOutputPortIndex = pBuffer->nOutputPortIndex;
			pktNode->sBuff.pBuffer = pBuffer->pBuffer;
			pktNode->sBuff.nFilledLen = pBuffer->nFilledLen;
			pktNode->sBuff.nTimeStamp = pBuffer->nTimeStamp;
			pktNode->sBuff.nFlags = pBuffer->nFlags;
			mpp_list_del(&pktNode->sList);
			mpp_list_add_tail(&pktNode->sList, &pVdecDataType->sInProcessedPkt);
			if (pVdecDataType->nWaitForReadyPkt == 1) {
				sMsg.message_id = OMX_CommandNops;
				sMsg.data_size = 0;
				aic_msg_put(&pVdecDataType->sMsgQue, &sMsg);
				pVdecDataType->nWaitForReadyPkt = 0;
			}
			if (pktNode->sBuff.nFlags & PACKET_FLAG_EOS) {
				pVdecDataType->nFlags |= VDEC_INPORT_STREAM_END_FLAG;
				//pVdecDataType->nStreamEndFlag = OMX_TRUE;
					printf("[%s:%d]:StreamEndFlag\n",__FUNCTION__,__LINE__);
			}
			pVdecDataType->nReceivePacktOkNum++;
			logd("pVdecDataType->nReceivePacktOkNum:%d\n",pVdecDataType->nReceivePacktOkNum);
			aic_pthread_mutex_unlock(&pVdecDataType->sInPktLock);
		} else if (pVdecDataType->sInBufSupplier.eBufferSupplier == OMX_BufferSupplyInput) {
			eError = OMX_ErrorNotImplemented;
			logw("OMX_ErrorNotImplemented\n");
		} else {
			eError = OMX_ErrorNotImplemented;
			logw("OMX_ErrorNotImplemented\n");
		}
	} else {
		struct mpp_packet pkt;
		pkt.size = pBuffer->nFilledLen;
		ret = mpp_decoder_get_packet(pVdecDataType->pDecoder, &pkt, pkt.size);
		if (ret != 0) {
			//loge("get pkt from decoder error ret:%d!!!\n",ret);
			if (ret == DEC_NO_EMPTY_PACKET) {
				pVdecDataType->nReceivePacktFailNum++;
				//return  OMX_ErrorOverflow;
				eError = OMX_ErrorOverflow;
				goto _EXIT;
			} else {
				pVdecDataType->nReceivePacktFailNum++;
				//return  OMX_ErrorInsufficientResources;
				eError = OMX_ErrorInsufficientResources;
				goto _EXIT;;
			}
		} else {
			//logw("get pkt from decoder ok!!!\n");
		}
		memcpy(pkt.data,pBuffer->pBuffer,pkt.size);
		pkt.flag =pBuffer->nFlags;
		pkt.pts = pBuffer->nTimeStamp;
		//mpp_decoder_get_packet is ok then mpp_decoder_put_packet is also ok
		mpp_decoder_put_packet(pVdecDataType->pDecoder, &pkt);
		if (pkt.flag & PACKET_FLAG_EOS) {
			pVdecDataType->nFlags |= VDEC_INPORT_STREAM_END_FLAG;
			//pVdecDataType->nStreamEndFlag = OMX_TRUE;
			logi("StreamEndFlag!!!\n");
		}
		pVdecDataType->nReceivePacktOkNum++;
		pVdecDataType->nGiveBackPacktOkNum++;
	}

_EXIT:
	aic_pthread_mutex_unlock(&pVdecDataType->stateLock);
	return eError;
}

static OMX_ERRORTYPE OMX_VdecFillThisBuffer(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	VDEC_DATA_TYPE* pVdecDataType;
	struct mpp_frame *pFrame;
	VDEC_OUT_FRAME *pFrameNode1 = NULL,*pFrameNode2 = NULL;
	OMX_BOOL bMatch = OMX_FALSE;
	OMX_S32 ret;
	pVdecDataType = (VDEC_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	if (pBuffer->nOutputPortIndex != VDEC_PORT_OUT_INDEX) {
		loge("port not match\n");
		return OMX_ErrorBadParameter;
	}

	if (!OMX_VdecListEmpty(&pVdecDataType->sOutProcessingFrame,pVdecDataType->sOutFrameLock)) {
		pFrame = (struct mpp_frame *)pBuffer->pBuffer;
		aic_pthread_mutex_lock(&pVdecDataType->sOutFrameLock);
		mpp_list_for_each_entry_safe(pFrameNode1,pFrameNode2,&pVdecDataType->sOutProcessingFrame,sList) {
			if (pFrameNode1->sFrameInfo.id == pFrame->id
				&&pFrameNode1->sFrameInfo.buf.phy_addr[0] == pFrame->buf.phy_addr[0]) {
					bMatch = OMX_TRUE;
					break;
				}
		}
		logi("bMatch:%d\n",bMatch);
		if (bMatch) {//give frame back to decoder
			struct aic_message sMsg;
			ret = mpp_decoder_put_frame(pVdecDataType->pDecoder, &pFrameNode1->sFrameInfo);
			if (ret != 0) {// how to do
				loge("mpp_decoder_put_frame error!!!!\n");
			}
			// now, no matter whether is back to decoder ok,move pFrameNode1 to sOutEmptyFrame
			pVdecDataType->nSendBackFrameOkNum++;
			mpp_list_del(&pFrameNode1->sList);
			mpp_list_add_tail(&pFrameNode1->sList, &pVdecDataType->sOutEmptyFrame);
			if (pVdecDataType->nWaitForEmptyFrame == 1) {
				sMsg.message_id = OMX_CommandNops;
				sMsg.data_size = 0;
				aic_msg_put(&pVdecDataType->sMsgQue, &sMsg);
				pVdecDataType->nWaitForEmptyFrame = 0;
			}
		} else {
			loge("frame not match!!!\n");
			eError =  OMX_ErrorBadParameter;
			pVdecDataType->nSendBackFrameErrorNum++;
		}
		logi("pVdecDataType->nSendBackFrameOkNum:%d,pAdecDataType->nSendBackFrameErrorNum:%d"
			,pVdecDataType->nSendBackFrameOkNum
			,pVdecDataType->nSendBackFrameErrorNum);
		aic_pthread_mutex_unlock(&pVdecDataType->sOutFrameLock);
	} else {
		logw("no frame need to back \n");
		eError =  OMX_ErrorBadParameter;
	}

	return eError;
}

static OMX_ERRORTYPE OMX_VdecSetCallbacks(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_CALLBACKTYPE* pCallbacks,
		OMX_IN	OMX_PTR pAppData)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	VDEC_DATA_TYPE* pVdecDataType;
	pVdecDataType = (VDEC_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	pVdecDataType->pCallbacks = pCallbacks;
	pVdecDataType->pAppData = pAppData;
	return eError;
}

OMX_ERRORTYPE OMX_VdecComponentDeInit(
		OMX_IN	OMX_HANDLETYPE hComponent)  {
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_COMPONENTTYPE *pComp;
	VDEC_DATA_TYPE *pVdecDataType;
	VDEC_IN_PACKET *pPktNode = NULL,*pPktNode1 = NULL;
	VDEC_OUT_FRAME *pFrameNode = NULL,*pFrameNode1 = NULL;
	pComp = (OMX_COMPONENTTYPE *)hComponent;
	struct aic_message sMsg;
	pVdecDataType = (VDEC_DATA_TYPE *)pComp->pComponentPrivate;

	aic_pthread_mutex_lock(&pVdecDataType->stateLock);
	if (pVdecDataType->state != OMX_StateLoaded) {
		logw("compoent is in %d,but not in OMX_StateLoaded(1),can not FreeHandle.\n",pVdecDataType->state);
		aic_pthread_mutex_unlock(&pVdecDataType->stateLock);
		return OMX_ErrorIncorrectStateOperation;
	}
	aic_pthread_mutex_unlock(&pVdecDataType->stateLock);

	sMsg.message_id = OMX_CommandStop;
	sMsg.data_size = 0;
	aic_msg_put(&pVdecDataType->sMsgQue, &sMsg);
	pthread_join(pVdecDataType->threadId, (void*)&eError);

	aic_pthread_mutex_lock(&pVdecDataType->sInPktLock);
	if (!mpp_list_empty(&pVdecDataType->sInEmptyPkt)) {
			mpp_list_for_each_entry_safe(pPktNode, pPktNode1, &pVdecDataType->sInEmptyPkt, sList) {
				mpp_list_del(&pPktNode->sList);
				mpp_free(pPktNode);
		}
	}

	if (!mpp_list_empty(&pVdecDataType->sInReadyPkt)) {
			mpp_list_for_each_entry_safe(pPktNode, pPktNode1, &pVdecDataType->sInReadyPkt, sList) {
				mpp_list_del(&pPktNode->sList);
				mpp_free(pPktNode);
		}
	}

	aic_pthread_mutex_unlock(&pVdecDataType->sInPktLock);

	aic_pthread_mutex_lock(&pVdecDataType->sOutFrameLock);
	if (!mpp_list_empty(&pVdecDataType->sOutEmptyFrame)) {
			mpp_list_for_each_entry_safe(pFrameNode, pFrameNode1, &pVdecDataType->sOutEmptyFrame, sList) {
				mpp_list_del(&pFrameNode->sList);
				mpp_free(pFrameNode);
		}
	}
	if (!mpp_list_empty(&pVdecDataType->sOutReadyFrame)) {
			mpp_list_for_each_entry_safe(pFrameNode, pFrameNode1, &pVdecDataType->sOutReadyFrame, sList) {
				mpp_list_del(&pFrameNode->sList);
				mpp_free(pFrameNode);
		}
	}

	if (!mpp_list_empty(&pVdecDataType->sOutProcessingFrame)) {
			mpp_list_for_each_entry_safe(pFrameNode, pFrameNode1, &pVdecDataType->sOutProcessingFrame, sList) {
				mpp_list_del(&pFrameNode->sList);
				mpp_free(pFrameNode);
		}
	}
	aic_pthread_mutex_unlock(&pVdecDataType->sOutFrameLock);

	pthread_mutex_destroy(&pVdecDataType->sInPktLock);
	pthread_mutex_destroy(&pVdecDataType->sOutFrameLock);
	pthread_mutex_destroy(&pVdecDataType->stateLock);

	aic_msg_destroy(&pVdecDataType->sMsgQue);

	if (pVdecDataType->pDecoder)  {
		mpp_decoder_destory(pVdecDataType->pDecoder);
		pVdecDataType->pDecoder = NULL;
	}

	mpp_free(pVdecDataType);
	pVdecDataType = NULL;

	logw("OMX_VideoRenderComponentDeInit\n");
	return eError;
}

OMX_ERRORTYPE OMX_VdecComponentInit(
		OMX_IN	OMX_HANDLETYPE hComponent)
{
	OMX_COMPONENTTYPE *pComp;
	VDEC_DATA_TYPE *pVdecDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_U32 err;
	OMX_U32 i;

	logw("OMX_VdecComponentInit....");

	pComp = (OMX_COMPONENTTYPE *)hComponent;

	pVdecDataType = (VDEC_DATA_TYPE *)mpp_alloc(sizeof(VDEC_DATA_TYPE));

	if (NULL == pVdecDataType)  {
		loge("mpp_alloc(sizeof(VDEC_DATA_TYPE) fail!");
		eError = OMX_ErrorInsufficientResources;
		goto _EXIT1;
	}

	memset(pVdecDataType, 0x0, sizeof(VDEC_DATA_TYPE));
	pComp->pComponentPrivate	= (void*) pVdecDataType;
	pVdecDataType->state 		= OMX_StateLoaded;
	pVdecDataType->hSelf 		= pComp;

	pComp->SetCallbacks 		= OMX_VdecSetCallbacks;
	pComp->SendCommand			= OMX_VdecSendCommand;
	pComp->GetState 			= OMX_VdecGetState;
	pComp->GetParameter 		= OMX_VdecGetParameter;
	pComp->SetParameter 		= OMX_VdecSetParameter;
	pComp->GetConfig			= OMX_VdecGetConfig;
	pComp->SetConfig			= OMX_VdecSetConfig;
	pComp->ComponentTunnelRequest = OMX_VdecComponentTunnelRequest;
	pComp->ComponentDeInit		= OMX_VdecComponentDeInit;
	pComp->FillThisBuffer		= OMX_VdecFillThisBuffer;
	pComp->EmptyThisBuffer		= OMX_VdecEmptyThisBuffer;

	pVdecDataType->sPortParam.nPorts = 2;
	pVdecDataType->sPortParam.nStartPortNumber = 0x0;

	pVdecDataType->sInPortDef.nPortIndex = VDEC_PORT_IN_INDEX;
	pVdecDataType->sInPortDef.bPopulated = OMX_TRUE;
	pVdecDataType->sInPortDef.bEnabled	= OMX_TRUE;
	pVdecDataType->sInPortDef.eDomain = OMX_PortDomainVideo;
	pVdecDataType->sInPortDef.eDir = OMX_DirInput;

	pVdecDataType->sOutPortDef.nPortIndex = VDEC_PORT_OUT_INDEX;
	pVdecDataType->sOutPortDef.bPopulated = OMX_TRUE;
	pVdecDataType->sOutPortDef.bEnabled = OMX_TRUE;
	pVdecDataType->sOutPortDef.eDomain = OMX_PortDomainVideo;
	pVdecDataType->sOutPortDef.eDir = OMX_DirOutput;

	pVdecDataType->sInPortTunneledInfo.nPortIndex = VDEC_PORT_IN_INDEX;
	pVdecDataType->sInPortTunneledInfo.pSelfComp = hComponent;
	pVdecDataType->sOutPortTunneledInfo.nPortIndex = VDEC_PORT_OUT_INDEX;
	pVdecDataType->sOutPortTunneledInfo.pSelfComp = hComponent;

	//now  demux  support buffers ,later modify
	pVdecDataType->sInBufSupplier.nPortIndex = 0x0;
	pVdecDataType->sInBufSupplier.eBufferSupplier = OMX_BufferSupplyOutput;
	pVdecDataType->nInPktNodeNum = 0;
	mpp_list_init(&pVdecDataType->sInEmptyPkt);
	mpp_list_init(&pVdecDataType->sInReadyPkt);
	mpp_list_init(&pVdecDataType->sInProcessedPkt);
	pthread_mutex_init(&pVdecDataType->sInPktLock, NULL);
	for(i =0 ; i < VDEC_PACKET_ONE_TIME_CREATE_NUM; i++) {
		VDEC_IN_PACKET *pPktNode = (VDEC_IN_PACKET*)mpp_alloc(sizeof(VDEC_IN_PACKET));
		if (NULL == pPktNode) {
			break;
		}
		memset(pPktNode,0x00,sizeof(VDEC_IN_PACKET));
		mpp_list_add_tail(&pPktNode->sList, &pVdecDataType->sInEmptyPkt);
		pVdecDataType->nInPktNodeNum++;
	}
	if (pVdecDataType->nInPktNodeNum == 0) {
		loge("mpp_alloc empty video node fail\n");
		eError = OMX_ErrorInsufficientResources;
		goto _EXIT2;
	}

	// vdec support out pot buffer
	pVdecDataType->sOutBufSupplier.nPortIndex = 0x1;
	pVdecDataType->sOutBufSupplier.eBufferSupplier = OMX_BufferSupplyOutput;
	pVdecDataType->nOutFrameNodeNum = 0;
	mpp_list_init(&pVdecDataType->sOutEmptyFrame);
	mpp_list_init(&pVdecDataType->sOutReadyFrame);
	mpp_list_init(&pVdecDataType->sOutProcessingFrame);
	pthread_mutex_init(&pVdecDataType->sOutFrameLock, NULL);
	for(i =0 ; i < VDEC_FRAME_ONE_TIME_CREATE_NUM; i++) {
		VDEC_OUT_FRAME *pFrameNode = (VDEC_OUT_FRAME*)mpp_alloc(sizeof(VDEC_OUT_FRAME));
		if (NULL == pFrameNode) {
			break;
		}
		memset(pFrameNode,0x00,sizeof(VDEC_OUT_FRAME));
		mpp_list_add_tail(&pFrameNode->sList, &pVdecDataType->sOutEmptyFrame);
		pVdecDataType->nOutFrameNodeNum++;
	}
	if (pVdecDataType->nOutFrameNodeNum == 0) {
		loge("mpp_alloc empty video node fail\n");
		eError = OMX_ErrorInsufficientResources;
		goto _EXIT3;
	}

	if (aic_msg_create(&pVdecDataType->sMsgQue)<0)
	 {
		loge("aic_msg_create fail!");
		eError = OMX_ErrorInsufficientResources;
		goto _EXIT4;
	}

	pthread_mutex_init(&pVdecDataType->stateLock, NULL);
	// Create the component thread
	err = pthread_create(&pVdecDataType->threadId, NULL, OMX_VdecComponentThread, pVdecDataType);
	if (err || !pVdecDataType->threadId) {
		loge("pthread_create fail!");
		eError = OMX_ErrorInsufficientResources;
		goto _EXIT5;
	}

	return eError;

_EXIT5:
	aic_msg_destroy(&pVdecDataType->sMsgQue);
	pthread_mutex_destroy(&pVdecDataType->stateLock);

_EXIT4:
	if (!mpp_list_empty(&pVdecDataType->sInEmptyPkt)) {
		VDEC_IN_PACKET *pPktNode = NULL,*pPktNode1 = NULL;
		mpp_list_for_each_entry_safe(pPktNode, pPktNode1, &pVdecDataType->sInEmptyPkt, sList) {
			mpp_list_del(&pPktNode->sList);
			mpp_free(pPktNode);
		}
	}

_EXIT3:
	if (!mpp_list_empty(&pVdecDataType->sOutEmptyFrame)) {
		VDEC_OUT_FRAME *pFrameNode = NULL,*pFrameNode1 = NULL;
		mpp_list_for_each_entry_safe(pFrameNode, pFrameNode1, &pVdecDataType->sOutEmptyFrame, sList) {
			mpp_list_del(&pFrameNode->sList);
			mpp_free(pFrameNode);
		}
	}

_EXIT2:
	if (pVdecDataType) {
		mpp_free(pVdecDataType);
		pVdecDataType = NULL;
	}

_EXIT1:
	return eError;
}

static int OMX_VdecGiveBackAllPackets(VDEC_DATA_TYPE *pVdecDataType)
{
	int ret = 0;
	// ,move sInReadyPkt to sInProcessedPkt
	logi("Before OMX_VdecComponentThread exit,move node in sInReadyPkt to sInProcessedPkt\n");
	if (!OMX_VdecListEmpty(&pVdecDataType->sInReadyPkt,pVdecDataType->sInPktLock)) {
		logi("sInReadyFrame is not empty\n");
		VDEC_IN_PACKET *pktNode1,*pktNode2;
		aic_pthread_mutex_lock(&pVdecDataType->sInPktLock);
		mpp_list_for_each_entry_safe(pktNode1, pktNode2, &pVdecDataType->sInReadyPkt, sList) {
			pVdecDataType->nLeftReadyFrameWhenCompoentExitNum++;
			mpp_list_del(&pktNode1->sList);
			mpp_list_add_tail(&pktNode1->sList, &pVdecDataType->sInProcessedPkt);
		}
		aic_pthread_mutex_unlock(&pVdecDataType->sInPktLock);
	}

	//give back all in port pkts,
	logi("Before OMX_VdecComponentThread exit,it must give back all in port pkts\n");
	if (!OMX_VdecListEmpty(&pVdecDataType->sInProcessedPkt,pVdecDataType->sInPktLock)) {
		logi("sInProcessedPkt is not empty\n");
		VDEC_IN_PACKET *pktNode1;
		while(!OMX_VdecListEmpty(&pVdecDataType->sInProcessedPkt,pVdecDataType->sInPktLock)) {
			aic_pthread_mutex_lock(&pVdecDataType->sInPktLock);
			pktNode1 = mpp_list_first_entry(&pVdecDataType->sInProcessedPkt, VDEC_IN_PACKET, sList);
			aic_pthread_mutex_unlock(&pVdecDataType->sInPktLock);
			ret = 0;
			if (pVdecDataType->sInPortTunneledInfo.nTunneledFlag) {
				ret = OMX_FillThisBuffer(pVdecDataType->sInPortTunneledInfo.pTunneledComp,&pktNode1->sBuff);
				if (ret != 0) { // how to do ,deal with problem by TunneledComp, do nothing here
					loge("OMX_FillThisBuffer error \n");
				}
			} else {
				if (pVdecDataType->pCallbacks != NULL && pVdecDataType->pCallbacks->EmptyBufferDone!= NULL) {
					ret = pVdecDataType->pCallbacks->EmptyBufferDone(pVdecDataType->hSelf,pVdecDataType->pAppData,&pktNode1->sBuff);
					if (ret != 0) {// how to do ,deal with problem by app, do nothing here
						loge("EmptyBufferDone error \n");
					}
				}
			}
			if (ret == 0) {
				pVdecDataType->nGiveBackPacktOkNum++;
			} else {
				pVdecDataType->nGiveBackPacktFailNum++;
				continue;// must give back ok ,so retry to give back
			}
			aic_pthread_mutex_lock(&pVdecDataType->sInPktLock);
			mpp_list_del(&pktNode1->sList);
			mpp_list_add_tail(&pktNode1->sList, &pVdecDataType->sInEmptyPkt);
			logi("pVdecDataType->nGiveBackPacktOkNum:%d,pVdecDataType->nGiveBackPacktFailNum:%d\n"
				,pVdecDataType->nGiveBackPacktOkNum
				,pVdecDataType->nGiveBackPacktFailNum);
			aic_pthread_mutex_unlock(&pVdecDataType->sInPktLock);
		}
	}

	return 0;
}

static int OMX_VdecGiveBackAllFramesToDecoder(VDEC_DATA_TYPE *pVdecDataType)
{
	int ret = 0;

	//wait for all out port frames from other component or app to back.
	logi("Before OMX_VdecComponentThread exit,it must wait for sOutProcessingFrame empty\n");
	while(!OMX_VdecListEmpty(&pVdecDataType->sOutProcessingFrame,pVdecDataType->sOutFrameLock)) {
		usleep(1000);
	}

	//give back all frames in sOutReadyFrame to decoder
	logi("Before OMX_VdecComponentThread exit,it must give back all frames in sOutReadyFrame to decoder\n");
	if (!OMX_VdecListEmpty(&pVdecDataType->sOutReadyFrame,pVdecDataType->sOutFrameLock)) {
		logi("sOutReadyFrame is not empty\n");
		VDEC_OUT_FRAME *pFrameNode1,*pFrameNode2;
		aic_pthread_mutex_lock(&pVdecDataType->sOutFrameLock);
		mpp_list_for_each_entry_safe(pFrameNode1,pFrameNode2,&pVdecDataType->sOutReadyFrame,sList) {
			ret = mpp_decoder_put_frame(pVdecDataType->pDecoder, &pFrameNode1->sFrameInfo);
			pVdecDataType->nLeftReadyFrameWhenCompoentExitNum++;
			if (ret != 0) {// how to do
				loge("mpp_decoder_put_frame error!!!!\n");
			} else {

			}
			// now, no matter whether is back to decoder ok,move pFrameNode1 to sOutEmptyFrame
			mpp_list_del(&pFrameNode1->sList);
			mpp_list_add_tail(&pFrameNode1->sList, &pVdecDataType->sOutEmptyFrame);
		}
		aic_pthread_mutex_unlock(&pVdecDataType->sOutFrameLock);
	}
	return 0;
}

static void OMX_VdecEventNotify(
		VDEC_DATA_TYPE * pVdecDataType,
		OMX_EVENTTYPE event,
		OMX_U32 nData1,
		OMX_U32 nData2,
		OMX_PTR pEventData)
{
	if (pVdecDataType && pVdecDataType->pCallbacks && pVdecDataType->pCallbacks->EventHandler)  {
		pVdecDataType->pCallbacks->EventHandler(
					pVdecDataType->hSelf,
					pVdecDataType->pAppData,event,
					nData1, nData2, pEventData);
	}
}

static void OMX_VdecStateChangeToInvalid(VDEC_DATA_TYPE * pVdecDataType)
{
	pVdecDataType->state = OMX_StateInvalid;
	OMX_VdecEventNotify(pVdecDataType
						,OMX_EventError
						,OMX_ErrorInvalidState,0,NULL);
	OMX_VdecEventNotify(pVdecDataType
						,OMX_EventCmdComplete
						,OMX_CommandStateSet
						,pVdecDataType->state,NULL);

}

static void OMX_VdecStateChangeToIdle(VDEC_DATA_TYPE * pVdecDataType)
{
	int ret;
	if (pVdecDataType->state == OMX_StateLoaded) {
		//create decoder
		if (pVdecDataType->pDecoder == NULL) {
			pVdecDataType->pDecoder = mpp_decoder_create(pVdecDataType->eCodeType);
			if (pVdecDataType->pDecoder == NULL) {
				loge("mpp_decoder_create fail!!!!\n ");
				OMX_VdecEventNotify(pVdecDataType
							,OMX_EventError
							,OMX_ErrorIncorrectStateTransition
							,pVdecDataType->state,NULL);
				return;
			}
			logi("mpp_decoder_create ok!\n ");

			ret = mpp_decoder_init(pVdecDataType->pDecoder, &pVdecDataType->sDecoderConfig);
			if (ret) {
				loge("mpp_decoder_init %d failed", pVdecDataType->eCodeType);
				mpp_decoder_destory(pVdecDataType->pDecoder);
				pVdecDataType->pDecoder = NULL;
				OMX_VdecEventNotify(pVdecDataType
							,OMX_EventError
							,OMX_ErrorIncorrectStateTransition
							,pVdecDataType->state,NULL);
				return;
			}
			logi("mpp_decoder_init ok!\n ");
		}
	} else if (pVdecDataType->state == OMX_StatePause) {

	} else if (pVdecDataType->state == OMX_StateExecuting) {

	} else {
		OMX_VdecEventNotify(pVdecDataType
						,OMX_EventError
						,OMX_ErrorIncorrectStateTransition
						,pVdecDataType->state,NULL);
		loge("OMX_ErrorIncorrectStateTransition\n");
		return;
	}
	pVdecDataType->state = OMX_StateIdle;
	OMX_VdecEventNotify(pVdecDataType
						,OMX_EventCmdComplete
						,OMX_CommandStateSet
						,pVdecDataType->state,NULL);

}

static void OMX_VdecStateChangeToPause(VDEC_DATA_TYPE * pVdecDataType)
{
	if (pVdecDataType->state == OMX_StateLoaded) {

	} else if (pVdecDataType->state == OMX_StateIdle) {

	} else if (pVdecDataType->state == OMX_StateExecuting) {

	} else {
		OMX_VdecEventNotify(pVdecDataType
					,OMX_EventError
					,OMX_ErrorIncorrectStateTransition
					,pVdecDataType->state,NULL);
		loge("OMX_ErrorIncorrectStateTransition\n");
		return;
	}
	pVdecDataType->state = OMX_StatePause;
	OMX_VdecEventNotify(pVdecDataType
					,OMX_EventCmdComplete
					,OMX_CommandStateSet
					,pVdecDataType->state,NULL);

}

static void OMX_VdecStateChangeToLoaded(VDEC_DATA_TYPE * pVdecDataType)
{
	if (pVdecDataType->state == OMX_StateIdle) {
		OMX_VdecGiveBackAllPackets(pVdecDataType);
		OMX_VdecGiveBackAllFramesToDecoder(pVdecDataType);
	} else if (pVdecDataType->state == OMX_StateExecuting) {

	} else if (pVdecDataType->state == OMX_StatePause) {

	} else  {
		OMX_VdecEventNotify(pVdecDataType
							,OMX_EventError
							,OMX_ErrorIncorrectStateTransition
							,pVdecDataType->state,NULL);
		loge("OMX_ErrorIncorrectStateTransition\n");
		return;
	}
	pVdecDataType->state = OMX_StateLoaded;
	OMX_VdecEventNotify(pVdecDataType
						,OMX_EventCmdComplete
						,OMX_CommandStateSet
						,pVdecDataType->state,NULL);

}

static void OMX_VdecStateChangeToExecuting(VDEC_DATA_TYPE * pVdecDataType) {
	if (pVdecDataType->state == OMX_StateLoaded) {
		OMX_VdecEventNotify(pVdecDataType
					,OMX_EventError
					,OMX_ErrorIncorrectStateTransition
					,pVdecDataType->state,NULL);
		loge("OMX_ErrorIncorrectStateTransition\n");
		return;
	} else if (pVdecDataType->state == OMX_StateIdle) {

	} else if (pVdecDataType->state == OMX_StatePause) {

	} else {
		OMX_VdecEventNotify(pVdecDataType
					,OMX_EventError
					,OMX_ErrorIncorrectStateTransition
					,pVdecDataType->state,NULL);
		loge("OMX_ErrorIncorrectStateTransition\n");
		return;
	}
	pVdecDataType->state = OMX_StateExecuting;
	OMX_VdecEventNotify(pVdecDataType
				,OMX_EventCmdComplete
				,OMX_CommandStateSet
				,pVdecDataType->state,NULL);

}

static void* OMX_VdecComponentThread(void* pThreadData)
{
	struct aic_message message;
	OMX_S32 nCmd;		//OMX_COMMANDTYPE
	OMX_S32 nCmdData;	//OMX_STATETYPE
	VDEC_DATA_TYPE* pVdecDataType = (VDEC_DATA_TYPE*)pThreadData;
	OMX_S32 ret;
	OMX_S32 dec_ret;
	OMX_S32 bNotifyFrameEnd = 0;
	VDEC_OUT_FRAME *pFrameNode;
	struct mpp_frame   sFrame;
	//prctl(PR_SET_NAME,(u32)"Vdec");

	while(1) {
_AIC_MSG_GET_:
		if (aic_msg_get(&pVdecDataType->sMsgQue, &message) == 0) {
			nCmd = message.message_id;
			nCmdData = message.param;
			logi("nCmd:%d, nCmdData:%d\n",nCmd,nCmdData);
			if (OMX_CommandStateSet == nCmd) {
				aic_pthread_mutex_lock(&pVdecDataType->stateLock);
				if (pVdecDataType->state == (OMX_STATETYPE)(nCmdData)) {
					OMX_VdecEventNotify(pVdecDataType,OMX_EventError,OMX_ErrorSameState,0,NULL);
					aic_pthread_mutex_unlock(&pVdecDataType->stateLock);
					continue;
				}
				switch(nCmdData) {
					case OMX_StateInvalid:
						OMX_VdecStateChangeToInvalid(pVdecDataType);
						break;
					case OMX_StateLoaded:
						OMX_VdecStateChangeToLoaded(pVdecDataType);
						break;
					case OMX_StateIdle:
						OMX_VdecStateChangeToIdle(pVdecDataType);
						break;
					case OMX_StateExecuting:
						OMX_VdecStateChangeToExecuting(pVdecDataType);
						break;
					case OMX_StatePause:
						OMX_VdecStateChangeToPause(pVdecDataType);
						break;
					default:
						break;
				}
				aic_pthread_mutex_unlock(&pVdecDataType->stateLock);
			} else if (OMX_CommandFlush == nCmd) {

			} else if (OMX_CommandPortDisable == nCmd) {

			} else if (OMX_CommandPortEnable == nCmd) {

			} else if (OMX_CommandMarkBuffer == nCmd) {

			} else if (OMX_CommandStop == nCmd) {
				logw("OMX_VdecComponentThread ready to exit!!!\n");
				goto _EXIT;
			} else {

			}
		}

		if (pVdecDataType->state != OMX_StateExecuting) {
			//usleep(1000);
			aic_msg_wait_new_msg(&pVdecDataType->sMsgQue, 0);
			continue;
		}

		if (pVdecDataType->nFlags & VDEC_OUTPORT_SEND_ALL_FRAME_FLAG) {//(pVdecDataType->nFlags & VDEC_OUTPORT_SEND_ALL_FRAME_END_FLAG)
			if (!bNotifyFrameEnd) {
				//notify app decoder end
				OMX_VdecEventNotify(pVdecDataType,OMX_EventBufferFlag,0,0,NULL);
				/*
				//notify tunneld component decoder end
				if (pVdecDataType->sOutPortTunneledInfo.nTunneledFlag) {
					OMX_PARAM_FRAMEEND sFrameEnd;
					sFrameEnd.bFrameEnd = OMX_TRUE;
					OMX_SetParameter(pVdecDataType->sOutPortTunneledInfo.pTunneledComp, OMX_IndexVendorStreamFrameEnd,&sFrameEnd);
				}
				*/
				bNotifyFrameEnd = 1;

			}
			//usleep(1000);
			aic_msg_wait_new_msg(&pVdecDataType->sMsgQue, 0);
			continue;
		}
		bNotifyFrameEnd = 0;

		//give back packet to demux
		if ((pVdecDataType->sInPortTunneledInfo.nTunneledFlag)
			&& (!OMX_VdecListEmpty(&pVdecDataType->sInProcessedPkt,pVdecDataType->sInPktLock))) {
			VDEC_IN_PACKET *pktNode1 = NULL;
			while(!OMX_VdecListEmpty(&pVdecDataType->sInProcessedPkt,pVdecDataType->sInPktLock)) {
				aic_pthread_mutex_lock(&pVdecDataType->sInPktLock);
				pktNode1 = mpp_list_first_entry(&pVdecDataType->sInProcessedPkt, VDEC_IN_PACKET, sList);
				aic_pthread_mutex_unlock(&pVdecDataType->sInPktLock);
				ret = OMX_FillThisBuffer(pVdecDataType->sInPortTunneledInfo.pTunneledComp,&pktNode1->sBuff);
				if (ret == 0) {
					aic_pthread_mutex_lock(&pVdecDataType->sInPktLock);
					mpp_list_del(&pktNode1->sList);
					mpp_list_add_tail(&pktNode1->sList, &pVdecDataType->sInEmptyPkt);
					pVdecDataType->nGiveBackPacktOkNum++;
					logd("pVdecDataType->nGiveBackPacktOkNum:%d,pVdecDataType->nGiveBackPacktFailNum:%d\n"
						,pVdecDataType->nGiveBackPacktOkNum
						,pVdecDataType->nGiveBackPacktFailNum);
					aic_pthread_mutex_unlock(&pVdecDataType->sInPktLock);
				} else {
					logi("OMX_FillThisBuffer error \n");
					pVdecDataType->nGiveBackPacktFailNum++;
					logd("pVdecDataType->nGiveBackPacktOkNum:%d,pVdecDataType->nGiveBackPacktFailNum:%d\n"
						,pVdecDataType->nGiveBackPacktOkNum
						,pVdecDataType->nGiveBackPacktFailNum);
					break;
				}
			}
		}

        //decode
        aic_pthread_mutex_lock(&pVdecDataType->sInPktLock);
        aic_pthread_mutex_lock(&pVdecDataType->sOutFrameLock);
        dec_ret = mpp_decoder_decode(pVdecDataType->pDecoder);
        if (dec_ret == DEC_OK) {
            logd("mpp_decoder_decode ok!!!\n");
        } else if (dec_ret == DEC_NO_READY_PACKET) {
            pVdecDataType->nWaitForReadyPkt = 1;
        } else if (dec_ret == DEC_NO_EMPTY_FRAME) {
            pVdecDataType->nWaitForEmptyFrame = 1;
        } else if (dec_ret == DEC_NO_RENDER_FRAME) {
            loge("mpp_decoder_decode ret:%d !!!\n",dec_ret);
        } else {
            //ASSERT();
            loge("mpp_decoder_decode error serious,do not keep decoding ret:%d !!!\n",dec_ret);
            OMX_VdecEventNotify(pVdecDataType,OMX_EventError,OMX_ErrorMbErrorsInFrame,0,NULL);
            pVdecDataType->nFlags |= VDEC_OUTPORT_SEND_ALL_FRAME_FLAG;
            goto _AIC_MSG_GET_;
        }
		aic_pthread_mutex_unlock(&pVdecDataType->sOutFrameLock);
        aic_pthread_mutex_unlock(&pVdecDataType->sInPktLock);



		// get frame from decoder
		do {
			OMX_S32 result = 0;
			OMX_BUFFERHEADERTYPE sBuffHead;

			ret = mpp_decoder_get_frame(pVdecDataType->pDecoder, &sFrame);
			if (ret != DEC_OK) {
				logd("mpp_decoder_get_frame other error ret:%d \n",ret);
				break;
			}
			pVdecDataType->nGetFrameFromDecoderNum++;
			if (OMX_VdecListEmpty(&pVdecDataType->sOutEmptyFrame,pVdecDataType->sOutFrameLock)) {
				VDEC_OUT_FRAME *pFrameNode = (VDEC_OUT_FRAME*)mpp_alloc(sizeof(VDEC_OUT_FRAME));
				if (NULL == pFrameNode) {
					loge("mpp_alloc error \n");
					mpp_decoder_put_frame(pVdecDataType->pDecoder, &sFrame);
					goto _AIC_MSG_GET_;
				}
				memset(pFrameNode,0x00,sizeof(VDEC_OUT_FRAME));
				aic_pthread_mutex_lock(&pVdecDataType->sOutFrameLock);
				mpp_list_add_tail(&pFrameNode->sList, &pVdecDataType->sOutEmptyFrame);
				aic_pthread_mutex_unlock(&pVdecDataType->sOutFrameLock);
				pVdecDataType->nOutFrameNodeNum++;
			}
			sBuffHead.nOutputPortIndex = VDEC_PORT_OUT_INDEX;
			sBuffHead.pBuffer = (OMX_U8 *)&sFrame;
			if (pVdecDataType->sOutPortTunneledInfo.nTunneledFlag) {
				sBuffHead.nInputPortIndex = pVdecDataType->sOutPortTunneledInfo.nTunnelPortIndex;
				result = OMX_EmptyThisBuffer(pVdecDataType->sOutPortTunneledInfo.pTunneledComp,&sBuffHead);
			} else {
				if (pVdecDataType->pCallbacks != NULL && pVdecDataType->pCallbacks->FillBufferDone != NULL) {
					result = pVdecDataType->pCallbacks->FillBufferDone(pVdecDataType->hSelf,pVdecDataType->pAppData,&sBuffHead);
				}
			}
			if (result == 0) {
				aic_pthread_mutex_lock(&pVdecDataType->sOutFrameLock);
				pFrameNode = mpp_list_first_entry(&pVdecDataType->sOutEmptyFrame, VDEC_OUT_FRAME, sList);
				pFrameNode->sFrameInfo = sFrame;
				mpp_list_del(&pFrameNode->sList);
				mpp_list_add_tail(&pFrameNode->sList, &pVdecDataType->sOutProcessingFrame);
				aic_pthread_mutex_unlock(&pVdecDataType->sOutFrameLock);
				pVdecDataType->nSendFrameOkNum++;
				if (pFrameNode->sFrameInfo.flags & FRAME_FLAG_EOS) {
					printf("[%s:%d] nFrameEndFlag",__FUNCTION__,__LINE__);
					pVdecDataType->nFlags |= VDEC_OUTPORT_SEND_ALL_FRAME_FLAG;
					if (pVdecDataType->pCallbacks && pVdecDataType->pCallbacks->EventHandler)  {
						pVdecDataType->pCallbacks->EventHandler(pVdecDataType->hSelf,pVdecDataType->pAppData,OMX_EventBufferFlag,0, 0,NULL);
					}
				}
				//loge("pVdecDataType->nGetFrameFromDecoderNum:%d\n",pVdecDataType->nGetFrameFromDecoderNum);
				//loge("pVdecDataType->nSendFrameOkNum:%d\n",pVdecDataType->nSendFrameOkNum);
			} else {
				//this may drop last frame,so it must deal with this case
				result = mpp_decoder_put_frame(pVdecDataType->pDecoder, &sFrame);
				if (sFrame.flags & FRAME_FLAG_EOS) {
					printf("[%s:%d]frame end\n",__FUNCTION__,__LINE__);
					pVdecDataType->nFlags |= VDEC_OUTPORT_SEND_ALL_FRAME_FLAG;
					if (pVdecDataType->pCallbacks && pVdecDataType->pCallbacks->EventHandler)  {
						pVdecDataType->pCallbacks->EventHandler(pVdecDataType->hSelf,pVdecDataType->pAppData,OMX_EventBufferFlag,0, 0,NULL);
					}
				}
				if (result != 0) {// how to do
					loge("mpp_decoder_put_frame error!!!!\n");
					//ASSERT();
				}
				logw("OMX_EmptyThisBuffer or FillBufferDone fail!\n");
				pVdecDataType->nSendFrameErrorNum++;
			}
		} while(1);

        if (dec_ret == DEC_NO_READY_PACKET) {
            aic_pthread_mutex_lock(&pVdecDataType->sInPktLock);
            if (!(pVdecDataType->nFlags & VDEC_INPORT_STREAM_END_FLAG)) {
                aic_pthread_mutex_unlock(&pVdecDataType->sInPktLock);
                aic_msg_wait_new_msg(&pVdecDataType->sMsgQue,0);
            } else {
                aic_pthread_mutex_unlock(&pVdecDataType->sInPktLock);
                aic_msg_wait_new_msg(&pVdecDataType->sMsgQue, 5*1000);
            }
        } else if (dec_ret == DEC_NO_EMPTY_FRAME) {
            if (!(pVdecDataType->nFlags & VDEC_OUTPORT_SEND_ALL_FRAME_FLAG)) {
                aic_msg_wait_new_msg(&pVdecDataType->sMsgQue, 0);
            }
        }
		// send frame back to decoder in OMX_VdecFillThisBuffer
	}

_EXIT:
	logd("in port:nReceivePacktOkNum:%d,"\
		"nReceivePacktFailNum:%d,"\
		"nPutPacktToDecoderOkNum:%d,"\
		"nPutPacktToDecoderFailNum:%d,"\
		"nGiveBackPacktOkNum:%d"\
		"nGiveBackPacktFailNum:%d\n"
		,pVdecDataType->nReceivePacktOkNum
		,pVdecDataType->nReceivePacktFailNum
		,pVdecDataType->nPutPacktToDecoderOkNum
		,pVdecDataType->nPutPacktToDecoderFailNum
		,pVdecDataType->nGiveBackPacktOkNum
		,pVdecDataType->nGiveBackPacktFailNum);

	logd("out port:nGetFrameFromDecoderNum:%d,"\
		"nDropFrameFromDecoderNum:%d,"\
		"nSendFrameOkNum:%d,"\
		"nSendFrameErrorNum:%d,"\
		"nLeftReadyFrameWhenCompoentExitNum:%d\n"
		,pVdecDataType->nGetFrameFromDecoderNum
		,pVdecDataType->nDropFrameFromDecoderNum
		,pVdecDataType->nSendFrameOkNum
		,pVdecDataType->nSendFrameErrorNum
		,pVdecDataType->nLeftReadyFrameWhenCompoentExitNum);
	logd("OMX_VdecComponentThread EXIT\n");
	return (void*)OMX_ErrorNone;
}

