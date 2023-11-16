/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: aic_parser
*/

#include <string.h>

#include "aic_mov_parser.h"
#include "aic_raw_parser.h"
#include "aic_mp3_parser.h"

s32 aic_parser_create(unsigned char *uri, struct aic_parser **parser)
{
	char* ptr = NULL;

	if (uri == NULL) {
		return -1;
	}

	ptr = strrchr((char *)uri, '.');
	if (ptr == NULL) {
		return -1;
	}

	if (!strncmp(ptr+1, "mov", 3) || !strncmp(ptr+1, "mp4", 3)) {
		return aic_mov_parser_create(uri, parser);
	} else if (!strncmp(ptr+1, "264", 3)) {
		return aic_raw_parser_create(uri, parser);
	} else if (!strncmp(ptr+1, "mp3", 3)) {
		return aic_mp3_parser_create(uri, parser);
	}

	logw("unkown parser for (%s)", uri);
	return -1;
}
