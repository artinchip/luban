/*
 * Copyright (C) 2020-2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: <che.jiang@artinchip.com>
 * Desc: flv file demuxer
 */

#include "flv.h"
#include "aic_parser.h"
#include "aic_stream.h"
#include "aic_tag.h"
#include "mpp_log.h"
#include "mpp_mem.h"
#include <inttypes.h>
#include <stdlib.h>

#define FLV_MAX_DEPTH 16

/* TagType(1) + DataSize(3) + TimeStamp(3) + TimeStampExtended(1) + StremId(3) */
#define FLV_TAG_HEADER_SIZE 11

/* offsets for packed values */
#define FLV_AUDIO_SAMPLERATE_OFFSET 2
#define FLV_AUDIO_CODECID_OFFSET 4
#define FLV_VIDEO_FRAMETYPE_OFFSET 4

/* bitmasks to isolate specific values */
#define FLV_VIDEO_FRAMETYPE_MASK 0xf0

#define AMF_END_OF_OBJECT 0x09

/* AMF1 Packet: AMFType(1) + AMFLen(2) + AMFValue(10)*/
#define AMF1_TAG_HEADER_SIZE 13
/* AMF2 Packet: AMFType(1) + AMFLen(4)*/
#define AMF2_TAG_HEADER_SIZE 5

#define FLV_TYPE_ONTEXTDATA 1
#define FLV_TYPE_ONCAPTION 2
#define FLV_TYPE_ONCAPTIONINFO 3
#define FLV_TYPE_UNKNOWN 9

typedef enum {
    FLV_HEADER_FLAG_HASVIDEO = 1,
    FLV_HEADER_FLAG_HASAUDIO = 4,
} FLV_HEADER_FLAG;

typedef enum {
    FLV_TAG_TYPE_AUDIO = 0x08,
    FLV_TAG_TYPE_VIDEO = 0x09,
    FLV_TAG_TYPE_META = 0x12,
} FLV_TAG_TYPE;

typedef enum {
    FLV_STREAM_TYPE_VIDEO,
    FLV_STREAM_TYPE_AUDIO,
    FLV_STREAM_TYPE_SUBTITLE,
    FLV_STREAM_TYPE_DATA,
    FLV_STREAM_TYPE_NB,
} FLV_STREAM_TYPE;

typedef enum {
    FLV_MONO = 0,
    FLV_STEREO = 1,
} FLV_CHANNEL;

typedef enum {
    FLV_CODECID_PCM = 0,
    FLV_CODECID_ADPCM = 1 << FLV_AUDIO_CODECID_OFFSET,
    FLV_CODECID_MP3 = 2 << FLV_AUDIO_CODECID_OFFSET,
    FLV_CODECID_PCM_LE = 3 << FLV_AUDIO_CODECID_OFFSET,
    FLV_CODECID_NELLYMOSER_16KHZ_MONO = 4 << FLV_AUDIO_CODECID_OFFSET,
    FLV_CODECID_NELLYMOSER_8KHZ_MONO = 5 << FLV_AUDIO_CODECID_OFFSET,
    FLV_CODECID_NELLYMOSER = 6 << FLV_AUDIO_CODECID_OFFSET,
    FLV_CODECID_PCM_ALAW = 7 << FLV_AUDIO_CODECID_OFFSET,
    FLV_CODECID_PCM_MULAW = 8 << FLV_AUDIO_CODECID_OFFSET,
    FLV_CODECID_AAC = 10 << FLV_AUDIO_CODECID_OFFSET,
    FLV_CODECID_SPEEX = 11 << FLV_AUDIO_CODECID_OFFSET,
} FLV_CODECID_AUDIO;

typedef enum {
    FLV_CODECID_H263 = 2,
    FLV_CODECID_SCREEN = 3,
    FLV_CODECID_VP6 = 4,
    FLV_CODECID_VP6A = 5,
    FLV_CODECID_SCREEN2 = 6,
    FLV_CODECID_H264 = 7,
    FLV_CODECID_REALH263 = 8,
    FLV_CODECID_MPEG4 = 9,
} FLV_CODECID_VIDEO;

typedef enum {
    FLV_AVC_PACKET_HEADER = 0,
    FLV_AVC_PACKET_NALU = 1,
    FLV_AVC_PACKET_TAIL = 2,
} FLV_AVC_PACKET_TYPE;

