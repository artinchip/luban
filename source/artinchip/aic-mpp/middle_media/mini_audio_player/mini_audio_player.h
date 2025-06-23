/*
 * Copyright (C) 2020-2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: mini_audio_player
 */

#ifndef __MINI_AUDIO_PLAYER_H__
#define __MINI_AUDIO_PLAYER_H__

#include "mpp_dec_type.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct mini_audio_player;

enum MINI_AUDIO_PLAYER_STATE {
	MINI_AUDIO_PLAYER_STATE_STOPED = 0,
	MINI_AUDIO_PLAYER_STATE_PLAYING = 1,
	MINI_AUDIO_PLAYER_STATE_PAUSED = 2,
};

struct mini_player_audio_info {
	s64 file_size;
	s64 duration;
	s32 nb_channel;
	s32 bits_per_sample;
	s32 sample_rate;
};

struct mini_audio_player* mini_audio_player_create(void);

int mini_audio_player_destroy(struct mini_audio_player *player);

int mini_audio_player_play(struct mini_audio_player *player,char *uri);

int mini_audio_player_stop(struct mini_audio_player *player);

int mini_audio_player_pause(struct mini_audio_player *player);

int mini_audio_player_resume(struct mini_audio_player *player);

int mini_audio_player_get_media_info(struct mini_audio_player *player,struct mini_player_audio_info *audio_info);

int mini_audio_player_set_volume(struct mini_audio_player *player,int vol);

int mini_audio_player_get_volume(struct mini_audio_player *player,int *vol);

int mini_audio_player_get_state(struct mini_audio_player *player);


#ifdef __cplusplus
}
#endif /* End of #ifdef __cplusplus */

#endif
