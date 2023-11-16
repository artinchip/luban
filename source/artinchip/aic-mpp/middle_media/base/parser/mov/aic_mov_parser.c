/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: aic_mov_parser
*/
#define LOG_TAG "mov"

#include <malloc.h>
#include <string.h>
#include <stddef.h>
#include "aic_mov_parser.h"
#include "mpp_log.h"
#include "mpp_mem.h"
#include "mpp_dec_type.h"
#include "aic_stream.h"
#include "mov.h"

s32 mov_peek(struct aic_parser * parser, struct aic_parser_packet *pkt)
{
	struct aic_mov_parser *mov_parser = (struct aic_mov_parser *)parser;

	return mov_peek_packet(mov_parser, pkt);
}

s32 mov_read(struct aic_parser * parser, struct aic_parser_packet *pkt)
{
	struct aic_mov_parser *c = (struct aic_mov_parser *)parser;

	aic_stream_seek(c->stream, c->cur_sample->pos, SEEK_SET);
	aic_stream_read(c->stream, pkt->data, c->cur_sample->size);

	return 0;
}

s32 mov_get_media_info(struct aic_parser *parser, struct aic_parser_av_media_info *media)
{
	int i;
	int64_t duration = 0;
	struct aic_mov_parser *c = (struct aic_mov_parser *)parser;

	logi("================ media info =======================");
	for (i=0; i<c->nb_streams; i++) {
		struct mov_stream_ctx *st = c->streams[i];
		if (st->type == MPP_MEDIA_TYPE_VIDEO) {
			media->has_video = 1;
			if (st->id == CODEC_ID_H264)
				media->video_stream.codec_type = MPP_CODEC_VIDEO_DECODER_H264;
			else if (st->id == CODEC_ID_MJPEG)
				media->video_stream.codec_type = MPP_CODEC_VIDEO_DECODER_MJPEG;
			else
				media->video_stream.codec_type = -1;

			media->video_stream.width = st->width;
			media->video_stream.height = st->height;
			if (st->extra_data_size > 0) {
				media->video_stream.extra_data_size = st->extra_data_size;
				media->video_stream.extra_data = st->extra_data;
			}
			logi("video width: %d", st->width);
			logi("video height: %d", st->height);
			logi("video extra_data_size: %d", st->extra_data_size);
		} else if (st->type == MPP_MEDIA_TYPE_AUDIO) {
			media->has_audio = 1;
			if (st->id == CODEC_ID_MP3)
				media->audio_stream.codec_type = MPP_CODEC_AUDIO_DECODER_MP3;
			else if (st->id == CODEC_ID_AAC)
				media->audio_stream.codec_type = MPP_CODEC_AUDIO_DECODER_AAC;
			else
				media->audio_stream.codec_type = MPP_CODEC_AUDIO_DECODER_UNKOWN;

			media->audio_stream.bits_per_sample = st->bits_per_sample;
			media->audio_stream.nb_channel = st->channels;
			media->audio_stream.sample_rate = st->sample_rate;
			if (st->extra_data_size > 0) {
				media->audio_stream.extra_data_size = st->extra_data_size;
				media->audio_stream.extra_data = st->extra_data;
			}
			logi("audio bits_per_sample: %d", st->bits_per_sample);
			logi("audio channels: %d", st->channels);
			logi("audio sample_rate: %d", st->sample_rate);
			logi("audio extra_data_size: %d", st->extra_data_size);
		}

		if (st->duration > duration)
			duration = st->duration;
	}

	media->file_size = aic_stream_size(c->stream);
	media->seek_able = 1;
	media->duration = duration;

	return 0;
}

s32 mov_seek(struct aic_parser *parser, s64 time)
{
	s32 ret = 0;
	struct aic_mov_parser *mov_parser = (struct aic_mov_parser *)parser;
	ret = mov_seek_packet(mov_parser,time);
	return ret;
}

s32 mov_init(struct aic_parser *parser)
{
	struct aic_mov_parser *mov_parser = (struct aic_mov_parser *)parser;

	if (mov_read_header(mov_parser) < 0) {
		loge("mov open failed");
		return -1;
	}

	return 0;
}

s32 mov_destroy(struct aic_parser *parser)
{
	struct aic_mov_parser *mov_parser = (struct aic_mov_parser *)parser;
	if (parser == NULL) {
		return -1;
	}

	mov_close(mov_parser);
	aic_stream_close(mov_parser->stream);
	mpp_free(mov_parser);
	return 0;
}

s32 aic_mov_parser_create(unsigned char *uri, struct aic_parser **parser)
{
	s32 ret = 0;
	struct aic_mov_parser *mov_parser = NULL;

	mov_parser = (struct aic_mov_parser *)mpp_alloc(sizeof(struct aic_mov_parser));
	if (mov_parser == NULL) {
		loge("mpp_alloc aic_parser failed!!!!!\n");
		ret = -1;
		goto exit;
	}
	memset(mov_parser, 0, sizeof(struct aic_mov_parser));

	if (aic_stream_open((char *)uri, &mov_parser->stream) < 0) {
		loge("stream open fail");
		ret = -1;
		goto exit;
	}

	mov_parser->base.get_media_info = mov_get_media_info;
	mov_parser->base.peek = mov_peek;
	mov_parser->base.read = mov_read;
	mov_parser->base.destroy = mov_destroy;
	mov_parser->base.seek = mov_seek;
	mov_parser->base.init = mov_init;

	*parser = &mov_parser->base;

	return ret;

exit:
	if (mov_parser->stream) {
		aic_stream_close(mov_parser->stream);
	}
	if (mov_parser) {
		mpp_free(mov_parser);
	}
	return ret;
}