typedef enum {
    /* key frame (for AVC, a seekable frame) */
    FLV_FRAME_KEY = 1 << FLV_VIDEO_FRAMETYPE_OFFSET,
    /* inter frame (for AVC, a non-seekable frame) */
    FLV_FRAME_INTER = 2 << FLV_VIDEO_FRAMETYPE_OFFSET,
    /* disposable inter frame (H.263 only) */
    FLV_FRAME_DISP_INTER = 3 << FLV_VIDEO_FRAMETYPE_OFFSET,
    /* generated key frame (reserved for server use only)*/
    FLV_FRAME_GENERATED_KEY = 4 << FLV_VIDEO_FRAMETYPE_OFFSET,
    /* video info/command frame*/
    FLV_FRAME_VIDEO_INFO_CMD = 5 << FLV_VIDEO_FRAMETYPE_OFFSET,
} FLV_FRAME;

typedef enum {
    AMF_DATA_TYPE_NUMBER = 0x00,
    AMF_DATA_TYPE_BOOL = 0x01,
    AMF_DATA_TYPE_STRING = 0x02,
    AMF_DATA_TYPE_OBJECT = 0x03,
    AMF_DATA_TYPE_NULL = 0x05,
    AMF_DATA_TYPE_UNDEFINED = 0x06,
    AMF_DATA_TYPE_REFERENCE = 0x07,
    AMF_DATA_TYPE_MIXEDARRAY = 0x08,
    AMF_DATA_TYPE_OBJECT_END = 0x09,
    AMF_DATA_TYPE_ARRAY = 0x0a,
    AMF_DATA_TYPE_DATE = 0x0b,
    AMF_DATA_TYPE_LONG_STRING = 0x0c,
    AMF_DATA_TYPE_UNSUPPORTED = 0x0d,
} FLV_AMF_DATA_TYPE;

typedef struct flv_context {
    int trust_metadata;     // configure streams according onMetaData
    int wrong_dts;          // wrong dts due to negative cts
    int framerate;
    unsigned int packet_cnt;
    int64_t video_bit_rate;
    int64_t audio_bit_rate;
    int64_t seek_pts;
    int64_t last_pts;
    int64_t cur_pts;
    int64_t cur_pos;
    int64_t last_pos;
    int64_t pre_key_pts;
    int64_t pre_key_pos;
    int64_t last_key_pts;
    int64_t last_key_pos;
    int64_t next_key_pos;
    char has_video;
    char has_audio;
    char find_key_frame;
    char seek_dir;          // 0 - none, 1 - forward, 2 - backward
    int orig_size;
    int packet_size;
    int pre_tag_size;
    int cur_stream_type;
} flv_context;

union flv_intfloat64 {
    uint64_t i;
    double   f;
};

static double flv_int2double(uint64_t i)
{
    union flv_intfloat64 v;
    v.i = i;
    return v.f;
}


static struct flv_stream_ctx *flv_new_stream(struct aic_flv_parser *s)
{
    struct flv_stream_ctx *sc;

    sc = (struct flv_stream_ctx *)mpp_alloc(sizeof(struct flv_stream_ctx));
    if (sc == NULL) {
        return NULL;
    }
    memset(sc, 0, sizeof(struct flv_stream_ctx));

    sc->index = s->nb_streams;
    s->streams[s->nb_streams++] = sc;

    return sc;
}

static struct flv_stream_ctx *flv_find_stream(struct aic_flv_parser *s, int stream_type)
{
    int i = 0;
    struct flv_stream_ctx *st = NULL;

    for (i = 0; i < s->nb_streams; i++) {
        st = s->streams[i];
        if (stream_type == FLV_STREAM_TYPE_AUDIO) {
            if (st->codecpar.codec_type == MPP_MEDIA_TYPE_AUDIO)
                break;
        } else if (stream_type == FLV_STREAM_TYPE_VIDEO) {
            if (st->codecpar.codec_type == MPP_MEDIA_TYPE_VIDEO)
                break;
        } else if (stream_type == FLV_STREAM_TYPE_SUBTITLE ||
                   stream_type == FLV_STREAM_TYPE_DATA) {
                if (st->codecpar.codec_type == MPP_MEDIA_TYPE_OTHER)
                    break;
        }
    }

    if (i == s->nb_streams) {
        logw("unsupport another stream_type %d.", stream_type);
        return NULL;
    }

    return st;
}

static void flv_set_audio_codec(struct aic_flv_parser *s, struct flv_stream_ctx *astream,
                                struct aic_codec_param *apar, int flv_codecid)
{
    switch (flv_codecid) {
    // no distinction between S16 and S8 PCM codec flags
    case FLV_CODECID_PCM:
        apar->codec_id = apar->bits_per_coded_sample == 8
                             ? CODEC_ID_PCM_U8
#if HAVE_BIGENDIAN
                             : CODEC_ID_PCM_S16BE;
#else
                             : CODEC_ID_PCM_S16LE;
#endif
        break;
    case FLV_CODECID_AAC:
        apar->codec_id = CODEC_ID_AAC;
        break;
    case FLV_CODECID_MP3:
        apar->codec_id = CODEC_ID_MP3;
        break;
    default:
        loge("unsupport audio codec (%x)",
             flv_codecid >> FLV_AUDIO_CODECID_OFFSET);
        apar->codec_tag = flv_codecid >> FLV_AUDIO_CODECID_OFFSET;
    }
}

