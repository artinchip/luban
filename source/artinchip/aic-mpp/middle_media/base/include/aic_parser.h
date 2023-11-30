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
#include "aic_middle_media_common.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

enum parse_status {
	PARSER_ERROR = -1,
	PARSER_OK = 0,
	PARSER_EOS = 1,
};
#define PACKET_EOS (1)

#define aic_parser_stream_type aic_stream_type

#define aic_parser_packet  aic_av_packet

#define aic_parser_video_stream  aic_av_video_stream

#define aic_parser_audio_stream  aic_av_audio_stream

#define aic_parser_av_media_info  aic_av_media_info

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
