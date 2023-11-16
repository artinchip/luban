/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: aic_audio_decoder interface
*/



#ifndef _AUDIO_DECODER_H_
#define _AUDIO_DECODER_H_

#include "mpp_dec_type.h"
#include "mpp_decoder.h"
#include "aic_audio_decoder.h"
#include "audio_frame_manager.h"
#include "audio_packet_manager.h"

struct aic_audio_decoder {
	struct aic_audio_decoder_ops *ops;
	struct audio_packet_manager* pm;
	struct audio_frame_manager* fm;
};

struct aic_audio_decoder_ops {
	const char *name;
	s32 (*init)(struct aic_audio_decoder *decoder, struct aic_audio_decode_config *config);
	s32 (*destroy)(struct aic_audio_decoder *decoder);
	s32 (*decode)(struct aic_audio_decoder *decoder);
	s32 (*control)(struct aic_audio_decoder *decoder, int cmd, void *param);
	s32 (*reset)(struct aic_audio_decoder *decoder);
};




#endif