static int flv_set_video_codec(struct aic_flv_parser *s, struct flv_stream_ctx *vstream,
                               int flv_codecid, int read)
{
    int ret = 0;
    struct aic_codec_param *par = &vstream->codecpar;
    switch (flv_codecid) {
    case FLV_CODECID_H264:
        par->codec_id = CODEC_ID_H264;
        ret = 3; // not 4, reading packet type will consume one byte
        break;
    case FLV_CODECID_MPEG4:
        par->codec_id = CODEC_ID_MPEG4;
        ret = 3;
        break;
    default:
        loge("unsupport video codec (%x)", flv_codecid);
        par->codec_tag = flv_codecid;
    }

    return ret;
}

static int amf_get_string(struct aic_stream *ioc, char *buffer, int buffsize)
{
    int ret;
    int length = aic_stream_rb16(ioc);
    if (length >= buffsize) {
        aic_stream_skip(ioc, length);
        return PARSER_ERROR;
    }

    ret = aic_stream_read(ioc, buffer, length);
    if (ret < 0)
        return ret;
    if (ret < length)
        return -PARSER_INVALIDDATA;

    buffer[length] = '\0';

    return length;
}

static int amf_get_object(struct aic_flv_parser *s, struct flv_stream_ctx *astream,
                          struct flv_stream_ctx *vstream, const char *key,
                          FLV_AMF_DATA_TYPE amf_type, int depth, double num_val)
{
    if (!key)
        return PARSER_ERROR;
    if (depth != 1)
        return PARSER_OK;

    if (amf_type != AMF_DATA_TYPE_NUMBER && amf_type != AMF_DATA_TYPE_BOOL) {
        return PARSER_OK;
    }

    flv_context *flv = s->priv_data;
    struct aic_codec_param *apar, *vpar;
    apar = astream ? &astream->codecpar : NULL;
    vpar = vstream ? &vstream->codecpar : NULL;

    if (!strcmp(key, "duration")) {
        s->duration = num_val * MPP_TIME_BASE;
    } else if (!strcmp(key, "videodatarate") && 0 <= (int)(num_val * 1024.0)) {
        flv->video_bit_rate = num_val * 1024.0;
    } else if (!strcmp(key, "audiodatarate") && 0 <= (int)(num_val * 1024.0)) {
        flv->audio_bit_rate = num_val * 1024.0;
    } else if (!strcmp(key, "framerate")) {
        flv->framerate = num_val;
    } else if (flv->trust_metadata) {
        if (!strcmp(key, "videocodecid") && vpar) {
            int ret = flv_set_video_codec(s, vstream, (int)num_val, 0);
            if (ret < 0)
                return ret;
        } else if (!strcmp(key, "audiocodecid") && apar) {
            int id = ((int)num_val) << FLV_AUDIO_CODECID_OFFSET;
            flv_set_audio_codec(s, astream, apar, id);
        } else if (!strcmp(key, "audiosamplerate") && apar) {
            apar->sample_rate = num_val;
        } else if (!strcmp(key, "audiosamplesize") && apar) {
            apar->bits_per_coded_sample = num_val;
        } else if (!strcmp(key, "stereo") && apar) {
            apar->channels = num_val + 1;
        } else if (!strcmp(key, "width") && vpar) {
            vpar->width = num_val;
        } else if (!strcmp(key, "height") && vpar) {
            vpar->height = num_val;
        }
    }

    return PARSER_OK;
}

