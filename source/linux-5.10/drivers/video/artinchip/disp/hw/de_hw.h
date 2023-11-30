/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2020-2022 ArtInChip Technology Co., Ltd.
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#ifndef _DE_HW_H_
#define _DE_HW_H_

#include <linux/bits.h>
#include <linux/io.h>
#include <linux/types.h>

#define DE_CTRL_DITHER_EN                     BIT(0)
#define DE_MODE_SELECT_COLOR_BAR              BIT(0)
#define DE_CONFIG_UPDATE_EN                   BIT(0)
#define DE_SOFT_RESET_EN		      BIT(16)
#define DE_RAND_DITHER_EN                     BIT(31)
#define OUTPUT_COLOR_DEPTH_SET(r, g, b)       ((((~r) & 0x3) << 16) \
	| (((~g) & 0x3) << 8) \
	| ((~b) & 0x3))

#define VIDEO_LAYER_CTRL_INPUT_FORMAT_MASK    GENMASK(14, 8)
#define VIDEO_LAYER_CTRL_INPUT_FORMAT(x)      (((x) & 0x07f) << 8)
#define VIDEO_LAYER_CTRL_EN                   BIT(0)

#define VIDEO_LAYER_INPUT_SIZE_SET(w, h)      ((((h) & 0x1fff) << 16) \
					       | (((w) & 0x1fff) << 0))

#define VIDEO_LAYER_STRIDE_SET(p0, p1)        ((((p1) & 0x1fff) << 16) \
					       | (((p0) & 0x1fff) << 0))

#define VIDEO_LAYER_TILE_OFFSET0_SET(x, y)    ((((y) & 0xff) << 16) \
					       | (((x) & 0xff) << 0))

#define VIDEO_LAYER_TILE_OFFSET1_SET(x, y)    ((((y) & 0xff) << 16) \
					       | (((x) & 0xff) << 0))

#define UI_LAYER_CTRL_G_ALPHA_MASK            GENMASK(31, 24)
#define UI_LAYER_CTRL_G_ALPHA(x)              (((x) & 0xff) << 24)

#define UI_LAYER_CTRL_ALPHA_MODE_MASK         GENMASK(23, 22)
#define UI_LAYER_CTRL_ALPHA_MODE(x)           (((x) & 0x03) << 22)

#define UI_LAYER_CTRL_INPUT_FORMAT_MASK       GENMASK(14, 8)
#define UI_LAYER_CTRL_INPUT_FORMAT(x)         (((x) & 0x07f) << 8)
#define UI_LAYER_CTRL_BG_BLEND_EN             BIT(21)
#define UI_LAYER_CTRL_ALPHA_EN                BIT(2)
#define UI_LAYER_CTRL_COLOR_KEY_EN            BIT(1)
#define UI_LAYER_CTRL_EN                      BIT(0)

#define UI_LAYER_SIZE_SET(w, h)               ((((h) & 0x1fff) << 16) \
					       | (((w) & 0x1fff) << 0))
#define UI_LAYER_COLOER_KEY_SET(x)            ((x) & 0x00ffffff)
#define UI_LAYER_RECT_CTRL_EN(x)              BIT(x)

#define UI_RECT_INPUT_SIZE_SET(w, h)          ((((h) & 0x1fff) << 16) \
					       | (((w) & 0x1fff) << 0))

#define UI_RECT_OFFSET_SET(x, y)              ((((y) & 0x1fff) << 16) \
					       | (((x) & 0x1fff) << 0))

#define UI_RECT_STRIDE_SET(x)                 ((x) & 0x7fff)

#define BLENDING_BG_COLOR_SET(x)              ((x) & 0x00ffffff)
#define BLENDING_OUTPUT_SIZE_SET(w, h)        ((((h) & 0x1fff) << 16) \
					       | (((w) & 0x1fff) << 0))

#define UI_LAYER_OFFSET_SET(x, y)             ((((y) & 0x1fff) << 16) \
					       | (((x) & 0x1fff) << 0))

