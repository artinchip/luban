/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: aic_mp3_parser
 */

#include <malloc.h>
#include <string.h>
#include <stddef.h>
#include <fcntl.h>
#include "aic_mov_parser.h"
#include "mpp_log.h"
#include "mpp_mem.h"
#include "mpp_dec_type.h"
#include "aic_stream.h"
#include "mp3.h"
#include "aic_mp3_parser.h"

s32 mp3_peek(struct aic_parser * parser, struct aic_parser_packet *pkt)
{
	struct aic_mp3_parser *mp3_parser = (struct aic_mp3_parser *)parser;
	return mp3_peek_packet(mp3_parser,pkt);
}

s32 mp3_read(struct aic_parser * parser, struct aic_parser_packet *pkt)
{
	struct aic_mp3_parser *mp3_parser = (struct aic_mp3_parser *)parser;
	return mp3_read_packet(mp3_parser,pkt);
}

s32 mp3_get_media_info(struct aic_parser *parser, struct aic_parser_av_media_info *media)
{
	struct aic_mp3_parser *mp3_parser = (struct aic_mp3_parser *)parser;
	media->has_video = 0;
	media->has_audio = 1;
	media->file_size = mp3_parser->ctx.filesize;
	media->duration = mp3_parser->duration;
	media->audio_stream.codec_type = MPP_CODEC_AUDIO_DECODER_MP3;
	media->audio_stream.bits_per_sample = mp3_parser->header.frame_size;
	media->audio_stream.nb_channel = mp3_parser->header.nb_channels;
	media->audio_stream.sample_rate = mp3_parser->header.sample_rate;
	return 0;
}

s32 mp3_seek(struct aic_parser *parser, s64 time)
{
	struct aic_mp3_parser *mp3_parser = (struct aic_mp3_parser *)parser;
	return mp3_seek_packet(mp3_parser,time);
}

s32 mp3_init(struct aic_parser *parser)
{
	struct aic_mp3_parser *mp3_parser = (struct aic_mp3_parser *)parser;

	if (mp3_read_header(mp3_parser)) {
		loge("mp3 init failed");
		return -1;
	}
	return 0;
}

s32 mp3_destroy(struct aic_parser *parser)
{
	struct aic_mp3_parser *mp3_parser = (struct aic_mp3_parser *)parser;

	if (mp3_parser == NULL) {
		return -1;
	}
	mp3_close(mp3_parser);
	aic_stream_close(mp3_parser->stream);
	mpp_free(mp3_parser);
	return 0;
}

s32 aic_mp3_parser_create(unsigned char *uri, struct aic_parser **parser)
{
	s32 ret = 0;
	struct aic_mp3_parser *mp3_parser = NULL;

	mp3_parser = (struct aic_mp3_parser *)mpp_alloc(sizeof(struct aic_mp3_parser));
	if (mp3_parser == NULL) {
		loge("mpp_alloc aic_parser failed!!!!!\n");
		ret = -1;
		goto exit;
	}
	memset(mp3_parser, 0, sizeof(struct aic_mp3_parser));

	if (aic_stream_open((char *)uri, &mp3_parser->stream, O_RDONLY) < 0) {
		loge("stream open fail");
		ret = -1;
		goto exit;
	}

	mp3_parser->base.get_media_info = mp3_get_media_info;
	mp3_parser->base.peek = mp3_peek;
	mp3_parser->base.read = mp3_read;
	mp3_parser->base.control = NULL;
	mp3_parser->base.destroy = mp3_destroy;
	mp3_parser->base.seek = mp3_seek;
	mp3_parser->base.init = mp3_init;

	*parser = &mp3_parser->base;

	return ret;

exit:
	if (mp3_parser->stream) {
		aic_stream_close(mp3_parser->stream);
	}
	if (mp3_parser) {
		mpp_free(mp3_parser);
	}
	return ret;
}
