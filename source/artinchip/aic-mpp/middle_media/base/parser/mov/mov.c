/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <qi.xu@artinchip.com>
*  Desc: mov
*/
#define LOG_TAG "mov"

#include <stdlib.h>
#include <inttypes.h>
#include "aic_stream.h"
#include "mov.h"
#include "mov_tags.h"
#include "mpp_mem.h"
#include "mpp_log.h"

#define TIME_BASE 1000000LL

/* those functions parse an atom */
/* links atom IDs to parse functions */
struct mov_parse_table {
	uint32_t type;
	int (*parse)(struct aic_mov_parser *ctx, struct mov_atom atom);
};

static int mov_read_default(struct aic_mov_parser *c, struct mov_atom atom);

static int stream_skip(struct aic_stream* s, int len)
{
	return aic_stream_seek(s, len, SEEK_CUR);
}

static unsigned int r8(struct aic_stream* s)
{
	unsigned char val;
	aic_stream_read(s, &val, 1);
	return val;
}

static unsigned int rb16(struct aic_stream* s)
{
	unsigned int val;
	val = r8(s) << 8;
	val |= r8(s);
	return val;
}

static unsigned int rb24(struct aic_stream* s)
{
	unsigned int val;
	val = rb16(s) << 16;
	val |= r8(s);
	return val;
}

static unsigned int rb32(struct aic_stream* s)
{
	unsigned int val;
	val = rb16(s) << 16;
	val |= rb16(s);
	return val;
}

static uint64_t rb64(struct aic_stream* s)
{
	uint64_t val;
	val = (uint64_t)rb32(s) << 32;
	val |= (uint64_t)rb32(s);
	return val;
}

static unsigned int rl16(struct aic_stream* s)
{
	unsigned int val;
	val = r8(s);
	val |= r8(s) << 8;
	return val;
}

static unsigned int rl32(struct aic_stream* s)
{
	unsigned int val;
	val = rl16(s);
	val |= rl16(s) << 16;
	return val;
}

static int64_t get_time(int64_t pts, int64_t st_time_scale)
{
	return pts * TIME_BASE / st_time_scale;
}

static int mov_read_moov(struct aic_mov_parser *c, struct mov_atom atom)
{
	int ret;

	if (c->find_moov) {
		loge("find duplicated moov atom. skipped it");
		stream_skip(c->stream, atom.size);
		return 0;
	}

	if ((ret = mov_read_default(c, atom)) < 0)
		return ret;
	c->find_moov = 1;
	return 0;
}

static int mov_read_mvhd(struct aic_mov_parser *c, struct mov_atom atom)
{
	int i;
	int version = r8(c->stream);
	rb24(c->stream);

	if (version == 1) {
		rb64(c->stream); // create time
		rb64(c->stream); // modify time
	} else {
		rb32(c->stream); // create time
		rb32(c->stream); // modify time
	}

	c->time_scale = rb32(c->stream);
	if (c->time_scale <= 0) {
		loge("Invalid mvhd time scale %d, default to 1", c->time_scale);
		c->time_scale = 1;
	}

	c->duration = (version == 1) ? rb64(c->stream) : rb32(c->stream);

	rb32(c->stream); // preferred scale
	rb16(c->stream); /* preferred volume */

	stream_skip(c->stream, 10); /* reserved */

	/* movie display matrix, store it in main context and use it later on */
	for (i=0; i<3; i++) {
		c->movie_display_matrix[i][0] = rb32(c->stream); // 16.16 fixed point
		c->movie_display_matrix[i][1] = rb32(c->stream); // 16.16 fixed point
		c->movie_display_matrix[i][2] = rb32(c->stream); //  2.30 fixed point
	}

	rb32(c->stream); /* preview time */
	rb32(c->stream); /* preview duration */
	rb32(c->stream); /* poster time */
	rb32(c->stream); /* selection time */
	rb32(c->stream); /* selection duration */
	rb32(c->stream); /* current time */
	rb32(c->stream); /* next track ID */

	return 0;
}

static struct mov_stream_ctx *new_stream(struct aic_mov_parser *c)
{
	struct mov_stream_ctx *sc;

	sc = (struct mov_stream_ctx *)mpp_alloc(sizeof(struct mov_stream_ctx));
	if (sc == NULL) {
		return NULL;
	}
	memset(sc, 0, sizeof(struct mov_stream_ctx));

	sc->index = c->nb_streams;
	return sc;
}

