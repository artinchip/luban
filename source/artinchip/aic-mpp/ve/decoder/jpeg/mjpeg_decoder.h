/*
* Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
*
*  author: <qi.xu@artinchip.com>
*  Desc: jpeg decode context define
*
*/

#ifndef MJPEG_DECODER_H
#define MJPEG_DECODER_H

#include "mpp_codec.h"
#include "ve_buffer.h"
#include "frame_manager.h"
#include "packet_manager.h"
#include "read_bits.h"

/*
 * the macro COPY_DATA is used for debug.
 * copy jpeg data after SOS chunk to a new buffer,
 * it is not need to config the offset of bitstream
 */
//#define COPY_DATA

#define MAX_COMPONENTS 4
#define MAX_INDEX 4

#define JPEG420		0
#define JPEG411 	1 // not support
#define JPEG422		2
#define JPEG444		3
#define JPEG422T	4
#define JPEG400		5
#define JPEGERR		6

struct jpeg_huffman_table {
	unsigned short start_code[16]; 	// start_code[i], the minimum code of huffman code length i
	unsigned short max_code[16];   	// max_code[i], the max code of huffman code length i
	unsigned char  offset[16];	// the offset in val table of huffman code length i
	unsigned char  bits_table[16];
	unsigned char  symbol[256];	// huffman symbol
	unsigned short code[256];	// HUFFCODE table
	unsigned char  len[256];
	unsigned int   total_code;	// total number of huffman code
};

struct mjpeg_dec_ctx {
	struct mpp_decoder decoder;
	unsigned long regs_base;	// register base address
	int ve_fd;
	struct ve_buffer_allocator *ve_buf_handle;

	struct frame* curr_frame;	// current output frame
	struct packet* curr_packet;

	struct read_bit_context gb;

	int start_code;

	void *reg_list;

	enum mpp_pixel_format pix_fmt;     // if we support out_pix_fmt, pix_fmt = out_pix_fmt
	enum mpp_pixel_format out_pix_fmt; // output pixel format from config
	int yuv2rgb;
	int uv_interleave;

	const uint8_t *raw_scan_buffer;
	size_t         raw_scan_buffer_size;

	struct jpeg_huffman_table huffman_table[2][MAX_INDEX];	// [DC/AC][index]

	uint16_t q_matrixes[MAX_INDEX][64]; 		// 8x8 quant matrix, [index][q_matrix_pos]

	int first_picture;

	int nb_mcu_width;			// mcu aligned width
	int nb_mcu_height; 			// mcu aligned height
	int width, height;
	int nb_components;
	int component_id[MAX_COMPONENTS];
	int h_count[MAX_COMPONENTS]; 		// horizontal count for each component
	int v_count[MAX_COMPONENTS];		// vertical count for each component
	int comp_index[MAX_COMPONENTS];
	int dc_index[MAX_COMPONENTS];
	int ac_index[MAX_COMPONENTS];
	int quant_index[MAX_COMPONENTS];   	// quant table index for each component
	int got_picture;  			// we found a SOF and picture is valid,

	int restart_interval;
	int restart_count;

	int have_dht; 				// use the default huffman table, if there is no DHT marker

	// post-process
	int ver_scale;
	int hor_scale;
	int scale_en;				// scale enable
	int rotate; 				// clockwise rotate angle
	int mirror; 				// 0-disable; 1-horizontal; 2-vertical
	int rotmir_en;				// rotate and mirror enable

	int rm_h_stride[MAX_COMPONENTS];	// hor stride after post-process
	int rm_v_stride[MAX_COMPONENTS];	// ver stride after post-process
	int rm_h_real_size[MAX_COMPONENTS];	// hor real size after post-process
	int rm_v_real_size[MAX_COMPONENTS];	// ver real size after post-process
	int h_offset[MAX_COMPONENTS];		// hor crop offset after post-process
	int v_offset[MAX_COMPONENTS];		// ver crop offset after post-process

#ifdef COPY_DATA
	int sos_length;
	struct ve_buffer *sos_buf;
#endif
	int extra_frame_num;
};

#endif /* MJPEG_DECODER_H */
