/**
 ******************************************************************************
 *
 * @file asr_sdio.h
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */

#ifndef _ASR_SDIO_H_
#define _ASR_SDIO_H_

#define SDIO_PORT_IDX_0  0
#define SDIO_PORT_IDX_1  1
#define SDIO_PORT_IDX_2  2
#define SDIO_PORT_IDX_3  3
#define SDIO_PORT_IDX_4  4
#define SDIO_PORT_IDX_5  5
#define SDIO_PORT_IDX_6  6
#define SDIO_PORT_IDX_7  7
#define SDIO_PORT_IDX_8  8
#define SDIO_PORT_IDX_9  9
#define SDIO_PORT_IDX_10 10
#define SDIO_PORT_IDX_11 11
#define SDIO_PORT_IDX_12 12
#define SDIO_PORT_IDX_13 13
#define SDIO_PORT_IDX_14 14
#define SDIO_PORT_IDX_15 15
#define SDIO_PORT_IDX(idx) SDIO_PORT_IDX_##idx

//marvell sdio card reg
#define H2C_INTEVENT    0x0
#define HOST_INT_RSR    0x1
#define HOST_INTMASK    0x2
#define HOST_INT_UPLD_EN        (1 << 0)
#define HOST_INT_DNLD_EN        (1 << 1)
#define HOST_INT_UNDER_FLOW_EN  (1 << 2)
#define HOST_INT_OVER_FLOW_EN   (1 << 3)
#define HOST_INT_MASK    (HOST_INT_UPLD_EN | HOST_INT_DNLD_EN | HOST_INT_UNDER_FLOW_EN | HOST_INT_OVER_FLOW_EN)
#define HOST_INT_RSR_MASK  0xFF
#define HOST_INT_STATUS    0x3
#define HOST_INT_UPLD_ST        (1 << 0)
#define HOST_INT_DNLD_ST        (1 << 1)
#define HOST_INT_UNDER_FLOW_ST  (1 << 2)
#define HOST_INT_OVER_FLOW_ST   (1 << 3)
#define RD_BITMAP_L        0x4
#define RD_BITMAP_U        0x5
#define WR_BITMAP_L        0x6
#define WR_BITMAP_U        0x7

#define RD_LEN0_L        0x8
#define RD_LEN0_U        0x9
#define RD_LEN1_L        0xA
#define RD_LEN1_U        0xB
#define RD_LEN2_L        0xC
#define RD_LEN2_U        0xD
#define RD_LEN3_L        0xE
#define RD_LEN3_U        0xF
#define RD_LEN4_L        0x10
#define RD_LEN4_U        0x11
#define RD_LEN5_L        0x12
#define RD_LEN5_U        0x13
#define RD_LEN6_L        0x14
#define RD_LEN6_U        0x15
#define RD_LEN7_L        0x16
#define RD_LEN7_U        0x17
#define RD_LEN8_L        0x18
#define RD_LEN8_U        0x19
#define RD_LEN9_L        0x1A
#define RD_LEN9_U        0x1B
#define RD_LEN10_L       0x1C
#define RD_LEN10_U       0x1D
#define RD_LEN11_L       0x1E
#define RD_LEN11_U       0x1F
#define RD_LEN12_L       0x20
#define RD_LEN12_U       0x21
#define RD_LEN13_L       0x22
#define RD_LEN13_U       0x23
#define RD_LEN14_L       0x24
#define RD_LEN14_U       0x25
#define RD_LEN15_L       0x26
#define RD_LEN15_U       0x27

#define RD_LEN_L(index)    (0x8 + (index) * 2)
#define RD_LEN_U(index)    (0x9 + (index) * 2)

#define HOST_RESTART     0x28
#define C2H_INTEVENT     0x30
#define C2H_DNLD_CARD_RDY    (1 << 0)
#define C2H_UPLD_CARD_RDY    (1 << 1)
#define C2H_CIS_CARD_RDY     (1 << 2)
#define C2H_IO_RDY           (1 << 3)
#define C2H_DEFINE_1         (1 << 4)
#define C2H_DEFINE_2         (1 << 5)

#define CARD_INT_STATUS     0x38

#define RD_BASE_0        0x40
#define RD_BASE_1        0x41
#define RD_BASE_2        0x42
#define RD_BASE_3        0x43
#define WR_BASE_0        0x44
#define WR_BASE_1        0x45
#define WR_BASE_2        0x46
#define WR_BASE_3        0x47
#define RD_INDEX         0x48
#define WR_INDEX         0x49

#define SCRATCH_0        0x60
#define SCRATCH_1        0x61
#define CRC_SUCCESS     (1 << 14)
#define CRC_FAIL        0xABCE
#define BOOT_SUCCESS    (1 << 15)

#define SCRATCH0_2        0x62
#define SCRATCH0_3        0x63

#define SCRATCH1_0        0x64
#define SCRATCH1_1        0x65
#define SCRATCH1_2        0x66
#define SCRATCH1_3        0x67

#define SDIO_CONFIG2     0x6c
#define AUTO_RE_ENABLE_INT (1<<4)

#define IO_PORT_0        0x78
#define IO_PORT_1        0x79
#define IO_PORT_2        0x7a

