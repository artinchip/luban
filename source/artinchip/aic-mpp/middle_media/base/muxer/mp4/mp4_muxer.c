/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: mp4_muxer
*/
#include <string.h>
#include "mp4_muxer.h"
#include "mpp_log.h"
#include "mpp_mem.h"
#include "mpp_dec_type.h"
#include "aic_stream.h"
#include "aic_middle_media_common.h"

#define MODE_MP4  0x01
#define MOV_TIMESCALE 1000
#define AV_NOPTS_VALUE -1

static void w8(struct aic_stream *s, int b)
{
	aic_stream_write(s, &b, 1);
}

static void wb16(struct aic_stream *s, unsigned int val)
{
	w8(s, (int)val >> 8);
	w8(s, (uint8_t)val);
}

static void wb24(struct aic_stream *s, unsigned int val)
{
	wb16(s, (int)val >> 8);
	w8(s, (uint8_t)val);
}

static void wl32(struct aic_stream *s, unsigned int val)
{
	w8(s, (uint8_t) val);
	w8(s, (uint8_t)(val >> 8));
	w8(s, (uint8_t)(val >> 16));
	w8(s,           val >> 24);
}

static void wb32(struct aic_stream *s, unsigned int val)
{
	w8(s,           val >> 24);
	w8(s, (uint8_t)(val >> 16));
	w8(s, (uint8_t)(val >> 8));
	w8(s, (uint8_t) val);
}

static void wb64(struct aic_stream *s, uint64_t val)
{
	wb32(s, (uint32_t)(val >> 32));
	wb32(s, (uint32_t)(val & 0xffffffff));
}

static void wfourcc(struct aic_stream *s, const char *c)
{
	wl32(s, MKTAG(c[0], c[1], c[2], c[3]));
}

//FIXME support 64 bit variant with wide placeholders
static int64_t update_size(struct aic_stream  *s, int64_t pos)
{
	int64_t curpos = aic_stream_tell(s);

	aic_stream_seek(s, pos, SEEK_SET);
	wb32(s, curpos - pos); /* rewrite size */
	aic_stream_seek(s, curpos, SEEK_SET);
	return curpos - pos;
}

static int mov_write_ftyp_tag(struct aic_mov_muxer *s)
{
	int minor = 0x200;
	struct aic_stream* pb = s->stream;
	int64_t pos = aic_stream_tell(pb);

	wb32(pb, 0); /* size */
	wfourcc(pb, "ftyp");
	//MODE_MP4
	wfourcc(pb, "isom");
	wb32(pb, minor);
	//MODE_MP4
	wfourcc(pb, "isom");
	wfourcc(pb, "iso2");
	//MODE_MP4
	wfourcc(pb, "mp41");
	update_size(pb,pos);
	return 0;
}

static int mov_write_mdat_tag(struct aic_mov_muxer *s)
{
	wb32(s->stream, 8);    // placeholder for extended size field (64 bit)
	wfourcc(s->stream, "free");
	s->mdat_pos = aic_stream_tell(s->stream);
	wb32(s->stream, 0); /* size placeholder*/
	wfourcc(s->stream, "mdat");
	return 0;
}

static int64_t calc_pts_duration(struct aic_mov_muxer *s, struct mov_track *track)
{
	if (track->end_pts != AV_NOPTS_VALUE &&
		track->start_pts != AV_NOPTS_VALUE) {
		return track->end_pts - track->start_pts;
	}
	return track->track_duration;
}

static int mov_write_mdhd_tag(struct aic_mov_muxer *s,struct mov_track *track)
{
	int64_t duration = calc_pts_duration(s, track);
	int version = 0;
	struct aic_stream* pb = s->stream;

	wb32(pb, 32); /* size */
	wfourcc(pb, "mdhd");
	w8(pb, version);
	wb24(pb, 0); /* flags */
	wb32(pb, track->time); /* creation time */
	wb32(pb, track->time); /* modification time */
	/*maybe wrong */
	wb32(pb, track->timescale); /* time scale (sample rate for audio) */
	wb32(pb, duration); /* duration */
	wb16(pb, track->language); /* language */
	wb16(pb, 0); /* reserved (quality) */
	return 32;
}

