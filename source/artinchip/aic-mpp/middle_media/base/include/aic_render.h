/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: video_render interface
 */

#ifndef __AIC_RENDER_H__
#define __AIC_RENDER_H__

#include "mpp_dec_type.h"

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */

struct aic_video_render {
	s32 (*init)(struct aic_video_render *render,s32 layer_id,s32 dev_id);
	s32 (*destroy)(struct aic_video_render *render);
	s32 (*rend)(struct aic_video_render *render,struct mpp_frame *frame_info);
	s32 (*get_screen_size)(struct aic_video_render *render,struct mpp_size *size);
	s32 (*set_dis_rect)(struct aic_video_render *render,struct mpp_rect *rect);
	s32 (*get_dis_rect)(struct aic_video_render *render,struct mpp_rect *rect);
	s32 (*set_on_off)(struct aic_video_render *render,s32 on_off);
	s32 (*get_on_off)(struct aic_video_render *render,s32 *on_off);
};


#define aic_video_render_init(render,layer_id,dev_id)\
	    ((struct aic_video_render*)render)->init(render,layer_id,dev_id)

#define aic_video_render_destroy(render)\
	    ((struct aic_video_render*)render)->destroy(render)

#define aic_video_render_rend(render,frame_info)\
	    ((struct aic_video_render*)render)->rend(render,frame_info)

#define aic_video_render_set_dis_rect(          \
		   render,            \
		   rect)            \
	    ((struct aic_video_render*)render)->set_dis_rect(render,rect)

#define aic_video_render_get_dis_rect(          \
		   render,            \
		   rect)            \
	    ((struct aic_video_render*)render)->get_dis_rect(render,rect)

#define aic_video_render_set_on_off(          \
		   render,            \
		   on_off)            \
	    ((struct aic_video_render*)render)->set_on_off(render,on_off)


#define aic_video_render_get_on_off(          \
		   render,            \
		   on_off)            \
	    ((struct aic_video_render*)render)->get_on_off(render,on_off)

s32 aic_video_render_create(struct aic_video_render **render);

#ifdef __cplusplus
}
#endif /* End of #ifdef __cplusplus */


#endif