static int amf_parse_object(struct aic_flv_parser *s, struct flv_stream_ctx *astream,
                            struct flv_stream_ctx *vstream, const char *key,
                            int64_t max_pos, int depth)
{
    struct aic_stream *ioc;
    FLV_AMF_DATA_TYPE amf_type;
    char str_val[512];
    double num_val;

    if (depth > FLV_MAX_DEPTH)
        return PARSER_ERROR;

    num_val = 0;
    ioc = s->stream;

    amf_type = aic_stream_r8(ioc);

    switch (amf_type) {
    case AMF_DATA_TYPE_NUMBER:
        num_val = flv_int2double(aic_stream_rb64(ioc));
        logi("amf_type = %d key:%s num_val = %f.", amf_type, key, num_val);
        break;
    case AMF_DATA_TYPE_BOOL:
        num_val = aic_stream_r8(ioc);
        break;
    case AMF_DATA_TYPE_STRING:
        if (amf_get_string(ioc, str_val, sizeof(str_val)) < 0) {
            loge("AMF_DATA_TYPE_STRING parsing failed\n");
            return PARSER_ERROR;
        }
        break;
    case AMF_DATA_TYPE_OBJECT:
        while (aic_stream_tell(ioc) < max_pos - 2 &&
               amf_get_string(ioc, str_val, sizeof(str_val)) > 0)
            if (amf_parse_object(s, astream, vstream, str_val, max_pos,
                                 depth + 1) < 0)
                return PARSER_ERROR; // if we couldn't skip, bomb out.
        if (aic_stream_r8(ioc) != AMF_END_OF_OBJECT) {
            loge("Missing AMF_END_OF_OBJECT in AMF_DATA_TYPE_OBJECT\n");
            return PARSER_ERROR;
        }
        break;
    case AMF_DATA_TYPE_NULL:
    case AMF_DATA_TYPE_UNDEFINED:
    case AMF_DATA_TYPE_UNSUPPORTED:
        break; // these take up no additional space
    case AMF_DATA_TYPE_MIXEDARRAY: {
        unsigned v;
        aic_stream_skip(ioc, 4); // skip 32-bit max array index
        while (aic_stream_tell(ioc) < max_pos - 2 &&
               amf_get_string(ioc, str_val, sizeof(str_val)) > 0)
            /*this is the only case in which we would want a nested
              parse to not skip over the object */
            if (amf_parse_object(s, astream, vstream, str_val, max_pos,
                                 depth + 1) < 0)
                return PARSER_ERROR;
        v = aic_stream_r8(ioc);
        if (v != AMF_END_OF_OBJECT) {
            loge("Missing AMF_END_OF_OBJECT in AMF_DATA_TYPE_MIXEDARRAY, found %d\n", v);
            return PARSER_ERROR;
        }
        break;
    }
    case AMF_DATA_TYPE_ARRAY: {
        unsigned int arraylen, i;
        arraylen = aic_stream_rb32(ioc);
        for (i = 0; i < arraylen && aic_stream_tell(ioc) < max_pos - 1; i++)
            if (amf_parse_object(s, NULL, NULL, NULL, max_pos,
                                 depth + 1) < 0)
                return PARSER_ERROR; // if we couldn't skip, bomb out.
    } break;
    case AMF_DATA_TYPE_DATE:
        aic_stream_rb64(ioc);
        aic_stream_rb16(ioc);
        break;
    default: // unsupported type, we couldn't skip
        loge("unsupported amf type %d\n", amf_type);
        return PARSER_ERROR;
    }

    amf_get_object(s, astream, vstream, key, amf_type, depth, num_val);

    return PARSER_OK;
}


static int flv_read_metabody(struct aic_flv_parser *s, int64_t next_pos)
{
    int ret = PARSER_OK;
    int i = 0;
    char buffer[32];
    FLV_AMF_DATA_TYPE type;
    struct aic_stream *ioc = NULL;
    struct flv_stream_ctx *stream = NULL;
    struct flv_stream_ctx *astream = NULL;
    struct flv_stream_ctx *vstream = NULL;

    ioc = s->stream;

    /* first object needs to be "onMetaData" string */
    type = aic_stream_r8(ioc);
    if (type != AMF_DATA_TYPE_STRING) {
        loge("type %d is not support or amf.", type);
        return FLV_TYPE_UNKNOWN;
    }

    ret = amf_get_string(ioc, buffer, sizeof(buffer));
    if (ret < 0) {
        loge("amf_get_string  failed %d.", ret);
        return FLV_TYPE_UNKNOWN;
    }

    if (!strcmp(buffer, "onTextData")) {
        loge("the tag is %s, not metadata.", buffer);
        return FLV_TYPE_ONTEXTDATA;
    }

    if (!strcmp(buffer, "onCaption")) {
        loge("the tag is %s, not metadata.", buffer);
        return FLV_TYPE_ONCAPTION;
    }

    if (!strcmp(buffer, "onCaptionInfo")) {
        loge("the tag is %s, not metadata.", buffer);
        return FLV_TYPE_ONCAPTIONINFO;
    }

    if (strcmp(buffer, "onMetaData") && strcmp(buffer, "onCuePoint")) {
        logi("Unknown type %s.", buffer);
        return FLV_TYPE_UNKNOWN;
    }

    logi("type = 0x%x, buffer:%s.", type, buffer);

    /* find the streams now so that amf_parse_object doesn't need to do
       the lookup every time it is called.*/
    for (i = 0; i < s->nb_streams; i++) {
        stream = s->streams[i];
        if (stream->codecpar.codec_type == MPP_MEDIA_TYPE_VIDEO) {
            vstream = stream;
        } else if (stream->codecpar.codec_type == MPP_MEDIA_TYPE_AUDIO) {
            astream = stream;
        } else {
            loge("can not find stream type %d.", stream->codecpar.codec_type);
            return PARSER_NODATA;
        }
    }

    /* parse the second object (we want a mixed array)*/
    ret = amf_parse_object(s, astream, vstream, buffer, next_pos, 0);
    if (ret < 0) {
        loge("metadata amf parse object failed 0x%x.", ret);
        return PARSER_ERROR;
    }

    return PARSER_OK;
}

