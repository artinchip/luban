/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: OMX_VdecComponent tunneld  OMX_VideoRenderComponent demo
*/

#ifndef __AIC_RECORDER_H__
#define __AIC_RECORDER_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "mpp_dec_type.h"
#include "aic_middle_media_common.h"

struct aic_recorder;

struct video_encoding_config {
	enum mpp_codec_type codec_type;
	s32   out_width;
	s32   out_height;
	s32   out_bit_rate;
	s32   out_frame_rate;
	s32   out_qfactor;
	//now must be  out_width = in_width and out_height= in_height
	//case mjpeg encoder has no scale function
	s32   in_width;
	s32   in_height;
	s32   in_pix_fomat;
};

struct audio_encoding_config {
	enum aic_audio_codec_type   codec_type;
	int   out_bitrate;
	int   out_samplerate;
	int   out_channels;
	int   out_bits_per_sample;

	int   in_samplerate;
	int   in_channels;
	int   in_bits_per_sample;
};

struct aic_recorder_config {
	int file_duration;//unit:second one file duration
	int file_num;//0-loop, >0 record file_num and then stop recording.
	int file_muxer_type;//only support  mp4
	int qfactor;
	s8 has_video;
	s8 has_audio;
	struct audio_encoding_config audio_config;
	struct video_encoding_config video_config;
};

enum aic_recorder_event {
	AIC_RECORDER_EVENT_NEED_NEXT_FILE = 0,
	AIC_RECORDER_EVENT_COMPLETE, // when file_num > 0,record file_num then send this event
	AIC_RECORDER_EVENT_NO_SPACE,
	AIC_RECORDER_EVENT_RELEASE_VIDEO_BUFFER // notify app input_frame has used.
};

typedef s32 (*event_handler)(void* app_data,s32 event,s32 data1,s32 data2);

struct aic_recorder *aic_recorder_create(void);

s32 aic_recorder_destroy(struct aic_recorder *recorder);

s32 aic_recorder_set_event_callback(struct aic_recorder *recorder,void* app_data,event_handler event_handle);

s32 aic_recorder_set_output_file_path(struct aic_recorder *recorder, char *uri);

s32 aic_recorder_init(struct aic_recorder *recorder,struct aic_recorder_config *recorder_config);

s32 aic_recorder_start(struct aic_recorder *recorder);

s32 aic_recorder_stop(struct aic_recorder *recorder);

s32 aic_recorder_write_video_frame(struct aic_recorder *recorder, struct mpp_frame *frame);

//s32 aic_recorder_write_audio_frame(struct aic_recorder *recorder, struct mpp_frame *frame);

#ifdef __cplusplus
}
#endif /* End of #ifdef __cplusplus */

#endif
