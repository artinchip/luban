/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#ifndef _AICMAC_UTIL_H_
#define _AICMAC_UTIL_H_

#include <linux/phy.h>

#define AICMAC_GMAC_REGS_NUM 128

void aicmac_print_mac_addr(unsigned char *macaddr);
void aicmac_reg_dump_regs(void __iomem *ioaddr, u32 *reg_space);
void aicmac_print_reg(char *name, void __iomem *ioaddr, int len);
void aicmac_print_buf(unsigned char *buf, int len);
void aicmac_display_desc(char *name, void *head);
void aicmac_display_ex_desc(char *name, void *head);
void aicmac_display_all_rings(char *name, void *priv_ptr, bool tx);
void aicmac_display_one_ring(void *head, unsigned int size,
			     dma_addr_t dma_rx_phy, unsigned int desc_size);
int aicmac_interface_to_speed(phy_interface_t interface);
#endif