static void mov_build_index(struct aic_mov_parser *c, struct mov_stream_ctx *st)
{
	int i, j;
	int64_t current_offset;
	unsigned int stts_index = 0;
	unsigned int stsc_index = 0;
	unsigned int stss_index = 0;
	unsigned int current_dts = 0;
	uint64_t stream_size = 0;
	struct mov_stts *ctts_data_old = st->ctts_data;
	unsigned int ctts_count_old = st->ctts_count;

	/* only use old uncompressed audio chunk demuxing when stts specifies it */
	if (!(st->type == MPP_MEDIA_TYPE_AUDIO && st->stts_count == 1 &&
		st->stts_data[0].duration == 1)) {
		unsigned int current_sample = 0;
		unsigned int stts_sample = 0;
		unsigned int sample_size = 0;
		unsigned int distance = 0;
		int64_t last_dts = 0;
		int64_t dts_correction = 0;
		int key_off = st->keyframe_count && (st->keyframes[0] > 0);

		current_dts -= st->dts_shift;
		last_dts = current_dts;

		if (!st->sample_count)
			return;

		st->index_entries = (struct index_entry*)mpp_alloc(st->sample_count *
			sizeof(struct index_entry));

		if (ctts_data_old) {
			// Expand ctts entries such that we have a 1-1 mapping with samples
			st->ctts_count = 0;
			st->ctts_data = mpp_alloc(st->sample_count * sizeof(*st->ctts_data));
			if(st->ctts_data == NULL) {
				mpp_free(ctts_data_old);
				return;
			}
			memset((uint8_t*)(st->ctts_data), 0, st->sample_count * sizeof(*st->ctts_data));

			for (i=0; i<ctts_count_old && st->ctts_count < st->sample_count; i++) {
				for (j = 0; j<ctts_data_old[j].count && st->ctts_count < st->sample_count; j++) {
					st->ctts_data[st->ctts_count].count = 1;
					st->ctts_data[st->ctts_count].duration = ctts_data_old[i].duration;
					st->ctts_count ++;
				}
			}

			mpp_free(ctts_data_old);
		}

		for (i=0; i<st->chunk_count; i++) {
			int64_t next_offset = (i+1) < st->chunk_count ? st->chunk_offsets[i+1] : INT64_MAX;
			current_offset = st->chunk_offsets[i];
			while ((stsc_index < st->stsc_count-1) &&
			   (i+1 == st->stsc_data[stsc_index+1].first))
				stsc_index ++;

			if (next_offset > current_offset && st->sample_size > 0 && st->sample_size < st->stsz_sample_size &&
			  st->stsc_data[stsc_index].count * (int64_t)st->stsz_sample_size > next_offset - current_offset) {
				logw("stsz sample size %d invalid(too small), ignore", st->stsz_sample_size);
				st->stsz_sample_size = st->sample_size;
			}

			if (st->stsz_sample_size > 0 && st->stsz_sample_size < st->sample_size) {
				logw("stsz sample size %d invalid(too small), ignore", st->stsz_sample_size);
				st->stsz_sample_size = st->sample_size;
			}

			for (j=0; j<st->stsc_data[stsc_index].count; j++) {
				int keyframe = 0;
				if (current_sample >= st->sample_count) {
					loge("wrong sample count");
					return;
				}

				if (!st->keyframe_count || current_sample+key_off == st->keyframes[stss_index]) {
					keyframe = 1;
					if (stss_index + 1 < st->keyframe_count)
						stss_index++;
				}

				if (st->type == MPP_MEDIA_TYPE_AUDIO || (i==0 && j==0))
					keyframe = 1;

				if (keyframe)
					distance = 0;
				sample_size = st->stsz_sample_size > 0 ? st->stsz_sample_size :
					st->sample_sizes[current_sample];

				struct index_entry *e = &st->index_entries[st->nb_index_entries++];
				e->pos = current_offset;
				e->timestamp = current_dts;
				e->size = sample_size;
				e->min_distance = distance;

				current_offset += sample_size;
				stream_size += sample_size;

				if (st->stts_data[stts_index].duration < 0) {
					logw("Invalid sample delta % in stts",
					st->stts_data[stts_index].duration);

					st->stts_data[stts_index].duration = 1;
				}

				current_dts += st->stts_data[stts_index].duration;
				if (!dts_correction || current_dts + dts_correction > last_dts) {
					current_dts += dts_correction;
					dts_correction = 0;
				} else {
					/* Avoid creating non-monotonous DTS */
					dts_correction += current_dts - last_dts - 1;
					current_dts = last_dts + 1;
				}
				last_dts = current_dts;
				distance++;
				stts_sample++;
				current_sample++;
				if (stts_index + 1 < st->stts_count &&
					stts_sample == st->stts_data[stts_index].count) {
					stts_sample = 0;
					stts_index++;
				}
			}
		}
	} else {
		// if it is audio and stts entry is 1
		unsigned int chunk_samples, total = 0;

		if (!st->chunk_count)
			return;

		// compute total chunk count from stsc_data
		for (i=0; i<st->stsc_count; i++) {
			unsigned int count;
			unsigned int chunk_count;

			chunk_samples = st->stsc_data[i].count;
			if (i != st->stsc_count -1 && st->samples_per_frame &&
				chunk_samples % st->samples_per_frame) {
				loge("error unaligned chunk");
				return;
			}

			// count of audio frame in a chunk
			if (st->samples_per_frame > 160) {
				count = chunk_samples / st->samples_per_frame;
			} else if (st->samples_per_frame > 1) {
				unsigned int samples;
				samples = (1024/st->samples_per_frame)*st->samples_per_frame;
				count = (chunk_samples+samples-1) / samples;
			} else {
				count = (chunk_samples+1023) / 1024;
			}

			// count of chunk, which has the same number of samples
			if (i < st->stsc_count-1) {
				chunk_count = st->stsc_data[i+1].first - st->stsc_data[i].first;
			} else {
				chunk_count = st->chunk_count - (st->stsc_data[i].first - 1);
			}

			total += chunk_count * count;
		}

		logd("chunk count: %d", total);

		st->index_entries = (struct index_entry*)mpp_alloc(total *
			sizeof(struct index_entry));
		if (st->index_entries == NULL) {
			loge("mpp_alloc fail\n");
			return;
		}

		// populate index
		for (i = 0; i < st->chunk_count; i++) {
			current_offset = st->chunk_offsets[i];

			if ((stsc_index < st->stsc_count-1) &&
				(i + 1 == st->stsc_data[stsc_index + 1].first))
				stsc_index++;

			chunk_samples = st->stsc_data[stsc_index].count;

			while (chunk_samples > 0) {
				struct index_entry *e;
				unsigned int size, samples;

				if (st->samples_per_frame > 1 && !st->bytes_per_frame) {
					loge("maybe error");
					return;
				}

				if (st->samples_per_frame >= 160) {
					samples = st->samples_per_frame;
					size = st->bytes_per_frame;
				} else if (st->samples_per_frame > 1) {
					samples = MPP_MIN((1024 / st->samples_per_frame)*
						st->samples_per_frame, chunk_samples);
					size = (samples / st->samples_per_frame) * st->bytes_per_frame;
				} else {
					samples = MPP_MIN(1024, chunk_samples);
					size = samples * st->sample_size;
				}

				if (st->nb_index_entries >= total) {
					loge("wrong chunk count %d", total);
					return;
				}

				if (size > 0x3FFFFFFF) {
					loge("Sample size %d is too large\n", size);
					return;
				}
				e = &st->index_entries[st->nb_index_entries++];
				e->pos = current_offset;
				e->timestamp = current_dts;
				e->size = size;
				e->min_distance = 0;

				current_offset += size;
				current_dts += samples;
				chunk_samples -= samples;
			}
		}
	} // end audio sample
}

