/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: OMX_VdecComponent tunneld  OMX_VideoRenderComponent demo
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
#include "aic_message.h"
#include "bit_stream_parser.h"

#define  VDEC_EventCmdComplete  1
#define  VDEC_FILL_BUFFER_DONE  2
#define  VDEC_EVENT_ERROR  3
#define  VDEC_EventBufferFlag  4
#define  VIDEO_RENDER_EventBufferFlag  5

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
		logw("\nVDEC_EVENT_ERROR ,0x%x,0x%x\n",Data1,Data2);
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

OMX_CALLBACKTYPE vdec_callbacks = {
	.EventHandler    = vdec_event_handler,
	.EmptyBufferDone = vdec_empty_buffer_done,
	//.FillBufferDone  = vdec_fill_buffer_done
	.FillBufferDone  = NULL
};

static OMX_ERRORTYPE video_render_event_handler (
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
		msg.message_id = VIDEO_RENDER_EventBufferFlag;
		msg.data_size = 0;
		aic_msg_put(&g_msg_que,&msg);
	}else if(eEvent == VDEC_EVENT_ERROR){
		logw("\nVDEC_EVENT_ERROR ,0x%x,0x%x\n",Data1,Data2);
	}else{
		logw("param1 = 0x%x param2 = %d\n", (int)Data1, (int)Data2);
	}
	return eError;
}

OMX_CALLBACKTYPE video_render_callbacks = {
	.EventHandler    = video_render_event_handler,
	.EmptyBufferDone = NULL,
	.FillBufferDone  = NULL
};

