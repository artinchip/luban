/*
 * Copyright (C) 2020-2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: <che.jiang@artinchip.com>
 * Desc: flv file demuxer
 */

#ifndef FLV_H
#define FLV_H

#include <unistd.h>
#include "aic_parser.h"
#include "aic_tag.h"

struct flv_stream_ctx {
    void *priv_data;
    int index;
    int64_t duration;
    struct aic_codec_param codecpar;
};

#define FLV_MAX_TRACK_NUM 8
struct aic_flv_parser {
    struct aic_parser base;
    struct aic_stream *stream;
    void *priv_data;
    int64_t duration;
    int nb_streams;
    struct flv_stream_ctx *streams[FLV_MAX_TRACK_NUM];
};

int flv_read_header(struct aic_flv_parser *s);
int flv_read_close(struct aic_flv_parser *s);
int flv_peek_packet(struct aic_flv_parser *s, struct aic_parser_packet *pkt);
int flv_seek_packet(struct aic_flv_parser *s, s64 pts);
int flv_read_packet(struct aic_flv_parser *s, struct aic_parser_packet *pkt);
#endif /* FLV_H */
