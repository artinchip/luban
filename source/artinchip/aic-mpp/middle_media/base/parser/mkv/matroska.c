/*
 * Copyright (C) 2020-2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: <che.jiang@artinchip.com>
 * Desc: Matroska file demuxer
 */

#define LOG_TAG "mkv"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "matroska.h"
#include "aic_stream.h"
#include "avi.h"
#include "mpp_mem.h"
#include "mpp_log.h"
#include "mpp_list.h"
#include "aic_parser.h"
#include "aic_tag.h"

#define CHILD_OF(parent) { .def = { .n = parent } }
#define MKV_ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))

const int mkv_priv_mpeg4audio_sample_rates[16] = {
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025, 8000, 7350
};

static ebml_syntax g_ebml_syntax[3], g_matroska_segment[9],  g_matroska_track_video_color[15], g_matroska_track_video[19],
                  g_matroska_track[27], g_matroska_track_encoding[6], g_matroska_track_encodings[2],
                  g_matroska_track_combine_planes[2], g_matroska_track_operation[2], g_matroska_tracks[2],
                  g_matroska_attachments[2], g_matroska_chapter_entry[9], g_matroska_chapter[6], g_matroska_chapters[2],
                  g_matroska_index_entry[3], g_matroska_index[2], g_matroska_tag[3], g_matroska_tags[2], g_matroska_seekhead[2],
                  g_matroska_blockadditions[2], g_matroska_blockgroup[8], g_matroska_cluster_parsing[8];

static ebml_syntax g_ebml_header[] = {
    { EBML_ID_EBMLREADVERSION,    EBML_UINT, 0, mpp_offsetof(ebml_header, version),         { .u = EBML_VERSION } },
    { EBML_ID_EBMLMAXSIZELENGTH,  EBML_UINT, 0, mpp_offsetof(ebml_header, max_size),        { .u = 8 } },
    { EBML_ID_EBMLMAXIDLENGTH,    EBML_UINT, 0, mpp_offsetof(ebml_header, id_length),       { .u = 4 } },
    { EBML_ID_DOCTYPE,            EBML_STR,  0, mpp_offsetof(ebml_header, doctype),         { .s = "(none)" } },
    { EBML_ID_DOCTYPEREADVERSION, EBML_UINT, 0, mpp_offsetof(ebml_header, doctype_version), { .u = 1 } },
    { EBML_ID_EBMLVERSION,        EBML_NONE },
    { EBML_ID_DOCTYPEVERSION,     EBML_NONE },
    CHILD_OF(g_ebml_syntax)
};

static ebml_syntax g_ebml_syntax[] = {
    { EBML_ID_HEADER,      EBML_NEST, 0, 0, { .n = g_ebml_header } },
    { MATROSKA_ID_SEGMENT, EBML_STOP },
    { 0 }
};

static ebml_syntax g_matroska_info[] = {
    { MATROSKA_ID_TIMECODESCALE, EBML_UINT,  0, mpp_offsetof(struct matroska_demux_context, time_scale), { .u = 1000000 } },
    { MATROSKA_ID_DURATION,      EBML_FLOAT, 0, mpp_offsetof(struct matroska_demux_context, duration) },
    { MATROSKA_ID_TITLE,         EBML_UTF8,  0, mpp_offsetof(struct matroska_demux_context, title) },
    { MATROSKA_ID_WRITINGAPP,    EBML_NONE },
    { MATROSKA_ID_MUXINGAPP,     EBML_UTF8, 0, mpp_offsetof(struct matroska_demux_context, muxingapp) },
    { MATROSKA_ID_DATEUTC,       EBML_BIN,  0, mpp_offsetof(struct matroska_demux_context, date_utc) },
    { MATROSKA_ID_SEGMENTUID,    EBML_NONE },
    CHILD_OF(g_matroska_segment)
};

static ebml_syntax g_matroska_mastering_meta[] = {
    { MATROSKA_ID_VIDEOCOLOR_RX, EBML_FLOAT, 0, mpp_offsetof(matroska_mastering_meta, r_x), { .f=-1 } },
    { MATROSKA_ID_VIDEOCOLOR_RY, EBML_FLOAT, 0, mpp_offsetof(matroska_mastering_meta, r_y), { .f=-1 } },
    { MATROSKA_ID_VIDEOCOLOR_GX, EBML_FLOAT, 0, mpp_offsetof(matroska_mastering_meta, g_x), { .f=-1 } },
    { MATROSKA_ID_VIDEOCOLOR_GY, EBML_FLOAT, 0, mpp_offsetof(matroska_mastering_meta, g_y), { .f=-1 } },
    { MATROSKA_ID_VIDEOCOLOR_BX, EBML_FLOAT, 0, mpp_offsetof(matroska_mastering_meta, b_x), { .f=-1 } },
    { MATROSKA_ID_VIDEOCOLOR_BY, EBML_FLOAT, 0, mpp_offsetof(matroska_mastering_meta, b_y), { .f=-1 } },
    { MATROSKA_ID_VIDEOCOLOR_WHITEX, EBML_FLOAT, 0, mpp_offsetof(matroska_mastering_meta, white_x), { .f=-1 } },
    { MATROSKA_ID_VIDEOCOLOR_WHITEY, EBML_FLOAT, 0, mpp_offsetof(matroska_mastering_meta, white_y), { .f=-1 } },
    { MATROSKA_ID_VIDEOCOLOR_LUMINANCEMIN, EBML_FLOAT, 0, mpp_offsetof(matroska_mastering_meta, min_luminance), { .f=-1 } },
    { MATROSKA_ID_VIDEOCOLOR_LUMINANCEMAX, EBML_FLOAT, 0, mpp_offsetof(matroska_mastering_meta, max_luminance), { .f=-1 } },
    CHILD_OF(g_matroska_track_video_color)
};


static ebml_syntax g_matroska_track_video_color[] = {
    { MATROSKA_ID_VIDEOCOLORMATRIXCOEFF,      EBML_UINT, 0, mpp_offsetof(matroska_track_video_color, matrix_coefficients), { .u = 2 } },
    { MATROSKA_ID_VIDEOCOLORBITSPERCHANNEL,   EBML_UINT, 0, mpp_offsetof(matroska_track_video_color, bits_per_channel), { .u=0 } },
    { MATROSKA_ID_VIDEOCOLORCHROMASUBHORZ,    EBML_UINT, 0, mpp_offsetof(matroska_track_video_color, chroma_sub_horz), { .u=0 } },
    { MATROSKA_ID_VIDEOCOLORCHROMASUBVERT,    EBML_UINT, 0, mpp_offsetof(matroska_track_video_color, chroma_sub_vert), { .u=0 } },
    { MATROSKA_ID_VIDEOCOLORCBSUBHORZ,        EBML_UINT, 0, mpp_offsetof(matroska_track_video_color, cb_sub_horz), { .u=0 } },
    { MATROSKA_ID_VIDEOCOLORCBSUBVERT,        EBML_UINT, 0, mpp_offsetof(matroska_track_video_color, cb_sub_vert), { .u=0 } },
    { MATROSKA_ID_VIDEOCOLORCHROMASITINGHORZ, EBML_UINT, 0, mpp_offsetof(matroska_track_video_color, chroma_siting_horz), { .u = MATROSKA_COLOUR_CHROMASITINGHORZ_UNDETERMINED } },
    { MATROSKA_ID_VIDEOCOLORCHROMASITINGVERT, EBML_UINT, 0, mpp_offsetof(matroska_track_video_color, chroma_siting_vert), { .u = MATROSKA_COLOUR_CHROMASITINGVERT_UNDETERMINED } },
    { MATROSKA_ID_VIDEOCOLORRANGE,            EBML_UINT, 0, mpp_offsetof(matroska_track_video_color, range), { .u = 0 } },
    { MATROSKA_ID_VIDEOCOLORTRANSFERCHARACTERISTICS, EBML_UINT, 0, mpp_offsetof(matroska_track_video_color, transfer_characteristics), { .u = 2 } },
    { MATROSKA_ID_VIDEOCOLORPRIMARIES,        EBML_UINT, 0, mpp_offsetof(matroska_track_video_color, primaries), { .u = 2 } },
    { MATROSKA_ID_VIDEOCOLORMAXCLL,           EBML_UINT, 0, mpp_offsetof(matroska_track_video_color, max_cll), { .u=0 } },
    { MATROSKA_ID_VIDEOCOLORMAXFALL,          EBML_UINT, 0, mpp_offsetof(matroska_track_video_color, max_fall), { .u=0 } },
    { MATROSKA_ID_VIDEOCOLORMASTERINGMETA,    EBML_NEST, 0, mpp_offsetof(matroska_track_video_color, mastering_meta), { .n = g_matroska_mastering_meta } },
    CHILD_OF(g_matroska_track_video)
};

static ebml_syntax g_matroska_track_video_projection[] = {
    { MATROSKA_ID_VIDEOPROJECTIONTYPE,        EBML_UINT,  0, mpp_offsetof(matroska_track_video_projection, type), { .u = MATROSKA_VIDEO_PROJECTION_TYPE_RECTANGULAR } },
    { MATROSKA_ID_VIDEOPROJECTIONPRIVATE,     EBML_BIN,   0, mpp_offsetof(matroska_track_video_projection, private) },
    { MATROSKA_ID_VIDEOPROJECTIONPOSEYAW,     EBML_FLOAT, 0, mpp_offsetof(matroska_track_video_projection, yaw), { .f=0.0 } },
    { MATROSKA_ID_VIDEOPROJECTIONPOSEPITCH,   EBML_FLOAT, 0, mpp_offsetof(matroska_track_video_projection, pitch), { .f=0.0 } },
    { MATROSKA_ID_VIDEOPROJECTIONPOSEROLL,    EBML_FLOAT, 0, mpp_offsetof(matroska_track_video_projection, roll), { .f=0.0 } },
    CHILD_OF(g_matroska_track_video)
};

static ebml_syntax g_matroska_track_video[] = {
    { MATROSKA_ID_VIDEOFRAMERATE,      EBML_FLOAT, 0, mpp_offsetof(matroska_track_video, frame_rate) },
    { MATROSKA_ID_VIDEODISPLAYWIDTH,   EBML_UINT,  0, mpp_offsetof(matroska_track_video, display_width), { .u=-1 } },
    { MATROSKA_ID_VIDEODISPLAYHEIGHT,  EBML_UINT,  0, mpp_offsetof(matroska_track_video, display_height), { .u=-1 } },
    { MATROSKA_ID_VIDEOPIXELWIDTH,     EBML_UINT,  0, mpp_offsetof(matroska_track_video, pixel_width) },
    { MATROSKA_ID_VIDEOPIXELHEIGHT,    EBML_UINT,  0, mpp_offsetof(matroska_track_video, pixel_height) },
    { MATROSKA_ID_VIDEOCOLORSPACE,     EBML_BIN,   0, mpp_offsetof(matroska_track_video, color_space) },
    { MATROSKA_ID_VIDEOALPHAMODE,      EBML_UINT,  0, mpp_offsetof(matroska_track_video, alpha_mode) },
    { MATROSKA_ID_VIDEOCOLOR,          EBML_NEST,  sizeof(matroska_track_video_color), mpp_offsetof(matroska_track_video, color), { .n = g_matroska_track_video_color } },
    { MATROSKA_ID_VIDEOPROJECTION,     EBML_NEST,  0, mpp_offsetof(matroska_track_video, projection), { .n = g_matroska_track_video_projection } },
    { MATROSKA_ID_VIDEOPIXELCROPB,     EBML_NONE },
    { MATROSKA_ID_VIDEOPIXELCROPT,     EBML_NONE },
    { MATROSKA_ID_VIDEOPIXELCROPL,     EBML_NONE },
    { MATROSKA_ID_VIDEOPIXELCROPR,     EBML_NONE },
    { MATROSKA_ID_VIDEODISPLAYUNIT,    EBML_UINT,  0, mpp_offsetof(matroska_track_video, display_unit), { .u= MATROSKA_VIDEO_DISPLAYUNIT_PIXELS } },
    { MATROSKA_ID_VIDEOFLAGINTERLACED, EBML_UINT,  0, mpp_offsetof(matroska_track_video, interlaced),  { .u = MATROSKA_VIDEO_INTERLACE_FLAG_UNDETERMINED } },
    { MATROSKA_ID_VIDEOFIELDORDER,     EBML_UINT,  0, mpp_offsetof(matroska_track_video, field_order), { .u = MATROSKA_VIDEO_FIELDORDER_UNDETERMINED } },
    { MATROSKA_ID_VIDEOSTEREOMODE,     EBML_UINT,  0, mpp_offsetof(matroska_track_video, stereo_mode), { .u = MATROSKA_VIDEO_STEREOMODE_TYPE_NB } },
    { MATROSKA_ID_VIDEOASPECTRATIO,    EBML_NONE },
    CHILD_OF(g_matroska_track)
};

