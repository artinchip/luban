// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2024 ArtInChip Technology Co., Ltd.
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>

#include "video/artinchip_fb.h"
#include "reg_util.h"
#include "de_hw.h"

#define CSC_COEFFS_NUM 12

static int yuv2rgb_bt601_limit[3][4] = {
	{1192, 0, 1634, -3269},
	{1192, -401, -833, 2467},
	{1192, 2066, 0, -4131}
};

static int yuv2rgb_bt709_limit[3][4] = {
	{1192, 0, 1836, -3970},
	{1192, -218, -546, 1230},
	{1192, 2163, 0, -4624}
};

static int yuv2rgb_bt601_full[3][4] = {
	{1024, 0, 1436, -2871},
	{1024, -352, -731, 2167},
	{1024, 1815, 0, -3629}
};

static int yuv2rgb_bt709_full[3][4] = {
	{1024, 0, 1613, -3225},
	{1024, -192, -479, 1342},
	{1024, 1900, 0, -3800}
};

static int rgb2yuv_bt709_full[4][4] = {
	{218, 732, 74, 0},
	{-116, -394, 512, 128 * 1024},
	{512, -464, -46, 128 * 1024},
	{0, 0, 0, 1024},
};

static int sin_table[60] = {
	-1985,    -1922,    -1859,    -1795,    -1731,
	-1665,    -1600,    -1534,    -1467,    -1400,
	-1333,    -1265,    -1197,    -1129,    -1060,
	-990,     -921,     -851,     -781,     -711,
	-640,     -570,     -499,     -428,     -356,
	-285,     -214,     -142,      -71,        0,
	71,      142,      214,      285,      356,
	428,      499,      570,      640,      711,
	781,      851,      921,      990,     1060,
	1129,     1197,     1265,     1333,     1400,
	1467,     1534,     1600,     1665,     1731,
	1795,     1859,     1922,     1985,     2047,
};

static int cos_table[60] = {
	3582,     3616,     3649,     3681,     3712,
	3741,     3770,     3797,     3823,     3848,
	3872,     3895,     3917,     3937,     3956,
	3974,     3991,     4006,     4020,     4033,
	4045,     4056,     4065,     4073,     4080,
	4086,     4090,     4093,     4095,     4096,
	4095,     4093,     4090,     4086,     4080,
	4073,     4065,     4056,     4045,     4033,
	4020,     4006,     3991,     3974,     3956,
	3937,     3917,     3895,     3872,     3848,
	3823,     3797,     3770,     3741,     3712,
	3681,     3649,     3616,     3582,     3547,
};

static const unsigned int scaling_coeffs[4][32] = {
	[0] = {
		0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x0f001000, 0x0d000e00, 0x0b000c00, 0x09000a00,
		0x07000800, 0x05000600, 0x03000400, 0x01000200,
		0x01000000, 0x03000200, 0x05000400, 0x07000600,
		0x09000800, 0x0b000a00, 0x0d000c00, 0x0f000e00,
		0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000,
	},
	[1] = {
		0x03a10405, 0x02f80352, 0x023f0299, 0x019701e9,
		0x0103014a, 0x008400c1, 0x001b004d, 0x00000000,
		0x07ed07f6, 0x07bd07d9, 0x07660795, 0x06f60731,
		0x067006b6, 0x05d70625, 0x05300585, 0x047204d5,
		0x04720405, 0x053004d5, 0x05d70585, 0x06700625,
		0x06f606b6, 0x07660731, 0x07bd0795, 0x07ed07d9,
		0x00000000, 0x001b0000, 0x0084004d, 0x010300c1,
		0x0197014a, 0x023f01e9, 0x02f80299, 0x03a10352,
	},
	[2] = {
		0x0420045e, 0x03a703e3, 0x0333036d, 0x02c502fb,
		0x025b028f, 0x01f80229, 0x019a01c8, 0x0141016d,
		0x061a062c, 0x05f10606, 0x05c205da, 0x058d05a8,
		0x05530571, 0x05130534, 0x04ce04f1, 0x048504aa,
		0x0485045e, 0x04ce04aa, 0x051304f1, 0x05530534,
		0x058d0571, 0x05c205a8, 0x05f105da, 0x061a0606,
		0x01410118, 0x019a016d, 0x01f801c8, 0x025b0229,
		0x02c5028f, 0x033302fb, 0x03a7036d, 0x042003e3,
	},
	[3] = {
		0x04240444, 0x03e40404, 0x03a603c5, 0x03690387,
		0x032d034b, 0x02f30310, 0x02ba02d6, 0x0281029d,
		0x05070511, 0x04f104fc, 0x04da04e6, 0x04c204ce,
		0x04a804b5, 0x048d049b, 0x0471047f, 0x04540463,
		0x04540445, 0x04710463, 0x048d047f, 0x04a8049b,
		0x04c204b5, 0x04da04ce, 0x04f104e6, 0x050704fc,
		0x02810266, 0x02ba029d, 0x02f302d6, 0x032d0310,
		0x0369034b, 0x03a60387, 0x03e403c5, 0x04240404,
	},
};

