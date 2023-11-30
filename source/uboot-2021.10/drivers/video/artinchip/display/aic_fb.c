// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2021 ArtInChip Technology Co.,Ltd
 * Huahui Mai <huahui.mai@artinchip.com>
 */

#include <common.h>
#include <dm.h>
#include <dm/uclass-internal.h>
#include <dm/device_compat.h>
#include <display.h>
#include <fdtdec.h>
#include <video.h>
#include <linux/fb.h>
#include <asm/types.h>
#include <cpu_func.h>
#include "aic_com.h"

DECLARE_GLOBAL_DATA_PTR;

#define AICFB_ON	1
#define AICFB_OFF	0

struct aicfb_info {
	phys_addr_t fb_start;

	struct udevice *de_dev;
	struct udevice *di_dev;
	struct udevice *panel_dev;
	struct de_funcs *de;
	struct di_funcs *di;
	struct aic_panel *panel;
};

struct aicfb_format aicfb_format_lists[] = {
	{"a8r8g8b8", 32, {16, 8}, {8, 8}, {0, 8}, {24, 8}, AIC_FMT_ARGB_8888},
	{"a8b8g8r8", 32, {0, 8}, {8, 8}, {16, 8}, {24, 8}, AIC_FMT_ABGR_8888},
	{"x8r8g8b8", 32, {16, 8}, {8, 8}, {0, 8}, {0, 0}, AIC_FMT_XRGB_8888},
	{"r8g8b8", 24, {16, 8}, {8, 8}, {0, 8}, {0, 0}, AIC_FMT_RGB_888},
	{"r5g6b5", 16, {11, 5}, {5, 6}, {0, 5}, {0, 0}, AIC_FMT_RGB_565},
	{"a1r5g5b5", 16, {10, 5}, {5, 5}, {0, 5}, {15, 1}, AIC_FMT_ARGB_1555},
};

static int get_dev_by_port(ofnode node, enum uclass_id id,
							struct udevice **devp)
{
	ofnode endpoint, remote;
	u32 remote_phandle;
	struct udevice *dev;
	int ret;

	endpoint = ofnode_first_subnode(node);

	ret = ofnode_read_u32(endpoint, "remote-endpoint", &remote_phandle);
	if (ret)
		return ret;

	remote = ofnode_get_by_phandle(remote_phandle);
	if (!ofnode_valid(remote))
		return -EINVAL;

	while (ofnode_valid(remote)) {
		remote = ofnode_get_parent(remote);
		if (!ofnode_valid(remote)) {
			debug("%s: no UCLASS for remote-endpoint\n", __func__);
			return -EINVAL;
		}

		uclass_get_device_by_ofnode(id, remote, &dev);
		if (dev) {
			*devp = dev;
			debug("Found device: %s\n", dev_read_name(dev));
			break;
		}
	};
	return 0;
}

static int aicfb_find_de(struct udevice *dev)
{
	struct aicfb_dt  *plat = dev_get_plat(dev);
	struct aicfb_info *priv = dev_get_priv(dev);
	ofnode port;
	int ret;

	port = ofnode_find_subnode(plat->fb_np, "port");
	if (!ofnode_valid(port)) {
		debug("%s(%s): Failed to find port subnode\n",
		       __func__, dev_read_name(dev));
	}

	ret = get_dev_by_port(port, UCLASS_DISPLAY, &priv->de_dev);
	if (ret) {
		debug("Failed to find de udevice\n");
		return ret;
	}

	priv->de = dev_get_priv(priv->de_dev);
	return 0;
}

static int aicfb_find_di(struct udevice *dev)
{
	struct aicfb_info *priv = dev_get_priv(dev);
	ofnode port;
	int ret;

	port = dev_read_subnode(priv->de_dev, "port@1");

	ret = get_dev_by_port(port, UCLASS_DISPLAY, &priv->di_dev);
	if (ret) {
		debug("Failed to find di udevice\n");
		return ret;
	}

	priv->di = dev_get_priv(priv->di_dev);
	return 0;
}

static int aicfb_find_panel(struct udevice *dev)
{
	struct aicfb_info *priv = dev_get_priv(dev);
	ofnode port;
	int ret;

	port = dev_read_subnode(priv->di_dev, "port@1");

	ret = get_dev_by_port(port, UCLASS_PANEL, &priv->panel_dev);
	if (ret) {
		debug("Failed to find panel udevice\n");
		return ret;
	}

	priv->panel = dev_get_priv(priv->panel_dev);
	return 0;
}