static int mov_write_hdlr_tag(struct aic_mov_muxer *s,struct mov_track *track)
{
	const char *hdlr, *descr = NULL, *hdlr_type = NULL;
	struct aic_stream* pb = s->stream;
	int64_t pos = aic_stream_tell(pb);

	if (track) {
		hdlr = "\0\0\0\0";
		if (track->codec_para.codec_type == MPP_MEDIA_TYPE_VIDEO) {
			hdlr_type = "vide";
			descr     = "VideoHandler";
		} else if (track->codec_para.codec_type == MPP_MEDIA_TYPE_AUDIO) {
			hdlr_type = "soun";
			descr     = "SoundHandler";
		}
	}

	wb32(pb, 0); /* size */
	wfourcc(pb, "hdlr");
	wb32(pb, 0); /* Version & flags */
	aic_stream_write(pb, (void *)hdlr, 4); /* handler */
	wfourcc(pb, hdlr_type); /* handler type */
	wb32(pb, 0); /* reserved */
	wb32(pb, 0); /* reserved */
	wb32(pb, 0); /* reserved */
	//w8(pb, strlen(descr)); /* pascal string */
	aic_stream_write(pb, (void *)descr, strlen(descr)); /* handler description */
	w8(pb, 0); /* c string */
	return update_size(pb, pos);
}

static int mov_write_vmhd_tag(struct aic_mov_muxer *s)
{
	struct aic_stream* pb = s->stream;

	wb32(pb, 0x14); /* size (always 0x14) */
	wfourcc(pb, "vmhd");
	wb32(pb, 0x01); /* version & flags */
	wb64(pb, 0); /* reserved (graphics mode = copy) */
	return 0x14;
}

static int mov_write_smhd_tag(struct aic_mov_muxer *s)
{
	struct aic_stream* pb = s->stream;

	wb32(pb, 16); /* size */
	wfourcc(pb, "smhd");
	wb32(pb, 0); /* version & flags */
	wb16(pb, 0); /* reserved (balance, normally = 0) */
	wb16(pb, 0); /* reserved */
	return 16;
}

static int mov_write_dref_tag(struct aic_mov_muxer *s)
{
	struct aic_stream* pb = s->stream;

	wb32(pb, 28); /* size */
	wfourcc(pb, "dref");
	wb32(pb, 0); /* version & flags */
	wb32(pb, 1); /* entry count */
	wb32(pb, 0xc); /* size */
	//FIXME add the alis and rsrc atom
	wfourcc(pb, "url ");
	wb32(pb, 1); /* version & flags */

	return 28;
}

static int mov_write_dinf_tag(struct aic_mov_muxer *s)
{
	struct aic_stream* pb = s->stream;
	int64_t pos = aic_stream_tell(pb);

	wb32(pb, 0); /* size */
	wfourcc(pb, "dinf");
	mov_write_dref_tag(s);
	return update_size(pb, pos);
}

static void put_descr(struct aic_stream* pb, int tag, unsigned int size)
{
	int i = 3;

	w8(pb, tag);
	for (; i > 0; i--)
		w8(pb, (size >> (7 * i)) | 0x80);
	w8(pb, size & 0x7F);
}

static unsigned compute_avg_bitrate(struct mov_track *track)
{
	uint64_t size = 0;
	int i;

	if (!track->track_duration)
		return 0;
	for (i = 0; i < track->entry; i++)
		size += track->cluster[i].size;
	return size * 8 * track->timescale / track->track_duration;
}

const struct codec_tag ff_mp4_obj_type[] = {
	{ CODEC_ID_MP3         , 0x69 }, /* 13818-3 */
	{ CODEC_ID_MJPEG       , 0x6C }, /* 10918-1 */
	{ CODEC_ID_NONE        ,    0 },
};

