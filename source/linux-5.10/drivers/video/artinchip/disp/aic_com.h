/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2020-2022 ArtInChip Technology Co., Ltd.
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#ifndef _AIC_COM_H_
#define _AIC_COM_H_

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <video/display_timing.h>
#include <video/videomode.h>
#include <video/artinchip_fb.h>

#include "../mpp_func.h"

#define ALIGN_EVEN(x) ((x) & (~1))
#define ALIGN_8B(x) (((x) + (7)) & ~(7))
#define ALIGN_16B(x) (((x) + (15)) & ~(15))
#define ALIGN_32B(x) (((x) + (31)) & ~(31))
#define ALIGN_64B(x) (((x) + (63)) & ~(63))
#define ALIGN_128B(x) (((x) + (127)) & ~(127))

struct device;

enum AIC_COM_TYPE {
	AIC_VE_COM   = 0x00,  /* video engine component */
	AIC_RGB_COM  = 0x01,  /* rgb component */
	AIC_LVDS_COM = 0x02,  /* lvds component */
	AIC_MIPI_COM = 0x03,  /* mipi component */
};

struct panel_dbi_commands {
	const u8 *buf;
	size_t len;
};

struct spi_cfg {
	unsigned int qspi_mode;
	unsigned int vbp_num;
	unsigned int code1_cfg;
	unsigned int code[3];
};

struct panel_dbi {
	unsigned int type;
	unsigned int format;
	unsigned int first_line;
	unsigned int other_line;
	struct panel_dbi_commands commands;
	struct spi_cfg *spi;
};

struct panel_rgb {
	unsigned int mode;
	unsigned int format;
	unsigned int clock_phase;
	unsigned int data_order;
	bool data_mirror;
};

enum lvds_mode {
	NS          = 0x0,
	JEIDA_24BIT = 0x1,
	JEIDA_18BIT = 0x2
};

enum lvds_link_mode {
	SINGLE_LINK0  = 0x0,
	SINGLE_LINK1  = 0x1,
	DOUBLE_SCREEN = 0x2,
	DUAL_LINK     = 0x3
};

struct panel_lvds {
	enum lvds_mode mode;
	enum lvds_link_mode link_mode;
};

enum dsi_mode {
	DSI_MOD_VID_PULSE = 0,
	DSI_MOD_VID_EVENT = 1,
	DSI_MOD_VID_BURST = 2,
	DSI_MOD_CMD_MODE = 3,
	DSI_MOD_MAX
};

enum dsi_format {
	DSI_FMT_RGB888 = 0,
	DSI_FMT_RGB666L	= 1,
	DSI_FMT_RGB666 = 2,
	DSI_FMT_RGB565 = 3,
	DSI_FMT_MAX
};

struct panel_dsi {
	enum dsi_mode mode;
	enum dsi_format format;
	unsigned int lane_num;
};

struct aic_tearing_effect {
	unsigned int mode;
	unsigned int pulse_width;
};

/* Processor to Peripheral Direction (Processor-Sourced) Packet Data Types */
#define DSI_DT_VSS		0x01
#define DSI_DT_VSE		0x11
#define DSI_DT_HSS		0x21
#define DSI_DT_HSE		0x31
#define DSI_DT_EOT		0x08
#define DSI_DT_CM_OFF		0x02
#define DSI_DT_CM_ON		0x12
#define DSI_DT_SHUT_DOWN	0x22
#define DSI_DT_TURN_ON		0x32
#define DSI_DT_GEN_WR_P0	0x03
#define DSI_DT_GEN_WR_P1	0x13
#define DSI_DT_GEN_WR_P2	0x23
#define DSI_DT_GEN_RD_P0	0x04
#define DSI_DT_GEN_RD_P1	0x14
#define DSI_DT_GEN_RD_P2	0x24
#define DSI_DT_DCS_WR_P0	0x05
#define DSI_DT_DCS_WR_P1	0x15
#define DSI_DT_DCS_RD_P0	0x06
#define DSI_DT_MAX_RET_SIZE	0x37
#define DSI_DT_NULL		0x09
#define DSI_DT_BLK		0x19
#define DSI_DT_GEN_LONG_WR	0x29
#define DSI_DT_DCS_LONG_WR	0x39
#define DSI_DT_PIXEL_RGB565	0x0E
#define DSI_DT_PIXEL_RGB666P	0x1E
#define DSI_DT_PIXEL_RGB666	0x2E
#define DSI_DT_PIXEL_RGB888	0x3E
/* Data Types for Peripheral-sourced Packets */
#define DSI_DT_ACK_ERR		0x02
#define DSI_DT_EOT_PERI		0x08
#define DSI_DT_GEN_RD_R1	0x11
#define DSI_DT_GEN_RD_R2	0x12
#define DSI_DT_GEN_LONG_RD_R	0x1A
#define DSI_DT_DCS_LONG_RD_R	0x1C
#define DSI_DT_DCS_RD_R1	0x21
#define DSI_DT_DCS_RD_R2	0x22