static ebml_syntax g_matroska_track_audio[] = {
    { MATROSKA_ID_AUDIOSAMPLINGFREQ,    EBML_FLOAT, 0, mpp_offsetof(matroska_track_audio, samplerate), { .f = 8000.0 } },
    { MATROSKA_ID_AUDIOOUTSAMPLINGFREQ, EBML_FLOAT, 0, mpp_offsetof(matroska_track_audio, out_samplerate) },
    { MATROSKA_ID_AUDIOBITDEPTH,        EBML_UINT,  0, mpp_offsetof(matroska_track_audio, bitdepth) },
    { MATROSKA_ID_AUDIOCHANNELS,        EBML_UINT,  0, mpp_offsetof(matroska_track_audio, channels),   { .u = 1 } },
    CHILD_OF(g_matroska_track)
};

static ebml_syntax g_matroska_track_encoding_compression[] = {
    { MATROSKA_ID_ENCODINGCOMPALGO,     EBML_UINT, 0, mpp_offsetof(matroska_track_compression, algo), { .u = 0 } },
    { MATROSKA_ID_ENCODINGCOMPSETTINGS, EBML_BIN,  0, mpp_offsetof(matroska_track_compression, settings) },
    CHILD_OF(g_matroska_track_encoding)
};

static ebml_syntax g_matroska_track_encoding_encryption[] = {
    { MATROSKA_ID_ENCODINGENCALGO,        EBML_UINT, 0, mpp_offsetof(matroska_track_encryption,algo), {.u = 0} },
    { MATROSKA_ID_ENCODINGENCKEYID,       EBML_BIN, 0, mpp_offsetof(matroska_track_encryption,key_id) },
    { MATROSKA_ID_ENCODINGENCAESSETTINGS, EBML_NONE },
    { MATROSKA_ID_ENCODINGSIGALGO,        EBML_NONE },
    { MATROSKA_ID_ENCODINGSIGHASHALGO,    EBML_NONE },
    { MATROSKA_ID_ENCODINGSIGKEYID,       EBML_NONE },
    { MATROSKA_ID_ENCODINGSIGNATURE,      EBML_NONE },
    CHILD_OF(g_matroska_track_encoding)
};

static ebml_syntax g_matroska_track_encoding[] = {
    { MATROSKA_ID_ENCODINGSCOPE,       EBML_UINT, 0, mpp_offsetof(matroska_track_encoding, scope),       { .u = 1 } },
    { MATROSKA_ID_ENCODINGTYPE,        EBML_UINT, 0, mpp_offsetof(matroska_track_encoding, type),        { .u = 0 } },
    { MATROSKA_ID_ENCODINGCOMPRESSION, EBML_NEST, 0, mpp_offsetof(matroska_track_encoding, compression), { .n = g_matroska_track_encoding_compression } },
    { MATROSKA_ID_ENCODINGENCRYPTION,  EBML_NEST, 0, mpp_offsetof(matroska_track_encoding, encryption),  { .n = g_matroska_track_encoding_encryption } },
    { MATROSKA_ID_ENCODINGORDER,       EBML_NONE },
    CHILD_OF(g_matroska_track_encodings)
};

static ebml_syntax g_matroska_track_encodings[] = {
    { MATROSKA_ID_TRACKCONTENTENCODING, EBML_NEST, sizeof(g_matroska_track_encoding), mpp_offsetof(matroska_track, encodings), { .n = g_matroska_track_encoding } },
    CHILD_OF(g_matroska_track)
};

static ebml_syntax g_matroska_track_plane[] = {
    { MATROSKA_ID_TRACKPLANEUID,  EBML_UINT, 0, mpp_offsetof(matroska_track_plane, uid) },
    { MATROSKA_ID_TRACKPLANETYPE, EBML_UINT, 0, mpp_offsetof(matroska_track_plane, type) },
    CHILD_OF(g_matroska_track_combine_planes)
};

static ebml_syntax g_matroska_track_combine_planes[] = {
    { MATROSKA_ID_TRACKPLANE, EBML_NEST, sizeof(matroska_track_plane), mpp_offsetof(matroska_track_operation,combine_planes), {.n = g_matroska_track_plane} },
    CHILD_OF(g_matroska_track_operation)
};

static ebml_syntax g_matroska_track_operation[] = {
    { MATROSKA_ID_TRACKCOMBINEPLANES, EBML_NEST, 0, 0, {.n = g_matroska_track_combine_planes} },
    CHILD_OF(g_matroska_track)
};

static ebml_syntax g_matroska_track[] = {
    { MATROSKA_ID_TRACKNUMBER,           EBML_UINT,  0, mpp_offsetof(matroska_track, num) },
    { MATROSKA_ID_TRACKNAME,             EBML_UTF8,  0, mpp_offsetof(matroska_track, name) },
    { MATROSKA_ID_TRACKUID,              EBML_UINT,  0, mpp_offsetof(matroska_track, uid) },
    { MATROSKA_ID_TRACKTYPE,             EBML_UINT,  0, mpp_offsetof(matroska_track, type) },
    { MATROSKA_ID_CODECID,               EBML_STR,   0, mpp_offsetof(matroska_track, codec_id) },
    { MATROSKA_ID_CODECPRIVATE,          EBML_BIN,   0, mpp_offsetof(matroska_track, codec_priv) },
    { MATROSKA_ID_CODECDELAY,            EBML_UINT,  0, mpp_offsetof(matroska_track, codec_delay) },
    { MATROSKA_ID_TRACKLANGUAGE,         EBML_STR,   0, mpp_offsetof(matroska_track, language),     { .s = "eng" } },
    { MATROSKA_ID_TRACKDEFAULTDURATION,  EBML_UINT,  0, mpp_offsetof(matroska_track, default_duration) },
    { MATROSKA_ID_TRACKTIMECODESCALE,    EBML_FLOAT, 0, mpp_offsetof(matroska_track, time_scale),   { .f = 1.0 } },
    { MATROSKA_ID_TRACKFLAGDEFAULT,      EBML_UINT,  0, mpp_offsetof(matroska_track, flag_default), { .u = 1 } },
    { MATROSKA_ID_TRACKFLAGFORCED,       EBML_UINT,  0, mpp_offsetof(matroska_track, flag_forced),  { .u = 0 } },
    { MATROSKA_ID_TRACKVIDEO,            EBML_NEST,  0, mpp_offsetof(matroska_track, video),        { .n = g_matroska_track_video } },
    { MATROSKA_ID_TRACKAUDIO,            EBML_NEST,  0, mpp_offsetof(matroska_track, audio),        { .n = g_matroska_track_audio } },
    { MATROSKA_ID_TRACKOPERATION,        EBML_NEST,  0, mpp_offsetof(matroska_track, operation),    { .n = g_matroska_track_operation } },
    { MATROSKA_ID_TRACKCONTENTENCODINGS, EBML_NEST,  0, 0,                                     { .n = g_matroska_track_encodings } },
    { MATROSKA_ID_TRACKMAXBLKADDID,      EBML_UINT,  0, mpp_offsetof(matroska_track, max_block_additional_id) },
    { MATROSKA_ID_SEEKPREROLL,           EBML_UINT,  0, mpp_offsetof(matroska_track, seek_preroll) },
    { MATROSKA_ID_TRACKFLAGENABLED,      EBML_NONE },
    { MATROSKA_ID_TRACKFLAGLACING,       EBML_NONE },
    { MATROSKA_ID_CODECNAME,             EBML_NONE },
    { MATROSKA_ID_CODECDECODEALL,        EBML_NONE },
    { MATROSKA_ID_CODECINFOURL,          EBML_NONE },
    { MATROSKA_ID_CODECDOWNLOADURL,      EBML_NONE },
    { MATROSKA_ID_TRACKMINCACHE,         EBML_NONE },
    { MATROSKA_ID_TRACKMAXCACHE,         EBML_NONE },
    CHILD_OF(g_matroska_tracks)
};

static ebml_syntax g_matroska_tracks[] = {
    { MATROSKA_ID_TRACKENTRY, EBML_NEST, sizeof(matroska_track), mpp_offsetof(struct matroska_demux_context, tracks), { .n = g_matroska_track } },
    CHILD_OF(g_matroska_segment)
};

static ebml_syntax g_matroska_attachment[] = {
    { MATROSKA_ID_FILEUID,      EBML_UINT, 0, mpp_offsetof(matroska_attachment, uid) },
    { MATROSKA_ID_FILENAME,     EBML_UTF8, 0, mpp_offsetof(matroska_attachment, filename) },
    { MATROSKA_ID_FILEMIMETYPE, EBML_STR,  0, mpp_offsetof(matroska_attachment, mime) },
    { MATROSKA_ID_FILEDATA,     EBML_BIN,  0, mpp_offsetof(matroska_attachment, bin) },
    { MATROSKA_ID_FILEDESC,     EBML_UTF8, 0, mpp_offsetof(matroska_attachment, description) },
    CHILD_OF(g_matroska_attachments)
};

static ebml_syntax g_matroska_attachments[] = {
    { MATROSKA_ID_ATTACHEDFILE, EBML_NEST, sizeof(matroska_attachment), mpp_offsetof(struct matroska_demux_context, attachments), { .n = g_matroska_attachment } },
    CHILD_OF(g_matroska_segment)
};

static ebml_syntax g_matroska_chapter_display[] = {
    { MATROSKA_ID_CHAPSTRING,  EBML_UTF8, 0, mpp_offsetof(matroska_chapter, title) },
    { MATROSKA_ID_CHAPLANG,    EBML_NONE },
    { MATROSKA_ID_CHAPCOUNTRY, EBML_NONE },
    CHILD_OF(g_matroska_chapter_entry)
};

static ebml_syntax g_matroska_chapter_entry[] = {
    { MATROSKA_ID_CHAPTERTIMESTART,   EBML_UINT, 0, mpp_offsetof(matroska_chapter, start), { .u = 0 } },
    { MATROSKA_ID_CHAPTERTIMEEND,     EBML_UINT, 0, mpp_offsetof(matroska_chapter, end),   { .u = 0 } },
    { MATROSKA_ID_CHAPTERUID,         EBML_UINT, 0, mpp_offsetof(matroska_chapter, uid) },
    { MATROSKA_ID_CHAPTERDISPLAY,     EBML_NEST, 0,                        0,         { .n = g_matroska_chapter_display } },
    { MATROSKA_ID_CHAPTERFLAGHIDDEN,  EBML_NONE },
    { MATROSKA_ID_CHAPTERFLAGENABLED, EBML_NONE },
    { MATROSKA_ID_CHAPTERPHYSEQUIV,   EBML_NONE },
    { MATROSKA_ID_CHAPTERATOM,        EBML_NONE },
    CHILD_OF(g_matroska_chapter)
};