static int mov_codec_id(struct mov_stream_ctx *st, uint32_t format)
{
	int id = codec_get_id(mov_audio_tags, format);

	if (st->type == MPP_MEDIA_TYPE_VIDEO)
		id = codec_get_id(mov_video_tags, format);

	return id;
}

static int mov_read_trak(struct aic_mov_parser *c, struct mov_atom atom)
{
	int ret;
	struct mov_stream_ctx *st = new_stream(c);

	c->streams[c->nb_streams++] = st;

	if ((ret = mov_read_default(c, atom) < 0)) {
		loge("read trak error");
		return ret;
	}

	// build index
	mov_build_index(c, st);
	logi("stream id: %d, sample count: %d, index_entries: %d",
		c->nb_streams-1, st->sample_count, st->nb_index_entries);

	return 0;
}

static int mov_read_tkhd(struct aic_mov_parser *c, struct mov_atom atom)
{
	int i;
	int width;
	int height;
	int version;
	struct mov_stream_ctx *st;

	if (c->nb_streams < 1)
		return 0;

	st = c->streams[c->nb_streams - 1];
	version = r8(c->stream);
	rb24(c->stream);

	if (version == 1) {
		rb64(c->stream);
		rb64(c->stream);
	} else {
		rb32(c->stream);
		rb32(c->stream);
	}

	st->id = rb32(c->stream);
	rb32(c->stream); /* reserved */

	/* highlevel (considering edits) duration in movie timebase */
	(version == 1) ? rb64(c->stream) : rb32(c->stream);
	rb32(c->stream); /* reserved */
	rb32(c->stream); /* reserved */

	rb16(c->stream); /* layer */
	rb16(c->stream); /* alternate group */
	rb16(c->stream); /* volume */
	rb16(c->stream); /* reserved */

	// read in the display matrix (outlined in ISO 14496-12, Section 6.2.2)
	// they're kept in fixed point format through all calculations
	// save u,v,z to store the whole matrix in the AV_PKT_DATA_DISPLAYMATRIX
	// side data, but the scale factor is not needed to calculate aspect ratio
	for (i = 0; i < 3; i++) {
		rb32(c->stream);   // 16.16 fixed point
		rb32(c->stream);   // 16.16 fixed point
		rb32(c->stream);   //  2.30 fixed point
	}

	width = rb32(c->stream);    // 16.16 fixed point track width
	height = rb32(c->stream);   // 16.16 fixed point track height
	st->width = width >> 16;
	st->height = height >> 16;

	return 0;
}

static int mov_read_hdlr(struct aic_mov_parser *c, struct mov_atom atom)
{
	uint32_t type;
	int64_t title_size;
	struct mov_stream_ctx *st;

	r8(c->stream);
	rb24(c->stream);

	rl32(c->stream);
	type = rl32(c->stream);

	st = c->streams[c->nb_streams-1];

	if (type == MKTAG('v','i','d','e'))
		st->type = MPP_MEDIA_TYPE_VIDEO;
	else if (type == MKTAG('s','o','u','n'))
		st->type = MPP_MEDIA_TYPE_AUDIO;

	rb32(c->stream); /* component  manufacture */
	rb32(c->stream); /* component flags */
	rb32(c->stream); /* component flags mask */

	title_size = atom.size - 24;
	if (title_size > 0) {
		aic_stream_seek(c->stream, title_size, SEEK_CUR);
	}

	return 0;
}

static int mov_read_stsd_video(struct aic_mov_parser *c, struct mov_stream_ctx *st)
{
	uint32_t tmp, bit_depth;

	rb16(c->stream); /* version */
	rb16(c->stream); /* revision level */
	rl32(c->stream);
	rb32(c->stream); /* temporal quality */
	rb32(c->stream); /* spatial quality */

	st->width = rb16(c->stream);
	st->height = rb16(c->stream);

	rb32(c->stream); /* horiz resolution */
	rb32(c->stream); /* vert resolution */
	rb32(c->stream); /* data size, always 0 */
	rb16(c->stream); /* frames per samples */

	stream_skip(c->stream, 32); /* compress name*/

	tmp = rb16(c->stream); /* bit depth */
	bit_depth = tmp & 0x1f;

	rb16(c->stream);
	if ((bit_depth == 1 || bit_depth == 2 || bit_depth == 4 || bit_depth == 8)) {
		logw("video is pallet");
	}

	return 0;
}