static void aicfb_video_mode_to_dt(struct udevice *dev, struct fb_videomode *vm)
{
	struct aicfb_dt *dt = dev_get_plat(dev);
	struct aicfb_format *f = dt->format;

	if (!dt->width)
		dt->width = vm->xres;

	if (!dt->height)
		dt->height = vm->yres;

	if (!dt->stride)
		dt->stride = ALIGN_8B(dt->width * f->bits_per_pixel / 8);
}

static void aicfb_get_panel_info(struct aicfb_info *priv,
		struct fb_videomode *vm)
{
	struct de_funcs *de = priv->de;
	struct di_funcs *di = priv->di;
	struct aic_panel *panel = priv->panel;

	panel->funcs->get_video_mode(panel, vm);
	de->set_mode(panel, vm);
	di->attach_panel(panel);
}

static void aicfb_register_panel_callback(struct aicfb_info *priv)
{
	struct aic_panel *p = priv->panel;
	struct aic_panel_callbacks cb;

	cb.di_enable = priv->di->enable;
	cb.di_send_cmd = priv->di->send_cmd;
	cb.di_set_videomode = priv->di->set_videomode;
	cb.timing_enable = priv->de->timing_enable;
	p->funcs->register_callback(priv->panel, &cb);
}

static int aicfb_parse_dt(struct udevice *dev)
{
	struct aicfb_dt *dt = dev_get_plat(dev);
	const char *format;
	int i;

	dt->fb_np = dev_read_subnode(dev, "fb");
	if (!ofnode_valid(dt->fb_np)) {
		debug("%s(%s): 'fb0' subnode not found\n",
		      __func__, dev_read_name(dev));
		return -EINVAL;
	}

	if (ofnode_read_u32(dt->fb_np, "width", &dt->width))
		dt->width = 0;

	if (ofnode_read_u32(dt->fb_np, "height", &dt->height))
		dt->height = 0;

	if (ofnode_read_u32(dt->fb_np, "stride", &dt->stride))
		dt->stride = 0;

	dt->format = NULL;
	format = ofnode_read_string(dt->fb_np, "format");
	if (!format) {
		dt->format = &aicfb_format_lists[0];
	} else {
		for (i = 0; i < ARRAY_SIZE(aicfb_format_lists); i++) {
			if (strcmp(format, aicfb_format_lists[i].name))
				continue;
			dt->format = &aicfb_format_lists[i];
			break;
		}
	}
	return 0;
}

static void aicfb_enable_panel(struct udevice *dev, u32 on)
{
	struct aicfb_info *priv = dev_get_priv(dev);
	struct aic_panel *p = priv->panel;

	p->funcs->prepare(p);
	p->funcs->enable(p);
}

static void aicfb_enable_clk(struct aicfb_info *priv, u32 on)
{
	ulong pixclk;
	struct de_funcs *de = priv->de;
	struct di_funcs *di = priv->di;

	de->clk_enable();
	pixclk = de->pixclk_rate();
	di->pixclk2mclk(pixclk);
	di->clk_enable();
	de->pixclk_enable();
}

static void aicfb_update_layer(struct udevice *dev)
{
	struct video_priv *uc_priv = dev_get_uclass_priv(dev);
	struct aicfb_info *priv = dev_get_priv(dev);
	struct aicfb_dt *dt = dev_get_plat(dev);
	struct aicfb_layer_data layer = {0};
	struct de_funcs *de = priv->de;

	layer.layer_id = AICFB_LAYER_TYPE_UI;
	layer.rect_id = 0;
	layer.enable = 1;
	layer.buf.phy_addr[0] = (uintptr_t)uc_priv->fb;
	layer.buf.size.width = dt->width;
	layer.buf.size.height = dt->height;
	layer.buf.stride[0] = dt->stride;
	layer.buf.format = dt->format->format;
	de->update_layer_config(&layer);
}

