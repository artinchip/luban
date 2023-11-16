/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2020-2022 ArtInChip Technology Co., Ltd.
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#ifndef _PANEL_COM_H_
#define _PANEL_COM_H_

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/backlight.h>
#include <video/videomode.h>

#include "../aic_com.h"

struct panel_comp {
	struct aic_panel panel;
	struct display_timings *timings;
	bool   use_dt_timing;
	struct backlight_device *backlight;
	struct regulator *supply;
	struct gpio_desc *enable_gpio;
	struct gpio_desc *sleep_gpio;
};


/* Defined in panel_com.c, called in panel_xxx.c */

void panel_di_enable(struct aic_panel *panel, u32 ms);
void panel_di_disable(struct aic_panel *panel, u32 ms);
void panel_de_timing_enable(struct aic_panel *panel, u32 ms);
void panel_de_timing_disable(struct aic_panel *panel, u32 ms);
void panel_backlight_enable(struct aic_panel *panel, u32 ms);
void panel_backlight_disable(struct aic_panel *panel, u32 ms);

int panel_default_unprepare(struct aic_panel *panel);
int panel_default_prepare(struct aic_panel *panel);
int panel_default_enable(struct aic_panel *panel);
int panel_default_disable(struct aic_panel *panel);
int panel_default_get_video_mode(struct aic_panel *panel,
			struct videomode **vm);

void panel_default_unbind(struct device *dev, struct device *master,
			  void *data);
int panel_register_callback(struct aic_panel *panel,
				struct aic_panel_callbacks *pcallback);
int panel_parse_dts(struct panel_comp *p, struct device *dev);

void panel_send_command(u8 *para_cmd, u32 size, struct aic_panel *panel);

static inline
void panel_init(struct panel_comp *p, struct device *dev, struct videomode *vm,
		struct aic_panel_funcs *funcs, void *data)
{
	p->panel.dev = dev;
	p->panel.funcs = funcs;
	p->panel.vm = vm;
	p->panel.panel_private = data;
}

#endif /* _PANEL_COM_H_ */
