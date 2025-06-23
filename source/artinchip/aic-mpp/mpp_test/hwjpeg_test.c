/*
 * Copyright (C) 2020-2023 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <qi.xu@artinchip.com>
 *  Desc: jpeg hardware decode demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <linux/types.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <video/artinchip_fb.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <sys/mman.h>

#include "ve.h"
#include "ve_buffer.h"
#include "read_bits.h"
#include "mpp_log.h"

#define MAX_COMPONENTS 4
#define MAX_INDEX 4

#define JPEG420		0
#define JPEG411 	1 // not support
#define JPEG422		2
#define JPEG444		3
#define JPEG422T	4
#define JPEG400		5
#define JPEGERR		6

#define ALIGN_8B(x) (((x) + (7)) & ~(7))
#define ALIGN_16B(x) (((x) + (15)) & ~(15))
#define ALIGN_1024B(x) (((x) + (1023)) & ~(1023))

enum jpeg_marker {
	/* start of frame */
	SOF0  = 0xc0,       /* baseline */
	SOF1  = 0xc1,       /* extended sequential, huffman */
	SOF2  = 0xc2,       /* progressive, huffman */
	SOF3  = 0xc3,       /* lossless, huffman */
	DHT   = 0xc4,       /* define huffman tables */
	SOI   = 0xd8,       /* start of image */
	EOI   = 0xd9,       /* end of image */
	SOS   = 0xda,       /* start of scan */
	DQT   = 0xdb,       /* define quantization tables */
	DRI   = 0xdd,       /* define restart interval */
};

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

struct jpeg_ctx {
	struct read_bit_context gb;
	unsigned long regs_base;

	unsigned char* buf_start;
	int buf_size;
	int ve_fd;
	int dma_fd;
	int input_buf_fd;
	unsigned int input_phy_addr;
	struct mpp_frame output_frame;
	unsigned int frame_phy_addr[3];

	enum mpp_pixel_format pix_fmt;     // if we support out_pix_fmt, pix_fmt = out_pix_fmt
	enum mpp_pixel_format out_pix_fmt; // output pixel format from config
	int yuv2rgb;
	int uv_interleave;

	int start_code;
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

	int rm_h_stride[MAX_COMPONENTS];	// hor stride after post-process
	int rm_v_stride[MAX_COMPONENTS];	// ver stride after post-process
	int rm_h_real_size[MAX_COMPONENTS];	// hor real size after post-process
	int rm_v_real_size[MAX_COMPONENTS];	// ver real size after post-process
	int h_offset[MAX_COMPONENTS];		// hor crop offset after post-process
	int v_offset[MAX_COMPONENTS];		// ver crop offset after post-process
};

/********************* config regs ********************************************/
// regs
#define JPG_REG_OFFSET_ADDR     0x2000
#define VE_CLK_REG		0x00
#define VE_RST_REG		0x04
#define VE_INIT_REG		0x08
#define VE_IRQ_REG		0x0C
#define VE_JPG_EN_REG		0x14
#define JPG_START_REG 		(JPG_REG_OFFSET_ADDR + 0x00)
#define JPG_STATUS_REG 		(JPG_REG_OFFSET_ADDR + 0x04)
#define JPG_START_POS_REG 	(JPG_REG_OFFSET_ADDR + 0x0c)
#define JPG_CTRL_REG 		(JPG_REG_OFFSET_ADDR + 0x10)
#define JPG_SIZE_REG 		(JPG_REG_OFFSET_ADDR + 0x14)
#define JPG_MCU_INFO_REG 	(JPG_REG_OFFSET_ADDR + 0x18)
#define JPG_ROTMIR_REG 		(JPG_REG_OFFSET_ADDR + 0x1C)
#define JPG_SCALE_REG 		(JPG_REG_OFFSET_ADDR + 0x20)
#define JPG_CLIP_REG 		(JPG_REG_OFFSET_ADDR + 0x28)
#define JPG_HANDLE_NUM_REG 	(JPG_REG_OFFSET_ADDR + 0x2C)
#define JPG_UV_REG 		(JPG_REG_OFFSET_ADDR + 0x30)
#define JPG_FRAME_IDX_REG 	(JPG_REG_OFFSET_ADDR + 0x40)
#define JPG_HUFF_INFO_REG 	(JPG_REG_OFFSET_ADDR + 0x80)
#define JPG_HUFF_ADDR_REG 	(JPG_REG_OFFSET_ADDR + 0x84)
#define JPG_HUFF_VAL_REG 	(JPG_REG_OFFSET_ADDR + 0x88)
#define JPG_QMAT_INFO_REG 	(JPG_REG_OFFSET_ADDR + 0x90)
#define JPG_QMAT_ADDR_REG 	(JPG_REG_OFFSET_ADDR + 0x94)
#define JPG_QMAT_VAL_REG 	(JPG_REG_OFFSET_ADDR + 0x98)
#define JPG_RST_INTERVAL_REG 	(JPG_REG_OFFSET_ADDR + 0xb0)
#define JPG_INTRRUPT_EN_REG 	(JPG_REG_OFFSET_ADDR + 0xc0)
#define JPG_CYCLES_REG 		(JPG_REG_OFFSET_ADDR + 0xc8)
#define JPG_SUB_CTRL_REG 	(JPG_REG_OFFSET_ADDR + 0x100)
#define JPG_WD_PTR 		(JPG_REG_OFFSET_ADDR + 0x114)
#define JPG_MEM_SA_REG 		(JPG_REG_OFFSET_ADDR + 0x140)
#define JPG_MEM_EA_REG 		(JPG_REG_OFFSET_ADDR + 0x144)
#define JPG_MEM_IA_REG 		(JPG_REG_OFFSET_ADDR + 0x148)
#define JPG_MEM_HA_REG 		(JPG_REG_OFFSET_ADDR + 0x14C)
#define JPG_RBIT_OFFSET_REG 	(JPG_REG_OFFSET_ADDR + 0x160)
#define JPG_WBIT_OFFSET_REG 	(JPG_REG_OFFSET_ADDR + 0x164)
#define JPG_REQ_REG 		(JPG_REG_OFFSET_ADDR + 0x200)
#define JPG_STREAM_END_ADDR_REG (JPG_REG_OFFSET_ADDR + 0x208)
#define JPG_STREAM_READ_PTR_REG (JPG_REG_OFFSET_ADDR + 0x210)
#define JPG_STREAM_START_ADDR_REG (JPG_REG_OFFSET_ADDR + 0x214)
#define JPG_STREAM_INT_ADDR_REG	(JPG_REG_OFFSET_ADDR + 0x218)
#define JPG_DATA_CNT_REG 	(JPG_REG_OFFSET_ADDR + 0x21c)
#define JPG_COMMAND_REG 	(JPG_REG_OFFSET_ADDR + 0x220)
#define JPG_BUSY_REG 		(JPG_REG_OFFSET_ADDR + 0x224)
#define JPG_BITREQ_EN_REG 	(JPG_REG_OFFSET_ADDR + 0x228)
#define JPG_CUR_POS_REG 	(JPG_REG_OFFSET_ADDR + 0x22c)
#define JPG_BAS_ADDR_REG 	(JPG_REG_OFFSET_ADDR + 0x230)
#define JPG_STREAM_NUM_REG 	(JPG_REG_OFFSET_ADDR + 0x234)