#define VIDEO_LAYER_OFFSET_SET(x, y)          ((((y) & 0x1fff) << 16) \
					       | (((x) & 0x1fff) << 0))

#define CSC0_COEF_SET(x)                      ((x) & 0x1FFF)
#define CSC0_COEF_OFFSET_SET(x)               ((x) & 0x3FFF)

#define SCALER0_CTRL_CH1_V_COEF_LUT_EN        BIT(7)
#define SCALER0_CTRL_CH1_H_COEF_LUT_EN        BIT(6)
#define SCALER0_CTRL_CH0_V_COEF_LUT_EN        BIT(5)
#define SCALER0_CTRL_CH0_H_COEF_LUT_EN        BIT(4)
#define SCALER0_CTRL_BILINEAR_SELECT          BIT(2)
#define SCALER0_CTRL_EN                       BIT(0)

#define SCALER0_INPUT_SIZE_SET(w, h)          ((((h) & 0x1fff) << 16) \
					       | (((w) & 0x1fff) << 0))

#define SCALER0_OUTPUT_SIZE_SET(w, h)         ((((h) & 0x1fff) << 16) \
					       | (((w) & 0x1fff) << 0))

#define SCALER0_H_INIT_PHASE_SET(x)           ((x) & 0xfffff)
#define SCALER0_H_RATIO_SET(x)                ((x) & 0x1fffff)
#define SCALER0_V_INIT_PHASE_SET(x)           ((x) & 0xfffff)
#define SCALER0_V_RATIO_SET(x)                ((x) & 0x1fffff)

#define WB_CTRL_FORMAT(x)                     (((x) & 0x07f) << 8)
#define WB_CTRL_PATH_SELECT(x)                (((x) & 0x01) << 1)
#define WB_CTRL_START                         BIT(0)

#define WB_INT_OVERFLOW                       BIT(1)
#define WB_INT_FINISH                         BIT(0)

#define WB_INT_OVERFLOW_FLAG                  BIT(1)
#define WB_INT_FINISH_FLAG                    BIT(0)
#define WB_STRIDE_SET(x)                      ((x) & 0x1fff)

#define TIMING_TE_PULSE_WIDTH(x)	      (((x) & 0xffff) << 8)
#define TIMING_TE_PULSE_WIDTH_MASK	      GENMASK(23, 8)
#define TIMING_TE_MODE(x)		      (((x) & 0x03) << 4)
#define TIMING_TE_MODE_MASK		      GENMASK(5, 4)

#define TIMING_CTRL_EN                        BIT(0)

#define TIMING_INIT_SF_END		      BIT(8)
#define TIMING_INIT_UNDERFLOW                 BIT(2)
#define TIMING_INIT_LINE                      BIT(1)
#define TIMING_INIT_V_BLANK                   BIT(0)

#define TIMING_INIT_SF_END_FLAG		      BIT(8)
#define TIMING_INIT_UNDERFLOW_FLAG            BIT(2)
#define TIMING_INIT_LINE_FLAG                 BIT(1)
#define TIMING_INIT_V_BLANK_FLAG              BIT(0)

#define TIMING_LINE_SET_PREFETCH_LINE_MASK    GENMASK(28, 16)
#define TIMING_LINE_SET_PREFETCH_LINE(x)      (((x) & 0x1fff) << 16)
#define TIMING_LINE_SET_INT_LINE(x)           (((x) & 0x1fff))

#define TIMING_ACTIVE_SIZE_SET(w, h)          ((((h) & 0x1fff) << 16) \
					       | (((w) & 0x1fff) << 0))

#define TIMING_H_PORCH_SET(f, b)              ((((b) & 0x1fff) << 16) \
					       | (((f) & 0x1fff) << 0))

#define TIMING_V_PORCH_SET(f, b)              ((((b) & 0x1fff) << 16) \
					       | (((f) & 0x1fff) << 0))

