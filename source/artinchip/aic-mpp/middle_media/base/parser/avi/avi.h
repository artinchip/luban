/*
 * Copyright (C) 2020-2024 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <che.jiang@artinchip.com>
 *  Desc: avi.h
 */

#ifndef __AVI_H__
#define __AVI_H__

#include <unistd.h>
#include "aic_parser.h"
#include "aic_tag.h"

#define AVIF_HASINDEX       0x00000010 // Index at end of file?
#define AVIF_MUSTUSEINDEX   0x00000020
#define AVIF_ISINTERLEAVED  0x00000100
#define AVIF_TRUSTCKTYPE    0x00000800 // Use CKType to find key frames?
#define AVIF_WASCAPTUREFILE 0x00010000
#define AVIF_COPYRIGHTED    0x00020000

#define AVI_MAX_RIFF_SIZE 0x40000000LL

/* stream header flags */
#define AVISF_VIDEO_PALCHANGES 0x00010000

/* index flags */
#define AVIIF_INDEX   0x00000010
#define AVIIF_NO_TIME 0x00000100

#define AV_INPUT_BUFFER_PADDING_SIZE 64

struct avi_index_entry {
    int64_t pos;
    int64_t timestamp; // dts
    int size;
    int min_distance;
    int flags;
};

struct avi_stream {
    /* current frame (video) or byte (audio) counter * (used to compute the pts) */
    int64_t frame_offset;
    uint32_t handler;
    uint32_t scale;
    uint32_t rate;
    /* size of one sample (or packet) * (in the rate/scale sense) in bytes */
    int sample_size;
    int64_t cum_len; /* temporary storage (used during seek) */
    /* block align variable used to emulate bugs in * the MS dshow demuxer */
    int dshow_block_align;
};

struct avi_stream_ctx {
    void *priv_data;
    int index;
    int64_t nb_frames;
    int64_t duration;

    int cur_sample_idx;
    struct aic_codec_param codecpar;

    int nb_index_entries;
    int alloc_entries_flag;
    struct avi_index_entry *index_entries;
};

struct avi_context {
    int64_t riff_end;
    int64_t movi_end;
    int64_t fsize;
    int64_t io_fsize;
    int64_t movi_list;
    int64_t last_pkt_pos;
    int index_loaded;
    int is_odml;
    int non_interleaved;
    int stream_index;
    int odml_depth;
    int use_odml;
#define MAX_ODML_DEPTH 1000
    int64_t dts_max;
};

#define AVI_MAX_TRACK_NUM 8
struct aic_avi_parser {
    struct aic_parser base;
    struct aic_stream *stream;
    struct avi_context avi_c;

    int nb_streams;
    struct avi_stream_ctx *streams[AVI_MAX_TRACK_NUM];
    struct avi_index_entry *cur_sample;
};

int avi_read_header(struct aic_avi_parser *s);
int avi_read_close(struct aic_avi_parser *s);
int avi_peek_packet(struct aic_avi_parser *c, struct aic_parser_packet *pkt);
int avi_seek_packet(struct aic_avi_parser *c, s64 pts);
int avi_read_packet(struct aic_avi_parser *s, struct aic_parser_packet *pkt);
#endif /* __AVIDEC_H__ */