int flv_read_metadata(struct aic_flv_parser *s)
{
    int ret, size;
    FLV_TAG_TYPE type;
    int64_t next;
    int64_t dts = 0;
    int pre_tag_size = 0;

    /* pkt size is repeated at end. skip it */
    aic_stream_tell(s->stream);
    type = (aic_stream_r8(s->stream) & 0x1F); /*constant value 18*/
    size = aic_stream_rb24(s->stream);
    dts = aic_stream_rb24(s->stream); /* millseconds */
    dts |= aic_stream_r8(s->stream) << 28;

    logi("type:%d, size:%d, pos:%" PRId64 "\n",
         type, size, aic_stream_tell(s->stream));

    if (aic_stream_tell(s->stream) >= aic_stream_size(s->stream))
        return PARSER_EOS;

    aic_stream_skip(s->stream, 3); /* stream id, always 0 */
    next = size + aic_stream_tell(s->stream);

    if (type != FLV_TAG_TYPE_META) {
        loge("Not include the first metadata tag.\n");
        return PARSER_INVALIDDATA;
    }

    /* Header-type metadata stuff check*/
    if (size <= AMF1_TAG_HEADER_SIZE + AMF2_TAG_HEADER_SIZE) {
        loge("Metadata tag size is %d.\n", size);
        return PARSER_INVALIDDATA;
    }

    /* Read metabody to get audio and video params*/
    aic_stream_tell(s->stream);
    ret = flv_read_metabody(s, next);
    if (ret != PARSER_OK) {
        loge("metabody mismatch err = %d\n", ret);
        return PARSER_INVALIDDATA;
    }

    pre_tag_size = aic_stream_rb32(s->stream);
    if (pre_tag_size != size + FLV_TAG_HEADER_SIZE) {
        logw("Metadata size %d != %d check failed\n",
            pre_tag_size, size + FLV_TAG_HEADER_SIZE);
    }

    return PARSER_OK;
}

int flv_read_header(struct aic_flv_parser *s)
{
    int flags;
    int offset;
    int pre_tag_size = 0;
    flv_context *flv = NULL;
    struct flv_stream_ctx *st = NULL;

    s->priv_data = mpp_alloc(sizeof(struct flv_context));
    if (!s->priv_data) {
        loge("malloc for flv_context failed.\n");
        return PARSER_NOMEM;
    }
    memset(s->priv_data, 0, sizeof(struct flv_context));

    aic_stream_skip(s->stream, 4);
    flags = aic_stream_r8(s->stream);
    logi("read flv header flags = 0x%x.", flags);

    flv = s->priv_data;
    flv->trust_metadata = 1;

    offset = aic_stream_rb32(s->stream);
    aic_stream_seek(s->stream, offset, SEEK_SET);

    pre_tag_size = aic_stream_rb32(s->stream);
    if (pre_tag_size) {
        logw("Unstandard flv format, pre_tag_size(%d)\n", pre_tag_size);
    }

    /*Has video and audio, then create the stream in advanced*/
    if (flags & FLV_HEADER_FLAG_HASVIDEO) {
        flv->has_video = 1;
        st = flv_new_stream(s);
        if (!st) {
            loge("create stream for video failed.");
            return PARSER_NOMEM;
        }
        st->codecpar.codec_type = MPP_MEDIA_TYPE_VIDEO;
    }
    if (flags & FLV_HEADER_FLAG_HASAUDIO) {
        flv->has_audio = 1;
        st = flv_new_stream(s);
        if (!st) {
            loge("create stream for audio failed.");
            return PARSER_NOMEM;
        }
        st->codecpar.codec_type = MPP_MEDIA_TYPE_AUDIO;
    }

    /*Read data of metadata, then get audio and video parametes*/
    if (flv_read_metadata(s)) {
         loge("flv read metadata failed.");
         return PARSER_INVALIDDATA;
    }

    logi("flv read header success.");

    return PARSER_OK;
}


int flv_read_close(struct aic_flv_parser *s)
{
    int i = 0;
    struct flv_stream_ctx *st = NULL;
    for (i = 0; i < s->nb_streams; i++) {
        st = s->streams[i];
        if (st)
            mpp_free(st);
    }

    if (s->priv_data)
        mpp_free(s->priv_data);

    return PARSER_OK;
}

