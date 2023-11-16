/*
* Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
*
*  author: author: <qi.xu@artinchip.com>
*  Desc: jpeg register configuration
*
*/

#define LOG_TAG "jpeg_hal"
#include <string.h>
#include <unistd.h>
#include "mjpeg_decoder.h"
#include "jpeg_hal.h"
#include "ve.h"
#include "mpp_log.h"

static void ve_config_ve_top_reg(struct mjpeg_dec_ctx *s)
{
	int i;

	logd("config mjpeg reg");
	write_reg_u32(s->regs_base + VE_CLK_REG, 1);

	// [9:8]: pic module reset, [3:2]: jpeg module reset
	write_reg_u32(s->regs_base + VE_RST_REG, RST_JPG_PIC_MODULE);

	for (i = 0; i < 10; i++) {
		u32 val = read_reg_u32(s->regs_base + VE_RST_REG);
		if ((val >> 16) == 0)
			break;
	}

	write_reg_u32(s->regs_base + VE_INIT_REG, 1);
	write_reg_u32(s->regs_base + VE_IRQ_REG, 1);
	write_reg_u32(s->regs_base + VE_JPG_EN_REG, 1);
}

static void ve_config_pp_register(struct mjpeg_dec_ctx *s)
{
	jpg_reg_list *reg_list = (jpg_reg_list *)s->reg_list;

	u32 *pval;

	logd("config pp");

	if (s->scale_en) {
		reg_list->_20_scale_reg.scale_en = 1;
		reg_list->_20_scale_reg.h_ratio = s->hor_scale;
		reg_list->_20_scale_reg.v_ratio = s->ver_scale;
		pval = (u32 *)&reg_list->_20_scale_reg;
		write_reg_u32(s->regs_base + JPG_SCALE_REG, *pval);
	}

	// rotate & mirror
	if (s->rotate != MPP_ROTATION_0 || s->mirror) {
		reg_list->_1c_rotmir_reg.rotmir_en = 1;
		if (s->rotate == MPP_ROTATION_0)
			reg_list->_1c_rotmir_reg.rotate = 0;
		else if(s->rotate == MPP_ROTATION_270) // left rotate 90
			reg_list->_1c_rotmir_reg.rotate = 1;
		else if (s->rotate == MPP_ROTATION_180)
			reg_list->_1c_rotmir_reg.rotate = 2;
		else if (s->rotate == MPP_ROTATION_90)
			reg_list->_1c_rotmir_reg.rotate = 3;

		if (s->mirror == MPP_FLIP_V)
			reg_list->_1c_rotmir_reg.mirror = 1;
		else if (s->mirror == MPP_FLIP_H)
			reg_list->_1c_rotmir_reg.mirror = 2;
		else if(s->mirror == (MPP_FLIP_V | MPP_FLIP_H))
			reg_list->_1c_rotmir_reg.mirror = 3;
		else
			reg_list->_1c_rotmir_reg.mirror = 0;

		pval = (u32 *)&reg_list->_1c_rotmir_reg;
		write_reg_u32(s->regs_base + JPG_ROTMIR_REG, *pval);
	}
}

static void ve_config_bitstream_register(struct mjpeg_dec_ctx *s,  int offset, int is_last)
{
	int busy = 1;
	int bit_offset = 0;
	// Note: if it is the last stream, we need add 1 here, or it will halt
	int stream_num = (s->curr_packet->size + 255) / 256 +1;

	u32 packet_base_addr = s->curr_packet->phy_base + s->curr_packet->phy_offset;
	u32 base_addr = (packet_base_addr + offset) & (~7);
	bit_offset = ((packet_base_addr + offset) - base_addr)*8;


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
	write_reg_u32(s->regs_base + JPG_RBIT_OFFSET_REG, bit_offset);
	write_reg_u32(s->regs_base + JPG_SUB_CTRL_REG, 2);
}

