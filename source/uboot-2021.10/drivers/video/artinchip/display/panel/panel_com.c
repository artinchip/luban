// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Authors:  Huahui Mai <huahui.mai@artinchip.com>
 */

#include <common.h>
#include <dm.h>
#include <dm/uclass-internal.h>
#include <power/regulator.h>
#include <backlight.h>
#include <linux/kernel.h>

#include "panel_com.h"

static inline struct panel_priv *to_panel_priv(struct aic_panel *panel)
{
	return container_of(panel, struct panel_priv, panel);
}

/* Enable the display interface */
void panel_di_enable(struct aic_panel *panel, u32 ms)
{
	if (panel && panel->callbacks.di_enable)
		panel->callbacks.di_enable();

	if (ms)
		aic_delay_ms(ms);
}

/* Disable the display interface */
void panel_backlight_enable(struct aic_panel *panel, u32 ms)
{
	struct panel_priv *priv = to_panel_priv(panel);
	struct udevice *backlight = priv->backlight;

	if (backlight)
		backlight_enable(backlight);

	if (dm_gpio_is_valid(&priv->enable_gpio))
		dm_gpio_set_value(&priv->enable_gpio, 1);

	if (ms)
		aic_delay_ms(ms);
}

void panel_de_timing_enable(struct aic_panel *panel, u32 ms)
{
	if (panel && panel->callbacks.timing_enable)
		panel->callbacks.timing_enable();

	if (ms)
		aic_delay_ms(ms);
}

int panel_default_enable(struct aic_panel *panel)
{
	panel_di_enable(panel, 0);
	panel_de_timing_enable(panel, 40);
	panel_backlight_enable(panel, 0);
	return 0;
}

int panel_default_prepare(struct aic_panel *panel)
{
	struct panel_priv *priv = to_panel_priv(panel);
	int ret;

	if (IS_ENABLED(CONFIG_DM_REGULATOR) && priv->supply) {
		ret = regulator_set_enable(priv->supply, true);
		if (ret) {
			debug("%s() - Failed to enable panel supply: %d\n",
						priv->supply->name, ret);
			return ret;
		}
	}

	return 0;
}

void panel_videomode_from_timing(struct fb_videomode *vm,
		const struct display_timing *dt)
{
	vm->pixclock = dt->pixelclock.typ;
	vm->xres = dt->hactive.typ;
	vm->right_margin = dt->hfront_porch.typ;
	vm->left_margin = dt->hback_porch.typ;
	vm->hsync_len = dt->hsync_len.typ;

	vm->yres = dt->vactive.typ;
	vm->lower_margin = dt->vfront_porch.typ;
	vm->upper_margin = dt->vback_porch.typ;
	vm->vsync_len = dt->vsync_len.typ;

	vm->flag = dt->flags;
}

int panel_default_get_video_mode(struct aic_panel *panel,
						struct fb_videomode *vm)
{
	struct panel_priv *priv = to_panel_priv(panel);

	if (vm) {
		if (priv->use_dt_timing)
			panel_videomode_from_timing(vm, &priv->timing);
		else
			memcpy(vm, panel->vm, sizeof(struct fb_videomode));
	}

	return 0;
}

int panel_register_callback(struct aic_panel *panel,
				struct aic_panel_callbacks *pcallback)
{
	panel->callbacks.di_enable = pcallback->di_enable;
	panel->callbacks.di_send_cmd = pcallback->di_send_cmd;
	panel->callbacks.di_set_videomode = pcallback->di_set_videomode;
	panel->callbacks.timing_enable = pcallback->timing_enable;
	return 0;
}

int panel_parse_dts(struct udevice *dev)
{
	struct panel_priv *priv = dev_get_priv(dev);
	int ret;

	if (IS_ENABLED(CONFIG_DM_REGULATOR)) {
		ret = uclass_find_device_by_phandle(UCLASS_REGULATOR, dev,
						   "power", &priv->supply);
		if (ret) {
			debug("failed to request panel regulator\n");
			priv->supply = NULL;
		}
	}
	ret = uclass_get_device_by_phandle(UCLASS_PANEL_BACKLIGHT, dev,
					   "backlight", &priv->backlight);
	if (ret) {
		debug("failed to request backlight\n");
		priv->backlight = NULL;
	}

	ret = gpio_request_by_name(dev, "enable-gpios", 0, &priv->enable_gpio,
				   GPIOD_IS_OUT);
	if (ret)
		debug("failed to request enable_gpio\n");
	else
		dm_gpio_set_value(&priv->enable_gpio, 0);

	ret = ofnode_read_u32(dev_ofnode(dev), "disp-dither",
				&priv->panel.disp_dither);
	if (ret)
		priv->panel.disp_dither = 0;

	ret = ofnode_read_u32(dev_ofnode(dev), "tearing-effect",
				&priv->panel.te.mode);
	if (ret) {
		priv->panel.te.mode = 0;
		priv->panel.te.pulse_width = 0;
	} else {
		ret = ofnode_read_u32(dev_ofnode(dev), "tearing-effect",
					&priv->panel.te.pulse_width);
		if (ret) {
			debug("Can't get tearing effect signal pulse width\n");
			return ret;
		}

	}

	if (!ofnode_decode_display_timing(dev_ofnode(dev), 0, &priv->timing))
		priv->use_dt_timing = true;
	else
		priv->use_dt_timing = false;

	return 0;
}

void panel_send_command(u8 *para_cmd, u32 size, struct aic_panel *panel)
{
	u8 *p;
	u8 num, code;

	if (unlikely(!panel && !panel->callbacks.di_send_cmd))
		panic("Have no send_cmd() API for DSI.\n");

	for (p = para_cmd; p < (para_cmd + size);) {
		num  = *p++ - 1;
		code = *p++;

		if (num == 0)
			aic_delay_ms((u32)code);
		else
			panel->callbacks.di_send_cmd((u32)code, p, num);

		p += num;
	}
}

