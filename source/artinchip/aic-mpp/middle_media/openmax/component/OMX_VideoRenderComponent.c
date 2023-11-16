/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: OMX_VideoRenderComponent
*/

#include "OMX_VideoRenderComponent.h"

#define  aic_pthread_mutex_lock(mutex)\
{\
	pthread_mutex_lock(mutex);\
}

#define aic_pthread_mutex_unlock(mutex)\
{\
	pthread_mutex_unlock(mutex);\
}

#define OMX_VideoRenderListEmpty(list,mutex)\
({\
	int ret = 0;\
	aic_pthread_mutex_lock(&mutex);\
	ret = mpp_list_empty(list);\
	aic_pthread_mutex_unlock(&mutex);\
	(ret);\
})

#define OMX_VIDEO_RENDER_PRINT_FRAME_NUM (30)

static OMX_S64 OMX_ClockGetSystemTime();

static OMX_ERRORTYPE OMX_VideoRenderSendCommand(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_COMMANDTYPE Cmd,
		OMX_IN	OMX_U32 nParam1,
		OMX_IN	OMX_PTR pCmdData);

static OMX_ERRORTYPE OMX_VideoRenderGetParameter(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nParamIndex,
		OMX_INOUT OMX_PTR pComponentParameterStructure);

static OMX_ERRORTYPE OMX_VideoRenderSetParameter(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_IN	OMX_PTR pComponentParameterStructure);

static OMX_ERRORTYPE OMX_VideoRenderGetConfig(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_INOUT OMX_PTR pComponentConfigStructure);

static OMX_ERRORTYPE OMX_VideoRenderSetConfig(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_IN	OMX_PTR pComponentConfigStructure);

static OMX_ERRORTYPE OMX_VideoRenderGetState(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_OUT OMX_STATETYPE* pState);

static OMX_ERRORTYPE OMX_VideoRenderComponentTunnelRequest(
	OMX_IN	OMX_HANDLETYPE hComp,
	OMX_IN	OMX_U32 nPort,
	OMX_IN	OMX_HANDLETYPE hTunneledComp,
	OMX_IN	OMX_U32 nTunneledPort,
	OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup);

static OMX_ERRORTYPE OMX_VideoRenderEmptyThisBuffer(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_BUFFERHEADERTYPE* pBuffer);

static OMX_ERRORTYPE OMX_VideoRenderFillThisBuffer(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_BUFFERHEADERTYPE* pBuffer);

static OMX_ERRORTYPE OMX_VideoRenderSetCallbacks(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_CALLBACKTYPE* pCallbacks,
		OMX_IN	OMX_PTR pAppData);

static OMX_ERRORTYPE OMX_VideoRenderCapture(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_IN	OMX_PTR pComponentConfigStructure);

static int OMX_VideoRenderEncodeJpgPic(struct mpp_frame *frame,OMX_PARAM_VIDEO_CAPTURE *pCapture);

static int OMX_VideoRenderScalePic(struct mpp_frame* pSrcFrame, struct mpp_frame* pDstFrame,OMX_PARAM_VIDEO_CAPTURE *pCapture);

static int OMX_VideoRenderAllocDMABuffer(struct mpp_frame* pSrcFrame,OMX_PARAM_VIDEO_CAPTURE *pCapture);

static int OMX_VideoRenderFreeDMABuffer(struct mpp_frame* pFrame,int nDMAFd);

static int  OMX_VideoRenderGiveBackAllFrames(VIDEO_RENDER_DATA_TYPE *pVideoRenderDataType);

static OMX_ERRORTYPE OMX_VideoRenderCapture(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_IN	OMX_PTR pComponentConfigStructure)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	VIDEO_RENDER_DATA_TYPE *pVideoRenderDataType;
	OMX_PARAM_VIDEO_CAPTURE * pCapture = (OMX_PARAM_VIDEO_CAPTURE *)pComponentConfigStructure;
	VIDEO_RENDER_IN_FRAME *pFrameNode;
	pVideoRenderDataType = (VIDEO_RENDER_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	if (pVideoRenderDataType->state != OMX_StatePause) {
		return OMX_ErrorIncorrectStateOperation;
	}
	if (!OMX_VideoRenderListEmpty(&pVideoRenderDataType->sInProcessedFrmae,pVideoRenderDataType->sInFrameLock)) {
		int nDMAFd;
		struct mpp_frame dstFrame;

		memset(&dstFrame,0x00,sizeof(struct mpp_frame));
		logd("pCapture->nWidth:%d,pCapture->nHeight:%d,pCapture->pFilePath:%s\n",pCapture->nWidth,pCapture->nHeight,pCapture->pFilePath);
		pFrameNode = mpp_list_first_entry(&pVideoRenderDataType->sInProcessedFrmae, VIDEO_RENDER_IN_FRAME, sList);

		nDMAFd = OMX_VideoRenderAllocDMABuffer(&dstFrame,pCapture);

		if (nDMAFd <= 0) {
			loge("OMX_VideoRenderAllocDMABuffer fail \n");
			return OMX_ErrorUndefined;
		}

		if (OMX_VideoRenderScalePic(&pFrameNode->sFrameInfo,&dstFrame,pCapture) != 0) {
			loge("OMX_VideoRenderScalePic fail \n");
			OMX_VideoRenderFreeDMABuffer(&dstFrame,nDMAFd);
			return OMX_ErrorUndefined;
		}

		if (OMX_VideoRenderEncodeJpgPic(&dstFrame,pCapture) != 0) {
			loge("OMX_VideoRenderEncodeJpgPic fail \n");
			eError = OMX_ErrorUndefined;
		}

		OMX_VideoRenderFreeDMABuffer(&dstFrame,nDMAFd);

	} else {
		return OMX_ErrorUndefined;
	}

	return eError;
}

static int OMX_VideoRenderFreeDMABuffer(struct mpp_frame* pFrame,int nDMAFd)
{
	if (pFrame == NULL || nDMAFd<= 0) {
		return -1;
	}
	if (pFrame->buf.fd[0] > 0) {
		dmabuf_free(pFrame->buf.fd[0]);
	}
	if (pFrame->buf.fd[0] > 0) {
		dmabuf_free(pFrame->buf.fd[0]);
	}
	if (pFrame->buf.fd[0] > 0) {
		dmabuf_free(pFrame->buf.fd[0]);
	}
	dmabuf_device_close(nDMAFd);
	return 0;
}

static int OMX_VideoRenderAllocDMABuffer(struct mpp_frame* pFrame,OMX_PARAM_VIDEO_CAPTURE *pCapture)
{
	int ret = 0;
	int dma_fd;
	if (pFrame == NULL || pCapture == NULL) {
		return -1;
	}

	dma_fd = dmabuf_device_open();
	if (dma_fd < 0) {
		loge("dmabuf_device_open error dma_fd:%d \n",dma_fd);
		return -1;
	}

	pFrame->buf.fd[0] = dmabuf_alloc(dma_fd, pCapture->nWidth*pCapture->nHeight);
	if (pFrame->buf.fd[0] <= 0) {
		loge("pDstFrame->buf.fd[0] alloc fail!!!\n");
		ret = -1;
		goto _exit0;
	}

	pFrame->buf.fd[1] = dmabuf_alloc(dma_fd, pCapture->nWidth*pCapture->nHeight/4);
	if (pFrame->buf.fd[1] <= 0) {
		loge("pDstFrame->buf.fd[1] alloc fail!!!\n");
		ret = -1;
		goto _exit1;
	}

	pFrame->buf.fd[2] = dmabuf_alloc(dma_fd, pCapture->nWidth*pCapture->nHeight/4);
		if (pFrame->buf.fd[2] <= 0) {
		loge("pDstFrame->buf.fd[2] alloc fail!!!\n");
		ret = -1;
		goto _exit2;
	}

	return dma_fd;

_exit2:
	if (pFrame->buf.fd[1] > 0)
		dmabuf_free(pFrame->buf.fd[1]);
_exit1:
	if (pFrame->buf.fd[0] > 0)
		dmabuf_free(pFrame->buf.fd[0]);
_exit0:
	if (dma_fd > 0)
		dmabuf_device_close(dma_fd);

	return ret;
}



#define BYTE_ALIGN(x, byte) (((x) + ((byte) - 1))&(~((byte) - 1)))
static int OMX_VideoRenderScalePic(struct mpp_frame* pSrcFrame, struct mpp_frame* pDstFrame,OMX_PARAM_VIDEO_CAPTURE *pCapture)
{
	int ret = 0;
	struct ge_bitblt blt;
	struct mpp_ge *ge = NULL;

	if (pSrcFrame == NULL || pDstFrame == NULL) {
		loge("param error !!!\n");
		return -1;
	}

	ge = mpp_ge_open();
	if (!ge)  {
		loge("open ge device\n");
		return -1;
	}

	memset(&blt,0x00,sizeof(struct ge_bitblt));
	blt.src_buf.buf_type = MPP_DMA_BUF_FD;
	blt.src_buf.format = pSrcFrame->buf.format;
	blt.src_buf.fd[0] = pSrcFrame->buf.fd[0];
	blt.src_buf.fd[1] = pSrcFrame->buf.fd[1];
	blt.src_buf.fd[2] = pSrcFrame->buf.fd[2];
	blt.src_buf.stride[0] = pSrcFrame->buf.stride[0];
	blt.src_buf.stride[1] =  pSrcFrame->buf.stride[1];
	blt.src_buf.stride[2] =  pSrcFrame->buf.stride[2];
	blt.src_buf.size.width = pSrcFrame->buf.size.width;
	blt.src_buf.size.height = pSrcFrame->buf.size.height;
	blt.src_buf.crop_en = 0;

	blt.dst_buf.buf_type = pDstFrame->buf.buf_type= MPP_DMA_BUF_FD;
	blt.dst_buf.format = pDstFrame->buf.format= MPP_FMT_YUV420P;
	blt.dst_buf.fd[0] = pDstFrame->buf.fd[0];
	blt.dst_buf.fd[1] = pDstFrame->buf.fd[1];
	blt.dst_buf.fd[2] = pDstFrame->buf.fd[2];
	blt.dst_buf.stride[0] =pDstFrame->buf.stride[0] = pCapture->nWidth;
	blt.dst_buf.stride[1] = pDstFrame->buf.stride[1] = pCapture->nWidth/2;
	blt.dst_buf.stride[2] = pDstFrame->buf.stride[2] = pCapture->nWidth/2;
	blt.dst_buf.size.width = pDstFrame->buf.size.width = pCapture->nWidth;
	blt.dst_buf.size.height = pDstFrame->buf.size.height = pCapture->nHeight;
	blt.dst_buf.crop_en = 0;

	if (pSrcFrame->buf.fd[0] > 0)
		mpp_ge_add_dmabuf(ge, pSrcFrame->buf.fd[0]);
	if (pSrcFrame->buf.fd[1] > 0)
		mpp_ge_add_dmabuf(ge, pSrcFrame->buf.fd[1]);
	if (pSrcFrame->buf.fd[2] > 0)
		mpp_ge_add_dmabuf(ge, pSrcFrame->buf.fd[2]);

	if (pDstFrame->buf.fd[0] > 0)
		mpp_ge_add_dmabuf(ge, pDstFrame->buf.fd[0]);
	if (pDstFrame->buf.fd[1] > 0)
		mpp_ge_add_dmabuf(ge, pDstFrame->buf.fd[1]);
	if (pDstFrame->buf.fd[2] > 0)
		mpp_ge_add_dmabuf(ge, pDstFrame->buf.fd[2]);

	ret = mpp_ge_bitblt(ge, &blt);
	if (ret)  {
		loge("mpp_ge_bitblt task failed: %d\n", ret);
		ret = -1;
		goto _exit0;
	}

	ret = mpp_ge_emit(ge);
	if (ret)  {
		loge("mpp_ge_emit task failed: %d\n", ret);
		ret = -1;
		goto _exit0;
	}
	ret = mpp_ge_sync(ge);
	if (ret)  {
		loge("mpp_ge_sync task failed: %d\n", ret);
		ret = -1;
		goto _exit0;
	}

_exit0:
	if (pSrcFrame->buf.fd[0] > 0)
		mpp_ge_rm_dmabuf(ge, pSrcFrame->buf.fd[0]);

	if (pSrcFrame->buf.fd[1] > 0)
		mpp_ge_rm_dmabuf(ge, pSrcFrame->buf.fd[1]);

	if (pSrcFrame->buf.fd[2] > 0)
		mpp_ge_rm_dmabuf(ge, pSrcFrame->buf.fd[2]);

	if (pDstFrame->buf.fd[0] > 0)
		mpp_ge_rm_dmabuf(ge, pDstFrame->buf.fd[0]);

	if (pDstFrame->buf.fd[1] > 0)
		mpp_ge_rm_dmabuf(ge, pDstFrame->buf.fd[1]);

	if (pDstFrame->buf.fd[2] > 0)
		mpp_ge_rm_dmabuf(ge, pDstFrame->buf.fd[2]);

	if (ge)
		mpp_ge_close(ge);

	return ret;
}