unsigned int ff_codec_get_tag(const struct codec_tag *tags, enum CodecID id)
{
	while (tags->id != CODEC_ID_NONE) {
		if (tags->id == id)
			return tags->tag;
		tags++;
	}
	return 0;
}
static int mov_write_esds_tag(struct aic_mov_muxer *s,struct mov_track *track)
{
	struct aic_stream* pb = s->stream;
	int64_t pos = aic_stream_tell(pb);
	int decoder_specific_info_len = 0;
	unsigned avg_bitrate;

	wb32(pb, 0); // size
	wfourcc(pb, "esds");
	wb32(pb, 0); // Version

	// ES descriptor
	put_descr(pb, 0x03, 3 + 5+13 + decoder_specific_info_len + 5+1);
	wb16(pb, track->track_id);
	w8(pb, 0x00); // flags (= no flags)

	// DecoderConfig descriptor
	put_descr(pb, 0x04, 13 + decoder_specific_info_len);
	w8(pb, ff_codec_get_tag(ff_mp4_obj_type, track->codec_para.codec_id));
	if (track->codec_para.codec_type == MPP_MEDIA_TYPE_AUDIO)
		w8(pb, 0x15); // flags (= Audiostream)
	else
		w8(pb, 0x11); // flags (= Visualstream)

	avg_bitrate = compute_avg_bitrate(track);
	wb32(pb, avg_bitrate);
	wb32(pb, avg_bitrate);
	put_descr(pb, 0x06, 1);
	w8(pb, 0x02);
	return update_size(pb, pos);
}
static int mov_write_video_tag(struct aic_mov_muxer *s,struct mov_track *track)
{
	struct aic_stream* pb = s->stream;
	int64_t pos = aic_stream_tell(pb);
	char compressor_name[32] = "Lavc59.37.100 mjpeg";

	//char compressor_name[32] = {0};
	wb32(pb, 0); /* size */
	wl32(pb, track->tag); // store it byteswapped
	wb32(pb, 0); /* Reserved */
	wb16(pb, 0); /* Reserved */
	wb16(pb, 1); /* Data-reference index */

	wb16(pb, 0); /* Codec stream version */

	wb16(pb, 0); /* Codec stream revision (=0) */
	wb32(pb, 0); /* Reserved */
	wb32(pb, 0); /* Reserved */
	wb32(pb, 0); /* Reserved */

	wb16(pb, track->codec_para.width); /* Video width */
	wb16(pb, track->codec_para.height); /* Video height */
	wb32(pb, 0x00480000); /* Horizontal resolution 72dpi */
	wb32(pb, 0x00480000); /* Vertical resolution 72dpi */
	wb32(pb, 0); /* Data size (= 0) */
	wb16(pb, 1); /* Frame count (= 1) */

	/* FIXME not sure, ISO 14496-1 draft where it shall be set to 0 */
	//find_compressor(compressor_name, 32, track);
	w8(pb, strlen(compressor_name));
	aic_stream_write(pb, compressor_name, 31);

	wb16(pb, 0x18); /* Reserved */

	wb16(pb, 0xffff); /* Reserved */

	mov_write_esds_tag(s, track);
	//mov_write_fiel_tag(pb, track, AV_FIELD_UNKNOWN);
	//mov_write_clli_tag(pb, track);
	//mov_write_mdcv_tag(pb, track);

	return update_size(pb, pos);
}

static int mov_write_audio_tag(struct aic_mov_muxer *s,struct mov_track *track)
{
	struct aic_stream* pb = s->stream;
	int64_t pos = aic_stream_tell(pb);
	int version = 0;
	uint32_t tag = track->tag;
	int ret = 0;

	wb32(pb, 0); /* size */
	wl32(pb, tag); // store it byteswapped
	wb32(pb, 0); /* Reserved */
	wb16(pb, 0); /* Reserved */
	wb16(pb, 1); /* Data-reference index, XXX  == 1 */

	/* SoundDescription */
	wb16(pb, version); /* Version */
	wb16(pb, 0); /* Revision level */
	wb32(pb, 0); /* Reserved */

	/*channels*/
	wb16(pb, 2);
	/*bits_per_raw_sample*/
	wb16(pb, 16);

	wb16(pb, 0);
	wb16(pb, 0); /* packet size (= 0) */
	/*sample_rate*/
	wb16(pb, track->codec_para.sample_rate <= UINT16_MAX ?
					track->codec_para.sample_rate : 0);

	wb16(pb, 0); /* Reserved */

	// ret = mov_write_esds_tag(pb, track);

	// if (ret < 0)
	// 	return ret;

	ret = update_size(pb, pos);
	return ret;
}

static int mov_write_stsd_tag(struct aic_mov_muxer *s,struct mov_track *track)
{
	struct aic_stream* pb = s->stream;
	int64_t pos = aic_stream_tell(pb);
	int ret = 0;

	wb32(pb, 0); /* size */
	wfourcc(pb, "stsd");
	wb32(pb, 0); /* version & flags */
	wb32(pb, 1); /* entry count */
	if (track->codec_para.codec_type == MPP_MEDIA_TYPE_VIDEO)
		ret = mov_write_video_tag(s,track);
	else if (track->codec_para.codec_type == MPP_MEDIA_TYPE_AUDIO)
		ret = mov_write_audio_tag(s,track);

	if (ret < 0)
		return ret;

	return update_size(pb, pos);
}