static ebml_syntax g_matroska_chapter[] = {
    { MATROSKA_ID_CHAPTERATOM,        EBML_NEST, sizeof(matroska_chapter), mpp_offsetof(struct matroska_demux_context, chapters), { .n = g_matroska_chapter_entry } },
    { MATROSKA_ID_EDITIONUID,         EBML_NONE },
    { MATROSKA_ID_EDITIONFLAGHIDDEN,  EBML_NONE },
    { MATROSKA_ID_EDITIONFLAGDEFAULT, EBML_NONE },
    { MATROSKA_ID_EDITIONFLAGORDERED, EBML_NONE },
    CHILD_OF(g_matroska_chapters)
};

static ebml_syntax g_matroska_chapters[] = {
    { MATROSKA_ID_EDITIONENTRY, EBML_NEST, 0, 0, { .n = g_matroska_chapter } },
    CHILD_OF(g_matroska_segment)
};

static ebml_syntax g_matroska_index_pos[] = {
    { MATROSKA_ID_CUETRACK,           EBML_UINT, 0, mpp_offsetof(matroska_index_pos, track) },
    { MATROSKA_ID_CUECLUSTERPOSITION, EBML_UINT, 0, mpp_offsetof(matroska_index_pos, pos) },
    { MATROSKA_ID_CUERELATIVEPOSITION,EBML_NONE },
    { MATROSKA_ID_CUEDURATION,        EBML_NONE },
    { MATROSKA_ID_CUEBLOCKNUMBER,     EBML_NONE },
    CHILD_OF(g_matroska_index_entry)
};

static ebml_syntax g_matroska_index_entry[] = {
    { MATROSKA_ID_CUETIME,          EBML_UINT, 0,                        mpp_offsetof(matroska_index, time) },
    { MATROSKA_ID_CUETRACKPOSITION, EBML_NEST, sizeof(matroska_index_pos), mpp_offsetof(matroska_index, pos), { .n = g_matroska_index_pos } },
    CHILD_OF(g_matroska_index)
};

static ebml_syntax g_matroska_index[] = {
    { MATROSKA_ID_POINTENTRY, EBML_NEST, sizeof(matroska_index), mpp_offsetof(struct matroska_demux_context, index), { .n = g_matroska_index_entry } },
    CHILD_OF(g_matroska_segment)
};

static ebml_syntax g_matroska_simpletag[] = {
    { MATROSKA_ID_TAGNAME,        EBML_UTF8, 0,                   mpp_offsetof(matroska_tag, name) },
    { MATROSKA_ID_TAGSTRING,      EBML_UTF8, 0,                   mpp_offsetof(matroska_tag, string) },
    { MATROSKA_ID_TAGLANG,        EBML_STR,  0,                   mpp_offsetof(matroska_tag, lang), { .s = "und" } },
    { MATROSKA_ID_TAGDEFAULT,     EBML_UINT, 0,                   mpp_offsetof(matroska_tag, def) },
    { MATROSKA_ID_TAGDEFAULT_BUG, EBML_UINT, 0,                   mpp_offsetof(matroska_tag, def) },
    { MATROSKA_ID_SIMPLETAG,      EBML_NEST, sizeof(matroska_tag), mpp_offsetof(matroska_tag, sub),  { .n = g_matroska_simpletag } },
    CHILD_OF(g_matroska_tag)
};

static ebml_syntax g_matroska_tag_targets[] = {
    { MATROSKA_ID_TAGTARGETS_TYPE,       EBML_STR,  0, mpp_offsetof(matroska_tag_targets, type) },
    { MATROSKA_ID_TAGTARGETS_TYPEVALUE,  EBML_UINT, 0, mpp_offsetof(matroska_tag_targets, typevalue), { .u = 50 } },
    { MATROSKA_ID_TAGTARGETS_TRACKUID,   EBML_UINT, 0, mpp_offsetof(matroska_tag_targets, trackuid) },
    { MATROSKA_ID_TAGTARGETS_CHAPTERUID, EBML_UINT, 0, mpp_offsetof(matroska_tag_targets, chapteruid) },
    { MATROSKA_ID_TAGTARGETS_ATTACHUID,  EBML_UINT, 0, mpp_offsetof(matroska_tag_targets, attachuid) },
    CHILD_OF(g_matroska_tag)
};

static ebml_syntax g_matroska_tag[] = {
    { MATROSKA_ID_SIMPLETAG,  EBML_NEST, sizeof(matroska_tag), mpp_offsetof(matroska_tags, tag),    { .n = g_matroska_simpletag } },
    { MATROSKA_ID_TAGTARGETS, EBML_NEST, 0,                   mpp_offsetof(matroska_tags, target), { .n = g_matroska_tag_targets } },
    CHILD_OF(g_matroska_tags)
};

static ebml_syntax g_matroska_tags[] = {
    { MATROSKA_ID_TAG, EBML_NEST, sizeof(matroska_tags), mpp_offsetof(struct matroska_demux_context, tags), { .n = g_matroska_tag } },
    CHILD_OF(g_matroska_segment)
};

static ebml_syntax g_matroska_seekhead_entry[] = {
    { MATROSKA_ID_SEEKID,       EBML_UINT, 0, mpp_offsetof(matroska_seek_head, id) },
    { MATROSKA_ID_SEEKPOSITION, EBML_UINT, 0, mpp_offsetof(matroska_seek_head, pos), { .u = -1 } },
    CHILD_OF(g_matroska_seekhead)
};

static ebml_syntax g_matroska_seekhead[] = {
    { MATROSKA_ID_SEEKENTRY, EBML_NEST, sizeof(matroska_seek_head), mpp_offsetof(struct matroska_demux_context, seekhead), { .n = g_matroska_seekhead_entry } },
    CHILD_OF(g_matroska_segment)
};

static ebml_syntax g_matroska_segment[] = {
    { MATROSKA_ID_CLUSTER,     EBML_STOP },
    { MATROSKA_ID_INFO,        EBML_LEVEL1, 0, 0, { .n = g_matroska_info } },
    { MATROSKA_ID_TRACKS,      EBML_LEVEL1, 0, 0, { .n = g_matroska_tracks } },
    { MATROSKA_ID_ATTACHMENTS, EBML_LEVEL1, 0, 0, { .n = g_matroska_attachments } },
    { MATROSKA_ID_CHAPTERS,    EBML_LEVEL1, 0, 0, { .n = g_matroska_chapters } },
    { MATROSKA_ID_CUES,        EBML_LEVEL1, 0, 0, { .n = g_matroska_index } },
    { MATROSKA_ID_TAGS,        EBML_LEVEL1, 0, 0, { .n = g_matroska_tags } },
    { MATROSKA_ID_SEEKHEAD,    EBML_LEVEL1, 0, 0, { .n = g_matroska_seekhead } },
    { 0 }   /* We don't want to go back to level 0, so don't add the parent. */
};

static ebml_syntax g_matroska_segments[] = {
    { MATROSKA_ID_SEGMENT, EBML_NEST, 0, 0, { .n = g_matroska_segment } },
    { 0 }
};

static ebml_syntax g_matroska_blockmore[] = {
    { MATROSKA_ID_BLOCKADDID,      EBML_UINT, 0, mpp_offsetof(matroska_block,additional_id), { .u = 1 } },
    { MATROSKA_ID_BLOCKADDITIONAL, EBML_BIN,  0, mpp_offsetof(matroska_block,additional) },
    CHILD_OF(g_matroska_blockadditions)
};

static ebml_syntax g_matroska_blockadditions[] = {
    { MATROSKA_ID_BLOCKMORE, EBML_NEST, 0, 0, {.n = g_matroska_blockmore} },
    CHILD_OF(g_matroska_blockgroup)
};

static ebml_syntax g_matroska_blockgroup[] = {
    { MATROSKA_ID_BLOCK,          EBML_BIN,  0, mpp_offsetof(matroska_block, bin) },
    { MATROSKA_ID_BLOCKADDITIONS, EBML_NEST, 0, 0, { .n = g_matroska_blockadditions } },
    { MATROSKA_ID_BLOCKDURATION,  EBML_UINT, 0, mpp_offsetof(matroska_block, duration) },
    { MATROSKA_ID_DISCARDPADDING, EBML_SINT, 0, mpp_offsetof(matroska_block, discard_padding) },
    { MATROSKA_ID_BLOCKREFERENCE, EBML_SINT, 0, mpp_offsetof(matroska_block, reference), { .i = INT64_MIN } },
    { MATROSKA_ID_CODECSTATE,     EBML_NONE },
    { 1, EBML_UINT, 0, mpp_offsetof(matroska_block, non_simple), { .u = 1 } },
    CHILD_OF(g_matroska_cluster_parsing)
};

// The following array contains SimpleBlock and BlockGroup twice
// in order to reuse the other values for matroska_cluster_enter.
static ebml_syntax g_matroska_cluster_parsing[] = {
    { MATROSKA_ID_SIMPLEBLOCK,     EBML_STREAM,  0, mpp_offsetof(matroska_block, bin) },
    { MATROSKA_ID_BLOCKGROUP,      EBML_NEST, 0, 0, { .n = g_matroska_blockgroup } },
    { MATROSKA_ID_CLUSTERTIMECODE, EBML_UINT, 0, mpp_offsetof(matroska_cluster, timecode) },
    { MATROSKA_ID_SIMPLEBLOCK,     EBML_STOP },
    { MATROSKA_ID_BLOCKGROUP,      EBML_STOP },
    { MATROSKA_ID_CLUSTERPOSITION, EBML_NONE },
    { MATROSKA_ID_CLUSTERPREVSIZE, EBML_NONE },
    CHILD_OF(g_matroska_segment)
};

static ebml_syntax g_matroska_cluster_enter[] = {
    { MATROSKA_ID_CLUSTER,     EBML_NEST, 0, 0, { .n = &g_matroska_cluster_parsing[2] } },
    { 0 }
};
#undef CHILD_OF

static const char *const g_matroska_doctypes[] = { "matroska", "webm" };

const uint8_t log2_tab[256] = {
    0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};

static inline int log2_c(unsigned int v)
{
    int n = 0;
    if (v & 0xffff0000) {
        v >>= 16;
        n += 16;
    }
    if (v & 0xff00) {
        v >>= 8;
        n += 8;
    }
    n += log2_tab[v];

    return n;
}

static inline int sign_extend(int val, unsigned bits)
{
    unsigned shift = 8 * sizeof(int) - bits;
    union { unsigned u; int s; } v = { (unsigned) val << shift };
    return v.s >> shift;
}

/*
 * This function prepares the status for parsing of level 1 elements.
 */
static int matroska_reset_status(struct aic_stream *c,
                                 struct matroska_demux_context *matroska,
                                 uint32_t id, int64_t position)
{
    if (position >= 0) {
        int64_t err = aic_stream_seek(c, position, SEEK_SET);
        if (err < 0)
            return err;
    }

    matroska->current_id    = id;
    matroska->num_levels    = 1;
    matroska->unknown_count = 0;
    matroska->resync_pos = aic_stream_tell(c);
    if (id)
        matroska->resync_pos -= (log2_c(id) + 7) / 8;

    return 0;
}

