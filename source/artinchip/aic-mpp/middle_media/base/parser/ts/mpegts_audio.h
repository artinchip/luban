/*
 * Copyright (C) 2020-2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: <che.jiang@artinchip.com>
 * Desc: mp3 and aac audio frame analyse
 */

#ifndef __MPEGTS_AUDIO_H__
#define __MPEGTS_AUDIO_H__

#include <unistd.h>
#include "aic_parser.h"

#define MPEGTS_ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))

struct mpegts_mp3_decode_header {
    int error_protection;
    int mode;
    int mode_ext;
    int lsf;
};

struct mpegts_aac_decode_header {
    int header_offset;
};

struct mpegts_audio_decode_header {
    int type;
    int frame_size; /* size after encoding */
    int layer;
    int sample_rate;
    int sample_rate_index;
    int bit_rate;
    int nb_channels;
    int nb_samples_per_frame;
    int frame_duration; /* us */
    union {
        struct mpegts_mp3_decode_header mp3;
        struct mpegts_aac_decode_header aac;
    };
};

int mpegaudio_decode_mp3_header(uint8_t *data, struct mpegts_audio_decode_header *s);
int mpegaudio_decode_aac_header(uint8_t *data, struct mpegts_audio_decode_header *s);
#endif
