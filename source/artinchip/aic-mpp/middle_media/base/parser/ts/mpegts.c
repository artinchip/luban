/*
 * Copyright (C) 2020-2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: <che.jiang@artinchip.com>
 * Desc: mpegts file demuxer
 */

#define LOG_TAG "mpegts"

#include "mpegts.h"
#include "aic_parser.h"
#include "aic_stream.h"
#include "aic_tag.h"
#include "avi.h"
#include "mpegts_audio.h"
#include "mpp_list.h"
#include "mpp_log.h"
#include "mpp_mem.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#define TS_FEC_PACKET_SIZE 204
#define TS_DVHS_PACKET_SIZE 192
#define TS_PACKET_SIZE 188
#define TS_MAX_PACKET_SIZE 204
#define TS_TIMESTAMP_BASE_HZ 90000

#define NB_PID_MAX 8192
#define USUAL_SECTION_SIZE 1024 /* except EIT which is limited to 4096 */
#define MAX_SECTION_SIZE 4096

/* pids */
#define PAT_PID 0x0000  /* Program Association Table */
#define CAT_PID 0x0001  /* Conditional Access Table */
#define TSDT_PID 0x0002 /* Transport Stream Description Table */
#define IPMP_PID 0x0003
/* PID from 0x0004 to 0x000F are reserved */
#define NIT_PID 0x0010 /* Network Information Table */
#define SDT_PID 0x0011 /* Service Description Table */
#define BAT_PID 0x0011 /* Bouquet Association Table */
#define EIT_PID 0x0012 /* Event Information Table */
#define RST_PID 0x0013 /* Running Status Table */
#define TDT_PID 0x0014 /* Time and Date Table */
#define TOT_PID 0x0014
#define NET_SYNC_PID 0x0015
#define RNT_PID 0x0016 /* RAR Notification Table */
/* PID from 0x0017 to 0x001B are reserved for future use */
/* PID value 0x001C allocated to link-local inband signalling shall not be
 * used on any broadcast signals. It shall only be used between devices in a
 * controlled environment. */
#define LINK_LOCAL_PID 0x001C
#define MEASUREMENT_PID 0x001D
#define DIT_PID 0x001E /* Discontinuity Information Table */
#define SIT_PID 0x001F /* Selection Information Table */
/* PID from 0x0020 to 0x1FFA may be assigned as needed to PMT, elementary
 * streams and other data tables */
#define FIRST_OTHER_PID 0x0020
#define LAST_OTHER_PID 0x1FFA
/* PID 0x1FFB is used by DigiCipher 2/ATSC MGT metadata */
/* PID from 0x1FFC to 0x1FFE may be assigned as needed to PMT, elementary
 * streams and other data tables */
#define NULL_PID 0x1FFF /* Null packet (used for fixed bandwidth padding) */

/* m2ts pids */
#define M2TS_PMT_PID 0x0100
#define M2TS_PCR_PID 0x1001
#define M2TS_VIDEO_PID 0x1011
#define M2TS_AUDIO_START_PID 0x1100
#define M2TS_PGSSUB_START_PID 0x1200
#define M2TS_TEXTSUB_PID 0x1800
#define M2TS_SECONDARY_AUDIO_START_PID 0x1A00
#define M2TS_SECONDARY_VIDEO_START_PID 0x1B00

/* table ids */
#define PAT_TID 0x00  /* Program Association section */
#define CAT_TID 0x01  /* Conditional Access section */
#define PMT_TID 0x02  /* Program Map section */
#define TSDT_TID 0x03 /* Transport Stream Description section */
/* TID from 0x04 to 0x3F are reserved */
#define M4OD_TID 0x05
#define NIT_TID 0x40  /* Network Information section - actual network */
#define ONIT_TID 0x41 /* Network Information section - other network */
#define SDT_TID 0x42  /* Service Description section - actual TS */
/* TID from 0x43 to 0x45 are reserved for future use */
#define OSDT_TID 0x46 /* Service Descrition section - other TS */
/* TID from 0x47 to 0x49 are reserved for future use */
#define BAT_TID 0x4A /* Bouquet Association section */
#define UNT_TID 0x4B /* Update Notification Table section */
#define DFI_TID 0x4C /* Downloadable Font Info section */
/* TID 0x4D is reserved for future use */
#define EIT_TID 0x4E         /* Event Information section - actual TS */
#define OEIT_TID 0x4F        /* Event Information section - other TS */
#define EITS_START_TID 0x50  /* Event Information section schedule - actual TS */
#define EITS_END_TID 0x5F    /* Event Information section schedule - actual TS */
#define OEITS_START_TID 0x60 /* Event Information section schedule - other TS */
#define OEITS_END_TID 0x6F   /* Event Information section schedule - other TS */
#define TDT_TID 0x70         /* Time Date section */
#define RST_TID 0x71         /* Running Status section */
#define ST_TID 0x72          /* Stuffing section */
#define TOT_TID 0x73         /* Time Offset section */
#define AIT_TID 0x74         /* Application Inforamtion section */
#define CT_TID 0x75          /* Container section */
#define RCT_TID 0x76         /* Related Content section */
#define CIT_TID 0x77         /* Content Identifier section */
#define MPE_FEC_TID 0x78     /* MPE-FEC section */
#define RPNT_TID 0x79        /* Resolution Provider Notification section */
#define MPE_IFEC_TID 0x7A    /* MPE-IFEC section */
#define PROTMT_TID 0x7B      /* Protection Message section */
/* TID from 0x7C to 0x7D are reserved for future use */
#define DIT_TID 0x7E /* Discontinuity Information section */
#define SIT_TID 0x7F /* Selection Information section */
/* TID from 0x80 to 0xFE are user defined */
/* TID 0xFF is reserved */

#define STREAM_TYPE_VIDEO_MPEG1 0x01
#define STREAM_TYPE_VIDEO_MPEG2 0x02
#define STREAM_TYPE_AUDIO_MPEG1 0x03
#define STREAM_TYPE_AUDIO_MPEG2 0x04
#define STREAM_TYPE_PRIVATE_SECTION 0x05
#define STREAM_TYPE_PRIVATE_DATA 0x06
#define STREAM_TYPE_AUDIO_AAC 0x0f
#define STREAM_TYPE_AUDIO_AAC_LATM 0x11
#define STREAM_TYPE_VIDEO_MPEG4 0x10
#define STREAM_TYPE_METADATA 0x15
#define STREAM_TYPE_VIDEO_H264 0x1b
#define STREAM_TYPE_VIDEO_HEVC 0x24
#define STREAM_TYPE_VIDEO_CAVS 0x42
#define STREAM_TYPE_VIDEO_VC1 0xea
#define STREAM_TYPE_VIDEO_DIRAC 0xd1

#define STREAM_TYPE_AUDIO_AC3 0x81
#define STREAM_TYPE_AUDIO_DTS 0x82
#define STREAM_TYPE_AUDIO_TRUEHD 0x83
#define STREAM_TYPE_AUDIO_EAC3 0x87

/* maximum size in which we look for synchronization if
 * synchronization is lost */
#define MAX_RESYNC_SIZE 65536
#define MAX_PES_PAYLOAD 200 * 1024
#define MAX_MP4_DESCR_COUNT 16

/* enough for PES header + length */
#define PES_START_SIZE 6
#define PES_HEADER_SIZE 9
#define MAX_PES_HEADER_SIZE (9 + 255)
#define PES_H264_NAL_AUD_SIZE 6