static int matroska_resync(struct aic_stream *c, struct matroska_demux_context *matroska, int64_t last_pos)
{
    struct aic_stream *pb = c;
    uint32_t id;
    int64_t size = aic_stream_size(pb);

    /* Try to seek to the last position to resync from. If this doesn't work,
     * we resync from the earliest position available: The start of the buffer. */
    if (last_pos < aic_stream_tell(pb) && aic_stream_seek(pb, last_pos + 1, SEEK_SET) < 0) {
        logw("Seek to desired resync point failed. Seeking to "
               "earliest point available instead.\n");
        aic_stream_seek(pb, last_pos + 1, SEEK_SET);
    }

    id = aic_stream_rb32(pb);

    // try to find a toplevel element
    while (aic_stream_tell(pb) < size) {
        if (id == MATROSKA_ID_INFO     || id == MATROSKA_ID_TRACKS      ||
            id == MATROSKA_ID_CUES     || id == MATROSKA_ID_TAGS        ||
            id == MATROSKA_ID_SEEKHEAD || id == MATROSKA_ID_ATTACHMENTS ||
            id == MATROSKA_ID_CLUSTER  || id == MATROSKA_ID_CHAPTERS) {
            /* Prepare the context for parsing of a level 1 element. */
            matroska_reset_status(pb, matroska, id, -1);
            /* Given that we are here means that an error has occurred,
             * so treat the segment as unknown length in order not to
             * discard valid data that happens to be beyond the designated
             * end of the segment. */
            matroska->levels[0].length = EBML_UNKNOWN_LENGTH;
            return 0;
        }
        id = (id << 8) | aic_stream_r8(pb);
    }

    matroska->done = 1;
    return PARSER_EOS;
}

/*
 * Read: an "EBML number", which is defined as a variable-length
 * array of bytes. The first byte indicates the length by giving a
 * number of 0-bits followed by a one. The position of the first
 * "one" bit inside the first byte indicates the length of this
 * number.
 * Returns: number of bytes read, < 0 on error
 */
static int ebml_read_num(struct matroska_demux_context *matroska, struct aic_stream *pb,
                         int max_size, uint64_t *number, int eof_forbidden)
{
    int read, n = 1;
    uint64_t total;
    int64_t pos;

    /* The first byte tells us the length in bytes - except when it is zero. */
    /* e.g.: EBML Header tatal = 0x1a[0001 1010], ID len = 4*/
    total = aic_stream_r8(pb);

    /* get the length of the EBML number, e.g.: EBML Header read = 4 */
    read = 8 - log2_tab[total];

    if (!total || read > max_size) {
        pos = aic_stream_tell(pb) - 1;
        if (!total) {
            loge("0x00 at pos %"PRId64" (0x%"PRIx64") invalid as first byte "
                   "of an EBML number\n", pos, pos);
        } else {
            loge("Length %d indicated by an EBML number's first byte 0x%02x "
                   "at pos %"PRId64" (0x%"PRIx64") exceeds max length %d.\n",
                   read, (uint8_t) total, pos, pos, max_size);
        }
        return PARSER_ERROR;
    }

    /* read out length  Example: EBML Header total = 0xa*/
    total ^= 1 << log2_tab[total];

    /* e.g. EBML Header total = 0x0a45dfa3*/
    while (n++ < read)
        total = (total << 8) | aic_stream_r8(pb);

    *number = total;

    return read;
}

/**
 * Read a EBML length value.
 * This needs special handling for the "unknown length" case which has multiple
 * encodings.
 */
static int ebml_read_length(struct matroska_demux_context *matroska, struct aic_stream *pb,
                            uint64_t *number)
{
    int res = ebml_read_num(matroska, pb, 8, number, 1);
    if (res > 0 && *number + 1 == 1ULL << (7 * res))
        *number = EBML_UNKNOWN_LENGTH;
    return res;
}

/*
 * Read the next element as an unsigned int.
 * Returns NEEDS_CHECKING.
 */
static int ebml_read_uint(struct aic_stream *pb, int size, uint64_t *num)
{
    int n = 0;

    /* big-endian ordering; build up number */
    *num = 0;
    while (n++ < size)
        *num = (*num << 8) | aic_stream_r8(pb);

    return NEEDS_CHECKING;
}

/*
 * Read the next element as a signed int.
 * Returns NEEDS_CHECKING.
 */
static int ebml_read_sint(struct aic_stream *pb, int size, int64_t *num)
{
    int n = 1;

    if (size == 0) {
        *num = 0;
    } else {
        *num = sign_extend(aic_stream_r8(pb), 8);

        /* big-endian ordering; build up number */
        while (n++ < size)
            *num = ((uint64_t)*num << 8) | aic_stream_r8(pb);
    }

    return NEEDS_CHECKING;
}

/*
 * Read the next element as a float.
 * Returns NEEDS_CHECKING or < 0 on obvious failure.
 */
static int ebml_read_float(struct aic_stream *pb, int size, double *num)
{
    if (size == 0)
        *num = 0;
    else if (size == 4)
        *num = (float)aic_stream_rb32(pb);
    else if (size == 8)
        *num = (double)aic_stream_rb64(pb);
    else
        return PARSER_ERROR;

    return NEEDS_CHECKING;
}

/*
 * Read the next element as an ASCII string.
 * 0 is success, < 0 or NEEDS_CHECKING is failure.
 */
static int ebml_read_ascii(struct aic_stream *pb, int size, char **str)
{
    char *res;
    int ret;

    /* EBML strings are usually not 0-terminated, so we allocate one
     * byte more, read the string and NULL-terminate it ourselves. */
    if (!(res = malloc(size + 1)))
        return PARSER_NOMEM;
    if ((ret = aic_stream_read(pb, (uint8_t *) res, size)) != size) {
        free(res);
        return ret < 0 ? ret : NEEDS_CHECKING;
    }
    (res)[size] = '\0';
    free(*str);
    *str = res;

    return 0;
}

/*
 * Read the next element as binary data.
 * 0 is success, < 0 or NEEDS_CHECKING is failure.
 */
static int ebml_read_binary(struct aic_stream *pb, int length,
                            int64_t pos, ebml_bin *bin)
{
    int ret;
    bin->data = realloc(bin->data, length + 64);
    memset(bin->data + length, 0, 64);

    bin->size = length;
    bin->pos  = pos;

    if ((ret = aic_stream_read(pb, bin->data, length)) != length) {
        free(bin->data);
        bin->data = NULL;
        bin->size = 0;
        return ret < 0 ? ret : NEEDS_CHECKING;
    }

    return 0;
}

static int ebml_peek_stream(int length, int64_t pos, ebml_bin *bin)
{
    bin->size = length;
    bin->pos  = pos;
    return 0;
}


/*
 * Read the next element, but only the header. The contents
 * are supposed to be sub-elements which can be read separately.
 * 0 is success, < 0 is failure.
 */
static int ebml_read_master(struct matroska_demux_context *matroska,
                            uint64_t length, int64_t pos)
{
    matroska_level *level;

    if (matroska->num_levels >= EBML_MAX_DEPTH) {
        loge("File moves beyond max. allowed depth (%d)\n", EBML_MAX_DEPTH);
        return PARSER_ERROR;
    }

    level         = &matroska->levels[matroska->num_levels++];
    level->start  = pos;
    level->length = length;

    return 0;
}


/*
 * Read a signed "EBML number"
 * Return: number of bytes processed, < 0 on error
 */
static int matroska_ebmlnum_sint(struct matroska_demux_context *matroska,
                                 struct aic_stream *pb, int64_t *num)
{
    uint64_t unum;
    int res;

    /* read as unsigned number first */
    if ((res = ebml_read_num(matroska, pb, 8, &unum, 1)) <= 0)
        return res;

    /* make signed (weird way) */
    *num = unum - ((1LL << (7 * res - 1)) - 1);

    return res;
}



static ebml_syntax *ebml_parse_id(ebml_syntax *syntax, uint32_t id)
{
    int i;

    // Whoever touches this should be aware of the duplication
    // existing in matroska_cluster_parsing.
    for (i = 0; syntax[i].id; i++)
        if (id == syntax[i].id)
            break;

    return &syntax[i];
}

static int ebml_parse(struct matroska_demux_context *matroska, struct aic_stream *c,
                      ebml_syntax *syntax, void *data);

static int is_ebml_id_valid(uint32_t id)
{
    // Due to endian nonsense in Matroska, the highest byte with any bits set
    // will contain the leading length bit. This bit in turn identifies the
    // total byte length of the element by its position within the byte.
    unsigned int bits = log2_c(id);
    return id && (bits + 7) / 8 ==  (8 - bits % 8);
}

/*
 * Allocate and return the entry for the level1 element with the given ID. If
 * an entry already exists, return the existing entry.
 */
static matroska_level1_element *matroska_find_level1_elem(struct matroska_demux_context *matroska,
                                                        uint32_t id, int64_t pos)
{
    int i;
    matroska_level1_element *elem;

    if (!is_ebml_id_valid(id))
        return NULL;

    // Some files link to all clusters; useless.
    if (id == MATROSKA_ID_CLUSTER)
        return NULL;

    // There can be multiple SeekHeads and Tags.
    for (i = 0; i < matroska->num_level1_elems; i++) {
        if (matroska->level1_elems[i].id == id) {
            if (matroska->level1_elems[i].pos == pos ||
                (id != MATROSKA_ID_SEEKHEAD && id != MATROSKA_ID_TAGS))
                return &matroska->level1_elems[i];
        }
    }

    // Only a completely broken file would have more elements.
    if (matroska->num_level1_elems >= MKV_ARRAY_ELEMS(matroska->level1_elems)) {
        loge("Too many level1 elements.\n");
        return NULL;
    }

    elem = &matroska->level1_elems[matroska->num_level1_elems++];
    *elem = (matroska_level1_element){.id = id};

    return elem;
}

static int ebml_parse_nest(struct matroska_demux_context *matroska, struct aic_stream *c,
                           ebml_syntax *syntax, void *data)
{
    int res, i;

    if (data) {
        for (i = 0; syntax[i].id; i++)

            logd("i:%d type:%d, data_offset:%d!!!", i, syntax[i].type, (int)syntax[i].data_offset);

            switch (syntax[i].type) {
            case EBML_UINT:
                *(uint64_t *) ((char *) data + syntax[i].data_offset) = syntax[i].def.u;
                break;
            case EBML_SINT:
                *(int64_t *) ((char *) data + syntax[i].data_offset) = syntax[i].def.i;
                break;
            case EBML_FLOAT:
                *(double *) ((char *) data + syntax[i].data_offset) = syntax[i].def.f;
                break;
            case EBML_STR:
            case EBML_UTF8:
                // the default may be NULL
                if (syntax[i].def.s) {
                    uint8_t **dst = (uint8_t **) ((uint8_t *) data + syntax[i].data_offset);

                    logi("malloc for nest str %d!!!", (int)strlen(syntax[i].def.s) + 1);
                    *dst = realloc(NULL, strlen(syntax[i].def.s) + 1);
                    if (*dst) {
                        memcpy(*dst, syntax[i].def.s, strlen(syntax[i].def.s) + 1);
                    } else {
                        return PARSER_NOMEM;
                    }
                }
                break;
            default:
                logd("unknown type %d", syntax[i].type);
                break;
            }

        if (!matroska->levels[matroska->num_levels - 1].length) {
            matroska->num_levels--;
            return 0;
        }
    }

    do {
        res = ebml_parse(matroska, c, syntax, data);
    } while (!res);

    return res == LEVEL_ENDED ? 0 : res;
}