void de_config_prefetch_line_set(void __iomem *base_addr, u32 line)
{
	reg_set_bits(base_addr + TIMING_LINE_SET,
			TIMING_LINE_SET_PREFETCH_LINE_MASK,
			TIMING_LINE_SET_PREFETCH_LINE(line));
}

void de_config_tearing_effect(void __iomem *base_addr,
			u32 mode, u32 pulse_width)
{
	reg_set_bits(base_addr + TIMING_CTRL,
			 TIMING_TE_MODE_MASK,
			 TIMING_TE_MODE(mode));

	reg_set_bits(base_addr + TIMING_CTRL,
			 TIMING_TE_PULSE_WIDTH_MASK,
			 TIMING_TE_PULSE_WIDTH(pulse_width));
}

void de_soft_reset_ctrl(void __iomem *base_addr, u32 enable)
{
	if (enable)
		reg_set_bit(base_addr + WB_BASE, DE_SOFT_RESET_EN);
	else
		reg_clr_bit(base_addr + WB_BASE, DE_SOFT_RESET_EN);
}

void de_config_update_enable(void __iomem *base_addr, u32 enable)
{
	reg_write(base_addr + DE_CONFIG_UPDATE, enable);
}

void de_set_dither(void __iomem *base_addr, u32 r_depth,
		   u32 g_depth, u32 b_depth, u32 enable)
{
	if (enable) {
		reg_write(base_addr + OUTPUT_COLOR_DEPTH,
			  OUTPUT_COLOR_DEPTH_SET(r_depth, g_depth, b_depth));

		reg_set_bit(base_addr + DITHER_RAND_SEED, DE_RAND_DITHER_EN);
		reg_set_bit(base_addr + DE_CTRL, DE_CTRL_DITHER_EN);
	} else {
		reg_clr_bit(base_addr + DE_CTRL, DE_CTRL_DITHER_EN);
		reg_clr_bit(base_addr + DITHER_RAND_SEED, DE_RAND_DITHER_EN);
		reg_write(base_addr + OUTPUT_COLOR_DEPTH, 0x0);
	}
}

void de_set_qos(void __iomem *base_addr)
{
	const u32 pixel_bytes[4] = {1, 1, 1, 4};

	const u32 qos_low_th	= 32;
	const u32 qos_high_th	= 64;
	const u32 qos_red	= 10;
	const u32 qos_green	= 10;
	int i;

	for (i = 0; i < 4; i++)
		reg_write(base_addr + QOS_CONFIG(i),
				QOS_SET(qos_green, qos_high_th * pixel_bytes[i],
					qos_red, qos_low_th * pixel_bytes[i]));
}

void de_colorbar_ctrl(void __iomem *base_addr, u32 enable)
{
	if (enable)
		reg_set_bit(base_addr + DE_MODE_SELECT,
			     DE_MODE_SELECT_COLOR_BAR);
	else
		reg_clr_bit(base_addr + DE_MODE_SELECT,
			     DE_MODE_SELECT_COLOR_BAR);
}

u32 de_get_version_id(void __iomem *base_addr)
{
	return reg_read(base_addr + DE_VERSION_ID);
}

