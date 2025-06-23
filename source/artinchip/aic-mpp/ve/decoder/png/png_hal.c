/*
 * Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <qi.xu@artinchip.com>
 *  Desc: png register configure
 *
 */

#define LOG_TAG "png_hal"

#include <unistd.h>

#include "png_hal.h"
#include "ve.h"
#include "ve_top_register.h"
#include "mpp_log.h"

void config_ve_top_reg(struct png_dec_ctx *s)
{
	int i;
	u32 val;

	write_reg_u32(s->regs_base + VE_CLK_REG, 1);

	// [9:8]: pic module reset; [5:4]: png module reset
	write_reg_u32(s->regs_base + VE_RST_REG, RST_PNG_PIC_MODULE);

	for (i = 0; i < 10; i++) {
		val = read_reg_u32(s->regs_base + VE_RST_REG);
		if ((val>>16) == 0)
			break;
	}

	write_reg_u32(s->regs_base + VE_INIT_REG, 1);
	write_reg_u32(s->regs_base + VE_IRQ_REG, 1);
	write_reg_u32(s->regs_base + VE_PNG_EN_REG, 1);
}

void png_reset(struct png_dec_ctx *s)
{
	write_reg_u32(s->regs_base + INFLATE_RESET_REG, 1);
}

static int set_bitstream_and_wait(struct png_dec_ctx *s, unsigned char *data, int length)
{
	int offset = s->vbv_offset;
	int is_first = 1;
	int is_last = 1;
	unsigned int status = 0;

	struct png_register_list *reg_list = (struct png_register_list *)s->reg_list;
	u32 *pval;
	u32 val;

	logd("config bit stream");

	val = s->idat_mpp_buf->phy_addr;
	write_reg_u32(s->regs_base + INPUT_BS_START_ADDR_REG, val);

	val = s->idat_mpp_buf->phy_addr + s->idat_mpp_buf->size - 1;
	write_reg_u32(s->regs_base + INPUT_BS_END_ADDR_REG, val);

	write_reg_u32(s->regs_base + INPUT_BS_OFFSET_REG, offset * 8);
	write_reg_u32(s->regs_base + INPUT_BS_LENGTH_REG, length * 8);

	pval = (u32 *)&reg_list->_48_inflate_valid;
	reg_list->_48_inflate_valid.first = is_first;
	reg_list->_48_inflate_valid.last = is_last;
	reg_list->_48_inflate_valid.valid = 1;
	write_reg_u32(s->regs_base + INPUT_BS_DATA_VALID_REG, *pval);

	// wait IRQ
	if(ve_wait(&status) < 0) {
		usleep(10000);
		ve_reset();
		loge("png timeout");
		return -1;
	}

	if(status & PNG_ERROR) {
		loge("png decode error, status: %08x", status);
		usleep(10000);
		ve_reset();
		return -1;
	} else if(status & PNG_FINISH) {
		return 0;
	} else if(status & PNG_BITREQ) {
		loge("png bit request, not support now");
		// TODO
		return -1;
	}

	return 0;
}

