/*
 * Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <qi.xu@artinchip.com>
 *  Desc: png decode
 *
 */

#ifndef PNG_DECODER_H
#define PNG_DECODER_H

#include "mpp_codec.h"
#include "ve_buffer.h"
#include "bytestream.h"

#define PNG_MAX_RAWSIZE (1024*1024*8)
#define INFLATE_MAX_OUTPUT (1024*1024*8)

#define ARGB8888   0
#define ABGR8888   1
#define RGBA8888   2
#define BGRA8888   3
#define RGB888     4
#define BGR888     5
#define RGB565     6
#define BGR565     7

#define PNG_COLOR_MASK_PALETTE    1
#define PNG_COLOR_MASK_COLOR      2
#define PNG_COLOR_MASK_ALPHA      4

#define PNG_COLOR_TYPE_GRAY 0
#define PNG_COLOR_TYPE_PALETTE  (PNG_COLOR_MASK_COLOR | PNG_COLOR_MASK_PALETTE)
#define PNG_COLOR_TYPE_RGB        (PNG_COLOR_MASK_COLOR)
#define PNG_COLOR_TYPE_RGB_ALPHA  (PNG_COLOR_MASK_COLOR | PNG_COLOR_MASK_ALPHA)
#define PNG_COLOR_TYPE_GRAY_ALPHA (PNG_COLOR_MASK_ALPHA)

#define PNG_FILTER_TYPE_LOCO   64
#define PNG_FILTER_VALUE_NONE  0
#define PNG_FILTER_VALUE_SUB   1
#define PNG_FILTER_VALUE_UP    2
#define PNG_FILTER_VALUE_AVG   3
#define PNG_FILTER_VALUE_PAETH 4
#define PNG_FILTER_VALUE_MIXED 5

#define PNG_IHDR      0x0001
#define PNG_IDAT      0x0002
#define PNG_ALLIMAGE  0x0004
#define PNG_PLTE      0x0008

#define PNGSIG 0x89504e470d0a1a0a
#define MNGSIG 0x8a4d4e470d0a1a0a

enum {
	APNG_DISPOSE_OP_NONE       = 0,
	APNG_DISPOSE_OP_BACKGROUND = 1,
	APNG_DISPOSE_OP_PREVIOUS   = 2,
};

enum {
	APNG_BLEND_OP_SOURCE = 0,
	APNG_BLEND_OP_OVER   = 1,
};

struct png_dec_ctx {
	struct mpp_decoder decoder;
	get_byte_ctx gb;

	enum mpp_pixel_format pix_fmt;		// output pixel format
	struct frame* curr_frame;		// current output frame
	struct packet* curr_packet;

	struct ve_buffer_allocator *ve_buf_handle;
	int bitstream_buffer_size;
	int ve_fd;

	int packet_size;

	void *reg_list;
	unsigned long regs_base;

	struct ve_buffer *filter_mpp_buf;
	struct ve_buffer *palette_mpp_buf;
	struct ve_buffer *lz77_mpp_buf;
	struct ve_buffer *idat_mpp_buf;
	int idat_data_size;			// inflate data size in IDAT

	int state;
	int width, height;			// width and height in IHDR

	int stride; 				// align output width

	int cur_w, cur_h;
	int x_offset, y_offset;
	uint8_t dispose_op, blend_op;
	uint8_t last_dispose_op;
	int bit_depth;
	int color_type;
	int compression_type;
	int filter_type;
	int channels;
	int bits_per_pixel;

	uint32_t palette[256];

	int hw_size;
	int vbv_offset;
};

#endif /* PNG_DECODER_H */