static int ebml_parse(struct matroska_demux_context *matroska, struct aic_stream *c,
                      ebml_syntax *syntax, void *data)
{
    static const uint64_t max_lengths[EBML_TYPE_COUNT] = {
        // Forbid unknown-length EBML_NONE elements.
        [EBML_NONE]  = EBML_UNKNOWN_LENGTH - 1,
        [EBML_UINT]  = 8,
        [EBML_SINT]  = 8,
        [EBML_FLOAT] = 8,
        // max. 16 MB for strings
        [EBML_STR]   = 0x1000000,
        [EBML_UTF8]  = 0x1000000,
        // max. 256 MB for binary data
        [EBML_BIN]   = 0x10000000,
        // stream only for peek
        [EBML_STREAM]   = 0x1000000,
    };
    struct aic_stream *pb = c;
    uint32_t id;
    uint64_t length;
    int64_t pos = aic_stream_tell(pb), pos_alt;
    int res = 0, update_pos = 1, level_check = 0;
    matroska_level1_element *level1_elem;
    matroska_level *level = matroska->num_levels ? &matroska->levels[matroska->num_levels - 1] : NULL;

    /*curent level data overange need to back to pre level*/
    if (matroska->num_levels > 0 && level != NULL) {
        if (pos == level->start + level->length) {
            level_check = LEVEL_ENDED;
            goto level_check;
        }
    }

    if (!matroska->current_id) {
        uint64_t temp_id;
        res = ebml_read_num(matroska, pb, 4, &temp_id, 0);
        if (res < 0) {
            if (res == PARSER_EOS) {
                if (matroska->is_live)
                    // in live mode, finish parsing if EOF is reached.
                    return 1;
                if (level && pos == aic_stream_tell(pb)) {
                    if (level->length == EBML_UNKNOWN_LENGTH) {
                        // Unknown-length levels automatically end at EOF.
                        loge("unknown length levels automatically end at EOF");
                        matroska->num_levels--;
                        return LEVEL_ENDED;
                    } else {
                        loge( "File ended prematurely "
                               "at pos. %"PRIu64" (0x%"PRIx64")\n", pos, pos);
                    }
                }
            }
            return res;
        }
        matroska->current_id = temp_id | 1 << (7 * res);
        pos_alt = pos + res;
    } else {
        pos_alt = pos;
        pos    -= (log2_c(matroska->current_id) + 7) / 8;
    }

    id = matroska->current_id;

    syntax = ebml_parse_id(syntax, id);
    if (!syntax->id && id != EBML_ID_VOID && id != EBML_ID_CRC32) {
        if (level && level->length == EBML_UNKNOWN_LENGTH) {
            // Unknown-length levels end when an element from an upper level
            // in the hierarchy is encountered.
            while (syntax->def.n) {
                syntax = ebml_parse_id(syntax->def.n, id);
                if (syntax->id) {
                    loge("unknown syntax id %u automatically end at EOF", syntax->id);
                    matroska->num_levels--;
                    return LEVEL_ENDED;
                }
            };
        }

        loge("Unknown entry id 0x%"PRIX32" at pos.0x%"PRIx64", "
            "synatx id 0x%"PRIX32"\n", id, pos, syntax->id);
        update_pos = 0; /* Don't update resync_pos as an error might have happened. */
    }

    if (data) {
        data = (char *) data + syntax->data_offset;
        if (syntax->list_elem_size) {
            ebml_list *list = data;
            void *newelem;

            if ((unsigned)list->nb_elem + 1 >= UINT_MAX / syntax->list_elem_size)
                return PARSER_NOMEM;
            newelem = realloc(list->elem, (list->nb_elem + 1) * syntax->list_elem_size);
            if (!newelem)
                return PARSER_NOMEM;
            list->elem = newelem;
            data = (char *) list->elem + list->nb_elem * syntax->list_elem_size;
            memset(data, 0, syntax->list_elem_size);
            list->nb_elem++;
        }
    }

    if (syntax->type != EBML_STOP) {
        matroska->current_id = 0;
        if ((res = ebml_read_length(matroska, pb, &length)) < 0)
            return res;

        pos_alt += res;

        if (matroska->num_levels > 0) {
            if (length != EBML_UNKNOWN_LENGTH &&
                level->length != EBML_UNKNOWN_LENGTH) {
                uint64_t elem_end = pos_alt + length;
                uint64_t level_end = level->start + level->length;

                if (elem_end < level_end) {
                    level_check = 0;
                } else if (elem_end == level_end) {
                    logd("elem_end = level_end(0x%"PRIx64")", elem_end);
                    level_check = LEVEL_ENDED;
                } else {
                    loge("id: 0x%x, pos_alt 0x%"PRIx64" length 0x%"PRIx64","
                         " level: start 0x%"PRIx64" length 0x%"PRIx64"",
                         id, pos_alt, length, level->start, level->length);
                    loge("Element at 0x%"PRIx64" ending at 0x%"PRIx64" exceeds "
                           "containing master element ending at 0x%"PRIx64"\n",
                           pos, elem_end, level_end);
                    return PARSER_ERROR;
                }
            } else if (length != EBML_UNKNOWN_LENGTH) {
                level_check = 0;
            } else if (level->length != EBML_UNKNOWN_LENGTH) {
                loge( "Unknown-sized element "
                       "at 0x%"PRIx64" inside parent with finite size\n", pos);
                return PARSER_ERROR;
            } else {
                level_check = 0;
                if (id != MATROSKA_ID_CLUSTER && (syntax->type == EBML_LEVEL1
                                              ||  syntax->type == EBML_NEST)) {
                    // According to the current specifications only clusters and
                    // segments are allowed to be unknown-length. We also accept
                    // other unknown-length master elements.
                    logw("Found unknown-length element 0x%"PRIX32" other than "
                           "a cluster at 0x%"PRIx64". Spec-incompliant, but "
                           "parsing will nevertheless be attempted.\n", id, pos);
                    update_pos = -1;
                }
            }
        } else {
            level_check = 0;
        }

        if (max_lengths[syntax->type] && length > max_lengths[syntax->type]) {
            if (length != EBML_UNKNOWN_LENGTH) {
                loge("Invalid length 0x%"PRIx64" > 0x%"PRIx64" for element "
                       "with ID 0x%"PRIX32" at 0x%"PRIx64"\n",
                       length, max_lengths[syntax->type], id, pos);
            } else if (syntax->type != EBML_NONE) {
                loge("Element with ID 0x%"PRIX32" at pos. 0x%"PRIx64" has "
                       "unknown length, yet the length of an element of its "
                       "type must be known.\n", id, pos);
            } else {
                loge("Found unknown-length element with ID 0x%"PRIX32" at "
                       "pos. 0x%"PRIx64" for which no syntax for parsing is "
                       "available.\n", id, pos);
            }
            return PARSER_ERROR;
        }

        if (update_pos > 0) {
            // We have found an element that is allowed at this place
            // in the hierarchy and it passed all checks, so treat the beginning
            // of the element as the "last known good" position.
            matroska->resync_pos = pos;
        }

        if (!data && length != EBML_UNKNOWN_LENGTH)
            goto skip;
    }

    switch (syntax->type) {
    case EBML_UINT:
        res = ebml_read_uint(pb, length, data);
        break;
    case EBML_SINT:
        res = ebml_read_sint(pb, length, data);
        break;
    case EBML_FLOAT:
        res = ebml_read_float(pb, length, data);
        break;
    case EBML_STR:
    case EBML_UTF8:
        res = ebml_read_ascii(pb, length, data);
        break;
    case EBML_BIN:
        res = ebml_read_binary(pb, length, pos_alt, data);
        break;
    case EBML_STREAM:
        res = ebml_peek_stream(length, pos_alt, data);
        break;
    case EBML_LEVEL1:
    case EBML_NEST:
        if ((res = ebml_read_master(matroska, length, pos_alt)) < 0)
            return res;
        if (id == MATROSKA_ID_SEGMENT)
            matroska->segment_start = pos_alt;
        if (id == MATROSKA_ID_CUES)
            matroska->cues_parsing_deferred = 0;
        if (syntax->type == EBML_LEVEL1 &&
            (level1_elem = matroska_find_level1_elem(matroska, syntax->id, pos))) {
            if (!level1_elem->pos) {
                // Zero is not a valid position for a level 1 element.
                level1_elem->pos = pos;
            } else if (level1_elem->pos != pos) {
                loge("Duplicate element\n");
            }
            level1_elem->parsed = 1;
        }
        /*eg. nest is g_ebml_header*/
        res = ebml_parse_nest(matroska, c, syntax->def.n, data);
        if (res != 0)
            return res;
        break;
    case EBML_STOP:
        return 1;
    skip:
    default:
        if (length) {
            int64_t res2;
            if ((res2 = aic_stream_skip(pb, length - 1)) >= 0) {
                // aic_stream_skip might take us past EOF. We check for this
                // by skipping only length - 1 bytes, reading a byte and
                // checking the error flags. This is done in order to check
                // that the element has been properly skipped even when
                // no filesize (that ffio_limit relies on) is available.
                aic_stream_r8(pb);
                res = NEEDS_CHECKING;
            } else {
                res = res2;
            }
        } else {
            res = 0;
        }
    }
    if (res) {
        if (res == NEEDS_CHECKING) {
            goto level_check;
        }

        if (res == PARSER_NOMEM) {
            loge("Malloc failed\n");
        } else if (res == PARSER_EOS) {
            loge("File ended prematurely\n");
        } else if (res < 0) {
            loge("Invalid element\n");
        }

        return res;
    }

level_check:
    if (level_check == LEVEL_ENDED && matroska->num_levels) {
        level = &matroska->levels[matroska->num_levels - 1];
        pos   = aic_stream_tell(pb);

        // Given that pos >= level->start no check for
        // level->length != EBML_UNKNOWN_LENGTH is necessary.
        while (matroska->num_levels && pos == level->start + level->length) {
            matroska->num_levels--;
            level--;
        }
    }

    return level_check;
}

static void ebml_freep(void *arg)
{
    void *val;

    memcpy(&val, arg, sizeof(val));
    memcpy(arg, &(void *){ NULL }, sizeof(val));
    free(val);
}

static void ebml_free(ebml_syntax *syntax, void *data)
{
    int i, j;
    for (i = 0; syntax[i].id; i++) {
        void *data_off = (char *) data + syntax[i].data_offset;
        switch (syntax[i].type) {
        case EBML_STR:
        case EBML_UTF8:
            ebml_freep(data_off);
            break;
        case EBML_BIN:
            ebml_freep(&((ebml_bin *) data_off)->data);
            break;
        case EBML_LEVEL1:
        case EBML_NEST:
            if (syntax[i].list_elem_size) {
                ebml_list *list = data_off;
                char *ptr = list->elem;
                for (j = 0; j < list->nb_elem;
                     j++, ptr += syntax[i].list_elem_size)
                    ebml_free(syntax[i].def.n, ptr);
                ebml_freep(&list->elem);
                list->nb_elem = 0;
                list->alloc_elem_size = 0;
            } else {
                ebml_free(syntax[i].def.n, data_off);
            }

        default:
            break;
        }
    }
}

static matroska_track *matroska_find_track_by_num(struct matroska_demux_context *matroska,
                                                 uint64_t num)
{
    matroska_track *tracks = matroska->tracks.elem;
    int i;

    for (i = 0; i < matroska->tracks.nb_elem; i++) {
        logd("track[%d] number %"PRIu64"\n", i, tracks[i].num);
        if (tracks[i].num == num)
            return &tracks[i];
    }

    loge("Invalid track number %"PRIu64", nb_elem %d is euqal i %d\n",
        num, matroska->tracks.nb_elem, i);
    return NULL;
}