static int mov_read_stsd_audio(struct aic_mov_parser *c, struct mov_stream_ctx *st)
{
	uint16_t version = rb16(c->stream);

	rb16(c->stream); /* revision level */
	rl32(c->stream); /* vendor */

	st->channels = rb16(c->stream);
	st->bits_per_sample = rb16(c->stream);

	rb16(c->stream); /* compress id */
	rb16(c->stream); /* packet size = 0 */

	st->sample_rate = (rb32(c->stream) >> 16);

	if (!c->isom) {
		if (version == 1) {
			rb32(c->stream); /* bytes per packet */
			st->samples_per_frame = rb32(c->stream); /* sample per frame */
			st->bytes_per_frame = rb32(c->stream); /* bytes per frame */
			rb32(c->stream); /* bytes per sample */
		} else if (version == 2) {
			rb32(c->stream); /* sizeof struct only */

			union int_float64{
                            long long i;
                            double f;
                        };
                        union int_float64 v;
			v.i = rb64(c->stream);
			st->sample_rate = v.f;
			st->channels = rb32(c->stream);
			rb32(c->stream);
			st->bits_per_sample = rb32(c->stream);

			rb32(c->stream); /* lpcm format specific flag */
			st->bytes_per_frame   = rb32(c->stream);
			st->samples_per_frame = rb32(c->stream);
		}
	}

	if (st->format == 0) {
		if (st->bits_per_sample == 8) {
			st->id = mov_codec_id(st, MKTAG('r','a','w',' '));
		} else if (st->bits_per_sample == 16) {
			st->id = mov_codec_id(st, MKTAG('t','w','o','s'));
		}
	}

	st->sample_size = (st->bits_per_sample >> 3) * st->channels;
	return 0;
}

static int mov_read_stsd_entries(struct aic_mov_parser *c, int entries)
{
	int i;
	struct mov_stream_ctx *st = c->streams[c->nb_streams-1];

	for (i=0; i<entries; i++) {
		struct mov_atom a = {0x64737473, 0};
		int ret;
		int64_t start_pos = aic_stream_tell(c->stream);
		int64_t size = rb32(c->stream);
		uint32_t format = rl32(c->stream);

		if (size >= 16) {
			rb32(c->stream);
			rb16(c->stream);
			rb16(c->stream);
		} else if (size <= 7) {
			loge("invalid size %"PRId64" in stsd", size);
			return -1;
		}

		st->id = mov_codec_id(st, format);
		st->format = format;

		if (st->type == MPP_MEDIA_TYPE_VIDEO) {
			mov_read_stsd_video(c, st);
		} else if (st->type == MPP_MEDIA_TYPE_AUDIO) {
			mov_read_stsd_audio(c, st);
		}

		/* read extra atoms at the end(avcC ...)*/
		a.size = size - (aic_stream_tell(c->stream) - start_pos);
		if (a.size > 8) {
			if ((ret = mov_read_default(c, a)) < 0)
				return ret;
		} else if (a.size > 0) {
			stream_skip(c->stream, a.size);
		}

		st->stsd_count ++;
	}

	return 0;
}

static int mov_read_stsd(struct aic_mov_parser *c, struct mov_atom atom)
{
	int entries;
	struct mov_stream_ctx *st;

	if (c->nb_streams < 1)
		return 0;

	st = c->streams[c->nb_streams-1];
	st->stsd_version = r8(c->stream);
	rb24(c->stream);
	entries = rb32(c->stream);

	/* Each entry contains a size (4 bytes) and format (4 bytes). */
	if (entries <= 0 || entries > atom.size / 8 || entries > 1024) {
		loge("invalid STSD entries %d", entries);
		return -1;
	}

	if (st->extra_data) {
		loge("Duplicate stsd found in this track.");
		return -1;
    	}

	if (entries > 1) {
		logw("entries(%d) > 1", entries);
	}

	return mov_read_stsd_entries(c, entries);
}

static int mov_read_stts(struct aic_mov_parser *c, struct mov_atom atom)
{
	struct mov_stream_ctx *st = c->streams[c->nb_streams-1];
	unsigned int entries;
	unsigned int i;
	int64_t duration = 0;
	int64_t total_sample_count = 0;

	if (c->nb_streams < 1)
		return 0;

	r8(c->stream);   /* version */
	rb24(c->stream); /* flags */
	entries = rb32(c->stream);

	if (st->stts_data) {
		logw("duplicated stts data");
		mpp_free(st->stts_data);
		st->stts_data = NULL;
	}

	st->stts_count = entries;
	st->stts_data = (struct mov_stts*)mpp_alloc(entries*sizeof(struct mov_stts));

	for (i=0; i<entries; i++) {
		int sample_duration;
		unsigned int sample_count;

		sample_count = rb32(c->stream);
		sample_duration = rb32(c->stream);

		st->stts_data[i].count = sample_count;
		st->stts_data[i].duration = sample_duration;

		duration += (int64_t)sample_duration*(uint64_t)sample_count;
		total_sample_count += sample_count;
	}

	st->nb_frames = total_sample_count;
	st->duration = get_time(duration, st->time_scale);

	return 0;
}

