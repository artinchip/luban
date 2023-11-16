/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: OMX_VdecComponent demo
*/

#include <string.h>
#include <malloc.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

#include "OMX_Core.h"
#include "OMX_CoreExt1.h"
#include "mpp_dec_type.h"
#include "mpp_list.h"
#include "mpp_log.h"
#include "mpp_mem.h"
#include "aic_message.h"
#include "bit_stream_parser.h"

#define  VDEC_EventCmdComplete  1
#define  VDEC_FILL_BUFFER_DONE  2
#define  VDEC_EVENT_ERROR  3
#define  VDEC_EventBufferFlag  4

struct packet_node {
	struct OMX_BUFFERHEADERTYPE  sBuff;
	struct mpp_list  sList;
};

#define PKT_MAX_NUM 16

static struct aic_message_queue g_msg_que;

static OMX_ERRORTYPE vdec_event_handler (
	OMX_HANDLETYPE hComponent,
	OMX_PTR pAppData,
	OMX_EVENTTYPE eEvent,
	OMX_U32 Data1,
	OMX_U32 Data2,
	OMX_PTR pEventData)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;

	struct aic_message msg;

	if(eEvent == OMX_EventCmdComplete){
		logw("vdec state set to %u\n", Data2);
		msg.message_id = VDEC_EventCmdComplete;
		msg.data_size = 0;
		aic_msg_put(&g_msg_que,&msg);
	}else if(eEvent == OMX_EventBufferFlag){
		logw("\nEOS ,0x%x\n",Data1);
		msg.message_id = VDEC_EventBufferFlag;
		msg.data_size = 0;
		aic_msg_put(&g_msg_que,&msg);
	}else if(eEvent == VDEC_EVENT_ERROR){

	}else{
		logw("param1 = 0x%x param2 = %d\n", (int)Data1, (int)Data2);
	}

	return eError;
}

static OMX_ERRORTYPE vdec_empty_buffer_done (
	OMX_HANDLETYPE hComponent,
	OMX_PTR pAppData,
	OMX_BUFFERHEADERTYPE* pBuffer)
{
		OMX_ERRORTYPE eError = OMX_ErrorNone;
		return eError;
}

static OMX_ERRORTYPE vdec_fill_buffer_done (
	OMX_HANDLETYPE hComponent,
	OMX_PTR pAppData,
	OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	struct aic_message msg;
	msg.message_id = VDEC_FILL_BUFFER_DONE;
	//msg.param = pBuffer->pBuffer;
	//msg.data_size = 0;
	msg.data = mpp_alloc(sizeof(OMX_U8*));
	memcpy(msg.data,&pBuffer->pBuffer,sizeof(OMX_U8*));
	msg.data_size = sizeof(OMX_U8*);
	aic_msg_put(&g_msg_que,&msg);
	logw("VDEC_FILL_BUFFER_DONE \n");
	return eError;
}

OMX_CALLBACKTYPE vdec_callbacks = {
	.EventHandler    = vdec_event_handler,
	.EmptyBufferDone = vdec_empty_buffer_done,
	.FillBufferDone  = vdec_fill_buffer_done
};

static void print_frame_info(struct mpp_frame* pFrameInfo)
{
	logi("flags:%d,"\
		"id:0x%xh,"\
		"pts:%lld,"\
		"buf_type:%d,"\
		"crop.x:%d,"\
		"crop.y:%d,"\
		"crop.width:%d,"\
		"crop.height:%d,"\
		"crop_en:%d,"\
		"fd[0]:%d,"\
		"fd[1]:%d,"\
		"fd[2]:%d,"\
		"flags:%d,"\
		"format:%d,"\
		"size.width:%d,"\
		"size.height:%d,"\
		"stride[0]:%d,"\
		"stride[1]:%d,"\
		"stride[2]:%d\n"\
		,pFrameInfo->flags
		,pFrameInfo->id
		,pFrameInfo->pts
		,pFrameInfo->buf.buf_type
		,pFrameInfo->buf.crop.x
		,pFrameInfo->buf.crop.y
		,pFrameInfo->buf.crop.width
		,pFrameInfo->buf.crop.height
		,pFrameInfo->buf.crop_en
		,pFrameInfo->buf.fd[0]
		,pFrameInfo->buf.fd[1]
		,pFrameInfo->buf.fd[2]
		,pFrameInfo->buf.flags
		,pFrameInfo->buf.format
		,pFrameInfo->buf.size.width
		,pFrameInfo->buf.size.height
		,pFrameInfo->buf.stride[0]
		,pFrameInfo->buf.stride[1]
		,pFrameInfo->buf.stride[2]);

}

