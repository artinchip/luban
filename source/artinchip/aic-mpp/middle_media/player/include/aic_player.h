/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: aic_player
 */

#ifndef __AIC_PLAYER_H__
#define __AIC_PLAYER_H__

#include "mpp_dec_type.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

struct aic_player;

#define AIC_PLAYER_PREPARE_SYNC 0
#define AIC_PLAYER_PREPARE_ASYNC 1

enum aic_player_event {
	AIC_PLAYER_EVENT_PLAY_END = 0,	// play file  to end
	AIC_PLAYER_EVENT_PLAY_TIME,		//pts = data1<<32|data2
	AIC_PLAYER_EVENT_DEMUXER_FORMAT_DETECTED,
	AIC_PLAYER_EVENT_DEMUXER_FORMAT_NOT_DETECTED
};

// now do not support  setting  nWidth and  nHeight,just only support setting pFilePath
struct aic_capture_info {
    s8 *file_path;
    s32 width;
    s32 height;
    s32 quality;
};

struct aic_video_stream {
	s32   width;
	s32   height;
};

struct aic_audio_stream {
	s32 nb_channel;
	s32 bits_per_sample;
	s32 sample_rate;
};

struct av_media_info {
	s64  file_size;
	s64  duration;
	u8   has_video;
	u8   has_audio;
	u8   seek_able;
	struct aic_video_stream video_stream;
	struct aic_audio_stream audio_stream;
};

typedef s32 (*event_handler)(void* app_data,s32 event,s32 data1,s32 data2);

struct aic_player *aic_player_create(char *uri);

s32 aic_player_destroy(struct aic_player *player);

s32 aic_player_set_uri(struct aic_player *player,char *uri);

s32 aic_player_prepare_async(struct aic_player *player);

s32 aic_player_prepare_sync(struct aic_player *player);

s32 aic_player_start(struct aic_player *player);

s32 aic_player_play(struct aic_player *player);

s32 aic_player_pause(struct aic_player *player);

s32 aic_player_stop(struct aic_player *player);

s32 aic_player_get_media_info(struct aic_player *player,struct av_media_info *media_info);

s32 aic_player_set_event_callback(struct aic_player *player,void* app_data,event_handler event_handle);

s32 aic_player_get_screen_size(struct aic_player *player,struct mpp_size *screen_size);

s32 aic_player_set_disp_rect(struct aic_player *player,struct mpp_rect *disp_rect);

s32 aic_player_get_disp_rect(struct aic_player *player,struct mpp_rect *disp_rect);

s32 aic_player_set_mute(struct aic_player *player);

s32 aic_player_set_volum(struct aic_player *player,s32 vol);

s32 aic_player_get_volum(struct aic_player *player,s32 *vol);

s64 aic_player_get_play_time(struct aic_player *player);

s32 aic_player_seek(struct aic_player *player, u64 seek_time);

/* only can capture when state in pause,so call aic_player_pause before call this function */
s32 aic_player_capture(struct aic_player *player, struct aic_capture_info *capture_info);

//MPP_ROTATION_0  MPP_ROTATION_90  MPP_ROTATION_180 MPP_ROTATION_270

s32 aic_player_set_rotation(struct aic_player *player, int rotation_angle);

s32 aic_player_get_rotation(struct aic_player *player);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif

