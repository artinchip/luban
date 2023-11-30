/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: aic_mp4_muxer
*/
#define LOG_TAG "mov"

#include <malloc.h>
#include <string.h>
#include <stddef.h>
#include <fcntl.h>

#include "aic_mp4_muxer.h"
#include "mpp_log.h"
#include "mpp_mem.h"
#include "mpp_dec_type.h"
#include "aic_stream.h"
#include "mp4_muxer.h"

static s32 mp4_muxer_destroy(struct aic_muxer *muxer)
{
	struct aic_mov_muxer *s = (struct aic_mov_muxer *)muxer;
	mp4_close(s);
	mpp_free(muxer);
	return 0;
}

static	s32 mp4_muxer_init(struct aic_muxer *muxer,struct aic_av_media_info *info)
{
	struct aic_mov_muxer *s = (struct aic_mov_muxer *)muxer;
	return mp4_init(s,info);
}

static	s32 mp4_muxer_write_header(struct aic_muxer *muxer)
{
	struct aic_mov_muxer *s = (struct aic_mov_muxer *)muxer;
	return mp4_write_header(s);
}

static	s32 mp4_muxer_write_packet(struct aic_muxer *muxer, struct aic_av_packet *packet)
{
	struct aic_mov_muxer *s = (struct aic_mov_muxer *)muxer;
	return mp4_write_packet(s,packet);
}

static	s32 mp4_muxer_write_trailer(struct aic_muxer *muxer)
{
	struct aic_mov_muxer *s = (struct aic_mov_muxer *)muxer;
	return mp4_write_trailer(s);
}


s32 aic_mp4_muxer_create(unsigned char *uri, struct aic_muxer **muxer)
{
	s32 ret = 0;
	struct aic_mov_muxer *mov_muxer = NULL;

	mov_muxer = (struct aic_mov_muxer *)mpp_alloc(sizeof(struct aic_mov_muxer));
	if (mov_muxer == NULL) {
		loge("mpp_alloc aic_muxer failed!!!!!\n");
		ret = -1;
		goto exit;
	}
	memset(mov_muxer, 0, sizeof(struct aic_mov_muxer));

	if (aic_stream_open((char *)uri, &mov_muxer->stream,O_RDWR|O_CREAT) < 0) {
		loge("stream open fail");
		ret = -1;
		goto exit;
	}

	mov_muxer->base.init = mp4_muxer_init;
	mov_muxer->base.destroy = mp4_muxer_destroy;
	mov_muxer->base.write_header = mp4_muxer_write_header;
	mov_muxer->base.write_packet = mp4_muxer_write_packet;
	mov_muxer->base.write_trailer = mp4_muxer_write_trailer;
	*muxer = &mov_muxer->base;
	return ret;

exit:
	if (mov_muxer->stream) {
		aic_stream_close(mov_muxer->stream);
	}
	if (mov_muxer) {
		mpp_free(mov_muxer);
	}
	return ret;
}