static int mov_read_stsc(struct aic_mov_parser *c, struct mov_atom atom)
{
	unsigned int i, entries;

	if (c->nb_streams < 1)
		return 0;

	struct mov_stream_ctx *st = c->streams[c->nb_streams-1];

	r8(c->stream);
	rb24(c->stream);
	entries = rb32(c->stream);

	if (entries*12+4 > atom.size) {
		loge("stsc size error");
		return -1;
	}

	if (!entries)
		return 0;

	if (st->stsc_data) {
		logw("duplicated stsc atom");
		mpp_free(st->stsc_data);
		st->stsc_data = NULL;
	}

	st->stsc_count = entries;
	st->stsc_data = (struct mov_stsc*)mpp_alloc(entries*sizeof(struct mov_stsc));
	if (st->stsc_data == NULL)
		return -1;

	for (i=0; i<entries; i++) {
		st->stsc_data[i].first = rb32(c->stream);
		st->stsc_data[i].count = rb32(c->stream);
		st->stsc_data[i].id = rb32(c->stream);
	}

	// check stsc data valid
	for (i = st->stsc_count - 1; i < UINT_MAX; i--) {
		int64_t first_min = i + 1;
		if ((i+1 < st->stsc_count && st->stsc_data[i].first >= st->stsc_data[i+1].first) ||
		    (i > 0 && st->stsc_data[i].first <= st->stsc_data[i-1].first) ||
			st->stsc_data[i].first < first_min ||
			st->stsc_data[i].count < 1 ||
			st->stsc_data[i].id < 1) {
			logw("STSC entry %d is invalid (first=%d count=%d id=%d)\n",
				i, st->stsc_data[i].first, st->stsc_data[i].count, st->stsc_data[i].id);

			if (i+1 >= st->stsc_count) {
				if (st->stsc_data[i].count == 0 && i > 0) {
					st->stsc_count --;
					continue;
				}
				st->stsc_data[i].first = MPP_MAX(st->stsc_data[i].first, first_min);
				if (i > 0 && st->stsc_data[i].first <= st->stsc_data[i-1].first)
					st->stsc_data[i].first = MPP_MIN(st->stsc_data[i-1].first + 1LL, INT_MAX);
				st->stsc_data[i].count = MPP_MAX(st->stsc_data[i].count, 1);
				st->stsc_data[i].id    = MPP_MAX(st->stsc_data[i].id, 1);
				continue;
			}

			// We replace this entry by the next valid
			st->stsc_data[i].first = st->stsc_data[i+1].first - 1;
			st->stsc_data[i].count = st->stsc_data[i+1].count;
			st->stsc_data[i].id    = st->stsc_data[i+1].id;
		}
	}

	return 0;
}

static int mov_read_stsz(struct aic_mov_parser *c, struct mov_atom atom)
{
	unsigned int i, entries;
	unsigned int sample_size;

	if (c->nb_streams < 1)
		return 0;

	struct mov_stream_ctx *st = c->streams[c->nb_streams-1];

	r8(c->stream);
	rb24(c->stream);

	if (atom.type == MKTAG('s','t','s','z')) {
		sample_size = rb32(c->stream);
		if (!st->sample_size) /* do not overwrite value computed in stsd */
			st->sample_size = sample_size;
		st->stsz_sample_size = sample_size;
	} else {
		sample_size = 0;
		rb24(c->stream);
		r8(c->stream);  //field_size
	}

	entries = rb32(c->stream);
	if (!entries)
		return 0;

	if (sample_size) {
		st->sample_count = entries;
		return 0;
	}

	logi("read_stsz entries: %d", entries);
	if (st->sample_sizes) {
		logw("duplicated stsz data");
		mpp_free(st->sample_sizes);
		st->sample_sizes = NULL;
	}

	st->sample_sizes = mpp_alloc(entries*sizeof(*st->sample_sizes));
	if (st->sample_sizes == NULL)
		return -1;

	for (i=0; i<entries; i++) {
		st->sample_sizes[i] = rb32(c->stream);
		if (st->sample_sizes[i] < 0) {
			loge("invalid sample size: %d", st->sample_sizes[i]);
			return -1;
		}
	}
	st->sample_count = entries;

	return 0;
}

static int mov_read_stco(struct aic_mov_parser *c, struct mov_atom atom)
{
	unsigned int i, entries;

	if (c->nb_streams < 1)
		return 0;

	struct mov_stream_ctx *st = c->streams[c->nb_streams-1];

	r8(c->stream);
	rb24(c->stream);
	entries = rb32(c->stream);

	if (!entries) {
		logw("stco entries is 0");
		return 0;
	}

	if (st->chunk_offsets) {
		logw("ignore duplicated stco  atom");
		return 0;
	}

	st->chunk_offsets = mpp_alloc(entries*sizeof(*st->chunk_offsets));
	if (st->chunk_offsets == NULL)
		return -1;
	st->chunk_count = entries;

	if (atom.type == MKTAG('s','t','c','o')) {
		for (i=0; i<entries; i++) {
			st->chunk_offsets[i] = rb32(c->stream);
		}
	} else if (atom.type == MKTAG('c','o','6','4')) {
		for (i=0; i<entries; i++) {
			st->chunk_offsets[i] = rb64(c->stream);
		}
	} else {
		return -1;
	}

	return 0;
}

static void update_dts_shift(struct mov_stream_ctx *st, int duration)
{
	if (duration > 0) {
		if (duration == INT_MIN) {
			duration ++;
		}
		st->dts_shift = MPP_MAX(st->dts_shift, -duration);
	}
}

