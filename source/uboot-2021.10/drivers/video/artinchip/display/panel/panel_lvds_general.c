// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021 ArtInChip Technology Co.,Ltd
 * Authors: Huahui Mai <huahui.mai@artinchip.com>
 */

#include <common.h>
#include <dm.h>
#include <dm/uclass-internal.h>
#include <display.h>
#include <video.h>
#include <fdtdec.h>
#include <linux/fb.h>
#include <panel.h>

#include "panel_com.h"

#define PANEL_DEV_NAME		"panel_lvds_general"

static struct aic_panel_funcs panel_funcs = {
	.prepare = panel_default_prepare,
	.enable = panel_default_enable,
	.get_video_mode = panel_default_get_video_mode,
	.register_callback = panel_register_callback,
};

/* Init the videomode parameter, dts will override the initial value. */
static struct fb_videomode panel_vm = {
	.flag = DISPLAY_FLAGS_HSYNC_LOW | DISPLAY_FLAGS_VSYNC_LOW |
		DISPLAY_FLAGS_DE_HIGH | DISPLAY_FLAGS_PIXDATA_POSEDGE
};

static int lvds_mode_match(const char *str)
{
	if (strcmp(str, "jeida-18") == 0)
		return JEIDA_18BIT;
	else if (strcmp(str, "jeida-24") == 0)
		return JEIDA_24BIT;
	else if (strcmp(str, "vesa-24") == 0)
		return NS;

	pr_err("invalid data-mapping properety: %s", str);
	return NS;
}

static int lvds_link_mode_match(const char *str)
{
	if (strcmp(str, "single-link0") == 0)
		return SINGLE_LINK0;
	else if (strcmp(str, "single-link1") == 0)
		return SINGLE_LINK1;
	else if (strcmp(str, "double-screen") == 0)
		return DOUBLE_SCREEN;
	else if (strcmp(str, "dual-link") == 0)
		return DUAL_LINK;

	pr_err("invalid data-channel properety: %s", str);
	return SINGLE_LINK0;
}

static int panel_lvds_parse_dt(struct udevice *dev, struct panel_priv *p)
{
	ofnode node = dev_ofnode(dev);
	struct panel_lvds *lvds;
	const char *str;

	lvds = malloc(sizeof(*lvds));
	if (!lvds)
		return -ENOMEM;

	if (panel_parse_dts(dev) < 0) {
		free(lvds);
		return -1;
	}

	str = ofnode_read_string(node, "data-mapping");
	if (!str) {
		debug("Can't parse data-mapping property\n");
		free(lvds);
		return -EINVAL;
	}
	lvds->mode = lvds_mode_match(str);

	str = ofnode_read_string(node, "data-channel");
	if (!str) {
		debug("Can't parse data-mapping property\n");
		free(lvds);
		return -EINVAL;
	}
	lvds->link_mode = lvds_link_mode_match(str);

	p->panel.lvds = lvds;
	return 0;
}

static int panel_probe(struct udevice *dev)
{
	struct panel_priv *priv = dev_get_priv(dev);
	int ret;

	ret = panel_lvds_parse_dt(dev, priv);
	if (ret < 0)
		return ret;

	panel_init(priv, dev, &panel_vm, &panel_funcs, NULL);

	return 0;
}

static const struct udevice_id panel_match_ids[] = {
	{.compatible = "artinchip,aic-general-lvds-panel"},
	{ /* sentinel */}
};

U_BOOT_DRIVER(panel_lvds_general) = {
	.name      = PANEL_DEV_NAME,
	.id        = UCLASS_PANEL,
	.of_match  = panel_match_ids,
	.probe     = panel_probe,
	.priv_auto = sizeof(struct panel_priv),
};