#define MAX_PIDS_PER_PROGRAM 64

#define PROBE_PACKET_MAX_BUF 8192
#define PROBE_PACKET_MARGIN 5
#define MPEGTS_INPUT_BUFFER_PADDING_SIZE 64

enum MPEGTS_FILTER_TYPE {
    MPEGTS_PES,
    MPEGTS_SECTION,
    MPEGTS_PCR,
};

enum MPEGTS_TS_STATE {
    MPEGTS_HEADER = 0,
    MPEGTS_PESHEADER,
    MPEGTS_PESHEADER_FILL,
    MPEGTS_PAYLOAD,
    MPEGTS_SKIP,
};

typedef struct mpegts_ts_filter mpegts_ts_filter;
typedef struct mpegts_pes_context mpegts_pes_context;

typedef int mpegts_pes_callback(mpegts_ts_filter *f, const uint8_t *buf, int len,
                                int is_start, int64_t pos);

typedef void mpegts_section_callback(mpegts_ts_filter *f, const uint8_t *buf, int len);

typedef struct mpegts_pes_filter {
    mpegts_pes_callback *pes_cb;
    void *opaque;
} mpegts_pes_filter;

typedef struct mpegts_section_filter {
    int section_index;
    int section_h_size;
    int last_ver;
    unsigned crc;
    unsigned last_crc;
    uint8_t *section_buf;
    unsigned int check_crc : 1;
    unsigned int end_of_section_reached : 1;
    mpegts_section_callback *section_cb;
    void *opaque;
} mpegts_section_filter;

struct mpegts_ts_filter {
    int pid;
    int es_id;
    int last_cc; /* last cc code (-1 if first packet) */
    int64_t last_pcr;
    int discard;
    enum MPEGTS_FILTER_TYPE type;
    union {
        mpegts_pes_filter pes_filter;
        mpegts_section_filter section_filter;
    } u;
};

typedef struct mpegts_section_header {
    uint8_t tid;
    uint16_t id;
    uint8_t version;
    uint8_t sec_num;
    uint8_t last_sec_num;
} mpegts_section_header;

typedef struct mpegts_context {
    /* user data */
    struct aic_mpegts_parser *parser;
    /** raw packet size, including FEC if present */
    int raw_packet_size;

    int64_t pos47_full;

    /** if true, all pids are analyzed to find streams */
    int auto_guess;

    /* data needed to handle file based ts */
    /** stop parsing loop */
    int stop_parse;
    /** packet temp Audio/Video data */
    struct aic_parser_packet pkt;
    enum CodecID cur_codec_id;
    uint8_t first_pkt[TS_PACKET_SIZE];
    int first_pkt_size;
    mpegts_pes_context *last_pes;
    uint32_t read_offset;
    uint32_t no_need_peek;
    uint32_t find_first_audio;
    uint32_t has_audio;
    /** to detect seek */
    int64_t last_pos;

    int skip_changes;
    int skip_pmt;

    int resync_size;
    int merge_pmt_versions;
    int id;

    /** filters for various streams specified by PMT + for the PAT and PMT */
    mpegts_ts_filter *pids[NB_PID_MAX];
    int current_pid;
} mpegts_context;

struct mpegts_pes_context {
    int pid;
    int pcr_pid; /**< if -1 then all packets containing PCR are considered */
    int stream_type;
    struct mpegts_context *ts;
    struct aic_mpegts_parser *parser;
    struct mpegts_stream_ctx *st;
    enum MPEGTS_TS_STATE state;
    /* used to get the format */
    int data_index;
    int flags; /**< copied to the Packet flags */
    int total_size;
    int pes_header_size;
    int extended_stream_id;
    uint8_t stream_id;
    int64_t pts, dts;
    int64_t ts_packet_pos; /**< position of first TS packet of this PES packet */
    uint8_t header[MAX_PES_HEADER_SIZE];
    void *buffer;
    int merged_st;
};

typedef struct mpegts_stream_type {
    uint32_t stream_type;
    enum aic_stream_type codec_type;
    enum CodecID codec_id;
} mpegts_stream_type;

static const mpegts_stream_type iso_types[] = {
    {STREAM_TYPE_AUDIO_MPEG1, MPP_MEDIA_TYPE_AUDIO, CODEC_ID_MP3},
    {STREAM_TYPE_AUDIO_MPEG2, MPP_MEDIA_TYPE_AUDIO, CODEC_ID_MP3},
    {STREAM_TYPE_AUDIO_AAC, MPP_MEDIA_TYPE_AUDIO, CODEC_ID_AAC},
    {STREAM_TYPE_VIDEO_MPEG4, MPP_MEDIA_TYPE_VIDEO, CODEC_ID_MPEG4},
    {STREAM_TYPE_VIDEO_H264, MPP_MEDIA_TYPE_VIDEO, CODEC_ID_H264},
    {STREAM_TYPE_PRIVATE_DATA, MPP_MEDIA_TYPE_VIDEO, CODEC_ID_MJPEG},
    {0x1c, MPP_MEDIA_TYPE_AUDIO, CODEC_ID_AAC},
    {0x20, MPP_MEDIA_TYPE_VIDEO, CODEC_ID_H264},
    {0x21, MPP_MEDIA_TYPE_VIDEO, CODEC_ID_MJPEG},
    {0},
};

static int64_t mpegts_parse_pes_pts(const uint8_t *buf)
{
    return (int64_t)(*buf & 0x0e) << 29 |
           (AIC_RB16(buf + 1) >> 1) << 15 |
           AIC_RB16(buf + 3) >> 1;
}

static int mpegts_mid_pred(int a, int b, int c)
{
    if (a > b) {
        if (c > b) {
            if (c > a)
                b = a;
            else
                b = c;
        }
    } else {
        if (b > c) {
            if (c > a)
                b = c;
            else
                b = a;
        }
    }
    return b;
}

/**
 *  Assemble PES packets out of TS packets, and then call the "section_cb"
 *  function when they are complete.
 */
static void write_section_data(mpegts_context *ts, mpegts_ts_filter *tss1,
                               const uint8_t *buf, int buf_size, int is_start)
{
    mpegts_section_filter *tss = &tss1->u.section_filter;
    uint8_t *cur_section_buf = NULL;
    int len, offset;

    if (is_start) {
        memcpy(tss->section_buf, buf, buf_size);
        tss->section_index = buf_size;
        tss->section_h_size = -1;
        tss->end_of_section_reached = 0;
    } else {
        if (tss->end_of_section_reached)
            return;
        len = MAX_SECTION_SIZE - tss->section_index;
        if (buf_size < len)
            len = buf_size;
        memcpy(tss->section_buf + tss->section_index, buf, len);
        tss->section_index += len;
    }

    offset = 0;
    cur_section_buf = tss->section_buf;
    while (cur_section_buf - tss->section_buf < MAX_SECTION_SIZE && cur_section_buf[0] != 0xff) {
        /* compute section length if possible */
        if (tss->section_h_size == -1 && tss->section_index - offset >= 3) {
            len = (AIC_RB16(cur_section_buf + 1) & 0xfff) + 3;
            if (len > MAX_SECTION_SIZE)
                return;
            tss->section_h_size = len;
        }

        if (tss->section_h_size != -1 &&
            tss->section_index >= offset + tss->section_h_size) {
            tss->end_of_section_reached = 1;

            tss->section_cb(tss1, cur_section_buf, tss->section_h_size);

            cur_section_buf += tss->section_h_size;
            offset += tss->section_h_size;
            tss->section_h_size = -1;
        } else {
            tss->section_h_size = -1;
            tss->end_of_section_reached = 0;
            break;
        }
    }
}