static int mov_read_ctts(struct aic_mov_parser *c, struct mov_atom atom)
{
	unsigned int i, entries;
	int total_count = 0;

	if (c->nb_streams < 1)
		return 0;

	struct mov_stream_ctx *st = c->streams[c->nb_streams-1];

	r8(c->stream);
	rb24(c->stream);
	entries = rb32(c->stream);

	if (!entries) {
		logw("ctts entries is 0");
		return 0;
	}

	if (st->ctts_data) {
		logw("duplicated ctts data");
		mpp_free(st->ctts_data);
		st->ctts_data = NULL;
	}

	st->ctts_count = 0;
	st->ctts_data = mpp_alloc(entries*sizeof(*st->ctts_data));
	if (st->ctts_data == NULL)
		return -1;

	for (i=0; i<entries; i++) {
		int count = rb32(c->stream);
		int duration = rb32(c->stream);

		total_count += count;

		if (count <= 0) {
			logi("ignore ctts entry");
			continue;
		}

		st->ctts_data[i].count = count;
		st->ctts_data[i].duration = duration;
		st->ctts_count ++;

		if (i+2 < entries) {
			update_dts_shift(st, duration);
		}
	}

	// if st->ctts_data[i].count > 1, we should relloc a large buffer for ctts
	if (total_count != st->ctts_count) {
		struct mov_stts *ctts_data_old = st->ctts_data;
		int ctts_idx = 0;
		int j;

		st->ctts_data = mpp_alloc(total_count*sizeof(*st->ctts_data));
		for (i=0; i<entries; i++) {
			for(j=0; j<ctts_data_old[i].count; j++) {
				st->ctts_data[ctts_idx].count = 1;
				st->ctts_data[ctts_idx].duration = ctts_data_old[i].duration;
				ctts_idx ++;
			}
		}

		mpp_free(ctts_data_old);
		st->ctts_count = total_count;
	}

	logi("dts shift: %d", st->dts_shift);

	return 0;
}

static int mov_read_stss(struct aic_mov_parser *c, struct mov_atom atom)
{
	unsigned int i, entries;

	if (c->nb_streams < 1)
		return 0;

	struct mov_stream_ctx *st = c->streams[c->nb_streams-1];

	r8(c->stream);
	rb24(c->stream);
	entries = rb32(c->stream);

	if (!entries) {
		logw("stts entries is 0");
		return 0;
	}

	if (st->keyframes) {
		logw("duplicated stss data");
		mpp_free(st->keyframes);
		st->keyframes = NULL;
	}

	st->keyframe_count = entries;
	st->keyframes = mpp_alloc(entries*sizeof(*st->keyframes));

	logd("keyframe_count:%d\n",st->keyframe_count);

	for (i=0; i<entries; i++) {
		st->keyframes[i] = rb32(c->stream);
		logd("st->keyframes[%d]:%d\n",i,st->keyframes[i]);
	}

	return 0;
}

static int mov_read_mdhd(struct aic_mov_parser *c, struct mov_atom atom)
{
	int version;
	struct mov_stream_ctx *st;
	if (c->nb_streams < 1)
		return 0;
	st = c->streams[c->nb_streams-1];

	if (st->time_scale) {
		loge("multi mdhd?");
		return -1;
	}

	version = r8(c->stream);
	rb24(c->stream);
	if (version > 1) {
		rb64(c->stream);
		rb64(c->stream);
	} else {
		rb32(c->stream);
		rb32(c->stream);
	}

	st->time_scale = rb32(c->stream);
	if (st->time_scale <= 0) {
		st->time_scale = c->time_scale;
	}

	st->duration = (version == 1) ? rb64(c->stream) : rb32(c->stream);

	rb16(c->stream);
	rb16(c->stream);

	return 0;
}

static int mov_read_glbl(struct aic_mov_parser *c, struct mov_atom atom)
{
	struct mov_stream_ctx *st;

	if (c->nb_streams < 1)
		return 0;
	st = c->streams[c->nb_streams-1];

	st->extra_data_size = atom.size;
	st->extra_data = (unsigned char*)mpp_alloc(atom.size);
	if (st->extra_data == NULL)
		return -1;
	memset(st->extra_data, 0, st->extra_data_size);

	aic_stream_read(c->stream, st->extra_data, st->extra_data_size);

	if (atom.type == MKTAG('h','v','c','C')) {
		loge("not support hevc");
		return -1;
	}

	return 0;
}

static int mov_read_ftyp(struct aic_mov_parser *c, struct mov_atom atom)
{
	uint8_t type[5] = {0};

	aic_stream_read(c->stream, type, 4);
	if (strcmp((char*)type, "qt  "))
		c->isom = 1;

	rb32(c->stream); // minor version

	return 0;
}

static int mov_read_mdat(struct aic_mov_parser *c, struct mov_atom atom)
{
	if (atom.size == 0)
		return 0;
	c->find_mdat = 1;
	return 0;
}

static int mp4_read_desc(struct aic_stream *s, int *tag)
{
	int len = 0;
	*tag = r8(s);

	int count = 4;
	while (count--) {
		int c = r8(s);
		len = (len << 7) | (c & 0x7f);
		if (!(c & 0x80))
			break;
	}
	return len;
}

#define MP4ESDescrTag 			0x03
#define MP4DecConfigDescrTag		0x04
#define MP4DecSpecificDescrTag		0x05
static int mp4_read_dec_config_descr(struct aic_mov_parser *c, struct mov_stream_ctx *st)
{
	enum CodecID codec_id;
	int len, tag;

	int object_type_id = r8(c->stream);
	r8(c->stream);
	rb24(c->stream);
	rb32(c->stream);
	rb32(c->stream); // avg bitrate

	codec_id = codec_get_id(mp4_obj_type, object_type_id);
	if (codec_id)
		st->id = codec_id;

	len = mp4_read_desc(c->stream, &tag);
	if (tag == MP4DecSpecificDescrTag) {
		if (len > (1<<30))
			return -1;

		st->extra_data_size = len;
		st->extra_data = (unsigned char*)mpp_alloc(len);
		if (st->extra_data == NULL)
			return -1;
		memset(st->extra_data, 0, st->extra_data_size);

		aic_stream_read(c->stream, st->extra_data, st->extra_data_size);
	}

	return 0;
}

