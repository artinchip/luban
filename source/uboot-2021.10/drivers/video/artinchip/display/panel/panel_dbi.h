/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2023 ArtInChip Technology Co., Ltd.
 * Authors:  Huahui Mai <huahui.mai@artinchip.com>
 */

#ifndef _PANEL_DBI_H_
#define _PANEL_DBI_H_

#include "panel_com.h"

int panel_dbi_default_enable(struct aic_panel *panel);

int panel_dbi_commands_execute(struct aic_panel *panel,
				struct panel_dbi_commands *commands);

#endif /* _PANEL_DBI_H_ */