static int get_cluster_duration(struct mov_track *track, int cluster_idx)
{
	int64_t next_pts;

	if (cluster_idx >= track->entry)
		return 0;

	if (cluster_idx + 1 == track->entry) {
		next_pts = track->track_duration + track->start_pts;
		logd("next_dts:%ld,track_duration:%ld,start_pts:%ld,dts:%ld",next_pts,track->track_duration,track->start_pts,track->cluster[cluster_idx].pts);
	} else {
		next_pts = track->cluster[cluster_idx + 1].pts;
	}
	next_pts -= track->cluster[cluster_idx].pts;

	return next_pts;
}

static int mov_write_stts_tag(struct aic_mov_muxer *s,struct mov_track *track)
{
	struct aic_stream* pb = s->stream;
	struct mov_stts *stts_entries = NULL;
	uint32_t entries = -1;
	uint32_t atom_size;
	int i;

	if (track->codec_para.codec_type == MPP_MEDIA_TYPE_AUDIO) {
		stts_entries = mpp_alloc(sizeof(*stts_entries)); /* one entry */
		if (!stts_entries) {
			loge("ENOMEM");
			return -1;
		}
		stts_entries[0].count = track->sample_count;
		stts_entries[0].duration = 1;
		entries = 1;
	} else {
		if (track->entry) {
			stts_entries = mpp_alloc(track->entry * sizeof(*stts_entries)); /* worst case */
			if (!stts_entries) {
				loge("ENOMEM");
				return -1;
			}
		}
		for (i = 0; i < track->entry; i++) {
			int duration = get_cluster_duration(track, i);
			if (i && duration == stts_entries[entries].duration) {
				stts_entries[entries].count++; /* compress */
			} else {
				entries++;
				stts_entries[entries].duration = duration;
				stts_entries[entries].count = 1;
			}
		}
		entries++; /* last one */
	}
	atom_size = 16 + (entries * 8);
	wb32(pb, atom_size); /* size */
	wfourcc(pb, "stts");
	wb32(pb, 0); /* version & flags */
	wb32(pb, entries); /* entry count */
	for (i = 0; i < entries; i++) {
		wb32(pb, stts_entries[i].count);
		wb32(pb, stts_entries[i].duration);
	}
	mpp_free(stts_entries);
	return atom_size;
}

static int mov_write_stsc_tag(struct aic_mov_muxer *s,struct mov_track *track)
{
	int index = 0, oldval = -1, i;
	int64_t entrypos, curpos;
	struct aic_stream* pb = s->stream;
	int64_t pos = aic_stream_tell(pb);

	wb32(pb, 0); /* size */
	wfourcc(pb, "stsc");
	wb32(pb, 0); // version & flags
	entrypos = aic_stream_tell(pb);
	wb32(pb, track->chunk_count); // entry count
	for (i = 0; i < track->entry; i++) {
		if (oldval != track->cluster[i].samples_in_chunk && track->cluster[i].chunk_num) {
			wb32(pb, track->cluster[i].chunk_num); // first chunk
			wb32(pb, track->cluster[i].samples_in_chunk); // samples per chunk
			wb32(pb, 0x1); // sample description index
			oldval = track->cluster[i].samples_in_chunk;
			index++;
		}
	}
	curpos = aic_stream_tell(pb);
	aic_stream_seek(pb, entrypos, SEEK_SET);
	wb32(pb, index); // rewrite size
	aic_stream_seek(pb, curpos, SEEK_SET);
	return update_size(pb, pos);
}

static int mov_write_stsz_tag(struct aic_mov_muxer *s,struct mov_track *track)
{
	struct aic_stream* pb = s->stream;
	int equal_chunks = 1;
	int i, j, entries = 0, tst = -1, oldtst = -1;
	int64_t pos = aic_stream_tell(pb);

	wb32(pb, 0); /* size */
	wfourcc(pb, "stsz");
	wb32(pb, 0); /* version & flags */

	for (i = 0; i < track->entry; i++) {
		tst = track->cluster[i].size / track->cluster[i].entries;
		if (oldtst != -1 && tst != oldtst)
			equal_chunks = 0;
		oldtst = tst;
		entries += track->cluster[i].entries;
	}
	if (equal_chunks && track->entry) {
		int size = track->entry ? track->cluster[0].size / track->cluster[0].entries : 0;
		size = (1 > size) ? 1 : size; // adpcm mono case could make sSize == 0
		wb32(pb, size); // sample size
		wb32(pb, entries); // sample count
	} else {
		wb32(pb, 0); // sample size
		wb32(pb, entries); // sample count
		for (i = 0; i < track->entry; i++) {
			for (j = 0; j < track->cluster[i].entries; j++) {
				wb32(pb, track->cluster[i].size /
							track->cluster[i].entries);
			}
		}
	}
	return update_size(pb, pos);
}