static mpegts_ts_filter *mpegts_open_filter(struct mpegts_context *ts, unsigned int pid,
                                            enum MPEGTS_FILTER_TYPE type)
{
    mpegts_ts_filter *filter;

    if (pid >= NB_PID_MAX || ts->pids[pid])
        return NULL;
    filter = mpp_alloc(sizeof(mpegts_ts_filter));
    if (!filter)
        return NULL;
    memset(filter, 0, sizeof(mpegts_ts_filter));
    ts->pids[pid] = filter;

    filter->type = type;
    filter->pid = pid;
    filter->es_id = -1;
    filter->last_cc = -1;
    filter->last_pcr = -1;

    return filter;
}

static mpegts_ts_filter *mpegts_open_section_filter(struct mpegts_context *ts,
                                                    unsigned int pid,
                                                    mpegts_section_callback *section_cb,
                                                    void *opaque,
                                                    int check_crc)
{
    mpegts_ts_filter *filter;
    mpegts_section_filter *sec;

    if (!(filter = mpegts_open_filter(ts, pid, MPEGTS_SECTION)))
        return NULL;
    sec = &filter->u.section_filter;
    sec->section_cb = section_cb;
    sec->opaque = opaque;
    sec->section_buf = mpp_alloc(MAX_SECTION_SIZE);
    sec->check_crc = check_crc;
    sec->last_ver = -1;

    if (!sec->section_buf) {
        mpp_free(filter);
        return NULL;
    } else {
        memset(sec->section_buf, 0, MAX_SECTION_SIZE);
    }

    return filter;
}

static mpegts_ts_filter *mpegts_open_pes_filter(struct mpegts_context *ts, unsigned int pid,
                                                mpegts_pes_callback *pes_cb,
                                                void *opaque)
{
    mpegts_ts_filter *filter;
    mpegts_pes_filter *pes;

    if (!(filter = mpegts_open_filter(ts, pid, MPEGTS_PES)))
        return NULL;

    pes = &filter->u.pes_filter;
    pes->pes_cb = pes_cb;
    pes->opaque = opaque;
    return filter;
}

static void mpegts_close_filter(struct mpegts_context *ts, mpegts_ts_filter *filter)
{
    if (filter == NULL)
        return;

    int pid = filter->pid;
    if (filter->type == MPEGTS_SECTION) {
        mpp_free(filter->u.section_filter.section_buf);
        filter->u.section_filter.section_buf = NULL;
    } else if (filter->type == MPEGTS_PES) {
        mpp_free(filter->u.pes_filter.opaque);
    }
    mpp_free(filter);
    ts->pids[pid] = NULL;
}

static int mpegts_analyze(const uint8_t *buf, int size, int packet_size,
                          int probe)
{
    int stat[TS_MAX_PACKET_SIZE];
    int stat_all = 0;
    int i;
    int best_score = 0;

    memset(stat, 0, packet_size * sizeof(*stat));

    for (i = 0; i < size - 3; i++) {
        if (buf[i] == 0x47) { // sync_byte
            int pid = AIC_RB16(buf + 1) & 0x1FFF;
            int asc = buf[i + 3] & 0x30;
            if (!probe || pid == 0x1FFF || asc) {
                int x = i % packet_size;
                stat[x]++;
                stat_all++;
                if (stat[x] > best_score) {
                    best_score = stat[x];
                }
            }
        }
    }

    return best_score - MPP_MAX(stat_all - 10 * best_score, 0) / 10;
}

/* autodetect fec presence */
static int get_packet_size(struct aic_mpegts_parser *s)
{
    int score, fec_score, dvhs_score;
    int margin;
    int ret;

    /*init buffer to store stream for probing */
    uint8_t *buf = mpp_alloc(PROBE_PACKET_MAX_BUF);
    if (buf == NULL) {
        loge("malloc probe packet buf failed");
        return PARSER_NOMEM;
    }
    int buf_size = 0;

    while (buf_size < PROBE_PACKET_MAX_BUF) {
        ret = aic_stream_read(s->stream, buf + buf_size, PROBE_PACKET_MAX_BUF - buf_size);
        if (ret < 0)
            goto FAILED;
        buf_size += ret;

        score = mpegts_analyze(buf, buf_size, TS_PACKET_SIZE, 0);
        dvhs_score = mpegts_analyze(buf, buf_size, TS_DVHS_PACKET_SIZE, 0);
        fec_score = mpegts_analyze(buf, buf_size, TS_FEC_PACKET_SIZE, 0);
        logi("Probe: %d, score: %d, dvhs_score: %d, fec_score: %d \n",
             buf_size, score, dvhs_score, fec_score);

        margin = mpegts_mid_pred(score, fec_score, dvhs_score);

        if (buf_size < PROBE_PACKET_MAX_BUF)
            margin += PROBE_PACKET_MARGIN; /*if buffer not filled */

        if (score > margin) {
            mpp_free(buf);
            return TS_PACKET_SIZE;
        } else if (dvhs_score > margin) {
            mpp_free(buf);
            return TS_DVHS_PACKET_SIZE;
        } else if (fec_score > margin) {
            mpp_free(buf);
            return TS_FEC_PACKET_SIZE;
        }
    }

FAILED:
    mpp_free(buf);
    return PARSER_INVALIDDATA;
}

static inline int get8(const uint8_t **pp, const uint8_t *p_end)
{
    const uint8_t *p;
    int c;

    p = *pp;
    if (p >= p_end)
        return PARSER_ERROR;
    c = *p++;
    *pp = p;
    return c;
}

static inline int get16(const uint8_t **pp, const uint8_t *p_end)
{
    const uint8_t *p;
    int c;

    p = *pp;
    if (1 >= p_end - p)
        return PARSER_ERROR;
    c = AIC_RB16(p);
    p += 2;
    *pp = p;
    return c;
}

static int parse_section_header(mpegts_section_header *h,
                                const uint8_t **pp, const uint8_t *p_end)
{
    int val;

    val = get8(pp, p_end);
    if (val < 0)
        return val;
    h->tid = val;
    *pp += 2;
    val = get16(pp, p_end);
    if (val < 0)
        return val;
    h->id = val;
    val = get8(pp, p_end);
    if (val < 0)
        return val;
    h->version = (val >> 1) & 0x1f;
    val = get8(pp, p_end);
    if (val < 0)
        return val;
    h->sec_num = val;
    val = get8(pp, p_end);
    if (val < 0)
        return val;
    h->last_sec_num = val;
    return 0;
}

static int mpegts_get_pkt_codec_type(uint32_t stream_type,
                                     const mpegts_stream_type *types)
{
    for (; types->stream_type; types++)
        if (stream_type == types->stream_type) {
            return types->codec_type;
        }

    return MPP_MEDIA_TYPE_UNKNOWN;
}

static int mpegts_get_pkt_codec_id(uint32_t stream_type,
                                   const mpegts_stream_type *types)
{
    for (; types->stream_type; types++)
        if (stream_type == types->stream_type) {
            return types->codec_id;
        }

    return MPP_MEDIA_TYPE_UNKNOWN;
}

