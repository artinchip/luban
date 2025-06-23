/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2020-2022 ArtInChip Technology Co., Ltd.
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#ifndef _AIC_COM_H_
#define _AIC_COM_H_

#include <linux/fb.h>
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
	DSI_MOD_VID_PULSE        = BIT(0),
	DSI_MOD_VID_EVENT        = BIT(1),
	DSI_MOD_VID_BURST        = BIT(2),
	DSI_MOD_CMD_MODE         = BIT(3),

	DSI_CLOCK_NON_CONTINUOUS = BIT(4),
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

struct aicfb_info {
	char name[8];

	int fb_rotate;
	int disp_buf_num;

	int fb_size;
	void *fb_start;
	dma_addr_t fb_start_dma;
	bool dma_coherent;

	struct mpp_size screen_size;
	u32 pseudo_palette[16];

	struct device *de_dev;
	struct device *di_dev;
	struct device *panel_dev;
	struct de_funcs *de;
	struct di_funcs *di;
	struct aic_panel *panel;

	struct mutex mutex;

	struct aicfb_disp_prop *disp_prop;
	bool set_hsbc;
};

struct aicfb_ioctl_cmd {
	u32 cmd;
	s32 (*f)(struct aicfb_info *fbi, unsigned long arg);
	char desc[32];
};

int aic_fb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg);

static inline void aic_delay_ms(u32 ms)
{
	mdelay(ms);
}

static inline void aic_delay_us(u32 us)
{
	udelay(us);
}

#endif /* _AIC_COM_H_ */
