/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: mp3_parser
 */

#include <stdlib.h>
#include <inttypes.h>
#include "aic_stream.h"
#include "mp3.h"
#include "mpp_mem.h"
#include "mpp_log.h"

const uint16_t avpriv_mpa_freq_tab[3] = { 44100, 48000, 32000 };

const uint16_t avpriv_mpa_bitrate_tab[2][3][15] = {
	{ {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448 },
		{0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384 },
		{0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320 } },
	{ {0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256},
		{0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160},
		{0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160}
	}
};

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

static int id3v2_match(const uint8_t *buf, const char *magic)
{
	return  buf[0]         == magic[0] &&
			buf[1]         == magic[1] &&
			buf[2]         == magic[2] &&
			buf[3]         != 0xff     &&
			buf[4]         != 0xff     &&
			(buf[6] & 0x80) == 0        &&
			(buf[7] & 0x80) == 0        &&
			(buf[8] & 0x80) == 0        &&
			(buf[9] & 0x80) == 0;
}

/* fast header check for resync */
static int mpa_check_header(uint32_t header)
{
	/* header */
	if ((header & 0xffe00000) != 0xffe00000)
		return -1;
	/* version check */
	if ((header & (3<<19)) == 1<<19)
		return -1;
	/* layer check */
	if ((header & (3<<17)) == 0)
		return -1;
	/* bit rate */
	if ((header & (0xf<<12)) == 0xf<<12)
		return -1;
	/* frequency */
	if ((header & (3<<10)) == 3<<10)
		return -1;
	return 0;
}

int mpegaudio_decode_header(struct mpa_decode_header *s, uint32_t header)
{
	int sample_rate, frame_size, mpeg25, padding;
	int sample_rate_index, bitrate_index;
	int ret;

	ret = mpa_check_header(header);
	if (ret < 0)
		return ret;

	if (header & (1<<20)) {
		s->lsf = (header & (1<<19)) ? 0 : 1;
		mpeg25 = 0;
	} else {
		s->lsf = 1;
		mpeg25 = 1;
	}

	s->layer = 4 - ((header >> 17) & 3);

	/* extract frequency */
	sample_rate_index = (header >> 10) & 3;
	if (sample_rate_index >= ARRAY_ELEMS(avpriv_mpa_freq_tab))
		sample_rate_index = 0;
	sample_rate = avpriv_mpa_freq_tab[sample_rate_index] >> (s->lsf + mpeg25);
	sample_rate_index += 3 * (s->lsf + mpeg25);
	s->sample_rate_index = sample_rate_index;
	s->error_protection = ((header >> 16) & 1) ^ 1;
	s->sample_rate = sample_rate;

	s->nb_samples_per_frame = 1152;
	if (s->layer == 1) {
		s->nb_samples_per_frame = 384;
	} else if(s->layer == 3 && s->lsf) {
		s->nb_samples_per_frame = 576;
	} else {
		s->nb_samples_per_frame = 1152;
	}
	s->frame_duration = s->nb_samples_per_frame * 1000 * 1000 / s->sample_rate;

	bitrate_index = (header >> 12) & 0xf;
	padding = (header >> 9) & 1;
	//extension = (header >> 8) & 1;
	s->mode = (header >> 6) & 3;
	s->mode_ext = (header >> 4) & 3;
	//copyright = (header >> 3) & 1;
	//original = (header >> 2) & 1;
	//emphasis = header & 3;

	if (s->mode == MPA_MONO)
		s->nb_channels = 1;
	else
		s->nb_channels = 2;

	if (bitrate_index != 0) {
		frame_size = avpriv_mpa_bitrate_tab[s->lsf][s->layer - 1][bitrate_index];
		s->bit_rate = frame_size * 1000;
		switch(s->layer) {
		case 1:
			frame_size = (frame_size * 12000) / sample_rate;
			frame_size = (frame_size + padding) * 4;
			break;
		case 2:
			frame_size = (frame_size * 144000) / sample_rate;
			frame_size += padding;
			break;
		default:
		case 3:
			frame_size = (frame_size * 144000) / (sample_rate << s->lsf);
			frame_size += padding;
			break;
		}
		s->frame_size = frame_size;
	} else {
		/* if no frame size computed, signal it */
		return -1;
	}
	return 0;
}

