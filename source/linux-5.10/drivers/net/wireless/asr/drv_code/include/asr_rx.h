/**
 ******************************************************************************
 *
 * @file asr_rx.h
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */
#ifndef _ASR_RX_H_
#define _ASR_RX_H_

enum rx_status_bits {
	/// The buffer can be forwarded to the networking stack
	RX_STAT_FORWARD = 1 << 0,
	/// A new buffer has to be allocated
	RX_STAT_ALLOC = 1 << 1,
	/// The buffer has to be deleted
	RX_STAT_DELETE = 1 << 2,
    /************************************* defrag used start*********************************/	
	/// The length of the buffer has to be updated
	RX_STAT_LEN_UPDATE = 1 << 3,
	/// The length in the Ethernet header has to be updated
	RX_STAT_ETH_LEN_UPDATE = 1 << 4,
	/// Simple copy
	RX_STAT_COPY = 1 << 5,
	/************************************* defrag used end*********************************/
    /// Spurious frame (inform upper layer and discard)
    RX_STAT_SPURIOUS = 1 << 6,
    /// Frame for monitor interface
    RX_STAT_MONITOR = 1 << 7,
    /// unsupported frame
    RX_STAT_UF = 1 << 8,
};

/*
 * Decryption status subfields.
 * {
 */
#define ASR_RX_HD_DECR_UNENC           0	// Frame unencrypted
#define ASR_RX_HD_DECR_ICVFAIL         1	// WEP/TKIP ICV failure
#define ASR_RX_HD_DECR_CCMPFAIL        2	// CCMP failure
#define ASR_RX_HD_DECR_AMSDUDISCARD    3	// A-MSDU discarded at HW
#define ASR_RX_HD_DECR_NULLKEY         4	// NULL key found
#define ASR_RX_HD_DECR_WEPSUCCESS      5	// Security type WEP
#define ASR_RX_HD_DECR_TKIPSUCCESS     6	// Security type TKIP
#define ASR_RX_HD_DECR_CCMPSUCCESS     7	// Security type CCMP

#ifdef CONFIG_ASR_NAPI
#define ASR_NAPI_WEIGHT (32)
#endif
// @}
#ifdef CONFIG_ASR595X
struct rx_leg_vect {
	u8 dyn_bw_in_non_ht:1;
	u8 chn_bw_in_non_ht:2;
	u8 rsvd_nht:4;
	u8 lsig_valid:1;
} __packed;

struct rx_ht_vect {
	u16 sounding:1;
	u16 smoothing:1;
	u16 short_gi:1;
	u16 aggregation:1;
	u16 stbc:1;
	u16 num_extn_ss:2;
	u16 lsig_valid:1;
	u16 mcs:7;
	u16 fec:1;
	u16 length:16;
} __packed;

struct rx_vht_vect {
	u8 sounding:1;
	u8 beamformed:1;
	u8 short_gi:1;
	u8 rsvd_vht1:1;
	u8 stbc:1;
	u8 doze_not_allowed:1;
	u8 first_user:1;
	u8 rsvd_vht2:1;
	u16 partial_aid:9;
	u16 group_id:6;
	u16 rsvd_vht3:1;
	u32 mcs:4;
	u32 nss:3;
	u32 fec:1;
	u32 length:20;
	u32 rsvd_vht4:4;
} __packed;

struct rx_he_vect {
	u8 sounding:1;
	u8 beamformed:1;
	u8 gi_type:2;
	u8 stbc:1;
	u8 rsvd_he1:3;

	u8 uplink_flag:1;
	u8 beam_change:1;
	u8 dcm:1;
	u8 he_ltf_type:2;
	u8 doppler:1;
	u8 rsvd_he2:2;

	u8 bss_color:6;
	u8 rsvd_he3:2;

	u8 txop_duration:7;
	u8 rsvd_he4:1;

	u8 pe_duration:4;
	u8 spatial_reuse:4;

	u8 sig_b_comp_mode:1;
	u8 dcm_sig_b:1;
	u8 mcs_sig_b:3;
	u8 ru_size:3;

	u32 mcs:4;
	u32 nss:3;
	u32 fec:1;
	u32 length:20;
	u32 rsvd_he6:4;
} __packed;

struct rx_vector_1 {
	u8 format_mod:4;
	u8 ch_bw:3;
	u8 pre_type:1;
	u8 antenna_set:8;
	s32 rssi_leg:8;
	u32 leg_length:12;
	u32 leg_rate:4;
	s32 rssi1:8;

	union {
		struct rx_leg_vect leg;
		struct rx_ht_vect ht;
		struct rx_vht_vect vht;
		struct rx_he_vect he;
	};
} __packed;

struct rx_vector_2 {
    /** Receive Vector 2a */
	u32 rcpi1:8;
	u32 rcpi2:8;
	u32 rcpi3:8;
	u32 rcpi4:8;
    /** Receive Vector 2b */
	u32 evm1:8;
	u32 evm2:8;
	u32 evm3:8;
	u32 evm4:8;
};

#else

struct rx_vector_1 {
    /** Receive Vector 1a */
	u32 leg_length:12;
	u32 leg_rate:4;
	u32 ht_length:16;

    /** Receive Vector 1b */
	u32 _ht_length:4;	// FIXME
	u32 short_gi:1;
	u32 stbc:2;
	u32 smoothing:1;
	u32 mcs:7;
	u32 pre_type:1;
	u32 format_mod:3;
	u32 ch_bw:2;
	u32 n_sts:3;
	u32 lsig_valid:1;
	u32 sounding:1;
	u32 num_extn_ss:2;
	u32 aggregation:1;
	u32 fec_coding:1;
	u32 dyn_bw:1;
	u32 doze_not_allowed:1;