static void mpegts_find_stream_type(struct mpegts_stream_ctx *st,
                                    uint32_t stream_type,
                                    const mpegts_stream_type *types)
{
    for (; types->stream_type; types++)
        if (stream_type == types->stream_type) {
            if (st->codecpar.codec_type != types->codec_type ||
                st->codecpar.codec_id != types->codec_id) {
                st->codecpar.codec_type = types->codec_type;
                st->codecpar.codec_id = types->codec_id;
            }
            return;
        }
}

static struct mpegts_stream_ctx *mpegts_new_stream(struct aic_mpegts_parser *s)
{
    struct mpegts_stream_ctx *sc;

    sc = (struct mpegts_stream_ctx *)mpp_alloc(sizeof(struct mpegts_stream_ctx));
    if (sc == NULL) {
        return NULL;
    }
    memset(sc, 0, sizeof(struct mpegts_stream_ctx));

    sc->index = s->nb_streams;
    s->streams[s->nb_streams++] = sc;

    return sc;
}

static int mpegts_set_stream_info(struct mpegts_stream_ctx *st, mpegts_pes_context *pes,
                                  uint32_t stream_type, uint32_t prog_reg_desc)
{
    int old_codec_type = st->codecpar.codec_type;
    int old_codec_id = st->codecpar.codec_id;
    struct mpegts_context *ts = pes->ts;
    // avpriv_set_pts_info(st, 33, 1, 90000);

    st->priv_data = pes;
    st->codecpar.codec_type = MPP_MEDIA_TYPE_OTHER;
    st->codecpar.codec_id = CODEC_ID_NONE;
    pes->st = st;
    pes->stream_type = stream_type;

    logd("stream=%d stream_type=%x pid=%x prog_reg_desc=%.4s\n",
         st->index, pes->stream_type, pes->pid, (char *)&prog_reg_desc);

    st->codecpar.codec_tag = pes->stream_type;

    mpegts_find_stream_type(st, pes->stream_type, iso_types);

    if (st->codecpar.codec_id == CODEC_ID_NONE) {
        st->codecpar.codec_id = old_codec_id;
        st->codecpar.codec_type = old_codec_type;
    }
    if (st->codecpar.codec_type == MPP_MEDIA_TYPE_AUDIO) {
        ts->has_audio = 1;
    }
    return 0;
}

static int mpegts_set_audio_info(struct mpegts_stream_ctx *st, mpegts_pes_context *pes)
{
    int ret = PARSER_OK;
    struct mpegts_context *ts = pes->ts;

    struct mpegts_audio_decode_header audio_header = {0};

    if (ts->cur_codec_id == CODEC_ID_MP3) {
        ret = mpegaudio_decode_mp3_header(ts->pkt.data + ts->read_offset, &audio_header);
        if (ret < 0) {
            loge("mpegaudio_decode_mp3_header failed, ret:%d", ret);
            return ret;
        }

    } else if (ts->cur_codec_id == CODEC_ID_AAC) {
        ret = mpegaudio_decode_aac_header(ts->pkt.data + ts->read_offset, &audio_header);
        if (ret < 0) {
            loge("mpegaudio_decode_aac_header failed, ret:%d", ret);
            return ret;
        }
    }
    st->codecpar.bits_per_coded_sample = 16;
    st->codecpar.channels = audio_header.nb_channels;
    st->codecpar.sample_rate = audio_header.sample_rate;

    logi("get audio params: bits_per_coded_sample=%d channels=%d sample_rate=%d\n",
         st->codecpar.bits_per_coded_sample, st->codecpar.channels, st->codecpar.sample_rate);

    return 0;
}

static void reset_pes_packet_state(mpegts_pes_context *pes)
{
    pes->pts = 0;
    pes->dts = 0;
    pes->data_index = 0;
    pes->flags = 0;
}

static int new_pes_packet(mpegts_pes_context *pes, struct aic_parser_packet *pkt)
{
    pkt->type = mpegts_get_pkt_codec_type(pes->stream_type, iso_types);
    pkt->size = pes->data_index;
    pkt->pts = pes->pts;
    pkt->dts = 0;
    pkt->flag = pes->flags;
    pkt->duration = 0;
    reset_pes_packet_state(pes);
    return 0;
}

int mpegts_skip_h264_nalu_aud(struct mpegts_stream_ctx *st, const uint8_t *buf)
{
    if (st->codecpar.codec_type != MPP_MEDIA_TYPE_VIDEO ||
        st->codecpar.codec_id != CODEC_ID_H264) {
        return 0;
    }

    int i = 0, j = 0;
    int offset = 0;
    int find_aud_off = 0;
    const uint8_t *tmp_buf = buf;

    for (i = 0; i < 16; i++) {
        if (tmp_buf[i] == 0 && tmp_buf[i + 1] == 0 &&
            tmp_buf[i + 2] == 1 && tmp_buf[i + 3] == 9) {
            find_aud_off = i + 3;
            break;
        }
    }
    if (find_aud_off) {
        for (j = find_aud_off; j < find_aud_off + 16; j++) {
            if (tmp_buf[j] == 0 && tmp_buf[j + 1] == 0 &&
                tmp_buf[j + 2] == 1) {
                if (tmp_buf[j - 1] == 0) {
                    offset = j - 1;
                } else {
                    offset = j;
                }
                break;
            }
        }
        if (offset == 0)
            offset = PES_H264_NAL_AUD_SIZE;
    }
    return offset;
}

static int mpegts_start_pes(mpegts_pes_context *pes)
{
    int ret = -1;
    struct mpegts_context *ts = pes->ts;
    mpegts_pes_context *last_pes = ts->last_pes;

    if (last_pes) {
        if (last_pes->state == MPEGTS_PAYLOAD && last_pes->data_index > 0) {
            memcpy(ts->pkt.data, ts->first_pkt, ts->first_pkt_size);
            ts->cur_codec_id = mpegts_get_pkt_codec_id(last_pes->stream_type, iso_types);
            ret = new_pes_packet(last_pes, &ts->pkt);
            if (ret < 0)
                return ret;
            ts->stop_parse = 1;

            /*find the first audio then clear find flag*/
            if (ts->find_first_audio && ts->pkt.type == MPP_MEDIA_TYPE_AUDIO) {
                mpegts_set_audio_info(last_pes->st, last_pes);
                ts->find_first_audio = 0;
            }
        } else {
            loge("last pes is null should be not happend!!!");
        }
    }
    reset_pes_packet_state(pes);
    ts->read_offset = 0;
    ts->no_need_peek = 0;
    ts->last_pes = NULL;
    pes->state = MPEGTS_HEADER;

    return 0;
}

