/*
 * Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <qi.xu@artinchip.com>
 *  Desc: frame buffer manager
 */

#ifndef FRAME_MANAGER_H
#define FRAME_MANAGER_H

#include "mpp_dec_type.h"
#include "ve_buffer.h"
#include "frame_allocator.h"

/**
 * struct frame is used by decoder
 * struct mpp_frame is used by render
 * struct mpp_frame is not contain physic address, but struct frame need it
 */
struct frame {
	struct mpp_frame        mpp_frame;
	unsigned int 		phy_addr[3];		// physic address
};

struct frame_manager_init_cfg {
	struct frame_allocator	 	*allocator;	// frame buffer allocator
	enum mpp_pixel_format 		pixel_format;	// frame pixel format
	int 				width;		// frame real width
	int 				height;		// frame real height
	int 				stride;		// frame buffer stride
	int 				height_align;	// frame buffer aligned height
	int 				frame_count;	// frame count
};

struct frame_manager;

/*
  create frame manager
*/
struct frame_manager *fm_create(struct frame_manager_init_cfg *init_cfg);

/*
  destory frame manager
*/
int fm_destory(struct frame_manager *fm);

/*
  get a display frame for render
*/
int fm_render_get_frame(struct frame_manager *fm, struct mpp_frame *frame);

/*
  render put the mpp frame (get from fm_external_get_frame) to frame_manager

  internal ref count will minus 1 (not used by render)
*/
int fm_render_put_frame(struct frame_manager* fm, struct mpp_frame *frame);

/*
  decoder return the frame to frame_manager.the frame will not used by decoder any more.
  1) the frame is a refrence frame, call this when it is not be used as refrence frame
  2) the frame is an non-refrence frame, call this after fm_decoder_frame_to_render

  internal ref count will minus 1 (not used by decode)
*/
int fm_decoder_put_frame(struct frame_manager* fm, struct frame *frame);

/*
  decoder reclaim all used frame to frame_manager.the frame will not used by decoder any more.
*/
int fm_decoder_reclaim_all_used_frame(struct frame_manager* fm);

/*
  get a empty frame for decoder.

  internal ref count will add 1 (used by decode)
*/
struct frame *fm_decoder_get_frame(struct frame_manager *fm);

/*
  codec put the decoded frame to frame_manager,
1) the frame is decoded, need be render (display = 1)
2) the frame is decoded, but no need to render (display = 0)

  internal ref count will add 1 (used by render)
*/
int fm_decoder_frame_to_render(struct frame_manager *fm, struct frame *frame, int display);

/*
	get a frame by id for decoder.
*/
struct frame *fm_get_frame_by_id(struct frame_manager *fm, int id);

/*
	get frame number in empty list
*/
int fm_get_empty_frame_num(struct frame_manager *fm);

/*
	get frame number in render list
*/
int fm_get_render_frame_num(struct frame_manager *fm);


int fm_reset(struct frame_manager *fm);

#endif /* FRAME_MANAGER_H */
