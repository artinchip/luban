/*
 * Copyright (C) 2020-2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <che.jiang@artinchip.com>
 *  Desc: avi tag
 */


#include <string.h>
#include "aic_tag.h"
#include "mpp_mem.h"
#include "mpp_log.h"


const struct codec_tag aic_codec_bmp_tags[] = {
    { CODEC_ID_H264,         MKTAG('H', '2', '6', '4') },
    { CODEC_ID_H264,         MKTAG('h', '2', '6', '4') },
    { CODEC_ID_H264,         MKTAG('X', '2', '6', '4') },
    { CODEC_ID_H264,         MKTAG('x', '2', '6', '4') },
    { CODEC_ID_H264,         MKTAG('a', 'v', 'c', '1') },
    { CODEC_ID_H264,         MKTAG('D', 'A', 'V', 'C') },
    { CODEC_ID_H264,         MKTAG('S', 'M', 'V', '2') },
    { CODEC_ID_H264,         MKTAG('V', 'S', 'S', 'H') },
    { CODEC_ID_H264,         MKTAG('Q', '2', '6', '4') }, /* QNAP surveillance system */
    { CODEC_ID_H264,         MKTAG('V', '2', '6', '4') }, /* CCTV recordings */
    { CODEC_ID_H264,         MKTAG('G', 'A', 'V', 'C') }, /* GeoVision camera */
    { CODEC_ID_H264,         MKTAG('U', 'M', 'S', 'V') },
    { CODEC_ID_H264,         MKTAG('t', 's', 'h', 'd') },
    { CODEC_ID_H264,         MKTAG('I', 'N', 'M', 'C') },
    { CODEC_ID_MPEG4,        MKTAG('F', 'M', 'P', '4') },
    { CODEC_ID_MPEG4,        MKTAG('D', 'I', 'V', 'X') },
    { CODEC_ID_MPEG4,        MKTAG('D', 'X', '5', '0') },
    { CODEC_ID_MPEG4,        MKTAG('X', 'V', 'I', 'D') },
    { CODEC_ID_MPEG4,        MKTAG('M', 'P', '4', 'S') },
    { CODEC_ID_MPEG4,        MKTAG('M', '4', 'S', '2') },
    /* some broken AVIs use this */
    { CODEC_ID_MPEG4,        MKTAG( 4 ,  0 ,  0 ,  0 ) },
    /* some broken AVIs use this */
    { CODEC_ID_MPEG4,        MKTAG('Z', 'M', 'P', '4') },
    { CODEC_ID_MPEG4,        MKTAG('D', 'I', 'V', '1') },
    { CODEC_ID_MPEG4,        MKTAG('B', 'L', 'Z', '0') },
    { CODEC_ID_MPEG4,        MKTAG('m', 'p', '4', 'v') },
    { CODEC_ID_MPEG4,        MKTAG('U', 'M', 'P', '4') },
    { CODEC_ID_MPEG4,        MKTAG('W', 'V', '1', 'F') },
    { CODEC_ID_MPEG4,        MKTAG('S', 'E', 'D', 'G') },
    { CODEC_ID_MPEG4,        MKTAG('R', 'M', 'P', '4') },
    { CODEC_ID_MPEG4,        MKTAG('3', 'I', 'V', '2') },
    /* WaWv MPEG-4 Video Codec */
    { CODEC_ID_MPEG4,        MKTAG('W', 'A', 'W', 'V') },
    { CODEC_ID_MPEG4,        MKTAG('F', 'F', 'D', 'S') },
    { CODEC_ID_MPEG4,        MKTAG('F', 'V', 'F', 'W') },
    { CODEC_ID_MPEG4,        MKTAG('D', 'C', 'O', 'D') },
    { CODEC_ID_MPEG4,        MKTAG('M', 'V', 'X', 'M') },
    { CODEC_ID_MPEG4,        MKTAG('P', 'M', '4', 'V') },
    { CODEC_ID_MPEG4,        MKTAG('S', 'M', 'P', '4') },
    { CODEC_ID_MPEG4,        MKTAG('D', 'X', 'G', 'M') },
    { CODEC_ID_MPEG4,        MKTAG('V', 'I', 'D', 'M') },
    { CODEC_ID_MPEG4,        MKTAG('M', '4', 'T', '3') },
    { CODEC_ID_MPEG4,        MKTAG('G', 'E', 'O', 'X') },
    /* flipped video */
    { CODEC_ID_MPEG4,        MKTAG('G', '2', '6', '4') },
    /* flipped video */
    { CODEC_ID_MPEG4,        MKTAG('H', 'D', 'X', '4') },
    { CODEC_ID_MPEG4,        MKTAG('D', 'M', '4', 'V') },
    { CODEC_ID_MPEG4,        MKTAG('D', 'M', 'K', '2') },
    { CODEC_ID_MPEG4,        MKTAG('D', 'Y', 'M', '4') },
    { CODEC_ID_MPEG4,        MKTAG('D', 'I', 'G', 'I') },
    /* Ephv MPEG-4 */
    { CODEC_ID_MPEG4,        MKTAG('E', 'P', 'H', 'V') },
    { CODEC_ID_MPEG4,        MKTAG('E', 'M', '4', 'A') },
    /* Divio MPEG-4 */
    { CODEC_ID_MPEG4,        MKTAG('M', '4', 'C', 'C') },
    { CODEC_ID_MPEG4,        MKTAG('S', 'N', '4', '0') },
    { CODEC_ID_MPEG4,        MKTAG('V', 'S', 'P', 'X') },
    { CODEC_ID_MPEG4,        MKTAG('U', 'L', 'D', 'X') },
    { CODEC_ID_MPEG4,        MKTAG('G', 'E', 'O', 'V') },

    { CODEC_ID_MJPEG,        MKTAG('M', 'J', 'P', 'G') },
    { CODEC_ID_MJPEG,        MKTAG('M', 'S', 'C', '2') }, /* Multiscope II */
    { CODEC_ID_MJPEG,        MKTAG('L', 'J', 'P', 'G') },
    { CODEC_ID_MJPEG,        MKTAG('d', 'm', 'b', '1') },
    { CODEC_ID_MJPEG,        MKTAG('m', 'j', 'p', 'a') },
    { CODEC_ID_MJPEG,        MKTAG('J', 'R', '2', '4') }, /* Quadrox Mjpeg */

    /* Pegasus lossless JPEG */
    { CODEC_ID_MJPEG,        MKTAG('J', 'P', 'G', 'L') },

    /* JPEG-LS custom FOURCC for AVI - decoder */
    { CODEC_ID_MJPEG,        MKTAG('M', 'J', 'L', 'S') },
    { CODEC_ID_MJPEG,        MKTAG('j', 'p', 'e', 'g') },
    { CODEC_ID_MJPEG,        MKTAG('I', 'J', 'P', 'G') },

    { CODEC_ID_MJPEG,        MKTAG('A', 'C', 'D', 'V') },
    { CODEC_ID_MJPEG,        MKTAG('Q', 'I', 'V', 'G') },
    /* SL M-JPEG */
    { CODEC_ID_MJPEG,        MKTAG('S', 'L', 'M', 'J') },
    /* Creative Webcam JPEG */
    { CODEC_ID_MJPEG,        MKTAG('C', 'J', 'P', 'G') },
    /* Intel JPEG Library Video Codec */
    { CODEC_ID_MJPEG,        MKTAG('I', 'J', 'L', 'V') },
    /* Midvid JPEG Video Codec */
    { CODEC_ID_MJPEG,        MKTAG('M', 'V', 'J', 'P') },
    { CODEC_ID_MJPEG,        MKTAG('A', 'V', 'I', '1') },
    { CODEC_ID_MJPEG,        MKTAG('A', 'V', 'I', '2') },
    { CODEC_ID_MJPEG,        MKTAG('M', 'T', 'S', 'J') },
    /* Paradigm Matrix M-JPEG Codec */
    { CODEC_ID_MJPEG,        MKTAG('Z', 'J', 'P', 'G') },
    { CODEC_ID_NONE,         0 }
};


