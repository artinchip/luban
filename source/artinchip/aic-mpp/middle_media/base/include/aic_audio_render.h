/*
 * Copyright (C) 2020-2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: aic_audio_render
 */

#ifndef __AIC_AUDIO_RENDER_H__
#define __AIC_AUDIO_RENDER_H__

#include "mpp_dec_type.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct aic_audio_render_attr {
	s32 channels;
	s32 sample_rate;
	s32 bits_per_sample;
	s32 smples_per_frame;
};

struct aic_audio_render
{
	s32 (*init)(struct aic_audio_render *render,s32 dev_id);

	s32 (*destroy)(struct aic_audio_render *render);

	s32 (*set_attr)(struct aic_audio_render *render,struct aic_audio_render_attr *attr);

	s32 (*get_attr)(struct aic_audio_render *render,struct aic_audio_render_attr *attr);

	s32 (*rend)(struct aic_audio_render *render, void* pData, s32 nDataSize);

	s32 (*pause)(struct aic_audio_render *render);

	s64 (*get_cached_time)(struct aic_audio_render *render);

	s32 (*get_volume)(struct aic_audio_render *render);

	s32 (*set_volume)(struct aic_audio_render *render,s32 vol);

	s32 (*clear_cache)(struct aic_audio_render *render);

};


#define aic_audio_render_init(render,dev_id)\
	    ((struct aic_audio_render*)render)->init(render,dev_id)

#define aic_audio_render_destroy(render)\
	    ((struct aic_audio_render*)render)->destroy(render)

#define aic_audio_render_set_attr(render,attr)\
	    ((struct aic_audio_render*)render)->set_attr(render,attr)

#define aic_audio_render_get_attr(render,attr)\
	    ((struct aic_audio_render*)render)->get_attr(render,attr)

#define aic_audio_render_rend(render,pData,nDataSize)\
	    ((struct aic_audio_render*)render)->rend(render,pData,nDataSize)

#define aic_audio_render_get_cached_time(render)\
			((struct aic_audio_render*)render)->get_cached_time(render)

#define aic_audio_render_pause(render)\
	    ((struct aic_audio_render*)render)->pause(render)

#define aic_audio_render_set_volume(render,vol)\
	    ((struct aic_audio_render*)render)->set_volume(render,vol)

#define aic_audio_render_get_volume(render)\
	    ((struct aic_audio_render*)render)->get_volume(render)


#define aic_audio_render_clear_cache(render)\
	    ((struct aic_audio_render*)render)->clear_cache(render)


s32 aic_audio_render_create(struct aic_audio_render **render);


#ifdef __cplusplus
}
#endif /* End of #ifdef __cplusplus */


#endif


