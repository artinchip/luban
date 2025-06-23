/*
 * Copyright (C) 2020-2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: mini_audio_player
 */

#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include "aic_audio_decoder.h"
#include "aic_audio_render.h"
#include "aic_parser.h"
#include "aic_message.h"
#include "mpp_log.h"
#include "mpp_mem.h"
#include "mini_audio_player.h"

#define WAIT_TIME_FOREVER -1

#define _MINI_AUDIO_PLAYER_DEBUG_

#ifdef _MINI_AUDIO_PLAYER_DEBUG_
#define MINI_AUDIO_PLAYER_DEBUG(fmt,arg...)\
		printf("[%s:%d]"fmt"\n",__FUNCTION__,__LINE__,##arg)
#else
#define MINI_AUDIO_PLAYER_DEBUG(...)
#endif

#define MINI_AUDIO_PLAYER_ERROR(fmt,arg...)\
		printf("\033[40;31m[%s:%d]"fmt"\033[0m \n",__FUNCTION__,__LINE__,##arg)

enum MINI_AUDIO_PLAYER_MSG_TYPE {
	MINI_AUDIO_PLAYER_MSG_NONE = 0,
	MINI_AUDIO_PLAYER_MSG_START = 1,
	MINI_AUDIO_PLAYER_MSG_STOP = 2,
	MINI_AUDIO_PLAYER_MSG_PAUSE = 3,
	MINI_AUDIO_PLAYER_MSG_RESUME = 4,
	MINI_AUDIO_PLAYER_MSG_DESTROY = 5,
};

enum MINI_AUDIO_PLAYER_EVENT {
	MINI_AUDIO_PLAYER_EVENT_NONE = 0,
	MINI_AUDIO_PLAYER_EVENT_PLAY = 1,
	MINI_AUDIO_PLAYER_EVENT_STOP = 2,
	MINI_AUDIO_PLAYER_EVENT_PAUSE = 3,
	MINI_AUDIO_PLAYER_EVENT_RESUME = 4,
	MINI_AUDIO_PLAYER_EVENT_DESTROY = 5,
};

enum PLAYER_FILE_TYPE {
	PLAYER_FILE_MP3 = 0,
	PLAYER_FILE_WAV = 1,
};

#define MINI_AUDIO_PLAYER_WAVE_BUFF_SIZE (4*1024)

struct mini_audio_player {
	char uri[128];
	int type;
	int fd;
	int state;
	int volume;
	char *wav_buff;
	int wav_buff_size;
	struct aic_audio_frame frame_info;
	struct aic_audio_decoder *decoder;
	struct aic_audio_decode_config dec_cfg;
	struct aic_audio_render *render;
	struct aic_parser *parser;
	struct mini_player_audio_info audio_info;

	pthread_t tid;
	struct aic_message_queue mq;
	pthread_mutexattr_t lock_attr;
	pthread_mutex_t lock;
	sem_t sem_thread_exit;
	sem_t sem_ack;
};

static int mini_audio_player_msg_send(struct mini_audio_player *player, int type, void *data)
{
	struct aic_message msg;

	msg.message_id = type;
	msg.data_size = 0;
	msg.data = NULL;

	return aic_msg_put(&player->mq, &msg);
}

int mini_audio_player_play(struct mini_audio_player *player,char *uri)
{
	int result = 0;

	pthread_mutex_lock(&player->lock);
	if (player->state != MINI_AUDIO_PLAYER_STATE_STOPED) {
		mini_audio_player_stop(player);
	}
	strcpy(player->uri,uri);
	result = mini_audio_player_msg_send(player, MINI_AUDIO_PLAYER_MSG_START, NULL);
	if (result != 1) {
		result = -1;
	}
	sem_wait(&player->sem_ack);
	pthread_mutex_unlock(&player->lock);
	return result;
}

int mini_audio_player_stop(struct mini_audio_player *player)
{
	int result = 0;

	pthread_mutex_lock(&player->lock);
	if (player->state != MINI_AUDIO_PLAYER_STATE_STOPED) {
		result = mini_audio_player_msg_send(player, MINI_AUDIO_PLAYER_MSG_STOP, NULL);
		if (result != 1) {
			result = -1;
		}
		sem_wait(&player->sem_ack);
	}
	pthread_mutex_unlock(&player->lock);

	return result;
}

int mini_audio_player_pause(struct mini_audio_player *player)
{
	int result = 0;

	pthread_mutex_lock(&player->lock);
	if (player->state == MINI_AUDIO_PLAYER_STATE_PLAYING) {
		result = mini_audio_player_msg_send(player, MINI_AUDIO_PLAYER_MSG_PAUSE, NULL);
		if (result != 1) {
			result = -1;
		}
		sem_wait(&player->sem_ack);
	}
    if (player->render) {
        aic_audio_render_pause(player->render);
    }
	pthread_mutex_unlock(&player->lock);

	return result;
}

int mini_audio_player_resume(struct mini_audio_player *player)
{
	int result = 0;

	pthread_mutex_lock(&player->lock);
	if (player->state  == MINI_AUDIO_PLAYER_STATE_PAUSED) {
		result = mini_audio_player_msg_send(player, MINI_AUDIO_PLAYER_MSG_RESUME, NULL);
		if (result != 1) {
			result = -1;
		}
		sem_wait(&player->sem_ack);
	}

    if (player->render) {
        aic_audio_render_pause(player->render);
    }
	pthread_mutex_unlock(&player->lock);

	return result;
}

int mini_audio_player_get_media_info(struct mini_audio_player *player,struct mini_player_audio_info *audio_info)
{
	if (player == NULL || audio_info == NULL) {
		return -1;
	}
	memcpy(audio_info,&player->audio_info,sizeof(struct mini_player_audio_info));
	return 0;
}
int mini_audio_player_set_volume(struct mini_audio_player *player,int vol)
{
	int volume;

	volume = vol;
	if (volume < 0) {
		volume = 0;
	}
	if (volume > 100) {
		volume = 100;
	}
	player->volume = volume;
	if (player->render) {
		aic_audio_render_set_volume(player->render,player->volume);
	}
	return 0;
}
int mini_audio_player_get_volume(struct mini_audio_player *player,int *vol)
{
	if (vol == NULL) {
		return -1;
	}
	*vol = player->volume;
	return 0;
}
int mini_audio_player_get_state(struct mini_audio_player *player)
{
	return player->state;
}

static int mini_audio_player_open(struct mini_audio_player *player)
{
	char* ptr = NULL;
	struct aic_parser_av_media_info media_info;
	struct aic_audio_render_attr attr;

	if (player == NULL) {
		MINI_AUDIO_PLAYER_ERROR("para error\n");
		goto _exit;
	}
	ptr = strrchr(player->uri, '.');
	if (!strncmp(ptr+1, "mp3", 3)) {
		player->type = PLAYER_FILE_MP3;
	} else if (!strncmp(ptr+1, "wav", 3)) {
		player->type = PLAYER_FILE_WAV;
		if (player->wav_buff == NULL) {
			player->wav_buff_size = MINI_AUDIO_PLAYER_WAVE_BUFF_SIZE;
			player->wav_buff = mpp_alloc(player->wav_buff_size);
			if (player->wav_buff == NULL) {
				MINI_AUDIO_PLAYER_ERROR("aicos_malloc error\n");
				goto _exit;
			}
		}
	} else {
		MINI_AUDIO_PLAYER_ERROR("unsupport file type\n");
		goto _exit;
	}

	aic_parser_create((unsigned char *)player->uri,&player->parser);
	if (player->parser == NULL) {
		MINI_AUDIO_PLAYER_ERROR("aic_parser_create fail\n");
		goto _exit;
	}
	if (aic_parser_init(player->parser)) {
		MINI_AUDIO_PLAYER_ERROR("aic_parser_init fail\n");
		goto _exit;
	}
	if (aic_parser_get_media_info(player->parser,&media_info)) {
		MINI_AUDIO_PLAYER_ERROR("aic_parser_get_media_info fail\n");
		goto _exit;
	}
	player->audio_info.file_size = media_info.file_size;
	player->audio_info.duration = media_info.duration;
	player->audio_info.sample_rate = media_info.audio_stream.sample_rate;
	player->audio_info.nb_channel = media_info.audio_stream.nb_channel;
	player->audio_info.bits_per_sample = media_info.audio_stream.bits_per_sample;

	if (player->type == PLAYER_FILE_MP3) {
		player->decoder = aic_audio_decoder_create(MPP_CODEC_AUDIO_DECODER_MP3);
		if (player->decoder == NULL) {
			MINI_AUDIO_PLAYER_ERROR("aic_audio_decoder_create fail\n");
			goto _exit;
		}
		player->dec_cfg.packet_buffer_size = 4*1024;
		player->dec_cfg.packet_count = 2;
		player->dec_cfg.frame_count = 2;
		aic_audio_decoder_init(player->decoder, &player->dec_cfg);
	}

	aic_audio_render_create(&player->render);
	if (player->render == NULL) {
		MINI_AUDIO_PLAYER_ERROR("aic_audio_render_create fail\n");
		goto _exit;
	}
	if (aic_audio_render_init(player->render,0)) {
		MINI_AUDIO_PLAYER_ERROR("aic_audio_render_init fail\n");
		goto _exit;
	}

	// set volume
	if (player->volume != 0) {
		aic_audio_render_set_volume(player->render,player->volume);
	} else {
		player->volume = aic_audio_render_get_volume(player->render);
	}
	// set attr
	attr.smples_per_frame = 8*1024;
	attr.bits_per_sample = 16;
	attr.channels = player->audio_info.nb_channel;
	attr.sample_rate = player->audio_info.sample_rate;
	if (aic_audio_render_set_attr(player->render,&attr)) {
		MINI_AUDIO_PLAYER_ERROR("aic_audio_render_set_attr fail\n");
		goto _exit;
	}


	return 0;
_exit:
	if (player->parser) {
		aic_parser_destroy(player->parser);
		player->parser = NULL;
	}
	if (player->decoder) {
		aic_audio_decoder_destroy(player->decoder);
		player->decoder = NULL;
	}
	if (player->render) {
		aic_audio_render_destroy(player->render);
		player->render = NULL;
	}
	return -1;
}

static int mini_audio_player_close(struct mini_audio_player *player)
{
	if (player->parser) {
		aic_parser_destroy(player->parser);
		player->parser = NULL;
	}
	if (player->decoder) {
		aic_audio_decoder_destroy(player->decoder);
		player->decoder = NULL;
	}
	if (player->render) {
		aic_audio_render_destroy(player->render);
		player->render = NULL;
	}
	return 0;
}

static int mini_audio_playe_event_handler(struct mini_audio_player *player, int timeout)
{
	int event;
	int  result;
	struct aic_message msg;
	u64 time_out;
#ifdef _MINI_AUDIO_PLAYER_DEBUG_
	int last_state;
#endif
	//0-ok,other-fail
	if (timeout == WAIT_TIME_FOREVER) {
		time_out = UINT64_MAX;
	} else {
		time_out = timeout;
	}
	aic_msg_wait_new_msg(&player->mq,time_out);
	result = aic_msg_get(&player->mq, &msg);
	if (result != 0) {
		event = MINI_AUDIO_PLAYER_MSG_NONE;
		return event;
	}
#ifdef _MINI_AUDIO_PLAYER_DEBUG_
	last_state = player->state;
#endif
	switch (msg.message_id) {
	case MINI_AUDIO_PLAYER_MSG_START:
		event = MINI_AUDIO_PLAYER_EVENT_PLAY;
		player->state = MINI_AUDIO_PLAYER_STATE_PLAYING;
		break;
	case MINI_AUDIO_PLAYER_MSG_STOP:
		event = MINI_AUDIO_PLAYER_EVENT_STOP;
		player->state = MINI_AUDIO_PLAYER_STATE_STOPED;
		break;

	case MINI_AUDIO_PLAYER_MSG_PAUSE:
		event = MINI_AUDIO_PLAYER_EVENT_PAUSE;
		player->state = MINI_AUDIO_PLAYER_STATE_PAUSED;
		break;

	case MINI_AUDIO_PLAYER_MSG_RESUME:
		event = MINI_AUDIO_PLAYER_EVENT_RESUME;
		player->state = MINI_AUDIO_PLAYER_STATE_PLAYING;
		break;

	case MINI_AUDIO_PLAYER_MSG_DESTROY:
		event = MINI_AUDIO_PLAYER_EVENT_DESTROY;
		break;

	default:
		event = MINI_AUDIO_PLAYER_EVENT_NONE;
		break;
	}
	sem_post(&player->sem_ack);

	MINI_AUDIO_PLAYER_DEBUG("%d %d->%d\n", event, last_state, player->state);

	return event;
}

static void* mini_audio_player_entry(void *parameter)
{
	int result;
	int event;
	struct aic_parser_packet parser_pkt;
	struct mpp_packet decoder_pkt;
	struct aic_audio_frame audio_frame;
	struct mini_audio_player *player = (struct mini_audio_player *)parameter;
	int eos;
	int need_peek = 1;
	int parser_ret = 0;
	int decoder_ret = 0;

	while(1) {
		event = mini_audio_playe_event_handler(player, WAIT_TIME_FOREVER);
		if (event == MINI_AUDIO_PLAYER_EVENT_DESTROY) {
			goto _exit;
		}
		if (event != MINI_AUDIO_PLAYER_EVENT_PLAY) {
			continue;
		}
		/* open mp3 player */
		result = mini_audio_player_open(player);
		if (result != 0) {
			player->state = MINI_AUDIO_PLAYER_STATE_STOPED;
			MINI_AUDIO_PLAYER_ERROR("mini_audio_player_open failed\n");
			continue;
		}
		eos =0;
		need_peek = 1;
		while(1) {
			event = mini_audio_playe_event_handler(player, 1);
			switch (event) {
			case MINI_AUDIO_PLAYER_EVENT_NONE: {
				if (player->type != PLAYER_FILE_WAV) {
					if (!eos) {
						if (need_peek) {
							parser_ret = aic_parser_peek(player->parser,&parser_pkt);
						}
						if (parser_ret != PARSER_EOS) {
							decoder_pkt.size =  parser_pkt.size;
							decoder_ret = aic_audio_decoder_get_packet(player->decoder,&decoder_pkt,decoder_pkt.size);
							if (decoder_ret == DEC_OK) {
								parser_pkt.data = decoder_pkt.data;
								parser_pkt.flag = 0;
								aic_parser_read(player->parser,&parser_pkt);
								decoder_pkt.flag = parser_pkt.flag;
								aic_audio_decoder_put_packet(player->decoder,&decoder_pkt);
								need_peek = 1;
							} else {
								need_peek = 0;
							}
						} else {
							eos = 1;
						}
					}
					aic_audio_decoder_decode(player->decoder);
					decoder_ret = aic_audio_decoder_get_frame(player->decoder,&audio_frame);
					if (decoder_ret == DEC_OK) {
						if (aic_audio_render_get_cached_time(player->render) > 100*1000) {// 100 ms
							usleep(50*1000);
						}
						aic_audio_render_rend(player->render,audio_frame.data,audio_frame.size);
						aic_audio_decoder_put_frame(player->decoder,&audio_frame);
						if (audio_frame.flag & PARSER_EOS) {
							player->state = MINI_AUDIO_PLAYER_STATE_STOPED;
						}
					}
				} else {// wav
					parser_ret = aic_parser_peek(player->parser,&parser_pkt);
					if (parser_ret == PARSER_EOS) {
						player->state = MINI_AUDIO_PLAYER_STATE_STOPED;
						break;
					}
					if (parser_pkt.size > player->wav_buff_size) {
						MINI_AUDIO_PLAYER_ERROR("pkt size[%d] larger than wav_buf_size[%d]\n",parser_pkt.size,player->wav_buff_size);
						player->state = MINI_AUDIO_PLAYER_STATE_STOPED;
						break;
					}
					parser_pkt.data = player->wav_buff;
					aic_parser_read(player->parser,&parser_pkt);
					if (aic_audio_render_get_cached_time(player->render) > 100*1000) {// 100 ms
						usleep(50*1000);
					}
					aic_audio_render_rend(player->render,parser_pkt.data,parser_pkt.size);
				}
				break;
			}
			case MINI_AUDIO_PLAYER_EVENT_PAUSE: {
				event = mini_audio_playe_event_handler(player, WAIT_TIME_FOREVER);
				break;
			}
			default:
				break;
			}
			if (player->state == MINI_AUDIO_PLAYER_STATE_STOPED) {
				break;
			}
		}
		mini_audio_player_close(player);
	}
_exit:
	sem_post(&player->sem_thread_exit);
	return (void *)0;
}

struct mini_audio_player* mini_audio_player_create(void)
{
	struct mini_audio_player *player = NULL;
	int32_t err = 0;
	int8_t mq_create = 0;
	int8_t lock_create = 0;
	int8_t lock_attr_create = 0;
	int8_t sem_thread_exit_create = 0;
	int8_t sem_ack_create = 0;

	player = (struct mini_audio_player *)mpp_alloc(sizeof(struct mini_audio_player));
	if (player == NULL) {
		MINI_AUDIO_PLAYER_ERROR("aicos_malloc error\n");
		goto _exit;
	}

	memset(player,0x00,sizeof(struct mini_audio_player));

	if (aic_msg_create(&player->mq) < 0) {
		MINI_AUDIO_PLAYER_ERROR("aicos_queue_create error\n");
		goto _exit;
	}
	mq_create = 1;

	if (pthread_mutexattr_init(&player->lock_attr)) {
		MINI_AUDIO_PLAYER_ERROR("pthread_mutexattr_init error\n");
		goto _exit;
	}
	lock_attr_create = 1;

	pthread_mutexattr_settype(&player->lock_attr,PTHREAD_MUTEX_RECURSIVE);

	if (pthread_mutex_init(&player->lock, &player->lock_attr)) {
		MINI_AUDIO_PLAYER_ERROR("aicos_mutex_create error\n");
		goto _exit;
	}
	lock_create = 1;

	if (sem_init(&player->sem_thread_exit,0,0)) {
		MINI_AUDIO_PLAYER_ERROR("aicos_sem_create error\n");
		goto _exit;
	}
	sem_thread_exit_create = 1;

	if (sem_init(&player->sem_ack,0,0)) {
		MINI_AUDIO_PLAYER_ERROR("aicos_sem_create error\n");
		goto _exit;
	}
	sem_ack_create = 1;

	err = pthread_create(&player->tid, NULL,
						 mini_audio_player_entry, player);

	if(err || !player->tid) {
		MINI_AUDIO_PLAYER_ERROR("aicos_thread_create error\n");
		goto _exit;
	}
	return player;
_exit:
	if (player) {
		if (mq_create) {
			 aic_msg_destroy(&player->mq);
		}
		if (lock_attr_create) {
			pthread_mutexattr_destroy(&player->lock_attr);
		}
		if (lock_create) {
			 pthread_mutex_destroy(&player->lock);
		}
		if (sem_thread_exit_create) {
			sem_destroy(&player->sem_thread_exit);
		}
		if (sem_ack_create) {
			 sem_destroy(&player->sem_ack);
		}
		mpp_free(player);
	}
	return NULL;
}

int mini_audio_player_destroy(struct mini_audio_player *player)
{
	if (!player) {
		MINI_AUDIO_PLAYER_ERROR("para error");
		return -1;
	}

	pthread_mutex_lock(&player->lock);
	if (player->state != MINI_AUDIO_PLAYER_STATE_STOPED) {
		MINI_AUDIO_PLAYER_DEBUG(" ");
		mini_audio_player_stop(player);
	}
	pthread_mutex_unlock(&player->lock);
	MINI_AUDIO_PLAYER_DEBUG(" ");
	if (player->tid) {
		mini_audio_player_msg_send(player, MINI_AUDIO_PLAYER_MSG_DESTROY, NULL);
		MINI_AUDIO_PLAYER_DEBUG(" ");
		sem_wait(&player->sem_thread_exit);
	}
	MINI_AUDIO_PLAYER_DEBUG(" ");
	aic_msg_destroy(&player->mq);
	pthread_mutexattr_destroy(&player->lock_attr);
	pthread_mutex_destroy(&player->lock);
	sem_destroy(&player->sem_thread_exit);
	sem_destroy(&player->sem_ack);
	if (player->wav_buff) {
		mpp_free(player->wav_buff);
		player->wav_buff = NULL;
		player->wav_buff_size = 0;
	}
	pthread_join(player->tid, NULL);
	mpp_free(player);
	MINI_AUDIO_PLAYER_DEBUG(" ");
	return 0;
}
