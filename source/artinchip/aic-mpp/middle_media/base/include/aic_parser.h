/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: aic_parser
*/

#ifndef __AIC_PARSER_H__
#define __AIC_PARSER_H__

#include "mpp_dec_type.h"
#include "aic_audio_decoder.h"
#include "aic_stream.h"
#include "mpp_log.h"

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */

enum aic_parser_type {
	PARSER_TYPE_UNKNOW = -1,
	PARSER_TYPE_MOV,
	PARSER_TYPE_RAW,
};

enum aic_parser_stream_type {
	MPP_MEDIA_TYPE_UNKNOWN,
	MPP_MEDIA_TYPE_VIDEO,
	MPP_MEDIA_TYPE_AUDIO,
	MPP_MEDIA_TYPE_OTHER
};

enum parse_status {
	PARSER_ERROR = -1,
	PARSER_OK = 0,
	PARSER_EOS = 1,
};

#define PACKET_EOS (1)
struct aic_parser_packet {
	enum aic_parser_stream_type type;
	void *data;
	s32 size;
	s64 pts;
	u32 flag;
};

struct aic_parser_video_stream {
	enum mpp_codec_type codec_type;
	s32   width;
	s32   height;
	s32   extra_data_size;
	u8    *extra_data;
};

struct aic_parser_audio_stream {
	enum aic_audio_codec_type codec_type;
	s32 nb_channel;
	s32 bits_per_sample;
	s32 sample_rate;
	s32 extra_data_size;
	u8 *extra_data;
};

struct aic_parser_av_media_info {
	s64  file_size;
	s64  duration;
	u8   has_video;
	u8   has_audio;
	u8   seek_able;
	struct aic_parser_video_stream video_stream;
	struct aic_parser_audio_stream audio_stream;
};

struct aic_parser {
	/* destroy the parser*/
	s32 (*destroy)(struct aic_parser *parser);

	/**
	 * peek - get the next packet infomation from parser
	 *
	 * return: enum parse_status
	 */
	s32 (*peek)(struct aic_parser *parser, struct aic_parser_packet *packet); /*just see packet info*/

	/* read the packet data */
	s32 (*read)(struct aic_parser *parser, struct aic_parser_packet *packet); /* */

	/* get media info of audio and video */
	s32 (*get_media_info)(struct aic_parser *parser, struct aic_parser_av_media_info *info);

	/* seek */
	s32 (*seek)(struct aic_parser *parser, s64 time); //unit:us

	/* init, parse file header */
	s32 (*init)(struct aic_parser *parser);
};

#define aic_parser_destroy(parser)\
	    ((struct aic_parser*)parser)->destroy(parser)

#define aic_parser_peek(          \
		   parser,            \
		   packet)            \
	    ((struct aic_parser*)parser)->peek(parser,packet)

#define aic_parser_read(          \
		   parser,            \
		   packet)            \
	    ((struct aic_parser*)parser)->read(parser,packet)

#define aic_parser_seek(          \
		   parser,            \
		   time)            \
	    ((struct aic_parser*)parser)->seek(parser,time)

#define aic_parser_get_media_info(          \
		   parser,            \
		   media_info)            \
	    ((struct aic_parser*)parser)->get_media_info(parser,media_info)

#define aic_parser_init(          \
		   parser)            \
	    ((struct aic_parser*)parser)->init(parser)

s32 aic_parser_create(unsigned char *uri, struct aic_parser **parser);

#ifdef __cplusplus
}
#endif /* End of #ifdef __cplusplus */
#endif