#define __io_br()       do {} while (0)
#define __io_aw()       do {} while (0)
#ifdef ARCH_RISCV
	#define __io_ar(v)      __asm__ __volatile__ ("fence i,r" : : : "memory")
	#define __io_bw()       __asm__ __volatile__ ("fence w,o" : : : "memory")
#else
	#define __io_ar(v)      do {} while (0)
	#define __io_bw()       do {} while (0)
#endif
#define	read_reg_u32(offset)	({ uint32_t __v; __io_br(); __v = (*((volatile uint32_t*)(offset))); __io_ar(__v); __v; })
//#define	write_reg_u32(offset,v)	({ __io_bw(); (*((volatile uint32_t *)(offset)) = (v)); __io_aw(); })
static void write_reg_u32(unsigned long offset, uint32_t v)
{
	//printf("write regs: %08x, %08x\n", offset, v);
	({ __io_bw(); (*((volatile uint32_t *)(offset)) = (v)); __io_aw(); });
}

static void ve_config_ve_top_reg(struct jpeg_ctx *s)
{
	int i;

	logd("config mjpeg reg");
	write_reg_u32(s->regs_base + VE_CLK_REG, 1);

	write_reg_u32(s->regs_base + VE_RST_REG, 0);

	for (i = 0; i < 10; i++) {
		uint32_t val = read_reg_u32(s->regs_base + VE_RST_REG);
		if ((val >> 16) == 0)
			break;
	}

	write_reg_u32(s->regs_base + VE_INIT_REG, 1);
	write_reg_u32(s->regs_base + VE_IRQ_REG, 1);
	write_reg_u32(s->regs_base + VE_JPG_EN_REG, 1);
}

static void ve_config_bitstream_register(struct jpeg_ctx *s,  int offset, int is_last)
{
	int busy = 1;
	logi("config bitstream");
	// Note: if it is the last stream, we need add 1 here, or it will halt
	int stream_num = (s->buf_size + 255) / 256 +1 ;

	uint32_t packet_base_addr = s->input_phy_addr;
	uint32_t base_addr = (packet_base_addr + offset) & (~7);
	// stream end address, 256 byte align
	uint32_t end_addr = packet_base_addr + (stream_num * 256);
	write_reg_u32(s->regs_base + JPG_STREAM_END_ADDR_REG, end_addr);

	// stream read ptr
	write_reg_u32(s->regs_base + JPG_STREAM_READ_PTR_REG, 0);

	// bas address
	write_reg_u32(s->regs_base + JPG_BAS_ADDR_REG, base_addr);
	// start address of bitstream
	write_reg_u32(s->regs_base + JPG_STREAM_START_ADDR_REG, base_addr);

	write_reg_u32(s->regs_base + JPG_STREAM_INT_ADDR_REG, 0);

	// read data cnt 64x32bit
	write_reg_u32(s->regs_base + JPG_DATA_CNT_REG, 64);

	// bit request enable
	write_reg_u32(s->regs_base + JPG_BITREQ_EN_REG, 1);

	write_reg_u32(s->regs_base + JPG_CUR_POS_REG, 0);
	write_reg_u32(s->regs_base + JPG_STREAM_NUM_REG, stream_num);

	write_reg_u32(s->regs_base + JPG_MEM_SA_REG, 0);
	write_reg_u32(s->regs_base + JPG_MEM_EA_REG, 0x7f);
	write_reg_u32(s->regs_base + JPG_MEM_IA_REG, 0);
	write_reg_u32(s->regs_base + JPG_MEM_HA_REG, 0);

	write_reg_u32(s->regs_base + JPG_STATUS_REG, 0xf);
	write_reg_u32(s->regs_base + JPG_REQ_REG, 1);
	do {
		busy = read_reg_u32(s->regs_base + JPG_BUSY_REG);
	} while(busy == 1);

	// init sub module before start
	write_reg_u32(s->regs_base + JPG_SUB_CTRL_REG, 4);
	write_reg_u32(s->regs_base + JPG_RBIT_OFFSET_REG, (offset & 7)*8);
	write_reg_u32(s->regs_base + JPG_SUB_CTRL_REG, 2);
}

/*
quant matrix should be stored in zigzag order, it is also the order parse from DQT
*/
static void ve_config_quant_matrix(struct jpeg_ctx *s)
{
	int comp, i;
	uint32_t val;

	logd("config quant matrix");

	for (comp = 0; comp < 3; comp++) {
		write_reg_u32(s->regs_base + JPG_QMAT_INFO_REG, (comp << 6) | 3);
		write_reg_u32(s->regs_base + JPG_QMAT_ADDR_REG, comp << 6);

		for (i = 0; i < 64; i++) {
			val = s->q_matrixes[s->quant_index[comp]][i];
			write_reg_u32(s->regs_base + JPG_QMAT_VAL_REG, val);
		}
	}

	write_reg_u32(s->regs_base + JPG_QMAT_INFO_REG, 0);
}

