/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#ifndef _AICMAC_DMA_H_
#define _AICMAC_DMA_H_

#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/platform_device.h>

#define DEFAULT_DMA_PBL		8
#define SF_DMA_MODE		1	/* DMA STORE-AND-FORWARD */

#define AICMAC_RX_COE_NONE	0	/*ChecksuM disabled*/
#define AICMAC_RX_COE_TYPE1	1	/* Checksum Offload IP Header*/
#define AICMAC_RX_COE_TYPE2	2	/* Checksum Offload Full*/

#define DMA_MIN_TX_SIZE		64
#define DMA_MAX_TX_SIZE		1024
#define DMA_DEFAULT_TX_SIZE	512
#define DMA_MIN_RX_SIZE		64
#define DMA_MAX_RX_SIZE		1024
#define DMA_DEFAULT_RX_SIZE	512
#define DMA_TX_SIZE		DMA_DEFAULT_TX_SIZE
#define DMA_RX_SIZE		DMA_DEFAULT_RX_SIZE
#define AICMAC_TX_THRESH	(DMA_TX_SIZE / 4)
#define AICMAC_RX_THRESH	(DMA_RX_SIZE / 4)

#define AICMAC_GET_ENTRY(x, size)	((x + 1) & (size - 1))

#define AICMAC_TBS_AVAIL	BIT(0)
#define AICMAC_TBS_EN		BIT(1)

#define AXI_BLEN        7

struct aicmac_axi {
	bool axi_lpi_en;
	bool axi_xit_frm;
	u32 axi_wr_osr_lmt;
	u32 axi_rd_osr_lmt;
	bool axi_kbbe;
	u32 axi_blen[AXI_BLEN];
	bool axi_fb;
	bool axi_mb;
	bool axi_rb;
};

struct aicmac_dma_data {
	int pbl;
	int txpbl;
	int rxpbl;
	bool pblx8;
	int fixed_burst;
	int mixed_burst;
	bool aal;
	unsigned int tx_coe;
	unsigned int rx_coe;
	unsigned int dma_buf_sz;

	int force_sf_dma_mode;
	int force_thresh_dma_mode;

	unsigned int tx_fifo_size;
	unsigned int rx_fifo_size;
};

struct aicmac_dma_data *aicmac_dma_init_data(struct platform_device *pdev,
					     struct device_node *np);
int aicmac_dma_init(void *priv_ptr);
int aicmac_dma_alloc_dma_desc_resources(void *priv_ptr);
int aicmac_dma_init_desc_rings(struct net_device *dev, gfp_t flags);
void aicmac_dma_free_desc_resources(void *priv_ptr);
int aicmac_dma_init_engine(void *priv_ptr);
void aicmac_dma_operation_mode(void *priv_ptr);
void aicmac_dma_start_all_dma(void *priv_ptr);
void aicmac_dma_stop_all_dma(void *priv_ptr);
void aicmac_dma_set_operation_mode(void *priv_ptr, u32 txmode, u32 rxmode,
				   u32 chan);
void aicmac_dma_free_tx_skbufs(void *priv_ptr, u32 queue);
#endif
