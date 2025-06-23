/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#ifndef __AICMAC_DMA_DESC_H__
#define __AICMAC_DMA_DESC_H__

#include <linux/bitops.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include <linux/phy.h>
#include <linux/module.h>
#if IS_ENABLED(CONFIG_VLAN_8021Q)
#define AICAC_VLAN_TAG_USED
#include <linux/if_vlan.h>
#endif

#include "aicmac.h"

/* Normal receive descriptor defines */

/* RDES0 */
#define	RDES0_PAYLOAD_CSUM_ERR		BIT(0)
#define	RDES0_CRC_ERROR			BIT(1)
#define	RDES0_DRIBBLING			BIT(2)
#define	RDES0_MII_ERROR			BIT(3)
#define	RDES0_RECEIVE_WATCHDOG		BIT(4)
#define	RDES0_FRAME_TYPE		BIT(5)
#define	RDES0_COLLISION			BIT(6)
#define	RDES0_IPC_CSUM_ERROR		BIT(7)
#define	RDES0_LAST_DESCRIPTOR		BIT(8)
#define	RDES0_FIRST_DESCRIPTOR		BIT(9)
#define	RDES0_VLAN_TAG			BIT(10)
#define	RDES0_OVERFLOW_ERROR		BIT(11)
#define	RDES0_LENGTH_ERROR		BIT(12)
#define	RDES0_SA_FILTER_FAIL		BIT(13)
#define	RDES0_DESCRIPTOR_ERROR		BIT(14)
#define	RDES0_ERROR_SUMMARY		BIT(15)
#define	RDES0_FRAME_LEN_MASK		GENMASK(29, 16)
#define RDES0_FRAME_LEN_SHIFT		16
#define	RDES0_DA_FILTER_FAIL		BIT(30)
#define	RDES0_OWN			BIT(31)

/* RDES1 */
#define	RDES1_BUFFER1_SIZE_MASK		GENMASK(10, 0)
#define	RDES1_BUFFER2_SIZE_MASK		GENMASK(21, 11)
#define	RDES1_BUFFER2_SIZE_SHIFT	11
#define	RDES1_SECOND_ADDRESS_CHAINED	BIT(24)
#define	RDES1_END_RING			BIT(25)
#define	RDES1_DISABLE_IC		BIT(31)

/* Enhanced receive descriptor defines */

/* RDES0 (similar to normal RDES) */
#define	 ERDES0_RX_MAC_ADDR		BIT(0)

/* RDES1: completely differ from normal desc definitions */
#define	ERDES1_BUFFER1_SIZE_MASK	GENMASK(12, 0)
#define	ERDES1_SECOND_ADDRESS_CHAINED	BIT(14)
#define	ERDES1_END_RING			BIT(15)
#define	ERDES1_BUFFER2_SIZE_MASK	GENMASK(28, 16)
#define ERDES1_BUFFER2_SIZE_SHIFT	16
#define	ERDES1_DISABLE_IC		BIT(31)

/* Normal transmit descriptor defines */
/* TDES0 */
#define	TDES0_DEFERRED			BIT(0)
#define	TDES0_UNDERFLOW_ERROR		BIT(1)
#define	TDES0_EXCESSIVE_DEFERRAL	BIT(2)
#define	TDES0_COLLISION_COUNT_MASK	GENMASK(6, 3)
#define	TDES0_VLAN_FRAME		BIT(7)
#define	TDES0_EXCESSIVE_COLLISIONS	BIT(8)
#define	TDES0_LATE_COLLISION		BIT(9)
#define	TDES0_NO_CARRIER		BIT(10)
#define	TDES0_LOSS_CARRIER		BIT(11)
#define	TDES0_PAYLOAD_ERROR		BIT(12)
#define	TDES0_FRAME_FLUSHED		BIT(13)
#define	TDES0_JABBER_TIMEOUT		BIT(14)
#define	TDES0_ERROR_SUMMARY		BIT(15)
#define	TDES0_IP_HEADER_ERROR		BIT(16)
#define	TDES0_TIME_STAMP_STATUS		BIT(17)
#define	TDES0_OWN			((u32)BIT(31))
/* TDES1 */
#define	TDES1_BUFFER1_SIZE_MASK		GENMASK(10, 0)
#define	TDES1_BUFFER2_SIZE_MASK		GENMASK(21, 11)
#define	TDES1_BUFFER2_SIZE_SHIFT	11
#define	TDES1_TIME_STAMP_ENABLE		BIT(22)
#define	TDES1_DISABLE_PADDING		BIT(23)
#define	TDES1_SECOND_ADDRESS_CHAINED	BIT(24)
#define	TDES1_END_RING			BIT(25)
#define	TDES1_CRC_DISABLE		BIT(26)
#define	TDES1_CHECKSUM_INSERTION_MASK	GENMASK(28, 27)
#define	TDES1_CHECKSUM_INSERTION_SHIFT	27
#define	TDES1_FIRST_SEGMENT		BIT(29)
#define	TDES1_LAST_SEGMENT		BIT(30)
#define	TDES1_INTERRUPT			BIT(31)