static int OMX_VideoRenderEncodeJpgPic(struct mpp_frame* pFrame,OMX_PARAM_VIDEO_CAPTURE *pCapture)
{
	int ret = 0;
	int quality = 90;
	int len,buf_len;
	int width,height;
	int jpeg_data_fd,dma_fd;
	FILE* fp_save = NULL;
	unsigned char* jpeg_vir_addr = NULL;

	dma_fd = dmabuf_device_open();
	if (dma_fd < 0) {
		loge("dmabuf_device_open error dma_fd:%d \n",dma_fd);
		return -1;
	}

	fp_save = fopen((char *)pCapture->pFilePath, "wb");
	if (fp_save  == NULL) {
		loge("fopen %s error\n",pCapture->pFilePath);
		ret = -1;
		goto _exit0;
	}

	quality = pCapture->nQuality;
	width = pFrame->buf.size.width;
	height = pFrame->buf.size.height;
	buf_len = width * height * 4/5 * quality / 100;

	jpeg_data_fd = dmabuf_alloc(dma_fd, buf_len);
	if (jpeg_data_fd < 0) {
		loge("dmabuf_device_open error jpeg_data_fd:%d \n",jpeg_data_fd);
		ret = -1;
		goto _exit1;
	}

	jpeg_vir_addr = dmabuf_mmap(jpeg_data_fd, buf_len);
	if (!jpeg_vir_addr) {
		loge("dmabuf_mmap error\n");
		ret = -1;
		goto _exit2;
	}

	if (mpp_encode_jpeg(pFrame, quality, jpeg_data_fd, buf_len, &len) < 0)  {
		loge("encode failed");
		ret = -1;
		goto _exit3;
	}

	fwrite(jpeg_vir_addr, 1, len, fp_save);

_exit3:
	if (jpeg_vir_addr)
		dmabuf_munmap(jpeg_vir_addr, buf_len);
_exit2:
	if (jpeg_data_fd > 0)
		dmabuf_free(jpeg_data_fd);
_exit1:
	if (fp_save)
		fclose(fp_save);
_exit0:
	if (dma_fd > 0)
		dmabuf_device_close(dma_fd);

	return ret;
}

static s32 OMX_VideoRenderGetComponentNum(enum mpp_pixel_format format)
{
	int component_num = 0;
		if (format == MPP_FMT_ARGB_8888) {
			component_num = 1;
		} else if (format == MPP_FMT_RGBA_8888) {
			component_num = 1;
		} else if (format == MPP_FMT_RGB_888) {
			component_num = 1;
		} else if (format == MPP_FMT_YUV420P) {
			component_num = 3;
		} else if (format == MPP_FMT_NV12 || format == MPP_FMT_NV21) {
			component_num = 2;
		} else if (format == MPP_FMT_YUV444P) {
			component_num = 3;
		} else if (format == MPP_FMT_YUV422P) {
			component_num = 3;
		} else if (format == MPP_FMT_YUV400) {
			component_num = 1;
		} else {
			loge("no support picture foramt %d, default argb8888", format);
		}
		return component_num;
}

static int OMX_VideoRenderIsExistDMAFd(VIDEO_RENDER_DATA_TYPE * pVideoRenderDataType,struct mpp_frame* pFrame)
{
	int comp= 0;
	int find = 0;
	if (mpp_list_empty(&pVideoRenderDataType->sDMAFd)) {
		return find;
	}
	comp = OMX_VideoRenderGetComponentNum(pFrame->buf.format);
	if (!mpp_list_empty(&pVideoRenderDataType->sDMAFd)) {
		VIDEO_RENDER_DMA_FD *dma_fd_node = NULL,*dma_fd_node1 = NULL;
		mpp_list_for_each_entry_safe(dma_fd_node,dma_fd_node1, &pVideoRenderDataType->sDMAFd, sList) {
			if (comp == 1) {
				if(dma_fd_node->fd[0] == pFrame->buf.fd[0]) {
					find = 1;
					break;
				}
			} else if (comp == 2) {
				if(dma_fd_node->fd[0] == pFrame->buf.fd[0]
					&& dma_fd_node->fd[1] == pFrame->buf.fd[1]) {
					find = 1;
					break;
				}
			} else if (comp == 3) {
				if(dma_fd_node->fd[0] == pFrame->buf.fd[0]
					&& dma_fd_node->fd[1] == pFrame->buf.fd[1]
					&& dma_fd_node->fd[2] == pFrame->buf.fd[2]) {
					find = 1;
					break;
				}
			}
		}
	}
	return find;
}

static int OMX_VideoRenderAddDMAFd(VIDEO_RENDER_DATA_TYPE * pVideoRenderDataType,struct mpp_frame* pFrame)
{
	int comp = 0,i=0;
	VIDEO_RENDER_DMA_FD *dma_fd_node = NULL;
	comp = OMX_VideoRenderGetComponentNum(pFrame->buf.format);
	dma_fd_node = (VIDEO_RENDER_DMA_FD *)mpp_alloc(sizeof(VIDEO_RENDER_DMA_FD));
	memset(dma_fd_node,0x00,sizeof(VIDEO_RENDER_DMA_FD));
	for (i = 0; i < comp; i++) {
		dma_fd_node->fd[i] = pFrame->buf.fd[i];
	}
	mpp_list_add_tail(&dma_fd_node->sList, &pVideoRenderDataType->sDMAFd);
	return 0;
}

static int OMX_VideoRenderRemoveDMAFd(VIDEO_RENDER_DATA_TYPE * pVideoRenderDataType,struct mpp_frame* pFrame)
{
	int comp= 0;
	int find = -1;
	if (mpp_list_empty(&pVideoRenderDataType->sDMAFd)) {
		return find;
	}
	comp = OMX_VideoRenderGetComponentNum(pFrame->buf.format);
	if (!mpp_list_empty(&pVideoRenderDataType->sDMAFd)) {
		VIDEO_RENDER_DMA_FD *dma_fd_node = NULL,*dma_fd_node1 = NULL;
		mpp_list_for_each_entry_safe(dma_fd_node,dma_fd_node1, &pVideoRenderDataType->sDMAFd, sList) {
			if (comp == 1) {
				if (dma_fd_node->fd[0] == pFrame->buf.fd[0]) {
					mpp_list_del(&dma_fd_node->sList);
					mpp_free(dma_fd_node);
					find = 0;
					break;
				}
			} else if (comp == 2) {
				if (dma_fd_node->fd[0] == pFrame->buf.fd[0]
					&& dma_fd_node->fd[1] == pFrame->buf.fd[1]) {
					mpp_list_del(&dma_fd_node->sList);
					mpp_free(dma_fd_node);
					find = 0;
					break;
				}
			} else if (comp == 3) {
				if (dma_fd_node->fd[0] == pFrame->buf.fd[0]
					&& dma_fd_node->fd[1] == pFrame->buf.fd[1]
					&& dma_fd_node->fd[2] == pFrame->buf.fd[2]) {
					mpp_list_del(&dma_fd_node->sList);
					mpp_free(dma_fd_node);
					find = 0;
					break;
				}
			}
		}
	}
	return find;
}



static int OMX_VideoRenderAllocFrameBuffer(VIDEO_RENDER_DATA_TYPE * pVideoRenderDataType,struct mpp_frame* pFrame)
{
	int height;
	int dma_fd;
	int buf_size;
	if (pFrame == NULL) {
		return -1;
	}
	height = pFrame->buf.size.height;
	buf_size = height * pFrame->buf.stride[0];
	dma_fd = pVideoRenderDataType->nDMAFd;
	if (dma_fd < 0) {
		return -1;
	}
	pFrame->buf.fd[0] = -1;
	pFrame->buf.fd[1] = -1;
	pFrame->buf.fd[2] = -1;

	switch(pFrame->buf.format) {
	case MPP_FMT_YUV420P:
		pFrame->buf.fd[0] = dmabuf_alloc(dma_fd,buf_size);
		pFrame->buf.fd[1] = dmabuf_alloc(dma_fd,(buf_size >> 2));
		pFrame->buf.fd[2] = dmabuf_alloc(dma_fd,(buf_size >> 2));
		break;
	case MPP_FMT_YUV444P:
		pFrame->buf.fd[0] = dmabuf_alloc(dma_fd,buf_size);
		pFrame->buf.fd[1] = dmabuf_alloc(dma_fd,buf_size);
		pFrame->buf.fd[2] = dmabuf_alloc(dma_fd,buf_size);
		break;
	case MPP_FMT_YUV422P:
		pFrame->buf.fd[0] = dmabuf_alloc(dma_fd,buf_size);
		pFrame->buf.fd[1] = dmabuf_alloc(dma_fd,(buf_size >> 1));
		pFrame->buf.fd[2] = dmabuf_alloc(dma_fd,(buf_size >> 1));
		break;
	case MPP_FMT_NV12:
	case MPP_FMT_NV21:
		pFrame->buf.fd[0] = dmabuf_alloc(dma_fd,buf_size);
		pFrame->buf.fd[1] = dmabuf_alloc(dma_fd,(buf_size >> 1));
		pFrame->buf.stride[1] = pFrame->buf.stride[0]>>1;
		pFrame->buf.stride[2] = -1;
		break;
	case MPP_FMT_YUV400:
	case MPP_FMT_ABGR_8888:
	case MPP_FMT_ARGB_8888:
	case MPP_FMT_RGBA_8888:
	case MPP_FMT_BGRA_8888:
	case MPP_FMT_BGR_888:
	case MPP_FMT_RGB_888:
	case MPP_FMT_BGR_565:
	case MPP_FMT_RGB_565:
		loge("unsupport format");
		return -1;
	default:
		break;
	}
	return 0;
}

static int OMX_VideoRenderFreeFrameBuffer(VIDEO_RENDER_DATA_TYPE * pVideoRenderDataType,struct mpp_frame* pFrame)
{
	int i = 0;
	int comp = 0;

	if (!pVideoRenderDataType || !pFrame) {
		return -1;
	}

	comp = OMX_VideoRenderGetComponentNum(pFrame->buf.format);

	// remove dmafd in the DE
	//aic_video_render_remove_dma_fd(pVideoRenderDataType->render,pFrame->buf.fd,comp);

	// remove dmafd in the GE
	if (OMX_VideoRenderIsExistDMAFd(pVideoRenderDataType,pFrame)) {
		OMX_VideoRenderRemoveDMAFd(pVideoRenderDataType,pFrame);
		for (i = 0;i < comp; i++) {
			mpp_ge_rm_dmabuf(pVideoRenderDataType->sGeHandle,pFrame->buf.fd[i]);
			printf("[%s:%d]:%d\n",__FUNCTION__,__LINE__,pFrame->buf.fd[i]);
		}
	}

	for (i = 0; i < comp;i++) {
		if (pFrame->buf.fd[i] > 0) {
			dmabuf_free(pFrame->buf.fd[i]);
			pFrame->buf.fd[i] = -1;
		}
	}

	return 0;
}


static int OMX_VideoRenderSetRotationFrameInfo(VIDEO_RENDER_DATA_TYPE * pVideoRenderDataType,struct mpp_frame* pFrame)
{
	int i = 0;
	int cnt;

	cnt = sizeof(pVideoRenderDataType->sRotationFrames)/sizeof(pVideoRenderDataType->sRotationFrames[0]);

	for (i = 0; i < cnt; i++) {
		pVideoRenderDataType->sRotationFrames[i].id = pVideoRenderDataType->nRotationIndex++;
		pVideoRenderDataType->sRotationFrames[i].buf.buf_type  = pFrame->buf.buf_type;
		pVideoRenderDataType->sRotationFrames[i].buf.format = pFrame->buf.format;
		if (pVideoRenderDataType->nRotationAngle == MPP_ROTATION_90 || pVideoRenderDataType->nRotationAngle == MPP_ROTATION_270) {
			if (pFrame->buf.crop_en) {
				pVideoRenderDataType->sRotationFrames[i].buf.size.width = pFrame->buf.crop.height & (~0x01);
				pVideoRenderDataType->sRotationFrames[i].buf.size.height =  pFrame->buf.crop.width & (~0x01);
			} else {
				pVideoRenderDataType->sRotationFrames[i].buf.size.width = pFrame->buf.size.height;
				pVideoRenderDataType->sRotationFrames[i].buf.size.height = pFrame->buf.size.width;
			}
		} else if (pVideoRenderDataType->nRotationAngle == MPP_ROTATION_180 || pVideoRenderDataType->nRotationAngle == MPP_ROTATION_0) {
			if (pFrame->buf.crop_en) {
				pVideoRenderDataType->sRotationFrames[i].buf.size.width = pFrame->buf.crop.width  & (~0x01);
				pVideoRenderDataType->sRotationFrames[i].buf.size.height =pFrame->buf.crop.height & (~0x01);
			} else {
				pVideoRenderDataType->sRotationFrames[i].buf.size.width = pFrame->buf.size.width;
				pVideoRenderDataType->sRotationFrames[i].buf.size.height = pFrame->buf.size.height;
			}
		}
	}

	for (i = 0;i < cnt; i++) {
		switch(pFrame->buf.format) {
		case MPP_FMT_YUV420P:
			pVideoRenderDataType->sRotationFrames[i].buf.stride[0] = (pVideoRenderDataType->sRotationFrames[i].buf.size.width+15)/16*16;
			pVideoRenderDataType->sRotationFrames[i].buf.stride[1] = pVideoRenderDataType->sRotationFrames[i].buf.stride[0]>>1;
			pVideoRenderDataType->sRotationFrames[i].buf.stride[2] = pVideoRenderDataType->sRotationFrames[i].buf.stride[0]>>1;
			break;
		case MPP_FMT_YUV444P:
			pVideoRenderDataType->sRotationFrames[i].buf.stride[0] = (pVideoRenderDataType->sRotationFrames[i].buf.size.width+15)/16*16;
			pVideoRenderDataType->sRotationFrames[i].buf.stride[1] = pVideoRenderDataType->sRotationFrames[i].buf.stride[0];
			pVideoRenderDataType->sRotationFrames[i].buf.stride[2] = pVideoRenderDataType->sRotationFrames[i].buf.stride[0];
			break;
		case MPP_FMT_YUV422P:
			pVideoRenderDataType->sRotationFrames[i].buf.stride[0] = (pVideoRenderDataType->sRotationFrames[i].buf.size.width+15)/16*16;
			pVideoRenderDataType->sRotationFrames[i].buf.stride[1] = pVideoRenderDataType->sRotationFrames[i].buf.stride[0]>>1;
			pVideoRenderDataType->sRotationFrames[i].buf.stride[2] = pVideoRenderDataType->sRotationFrames[i].buf.stride[0]>>1;
			break;
		case MPP_FMT_NV12:
		case MPP_FMT_NV21:
			pVideoRenderDataType->sRotationFrames[i].buf.stride[0] = (pVideoRenderDataType->sRotationFrames[i].buf.size.width+15)/16*16;
			pVideoRenderDataType->sRotationFrames[i].buf.stride[1] = pVideoRenderDataType->sRotationFrames[i].buf.stride[0]>>1;
			pVideoRenderDataType->sRotationFrames[i].buf.stride[2] = 0;
			break;
		case MPP_FMT_YUV400:
		case MPP_FMT_ABGR_8888:
		case MPP_FMT_ARGB_8888:
		case MPP_FMT_RGBA_8888:
		case MPP_FMT_BGRA_8888:
		case MPP_FMT_BGR_888:
		case MPP_FMT_RGB_888:
		case MPP_FMT_BGR_565:
		case MPP_FMT_RGB_565:
			pVideoRenderDataType->sRotationFrames[i].buf.stride[0] = (pVideoRenderDataType->sRotationFrames[i].buf.size.width+15)/16*16;
			pVideoRenderDataType->sRotationFrames[i].buf.stride[1] = 0;
			pVideoRenderDataType->sRotationFrames[i].buf.stride[2] = 0;
			break;
		default:
			loge("unsupport format");
			return -1;
		}
	}
	return 0;
}

