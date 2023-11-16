/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: aic_audio_decoder interface
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
#include <pthread.h>
#include <inttypes.h>

#include "aic_audio_decoder.h"
#include "mpp_log.h"
#include "aic_audio_render.h"
#include "pcm2wav.h"

static int g_bs_eof_flag = 0;
static int g_decoder_end = 0;

#define PKT_SIZE (2*1024)

#define SAVE_PCM

#ifdef SAVE_PCM

#define PCM_TO_WAV

#endif

static void* decode_thread(void *p)
{
	struct aic_audio_decoder* audio_decoder = (struct aic_audio_decoder* )p;
	logi("decode_thread start!!!!!\n");
	aic_audio_decoder_decode(audio_decoder);
	g_decoder_end = 1;
	logi("decode_thread exit!!!!!\n");
	return (void *)0;
}

int main(int argc ,char *argv[])
{
	int file_fd;
	int file_size;
	int read_len = 0;
	int ret = 0;
	struct mpp_packet  pkt;
	struct aic_audio_decoder* audio_decoder = NULL;
	struct aic_audio_decode_config audio_decoder_config;
	struct aic_audio_frame audio_frame;
	pthread_t decode_thread_id;
	struct aic_audio_render *render;
	int first_frame_show =1;
	struct aic_audio_render_attr  audio_render_attr;
	int save_fd;

	struct timeval pre,cur;

	struct timeval before_rend,after_rend;

	unsigned long intelval = 0;

	memset(&pre,0x00,sizeof(struct timeval));
	memset(&cur,0x00,sizeof(struct timeval));

	if(argc < 2){
		logd("param error\n");
		return -1;
	}
	file_fd = open(argv[1], O_RDONLY);
	if (file_fd < 0) {
		loge("failed to open input file %s", argv[1]);
		ret = -1;
		goto _EXIT0;
	}
	file_size = lseek(file_fd, 0, SEEK_END);
	lseek(file_fd, 0, SEEK_SET);
	audio_decoder = aic_audio_decoder_create(MPP_CODEC_AUDIO_DECODER_MP3);
	if(audio_decoder == NULL){
		loge("aic_audio_decoder_create error!!!\n");
		ret =-1;
		goto _EXIT1;
	}

	if(aic_audio_render_create(&render) != 0){
		loge("aic_audio_decoder_create error!!!\n");
		ret = -1;
		goto _EXIT2;
	}

	audio_decoder_config.packet_buffer_size = 16*1024;
	audio_decoder_config.packet_count = 8;
	audio_decoder_config.frame_count = 16;

	if(aic_audio_decoder_init(audio_decoder, &audio_decoder_config) != 0){
		loge("aic_audio_decoder_init error!!!\n");
		ret =-1;
		goto _EXIT3;
	}

#ifdef SAVE_PCM
	save_fd = open("save.pcm", O_RDWR|O_CREAT);
	if (save_fd < 0) {
		loge("failed to open save.pcm file\n");
		ret = -1;
		goto _EXIT3;
	}
#endif

	// must create thread

	pthread_create(&decode_thread_id, NULL, decode_thread, audio_decoder);

	memset(&pkt,0x00,sizeof(struct mpp_packet));

	while(1){
		if(!g_bs_eof_flag){
			pkt.size = PKT_SIZE;
			// if get packet fail,do not wait
			ret = aic_audio_decoder_get_packet(audio_decoder,&pkt,pkt.size);
			if(ret == 0){//ger packet success
				ret = read(file_fd, pkt.data, pkt.size);
				if(ret > 0){
					pkt.size = ret;
					read_len += ret;
					if(read_len >= file_size){
						g_bs_eof_flag = 1;
						pkt.flag |= PACKET_FLAG_EOS;
						logi("end of stream\n");
					}
					logd("read size:%d,read_len:%d,file_size:%d\n",pkt.size,read_len,file_size);
				}else if(ret == 0){
					g_bs_eof_flag = 1;
					pkt.flag |= PACKET_FLAG_EOS;
					logi("end of stream\n");
				}else{
					logd("read error!!!\n");
					// need to do
					break;
				}
				aic_audio_decoder_put_packet(audio_decoder,&pkt);
			}else{
				// nothing to do ,do not need to wait
			}
		}

		ret = aic_audio_decoder_get_frame(audio_decoder,&audio_frame);
		if(ret == DEC_OK){
			logd("bits_per_sample:%d,"\
				"channels:%d,"\
				"data:%p,"\
				"flag:0x%x,"\
				"id:%d,"\
				"pts:%"PRId64","\
				"sample_rate:%d,"\
				"size:%d,\n"\
				,audio_frame.bits_per_sample
				,audio_frame.channels
				,audio_frame.data
				,audio_frame.flag
				,audio_frame.id
				,audio_frame.pts
				,audio_frame.sample_rate
				,audio_frame.size);
			if(first_frame_show){
				audio_render_attr.bits_per_sample = audio_frame.bits_per_sample;
				audio_render_attr.channels = audio_frame.channels;
				audio_render_attr.sample_rate = audio_frame.sample_rate;
				audio_render_attr.smples_per_frame = 1024;
				ret = render->set_attr(render,&audio_render_attr);
				logi("render->set_attr:%d\n",ret);
				ret = render->init(render,0);
				logi("render->init:%d\n",ret);
				first_frame_show = 0;
			}

			gettimeofday(&cur, NULL);
			if(pre.tv_sec != 0 || pre.tv_usec != 0){
				intelval = ((cur.tv_sec - pre.tv_sec)*1000000) + (cur.tv_usec - pre.tv_usec);
				logd("get frame intelval time :%ld\n",intelval);
			}
			pre = cur;

			gettimeofday(&before_rend, NULL);
			ret = render->rend(render,audio_frame.data,audio_frame.size);
			gettimeofday(&after_rend, NULL);
			logd("rend intelval time :%ld\n",((after_rend.tv_sec - before_rend.tv_sec)*1000000) + (after_rend.tv_usec - before_rend.tv_usec));

#ifdef SAVE_PCM
			write(save_fd,audio_frame.data,audio_frame.size);
#endif
			aic_audio_decoder_put_frame(audio_decoder,&audio_frame);
		}else if(ret == DEC_NO_RENDER_FRAME){
			if(g_bs_eof_flag && g_decoder_end){
				logw("decoder end break;\n");
				break;
			}
			//usleep(1000);
			//logw("mpp_dec_get_frame error, ret: %x", ret);
			continue;
		}else if(ret == DEC_NO_EMPTY_FRAME){
			//usleep(1000);
			logw("mpp_dec_get_frame error, ret: %d", ret);
			continue;
		}else if(ret == DEC_ERR_FM_NOT_CREATE){
			//usleep(1000);
			logw("mpp_dec_get_frame error, ret: %d", ret);
			continue;
		}else{
			logw("mpp_dec_get_frame error, ret: %d", ret);
			break;
		}
	}
	pthread_join(decode_thread_id, NULL);
	logi("demo will exit!!!\n");

#ifdef SAVE_PCM
close(save_fd);
#ifdef PCM_TO_WAV
	ret = pcm_to_wav("save.pcm", "save.wav");
	logi("pcm_to_wav:%d\n",ret);
#endif
#endif

_EXIT3:
	aic_audio_render_destroy(render);
_EXIT2:
	aic_audio_decoder_destroy(audio_decoder);
_EXIT1:
	close(file_fd);
_EXIT0:
	logi("demo will exit!!!\n");
	return ret;
}

