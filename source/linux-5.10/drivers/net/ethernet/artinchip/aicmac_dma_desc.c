// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#include <linux/platform_device.h>

#include "aicmac.h"
#include "aicmac_dma.h"
#include "aicmac_dma_reg.h"
#include "aicmac_dma_desc.h"
#include "aicmac_dma_ring.h"
#include "aicmac_dma_chain.h"

int aicmac_dma_desc_get_tx_status(struct dma_desc *p, void __iomem *ioaddr)
{
	unsigned int tdes0 = le32_to_cpu(p->des0);
	int ret = tx_done;

	/* Get tx owner first */
	if (unlikely(tdes0 & ETDES0_OWN))
		return tx_dma_own;

	/* Verify tx error by looking at the last segment. */
	if (likely(!(tdes0 & ETDES0_LAST_SEGMENT)))
		return tx_not_ls;

	if (unlikely(tdes0 & ETDES0_ERROR_SUMMARY)) {
		if (unlikely(tdes0 & ETDES0_FRAME_FLUSHED))
			aicmac_dma_reg_flush_tx_fifo(ioaddr);

		if (unlikely(tdes0 & ETDES0_UNDERFLOW_ERROR))
			aicmac_dma_reg_flush_tx_fifo(ioaddr);

		if (unlikely(tdes0 & ETDES0_PAYLOAD_ERROR))
			aicmac_dma_reg_flush_tx_fifo(ioaddr);

		ret = tx_err;
	}

	return ret;
}

int aicmac_dma_desc_get_tx_len(struct dma_desc *p)
{
	return (le32_to_cpu(p->des1) & ETDES1_BUFFER1_SIZE_MASK);
}

static int aicmac_dma_desc_coe_rdes0(int ipc_err, int type, int payload_err)
{
	int ret = good_frame;
	u32 status = (type << 2 | ipc_err << 1 | payload_err) & 0x7;

	/* bits 5 7 0 | Frame status
	 * ----------------------------------------------------------
	 *      0 0 0 | IEEE 802.3 Type frame (length < 1536 octects)
	 *      1 0 0 | IPv4/6 No CSUM errorS.
	 *      1 0 1 | IPv4/6 CSUM PAYLOAD error
	 *      1 1 0 | IPv4/6 CSUM IP HR error
	 *      1 1 1 | IPv4/6 IP PAYLOAD AND HEADER errorS
	 *      0 0 1 | IPv4/6 unsupported IP PAYLOAD
	 *      0 1 1 | COE bypassed.. no IPv4/6 frame
	 *      0 1 0 | Reserved.
	 */
	if (status == 0x0)
		ret = llc_snap;
	else if (status == 0x4)
		ret = good_frame;
	else if (status == 0x5)
		ret = csum_none;
	else if (status == 0x6)
		ret = csum_none;
	else if (status == 0x7)
		ret = csum_none;
	else if (status == 0x1)
		ret = discard_frame;
	else if (status == 0x3)
		ret = discard_frame;

	return ret;
}

int aicmac_dma_desc_get_rx_status(struct dma_desc *p, void __iomem *ioaddr)
{
	unsigned int rdes0 = le32_to_cpu(p->des0);
	int ret = good_frame;

	if (unlikely(rdes0 & RDES0_OWN))
		return dma_own;

	if (unlikely(!(rdes0 & RDES0_LAST_DESCRIPTOR)))
		return discard_frame;

	if (unlikely(rdes0 & RDES0_ERROR_SUMMARY))
		ret = discard_frame;

	if (likely(ret == good_frame))
		ret = aicmac_dma_desc_coe_rdes0(!!(rdes0 &
						RDES0_IPC_CSUM_ERROR),
						!!(rdes0 & RDES0_FRAME_TYPE),
						!!(rdes0 &
						ERDES0_RX_MAC_ADDR));

	if (unlikely(rdes0 & RDES0_SA_FILTER_FAIL))
		ret = discard_frame;

	if (unlikely(rdes0 & RDES0_DA_FILTER_FAIL))
		ret = discard_frame;

	if (unlikely(rdes0 & RDES0_LENGTH_ERROR))
		ret = discard_frame;

	return ret;
}

void aicmac_dma_desc_init_rx_desc(struct dma_desc *p, int disable_rx_ic,
				  int mode, int end, int bfsize)
{
	int bfsize1;

	p->des0 |= cpu_to_le32(RDES0_OWN);

	bfsize1 = min(bfsize, BUF_SIZE_8KiB);
	p->des1 |= cpu_to_le32(bfsize1 & ERDES1_BUFFER1_SIZE_MASK);

	if (mode == AICMAC_CHAIN_MODE)
		ehn_desc_rx_set_on_chain(p);
	else
		ehn_desc_rx_set_on_ring(p, end, bfsize);

	if (disable_rx_ic)
		p->des1 |= cpu_to_le32(ERDES1_DISABLE_IC);
}

void aicmac_dma_desc_init_tx_desc(struct dma_desc *p, int mode, int end)
{
	p->des0 &= cpu_to_le32(~ETDES0_OWN);
	if (mode == AICMAC_CHAIN_MODE)
		enh_desc_end_tx_desc_on_chain(p);
	else
		enh_desc_end_tx_desc_on_ring(p, end);
}

int aicmac_dma_desc_get_tx_owner(struct dma_desc *p)
{
	return (le32_to_cpu(p->des0) & ETDES0_OWN) >> 31;
}