/*
4 huffman tables, including DC_Luma\DC_Chroma\AC_LUMA\AC_Chroma
every table including 4 parts:
	1)start_code: the minimum huffman code in all codes with the same code length,
			for example, the minimum code of code length 3 is 3'b101, it is 16'Hffff if not exist
	2)max_code: the max huffman code in all codes with the same code length, it is 16'Hffff if not exist
	3)huff_offset: the symbol offset in huff_val, the symbol is the start code in code length i
	4)huff_val: the symbols of all the huffman code

DC_Luma huff_val address 0-11
DC_Chroma huff_val address 12-23
AC_Luma huff_val address 24-185
AC_Chroma huff_val address 186-347
*/
static void ve_config_huffman_table(struct jpeg_ctx *s)
{
	int i;
	uint32_t val;

	logd("config huffman table");

	//* 1. config start_code
	write_reg_u32(s->regs_base + JPG_HUFF_INFO_REG, 3);
	write_reg_u32(s->regs_base + JPG_HUFF_ADDR_REG, 0);

	// DC_LUMA -> DC_CHROMA -> AC_LUMA ->AC_CHROMA ->
	for (i = 0; i < 16; i++) {
		val = s->huffman_table[0][s->dc_index[0]].start_code[i];
		write_reg_u32(s->regs_base + JPG_HUFF_VAL_REG, val);
	}
	for (i = 0; i < 16; i++) {
		val = s->huffman_table[0][s->dc_index[1]].start_code[i];
		write_reg_u32(s->regs_base + JPG_HUFF_VAL_REG, val);
	}
	for (i = 0; i < 16; i++) {
		val = s->huffman_table[1][s->ac_index[0]].start_code[i];
		write_reg_u32(s->regs_base + JPG_HUFF_VAL_REG, val);
	}
	for (i = 0; i < 16; i++) {
		val = s->huffman_table[1][s->ac_index[1]].start_code[i];
		write_reg_u32(s->regs_base + JPG_HUFF_VAL_REG, val);
	}


	//* 2. config max code
	logd("max_code");
	write_reg_u32(s->regs_base + JPG_HUFF_INFO_REG, (1<< 10) | 3);
	write_reg_u32(s->regs_base + JPG_HUFF_ADDR_REG, (1<< 10));

	for (i = 0; i < 16; i++) {
		val = s->huffman_table[0][s->dc_index[0]].max_code[i];
		write_reg_u32(s->regs_base + JPG_HUFF_VAL_REG, val);
	}
	for (i = 0; i < 16; i++) {
		val = s->huffman_table[0][s->dc_index[1]].max_code[i];
		write_reg_u32(s->regs_base + JPG_HUFF_VAL_REG, val);
	}
	for (i = 0; i < 16; i++) {
		val = s->huffman_table[1][s->ac_index[0]].max_code[i];
		write_reg_u32(s->regs_base + JPG_HUFF_VAL_REG, val);
	}
	for (i = 0; i < 16; i++) {
		val = s->huffman_table[1][s->ac_index[1]].max_code[i];
		write_reg_u32(s->regs_base + JPG_HUFF_VAL_REG, val);
	}

	//* 3. config huffman offset
	write_reg_u32(s->regs_base + JPG_HUFF_INFO_REG, (2<<10) | 3);
	write_reg_u32(s->regs_base + JPG_HUFF_ADDR_REG, (2<<10));

	for (i = 0; i < 16; i++) {
		val = s->huffman_table[0][s->dc_index[0]].offset[i];
		write_reg_u32(s->regs_base + JPG_HUFF_VAL_REG, val);
	}
	for (i = 0; i < 16; i++) {
		val = s->huffman_table[0][s->dc_index[1]].offset[i];
		write_reg_u32(s->regs_base + JPG_HUFF_VAL_REG, val);
	}
	for (i = 0; i < 16; i++) {
		val = s->huffman_table[1][s->ac_index[0]].offset[i];
		write_reg_u32(s->regs_base + JPG_HUFF_VAL_REG, val);
	}
	for (i = 0; i < 16; i++) {
		val = s->huffman_table[1][s->ac_index[1]].offset[i];
		write_reg_u32(s->regs_base + JPG_HUFF_VAL_REG, val);
	}

	//* 4.1 config huffman val (dc luma)
	logd("huff_val dc_luma");
	write_reg_u32(s->regs_base + JPG_HUFF_INFO_REG, (3<<10) | 3);
	write_reg_u32(s->regs_base + JPG_HUFF_ADDR_REG, (3<<10));

	if (s->huffman_table[0][s->dc_index[0]].total_code > 12) {
		loge("dc luma code num : %d", s->huffman_table[0][s->dc_index[0]].total_code);
		//abort();
	}
	for (i = 0; i < s->huffman_table[0][s->dc_index[0]].total_code; i++) {
		val = s->huffman_table[0][s->dc_index[0]].symbol[i];
		write_reg_u32(s->regs_base + JPG_HUFF_VAL_REG, val);
	}

	//* 4.2 config huffman val (dc chroma)
	logd("huff_val dc_chroma");
	write_reg_u32(s->regs_base + JPG_HUFF_INFO_REG, (3<<10) | 3);

	write_reg_u32(s->regs_base + JPG_HUFF_ADDR_REG, (3<<10) | 12);
	if (s->huffman_table[0][s->dc_index[1]].total_code > 12) {
		loge("dc chroma code num : %d", s->huffman_table[0][s->dc_index[1]].total_code);
		//abort();
	}
	for (i = 0; i < s->huffman_table[0][s->dc_index[1]].total_code; i++) {
		val = s->huffman_table[0][s->dc_index[1]].symbol[i];
		write_reg_u32(s->regs_base + JPG_HUFF_VAL_REG, val);
		logv("write JPG_HUFF_VAL_REG %02X %02X", JPG_HUFF_VAL_REG, val);
	}

	//* 4.3 config huffman val (ac luma)
	write_reg_u32(s->regs_base + JPG_HUFF_INFO_REG, (3<<10) | 3);
	write_reg_u32(s->regs_base + JPG_HUFF_ADDR_REG, (3<<10) | 24);

	if (s->huffman_table[1][s->ac_index[0]].total_code > 162) {
		loge("ac luma code num : %d", s->huffman_table[1][s->ac_index[0]].total_code);
		//abort();
	}
	for (i = 0; i < s->huffman_table[1][s->ac_index[0]].total_code; i++) {
		val = s->huffman_table[1][s->ac_index[0]].symbol[i];
		write_reg_u32(s->regs_base + JPG_HUFF_VAL_REG, val);
	}

	//* 4.4 config huffman val (ac chroma)
	write_reg_u32(s->regs_base + JPG_HUFF_INFO_REG, (3<<10) | 3);
	write_reg_u32(s->regs_base + JPG_HUFF_ADDR_REG, (3<<10) | 186);

	if (s->huffman_table[1][s->ac_index[1]].total_code > 162) {
		loge("dc chroma code num : %d", s->huffman_table[1][s->ac_index[1]].total_code);
		//abort();
	}
	for (i = 0; i < s->huffman_table[1][s->ac_index[1]].total_code; i++) {
		val = s->huffman_table[1][s->ac_index[1]].symbol[i];
		write_reg_u32(s->regs_base + JPG_HUFF_VAL_REG, val);
	}

	// clear
	write_reg_u32(s->regs_base + JPG_HUFF_INFO_REG, 0);
}