static int OMX_VideoRenderInitRotationParam(VIDEO_RENDER_DATA_TYPE * pVideoRenderDataType,struct mpp_frame* pFrame)
{
	int i = 0;
	int cnt = 0;

	pVideoRenderDataType->nInitRotationParam = 0;
	if (pVideoRenderDataType->nRotationAngle == MPP_ROTATION_0) {
		pVideoRenderDataType->nInitRotationParam = 1;
		return 0;
	}
	//set rotation frame info
	if (OMX_VideoRenderSetRotationFrameInfo(pVideoRenderDataType,pFrame) != 0) {
		loge("OMX_VideoRenderSetRotationFrameInfo\n");
		pVideoRenderDataType->nInitRotationParam = -1;
		return -1;
	}
	//open GE
	if (pVideoRenderDataType->sGeHandle == NULL) {
		pVideoRenderDataType->sGeHandle = mpp_ge_open();
		if (pVideoRenderDataType->sGeHandle == NULL) {
			loge("open ge device error\n");
			pVideoRenderDataType->nInitRotationParam = -1;
			return -1;
		}
	}
	// open DMA  device
	if (pVideoRenderDataType->nDMAFd <= 0) {
		pVideoRenderDataType->nDMAFd = dmabuf_device_open();
		if (pVideoRenderDataType->nDMAFd < 0) {
			loge("dmabuf_device_open error\n");
			goto _exit;
		}
	}

	cnt = sizeof(pVideoRenderDataType->sRotationFrames)/sizeof(pVideoRenderDataType->sRotationFrames[0]);

	// free last DMA
	for (i = 0; i < cnt; i++) {
		if (OMX_VideoRenderFreeFrameBuffer(pVideoRenderDataType,&pVideoRenderDataType->sRotationFrames[i]) < 0) {
			loge("OMX_VideoRenderFreeFrameBuffer error\n");
			break;
		}
	}
	// alloc this DMA
	for (i = 0; i < cnt; i++) {
		if (OMX_VideoRenderAllocFrameBuffer(pVideoRenderDataType,&pVideoRenderDataType->sRotationFrames[i]) < 0) {
			loge("OMX_VideoRenderAllocFrameBuffer error\n");
			break;
		}
	}

	if (i < cnt) {
		goto _exit;
	}

	pVideoRenderDataType->nInitRotationParam = 1;
	return 0;

_exit:
	for (; i >= 0; i--) {
		OMX_VideoRenderFreeFrameBuffer(pVideoRenderDataType,&pVideoRenderDataType->sRotationFrames[i]);
	}

	if (pVideoRenderDataType->nDMAFd > 0) {
		dmabuf_device_close(pVideoRenderDataType->nDMAFd);
		pVideoRenderDataType->nDMAFd = -1;
	}

	if (pVideoRenderDataType->sGeHandle) {
		mpp_ge_close(pVideoRenderDataType->sGeHandle);
		pVideoRenderDataType->sGeHandle = NULL;
	}
	pVideoRenderDataType->nInitRotationParam = -1;
	return -1;

}

static int OMX_VideoRenderDeInitRotationParam(VIDEO_RENDER_DATA_TYPE * pVideoRenderDataType)
{
	int i = 0;
	int cnt = 0;
	if (pVideoRenderDataType->nInitRotationParam != 1) {// init fail ,so do not need to deinit
		return 0;
	}
	cnt = sizeof(pVideoRenderDataType->sRotationFrames)/sizeof(pVideoRenderDataType->sRotationFrames[0]);
	for (i = 0; i < cnt; i++) {
		OMX_VideoRenderFreeFrameBuffer(pVideoRenderDataType,&pVideoRenderDataType->sRotationFrames[i]);
	}
	if (pVideoRenderDataType->nDMAFd > 0) {
		dmabuf_device_close(pVideoRenderDataType->nDMAFd);
		pVideoRenderDataType->nDMAFd = -1;
	}

	if (!mpp_list_empty(&pVideoRenderDataType->sDMAFd) && pVideoRenderDataType->sGeHandle) {
		VIDEO_RENDER_DMA_FD *dma_fd_node = NULL,*dma_fd_node1 = NULL;
		mpp_list_for_each_entry_safe(dma_fd_node,dma_fd_node1, &pVideoRenderDataType->sDMAFd, sList) {
			for (i = 0; i < 3; i++) {
				if (dma_fd_node->fd[i] > 0) {
					mpp_ge_rm_dmabuf(pVideoRenderDataType->sGeHandle,dma_fd_node->fd[i]);
					printf("[%s:%d]:%d\n",__FUNCTION__,__LINE__,dma_fd_node->fd[i]);
					dma_fd_node->fd[i] = -1;
				 }
			}
			mpp_list_del(&dma_fd_node->sList);
			mpp_free(dma_fd_node);
		}
	}

	if (pVideoRenderDataType->sGeHandle) {
		mpp_ge_close(pVideoRenderDataType->sGeHandle);
		pVideoRenderDataType->sGeHandle = NULL;
	}

	return 0;
}

static int OMX_VideoRenderRotateFrame(VIDEO_RENDER_DATA_TYPE * pVideoRenderDataType,struct mpp_frame* pFrame)
{
	int ret = 0;
	struct ge_bitblt blt;
	struct mpp_ge *ge = NULL;
	int find = 0;
	int comp = 0;
	int i = 0;
	if (pVideoRenderDataType == NULL || pFrame == NULL) {
		loge("param error !!!\n");
		return -1;
	}

	if (pVideoRenderDataType->nRotationAngle == MPP_ROTATION_0) {
		pVideoRenderDataType->pCurDisplayFrame = pFrame;
		return 0;
	}

	if (pVideoRenderDataType->nInitRotationParam != 1) {
		pVideoRenderDataType->pCurDisplayFrame = pFrame;
		loge("RotationParam do not init ok !!!\n");
		return -1;
	}

	memset(&blt,0x00,sizeof(struct ge_bitblt));
	blt.src_buf.buf_type = MPP_DMA_BUF_FD;
	blt.src_buf.format = pFrame->buf.format;
	blt.src_buf.fd[0] = pFrame->buf.fd[0];
	blt.src_buf.fd[1] = pFrame->buf.fd[1];
	blt.src_buf.fd[2] = pFrame->buf.fd[2];
	blt.src_buf.stride[0] = pFrame->buf.stride[0];
	blt.src_buf.stride[1] =  pFrame->buf.stride[1];
	blt.src_buf.stride[2] =  pFrame->buf.stride[2];
	blt.src_buf.size.width = pFrame->buf.size.width;
	blt.src_buf.size.height = pFrame->buf.size.height;

	blt.src_buf.crop_en = pFrame->buf.crop_en;
	blt.src_buf.crop.x = pFrame->buf.crop.x;
	blt.src_buf.crop.y = pFrame->buf.crop.y;
	blt.src_buf.crop.width = pFrame->buf.crop.width&(~0x01);
	blt.src_buf.crop.height = pFrame->buf.crop.height&(~0x01);

	if (pVideoRenderDataType->pCurDisplayFrame == &pVideoRenderDataType->sRotationFrames[0]) {
		pVideoRenderDataType->pCurDisplayFrame = &pVideoRenderDataType->sRotationFrames[1];
	} else {
		pVideoRenderDataType->pCurDisplayFrame = &pVideoRenderDataType->sRotationFrames[0];
	}

	blt.dst_buf.buf_type = pVideoRenderDataType->pCurDisplayFrame->buf.buf_type;
	blt.dst_buf.format =pVideoRenderDataType->pCurDisplayFrame->buf.format;
	blt.dst_buf.fd[0] = pVideoRenderDataType->pCurDisplayFrame->buf.fd[0];
	blt.dst_buf.fd[1] = pVideoRenderDataType->pCurDisplayFrame->buf.fd[1];
	blt.dst_buf.fd[2] = pVideoRenderDataType->pCurDisplayFrame->buf.fd[2];
	blt.dst_buf.stride[0] = pVideoRenderDataType->pCurDisplayFrame->buf.stride[0];
	blt.dst_buf.stride[1] = pVideoRenderDataType->pCurDisplayFrame->buf.stride[1];
	blt.dst_buf.stride[2] = pVideoRenderDataType->pCurDisplayFrame->buf.stride[2];
	blt.dst_buf.size.width = pVideoRenderDataType->pCurDisplayFrame->buf.size.width;
	blt.dst_buf.size.height = pVideoRenderDataType->pCurDisplayFrame->buf.size.height;
	blt.dst_buf.crop_en = 0;
	blt.ctrl.flags = pVideoRenderDataType->nRotationAngle;

	//pVideoRenderDataType->pCurDisplayFrame->id = pFrame->id;
	pVideoRenderDataType->pCurDisplayFrame->pts = pFrame->pts;
	pVideoRenderDataType->pCurDisplayFrame->flags = pFrame->flags;
	pVideoRenderDataType->pCurDisplayFrame->buf.crop_en = 0;

	ge = pVideoRenderDataType->sGeHandle;

	comp = OMX_VideoRenderGetComponentNum(pFrame->buf.format);
	find = OMX_VideoRenderIsExistDMAFd(pVideoRenderDataType,pFrame);
	if (!find) {
		for(i = 0;i < comp; i++) {
			mpp_ge_add_dmabuf(ge, pFrame->buf.fd[i]);
		}
		OMX_VideoRenderAddDMAFd(pVideoRenderDataType,pFrame);
	}

	comp = OMX_VideoRenderGetComponentNum(pVideoRenderDataType->pCurDisplayFrame->buf.format);
	find = OMX_VideoRenderIsExistDMAFd(pVideoRenderDataType,pVideoRenderDataType->pCurDisplayFrame);
	if (!find) {
		for(i = 0;i < comp; i++) {
			mpp_ge_add_dmabuf(ge, pVideoRenderDataType->pCurDisplayFrame->buf.fd[i]);
			printf("[%s:%d]:%d\n",__FUNCTION__,__LINE__,pVideoRenderDataType->pCurDisplayFrame->buf.fd[i]);
		}
		OMX_VideoRenderAddDMAFd(pVideoRenderDataType,pVideoRenderDataType->pCurDisplayFrame);
	}

	ret = mpp_ge_bitblt(ge, &blt);
	if (ret) {
		loge("mpp_ge_bitblt task failed: %d\n", ret);
		ret = -1;
		goto _exit0;
	}
	ret = mpp_ge_emit(ge);
	if (ret) {
		loge("mpp_ge_emit task failed: %d\n", ret);
		ret = -1;
		goto _exit0;
	}
	ret = mpp_ge_sync(ge);
	if (ret) {
		loge("mpp_ge_sync task failed: %d\n", ret);
		ret = -1;
		goto _exit0;
	}

_exit0:
	return ret;
}

static int  OMX_VideoRenderGiveBackAllFrames(VIDEO_RENDER_DATA_TYPE * pVideoRenderDataType)
{
	int ret;
	// 1 move ready node to using list
	logi("Before OMX_VideoRenderComponentThread exit,move node in sInReadyFrame to sInProcessedFrmae\n");
		if (!OMX_VideoRenderListEmpty(&pVideoRenderDataType->sInReadyFrame,pVideoRenderDataType->sInFrameLock)) {
			VIDEO_RENDER_IN_FRAME *pFrameNode,*pFrameNode1;
			aic_pthread_mutex_lock(&pVideoRenderDataType->sInFrameLock);
			mpp_list_for_each_entry_safe(pFrameNode, pFrameNode1, &pVideoRenderDataType->sInReadyFrame, sList) {
				pVideoRenderDataType->nLeftReadyFrameWhenCompoentExitNum++;
				mpp_list_del(&pFrameNode->sList);
				mpp_list_add_tail(&pFrameNode->sList, &pVideoRenderDataType->sInProcessedFrmae);
			}
			aic_pthread_mutex_unlock(&pVideoRenderDataType->sInFrameLock);
		}
	// 2 give back frames in using list to vdec or app
	logi("Before OMX_AudioRenderComponentThread exit,give all frames back to Vdec\n");
	if (!OMX_VideoRenderListEmpty(&pVideoRenderDataType->sInProcessedFrmae,pVideoRenderDataType->sInFrameLock)) {
		VIDEO_RENDER_IN_FRAME *pFrameNode = NULL;
		OMX_BUFFERHEADERTYPE sBuffHead;
		while(!OMX_VideoRenderListEmpty(&pVideoRenderDataType->sInProcessedFrmae,pVideoRenderDataType->sInFrameLock)) {
			aic_pthread_mutex_lock(&pVideoRenderDataType->sInFrameLock);
			pFrameNode = mpp_list_first_entry(&pVideoRenderDataType->sInProcessedFrmae, VIDEO_RENDER_IN_FRAME, sList);
			aic_pthread_mutex_unlock(&pVideoRenderDataType->sInFrameLock);
			ret = -1;
			if (pVideoRenderDataType->sInPortTunneledInfo[VIDEO_RENDER_PORT_IN_VIDEO_INDEX].nTunneledFlag) {
				sBuffHead.nInputPortIndex = VIDEO_RENDER_PORT_IN_VIDEO_INDEX;
				sBuffHead.nOutputPortIndex = pVideoRenderDataType->sInPortTunneledInfo[VIDEO_RENDER_PORT_IN_VIDEO_INDEX].nTunnelPortIndex;
				sBuffHead.pBuffer = (OMX_U8 *)&pFrameNode->sFrameInfo;
				ret = OMX_FillThisBuffer(pVideoRenderDataType->sInPortTunneledInfo[VIDEO_RENDER_PORT_IN_VIDEO_INDEX].pTunneledComp,&sBuffHead);
			} else {
				if (pVideoRenderDataType->pCallbacks != NULL && pVideoRenderDataType->pCallbacks->EmptyBufferDone!= NULL) {
					sBuffHead.pBuffer =  (OMX_U8 *)&pFrameNode->sFrameInfo;
					ret = pVideoRenderDataType->pCallbacks->EmptyBufferDone(pVideoRenderDataType->hSelf,pVideoRenderDataType->pAppData,&sBuffHead);
				}
			}
			if (ret == 0) {
				logd("give back frame to vdec ok");
				pVideoRenderDataType->nGiveBackFrameOkNum++;
			} else {
				loge("give back frame to vdec fail\n");
				pVideoRenderDataType->nGiveBackFrameFailNum++;
				continue;// must give back ok ,so retry to give back
			}
			logi("nGiveBackFrameOkNum:%d,nGiveBackFrameFailNum:%d\n"
				,pVideoRenderDataType->nGiveBackFrameOkNum
				,pVideoRenderDataType->nGiveBackFrameFailNum);

			aic_pthread_mutex_lock(&pVideoRenderDataType->sInFrameLock);
			mpp_list_del(&pFrameNode->sList);
			mpp_list_add_tail(&pFrameNode->sList, &pVideoRenderDataType->sInEmptyFrame);
			aic_pthread_mutex_unlock(&pVideoRenderDataType->sInFrameLock);
		}
	}
	return 0;
}