#ifdef COPY_DATA
static void ve_config_bitstream_sos(struct mjpeg_dec_ctx *s)
{
	u32 val;
	int busy = 1;

	// Note: if it is the last stream, we need add 1 here, or it will halt
	int stream_num = (s->sos_length + 255) / 256 +1;

	// stream end address, 256 byte align
	val = s->sos_buf->phy_addr + (stream_num * 256);
	write_reg_u32(s->regs_base + JPG_STREAM_END_ADDR_REG, val);

	// stream read ptr
	write_reg_u32(s->regs_base + JPG_STREAM_READ_PTR_REG, 0);

	// bas address
	write_reg_u32(s->regs_base + JPG_BAS_ADDR_REG, s->sos_buf->phy_addr);

	// start address of bitstream
	write_reg_u32(s->regs_base + JPG_STREAM_START_ADDR_REG, s->sos_buf->phy_addr);

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
	write_reg_u32(s->regs_base + JPG_SUB_CTRL_REG, 2);
}
#endif

/*
quant matrix should be stored in zigzag order, it is also the order parse from DQT
*/
static void ve_config_quant_matrix(struct mjpeg_dec_ctx *s)
{
	int i;
	jpg_reg_list *reg_list = (jpg_reg_list *)s->reg_list;

	u32 *pval;
	u32 val;

	logd("config quant matrix");

	for (int comp = 0; comp < 3; comp++) {
		pval = (u32 *)&reg_list->_90_qmat_info_reg;
		reg_list->_90_qmat_info_reg.qmat_auto = 1;
		reg_list->_90_qmat_info_reg.qmat_enable = 1;
		reg_list->_90_qmat_info_reg.qmat_idx = comp;
		write_reg_u32(s->regs_base + JPG_QMAT_INFO_REG, *pval);

		pval = (u32 *)&reg_list->_94_qmat_addr_reg;
		reg_list->_94_qmat_addr_reg.qmat_idx = comp;
		reg_list->_94_qmat_addr_reg.qmat_addr = 0;
		write_reg_u32(s->regs_base + JPG_QMAT_ADDR_REG, *pval);

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
static void ve_config_huffman_table(struct mjpeg_dec_ctx *s)
{
	int i;
	jpg_reg_list *reg_list = (jpg_reg_list *)s->reg_list;

	u32 *pval;
	u32 val;

	logd("config huffman table");

	// 1. config start_code
	reg_list->_80_huff_info_reg.huff_auto = 1;
	reg_list->_80_huff_info_reg.enable = 1;
	reg_list->_80_huff_info_reg.huff_idx = 0;
	pval = (u32 *)&reg_list->_80_huff_info_reg;
	write_reg_u32(s->regs_base + JPG_HUFF_INFO_REG, *pval);

	reg_list->_84_huff_addr_reg.huff_idx = 0;
	reg_list->_84_huff_addr_reg.huff_addr = 0;
	pval = (u32 *)&reg_list->_84_huff_addr_reg;
	write_reg_u32(s->regs_base + JPG_HUFF_ADDR_REG, *pval);

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


	// 2. config max code
	logd("max_code");
	pval = (u32 *)&reg_list->_80_huff_info_reg;
	reg_list->_80_huff_info_reg.huff_auto = 1;
	reg_list->_80_huff_info_reg.enable = 1;
	reg_list->_80_huff_info_reg.huff_idx = 1;
	write_reg_u32(s->regs_base + JPG_HUFF_INFO_REG, *pval);

	pval = (u32 *)&reg_list->_84_huff_addr_reg;
	reg_list->_84_huff_addr_reg.huff_idx = 1;
	reg_list->_84_huff_addr_reg.huff_addr = 0;
	write_reg_u32(s->regs_base + JPG_HUFF_ADDR_REG, *pval);

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

	// 3. config huffman offset
	pval = (u32 *)&reg_list->_80_huff_info_reg;
	reg_list->_80_huff_info_reg.huff_auto = 1;
	reg_list->_80_huff_info_reg.enable = 1;
	reg_list->_80_huff_info_reg.huff_idx = 2;
	write_reg_u32(s->regs_base + JPG_HUFF_INFO_REG, *pval);

	pval = (u32 *)&reg_list->_84_huff_addr_reg;
	reg_list->_84_huff_addr_reg.huff_idx = 2;
	reg_list->_84_huff_addr_reg.huff_addr = 0;

	write_reg_u32(s->regs_base + JPG_HUFF_ADDR_REG, *pval);

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

	// 4.1 config huffman val (dc luma)
	logd("huff_val dc_luma");
	pval = (u32 *)&reg_list->_80_huff_info_reg;
	reg_list->_80_huff_info_reg.huff_auto = 1;
	reg_list->_80_huff_info_reg.enable = 1;
	reg_list->_80_huff_info_reg.huff_idx = 3;
	write_reg_u32(s->regs_base + JPG_HUFF_INFO_REG, *pval);

	pval = (u32 *)&reg_list->_84_huff_addr_reg;
	reg_list->_84_huff_addr_reg.huff_idx = 3;
	reg_list->_84_huff_addr_reg.huff_addr = 0;

	write_reg_u32(s->regs_base + JPG_HUFF_ADDR_REG, *pval);

	if (s->huffman_table[0][s->dc_index[0]].total_code > 12) {
		loge("dc luma code num : %d", s->huffman_table[0][s->dc_index[0]].total_code);
		//abort();
	}
	for (i = 0; i < s->huffman_table[0][s->dc_index[0]].total_code; i++) {
		val = s->huffman_table[0][s->dc_index[0]].symbol[i];
		write_reg_u32(s->regs_base + JPG_HUFF_VAL_REG, val);
	}

	// 4.2 config huffman val (dc chroma)
	logd("huff_val dc_chroma");
	pval = (u32 *)&reg_list->_80_huff_info_reg;
	reg_list->_80_huff_info_reg.huff_auto = 1;
	reg_list->_80_huff_info_reg.enable = 1;
	reg_list->_80_huff_info_reg.huff_idx = 3;

	write_reg_u32(s->regs_base + JPG_HUFF_INFO_REG, *pval);
	logv("write JPG_HUFF_INFO_REG %02X %02X", JPG_HUFF_INFO_REG, *pval);

	pval = (u32 *)&reg_list->_84_huff_addr_reg;
	reg_list->_84_huff_addr_reg.huff_idx = 3;
	reg_list->_84_huff_addr_reg.huff_addr = 12;
	write_reg_u32(s->regs_base + JPG_HUFF_ADDR_REG, *pval);
	logv("write JPG_HUFF_ADDR_REG %02X %02X", JPG_HUFF_ADDR_REG, *pval);

	if (s->huffman_table[0][s->dc_index[1]].total_code > 12) {
		loge("dc chroma code num : %d", s->huffman_table[0][s->dc_index[1]].total_code);
		//abort();
	}
	for (i = 0; i < s->huffman_table[0][s->dc_index[1]].total_code; i++) {
		val = s->huffman_table[0][s->dc_index[1]].symbol[i];
		write_reg_u32(s->regs_base + JPG_HUFF_VAL_REG, val);
		logv("write JPG_HUFF_VAL_REG %02X %02X", JPG_HUFF_VAL_REG, val);
	}

	// 4.3 config huffman val (ac luma)
	pval = (u32 *)&reg_list->_80_huff_info_reg;
	reg_list->_80_huff_info_reg.huff_auto = 1;
	reg_list->_80_huff_info_reg.enable = 1;
	reg_list->_80_huff_info_reg.huff_idx = 3;
	write_reg_u32(s->regs_base + JPG_HUFF_INFO_REG, *pval);

	pval = (u32 *)&reg_list->_84_huff_addr_reg;
	reg_list->_84_huff_addr_reg.huff_idx = 3;
	reg_list->_84_huff_addr_reg.huff_addr = 24;

	write_reg_u32(s->regs_base + JPG_HUFF_ADDR_REG, *pval);

	if (s->huffman_table[1][s->ac_index[0]].total_code > 162) {
		loge("ac luma code num : %d", s->huffman_table[1][s->ac_index[0]].total_code);
		//abort();
	}
	for (i = 0; i < s->huffman_table[1][s->ac_index[0]].total_code; i++) {
		val = s->huffman_table[1][s->ac_index[0]].symbol[i];
		write_reg_u32(s->regs_base + JPG_HUFF_VAL_REG, val);
	}

	// 4.4 config huffman val (ac chroma)
	pval = (u32 *)&reg_list->_80_huff_info_reg;
	reg_list->_80_huff_info_reg.huff_auto = 1;
	reg_list->_80_huff_info_reg.enable = 1;
	reg_list->_80_huff_info_reg.huff_idx = 3;
	write_reg_u32(s->regs_base + JPG_HUFF_INFO_REG, *pval);

	pval = (u32 *)&reg_list->_84_huff_addr_reg;
	reg_list->_84_huff_addr_reg.huff_idx = 3;
	reg_list->_84_huff_addr_reg.huff_addr = 186;
	write_reg_u32(s->regs_base + JPG_HUFF_ADDR_REG, *pval);

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

static void config_jpeg_picture_info_register(struct mjpeg_dec_ctx *s)
{
	u32 *pval;

	logd("config picture info register");

	struct frame_format_reg frame_format;
	memset(&frame_format, 0, sizeof(int));
	struct frame_size_reg frame_size;
	memset(&frame_size, 0, sizeof(int));
	frame_size.pic_xsize = s->rm_h_real_size[0];
	frame_size.pic_ysize = s->rm_v_real_size[0];

	frame_format.cbcr_interleaved = s->uv_interleave;
	frame_format.stride = s->curr_frame->mpp_frame.buf.stride[0];
	if (s->pix_fmt == MPP_FMT_YUV420P || s->pix_fmt == MPP_FMT_NV12
	|| s->pix_fmt == MPP_FMT_NV21) {
		frame_format.color_mode = 0;
	} else if(s->pix_fmt == MPP_FMT_YUV444P) {
		frame_format.color_mode = 3;
	} else if (s->pix_fmt == MPP_FMT_YUV400) {
		frame_format.color_mode = 4;
	} else if (s->pix_fmt == MPP_FMT_YUV422P || s->pix_fmt == MPP_FMT_NV16
	|| s->pix_fmt == MPP_FMT_NV61) {
		// YUV422 rotate 90/270 is YUV224
		frame_format.color_mode = 1;
		if(s->rotate == MPP_ROTATION_270 || s->rotate == MPP_ROTATION_90)
			frame_format.color_mode = 2; // yuv224, display not support
	} else {
		loge("not supprt this format(%d)", s->pix_fmt);
		//abort();
	}

	pval = (u32 *)&frame_format;
	write_reg_u32(s->regs_base + FRAME_FORMAT_REG(0), *pval);

	pval = (u32 *)&frame_size;
	write_reg_u32(s->regs_base + FRAME_SIZE_REG(0), *pval);

	write_reg_u32(s->regs_base + FRAME_YADDR_REG(0), s->curr_frame->phy_addr[0]);
	write_reg_u32(s->regs_base + FRAME_CBADDR_REG(0), s->curr_frame->phy_addr[1]);
	write_reg_u32(s->regs_base + FRAME_CRADDR_REG(0), s->curr_frame->phy_addr[2]);
}

static void config_header_info(struct mjpeg_dec_ctx *s)
{
	jpg_reg_list *reg_list = (jpg_reg_list *)s->reg_list;
	u32 *pval;

	write_reg_u32(s->regs_base + JPG_START_POS_REG, 0);

	reg_list->_10_ctrl_reg.encode = 0;
	reg_list->_10_ctrl_reg.dir = 0;
	reg_list->_10_ctrl_reg.use_huff_en = s->have_dht;
	reg_list->_10_ctrl_reg.dc_huff_idx = (s->dc_index[0] <<2) | (s->dc_index[1] <<1) | s->dc_index[2];
	reg_list->_10_ctrl_reg.ac_huff_idx = (s->ac_index[0] <<2) | (s->ac_index[1] <<1) | s->ac_index[2];

	pval = (u32 *)&reg_list->_10_ctrl_reg;
	write_reg_u32(s->regs_base + JPG_CTRL_REG, *pval);

	logd("picture size");

	int mcu_w = s->h_count[0] * 8;
	int mcu_h = s->v_count[0] * 8;
	reg_list->_14_size_reg.jpeg_hsize = (s->width + mcu_w - 1) / mcu_w * mcu_w;
	reg_list->_14_size_reg.jpeg_vsize = (s->height + mcu_h - 1) / mcu_h * mcu_h;
	pval = (u32 *)&reg_list->_14_size_reg;
	write_reg_u32(s->regs_base + JPG_SIZE_REG, *pval);

	int tatal_blks = s->h_count[0] * s->v_count[0] +
		s->h_count[1] * s->v_count[1] + s->h_count[2] * s->v_count[2];
	reg_list->_18_mcu_reg.y_h_size = s->h_count[0];
	reg_list->_18_mcu_reg.y_v_size = s->v_count[0];
	reg_list->_18_mcu_reg.cb_h_size = s->h_count[1];
	reg_list->_18_mcu_reg.cb_v_size = s->v_count[1];
	reg_list->_18_mcu_reg.cr_h_size = s->h_count[1];
	reg_list->_18_mcu_reg.cr_v_size = s->v_count[1];
	reg_list->_18_mcu_reg.comp_num = s->nb_components;
	reg_list->_18_mcu_reg.blk_num = tatal_blks;
	pval = (u32 *)&reg_list->_18_mcu_reg;
	write_reg_u32(s->regs_base + JPG_MCU_INFO_REG, *pval);

	int num = 12 / tatal_blks;
	write_reg_u32(s->regs_base + JPG_HANDLE_NUM_REG,  num > 4? 4: num);

	write_reg_u32(s->regs_base + JPG_UV_REG, s->uv_interleave);
	write_reg_u32(s->regs_base + JPG_FRAME_IDX_REG, 0);
	write_reg_u32(s->regs_base + JPG_RST_INTERVAL_REG, s->restart_interval);
	write_reg_u32(s->regs_base + JPG_INTRRUPT_EN_REG, 0);
}

int ve_decode_jpeg(struct mjpeg_dec_ctx *s, int byte_offset)
{
	unsigned int status;

	ve_get_client();

	// 1. config ve top
	ve_config_ve_top_reg(s);

	// 2. config header info
	config_header_info(s);
	config_jpeg_picture_info_register(s);

	// 3. config quant matrix
	ve_config_quant_matrix(s);

	// 4. config huffman table
	if(s->have_dht) {
		ve_config_huffman_table(s);
	}

	// 5. config post process
	ve_config_pp_register(s);

	// 6. config bitstream
#ifdef COPY_DATA
	ve_config_bitstream_sos(s);
#else
	ve_config_bitstream_register(s, byte_offset, 1);
#endif

	// 7. decode start
	write_reg_u32(s->regs_base + JPG_START_REG, 1);

	if(ve_wait(&status) < 0) {
		loge("ve wait irq timeout");
		logi("read JPG_STATUS_REG  %x", read_reg_u32(s->regs_base + JPG_STATUS_REG));

		ve_reset();
		ve_put_client();
		return -1;
	}

	if(status > 1) {
		loge("status error %x",status);
		ve_reset();
		ve_put_client();
		return -1;
	}

	logi("ve status %x", status);

	logi("read JPG_CYCLES_REG  %x", read_reg_u32(s->regs_base + JPG_CYCLES_REG));

	// disable jpeg module
	write_reg_u32(s->regs_base + VE_JPG_EN_REG, 0);
	ve_put_client();
	return 0;
}