static int64_t flv_sat_add64(int64_t a, int64_t b)
{

    int64_t s = a + (uint64_t)b;
    if ((int64_t)((a ^ b) | (~s ^ b)) >= 0)
        return INT64_MAX ^ (b >> 63);
    return s;
}

int flv_seek_audio_pos(struct aic_flv_parser *s, int64_t next)
{
    flv_context *flv = s->priv_data;

    if (flv->seek_dir == 1) { //forward
        if (flv->seek_pts > flv->cur_pts * 1000) {
            aic_stream_seek(s->stream, next + 4, SEEK_SET);
        } else {
            flv->seek_dir = 0;  // reset direction
            return 0;
        }
    } else if (flv->seek_dir == 2) { //backward
        if (flv->seek_pts < flv->cur_pts * 1000) {
            /*Get the pre frame size then seek to the pre frame*/
            aic_stream_seek(s->stream, flv->cur_pos - 4, SEEK_SET);
            int pre_tag_size = aic_stream_rb32(s->stream);
            int64_t pre_tag_pos = flv->cur_pos - (pre_tag_size + 4);
            if (pre_tag_pos >= 0) {
                aic_stream_seek(s->stream, pre_tag_pos, SEEK_SET);
            } else {
                aic_stream_seek(s->stream, 0, SEEK_SET);
                flv->seek_dir = 0;
                return 0;
            }
        } else {
            flv->seek_dir = 0;
            return 0;
        }
    } else {
        flv->seek_dir = 0;  // reset direction
        return 0;
    }

    return 1;
}

int flv_seek_video_pos(struct aic_flv_parser *s, int stream_type, int flags, int64_t next)
{
    int64_t  diff = 0;
    static int64_t min_diff = INT64_MAX;
    flv_context *flv = s->priv_data;
    if (flv->seek_dir == 1) { //forward
        logd("seek flv packet: seek_pts %"PRId64", cur_pts %"PRId64".\n", flv->seek_pts, next);
        if (flv->seek_pts >= flv->cur_pts * 1000) {
            diff = MPP_ABS(flv->seek_pts, flv->cur_pts * 1000);
            if (min_diff == INT64_MAX) {
                flv->pre_key_pos = flv->last_key_pos;
                flv->pre_key_pts = flv->last_key_pts;
            }
            if (diff < min_diff) {
                min_diff = diff;
                if (stream_type == FLV_STREAM_TYPE_VIDEO &&
                    ((flags & FLV_VIDEO_FRAMETYPE_MASK) == FLV_FRAME_KEY)) {
                    flv->find_key_frame = 1;
                    flv->last_key_pos = flv->cur_pos;
                }
            }
            /*Seek next packet need skip pre_tag_size*/
            aic_stream_seek(s->stream, next + 4, SEEK_SET);
        } else {
            /*If find key frame then seek to key frame postion,
             otherwise seek to the start postion */
            if (flv->find_key_frame) {
                aic_stream_seek(s->stream, flv->last_key_pos, SEEK_SET);
                logi("find key frame, key_pos: %"PRId64"", flv->last_key_pos);
                flv->find_key_frame = 0;
                flv->seek_dir = 0;  // reset direction
            } else {
                /*Forward to find the next key frame, compare with the pre key pos
                  and get the min diff time, finally get the position*/
                if (stream_type == FLV_STREAM_TYPE_VIDEO &&
                    ((flags & FLV_VIDEO_FRAMETYPE_MASK) == FLV_FRAME_KEY)) {
                    logw("cur_pts = %"PRId64", pre_key_pts = %"PRId64", seek_pts = %"PRId64"",
                        flv->cur_pts * 1000, flv->pre_key_pts * 1000, flv->seek_pts);
                    diff = MPP_ABS(flv->seek_pts, flv->cur_pts * 1000);
                    if (diff <= MPP_ABS(flv->seek_pts, flv->pre_key_pts * 1000)) {
                        flv->find_key_frame = 1;
                        flv->last_key_pos = flv->cur_pos;
                        flv->seek_dir = 0;  // reset direction
                        min_diff = INT64_MAX;
                        logi("find key frame, key_pos: %"PRId64", key_pts: %"PRId64"",
                            flv->cur_pos, flv->cur_pts * 1000);
                        return 0;
                    } else {
                        aic_stream_seek(s->stream, flv->pre_key_pos, SEEK_SET);
                    }
                } else {
                    aic_stream_seek(s->stream, next + 4, SEEK_SET);
                    logd("can not find key frame, then find next frame");
                }
            }
        }
    } else if (flv->seek_dir == 2) { //backward
        if (flv->seek_pts < flv->cur_pts * 1000) {
            /*Get the pre frame size then seek to the pre frame*/
            aic_stream_seek(s->stream, flv->cur_pos - 4, SEEK_SET);
            int pre_tag_size = aic_stream_rb32(s->stream);
            int64_t pre_tag_pos = flv->cur_pos - (pre_tag_size + 4);
            if (pre_tag_pos >= 0) {
                aic_stream_seek(s->stream, pre_tag_pos, SEEK_SET);
            } else {
                aic_stream_seek(s->stream, 0, SEEK_SET);
                flv->seek_dir = 0;
                return 0;
            }
        } else {
            /*If not find key frame need to find the pre frame*/
            if (stream_type == FLV_STREAM_TYPE_VIDEO &&
                ((flags & FLV_VIDEO_FRAMETYPE_MASK) == FLV_FRAME_KEY)) {
                flv->find_key_frame = 1;
                flv->last_key_pos = flv->cur_pos;
                flv->seek_dir = 0;
                return 0;
            } else {
                if (flv->cur_pts == 0) {
                    flv->seek_dir = 0;
                    return 0;
                }
                /*Get the pre frame size then seek to the pre frame*/
                aic_stream_seek(s->stream, flv->cur_pos - 4, SEEK_SET);
                int pre_tag_size = aic_stream_rb32(s->stream);
                int64_t pre_tag_pos = flv->cur_pos - (pre_tag_size + 4);
                if (pre_tag_pos >= 0) {
                    aic_stream_seek(s->stream, pre_tag_pos, SEEK_SET);
                } else {
                    aic_stream_seek(s->stream, 0, SEEK_SET);
                    flv->seek_dir = 0;
                    return 0;
                }
            }
        }
    } else {
        flv->seek_dir = 0;  // reset direction
        return 0;
    }

    return 1;  // need try again
}