int main(int argc,char **argv)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_BUFFERHEADERTYPE buff_head;
	OMX_VIDEO_PARAM_PORTFORMATTYPE port_format;
	OMX_HANDLETYPE vdecoder;
	OMX_HANDLETYPE video_render;
	struct mpp_packet  pkt;
	s32  pre_pkt_szie;
	struct aic_message msg;
	int file_fd;
	struct bit_stream_parser *parser;
	int bs_eof_flag = 0;
	OMX_STATETYPE state = OMX_StateInvalid;
	int try_times;
	int decode_count = 0;
	int vdec_stream_end;
	int video_render_stream_end;
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

	// 1 get  vdecoder  video_render
	logw("OMX_GetHandle!!!\n");
	if (OMX_ErrorNone !=OMX_GetHandle(&vdecoder, OMX_COMPONENT_VDEC_NAME,NULL, &vdec_callbacks)){
		loge("unable to get vdecoder handle.\n");
		goto _EXIT2;
	}

	if (OMX_ErrorNone !=OMX_GetHandle(&video_render, OMX_COMPONENT_VIDEO_RENDER_NAME,NULL, &video_render_callbacks)){
		loge("unable to get video_render handle.\n");
		goto _EXIT3;
	}

	// 2 set up tunnel vdecoder.out_port=1 ----> video_render.in_port=0
	if (OMX_ErrorNone !=OMX_SetupTunnel(vdecoder, VDEC_PORT_OUT_INDEX, video_render, VIDEO_RENDER_PORT_IN_VIDEO_INDEX)){
		loge("unable to get video_render handle.\n");
		goto _EXIT4;
	}

	//3 start  video_render
	logw(" video_render OMX_SendCommand OMX_StateIdle!!!\n");
	if (OMX_ErrorNone !=OMX_SendCommand(video_render, OMX_CommandStateSet, OMX_StateIdle, NULL)){
		loge("unable to set vdec to OMX_StateIdle.\n");
		goto _EXIT4;
	}
	do{
		OMX_GetState(video_render, &state);
		usleep(1000);
	}while(state != OMX_StateIdle);
	logw("video_render state: OMX_StateLoaded-->OMX_StateIdle!!!\n");
	if (OMX_ErrorNone !=OMX_SendCommand(video_render, OMX_CommandStateSet, OMX_StateExecuting, NULL)){
		loge("unable to set vdec to OMX_StateExecuting.\n");
		goto _EXIT4;
	}
	do{
		OMX_GetState(video_render, &state);
		usleep(1000);
	}while(state != OMX_StateExecuting);
	logw("video_render state: OMX_StateIdle-->OMX_StateExecuting!!!\n");

	// 4 start  vdecoder
	logw("OMX_SetParameter!!!\n");
	port_format.nPortIndex = 0;
	port_format.eCompressionFormat = OMX_VIDEO_CodingAVC;
	port_format.eColorFormat = OMX_COLOR_FormatYUV420Planar;
	if(OMX_ErrorNone != OMX_SetParameter(vdecoder, OMX_IndexParamVideoPortFormat,&port_format)){
		loge("OMX_SetParameter Error!!!!.\n");
		goto _EXIT4;
	}

	logw("OMX_SendCommand OMX_StateIdle!!!\n");
	if (OMX_ErrorNone !=OMX_SendCommand(vdecoder, OMX_CommandStateSet, OMX_StateIdle, NULL)){
		loge("unable to set vdec to OMX_StateIdle.\n");
		goto _EXIT4;
	}

	do{
		OMX_GetState(vdecoder, &state);
		usleep(1000);
	}while(state != OMX_StateIdle);

	logw("vdecdoer state: OMX_StateLoaded-->OMX_StateIdle!!!\n");

	if (OMX_ErrorNone !=OMX_SendCommand(vdecoder, OMX_CommandStateSet, OMX_StateExecuting, NULL)){
		loge("unable to set vdec to OMX_StateExecuting.\n");
		goto _EXIT4;
	}

	do{
		OMX_GetState(vdecoder, &state);
		usleep(1000);
	}while(state != OMX_StateExecuting);
	logw("vdecdoer state: OMX_StateIdle-->OMX_StateExecuting!!!\n");

	// 5 open stream
	file_fd = open(argv[1], O_RDONLY);
	if (file_fd < 0) {
		loge("failed to open input file %s", argv[1]);
		goto _EXIT4;
	}
	lseek(file_fd, 0, SEEK_SET);
	parser = bs_create(file_fd);

	// 6 read stream to vdec and process msg from vdecoder and video_render
	while(1){
		if(aic_msg_get(&g_msg_que, &msg) == 0){
			if(msg.message_id == VDEC_EventCmdComplete){
				logw("receive  VDEC_EventCmdComplete \n");
			}else if(msg.message_id == VDEC_EventBufferFlag){ //end fo stream stop
				logw("receive end of stream from decoder,decode_count:%d!!!!\n",decode_count);
				vdec_stream_end = 1;
				if(vdec_stream_end==1 && video_render_stream_end==1){
					logw("receive end of stream from video_render and vdec!!!!\n");
					break;
				}
			}else if(msg.message_id == VIDEO_RENDER_EventBufferFlag){
				logw("receive end of stream from video_render!!!!\n");
				video_render_stream_end = 1;
				if(vdec_stream_end==1 && video_render_stream_end==1){
					logw("receive end of stream from video_render and vdec!!!!\n");
					break;
				}
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

	// 7 stop  video render  must first stop
	OMX_SendCommand(video_render, OMX_CommandStateSet, OMX_StateIdle, NULL);
	do{
		OMX_GetState(video_render, &state);
		usleep(1000);
	}while(state != OMX_StateIdle);

	OMX_SendCommand(video_render, OMX_CommandStateSet, OMX_StateLoaded, NULL);
	do{
		OMX_GetState(video_render, &state);
		usleep(1000);
	}while(state != OMX_StateLoaded);

	//8 stop  vdecoder
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

	OMX_SetupTunnel(vdecoder,VDEC_PORT_OUT_INDEX,NULL,VIDEO_RENDER_PORT_IN_VIDEO_INDEX);
	OMX_SetupTunnel(NULL,1,video_render,0);

	logw("decode_count:%d\n",decode_count);

_EXIT4:
	if(video_render){
		OMX_FreeHandle(video_render);
		video_render = NULL;
	}
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

