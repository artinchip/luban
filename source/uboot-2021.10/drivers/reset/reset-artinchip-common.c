// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd
 * Dehuang Wu <dehuang.wu@artinchip.com>
 */

#include <common.h>
#include <mapmem.h>
#include <dm.h>
#include <errno.h>
#include <reset-uclass.h>
#include <dt-structs.h>
#include <asm/io.h>
#include <dm/lists.h>
#include <linux/log2.h>
#include "reset-artinchip-common.h"
#ifdef CONFIG_RESET_ARTINCHIP_V2_0
#include <dt-bindings/reset/artinchip,aic-reset-v20.h>
#elif CONFIG_RESET_ARTINCHIP_V3_0
#include <dt-bindings/reset/artinchip,aic-reset-v30.h>
#else
#include <dt-bindings/reset/artinchip,aic-reset.h>
#endif


static int _get_reset_index(struct artinchip_reset_priv *priv, u32 id)
{
	int i;

	for (i = 0; i < priv->count; i++) {
		if (priv->rests[i].id == id) {
			return i;
		}
	}

	return -1;
}

int artinchip_reset_request(struct reset_ctl *reset_ctl)
{
	struct artinchip_reset_priv *priv = dev_get_priv(reset_ctl->dev);

	if (_get_reset_index(priv, reset_ctl->id) < 0)
		return -EINVAL;

	return 0;
}

int artinchip_reset_free(struct reset_ctl *reset_ctl)
{
	return 0;
}

int artinchip_set_reset(struct reset_ctl *reset_ctl, bool on)
{
	int index;
	u32 value;
	void *base;
	struct artinchip_reset *reset;
	struct artinchip_reset_priv *priv = dev_get_priv(reset_ctl->dev);

	index = _get_reset_index(priv, reset_ctl->id);
	if (index < 0) {
		return -EINVAL;
	}

#ifdef CONFIG_RESET_ARTINCHIP_V1_0
	base = priv->base;
#else
	if (reset_ctl->id < RST_CROSS_ZONE)
		base = priv->base;
	else
		base = priv->cz_base;
#endif

	reset = &priv->rests[index];
	value = readl(base + reset->reg);

	if (on)
		value |= 1 << reset->bit;
	else
		value &= ~(1 << reset->bit);
	writel(value, base + reset->reg);

	return 0;
}

int artinchip_reset_assert(struct reset_ctl *reset_ctl)
{
	return artinchip_set_reset(reset_ctl, false);
}

int artinchip_reset_deassert(struct reset_ctl *reset_ctl)
{
	return artinchip_set_reset(reset_ctl, true);
}