static int co64_required(struct mov_track *track)
{
	if (track->entry > 0 && track->cluster[track->entry - 1].pos + track->data_offset > UINT32_MAX)
		return 1;
	return 0;
}

/* Chunk offset atom */
static int mov_write_stco_tag(struct aic_mov_muxer *s, struct mov_track *track)
{
	int i;
	struct aic_stream* pb = s->stream;
	int mode64 = co64_required(track); // use 32 bit size variant if possible
	int64_t pos = aic_stream_tell(pb);

	wb32(pb, 0); /* size */
	if (mode64)
		wfourcc(pb, "co64");
	else
		wfourcc(pb, "stco");
	wb32(pb, 0); /* version & flags */
	wb32(pb, track->chunk_count); /* entry count */
	for (i = 0; i < track->entry; i++) {
		if (!track->cluster[i].chunk_num)
			continue;
		if (mode64 == 1)
			wb64(pb, track->cluster[i].pos + track->data_offset);
		else
			wb32(pb, track->cluster[i].pos + track->data_offset);
	}
	return update_size(pb, pos);
}

static int mov_write_stbl_tag(struct aic_mov_muxer *s,struct mov_track *track)
{
	struct aic_stream* pb = s->stream;
	int64_t pos = aic_stream_tell(pb);
	int ret = 0;

	wb32(pb, 0); /* size */
	wfourcc(pb, "stbl");
	if ((ret = mov_write_stsd_tag(s,track)) < 0)
		return ret;
	mov_write_stts_tag(s, track);

	// if (track->codec_para.codec_type == MPP_MEDIA_TYPE_VIDEO &&
	// 	track->flags & MOV_TRACK_CTTS && track->entry) {
	// 	if ((ret = mov_write_ctts_tag(s,track)) < 0)
	// 		return ret;
	// }
	mov_write_stsc_tag(s, track);
	mov_write_stsz_tag(s, track);
	mov_write_stco_tag(s, track);
	// if (track->par->codec_id == AV_CODEC_ID_OPUS || track->par->codec_id == AV_CODEC_ID_AAC) {
	// 	mov_preroll_write_stbl_atoms(pb, track);
	// }
	return update_size(pb, pos);
}

static int mov_write_minf_tag(struct aic_mov_muxer *s,struct mov_track *track)
{
	struct aic_stream* pb = s->stream;
	int64_t pos = aic_stream_tell(pb);
	int ret;

	wb32(pb, 0); /* size */
	wfourcc(pb, "minf");
	if (track->codec_para.codec_type == MPP_MEDIA_TYPE_VIDEO) {
		mov_write_vmhd_tag(s);
	} else if (track->codec_para.codec_type == MPP_MEDIA_TYPE_AUDIO) {
		mov_write_smhd_tag(s);
	}
	mov_write_dinf_tag(s);
	if ((ret = mov_write_stbl_tag(s,track)) < 0) {
		return ret;
	}
	return update_size(pb, pos);
}

static int mov_write_mdia_tag(struct aic_mov_muxer *s,struct mov_track *track)
{
	struct aic_stream* pb = s->stream;
	int64_t pos = aic_stream_tell(pb);
	int ret;

	wb32(pb, 0); /* size */
	wfourcc(pb, "mdia");
	mov_write_mdhd_tag(s, track);
	mov_write_hdlr_tag(s, track);
	if ((ret = mov_write_minf_tag(s,track)) < 0)
		return ret;
	return update_size(pb, pos);
}

	#define MOV_TKHD_FLAG_ENABLED       0x0001
	#define MOV_TKHD_FLAG_IN_MOVIE      0x0002

	/* transformation matrix
		|a  b  u|
		|c  d  v|
		|tx ty w| */