static void config_jpeg_picture_info_register(struct jpeg_ctx *s)
{
	logi("config picture info");
	uint32_t val;
	int color_mode = 0;
	if (s->pix_fmt == MPP_FMT_YUV420P || s->pix_fmt == MPP_FMT_NV12
	|| s->pix_fmt == MPP_FMT_NV21) {
		color_mode = 0;
	} else if(s->pix_fmt == MPP_FMT_YUV444P) {
		color_mode = 3;
	} else if (s->pix_fmt == MPP_FMT_YUV400) {
		color_mode = 4;
	} else if (s->pix_fmt == MPP_FMT_YUV422P || s->pix_fmt == MPP_FMT_NV16
	|| s->pix_fmt == MPP_FMT_NV61) {
		color_mode = 1;
	} else {
		loge("not supprt this format(%d)", s->pix_fmt);
		//abort();
	}

	val = s->output_frame.buf.stride[0] | (s->uv_interleave << 16) | (color_mode << 17);

#define FRAME_FORMAT_REG	0x1400
#define FRAME_SIZE_REG		0x1404
#define FRAME_YADDR_REG		0x1408
#define FRAME_CBADDR_REG	0x140C
#define FRAME_CRADDR_REG	0x1410

	logi("size: %d %d", s->rm_h_real_size[0], s->rm_h_real_size[1]);
	write_reg_u32(s->regs_base + FRAME_FORMAT_REG, val);
	write_reg_u32(s->regs_base + FRAME_SIZE_REG, (s->width << 16) | s->height);
	write_reg_u32(s->regs_base + FRAME_YADDR_REG, s->frame_phy_addr[0]);
	write_reg_u32(s->regs_base + FRAME_CBADDR_REG, s->frame_phy_addr[1]);
	write_reg_u32(s->regs_base + FRAME_CRADDR_REG, s->frame_phy_addr[2]);
}

static void config_header_info(struct jpeg_ctx *s)
{
	uint32_t val = 0;

	write_reg_u32(s->regs_base + JPG_START_POS_REG, 0);

	val = (s->have_dht << 6) | (s->dc_index[0] <<10) | (s->dc_index[1] <<9) | (s->dc_index[2] << 8) |
		(s->ac_index[0] <<14) | (s->ac_index[1] <<13) | (s->ac_index[2] <<12);
	write_reg_u32(s->regs_base + JPG_CTRL_REG, val);

	logd("picture size");
	int mcu_w = s->h_count[0] * 8;
	int mcu_h = s->v_count[0] * 8;
	int jpeg_hsize = (s->width + mcu_w - 1) / mcu_w * mcu_w;
	int jpeg_vsize = (s->height + mcu_h - 1) / mcu_h * mcu_h;
	write_reg_u32(s->regs_base + JPG_SIZE_REG, (jpeg_hsize << 16) | jpeg_vsize);

	int tatal_blks = s->h_count[0] * s->v_count[0] +
		s->h_count[1] * s->v_count[1] + s->h_count[2] * s->v_count[2];
	val = s->v_count[1] | (s->h_count[1] << 2) | (s->v_count[1] << 4) | (s->h_count[1] << 6) |
		(s->v_count[0] << 8) | (s->h_count[0] << 10) | (s->nb_components << 12) |
		(tatal_blks << 16);
	write_reg_u32(s->regs_base + JPG_MCU_INFO_REG, val);

	int num = 12 / tatal_blks;
	write_reg_u32(s->regs_base + JPG_HANDLE_NUM_REG,  num > 4? 4: num);

	write_reg_u32(s->regs_base + JPG_UV_REG, s->uv_interleave);
	write_reg_u32(s->regs_base + JPG_FRAME_IDX_REG, 0);
	write_reg_u32(s->regs_base + JPG_RST_INTERVAL_REG, s->restart_interval);
	write_reg_u32(s->regs_base + JPG_INTRRUPT_EN_REG, 0);
}

int ve_decode_jpeg(struct jpeg_ctx *s, int byte_offset)
{
	unsigned int status;

	ve_get_client();

	//* 1. config ve top
	ve_config_ve_top_reg(s);

	//* 2. config header info
	config_header_info(s);
	config_jpeg_picture_info_register(s);

	//* 3. config quant matrix
	ve_config_quant_matrix(s);

	//* 4. config huffman table
	if(s->have_dht) {
		ve_config_huffman_table(s);
	}

	//* 5. config bitstream
	ve_config_bitstream_register(s, byte_offset, 1);

	//* 6. decode start
	write_reg_u32(s->regs_base + JPG_START_REG, 1);

	if(ve_wait(&status) < 0) {
		loge("ve wait irq timeout");
		logi("read JPG_STATUS_REG  %x", read_reg_u32(s->regs_base + JPG_STATUS_REG));

		ve_reset();
		ve_put_client();
		return -1;
	}

	if(status > 1) {
		loge("status error");
		ve_reset();
		ve_put_client();
		return -1;
	}

	logi("ve status %x", status);

	logi("read JPG_CYCLES_REG  %x", read_reg_u32(s->regs_base + JPG_CYCLES_REG));

	ve_put_client();
	return 0;
}
/*****************************************************************************/

static void print_help(char* prog)
{
	printf("%s\n", prog);
	printf("Compile time: %s\n", __TIME__);
	printf("Usage:\n\n"
		"Options:\n"
		" -i                             input file name\n"
		" -h                             help\n\n"
		"End:\n");
}

static int get_file_size(FILE* fp)
{
	int len = 0;
	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	return len;
}

/************************** render frame *************************************/
static int set_fb_layer_alpha(int fb0_fd, int val)
{
	int ret = 0;
	struct aicfb_alpha_config alpha = {0};

	if (fb0_fd < 0)
		return -1;

	alpha.layer_id = 1;
	alpha.enable = 1;
	alpha.mode = 1;
	alpha.value = val;
	ret = ioctl(fb0_fd, AICFB_UPDATE_ALPHA_CONFIG, &alpha);
	if (ret < 0)
		loge("fb ioctl() AICFB_UPDATE_ALPHA_CONFIG failed!");

	return ret;
}

