/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: OMX_AdecComponent demo
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
#include "aic_audio_decoder.h"

#define  ADEC_EventCmdComplete  1
#define  ADEC_FILL_BUFFER_DONE  2
#define  ADEC_EVENT_ERROR  3
#define  ADEC_EventBufferFlag  4

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

	}else{
		logw("param1 = 0x%x param2 = %d\n", (int)Data1, (int)Data2);
	}

	return eError;
}

static OMX_ERRORTYPE adec_empty_buffer_done (
	OMX_HANDLETYPE hComponent,
	OMX_PTR pAppData,
	OMX_BUFFERHEADERTYPE* pBuffer)
{
		OMX_ERRORTYPE eError = OMX_ErrorNone;
		return eError;
}

static OMX_ERRORTYPE adec_fill_buffer_done (
	OMX_HANDLETYPE hComponent,
	OMX_PTR pAppData,
	OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	struct aic_message msg;
	msg.message_id = ADEC_FILL_BUFFER_DONE;
	//msg.param = pBuffer->pBuffer;
	//msg.data_size = 0;
	msg.data = mpp_alloc(sizeof(OMX_U8*));
	memcpy(msg.data,&pBuffer->pBuffer,sizeof(OMX_U8*));
	msg.data_size = sizeof(OMX_U8*);
	aic_msg_put(&g_msg_que,&msg);
	logw("ADEC_FILL_BUFFER_DONE \n");
	return eError;
}

OMX_CALLBACKTYPE adec_callbacks = {
	.EventHandler    = adec_event_handler,
	.EmptyBufferDone = adec_empty_buffer_done,
	.FillBufferDone  = adec_fill_buffer_done
};
int main(int argc,char **argv)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_BUFFERHEADERTYPE buff_head;
	OMX_AUDIO_PARAM_PORTFORMATTYPE port_format;
	OMX_HANDLETYPE adecoder;
	struct mpp_packet  pkt;
	struct aic_message msg;
	int file_fd;
	int bs_eof_flag = 0;
	struct aic_audio_frame   *audio_frame;
	OMX_STATETYPE state = OMX_StateInvalid;
	int decode_count = 0;
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

	logi("OMX_GetHandle!!!\n");
	if (OMX_ErrorNone !=OMX_GetHandle(&adecoder, OMX_COMPONENT_ADEC_NAME,NULL, &adec_callbacks)){
		loge("unable to get demuxer handle.\n");
		goto _EXIT2;
	}

	logi("OMX_SetParameter!!!\n");
	port_format.nPortIndex = 0;
	port_format.eEncoding = OMX_AUDIO_CodingMP3;
	if(OMX_ErrorNone != OMX_SetParameter(adecoder, OMX_IndexParamAudioPortFormat,&port_format)){
		loge("OMX_SetParameter Error!!!!.\n");
		goto _EXIT3;
	}

	logi("OMX_SendCommand OMX_StateIdle!!!\n");

	if (OMX_ErrorNone !=OMX_SendCommand(adecoder, OMX_CommandStateSet, OMX_StateIdle, NULL)){
		loge("unable to set adec to OMX_StateIdle.\n");
		goto _EXIT3;
	}

	do{
		OMX_GetState(adecoder, &state);
		usleep(1000);
	}while(state != OMX_StateIdle);

	logi("adecdoer state: OMX_StateLoaded-->OMX_StateIdle!!!\n");

	if (OMX_ErrorNone !=OMX_SendCommand(adecoder, OMX_CommandStateSet, OMX_StateExecuting, NULL)){
		loge("unable to set adec to OMX_StateExecuting.\n");
		goto _EXIT3;
	}

	do{
		OMX_GetState(adecoder, &state);
		usleep(1000);
	}while(state != OMX_StateExecuting);

	logi("adecdoer state: OMX_StateIdle-->OMX_StateExecuting!!!\n");

	file_fd = open(argv[1], O_RDONLY);
	if (file_fd < 0) {
		loge("failed to open input file %s", argv[1]);
		goto _EXIT3;
	}
	file_size = lseek(file_fd, 0, SEEK_END);
	lseek(file_fd, 0, SEEK_SET);

	while(1){
		if(aic_msg_get(&g_msg_que, &msg) == 0){
			if(msg.message_id == ADEC_FILL_BUFFER_DONE){
				//audio_frame = msg.param;
				memcpy(&audio_frame,msg.data,sizeof(OMX_U8*));
				mpp_free(msg.data);
				msg.data = NULL;
				msg.data_size = 0;

				logi("bits_per_sample:%d,"\
					"channels:%d,"\
					"data:%p,"\
					"flag:0x%x,"\
					"id:%d,"\
					"pts:%"PRId64","\
					"sample_rate:%d,"\
					"size:%d\n"\
					,audio_frame->bits_per_sample
					,audio_frame->channels
					,audio_frame->data
					,audio_frame->flag
					,audio_frame->id
					,audio_frame->pts
					,audio_frame->sample_rate
					,audio_frame->size);

					buff_head.nOutputPortIndex = 1;
					buff_head.pBuffer = (OMX_U8 *)audio_frame;
					decode_count++;
					OMX_FillThisBuffer(adecoder,&buff_head);
					if(audio_frame->flag &FRAME_FLAG_EOS){
						logi("receive  last frame break \n");
						break;
					}
			}else if(msg.message_id == ADEC_EventCmdComplete){
				logi("receive  ADEC_EventCmdComplete \n");
			}else if(msg.message_id == ADEC_EventBufferFlag){ //end fo stream stop
				logi("receive end of stream from decoder,decode_count:%d!!!!\n",decode_count);
				//break;

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
	logi("decode_count:%d\n",decode_count);

	close(file_fd);

	OMX_SendCommand(adecoder, OMX_CommandStateSet, OMX_StateIdle, NULL);
	do{
		OMX_GetState(adecoder, &state);
		usleep(1000);
	}while(state != OMX_StateIdle);

	OMX_SendCommand(adecoder, OMX_CommandStateSet, OMX_StateLoaded, NULL);
	do{
		OMX_GetState(adecoder, &state);
		usleep(1000);
	}while(state != OMX_StateLoaded);

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
	logi("adecoder_test exit\n");
	return (int )eError;
}