static int check(struct aic_stream* stream, int64_t pos, uint32_t *ret_header)
{
	int64_t ret;
	uint8_t header_buf[4];
	unsigned header;
	struct mpa_decode_header sd;

	ret = aic_stream_seek(stream, pos, SEEK_SET);
	if (ret < 0)
		return CHECK_SEEK_FAILED;

	ret = aic_stream_read(stream, &header_buf[0], 4);
	/* We should always find four bytes for a valid mpa header. */
	if (ret < 4)
		return CHECK_SEEK_FAILED;

	//header = AV_RB32(&header_buf[0]);
	header = MKBETAG(header_buf[0],header_buf[1],header_buf[2],header_buf[3]);
	if (mpa_check_header(header) < 0)
		return CHECK_WRONG_HEADER;
	if (mpegaudio_decode_header(&sd, header) == 1)
		return CHECK_WRONG_HEADER;

	if (ret_header)
		*ret_header = header;
	return sd.frame_size;
}

static void mp3_parse_info_tag(struct aic_mp3_parser *s, uint32_t spf)
{
	uint32_t v;
	char version[10];
	struct mp3_dec_context * mp3 = &s->ctx;
	static const int64_t xing_offtbl[2][2] = {{32, 17}, {17,9}};
	uint64_t fsize = aic_stream_size(s->stream);
	fsize = fsize >= aic_stream_tell(s->stream) ? fsize - aic_stream_tell(s->stream) : 0;

	/* Check for Xing / Info tag */
	stream_skip(s->stream, xing_offtbl[s->header.lsf == 1][s->header.nb_channels == 1]);
	v = rb32(s->stream);
	mp3->is_cbr = v == MKBETAG('I', 'n', 'f', 'o');
	if (v != MKBETAG('X', 'i', 'n', 'g') && !mp3->is_cbr)
		return;

	v = rb32(s->stream);
	if (v & XING_FLAG_FRAMES)
		mp3->frames = rb32(s->stream);
	if (v & XING_FLAG_SIZE)
		mp3->header_filesize = rb32(s->stream);
	if (fsize && mp3->header_filesize) {
		uint64_t min, delta;
		min = MPP_MIN(fsize, mp3->header_filesize);
		delta = MPP_MAX(fsize, mp3->header_filesize) - min;
		if (fsize > mp3->header_filesize && delta > min >> 4) {
			mp3->frames = 0;
			logw("invalid concatenated file detected - using bitrate for duration\n");
		} else if (delta > min >> 4) {
			logw("filesize and duration do not match (growing file?)\n");
		}
	}
	if (v & XING_FLAG_TOC){
		aic_stream_read(s->stream, mp3->xing_toc_index, XING_TOC_COUNT);
		mp3->xing_toc = 1;
	}

	/* VBR quality */
	if (v & XING_FLAC_QSCALE)
		rb32(s->stream);

	/* Encoder short version string */
	memset(version, 0, sizeof(version));
	aic_stream_read(s->stream, version, 9);
	/* Info Tag revision + VBR method */
	r8(s->stream);
	/* Lowpass filter value */
	r8(s->stream);
	/* ReplayGain peak */
	v    = rb32(s->stream);
	/* Radio ReplayGain */
	v = rb16(s->stream);
	/* Audiophile ReplayGain */
	v = rb16(s->stream);
	/* Encoding flags + ATH Type */
	r8(s->stream);
	/* if ABR {specified bitrate} else {minimal bitrate} */
	r8(s->stream);
	/* Encoder delays */
	v = rb24(s->stream);
	if (MKBETAG(version[0],version[1],version[2],version[3]) == MKBETAG('L', 'A', 'M', 'E')
		|| MKBETAG(version[0],version[1],version[2],version[3])  == MKBETAG('L', 'a', 'v', 'f')
		|| MKBETAG(version[0],version[1],version[2],version[3])  == MKBETAG('L', 'a', 'v', 'c')
	) {
		mp3->start_pad = v>>12;
		mp3->  end_pad = v&4095;
	}
	/* Misc */
	r8(s->stream);
	/* MP3 gain */
	r8(s->stream);
	/* Preset and surround info */
	rb16(s->stream);
	/* Music length */
	rb32(s->stream);
	/* Music CRC */
	rb16(s->stream);
	/* Info Tag CRC */
	v = rb16(s->stream);
}