void de_set_video_layer_info(void __iomem *base_addr, u32 w, u32 h,
			     enum mpp_pixel_format format,
			     u32 stride0, u32 stride1,
			     u32 addr0, u32 addr1, u32 addr2,
			     u32 x_offset, u32 y_offset)
{
	reg_set_bits(base_addr + VIDEO_LAYER_CTRL,
			 VIDEO_LAYER_CTRL_INPUT_FORMAT_MASK,
			 VIDEO_LAYER_CTRL_INPUT_FORMAT(format));

	reg_write(base_addr + VIDEO_LAYER_INPUT_SIZE,
		  VIDEO_LAYER_INPUT_SIZE_SET(w, h));
	reg_write(base_addr + VIDEO_LAYER_STRIDE,
		  VIDEO_LAYER_STRIDE_SET(stride0, stride1));
	reg_write(base_addr + VIDEO_LAYER_ADDR0, addr0);
	reg_write(base_addr + VIDEO_LAYER_ADDR1, addr1);
	reg_write(base_addr + VIDEO_LAYER_ADDR2, addr2);
	reg_write(base_addr + VIDEO_LAYER_OFFSET,
		  VIDEO_LAYER_OFFSET_SET(x_offset, y_offset));
}

void de_set_video_layer_tile_offset(void __iomem *base_addr,
				    u32 p0_x_offset, u32 p0_y_offset,
				    u32 p1_x_offset, u32 p1_y_offset)
{
	reg_write(base_addr + VIDEO_LAYER_TILE_OFFSET0,
		  VIDEO_LAYER_TILE_OFFSET0_SET(p0_x_offset, p0_y_offset));

	reg_write(base_addr + VIDEO_LAYER_TILE_OFFSET1,
		  VIDEO_LAYER_TILE_OFFSET1_SET(p1_x_offset, p1_y_offset));
}

void de_video_layer_enable(void __iomem *base_addr, u32 enable)
{
	if (enable)
		reg_set_bit(base_addr + VIDEO_LAYER_CTRL, VIDEO_LAYER_CTRL_EN);
	else
		reg_clr_bit(base_addr + VIDEO_LAYER_CTRL, VIDEO_LAYER_CTRL_EN);
}

void de_set_csc0_coefs(void __iomem *base_addr, int color_space)
{
	const int *coefs;
	int i;

	switch (color_space) {
	case MPP_COLOR_SPACE_BT601:
		coefs = &yuv2rgb_bt601_limit[0][0];
		break;
	case MPP_COLOR_SPACE_BT709:
		coefs = &yuv2rgb_bt709_limit[0][0];
		break;
	case MPP_COLOR_SPACE_BT601_FULL_RANGE:
		coefs = &yuv2rgb_bt601_full[0][0];
		break;
	case MPP_COLOR_SPACE_BT709_FULL_RANGE:
		coefs = &yuv2rgb_bt709_full[0][0];
		break;
	default:
		coefs = &yuv2rgb_bt601_limit[0][0];
		break;
	}

	for (i = 0; i < CSC_COEFFS_NUM; i++) {
		if (i == 3 || i == 7 || i == 11)
			reg_write(base_addr + VIDEO_LAYER_CSC0_COEF(i),
				  CSC0_COEF_OFFSET_SET(coefs[i]));
		else
			reg_write(base_addr + VIDEO_LAYER_CSC0_COEF(i),
				  CSC0_COEF_SET(coefs[i]));
	}
}

static int get_hsbc_coefs(int bright, int contrast,
			  int sat, int hue, int coef[][4])
{
	int sinv = sin_table[hue + 29];
	int cosv = cos_table[hue + 29];

	sat = sat + 128;
	contrast = contrast + 128;

	coef[0][0] = contrast << 3;
	coef[0][1] = 0;
	coef[0][2] = 0;
	coef[0][3] = ((bright << 3) << 8) + ((1024 - (contrast << 3)) << 4);

	coef[1][0] = 0;
	coef[1][1] = (contrast * sat * cosv) >> 16;
	coef[1][2] = (contrast * sat * sinv) >> 16;
	coef[1][3] = (1024 - (coef[1][1] + coef[1][2])) << 7;

	coef[2][0] = 0;
	coef[2][1] = (-contrast * sat * sinv) >> 16;
	coef[2][2] = (contrast * sat * cosv) >> 16;
	coef[2][3] = (1024 - (coef[2][1] + coef[2][2])) << 7;

	coef[3][0] = 0;
	coef[3][1] = 0;
	coef[3][2] = 0;
	coef[3][3] = 1024;

	return 0;
}

