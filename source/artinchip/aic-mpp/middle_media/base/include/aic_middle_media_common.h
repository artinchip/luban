/*
 * Copyright (C) 2020-2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: aic_middle_media_common
 */

#ifndef __AIC_MIDDLE_MEDIA_COMMON_H__
#define __AIC_MIDDLE_MEDIA_COMMON_H__

#include "mpp_dec_type.h"
#include "aic_audio_decoder.h"
#include "aic_stream.h"
#include "mpp_log.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define MPP_TIME_BASE 1000000LL
#define MPP_MAX(a, b) ((a)>(b)? (a) : (b))
#define MPP_MIN(a, b) ((a)<(b)? (a) : (b))

enum aic_stream_type {
	MPP_MEDIA_TYPE_UNKNOWN = -1,
	MPP_MEDIA_TYPE_VIDEO = 1,
	MPP_MEDIA_TYPE_AUDIO,
	MPP_MEDIA_TYPE_OTHER
};


struct aic_av_packet {
	enum aic_stream_type type;
	void *data;
	s32 size;
	s64 pts;
	u32 flag;
	s64 dts;
	s32 duration;
	//s32 stream_index;
};

struct aic_av_video_stream {
	enum mpp_codec_type codec_type;
	s32   width;
	s32   height;
	s32   extra_data_size;
	u8    *extra_data;
	s32   bit_rate;
	s32   frame_rate;
};

struct aic_av_audio_stream {
	enum aic_audio_codec_type codec_type;
	s32 nb_channel;
	s32 bits_per_sample;
	s32 sample_rate;
	s32 extra_data_size;
	u8 *extra_data;
	s32 bit_rate;
};

struct aic_av_media_info {
	s64  file_size;
	s64  duration;
	u8   has_video;
	u8   has_audio;
	u8   seek_able;
	struct aic_av_video_stream video_stream;
	struct aic_av_audio_stream audio_stream;
};


enum aic_muxer_type {
	AIC_MUXER_TYPE_UNKNOWN = -1,
	AIC_MUXER_TYPE_MP4,
	AIC_MUXER_TYPE_AVI,
};

#ifdef __cplusplus
}
#endif /* End of #ifdef __cplusplus */
#endif