static void write_matrix(struct aic_mov_muxer *s, int16_t a, int16_t b, int16_t c,
							int16_t d, int16_t tx, int16_t ty)
{
	struct aic_stream* pb = s->stream;

	wb32(pb, a << 16);  /* 16.16 format */
	wb32(pb, b << 16);  /* 16.16 format */
	wb32(pb, 0);        /* u in 2.30 format */
	wb32(pb, c << 16);  /* 16.16 format */
	wb32(pb, d << 16);  /* 16.16 format */
	wb32(pb, 0);        /* v in 2.30 format */
	wb32(pb, tx << 16); /* 16.16 format */
	wb32(pb, ty << 16); /* 16.16 format */
	wb32(pb, 1 << 30);  /* w in 2.30 format */
}

static int mov_write_tkhd_tag(struct aic_mov_muxer *s,struct mov_track *track)
{
	struct aic_stream* pb = s->stream;
	int version = 0;
	int group   = 0;
	int flags   = MOV_TKHD_FLAG_IN_MOVIE|MOV_TKHD_FLAG_ENABLED;
	int64_t duration = calc_pts_duration(s,track);

	group = track->codec_para.codec_type;
	wb32(pb, 92);
	wfourcc(pb, "tkhd");
	w8(pb, version);
	wb24(pb, flags);
	wb32(pb, track->time); /* creation time */
	wb32(pb, track->time); /* modification time */
	wb32(pb, track->track_id); /* track-id */
	wb32(pb, 0); /* reserved */
	wb32(pb, duration);
	wb32(pb, 0); /* reserved */
	wb32(pb, 0); /* reserved */
	wb16(pb, 0); /* layer */
	wb16(pb, group); /* alternate group) */

	/* Volume, only for audio */
	if (track->codec_para.codec_type == MPP_MEDIA_TYPE_AUDIO)
		wb16(pb, 0x0100);
	else
		wb16(pb, 0);

	wb16(pb, 0); /* reserved */
	write_matrix(s,  1,  0,  0,  1, 0, 0);
	/* Track width and height, for visual only */
	if (track->codec_para.codec_type == MPP_MEDIA_TYPE_VIDEO) {
		wb32(pb, track->codec_para.width*0x10000);
		wb32(pb, track->codec_para.height*0x10000);
		//wb32(pb, track->height);
	} else {
		wb32(pb, 0);
		wb32(pb, 0);
	}
	return 0x5c;
}

static int mov_write_trak_tag(struct aic_mov_muxer *s,struct mov_track *track)
{
	int ret = 0;
	int64_t pos = aic_stream_tell(s->stream);
	struct aic_stream* pb = s->stream;

	wb32(pb, 0); /* size */
	wfourcc(pb, "trak");
	mov_write_tkhd_tag(s, track);

	// if (track->start_pts != AV_NOPTS_VALUE) {
	// 	mov_write_edts_tag(s, track);  // PSP Movies and several other cases require edts box
	// }

	if ((ret = mov_write_mdia_tag(s,track)) < 0)
		return ret;

	return update_size(pb, pos);

}

static void build_chunks(struct mov_track *trk)
{
	int i;
	struct mov_entry *chunk = &trk->cluster[0];
	uint64_t chunk_size = chunk->size;

	chunk->chunk_num = 1;
	if (trk->chunk_count)
		return;
	trk->chunk_count = 1;
	for (i = 1; i<trk->entry; i++) {
		if (chunk->pos + chunk_size == trk->cluster[i].pos &&
			chunk_size + trk->cluster[i].size < (1<<20)) {
			chunk_size             += trk->cluster[i].size;
			chunk->samples_in_chunk += trk->cluster[i].entries;
		} else {
			trk->cluster[i].chunk_num = chunk->chunk_num+1;
			chunk=&trk->cluster[i];
			chunk_size = chunk->size;
			trk->chunk_count++;
		}
	}
}

