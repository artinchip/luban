/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <qi.xu@artinchip.com>
 *  Desc: jpeg encoder hal
 */

#include <stdlib.h>
#include "ve_top_register.h"
#include "jpeg_register.h"
#include "mpp_log.h"
#include "ve_buffer.h"
#include "ve.h"
#include "jpeg_enc_ctx.h"

static void config_ve_top_reg(struct jpeg_ctx *s)
{
	logd("config mjpeg reg");
	write_reg_u32(s->regs_base + VE_CLK_REG, 1);
	write_reg_u32(s->regs_base + VE_RST_REG, RST_JPG_PIC_MODULE);

	while (1) {
		uint32_t val = read_reg_u32(s->regs_base + VE_RST_REG);
		if ((val >> 16) == 0)
			break;
	}

	write_reg_u32(s->regs_base + VE_INIT_REG, 1);
	write_reg_u32(s->regs_base + VE_IRQ_REG, 1);
	write_reg_u32(s->regs_base + VE_JPG_EN_REG, 1);
}

static void config_header_info(struct jpeg_ctx *s)
{
	uint32_t val = 0;
	write_reg_u32(s->regs_base + JPG_START_POS_REG, 0);

	write_reg_u32(s->regs_base + JPG_CTRL_REG, (3 << 12) | (3 << 8) | (3 << 3));

	int width_align = (s->width + 15) & (~15);
	val = s->height | (width_align << 16);
	write_reg_u32(s->regs_base + JPG_SIZE_REG, val);

	int total_blks = s->h_count[0] * s->v_count[0] +
		s->h_count[1] * s->v_count[1] + s->h_count[2] * s->v_count[2];
	val = s->v_count[2] | (s->h_count[2] << 2) |
		(s->v_count[1] << 4) | (s->h_count[1] << 6) |
		(s->v_count[0] << 8) | (s->h_count[0] << 10) |
		(s->comp_num << 12) | (total_blks << 16);
	write_reg_u32(s->regs_base + JPG_MCU_INFO_REG, val);

	val = 12 / total_blks;
	write_reg_u32(s->regs_base + JPG_HANDLE_NUM_REG, val);

	write_reg_u32(s->regs_base + JPG_UV_REG, s->uv_interleave);
	write_reg_u32(s->regs_base + JPG_FRAME_IDX_REG, 0);
	write_reg_u32(s->regs_base + JPG_RST_INTERVAL_REG, 0);
	write_reg_u32(s->regs_base + JPG_INTRRUPT_EN_REG, 0);
}

static void config_picture_info_register(struct jpeg_ctx *s)
{
	uint32_t val = 0;
	uint32_t color_mode = 0;

	if (s->cur_frame->buf.format == MPP_FMT_YUV420P || s->cur_frame->buf.format == MPP_FMT_NV12) {
		color_mode = 0;
	} else if (s->cur_frame->buf.format == MPP_FMT_YUV444P) {
		color_mode = 3;
	} else if (s->cur_frame->buf.format == MPP_FMT_YUV400) {
		color_mode = 4;
	} else if (s->cur_frame->buf.format == MPP_FMT_YUV422P) {
		color_mode = 1;
	} else {
		loge("not supprt this format(%d)", s->cur_frame->buf.format);
	}

	val = s->y_stride | (s->uv_interleave << 16) | (color_mode << 17);
	write_reg_u32(s->regs_base + PIC_INFO_START_REG, val);

	val = s->height | (s->y_stride << 16);
	write_reg_u32(s->regs_base + PIC_INFO_START_REG + 4, val);
	write_reg_u32(s->regs_base + PIC_INFO_START_REG + 8, s->phy_addr[0]);
	write_reg_u32(s->regs_base + PIC_INFO_START_REG + 12, s->phy_addr[1]);
	write_reg_u32(s->regs_base + PIC_INFO_START_REG + 16, s->phy_addr[2]);
}

/*
quant matrix store in zigzag order
*/
static void ve_config_quant_matrix(struct jpeg_ctx *s)
{
	int i, j;

	uint32_t val;
	unsigned int* quant_tab[3] = { s->luma_quant_table, s->chroma_quant_table, s->chroma_quant_table };
	for (int comp = 0; comp < 3; comp++) {
		write_reg_u32(s->regs_base + JPG_QMAT_INFO_REG, (comp << 6) | 3);
		write_reg_u32(s->regs_base + JPG_QMAT_ADDR_REG, comp << 6);
		for (i = 0; i < 64; i++) {
			j = zigzag_direct[i];
			// qmatrix should be  (1<<19)/q
			val = (1 << QUANT_FIXED_POINT_BITS) / quant_tab[comp][j];
			write_reg_u32(s->regs_base + JPG_QMAT_VAL_REG, val);
		}
	}

	write_reg_u32(s->regs_base + JPG_QMAT_INFO_REG, 0);
}

