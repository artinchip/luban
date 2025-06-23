/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <qi.xu@artinchip.com>
 *  Desc: mov tags
 */

#include "mov_tags.h"

const struct codec_tag mp4_obj_type[] = {
    { CODEC_ID_AAC         , 0x40 },
    { CODEC_ID_AAC         , 0x66 }, /* MPEG-2 AAC Main */
    { CODEC_ID_AAC         , 0x67 }, /* MPEG-2 AAC Low */
    { CODEC_ID_AAC         , 0x68 }, /* MPEG-2 AAC SSR */
    { CODEC_ID_MP3         , 0x69 }, /* 13818-3 */
    { CODEC_ID_MP3         , 0x6B }, /* 11172-3 */
    { CODEC_ID_MJPEG       , 0x6C }, /* 10918-1 */
    { CODEC_ID_NONE        ,    0 },
};

const struct codec_tag mov_audio_tags[] = {
    { CODEC_ID_AAC,             MKTAG('m', 'p', '4', 'a') },
    { CODEC_ID_MP3,             MKTAG('.', 'm', 'p', '3') },
    { CODEC_ID_MP3,             MKTAG('m', 'p', '3', ' ') }, /* vlc */
    { CODEC_ID_MP3,                            0x6D730055 },
    { CODEC_ID_PCM_ALAW,        MKTAG('a', 'l', 'a', 'w') },
    { CODEC_ID_PCM_F32BE,       MKTAG('f', 'l', '3', '2') },
    { CODEC_ID_PCM_F32LE,       MKTAG('f', 'l', '3', '2') },
    { CODEC_ID_PCM_F64BE,       MKTAG('f', 'l', '6', '4') },
    { CODEC_ID_PCM_F64LE,       MKTAG('f', 'l', '6', '4') },
    { CODEC_ID_PCM_MULAW,       MKTAG('u', 'l', 'a', 'w') },
    { CODEC_ID_PCM_S16BE,       MKTAG('t', 'w', 'o', 's') },
    { CODEC_ID_PCM_S16LE,       MKTAG('s', 'o', 'w', 't') },
    { CODEC_ID_PCM_S16BE,       MKTAG('l', 'p', 'c', 'm') },
    { CODEC_ID_PCM_S16LE,       MKTAG('l', 'p', 'c', 'm') },
    { CODEC_ID_PCM_S24BE,       MKTAG('i', 'n', '2', '4') },
    { CODEC_ID_PCM_S24LE,       MKTAG('i', 'n', '2', '4') },
    { CODEC_ID_PCM_S32BE,       MKTAG('i', 'n', '3', '2') },
    { CODEC_ID_PCM_S32LE,       MKTAG('i', 'n', '3', '2') },
    { CODEC_ID_PCM_S8,          MKTAG('s', 'o', 'w', 't') },
    { CODEC_ID_PCM_U8,          MKTAG('r', 'a', 'w', ' ') },
    { CODEC_ID_PCM_U8,          MKTAG('N', 'O', 'N', 'E') },
    { CODEC_ID_NONE, 0 },
};

const struct codec_tag mov_video_tags[] = {
    { CODEC_ID_MJPEG,  MKTAG('j', 'p', 'e', 'g') }, /* PhotoJPEG */
    { CODEC_ID_MJPEG,  MKTAG('m', 'j', 'p', 'a') }, /* Motion-JPEG (format A) */
    { CODEC_ID_MJPEG,  MKTAG('A', 'V', 'D', 'J') }, /* MJPEG with alpha-channel (AVID JFIF meridien compressed) */
    { CODEC_ID_MJPEG,  MKTAG('A', 'V', 'R', 'n') }, /* MJPEG with alpha-channel (AVID ABVB/Truevision NuVista) */
    { CODEC_ID_MJPEG,  MKTAG('d', 'm', 'b', '1') }, /* Motion JPEG OpenDML */

    { CODEC_ID_H264,   MKTAG('a', 'v', 'c', '1') }, /* AVC-1/H.264 */
    { CODEC_ID_H264,   MKTAG('a', 'v', 'c', '2') },
    { CODEC_ID_H264,   MKTAG('a', 'v', 'c', '3') },
    { CODEC_ID_H264,   MKTAG('a', 'v', 'c', '4') },
    { CODEC_ID_H264,   MKTAG('a', 'i', '5', 'p') }, /* AVC-Intra  50M 720p24/30/60 */
    { CODEC_ID_H264,   MKTAG('a', 'i', '5', 'q') }, /* AVC-Intra  50M 720p25/50 */
    { CODEC_ID_H264,   MKTAG('a', 'i', '5', '2') }, /* AVC-Intra  50M 1080p25/50 */
    { CODEC_ID_H264,   MKTAG('a', 'i', '5', '3') }, /* AVC-Intra  50M 1080p24/30/60 */
    { CODEC_ID_H264,   MKTAG('a', 'i', '5', '5') }, /* AVC-Intra  50M 1080i50 */
    { CODEC_ID_H264,   MKTAG('a', 'i', '5', '6') }, /* AVC-Intra  50M 1080i60 */
    { CODEC_ID_H264,   MKTAG('a', 'i', '1', 'p') }, /* AVC-Intra 100M 720p24/30/60 */
    { CODEC_ID_H264,   MKTAG('a', 'i', '1', 'q') }, /* AVC-Intra 100M 720p25/50 */
    { CODEC_ID_H264,   MKTAG('a', 'i', '1', '2') }, /* AVC-Intra 100M 1080p25/50 */
    { CODEC_ID_H264,   MKTAG('a', 'i', '1', '3') }, /* AVC-Intra 100M 1080p24/30/60 */
    { CODEC_ID_H264,   MKTAG('a', 'i', '1', '5') }, /* AVC-Intra 100M 1080i50 */
    { CODEC_ID_H264,   MKTAG('a', 'i', '1', '6') }, /* AVC-Intra 100M 1080i60 */
    { CODEC_ID_H264,   MKTAG('A', 'V', 'i', 'n') }, /* AVC-Intra with implicit SPS/PPS */
    { CODEC_ID_H264,   MKTAG('a', 'i', 'v', 'x') }, /* XAVC 10-bit 4:2:2 */
    { CODEC_ID_H264,   MKTAG('r', 'v', '6', '4') }, /* X-Com Radvision */
    { CODEC_ID_H264,   MKTAG('x', 'a', 'l', 'g') }, /* XAVC-L HD422 produced by FCP */
    { CODEC_ID_H264,   MKTAG('a', 'v', 'l', 'g') }, /* Panasonic P2 AVC-LongG */
    { CODEC_ID_H264,   MKTAG('d', 'v', 'a', '1') }, /* AVC-based Dolby Vision derived from avc1 */
    { CODEC_ID_H264,   MKTAG('d', 'v', 'a', 'v') }, /* AVC-based Dolby Vision derived from avc3 */

    { CODEC_ID_NONE,   0 },
};

