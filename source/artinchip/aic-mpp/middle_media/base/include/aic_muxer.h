/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: aic_muxer
*/

#ifndef __AIC_MUXER_H__
#define __AIC_MUXER_H__

#include "mpp_dec_type.h"
#include "aic_middle_media_common.h"
#include "aic_stream.h"
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct aic_muxer {

	s32 (*destroy)(struct aic_muxer *muxer);

	s32 (*init)(struct aic_muxer *muxer,struct aic_av_media_info *info);

	s32 (*write_header)(struct aic_muxer *muxer);

	s32 (*write_packet)(struct aic_muxer *muxer, struct aic_av_packet *packet);

	s32 (*write_trailer)(struct aic_muxer *muxer);

};

#define aic_muxer_destroy(muxer)\
	    ((struct aic_muxer*)muxer)->destroy(muxer)

#define aic_muxer_init(muxer,info)\
	    ((struct aic_muxer*)muxer)->init(muxer,info)

#define aic_muxer_write_header(muxer)\
	    ((struct aic_muxer*)muxer)->write_header(muxer)

#define aic_muxer_write_packet(muxer,packet)\
	    ((struct aic_muxer*)muxer)->write_packet(muxer,packet)

#define aic_muxer_write_trailer(muxer)\
	    ((struct aic_muxer*)muxer)->write_trailer(muxer)

s32 aic_muxer_create(unsigned char *uri, struct aic_muxer **muxer, enum aic_muxer_type type);

#ifdef __cplusplus
}
#endif /* End of #ifdef __cplusplus */
#endif