static int mov_read_esds(struct aic_mov_parser *c, struct mov_atom atom)
{
	struct mov_stream_ctx *st;
	int tag;
	int ret = 0;

	if (c->nb_streams < 1)
		return 0;
	st = c->streams[c->nb_streams-1];
	rb32(c->stream);

	mp4_read_desc(c->stream, &tag);
	if (tag == MP4ESDescrTag) {
		rb24(c->stream);
	} else {
		rb16(c->stream);
	}

	mp4_read_desc(c->stream, &tag);
	if (tag == MP4DecConfigDescrTag) {
		mp4_read_dec_config_descr(c, st);
	}

	return ret;
}

static const struct mov_parse_table mov_default_parse_table[] = {
	{ MKTAG('c','o','6','4'), mov_read_stco },
	{ MKTAG('c','t','t','s'), mov_read_ctts }, /* composition time to sample */
	{ MKTAG('d','i','n','f'), mov_read_default },
	{ MKTAG('e','d','t','s'), mov_read_default },
	{ MKTAG('f','t','y','p'), mov_read_ftyp },
	{ MKTAG('g','l','b','l'), mov_read_glbl },
	{ MKTAG('h','d','l','r'), mov_read_hdlr },
	{ MKTAG('m','d','a','t'), mov_read_mdat },
	{ MKTAG('m','d','h','d'), mov_read_mdhd },
	{ MKTAG('m','d','i','a'), mov_read_default },
	{ MKTAG('m','i','n','f'), mov_read_default },
	{ MKTAG('m','o','o','v'), mov_read_moov },
	{ MKTAG('m','v','e','x'), mov_read_default },
	{ MKTAG('m','v','h','d'), mov_read_mvhd },
	{ MKTAG('a','v','c','C'), mov_read_glbl },
	{ MKTAG('s','t','b','l'), mov_read_default },
	{ MKTAG('s','t','c','o'), mov_read_stco },
	{ MKTAG('s','t','s','c'), mov_read_stsc },
	{ MKTAG('s','t','s','d'), mov_read_stsd }, /* sample description */
	{ MKTAG('s','t','s','s'), mov_read_stss }, /* sync sample */
	{ MKTAG('s','t','s','z'), mov_read_stsz }, /* sample size */
	{ MKTAG('s','t','t','s'), mov_read_stts },
	{ MKTAG('s','t','z','2'), mov_read_stsz }, /* compact sample size */
	{ MKTAG('t','k','h','d'), mov_read_tkhd }, /* track header */
	{ MKTAG('t','r','a','k'), mov_read_trak },
	{ MKTAG('t','r','a','f'), mov_read_default },
	{ MKTAG('t','r','e','f'), mov_read_default },
	{ MKTAG('u','d','t','a'), mov_read_default },
	//{ MKTAG('c','m','o','v'), mov_read_cmov },
	{ MKTAG('h','v','c','C'), mov_read_glbl },
	{ MKTAG('e','s','d','s'), mov_read_esds },
	{ 0, NULL }
};

static int mov_read_default(struct aic_mov_parser *c, struct mov_atom atom)
{
	int i = 0;
	int64_t total_size = 0;
	struct mov_atom a;

	if (c->atom_depth > 10) {
		loge("atom too deeply nested");
		return -1;
	}
	c->atom_depth ++;

	if (atom.size < 0)
		atom.size = INT64_MAX;
	while (total_size <= (atom.size - 8)) {
		int (*parse)(struct aic_mov_parser*, struct mov_atom) = NULL;
		a.size = atom.size;
		a.type = 0;
		if (atom.size >= 8) {
			a.size = rb32(c->stream);
			a.type = rl32(c->stream);

			total_size += 8;
			if (a.size == 1 && total_size + 8 <= atom.size) {
				// 64 bit extended size
				a.size = rb64(c->stream) - 8;
				total_size += 8;
			}
		}
		logi("atom type:%c%c%c%c, parent:%c%c%c%c, sz:" "%"PRId64"%"PRId64"%"PRId64"",
			(a.type)&0xff, (a.type>>8)&0xff,
			(a.type>>16)&0xff, (a.type>>24)&0xff,
			(atom.type)&0xff, (atom.type>>8)&0xff,
			(atom.type>>16)&0xff, (atom.type>>24)&0xff,
			a.size, total_size, atom.size);

		if (a.size == 0) {
			a.size = atom.size - total_size + 8;
		}
		if (a.size < 0)
			break;
		a.size -= 8;
		if (a.size < 0)
			break;
		if (atom.size - total_size < a.size)
			a.size = atom.size - total_size;

		for (i=0; i<mov_default_parse_table[i].type; i++) {
			if (mov_default_parse_table[i].type == a.type) {
				parse = mov_default_parse_table[i].parse;
				break;
			}
		}

		if (!parse) {
			stream_skip(c->stream, a.size);
		} else {
			int64_t start_pos = aic_stream_tell(c->stream);
			int64_t left;
			int err = parse(c, a);
			if (err < 0) {
				c->atom_depth --;
				return err;
			}

			if (c->find_moov && c->find_mdat) {
				c->atom_depth --;
				return 0;
			}

			left = a.size - aic_stream_tell(c->stream) + start_pos;
			if (left > 0) {
				/* skip garbage at atom end */
				stream_skip(c->stream, left);
			} else if (left < 0) {
				loge("overread end of atom %x", a.type);
				aic_stream_seek(c->stream, left, SEEK_CUR);
			}
		}

		total_size += a.size;
	}

	if (total_size < atom.size && atom.size < 0x7ffff)
		stream_skip(c->stream, atom.size - total_size);

	c->atom_depth --;
	return 0;
}

static struct index_entry *find_next_sample(struct aic_mov_parser *c, struct mov_stream_ctx **st)
{
	int i;
	struct index_entry *sample = NULL;
	int64_t best_dts = INT64_MAX;