static int matroska_decode_buffer(uint8_t **buf, int *buf_size,
                                  matroska_track *track)
{
    matroska_track_encoding *encodings = track->encodings.elem;
    uint8_t *data = *buf;
    int isize = *buf_size;
    uint8_t *pkt_data = NULL;
    int pkt_size = isize;

    if (pkt_size >= 10000000U)
        return PARSER_ERROR;

    switch (encodings[0].compression.algo) {
    case MATROSKA_TRACK_ENCODING_COMP_HEADERSTRIP:
    {
        int header_size = encodings[0].compression.settings.size;
        uint8_t *header = encodings[0].compression.settings.data;

        if (header_size && !header) {
            loge("Compression size but no data in headerstrip\n");
            return -1;
        }

        if (!header_size)
            return 0;

        pkt_size = isize + header_size;
        pkt_data = malloc(pkt_size + 64);
        if (!pkt_data)
            return PARSER_NOMEM;

        memcpy(pkt_data, header, header_size);
        memcpy(pkt_data + header_size, data, isize);
        break;
    }

    default:
        return PARSER_ERROR;
    }

    memset(pkt_data + pkt_size, 0, 64);

    *buf      = pkt_data;
    *buf_size = pkt_size;
    return 0;
}

static int matroska_parse_seekhead_entry(struct matroska_demux_context *matroska,
                                         struct aic_stream *c, int64_t pos)
{
    uint32_t saved_id  = matroska->current_id;
    int64_t before_pos = aic_stream_tell(c);
    int ret = 0;

    /* seek */
    if (aic_stream_seek(c, pos, SEEK_SET) == pos) {
        /* We don't want to lose our seekhead level, so we add
         * a dummy. This is a crude hack. */
        if (matroska->num_levels == EBML_MAX_DEPTH) {
            logi("Max EBML element depth (%d) reached, "
                   "cannot parse further.\n", EBML_MAX_DEPTH);
            ret = PARSER_ERROR;
        } else {
            matroska->levels[matroska->num_levels] = (matroska_level) { 0, EBML_UNKNOWN_LENGTH };
            matroska->num_levels++;
            matroska->current_id                   = 0;

            ret = ebml_parse(matroska, c, g_matroska_segment, matroska);
            if (ret == LEVEL_ENDED) {
                /* This can only happen if the seek brought us beyond EOF. */
                ret = PARSER_EOS;
            }
        }
    }
    /* Seek back - notice that in all instances where this is used
     * it is safe to set the level to 1. */
    matroska_reset_status(c, matroska, saved_id, before_pos);

    return ret;
}

static void matroska_execute_seekhead(struct matroska_demux_context *matroska, struct aic_stream *c)
{
    ebml_list *seekhead_list = &matroska->seekhead;
    int i;

    for (i = 0; i < seekhead_list->nb_elem; i++) {
        matroska_seek_head *seekheads = seekhead_list->elem;
        uint32_t id = seekheads[i].id;
        int64_t pos = seekheads[i].pos + matroska->segment_start;
        matroska_level1_element *elem;

        if (id != seekheads[i].id || pos < matroska->segment_start)
            continue;

        elem = matroska_find_level1_elem(matroska, id, pos);
        if (!elem || elem->parsed)
            continue;

        elem->pos = pos;

        // defer cues parsing until we actually need cue data.
        if (id == MATROSKA_ID_CUES)
            continue;

        if (matroska_parse_seekhead_entry(matroska, c, pos) < 0) {
            // mark index as broken
            matroska->cues_parsing_deferred = -1;
            break;
        }

        elem->parsed = 1;
    }
}

static int matroska_add_index_entries(struct matroska_demux_context *matroska,
                                       struct matroska_stream_ctx *st)
{
    ebml_list *index_list;
    matroska_index *index;
    int i = 0, j = 0;
    uint32_t size = 0;

    index_list = &matroska->index;
    index      = index_list->elem;
    if (index_list->nb_elem < 2)
        return PARSER_ERROR;
    if (index[1].time > 1E14 / matroska->time_scale) {
        logw("Dropping apparently-broken index.\n");
        return PARSER_ERROR;
    }
    logd("index_list:nb_elem %d", index_list->nb_elem);
    st->nb_index_entries = index_list->nb_elem;
    size = st->nb_index_entries * sizeof(struct matroska_index_entry);
    st->index_entries = malloc(size);
    if (st->index_entries == NULL) {
        loge("malloc index_entries size %d failed\n", size);
        return PARSER_NOMEM;
    }
    for (i = 0; i < index_list->nb_elem; i++) {
        ebml_list *pos_list    = &index[i].pos;
        matroska_index_pos *pos = pos_list->elem;
        for (j = 0; j < pos_list->nb_elem; j++) {
            matroska_track *track = matroska_find_track_by_num(matroska, pos[j].track);
            if (track) {
                st->index_entries[i].pos = pos->pos + matroska->segment_start;
                st->index_entries[i].timestamp = index[i].time * 1000;
                st->index_entries[i].flags = 1;
            }
        }
    }

    return 0;
}

static void matroska_release_index_entries(struct matroska_stream_ctx *st)
{
    if (st->index_entries) {
        free(st->index_entries);
        st->index_entries = NULL;
    }
}

static void matroska_parse_cues(struct matroska_demux_context *matroska,
                                struct aic_stream *c, struct matroska_stream_ctx *st)
{
    int i;
    logd("num_level1_elems: %d", matroska->num_level1_elems);
    for (i = 0; i < matroska->num_level1_elems; i++) {
        matroska_level1_element *elem = &matroska->level1_elems[i];
        if (elem->id == MATROSKA_ID_CUES && !elem->parsed) {
            logd("elem: id 0x%x, pos 0x%lx", elem->id, elem->pos);
            if (matroska_parse_seekhead_entry(matroska, c, elem->pos) < 0)
                matroska->cues_parsing_deferred = -1;
            elem->parsed = 1;
            break;
        }
    }

    matroska_add_index_entries(matroska, st);
}


static int matroska_aac_profile(char *codec_id)
{
    static const char *const aac_profiles[] = { "MAIN", "LC", "SSR" };
    int profile;

    for (profile = 0; profile < MKV_ARRAY_ELEMS(aac_profiles); profile++)
        if (strstr(codec_id, aac_profiles[profile]))
            break;
    return profile + 1;
}

static int matroska_aac_sri(int samplerate)
{
    int sri;

    for (sri = 0; sri < MKV_ARRAY_ELEMS(mkv_priv_mpeg4audio_sample_rates); sri++)
        if (mkv_priv_mpeg4audio_sample_rates[sri] == samplerate)
            break;
    return sri;
}


static void mkv_stereo_mode_display_mul(int stereo_mode,
                                        int *h_width, int *h_height)
{
    switch (stereo_mode) {
        case MATROSKA_VIDEO_STEREOMODE_TYPE_MONO:
        case MATROSKA_VIDEO_STEREOMODE_TYPE_CHECKERBOARD_RL:
        case MATROSKA_VIDEO_STEREOMODE_TYPE_CHECKERBOARD_LR:
        case MATROSKA_VIDEO_STEREOMODE_TYPE_BOTH_EYES_BLOCK_RL:
        case MATROSKA_VIDEO_STEREOMODE_TYPE_BOTH_EYES_BLOCK_LR:
            break;
        case MATROSKA_VIDEO_STEREOMODE_TYPE_RIGHT_LEFT:
        case MATROSKA_VIDEO_STEREOMODE_TYPE_LEFT_RIGHT:
        case MATROSKA_VIDEO_STEREOMODE_TYPE_COL_INTERLEAVED_RL:
        case MATROSKA_VIDEO_STEREOMODE_TYPE_COL_INTERLEAVED_LR:
            *h_width = 2;
            break;
        case MATROSKA_VIDEO_STEREOMODE_TYPE_BOTTOM_TOP:
        case MATROSKA_VIDEO_STEREOMODE_TYPE_TOP_BOTTOM:
        case MATROSKA_VIDEO_STEREOMODE_TYPE_ROW_INTERLEAVED_RL:
        case MATROSKA_VIDEO_STEREOMODE_TYPE_ROW_INTERLEAVED_LR:
            *h_height = 2;
            break;
    }
}


static struct matroska_stream_ctx *matroska_new_stream(struct aic_matroska_parser *s)
{
    struct matroska_stream_ctx *sc;

    sc = (struct matroska_stream_ctx *)mpp_alloc(sizeof(struct matroska_stream_ctx));
    if (sc == NULL) {
        return NULL;
    }
    memset(sc, 0, sizeof(struct matroska_stream_ctx));
    sc->index = s->nb_streams;
    s->streams[s->nb_streams++] = sc;

    return sc;
}