static void video_layer_set(int fb0_fd, struct mpp_buf *picture_buf)
{
	struct aicfb_layer_data layer = {0};
	int dmabuf_num = 0;
	struct dma_buf_info dmabuf_fd[3];
	int data_size[3];
	int i;

	if (fb0_fd < 0)
		return;

	layer.layer_id = AICFB_LAYER_TYPE_VIDEO;
	layer.enable = 1;
	layer.scale_size.width = 0;
	layer.scale_size.height= 0;

	layer.pos.x = 0;
	layer.pos.y = 0;
	memcpy(&layer.buf, picture_buf, sizeof(struct mpp_buf));

	logi("width: %d, height: %d, format: %d", picture_buf->size.width,
		picture_buf->size.height, picture_buf->format);
	logi("x: %d, y: %d, width: %d, height: %d", picture_buf->crop.x, picture_buf->crop.y,
		picture_buf->crop.width, picture_buf->crop.height);

	data_size[0] = picture_buf->size.height * picture_buf->stride[0];
	if (picture_buf->format == MPP_FMT_ARGB_8888) {
		dmabuf_num = 1;
	} else if (picture_buf->format == MPP_FMT_RGBA_8888) {
		dmabuf_num = 1;
	} else if (picture_buf->format == MPP_FMT_RGB_888) {
		dmabuf_num = 1;
	} else if (picture_buf->format == MPP_FMT_YUV420P) {
		dmabuf_num = 3;
		data_size[1] = data_size[2] = data_size[0]/4;
	} else if (picture_buf->format == MPP_FMT_YUV444P) {
		dmabuf_num = 3;
		data_size[1] = data_size[2] = data_size[0];
	} else if (picture_buf->format == MPP_FMT_YUV422P) {
		dmabuf_num = 3;
		data_size[1] = data_size[2] = data_size[0]/2;
	} else if (picture_buf->format == MPP_FMT_YUV400) {
		dmabuf_num = 1;
	} else {
		loge("no support picture foramt %d, default argb8888", picture_buf->format);
	}

#if 0
	unsigned char* hw_data[3];
	FILE* fp_save = fopen("save.bin", "wb");
	for (i=0; i<dmabuf_num; i++) {
		hw_data[i] = mmap(NULL, data_size[i], PROT_READ, MAP_SHARED, picture_buf->fd[i], 0);
		if (hw_data[i] == MAP_FAILED) {
			loge("dmabuf alloc mmap failed!");
			return ;
		}
		if(fp_save)
			fwrite(hw_data[i], 1, data_size[i], fp_save);
	}

	for (i=0; i<dmabuf_num; i++) {
		munmap(hw_data[i], data_size[i]);
	}
	fclose(fp_save);
#endif
	//* add dmabuf to de driver
	for(i=0; i<dmabuf_num; i++) {
		dmabuf_fd[i].fd = picture_buf->fd[i];
		if (ioctl(fb0_fd, AICFB_ADD_DMABUF, &dmabuf_fd[i]) < 0)
			loge("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");
	}

	//* update layer config (it is async interface)
	if (ioctl(fb0_fd, AICFB_UPDATE_LAYER_CONFIG, &layer) < 0)
		loge("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");

	//* wait vsync (wait layer config)
	ioctl(fb0_fd, AICFB_WAIT_FOR_VSYNC, NULL);

	//* display this picture 2 seconds
	usleep(2000000);

	//* disable layer
	layer.enable = 0;
	if(ioctl(fb0_fd, AICFB_UPDATE_LAYER_CONFIG, &layer) < 0)
		loge("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");

	//* remove dmabuf to de driver
	for(i=0; i<dmabuf_num; i++) {
		if (ioctl(fb0_fd, AICFB_RM_DMABUF, &dmabuf_fd[i]) < 0)
			loge("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");
	}
}

static int render_frame(struct mpp_frame* frame)
{
	int fb0_fd = 0;
	struct fb_fix_screeninfo finfo;

	//* 1. open fb0
	fb0_fd = open("/dev/fb0", O_RDWR);
	if (fb0_fd < 0) {
		logw("open fb0 failed!");
		return -1;
	}

	if (ioctl(fb0_fd, FBIOGET_FSCREENINFO, &finfo) == -1) {
		loge("read fixed information failed!");
		close(fb0_fd);
		return -1;
	}

	//* 2 disp frame;
	set_fb_layer_alpha(fb0_fd, 10);
	video_layer_set(fb0_fd, &frame->buf);

	if (fb0_fd >= 0)
		close(fb0_fd);
	return 0;
}

/************************ parse jpeg header **************************************************/
/* return the 8 bit start code value and update the search
   state. Return -1 if no start code found */
static int find_marker(const uint8_t **pbuf_ptr, const uint8_t *buf_end)
{
	const uint8_t *buf_ptr;
	unsigned int v, v2;
	int val;
	int skipped = 0;

	buf_ptr = *pbuf_ptr;
	while (buf_end - buf_ptr > 1) {
		v  = *buf_ptr++;
		v2 = *buf_ptr;
		if ((v == 0xff) && (v2 >= SOF0) && (v2 <= 0xfe) && buf_ptr < buf_end) {
			val = *buf_ptr++;
			goto found;
		}
		skipped++;
	}
	buf_ptr = buf_end;
	val = -1;
found:
	*pbuf_ptr = buf_ptr;

	return val;
}

static int mjpeg_find_marker(struct jpeg_ctx *s,
			const uint8_t **buf_ptr, const uint8_t *buf_end,
			const uint8_t **unescaped_buf_ptr,
			int *unescaped_buf_size)
{
	int start_code;
	start_code = find_marker(buf_ptr, buf_end);

	*unescaped_buf_ptr  = *buf_ptr;
	*unescaped_buf_size = buf_end - *buf_ptr;

	return start_code;
}

static void skip_variable_marker(struct jpeg_ctx *s)
{
	int left = read_bits(&s->gb, 16);
	left -= 2;
	while(left) {
		skip_bits(&s->gb, 8);
		left --;
	}
}

