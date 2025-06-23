// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2022 ArtInChip Technology Co., Ltd.
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <video/of_display_timing.h>
#include <linux/regulator/consumer.h>

#include "panel_com.h"

static DEFINE_MUTEX(panel_lock);

static inline struct panel_comp *to_panel_comp(struct aic_panel *panel)
{
	return container_of(panel, struct panel_comp, panel);
}

/* The follow functions are defined for panel driver */

/* Enable the display interface */
void panel_di_enable(struct aic_panel *panel, u32 ms)
{
	if (panel && panel->callbacks.di_enable)
		panel->callbacks.di_enable();

	if (ms)
		aic_delay_ms(ms);
}

/* Disable the display interface */
void panel_di_disable(struct aic_panel *panel, u32 ms)
{
	if (panel && panel->callbacks.di_disable)
		panel->callbacks.di_disable();

	if (ms)
		aic_delay_ms(ms);
}

void panel_de_timing_enable(struct aic_panel *panel, u32 ms)
{
	if (panel && panel->callbacks.timing_enable)
		panel->callbacks.timing_enable(true);

	if (ms)
		aic_delay_ms(ms);
}

void panel_de_timing_disable(struct aic_panel *panel, u32 ms)
{
	if (panel && panel->callbacks.timing_disable)
		panel->callbacks.timing_disable();

	if (ms)
		aic_delay_ms(ms);
}

void panel_backlight_enable(struct aic_panel *panel, u32 ms)
{
	struct panel_comp *p = to_panel_comp(panel);
	struct backlight_device *bd = p->backlight;

	if (bd)
		backlight_enable(bd);

	if (p->enable_gpio)
		gpiod_direction_output(p->enable_gpio, 1);

	if (ms)
		aic_delay_ms(ms);
}

void panel_backlight_disable(struct aic_panel *panel, u32 ms)
{
	struct panel_comp *p = to_panel_comp(panel);
	struct backlight_device *bd = p->backlight;

	if (bd)
		backlight_disable(bd);

	if (p->enable_gpio)
		gpiod_direction_output(p->enable_gpio, 0);

	if (ms)
		aic_delay_ms(ms);
}

int panel_default_unprepare(struct aic_panel *panel)
{
	struct panel_comp *p = to_panel_comp(panel);

	if (p->sleep_gpio)
		gpiod_set_value_cansleep(p->sleep_gpio, 0);

	if (p->supply)
		regulator_disable(p->supply);

	return 0;
}

int panel_default_prepare(struct aic_panel *panel)
{
	struct panel_comp *p = to_panel_comp(panel);
	int ret = 0;

	if (p->supply) {
		ret = regulator_enable(p->supply);
		if (ret < 0) {
			pr_err("%s() - Failed to enable panel supply: %d\n",
				__func__, ret);
			return ret;
		}
	}

	if (p->sleep_gpio)
		gpiod_direction_output(p->sleep_gpio, 1);

	// TODO: delay prepare
	return ret;
}

int panel_default_enable(struct aic_panel *panel)
{
	panel_di_enable(panel, 0);
	panel_de_timing_enable(panel, 40);
	panel_backlight_enable(panel, 0);
	return 0;
}

int panel_default_disable(struct aic_panel *panel)
{
	panel_backlight_disable(panel, 0);
	panel_di_disable(panel, 0);
	panel_de_timing_disable(panel, 0);
	return 0;
}

int panel_default_get_video_mode(struct aic_panel *panel, struct videomode **vm)
{
	int switch_gpio;
	struct panel_comp *p = to_panel_comp(panel);

	p->gpio_switch = devm_gpiod_get(p->panel.dev, "switch", GPIOD_IN);
	if (IS_ERR(p->gpio_switch))
		dev_warn(panel->dev, "Faild to get switch io\r\n");

	switch_gpio = gpiod_get_value(p->gpio_switch);

	if (switch_gpio < 0)
		switch_gpio = 0;

	if (p->use_dt_timing)
		videomode_from_timings(p->timings, panel->vm,
				switch_gpio);

	*vm = panel->vm;

	return 0;
}

int panel_register_callback(struct aic_panel *panel,
				struct aic_panel_callbacks *pcallback)
{
	panel->callbacks.di_enable = pcallback->di_enable;
	panel->callbacks.di_disable = pcallback->di_disable;
	panel->callbacks.di_send_cmd = pcallback->di_send_cmd;
	panel->callbacks.di_set_videomode = pcallback->di_set_videomode;
	panel->callbacks.timing_enable = pcallback->timing_enable;
	panel->callbacks.timing_disable = pcallback->timing_disable;
	return 0;
}

int panel_parse_dts(struct panel_comp *p, struct device *dev)
{
	int ret;

	p->supply = devm_regulator_get_optional(dev, "power");
	if (IS_ERR(p->supply)) {
		dev_dbg(dev, "failed to request panel regulator");
		p->supply = NULL;
	}

	p->enable_gpio = devm_gpiod_get_optional(dev, "enable", GPIOD_ASIS);
	if (IS_ERR(p->enable_gpio)) {
		dev_warn(dev, "failed to request enable_gpio: %ld\n",
			PTR_ERR(p->enable_gpio));
		p->enable_gpio = NULL;
	}

	p->sleep_gpio = devm_gpiod_get(dev, "sleep", GPIOD_OUT_HIGH);
	if (IS_ERR(p->sleep_gpio)) {
		dev_warn(dev, "failed to request sleep_gpio: %ld\n",
			PTR_ERR(p->sleep_gpio));
		p->sleep_gpio = NULL;
	}

	p->backlight = devm_of_find_backlight(dev);
	if (IS_ERR(p->backlight))
		return PTR_ERR(p->backlight);

	p->timings = of_get_display_timings(dev->of_node);
	if (p->timings)
		p->use_dt_timing = true;
	else
		p->use_dt_timing = false;

	ret = of_property_read_u32(dev->of_node, "disp-dither",
					&p->panel.disp_dither);
	if (ret)
		p->panel.disp_dither = 0;

	ret = of_property_read_u32(dev->of_node, "tearing-effect",
				&p->panel.te.mode);
	if (ret) {
		p->panel.te.mode = 0;
		p->panel.te.pulse_width = 0;
	} else {
		ret = of_property_read_u32(dev->of_node, "te-pulse-width",
				&p->panel.te.pulse_width);
		if (ret) {
			dev_err(dev,
			"Can't parse tearing effect signal pulse width\n");
			return ret;
		}
	}

	return 0;
}

void panel_default_unbind(struct device *dev, struct device *master,
			  void *data)
{
	struct panel_comp *p = dev_get_drvdata(dev);

	panel_default_disable(&p->panel);
	panel_default_unprepare(&p->panel);

	if (p->backlight)
		put_device(&p->backlight->dev);
}

void panel_send_command(u8 *para_cmd, u32 size, struct aic_panel *panel)
{
	u8 *p;
	u8 num, code;

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