static int matroska_parse_tracks(struct aic_matroska_parser *s)
{
    struct matroska_demux_context *matroska = &s->matroska_c;
    matroska_track *tracks = (matroska_track *)matroska->tracks.elem;
    struct matroska_stream_ctx *st = NULL;
    int i, j;
    int k;

    for (i = 0; i < matroska->tracks.nb_elem; i++) {
        matroska_track *track = (matroska_track *)&tracks[i];
        enum CodecID codec_id = CODEC_ID_NONE;
        ebml_list *encodings_list = &track->encodings;
        matroska_track_encoding *encodings = encodings_list->elem;
        uint8_t *extradata = NULL;
        int extradata_size = 0;
        int extradata_offset = 0;
        uint32_t fourcc = 0;
        //struct aic_stream b;
        //char* key_id_base64 = NULL;
        int bit_depth = -1;

        /* Apply some sanity checks. */
        if (track->type != MATROSKA_TRACK_TYPE_VIDEO &&
            track->type != MATROSKA_TRACK_TYPE_AUDIO &&
            track->type != MATROSKA_TRACK_TYPE_SUBTITLE &&
            track->type != MATROSKA_TRACK_TYPE_METADATA) {
            logi("Unknown or unsupported track type %"PRIu64"\n", track->type);
            continue;
        }
        if (!track->codec_id)
            continue;

        if (   (track->type == MATROSKA_TRACK_TYPE_AUDIO && track->codec_id[0] != 'A')
            || (track->type == MATROSKA_TRACK_TYPE_VIDEO && track->codec_id[0] != 'V')
            || (track->type == MATROSKA_TRACK_TYPE_SUBTITLE && track->codec_id[0] != 'D' && track->codec_id[0] != 'S')
            || (track->type == MATROSKA_TRACK_TYPE_METADATA && track->codec_id[0] != 'D' && track->codec_id[0] != 'S')) {
            logi("Inconsistent track type\n");
            continue;
        }

        if (track->audio.samplerate < 0 || track->audio.samplerate > UINT_MAX) {
            logw("Invalid sample rate %f, defaulting to 8000 instead.\n",
                   track->audio.samplerate);
            track->audio.samplerate = 8000;
        }

        if (track->type == MATROSKA_TRACK_TYPE_VIDEO) {
            if (!track->default_duration && track->video.frame_rate > 0) {
                double default_duration = 1000000000 / track->video.frame_rate;
                if (default_duration > UINT64_MAX || default_duration < 0) {
                    logw("Invalid frame rate %e. Cannot calculate default duration.\n",
                         track->video.frame_rate);
                } else {
                    track->default_duration = default_duration;
                }
            }
            if (track->video.display_width == -1)
                track->video.display_width = track->video.pixel_width;
            if (track->video.display_height == -1)
                track->video.display_height = track->video.pixel_height;
            if (track->video.color_space.size == 4)
                fourcc = AIC_RL32(track->video.color_space.data);
        } else if (track->type == MATROSKA_TRACK_TYPE_AUDIO) {
            if (!track->audio.out_samplerate)
                track->audio.out_samplerate = track->audio.samplerate;
        }
        if (encodings_list->nb_elem > 1) {
            loge("Multiple combined encodings not supported");
        } else if (encodings_list->nb_elem == 1) {
            if (encodings[0].type) {
                if (encodings[0].encryption.key_id.size > 0) {
                    /* Save the encryption key id to be stored later as a
                       metadata tag. */
                    loge("Unsupported encoding encryption");
                } else {
                    encodings[0].scope = 0;
                    loge("Unsupported encoding type");
                }
            } else if (
#if CONFIG_ZLIB
                encodings[0].compression.algo != MATROSKA_TRACK_ENCODING_COMP_ZLIB  &&
#endif
#if CONFIG_BZLIB
                encodings[0].compression.algo != MATROSKA_TRACK_ENCODING_COMP_BZLIB &&
#endif
#if CONFIG_LZO
                encodings[0].compression.algo != MATROSKA_TRACK_ENCODING_COMP_LZO   &&
#endif
                encodings[0].compression.algo != MATROSKA_TRACK_ENCODING_COMP_HEADERSTRIP) {
                encodings[0].scope = 0;
                loge("Unsupported encoding type");
            } else if (track->codec_priv.size && encodings[0].scope & 2) {
                uint8_t *codec_priv = track->codec_priv.data;
                int ret = matroska_decode_buffer(&track->codec_priv.data,
                                                 &track->codec_priv.size,
                                                 track);
                if (ret < 0) {
                    track->codec_priv.data = NULL;
                    track->codec_priv.size = 0;
                    loge("Failed to decode codec private data\n");
                }

                if (codec_priv != track->codec_priv.data) {
                    free(&track->codec_priv.data);

                    track->codec_priv.data = malloc(track->codec_priv.size + 64);
                    if (!track->codec_priv.data) {
                        track->codec_priv.size = 0;
                        return PARSER_NOMEM;
                    }

                }
            }
        }
        track->needs_decoding = encodings && !encodings[0].type &&
                                encodings[0].scope & 1          &&
                                (encodings[0].compression.algo !=
                                   MATROSKA_TRACK_ENCODING_COMP_HEADERSTRIP ||
                                 encodings[0].compression.settings.size);

        for (j = 0; mkv_codec_tags[j].id != CODEC_ID_NONE; j++) {
            if (!strncmp(mkv_codec_tags[j].str, track->codec_id,
                         strlen(mkv_codec_tags[j].str))) {
                codec_id = mkv_codec_tags[j].id;
                break;
            }
        }

        st = track->stream = matroska_new_stream(s);
        if (!st) {
            return PARSER_NOMEM;
        }

        if (codec_id == CODEC_ID_PCM_S16BE) {
            switch (track->audio.bitdepth) {
            case  8:
                codec_id = CODEC_ID_PCM_U8;
                break;
            case 24:
                codec_id = CODEC_ID_PCM_S24BE;
                break;
            case 32:
                codec_id = CODEC_ID_PCM_S32BE;
                break;
            }
        } else if (codec_id == CODEC_ID_PCM_S16LE) {
            switch (track->audio.bitdepth) {
            case  8:
                codec_id = CODEC_ID_PCM_U8;
                break;
            case 24:
                codec_id = CODEC_ID_PCM_S24LE;
                break;
            case 32:
                codec_id = CODEC_ID_PCM_S32LE;
                break;
            }
        } else if (codec_id == CODEC_ID_PCM_F32LE &&
                   track->audio.bitdepth == 64) {
            codec_id = CODEC_ID_PCM_F64LE;
        } else if (codec_id == CODEC_ID_AAC && !track->codec_priv.size) {
            int profile = matroska_aac_profile(track->codec_id);
            int sri     = matroska_aac_sri(track->audio.samplerate);
            extradata   = malloc(5 + 64);
            if (!extradata)
                return PARSER_NOMEM;
            extradata[0] = (profile << 3) | ((sri & 0x0E) >> 1);
            extradata[1] = ((sri & 0x01) << 7) | (track->audio.channels << 3);
            if (strstr(track->codec_id, "SBR")) {
                sri            = matroska_aac_sri(track->audio.out_samplerate);
                extradata[2]   = 0x56;
                extradata[3]   = 0xE5;
                extradata[4]   = 0x80 | (sri << 3);
                extradata_size = 5;
            } else {
                extradata_size = 2;
            }
        }
        track->codec_priv.size -= extradata_offset;

        if (codec_id == CODEC_ID_NONE)
            loge("Unknown/unsupported CodecID %d.\n", codec_id);

        st->codecpar.codec_id = codec_id;

        if (!st->codecpar.extradata) {
            if (extradata) {
                st->codecpar.extradata      = extradata;
                st->codecpar.extradata_size = extradata_size;
            } else if (track->codec_priv.data && track->codec_priv.size > 0) {
                st->codecpar.extradata = malloc(track->codec_priv.size + 64);
                if (NULL == st->codecpar.extradata)
                    return PARSER_NOMEM;
                st->codecpar.extradata_size = track->codec_priv.size;
                memset(st->codecpar.extradata + track->codec_priv.size, 0, 64);
                memcpy(st->codecpar.extradata,
                       track->codec_priv.data + extradata_offset,
                       track->codec_priv.size);
            }
        }

        if (track->type == MATROSKA_TRACK_TYPE_VIDEO) {
            matroska_track_plane *planes = track->operation.combine_planes.elem;
            int display_width_mul  = 1;
            int display_height_mul = 1;

            st->codecpar.codec_type = MPP_MEDIA_TYPE_VIDEO;
            st->codecpar.codec_tag  = fourcc;
            if (bit_depth >= 0)
                st->codecpar.bits_per_coded_sample = bit_depth;
            st->codecpar.width      = track->video.pixel_width;
            st->codecpar.height     = track->video.pixel_height;


            if (track->video.stereo_mode && track->video.stereo_mode < MATROSKA_VIDEO_STEREOMODE_TYPE_NB)
                mkv_stereo_mode_display_mul(track->video.stereo_mode, &display_width_mul, &display_height_mul);

            /* if we have virtual track, mark the real tracks */
            for (j=0; j < track->operation.combine_planes.nb_elem; j++) {
                if (planes[j].type >= MATROSKA_VIDEO_STEREO_PLANE_COUNT)
                    continue;

                for (k=0; k < matroska->tracks.nb_elem; k++)
                    if (planes[j].uid == tracks[k].uid) {
                        break;
                    }
            }

            st->duration = matroska->duration * matroska->time_scale * 1000 / MPP_TIME_BASE;
        } else if (track->type == MATROSKA_TRACK_TYPE_AUDIO) {
            st->codecpar.codec_type  = MPP_MEDIA_TYPE_AUDIO;
            st->codecpar.codec_tag   = fourcc;
            st->codecpar.sample_rate = track->audio.out_samplerate;
            st->codecpar.channels    = track->audio.channels;
            if (!st->codecpar.bits_per_coded_sample)
                st->codecpar.bits_per_coded_sample = track->audio.bitdepth;
            st->duration = matroska->duration * matroska->time_scale * 1000 / MPP_TIME_BASE;
        }
    }

    return 0;
}

static void matroska_parse_cluster_eos(struct matroska_demux_context *matroska)
{
    int i;
    for (i = 0; i < matroska->num_level1_elems; i++) {
        matroska_level1_element *elem = &matroska->level1_elems[i];
        if (elem->id == MATROSKA_ID_CUES && !elem->parsed) {
            printf("elem: id 0x%x, pos 0x%lx", elem->id, elem->pos);
            matroska->eos_pos = elem->pos;
            break;
        }
    }
}

int matroska_read_header(struct aic_matroska_parser *s)
{
    struct matroska_demux_context *matroska = &s->matroska_c;
    struct aic_stream *c   = s->stream;

    int64_t pos;
    ebml_header ebml = { 0 };
    int i, res = PARSER_OK;

    matroska->cues_parsing_deferred = 1;

    /* First read the EBML header. */
    if (ebml_parse(matroska, c, g_ebml_syntax, &ebml) || !ebml.doctype) {
        loge("EBML header parsing failed\n");
        ebml_free(g_ebml_syntax, &ebml);
        return PARSER_ERROR;
    }
    if (ebml.version         > EBML_VERSION      ||
        ebml.max_size        > sizeof(uint64_t)  ||
        ebml.id_length       > sizeof(uint32_t)  ||
        ebml.doctype_version > 3) {
        loge("EBML version %"PRIu64", doctype %s, doc version %"PRIu64,
            ebml.version, ebml.doctype, ebml.doctype_version);
        ebml_free(g_ebml_syntax, &ebml);
        return PARSER_ERROR;
    } else if (ebml.doctype_version == 3) {
        logw("EBML header using unsupported features\n"
               "(EBML version %"PRIu64", doctype %s, doc version %"PRIu64")\n",
               ebml.version, ebml.doctype, ebml.doctype_version);
    }

    for (i = 0; i < MKV_ARRAY_ELEMS(g_matroska_doctypes); i++)
        if (!strcmp(ebml.doctype, g_matroska_doctypes[i]))
            break;
    if (i >= MKV_ARRAY_ELEMS(g_matroska_doctypes)) {
        logw("Unknown EBML doctype '%s'\n", ebml.doctype);
    }
    ebml_free(g_ebml_syntax, &ebml);

    /* The next thing is a segment. */
    pos = aic_stream_tell(c);

    res = ebml_parse(matroska, c, g_matroska_segments, matroska);

    // Try resyncing until we find an EBML_STOP type element.
    while (res != 1) {
        res = matroska_resync(c, matroska, pos);
        if (res < 0)
            goto fail;
        pos = aic_stream_tell(c);
        res = ebml_parse(matroska, c, g_matroska_segment, matroska);
    }

    matroska_execute_seekhead(matroska, c);

    if (!matroska->time_scale)
        matroska->time_scale = 1000000;

    res = matroska_parse_tracks(s);
    if (res < 0)
        goto fail;

    /* Find the end cluster */
    matroska_parse_cluster_eos(matroska);


    return 0;
fail:
    matroska_read_close(s);
    return res;
}



static int matroska_parse_laces(struct matroska_demux_context *matroska, uint8_t **buf,
                                int size, int type, struct aic_stream *pb,
                                uint32_t lace_size[256], int *laces)
{
    int n;
    uint8_t *data = *buf;

    if (!type) {
        *laces    = 1;
        lace_size[0] = size;
        return 0;
    }

    if (size <= 0)
        return PARSER_ERROR;

    *laces = *data + 1;
    data  += 1;
    size  -= 1;

    switch (type) {
    case 0x1: /* Xiph lacing */
    {
        uint8_t temp;
        uint32_t total = 0;
        for (n = 0; n < *laces - 1; n++) {
            lace_size[n] = 0;

            do {
                if (size <= total)
                    return PARSER_ERROR;
                temp          = *data;
                total        += temp;
                lace_size[n] += temp;
                data         += 1;
                size         -= 1;
            } while (temp ==  0xff);
        }
        if (size < total)
            return PARSER_ERROR;

        lace_size[n] = size - total;
        break;
    }

    case 0x2: /* fixed-size lacing */
        if (size % (*laces))
            return PARSER_ERROR;
        for (n = 0; n < *laces; n++)
            lace_size[n] = size / *laces;
        break;

    case 0x3: /* EBML lacing */
    {
        uint64_t num;
        uint64_t total;
        int offset;

        aic_stream_skip(pb, 4);

        n = ebml_read_num(matroska, pb, 8, &num, 1);
        if (n < 0)
            return n;
        if (num > INT_MAX)
            return PARSER_ERROR;

        total = lace_size[0] = num;
        offset = n;
        for (n = 1; n < *laces - 1; n++) {
            int64_t snum;
            int r;
            r = matroska_ebmlnum_sint(matroska, pb, &snum);
            if (r < 0)
                return r;
            if (lace_size[n - 1] + snum > (uint64_t)INT_MAX)
                return PARSER_ERROR;

            lace_size[n] = lace_size[n - 1] + snum;
            total       += lace_size[n];
            offset      += r;
        }
        data += offset;
        size -= offset;
        if (size < total)
            return PARSER_ERROR;

        lace_size[*laces - 1] = size - total;
        break;
    }
    }

    *buf = data;

    return 0;
}


