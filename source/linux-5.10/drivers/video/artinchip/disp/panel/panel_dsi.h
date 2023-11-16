/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2023 ArtInChip Technology Co., Ltd.
 * Authors:  Huahui Mai <huahui.mai@artinchip.com>
 */

#ifndef _PANEL_DSI_H_
#define _PANEL_DSI_H_

#include <video/mipi_display.h>
#include "panel_com.h"

int mipi_dsi_generic_send(struct aic_panel *panel, const u8 *data, size_t size);
int mipi_dsi_dcs_send(struct aic_panel *panel, const u8 *data, size_t size);

#define panel_dsi_generic_send_seq(panel, seq...) do {			    \
		static const u8 d[] = { seq };				    \
		int ret;						    \
		ret = mipi_dsi_generic_send(panel, d, ARRAY_SIZE(d));	    \
		if (ret < 0)						    \
			return ret;					    \
	} while (0)

#define panel_dsi_dcs_send_seq(panel, seq...) do {			    \
		static const u8 d[] = { seq };				    \
		int ret;						    \
		ret = mipi_dsi_dcs_send(panel, d, ARRAY_SIZE(d));	    \
		if (ret < 0)						    \
			return ret;					    \
	} while (0)

int panel_dsi_dcs_set_display_on(struct aic_panel *panel);
int panel_dsi_dcs_set_display_off(struct aic_panel *panel);

int panel_dsi_dcs_exit_sleep_mode(struct aic_panel *panel);
int panel_dsi_dcs_enter_sleep_mode(struct aic_panel *panel);

void panel_dsi_send_perpare(struct aic_panel *panel);
void panel_dsi_setup_realmode(struct aic_panel *panel);
int panel_dsi_str2fmt(const s8 *str);
int panel_dsi_str2mode(const s8 *str);

#endif /* _PANEL_DSI_H_ */
