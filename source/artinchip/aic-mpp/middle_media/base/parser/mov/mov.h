/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <qi.xu@artinchip.com>
*  Desc: mov
*/

#ifndef __MOV_H__
#define __MOV_H__

#include <unistd.h>
#include "aic_parser.h"
#include "mov_tags.h"

struct mov_atom {
	uint32_t type;
	int64_t size;
};

struct mov_stsc {
	int first;
	int count;
	int id;
};

struct mov_stts {
	unsigned int count;
	int duration;
};

struct index_entry {
	int64_t pos;
	int64_t timestamp; // dts
	int size;
	int min_distance;
};

struct mov_stream_ctx {
	int index;
	int time_scale;
	int width;
	int height;
	int type;   // enum aic_parser_stream_type
	enum CodecID   id;
	uint32_t format;
	int stsd_version;
	int stsd_count;

	int cur_sample_idx;

	int channels;         /* audio only*/
	int bits_per_sample;  /* audio only*/
	int sample_rate;      /* audio only*/

	unsigned int samples_per_frame;
	unsigned int bytes_per_frame;

	int extra_data_size;
	unsigned char *extra_data;

	int64_t nb_frames;
	int64_t duration;

	int dts_shift;
	int keyframe_count;
	int *keyframes;  // parse from stss
	unsigned int chunk_count;
	int64_t *chunk_offsets;     // chunk offset
	unsigned int stts_count;
	struct mov_stts *stts_data;  // duration of samples
	unsigned int ctts_count;
	struct mov_stts *ctts_data;
	unsigned int stsc_count;
	struct mov_stsc *stsc_data; // chunk info for sample
	unsigned int stsc_index;
	int stsc_sample;
	int ctts_index;

	unsigned int sample_size; ///< may contain value calculated from stsd or value from stsz atom
	unsigned int stsz_sample_size; ///< always contains sample size from stsz atom
	unsigned int sample_count;
	int *sample_sizes;       // stsz

	int nb_index_entries;
	struct index_entry *index_entries;
};

#define MAX_TRACK_NUM 8
struct aic_mov_parser {
	struct aic_parser base;
	struct aic_stream* stream;

	int atom_depth;
	int find_moov;
	int find_mdat;
	int isom; // 1 if files is ISO Media (mp4/3gp)

	int time_scale; // time scale in mvhd
	int64_t duration; // duration of the longest track, parse from mvhd
	int movie_display_matrix[3][3]; ///< display matrix from mvhd

	int nb_streams;
	struct mov_stream_ctx* streams[MAX_TRACK_NUM];

	struct index_entry *cur_sample;
};

int mov_read_header(struct aic_mov_parser *c);
int mov_close(struct aic_mov_parser *c);
int mov_peek_packet(struct aic_mov_parser *c, struct aic_parser_packet *pkt);
int mov_seek_packet(struct aic_mov_parser *c, s64 pts);

#endif
