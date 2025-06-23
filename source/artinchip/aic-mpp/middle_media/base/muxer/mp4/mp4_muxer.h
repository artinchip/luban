/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: mp4_muxer
 */

#ifndef __MP4_MUXER_H__
#define __MP4_MUXER_H__

#include <unistd.h>
#include "aic_muxer.h"
#include "aic_stream.h"
#include "aic_tag.h"

struct mov_stts {
	unsigned int count;
	int duration;
};

#define MOV_SYNC_SAMPLE         0x0001
#define MOV_PARTIAL_SYNC_SAMPLE 0x0002
#define MOV_DISPOSABLE_SAMPLE   0x0004

struct mov_entry {
	uint64_t     pos;
	int64_t      dts;
	int64_t      pts;
	unsigned int size;
	unsigned int samples_in_chunk;
	unsigned int chunk_num;              ///< Chunk number if the current entry is a chunk start otherwise 0
	unsigned int entries;
	int          cts;
	uint32_t     flags;
};

struct codec_parameters {
	int codec_type;
	enum CodecID   codec_id;
	uint32_t         codec_tag;
	int format;
	int64_t bit_rate;
	int bits_per_coded_sample;
	int width;
	int height;
	int frame_size;
	/*Audio only*/
	int      channels;
	int      sample_rate;
};

struct mov_track {
	int         mode;
	int         entry;
	unsigned    timescale;
	uint64_t    time;
	int64_t     track_duration;
	long        sample_count;
	long        sample_size;
	long        chunk_count;
	int         language;
	int         track_id;
	int         tag;
	struct codec_parameters codec_para;
	struct mov_entry   *cluster;
	unsigned    cluster_capacity;
	int64_t     start_pts;
	int64_t     end_pts;
	int64_t     data_offset;
};

struct aic_mov_muxer {
	struct aic_muxer base;
	struct aic_stream* stream;
	struct mov_track *track;
	uint64_t    time;
	int     mode;
	int     nb_streams;
	int64_t mdat_pos;
	uint64_t mdat_size;
	int track_ids_ok;
	struct mov_track *tracks;
	const struct codec_tag * const *codec_tag;
};

s32 mp4_init(struct aic_mov_muxer *muxer,struct aic_av_media_info *info);
s32 mp4_write_header(struct aic_mov_muxer *muxer);
s32 mp4_write_packet(struct aic_mov_muxer *muxer,struct aic_av_packet *packet);
s32 mp4_write_trailer(struct aic_mov_muxer *muxer);
s32 mp4_close(struct aic_mov_muxer *muxer);

#endif