#define DSI_DCS_ENTER_IDLE_MODE		0x39
#define DSI_DCS_ENTER_INVERT_MODE	0x21
#define DSI_DCS_ENTER_NORMAL_MODE	0x13
#define DSI_DCS_ENTER_PARTIAL_MODE	0x12
#define DSI_DCS_ENTER_SLEEP_MODE	0x10
#define DSI_DCS_EXIT_IDLE_MODE		0x38
#define DSI_DCS_EXIT_INVERT_MODE	0x20
#define DSI_DCS_EXIT_SLEEP_MODE		0x11
#define DSI_DCS_GET_ADDRESS_MODE	0x0b
#define DSI_DCS_GET_BLUE_CHANNEL	0x08
#define DSI_DCS_GET_DIAGNOSTIC_RESULT	0x0f
#define DSI_DCS_GET_DISPLAY_MODE	0x0d
#define DSI_DCS_GET_GREEN_CHANNEL	0x07
#define DSI_DCS_GET_PIXEL_FORMAT	0x0c
#define DSI_DCS_GET_POWER_MODE		0x0a
#define DSI_DCS_GET_RED_CHANNEL		0x06
#define DSI_DCS_GET_SCANLINE		0x45
#define DSI_DCS_GET_SIGNAL_MODE		0x0e
#define DSI_DCS_NOP			0x00
#define DSI_DCS_READ_DDB_CONTINUE	0xa8
#define DSI_DCS_READ_DDB_START		0xa1
#define DSI_DCS_READ_MEMORY_CONTINUE	0x3e
#define DSI_DCS_READ_MEMORY_START	0x2e
#define DSI_DCS_SET_ADDRESS_MODE	0x36
#define DSI_DCS_SET_COLUMN_ADDRESS	0x2a
#define DSI_DCS_SET_DISPLAY_OFF		0x28
#define DSI_DCS_SET_DISPLAY_ON		0x29
#define DSI_DCS_SET_GAMMA_CURVE		0x26
#define DSI_DCS_SET_PAGE_ADDRESS	0x2b
#define DSI_DCS_SET_PARTIAL_AREA	0x30
#define DSI_DCS_SET_PIXEL_FORMAT	0x3a
#define DSI_DCS_SET_SCROLL_AREA		0x33
#define DSI_DCS_SET_SCROLL_START	0x37
#define DSI_DCS_SET_TEAR_OFF		0x34
#define DSI_DCS_SET_TEAR_ON		0x35
#define DSI_DCS_SET_TEAR_SCANLINE	0x44
#define DSI_DCS_SOFT_RESET		0x01
#define DSI_DCS_WRITE_LUT		0x2d
#define DSI_DCS_WRITE_MEMORY_CONTINUE	0x3c
#define DSI_DCS_WRITE_MEMORY_START	0x2c

struct aic_panel;

struct de_funcs {
	int (*set_mode)(struct aic_panel *panel, struct videomode *vm);
	int (*clk_enable)(void);
	int (*clk_disable)(void);
	int (*timing_enable)(u32 flags);
	int (*timing_disable)(void);
	int (*wait_for_vsync)(void);
	int (*shadow_reg_ctrl)(int enable);
	int (*get_layer_num)(struct aicfb_layer_num *num);
	int (*get_layer_cap)(struct aicfb_layer_capability *cap);
	int (*get_layer_config)(struct aicfb_layer_data *layer_data);
	int (*update_layer_config)(struct aicfb_layer_data *layer_data);
	int (*update_layer_config_list)(struct aicfb_config_lists *list);
	int (*get_alpha_config)(struct aicfb_alpha_config *alpha);
	int (*update_alpha_config)(struct aicfb_alpha_config *alpha);
	int (*get_ck_config)(struct aicfb_ck_config *ck);
	int (*update_ck_config)(struct aicfb_ck_config *ck);
#ifdef CONFIG_DMA_SHARED_BUFFER
	int (*add_dmabuf)(struct dma_buf_info *fds);
	int (*remove_dmabuf)(struct dma_buf_info *fds);
	int (*release_dmabuf)(void);
#endif
	int (*color_bar_ctrl)(int enable);
	ulong (*pixclk_rate)(void);
	int (*pixclk_enable)(void);
	int (*set_display_prop)(struct aicfb_disp_prop *disp_prop);
	int (*get_display_prop)(struct aicfb_disp_prop *disp_prop);
};

struct di_funcs {
	enum AIC_COM_TYPE type;
	s32 (*clk_enable)(void);
	s32 (*clk_disable)(void);
	s32 (*enable)(void);
	s32 (*disable)(void);
	s32 (*attach_panel)(struct aic_panel *panel);
	s32 (*pixclk2mclk)(ulong pixclk);
	s32 (*set_videomode)(struct videomode *vm, int enable);
	s32 (*send_cmd)(u32 dt, const u8 *data, u32 len);
};

struct aic_panel_callbacks {
	int (*di_enable)(void);
	int (*di_disable)(void);
	int (*di_send_cmd)(u32 dt, const u8 *data, u32 len);
	int (*di_set_videomode)(struct videomode *vm, int enable);
	int (*timing_enable)(u32 flags);
	int (*timing_disable)(void);
};

/* Each panel driver should define the follow functions. */
struct aic_panel_funcs {
	int (*prepare)(struct aic_panel *panel);
	int (*enable)(struct aic_panel *panel);
	int (*disable)(struct aic_panel *panel);
	int (*unprepare)(struct aic_panel *panel);
	int (*get_video_mode)(struct aic_panel *panel, struct videomode **vm);
	int (*register_callback)(struct aic_panel *panel,
				struct aic_panel_callbacks *pcallback);
};

struct aic_panel {
	struct aic_panel_funcs *funcs;
	struct aic_panel_callbacks callbacks;
	struct aic_tearing_effect te;
	int disp_dither;
	struct videomode *vm;
	struct device *dev;
	union {
		struct panel_rgb *rgb;
		struct panel_lvds *lvds;
		struct panel_dsi *dsi;
		struct panel_dbi *dbi;
	};
	void *panel_private;
};

static inline void aic_delay_ms(u32 ms)
{
	mdelay(ms);
}

static inline void aic_delay_us(u32 us)
{
	udelay(us);
}

#endif /* _AIC_COM_H_ */