static int mpegts_pes_header(mpegts_pes_context *pes)
{
    int code = 0;
    struct mpegts_context *ts = pes->ts;

    if (pes->data_index == PES_START_SIZE) {
        /* we got all the PES or section header. We can now
            * decide */
        if (pes->header[0] == 0x00 && pes->header[1] == 0x00 &&
            pes->header[2] == 0x01) {
            /* it must be an MPEG-2 PES stream */
            code = pes->header[3] | 0x100;
            logd("pid=%x pes_code=%#x\n", pes->pid, code);
            pes->stream_id = pes->header[3];

            /* stream not present in PMT */
            if (!pes->st) {
                if (ts->skip_changes)
                    goto skip;
                if (ts->merge_pmt_versions)
                    goto skip; /* wait for PMT to merge new stream */

                pes->st = mpegts_new_stream(ts->parser);
                if (!pes->st)
                    return PARSER_NOMEM;
                pes->st->index = pes->pid;
                mpegts_set_stream_info(pes->st, pes, 0, 0);
            }
            pes->buffer = ts->pkt.data;
            memset(ts->first_pkt, 0, TS_PACKET_SIZE);
            ts->first_pkt_size = 0;
            ts->last_pes = pes;
            pes->total_size = MAX_PES_PAYLOAD;

            if (code != 0x1bc && code != 0x1bf && /* program_stream_map, private_stream_2 */
                code != 0x1f0 && code != 0x1f1 && /* ECM, EMM */
                code != 0x1ff && code != 0x1f2 && /* program_stream_directory, DSMCC_stream */
                code != 0x1f8) {                  /* ITU-T Rec. H.222.1 type E stream */
                pes->state = MPEGTS_PESHEADER;
            } else {
                pes->pes_header_size = 6;
                pes->state = MPEGTS_PAYLOAD;
                pes->data_index = 0;
            }
        } else {
            /* otherwise, it should be a table */
            /* skip packet */
        skip:
            pes->state = MPEGTS_SKIP;
            return 1;
        }
    }

    return 0;
}


static void mpegts_pes_header_fill(mpegts_pes_context *pes)
{
    if (pes->data_index == pes->pes_header_size) {
        const uint8_t *r;
        unsigned int flags, pes_ext, skip;

        flags = pes->header[7];
        r = pes->header + 9;
        pes->pts = 0;
        pes->dts = 0;
        if ((flags & 0xc0) == 0x80) {
            pes->dts = pes->pts = mpegts_parse_pes_pts(r) * 1000000 / TS_TIMESTAMP_BASE_HZ;
            r += 5;
        } else if ((flags & 0xc0) == 0xc0) {
            pes->pts = mpegts_parse_pes_pts(r) * 1000000 / TS_TIMESTAMP_BASE_HZ;
            r += 5;
            pes->dts = mpegts_parse_pes_pts(r) * 1000000 / TS_TIMESTAMP_BASE_HZ;
            r += 5;
        }

        pes->extended_stream_id = -1;
        if (flags & 0x01) { /* PES extension */
            pes_ext = *r++;
            /* Skip PES private data, program packet sequence counter and P-STD buffer */
            skip = (pes_ext >> 4) & 0xb;
            skip += skip & 0x9;
            r += skip;
            if ((pes_ext & 0x41) == 0x01 &&
                (r + 2) <= (pes->header + pes->pes_header_size)) {
                /* PES extension 2 */
                if ((r[0] & 0x7f) > 0 && (r[1] & 0x80) == 0)
                    pes->extended_stream_id = r[1];
            }
        }

        /* we got the full header. We parse it and get the payload */
        pes->state = MPEGTS_PAYLOAD;
        pes->data_index = 0;
    }
}

/* return non zero if a packet could be constructed */
static int mpegts_push_data(mpegts_ts_filter *filter,
                            const uint8_t *buf, int buf_size, int is_start,
                            int64_t pos)
{
    mpegts_pes_context *pes = filter->u.pes_filter.opaque;
    struct mpegts_context *ts = pes->ts;
    const uint8_t *p;
    int ret = -1, len;

    if (is_start) {
        ret = mpegts_start_pes(pes);
        if (ret < 0)
            return ret;
        pes->ts_packet_pos = pos;
    }
    p = buf;
    while (buf_size > 0) {
        switch (pes->state) {
        case MPEGTS_HEADER:
            len = PES_START_SIZE - pes->data_index;
            if (len > buf_size)
                len = buf_size;
            memcpy(pes->header + pes->data_index, p, len);
            pes->data_index += len;
            p += len;
            buf_size -= len;
            if (mpegts_pes_header(pes)) {
                continue;
            }
            break;
        /**********************************************/
        /* PES packing parsing */
        case MPEGTS_PESHEADER:
            len = PES_HEADER_SIZE - pes->data_index;
            if (len < 0)
                return PARSER_INVALIDDATA;
            if (len > buf_size)
                len = buf_size;
            memcpy(pes->header + pes->data_index, p, len);
            pes->data_index += len;
            p += len;
            buf_size -= len;
            if (pes->data_index == PES_HEADER_SIZE) {
                pes->pes_header_size = pes->header[8] + 9;
                pes->state = MPEGTS_PESHEADER_FILL;
            }
            break;
        case MPEGTS_PESHEADER_FILL:
            len = pes->pes_header_size - pes->data_index;
            if (len < 0)
                return PARSER_INVALIDDATA;
            if (len > buf_size)
                len = buf_size;
            memcpy(pes->header + pes->data_index, p, len);
            pes->data_index += len;
            p += len;
            buf_size -= len;
            mpegts_pes_header_fill(pes);
            break;
        case MPEGTS_PAYLOAD:
            if (pes->buffer) {
                if (pes->data_index > 0 &&
                    pes->data_index + buf_size > pes->total_size) {
                    loge("frame size %d overange total size %d, then drop it",
                         pes->data_index + buf_size, pes->total_size);
                    ts->stop_parse = 1;
                } else if (pes->data_index == 0 &&
                           buf_size > pes->total_size) {
                    // pes packet size is < ts size packet and pes data is padded with 0xff
                    // not sure if this is legal in ts but see issue #2392
                    buf_size = pes->total_size;
                }
                if (is_start) {
                    if (buf_size < TS_PACKET_SIZE) {
                        int offset = mpegts_skip_h264_nalu_aud(pes->st, p);
                        logd("skip offset %d, buf_size %d", offset, buf_size);
                        buf_size -= offset;
                        memcpy(ts->first_pkt, p + offset, buf_size);
                        ts->first_pkt_size = buf_size;
                    } else {
                        loge("first payload(%d) shoule be small than(%d)",
                             buf_size, TS_PACKET_SIZE);
                        return -1;
                    }
                } else {
                    memcpy(pes->buffer + pes->data_index, p, buf_size);
                }
                pes->data_index += buf_size;
                /* emit complete packets with known packet size
                 * decreases demuxer delay for infrequent packets like subtitles from
                 * a couple of seconds to milliseconds for properly muxed files.
                 * total_size is the number of bytes following pes_packet_length
                 * in the pes header, i.e. not counting the first PES_START_SIZE bytes */
                if (!ts->stop_parse && pes->total_size < MAX_PES_PAYLOAD &&
                    pes->pes_header_size + pes->data_index == pes->total_size + PES_START_SIZE) {
                    ts->stop_parse = 1;
                }
            }
            buf_size = 0;
            break;
        case MPEGTS_SKIP:
            buf_size = 0;
            break;
        }
    }

    return 0;
}

static mpegts_pes_context *add_pes_stream(struct mpegts_context *ts, int pid, int pcr_pid)
{
    mpegts_ts_filter *tss;
    mpegts_pes_context *pes;

    /* if no pid found, then add a pid context */
    pes = mpp_alloc(sizeof(mpegts_pes_context));
    if (!pes)
        return 0;
    memset(pes, 0, sizeof(mpegts_pes_context));
    pes->ts = ts;
    pes->parser = ts->parser;
    pes->pid = pid;
    pes->state = MPEGTS_SKIP;
    pes->pts = 0;
    pes->dts = 0;
    tss = mpegts_open_pes_filter(ts, pid, mpegts_push_data, pes);
    if (!tss) {
        mpp_free(pes);
        return NULL;
    }

    return pes;
}