void aicmac_dma_desc_set_tx_owner(struct dma_desc *p)
{
	p->des0 |= cpu_to_le32(ETDES0_OWN);
}

void aicmac_dma_desc_set_rx_owner(struct dma_desc *p, int disable_rx_ic)
{
	p->des0 |= cpu_to_le32(RDES0_OWN);
}

int aicmac_dma_desc_get_tx_ls(struct dma_desc *p)
{
	return (le32_to_cpu(p->des0) & ETDES0_LAST_SEGMENT) >> 29;
}

void aicmac_dma_desc_release_tx_desc(struct dma_desc *p, int mode)
{
	int ter = (le32_to_cpu(p->des0) & ETDES0_END_RING) >> 21;

	//memset(p, 0, offsetof(struct dma_desc, des2));
	p->des0 = 0;
	p->des1 = 0;

	if (mode == AICMAC_CHAIN_MODE)
		enh_desc_end_tx_desc_on_chain(p);
	else
		enh_desc_end_tx_desc_on_ring(p, ter);
}

void aicmac_dma_desc_prepare_tx_desc(struct dma_desc *p, int is_fs, int len,
				     bool csum_flag, int mode, bool tx_own,
				     bool ls, unsigned int tot_pkt_len)
{
	unsigned int tdes0 = le32_to_cpu(p->des0);

	if (mode != AICMAC_CHAIN_MODE)
		enh_set_tx_desc_len_on_ring(p, len);
	else
		enh_set_tx_desc_len_on_chain(p, len);

	if (is_fs)
		tdes0 |= ETDES0_FIRST_SEGMENT;
	else
		tdes0 &= ~ETDES0_FIRST_SEGMENT;

	if (likely(csum_flag))
		tdes0 |= (TX_CIC_FULL << ETDES0_CHECKSUM_INSERTION_SHIFT);
	else
		tdes0 &= ~(TX_CIC_FULL << ETDES0_CHECKSUM_INSERTION_SHIFT);

	if (ls)
		tdes0 |= ETDES0_LAST_SEGMENT;

	/* Finally set the OWN bit. Later the DMA will start! */
	if (tx_own)
		tdes0 |= ETDES0_OWN;

	if (is_fs && tx_own)
		/* When the own bit, for the first frame, has to be set, all
		 * descriptors for the same frame has to be set before, to
		 * avoid race condition.
		 */
		dma_wmb();

	p->des0 = cpu_to_le32(tdes0);
}

void aicmac_dma_desc_set_tx_ic(struct dma_desc *p)
{
	p->des0 |= cpu_to_le32(ETDES0_INTERRUPT);
}

int aicmac_dma_desc_get_rx_frame_len(struct dma_desc *p, int rx_coe_type)
{
	unsigned int csum = 0;

	if (rx_coe_type == AICMAC_RX_COE_TYPE1)
		csum = 2;

	return (((le32_to_cpu(p->des0) & RDES0_FRAME_LEN_MASK)
				>> RDES0_FRAME_LEN_SHIFT) - csum);
}

void aicmac_dma_desc_enable_tx_timestamp(struct dma_desc *p)
{
	p->des0 |= cpu_to_le32(ETDES0_TIME_STAMP_ENABLE);
}

int aicmac_dma_desc_get_tx_timestamp_status(struct dma_desc *p)
{
	return (le32_to_cpu(p->des0) & ETDES0_TIME_STAMP_STATUS) >> 17;
}

void aicmac_dma_desc_get_timestamp(void *desc, u32 ats, u64 *ts)
{
	u64 ns;

	if (ats) {
		struct dma_extended_desc *p = (struct dma_extended_desc *)desc;

		ns = le32_to_cpu(p->des6);
		/* convert high/sec time stamp value to nanosecond */
		ns += le32_to_cpu(p->des7) * 1000000000ULL;
	} else {
		struct dma_desc *p = (struct dma_desc *)desc;

		ns = le32_to_cpu(p->des2);
		ns += le32_to_cpu(p->des3) * 1000000000ULL;
	}

	*ts = ns;
}

int aicmac_dma_desc_get_rx_timestamp_status(void *desc, void *next_desc,
					    u32 ats)
{
	int ret = 0;

	if (ats) {
		struct dma_extended_desc *p = (struct dma_extended_desc *)desc;

		ret = (le32_to_cpu(p->basic.des0) & RDES0_IPC_CSUM_ERROR) >> 7;
	} else {
		struct dma_desc *p = (struct dma_desc *)desc;

		if ((le32_to_cpu(p->des2) == 0xffffffff) &&
		    (le32_to_cpu(p->des3) == 0xffffffff))
			ret = 0;
		else
			ret = 1;
	}

	return ret;
}

void aicmac_dma_desc_get_addr(struct dma_desc *p, unsigned int *addr)
{
	*addr = le32_to_cpu(p->des2);
}

void aicmac_dma_desc_set_addr(struct dma_desc *p, dma_addr_t addr)
{
	p->des2 = cpu_to_le32(addr);
}

void aicmac_dma_desc_clear(struct dma_desc *p)
{
	p->des2 = 0;
}

void aicmac_dma_desc_set_sec_addr(struct dma_desc *p, dma_addr_t addr)
{
	p->des3 = cpu_to_le32(addr);
}
