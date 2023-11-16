/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <qi.xu@artinchip.com>
*  Desc: parser for H.264/H.265 raw data
*/

#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <inttypes.h>

#include "aic_raw_parser.h"
#include "mpp_log.h"
#include "mpp_mem.h"
#include "mpp_dec_type.h"
#include "aic_stream.h"

#define STEAM_BUF_LEN (512*1024)

struct aic_raw_parser {
	struct aic_parser base;
	struct aic_stream* stream;

	unsigned char* stream_buf;
	int cur_read_pos;
	int buf_len;
	int valid_size;
	int cur_read_len;
	int stream_end_flag;
	int packet_index;
};

static int get_data(struct aic_raw_parser* p)
{
	int r_len = 0;

	if(p->valid_size <= 0) {
		r_len = aic_stream_read(p->stream, p->stream_buf, p->buf_len);
		if(r_len <= 0) {
			return r_len;
		}
	} else {
		memmove(p->stream_buf, (p->stream_buf + p->cur_read_pos), p->valid_size);

		int len = p->buf_len - p->valid_size;
		r_len = aic_stream_read(p->stream, p->stream_buf + p->valid_size, len);
		if(r_len < 0) {
			return r_len;
		}
	}

	p->cur_read_len += r_len;
	p->valid_size += r_len;
	p->cur_read_pos = 0;
	return r_len;
}

s32 raw_peek( struct aic_parser *parser ,struct aic_parser_packet *pkt)
{
	int i = 0;
	char tmp_buf[3];
	int find_start_code = 0;
	int start = 0;
	int stream_data_len = -1;
	int ret = 0;
	char *cur_data_ptr = NULL;
	struct aic_raw_parser *p = (struct aic_raw_parser*)parser;

	if(p->stream_end_flag){
		return PARSER_EOS;
	}

	if (p->valid_size <= 0) {
		if (get_data(p) < 0) {
			loge("get data error");
			return -1;
		}
	}

	pkt->type = MPP_MEDIA_TYPE_VIDEO;

find_start_code:
	cur_data_ptr = (char *)(p->stream_buf + p->cur_read_pos);
	logd("data: %x, %x, %x, %x, %x, %x, %x, %x", *(cur_data_ptr), *(cur_data_ptr + 1),
		*(cur_data_ptr+2),*(cur_data_ptr+3),
		*(cur_data_ptr+4),*(cur_data_ptr+5),*(cur_data_ptr+6),*(cur_data_ptr+7));

	//* find the first start_code
	for(i = 0; i < (p->valid_size - 3); i++) {
		tmp_buf[0] = *(cur_data_ptr + i);
		tmp_buf[1] = *(cur_data_ptr + i + 1);
		tmp_buf[2] = *(cur_data_ptr + i + 2);
		if(tmp_buf[0] == 0 && tmp_buf[1] == 0 && tmp_buf[2] == 1) {
			find_start_code = 1;
			break;
		}
	}

	logd("find_start_code = %d, i = %d, validSize = %d",\
		find_start_code, i, p->valid_size);
	if(find_start_code == 1) {
		p->cur_read_pos += i;
		start = i;

		// if the last byte is 0x00, read_pos minus 1
		if (p->cur_read_pos && (*(cur_data_ptr + i -1) == 0)) {
			p->cur_read_pos -= 1;
			start -= 1;
		}
		find_start_code = 0;

		//* find the next start code
		for(i += 3; i < (p->valid_size - 3); i++) {
			logv("cur_data_ptr = %p, i = %d", cur_data_ptr, i);
			tmp_buf[0] = *(cur_data_ptr + i);
			tmp_buf[1] = *(cur_data_ptr + i + 1);
			tmp_buf[2] = *(cur_data_ptr + i + 2);
			if(tmp_buf[0] == 0 && tmp_buf[1] == 0 && tmp_buf[2] == 1) {
				find_start_code = 1;
				break;
			}
		}

		if(find_start_code == 1) {
			if(*(cur_data_ptr + i - 1) == 0) {
				stream_data_len = i - start - 1;
			} else {
				stream_data_len = i - start;
			}
		} else {
			ret = get_data(p);
			if(ret == -1)
				return -1;
			if(ret == 0) {
				logi("eos, file_size: %"PRId64", cur_read: %d", aic_stream_size(p->stream), p->cur_read_len);
				stream_data_len = p->valid_size - start;
				pkt->flag |= PACKET_FLAG_EOS;
				pkt->size = stream_data_len;
				p->stream_end_flag = 1;
				return 0;
			}

			goto find_start_code;
		}
	} else {
		ret = get_data(p);
		if(ret == -1 || ret == 0){
			return -1;
		}

		goto find_start_code;
	}

	pkt->size = stream_data_len;
	return 0;
}