int png_hardware_decode(struct png_dec_ctx *s, unsigned char *buf, int length)
{
	struct png_register_list *reg_list = (struct png_register_list *)s->reg_list;
	u32 *pval;
	u32 val;

	memset(reg_list, 0, sizeof(struct png_register_list));
	s->hw_size = s->stride * s->height;

	ve_get_client();

	//* 1. reset ve
	config_ve_top_reg(s);
	png_reset(s);

	//* 2.set png info
	pval = (u32 *)&reg_list->_10_png_ctrl;
	reg_list->_10_png_ctrl.bit_depth = s->bit_depth;
	reg_list->_10_png_ctrl.color_type = s->color_type;
	reg_list->_10_png_ctrl.dec_type = 1;
	write_reg_u32(s->regs_base + PNG_CTRL_REG, *pval);

	//* 3. set picture size
	pval = (u32 *)&reg_list->_14_png_size;
	reg_list->_14_png_size.width = s->width;
	reg_list->_14_png_size.height = s->height;
	write_reg_u32(s->regs_base + PNG_SIZE_REG, *pval);

	write_reg_u32(s->regs_base + PNG_STRIDE_REG, s->curr_frame->mpp_frame.buf.stride[0]);

	int format = 0;
	if(s->pix_fmt == MPP_FMT_RGBA_8888)
		format = RGBA8888;
	else if(s->pix_fmt == MPP_FMT_BGRA_8888)
		format = BGRA8888;
	else if(s->pix_fmt == MPP_FMT_ABGR_8888)
		format = ABGR8888;
	else if(s->pix_fmt == MPP_FMT_ARGB_8888)
		format = ARGB8888;
	else if(s->pix_fmt == MPP_FMT_BGR_888)
		format = BGR888;
	else if(s->pix_fmt == MPP_FMT_RGB_888)
		format = RGB888;
	else if(s->pix_fmt == MPP_FMT_BGR_565)
		format = BGR565;
	else if(s->pix_fmt == MPP_FMT_RGB_565)
		format = RGB565;

	write_reg_u32(s->regs_base + PNG_FORMAT_REG, format);

	//* 4. set output buffer
	val = s->curr_frame->phy_addr[0];
	write_reg_u32(s->regs_base + OUTPUT_BUFFER_ADDR_REG, val);

	val = s->height * s->curr_frame->mpp_frame.buf.stride[0];
	write_reg_u32(s->regs_base + OUTPUT_BUFFER_LENGTH_REG, val);

	//* 5. set LZ77 buffer 32K
	val = s->lz77_mpp_buf->phy_addr;
	write_reg_u32(s->regs_base + INFLATE_WINDOW_BUFFER_ADDR_REG, val);

	//* 6. set memory register for palette
	if (s->color_type == PNG_COLOR_TYPE_PALETTE) {
		//* PNG filter line buffer address
		val = s->filter_mpp_buf->phy_addr;
		write_reg_u32(s->regs_base + PNG_FILTER_LINE_BUF_ADDR_REG, val);

		unsigned char* palette_buf = (unsigned char*)&s->palette;
		memcpy(s->palette_mpp_buf->vir_addr, palette_buf, 256 * 4);
		ve_buffer_sync(s->palette_mpp_buf, CACHE_CLEAN);

		val = s->palette_mpp_buf->phy_addr;
		write_reg_u32(s->regs_base + PNG_PNG_PALETTE_ADDR_REG, val);
	}

	//* 7. decode start
	logd("config start");
	write_reg_u32(s->regs_base + INFLATE_INTERRUPT_REG, 15);
	write_reg_u32(s->regs_base + INFLATE_STATUS_REG, 15);
	write_reg_u32(s->regs_base + INFLATE_START_REG, 1);

	//* 9.set bitstream
	if (set_bitstream_and_wait(s, buf, length)) {
		ve_put_client();
		return -1;
	}

	val = read_reg_u32(s->regs_base + PNG_COUNT_REG);
	logi("png clock: %d", val);

	// disable png module
	write_reg_u32(s->regs_base + VE_PNG_EN_REG, 0);
	ve_put_client();

	return 0;
}

int gzip_hardware_decode(struct png_dec_ctx *s, unsigned char *buf, int length)
{
	struct png_register_list *reg_list = (struct png_register_list *)s->reg_list;
	u32 val;

	int output_data_len = INFLATE_MAX_OUTPUT;

	memset(reg_list, 0, sizeof(struct png_register_list));
	ve_get_client();

	//* 1. reset ve
	config_ve_top_reg(s);
	png_reset(s);  // TODO

	//* 2. set decode type to inflate
	write_reg_u32(s->regs_base + PNG_CTRL_REG, 0);

	//* 3. set output buffer
	val = s->curr_frame->phy_addr[0];
	write_reg_u32(s->regs_base + OUTPUT_BUFFER_ADDR_REG, val);

	val = INFLATE_MAX_OUTPUT;
	write_reg_u32(s->regs_base + OUTPUT_BUFFER_LENGTH_REG, val);

	//* 4. set LZ77 buffer 32K
	val = s->lz77_mpp_buf->phy_addr;
	write_reg_u32(s->regs_base + INFLATE_WINDOW_BUFFER_ADDR_REG, val);

	//* 5. decode start
	logd("config start");
	write_reg_u32(s->regs_base + INFLATE_INTERRUPT_REG, 15);
	write_reg_u32(s->regs_base + INFLATE_STATUS_REG, 15);
	write_reg_u32(s->regs_base + INFLATE_START_REG, 1);

	//* 6. set bitstream and wait finish irq
	if (set_bitstream_and_wait(s, buf, length)) {
		ve_put_client();
		return -1;
	}

	unsigned int buf_size = 0;

	val = read_reg_u32(s->regs_base + OUTPUT_COUNT_REG);
	logv("read OUTPUT_COUNT_REG %02X %02X", OUTPUT_COUNT_REG, val);

	buf_size = val;

	if (buf_size <= (unsigned int)output_data_len) {
		logd("hw out size:%d", buf_size);
		s->hw_size = buf_size;
	} else {
		loge("output size:%u is too big", buf_size);
		return -1;
	}

	ve_put_client();

	return 0;
}
