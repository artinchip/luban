/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: OMX_AdecComponent  Tunneld OMX_AudidoRendercComponent demo
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
#include <inttypes.h>

#include "OMX_Core.h"
#include "OMX_CoreExt1.h"
#include "mpp_dec_type.h"
#include "mpp_list.h"
#include "mpp_log.h"
#include "mpp_mem.h"

#include "aic_message.h"
#include "bit_stream_parser.h"

#define  ADEC_EventCmdComplete  1
#define  ADEC_FILL_BUFFER_DONE  2
#define  ADEC_EVENT_ERROR  3
#define  ADEC_EventBufferFlag  4
#define  AUDIO_RENDER_EventBufferFlag  5

struct packet_node {
	struct OMX_BUFFERHEADERTYPE  sBuff;
	struct mpp_list  sList;
};

#define PKT_MAX_NUM 16

static struct aic_message_queue g_msg_que;

static OMX_ERRORTYPE adec_event_handler (
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
		logw("adec state set to %u\n", Data2);
		msg.message_id = ADEC_EventCmdComplete;
		msg.data_size = 0;
		aic_msg_put(&g_msg_que,&msg);
	}else if(eEvent == OMX_EventBufferFlag){
		logw("\nEOS ,0x%x\n",Data1);
		msg.message_id = ADEC_EventBufferFlag;
		msg.data_size = 0;
		aic_msg_put(&g_msg_que,&msg);
	}else if(eEvent == ADEC_EVENT_ERROR){
		logw("\nADEC_EVENT_ERROR ,0x%x,0x%x\n",Data1,Data2);
	}else{
		logw("param1 = 0x%x param2 = %d\n", (int)Data1, (int)Data2);
	}
	return eError;
}

OMX_CALLBACKTYPE adec_callbacks = {
	.EventHandler    = adec_event_handler,
	//.EmptyBufferDone = adec_empty_buffer_done,
	.EmptyBufferDone = NULL,
	//.FillBufferDone  = adec_fill_buffer_done
	.FillBufferDone  = NULL
};

static OMX_ERRORTYPE audio_render_event_handler (
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
		logw("adec state set to %u\n", Data2);
		msg.message_id = ADEC_EventCmdComplete;
		msg.data_size = 0;
		aic_msg_put(&g_msg_que,&msg);
	}else if(eEvent == OMX_EventBufferFlag){
		logw("\nEOS ,0x%x\n",Data1);
		msg.message_id = AUDIO_RENDER_EventBufferFlag;
		msg.data_size = 0;
		aic_msg_put(&g_msg_que,&msg);
	}else if(eEvent == ADEC_EVENT_ERROR){
		logw("\nADEC_EVENT_ERROR ,0x%x,0x%x\n",Data1,Data2);
	}else{
		logw("param1 = 0x%x param2 = %d\n", (int)Data1, (int)Data2);
	}
	return eError;
}

OMX_CALLBACKTYPE audio_render_callbacks = {
	.EventHandler    = audio_render_event_handler,
	.EmptyBufferDone = NULL,
	.FillBufferDone  = NULL
};