/* Enhanced transmit descriptor defines */
/* TDES0 */
#define	ETDES0_DEFERRED			BIT(0)
#define	ETDES0_UNDERFLOW_ERROR		BIT(1)
#define	ETDES0_EXCESSIVE_DEFERRAL	BIT(2)
#define	ETDES0_COLLISION_COUNT_MASK	GENMASK(6, 3)
#define	ETDES0_VLAN_FRAME		BIT(7)
#define	ETDES0_EXCESSIVE_COLLISIONS	BIT(8)
#define	ETDES0_LATE_COLLISION		BIT(9)
#define	ETDES0_NO_CARRIER		BIT(10)
#define	ETDES0_LOSS_CARRIER		BIT(11)
#define	ETDES0_PAYLOAD_ERROR		BIT(12)
#define	ETDES0_FRAME_FLUSHED		BIT(13)
#define	ETDES0_JABBER_TIMEOUT		BIT(14)
#define	ETDES0_ERROR_SUMMARY		BIT(15)
#define	ETDES0_IP_HEADER_ERROR		BIT(16)
#define	ETDES0_TIME_STAMP_STATUS	BIT(17)
#define	ETDES0_SECOND_ADDRESS_CHAINED	BIT(20)
#define	ETDES0_END_RING			BIT(21)
#define	ETDES0_CHECKSUM_INSERTION_MASK	GENMASK(23, 22)
#define	ETDES0_CHECKSUM_INSERTION_SHIFT	22
#define	ETDES0_TIME_STAMP_ENABLE	BIT(25)
#define	ETDES0_DISABLE_PADDING		BIT(26)
#define	ETDES0_CRC_DISABLE		BIT(27)
#define	ETDES0_FIRST_SEGMENT		BIT(28)
#define	ETDES0_LAST_SEGMENT		BIT(29)
#define	ETDES0_INTERRUPT		BIT(30)
#define	ETDES0_OWN			((u32)BIT(31))
/* TDES1 */
#define	ETDES1_BUFFER1_SIZE_MASK	GENMASK(12, 0)
#define	ETDES1_BUFFER2_SIZE_MASK	GENMASK(28, 16)
#define	ETDES1_BUFFER2_SIZE_SHIFT	16

/* Extended Receive descriptor definitions */
#define	ERDES4_IP_PAYLOAD_TYPE_MASK	GENMASK(6, 2)
#define	ERDES4_IP_HDR_ERR		BIT(3)
#define	ERDES4_IP_PAYLOAD_ERR		BIT(4)
#define	ERDES4_IP_CSUM_BYPASSED		BIT(5)
#define	ERDES4_IPV4_PKT_RCVD		BIT(6)
#define	ERDES4_IPV6_PKT_RCVD		BIT(7)
#define	ERDES4_MSG_TYPE_MASK		GENMASK(11, 8)
#define	ERDES4_PTP_FRAME_TYPE		BIT(12)
#define	ERDES4_PTP_VER			BIT(13)
#define	ERDES4_TIMESTAMP_DROPPED	BIT(14)
#define	ERDES4_AV_PKT_RCVD		BIT(16)
#define	ERDES4_AV_TAGGED_PKT_RCVD	BIT(17)
#define	ERDES4_VLAN_TAG_PRI_VAL_MASK	GENMASK(20, 18)
#define	ERDES4_L3_FILTER_MATCH		BIT(24)
#define	ERDES4_L4_FILTER_MATCH		BIT(25)
#define	ERDES4_L3_L4_FILT_NO_MATCH_MASK	GENMASK(27, 26)