static struct mpegts_stream_ctx *find_matching_stream(mpegts_context *ts, int pid,
                                                      unsigned int programid,
                                                      int stream_identifier,
                                                      int pmt_stream_idx)
{
    struct aic_mpegts_parser *s = ts->parser;
    int i;
    struct mpegts_stream_ctx *found = NULL;

    for (i = 0; i < s->nb_streams; i++) {
        struct mpegts_stream_ctx *st = s->streams[i];
        if (st->program_num != programid)
            continue;
        /* match based on "stream identifier descriptor" if present */
        if (stream_identifier != -1) {
            if (st->stream_identifier == stream_identifier + 1) {
                found = st;
                break;
            }
        } else if (st->pmt_stream_idx == pmt_stream_idx) {
            found = st; /* match based on position within the PMT */
            break;
        }
    }

    return found;
}

static int is_pes_stream(int stream_type, uint32_t prog_reg_desc)
{
    return !(stream_type == 0x13 ||
             (stream_type == 0x86 && prog_reg_desc == AIC_RL32("CUEI")));
}

static void sdt_cb(mpegts_ts_filter *filter, const uint8_t *section, int section_len)
{
    return;
}

static mpegts_pes_context* create_pmt_stream(mpegts_context *ts, mpegts_section_header *h,
                             int stream_type, int stream_idx, int pid, int pcr_pid)
{
    struct mpegts_stream_ctx *st = NULL;
    mpegts_pes_context *pes = NULL;
    uint32_t prog_reg_desc = 0; /* registration descriptor */
    int stream_identifier = -1;

    if (ts->pids[pid] && ts->pids[pid]->type == MPEGTS_PES) {
        pes = ts->pids[pid]->u.pes_filter.opaque;
        if (ts->merge_pmt_versions && !pes->st) {
            st = find_matching_stream(ts, pid, h->id, stream_identifier, stream_idx);
            if (st) {
                pes->st = st;
                pes->stream_type = stream_type;
                pes->merged_st = 1;
            }
        }
        if (!pes->st) {
            pes->st = mpegts_new_stream(pes->parser);
            if (!pes->st)
                return NULL;
            pes->st->index = pes->pid;
            pes->st->program_num = h->id;
            pes->st->pmt_stream_idx = stream_idx;
        }
        st = pes->st;
    } else if (is_pes_stream(stream_type, prog_reg_desc)) {
        if (ts->pids[pid])
            mpegts_close_filter(ts, ts->pids[pid]); // wrongly added sdt filter probably
        pes = add_pes_stream(ts, pid, pcr_pid);
        if (ts->merge_pmt_versions && pes && !pes->st) {
            st = find_matching_stream(ts, pid, h->id, stream_identifier, stream_idx);
            if (st) {
                pes->st = st;
                pes->stream_type = stream_type;
                pes->merged_st = 1;
            }
        }
        if (pes && !pes->st) {
            st = mpegts_new_stream(pes->parser);
            if (!st)
                return NULL;
            st->index = pes->pid;
            st->program_num = h->id;
            st->pmt_stream_idx = stream_idx;
        }
    }

    if (!st)
        return NULL;

    if (pes && !pes->stream_type) {
        mpegts_set_stream_info(st, pes, stream_type, prog_reg_desc);
    }

    return pes;
}

static void pmt_cb(mpegts_ts_filter *filter, const uint8_t *section, int section_len)
{
    mpegts_context *ts = filter->u.section_filter.opaque;
    mpegts_section_header h1, *h = &h1;
    const uint8_t *p, *p_end, *desc_list_end;
    int program_info_length, pcr_pid, pid, stream_type;
    int desc_list_len;
    int i;

    p_end = section + section_len - 4;
    p = section;
    if (parse_section_header(h, &p, p_end) < 0)
        return;
    if (h->tid != PMT_TID) /* table_id = 0x2*/
        return;

    // stop parsing after pmt, we found header
    if (ts->skip_pmt)
        return;

    pcr_pid = get16(&p, p_end); /*drop pcr id*/

    logi("PMT: len %i\n", section_len);
    logi("sid=0x%x sec_num=%d/%d version=%d tid=%d\n",
         h->id, h->sec_num, h->last_sec_num, h->version, h->tid);
    logi("pcr_pid=0x%x\n", pcr_pid);

    program_info_length = get16(&p, p_end);
    program_info_length &= 0xfff;
    p += program_info_length;
    if (p >= p_end)
        return;

    ts->skip_pmt = 1;

    for (i = 0;; i++) {
        stream_type = get8(&p, p_end); /* get video and audio type*/
        if (stream_type < 0)
            break;
        pid = get16(&p, p_end);
        if (pid < 0)
            return;
        pid &= 0x1fff;
        if (pid == ts->current_pid)
            return;

        if (create_pmt_stream(ts, h, stream_type, i, pid, pcr_pid) == NULL) {
            return;
        }

        desc_list_len = get16(&p, p_end);
        if (desc_list_len < 0)
            return;
        desc_list_len &= 0xfff;
        desc_list_end = p + desc_list_len;
        if (desc_list_end > p_end)
            return;
    }
}

static void pat_cb(mpegts_ts_filter *filter, const uint8_t *section, int section_len)
{
    struct mpegts_context *ts = filter->u.section_filter.opaque;
    mpegts_section_header h1, *h = &h1;
    const uint8_t *p, *p_end;
    int sid, pmt_pid;

    p_end = section + section_len - 4;
    p = section;
    if (parse_section_header(h, &p, p_end) < 0)
        return;
    if (h->tid != PAT_TID)
        return;
    if (ts->skip_changes)
        return;

    ts->skip_changes = 1;
    logi("PAT:\n");
    for (;;) {
        sid = get16(&p, p_end);
        if (sid < 0)
            break;
        pmt_pid = get16(&p, p_end);
        if (pmt_pid < 0)
            break;
        pmt_pid &= 0x1fff;

        if (pmt_pid == ts->current_pid)
            break;

        logd("sid=0x%x pid=0x%x\n", sid, pmt_pid);

        if (sid == 0x0000) {
            /* NIT info */
        } else {
            mpegts_ts_filter *fil = ts->pids[pmt_pid];
            if (fil)
                if (fil->type != MPEGTS_SECTION || fil->pid != pmt_pid ||
                    fil->u.section_filter.section_cb != pmt_cb)
                    mpegts_close_filter(ts, ts->pids[pmt_pid]);

            /* Program Map Table: parse stream type */
            if (!ts->pids[pmt_pid])
                mpegts_open_section_filter(ts, pmt_pid, pmt_cb, ts, 1);
            break;
        }
    }
}

/* return the 90kHz PCR and the extension for the 27MHz PCR. return
 * (-1) if not available */