/*
huffman table include 4 tables: DC_Luma\DC_Chroma\AC_LUMA\AC_Chroma
	internal SRAM in VE is 544x20bit
	20bit data: [19:16]: code_length-1; [15:0]: code word
address of the 4 tables:
	Luma_AC: 0-255
	Chroma_AC: 256-511
	Luma_DC: 512-528
	Chroma_DC: 528-544
*/
static void ve_config_huffman_table(struct jpeg_ctx *s)
{
	int i;
	uint32_t val;

	write_reg_u32(s->regs_base + JPG_HUFF_INFO_REG, 3);
	write_reg_u32(s->regs_base + JPG_HUFF_ADDR_REG, 0);

	// luma ac
	for (i = 0; i < 256; i++) {
		val = ((s->huff_size_ac_luminance[i] - 1) << 16) | s->huff_code_ac_luminance[i];
		write_reg_u32(s->regs_base + JPG_HUFF_VAL_REG, val);
	}

	// chroma ac
	for (i = 0; i < 256; i++) {
		val = ((s->huff_size_ac_chrominance[i] - 1) << 16) | s->huff_code_ac_chrominance[i];
		write_reg_u32(s->regs_base + JPG_HUFF_VAL_REG, val);
	}

	// luma dc
	for (i = 0; i < 12; i++) {
		val = ((s->huff_size_dc_luminance[i] - 1) << 16) | s->huff_code_dc_luminance[i];
		write_reg_u32(s->regs_base + JPG_HUFF_VAL_REG, val);
	}

	// chroma dc
	write_reg_u32(s->regs_base + JPG_HUFF_INFO_REG, 3);
	write_reg_u32(s->regs_base + JPG_HUFF_ADDR_REG, 528);
	for (i = 0; i < 12; i++) {
		val = ((s->huff_size_dc_chrominance[i] - 1) << 16) | s->huff_code_dc_chrominance[i];
		write_reg_u32(s->regs_base + JPG_HUFF_VAL_REG, val);
	}

	// clear
	write_reg_u32(s->regs_base + JPG_HUFF_INFO_REG, 0);
}

static void ve_config_stream_register(struct jpeg_ctx *s)
{
	int head_offset = s->header_offset;
	int stream_num = s->stream_num;
	// end addr of output stream
	unsigned int align_256_addr = s->bitstream_phy_addr + stream_num * 256;
	write_reg_u32(s->regs_base + JPG_STREAM_END_ADDR_REG, align_256_addr);
	// base addr of output stream
	write_reg_u32(s->regs_base + JPG_BAS_ADDR_REG, s->bitstream_phy_addr + head_offset);
	// start addr of output stream
	write_reg_u32(s->regs_base + JPG_STREAM_START_ADDR_REG, s->bitstream_phy_addr + head_offset);

	write_reg_u32(s->regs_base + JPG_STREAM_WRITE_PTR_REG, s->bitstream_phy_addr + head_offset);
	write_reg_u32(s->regs_base + JPG_STREAM_READ_PTR_REG, s->bitstream_phy_addr + head_offset);

	// current stream pos
	write_reg_u32(s->regs_base + JPG_CUR_POS_REG, 0);
	write_reg_u32(s->regs_base + JPG_DATA_CNT_REG, 64);

	write_reg_u32(s->regs_base + JPG_MEM_EA_REG, 0x7f);
	write_reg_u32(s->regs_base + JPG_MEM_IA_REG, 0x40);
	write_reg_u32(s->regs_base + JPG_MEM_HA_REG, 0x40);

	write_reg_u32(s->regs_base + JPG_BITREQ_EN_REG, 1);
}

int jpeg_hw_encode(struct jpeg_ctx *s)
{
	int ret = 0;
	uint32_t status;

	ve_get_client();

	// 1. config ve top
	config_ve_top_reg(s);

	// 2. config jpeg header
	config_header_info(s);

	// 3. config picture info
	config_picture_info_register(s);

	// 4. config quant/huffman table
	ve_config_quant_matrix(s);
	ve_config_huffman_table(s);

	// 5. config bitstream
	ve_config_stream_register(s);

	write_reg_u32(s->regs_base + JPG_STATUS_REG, 0xf);
	write_reg_u32(s->regs_base + JPG_START_REG, 1);

	if (ve_wait(&status) < 0) {
		loge("ve wait irq timeout");
		logi("read JPG_STATUS_REG  %x", read_reg_u32(s->regs_base + JPG_STATUS_REG));

		ve_reset();
		ret = -1;
		goto out;
	}

	if(status > 1) {
		loge("status error");
		ve_reset();
		ret = -1;
		goto out;
	}

	uint32_t cycles = 0;
	cycles = read_reg_u32(s->regs_base + JPG_CYCLES_REG);

	uint32_t end_addr = 0;
	end_addr = read_reg_u32(s->regs_base + JPG_STREAM_WRITE_PTR_REG);
	s->encode_data_len = end_addr - s->bitstream_phy_addr;

	logi("cycles: %d, data len: %d", cycles, s->encode_data_len);

	// disable jpeg module
	write_reg_u32(s->regs_base + VE_JPG_EN_REG, 0);

out:
	ve_put_client();
	return ret;
}
