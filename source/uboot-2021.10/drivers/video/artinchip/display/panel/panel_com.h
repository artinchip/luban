/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 ArtInChip Technology Co.,Ltd
 * Authors: Ning Fang <ning.fang@artinchip.com>
 */

#ifndef _PANEL_COM_H_
#define _PANEL_COM_H_

#include <backlight.h>
#include <linux/fb.h>
#include <asm/gpio.h>
#include <malloc.h>

#include "../aic_com.h"

struct panel_priv {
	struct aic_panel panel;
	struct udevice *supply;
	struct udevice *backlight;
	struct gpio_desc enable_gpio;
	struct gpio_desc gpiod_switch;
	struct display_timing timing;
	bool use_dt_timing;
};


void panel_di_enable(struct aic_panel *panel, u32 ms);
void panel_de_timing_enable(struct aic_panel *panel, u32 ms);
void panel_backlight_enable(struct aic_panel *panel, u32 ms);

int panel_default_prepare(struct aic_panel *panel);
int panel_default_enable(struct aic_panel *panel);
int panel_default_get_video_mode(struct aic_panel *panel,
						struct fb_videomode *vm);

int panel_register_callback(struct aic_panel *panel,
				struct aic_panel_callbacks *pcallback);

int panel_parse_dts(struct udevice *dev);

void panel_send_command(u8 *para_cmd, u32 size, struct aic_panel *panel);

static inline void
panel_init(struct panel_priv *p, struct udevice *dev, struct fb_videomode *vm,
		struct aic_panel_funcs *funcs, void *data)
{
	p->panel.dev = dev;
	p->panel.funcs = funcs;
	p->panel.vm = vm;
	p->panel.panel_private = data;
}

#endif