static void fill_huffman_startcode(struct jpeg_ctx *s, int class, int index, const uint8_t *bits_table)
{
	int i, j, k, nb, code;

	code = 0;
	k = 0;
	for (i=1; i<=16; i++) {
		nb = bits_table[i];
		s->huffman_table[class][index].start_code[i - 1] = code;
		s->huffman_table[class][index].max_code[i - 1] = code + nb-1;

		for (j=0; j<nb; j++) {
			s->huffman_table[class][index].code[k] = code;
			s->huffman_table[class][index].len[k] = i;
			k++;
			code++;
		}
		code <<= 1;
	}

	s->huffman_table[class][index].total_code = k;
	for (i=16; i>=1; i--) {
		if (bits_table[i] == 0) {
			s->huffman_table[class][index].start_code[i - 1] = 0xffff;
			s->huffman_table[class][index].max_code[i - 1] = 0xffff;
		}
		else
			break;
	}
}

/* decode huffman tables and build VLC decoders */
int mjpeg_decode_dht(struct jpeg_ctx *s)
{
	int len, index, i, class, n, v, code_max;
	uint8_t bits_table[17];

	logd("====== DHT (huffman table) ======");

	len = read_bits(&s->gb, 16) - 2;

	if (8*len > read_bits_left(&s->gb)) {
		loge("dht: len %d is too large", len);
		return -1;
	}

	while (len > 0) {
		if (len < 17)
			return -1;
		class = read_bits(&s->gb, 4);
		if (class >= 2)
			return -1;
		index = read_bits(&s->gb, 4);
		if (index >= 4)
			return -1;

		logi("class: %d, index: %d", class, index);
		// initial huffman table
		for (i = 0; i < 16; i++) {
			s->huffman_table[class][index].offset[i] = 0;
			s->huffman_table[class][index].start_code[i] = 0;
		}
		for (i = 0; i < 256; i++)
			s->huffman_table[class][index].symbol[i] = 0;

		n = 0;
		// number of huffman code with code length i
		bits_table[0] = 0;

		//* 1. parse BITS(BITS is the number of huffman code which is the same
		// code length,
		//     the max code length is 16), generate HUFFSIZE according BITS
		//     table
		for (i = 1; i <= 16; i++) {
			s->huffman_table[class][index].offset[i - 1] = n;
			bits_table[i] = read_bits(&s->gb, 8);
			s->huffman_table[class][index].bits_table[i - 1] = bits_table[i];
			n += bits_table[i];
		}
		len -= 17;
		if (len < n || n > 256)
			return -1;

		code_max = 0;
		//* 2.parse HUFFVAL table, the huffman code to symbol
		for (i = 0; i < n; i++) {
			v = read_bits(&s->gb, 8);
			if (v > code_max)
				code_max = v;
			s->huffman_table[class][index].symbol[i] = v;
		}
		len -= n;

		//* 3. generate HUFFCODE table, it is used for config ve
		fill_huffman_startcode(s, class, index, bits_table);
	}
	return 0;
}

/* quantize tables */
int mjpeg_decode_dqt(struct jpeg_ctx *s)
{
	int len, index, i;

	len = read_bits(&s->gb, 16) - 2;

	if (8 * len > read_bits_left(&s->gb)) {
		loge("dqt: len %d is too large", len);
		return -1;
	}

	while (len >= 65) {
		int pr = read_bits(&s->gb, 4);
		if (pr > 1) {
			loge("dqt: invalid precision");
			return -1;
		}
		if (pr != 0) {
			loge("dqt: only support 8 bit precision");
			return -1;
		}
		index = read_bits(&s->gb, 4);
		if (index >= 4)
			return -1;
		logd("index=%d", index);
		/* read quant table */
		for (i = 0; i < 64; i++) {
			s->q_matrixes[index][i] = read_bits(&s->gb, pr ? 16 : 8);
		}

		len -= 1 + 64 * (1 + pr);
	}
	return 0;
}

