/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: aic_mp4_muxer
*/

#ifndef __AIC_MP4_MUXER_H__
#define __AIC_MP4_MUXER_H__

#include "aic_muxer.h"
#include "aic_stream.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

s32 aic_mp4_muxer_create(unsigned char* uri, struct aic_muxer **muxer);

#ifdef __cplusplus
}
#endif /* End of #ifdef __cplusplus */

#endif