int main(int argc,char **argv)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_BUFFERHEADERTYPE buff_head;
	OMX_VIDEO_PARAM_PORTFORMATTYPE port_format;
	OMX_HANDLETYPE vdecoder;
	struct mpp_packet  pkt;
	s32  pre_pkt_szie;
	struct aic_message msg;
	int file_fd;
	struct bit_stream_parser *parser;
	int bs_eof_flag = 0;
	struct mpp_frame   *pFrameInfo;
	OMX_STATETYPE state = OMX_StateInvalid;
	int try_times;
	int decode_count = 0;;
	if(argc < 2){
		loge("param error!!!\n");
		return -1;
	}
	memset(&pkt,0x00,sizeof(struct mpp_packet));
	pre_pkt_szie = 0;
	if(aic_msg_create(&g_msg_que) != 0){
		loge("aic_msg_create error!!!\n");
		goto _EXIT0;
	}

	logw("OMX_init!!!\n");
	eError = OMX_Init();
	if(eError != OMX_ErrorNone){
		loge("OMX_init error!!!\n");
		goto _EXIT1;
	}

	logw("OMX_GetHandle!!!\n");
	if (OMX_ErrorNone !=OMX_GetHandle(&vdecoder, OMX_COMPONENT_VDEC_NAME,NULL, &vdec_callbacks)){
		loge("unable to get demuxer handle.\n");
		goto _EXIT2;
	}

	logw("OMX_SetParameter!!!\n");
	port_format.nPortIndex = 0;
	port_format.eCompressionFormat = OMX_VIDEO_CodingAVC;
	port_format.eColorFormat = OMX_COLOR_FormatYUV420Planar;
	if(OMX_ErrorNone != OMX_SetParameter(vdecoder, OMX_IndexParamVideoPortFormat,&port_format)){
		loge("OMX_SetParameter Error!!!!.\n");
		goto _EXIT3;
	}

	logw("OMX_SendCommand OMX_StateIdle!!!\n");

	if (OMX_ErrorNone !=OMX_SendCommand(vdecoder, OMX_CommandStateSet, OMX_StateIdle, NULL)){
		loge("unable to set vdec to OMX_StateIdle.\n");
		goto _EXIT3;
	}

	do{
		OMX_GetState(vdecoder, &state);
		usleep(1000);
	}while(state != OMX_StateIdle);

	logw("vdecdoer state: OMX_StateLoaded-->OMX_StateIdle!!!\n");

	if (OMX_ErrorNone !=OMX_SendCommand(vdecoder, OMX_CommandStateSet, OMX_StateExecuting, NULL)){
		loge("unable to set vdec to OMX_StateExecuting.\n");
		goto _EXIT3;
	}

	do{
		OMX_GetState(vdecoder, &state);
		usleep(1000);
	}while(state != OMX_StateExecuting);

	logw("vdecdoer state: OMX_StateIdle-->OMX_StateExecuting!!!\n");

	file_fd = open(argv[1], O_RDONLY);
	if (file_fd < 0) {
		loge("failed to open input file %s", argv[1]);
		goto _EXIT3;
	}
	lseek(file_fd, 0, SEEK_SET);
	parser = bs_create(file_fd);

	while(1){
		if(aic_msg_get(&g_msg_que, &msg) == 0){
			if(msg.message_id == VDEC_FILL_BUFFER_DONE){
				//pFrameInfo  = msg.param;
				memcpy(&pFrameInfo,msg.data,sizeof(OMX_U8*));
				mpp_free(msg.data);
				msg.data = NULL;
				msg.data_size = 0;
				print_frame_info(pFrameInfo);
				buff_head.nOutputPortIndex = 1;
				buff_head.pBuffer = (OMX_U8 *)pFrameInfo;
				decode_count++;
				OMX_FillThisBuffer(vdecoder,&buff_head);
			}else if(msg.message_id == VDEC_EventCmdComplete){
				logw("receive  VDEC_EventCmdComplete \n");
			}else if(msg.message_id == VDEC_EventBufferFlag){ //end fo stream stop
				logw("receive end of stream from decoder,decode_count:%d!!!!\n",decode_count);
				OMX_SendCommand(vdecoder, OMX_CommandStateSet, OMX_StateIdle, NULL);
				do{
					OMX_GetState(vdecoder, &state);
					usleep(1000);
				}while(state != OMX_StateIdle);

				OMX_SendCommand(vdecoder, OMX_CommandStateSet, OMX_StateLoaded, NULL);
				do{
					OMX_GetState(vdecoder, &state);
					usleep(1000);
				}while(state != OMX_StateLoaded);
				break;

			}else{
				loge("unknown event");
			}
		}

		if(!bs_eof_flag){
			bs_prefetch(parser, &pkt);
			if((pre_pkt_szie < pkt.size) || (pkt.data == NULL)){
				if(pkt.data != NULL){
					free(pkt.data);
					pkt.data  = NULL;
				}
				logw("malloc pkt.szie:%d\n",pkt.size);
				pkt.data = malloc(pkt.size);
				pre_pkt_szie = pkt.size;
			}
			bs_read(parser, &pkt);
			buff_head.nFilledLen = pkt.size;
			buff_head.pBuffer = pkt.data;
			buff_head.nFlags = pkt.flag;
			buff_head.nTimeStamp = pkt.pts;
			try_times = 0;
			do{
				eError = OMX_EmptyThisBuffer(vdecoder,&buff_head);
				if(eError == OMX_ErrorNone){
					logw("OMX_EmptyThisBuffer ok\n");
					break;
				}
				usleep(1000);
				logw("try_times:%d,eError:0x%x\n",try_times++,eError);
			}while(1);
			if((pkt.flag & PACKET_FLAG_EOS)){
				bs_eof_flag = 1;
				logw("end of stream\n");
			}
		}
	}
	logw("decode_count:%d\n",decode_count);

_EXIT3:
	if(vdecoder){
		OMX_FreeHandle(vdecoder);
		vdecoder = NULL;
	}
_EXIT2:
	OMX_Deinit();
_EXIT1:
	aic_msg_destroy(&g_msg_que);
_EXIT0:
	return (int )eError;
}

