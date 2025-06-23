/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Mailbox driver of ArtInChip SoC Uboot
 *
 * Copyright (C) 2024 ArtInChip Technology Co., Ltd.
 * Authors:  weihui.xu <weihui.xu@artinchip.com>
 */

#ifndef __ARTINCHIP_MAILBOX_HAL_H__
#define __ARTINCHIP_MAILBOX_HAL_H__

#include <mailbox.h>
#include <mailbox-uclass.h>

#define MBOX_CH_NUM		1
#define MBOX_MAX_DAT_LEN	32

void aic_mbox_dbg_mode(void __iomem *base);
void aic_mbox_cmp_mode(void __iomem *base, u32 no_irq);

void aic_mbox_rxfifo_rst(void __iomem *base);
void aic_mbox_rxfifo_lvl(void __iomem *base, u32 level);
u32 aic_mbox_rxfifo_cnt(void __iomem *base);
void aic_mbox_txfifo_rst(void __iomem *base);
void aic_mbox_txfifo_lvl(void __iomem *base, u32 level);
u32 aic_mbox_txfifo_cnt(void __iomem *base);

u32 aic_mbox_int_sta(void __iomem *base);
void aic_mbox_int_clr(void __iomem *base, u32 sta);
u32 aic_mbox_int_sta_is_tx_cmp(u32 sta);
u32 aic_mbox_int_sta_is_rx_uf(u32 sta);
u32 aic_mbox_int_sta_is_rx_full(u32 sta);
u32 aic_mbox_int_sta_is_rx_empty(u32 sta);
u32 aic_mbox_int_sta_is_rx_cmp(u32 sta);
u32 aic_mbox_int_sta_is_tx_of(u32 sta);
u32 aic_mbox_int_sta_is_tx_full(u32 sta);
u32 aic_mbox_int_sta_is_tx_empty(u32 sta);
u32 aic_mbox_int_sta_is_cared(u32 sta);
void aic_mbox_int_en(void __iomem *base, u32 bit);
void aic_mbox_int_all_en(void __iomem *base, u32 enable);

u32 aic_mbox_wr(void __iomem *base, u32 *data, u32 cnt);
void aic_mbox_wr_cmp(void __iomem *base, u32 data);
u32 aic_mbox_rd(void __iomem *base, u32 *data, u32 cnt);
u32 aic_mbox_rd_cmp(void __iomem *base);

typedef void (*mbox_cb_t)(u32 *data, u32 len);
void hal_mbox_set_cb(mbox_cb_t cb);

struct aic_mbox {
	u32		*recv_buf;
	u32		last_tx_done;
	void __iomem	*regs;
};

struct aic_rpmsg {
	unsigned short	cmd;
	unsigned char	seq;
	unsigned char	len;	/* length of data[], unit: dword */
	unsigned int	data[];	/* length varies according to the scene */
};

#endif /* __ARTINCHIP_MAILBOX_HAL_H__ */