const struct codec_tag aic_codec_wav_tags[] = {
    { CODEC_ID_PCM_S16LE,       0x0001 },
    /* must come after s16le in this list */
    { CODEC_ID_PCM_U8,          0x0001 },
    { CODEC_ID_PCM_S24LE,       0x0001 },
    { CODEC_ID_PCM_S32LE,       0x0001 },
    { CODEC_ID_PCM_S64LE,       0x0001 },
    { CODEC_ID_PCM_F32LE,       0x0003 },
    /* must come after f32le in this list */
    { CODEC_ID_PCM_F64LE,       0x0003 },
    { CODEC_ID_PCM_ALAW,        0x0006 },
    { CODEC_ID_PCM_MULAW,       0x0007 },
    { CODEC_ID_MP3,             0x0055 },
    { CODEC_ID_AAC,             0x00ff },
    /* ADTS AAC */
    { CODEC_ID_AAC,             0x1600 },
    { CODEC_ID_AAC,             0x706d },
    { CODEC_ID_AAC,             0x4143 },
    { CODEC_ID_AAC,             0xA106 },

    /* HACK/FIXME: Does Vorbis in WAV/AVI have an (in)official ID? */
    { CODEC_ID_NONE,      0 },
};

enum CodecID aic_codec_get_id(const struct codec_tag *tags, unsigned int tag)
{
    int i;
    for (i = 0; tags[i].id != CODEC_ID_NONE; i++) {
        if (tag == tags[i].tag)
            return tags[i].id;
    }

    return CODEC_ID_NONE;
}