static OMX_ERRORTYPE OMX_VideoRenderSendCommand(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_COMMANDTYPE Cmd,
		OMX_IN	OMX_U32 nParam1,
		OMX_IN	OMX_PTR pCmdData)
{
	VIDEO_RENDER_DATA_TYPE *pVideoRenderDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	struct aic_message sMsg;
	pVideoRenderDataType = (VIDEO_RENDER_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	sMsg.message_id = Cmd;
	sMsg.param = nParam1;
	sMsg.data_size = 0;

	//now not use always NULL
	if (pCmdData != NULL) {
		sMsg.data = pCmdData;
		sMsg.data_size = strlen((char*)pCmdData);
	}

	aic_msg_put(&pVideoRenderDataType->sMsgQue, &sMsg);
	return eError;
}

static void* OMX_VideoRenderComponentThread(void* pThreadData);

static OMX_ERRORTYPE OMX_VideoRenderGetParameter(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nParamIndex,
		OMX_INOUT OMX_PTR pComponentParameterStructure)
{
	VIDEO_RENDER_DATA_TYPE *pVideoRenderDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_S32 index =(OMX_S32)nParamIndex;
	pVideoRenderDataType = (VIDEO_RENDER_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

	switch (index) {
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
			if (sBufferSupplier->nPortIndex == VIDEO_RENDER_PORT_IN_VIDEO_INDEX) {
				sBufferSupplier->eBufferSupplier = pVideoRenderDataType->sInBufSupplier[VIDEO_RENDER_PORT_IN_VIDEO_INDEX].eBufferSupplier;
			} else if (sBufferSupplier->nPortIndex == VIDEO_RENDER_PORT_IN_CLOCK_INDEX) {
				sBufferSupplier->eBufferSupplier = pVideoRenderDataType->sInBufSupplier[VIDEO_RENDER_PORT_IN_CLOCK_INDEX].eBufferSupplier;
			} else {
				loge("error nPortIndex\n");
				eError = OMX_ErrorBadPortIndex;
			}
			break;
		}
		case OMX_IndexConfigCommonOutputCrop:
			((OMX_CONFIG_RECTTYPE*)pComponentParameterStructure)->nLeft = pVideoRenderDataType->sDisRect.x;
			((OMX_CONFIG_RECTTYPE*)pComponentParameterStructure)->nTop = pVideoRenderDataType->sDisRect.y;
			((OMX_CONFIG_RECTTYPE*)pComponentParameterStructure)->nWidth = pVideoRenderDataType->sDisRect.width;
			((OMX_CONFIG_RECTTYPE*)pComponentParameterStructure)->nHeight = pVideoRenderDataType->sDisRect.height;

			break;
		case OMX_IndexVendorVideoRenderScreenSize: {
			struct mpp_size sSize;
			if (pVideoRenderDataType->nVideoRenderInitFlag != 1) {
				loge("pVideoRenderDataType->render not init!!!\n");
				eError = OMX_ErrorUndefined;
				break;
			}
			pVideoRenderDataType->render->get_screen_size(pVideoRenderDataType->render,&sSize);
			((OMX_PARAM_SCREEN_SIZE*)pComponentParameterStructure)->nWidth = sSize.width;
			((OMX_PARAM_SCREEN_SIZE*)pComponentParameterStructure)->nHeight = sSize.height;
			break;
		}
		default:
			eError = OMX_ErrorNotImplemented;
		 break;
	}

	return eError;
}

static OMX_ERRORTYPE OMX_VideoRenderSetParameter(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nParamIndex,
		OMX_IN	OMX_PTR pComponentParameterStructure)
{
	VIDEO_RENDER_DATA_TYPE *pVideoRenderDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_PARAM_FRAMEEND *sFrameEnd;
	OMX_S32 index = (OMX_S32)nParamIndex;
	pVideoRenderDataType = (VIDEO_RENDER_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	if (pComponentParameterStructure == NULL) {
		loge("param error!!!\n");
		return OMX_ErrorBadParameter;
	}
	switch (index) {
		case OMX_IndexParamPortDefinition:
			break;
		case OMX_IndexParamVideoPortFormat:
			break;
		case OMX_IndexParamVideoAvc:
			break;
		case OMX_IndexParamVideoProfileLevelQuerySupported:
			break;
		case OMX_IndexParamVideoProfileLevelCurrent:
			break;
		case OMX_IndexParamVideoMpeg4:
			break;
		case OMX_IndexConfigCommonOutputCrop:

			pVideoRenderDataType->sDisRect.x = ((OMX_CONFIG_RECTTYPE*)pComponentParameterStructure)->nLeft;
			pVideoRenderDataType->sDisRect.y = ((OMX_CONFIG_RECTTYPE*)pComponentParameterStructure)->nTop;
			pVideoRenderDataType->sDisRect.width = ((OMX_CONFIG_RECTTYPE*)pComponentParameterStructure)->nWidth;
			pVideoRenderDataType->sDisRect.height =((OMX_CONFIG_RECTTYPE*)pComponentParameterStructure)->nHeight;
			pVideoRenderDataType->nDisRectChange = OMX_TRUE;
			break;
		case OMX_IndexVendorStreamFrameEnd:
			sFrameEnd = (OMX_PARAM_FRAMEEND*)pComponentParameterStructure;
			if (sFrameEnd->bFrameEnd == OMX_TRUE) {
				pVideoRenderDataType->nFrameEndFlag = OMX_TRUE;
				logi("setup nFrameEndFlag\n");
			} else {
				pVideoRenderDataType->nFrameEndFlag = OMX_FALSE;
				logi("cancel nFrameEndFlag\n");
			}

			break;
		default:
		 break;
	}
	return eError;
}

static OMX_ERRORTYPE OMX_VideoRenderGetConfig(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_INOUT OMX_PTR pComponentConfigStructure)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	VIDEO_RENDER_DATA_TYPE* pVideoRenderDataType = (VIDEO_RENDER_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	OMX_S32 index = (OMX_S32)nIndex;
	switch (index) {
	case OMX_IndexConfigCommonRotate: {
		OMX_CONFIG_ROTATIONTYPE * pRotation = (OMX_CONFIG_ROTATIONTYPE *)pComponentConfigStructure;
		pRotation->nRotation = pVideoRenderDataType->nRotationAngle;
		break;
	}
	default:
		break;
	}
	return eError;

}

static OMX_ERRORTYPE OMX_VideoRenderSetConfig(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_INDEXTYPE nIndex,
		OMX_IN	OMX_PTR pComponentConfigStructure)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	VIDEO_RENDER_DATA_TYPE* pVideoRenderDataType = (VIDEO_RENDER_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	int ret;
	OMX_S32 index = (OMX_S32)nIndex;
	switch (index) {
	case OMX_IndexConfigTimePosition:
		//1 clear input buffer list
		OMX_VideoRenderGiveBackAllFrames(pVideoRenderDataType);
		//2 reset flag
		pVideoRenderDataType->nFrameFisrtShowFlag = OMX_TRUE;
		pVideoRenderDataType->nFlags = 0;
		pVideoRenderDataType->eClockState = OMX_TIME_ClockStateWaitingForStartTime;
		if ((pVideoRenderDataType->sDisRect.width != 0) && (pVideoRenderDataType->sDisRect.width != 0)) {
			pVideoRenderDataType->nDisRectChange = OMX_TRUE;
		}

		break;
	case OMX_IndexConfigTimeSeekMode:
		break;
	case OMX_IndexConfigTimeClockState: {
		OMX_TIME_CONFIG_CLOCKSTATETYPE* state = (OMX_TIME_CONFIG_CLOCKSTATETYPE *)pComponentConfigStructure;
		pVideoRenderDataType->eClockState = state->eState;
		printf("[%s:%d] pVideoRenderDataType->eClockState:%d\n",__FUNCTION__,__LINE__,pVideoRenderDataType->eClockState);
		break;
	}
	case OMX_IndexVendorVideoRenderInit:
		if (!pVideoRenderDataType->render) {
			ret = aic_video_render_create(&pVideoRenderDataType->render);
			if (ret) {
				eError = OMX_ErrorInsufficientResources;
				loge("aic_video_render_create error!!!!\n");
				break;
			}
			ret = pVideoRenderDataType->render->init(pVideoRenderDataType->render,
										pVideoRenderDataType->nLayerId,pVideoRenderDataType->nDevId);
			if (!ret) {
				logd("pVideoRenderDataType->render->init ok\n");
				pVideoRenderDataType->nVideoRenderInitFlag = 1;
			} else {
				loge("pVideoRenderDataType->render->init fail\n");
				eError = OMX_ErrorInsufficientResources;
			}
		} else {
			loge("pVideoRenderDataType->render has been created!!1!\n");
		}
		break;
	case OMX_IndexVendorVideoRenderCapture:
			OMX_VideoRenderCapture(hComponent,nIndex,pComponentConfigStructure);
		break;
	case OMX_IndexConfigCommonRotate: {
		OMX_CONFIG_ROTATIONTYPE *pRotation = (OMX_CONFIG_ROTATIONTYPE *)pComponentConfigStructure;
		if (pVideoRenderDataType->nRotationAngle != pRotation->nRotation ) {//MPP_ROTATION_0 MPP_ROTATION_90 MPP_ROTATION_180 MPP_ROTATION_270
			pVideoRenderDataType->nRotationAngle = pRotation->nRotation;
			pVideoRenderDataType->nRotationAngleChange = 1;
		}
		break;
	}

	default:
		break;
	}
	return eError;

}

static OMX_ERRORTYPE OMX_VideoRenderGetState(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_OUT OMX_STATETYPE* pState)
{
	VIDEO_RENDER_DATA_TYPE* pVideoRenderDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	pVideoRenderDataType = (VIDEO_RENDER_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

	aic_pthread_mutex_lock(&pVideoRenderDataType->stateLock);
	*pState = pVideoRenderDataType->state;
	aic_pthread_mutex_unlock(&pVideoRenderDataType->stateLock);

	return eError;

}

static OMX_ERRORTYPE OMX_VideoRenderComponentTunnelRequest(
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
	VIDEO_RENDER_DATA_TYPE* pVideoRenderDataType;
	pVideoRenderDataType = (VIDEO_RENDER_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComp)->pComponentPrivate);
	if (pVideoRenderDataType->state != OMX_StateLoaded)
	 {
		loge("Component is not in OMX_StateLoaded,it is in%d,it can not tunnel\n",pVideoRenderDataType->state);
		return OMX_ErrorInvalidState;
	}
	if (nPort == VIDEO_RENDER_PORT_IN_VIDEO_INDEX) {
		pPort = &pVideoRenderDataType->sInPortDef[VIDEO_RENDER_PORT_IN_VIDEO_INDEX];
		pTunneledInfo = &pVideoRenderDataType->sInPortTunneledInfo[VIDEO_RENDER_PORT_IN_VIDEO_INDEX];
		pBufSupplier = &pVideoRenderDataType->sInBufSupplier[VIDEO_RENDER_PORT_IN_VIDEO_INDEX];
	} else if (nPort == VIDEO_RENDER_PORT_IN_CLOCK_INDEX) {
		pPort = &pVideoRenderDataType->sInPortDef[VIDEO_RENDER_PORT_IN_CLOCK_INDEX];
		pTunneledInfo = &pVideoRenderDataType->sInPortTunneledInfo[VIDEO_RENDER_PORT_IN_CLOCK_INDEX];
		pBufSupplier = &pVideoRenderDataType->sInBufSupplier[VIDEO_RENDER_PORT_IN_CLOCK_INDEX];
	} else {
		loge("component can not find port :%d,VIDEO_RENDER_PORT_IN_VIDEO_INDEX:%d\n",nPort,VIDEO_RENDER_PORT_IN_VIDEO_INDEX);
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

static OMX_ERRORTYPE OMX_VideoRenderEmptyThisBuffer(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	VIDEO_RENDER_DATA_TYPE* pVideoRenderDataType;
	VIDEO_RENDER_IN_FRAME *pFrame;
	struct aic_message sMsg;
	pVideoRenderDataType = (VIDEO_RENDER_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

	aic_pthread_mutex_lock(&pVideoRenderDataType->stateLock);
	if (pVideoRenderDataType->state != OMX_StateExecuting) {
		logw("component is not in OMX_StateExecuting,it is in [%d]!!!\n",pVideoRenderDataType->state);
		aic_pthread_mutex_unlock(&pVideoRenderDataType->stateLock);
		return OMX_ErrorIncorrectStateOperation;
	}
	//aic_pthread_mutex_unlock(&pVideoRenderDataType->stateLock);

	// if (OMX_VideoRenderListEmpty(&pVideoRenderDataType->sInEmptyFrame,pVideoRenderDataType->sInFrameLock))
	//  {
	// 	if (pVideoRenderDataType->nInFrameNodeNum + 1> VIDEO_RENDER_FRAME_NUM_MAX) {
	// 		loge("empty node has aready increase to max [%d]!!!\n",pVideoRenderDataType->nInFrameNodeNum);
	// 		eError = OMX_ErrorInsufficientResources;
	// 		return  eError;
	// 	} else {
	// 		int i;
	// 		logw("no empty node,need to extend!!!\n");
	// 		for(i =0 ; i < VIDEO_RENDER_FRAME_ONE_TIME_CREATE_NUM; i++ ) {
	// 			VIDEO_RENDER_IN_FRAME *pFrameNode = (VIDEO_RENDER_IN_FRAME*)mpp_alloc(sizeof(VIDEO_RENDER_IN_FRAME));
	// 			if (NULL == pFrameNode) {
	// 				break;
	// 			}
	// 			memset(pFrameNode,0x00,sizeof(VIDEO_RENDER_IN_FRAME));
	// 			aic_pthread_mutex_lock(&pVideoRenderDataType->sInFrameLock);
	// 			mpp_list_add_tail(&pFrameNode->sList, &pVideoRenderDataType->sInEmptyFrame);
	// 			aic_pthread_mutex_unlock(&pVideoRenderDataType->sInFrameLock);
	// 			pVideoRenderDataType->nInFrameNodeNum++;
	// 		}
	// 		if (i == 0) {
	// 			loge("mpp_alloc empty video node fail\n");
	// 			eError = OMX_ErrorInsufficientResources;
	// 			return  eError;
	// 		}
	// 	}
	// }

	if (OMX_VideoRenderListEmpty(&pVideoRenderDataType->sInEmptyFrame,pVideoRenderDataType->sInFrameLock)) {
		VIDEO_RENDER_IN_FRAME *pFrameNode = (VIDEO_RENDER_IN_FRAME*)mpp_alloc(sizeof(VIDEO_RENDER_IN_FRAME));
		if (NULL == pFrameNode) {
			loge("OMX_ErrorInsufficientResources\n");
			aic_pthread_mutex_unlock(&pVideoRenderDataType->stateLock);
			return OMX_ErrorInsufficientResources;
		}
		memset(pFrameNode,0x00,sizeof(VIDEO_RENDER_IN_FRAME));
		aic_pthread_mutex_lock(&pVideoRenderDataType->sInFrameLock);
		mpp_list_add_tail(&pFrameNode->sList, &pVideoRenderDataType->sInEmptyFrame);
		aic_pthread_mutex_unlock(&pVideoRenderDataType->sInFrameLock);
		pVideoRenderDataType->nInFrameNodeNum++;
	}

	if (pVideoRenderDataType->sInPortTunneledInfo[VIDEO_RENDER_PORT_IN_VIDEO_INDEX].nTunneledFlag) {// now Tunneled and non-Tunneled are same

		aic_pthread_mutex_lock(&pVideoRenderDataType->sInFrameLock);
		pFrame = mpp_list_first_entry(&pVideoRenderDataType->sInEmptyFrame, VIDEO_RENDER_IN_FRAME, sList);
		memcpy(&pFrame->sFrameInfo,pBuffer->pBuffer,sizeof(struct mpp_frame));
		mpp_list_del(&pFrame->sList);
		mpp_list_add_tail(&pFrame->sList, &pVideoRenderDataType->sInReadyFrame);
		aic_pthread_mutex_unlock(&pVideoRenderDataType->sInFrameLock);
		pVideoRenderDataType->nReceiveFrameNum++;
		logi("nReceiveFrameNum:%d\n",pVideoRenderDataType->nReceiveFrameNum);
	} else { // now Tunneled and non-Tunneled are same
		aic_pthread_mutex_lock(&pVideoRenderDataType->sInFrameLock);
		pFrame = mpp_list_first_entry(&pVideoRenderDataType->sInEmptyFrame, VIDEO_RENDER_IN_FRAME, sList);
		memcpy(&pFrame->sFrameInfo,pBuffer->pBuffer,sizeof(struct mpp_frame));
		mpp_list_del(&pFrame->sList);
		mpp_list_add_tail(&pFrame->sList, &pVideoRenderDataType->sInReadyFrame);
		aic_pthread_mutex_unlock(&pVideoRenderDataType->sInFrameLock);
		pVideoRenderDataType->nReceiveFrameNum++;
		logw("nReceiveFrameNum:%d\n",pVideoRenderDataType->nReceiveFrameNum);
	}

	aic_pthread_mutex_lock(&pVideoRenderDataType->sWaitReayFrameLock);
	if (pVideoRenderDataType->nWaitReayFrameFlag) {
		sMsg.message_id = OMX_CommandNops;
		sMsg.data_size = 0;
		aic_msg_put(&pVideoRenderDataType->sMsgQue, &sMsg);
		pVideoRenderDataType->nWaitReayFrameFlag = 0;
	}
	aic_pthread_mutex_unlock(&pVideoRenderDataType->sWaitReayFrameLock);
	aic_pthread_mutex_unlock(&pVideoRenderDataType->stateLock);
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

static OMX_ERRORTYPE OMX_VideoRenderFillThisBuffer(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;

	return eError;
}

static OMX_ERRORTYPE OMX_VideoRenderSetCallbacks(
		OMX_IN	OMX_HANDLETYPE hComponent,
		OMX_IN	OMX_CALLBACKTYPE* pCallbacks,
		OMX_IN	OMX_PTR pAppData)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	VIDEO_RENDER_DATA_TYPE* pVideoRenderDataType;
	pVideoRenderDataType = (VIDEO_RENDER_DATA_TYPE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
	pVideoRenderDataType->pCallbacks = pCallbacks;
	pVideoRenderDataType->pAppData = pAppData;
	return eError;
}

OMX_ERRORTYPE OMX_VideoRenderComponentDeInit(
		OMX_IN	OMX_HANDLETYPE hComponent)  {
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_COMPONENTTYPE *pComp;
	VIDEO_RENDER_DATA_TYPE *pVideoRenderDataType;
	VIDEO_RENDER_IN_FRAME	*pFrameNode = NULL,*pFrameNode1 = NULL;
	pComp = (OMX_COMPONENTTYPE *)hComponent;
	struct aic_message sMsg;
	pVideoRenderDataType = (VIDEO_RENDER_DATA_TYPE *)pComp->pComponentPrivate;

	aic_pthread_mutex_lock(&pVideoRenderDataType->stateLock);
	if (pVideoRenderDataType->state != OMX_StateLoaded) {
		logw("compoent is in %d,but not in OMX_StateLoaded(1),can ont FreeHandle.\n",pVideoRenderDataType->state);
		aic_pthread_mutex_unlock(&pVideoRenderDataType->stateLock);
		return OMX_ErrorIncorrectStateOperation;
	}
	aic_pthread_mutex_unlock(&pVideoRenderDataType->stateLock);

	sMsg.message_id = OMX_CommandStop;
	sMsg.data_size = 0;
	aic_msg_put(&pVideoRenderDataType->sMsgQue, &sMsg);
	pthread_join(pVideoRenderDataType->threadId, (void*)&eError);

	aic_pthread_mutex_lock(&pVideoRenderDataType->sInFrameLock);
	if (!mpp_list_empty(&pVideoRenderDataType->sInEmptyFrame)) {
			mpp_list_for_each_entry_safe(pFrameNode, pFrameNode1, &pVideoRenderDataType->sInEmptyFrame, sList) {
				mpp_list_del(&pFrameNode->sList);
				mpp_free(pFrameNode);
		}
	}

	if (!mpp_list_empty(&pVideoRenderDataType->sInReadyFrame)) {
			mpp_list_for_each_entry_safe(pFrameNode, pFrameNode1, &pVideoRenderDataType->sInReadyFrame, sList) {
				mpp_list_del(&pFrameNode->sList);
				mpp_free(pFrameNode);
		}
	}
	if (!mpp_list_empty(&pVideoRenderDataType->sInProcessedFrmae)) {
			mpp_list_for_each_entry_safe(pFrameNode, pFrameNode1, &pVideoRenderDataType->sInProcessedFrmae, sList) {
				mpp_list_del(&pFrameNode->sList);
				mpp_free(pFrameNode);
		}
	}
	aic_pthread_mutex_unlock(&pVideoRenderDataType->sInFrameLock);

	pthread_mutex_destroy(&pVideoRenderDataType->sInFrameLock);
	pthread_mutex_destroy(&pVideoRenderDataType->stateLock);
	pthread_mutex_destroy(&pVideoRenderDataType->sWaitReayFrameLock);

	aic_msg_destroy(&pVideoRenderDataType->sMsgQue);

	OMX_VideoRenderDeInitRotationParam(pVideoRenderDataType);

	if (pVideoRenderDataType->render)  {
		pVideoRenderDataType->render->set_on_off(pVideoRenderDataType->render,0);
		aic_video_render_destroy(pVideoRenderDataType->render);
		pVideoRenderDataType->render = NULL;
	}



	mpp_free(pVideoRenderDataType);
	pVideoRenderDataType = NULL;

	logw("OMX_VideoRenderComponentDeInit\n");
	return eError;
}

OMX_ERRORTYPE OMX_VideoRenderComponentInit(
		OMX_IN	OMX_HANDLETYPE hComponent)
{
	OMX_COMPONENTTYPE *pComp;
	VIDEO_RENDER_DATA_TYPE *pVideoRenderDataType;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_U32 err;
	OMX_U32 i;

	logw("OMX_VideoRenderComponentInit....");

	pComp = (OMX_COMPONENTTYPE *)hComponent;

	pVideoRenderDataType = (VIDEO_RENDER_DATA_TYPE *)mpp_alloc(sizeof(VIDEO_RENDER_DATA_TYPE));

	if (NULL == pVideoRenderDataType) {
		loge("mpp_alloc(sizeof(VIDEO_RENDER_DATA_TYPE) fail!");
		eError = OMX_ErrorInsufficientResources;
		goto _EXIT1;
	}

	memset(pVideoRenderDataType, 0x0, sizeof(VIDEO_RENDER_DATA_TYPE));
	pComp->pComponentPrivate	= (void*) pVideoRenderDataType;
	pVideoRenderDataType->nFrameFisrtShowFlag = OMX_TRUE;
	pVideoRenderDataType->state 		= OMX_StateLoaded;
	pVideoRenderDataType->hSelf 		= pComp;

	pComp->SetCallbacks 		= OMX_VideoRenderSetCallbacks;
	pComp->SendCommand			= OMX_VideoRenderSendCommand;
	pComp->GetState 			= OMX_VideoRenderGetState;
	pComp->GetParameter 		= OMX_VideoRenderGetParameter;
	pComp->SetParameter 		= OMX_VideoRenderSetParameter;
	pComp->GetConfig			= OMX_VideoRenderGetConfig;
	pComp->SetConfig			= OMX_VideoRenderSetConfig;
	pComp->ComponentTunnelRequest = OMX_VideoRenderComponentTunnelRequest;
	pComp->ComponentDeInit		= OMX_VideoRenderComponentDeInit;
	pComp->FillThisBuffer		= OMX_VideoRenderFillThisBuffer;
	pComp->EmptyThisBuffer		= OMX_VideoRenderEmptyThisBuffer;

	pVideoRenderDataType->sPortParam.nPorts = 2;
	pVideoRenderDataType->sPortParam.nStartPortNumber = 0x0;

	pVideoRenderDataType->sInPortDef[VIDEO_RENDER_PORT_IN_VIDEO_INDEX].nPortIndex = VIDEO_RENDER_PORT_IN_VIDEO_INDEX;
	pVideoRenderDataType->sInPortDef[VIDEO_RENDER_PORT_IN_VIDEO_INDEX].bPopulated = OMX_TRUE;
	pVideoRenderDataType->sInPortDef[VIDEO_RENDER_PORT_IN_VIDEO_INDEX].bEnabled	= OMX_TRUE;
	pVideoRenderDataType->sInPortDef[VIDEO_RENDER_PORT_IN_VIDEO_INDEX].eDomain = OMX_PortDomainVideo;
	pVideoRenderDataType->sInPortDef[VIDEO_RENDER_PORT_IN_VIDEO_INDEX].eDir = OMX_DirInput;

	pVideoRenderDataType->sInBufSupplier[VIDEO_RENDER_PORT_IN_VIDEO_INDEX].nPortIndex = 0x0;
	pVideoRenderDataType->sInBufSupplier[VIDEO_RENDER_PORT_IN_VIDEO_INDEX].eBufferSupplier = OMX_BufferSupplyOutput;

	pVideoRenderDataType->sInPortDef[VIDEO_RENDER_PORT_IN_CLOCK_INDEX].nPortIndex = VIDEO_RENDER_PORT_IN_CLOCK_INDEX;
	pVideoRenderDataType->sInPortDef[VIDEO_RENDER_PORT_IN_CLOCK_INDEX].bPopulated = OMX_TRUE;
	pVideoRenderDataType->sInPortDef[VIDEO_RENDER_PORT_IN_CLOCK_INDEX].bEnabled	= OMX_TRUE;
	pVideoRenderDataType->sInPortDef[VIDEO_RENDER_PORT_IN_CLOCK_INDEX].eDomain = OMX_PortDomainOther;
	pVideoRenderDataType->sInPortDef[VIDEO_RENDER_PORT_IN_CLOCK_INDEX].eDir = OMX_DirInput;

	pVideoRenderDataType->sInBufSupplier[VIDEO_RENDER_PORT_IN_CLOCK_INDEX].nPortIndex = 0x0;
	pVideoRenderDataType->sInBufSupplier[VIDEO_RENDER_PORT_IN_CLOCK_INDEX].eBufferSupplier = OMX_BufferSupplyOutput;

	pVideoRenderDataType->nInFrameNodeNum = 0;
	mpp_list_init(&pVideoRenderDataType->sInEmptyFrame);
	mpp_list_init(&pVideoRenderDataType->sInReadyFrame);
	mpp_list_init(&pVideoRenderDataType->sInProcessedFrmae);
	pthread_mutex_init(&pVideoRenderDataType->sInFrameLock, NULL);
	for(i =0 ; i < VIDEO_RENDER_FRAME_ONE_TIME_CREATE_NUM; i++) {
		VIDEO_RENDER_IN_FRAME *pFrameNode = (VIDEO_RENDER_IN_FRAME*)mpp_alloc(sizeof(VIDEO_RENDER_IN_FRAME));
		if (NULL == pFrameNode) {
			break;
		}
		memset(pFrameNode,0x00,sizeof(VIDEO_RENDER_IN_FRAME));
		mpp_list_add_tail(&pFrameNode->sList, &pVideoRenderDataType->sInEmptyFrame);
		pVideoRenderDataType->nInFrameNodeNum++;
	}
	if (pVideoRenderDataType->nInFrameNodeNum == 0) {
		loge("mpp_alloc empty video node fail\n");
		eError = OMX_ErrorInsufficientResources;
		goto _EXIT2;
	}

	if (aic_msg_create(&pVideoRenderDataType->sMsgQue)<0)
	 {
		loge("aic_msg_create fail!");
		eError = OMX_ErrorInsufficientResources;
		goto _EXIT4;
	}

	pVideoRenderDataType->eClockState = OMX_TIME_ClockStateStopped;

	pVideoRenderDataType->nRotationAngle = MPP_ROTATION_0;
	pVideoRenderDataType->nRotationAngleChange = 0;
	pVideoRenderDataType->nInitRotationParam = 0;
	mpp_list_init(&pVideoRenderDataType->sDMAFd);
	pVideoRenderDataType->nRotationIndex = 1;

	pthread_mutex_init(&pVideoRenderDataType->stateLock, NULL);
	pthread_mutex_init(&pVideoRenderDataType->sWaitReayFrameLock, NULL);

	// Create the component thread
	err = pthread_create(&pVideoRenderDataType->threadId, NULL, OMX_VideoRenderComponentThread, pVideoRenderDataType);
	if (err || !pVideoRenderDataType->threadId) {
		loge("pthread_create fail!");
		eError = OMX_ErrorInsufficientResources;
		goto _EXIT5;
	}

	return eError;

_EXIT5:
	aic_msg_destroy(&pVideoRenderDataType->sMsgQue);
	pthread_mutex_destroy(&pVideoRenderDataType->stateLock);
	pthread_mutex_destroy(&pVideoRenderDataType->sWaitReayFrameLock);

_EXIT4:
	if (!mpp_list_empty(&pVideoRenderDataType->sInEmptyFrame)) {
		VIDEO_RENDER_IN_FRAME	*pFrameNode = NULL,*pFrameNode1 = NULL;
		mpp_list_for_each_entry_safe(pFrameNode, pFrameNode1, &pVideoRenderDataType->sInEmptyFrame, sList) {
			mpp_list_del(&pFrameNode->sList);
			mpp_free(pFrameNode);
		}
	}

_EXIT2:
	if (pVideoRenderDataType) {
		mpp_free(pVideoRenderDataType);
		pVideoRenderDataType = NULL;
	}

_EXIT1:
	return eError;
}

static void OMX_VideoRenderEventNotify(
		VIDEO_RENDER_DATA_TYPE * pVideoRenderDataType,
		OMX_EVENTTYPE event,
		OMX_U32 nData1,
		OMX_U32 nData2,
		OMX_PTR pEventData)
{
	if (pVideoRenderDataType && pVideoRenderDataType->pCallbacks && pVideoRenderDataType->pCallbacks->EventHandler)  {
		pVideoRenderDataType->pCallbacks->EventHandler(
					pVideoRenderDataType->hSelf,
					pVideoRenderDataType->pAppData,event,
					nData1, nData2, pEventData);
	}
}

static void OMX_VideoRenderStateChangeToInvalid(VIDEO_RENDER_DATA_TYPE * pVideoRenderDataType)
{
	pVideoRenderDataType->state = OMX_StateInvalid;
	OMX_VideoRenderEventNotify(pVideoRenderDataType
						,OMX_EventError
						,OMX_ErrorInvalidState,0,NULL);
	OMX_VideoRenderEventNotify(pVideoRenderDataType
						,OMX_EventCmdComplete
						,OMX_CommandStateSet
						,pVideoRenderDataType->state,NULL);

}

static void OMX_VideoRenderStateChangeToLoaded(VIDEO_RENDER_DATA_TYPE * pVideoRenderDataType)
{
	if (pVideoRenderDataType->state == OMX_StateIdle) {
		OMX_VideoRenderGiveBackAllFrames(pVideoRenderDataType);
	} else if (pVideoRenderDataType->state == OMX_StateExecuting) {

	} else if (pVideoRenderDataType->state == OMX_StatePause) {

	} else  {
		OMX_VideoRenderEventNotify(pVideoRenderDataType
									,OMX_EventError
									,OMX_ErrorIncorrectStateTransition
									,pVideoRenderDataType->state,NULL);
		return;
	}
	pVideoRenderDataType->state = OMX_StateLoaded;
	OMX_VideoRenderEventNotify(pVideoRenderDataType
									,OMX_EventCmdComplete
									,OMX_CommandStateSet
									,pVideoRenderDataType->state,NULL);

}

static void OMX_VideoRenderStateChangeToIdle(VIDEO_RENDER_DATA_TYPE * pVideoRenderDataType)
{
	int ret = 0;
	if (pVideoRenderDataType->state == OMX_StateLoaded) {
		//create video_handle
	if (!pVideoRenderDataType->render) {
		 ret = aic_video_render_create(&pVideoRenderDataType->render);
	}
	if (ret != 0) {
		loge("aic_video_render_create fail\n");
		OMX_VideoRenderEventNotify(pVideoRenderDataType
									,OMX_EventError
									,OMX_ErrorIncorrectStateTransition
									,pVideoRenderDataType->state,NULL);
		return;
	}

	} else if (pVideoRenderDataType->state == OMX_StatePause) {

	} else if (pVideoRenderDataType->state == OMX_StateExecuting) {

	} else {
		OMX_VideoRenderEventNotify(pVideoRenderDataType
									,OMX_EventError
									,OMX_ErrorIncorrectStateTransition
									,pVideoRenderDataType->state,NULL);

		return;
	}
	pVideoRenderDataType->state = OMX_StateIdle;
	OMX_VideoRenderEventNotify(pVideoRenderDataType
									,OMX_EventCmdComplete
									,OMX_CommandStateSet
									,pVideoRenderDataType->state,NULL);

}

static void OMX_VideoRenderStateChangeToPause(VIDEO_RENDER_DATA_TYPE * pVideoRenderDataType)
{
	OMX_PORT_TUNNELEDINFO *pTunneldClock = &pVideoRenderDataType->sInPortTunneledInfo[VIDEO_RENDER_PORT_IN_CLOCK_INDEX];

	if (pVideoRenderDataType->state == OMX_StateLoaded) {

	} else if (pVideoRenderDataType->state == OMX_StateIdle) {

	} else if (pVideoRenderDataType->state == OMX_StateExecuting) {
		OMX_TIME_CONFIG_TIMESTAMPTYPE sTimeStamp;
		if (pTunneldClock->nTunneledFlag) {
			OMX_GetConfig(pTunneldClock->pTunneledComp,OMX_IndexConfigTimeCurrentMediaTime, &sTimeStamp);
			//loge("Excuting--->Pause,sTimeStamp:%ld\n",sTimeStamp.nTimestamp);
		} else {
			pVideoRenderDataType->sPauseTimePoint = OMX_ClockGetSystemTime();
			//loge("Excuting--->Pause,sPauseTimePoint:%ld\n",pVideoRenderDataType->sPauseTimePoint);
		}
	} else {
		OMX_VideoRenderEventNotify(pVideoRenderDataType
									,OMX_EventError
									,OMX_ErrorIncorrectStateTransition
									,pVideoRenderDataType->state,NULL);
		return;

	}
	pVideoRenderDataType->state = OMX_StatePause;
	OMX_VideoRenderEventNotify(pVideoRenderDataType
									,OMX_EventCmdComplete
									,OMX_CommandStateSet
									,pVideoRenderDataType->state,NULL);

}

static void OMX_VideoRenderStateChangeToExecuting(VIDEO_RENDER_DATA_TYPE * pVideoRenderDataType)
{
	if (pVideoRenderDataType->state == OMX_StateLoaded) {
		OMX_VideoRenderEventNotify(pVideoRenderDataType
									,OMX_EventError
									,OMX_ErrorIncorrectStateTransition
									,pVideoRenderDataType->state,NULL);

		return;

	} else if (pVideoRenderDataType->state == OMX_StateIdle) {

	} else if (pVideoRenderDataType->state == OMX_StatePause) {
		OMX_TIME_CONFIG_TIMESTAMPTYPE sTimeStamp;
		OMX_PORT_TUNNELEDINFO *pTunneldClock = &pVideoRenderDataType->sInPortTunneledInfo[VIDEO_RENDER_PORT_IN_CLOCK_INDEX];
		if (pTunneldClock->nTunneledFlag) {
			OMX_GetConfig(pTunneldClock->pTunneledComp,OMX_IndexConfigTimeCurrentMediaTime, &sTimeStamp);
			//loge("Pause--->Excuting,sTimeStamp:%ld\n",sTimeStamp.nTimestamp);
		} else {
			pVideoRenderDataType->sPauseTimeDurtion += (OMX_ClockGetSystemTime() - pVideoRenderDataType->sPauseTimePoint);
			//loge("Pause--->Excuting,sPauseTimePoint:%ld,curTime:%ld,pauseDura:%ld\n",pVideoRenderDataType->sPauseTimePoint,OMX_ClockGetSystemTime(),pVideoRenderDataType->sPauseTimeDurtion);
		}
	} else {
		OMX_VideoRenderEventNotify(pVideoRenderDataType
									,OMX_EventError
									,OMX_ErrorIncorrectStateTransition
									,pVideoRenderDataType->state,NULL);

		return;
	}
	pVideoRenderDataType->state = OMX_StateExecuting;
	OMX_VideoRenderEventNotify(pVideoRenderDataType
									,OMX_EventCmdComplete
									,OMX_CommandStateSet
									,pVideoRenderDataType->state,NULL);

}

static OMX_S64 OMX_ClockGetSystemTime()
{
	struct timespec ts =  {0,0};
	OMX_S64 tick = 0;
	clock_gettime(CLOCK_MONOTONIC,&ts);
	tick = ts.tv_sec*1000000 + ts.tv_nsec/1000;
	return tick;
}

typedef enum OMX_VIDEO_SYNC_TYPE {
	OMX_VIDEO_SYNC_INVAILD = -1,
	OMX_VIDEO_SYNC_DROP = 0,
	OMX_VIDEO_SYNC_DEALY,
	OMX_VIDEO_SYNC_SHOW,
} OMX_VIDEO_SYNC_TYPE;

static OMX_TICKS OMX_VdieoRenderGetMediaTime(VIDEO_RENDER_DATA_TYPE* pVideoRenderDataType)
{
	OMX_TICKS sMeidaTime;
	sMeidaTime = OMX_ClockGetSystemTime()- pVideoRenderDataType->sWallTimeBase
				- pVideoRenderDataType->sPauseTimeDurtion + pVideoRenderDataType->sFisrtShowPts;
	return sMeidaTime;
}

static void OMX_VdieoRenderSetMediaClock(VIDEO_RENDER_DATA_TYPE* pVideoRenderDataType,struct mpp_frame *pFrameInfo)
{
	pVideoRenderDataType->sWallTimeBase =  OMX_ClockGetSystemTime();
	pVideoRenderDataType->sPauseTimeDurtion = 0;
	pVideoRenderDataType->sFisrtShowPts = pFrameInfo->pts;
	pVideoRenderDataType->sPreFramePts = pFrameInfo->pts;
// 	loge("pVideoRenderDataType->sFisrtShowPts:%ld,pVideoRenderDataType->sWallTimeBase:%ld\n"
// 		,pVideoRenderDataType->sFisrtShowPts
// 		,pVideoRenderDataType->sWallTimeBase);
 }

static int OMX_GiveBackProcessedFrames(VIDEO_RENDER_DATA_TYPE* pVideoRenderDataType)
{
	int ret = 0;
	OMX_S32 nCntInUsingList = 0;
	VIDEO_RENDER_IN_FRAME *pFrameNode;
	OMX_PORT_TUNNELEDINFO *pTunneldVideo;
	pTunneldVideo = &pVideoRenderDataType->sInPortTunneledInfo[VIDEO_RENDER_PORT_IN_VIDEO_INDEX];

	mpp_list_for_each_entry(pFrameNode, &pVideoRenderDataType->sInProcessedFrmae, sList) {
		nCntInUsingList++;
	}
	while(nCntInUsingList > 1) {
		OMX_BUFFERHEADERTYPE sBuffHead;
		aic_pthread_mutex_lock(&pVideoRenderDataType->sInFrameLock);
		pFrameNode = mpp_list_first_entry(&pVideoRenderDataType->sInProcessedFrmae, VIDEO_RENDER_IN_FRAME, sList);
		aic_pthread_mutex_unlock(&pVideoRenderDataType->sInFrameLock);
		ret = -1;
		if (pTunneldVideo->nTunneledFlag) {
			sBuffHead.pBuffer = (OMX_U8 *)&pFrameNode->sFrameInfo;
			sBuffHead.nInputPortIndex = VIDEO_RENDER_PORT_IN_VIDEO_INDEX;
			sBuffHead.nOutputPortIndex = pTunneldVideo->nTunnelPortIndex;
			ret = OMX_FillThisBuffer(pTunneldVideo->pTunneledComp,&sBuffHead);
		} else {
			sBuffHead.pBuffer = (OMX_U8 *)&pFrameNode->sFrameInfo;
			if (pVideoRenderDataType->pCallbacks->EmptyBufferDone) {
				ret = pVideoRenderDataType->pCallbacks->EmptyBufferDone(pVideoRenderDataType->hSelf
														,pVideoRenderDataType->pAppData,&sBuffHead);
			}
		}

		if (ret == 0) {
			aic_pthread_mutex_lock(&pVideoRenderDataType->sInFrameLock);
			mpp_list_del(&pFrameNode->sList);
			mpp_list_add_tail(&pFrameNode->sList, &pVideoRenderDataType->sInEmptyFrame);
			aic_pthread_mutex_unlock(&pVideoRenderDataType->sInFrameLock);
			pVideoRenderDataType->nGiveBackFrameOkNum++;
			logd("give back frame to vdec ok");
			nCntInUsingList--;
		} else { // how to do ,do nothing or move to empty list,now move to  empty list
			loge("give back frame to vdec fail\n");
			pVideoRenderDataType->nGiveBackFrameFailNum++;
			break;
		}
		logi("nGiveBackFrameOkNum:%d,nGiveBackFrameFailNum:%d\n"
			,pVideoRenderDataType->nGiveBackFrameOkNum,pVideoRenderDataType->nGiveBackFrameFailNum);
	}
	return ret;
}

#define MAX_DIFFF_TIME (10*1000) // 5ms need to to discuss
static OMX_VIDEO_SYNC_TYPE OMX_VdieoRenderProcessVideoSync(VIDEO_RENDER_DATA_TYPE* pVideoRenderDataType,struct mpp_frame *pFrameInfo,OMX_TICKS *nDelay)
{
	OMX_TICKS sDelayime;
	OMX_TIME_CONFIG_TIMESTAMPTYPE sTimeStamp;
	OMX_VIDEO_SYNC_TYPE eSyncType;
	OMX_PORT_TUNNELEDINFO *pTunneldClock = &pVideoRenderDataType->sInPortTunneledInfo[VIDEO_RENDER_PORT_IN_CLOCK_INDEX];
	if (pVideoRenderDataType == NULL || pFrameInfo == NULL || nDelay == NULL) {
			return OMX_VIDEO_SYNC_INVAILD;
	}
	if (pTunneldClock->nTunneledFlag) {
		sTimeStamp.nPortIndex = pTunneldClock->nTunnelPortIndex;
		OMX_GetConfig(pTunneldClock->pTunneledComp,OMX_IndexConfigTimeCurrentMediaTime, &sTimeStamp);
		if (sTimeStamp.nTimestamp == -1) {
			/*
				1 need to define valid pts,suppose pts = -1.
				2 after start up or seek, audio do not come yet,so get media time is invaild.
				when vaild audio do not come,control by video self
			*/
			sDelayime = pFrameInfo->pts -  OMX_VdieoRenderGetMediaTime(pVideoRenderDataType);
			//
			if (sDelayime > 10*1000*1000 || sDelayime < -10*1000*1000) {
				loge("tunneld clock ,but audio do not come ,vidoe pts jump event!!!\n");
				sDelayime = 40*1000;//this should  arccording to fame rate
				OMX_VdieoRenderSetMediaClock(pVideoRenderDataType,pFrameInfo);
			}
		} else {
			sDelayime = pFrameInfo->pts - sTimeStamp.nTimestamp;
			logd("pFrameInfo->pts:%lld,sTimeStamp.nTimestamp:%ld\n",pFrameInfo->pts,sTimeStamp.nTimestamp);
			if (sDelayime > 10*1000*1000 || sDelayime < -10*1000*1000) {
				if (MPP_ABS(pFrameInfo->pts,pVideoRenderDataType->sPreFramePts) > 10*1000*1000) {//video pts jump
					loge("video pts jump event!!!\n");
				} else {//audio pts jump
					loge("audio pts jump event!!!\n");
				}
			}
		}
	} else {// frame rate control by videoRender self
		sDelayime = pFrameInfo->pts -  OMX_VdieoRenderGetMediaTime(pVideoRenderDataType);
		if (sDelayime > 10*1000*1000 || sDelayime < -10*1000*1000) {//pts jump event
			loge("not tunneled clock ,vidoe pts jump event!!!\n");
			sDelayime = 40*1000;//this should  arccording to fame rate
			OMX_VdieoRenderSetMediaClock(pVideoRenderDataType,pFrameInfo);
		}
	}

	if (pFrameInfo->flags & FRAME_FLAG_EOS) {
		logi("pts: %lld\n",pFrameInfo->pts);
	}

#if 1
	if (sDelayime > MAX_DIFFF_TIME) {
		eSyncType = OMX_VIDEO_SYNC_DEALY;
	} else if (sDelayime > -MAX_DIFFF_TIME) {
		eSyncType = OMX_VIDEO_SYNC_SHOW;
	} else {
		eSyncType = OMX_VIDEO_SYNC_DROP;
	}
#else
	if (sDelayime > 1*1000*1000) {
		eSyncType = OMX_VIDEO_SYNC_SHOW;
	} else if (sDelayime > MAX_DIFFF_TIME) {
		eSyncType = OMX_VIDEO_SYNC_DEALY;
	} else if (sDelayime > -MAX_DIFFF_TIME) {
		eSyncType = OMX_VIDEO_SYNC_SHOW;
	} else if (sDelayime > -1*1000*1000) {
		eSyncType = OMX_VIDEO_SYNC_DROP;
	} else {
		eSyncType = OMX_VIDEO_SYNC_SHOW;
	}
#endif

	*nDelay = sDelayime;

	return eSyncType;
}

//#define  OMX_VIDEORENDER_ENABLE_DUMP_PIC

#ifdef OMX_VIDEORENDER_ENABLE_DUMP_PIC
static int OMX_VideoRenderDumpPic(struct mpp_buf* video,int index)
{
	int i;
	int data_size[3] =  {0, 0, 0};
	unsigned char* hw_data[3] =  {0};
	int comp = 3;
	FILE* fp_save = NULL;
	char fileName[255] =  {0};
	if (video->format == MPP_FMT_YUV420P)  {
		comp = 3;
		data_size[0] = video->size.height * video->stride[0];
		data_size[1] = data_size[2] = data_size[0]/4;
	}  else if (video->format == MPP_FMT_NV12 || video->format == MPP_FMT_NV21)  {
		comp = 2;
		data_size[0] = video->size.height * video->stride[0];
		data_size[1] =  data_size[0]/2;
	}  else if (video->format == MPP_FMT_YUV444P)  {
		comp = 3;
		data_size[0] = video->size.height * video->stride[0];
		data_size[1] = data_size[2] = data_size[0];
	}  else if (video->format == MPP_FMT_YUV422P)  {
		comp = 3;
		data_size[0] = video->size.height * video->stride[0];
		data_size[1] = data_size[2] = data_size[0]/2;
	}  else if (video->format == MPP_FMT_RGBA_8888 || video->format == MPP_FMT_BGRA_8888
		|| video->format == MPP_FMT_ARGB_8888 || video->format == MPP_FMT_ABGR_8888)  {
		comp = 1;
		data_size[0] = video->size.height * video->stride[0];
	}  else if (video->format == MPP_FMT_RGB_888 || video->format == MPP_FMT_BGR_888)  {
		comp = 1;
		data_size[0] = video->size.height * video->stride[0];
	}  else if (video->format == MPP_FMT_RGB_565 || video->format == MPP_FMT_BGR_565)  {
		comp = 1;
		data_size[0] = video->size.height * video->stride[0];
	}
	logd("data_size: %d %d %d, height: %d, stride: %d, format: %d",
		data_size[0], data_size[1], data_size[2],
		video->size.height, video->stride[0], video->format);
	snprintf(fileName,sizeof(fileName),"/mnt/video/pics/pic%d.yuv",index);
	fp_save = fopen(fileName, "wb");
	if (fp_save  == NULL) {
		loge("fopen %s error\n",fileName);
		return -1;
	}
	logd("fopen %s ok\n",fileName);
	// mmap dmabuf to virtual space and save the frame yuv data
	for(i=0; i<comp; i++)  {
		hw_data[i] = mmap(NULL, data_size[i], PROT_READ, MAP_SHARED, video->fd[i], 0);
		if (hw_data[i] == MAP_FAILED) {
			loge("dmabuf alloc mmap failed!");
			return -1;
		}
		if (fp_save)
			fwrite(hw_data[i], 1, data_size[i], fp_save);
	}
	fclose(fp_save);
	for(i=0; i<comp; i++)  {
		munmap(hw_data[i], data_size[i]);
	}
	return 0;
}
#endif

static void* OMX_VideoRenderComponentThread(void* pThreadData)
{
	struct aic_message message;
	OMX_S32 nCmd;		//OMX_COMMANDTYPE
	OMX_S32 nCmdData;	//OMX_STATETYPE
	VIDEO_RENDER_DATA_TYPE* pVideoRenderDataType = (VIDEO_RENDER_DATA_TYPE*)pThreadData;
	OMX_S32 ret;
	OMX_S32 bNotifyFrameEnd = 0;
	VIDEO_RENDER_IN_FRAME *pFrameNode;
	//prctl(PR_SET_NAME,(u32)"VideoRender");
	OMX_PORT_TUNNELEDINFO *pTunneldClock;
	pTunneldClock = &pVideoRenderDataType->sInPortTunneledInfo[VIDEO_RENDER_PORT_IN_CLOCK_INDEX];

	while(1) {
_AIC_MSG_GET_:
		if  (aic_msg_get(&pVideoRenderDataType->sMsgQue, &message) == 0) {
			nCmd = message.message_id;
			nCmdData = message.param;
			logi("nCmd:%d, nCmdData:%d\n",nCmd,nCmdData);
			if (OMX_CommandStateSet == nCmd) {
				aic_pthread_mutex_lock(&pVideoRenderDataType->stateLock);
				if (pVideoRenderDataType->state == (OMX_STATETYPE)(nCmdData)) {
					OMX_VideoRenderEventNotify(pVideoRenderDataType,OMX_EventError,OMX_ErrorSameState,0,NULL);
					aic_pthread_mutex_unlock(&pVideoRenderDataType->stateLock);
					continue;
				}
				switch(nCmdData) {
					case OMX_StateInvalid:
						OMX_VideoRenderStateChangeToInvalid(pVideoRenderDataType);
						break;
					case OMX_StateLoaded:
						OMX_VideoRenderStateChangeToLoaded(pVideoRenderDataType);
						break;
					case OMX_StateIdle:
						OMX_VideoRenderStateChangeToIdle(pVideoRenderDataType);
						break;
					case OMX_StateExecuting:
						OMX_VideoRenderStateChangeToExecuting(pVideoRenderDataType);
						break;
					case OMX_StatePause:
						OMX_VideoRenderStateChangeToPause(pVideoRenderDataType);
						break;
					default:
						break;
				}
				aic_pthread_mutex_unlock(&pVideoRenderDataType->stateLock);
			} else if (OMX_CommandFlush == nCmd) {

			} else if (OMX_CommandPortDisable == nCmd) {

			} else if (OMX_CommandPortEnable == nCmd) {

			} else if (OMX_CommandMarkBuffer == nCmd) {

			} else if (OMX_CommandStop == nCmd) {
				logi("OMX_VideoRenderComponentThread ready to exit!!!\n");
				goto _EXIT;
			} else {

			}
		}
		if (pVideoRenderDataType->state != OMX_StateExecuting) {
			//usleep(1000);
			aic_msg_wait_new_msg(&pVideoRenderDataType->sMsgQue, 0);
			continue;
		}

		if (pVideoRenderDataType->nFlags & VIDEO_RENDER_INPORT_SEND_ALL_FRAME_FLAG) {
			if (!bNotifyFrameEnd) {
				OMX_VideoRenderEventNotify(pVideoRenderDataType,OMX_EventBufferFlag,0,0,NULL);
				bNotifyFrameEnd = 1;
			}
			aic_msg_wait_new_msg(&pVideoRenderDataType->sMsgQue, 0);
			//usleep(1000);
			continue;
		}
		bNotifyFrameEnd = 0;

		OMX_GiveBackProcessedFrames(pVideoRenderDataType);

		if (OMX_VideoRenderListEmpty(&pVideoRenderDataType->sInReadyFrame,pVideoRenderDataType->sInFrameLock)) {
			struct timespec before = {0},after = {0};
			long diff;
			aic_pthread_mutex_lock(&pVideoRenderDataType->sWaitReayFrameLock);
			pVideoRenderDataType->nWaitReayFrameFlag = 1;
			aic_pthread_mutex_unlock(&pVideoRenderDataType->sWaitReayFrameLock);

			clock_gettime(CLOCK_REALTIME,&before);
			aic_msg_wait_new_msg(&pVideoRenderDataType->sMsgQue, 0);
			clock_gettime(CLOCK_REALTIME,&after);
			diff = (after.tv_sec - before.tv_sec)*1000*1000 + (after.tv_nsec - before.tv_nsec)/1000;
			if (diff > 50*1000) {
				printf("[%s:%d]:%ld\n",__FUNCTION__,__LINE__,diff);
			}
			goto _AIC_MSG_GET_;
		}

		while(!OMX_VideoRenderListEmpty(&pVideoRenderDataType->sInReadyFrame,pVideoRenderDataType->sInFrameLock)) {
			aic_pthread_mutex_lock(&pVideoRenderDataType->sInFrameLock);
			pFrameNode = mpp_list_first_entry(&pVideoRenderDataType->sInReadyFrame, VIDEO_RENDER_IN_FRAME, sList);
			aic_pthread_mutex_unlock(&pVideoRenderDataType->sInFrameLock);
			if (pVideoRenderDataType->nFrameFisrtShowFlag) {
				struct mpp_size sSize;
				struct mpp_rect sDisRect;
				if (!pVideoRenderDataType->nVideoRenderInitFlag) {
					ret = pVideoRenderDataType->render->init(pVideoRenderDataType->render,pVideoRenderDataType->nLayerId,pVideoRenderDataType->nDevId);
					if (!ret) {
						printf("[%s:%d]pVideoRenderDataType->render->init ok\n",__FUNCTION__,__LINE__);
						pVideoRenderDataType->nVideoRenderInitFlag = 1;
					} else {
						loge("pVideoRenderDataType->render->init fail\n");
					}
				}

				if (pTunneldClock->nTunneledFlag) {
					OMX_TIME_CONFIG_TIMESTAMPTYPE sTimeStamp;
					sTimeStamp.nPortIndex = pTunneldClock->nTunnelPortIndex;
					sTimeStamp.nTimestamp = pFrameNode->sFrameInfo.pts;
					OMX_SetConfig(pTunneldClock->pTunneledComp,OMX_IndexConfigTimeClientStartTime, &sTimeStamp);
					// whether need to wait????
					// printf("[%s:%d]wait audio start time\n",__FUNCTION__,__LINE__);
					// while(pVideoRenderDataType->eClockState != OMX_TIME_ClockStateRunning) {
					// 		usleep(1*1000);
					// }
					if (pVideoRenderDataType->eClockState != OMX_TIME_ClockStateRunning) {
						aic_msg_wait_new_msg(&pVideoRenderDataType->sMsgQue, 1*1000);
						goto _AIC_MSG_GET_;
					}
					printf("[%s:%d]audio start time arrive\n",__FUNCTION__,__LINE__);

				} else {//if  it does not tunneld with clock ,it need calcuaute media time by self for control frame rate
					OMX_VdieoRenderSetMediaClock(pVideoRenderDataType,&pFrameNode->sFrameInfo);
				}

				//pVideoRenderDataType->render->set_on_off(pVideoRenderDataType->render,0);
				pVideoRenderDataType->render->get_screen_size(pVideoRenderDataType->render,&sSize);
				sDisRect.x = 0;
				sDisRect.y = 0;
				sDisRect.width = sSize.width;
				sDisRect.height = sSize.height;
				//sDisRect.width = 1024;
				//sDisRect.height = 600;
				if (pVideoRenderDataType->nDisRectChange) {
					pVideoRenderDataType->render->set_dis_rect(pVideoRenderDataType->render,&pVideoRenderDataType->sDisRect);
					logi("init dis rect:[%d,%d,%d,%d]!!!\n"
						,pVideoRenderDataType->sDisRect.x
						,pVideoRenderDataType->sDisRect.y
						,pVideoRenderDataType->sDisRect.width
						,pVideoRenderDataType->sDisRect.height);
					pVideoRenderDataType->nDisRectChange = OMX_FALSE;
				} else {
					pVideoRenderDataType->render->set_dis_rect(pVideoRenderDataType->render,&sDisRect);
					printf("[%s:%d]init dis rect:[%d,%d,%d,%d]!!!\n",__FUNCTION__,__LINE__,sDisRect.x,sDisRect.y,sDisRect.width,sDisRect.height);
				}

				printf("[%s:%d]stride[0]:%d,stride[1]:%d,stride[2]:%d,format:%d,width:%d,height:%d"\
					"crop_en:%d,crop.x:%d,crop.y:%d,crop.width:%d,crop.height:%d\n"
					,__FUNCTION__,__LINE__
					,pFrameNode->sFrameInfo.buf.stride[0]
					,pFrameNode->sFrameInfo.buf.stride[1]
					,pFrameNode->sFrameInfo.buf.stride[2]
					,pFrameNode->sFrameInfo.buf.format
					,pFrameNode->sFrameInfo.buf.size.width
					,pFrameNode->sFrameInfo.buf.size.height
					,pFrameNode->sFrameInfo.buf.crop_en
					,pFrameNode->sFrameInfo.buf.crop.x
					,pFrameNode->sFrameInfo.buf.crop.y
					,pFrameNode->sFrameInfo.buf.crop.width
					,pFrameNode->sFrameInfo.buf.crop.height);
				pVideoRenderDataType->nDumpIndex = 0;

				if (pVideoRenderDataType->nRotationAngleChange) {
					OMX_VideoRenderInitRotationParam(pVideoRenderDataType,&pFrameNode->sFrameInfo);
					pVideoRenderDataType->nRotationAngleChange = 0;
				}

				OMX_VideoRenderRotateFrame(pVideoRenderDataType,&pFrameNode->sFrameInfo);

			#ifdef OMX_VIDEORENDER_ENABLE_DUMP_PIC
				OMX_VideoRenderDumpPic(&pVideoRenderDataType->pCurDisplayFrame->buf,pVideoRenderDataType->nDumpIndex++);
			#endif
				ret = pVideoRenderDataType->render->rend(pVideoRenderDataType->render,pVideoRenderDataType->pCurDisplayFrame);

				//ret = pVideoRenderDataType->render->rend(pVideoRenderDataType->render,&pFrameNode->sFrameInfo);
				//pVideoRenderDataType->render->set_on_off(pVideoRenderDataType->render,1);
				aic_pthread_mutex_lock(&pVideoRenderDataType->sInFrameLock);
				if (ret == 0) {
					mpp_list_del(&pFrameNode->sList);
					mpp_list_add_tail(&pFrameNode->sList, &pVideoRenderDataType->sInProcessedFrmae);
					pVideoRenderDataType->nShowFrameOkNum++;
					pVideoRenderDataType->nFrameFisrtShowFlag = OMX_FALSE;
					OMX_VideoRenderEventNotify(pVideoRenderDataType,OMX_EventVideoRenderFirstFrame,0,0,NULL);
					if (pFrameNode->sFrameInfo.flags & FRAME_FLAG_EOS) {
						pVideoRenderDataType->nFlags  |= VIDEO_RENDER_INPORT_SEND_ALL_FRAME_FLAG;
						//pVideoRenderDataType->nFrameEndFlag = OMX_TRUE;
						printf("[%s:%d]receive nFrameEndFlag\n",__FUNCTION__,__LINE__);
					}
				} else {
					// how to do ,now deal with  same success
					loge("render error");
					mpp_list_del(&pFrameNode->sList);
					mpp_list_add_tail(&pFrameNode->sList, &pVideoRenderDataType->sInProcessedFrmae);
					pVideoRenderDataType->nShowFrameFailNum++;
					break;
				}
				aic_pthread_mutex_unlock(&pVideoRenderDataType->sInFrameLock);

				logd("nReceiveFrameNum:%d,nShowFrameOkNum:%d,nShowFrameFailNum:%d\n"
					,pVideoRenderDataType->nReceiveFrameNum
					,pVideoRenderDataType->nShowFrameOkNum
					,pVideoRenderDataType->nShowFrameFailNum);
			} else {// not fisrt show
				OMX_S32 data1,data2;
				OMX_TICKS sDelayime;
				OMX_VIDEO_SYNC_TYPE eSyncType;
				static struct timespec pev =  {0} ,cur =  {0};
				//loge("!!!!!!!!!!!!!!! ReadyFrame!!!!!!!!! \n");
				eSyncType = OMX_VdieoRenderProcessVideoSync(pVideoRenderDataType,&pFrameNode->sFrameInfo,&sDelayime);
				//eSyncType = OMX_VIDEO_SYNC_SHOW;

				if (pev.tv_sec == 0) {
					clock_gettime(CLOCK_REALTIME,&pev);
				} else {
					long diff = 0;
					clock_gettime(CLOCK_REALTIME,&cur);
					diff = (cur.tv_sec - pev.tv_sec)*1000*1000 + (cur.tv_nsec - pev.tv_nsec)/1000;
					if (diff > 42*1000) {
						printf("[%s:%d]threadId:%ld,diff:%ld,sDelayime:%ld,eSyncType:%d,pts:%lld\n"
								,__FUNCTION__,__LINE__,syscall(SYS_gettid)
								,diff
								,sDelayime
								,eSyncType
								,pFrameNode->sFrameInfo.pts);
					}
					pev = cur;
				}

				if (eSyncType == OMX_VIDEO_SYNC_SHOW) {

					if (pVideoRenderDataType->nRotationAngleChange) {
						OMX_VideoRenderInitRotationParam(pVideoRenderDataType,&pFrameNode->sFrameInfo);
						pVideoRenderDataType->nRotationAngleChange = 0;
					}
					//time_start(RotateFrame);
					OMX_VideoRenderRotateFrame(pVideoRenderDataType,&pFrameNode->sFrameInfo);
					//time_end(RotateFrame);
				#ifdef OMX_VIDEORENDER_ENABLE_DUMP_PIC
					if (pVideoRenderDataType->nDumpIndex < 24) {
						OMX_VideoRenderDumpPic(&pVideoRenderDataType->pCurDisplayFrame->buf,pVideoRenderDataType->nDumpIndex++);
					}
				#endif
					ret = pVideoRenderDataType->render->rend(pVideoRenderDataType->render,pVideoRenderDataType->pCurDisplayFrame);

					//ret = pVideoRenderDataType->render->rend(pVideoRenderDataType->render,&pFrameNode->sFrameInfo);

					aic_pthread_mutex_lock(&pVideoRenderDataType->sInFrameLock);
					if (ret == 0) {
						mpp_list_del(&pFrameNode->sList);
						mpp_list_add_tail(&pFrameNode->sList, &pVideoRenderDataType->sInProcessedFrmae);
						pVideoRenderDataType->nShowFrameOkNum++;
						if (pFrameNode->sFrameInfo.flags & FRAME_FLAG_EOS) {
							pVideoRenderDataType->nFlags  |= VIDEO_RENDER_INPORT_SEND_ALL_FRAME_FLAG;
							//pVideoRenderDataType->nFrameEndFlag = OMX_TRUE;
							printf("[%s:%d]receive nFrameEndFlag\n",__FUNCTION__,__LINE__);
						}
					} else {
						// how to do ,now deal with  same success
						loge("render error");
						mpp_list_del(&pFrameNode->sList);
						mpp_list_add_tail(&pFrameNode->sList, &pVideoRenderDataType->sInProcessedFrmae);
						pVideoRenderDataType->nShowFrameFailNum++;
						break;
					}
					aic_pthread_mutex_unlock(&pVideoRenderDataType->sInFrameLock);

					//OMX_GiveBackProcessedFrames(pVideoRenderDataType);

					data1 = (pFrameNode->sFrameInfo.pts >> 32) & 0x00000000ffffffff;
					data2 = pFrameNode->sFrameInfo.pts & 0x00000000ffffffff;
					//loge("OMX_VIDEO_SYNC_SHOW:%ld\n",sDelayime);
					OMX_VideoRenderEventNotify(pVideoRenderDataType,OMX_EventVideoRenderPts,data1,data2,NULL);
				} else if (eSyncType == OMX_VIDEO_SYNC_DROP) {
					static int nDropNum = 0;
					if (pFrameNode->sFrameInfo.flags & FRAME_FLAG_EOS) {
						pVideoRenderDataType->nFlags  |= VIDEO_RENDER_INPORT_SEND_ALL_FRAME_FLAG;
						printf("[%s:%d]receive nFrameEndFlag\n",__FUNCTION__,__LINE__);
					}
					aic_pthread_mutex_lock(&pVideoRenderDataType->sInFrameLock);
					mpp_list_del(&pFrameNode->sList);
					mpp_list_add_tail(&pFrameNode->sList, &pVideoRenderDataType->sInProcessedFrmae);
					pVideoRenderDataType->nDropFrameNum++;
					nDropNum++;
					if (nDropNum  > 50) {
						nDropNum = 0;
						printf("[%s:%d]OMX_VIDEO_SYNC_DROP:nDropFrameNum:%d,sDelayime:%ld\n",__FUNCTION__,__LINE__,pVideoRenderDataType->nDropFrameNum,sDelayime);
					}

					aic_pthread_mutex_unlock(&pVideoRenderDataType->sInFrameLock);
					//OMX_GiveBackProcessedFrames(pVideoRenderDataType);

				} else if (eSyncType == OMX_VIDEO_SYNC_DEALY) {
					struct timespec delay_before =  {0} ,delay_after =  {0};
					//long  delay = 0;
					//loge("OMX_VIDEO_SYNC_DEALY:%ld,%ld\n",sDelayime,sDelayime-MAX_DIFFF_TIME);
					clock_gettime(CLOCK_REALTIME,&delay_before);
					//aic_msg_wait_new_msg(&pVideoRenderDataType->sMsgQue, sDelayime - MAX_DIFFF_TIME);
					aic_msg_wait_new_msg(&pVideoRenderDataType->sMsgQue, MAX_DIFFF_TIME);
					//usleep(sDelayime - MAX_DIFFF_TIME);
					//aic_udelay(sDelayime - MAX_DIFFF_TIME);
					clock_gettime(CLOCK_REALTIME,&delay_after);
					//delay = (delay_after.tv_sec - delay_before.tv_sec)*1000*1000 + (delay_after.tv_nsec - delay_before.tv_nsec)/1000;
					//printf("[%s:%d]:%ld,%ld\n",__FUNCTION__,__LINE__,delay,sDelayime - MAX_DIFFF_TIME);
				} else {
					//logd("OMX_VIDEO_SYNC_DEALY:%lld\n",sDelayime);
				}
			}
			if(pVideoRenderDataType->nDisRectChange) {
				pVideoRenderDataType->render->set_dis_rect(pVideoRenderDataType->render,&pVideoRenderDataType->sDisRect);
				logi("init dis rect:[%d,%d,%d,%d]!!!\n"
					,pVideoRenderDataType->sDisRect.x
					,pVideoRenderDataType->sDisRect.y
					,pVideoRenderDataType->sDisRect.width
					,pVideoRenderDataType->sDisRect.height);
				pVideoRenderDataType->nDisRectChange = OMX_FALSE;
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
			,pVideoRenderDataType->nReceiveFrameNum
			,pVideoRenderDataType->nLeftReadyFrameWhenCompoentExitNum
			,pVideoRenderDataType->nShowFrameOkNum
			,pVideoRenderDataType->nShowFrameFailNum
			,pVideoRenderDataType->nGiveBackFrameOkNum
			,pVideoRenderDataType->nGiveBackFrameFailNum);
	logi("OMX_VideoRenderComponentThread EXIT\n");
	return (void*)OMX_ErrorNone;
}

