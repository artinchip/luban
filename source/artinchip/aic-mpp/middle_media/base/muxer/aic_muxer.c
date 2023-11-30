/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: aic_muxer
*/

#include <string.h>

#include "aic_mp4_muxer.h"



s32 aic_muxer_create(unsigned char *uri, struct aic_muxer **muxer, enum aic_muxer_type type)
{
	if (uri == NULL) {
		return -1;
	}

	if (type == AIC_MUXER_TYPE_MP4) {
		return aic_mp4_muxer_create(uri, muxer);
	}

	logw("unkown muxer for (%s)", uri);
	return -1;
}
