/*
* Copyright (C) 2020-2024 Artinchip Technology Co. Ltd
*
*  author: <che.jiang@artinchip.com>
*  Desc: aic_mkv_parser
*/
#define LOG_TAG "mkv_parse"

#include <malloc.h>
#include <string.h>
#include <stddef.h>
#include <fcntl.h>
#include <inttypes.h>
#include "aic_mkv_parser.h"
#include "mpp_log.h"
#include "mpp_mem.h"
#include "mpp_dec_type.h"
#include "aic_stream.h"
#include "aic_parser.h"
#include "matroska.h"

s32 mkv_peek(struct aic_parser *parser, struct aic_parser_packet *pkt)
{
    struct aic_matroska_parser *mkv_parse_parser = (struct aic_matroska_parser *)parser;

    return matroska_peek_packet(mkv_parse_parser, pkt);
}

s32 mkv_read(struct aic_parser *parser, struct aic_parser_packet *pkt)
{
    struct aic_matroska_parser *mkv_parse_parser = (struct aic_matroska_parser *)parser;

    return matroska_read_packet(mkv_parse_parser, pkt);
}


s32 mkv_control(struct aic_parser *parser, enum parse_command cmd, void *param)
{
    struct aic_matroska_parser *mkv_parse_parser = (struct aic_matroska_parser *)parser;

    return matroska_control(mkv_parse_parser, cmd, param);
}

s32 mkv_get_media_info(struct aic_parser *parser,
                       struct aic_parser_av_media_info *media)
{
    int i;
    int64_t duration = 0;
    struct aic_matroska_parser *c = (struct aic_matroska_parser *)parser;

    logi("================ media info =======================");
    for (i = 0; i < c->nb_streams; i++) {
        struct matroska_stream_ctx *st = c->streams[i];
        if (st->codecpar.codec_type == MPP_MEDIA_TYPE_VIDEO) {
            media->has_video = 1;
            if (st->codecpar.codec_id == CODEC_ID_H264)
                media->video_stream.codec_type = MPP_CODEC_VIDEO_DECODER_H264;
            else if (st->codecpar.codec_id == CODEC_ID_MJPEG)
                media->video_stream.codec_type = MPP_CODEC_VIDEO_DECODER_MJPEG;
            else
                media->video_stream.codec_type = -1;

            media->video_stream.width = st->codecpar.width;
            media->video_stream.height = st->codecpar.height;
            if (st->codecpar.extradata_size > 0) {
                media->video_stream.extra_data_size =
                    st->codecpar.extradata_size;
                media->video_stream.extra_data = st->codecpar.extradata;
            }
            logi("video codec_type: %d codec_id %d",
                 media->video_stream.codec_type, st->codecpar.codec_id);
            logi("video width: %d, height: %d", st->codecpar.width,
                 st->codecpar.height);
            logi("video extra_data_size: %d", st->codecpar.extradata_size);
        } else if (st->codecpar.codec_type == MPP_MEDIA_TYPE_AUDIO) {
            media->has_audio = 1;
            if (st->codecpar.codec_id == CODEC_ID_MP3)
                media->audio_stream.codec_type = MPP_CODEC_AUDIO_DECODER_MP3;
            else if (st->codecpar.codec_id == CODEC_ID_AAC)
                media->audio_stream.codec_type = MPP_CODEC_AUDIO_DECODER_AAC;
            else
                media->audio_stream.codec_type = MPP_CODEC_AUDIO_DECODER_UNKOWN;

            media->audio_stream.bits_per_sample =
                st->codecpar.bits_per_coded_sample;
            media->audio_stream.nb_channel = st->codecpar.channels;
            media->audio_stream.sample_rate = st->codecpar.sample_rate;
            if (st->codecpar.extradata_size > 0) {
                media->audio_stream.extra_data_size =
                    st->codecpar.extradata_size;
                media->audio_stream.extra_data = st->codecpar.extradata;
            }
            logi("audio bits_per_sample: %d",
                 st->codecpar.bits_per_coded_sample);
            logi("audio channels: %d", st->codecpar.channels);
            logi("audio sample_rate: %d", st->codecpar.sample_rate);
            logi("audio extra_data_size: %d", st->codecpar.extradata_size);
        } else {
            loge("unknown stream(%d) type: %d", i, st->codecpar.codec_type);
        }

        if (st->duration > duration)
            duration = st->duration;
    }

    media->file_size = aic_stream_size(c->stream);
    media->seek_able = 1;
    media->duration = duration;

    return 0;
}

s32 mkv_seek(struct aic_parser *parser, s64 time)
{
    s32 ret = 0;
    struct aic_matroska_parser *mkv_parse_parser = (struct aic_matroska_parser *)parser;
    ret = matroska_seek_packet(mkv_parse_parser, time);
    return ret;
}

s32 mkv_parse_init(struct aic_parser *parser)
{
    struct aic_matroska_parser *mkv_parse_parser = (struct aic_matroska_parser *)parser;

    if (matroska_read_header(mkv_parse_parser) < 0) {
        loge("mkv_parse open failed");
        return -1;
    }

    return 0;
}

s32 mkv_parse_destroy(struct aic_parser *parser)
{
    struct aic_matroska_parser *mkv_parser = (struct aic_matroska_parser *)parser;
    if (parser == NULL) {
        return -1;
    }

    matroska_read_close(mkv_parser);
    aic_stream_close(mkv_parser->stream);
    mpp_free(mkv_parser);
    return 0;
}

s32 aic_mkv_parser_create(unsigned char *uri, struct aic_parser **parser)
{
    s32 ret = 0;
    struct aic_matroska_parser *mkv_parser = NULL;

    mkv_parser =
        (struct aic_matroska_parser *)mpp_alloc(sizeof(struct aic_matroska_parser));
    if (mkv_parser == NULL) {
        loge("mpp_alloc aic_parser failed!!!!!\n");
        ret = -1;
        goto exit;
    }
    memset(mkv_parser, 0, sizeof(struct aic_matroska_parser));

    if (aic_stream_open((char *)uri, &mkv_parser->stream, O_RDONLY) < 0) {
        loge("stream open fail");
        ret = -1;
        goto exit;
    }

    mkv_parser->base.get_media_info = mkv_get_media_info;
    mkv_parser->base.peek = mkv_peek;
    mkv_parser->base.read = mkv_read;
    mkv_parser->base.control = mkv_control;
    mkv_parser->base.destroy = mkv_parse_destroy;
    mkv_parser->base.seek = mkv_seek;
    mkv_parser->base.init = mkv_parse_init;

    *parser = &mkv_parser->base;

    return ret;

exit:
    if (mkv_parser->stream) {
        aic_stream_close(mkv_parser->stream);
    }
    if (mkv_parser) {
        mpp_free(mkv_parser);
    }
    return ret;
}
