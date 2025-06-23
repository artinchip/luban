/*
 * Copyright (C) 2020-2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: aic_parser
 */

#include <string.h>

#include "aic_mov_parser.h"
#include "aic_raw_parser.h"
#include "aic_mp3_parser.h"
#ifdef WAV_DEMUXER
#include "aic_wav_parser.h"
#endif
#ifdef AVI_DEMUXER
#include "aic_avi_parser.h"
#endif
#ifdef MKV_DEMUXER
#include "aic_mkv_parser.h"
#endif
#ifdef TS_DEMUXER
#include "aic_ts_parser.h"
#endif
#ifdef FLV_DEMUXER
#include "aic_flv_parser.h"
#endif
#ifdef RTSP_DEMUXER
#include "aic_rtsp_parser.h"
#endif


struct aic_parser_create_tbl {
	char  file_type[7];
	unsigned char len;
	s32 (*parser)(unsigned char *uri, struct aic_parser **parser);
};

struct aic_parser_create_tbl create_tbl[] = {
	{"mov", 3, aic_mov_parser_create},
	{"mp4", 3, aic_mov_parser_create},
	{"264", 3, aic_raw_parser_create},
	{"mp3", 3, aic_mp3_parser_create},
#ifdef WAV_DEMUXER
	{"wav", 3, aic_wav_parser_create},
#endif
#ifdef AVI_DEMUXER
	{"avi", 3, aic_avi_parser_create},
#endif
#ifdef MKV_DEMUXER
	{"mkv", 3, aic_mkv_parser_create},
#endif
#ifdef TS_DEMUXER
	{"ts", 2, aic_ts_parser_create},
#endif
#ifdef FLV_DEMUXER
	{"flv", 3, aic_flv_parser_create},
#endif
#ifdef RTSP_DEMUXER
	{"rtsp", 4, aic_rtsp_parser_create},
#endif

};

s32 aic_parser_create(unsigned char *uri, struct aic_parser **parser)
{
	int i = 0;
	char* ptr = NULL;
	int size = 0;

	if (uri == NULL) {
		return -1;
	}

	ptr = strrchr((char *)uri, '.');
	ptr += 1;
	if (strlen((char *)uri) > 7 && !strncmp((char *)uri, "rtsp://", 7)) {
		ptr = (char *)uri;
	}
	if (ptr == NULL) {
		return -1;
	}

	printf("parser for (%s)\n", uri);

	size = sizeof(create_tbl)/sizeof(struct aic_parser_create_tbl);

	for (i = 0; i < size; i++) {
		if (!strncmp(ptr, create_tbl[i].file_type, create_tbl[i].len)) {
			return create_tbl[i].parser(uri, parser);
		}
	}

	loge("unkown parser for (%s)", uri);
	return -1;
}
