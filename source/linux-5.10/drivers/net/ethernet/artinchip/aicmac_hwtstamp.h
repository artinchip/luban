/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#ifndef __AICMAC_HWTSTAMP_H__
#define __AICMAC_HWTSTAMP_H__

void aicmac_hwtstamp_config_hw_tstamping(void __iomem *ioaddr, u32 data);
void aicmac_hwtstamp_config_sub_second_increment(void __iomem *ioaddr,
						 u32 ptp_clock, u32 *ssinc);
int aicmac_hwtstamp_init_systime(void __iomem *ioaddr, u32 sec, u32 nsec);
int aicmac_hwtstamp_config_addend(void __iomem *ioaddr, u32 addend);
int aicmac_hwtstamp_adjust_systime(void __iomem *ioaddr, u32 sec, u32 nsec,
				   int add_sub);
void aicmac_hwtstamp_get_systime(void __iomem *ioaddr, u64 *systime);
#endif