#define TIMING_SYNC_PLUSE_SET_H_V(h, v)       ((((v) & 0x7ff) << 16) \
					       | (((h) & 0x7ff) << 0))

#define TIMING_POL_SET_H_V(h, v)              ((((v) & 0x1) << 1) \
					       | (((h) & 0x1) << 0))

/* base offset */
#define G_BASE                       0x000
#define DITHER_BASE                  0x00c
#define VIDEO_BASE                   0x020
#define UI_BASE                      0x0a0
#define BLENDING_BASE                0x100
#define SCALER0_BASE                 0x120
#define WB_BASE                      0x170
#define TIMING_BASE                  0x1d0
#define QOS_BASE		     0x880

/* global control */
#define DE_CTRL                      (G_BASE + 0x000)
#define DE_MODE_SELECT               (G_BASE + 0x004)
#define DE_CONFIG_UPDATE             (G_BASE + 0x008)
#define DE_VERSION_ID                (G_BASE + 0x010)

/* dither control */
#define OUTPUT_COLOR_DEPTH           (DITHER_BASE + 0x000)
#define DITHER_RAND_SEED	     (DITHER_BASE + 0x008)
#define DITHER_RAND_MASK_BITS	     (DITHER_BASE + 0x00c)

/* video layer control */
#define VIDEO_LAYER_CTRL             (VIDEO_BASE + 0x000)
#define VIDEO_LAYER_INPUT_SIZE       (VIDEO_BASE + 0x004)

#define VIDEO_LAYER_STRIDE           (VIDEO_BASE + 0x010)
#define VIDEO_LAYER_ADDR0            (VIDEO_BASE + 0x020)
#define VIDEO_LAYER_ADDR1            (VIDEO_BASE + 0x024)
#define VIDEO_LAYER_ADDR2            (VIDEO_BASE + 0x028)

#define VIDEO_LAYER_TILE_OFFSET0     (VIDEO_BASE + 0x040)
#define VIDEO_LAYER_TILE_OFFSET1     (VIDEO_BASE + 0x044)

/* n = 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 */
#define VIDEO_LAYER_CSC0_COEF(n)     (VIDEO_BASE + 0x050 + 0x4 * (n))

/* ui layer control */
#define UI_LAYER_CTRL                (UI_BASE + 0x000)
#define UI_LAYER_SIZE                (UI_BASE + 0x004)
#define UI_LAYER_BG_COLOR            (UI_BASE + 0x008)
#define UI_LAYER_COLOER_KEY          (UI_BASE + 0x00c)
#define UI_LAYER_RECT_CTRL           (UI_BASE + 0x010)

/* n = 0, 1, 2, 3 */
#define UI_RECT_INPUT_SIZE(n)        (UI_BASE + 0x020 + 0x10 * (n))
#define UI_RECT_OFFSET(n)            (UI_BASE + 0x024 + 0x10 * (n))
#define UI_RECT_STRIDE(n)            (UI_BASE + 0x028 + 0x10 * (n))
#define UI_RECT_ADDR(n)              (UI_BASE + 0x02c + 0x10 * (n))

/* blending control */
#define BLENDING_BG_COLOR            (BLENDING_BASE + 0x000)
#define BLENDING_OUTPUT_SIZE         (BLENDING_BASE + 0x004)
#define UI_LAYER_OFFSET              (BLENDING_BASE + 0x00c)
#define VIDEO_LAYER_OFFSET           (BLENDING_BASE + 0x014)

/* scaler0 control */
#define SCALER0_CTRL                 (SCALER0_BASE + 0x000)

