/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: OMX_DemuxerComponent demo
*/

#include <string.h>
#include <malloc.h>
#include <stddef.h>
#include <stdlib.h>
#include <inttypes.h>
#include "mpp_dec_type.h"
#include "mpp_list.h"
#include "mpp_log.h"
#include "mpp_mem.h"
#include "aic_message.h"
#include "OMX_Core.h"
#include "OMX_CoreExt1.h"

#define  PORT_FORMAT_DETECTED  1
#define  BUFFER_FLAG  2
#define  EVENT_ERROR  3
#define  DEMUXER_FILL_BUFFER_DONE  4
#define  PORT_FORMAT_NOTDETECTED  5

#define PKT_MAX_NUM 16
struct packet_node {
	OMX_BUFFERHEADERTYPE  sBuff;
	struct mpp_list  sList;
};
static struct mpp_list g_pkt_empty_list;// receive
static struct mpp_list g_pkt_ready_list;
static pthread_mutex_t g_pkt_mutex;

static struct aic_message_queue g_msg_que;
static int g_eof;

static OMX_ERRORTYPE demuxer_event_handler (
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
		logi("demuxer state set to %u\n", Data2);
	}else if(eEvent == OMX_EventPortFormatDetected){
		msg.message_id = PORT_FORMAT_DETECTED;
		msg.data_size = 0;
		aic_msg_put(&g_msg_que,&msg);
		logi("port %u format detected.\n", Data1);
	}else if(eEvent == OMX_EventBufferFlag){
		msg.message_id = BUFFER_FLAG;
		msg.data_size = 0;
		aic_msg_put(&g_msg_que,&msg);
		g_eof =1;
		logi("\nEOS detected\n");
	}else if(eEvent == OMX_EventError){
		logi("\nEOS OMX_EventError,0x%x\n",Data1);
		if(Data1 == OMX_ErrorFormatNotDetected){
			msg.message_id = PORT_FORMAT_NOTDETECTED;
			msg.data_size = 0;
			aic_msg_put(&g_msg_que,&msg);
			logi("port %u format not detected.\n", Data1);
		}

	}else{
		logi("param1 = %d param2 = %d\n", (int)Data1, (int)Data2);
	}

	return eError;
}

static OMX_ERRORTYPE demuxer_empty_buffer_done (
	OMX_HANDLETYPE hComponent,
	OMX_PTR pAppData,
	OMX_BUFFERHEADERTYPE* pBuffer)
{
		OMX_ERRORTYPE eError = OMX_ErrorNone;
		return eError;
}

static OMX_ERRORTYPE demuxer_fill_buffer_done (
	OMX_HANDLETYPE hComponent,
	OMX_PTR pAppData,
	OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	struct aic_message msg;
	if(!g_eof){

		msg.message_id = DEMUXER_FILL_BUFFER_DONE;
		//msg.param = (s32)pBuffer; //convert point to int ,complier do not allow,so save point in data
		//msg.data_size = 0;
		msg.data = mpp_alloc(sizeof(OMX_BUFFERHEADERTYPE*));
		memcpy(msg.data,&pBuffer,sizeof(OMX_BUFFERHEADERTYPE*));
		msg.data_size = sizeof(OMX_BUFFERHEADERTYPE*);
		aic_msg_put(&g_msg_que,&msg);
	}else{
		logd("end steam!!!\n");
	}

	return eError;
}

OMX_CALLBACKTYPE demuxer_callbacks = {
	.EventHandler    = demuxer_event_handler,
	.EmptyBufferDone = demuxer_empty_buffer_done,
	.FillBufferDone  = demuxer_fill_buffer_done
};

void print_audio_encoding(OMX_AUDIO_CODINGTYPE type)
{
	switch (type)
	{
	case OMX_AUDIO_CodingMP3 :
		logi("mp3 audio detected on port 0.\n");
		break;
	case OMX_AUDIO_CodingAAC :
		logi("aac audio detected on port 0.\n");
		break;
	default :
		logi("%d audio detected on port 0.\n",type);
		break;
	}
	return;
}

void print_video_encoding(OMX_VIDEO_CODINGTYPE type)
{
	switch (type)
	{
	case OMX_VIDEO_CodingMPEG4 :
		logi("mpeg4 video detected on port 1.\n");
		break;
	case OMX_VIDEO_CodingAVC :
		logi("h264 video detected on port 1.\n");
		break;
	default :
		logi("%d video detected on port 1.\n",type);
		break;
	}

	return;
}