static int parse_pcr(int64_t *ppcr_high, int *ppcr_low, const uint8_t *packet)
{
    int afc, len, flags;
    const uint8_t *p;
    unsigned int v;

    afc = (packet[3] >> 4) & 3;
    if (afc <= 1)
        return PARSER_INVALIDDATA;
    p = packet + 4;
    len = p[0];
    p++;
    if (len == 0)
        return PARSER_INVALIDDATA;
    flags = *p++;
    len--;
    if (!(flags & 0x10))
        return PARSER_INVALIDDATA;
    if (len < 6)
        return PARSER_INVALIDDATA;
    v = AIC_RB32(p);
    *ppcr_high = ((int64_t)v << 1) | (p[4] >> 7);
    *ppcr_low = ((p[4] & 1) << 8) | p[5];
    return 0;
}

/* handle one TS packet */
static int handle_packet(struct mpegts_context *ts, const uint8_t *packet, int64_t pos)
{
    mpegts_ts_filter *tss;
    int len, pid, cc, expected_cc, cc_ok, afc, is_start, is_discontinuity,
        has_adaptation, has_payload;
    const uint8_t *p, *p_end;

    pid = AIC_RB16(packet + 1) & 0x1fff;
    is_start = packet[1] & 0x40; /* payload unit start indicator */
    tss = ts->pids[pid];
    if (ts->auto_guess && !tss && is_start) {
        add_pes_stream(ts, pid, -1);
        tss = ts->pids[pid];
    }

    if (!tss)
        return 0;

    ts->current_pid = pid;

    afc = (packet[3] >> 4) & 3;
    if (afc == 0) /* reserved value */
        return 0;
    has_adaptation = afc & 2;
    has_payload = afc & 1;
    is_discontinuity = has_adaptation &&
                       packet[4] != 0 &&   /* with length > 0 */
                       (packet[5] & 0x80); /* and discontinuity indicated */

    /* continuity check (currently not used) */
    cc = (packet[3] & 0xf);
    expected_cc = has_payload ? (tss->last_cc + 1) & 0x0f : tss->last_cc;
    cc_ok = pid == 0x1FFF || // null packet PID
            is_discontinuity ||
            tss->last_cc < 0 ||
            expected_cc == cc;

    tss->last_cc = cc;
    if (!cc_ok) {
        logd("Continuity check failed for pid %d expected %d got %d\n",
             pid, expected_cc, cc);
        if (tss->type == MPEGTS_PES) {
            mpegts_pes_context *pc = tss->u.pes_filter.opaque;
            pc->flags |= 0x2;
        }
    }

    p = packet + 4;
    if (has_adaptation) {
        int64_t pcr_h;
        int pcr_l;
        if (parse_pcr(&pcr_h, &pcr_l, packet) == 0)
            tss->last_pcr = pcr_h * 300 + pcr_l;
        /* skip adaptation field */
        p += p[0] + 1;
    }
    /* if past the end of packet, ignore */
    p_end = packet + TS_PACKET_SIZE;
    if (p >= p_end || !has_payload)
        return 0;

    if (pos >= 0) {
        mpp_assert(pos >= TS_PACKET_SIZE);
        ts->pos47_full = pos - TS_PACKET_SIZE;
    }

    if (tss->type == MPEGTS_SECTION) {
        if (is_start) {
            /* pointer field present */
            len = *p++;
            if (len > p_end - p)
                return 0;
            if (len && cc_ok) {
                /* write remaining section bytes */
                write_section_data(ts, tss,
                                   p, len, 0);
                /* check whether filter has been closed */
                if (!ts->pids[pid])
                    return 0;
            }
            p += len;
            if (p < p_end) {
                write_section_data(ts, tss,
                                   p, p_end - p, 1);
            }
        } else {
            if (cc_ok) {
                write_section_data(ts, tss,
                                   p, p_end - p, 0);
            }
        }
    } else {
        int ret;
        // Note: The position here points actually behind the current packet.
        if (tss->type == MPEGTS_PES) {
            if ((ret = tss->u.pes_filter.pes_cb(tss, p, p_end - p, is_start,
                                                pos - ts->raw_packet_size)) < 0)
                return ret;
        }
    }

    return 0;
}

static int mpegts_resync(struct aic_mpegts_parser *s, int seekback, const uint8_t *current_packet)
{
    struct mpegts_context *ts = s->priv_data;
    struct aic_stream *pb = s->stream;
    int c, i;
    uint64_t pos = aic_stream_tell(pb);
    int64_t back = MPP_MIN(seekback, pos);

    // Special case for files like 01c56b0dc1.ts
    if (current_packet[0] == 0x80 && current_packet[12] == 0x47) {
        aic_stream_seek(pb, 12 - back, SEEK_CUR);
        return 0;
    }

    aic_stream_seek(pb, -back, SEEK_CUR);

    for (i = 0; i < ts->resync_size; i++) {
        c = aic_stream_r8(pb);

        if (c == 0x47) { // TS Packet Header
            int new_packet_size;
            aic_stream_seek(pb, -1, SEEK_CUR);
            pos = aic_stream_tell(pb);
            new_packet_size = get_packet_size(s);
            if (new_packet_size > 0 && new_packet_size != ts->raw_packet_size) {
                logw("changing packet size to %d\n", new_packet_size);
                ts->raw_packet_size = new_packet_size;
            }
            aic_stream_seek(pb, pos, SEEK_SET);
            return 0;
        }
    }
    loge("max resync size reached, could not find sync byte\n");
    /* no sync found */
    return PARSER_INVALIDDATA;
}

static int read_packet(struct aic_mpegts_parser *s, uint8_t *buf, int raw_packet_size)
{
    struct aic_stream *pb = s->stream;
    int len;

    for (;;) {
        len = aic_stream_read(pb, buf, TS_PACKET_SIZE);
        if (len != TS_PACKET_SIZE)
            return len < 0 ? len : PARSER_EOS;
        /* check packet sync byte */
        if (buf[0] != 0x47) {
            /* find a new packet start */
            if (mpegts_resync(s, raw_packet_size, buf) < 0)
                return PARSER_ERROR;
            else
                continue;
        } else {
            break;
        }
    }
    return 0;
}

static void finished_reading_packet(struct aic_mpegts_parser *s, int raw_packet_size)
{
    struct aic_stream *pb = s->stream;
    int skip = raw_packet_size - TS_PACKET_SIZE;
    if (skip > 0)
        aic_stream_skip(pb, skip);
}

static int handle_packets(struct mpegts_context *ts, int64_t nb_packets)
{
    struct aic_mpegts_parser *s = ts->parser;
    struct aic_stream *pb = s->stream;
    uint8_t packet[TS_PACKET_SIZE + MPEGTS_INPUT_BUFFER_PADDING_SIZE];
    const uint8_t *data;
    int64_t packet_num;
    int ret = 0;

    if (aic_stream_tell(pb) != ts->last_pos) {
        int i;
        logi("Skipping after seek\n");
        /* seek detected, flush pes buffer */
        for (i = 0; i < NB_PID_MAX; i++) {
            if (ts->pids[i]) {
                if (ts->pids[i]->type == MPEGTS_PES) {
                    mpegts_pes_context *pes = ts->pids[i]->u.pes_filter.opaque;
                    pes->data_index = 0;
                    pes->state = MPEGTS_SKIP; /* skip until pes header */
                } else if (ts->pids[i]->type == MPEGTS_SECTION) {
                    ts->pids[i]->u.section_filter.last_ver = -1;
                }
                ts->pids[i]->last_cc = -1;
                ts->pids[i]->last_pcr = -1;
            }
        }
    }

    ts->stop_parse = 0;
    packet_num = 0;
    memset(packet + TS_PACKET_SIZE, 0, MPEGTS_INPUT_BUFFER_PADDING_SIZE);
    for (;;) {
        packet_num++;
        if ((nb_packets != 0 && packet_num > nb_packets) ||
            ts->stop_parse > 1) {
            ret = PARSER_NODATA;
            break;
        }
        if (!ts->find_first_audio) {
            if (ts->stop_parse > 0)
                break;
        }

        ret = read_packet(s, packet, ts->raw_packet_size);
        if (ret != 0)
            break;
        data = packet;
        ret = handle_packet(ts, data, aic_stream_tell(pb));
        finished_reading_packet(s, ts->raw_packet_size);
        if (ret != 0)
            break;
    }
    ts->last_pos = aic_stream_tell(pb);
    return ret;
}

