/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: aic_audio_decoder interface
 */


#ifndef _AUDIO_FRAME_MANAGER_H_
#define _AUDIO_FRAME_MANAGER_H_

#include "aic_audio_decoder.h"


struct audio_frame_manager_cfg {
	int 				frame_count;
	int 				samples_per_frame;//include all channels
	int 				bits_per_sample;
};

struct audio_frame_manager;

/*
	create packet manager
*/

struct audio_frame_manager *audio_fm_create(struct audio_frame_manager_cfg *cfg);

/*
	destroy frame manager
*/
int audio_fm_destroy(struct audio_frame_manager *fm);


struct aic_audio_frame * audio_fm_decoder_get_frame(struct audio_frame_manager *fm);


int audio_fm_decoder_put_frame(struct audio_frame_manager *fm, struct aic_audio_frame *frame);


int audio_fm_render_get_frame(struct audio_frame_manager *fm,struct aic_audio_frame *frame);


int audio_fm_render_put_frame(struct audio_frame_manager *fm, struct aic_audio_frame *frame);


int audio_fm_get_empty_frame_num(struct audio_frame_manager *fm);


int audio_fm_get_render_frame_num(struct audio_frame_manager *fm);

int audio_fm_reset(struct audio_frame_manager *fm);

#endif /* PACKET_MANAGER_H */