int main(int argc,char **argv)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_BUFFERHEADERTYPE buff_head;
	OMX_AUDIO_PARAM_PORTFORMATTYPE port_format;
	OMX_HANDLETYPE adecoder;
	OMX_HANDLETYPE audio_render;
	struct mpp_packet  pkt;;
	struct aic_message msg;
	int file_fd;
	int bs_eof_flag = 0;
	OMX_STATETYPE state = OMX_StateInvalid;
	int adec_stream_end;
	int audio_render_stream_end;

	int file_size = 0;
	int read_len = 0;
	int read_sum_len = 0;
	int need_read = 1;

	if(argc < 2){
		loge("param error!!!\n");
		return -1;
	}
	memset(&pkt,0x00,sizeof(struct mpp_packet));


	if(aic_msg_create(&g_msg_que) != 0){
		loge("aic_msg_create error!!!\n");
		goto _EXIT0;
	}

	logi("OMX_init!!!\n");
	eError = OMX_Init();
	if(eError != OMX_ErrorNone){
		loge("OMX_init error!!!\n");
		goto _EXIT1;
	}

	// 1 get  adecoder  audio_render
	logi("OMX_GetHandle!!!\n");
	if (OMX_ErrorNone !=OMX_GetHandle(&adecoder, OMX_COMPONENT_ADEC_NAME,NULL, &adec_callbacks)){
		loge("unable to get adecoder handle.\n");
		goto _EXIT2;
	}

	if (OMX_ErrorNone !=OMX_GetHandle(&audio_render, OMX_COMPONENT_AUDIO_RENDER_NAME,NULL, &audio_render_callbacks)){
		loge("unable to get audio_render handle.\n");
		goto _EXIT3;
	}

	// 2 set up tunnel adecoder.out_port=1 ----> audio_render.in_port=0
	if (OMX_ErrorNone !=OMX_SetupTunnel(adecoder, ADEC_PORT_OUT_INDEX, audio_render, AUDIO_RENDER_PORT_IN_AUDIO_INDEX)){
		loge("unable to get audio_render handle.\n");
		goto _EXIT4;
	}

	//3 start  audio_render
	logi(" audio_render OMX_SendCommand OMX_StateIdle!!!\n");
	if (OMX_ErrorNone !=OMX_SendCommand(audio_render, OMX_CommandStateSet, OMX_StateIdle, NULL)){
		loge("unable to set adec to OMX_StateIdle.\n");
		goto _EXIT4;
	}
	do{
		OMX_GetState(audio_render, &state);
		usleep(1000);
	}while(state != OMX_StateIdle);
	logi("audio_render state: OMX_StateLoaded-->OMX_StateIdle!!!\n");
	if (OMX_ErrorNone !=OMX_SendCommand(audio_render, OMX_CommandStateSet, OMX_StateExecuting, NULL)){
		loge("unable to set adec to OMX_StateExecuting.\n");
		goto _EXIT4;
	}
	do{
		OMX_GetState(audio_render, &state);
		usleep(1000);
	}while(state != OMX_StateExecuting);
	logi("audio_render state: OMX_StateIdle-->OMX_StateExecuting!!!\n");

	// 4 start  adecoder
	logi("OMX_SetParameter!!!\n");
	port_format.nPortIndex = 0;
	port_format.eEncoding = OMX_AUDIO_CodingMP3;
	if(OMX_ErrorNone != OMX_SetParameter(adecoder, OMX_IndexParamAudioPortFormat,&port_format)){
		loge("OMX_SetParameter Error!!!!.\n");
		goto _EXIT4;
	}

	logi("OMX_SendCommand OMX_StateIdle!!!\n");
	if (OMX_ErrorNone !=OMX_SendCommand(adecoder, OMX_CommandStateSet, OMX_StateIdle, NULL)){
		loge("unable to set adec to OMX_StateIdle.\n");
		goto _EXIT4;
	}

	do{
		OMX_GetState(adecoder, &state);
		usleep(1000);
	}while(state != OMX_StateIdle);

	logi("adecdoer state: OMX_StateLoaded-->OMX_StateIdle!!!\n");

	if (OMX_ErrorNone !=OMX_SendCommand(adecoder, OMX_CommandStateSet, OMX_StateExecuting, NULL)){
		loge("unable to set adec to OMX_StateExecuting.\n");
		goto _EXIT4;
	}

	do{
		OMX_GetState(adecoder, &state);
		usleep(1000);
	}while(state != OMX_StateExecuting);
	logi("adecdoer state: OMX_StateIdle-->OMX_StateExecuting!!!\n");

	// 5 open stream
	file_fd = open(argv[1], O_RDONLY);
	if (file_fd < 0) {
		loge("failed to open input file %s", argv[1]);
		goto _EXIT4;
	}
	file_size = lseek(file_fd, 0, SEEK_END);
	lseek(file_fd, 0, SEEK_SET);

	// 6 read stream to adec and process msg from adecoder and audio_render
	while(1){
		if(aic_msg_get(&g_msg_que, &msg) == 0){
			if(msg.message_id == ADEC_EventCmdComplete){
				logi("receive  ADEC_EventCmdComplete \n");
			}else if(msg.message_id == ADEC_EventBufferFlag){ //end fo stream stop
				logi("receive end of stream from decoder!!!!\n");
				adec_stream_end = 1;
				if(adec_stream_end==1 && audio_render_stream_end==1){
					logi("receive end of stream from audio_render and adec!!!!\n");
					break;
				}
			}else if(msg.message_id == AUDIO_RENDER_EventBufferFlag){
				logi("receive end of stream from audio_render!!!!\n");
				audio_render_stream_end = 1;
				if(adec_stream_end==1 && audio_render_stream_end==1){
					logi("receive end of stream from audio_render and adec!!!!\n");
					break;
				}
			}else{
				loge("unknown event");
			}
		}

		#define PKT_SIZE (2*1024)
		if(!bs_eof_flag){
			if(pkt.size == 0 || pkt.data == NULL){
				pkt.size = PKT_SIZE;
				pkt.data = mpp_alloc(pkt.size);
				if(pkt.data == NULL){
					loge("mpp_alloc err!\n");
					break;
				}
			}

			if(need_read){
				read_len = read(file_fd, pkt.data, pkt.size);
				if(read_len > 0){
					pkt.size = read_len;
					read_sum_len += read_len;
					if(read_sum_len >= file_size){
						pkt.flag |= PACKET_FLAG_EOS;
					}
				}else if(read_len == 0){
					pkt.size = read_len;
					pkt.flag |= PACKET_FLAG_EOS;
					logi("end of stream\n");
				}else{
					logd("read error!!!\n");
					// need to do
					break;
				}
			}

			buff_head.nFilledLen = pkt.size;
			buff_head.pBuffer = pkt.data;
			buff_head.nFlags = pkt.flag;
			buff_head.nTimeStamp = pkt.pts;

			if(OMX_EmptyThisBuffer(adecoder,&buff_head) == OMX_ErrorNone){
				need_read = 1;//ok,next need to read
				if(pkt.flag & PACKET_FLAG_EOS){
					bs_eof_flag = 1;
					logi("end of stream\n");
				}
			}else{
				need_read = 0;//fail,next do not need to read
			}
		}

	}

	// 7 stop  audio render  must first stop
	OMX_SendCommand(audio_render, OMX_CommandStateSet, OMX_StateIdle, NULL);
	do{
		OMX_GetState(audio_render, &state);
		usleep(1000);
	}while(state != OMX_StateIdle);
	logi("audio_render state: OMX_StateExecuting-->OMX_StateIdle!!!\n");

	OMX_SendCommand(audio_render, OMX_CommandStateSet, OMX_StateLoaded, NULL);
	do{
		OMX_GetState(audio_render, &state);
		usleep(1000);
	}while(state != OMX_StateLoaded);
	logi("audio_render state: OMX_StateIdle-->OMX_StateLoaded!!!\n");

	//8 stop  adecoder
	OMX_SendCommand(adecoder, OMX_CommandStateSet, OMX_StateIdle, NULL);
	do{
		OMX_GetState(adecoder, &state);
		usleep(1000);
	}while(state != OMX_StateIdle);
	logi("adecoder state: OMX_StateExecuting-->OMX_StateIdle!!!\n");

	OMX_SendCommand(adecoder, OMX_CommandStateSet, OMX_StateLoaded, NULL);
	do{
		OMX_GetState(adecoder, &state);
		usleep(1000);
	}while(state != OMX_StateLoaded);
	logi("adecoder state: OMX_StateIdle-->OMX_StateLoaded!!!\n");

	OMX_SetupTunnel(adecoder,ADEC_PORT_OUT_INDEX,NULL,0);
	OMX_SetupTunnel(NULL,0,audio_render,AUDIO_RENDER_PORT_IN_AUDIO_INDEX);

	logi("adecoder and audio_render cancel setup OMX_SetupTunnel!!!\n");

	close(file_fd);

_EXIT4:
	if(audio_render){
		OMX_FreeHandle(audio_render);
		audio_render = NULL;
	}
_EXIT3:
	if(adecoder){
		OMX_FreeHandle(adecoder);
		adecoder = NULL;
	}
_EXIT2:
	OMX_Deinit();
_EXIT1:
	aic_msg_destroy(&g_msg_que);
_EXIT0:
	logi("adec2ao_test exitl!!!\n");
	return (int )eError;
}