static void matrix_4x4_multi(int a[][4], int b[][4], int out[][4])
{
	int i, j;
	int c;

	for (j = 0; j < 4; j++) {
		for (i = 0; i < 4; i++) {
			for (c = 0; c < 4; c++)
				out[j][i] += (a[j][c] * b[c][i]) >> 10;
		}
	}
}

static int get_csc_coefs_for_hsbc(int is_rgb, int color_space,
				  int bright, int contrast,
				  int saturation, int hue,
				  u32 *csc_coef)
{
	int(*y2r)[4];
	int(*r2y)[4];
	int(*cur_coef)[4];
	int i, j, c, k;
	int hsv_coef[4][4] = {0};
	int out_coef[4][4] = {0};

	bright = bright > 100 ? 100 : bright;
	contrast = contrast > 100 ? 100 : contrast;
	saturation = saturation > 100 ? 100 : saturation;
	hue = hue > 100 ? 100 : hue;

	bright = bright * 128 / 100 - 64;
	contrast = contrast * 180 / 100 - 90;
	saturation = saturation * 128 / 100 - 64;
	hue = hue * 58 / 100 - 29;

	if (color_space == 0)
		y2r = yuv2rgb_bt601_limit;
	else if (color_space == 1)
		y2r = yuv2rgb_bt709_limit;
	else if (color_space == 2)
		y2r = yuv2rgb_bt601_full;
	else if (color_space == 3)
		y2r = yuv2rgb_bt709_full;
	else
		y2r = yuv2rgb_bt601_limit;

	get_hsbc_coefs(bright, contrast, saturation, hue, hsv_coef);

	if (is_rgb) {
		r2y = rgb2yuv_bt709_full;
		matrix_4x4_multi(hsv_coef, r2y, out_coef);
		cur_coef = out_coef;
	} else {
		cur_coef = hsv_coef;
	}

	k = 0;
	for (j = 0; j < 3; j++) {
		for (i = 0; i < 4; i++) {
			unsigned int value;
			int accum;

			accum = 0;
			for (c = 0; c < 4; c++) {
				int cur_value =  y2r[j][c];

				if (c == 3)
					cur_value = cur_value << 6;

				accum += cur_value * cur_coef[c][i];
			}

			if (i == 3) {
				accum = accum >> 16;
				accum = accum > 8091 ? 8091 : accum;
				accum = accum < -8092 ? -8092 : accum;
				value = (unsigned int)accum;
				value = CSC0_COEF_OFFSET_SET(value);
			} else {
				accum = accum >> 10;
				accum = accum > 4095 ? 4095 : accum;
				accum = accum < -4096 ? -4096 : accum;
				value = (unsigned int)accum;
				value = CSC0_COEF_SET(value);
			}
			csc_coef[k++] = value;
		}
	}

	return 0;
}

int de_set_hsbc_with_csc_coefs(void __iomem *base_addr, int color_space,
			       int bright, int contrast,
			       int saturation, int hue)
{
	int i;
	u32 csc_coef[CSC_COEFFS_NUM];

	get_csc_coefs_for_hsbc(0, color_space, bright, contrast, saturation,
			       hue, csc_coef);

	for (i = 0; i < CSC_COEFFS_NUM; i++)
		reg_write(base_addr + VIDEO_LAYER_CSC0_COEF(i), csc_coef[i]);

	return 0;
}

int get_rgb_hsbc_csc_coefs(int bright, int contrast, int saturation, int hue,
			   u32 *csc_coef)
{
	get_csc_coefs_for_hsbc(1, 3, bright, contrast, saturation,
			       hue, csc_coef);

	return 0;
}

