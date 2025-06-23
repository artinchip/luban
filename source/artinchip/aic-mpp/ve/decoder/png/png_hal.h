/*
 * Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <qi.xu@artinchip.com>
 *  Desc: png register define
 */

#ifndef PNG_HAL_H
#define PNG_HAL_H

#include "ve_top_register.h"
#include "png_decoder.h"

// buffer size
#define LZ77_WINDOW_SIZE 	(32*1024)
#define PALLETE_SIZE		(1024)

// decode status
#define PNG_FINISH		(1)
#define PNG_ERROR		(2)
#define PNG_BITREQ		(4)

struct reg_inflate_interrupt
{
	unsigned finish_en : 1;		// [0]: finish interrupt enable
	unsigned error_en : 1;		// [1]: error interrupt enable
	unsigned bit_request_en : 1;	// [2]: bitrequest interrupt enable
	unsigned overtime_en : 1;	// [3]: overtime interrupt enable
	unsigned reserve : 28;
};
#define INFLATE_INTERRUPT_REG		(PNG_REG_OFFSET_ADDR + 0x00)

struct reg_inflate_status
{
	unsigned finish : 1;		// [0]: decode finish
	unsigned error : 1;		// [1]: decode error
	unsigned bit_request : 1;	// [2]: request bitstream
	unsigned overtime : 1;		// [3]: overtime
	unsigned outbuf_overflow : 1;	// [4]: out buf overflow
	unsigned header_dec_err : 3;
	unsigned ccl_err : 1;
	unsigned cl_err : 2;
	unsigned lz77_dec_err : 4;
	unsigned rdma_err : 4;
	unsigned wdma_err : 3;
	unsigned dctrl_err : 1;
	unsigned reserve : 9;
};
#define INFLATE_STATUS_REG		(PNG_REG_OFFSET_ADDR + 0x04)

//[0]: decode start
#define INFLATE_START_REG		(PNG_REG_OFFSET_ADDR + 0x08)

struct reg_png_ctrl
{
	unsigned dec_type : 2;		// [0]: 0-inflate; 1-inflate & png defilter
	unsigned check_func : 2;	// [2:1]: 0-no; 1-adler32; 2-crc32
	unsigned r1 : 4;
	unsigned color_type : 3;	// [10:8]: 2-PNG24; 3-palette; 6-PNG32
	unsigned bit_depth : 2;		// [12:11]: 0-8bits; 1-4bits; 2-2bits; 3-1bits;
	unsigned r2 : 19;
};
#define PNG_CTRL_REG			(PNG_REG_OFFSET_ADDR + 0x10)

struct reg_png_size
{
	unsigned width : 13;	// [12:0]
	unsigned r1 : 3;
	unsigned height : 13;	// [28:16]
	unsigned r2 : 3;
};
#define PNG_SIZE_REG			(PNG_REG_OFFSET_ADDR + 0x14)

#define PNG_STRIDE_REG			(PNG_REG_OFFSET_ADDR + 0x18)

// [2:0]: 0-argb8888; 1-ABGR8888; 2-RGBA8888; 3-BGRA8888;
//	4-RGB888; 5-BGR888;
#define PNG_FORMAT_REG			(PNG_REG_OFFSET_ADDR + 0x1C)

// bit stream buffer reg
#define INPUT_BS_START_ADDR_REG		(PNG_REG_OFFSET_ADDR + 0x20)
#define INPUT_BS_END_ADDR_REG		(PNG_REG_OFFSET_ADDR + 0x24)
#define INPUT_BS_OFFSET_REG		(PNG_REG_OFFSET_ADDR + 0x28)
#define INPUT_BS_LENGTH_REG		(PNG_REG_OFFSET_ADDR + 0x2C)

// output buffer reg
#define OUTPUT_BUFFER_ADDR_REG		(PNG_REG_OFFSET_ADDR + 0x30)
#define OUTPUT_BUFFER_LENGTH_REG	(PNG_REG_OFFSET_ADDR + 0x34)

// decode output data, byte unit
#define OUTPUT_COUNT_REG		(PNG_REG_OFFSET_ADDR + 0x38)


#define INFLATE_CHECKSUM_REG		(PNG_REG_OFFSET_ADDR + 0x3C)

// LZ77 window Buffer is 32K Bytes
#define INFLATE_WINDOW_BUFFER_ADDR_REG	(PNG_REG_OFFSET_ADDR + 0x40)

// Inflate buffer
#define PNG_FILTER_LINE_BUF_ADDR_REG	(PNG_REG_OFFSET_ADDR + 0x44)

struct reg_inflate_bs_valid
{
	unsigned first : 1;
	unsigned last : 1;
	unsigned r : 29;
	unsigned valid : 1;
};
#define INPUT_BS_DATA_VALID_REG		(PNG_REG_OFFSET_ADDR + 0x48)

struct reg_png_palette_addr
{
	unsigned addr : 32; // PNG palette address
	/*
	______________________________________________________
	| index | bit 31:24 | bit 23:16 | bit 15:8 | bit 7:0 |
	|_______|___________|___________|__________|_________|
	|  0    | alpha[0]  | R[0]      | G[0]     |  B[0]   |
	|  ...                                               |
	| 255   | alpha[255]| R[255]    | G[255]   |  B[255] |
	|----------------------------------------------------|
	*/
};
#define PNG_PNG_PALETTE_ADDR_REG	(PNG_REG_OFFSET_ADDR + 0x4C)

// [0]: reset
#define INFLATE_RESET_REG		(PNG_REG_OFFSET_ADDR + 0x50)

// read data bit offset
#define PNG_BIT_OFFSET_HW_REG		(PNG_REG_OFFSET_ADDR + 0xbc)

// png clock
#define PNG_COUNT_REG			(PNG_REG_OFFSET_ADDR + 0xc0)
/***********************************************************************/

struct png_register_list
{
	struct reg_png_ctrl                   _10_png_ctrl;
	struct reg_png_size                   _14_png_size;
	struct reg_inflate_bs_valid           _48_inflate_valid;
};

int png_hardware_decode(struct png_dec_ctx* s, unsigned char* buf, int length);
int gzip_hardware_decode(struct png_dec_ctx* s, unsigned char* buf, int length);

#endif