static int matroska_parse_block(struct matroska_demux_context *matroska, struct aic_stream *pb, uint8_t *data,
                                int size, int64_t pos, uint64_t cluster_time,
                                uint64_t block_duration, int is_keyframe,
                                uint8_t *additional, uint64_t additional_id, int additional_size,
                                int64_t cluster_pos, int64_t discard_padding, struct aic_parser_packet *pkt)
{
    uint64_t timecode = 0;
    matroska_track *track;
    int res = 0;
    struct matroska_stream_ctx *st = NULL;
    int16_t block_time;
    uint32_t lace_size[256];
    int n, flags, laces = 0;
    uint64_t num = 0;
    int trust_default_duration = 1;

    if ((n = ebml_read_num(matroska, pb, 8, &num, 1)) < 0)
        return n;

    size -= n;
    if (aic_stream_tell(pb) >= matroska->eos_pos) {
        matroska->done = 1;
        return PARSER_EOS;
    }
    logi("simple block: num %lu", num);
    track = matroska_find_track_by_num(matroska, num);
    if (!track || size < 3) {
        loge("pos 0x%"PRIx64", luster_pos 0x%"PRIx64", cur_pos 0x%"PRIx64", size %d",
            pos, cluster_pos, aic_stream_tell(pb), size);
        return PARSER_ERROR;
    }

    if (!(st = track->stream)) {
        loge("No stream associated to TrackNumber %"PRIu64". "
               "Ignoring Block with this TrackNumber.\n", num);
        return 0;
    }

    if (block_duration > INT64_MAX)
        block_duration = INT64_MAX;

    block_time = aic_stream_rb16(pb);
    flags      = aic_stream_r8(pb);
    size      -= 3;
    if (is_keyframe == -1)
        is_keyframe = flags & 0x80 ? 1 : 0;

    if (cluster_time != (uint64_t) -1 &&
        (block_time >= 0 || cluster_time >= -block_time)) {
        timecode = cluster_time + block_time - track->codec_delay_in_track_tb;
    }

    if (matroska->skip_to_keyframe &&
        track->type != MATROSKA_TRACK_TYPE_SUBTITLE) {
        // Compare signed timecodes. Timecode may be negative due to codec delay
        // offset. We don't support timestamps greater than int64_t
        if ((int64_t)timecode < (int64_t)matroska->skip_to_timecode)
            return res;
        if (is_keyframe)
            matroska->skip_to_keyframe = 0;
    }
    res = matroska_parse_laces(matroska, &data, size, (flags & 0x06) >> 1,
                               pb, lace_size, &laces);
    if (res < 0) {
        loge("Error parsing frame sizes.\n");
        return res;
    }

    logi("skip_track 0x%x track:type %"PRIu64" default_duration %"PRIu64" "
        "end_timecode %"PRIu64", needs_decoding %d audio.buf %p",
        matroska->skip_track, track->type, track->default_duration,
        track->end_timecode, track->needs_decoding, track->audio.buf);

    if (!block_duration && trust_default_duration)
        block_duration = track->default_duration * laces / matroska->time_scale;

    if (cluster_time != (uint64_t)-1 && (block_time >= 0 || cluster_time >= -block_time))
        track->end_timecode =
            MPP_MAX(track->end_timecode, timecode + block_duration);

    for (n = 0; n < laces; n++) {
        int64_t lace_duration = block_duration*(n+1) / laces - block_duration*n / laces;
        int      out_size = lace_size[n];

        if (track->needs_decoding) {
            loge("not support needs decoding");
            return PARSER_ERROR;
        }
        matroska->cur_pos = aic_stream_tell(pb);
        pkt->size         = out_size;
        pkt->pts          = timecode * 1000;
        pkt->duration     = lace_duration * 1000;
        pkt->type         = st->codecpar.codec_type;
        pkt->flag         = 0;
        logd("type %d cur_pos 0x%lx, eos_pos 0x%lx size 0x%x",
            pkt->type, matroska->cur_pos, matroska->eos_pos, out_size);

        /*Unsupport video or audio type need to be skip cur packet*/
        if (pkt->type == MPP_MEDIA_TYPE_VIDEO &&
            matroska->skip_track & EBML_SKIP_VIDEO_TRACK) {
            aic_stream_skip(pb, out_size);
        }
        if (pkt->type == MPP_MEDIA_TYPE_AUDIO &&
            matroska->skip_track & EBML_SKIP_AUDIO_TRACK) {
            aic_stream_skip(pb, out_size);
        }

        if (timecode != 0)
            timecode = lace_duration ? timecode + lace_duration : 0;
        data += lace_size[n];
    }

    return PARSER_OK;
}

static int matroska_parse_cluster(struct matroska_demux_context *matroska,
                                  struct aic_stream *c, struct aic_parser_packet *pkt)
{
    matroska_cluster *cluster = &matroska->current_cluster;
    matroska_block     *block = &cluster->block;
    int res = PARSER_OK;

    matroska->cur_pos = 0;

    assert(matroska->num_levels <= 2);

    if (matroska->num_levels == 1) {
        res = ebml_parse(matroska, c, g_matroska_segment, NULL);
        logd("current_id 0x%x, num_levels:%d, g_matroska_segment res:%d eos_pos 0x%lx",
            matroska->current_id, matroska->num_levels, res, matroska->eos_pos);

        if (res == 1) {
            /* Found a cluster: subtract the size of the ID already read. */
            cluster->pos = aic_stream_tell(c) - 4;

            res = ebml_parse(matroska, c, g_matroska_cluster_enter, cluster);
            if (res < 0)
                return res;
        }
    }

    if (matroska->num_levels == 2) {
        /* We are inside a cluster. */
        res = ebml_parse(matroska, c, g_matroska_cluster_parsing, cluster);

        logd("matroska: current_id 0x%x, num_levels:%d, "
            "cluster parsing: timecode %"PRIu64", pos 0x%"PRIx64",cur pos 0x%"PRIx64", "
            "block: size %d, pos 0x%"PRIx64", duration %"PRIu64", reference %"PRId64"",
            matroska->current_id, matroska->num_levels,
            cluster->timecode, cluster->pos, aic_stream_tell(c),
            block->bin.size, block->bin.pos, block->duration, block->reference);

        if (res >= 0 && block->bin.size > 0) {
            int is_keyframe = block->non_simple ? block->reference == INT64_MIN : -1;
            uint8_t* additional = block->additional.size > 0 ?
                                    block->additional.data : NULL;

            res = matroska_parse_block(matroska, c, block->bin.data,
                                       block->bin.size, block->bin.pos,
                                       cluster->timecode, block->duration,
                                       is_keyframe, additional, block->additional_id,
                                       block->additional.size, cluster->pos,
                                       block->discard_padding, pkt);
        } else {
            memset(pkt, 0, sizeof(struct aic_parser_packet));
            pkt->type = MPP_MEDIA_TYPE_UNKNOWN;
        }

        ebml_free(g_matroska_blockgroup, block);
        memset(block, 0, sizeof(*block));
    } else if (!matroska->num_levels) {
        res = PARSER_EOS;
        loge("warong levels %d", matroska->num_levels);
        goto  exit;
    }

    if (aic_stream_tell(c) >= matroska->eos_pos) {
        res = PARSER_EOS;
        goto  exit;
    }

    if (res != PARSER_OK) {
        goto exit;
    }

    return PARSER_OK;

exit:
    if (res == PARSER_EOS) {
        matroska->done = 1;
        pkt->size = 0;
        pkt->flag = PACKET_EOS;
        printf("[%s:%d]packet eos!!!!!!", __FUNCTION__, __LINE__);
    }

    return res;
}

int matroska_peek_packet(struct aic_matroska_parser *s, struct aic_parser_packet *pkt)
{
    struct aic_stream *c = s->stream;
    struct matroska_demux_context *matroska = &s->matroska_c;

    if (matroska->resync_pos == -1) {
        // This can only happen if generic seeking has been used.
        matroska->resync_pos = aic_stream_tell(c);
    }

    /*cluster contains a data stream of audio, video and subtitle tracks*/
    if (matroska_parse_cluster(matroska, c, pkt) < 0)
        matroska_resync(c, matroska, matroska->resync_pos);

    if (matroska->done) {
        return PARSER_EOS;
    }

    return PARSER_OK;
}

int matroska_read_packet(struct aic_matroska_parser *s, struct aic_parser_packet *pkt)
{
    aic_stream_seek(s->stream, s->matroska_c.cur_pos, SEEK_SET);
    aic_stream_read(s->stream, pkt->data, pkt->size);

    return PARSER_OK;
}

static int find_index_by_pts(struct matroska_stream_ctx *st, s64 pts)
{
    int i, index = 0;
    int64_t min = INT64_MAX;
    int64_t sample_pts = 0;
    int64_t diff = 0;
    struct matroska_index_entry *cur_sample = NULL;

    logd("nb_index_entries:%d\n", st->nb_index_entries);

    /* First step: find current frame by pts*/
    for (i = 0; i < st->nb_index_entries; i++) {
        cur_sample = &st->index_entries[i];
        sample_pts = cur_sample->timestamp;
        diff = MPP_ABS(pts, sample_pts);
        if (diff < min) {
            min = diff;
            index = i;
        }
    }

    if (index > 0) {
        return index;
    }

    return PARSER_ERROR;
}

int matroska_seek_packet(struct aic_matroska_parser *s, s64 pts)
{
    struct matroska_demux_context *matroska = &s->matroska_c;
    struct matroska_stream_ctx *st = s->streams[0];
    struct aic_stream *c = s->stream;
    int  index;

    /* Parse the CUES now since we need the index data to seek. */
    if (matroska->cues_parsing_deferred > 0)
    {
        matroska->cues_parsing_deferred = 0;
        matroska_parse_cues(matroska, c, st);
    }

    if (st->nb_index_entries <= 0) {
        logd("find index entries %d", st->nb_index_entries);
        goto err;
    }

    index = find_index_by_pts(st, pts);
    if (index < 0) {
        logd("find index failed");
        goto err;
    }

    /* We seek to a level 1 element, so set the appropriate status. */
    matroska_reset_status(c, matroska, 0, st->index_entries[index].pos);

    return PARSER_OK;

err:
    // slightly hackish but allows proper fallback to
    // the generic seeking code.
    matroska_reset_status(c, matroska, 0, -1);
    matroska->resync_pos = -1;

    return PARSER_ERROR;
}

int matroska_read_close(struct aic_matroska_parser *s)
{
    struct matroska_demux_context *matroska = &s->matroska_c;

    matroska_release_index_entries(s->streams[0]);

    ebml_free(g_matroska_segment, matroska);

    return PARSER_OK;
}

int matroska_control(struct aic_matroska_parser *s, enum parse_command cmd, void *params)
{
    switch (cmd) {
        case PARSER_VIDEO_SKIP_PACKET:
            s->matroska_c.skip_track |= EBML_SKIP_VIDEO_TRACK;
            break;

        case PARSER_AUDIO_SKIP_PACKET:
            s->matroska_c.skip_track |= EBML_SKIP_AUDIO_TRACK;
            break;

        default:
            return PARSER_INVALIDPARAM;

    }

    return PARSER_OK;
}