    /** Receive Vector 1c */
	u32 antenna_set:8;
	u32 partial_aid:9;
	u32 group_id:6;
	u32 reserved_1c:1;
	s32 rssi1:8;

    /** Receive Vector 1d */
	s32 rssi2:8;
	s32 rssi3:8;
	s32 rssi4:8;
	u32 reserved_1d:8;
};
#endif
struct hw_vect {
    /** Total length for the MPDU transfer */
	u32 len:16;

	u32 reserved:8;

    /** AMPDU Status Information */
	u32 mpdu_cnt:6;
	u32 ampdu_cnt:2;

    /** TSF Low */
	__le32 tsf_lo;
    /** TSF High */
	__le32 seq_num;		//tsf_hi;

    /** Receive Vector 1 */
	struct rx_vector_1 rx_vect1;

#ifdef CONFIG_ASR595X
    /** Receive Vector 2 */
	struct rx_vector_2 rx_vect2;
#else
    /** Receive Vector 2a */
	u32 rcpi:8;
	u32 evm1:8;
	u32 evm2:8;
	u32 evm3:8;

    /** Receive Vector 2b */
	u32 evm4:8;
	u32 reserved2b_1:8;
	u32 reserved2b_2:8;
	u32 reserved2b_3:8;
#endif

    /** Status **/
	u32 rx_vect2_valid:1;
	u32 resp_frame:1;
    /** Decryption Status */
	u32 decr_status:3;
	u32 rx_fifo_oflow:1;

    /** Frame Unsuccessful */
	u32 undef_err:1;
	u32 phy_err:1;
	u32 fcs_err:1;
	u32 addr_mismatch:1;
	u32 ga_frame:1;
	u32 current_ac:2;

	u32 frm_successful_rx:1;
    /** Descriptor Done  */
	u32 desc_done_rx:1;
    /** Key Storage RAM Index */
	u32 key_sram_index:10;
    /** Key Storage RAM Index Valid */
	u32 key_sram_v:1;
	u32 type:2;
	u32 subtype:4;
};

struct hw_rxhdr {
    /** RX vector */
	struct hw_vect hwvect;

    /** PHY channel information 1 */
	u32 phy_band:8;
	u32 phy_channel_type:8;
	u32 phy_prim20_freq:16;
    /** PHY channel information 2 */
	u32 phy_center1_freq:16;
	u32 phy_center2_freq:16;
    /** RX flags */
	u32 flags_is_amsdu:1;
	u32 flags_is_80211_mpdu:1;
	u32 flags_is_4addr:1;
	u32 flags_new_peer:1;
	u32 flags_user_prio:3;
	u32 flags_rsvd0:1;
	u32 flags_vif_idx:8;	// 0xFF if invalid VIF index
	u32 flags_sta_idx:8;	// 0xFF if invalid STA index
	u32 flags_dst_idx:8;	// 0xFF if unknown destination STA
};

//sdio fw add
struct host_rx_desc {
	//id to indicate data/log/msg
	u16 id;			// data=0xffff, log=0xfffe, others is msg
	// payload offset
	u16 pld_offset;
	/// Total length of the payload
	u16 frmlen;
	/// AMPDU status information
	u16 ampdu_stat_info;
	/// TSF Low
	u16 sdio_rx_len;	//this pkt sdio transfer total len, n*blocksize
	/// TSF Low
	u16 totol_frmlen;
	/// TSF High
	u16 seq_num;
	/// TSF High
	u16 fn_num;
	/// Contains the bytes 4 - 1 of Receive Vector 1
	u32 recvec1a;
	/// Contains the bytes 8 - 5 of Receive Vector 1
	u32 recvec1b;
	/// Contains the bytes 12 - 9 of Receive Vector 1
	u32 recvec1c;
	/// Contains the bytes 16 - 13 of Receive Vector 1
	u32 recvec1d;
	/// Contains the bytes 4 - 1 of Receive Vector 2
	u32 recvec2a;
	///  Contains the bytes 8 - 5 of Receive Vector 2
	u32 recvec2b;
	/// MPDU status information
	u32 statinfo;
	/// Structure containing the information about the PHY channel that was used for this RX
	struct phy_channel_info phy_info;
	/// Word containing some SW flags about the RX packet
	u32 flags;
	// sdio rx agg num.
	u16 num;
    // new add for ooo
    uint8_t            sta_idx;
	uint8_t            tid;
    uint16_t           rx_status;	
};
#define HOST_RX_DESC_SIZE (sizeof(struct host_rx_desc))
#define HOST_RX_DESC_PART_LEN 38	//from ampdu_stat_info to statinfo
#define HOST_RX_DATA_ID (0xFFFF)
#define HOST_RX_DESC_ID (0x7FFE)

extern const u8 legrates_lut[];

u8 asr_rxdataind(void *pthis, void *hostid);
#ifdef CONFIG_ASR_NAPI
int asr_recv_napi_poll(struct napi_struct *napi, int budget);
enum hrtimer_restart rx_napi_hrtimer_handler(struct hrtimer *timer);
#endif
#if 0//def CFG_SNIFFER_SUPPORT
uint8_t asr_rxdataind_sniffer(void *pthis, void *hostid);
#endif

#endif /* _ASR_RX_H_ */