int mjpeg_decode_sof(struct jpeg_ctx *s)
{
	int len, nb_components, i, bits;
	int phy_h_stride[4]; // hor stride ( before post-process)
	int phy_v_stride[4]; // ver stride ( before post-process)
	int h_real_size[4];  // hor real size ( before post-process)
	int v_real_size[4];  // hor real size ( before post-process)

	logd("===== ff_mjpeg_decode_sof ====== ");

	len = read_bits(&s->gb, 16);
	bits = read_bits(&s->gb, 8);

	if (bits > 16 || bits < 1) {
		loge("bits %d is invalid", bits);
		return -1;
	}

	/* only 8 bits/component accepted */
	if (bits != 8) {
		loge("only support 8 bits, bits %d is invalid", bits);
		return -1;
	}

	s->height = read_bits(&s->gb, 16);
	s->width = read_bits(&s->gb, 16);

	if (s->width < 1 || s->height < 1) {
		loge("size too small: width %d, height %d", s->width, s->height);
		return -1;
	}

	nb_components = read_bits(&s->gb, 8);
	if (nb_components <= 0 || nb_components > MAX_COMPONENTS)
		return -1;

	if (len != 8 + 3 * nb_components) {
		loge("decode_sof0: error, len(%d) mismatch %d components", len, nb_components);
		return -1;
	}

	s->nb_components = nb_components;
	for (i = 0; i < nb_components; i++) {
		/* component id */
		s->component_id[i] = read_bits(&s->gb, 8) - 1;
		s->h_count[i]      = read_bits(&s->gb, 4);
		s->v_count[i]      = read_bits(&s->gb, 4);
		s->quant_index[i]  = read_bits(&s->gb, 8);
		if (s->quant_index[i] >= 4) {
			loge("quant_index is invalid");
			return -1;
		}
		if (!s->h_count[i] || !s->v_count[i]) {
			loge("Invalid sampling factor in component %d %d:%d",
				i, s->h_count[i], s->v_count[i]);
			return -1;
		}

		logd("component %d %d:%d id: %d quant:%d",i, s->h_count[i], s->v_count[i],
			s->component_id[i], s->quant_index[i]);
	}

	logi("s->width %d, s->height %d", s->width, s->height);

	if (s->h_count[0] == 2 && s->v_count[0] == 2 && s->h_count[1] == 1 &&
		s->v_count[1] == 1 && s->h_count[2] == 1 && s->v_count[2] == 1) {
		s->pix_fmt = MPP_FMT_YUV420P;
		logi("pixel format: yuv420");
	} else if (s->h_count[0] == 2 && s->v_count[0] == 1 && s->h_count[1] == 1 &&
			   s->v_count[1] == 1 && s->h_count[2] == 1 && s->v_count[2] == 1) {
		s->pix_fmt = MPP_FMT_YUV422P;
		logi("pixel format: yuv422");
	} else if (s->h_count[0] == 1 && s->v_count[0] == 1 && s->h_count[1] == 1 &&
			   s->v_count[1] == 1 && s->h_count[2] == 1 && s->v_count[2] == 1) {
		s->pix_fmt = MPP_FMT_YUV444P;
		logi("pixel format: yuv444");
	} else if (s->h_count[0] == 1 && s->v_count[0] == 2 && s->h_count[1] == 1 &&
			   s->v_count[1] == 2 && s->h_count[2] == 1 && s->v_count[2] == 2) {
		s->pix_fmt = MPP_FMT_YUV444P;
		logi("pixel format: ffmpeg yuv444");
	} else if (s->h_count[1] == 0 && s->v_count[1] == 0 && s->h_count[2] == 0 &&
			   s->v_count[2] == 0) {
		s->pix_fmt = MPP_FMT_YUV400;
		logi("pixel format: yuv400");
	} else {
		loge("Not support format! h_count: %d %d %d, v_count: %d %d %d",
			s->h_count[0], s->h_count[1], s->h_count[2],
			s->v_count[0], s->v_count[1], s->v_count[2]);
		return -1;
	}

	//* get the output size of scale down
	s->nb_mcu_width = (s->width + 8 * s->h_count[0] - 1) / (8 * s->h_count[0]);
	s->nb_mcu_height = (s->height + 8 * s->v_count[0] - 1) / (8 * s->v_count[0]);
	int h_stride_y = (s->nb_mcu_width * s->h_count[0] * 8);
	int v_stride_y = (s->nb_mcu_height * s->v_count[0] * 8);
	phy_h_stride[0] = phy_h_stride[1] = phy_h_stride[2] = (h_stride_y + 15) / 16 * 16;
	phy_v_stride[0] = phy_v_stride[1] = phy_v_stride[2] = (v_stride_y + 15) / 16 * 16;
	h_real_size[0] = s->width;
	v_real_size[0] = s->height;

	if (s->pix_fmt == MPP_FMT_YUV420P) {
		phy_h_stride[1] = phy_h_stride[2] = phy_h_stride[0] / 2;
		phy_v_stride[1] = phy_v_stride[2] = phy_v_stride[0] / 2;
		h_real_size[1] = h_real_size[2] = h_real_size[0] / 2;
		v_real_size[1] = v_real_size[2] = v_real_size[0] / 2;
	} else if (s->pix_fmt == MPP_FMT_YUV444P || s->pix_fmt == MPP_FMT_YUV400) {
		phy_h_stride[0] = (h_stride_y + 7) / 8 * 8;
		phy_v_stride[0] = (v_stride_y + 7) / 8 * 8;
		phy_h_stride[1] = phy_h_stride[2] = phy_h_stride[0];
		phy_v_stride[1] = phy_v_stride[2] = phy_v_stride[0];
		h_real_size[1] = h_real_size[2] = h_real_size[0];
		v_real_size[1] = v_real_size[2] = v_real_size[0];
	} else if (s->pix_fmt == MPP_FMT_YUV422P) {
		phy_h_stride[1] = phy_h_stride[2] = phy_h_stride[0] / 2;
		phy_v_stride[1] = phy_v_stride[2] = phy_v_stride[0];
		h_real_size[1] = h_real_size[2] = h_real_size[0] / 2;
		v_real_size[1] = v_real_size[2] = v_real_size[0];
	}

	//* get the output size of rotate
	for (int k = 0; k < 3; k++) {
		s->rm_h_real_size[k] = h_real_size[k];
		s->rm_v_real_size[k] = v_real_size[k];
		s->rm_h_stride[k] = phy_h_stride[k];
		s->rm_v_stride[k] = phy_v_stride[k];
	}

	s->got_picture = 1;

	return 0;
}

static int mjpeg_decode_sos(struct jpeg_ctx *s,
                     const uint8_t *mb_bitmask,
                     int mb_bitmask_size)
{
	int len, nb_components, i;
	int index, id;

	logd("===== ff_mjpeg_decode_sos ====== ");

	if (!s->got_picture) {
		logw("Can not process SOS before SOF, skipping");
		return -1;
	}

	//* 1. parse SOS info
	len = read_bits(&s->gb, 16);
	nb_components = read_bits(&s->gb, 8);
	if (nb_components == 0 || nb_components > MAX_COMPONENTS) {
		return -1;
	}
	if (len != 6 + 2 * nb_components) {
		loge("decode_sos: invalid len (%d)", len);
		return -1;
	}

	for (i = 0; i < nb_components; i++) {
		id = read_bits(&s->gb, 8) - 1;
		/* find component index */
		for (index = 0; index < s->nb_components; index++)
			if (id == s->component_id[index])
				break;
		if (index == s->nb_components) {
			loge("decode_sos: index(%d) out of components", index);
			return -1;
		}

		s->dc_index[i] = read_bits(&s->gb, 4);
		s->ac_index[i] = read_bits(&s->gb, 4);

		if (s->dc_index[i] <  0 || s->ac_index[i] < 0 ||
			s->dc_index[i] >= 4 || s->ac_index[i] >= 4) {
			loge("index out of range");
			return -1;
		}
	}

	skip_bits(&s->gb, 8);	/* JPEG Ss / lossless JPEG predictor /JPEG-LS NEAR */
	skip_bits(&s->gb, 8);	/* JPEG Se / JPEG-LS ILV */
	skip_bits(&s->gb, 4);	/* Ah */
	skip_bits(&s->gb, 4);	/* Al */

	int sos_size = read_bits_count(&s->gb) / 8;
	logd("sos_size %d", sos_size);

	s->output_frame.buf.size.height = s->rm_v_stride[0];
	s->output_frame.buf.size.width = s->rm_h_stride[0];
	s->output_frame.buf.stride[0] = s->rm_h_stride[0];
	s->output_frame.buf.stride[1] = s->rm_h_stride[1];
	s->output_frame.buf.stride[2] = s->rm_h_stride[2];
	s->output_frame.buf.format = s->pix_fmt;
	s->output_frame.buf.crop_en = 1;
	s->output_frame.buf.crop.x = 0;
	s->output_frame.buf.crop.y = 0;
	s->output_frame.buf.crop.width = s->width;
	s->output_frame.buf.crop.height = s->height;
	mpp_buf_alloc(s->dma_fd, &s->output_frame.buf);

	logi("width: %d, height: %d", s->output_frame.buf.size.width, s->output_frame.buf.size.height);

	ve_add_dma_buf(s->output_frame.buf.fd[0], &s->frame_phy_addr[0]);
	ve_add_dma_buf(s->output_frame.buf.fd[1], &s->frame_phy_addr[1]);
	ve_add_dma_buf(s->output_frame.buf.fd[2], &s->frame_phy_addr[2]);

	int offset = (s->raw_scan_buffer - s->buf_start) + sos_size;
	logw("offste: %d", offset);
	if (ve_decode_jpeg(s, offset))
		return -1;

	return 0;
}