/* Extended RDES4 message type definitions */
#define RDES_EXT_NO_PTP			0x0
#define RDES_EXT_SYNC			0x1
#define RDES_EXT_FOLLOW_UP		0x2
#define RDES_EXT_DELAY_REQ		0x3
#define RDES_EXT_DELAY_RESP		0x4
#define RDES_EXT_PDELAY_REQ		0x5
#define RDES_EXT_PDELAY_RESP		0x6
#define RDES_EXT_PDELAY_FOLLOW_UP	0x7
#define RDES_PTP_ANNOUNCE		0x8
#define RDES_PTP_MANAGEMENT		0x9
#define RDES_PTP_SIGNALING		0xa
#define RDES_PTP_PKT_RESERVED_TYPE	0xf

/* Transmit checksum insertion control */
#define	TX_CIC_FULL			3

/* Packets types */
enum packets_types {
	PACKET_AVCPQ = 0x1, /* AV Untagged Control packets */
	PACKET_PTPQ = 0x2, /* PTP Packets */
	PACKET_DCBCPQ = 0x3, /* DCB Control Packets */
	PACKET_UPQ = 0x4, /* Untagged Packets */
	PACKET_MCBCQ = 0x5, /* Multicast & Broadcast Packets */
};

/* Rx IPC status */
enum rx_frame_status {
	good_frame = 0x0,
	discard_frame = 0x1,
	csum_none = 0x2,
	llc_snap = 0x4,
	dma_own = 0x8,
	rx_not_ls = 0x10,
};

/* Tx status */
enum tx_frame_status {
	tx_done = 0x0,
	tx_not_ls = 0x1,
	tx_err = 0x2,
	tx_dma_own = 0x4,
};

enum dma_irq_status {
	tx_hard_error = 0x1,
	tx_hard_error_bump_tc = 0x2,
	handle_rx = 0x4,
	handle_tx = 0x8,
};

/* Basic descriptor structure for normal and alternate descriptors */
struct dma_desc {
	__le32 des0;
	__le32 des1;
	__le32 des2;
	__le32 des3;
};

/* Extended descriptor structure (e.g. >= databook 3.50a) */
struct dma_extended_desc {
	struct dma_desc basic;	/* Basic descriptors */
	__le32 des4;	/* Extended Status */
	__le32 des5;	/* Reserved */
	__le32 des6;	/* Tx/Rx Timestamp Low */
	__le32 des7;	/* Tx/Rx Timestamp High */
};

static inline void ehn_desc_rx_set_on_ring(struct dma_desc *p, int end,
					   int bfsize)
{
	if (bfsize == BUF_SIZE_16KiB)
		p->des1 |= cpu_to_le32((BUF_SIZE_8KiB
				<< ERDES1_BUFFER2_SIZE_SHIFT)
			   & ERDES1_BUFFER2_SIZE_MASK);

	if (end)
		p->des1 |= cpu_to_le32(ERDES1_END_RING);
}

static inline void enh_desc_end_tx_desc_on_ring(struct dma_desc *p, int end)
{
	if (end)
		p->des0 |= cpu_to_le32(ETDES0_END_RING);
	else
		p->des0 &= cpu_to_le32(~ETDES0_END_RING);
}

static inline void enh_set_tx_desc_len_on_ring(struct dma_desc *p, int len)
{
	if (likely(len <= BUF_SIZE_4KiB))
		p->des1 |= cpu_to_le32((len & ETDES1_BUFFER1_SIZE_MASK));
	else
		p->des1 |= cpu_to_le32((((len - BUF_SIZE_4KiB)
					<< ETDES1_BUFFER2_SIZE_SHIFT)
			    & ETDES1_BUFFER2_SIZE_MASK) | (BUF_SIZE_4KiB
			    & ETDES1_BUFFER1_SIZE_MASK));
}

/* Normal descriptors */
static inline void ndesc_rx_set_on_ring(struct dma_desc *p, int end, int bfsize)
{
	if (bfsize >= BUF_SIZE_2KiB) {
		int bfsize2;

		bfsize2 = min(bfsize - BUF_SIZE_2KiB + 1, BUF_SIZE_2KiB - 1);
		p->des1 |= cpu_to_le32((bfsize2 << RDES1_BUFFER2_SIZE_SHIFT)
			    & RDES1_BUFFER2_SIZE_MASK);
	}

	if (end)
		p->des1 |= cpu_to_le32(RDES1_END_RING);
}