static void mp3_parse_vbri_tag(struct aic_mp3_parser *s, int64_t base)
{
    uint32_t v;
    struct mp3_dec_context * mp3 = &s->ctx;

    /* Check for VBRI tag (always 32 bytes after end of mpegaudio header) */
    aic_stream_seek(s->stream, base + 4 + 32, SEEK_SET);
    v = rb32(s->stream);
    if (v == MKBETAG('V', 'B', 'R', 'I')) {
        /* Check tag version */
        if (rb16(s->stream) == 1) {
            /* skip delay and quality */
            stream_skip(s->stream, 4);
            mp3->header_filesize = rb32(s->stream);
            mp3->frames = rb32(s->stream);
        }
    }
}

static int mp3_parse_vbr_tags(struct aic_mp3_parser *s, int64_t base)
{
	uint32_t v, spf;
	int vbrtag_size = 0;
	struct mp3_dec_context *mp3 = &s->ctx;

	v = rb32(s->stream);
	if(mpegaudio_decode_header(&s->header,v)){
		loge("error header");
		return -1;
	}

	vbrtag_size = s->header.frame_size; /*size after encoding*/
	spf = s->header.lsf ? 576 : 1152;  /* Samples per frame, layer 3 ,before encoding */

	mp3->frames = 0;
    mp3->header_filesize   = 0;
	mp3_parse_info_tag(s, spf);
	mp3_parse_vbri_tag(s, base);

	if (!mp3->frames && !mp3->header_filesize){
		return -1;
	}

	printf("[%s:%d]frames:%u,header_filesize:%u\n",__FUNCTION__,__LINE__,mp3->frames,mp3->header_filesize);

	/* Skip the vbr tag frame */
	aic_stream_seek(s->stream, base + vbrtag_size, SEEK_SET);

	//calculate total duration

	if (mp3->frames) {
		s->duration = mp3->frames * spf / s->header.sample_rate * 1000 * 1000;
		printf("[%s:%d]sample_rate:%d,spf:%d\n",__FUNCTION__,__LINE__,s->header.sample_rate,spf);
	}
	//calculate avg bit rate
	if (mp3->header_filesize && mp3->frames && !mp3->is_cbr) {
		//s->header.bit_rate = mp3->header_filesize/st->duration;
	}

	return 0;
}

/*
	mp3 file struct
	| ID3V2 (may not contain)| frame_0, frame_1, ..., frame_n | ID3V1 |
*/