s32 raw_read(struct aic_parser *parser ,struct aic_parser_packet *pkt)
{
	struct aic_raw_parser *p = (struct aic_raw_parser*)parser;
	if(pkt->size <= 0)
		return -1;

	char* read_ptr = (char*)(p->stream_buf + p->cur_read_pos);

	logd("read data: %x, %x, %x, %x", *read_ptr,*(read_ptr+1), *(read_ptr+2), *(read_ptr+3));

	memcpy(pkt->data, read_ptr, pkt->size);

	pkt->pts = p->packet_index*33000;/* default 30 fps*/
	p->packet_index++;

	p->cur_read_pos += pkt->size;
	p->valid_size -= pkt->size;

	return 0;
}

s32 raw_get_media_info( struct aic_parser *parser ,struct aic_parser_av_media_info *media)
{
	media->has_audio = 0;
	media->has_video = 1;
	media->seek_able = 0;
	media->duration = 0;
	media->video_stream.codec_type = MPP_CODEC_VIDEO_DECODER_H264;

	// unkown width and height from raw data
	media->video_stream.height = 0;
	media->video_stream.width = 0;

	return 0;
}

s32 raw_seek( struct aic_parser *parser , s64 time)
{
	// not support
	return -1;
}

s32 raw_init( struct aic_parser *parser)
{
	// do nothing
	return 0;
}

s32 raw_destroy(struct aic_parser *parser)
{
	struct aic_raw_parser *impl = (struct aic_raw_parser*)parser;
	if (impl == NULL) {
		return -1;
	}

	aic_stream_close(impl->stream);
	mpp_free(impl->stream_buf);
	mpp_free(impl);
	return 0;
}

s32 aic_raw_parser_create(unsigned char *uri, struct aic_parser **parser)
{
	struct aic_raw_parser *impl = NULL;

	impl = (struct aic_raw_parser *)mpp_alloc(sizeof(struct aic_raw_parser));
	if (impl == NULL) {
		loge("mpp_alloc raw_parser failed!!!!!\n");
		return -1;
	}
	memset(impl, 0, sizeof(struct aic_raw_parser));

	impl->stream_buf = (unsigned char*)mpp_alloc(STEAM_BUF_LEN);
	if (!impl->stream_buf) {
		loge("mpp_alloc fail !!!!\n");
		goto exit;
	}
	impl->buf_len = STEAM_BUF_LEN;

	if (aic_stream_open((char *)uri, &impl->stream) < 0) {
		loge("stream open fail");
		goto exit;
	}
	impl->base.get_media_info	= raw_get_media_info;
	impl->base.peek			= raw_peek;
	impl->base.read			= raw_read;
	impl->base.destroy		= raw_destroy;
	impl->base.seek			= raw_seek;
	impl->base.init			= raw_init;

	*parser = &impl->base;
	return 0;

exit:
	if (impl->stream) {
		aic_stream_close(impl->stream);
	}
	if (impl->stream_buf) {
		mpp_free(impl->stream_buf);
	}
	if (impl) {
		mpp_free(impl);
	}
	return -1;
}