void de_set_ui_layer_size(void __iomem *base_addr, u32 w, u32 h,
			  u32 x_offset, u32 y_offset)
{
	reg_write(base_addr + UI_LAYER_SIZE, UI_LAYER_SIZE_SET(w, h));
	reg_write(base_addr + UI_LAYER_OFFSET,
		  UI_LAYER_OFFSET_SET(x_offset, y_offset));
}

void de_ui_alpha_blending_enable(void __iomem *base_addr, u32 g_alpha,
				 u32 alpha_mode, u32 enable)
{
	if (enable)
		reg_set_bits(base_addr + UI_LAYER_CTRL,
				 UI_LAYER_CTRL_G_ALPHA_MASK |
				 UI_LAYER_CTRL_ALPHA_MODE_MASK |
				 UI_LAYER_CTRL_ALPHA_EN,
				 UI_LAYER_CTRL_G_ALPHA(g_alpha) |
				 UI_LAYER_CTRL_ALPHA_MODE(alpha_mode) |
				 UI_LAYER_CTRL_BG_BLEND_EN |
				 UI_LAYER_CTRL_ALPHA_EN);
	else
		reg_clr_bit(base_addr + UI_LAYER_CTRL, UI_LAYER_CTRL_ALPHA_EN);
}

void de_set_ui_layer_format(void __iomem *base_addr,
			    enum mpp_pixel_format format)
{
	reg_set_bits(base_addr + UI_LAYER_CTRL,
			 UI_LAYER_CTRL_INPUT_FORMAT_MASK,
			 UI_LAYER_CTRL_INPUT_FORMAT(format));
}

void de_ui_layer_color_key_enable(void __iomem *base_addr, u32 color_key,
				  u32 ck_en)
{
	if (ck_en) {
		reg_write(base_addr + UI_LAYER_COLOER_KEY,
			  UI_LAYER_COLOER_KEY_SET(color_key));
		reg_set_bit(base_addr + UI_LAYER_CTRL,
			     UI_LAYER_CTRL_COLOR_KEY_EN);
	} else {
		reg_clr_bit(base_addr + UI_LAYER_CTRL,
			     UI_LAYER_CTRL_COLOR_KEY_EN);
	}
}

void de_ui_layer_set_rect(void __iomem *base_addr, u32 w, u32 h,
			  u32 x_offset, u32 y_offset,
			  u32 stride, u32 addr, u32 id)
{
	reg_write(base_addr + UI_RECT_INPUT_SIZE(id),
		  UI_RECT_INPUT_SIZE_SET(w, h));
	reg_write(base_addr + UI_RECT_OFFSET(id),
		  UI_RECT_OFFSET_SET(x_offset, y_offset));
	reg_write(base_addr + UI_RECT_STRIDE(id),
		  UI_RECT_STRIDE_SET(stride));
	reg_write(base_addr + UI_RECT_ADDR(id), addr);
}

void de_ui_layer_enable(void __iomem *base_addr, u32 enable)
{
	if (enable)
		reg_set_bit(base_addr + UI_LAYER_CTRL, UI_LAYER_CTRL_EN);
	else
		reg_clr_bit(base_addr + UI_LAYER_CTRL, UI_LAYER_CTRL_EN);
}

void de_ui_layer_rect_enable(void __iomem *base_addr, u32 index, u32 enable)
{
	if (enable)
		reg_set_bit(base_addr + UI_LAYER_RECT_CTRL,
			     UI_LAYER_RECT_CTRL_EN(index));
	else
		reg_clr_bit(base_addr + UI_LAYER_RECT_CTRL,
			     UI_LAYER_RECT_CTRL_EN(index));
}

void de_scaler0_active_handle(void __iomem *base_addr, u32 index)
{
	const unsigned int *table;
	int i;

	table = scaling_coeffs[index];

	reg_set_bit(base_addr + SCALER0_CTRL, SCALER0_CTRL_CH0_V_COEF_LUT_EN);
	reg_set_bit(base_addr + SCALER0_CTRL, SCALER0_CTRL_CH1_V_COEF_LUT_EN);
	for (i = 0; i < 32; i++) {
		reg_write(base_addr + SCALER0_CH0_V_COEF(i), table[i]);
		reg_write(base_addr + SCALER0_CH1_V_COEF(i), table[i]);
	}
}