/* ch = 0, 1 */
#define SCALER0_INPUT_SIZE(ch)       (SCALER0_BASE + 0x010 + 0x20 * (ch))
#define SCALER0_OUTPUT_SIZE(ch)      (SCALER0_BASE + 0x014 + 0x20 * (ch))
#define SCALER0_H_INIT_PHASE(ch)     (SCALER0_BASE + 0x018 + 0x20 * (ch))
#define SCALER0_H_RATIO(ch)          (SCALER0_BASE + 0x01c + 0x20 * (ch))
#define SCALER0_V_INIT_PHASE(ch)     (SCALER0_BASE + 0x020 + 0x20 * (ch))
#define SCALER0_V_RATIO(ch)          (SCALER0_BASE + 0x024 + 0x20 * (ch))

/* write back control */
#define WB_CTRL                      (WB_BASE + 0x000)
#define WB_INT                       (WB_BASE + 0x004)
#define WB_INT_STATUS                (WB_BASE + 0x008)
#define WB_STRIDE                    (WB_BASE + 0x010)
#define	WB_ADDR                      (WB_BASE + 0x020)

/* timing control */
#define TIMING_CTRL                  (TIMING_BASE + 0x000)
#define TIMING_INIT                  (TIMING_BASE + 0x004)
#define TIMING_STATUS                (TIMING_BASE + 0x008)
#define TIMING_LINE_SET              (TIMING_BASE + 0x00c)
#define TIMING_ACTIVE_SIZE           (TIMING_BASE + 0x010)
#define TIMING_H_PORCH               (TIMING_BASE + 0x014)
#define TIMING_V_PORCH               (TIMING_BASE + 0x018)
#define TIMING_SYNC_PLUSE            (TIMING_BASE + 0x01c)
#define TIMING_POL_SET               (TIMING_BASE + 0x020)
#define TIMING_DEBUG                 (TIMING_BASE + 0x024)

/* ch = 0, 1 */
#define SCALER0_CH0_H_COEF(n)        (0x400 + 4 * (n))
#define SCALER0_CH0_V_COEF(n)        (0x500 + 4 * (n))
#define SCALER0_CH1_H_COEF(n)        (0x600 + 4 * (n))
#define SCALER0_CH1_V_COEF(n)        (0x700 + 4 * (n))

/* n = 0, 1, 2, 3 */
#define QOS_CONFIG(n)		     (QOS_BASE + 4 * (n))
#define QOS_URGENT		     (QOS_BASE + 0x010)

#define DMAR_QOS_GREEN(x)	     (((x) & 0xf) << 28)
#define DMAR_QOS_HIGH(x)	     (((x) & 0x3ff) << 16)
#define DMAR_QOS_RED(x)		     (((x) & 0xf) << 12)
#define DMAR_QOS_LOW(x)		     (((x) & 0x3ff) << 0)

#define QOS_SET(x0, x1, x2, x3)	     (DMAR_QOS_GREEN(x0)   | DMAR_QOS_HIGH(x1) \
					| DMAR_QOS_RED(x2) | DMAR_QOS_LOW(x3))

#define OUTSTANDING(x)		     (((x) & 0x1f) << 16)
#define DMAR_URGENT_EN(x)	     (((x) & 0x1) << 15)
#define DMAR_URGENT_TH(x)	     (((x) & 0x3ff) << 0)

#define QOS_URGENT_SET(x0, x1, x2)   (OUTSTANDING(x0) | DMAR_URGENT_EN(x1)     \
					| DMAR_URGENT_TH(x2))

/**
 *@ enable
 * 0: disable update config
 * 1: enable update config
 */
void de_config_update_enable(void __iomem *base_addr, u32 enable);

/**
 *@ r_depth
 *@ g_depth
 *@ b_depth
 *
 * 0: 8 bits
 * 1: 6 bits
 * 2: 5 bits
 *
 */
void de_set_dither(void __iomem *base_addr, u32 r_depth,
		   u32 g_depth, u32 b_depth, u32 enable);

void de_set_qos(void __iomem *base_addr);

void de_colorbar_ctrl(void __iomem *base_addr, u32 enable);

void de_soft_reset_ctrl(void __iomem *base_addr, u32 enable);

u32 de_get_version_id(void __iomem *base_addr);