#define MAX_POLL_TRIES    100
#define SDIO_BLOCK_SIZE_DLD  512

#ifdef CONFIG_ASR595X
#define SDIO_BLOCK_SIZE     32	//4 SDIO max transfer in one cmd53:4*512=2KB
#define SDIO_BLOCK_SIZE_TX  32	//tx remain 32byte align
#else
#ifndef SDIO_BLOCK_SIZE
#define SDIO_BLOCK_SIZE     32	//SDIO max transfer in one cmd53:32*512=16KB
#endif
#endif

#define SDIO_DOWNLOAD_BLOCKS 270

#define SDIO_RX_AGG_TRI_CNT 	6
#define SDIO_TX_AGG_TRI_CNT 	7

#ifdef CONFIG_ASR595X
#define ASR_ALIGN_BLKSZ_HI(x) (((x)+SDIO_BLOCK_SIZE_TX-1)&(~(SDIO_BLOCK_SIZE_TX-1)))
#else
#define ASR_ALIGN_BLKSZ_HI(x) (((x)+SDIO_BLOCK_SIZE-1)&(~(SDIO_BLOCK_SIZE-1)))
#endif

#define ASR_ALIGN_DLDBLKSZ_HI(x) (((x)+SDIO_BLOCK_SIZE_DLD-1)&(~(SDIO_BLOCK_SIZE_DLD-1)))

/** Macros for Data Alignment : size */
#if 0
#define ALIGN_SZ(p, a)  \
    (((p) + ((a) - 1)) & ~((a) - 1))

/** Macros for Data Alignment : address */
#define ALIGN_ADDR(p, a)    \
    ((((unsigned int)(p)) + (((unsigned int)(a)) - 1)) & ~(((unsigned int)(a)) - 1))
#else
#define ALIGN_ADDR(addr, boundary) (void *)(((uintptr_t)(addr) + (boundary) - 1) \
                                                 & ~((boundary) - 1))
#define ALIGN_SZ(size, boundary) (((size) + (boundary) - 1) \
                                                 & ~((boundary) - 1))
#endif

#define WLAN_UPLD_SIZE    (2312)
#define DMA_ALIGNMENT     32

struct sdio_host_sec_fw_header {
	uint32_t sec_fw_len;	// total length of FW: should be 4 byte align
	uint32_t sec_crc;	// fw bin crc32 results
	uint32_t chip_ram_addr;	// address where fw be loaded to
	uint32_t transfer_unit;	// download length in once cmd53 transmition
	uint32_t transfer_times;	// TX times needed for download FW, not include the first cmd transmition
};

struct sdio_fw_sec_fw_header {
	uint32_t sec_fw_len;	// total length of FW: should be 4 byte align
	uint32_t sec_crc;	// fw bin crc32 results
	uint32_t chip_ram_addr;	// address where fw be loaded to
};

int asr_sdio_register_drv(void);
void asr_sdio_unregister_drv(void);
int asr_sdio_send_data(struct asr_hw *asr_hw, u8 type, u8 * src, u16 len, unsigned int io_addr, u16 bitmap_record);
int asr_sdio_rw_extended_sg(struct sdio_func *func, int write,
	unsigned addr, int incr_addr, struct sk_buff_head *skb_list, unsigned size);

#ifdef CONFIG_ASR5825
int asr_sdio_download_1st_firmware(struct sdio_func *func, const struct firmware *fw_img);
int asr_sdio_download_2nd_firmware(struct sdio_func *func, const struct firmware *fw_img);
#else
int asr_sdio_download_firmware(struct sdio_func *func, const struct firmware *fw_img);
#endif

int asr_sdio_download_ATE_firmware(struct sdio_func *func, const struct firmware *fw_img);
int asr_sdio_enable_int(struct sdio_func *func, int mask);
void asr_sdio_disable_int(struct sdio_func *func, int mask);
int sdio_get_ioport(struct sdio_func *func);
int asr_sdio_config_rsr(struct sdio_func *func, u8 mask);
int asr_sdio_config_auto_reenable(struct sdio_func *func);
int asr_sdio_tx_common_port_dispatch(struct asr_hw *asr_hw, u8 * src, u32 len, unsigned int io_addr, u16 bitmap_record);
u8 asr_sdio_tx_get_available_data_port(struct asr_hw *asr_hw,
				       u16 ava_pkt_num, u8 * port_num, unsigned int *io_addr, u16 * bitmap_record);
#if defined(CONFIG_ASR5825)
int asr_sdio_send_section_firmware(struct sdio_func *func, struct sdio_host_sec_fw_header
				   *p_sec_fw_hdr, u8 * fw_buf, u8 * fw, int blk_size);
#else
int asr_sdio_send_section_firmware(struct sdio_func *func, struct sdio_host_sec_fw_header
				   *p_sec_fw_hdr, u8 * fw_buf, u8 * fw);
#endif

static inline void asr_sdio_set_state(struct asr_plat *plat, enum asr_sdio_state state) {
	if (plat->state == state)
		return;

	plat->state = state;
}

static inline int asr_sdio_get_state(struct asr_plat *plat) {
	return plat->state;
}

#endif /* _ASR_SDIO_H_ */
