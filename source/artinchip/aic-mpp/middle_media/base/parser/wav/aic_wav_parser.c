/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: aic_wav_parser
 */

#include <malloc.h>
#include <string.h>
#include <stddef.h>
#include <fcntl.h>
#include "aic_mov_parser.h"
#include "mpp_log.h"
#include "mpp_mem.h"
#include "mpp_dec_type.h"
#include "aic_stream.h"
#include "wav.h"
#include "aic_wav_parser.h"

s32 wav_peek(struct aic_parser * parser, struct aic_parser_packet *pkt)
{
    struct aic_wav_parser *wav_parser = (struct aic_wav_parser *)parser;
    return wav_peek_packet(wav_parser,pkt);
}

s32 wav_read(struct aic_parser * parser, struct aic_parser_packet *pkt)
{
    struct aic_wav_parser *wav_parser = (struct aic_wav_parser *)parser;
    return wav_read_packet(wav_parser,pkt);
}

s32 wav_get_media_info(struct aic_parser *parser, struct aic_parser_av_media_info *media)
{
    struct aic_wav_parser *wav_parser = (struct aic_wav_parser *)parser;
    media->has_video = 0;
    media->has_audio = 1;
    media->file_size = wav_parser->file_size;
    media->duration = wav_parser->duration;
    media->audio_stream.codec_type = MPP_CODEC_AUDIO_DECODER_PCM;
    media->audio_stream.bits_per_sample = wav_parser->bits_per_sample;
    media->audio_stream.nb_channel = wav_parser->channels;
    media->audio_stream.sample_rate = wav_parser->sample_rate;
    return 0;
}

s32 wav_seek(struct aic_parser *parser, s64 time)
{
    struct aic_wav_parser *wav_parser = (struct aic_wav_parser *)parser;
    return wav_seek_packet(wav_parser,time);
}

s32 wav_init(struct aic_parser *parser)
{
    struct aic_wav_parser *wav_parser = (struct aic_wav_parser *)parser;

    if (wav_read_header(wav_parser)) {
        loge("wav init failed");
        return -1;
    }
    return 0;
}

s32 wav_destroy(struct aic_parser *parser)
{
    struct aic_wav_parser *wav_parser = (struct aic_wav_parser *)parser;

    if (wav_parser == NULL) {
        return -1;
    }
    wav_close(wav_parser);
    aic_stream_close(wav_parser->stream);
    mpp_free(wav_parser);
    return 0;
}

s32 aic_wav_parser_create(unsigned char *uri, struct aic_parser **parser)
{
    s32 ret = 0;
    struct aic_wav_parser *wav_parser = NULL;

    wav_parser = (struct aic_wav_parser *)mpp_alloc(sizeof(struct aic_wav_parser));
    if (wav_parser == NULL) {
        loge("mpp_alloc aic_parser failed!!!!!\n");
        ret = -1;
        goto exit;
    }
    memset(wav_parser, 0, sizeof(struct aic_wav_parser));

    if (aic_stream_open((char *)uri, &wav_parser->stream, O_RDONLY) < 0) {
        loge("stream open fail");
        ret = -1;
        goto exit;
    }

    wav_parser->base.get_media_info = wav_get_media_info;
    wav_parser->base.peek = wav_peek;
    wav_parser->base.read = wav_read;
    wav_parser->base.control = NULL;
    wav_parser->base.destroy = wav_destroy;
    wav_parser->base.seek = wav_seek;
    wav_parser->base.init = wav_init;

    *parser = &wav_parser->base;

    return ret;

exit:
    if (wav_parser->stream) {
        aic_stream_close(wav_parser->stream);
    }
    if (wav_parser) {
        mpp_free(wav_parser);
    }
    return ret;
}