void de_set_video_layer_info(void __iomem *base_addr, u32 w, u32 h,
			     enum mpp_pixel_format format,
			     u32 stride0, u32 stride1,
			     u32 addr0, u32 addr1, u32 addr2,
			     u32 x_offset, u32 y_offset);

void de_set_video_layer_tile_offset(void __iomem *base_addr,
				    u32 p0_x_offset, u32 p0_y_offset,
				    u32 p1_x_offset, u32 p1_y_offset);

void de_video_layer_enable(void __iomem *base_addr, u32 enable);

/**
 *@ color_space
 * 0: BT601;
 * 1: BT709;
 * 2: BT601 full range;
 * 3: BT709 full range;
 */
void de_set_csc0_coefs(void __iomem *base_addr, int color_space);

/*
 *@ color_space
 * 0: BT601;
 * 1: BT709;
 * 2: BT601 full range;
 * 3: BT709 full range;
 *
 * @hue, sat, bright, contrast: range [0 ,100]
*/
int de_set_hsbc_with_csc_coefs(void __iomem *base_addr, int color_space,
			       int bright, int contrast,
			       int saturation, int hue);

int get_rgb_hsbc_csc_coefs(int bright, int contrast, int saturation, int hue,
			   u32 *csc_coef);

void de_set_ui_layer_size(void __iomem *base_addr, u32 w, u32 h,
			  u32 x_offset, u32 y_offset);

/**
 *@ g_alpha (0~255)
 *@ alpha_mode
 * 0：pixel alpha
 * 1: global alpha
 * 2: mixer alpha
 */
void de_ui_alpha_blending_enable(void __iomem *base_addr, u32 g_alpha,
				 u32 alpha_mode, u32 enable);

void de_set_ui_layer_format(void __iomem *base_addr,
			    enum mpp_pixel_format format);

/**
 *@ color_key: 32 bits
 * [31:24] reserved
 * [23:16] r value
 * [15:8]  g value
 * [7:0]   b value
 */
void de_ui_layer_color_key_enable(void __iomem *base_addr,
				  u32 color_key, u32 ck_en);

void de_ui_layer_set_rect(void __iomem *base_addr, u32 w, u32 h,
			  u32 x_offset, u32 y_offset,
			  u32 stride, u32 addr, u32 id);

void de_ui_layer_enable(void __iomem *base_addr, u32 enable);

void de_ui_layer_rect_enable(void __iomem *base_addr, u32 index, u32 enable);

void de_set_scaler0_channel(void __iomem *base_addr, u32 input_w, u32 input_h,
			    u32 output_w, u32 output_h, u32 channel);

void de_scaler0_enable(void __iomem *base_addr, u32 enable);

/**
 *@ mask: interrupt mask
 * TIMING_INIT_UNDERFLOW
 * TIMING_INIT_LINE
 * TIMING_INIT_V_BLANK
 */
void de_timing_enable_interrupt(void __iomem *base_addr, u32 enable, u32 mask);

/**
 *@ return: interrupt status mask
 * TIMING_INIT_UNDERFLOW_FLAG
 * TIMING_INIT_LINE_FLAG
 * TIMING_INIT_V_BLANK_FLAG
 */
u32 de_timing_interrupt_status(void __iomem *base_addr);

void de_timing_interrupt_clean_status(void __iomem *base_addr, u32 status);

void de_timing_enable(void __iomem *base_addr, u32 enable);

void de_config_timing(void __iomem *base_addr,
		      u32 active_w, u32 active_h,
		      u32 hfp, u32 hbp,
		      u32 vfp, u32 vbp,
		      u32 hsync, u32 vsync,
		      bool h_pol, bool v_pol);

void de_set_blending_size(void __iomem *base_addr,
			  u32 active_w, u32 active_h);

void de_config_prefetch_line_set(void __iomem *base_addr, u32 line);

void de_config_tearing_effect(void __iomem *base_addr,
			u32 mode, u32 pulse_width);

#endif /* _DE_HW_H_ */