int mpegts_read_header(struct aic_mpegts_parser *s)
{
    struct mpegts_context *ts = NULL;

    s->priv_data = mpp_alloc(sizeof(struct mpegts_context));
    if (!s->priv_data) {
        return PARSER_NOMEM;
    }
    memset(s->priv_data, 0, sizeof(struct mpegts_context));
    ts = s->priv_data;
    ts->parser = s;
    ts->auto_guess = 1;
    ts->raw_packet_size = TS_PACKET_SIZE;
    ts->find_first_audio = 0;

    ts->pkt.size = 0;
    ts->pkt.data = mpp_alloc(MAX_PES_PAYLOAD);
    if (!ts->pkt.data) {
        loge("malloc temp buffer size %d failed", MAX_PES_PAYLOAD);
        mpp_free(s->priv_data);
        s->priv_data = NULL;
        return PARSER_NOMEM;
    }

    /* Service Description Table: useless data, discard */
    mpegts_open_section_filter(ts, SDT_PID, sdt_cb, ts, 1);

    /* Program Association Table */
    mpegts_open_section_filter(ts, PAT_PID, pat_cb, ts, 1);

    /* Read SDT、PAT and PMT get video and audio */
    handle_packets(ts, 3);

    if (ts->has_audio) {
        /* Save current pos, and then search the first audio pkt to get audio params*/
        uint64_t pos = aic_stream_tell(s->stream);
        ts->find_first_audio = 1;
        handle_packets(ts, 0);
        ts->find_first_audio = 0;
        ts->last_pes = NULL;
        aic_stream_seek(s->stream, pos, SEEK_SET);
    }

    return PARSER_OK;
}

static int handle_audio_packets(struct mpegts_context *ts, struct aic_parser_packet *pkt)
{
    int ret = PARSER_OK;
    int new_read_offset = 0;
    struct mpegts_audio_decode_header audio_header = {0};

    if (ts->cur_codec_id == CODEC_ID_MP3) {
        ret = mpegaudio_decode_mp3_header(ts->pkt.data + ts->read_offset, &audio_header);
        if (ret < 0)
            return ret;
        new_read_offset = ts->read_offset + audio_header.frame_size;
        if (new_read_offset <= ts->pkt.size) {
            ts->no_need_peek = (new_read_offset == ts->pkt.size) ? 0 : 1;
            pkt->size = audio_header.frame_size;
            pkt->pts += audio_header.frame_duration;
        } else {
            if (ts->read_offset < ts->pkt.size)
                loge("Audio packet may wrong, read_offset:%d, pkt.size:%d, new frame_size:%d",
                     ts->read_offset, ts->pkt.size, audio_header.frame_size);
            ts->no_need_peek = 0;
        }
    } else if (ts->cur_codec_id == CODEC_ID_AAC) {
        ret = mpegaudio_decode_aac_header(ts->pkt.data + ts->read_offset, &audio_header);
        if (ret < 0)
            return ret;
        new_read_offset = ts->read_offset + audio_header.frame_size;
        if (new_read_offset <= ts->pkt.size) {
            ts->no_need_peek = (new_read_offset == ts->pkt.size) ? 0 : 1;
            ts->read_offset += audio_header.aac.header_offset;
            pkt->size = audio_header.frame_size - audio_header.aac.header_offset;
            pkt->pts += audio_header.frame_duration;
        } else {
            ts->no_need_peek = 0;
        }
    } else {
        ts->no_need_peek = 0;
    }

    return ret;
}

static int update_audio_packets(struct mpegts_context *ts, struct aic_parser_packet *pkt)
{
    if (ts->cur_codec_id != CODEC_ID_MP3 &&
        ts->cur_codec_id != CODEC_ID_AAC) {
        return PARSER_OK;
    }

    if (ts->no_need_peek) {
        ts->read_offset += pkt->size;
    }

    return PARSER_OK;
}

int mpegts_peek_packet(struct aic_mpegts_parser *s, struct aic_parser_packet *pkt)
{
    if (!s->priv_data) {
        return PARSER_NOMEM;
    }
    int ret = PARSER_OK;
    struct mpegts_context *ts = s->priv_data;

    /* Read and combained all es packet of one frame to temp buffer*/
    if (ts->no_need_peek == 0) {
        ret = handle_packets(ts, 0);
        if (ret == PARSER_EOS) {
            pkt->size = 0;
            pkt->flag = PACKET_EOS;
        } else {
            pkt->type = ts->pkt.type;
            pkt->pts = ts->pkt.pts;
            pkt->dts = ts->pkt.dts;
            pkt->flag = ts->pkt.flag;
            pkt->duration = ts->pkt.duration;
            ts->read_offset = 0;
            pkt->size = ts->pkt.size;
            if (ts->pkt.type == MPP_MEDIA_TYPE_AUDIO) {
                handle_audio_packets(ts, pkt);
            } else {
                ts->no_need_peek = 0;
            }
        }
    } else {
        ret = handle_audio_packets(ts, pkt);
    }

    logd("peek %d no_need_peek %d, packet: type=%d, size=%d, pts=%ld",
         ret, ts->no_need_peek, pkt->type, pkt->size, pkt->pts);
    return ret;
}

int mpegts_read_packet(struct aic_mpegts_parser *s, struct aic_parser_packet *pkt)
{
    if (!pkt || !s->priv_data) {
        return PARSER_NOMEM;
    }
    struct mpegts_context *ts = s->priv_data;

    memcpy(pkt->data, ts->pkt.data + ts->read_offset, pkt->size);

    if (ts->pkt.type == MPP_MEDIA_TYPE_AUDIO) {
        update_audio_packets(ts, pkt);
    }

    return PARSER_OK;
}

int mpegts_seek_packet(struct aic_mpegts_parser *s, s64 pts)
{
    return PARSER_ERROR;
}

static void mpegts_free(struct mpegts_context *ts)
{
    int i;
    for (i = 0; i < NB_PID_MAX; i++)
        if (ts->pids[i])
            mpegts_close_filter(ts, ts->pids[i]);
}

int mpegts_read_close(struct aic_mpegts_parser *s)
{
    int i = 0;
    struct mpegts_context *ts = s->priv_data;
    for (i = 0; i < s->nb_streams; i++) {
        struct mpegts_stream_ctx *st = s->streams[i];
        if (!st) {
            continue;
        }
        mpp_free(st);
        s->streams[i] = NULL;
    }
    if (!ts) {
        return PARSER_NOMEM;
    }
    if (ts->pkt.data) {
        mpp_free(ts->pkt.data);
        ts->pkt.data = NULL;
    }
    mpegts_free(ts);
    mpp_free(ts);
    return PARSER_OK;
}