static void aicfb_video_init(struct udevice *dev)
{
	struct aicfb_dt *dt = dev_get_plat(dev);
	struct video_priv *uc_priv = dev_get_uclass_priv(dev);

	enum video_log2_bpp l2bpp;

	switch (dt->format->format) {
	case AIC_FMT_ARGB_8888:
		l2bpp = VIDEO_BPP32;
		break;
	case AIC_FMT_RGB_565:
		l2bpp = VIDEO_BPP16;
		break;
	default:
		l2bpp = VIDEO_BPP8;
	}

	uc_priv->xsize = dt->width;
	uc_priv->ysize = dt->height;
	uc_priv->line_length = dt->stride;
	uc_priv->bpix = l2bpp;

	video_set_flush_dcache(dev, true);
#ifdef CONFIG_SPL_BUILD
	struct aicfb_info *priv = dev_get_priv(dev);
	uc_priv->fb = (void *)priv->fb_start;
	uc_priv->fb_size = dt->stride * dt->height;

	memset(uc_priv->fb, 0xFF, uc_priv->fb_size);
	flush_dcache_range(priv->fb_start, priv->fb_start + uc_priv->fb_size);
#endif
}

static int aicfb_probe(struct udevice *dev)
{
	struct aicfb_info *priv = dev_get_priv(dev);
	struct video_uc_plat *plat = dev_get_uclass_plat(dev);
	struct fb_videomode vm;

#ifndef CONFIG_SPL_BUILD
	/* Before relocation we don't need to do anything */
	if (!(gd->flags & GD_FLG_RELOC))
		return 0;

	priv->fb_start = plat->base;
	debug("%s() fb_start: fb_base 0x%lx, size 0x%x\n",
	       __func__, plat->base, plat->size);
#else
	int parent, node, ret;
	struct fdt_resource reg_res;
	void *blob = (void *)CONFIG_SYS_SPL_ARGS_ADDR;

	parent = fdt_path_offset(blob, "/reserved-memory");
	if (parent < 0) {
		pr_err("not find reserved-memory\n");
		return parent;
	}

	fdt_for_each_subnode(node, blob, parent) {
		const char *name = fdt_get_name(blob, node, NULL);

		if (strncmp(name, "aic-logo", 8) == 0) {
			ret = fdt_get_resource(blob, node, "reg", 0, &reg_res);

			priv->fb_start = reg_res.start;
			plat->base = reg_res.start;
			break;
		}
	}
#endif

	/* Find the component device, and turn on */
	if (aicfb_find_de(dev)) {
		dev_err(dev, "Failed to find display engine\n");
		return -EINVAL;
	}

	if (aicfb_find_di(dev)) {
		dev_err(dev, "Failed to find display interface\n");
		return -EINVAL;
	}

	if (aicfb_find_panel(dev)) {
		dev_err(dev, "Failed to find panel\n");
		return -EINVAL;
	}

	aicfb_get_panel_info(priv, &vm);
	aicfb_video_mode_to_dt(dev, &vm);
	aicfb_register_panel_callback(priv);

	aicfb_enable_clk(priv, AICFB_ON);

	aicfb_video_init(dev);

	return 0;
}

static int aicfb_ofdata_to_platdata(struct udevice *dev)
{
	int ret;

	ret = aicfb_parse_dt(dev);
	if (ret) {
		debug("parse_dt failed %s(%s)\n", __func__, dev_read_name(dev));
		return ret;
	}
	return 0;
}

static int aicfb_bind(struct udevice *dev)
{
	struct video_uc_plat *plat = dev_get_uclass_plat(dev);

	plat->size = CONFIG_VIDEO_ARTINCHIP_MAX_XRES *
		CONFIG_VIDEO_ARTINCHIP_MAX_YRES *
		(CONFIG_VIDEO_ARTINCHIP_MAX_BPP >> 3);

	return 0;
}

struct aicfb_ops fb_ops = {
	.update_layer = aicfb_update_layer,
	.enable_panel = aicfb_enable_panel,
};

static const struct udevice_id aicfb_match_ids[] = {
	{ .compatible = "artinchip,aic-framebuffer",
	  .data = (ulong)&fb_ops },
	{ /* sentinel */ }
};

U_BOOT_DRIVER(aicfb) = {
	.name               = "aicfb",
	.id                 = UCLASS_VIDEO,
	.of_match           = aicfb_match_ids,
	.bind               = aicfb_bind,
	.probe              = aicfb_probe,
	.flags              = DM_FLAG_PRE_RELOC,
	.of_to_plat         = aicfb_ofdata_to_platdata,
	.priv_auto          = sizeof(struct aicfb_info),
	.plat_auto          = sizeof(struct aicfb_dt),
};