static int mov_write_mvhd_tag(struct aic_mov_muxer *s)
{
	int max_track_id = 1, i;
	int64_t max_track_len = 0;
	int version = 0;
	struct aic_stream* pb = s->stream;

	max_track_len = s->tracks[0].track_duration;
	max_track_id = s->tracks[0].track_id;

	for (i = 1; i < s->nb_streams; i++) {
		if (max_track_len < s->tracks[i].track_duration)
			max_track_len = s->tracks[i].track_duration;
		if (max_track_id < s->tracks[i].track_id)
			max_track_id = s->tracks[i].track_id;
	}

	wb32(pb,108); /* size */
	wfourcc(pb, "mvhd");
	w8(pb, version);
	wb24(pb, 0); /* flags */
	wb32(pb, s->time); /* creation time */
	wb32(pb, s->time); /* modification time */

	wb32(pb, MOV_TIMESCALE);
	wb32(pb, max_track_len); /* duration of longest track */

	wb32(pb, 0x00010000); /* reserved (preferred rate) 1.0 = normal */
	wb16(pb, 0x0100); /* reserved (preferred volume) 1.0 = normal */
	wb16(pb, 0); /* reserved */
	wb32(pb, 0); /* reserved */
	wb32(pb, 0); /* reserved */

	/* Matrix structure */
	write_matrix(s, 1, 0, 0, 1, 0, 0);

	wb32(pb, 0); /* reserved (preview time) */
	wb32(pb, 0); /* reserved (preview duration) */
	wb32(pb, 0); /* reserved (poster time) */
	wb32(pb, 0); /* reserved (selection time) */
	wb32(pb, 0); /* reserved (selection duration) */
	wb32(pb, 0); /* reserved (current time) */
	wb32(pb, max_track_id + 1); /* Next track id */
	return 0x6c;
}

static int mov_write_udta_tag(struct aic_mov_muxer *s)
{
	return 0;
}

static int mov_setup_track_ids(struct aic_mov_muxer *s)
{
	int i;

	if (s->track_ids_ok)
		return 0;

	for (i = 0; i < s->nb_streams; i++) {
		if (s->tracks[i].entry <= 0)
			continue;
		s->tracks[i].track_id = i + 1;
	}
	s->track_ids_ok = 1;
	return 0;
}

static int mov_write_moov_tag(struct aic_mov_muxer *s)
{
	int i;
	struct aic_stream* pb = s->stream;
	int64_t pos = aic_stream_tell(pb);

	wb32(pb, 0); /* size placeholder*/
	wfourcc(pb, "moov");
	mov_setup_track_ids(s);
	for (i = 0; i < s->nb_streams; i++) {
		s->tracks[i].time     = s->time;
		if (s->tracks[i].entry)
			build_chunks(&s->tracks[i]);
	}

	mov_write_mvhd_tag(s);

	for (i = 0; i < s->nb_streams; i++) {
		if (s->tracks[i].entry > 0) {
			int ret = mov_write_trak_tag(s, &s->tracks[i]);
			if (ret < 0)
				return ret;
		}
	}

	mov_write_udta_tag(s);

	return update_size(pb, pos);
}

const struct codec_tag codec_mp4_tags[] = {
	{ CODEC_ID_MJPEG,           MKTAG('m', 'p', '4', 'v') },
	{ CODEC_ID_AAC,             MKTAG('m', 'p', '4', 'a') },
	{ CODEC_ID_MP3,             MKTAG('m', 'p', '4', 'a') },
	{ CODEC_ID_NONE,               0 },
};

static unsigned int mov_find_codec_tag(struct aic_mov_muxer *s, struct mov_track *track)
{
	int i = 0;
	int codec_id = track->codec_para.codec_id;
	const struct codec_tag *const *tags = s->codec_tag;
	for (i = 0; tags && tags[i]; i++) {
		const struct codec_tag *codec_tags = tags[i];
		if (codec_tags->id == codec_id && codec_tags->id != CODEC_ID_NONE) {
			 return codec_tags->tag;
		}
	}
	return CODEC_ID_NONE;
}

