/*
* Copyright (C) 2020-2024 ArtInChip Technology Co. Ltd
*
* SPDX-License-Identifier: Apache-2.0
*
*  author: <jun.ma@artinchip.com>
*  Desc: aic_rtsp_parser
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
#include "rtsp.h"
#include "aic_rtsp_parser.h"



extern "C"  s32 aic_rtsp_peek(struct aic_parser * parser, struct aic_parser_packet *pkt)
{
	struct aic_rtsp_parser *rtsp_parser = (struct aic_rtsp_parser *)parser;
	return rtsp_peek(rtsp_parser->rtsp_ctx,pkt);
}

extern "C"  s32 aic_rtsp_read(struct aic_parser * parser, struct aic_parser_packet *pkt)
{
	struct aic_rtsp_parser *rtsp_parser = (struct aic_rtsp_parser *)parser;
	return rtsp_read(rtsp_parser->rtsp_ctx,pkt);
}

extern "C"  s32 aic_rtsp_get_media_info(struct aic_parser *parser, struct aic_parser_av_media_info *media)
{
	struct aic_rtsp_parser *rtsp_parser = (struct aic_rtsp_parser *)parser;
	AICRtspClientCtx *ctx = (AICRtspClientCtx *)rtsp_parser->rtsp_ctx;
	if (ctx->p_stream_info->av_present_flag & MPP_MEDIA_TYPE_VIDEO) {
		media->has_video = 1;
		media->video_stream.codec_type = (enum mpp_codec_type)ctx->p_stream_info->v_codec_id;// to do transform
		media->video_stream.width =  ctx->p_stream_info->width;
		media->video_stream.height = ctx->p_stream_info->height;
		if (ctx->p_v_extra && ctx->v_extra_size > 0) {
			media->video_stream.extra_data = ctx->p_v_extra;
			media->video_stream.extra_data_size = ctx->v_extra_size;
		}
	}
	if (ctx->p_stream_info->av_present_flag & MPP_MEDIA_TYPE_AUDIO) {
		media->has_audio = 1;
		media->audio_stream.codec_type =   (enum aic_audio_codec_type)ctx->p_stream_info->a_codec_id;
		media->audio_stream.bits_per_sample = ctx->p_stream_info->samp_bits;
		media->audio_stream.nb_channel = ctx->p_stream_info->channel_num;
		media->audio_stream.sample_rate = ctx->p_stream_info->samp_rate;
		if(ctx->p_a_extra && ctx->a_extra_size) {
			media->audio_stream.extra_data = ctx->p_a_extra;
			media->audio_stream.extra_data_size = ctx->a_extra_size;
		}
	}
	media->seek_able = 0;
	media->file_size = 0;
	media->duration = 0;
	return 0;
}

extern "C"  s32 aic_rtsp_seek(struct aic_parser *parser, int64_t time)
{
	return -1;
}

extern "C"  s32 aic_rtsp_init(struct aic_parser *parser)
{
	struct aic_rtsp_parser *rtsp_parser = (struct aic_rtsp_parser *)parser;

	RTSP_DEBUG(RTSP_DBG_ERR,"rtsp_parser->uri %s\n",rtsp_parser->uri);
	rtsp_parser->rtsp_ctx =  rtsp_open(rtsp_parser->uri);
	if (!rtsp_parser->rtsp_ctx) {
		return -1;
	}
	return rtsp_play(rtsp_parser->rtsp_ctx);
}

extern "C"  s32 aic_rtsp_destroy(struct aic_parser *parser)
{
	struct aic_rtsp_parser *rtsp_parser = (struct aic_rtsp_parser *)parser;
	if (!rtsp_parser) {
		return -1;
	}
	rtsp_close(rtsp_parser->rtsp_ctx);
	free(rtsp_parser);
	return 0;
}


extern "C"  s32 aic_rtsp_parser_create(unsigned char *uri, struct aic_parser **parser)
{
	int32_t ret = 0;
	struct aic_rtsp_parser *rtsp_parser = NULL;

	rtsp_parser = (struct aic_rtsp_parser *)malloc(sizeof(struct aic_rtsp_parser));

	if (rtsp_parser == NULL) {
		printf("mpp_alloc aic_parser failed!!!!!\n");
		ret = -1;
		goto exit;
	}
	memset(rtsp_parser, 0x00, sizeof(struct aic_rtsp_parser));

	strcpy(rtsp_parser->uri,(char *)uri);

	printf("uri %s:%ld:%ld\n",rtsp_parser->uri,sizeof(rtsp_parser->uri),sizeof(struct aic_rtsp_parser));

	rtsp_parser->base.get_media_info = aic_rtsp_get_media_info;
	rtsp_parser->base.peek = aic_rtsp_peek;
	rtsp_parser->base.read = aic_rtsp_read;
	rtsp_parser->base.destroy = aic_rtsp_destroy;
	rtsp_parser->base.seek = aic_rtsp_seek;
	rtsp_parser->base.init = aic_rtsp_init;

	*parser = &rtsp_parser->base;
	return ret;

exit:
	if (rtsp_parser) {
		free(rtsp_parser);
	}
	return ret;
}