/**
 *@ channel: scaler channel
 * 0: y channel
 * 1: c channel
 */
void de_set_scaler0_channel(void __iomem *base_addr, u32 input_w, u32 input_h,
			    u32 output_w, u32 output_h, u32 channel)
{
	u32 dx, dy, h_phase, v_phase;

	dx = (input_w << 16) / output_w;
	dy = (input_h << 16) / output_h;

	h_phase = (dx >= 65536) ? ((dx >> 1) - 32768) : (dx >> 1);
	v_phase = (dy >= 65536) ? ((dy >> 1) - 32768) : (dy >> 1);

	reg_write(base_addr + SCALER0_INPUT_SIZE(channel),
		  SCALER0_INPUT_SIZE_SET(input_w, input_h));
	reg_write(base_addr + SCALER0_OUTPUT_SIZE(channel),
		  SCALER0_OUTPUT_SIZE_SET(output_w, output_h));
	reg_write(base_addr + SCALER0_H_INIT_PHASE(channel),
		  SCALER0_H_INIT_PHASE_SET(h_phase));
	reg_write(base_addr + SCALER0_H_RATIO(channel),
		  SCALER0_H_RATIO_SET(dx));
	reg_write(base_addr + SCALER0_V_INIT_PHASE(channel),
		  SCALER0_V_INIT_PHASE_SET(v_phase));
	reg_write(base_addr + SCALER0_V_RATIO(channel),
		  SCALER0_V_RATIO_SET(dy));
}

void de_scaler0_enable(void __iomem *base_addr, u32 enable)
{
	if (enable)
		reg_set_bit(base_addr + SCALER0_CTRL, SCALER0_CTRL_EN);
	else
		reg_clr_bit(base_addr + SCALER0_CTRL, SCALER0_CTRL_EN);
}

void de_timing_enable_interrupt(void __iomem *base_addr, u32 enable, u32 mask)
{
	if (enable)
		reg_set_bit(base_addr + TIMING_INIT, mask);
	else
		reg_clr_bits(base_addr + TIMING_INIT, mask);
}

u32 de_timing_interrupt_status(void __iomem *base_addr)
{
	return reg_read(base_addr + TIMING_STATUS);
}

void de_timing_interrupt_clean_status(void __iomem *base_addr, u32 status)
{
	reg_write(base_addr + TIMING_STATUS, status);
}

void de_timing_enable(void __iomem *base_addr, u32 enable)
{
	if (enable)
		reg_set_bit(base_addr + TIMING_CTRL, TIMING_CTRL_EN);
	else
		reg_clr_bit(base_addr + TIMING_CTRL, TIMING_CTRL_EN);
}

void de_config_timing(void __iomem *base_addr,
		      u32 active_w, u32 active_h,
		      u32 hfp, u32 hbp,
		      u32 vfp, u32 vbp,
		      u32 hsync, u32 vsync,
		      bool h_pol, bool v_pol)
{
	reg_write(base_addr + TIMING_ACTIVE_SIZE,
		  TIMING_ACTIVE_SIZE_SET(active_w, active_h));
	reg_write(base_addr + TIMING_H_PORCH, TIMING_H_PORCH_SET(hfp, hbp));
	reg_write(base_addr + TIMING_V_PORCH, TIMING_V_PORCH_SET(vfp, vbp));
	reg_write(base_addr + TIMING_SYNC_PLUSE,
		  TIMING_SYNC_PLUSE_SET_H_V(hsync, vsync));
	reg_write(base_addr + TIMING_POL_SET,
		  TIMING_POL_SET_H_V(h_pol, v_pol));
}

void de_set_blending_size(void __iomem *base_addr, u32 active_w, u32 active_h)
{
	reg_write(base_addr + BLENDING_OUTPUT_SIZE,
		  BLENDING_OUTPUT_SIZE_SET(active_w, active_h));
}