s32 mp4_init(struct aic_mov_muxer *s,struct aic_av_media_info *info)
{
	int i = 0;

	if (info->has_video) {
		s->nb_streams++;
	}
	if (info->has_audio) {
		s->nb_streams++;
	}
	if (s->nb_streams == 0) {
		loge("no stream");
		return -1;
	}
	s->tracks = mpp_alloc(s->nb_streams * sizeof(*s->tracks));
	if (!s->tracks) {
		loge("mpp_alloc fail");
		return -1;
	}
	memset(s->tracks,0x00,s->nb_streams * sizeof(*s->tracks));
	s->mode = MODE_MP4;
	//s->codec_tag = codec_mp4_tags;
	s->codec_tag = (const struct codec_tag* const[]) {codec_mp4_tags, 0};
	/*init track */
	if (info->has_video) {
		s->tracks[i].codec_para.codec_type = MPP_MEDIA_TYPE_VIDEO;
		//need to transform
		if (info->video_stream.codec_type == MPP_CODEC_VIDEO_DECODER_MJPEG) {
			s->tracks[i].codec_para.codec_id = CODEC_ID_MJPEG;
		} else {
			return -1;
		}
		s->tracks[i].codec_para.width =  info->video_stream.width;
		s->tracks[i].codec_para.height =  info->video_stream.height;
		s->tracks[i].codec_para.bit_rate =  info->video_stream.bit_rate;
		s->tracks[i].codec_para.frame_size = info->video_stream.frame_rate;
		s->tracks[i].timescale = MOV_TIMESCALE;
		s->tracks[i].start_pts = AV_NOPTS_VALUE;
		s->tracks[i].end_pts = AV_NOPTS_VALUE;
		s->tracks[i].tag  = mov_find_codec_tag(s, &s->tracks[i]);
		s->tracks[i].data_offset = 0;
		loge("s->tracks[i].tag:0x%x",s->tracks[i].tag);
		i++;
	}
	if (info->has_audio) {
		s->tracks[i].codec_para.codec_type = MPP_MEDIA_TYPE_AUDIO;
		s->tracks[i].codec_para.codec_id = info->audio_stream.codec_type;
		s->tracks[i].codec_para.channels =  info->audio_stream.nb_channel;
		s->tracks[i].codec_para.sample_rate =  info->audio_stream.sample_rate;
		s->tracks[i].codec_para.bits_per_coded_sample = info->audio_stream.bits_per_sample;
		s->tracks[i].codec_para.bit_rate = info->audio_stream.bit_rate;
		s->tracks[i].timescale = MOV_TIMESCALE;
		s->tracks[i].sample_size = (info->audio_stream.bits_per_sample>>3)*info->audio_stream.nb_channel;
		s->tracks[i].start_pts = AV_NOPTS_VALUE;
		s->tracks[i].end_pts = AV_NOPTS_VALUE;
	}
	return 0;
}

s32 mp4_write_header(struct aic_mov_muxer *s)
{
	mov_write_ftyp_tag(s);
	mov_write_mdat_tag(s);
	return 0;
}

#define MOV_INDEX_CLUSTER_SIZE 1024

s32 mp4_write_packet(struct aic_mov_muxer *s,struct aic_av_packet *pkt)
{
	int samples_in_chunk;
	struct mov_track *trk = NULL;
	int size = pkt->size;

	trk = (pkt->type == MPP_MEDIA_TYPE_VIDEO)?&s->tracks[0]:&s->tracks[1];
	// write data into file
	aic_stream_write(s->stream,pkt->data,pkt->size);

	if (trk->entry >= trk->cluster_capacity) {
		unsigned new_capacity = trk->entry + MOV_INDEX_CLUSTER_SIZE;
		void *cluster = mpp_realloc(trk->cluster, new_capacity * sizeof(*trk->cluster));
		if (!cluster) {
			loge("no mem");
			return -1;
		}
		trk->cluster          = cluster;
		trk->cluster_capacity = new_capacity;
	}

	if (trk->sample_size) {
		samples_in_chunk = size / trk->sample_size;
	} else {
		samples_in_chunk = 1;
	}

	trk->cluster[trk->entry].pos              = aic_stream_tell(s->stream) - size;
	trk->cluster[trk->entry].samples_in_chunk = samples_in_chunk;
	trk->cluster[trk->entry].chunk_num         = 0;
	trk->cluster[trk->entry].size             = size;
	trk->cluster[trk->entry].entries          = samples_in_chunk;
	/*pts and dts need to transform*/
	trk->cluster[trk->entry].dts              = pkt->dts;
	trk->cluster[trk->entry].pts              = pkt->pts;

	if (trk->start_pts == AV_NOPTS_VALUE) {
		trk->start_pts = pkt->pts;
	}

	trk->track_duration = pkt->pts - trk->start_pts + pkt->duration;
	trk->entry++;
	trk->sample_count += samples_in_chunk;
	s->mdat_size    += size;
	return 0;
}

s32 mp4_write_trailer(struct aic_mov_muxer *s)
{
	struct aic_stream* pb = s->stream;
	int64_t pos = aic_stream_tell(pb);

	aic_stream_seek(pb, s->mdat_pos, SEEK_SET);
	wb32(pb, s->mdat_size + 8);
	aic_stream_seek(pb, pos, SEEK_SET);
	mov_write_moov_tag(s);
	return 0;
}

s32 mp4_close(struct aic_mov_muxer *s)
{
	int i;

	if (!s->tracks)
		return -1;
	for (i = 0; i < s->nb_streams; i++) {
		mpp_free(s->tracks[i].cluster);
	}
	mpp_free(s->tracks);
	aic_stream_close(s->stream);
	return 0;
}