int mp3_read_header(struct aic_mp3_parser *s)
{
	int found_header;
	int64_t off;
	unsigned char id3v2_header[ID3v2_HEADER_SIZE];
	int len = 0;
	int i =0;
	int ret;

	// proccess idv32 here,now do nothing,skip idv32
	aic_stream_read(s->stream, id3v2_header, 10);
	found_header = id3v2_match(id3v2_header, ID3v2_DEFAULT_MAGIC);
	if (found_header) {
		len = ((id3v2_header[6] & 0x7f) << 21) | ((id3v2_header[7] & 0x7f) << 14) | ((id3v2_header[8] & 0x7f) << 7) |(id3v2_header[9] & 0x7f);
		aic_stream_seek(s->stream, len, SEEK_CUR);//skip id3v2 header
	} else {
		printf("this file do not contian id3v2\n");
		aic_stream_seek(s->stream, 0, SEEK_SET);
	}

	off = aic_stream_tell(s->stream);

	if (mp3_parse_vbr_tags(s, off) < 0){
		aic_stream_seek(s->stream, off, SEEK_SET);
	}

	off = aic_stream_tell(s->stream);

	for (i = 0; i < 64 * 1024; i++) {
		uint32_t header, header2;
		int frame_size;
		frame_size = check(s->stream, off + i, &header);
		if (frame_size > 0) {
			ret = aic_stream_seek(s->stream, off, SEEK_SET);
			if (ret < 0)
				return ret;
			ret = check(s->stream, off + i + frame_size, &header2);
			if (ret >= 0 &&
				(header & MP3_MASK) == (header2 & MP3_MASK))
			{
				logw("Skipping %d bytes of junk at %"PRId64".\n", i, off);
				ret = aic_stream_seek(s->stream, off + i, SEEK_SET);
				if (ret < 0)
					return ret;
				break;
			} else if (ret == CHECK_SEEK_FAILED) {
				logw("Invalid frame size (%d): Could not seek to %"PRId64".\n", frame_size, off + i + frame_size);
				return -1;
			}
		} else if (frame_size == CHECK_SEEK_FAILED) {
			logw( "Failed to read frame size: Could not seek to %"PRId64".\n",(int64_t) (i + 1024 + frame_size + 4));
			return -1;
		}
		ret = aic_stream_seek(s->stream, off, SEEK_SET);
		if (ret < 0)
			return ret;
	}

	s->first_packet_pos = aic_stream_tell(s->stream);
	s->ctx.filesize =  aic_stream_size(s->stream);
	if (!s->duration) {
		s->duration = 1000 * 1000 * (s->ctx.filesize - s->first_packet_pos - ID3v1_TAG_SIZE ) / (s->header.bit_rate/8);
		printf("[%s:%d]filesize:%"PRId64",duration:%"PRId64"\n",__FUNCTION__,__LINE__,s->ctx.filesize,s->duration);
		if(!s->ctx.is_cbr){
			logw("this is VBR");
		}
	}
	printf("[%s:%d]filesize:%"PRId64",duration:%"PRId64"\n",__FUNCTION__,__LINE__,s->ctx.filesize,s->duration);
	return 0;
}

int mp3_close(struct aic_mp3_parser *c)
{
	return 0;
}

#define SEEK_WINDOW 4096

static int64_t mp3_sync(struct aic_mp3_parser *s, int64_t target_pos)
{
	int dir = 1;
	int64_t best_pos;
	int best_score, i, j;
	int64_t ret;

	ret = aic_stream_seek(s->stream, target_pos, SEEK_SET);
	if (ret < 0)
		return ret;

	best_pos = target_pos;
	best_score = 999;
	for (i = 0; i < SEEK_WINDOW; i++) {
		int64_t pos = target_pos + (dir > 0 ? i - SEEK_WINDOW/4 : -i);
		int64_t candidate = -1;
		int score = 999;

		if (pos < 0)
			continue;
	#define MIN_VALID 3
		for (j = 0; j < MIN_VALID; j++) {
			ret = check(s->stream, pos, NULL);
			if (ret < 0) {
				if (ret == CHECK_WRONG_HEADER) {
					break;
				} else if (ret == CHECK_SEEK_FAILED) {
					loge("Could not seek to %"PRId64".\n", pos);
					return -1;
				}
			}
			if ((target_pos - pos)*dir <= 0 && FFABS(MIN_VALID/2-j) < score) {
				candidate = pos;
				score = FFABS(MIN_VALID/2-j);
			}
			pos += ret;
		}
		if (best_score > score && j == MIN_VALID) {
			best_pos = candidate;
			best_score = score;
			if(score == 0)
				break;
		}
	}
	return aic_stream_seek(s->stream, best_pos, SEEK_SET);
}

