/*
 * Copyright (C) 2020-2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <che.jiang@artinchip.com>
 *  Desc: aic tag, internal header for aic based (de)muxers
 */


#ifndef AIC_TAGS_H
#define AIC_TAGS_H

#include "aic_parser.h"
#include "aic_stream.h"

#define MKTAG(a,b,c,d) ((a) | ((b) << 8) | ((c) << 16) | ((unsigned)(d) << 24))

enum CodecID {
	CODEC_ID_NONE,

	/* video codecs */
	CODEC_ID_MJPEG,
	CODEC_ID_H264,
	CODEC_ID_MPEG4,

	/* various PCM "codecs" */
	CODEC_ID_FIRST_AUDIO = 0x10000,     ///< A dummy id pointing at the start of audio codecs
	CODEC_ID_PCM_S16LE = 0x10000,
	CODEC_ID_PCM_S16BE,
	CODEC_ID_PCM_U16LE,
	CODEC_ID_PCM_U16BE,
	CODEC_ID_PCM_S8,
	CODEC_ID_PCM_U8,
	CODEC_ID_PCM_MULAW,
	CODEC_ID_PCM_ALAW,
	CODEC_ID_PCM_S32LE,
	CODEC_ID_PCM_S32BE,
	CODEC_ID_PCM_U32LE,
	CODEC_ID_PCM_U32BE,
	CODEC_ID_PCM_S24LE,
	CODEC_ID_PCM_S24BE,
	CODEC_ID_PCM_U24LE,
	CODEC_ID_PCM_U24BE,
	CODEC_ID_PCM_S24DAUD,
	CODEC_ID_PCM_ZORK,
	CODEC_ID_PCM_S16LE_PLANAR,
	CODEC_ID_PCM_DVD,
	CODEC_ID_PCM_F32BE,
	CODEC_ID_PCM_F32LE,
	CODEC_ID_PCM_F64BE,
	CODEC_ID_PCM_F64LE,
	CODEC_ID_PCM_BLURAY,
	CODEC_ID_PCM_LXF,
	CODEC_ID_PCM_S8_PLANAR,
	CODEC_ID_PCM_S24LE_PLANAR,
	CODEC_ID_PCM_S32LE_PLANAR,
	CODEC_ID_PCM_S16BE_PLANAR,
	CODEC_ID_PCM_S64LE,
	CODEC_ID_PCM_S64BE,
	CODEC_ID_PCM_F16LE,
	CODEC_ID_PCM_F24LE,
	CODEC_ID_PCM_VIDC,
	CODEC_ID_PCM_SGA,

	CODEC_ID_MP3, ///< preferred ID for decoding MPEG audio layer 1, 2 or 3
	CODEC_ID_AAC,
};

struct codec_tag {
	enum CodecID id;
	unsigned int tag;
};


struct aic_codec_param {
	enum aic_parser_stream_type codec_type;
	enum CodecID   codec_id;

	uint32_t  codec_tag;
	uint8_t  *extradata;
	int       extradata_size;
	int64_t   bit_rate;
	int       bits_per_coded_sample;

	/* Video only.*/
	int       width;
	int       height;

	/* Audio only.*/
	int       channels;
	int       sample_rate;
	int       block_align;
};

extern const struct codec_tag aic_codec_bmp_tags[];
extern const struct codec_tag aic_codec_wav_tags[];

enum CodecID aic_codec_get_id(const struct codec_tag *tags,  unsigned int tag);

#endif /* AIC_TAGS_H */
