/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#ifndef _AICMAC_MAC_H_
#define _AICMAC_MAC_H_

#include <linux/platform_device.h>

#include "aicmac.h"

struct aicmac_tc_entry {
	bool in_use;
	bool in_hw;
	bool is_last;
	bool is_frag;
	void *frag_ptr;
	unsigned int table_pos;
	u32 handle;
	u32 prio;
	struct {
		u32 match_data;
		u32 match_en;
		u8 af:1;
		u8 rf:1;
		u8 im:1;
		u8 nc:1;
		u8 res1:4;
		u8 frame_offset;
		u8 ok_index;
		u8 dma_ch_no;
		u32 res2;
	} __packed val;
};

struct aicmac_flow_entry {
	unsigned long cookie;
	unsigned long action;
	u8 ip_proto;
	int in_use;
	int idx;
	int is_l4;
};

struct mac_link {
	u32 speed_mask;
	u32 speed10;
	u32 speed100;
	u32 speed1000;
	u32 speed2500;
	u32 duplex;
	struct {
		u32 speed2500;
		u32 speed5000;
		u32 speed10000;
	} xgmii;
};

/* Physical Coding Sublayer */
struct rgmii_adv {
	unsigned int pause;
	unsigned int duplex;
	unsigned int lp_pause;
	unsigned int lp_duplex;
};

struct aicmac_mac_data {
	void __iomem *pcsr;
	phy_interface_t mac_interface;
	int max_speed;
	int maxmtu;
	int riwt_off;
	int bugged_jumbo;
	int mac_port_sel_speed;
	int en_tx_lpi_clockgating;

	unsigned int multicast_filter_bins;
	unsigned int unicast_filter_entries;
	unsigned int mcast_bits_log2;
	unsigned int pcs;
	unsigned int pmt;
	unsigned int ps;

	struct mac_link link;
	const struct phylink_mac_ops *phylink_mac_ops;
};

struct aicmac_mac_data *aicmac_mac_init_data(struct platform_device *pdev,
					     struct device_node *np);
int aicmac_mac_init(void *priv_ptr);

#endif