static int mjpeg_decode_dri(struct jpeg_ctx *s)
{
	if (read_bits(&s->gb, 16) != 4)
		return -1;
	s->restart_interval = read_bits(&s->gb, 16);
	s->restart_count    = 0;
	logd("restart interval: %d", s->restart_interval);

	return 0;
}

static int decode_jpeg(struct jpeg_ctx *s, FILE* fp, int buf_size)
{
	const uint8_t *buf_end, *buf_ptr;
	const uint8_t *unescaped_buf_ptr;
	int unescaped_buf_size;
	int start_code;
	int ret = 0;

	int align_size = ALIGN_1024B(buf_size + 1024);
	s->ve_fd = ve_open_device();
	s->regs_base = ve_get_reg_base();

	// bitstream buffer
	s->dma_fd = dmabuf_device_open();
	s->input_buf_fd = dmabuf_alloc(s->dma_fd, align_size);
	unsigned char* vir_addr = dmabuf_mmap(s->input_buf_fd, align_size);
	fread(vir_addr, buf_size, 1, fp);
	dmabuf_sync(s->input_buf_fd, CACHE_CLEAN);

	s->buf_start = vir_addr;
	s->buf_size = buf_size;

	ve_add_dma_buf(s->input_buf_fd, &s->input_phy_addr);

	if (vir_addr[0] != 0xff && vir_addr[1] != 0xd8) {
		loge("the file is not jpeg");
		return -1;
	}

	buf_ptr = vir_addr;
	buf_end = buf_ptr + buf_size;

	while (buf_ptr < buf_end) {
		/* find start next marker */
		start_code = mjpeg_find_marker(s, &buf_ptr, buf_end,
							&unescaped_buf_ptr,
							&unescaped_buf_size);
		/* EOF */
		if (start_code < 0) {
			break;
		} else if (unescaped_buf_size > INT_MAX / 8) {
			loge("MJPEG packet 0x%x too big (%d/%d), corrupt data?",
				start_code, unescaped_buf_size, buf_size);
			return -1;
		}

		ret = init_read_bits(&s->gb, unescaped_buf_ptr, unescaped_buf_size*8);

		if (ret < 0) {
			loge("invalid buffer");
			goto fail;
		}

		s->start_code = start_code;
		logi("startcode: %02x", start_code);

		ret = -1;

		switch (start_code) {
		case DQT:
			ret = mjpeg_decode_dqt(s);
			if (ret < 0)
				return ret;
			break;
		case SOI:
			s->restart_interval = 0;
			s->restart_count = 0;
			/* nothing to do on SOI */
			break;
		case DHT:
			s->have_dht = 1;
			if ((ret = mjpeg_decode_dht(s)) < 0) {
				loge("huffman table decode error");
				goto fail;
			}
			break;
		case SOF0:
		case SOF1:
		case SOF2:
			if ((ret = mjpeg_decode_sof(s)) < 0)
				goto fail;
			break;

		case EOI:
			if (!s->got_picture) {
				logw("Found EOI before any SOF, ignoring");
				break;
			}
			s->got_picture = 0;

			goto the_end;
		case SOS:
			s->raw_scan_buffer = buf_ptr;
			s->raw_scan_buffer_size = buf_end - buf_ptr;

			if ((ret = mjpeg_decode_sos(s, NULL, 0)) < 0)
				goto fail;

			render_frame(&s->output_frame);
			goto the_end;
		case DRI:
			if ((ret = mjpeg_decode_dri(s)) < 0)
				return ret;
			break;
		default:
			logi("skip this marker: %02x", start_code);
			skip_variable_marker(s);
			break;
		}

		/* eof process start code */
		buf_ptr += (read_bits_count(&s->gb) + 7) / 8;
		logd("marker parser used %d bytes (%d bits)", (read_bits_count(&s->gb) + 7) / 8, read_bits_count(&s->gb));
	}

	loge("No JPEG data found in image");
	return -1;
fail:
	s->got_picture = 0;

the_end:

	dmabuf_munmap(vir_addr, align_size);
	if(s->input_buf_fd > -1) {
		ve_rm_dma_buf(s->input_buf_fd, s->input_phy_addr);
		dmabuf_free(s->input_buf_fd);
	}

	if(s->output_frame.buf.fd[0] > -1)
		ve_rm_dma_buf(s->output_frame.buf.fd[0], s->frame_phy_addr[0]);
	if(s->output_frame.buf.fd[1] > -1)
		ve_rm_dma_buf(s->output_frame.buf.fd[1], s->frame_phy_addr[1]);
	if(s->output_frame.buf.fd[2] > -1)
		ve_rm_dma_buf(s->output_frame.buf.fd[2], s->frame_phy_addr[2]);

	mpp_buf_free(&s->output_frame.buf);

	ve_close_device();
	dmabuf_device_close(s->dma_fd);

	return ret;
}

int main(int argc, char **argv)
{
	int opt;
	int file_len;
	FILE* fp = NULL;
	struct jpeg_ctx ctx;
	memset(&ctx, 0, sizeof(struct jpeg_ctx));

	while (1) {
		opt = getopt(argc, argv, "i:h");
		if (opt == -1) {
			break;
		}
		switch (opt) {
		case 'i':
			logd("file path: %s", optarg);
			fp = fopen(optarg, "rb");

			break;
		case 'h':
		default:
			print_help(argv[0]);
			return -1;
		}
	}

	if (fp == NULL) {
		print_help(argv[0]);
		return -1;
	}


	file_len = get_file_size(fp);
	logi("file_len: %d", file_len);

	decode_jpeg(&ctx, fp, file_len);

	fclose(fp);

	return 0;
}
