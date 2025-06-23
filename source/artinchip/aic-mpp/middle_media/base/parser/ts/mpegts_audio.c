/*
 * Copyright (C) 2020-2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: <che.jiang@artinchip.com>
 * Desc: mp3 and aac audio frame analyse
 */

#include "mpegts_audio.h"
#include "aic_stream.h"
#include "mpp_log.h"
#include "mpp_mem.h"
#include <inttypes.h>
#include <stdlib.h>

#define MPA_DUAL 2
#define MPA_MONO 3

const uint16_t mpegts_mpa_freq_tab[3] = {44100, 48000, 32000};
const uint32_t mpegts_aac_freq_tab[] = {96000, 88200, 64000, 48000, 44100, 32000,
                                        24000, 22050, 16000, 12000, 11025, 8000, 7350};

const uint16_t mpegts_mpa_bitrate_tab[2][3][15] = {
    {{0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448},
     {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384},
     {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320}},
    {{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256},
     {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160},
     {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}}};

/* fast header check for resync */
static int mpa_check_header(uint32_t header)
{
    /* header */
    if ((header & 0xffe00000) != 0xffe00000)
        return -1;
    /* version check */
    if ((header & (3 << 19)) == 1 << 19)
        return -1;
    /* layer check */
    if ((header & (3 << 17)) == 0)
        return -1;
    /* bit rate */
    if ((header & (0xf << 12)) == 0xf << 12)
        return -1;
    /* frequency */
    if ((header & (3 << 10)) == 3 << 10)
        return -1;
    return 0;
}

int mpegaudio_decode_mp3_header(uint8_t *data, struct mpegts_audio_decode_header *s)
{
    int sample_rate, frame_size, mpeg25, padding;
    int sample_rate_index, bitrate_index;
    int ret;
    uint32_t header = AIC_RB32(data);
    ret = mpa_check_header(header);
    if (ret < 0)
        return ret;

    if (header & (1 << 20)) {
        s->mp3.lsf = (header & (1 << 19)) ? 0 : 1;
        mpeg25 = 0;
    } else {
        s->mp3.lsf = 1;
        mpeg25 = 1;
    }

    s->layer = 4 - ((header >> 17) & 3);

    /* extract frequency */
    sample_rate_index = (header >> 10) & 3;
    if (sample_rate_index >= MPEGTS_ARRAY_ELEMS(mpegts_mpa_freq_tab))
        sample_rate_index = 0;
    sample_rate = mpegts_mpa_freq_tab[sample_rate_index] >> (s->mp3.lsf + mpeg25);
    sample_rate_index += 3 * (s->mp3.lsf + mpeg25);
    s->sample_rate_index = sample_rate_index;
    s->mp3.error_protection = ((header >> 16) & 1) ^ 1;
    s->sample_rate = sample_rate;

    s->nb_samples_per_frame = 1152;
    if (s->layer == 1) {
        s->nb_samples_per_frame = 384;
    } else if (s->layer == 3 && s->mp3.lsf) {
        s->nb_samples_per_frame = 576;
    } else {
        s->nb_samples_per_frame = 1152;
    }
    s->frame_duration = s->nb_samples_per_frame * 1000 * 1000 / s->sample_rate;

    bitrate_index = (header >> 12) & 0xf;
    padding = (header >> 9) & 1;
    s->mp3.mode = (header >> 6) & 3;
    s->mp3.mode_ext = (header >> 4) & 3;

    if (s->mp3.mode == MPA_MONO)
        s->nb_channels = 1;
    else
        s->nb_channels = 2;

    if (bitrate_index != 0) {
        frame_size = mpegts_mpa_bitrate_tab[s->mp3.lsf][s->layer - 1][bitrate_index];
        s->bit_rate = frame_size * 1000;
        switch (s->layer) {
        case 1:
            frame_size = (frame_size * 12000) / sample_rate;
            frame_size = (frame_size + padding) * 4;
            break;
        case 2:
            frame_size = (frame_size * 144000) / sample_rate;
            frame_size += padding;
            break;
        default:
        case 3:
            frame_size = (frame_size * 144000) / (sample_rate << s->mp3.lsf);
            frame_size += padding;
            break;
        }
        s->frame_size = frame_size;
    } else {
        /* if no frame size computed, signal it */
        return -1;
    }
    return 0;
}

static int aac_check_header(uint32_t header)
{
    /* header */
    if ((header & 0xfff00000) != 0xfff00000)
        return -1;
    /* layer check */
    if ((header & (3 << 17)) != 0)
        return -1;
    /* protection absent check */
    if ((header & (1 << 16)) != (1 << 16))
        return -1;
    /* profile check, 1 for aac */
    if ((header & (3 << 14)) != (1 << 14))
        return -1;
    /* origininal check */
    if ((header & (1 << 5)) != 0)
        return -1;
    /* home check */
    if ((header & (1 << 4)) != 0)
        return -1;

    return 0;
}

int mpegaudio_decode_aac_header(uint8_t *data, struct mpegts_audio_decode_header *s)
{
    int frame_size;
    int sample_rate_index;
    int ret;
    /*adts(Audio dta transport stream) include 7Byte*/
    uint32_t header = AIC_RB32(data);
    uint32_t header1 = AIC_RB24(data + 4);

    /*Get adts fixed header: 28 bits*/
    ret = aac_check_header(header);
    if (ret < 0) {
        logw("aac_check_header failed, header: 0x%x", header);
        return ret;
    }

    sample_rate_index = (header >> 10) & 0xf;
    if (sample_rate_index >= MPEGTS_ARRAY_ELEMS(mpegts_aac_freq_tab))
        sample_rate_index = 4;

    s->sample_rate = mpegts_aac_freq_tab[sample_rate_index];

    /*Get adts variable header: 28 bits*/
    frame_size = ((header & 0x3) << 11);
    frame_size |= ((header1 >> 13) & 0x7FF);
    s->frame_size = frame_size;
    s->aac.header_offset = 7;
    /*one frame include 1024 sample*/
    s->frame_duration = 1024 * 1000 * 1000 / s->sample_rate;
    return 0;
}
