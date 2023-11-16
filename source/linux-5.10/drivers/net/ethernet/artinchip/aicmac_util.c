// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/phy.h>
#include <linux/ethtool.h>

#include "aicmac_mac.h"
#include "aicmac_dma_desc.h"
#include "aicmac_util.h"

int aicmac_interface_to_speed(phy_interface_t interface)
{
	switch (interface) {
	case PHY_INTERFACE_MODE_MII:
	case PHY_INTERFACE_MODE_RMII:
		return SPEED_100;
	case PHY_INTERFACE_MODE_GMII:
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_ID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_TXID:
		return SPEED_1000;
	default:
		return SPEED_100;
	}
}

void aicmac_print_mac_addr(unsigned char *addr)
{
	char addrarry[MAX_ADDR_LEN * 2];

	if (IS_ERR_OR_NULL(addr)) {
		pr_info("mac addr: NULL\n");
		return;
	}
	sprintf(addrarry, "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x", *addr, *(addr + 1),
		*(addr + 2), *(addr + 3), *(addr + 4), *(addr + 5));
	pr_info("mac addr: %s\n", addrarry);
}

void aicmac_reg_dump_regs(void __iomem *ioaddr, u32 *reg_space)
{
	int i;

	for (i = 0; i < AICMAC_GMAC_REGS_NUM; i++)
		reg_space[i] = readl(ioaddr + i * 4);
}

void aicmac_print_reg(char *name, void __iomem *ioaddr, int len)
{
	int i;

	pr_debug("---- dump %s registers:----\n", name);
	for (i = 0; i < len; i = i + 16)
		pr_debug("0x%p : 0x%08x  0x%08x  0x%08x  0x%08x\n",
			(ioaddr + i), readl(ioaddr + i), readl(ioaddr + i + 4),
			readl(ioaddr + i + 8), readl(ioaddr + i + 12));
}

void aicmac_print_buf(unsigned char *buf, int len)
{
	int i;
	u8 *ptr = buf;

	pr_debug("len = %d byte, buf addr: 0x%p\n", len, buf);

	for (i = 0; i < len; i = i + 8)
		pr_debug("0x%p :%02x %02x %02x %02x %02x %02x %02x %02x\n",
			(ptr + i), *(ptr + i), *(ptr + i + 1), *(ptr + i + 2),
			*(ptr + i + 3), *(ptr + i + 4), *(ptr + i + 5),
			*(ptr + i + 6), *(ptr + i + 7));
}

void aicmac_print_desc(char *name, __le32 des0, __le32 des1, __le32 des2,
		       __le32 des3)
{
	pr_debug("---- dump %s descriptor:----\n", name);
	pr_debug("0x%08x  0x%08x  0x%08x  0x%08x\n", le32_to_cpu(des0),
		le32_to_cpu(des1), le32_to_cpu(des2), le32_to_cpu(des3));
}

void aicmac_display_one_ring(void *head, unsigned int size,
			     dma_addr_t dma_rx_phy, unsigned int desc_size)
{
	struct dma_extended_desc *ep = (struct dma_extended_desc *)head;
	dma_addr_t dma_addr;
	int i;

	for (i = 0; i < 16; i++) {
		dma_addr = dma_rx_phy + i * sizeof(*ep);

		pr_debug("%03d [%p]: 0x%08x 0x%08x 0x%08x 0x%08x\n", i,
			(void *)dma_addr, le32_to_cpu(ep->basic.des0),
			le32_to_cpu(ep->basic.des1),
			le32_to_cpu(ep->basic.des2),
			le32_to_cpu(ep->basic.des3));
		ep++;
	}
}

void aicmac_display_all_rings(char *name, void *priv_ptr, bool tx)
{
	struct aicmac_priv *priv = priv_ptr;
	struct aicmac_rx_queue *rx_q;
	struct aicmac_tx_queue *tx_q;
	unsigned int desc_size;
	void *head_rx, *head_tx;

	if (!tx) {
		rx_q = &priv->plat->rx_queue[0];
		head_rx = (void *)rx_q->dma_erx;
		desc_size = sizeof(struct dma_extended_desc);

		pr_debug(" ----- %s rx descriptor cur_rx = %d ----\n", name,
			rx_q->cur_rx);
		aicmac_display_one_ring(head_rx, DMA_TX_SIZE, rx_q->dma_rx_phy,
					desc_size);
	} else {
		tx_q = &priv->plat->tx_queue[0];
		head_tx = (void *)tx_q->dma_etx;
		desc_size = sizeof(struct dma_extended_desc);

		pr_debug(" ----- %s tx descriptor cur_tx = %d ----\n", name,
			tx_q->cur_tx);
		aicmac_display_one_ring(head_tx, DMA_TX_SIZE, tx_q->dma_tx_phy,
					desc_size);
	}
}

void aicmac_display_desc(char *name, void *head)
{
	struct dma_desc *ep = (struct dma_desc *)head;

	pr_debug("---- dump %s descriptor:----\n", name);
	pr_debug("%s [0x%p]: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n", name, ep,
		le32_to_cpu(ep->des0), le32_to_cpu(ep->des1),
		le32_to_cpu(ep->des2), le32_to_cpu(ep->des3));
}

void aicmac_display_ex_desc(char *name, void *head)
{
	struct dma_extended_desc *ep = (struct dma_extended_desc *)head;

	pr_debug("---- dump %s descriptor:----\n", name);
	pr_debug("%s [0x%p]: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n", name, ep,
		le32_to_cpu(ep->basic.des0), le32_to_cpu(ep->basic.des1),
		le32_to_cpu(ep->basic.des2), le32_to_cpu(ep->basic.des3));
}
