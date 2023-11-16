// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023 ArtInChip Technology Co., Ltd.
 * Authors:  Huahui Mai <huahui.mai@artinchip.com>
 */

#include "panel_dsi.h"

int mipi_dsi_generic_send(struct aic_panel *panel, const u8 *data, size_t size)
{
	u8 type;

	switch (size) {
	case 0:
		type = MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM;
		break;

	case 1:
		type = MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM;
		break;

	case 2:
		type = MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM;
		break;

	default:
		type = MIPI_DSI_GENERIC_LONG_WRITE;
		break;
	}

	return panel->callbacks.di_send_cmd((u32)type, data, size);
}

int mipi_dsi_dcs_send(struct aic_panel *panel, const u8 *data, size_t size)
{
	u8 type;

	switch (size) {
	case 0:
		return -EINVAL;

	case 1:
		type = MIPI_DSI_DCS_SHORT_WRITE;
		break;

	case 2:
		type = MIPI_DSI_DCS_SHORT_WRITE_PARAM;
		break;

	default:
		type = MIPI_DSI_DCS_LONG_WRITE;
		break;
	}

	return panel->callbacks.di_send_cmd((u32)type, data, size);
}

int panel_dsi_dcs_set_display_on(struct aic_panel *panel)
{
	ssize_t err;

	err = mipi_dsi_dcs_send(panel, (u8[]){ MIPI_DCS_SET_DISPLAY_ON }, 1);
	if (err < 0)
		return err;

	return 0;
}

int panel_dsi_dcs_set_display_off(struct aic_panel *panel)
{
	ssize_t err;

	err = mipi_dsi_dcs_send(panel, (u8[]){ MIPI_DCS_SET_DISPLAY_OFF }, 1);
	if (err < 0)
		return err;

	return 0;
}

int panel_dsi_dcs_exit_sleep_mode(struct aic_panel *panel)
{
	ssize_t err;

	err = mipi_dsi_dcs_send(panel, (u8[]){ MIPI_DCS_EXIT_SLEEP_MODE }, 1);
	if (err < 0)
		return err;

	return 0;
}

int panel_dsi_dcs_enter_sleep_mode(struct aic_panel *panel)
{
	ssize_t err;

	err = mipi_dsi_dcs_send(panel, (u8[]){ MIPI_DCS_ENTER_SLEEP_MODE }, 1);
	if (err < 0)
		return err;

	return 0;
}

/* Set mipi dsi to command mode, ready to send command */
void panel_dsi_send_perpare(struct aic_panel *panel)
{
	if (panel && panel->callbacks.di_set_videomode)
		panel->callbacks.di_set_videomode(panel->vm, false);
}

/* Set mipi dsi to its real mode */
void panel_dsi_setup_realmode(struct aic_panel *panel)
{
	if (panel && panel->callbacks.di_set_videomode)
		panel->callbacks.di_set_videomode(panel->vm, true);
}

int panel_dsi_str2fmt(const s8 *str)
{
	s32 i;
	s8 *format[] = {"rgb888", "rgb666l", "rgb666", "rgb565"};

	for (i = 0; i < ARRAY_SIZE(format); i++)
		if (strncasecmp(str, format[i], strlen(format[i])) == 0)
			return i;

	pr_err("Invalid format: %s", str);
	return DSI_FMT_RGB888;
}

int panel_dsi_str2mode(const s8 *str)
{
	s32 i;
	s8 *mode[] = {"video-pulse", "video-event", "video-burst",
			"command-mode"};

	for (i = 0; i < ARRAY_SIZE(mode); i++)
		if (strncasecmp(str, mode[i], strlen(mode[i])) == 0)
			return i;

	pr_err("Invalid mode: %s", str);
	return DSI_MOD_VID_BURST;
}
