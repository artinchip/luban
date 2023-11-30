/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
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

enum aic_stream_type {
	MPP_MEDIA_TYPE_UNKNOWN = -1,
	MPP_MEDIA_TYPE_VIDEO,
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

#define MKTAG(a,b,c,d) ((a) | ((b) << 8) | ((c) << 16) | ((unsigned)(d) << 24))

enum CodecID {
	CODEC_ID_NONE,

	/* video codecs */
	CODEC_ID_MJPEG,
	CODEC_ID_H264,

	/* various PCM "codecs" */
	CODEC_ID_FIRST_AUDIO = 0x10000,     ///< A dummy id pointing at the start of audio codecs
	CODEC_ID_PCM_S16LE = 0x10000,
	CODEC_ID_PCM_S16BE,
	CODEC_ID_PCM_U16LE,
	CODEC_ID_PCM_U16BE,
	CODEC_ID_PCM_S8,
	CODEC_ID_PCM_U8,
	CODEC_ID_PCM_MULAW,
	CODEC_ID_PCM_ALAW,
	CODEC_ID_PCM_S32LE,
	CODEC_ID_PCM_S32BE,
	CODEC_ID_PCM_U32LE,
	CODEC_ID_PCM_U32BE,
	CODEC_ID_PCM_S24LE,
	CODEC_ID_PCM_S24BE,
	CODEC_ID_PCM_U24LE,
	CODEC_ID_PCM_U24BE,
	CODEC_ID_PCM_S24DAUD,
	CODEC_ID_PCM_ZORK,
	CODEC_ID_PCM_S16LE_PLANAR,
	CODEC_ID_PCM_DVD,
	CODEC_ID_PCM_F32BE,
	CODEC_ID_PCM_F32LE,
	CODEC_ID_PCM_F64BE,
	CODEC_ID_PCM_F64LE,
	CODEC_ID_PCM_BLURAY,
	CODEC_ID_PCM_LXF,
	CODEC_ID_PCM_S8_PLANAR,
	CODEC_ID_PCM_S24LE_PLANAR,
	CODEC_ID_PCM_S32LE_PLANAR,
	CODEC_ID_PCM_S16BE_PLANAR,
	CODEC_ID_PCM_S64LE,
	CODEC_ID_PCM_S64BE,
	CODEC_ID_PCM_F16LE,
	CODEC_ID_PCM_F24LE,
	CODEC_ID_PCM_VIDC,
	CODEC_ID_PCM_SGA,

	CODEC_ID_MP3, ///< preferred ID for decoding MPEG audio layer 1, 2 or 3
	CODEC_ID_AAC,
};

struct codec_tag {
	enum CodecID id;
	unsigned int tag;
};

enum aic_muxer_type {
	AIC_MUXER_TYPE_UNKNOWN = -1,
	AIC_MUXER_TYPE_MP4,
};

#ifdef __cplusplus
}
#endif /* End of #ifdef __cplusplus */
#endif