int mp3_seek_packet(struct aic_mp3_parser *s, s64 seek_time)
{
	struct mp3_dec_context *mp3 = &s->ctx;
	int64_t best_pos;
	int64_t pos;
	printf("[%s:%d]%"PRId64"\n",__FUNCTION__,__LINE__,seek_time);
	if (mp3->xing_toc && mp3->header_filesize && !mp3->is_cbr) {
		// NOTE: The MP3 TOC is not a precise lookup table. Accuracy is worse
		//for bigger files.
		logw("Using MP3 TOC to seek; may be imprecise.\n");
		int toc_index = seek_time * 100 / s->duration;
		toc_index = (toc_index < 0)?(0):(toc_index);
		toc_index = (toc_index > 99)?(99):(toc_index);
		pos = mp3->header_filesize * mp3->xing_toc_index[toc_index]/256 + s->first_packet_pos;
		printf("[%s:%d]%d,%d,%u,%u\n",__FUNCTION__,__LINE__,toc_index,mp3->xing_toc_index[toc_index],mp3->header_filesize,s->first_packet_pos);
	} else {
		if (!mp3->is_cbr) {
			logw("Using scaling to seek VBR MP3; may be imprecise.\n");
		}
		pos = (seek_time / 1000 / 1000) * s->header.bit_rate / 8  + s->first_packet_pos ;
		printf("[%s:%d]%"PRId64",%u,%u\n",__FUNCTION__,__LINE__,pos,mp3->header_filesize,s->first_packet_pos);
	}

	s->frame_id = seek_time / s->header.frame_duration;

	printf("[%s:%d]frame_id:%u\n",__FUNCTION__,__LINE__,s->frame_id);

	best_pos = mp3_sync(s, pos);
	if (best_pos < 0)
		return best_pos;

	return 0;
}

//#define MP3_PACKET_SIZE 1024

int mp3_peek_packet(struct aic_mp3_parser *s, struct aic_parser_packet *pkt)
{
	int64_t pos;
	struct mp3_dec_context *mp3 = &s->ctx;

#ifdef MP3_PACKET_SIZE
#else
	uint32_t v;
	struct mpa_decode_header h;
#endif

	pos = aic_stream_tell(s->stream);
	if (pos >= mp3->filesize - ID3v1_TAG_SIZE) {
		printf("[%s:%d]PARSER_EOS,%"PRId64",%"PRId64"\n",__FUNCTION__,__LINE__,pos,mp3->filesize);
		return PARSER_EOS;
	}

#ifdef MP3_PACKET_SIZE
	pkt->size = MP3_PACKET_SIZE;
#else
	// check header
	v = rb32(s->stream);
	if(mpa_check_header(v) < 0){
		loge("frame header error,resync header");
		if(mp3_sync(s,pos) < 0){
			loge("resync header error");
			return -1;
		}
		pos = aic_stream_tell(s->stream);
		v = rb32(s->stream);
	}
	aic_stream_seek(s->stream,-4,SEEK_CUR);

	// analize head get cur frame size
	mpegaudio_decode_header(&h,v);
	pkt->size = h.frame_size;
	pkt->pts = s->frame_id * s->header.frame_duration;
#endif
	pkt->type = MPP_MEDIA_TYPE_AUDIO;

	return 0;
}

int mp3_read_packet(struct aic_mp3_parser *s, struct aic_parser_packet *pkt)
{
    struct mp3_dec_context *mp3 = &s->ctx;
    int64_t pos;
	pos = aic_stream_tell(s->stream);
	if (pos >= mp3->filesize - ID3v1_TAG_SIZE) {
		return PARSER_EOS;
	}

#ifdef MP3_PACKET_SIZE
	pkt->size = aic_stream_read(s->stream,pkt->data,MP3_PACKET_SIZE);
#else
	aic_stream_read(s->stream,pkt->data,pkt->size);
#endif
	pos = aic_stream_tell(s->stream);
	if (pos >= mp3->filesize - ID3v1_TAG_SIZE) {
		pkt->flag |= PACKET_EOS;
	}
	s->frame_id++;
	return 0;
}