int main(int argc ,char*argv[]){
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_HANDLETYPE demuxer;
	OMX_PARAM_CONTENTURITYPE * uri_param;
	int bytes;
	struct aic_message msg;
	OMX_PARAM_U32TYPE num_streams,active_stream;
	OMX_AUDIO_PARAM_PORTFORMATTYPE audio_port_format;
	OMX_VIDEO_PARAM_PORTFORMATTYPE video_port_format;
	int i;
	struct packet_node * pkt;

	g_eof = 0;

	if(argc < 2){
		loge("param error!!!\n");
		return -1;
	}

	mpp_list_init(&g_pkt_empty_list);
	mpp_list_init(&g_pkt_ready_list);
	pthread_mutex_init(&g_pkt_mutex, NULL);

	for(i = 0;i<PKT_MAX_NUM;i++){
		pkt = (struct packet_node *)mpp_alloc(sizeof(struct packet_node));
		if(pkt == NULL){
			loge("malloc error\n");
			break;
		}
		mpp_list_add_tail(&pkt->sList, &g_pkt_empty_list);
	}
	if(i == 0){
		loge("no one empty node\n");
		return -1;
	}

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

	if (OMX_ErrorNone !=OMX_GetHandle(&demuxer, OMX_COMPONENT_DEMUXER_NAME,NULL, &demuxer_callbacks)){
		loge("unable to get demuxer handle.\n");
		goto _EXIT2;
	}

	logi("OMX_GetHandle!!!\n");

	bytes = strlen(argv[1]);
	uri_param = (OMX_PARAM_CONTENTURITYPE *)malloc(sizeof(OMX_PARAM_CONTENTURITYPE) + bytes);
	if(uri_param == NULL){
		loge("malloc uri error!!!!\n");
		goto _EXIT3;
	}

	uri_param->nSize = sizeof(OMX_PARAM_CONTENTURITYPE) + bytes;
	strcpy((char *)uri_param->contentURI, argv[1]);

	logi("contentURI!!!\n");

	if(OMX_ErrorNone != OMX_SetParameter(demuxer, OMX_IndexParamContentURI, uri_param)){
		loge("OMX_SetParameter Error!!!!.\n");
		goto _EXIT4;
	}

	logi("OMX_SetParameter!!!\n");

	if (OMX_ErrorNone !=OMX_SendCommand(demuxer, OMX_CommandStateSet, OMX_StateIdle, NULL)){
		loge("unable to set demuxer to idle.\n");
		goto _EXIT4;
	}

	logi("OMX_SendCommand!!!\n");
#define DEMUX_PORT_AUDIO_INDEX		0
#define DEMUX_PORT_VIDEO_INDEX		1
#define DEMUX_PORT_CLOCK_INDEX		2

	while(1){
		if(aic_msg_get(&g_msg_que, &msg) == 0){
			if(msg.message_id == PORT_FORMAT_DETECTED){
				num_streams.nSize = sizeof(OMX_PARAM_U32TYPE);
				num_streams.nPortIndex = DEMUX_PORT_AUDIO_INDEX;//audio
				if (OMX_ErrorNone !=OMX_GetParameter(demuxer, OMX_IndexParamNumAvailableStreams,&num_streams)){
					loge("unable to get available audio streams.\n");
					break;
				}
				logi("audio stream num :%d\n",num_streams.nU32);

				audio_port_format.nSize = sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE);
				audio_port_format.nPortIndex = DEMUX_PORT_AUDIO_INDEX;
				 for (i = 0; i < num_streams.nU32; i++){
					audio_port_format.nIndex = i;
					if (OMX_ErrorNone !=OMX_GetParameter(demuxer, OMX_IndexParamAudioPortFormat,&audio_port_format)){
						loge("unable to get port 0 format with index %d\n", i);
						break;
					}
					print_audio_encoding(audio_port_format.eEncoding);

				}

				num_streams.nSize = sizeof(OMX_PARAM_U32TYPE);
				num_streams.nPortIndex = DEMUX_PORT_VIDEO_INDEX;//video
				if (OMX_ErrorNone !=OMX_GetParameter(demuxer, OMX_IndexParamNumAvailableStreams,&num_streams)){
					 loge("unable to get available video streams.\n");
					 break;
				}
				logi("video stream num :%d\n",num_streams.nU32);

				video_port_format.nSize = sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE);
				video_port_format.nPortIndex = DEMUX_PORT_VIDEO_INDEX;
				 for (i = 0; i < num_streams.nU32; i++){
					video_port_format.nIndex = i;
					if (OMX_ErrorNone !=OMX_GetParameter(demuxer, OMX_IndexParamVideoPortFormat,&video_port_format)){
						loge("unable to get port 1 format with index %d\n", i);
						break;
					}
					print_video_encoding(video_port_format.eCompressionFormat);
				}

				active_stream.nSize = sizeof(OMX_PARAM_U32TYPE);
				active_stream.nPortIndex = DEMUX_PORT_AUDIO_INDEX;
				active_stream.nU32 = 0;//indicate which audio stream is selected,from 0
				if (OMX_ErrorNone !=OMX_SetParameter(demuxer, OMX_IndexParamActiveStream,&active_stream)){
					loge("unable to set active audio stream to %u.\n", active_stream.nU32);
					break;
				}
				active_stream.nSize = sizeof(OMX_PARAM_U32TYPE);
				active_stream.nPortIndex = DEMUX_PORT_VIDEO_INDEX;
				active_stream.nU32 = 0;//indicate which video stream is selected,from 0
				if (OMX_ErrorNone !=OMX_SetParameter(demuxer, OMX_IndexParamActiveStream,&active_stream)){
					loge("unable to set video stream to %u.\n", active_stream.nU32);
					break;
				}

				if (OMX_ErrorNone !=OMX_SendCommand(demuxer, OMX_CommandStateSet, OMX_StateExecuting, NULL)){
					loge("unable to set demuxer to idle.\n");
					break;
				}
			}else if(msg.message_id == BUFFER_FLAG) {// meet file end

				logi(" meet file end\n");
				if(!mpp_list_empty(&g_pkt_ready_list)){
					struct packet_node *pkt1,*pkt2;
					mpp_list_for_each_entry_safe(pkt1, pkt2, &g_pkt_ready_list, sList){
						logi("nTimeStamp:%"PRId64",port:%u,nAllocLen:%u,nFilledLen:%u,pBuffer:%p\n"
							,pkt1->sBuff.nTimeStamp
							,pkt1->sBuff.nOutputPortIndex
							,pkt1->sBuff.nAllocLen
							,pkt1->sBuff.nFilledLen
							,pkt1->sBuff.pBuffer);
						OMX_FillThisBuffer(demuxer, &pkt1->sBuff);
						mpp_list_del(&pkt1->sList);
						mpp_list_add_tail(&pkt1->sList, &g_pkt_empty_list);
					}
				}

				//stop  demuxer   OMX_StateExecuting--> OMX_StateIdle --> OMX_StateLoaded
				break;
			} else if(msg.message_id == DEMUXER_FILL_BUFFER_DONE){
				if(!mpp_list_empty(&g_pkt_empty_list)){
					OMX_BUFFERHEADERTYPE* pBuffer;
					//pBuffer = (OMX_BUFFERHEADERTYPE*)msg.param;
					memcpy(&pBuffer,msg.data,sizeof(OMX_BUFFERHEADERTYPE*));
					mpp_free(msg.data);
					msg.data = NULL;
					msg.data_size = 0;
					struct packet_node *pkt = mpp_list_first_entry(&g_pkt_empty_list,struct packet_node,sList);
					pkt->sBuff.nTimeStamp = pBuffer->nTimeStamp;
					pkt->sBuff.nOutputPortIndex = pBuffer->nOutputPortIndex;
					pkt->sBuff.nAllocLen = pBuffer->nAllocLen;
					pkt->sBuff.nFilledLen = pBuffer->nFilledLen;
					pkt->sBuff.pBuffer = pBuffer->pBuffer;
					mpp_list_del(&pkt->sList);
					mpp_list_add_tail(&pkt->sList, &g_pkt_ready_list);
					logd("nTimeStamp:%"PRId64",port:%u,nAllocLen:%u,nFilledLen:%u,pBuffer:%p\n"
					,pBuffer->nTimeStamp
					,pBuffer->nOutputPortIndex
					,pBuffer->nAllocLen
					,pBuffer->nFilledLen
					,pBuffer->pBuffer);
				}else{
					logw("drop one packet!!!\n");
				}

			} else if(msg.message_id == PORT_FORMAT_NOTDETECTED){
				logi("format not detected.\n");
				break;
			}else{
				loge("unknown event!\n");
			}
		}
		if(( NULL != demuxer) && (!mpp_list_empty(&g_pkt_ready_list))){
			struct packet_node *pkt1,*pkt2;
			mpp_list_for_each_entry_safe(pkt1, pkt2, &g_pkt_ready_list, sList){
				logi("nTimeStamp:%"PRId64",port:%u,nAllocLen:%u,nFilledLen:%u,pBuffer:%p\n"
					,pkt1->sBuff.nTimeStamp
					,pkt1->sBuff.nOutputPortIndex
					,pkt1->sBuff.nAllocLen
					,pkt1->sBuff.nFilledLen
					,pkt1->sBuff.pBuffer);
				OMX_FillThisBuffer(demuxer, &pkt1->sBuff);
				mpp_list_del(&pkt1->sList);
				mpp_list_add_tail(&pkt1->sList, &g_pkt_empty_list);
			}
		}
	}

	OMX_SendCommand(demuxer, OMX_CommandStateSet, OMX_StateIdle, NULL);
	OMX_SendCommand(demuxer, OMX_CommandStateSet, OMX_StateLoaded, NULL);

_EXIT4:
	if(uri_param){
		free(uri_param);
		uri_param = NULL;
	}
_EXIT3:
	if(demuxer){
		OMX_FreeHandle(demuxer);
		demuxer = NULL;
	}
_EXIT2:
	OMX_Deinit();
_EXIT1:
	aic_msg_destroy(&g_msg_que);
_EXIT0:
	i = 0;
	if(!mpp_list_empty(&g_pkt_empty_list)){
		struct packet_node *pkt1,*pkt2;
		mpp_list_for_each_entry_safe(pkt1, pkt2, &g_pkt_empty_list, sList){
			mpp_list_del(&pkt1->sList);
			mpp_free(pkt1);
			i++;
		}
	}
	logi("mpp_free %d pkt node",i);
	pthread_mutex_destroy(&g_pkt_mutex);
	return (int )eError;
}

