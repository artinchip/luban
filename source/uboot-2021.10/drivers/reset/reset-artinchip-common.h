/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd
 * Dehuang Wu <dehuang.wu@artinchip.com>
 */

#include <common.h>
#include <errno.h>
#ifndef _RESET_ARTINCHIP_H_
#define _RESET_ARTINCHIP_H_

struct artinchip_reset {
	u8  id;
	u8  bit;
	u16 reg;
};

struct artinchip_reset_priv {
	void *base;
	void *cz_base;
	u16  count;
	u16  max_id;
	struct artinchip_reset *rests;
};

int artinchip_reset_request(struct reset_ctl *reset_ctl);
int artinchip_reset_free(struct reset_ctl *reset_ctl);
int artinchip_set_reset(struct reset_ctl *reset_ctl, bool on);
int artinchip_reset_assert(struct reset_ctl *reset_ctl);
int artinchip_reset_deassert(struct reset_ctl *reset_ctl);

#endif /* _RESET_ARTINCHIP_H_ */
