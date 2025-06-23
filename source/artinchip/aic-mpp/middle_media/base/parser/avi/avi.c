/*
 * Copyright (C) 2020-2024 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <che.jiang@artinchip.com>
 *  Desc: avi.c
 */

#define LOG_TAG "avi"

#include <inttypes.h>
#include <stdlib.h>
#include "aic_stream.h"
#include "avi.h"
#include "mpp_mem.h"
#include "mpp_log.h"
#include "aic_parser.h"
#include "aic_tag.h"

static const char avi_headers[][8] = {
    { 'R', 'I', 'F', 'F', 'A', 'V', 'I', ' ' },
    { 'R', 'I', 'F', 'F', 'A', 'V', 'I', 'X' },
    { 'R', 'I', 'F', 'F', 'A', 'V', 'I', 0x19 },
    { 'O', 'N', '2', ' ', 'O', 'N', '2', 'f' },
    { 'R', 'I', 'F', 'F', 'A', 'M', 'V', ' ' },
    { 0 }
};

static int avi_load_index(struct aic_avi_parser *s);

int alloc_extradata(struct aic_codec_param *par, int size)
{
    mpp_free(par->extradata);
    par->extradata_size = 0;

    if (size < 0 || size >= INT32_MAX - AV_INPUT_BUFFER_PADDING_SIZE)
        return -1;

    par->extradata = mpp_alloc(size + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!par->extradata)
        return -1;

    memset(par->extradata + size, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    par->extradata_size = size;

    return 0;
}

int get_extradata(struct aic_codec_param *par, struct aic_stream *pb, int size)
{
    int ret = alloc_extradata(par, size);
    if (ret < 0)
        return ret;
    ret = aic_stream_read(pb, par->extradata, size);
    if (ret != size) {
        mpp_free(par->extradata);
        par->extradata_size = 0;
        logi("Failed to read extradata of size %d\n", size);
        return ret;
    }

    return ret;
}

int get_video_extradata(struct aic_stream *c, struct avi_stream_ctx *st)
{
    int ret = PARSER_OK;

    // FIXME: check if the encoder really did this correctly
    if (st->codecpar.extradata_size & 1)
        aic_stream_r8(c);

    /* Extract palette from extradata if bpp <= 8.
    * This code assumes that extradata contains only palette.
    * This is true for all paletted codecs implemented in
    * FFmpeg. */
    if (st->codecpar.extradata_size &&
        (st->codecpar.bits_per_coded_sample <= 8)) {
        int pal_size = (1 << st->codecpar.bits_per_coded_sample) << 2;
        const uint8_t *pal_src;

        pal_size = MPP_MIN(pal_size, st->codecpar.extradata_size);
        pal_src =
            st->codecpar.extradata + st->codecpar.extradata_size - pal_size;
        /* Exclude the "BottomUp" field from the palette */
        if (pal_src - st->codecpar.extradata >= 9 &&
            !memcmp(st->codecpar.extradata + st->codecpar.extradata_size - 9,
                    "BottomUp", 9))
            pal_src -= 9;
        // for (i = 0; i < pal_size / 4; i++)
        //     ast->pal[i] = 0xFFU<<24 | AV_RL32(pal_src + 4 * i);
        // ast->has_pal = 1;
    }

    logi("video width %d, height %d\n", st->codecpar.width,
         st->codecpar.height);

    if (st->codecpar.codec_tag == 0 && st->codecpar.height > 0 &&
        st->codecpar.extradata_size < 1U << 30) {
        int old_extradata_size =
            st->codecpar.extradata_size + AV_INPUT_BUFFER_PADDING_SIZE;

        st->codecpar.extradata_size += 9;
        void *extradata = mpp_alloc(st->codecpar.extradata_size +
                                    AV_INPUT_BUFFER_PADDING_SIZE);

        if (!extradata) {
            st->codecpar.extradata_size = 0;
            return PARSER_NOMEM;
        } else {
            memcpy(extradata, st->codecpar.extradata, old_extradata_size);
            mpp_free(st->codecpar.extradata);
            st->codecpar.extradata = extradata;
            memcpy(st->codecpar.extradata + st->codecpar.extradata_size - 9,
                   "BottomUp", 9);
        }
    }

    return ret;
}

/* "big_endian" values are needed for RIFX file format */
int get_wav_header(struct aic_stream *pb, struct aic_codec_param *par, int size,
                   int big_endian)
{
    int id;
    uint64_t bitrate = 0;

    if (size < 14) {
        loge("wav header size < 14");
        return PARSER_INVALIDDATA;
    }

    par->codec_type = MPP_MEDIA_TYPE_AUDIO;
    if (!big_endian) {
        id = aic_stream_rl16(pb);
        if (id != 0x0165) {
            par->channels = aic_stream_rl16(pb);
            par->sample_rate = aic_stream_rl32(pb);
            bitrate = aic_stream_rl32(pb) * 8LL;
            par->block_align = aic_stream_rl16(pb);
        }
    } else {
        id = aic_stream_rb16(pb);
        par->channels = aic_stream_rb16(pb);
        par->sample_rate = aic_stream_rb32(pb);
        bitrate = aic_stream_rb32(pb) * 8LL;
        par->block_align = aic_stream_rb16(pb);
    }
    if (size == 14) { /* We're dealing with plain vanilla WAVEFORMAT */
        par->bits_per_coded_sample = 8;
    } else {
        if (!big_endian) {
            par->bits_per_coded_sample = aic_stream_rl16(pb);
        } else {
            par->bits_per_coded_sample = aic_stream_rb16(pb);
        }
    }
    if (id == 0xFFFE) {
        par->codec_tag = 0;
    } else {
        par->codec_tag = id;
        par->codec_id = aic_codec_get_id(aic_codec_wav_tags, id);
    }
    if (size >= 18 &&
        id != 0x0165) { /* We're obviously dealing with WAVEFORMATEX */
        int cbSize = aic_stream_rl16(pb); /* cbSize */
        if (big_endian) {
            loge("WAVEFORMATEX support for RIFX files");
            return PARSER_ERROR;
        }
        size -= 18;
        cbSize = MPP_MIN(size, cbSize);
        if (cbSize >= 22 && id == 0xfffe) { /* WAVEFORMATEXTENSIBLE */
            cbSize -= 22;
            size -= 22;
        }

        /*audio decoder not support extradata, may be failed */
#if 0
        if (cbSize > 0) {
            if (get_extradata(par, pb, cbSize) < 0)
                return PARSER_INVALIDDATA;
            size -= cbSize;
        }
#endif
        /* It is possible for the chunk to contain garbage at the end */
        if (size > 0)
            aic_stream_skip(pb, size);
    } else if (id == 0x0165 && size >= 32) {
        int nb_streams, i;

        size -= 4;
        if (get_extradata(par, pb, size) < 0)
            return PARSER_INVALIDDATA;
        nb_streams = AIC_RL16(par->extradata + 4);
        par->sample_rate = AIC_RL32(par->extradata + 12);
        par->channels = 0;
        bitrate = 0;
        if (size < 8 + nb_streams * 20)
            return PARSER_INVALIDDATA;
        for (i = 0; i < nb_streams; i++)
            par->channels += par->extradata[8 + i * 20 + 17];
    }

    par->bit_rate = bitrate;

    if (par->sample_rate <= 0) {
        loge("Invalid sample rate: %d\n", par->sample_rate);
        return PARSER_INVALIDDATA;
    }

    return 0;
}

int get_bmp_header(struct aic_stream *pb, struct aic_codec_param *par,
                   unsigned int *size)
{
    int tag1;
    uint32_t size_ = aic_stream_rl32(pb);
    if (size)
        *size = size_;
    par->width = aic_stream_rl32(pb);
    par->height = (int32_t)aic_stream_rl32(pb);
    aic_stream_rl16(pb); /* planes */
    aic_stream_rl16(pb); /* depth */
    tag1 = aic_stream_rl32(pb);
    aic_stream_rl32(pb); /* ImageSize */
    aic_stream_rl32(pb); /* XPelsPerMeter */
    aic_stream_rl32(pb); /* YPelsPerMeter */
    aic_stream_rl32(pb); /* ClrUsed */
    aic_stream_rl32(pb); /* ClrImportant */
    return tag1;
}

static inline int get_duration(struct avi_stream *ast, int len)
{
    if (ast->sample_size)
        return len;
    else if (ast->dshow_block_align)
        return (len + (int64_t)ast->dshow_block_align - 1) /
               ast->dshow_block_align;
    else
        return 1;
}

static int64_t get_time(int64_t timestamp, uint32_t rate, uint32_t scale)
{
    if (!(scale && rate)) {
        loge("scale/rate is %" PRIu32 "/%" PRIu32 " which is invalid. "
             "(This file has been generated by broken software.)\n",
             scale, rate);
        rate = 25;
        scale = 1;
    }
    return (int64_t)(timestamp * MPP_TIME_BASE * scale) / rate;
}

static int find_index_by_pts(struct avi_stream_ctx *st, s64 pts)
{
    int i, index = 0;
    int64_t min = INT64_MAX;
    int64_t sample_pts = 0;
    int64_t diff = 0;
    struct avi_index_entry *cur_sample = NULL;
    struct avi_stream *ast = st->priv_data;

    for (i = 0; i < st->nb_index_entries; i++) {
        cur_sample = &st->index_entries[i];
        sample_pts = get_time(cur_sample->timestamp, ast->rate, ast->scale);
        diff = MPP_ABS(pts, sample_pts);
        if (diff < min) {
            min = diff;
            index = i;
        }
    }

    return index;
}

static int find_video_index_by_pts(struct avi_stream_ctx *video_st, s64 pts)
{
    int i, index = 0;
    int64_t min = INT64_MAX;
    int64_t sample_pts = 0;
    int64_t diff = 0;
    struct avi_index_entry *cur_sample = NULL;
    struct avi_stream *ast = video_st->priv_data;

    logd("nb_index_entries:%d\n", video_st->nb_index_entries);

    /* First step: find current frame by pts*/
    for (i = 0; i < video_st->nb_index_entries; i++) {
        cur_sample = &video_st->index_entries[i];
        sample_pts = get_time(cur_sample->timestamp, ast->rate, ast->scale);
        diff = MPP_ABS(pts, sample_pts);
        if (diff < min) {
            min = diff;
            index = i;
        }
    }

    /* Second step find the key frame before current frame*/
    while (index--) {
        cur_sample = &video_st->index_entries[index];
        if (cur_sample->flags == 1) {
            return index;
        }
    }

    return PARSER_ERROR;
}

static int get_riff(struct aic_avi_parser *s)
{
    struct aic_stream *c = s->stream;
    struct avi_context *avi = &s->avi_c;
    char header[8] = { 0 };
    int i;

    /* check RIFF header */
    aic_stream_read(c, header, 4);
    avi->riff_end = aic_stream_rl32(c);    /* RIFF chunk size */
    avi->riff_end += aic_stream_tell(c); /* RIFF chunk end */
    aic_stream_read(c, header + 4, 4);

    for (i = 0; avi_headers[i][0]; i++)
        if (!memcmp(header, avi_headers[i], 8))
            break;
    if (!avi_headers[i][0])
        return PARSER_ERROR;

    if (header[7] == 0x19)
        logi("This file has been generated by a totally broken muxer.\n");

    return PARSER_OK;
}

static struct avi_stream_ctx *avi_new_stream(struct aic_avi_parser *s)
{
    struct avi_stream_ctx *sc;

    sc = (struct avi_stream_ctx *)mpp_alloc(sizeof(struct avi_stream_ctx));
    if (sc == NULL) {
        return NULL;
    }
    memset(sc, 0, sizeof(struct avi_stream_ctx));

    sc->index = s->nb_streams;
    s->streams[s->nb_streams++] = sc;

    return sc;
}

int get_avi_header(struct aic_stream *c, struct avi_context *avi)
{
    int frame_period = aic_stream_rl32(c);
    aic_stream_rl32(c); /* max. bytes per second */
    aic_stream_rl32(c);
    avi->non_interleaved |= aic_stream_rl32(c) & AVIF_MUSTUSEINDEX;

    aic_stream_skip(c, 2 * 4);
    aic_stream_rl32(c);
    aic_stream_rl32(c);
    aic_stream_rl32(c); /* avih_width */
    aic_stream_rl32(c); /* avih_height */

    return frame_period;
}

int get_strh_info(struct aic_avi_parser *s, unsigned int size,
                  int frame_period, int *stream_index, int *type)
{
    struct avi_stream_ctx *st = NULL;
    struct avi_stream *ast = NULL;
    struct aic_stream *c = s->stream;
    int codec_type = MPP_MEDIA_TYPE_UNKNOWN;
    int tag1 = aic_stream_rl32(c);             /* stream_type, vids、auds */
    unsigned int handler = aic_stream_rl32(c); /* codec tag */

    if (tag1 == MKTAG('p', 'a', 'd', 's')) {
        aic_stream_skip(c, size - 8);
        return PARSER_OK;
    } else {
        (*stream_index)++;
        st = avi_new_stream(s);
        if (!st)
            return PARSER_ERROR;

        st->index = *stream_index;
        ast = mpp_alloc(sizeof(struct avi_stream));
        if (!ast)
            return PARSER_ERROR;
        st->priv_data = ast;
        memset(st->priv_data, 0, sizeof(struct avi_stream));
    }
    ast->handler = handler;

    aic_stream_rl32(c); /* flags */
    aic_stream_rl16(c); /* priority */
    aic_stream_rl16(c); /* language */
    aic_stream_rl32(c); /* initial frame */
    ast->scale = aic_stream_rl32(c);
    ast->rate = aic_stream_rl32(c);

    if (!(ast->scale && ast->rate)) {
        logw("scale/rate is %" PRIu32 "/%" PRIu32 " which is invalid. "
             "(This file has been generated by broken software.)\n",
             ast->scale, ast->rate);
        if (frame_period) {
            ast->rate = 1000000;
            ast->scale = frame_period;
        } else {
            ast->rate = 25;
            ast->scale = 1;
        }
    }

    ast->cum_len = aic_stream_rl32(c); /* start */
    st->nb_frames = aic_stream_rl32(c);

    aic_stream_rl32(c); /* buffer size */
    aic_stream_rl32(c); /* quality */
    if (ast->cum_len > 3600LL * ast->rate / ast->scale) {
        loge("crazy start time, iam scared, giving up\n");
        ast->cum_len = 0;
    }
    ast->sample_size = aic_stream_rl32(c);
    ast->cum_len *= MPP_MAX(1, ast->sample_size);

    switch (tag1) {
        case MKTAG('v', 'i', 'd', 's'):
            codec_type = MPP_MEDIA_TYPE_VIDEO;
            ast->sample_size = 0;
            break;
        case MKTAG('a', 'u', 'd', 's'):
            codec_type = MPP_MEDIA_TYPE_AUDIO;
            break;
        case MKTAG('t', 'x', 't', 's'):
        case MKTAG('d', 'a', 't', 's'):
        default:
            logi("unknown stream type %X\n", tag1);
    }
    *type = codec_type;
    st->duration = get_time(st->nb_frames, ast->rate, ast->scale);
    ast->frame_offset = ast->cum_len;

    aic_stream_skip(c, size - 12 * 4);
    return PARSER_OK;
}

int get_strf_info(struct aic_avi_parser *s, int stream_index, unsigned int size,
                  int codec_type, uint64_t list_end)
{
    int ret = PARSER_OK;
    struct aic_stream *c = s->stream;
    struct avi_context *avi = &s->avi_c;
    unsigned int tag1;
    unsigned int esize;
    struct avi_stream_ctx *st;
    struct avi_stream *ast = NULL;

    st = s->streams[stream_index];
    st->cur_sample_idx = 0;
    ast = st->priv_data;

    logi("strf: size=%d, codec_type=%d, stream_idx=%d, nb_streams=%d", size,
         codec_type, stream_index, s->nb_streams);
    if (!size && (codec_type == MPP_MEDIA_TYPE_AUDIO ||
                  codec_type == MPP_MEDIA_TYPE_VIDEO))
        return PARSER_OK;

    if (stream_index >= (unsigned)s->nb_streams) {
        aic_stream_skip(c, size);
        return PARSER_OK;
    } else {
        uint64_t cur_pos = aic_stream_tell(c);
        if (cur_pos < list_end)
            size = MPP_MIN(size, list_end - cur_pos);
    }

    switch (codec_type) {
        case MPP_MEDIA_TYPE_VIDEO:
            tag1 = get_bmp_header(c, &st->codecpar, &esize);
            st->codecpar.codec_type = MPP_MEDIA_TYPE_VIDEO;
            st->codecpar.codec_tag = tag1;
            st->codecpar.codec_id = aic_codec_get_id(aic_codec_bmp_tags, tag1);

            if (size > 10 * 4 && size < (1 << 30) && size < avi->fsize) {
                if (esize == size - 1 && (esize & 1)) {
                    st->codecpar.extradata_size = esize - 10 * 4;
                } else {
                    st->codecpar.extradata_size = size - 10 * 4;
                }

                if (st->codecpar.extradata) {
                    logw(
                        "New extradata in strf chunk, freeing previous one.\n");
                }

                ret = get_extradata(&st->codecpar, c,
                                    st->codecpar.extradata_size);
                if (ret < 0)
                    return ret;
            }

            ret = get_video_extradata(c, st);
            if (ret < 0)
                return ret;
            // aic_stream_skip(c, size - 5 * 4);
            break;
        case MPP_MEDIA_TYPE_AUDIO:
            ret = get_wav_header(c, &st->codecpar, size, 0);
            if (ret < 0)
                return ret;

            /* 2-aligned
            * (fix for Stargate SG-1 - 3x18 - Shades of Grey.avi) */
            if (size & 1)
                aic_stream_skip(c, 1);

            if (st->codecpar.codec_id == CODEC_ID_AAC &&
                ast->dshow_block_align <= 4 && ast->dshow_block_align) {
                logd("overriding invalid dshow_block_align of %d\n",
                     ast->dshow_block_align);
                ast->dshow_block_align = 0;
            }
            if ((st->codecpar.codec_id == CODEC_ID_AAC &&
                 ast->dshow_block_align == 1024 && ast->sample_size == 1024) ||
                (st->codecpar.codec_id == CODEC_ID_AAC &&
                 ast->dshow_block_align == 4096 && ast->sample_size == 4096) ||
                (st->codecpar.codec_id == CODEC_ID_MP3 &&
                 ast->dshow_block_align == 1152 && ast->sample_size == 1152)) {
                logd("overriding sample_size\n");
                ast->sample_size = 0;
            }
            break;

        default:
            st->codecpar.codec_type = MPP_MEDIA_TYPE_UNKNOWN;
            st->codecpar.codec_id = CODEC_ID_NONE;
            st->codecpar.codec_tag = 0;
            aic_stream_skip(c, size);
            break;
    }

    return PARSER_OK;
}

int get_strd_info(struct aic_avi_parser *s, int stream_index, uint64_t list_end,
                  unsigned int *size)
{
    int ret = PARSER_OK;
    struct avi_stream_ctx *st = NULL;
    struct aic_stream *c = s->stream;

    if (stream_index >= (unsigned)s->nb_streams ||
        s->streams[stream_index]->codecpar.extradata_size ||
        s->streams[stream_index]->codecpar.codec_tag ==
            MKTAG('H', '2', '6', '4')) {
        aic_stream_skip(c, *size);
    } else {
        uint64_t cur_pos = aic_stream_tell(c);
        if (cur_pos < list_end)
            *size = MPP_MIN(*size, list_end - cur_pos);
        st = s->streams[stream_index];

        if ((*size) < (1 << 30)) {
            if (st->codecpar.extradata) {
                logw("New extradata in strd chunk, free previous one.\n");
            }
            if ((ret = get_extradata(&st->codecpar, c, *size)) < 0)
                return ret;
        }

        //FIXME check if the encoder really did this correctly
        if (st->codecpar.extradata_size & 1)
            aic_stream_r8(c);
    }

    return PARSER_OK;
}

int avi_read_header(struct aic_avi_parser *s)
{
    struct aic_stream *c = s->stream;
    struct avi_context *avi = &s->avi_c;
    unsigned int tag, tag1, size;
    int ret, codec_type, stream_index, frame_period;
    uint64_t list_end = 0;
    int64_t pos;

    avi->stream_index = -1;

    ret = get_riff(s);
    if (ret < 0)
        return ret;

    avi->io_fsize = avi->fsize = aic_stream_size(c);
    if (avi->fsize <= 0 || avi->fsize < avi->riff_end)
        avi->fsize = avi->riff_end == 8 ? INT64_MAX : avi->riff_end;

    /* first list tag */
    stream_index = -1;
    codec_type = -1;
    frame_period = 0;
    for (;;) {
        tag = aic_stream_rl32(c);
        size = aic_stream_rl32(c);
        logi("tag 0x%x, size %d\n", tag, size);

        switch (tag) {
            case MKTAG('L', 'I', 'S', 'T'):
                list_end = aic_stream_tell(c) + size;
                tag1 = aic_stream_rl32(c);
                if (tag1 == MKTAG('m', 'o', 'v', 'i')) {
                    avi->movi_list = aic_stream_tell(c) - 4;
                    if (size)
                        avi->movi_end = avi->movi_list + size + (size & 1);
                    else
                        avi->movi_end = avi->fsize;
                    goto end_of_header;
                }
                break;
            case MKTAG('I', 'D', 'I', 'T'): {
                unsigned char date[64] = { 0 };
                size += (size & 1);
                size -=
                    aic_stream_read(c, date, MPP_MIN(size, sizeof(date) - 1));
                aic_stream_skip(c, size);
                break;
            }
            case MKTAG('d', 'm', 'l', 'h'):
                aic_stream_skip(c, size + (size & 1));
                break;
            case MKTAG('a', 'm', 'v', 'h'):
            case MKTAG('a', 'v', 'i', 'h'): /* AVI header */
                frame_period = get_avi_header(c, avi);
                aic_stream_skip(c, size - 10 * 4);
                break;
            case MKTAG('s', 't', 'r', 'h'): /* stream header */
                ret = get_strh_info(s, size, frame_period, &stream_index,
                                    &codec_type);
                if (ret != PARSER_OK) {
                    goto fail;
                }
                break;
            case MKTAG('s', 't', 'r', 'f'): /* stream header */
                ret =
                    get_strf_info(s, stream_index, size, codec_type, list_end);
                if (ret != PARSER_OK)
                    return ret;
                break;
            case MKTAG('s', 't', 'r', 'd'):
                get_strd_info(s, stream_index, list_end, &size);
                break;
            case MKTAG('i', 'n', 'd', 'x'):
                pos = aic_stream_tell(c);
                aic_stream_seek(c, pos + size, SEEK_SET);
                break;
            case MKTAG('v', 'p', 'r', 'p'):
                aic_stream_skip(c, size);
                break;
            case MKTAG('s', 't', 'r', 'n'):
                break;
            default:
                if (size > 1000000) {
                    loge("Something went wrong during header parsing, "
                         "tag 0x%x has size %u, "
                         "I will ignore it and try to continue anyway.\n",
                         tag, size);

                    avi->movi_list = aic_stream_tell(c) - 4;
                    avi->movi_end = avi->fsize;
                    goto end_of_header;
                }

            /* Do not fail for very large idx1 tags */
            case MKTAG('i', 'd', 'x', '1'): /* skip tag */
                size += (size & 1);
                aic_stream_skip(c, size);
                break;
        }
    }

end_of_header:
    /* check stream number */
    if (stream_index != s->nb_streams - 1) {
        logi("stream_index:%d != nb_streams - 1:%d", stream_index,
             s->nb_streams - 1);
    fail:
        return PARSER_ERROR;
    }

    if (!avi->index_loaded)
        avi_load_index(s);

    avi->index_loaded |= 1;

    return PARSER_OK;
}

static struct avi_index_entry *avi_find_next_sample(struct aic_avi_parser *c,
                                                    struct avi_stream_ctx **st)
{
    int i;
    struct avi_index_entry *sample = NULL;
    int64_t best_dts = INT64_MAX;

    /* find the sample with the smallest dts from every streams */
    for (i = 0; i < c->nb_streams; i++) {
        struct avi_stream_ctx *cur_st = c->streams[i];
        struct avi_stream *ast = cur_st->priv_data;

        if ((cur_st->codecpar.codec_type != MPP_MEDIA_TYPE_VIDEO) &&
            (cur_st->codecpar.codec_type != MPP_MEDIA_TYPE_AUDIO)) {
            continue;
        }
        if (cur_st->cur_sample_idx < cur_st->nb_index_entries) {
            struct avi_index_entry *cur_sample =
                &cur_st->index_entries[cur_st->cur_sample_idx];
            int64_t dts =
                get_time(cur_sample->timestamp, ast->rate, ast->scale);

            if (!sample || dts < best_dts) {
                sample = cur_sample;
                best_dts = dts;
                *st = cur_st;
            }
        }
    }

    if (*st) {
        (*st)->cur_sample_idx++;
    }

    return sample;
}

int avi_read_packet(struct aic_avi_parser *s, struct aic_parser_packet *pkt)
{
    struct aic_stream *c = s->stream;

    aic_stream_seek(c, s->cur_sample->pos, SEEK_SET);
    aic_stream_read(c, pkt->data, s->cur_sample->size);

    return PARSER_OK;
}

/* XXX: We make the implicit supposition that the positions are sorted
 * for each stream. */
static int avi_read_idx1(struct aic_avi_parser *s, int size)
{
    struct avi_context *avi = &s->avi_c;
    struct aic_stream *c = s->stream;
    int nb_index_entries, i;
    struct avi_stream_ctx *st;
    struct avi_stream *ast;
    int64_t pos;
    unsigned int index, tag, flags, len, first_packet = 1;
    int64_t last_pos = -1;
    unsigned last_idx = -1;
    int64_t idx1_pos, first_packet_pos = 0, data_offset = 0;
    int anykey = 0;

    nb_index_entries = size / 16;
    if (nb_index_entries <= 0)
        return PARSER_ERROR;

    idx1_pos = aic_stream_tell(c);
    aic_stream_seek(c, avi->movi_list + 4, SEEK_SET);

    avi->stream_index = -1;
    aic_stream_seek(c, idx1_pos, SEEK_SET);

    logi("size %d, nb_index_entries %d\n", size, nb_index_entries);

    /* Read the entries and sort them in each stream component. */
    for (i = 0; i < nb_index_entries; i++) {
        tag = aic_stream_rl32(c);   //chunk id
        flags = aic_stream_rl32(c); //keyframe
        pos = aic_stream_rl32(c);   //data offset in file
        len = aic_stream_rl32(c);   //data size

        index = ((tag & 0xff) - '0') * 10;
        index += (tag >> 8 & 0xff) - '0';
        if (index >= s->nb_streams)
            continue;

        st = s->streams[index];
        ast = st->priv_data;

        /* Skip 'xxpc' palette change entries in the index until a logic
         * to process these is properly implemented. */
        if ((tag >> 16 & 0xff) == 'p' && (tag >> 24 & 0xff) == 'c')
            continue;

        if (first_packet && first_packet_pos) {
            if (avi->movi_list + 4 != pos || pos + 500 > first_packet_pos)
                data_offset = first_packet_pos - pos;
            first_packet = 0;
        }
        pos += data_offset;
        logd(
            "stream(%d) len=%d cum_len=%d, sample_size=%d, dshow_block_align=%d\n",
            index, len, (int)ast->cum_len, ast->sample_size,
            ast->dshow_block_align);
        /* even if we have only a single stream, we should
         * switch to non-interleaved to get correct timestamps */
        if (last_pos == pos)
            avi->non_interleaved = 1;

        if (!st->alloc_entries_flag) {
            struct avi_index_entry *index_entries =
                mpp_alloc(st->nb_frames * sizeof(struct avi_index_entry));
            if (!index_entries) {
                loge("no mem for stream %d index_entries \n", index);
                return PARSER_NOMEM;
            }
            st->index_entries = index_entries;
            st->alloc_entries_flag = 1;
            st->nb_index_entries = 0;
            ast->cum_len = 0;
        }
        if (last_idx != pos && len && st->alloc_entries_flag) {
            st->index_entries[st->nb_index_entries].pos =
                avi->movi_list + pos + 8;
            st->index_entries[st->nb_index_entries].timestamp = ast->cum_len;
            st->index_entries[st->nb_index_entries].min_distance = 0;
            st->index_entries[st->nb_index_entries].size = len;
            st->index_entries[st->nb_index_entries].flags =
                (flags & AVIIF_INDEX) ? 1 : 0;
            last_idx = pos;
            st->nb_index_entries++;
        }

        ast->cum_len += get_duration(ast, len);
        last_pos = pos;
        anykey |= flags & AVIIF_INDEX;
    }

    if (!anykey) {
        for (index = 0; index < s->nb_streams; index++) {
            st = s->streams[index];
            if (st->nb_index_entries)
                st->index_entries[0].flags |= 1;
        }
    }
    return PARSER_OK;
}

static int avi_load_index(struct aic_avi_parser *s)
{
    struct avi_context *avi = &s->avi_c;
    struct aic_stream *c = s->stream;
    uint32_t tag, size;
    int64_t pos = aic_stream_tell(c);
    int64_t next;
    int ret = PARSER_ERROR;

    if (aic_stream_seek(c, avi->movi_end, SEEK_SET) < 0)
        goto the_end; // maybe truncated file
    logd("movi_end=0x%" PRIx64 "\n", avi->movi_end);
    for (;;) {
        tag = aic_stream_rl32(c);
        size = aic_stream_rl32(c);

        next = aic_stream_tell(c) + size + (size & 1);

        if (tag == MKTAG('i', 'd', 'x', '1') && avi_read_idx1(s, size) >= 0) {
            avi->index_loaded = 2;
            ret = 0;
        } else if (tag == MKTAG('L', 'I', 'S', 'T')) {
            aic_stream_rl32(c);
        } else if (!ret) {
            break;
        }

        if (aic_stream_seek(c, next, SEEK_SET) < 0)
            break; // something is wrong here
    }

the_end:
    aic_stream_seek(c, pos, SEEK_SET);
    return ret;
}

int avi_read_close(struct aic_avi_parser *s)
{
    int i;

    for (i = 0; i < s->nb_streams; i++) {
        struct avi_stream_ctx *st = s->streams[i];
        if (!st) {
            continue;
        }
        if (st->codecpar.extradata) {
            mpp_free(st->codecpar.extradata);
        }
        struct avi_stream *ast = st->priv_data;
        if (ast) {
            mpp_free(ast);
        }

        if (st->index_entries) {
            mpp_free(st->index_entries);
        }

        mpp_free(st);
    }

    return PARSER_OK;
}

int avi_peek_packet(struct aic_avi_parser *s, struct aic_parser_packet *pkt)
{
    struct avi_stream_ctx *st = NULL;
    struct avi_index_entry *sample = NULL;
    static struct avi_stream *ast = NULL;

    sample = avi_find_next_sample(s, &st);
    if (!sample) {
        // eos
        return PARSER_EOS;
    }

    s->cur_sample = sample;
    pkt->size = sample->size;
    pkt->type = st->codecpar.codec_type;
    pkt->flag = 0;

    ast = st->priv_data;

    if (ast) {
        pkt->pts = get_time(sample->timestamp, ast->rate, ast->scale);
    }

    if (st->cur_sample_idx == st->nb_index_entries) {
        // eos now
        logw("[%s:%d] this stream eos\n", __FUNCTION__, __LINE__);
        pkt->flag = PACKET_EOS;
    }

    // logd("peek packet(%d), size %d, type %d, pos=0x%"PRIx64", timestamp=%d, pts=%d\n",
    //     st->cur_sample_idx, pkt->size, pkt->type, sample->pos, (int)sample->timestamp, (int)pkt->pts);
    return PARSER_OK;
}

int avi_seek_packet(struct aic_avi_parser *s, s64 pts)
{
    int i;

    struct avi_stream_ctx *cur_st = NULL;
    struct avi_stream_ctx *video_st = NULL;
    struct avi_stream_ctx *audio_st = NULL;
    struct avi_index_entry *cur_sample = NULL;
    static struct avi_stream *video_ast = NULL;

    for (i = 0; i < s->nb_streams; i++) {
        cur_st = s->streams[i];
        if ((!video_st) &&
            (cur_st->codecpar.codec_type ==
             MPP_MEDIA_TYPE_VIDEO)) { // only support first video stream
            video_st = s->streams[i];
            video_ast = video_st->priv_data;
        } else if ((!audio_st) &&
                   (cur_st->codecpar.codec_type ==
                    MPP_MEDIA_TYPE_AUDIO)) { // only support first audio stream
            audio_st = s->streams[i];
        }
    }

    if (video_st) {
        video_st->cur_sample_idx = find_video_index_by_pts(video_st, pts);
        if (video_st->cur_sample_idx < 0) {
            loge("no keyframes\n");
            return PARSER_ERROR;
        }
    }

    if (audio_st) {
        int64_t tmp;
        tmp = pts;
        if (video_st) {
            cur_sample = &video_st->index_entries[video_st->cur_sample_idx];
            tmp = get_time(cur_sample->timestamp, video_ast->rate,
                           video_ast->scale);
        }
        audio_st->cur_sample_idx = find_index_by_pts(audio_st, tmp);
    }

    return PARSER_OK;
}