int flv_get_packet_data(struct aic_flv_parser *s, struct flv_stream_ctx *st,
                        struct aic_parser_packet *pkt)
{
    flv_context *flv = s->priv_data;

    if (st->codecpar.codec_id == CODEC_ID_AAC ||
        st->codecpar.codec_id == CODEC_ID_H264 ||
        st->codecpar.codec_id == CODEC_ID_MPEG4) {
        uint8_t packet_type = aic_stream_r8(s->stream);
        flv->packet_size--;
        if (flv->packet_size < 0) {
            return PARSER_INVALIDDATA;
        }
        if ((st->codecpar.codec_id == CODEC_ID_H264 || st->codecpar.codec_id == CODEC_ID_MPEG4)) {
            if (packet_type == FLV_AVC_PACKET_HEADER) {
                pkt->flag = PACKET_EXTRA_DATA;
            } else if (packet_type == FLV_AVC_PACKET_TAIL) {
                pkt->flag = PACKET_EOS;
                pkt->size = 0;
                return PARSER_EOS;
            }

            // sign extension
            int32_t cts = (aic_stream_rb24(s->stream) + 0xff800000) ^ 0xff800000;
            int64_t pts = flv_sat_add64(flv->cur_pts, cts);
            if (cts < 0) { // dts might be wrong
                if (!flv->wrong_dts)
                    logw("Negative cts, previous timestamps might be wrong.\n");
                flv->wrong_dts = 1;
            } else if (MPP_ABS(flv->cur_pts, pts) > 1000 * 60 * 15) {
                logw("invalid timestamps %" PRId64 " %" PRId64 "\n", flv->cur_pts, pts);
                flv->cur_pts = 0;
                return PARSER_INVALIDDATA;
            }
            flv->packet_size -= 3;
            if (flv->packet_size < 0) {
                return PARSER_INVALIDDATA;
            }
            flv->cur_pts = pts;
        }
    }

    return PARSER_OK;
}

