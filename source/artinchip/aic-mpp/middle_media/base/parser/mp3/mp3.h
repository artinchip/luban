/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: mp3
 */

#ifndef __MP3_H__
#define __MP3_H__

#include <unistd.h>
#include "aic_parser.h"

#define ID3v1_TAG_SIZE 128
#define ID3v2_HEADER_SIZE 10
#define ID3v2_DEFAULT_MAGIC "ID3"

#define MP3_MASK 0xFFFE0CCF

#define XING_FLAG_FRAMES 0x01
#define XING_FLAG_SIZE   0x02
#define XING_FLAG_TOC    0x04
#define XING_FLAC_QSCALE 0x08
#define XING_TOC_COUNT 100

#define MPA_STEREO  0
#define MPA_JSTEREO 1
#define MPA_DUAL    2
#define MPA_MONO    3

#define MKTAG(a,b,c,d) ((a) | ((b) << 8) | ((c) << 16) | ((unsigned)(d) << 24))
#define MKBETAG(a,b,c,d) ((d) | ((c) << 8) | ((b) << 16) | ((unsigned)(a) << 24))

#define ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))

#define FFABS(a) ((a) >= 0 ? (a) : (-(a)))

enum CheckRet {
	CHECK_WRONG_HEADER = -1,
	CHECK_SEEK_FAILED  = -2,
};

struct mpa_decode_header {
	int frame_size; // size after encoding
	int error_protection;
	int layer;
	int sample_rate;
	int sample_rate_index; /* between 0 and 8 */
	int bit_rate;
	int nb_channels;
	int mode;
	int mode_ext;
	int lsf;
	int nb_samples_per_frame;
	int frame_duration;//us
};

struct mp3_dec_context {
	int64_t filesize;
	int xing_toc;
	int start_pad;
	int end_pad;
	int usetoc;
	unsigned frames; /* Total number of frames in file */
	unsigned header_filesize;   /* Total number of bytes in the stream */
	int is_cbr;
	unsigned char xing_toc_index[XING_TOC_COUNT];
};

struct aic_mp3_parser {
	struct aic_parser base;
	struct aic_stream* stream;
	struct mpa_decode_header header;
	struct mp3_dec_context ctx;
	unsigned first_packet_pos;
	uint64_t duration;//us
	uint32_t frame_id;
};

int mp3_read_header(struct aic_mp3_parser *s);
int mp3_close(struct aic_mp3_parser *s);
int mp3_peek_packet(struct aic_mp3_parser *s, struct aic_parser_packet *pkt);
int mp3_seek_packet(struct aic_mp3_parser *s, s64 seek_time);
int mp3_read_packet(struct aic_mp3_parser *s, struct aic_parser_packet *pkt);
#endif