static inline void ndesc_end_tx_desc_on_ring(struct dma_desc *p, int end)
{
	if (end)
		p->des1 |= cpu_to_le32(TDES1_END_RING);
	else
		p->des1 &= cpu_to_le32(~TDES1_END_RING);
}

static inline void norm_set_tx_desc_len_on_ring(struct dma_desc *p, int len)
{
	if (unlikely(len > BUF_SIZE_2KiB)) {
		unsigned int buffer1 = (BUF_SIZE_2KiB - 1)
					& TDES1_BUFFER1_SIZE_MASK;
		p->des1 |= cpu_to_le32((((len - buffer1)
					<< TDES1_BUFFER2_SIZE_SHIFT)
				& TDES1_BUFFER2_SIZE_MASK) | buffer1);
	} else {
		p->des1 |= cpu_to_le32((len & TDES1_BUFFER1_SIZE_MASK));
	}
}

/* Specific functions used for Chain mode */

/* Enhanced descriptors */
static inline void ehn_desc_rx_set_on_chain(struct dma_desc *p)
{
	p->des1 |= cpu_to_le32(ERDES1_SECOND_ADDRESS_CHAINED);
}

static inline void enh_desc_end_tx_desc_on_chain(struct dma_desc *p)
{
	p->des0 |= cpu_to_le32(ETDES0_SECOND_ADDRESS_CHAINED);
}

static inline void enh_set_tx_desc_len_on_chain(struct dma_desc *p, int len)
{
	p->des1 |= cpu_to_le32(len & ETDES1_BUFFER1_SIZE_MASK);
}

/* Normal descriptors */
static inline void ndesc_rx_set_on_chain(struct dma_desc *p, int end)
{
	p->des1 |= cpu_to_le32(RDES1_SECOND_ADDRESS_CHAINED);
}

static inline void ndesc_tx_set_on_chain(struct dma_desc *p)
{
	p->des1 |= cpu_to_le32(TDES1_SECOND_ADDRESS_CHAINED);
}

static inline void norm_set_tx_desc_len_on_chain(struct dma_desc *p, int len)
{
	p->des1 |= cpu_to_le32(len & TDES1_BUFFER1_SIZE_MASK);
}

int aicmac_dma_desc_get_rx_status(struct dma_desc *p, void __iomem *ioaddr);
int aicmac_dma_desc_get_tx_status(struct dma_desc *p, void __iomem *ioaddr);
int aicmac_dma_desc_get_tx_len(struct dma_desc *p);
void aicmac_dma_desc_init_rx_desc(struct dma_desc *p, int disable_rx_ic,
				  int mode, int end, int bfsize);
void aicmac_dma_desc_init_tx_desc(struct dma_desc *p, int mode, int end);
int aicmac_dma_desc_get_tx_owner(struct dma_desc *p);
void aicmac_dma_desc_set_tx_owner(struct dma_desc *p);
void aicmac_dma_desc_set_rx_owner(struct dma_desc *p, int disable_rx_ic);
int aicmac_dma_desc_get_tx_ls(struct dma_desc *p);
void aicmac_dma_desc_release_tx_desc(struct dma_desc *p, int mode);
void aicmac_dma_desc_prepare_tx_desc(struct dma_desc *p, int is_fs, int len,
				     bool csum_flag, int mode, bool tx_own,
				     bool ls, unsigned int tot_pkt_len);
void aicmac_dma_desc_set_tx_ic(struct dma_desc *p);
int aicmac_dma_desc_get_rx_frame_len(struct dma_desc *p, int rx_coe_type);
void aicmac_dma_desc_enable_tx_timestamp(struct dma_desc *p);
int aicmac_dma_desc_get_tx_timestamp_status(struct dma_desc *p);
void aicmac_dma_desc_get_timestamp(void *desc, u32 ats, u64 *ts);
int aicmac_dma_desc_get_rx_timestamp_status(void *desc, void *next_desc,
					    u32 ats);
void aicmac_dma_desc_get_addr(struct dma_desc *p, unsigned int *addr);
void aicmac_dma_desc_set_addr(struct dma_desc *p, dma_addr_t addr);
void aicmac_dma_desc_clear(struct dma_desc *p);
void aicmac_dma_desc_set_sec_addr(struct dma_desc *p, dma_addr_t addr);
#endif