int flv_peek_packet(struct aic_flv_parser *s, struct aic_parser_packet *pkt)
{
    flv_context *flv = s->priv_data;
    int ret = 0, size, flags;
    int stream_type = -1;
    FLV_TAG_TYPE type;
    int64_t next, pos;
    int64_t dts = 0;
    struct flv_stream_ctx *st = NULL;
    int last = -1;

retry:
    /* pkt size is repeated at end. skip it */
    pos = aic_stream_tell(s->stream);
    type = (aic_stream_r8(s->stream) & 0x1F);
    size = aic_stream_rb24(s->stream);
    dts = aic_stream_rb24(s->stream); /* millseconds */
    dts |= (unsigned)aic_stream_r8(s->stream) << 24;
    flv->cur_pts = dts;

    if (aic_stream_tell(s->stream) >= aic_stream_size(s->stream)) {
        pkt->flag = PACKET_EOS;
        pkt->size = 0;
        return PARSER_EOS;
    }
    flv->orig_size = size;
    flv->cur_pos = pos;
    logd("type:%d, size:%d, last:%d, dts:%"PRId64", pos:%"PRId64", packet_cnt:%u\n",
         type, size, last, dts, pos, flv->packet_cnt);

    aic_stream_skip(s->stream, 3); /* stream id, always 0 */

    if (size == 0) {
        ret = PARSER_ERROR;
        goto out;
    }

    flags = 0;
    next = size + aic_stream_tell(s->stream);
    if (type == FLV_TAG_TYPE_AUDIO) {
        stream_type = FLV_STREAM_TYPE_AUDIO;
        flags = aic_stream_r8(s->stream);
        size--;
        pkt->type = MPP_MEDIA_TYPE_AUDIO;
    } else if (type == FLV_TAG_TYPE_VIDEO) {
        stream_type = FLV_STREAM_TYPE_VIDEO;
        flags = aic_stream_r8(s->stream);
        size--;
        pkt->type = MPP_MEDIA_TYPE_VIDEO;
        if ((flags & FLV_VIDEO_FRAMETYPE_MASK) == FLV_FRAME_KEY) {
            flv->last_key_pts = flv->cur_pts;
            flv->last_key_pos = flv->cur_pos;
        }
        if ((flags & FLV_VIDEO_FRAMETYPE_MASK) == FLV_FRAME_VIDEO_INFO_CMD)
            goto skip;
    } else {
        loge("Skipping flv packet: type %d, size %d, flags %d.\n", type, size, flags);
skip:
        /* May be happend err in this situation. */
        if (aic_stream_seek(s->stream, next, SEEK_SET) != next) {
            loge("Unable to seek to the next packet\n");
            return PARSER_INVALIDDATA;
        }
        ret = PARSER_ERROR;
        goto out;
    }

    /* skip empty data packets */
    if (!size) {
        ret = PARSER_ERROR;
        goto out;
    }
    st = flv_find_stream(s, stream_type);
    if (!st) {
        loge("find stream_type %d failed.", stream_type);
        return PARSER_NOMEM;
    }

    /* seek packet by pts */
    flv->packet_size = size;
    if (flv->seek_dir && (flv->seek_pts >= 0)) {
        logd("seek flv packet: type %d, flags %d, next %"PRId64".\n", stream_type, flags, next);
        if (!flv->has_video && flv->has_audio) {
            if (flv_seek_audio_pos(s, next))
                goto retry;
        } else if (flv->has_video) {
            if (flv_seek_video_pos(s, stream_type, flags, next))
                goto retry;
        }
    }

    pkt->flag = 0;
    ret = flv_get_packet_data(s, st, pkt);
    if (ret != PARSER_OK) {
        goto out;
    }

    pkt->pts = flv->cur_pts * 1000;
    pkt->size = flv->packet_size;
    flv->last_pts = flv->cur_pts;
    flv->last_pos = pos;
    flv->cur_pos = aic_stream_tell(s->stream);
    flv->packet_cnt++;

    logd("peek flv packet: type = %d, size = %d, pts = %"PRId64".\n",
        pkt->type, pkt->size, pkt->pts);
out:
    return ret;
}

int flv_read_packet(struct aic_flv_parser *s, struct aic_parser_packet *pkt)
{
    flv_context *flv = s->priv_data;
    int pre_tag_size = -1;
    aic_stream_seek(s->stream, flv->cur_pos, SEEK_SET);
    aic_stream_read(s->stream, pkt->data, pkt->size);

    pre_tag_size = aic_stream_rb32(s->stream);

    if (pre_tag_size != flv->orig_size + FLV_TAG_HEADER_SIZE) {
        loge("Packet mismatch %d %d\n", pre_tag_size, flv->orig_size + FLV_TAG_HEADER_SIZE);
    }

    logd("read flv packet: type %d, size %d, cur_pos %"PRId64",  pts = %"PRId64"\n",
            pkt->type, pkt->size, flv->cur_pos, pkt->pts);
    return PARSER_OK;
}

int flv_seek_packet(struct aic_flv_parser *s, s64 pts)
{
    flv_context *flv = s->priv_data;

    flv->seek_pts = pts;
    flv->seek_dir = (pts > flv->cur_pts * 1000) ? 1 : 2;
    if (flv->seek_pts < 0)
        flv->seek_dir = 0;

    /*Reset to cur position avoid some err happend*/
    if (flv->seek_dir && flv->seek_pts >= 0) {
        aic_stream_seek(s->stream, flv->last_pos, SEEK_SET);
    }
    logi("seek_pts:%"PRId64", cur_pts:%"PRId64", last_pos:%"PRId64", seek_dir:%d, packet_cnt:%u",
        pts, flv->cur_pts * 1000, flv->last_pos, flv->seek_dir, flv->packet_cnt);

    return PARSER_OK;
}