	// find the sample with the smallest dts from every streams
	for (i=0; i<c->nb_streams; i++) {
		struct mov_stream_ctx *cur_st = c->streams[i];
		if (cur_st->cur_sample_idx < cur_st->nb_index_entries) {
			struct index_entry *cur_sample = &cur_st->index_entries[cur_st->cur_sample_idx];
			int64_t dts = get_time(cur_sample->timestamp, cur_st->time_scale);

			if (!sample || dts < best_dts) {
				sample = cur_sample;
				best_dts = dts;
				*st = cur_st;
			}
		}
	}

	if (*st) {
		(*st)->cur_sample_idx ++;
	}

	return sample;
}

int mov_peek_packet(struct aic_mov_parser *c, struct aic_parser_packet *pkt)
{
	struct index_entry *sample = NULL;
	struct mov_stream_ctx *st = NULL;

	sample = find_next_sample(c, &st);
	if (!sample) {
		// eos
		return PARSER_EOS;
	}

	c->cur_sample = sample;

	pkt->size = sample->size;
	pkt->type = st->type;

	if (st->cur_sample_idx == st->nb_index_entries) {
		// eos now
		printf("[%s:%d] this stream eos\n",__FUNCTION__,__LINE__);
		pkt->flag = PACKET_EOS;
	}

	pkt->pts = get_time(sample->timestamp, st->time_scale);

	if (st->ctts_data && st->ctts_index < st->ctts_count) {
		pkt->pts = sample->timestamp + st->dts_shift + st->ctts_data[st->cur_sample_idx - 1].duration;

		pkt->pts = get_time(pkt->pts, st->time_scale);
	}

	return 0;
}

int mov_close(struct aic_mov_parser *c)
{
	int i;

	for (i=0; i<c->nb_streams; i++) {
		struct mov_stream_ctx *st = c->streams[i];

		if (st->ctts_data)
			mpp_free(st->ctts_data);
		if (st->chunk_offsets)
			mpp_free(st->chunk_offsets);
		if (st->stsc_data)
			mpp_free(st->stsc_data);
		if (st->sample_sizes)
			mpp_free(st->sample_sizes);
		if (st->keyframes)
			mpp_free(st->keyframes);
		if (st->stts_data)
			mpp_free(st->stts_data);
		if (st->extra_data)
			mpp_free(st->extra_data);
		if (st->index_entries)
			mpp_free(st->index_entries);

		mpp_free(st);
	}

	return 0;
}

int mov_read_header(struct aic_mov_parser *c)
{
	int err;
	struct mov_atom atom = {0, 0};

	atom.size = aic_stream_size(c->stream);
	do {
		err = mov_read_default(c, atom);
		if(err < 0) {
			loge("error reading header");
			return -1;
		}
	} while (!c->find_moov);

	if(!c->find_moov) {
		loge("moov atom not find");
		return -1;
	}

	return 0;
}

int find_index_by_pts(struct mov_stream_ctx *st,s64 pts)
{
	int i,index = 0;
	int64_t min = INT64_MAX;
	int64_t sample_pts;
	int64_t diff;
	struct index_entry *cur_sample = NULL;

	for (i = 0; i < st->nb_index_entries; i++) {
		cur_sample = &st->index_entries[i];
		sample_pts = get_time(cur_sample->timestamp, st->time_scale);
		diff = MPP_ABS(pts,sample_pts);
		if (diff < min) {
			min = diff;
			index = i;
		}
	}

	return index;
}

int  find_video_index_by_pts(struct mov_stream_ctx *video_st,s64 pts)
{
	int i,k,index = 0;
	int64_t min = INT64_MAX;
	int64_t sample_pts;
	int64_t diff;
	struct index_entry *cur_sample = NULL;

	for (i = 0 ;i < video_st->keyframe_count;i++) {
		k = video_st->keyframes[i]-1;
		cur_sample = &video_st->index_entries[k];
		sample_pts = get_time(cur_sample->timestamp, video_st->time_scale);
		diff = MPP_ABS(pts,sample_pts);
		if (diff < min) {
			min = diff;
			index = k;
		}
		logd("keyfame:%d,pts:%ld\n",k,sample_pts);
	}
	return index;
}

int mov_seek_packet(struct aic_mov_parser *c, s64 pts)
{
	int i;
	struct mov_stream_ctx *cur_st = NULL;
	struct mov_stream_ctx *video_st = NULL;
	struct mov_stream_ctx *audio_st = NULL;
	struct index_entry *cur_sample = NULL;

	for (i = 0; i< c->nb_streams ;i++) {
		cur_st = c->streams[i];
		if ((!video_st) && (cur_st->type == MPP_MEDIA_TYPE_VIDEO)) {//only support first video stream
			video_st = c->streams[i];
		} else if ((!audio_st) && (cur_st->type == MPP_MEDIA_TYPE_AUDIO)) {//only support first audio stream
			audio_st = c->streams[i];
		}
	}

	if (video_st) {
		if(video_st->id == CODEC_ID_MJPEG) {
			video_st->cur_sample_idx = find_index_by_pts(video_st,pts);
		} else {
			if (!video_st->keyframes) {
				loge("no keyframes\n");
				return -1;
			}
			video_st->cur_sample_idx = find_video_index_by_pts(video_st,pts);
		}
	}

	if (audio_st) {
		int64_t tmp;
		tmp = pts;
		if (video_st) {
			cur_sample = &video_st->index_entries[video_st->cur_sample_idx];
			tmp = get_time(cur_sample->timestamp, video_st->time_scale);
		}
		audio_st->cur_sample_idx = find_index_by_pts(audio_st,tmp);
	}

	return 0;
}
