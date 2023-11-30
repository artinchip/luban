// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2021 ArtInChip Technology Co., Ltd.
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <artinchip/artinchip_fb.h>
#include "reg_util.h"
#include "de_hw.h"

void de_config_prefetch_line_set(void __iomem *base_addr, u32 line)
{
	reg_set_bits(base_addr + TIMING_LINE_SET,
			TIMING_LINE_SET_PREFETCH_LINE_MASK,
			TIMING_LINE_SET_PREFETCH_LINE(line));
}
void de_soft_reset_ctrl(void __iomem *base_addr, u32 enable)
{
	if (enable)
		reg_set_bit(base_addr + WB_BASE, DE_SOFT_RESET_EN);
	else
		reg_clr_bit(base_addr + WB_BASE, DE_SOFT_RESET_EN);
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
			    enum aic_pixel_format format)
{
	reg_set_bits(base_addr + UI_LAYER_CTRL,
			 UI_LAYER_CTRL_INPUT_FORMAT_MASK,
			 UI_LAYER_CTRL_INPUT_FORMAT(format));
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
