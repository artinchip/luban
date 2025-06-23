/**
 ******************************************************************************
 *
 * @file asr_defs.h
 *
 * @brief Main driver structure declarations for fullmac driver
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */

#ifndef _ASR_DEFS_H_
#define _ASR_DEFS_H_

#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/skbuff.h>
#include <net/cfg80211.h>
#include <linux/slab.h>
#ifdef CONFIG_ASR_SDIO
#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#ifdef ASR_SDIO_HOST_SDHCI
#include <linux/mmc/sdhci.h>
#endif
#endif
#include <linux/firmware.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/device.h>
#ifdef CONFIG_ASR_NAPI
#include <linux/netdevice.h>
#endif
#include <linux/etherdevice.h>
#include "asr_mod_params.h"
#include "asr_debugfs.h"
#include "asr_tx.h"
#include "asr_rx.h"
#include "asr_utils.h"
#include "asr_platform.h"
#include "ipc_host.h"
#include <linux/atomic.h>

#define WPI_HDR_LEN    18
#define WPI_PN_LEN     16
#define WPI_PN_OFST     2
#define WPI_MIC_LEN    16
#define WPI_KEY_LEN    32
#define WPI_SUBKEY_LEN 16	// WPI key is actually two 16bytes key

#define LEGACY_PS_ID   0
#define UAPSD_ID       1

//#define ASR_FLAG_TX_BIT 1
//#define ASR_FLAG_RX_BIT 2
//#define ASR_FLAG_RX_TX_BIT 3
//#define ASR_FLAG_RX_TO_OS_BIT 4

//#define ASR_FLAG_TX (1<<1)
//#define ASR_FLAG_RX (1<<2)
//#define ASR_FLAG_RX_TX (1<<3)
//#define ASR_FLAG_RX_TO_OS (1<<4)
//#define ASR_FLAG_TX_PROCESS (ASR_FLAG_TX|ASR_FLAG_RX_TX)

#define TXRX_RATES_NUM	3

#define SDIO_REG_READ_LENGTH 64

#define ASR_FLAG_MAIN_TASK_BIT   2
#define ASR_FLAG_OOB_INT_BIT     3
#define ASR_FLAG_RX_TO_OS_BIT    4
#define ASR_FLAG_MSGIND_TASK_BIT 5
#define ASR_FLAG_SDIO_INT_BIT    6

#define ASR_FLAG_MAIN_TASK (1<<2)
#define ASR_FLAG_OOB_INTR  (1<<3)
#define ASR_FLAG_RX_TO_OS  (1<<4)
#define ASR_FLAG_MSGIND_TASK  (1<<5)
#define ASR_FLAG_SDIO_INT_TASK  (1<<6)

#define ASR_SDIO_DATA_MAX_LEN TX_AGG_BUF_UNIT_SIZE

#ifdef ASR_HEARTBEAT_DETECT
#define ASR_HEARTBEAT_TIMER_OUT	60000	//60s
#endif

#define ASR_CMD_CRASH_TIMER_OUT	5000	//5s

#ifdef ASR_REDUCE_TCP_ACK
#define ASR_TCP_ACK_TIMER_OUT	100	//100ms
#endif

#ifdef ASR_STATS_RATES_TIMER
#define ASR_STATS_RATES_TIMER_OUT	1000	// 1s
#define ASR_STATS_RATES_SCAN_THRESHOLD	100000
#endif

#ifdef ASR_DRV_DEBUG_TIMER
#define ASR_DRVDEBUG_TIMER_OUT		1000	// 1s
#endif
#define ASR_ATE_AT_CMD_TIMER_OUT	600	// 600ms

#define ASR_TXFLOW_TIMER_OUT		500
#define ASR_BROADCAST_FRAME_TIMEOUT 	300

#define ASR_PAIRWISE_KEY_NUM		6

enum {
	ASR_RESTART_REASON_CMD_CRASH = 0,
	ASR_RESTART_REASON_SCAN_FAIL = 1,
	ASR_RESTART_REASON_SDIO_ERR = 2,
	ASR_RESTART_REASON_TXMSG_FAIL = 3,

	ASR_RESTART_REASON_MAX,
};

#ifdef CONFIG_ASR_NAPI
enum _NAPI_STATE {
    NAPI_DISABLE = 0,
    NAPI_ENABLE = 1,
};
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 2, 0)
/**
 * struct beacon_parameters - beacon parameters
 *
 * Used to configure the beacon for an interface.
 *
 * @head: head portion of beacon (before TIM IE)
 *     or %NULL if not changed
 * @tail: tail portion of beacon (after TIM IE)
 *     or %NULL if not changed
 * @interval: beacon interval or zero if not changed
 * @dtim_period: DTIM period or zero if not changed
 * @head_len: length of @head
 * @tail_len: length of @tail
 */
struct beacon_parameters {
	u8 *head, *tail;
	int interval, dtim_period;
	int head_len, tail_len;
};
#endif
#endif

//#define CONFIG_POWER_SAVE
/**
 * struct asr_bcn - Information of the beacon in used (AP mode)
 *
 * @head: head portion of beacon (before TIM IE)
 * @tail: tail portion of beacon (after TIM IE)
 * @ies: extra IEs (not used ?)
 * @head_len: length of head data
 * @tail_len: length of tail data
 * @ies_len: length of extra IEs data
 * @tim_len: length of TIM IE
 * @len: Total beacon len (head + tim + tail + extra)
 * @dtim: dtim period
 */
struct asr_bcn {
	u8 *head;
	u8 *tail;
	u8 *ies;
	size_t head_len;
	size_t tail_len;
	size_t ies_len;
	size_t tim_len;
	size_t len;
	u8 dtim;
};

/**
 * struct asr_key - Key information
 *
 * @hw_idx: Idx of the key from hardware point of view
 */
struct asr_key {
	u8 hw_idx;
};

enum driver_work_mode {
	DRIVER_MODE_NORMAL,
	DRIVER_MODE_ATE,
};

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 10, 0)

enum nl80211_ac {
	NL80211_AC_VO,
	NL80211_AC_VI,
	NL80211_AC_BE,
	NL80211_AC_BK,
	NL80211_NUM_ACS
};

/**
 * enum ieee80211_vht_mcs_support - VHT MCS support definitions
 * @IEEE80211_VHT_MCS_SUPPORT_0_7: MCSes 0-7 are supported for the
 *	number of streams
 * @IEEE80211_VHT_MCS_SUPPORT_0_8: MCSes 0-8 are supported
 * @IEEE80211_VHT_MCS_SUPPORT_0_9: MCSes 0-9 are supported
 * @IEEE80211_VHT_MCS_NOT_SUPPORTED: This number of streams isn't supported
 *
 * These definitions are used in each 2-bit subfield of the @rx_mcs_map
 * and @tx_mcs_map fields of &struct ieee80211_vht_mcs_info, which are
 * both split into 8 subfields by number of streams. These values indicate
 * which MCSes are supported for the number of streams the value appears
 * for.
 */
enum ieee80211_vht_mcs_support {
	IEEE80211_VHT_MCS_SUPPORT_0_7 = 0,
	IEEE80211_VHT_MCS_SUPPORT_0_8 = 1,
	IEEE80211_VHT_MCS_SUPPORT_0_9 = 2,
	IEEE80211_VHT_MCS_NOT_SUPPORTED = 3,
};

enum nl80211_chan_width {
	NL80211_CHAN_WIDTH_20_NOHT,
	NL80211_CHAN_WIDTH_20,
	NL80211_CHAN_WIDTH_40,
	NL80211_CHAN_WIDTH_80,
	NL80211_CHAN_WIDTH_80P80,
	NL80211_CHAN_WIDTH_160,
	NL80211_CHAN_WIDTH_5,
	NL80211_CHAN_WIDTH_10,
};

/**
 * struct cfg80211_chan_def - channel definition
 * @chan: the (control) channel
 * @width: channel width
 * @center_freq1: center frequency of first segment
 * @center_freq2: center frequency of second segment
 *	(only with 80+80 MHz)
 */
struct cfg80211_chan_def {
	struct ieee80211_channel *chan;
	enum nl80211_chan_width width;
	u32 center_freq1;
	u32 center_freq2;
};

/**
 * ether_addr_equal - Compare two Ethernet addresses
 * @addr1: Pointer to a six-byte array containing the Ethernet address
 * @addr2: Pointer other six-byte array containing the Ethernet address
 *
 * Compare two Ethernet addresses, returns true if equal
 *
 * Please note: addr1 & addr2 must both be aligned to u16.
 */
static inline bool ether_addr_equal(const u8 * addr1, const u8 * addr2)
{
#if defined(CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS)
	u32 fold = ((*(const u32 *)addr1) ^ (*(const u32 *)addr2)) |
	    ((*(const u16 *)(addr1 + 4)) ^ (*(const u16 *)(addr2 + 4)));

	return fold == 0;
#else
	const u16 *a = (const u16 *)addr1;
	const u16 *b = (const u16 *)addr2;

	return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | (a[2] ^ b[2])) == 0;
#endif
}
#endif

/**
 * struct asr_csa - Information for CSA (Channel Switch Announcement)
 *
 * @vif: Pointer to the vif doing the CSA
 * @bcn: Beacon to use after CSA
 * @dma: DMA descriptor to send the new beacon to the fw
 * @chandef: defines the channel to use after the switch
 * @count: Current csa counter
 * @status: Status of the CSA at fw level
 * @ch_idx: Index of the new channel context
 * @work: work scheduled at the end of CSA
 */
struct asr_csa {
	struct asr_vif *vif;
	struct asr_bcn bcn;
	struct asr_dma_elem dma;
	struct cfg80211_chan_def chandef;
	int count;
	int status;
	int ch_idx;
	struct work_struct work;
};

/**
 * enum asr_ap_flags - AP flags
 *
 * @ASR_AP_ISOLATE Isolate clients (i.e. Don't brige packets transmitted by
 *                                   one client for another one)
 */
enum asr_ap_flags {
	ASR_AP_ISOLATE = BIT(0),
};

/*
 * Structure used to save information relative to the managed interfaces.
 * This is also linked within the asr_hw vifs list.
 *
 */
struct asr_vif {
	struct list_head list;
	struct asr_hw *asr_hw;
	struct wireless_dev wdev;
	struct net_device *ndev;
	struct net_device_stats net_stats;
	struct asr_key key[ASR_PAIRWISE_KEY_NUM];
	u8 drv_vif_index;	/* Identifier of the VIF in driver */
	u8 vif_index;		/* Identifier of the station in FW */
	u8 ch_index;		/* Channel context identifier */
	bool up;		/* Indicate if associated netdev is up
				   (i.e. Interface is created at fw level) */
	bool use_4addr;		/* Should we use 4addresses mode */
	bool is_resending;	/* Indicate if a frame is being resent on this interface */
	u8 auth_type;
	u16 auth_seq;
	u16 old_auth_transaction;
	u8 ssid[IEEE80211_MAX_SSID_LEN];
	u8 ssid_len;
	u8 bssid[ETH_ALEN];
	u32 generation;
#ifdef CONFIG_ASR_NAPI
    struct napi_struct napi;
    u8 napi_state;
    struct sk_buff_head rx_napi_skb_list;
    struct hrtimer rx_napi_hrtimer;
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 10, 0)
	struct ieee80211_channel ap_chan;
	struct cfg80211_chan_def ap_chandef;
#endif
    unsigned long dev_flags;
	bool is_roam;
	bool is_ap1_deauthed;
    u32 txring_bytes;     // bytes num in tx agg ring buf.
    u32 tx_skb_cnt;       // skb cnt used for flow ctrl in tx opt list
	union {
		struct {
			struct asr_sta *ap;	/* Pointer to the peer STA entry allocated for
						   the AP */
		} sta;
		struct {
			u16 flags;	/* see asr_ap_flags */
			struct list_head sta_list;	/* List of STA connected to the AP */
			struct asr_bcn bcn;	/* beacon */
			u8 bcmc_index;	/* Index of the BCMC sta to use */
			struct asr_csa *csa;
		} ap;
#ifdef CFG_SNIFFER_SUPPORT
		struct {
			struct ieee80211_channel st_chan;
			uint32_t rx_filter;
			enum nl80211_channel_type chan_type;
			uint8_t chan_num;
		} sniffer;
#endif
	};
};

#define ASR_VIF_TYPE(asr_vif) (asr_vif->wdev.iftype)

/**
 * Structure used to store information relative to PS mode.
 *
 * @active: True when the sta is in PS mode.
 *          If false, other values should be ignored
 * @pkt_ready: Number of packets buffered for the sta in drv's txq
 *             (1 counter for Legacy PS and 1 for U-APSD)
 * @sp_cnt: Number of packets that remain to be pushed in the service period.
 *          0 means that no service period is in progress
 *          (1 counter for Legacy PS and 1 for U-APSD)
 */
struct asr_sta_ps {
	bool active;
	u16 pkt_ready[2];
	u16 sp_cnt[2];
};

/**
 * struct asr_rx_rate_stats - Store statistics for RX rates
 *
 * @table: Table indicating how many frame has been receive which each
 * rate index. Rate index is the same as the one used by RC algo for TX
 * @size: Size of the table array
 * @cpt: number of frames received
 */
struct asr_rx_rate_stats {
	int *table;
	int size;
	int cpt;
#ifdef CONFIG_ASR595X
	int rate_cnt;
#endif
};

/**
 * struct asr_sta_stats - Structure Used to store statistics specific to a STA
 *
 * @last_rx: Hardware vector of the last received frame
 * @rx_rate: Statistics of the received rates
 */
struct asr_sta_stats {
	u32 rx_pkts;
	u32 tx_pkts;
	u64 rx_bytes;
	u64 tx_bytes;
	struct hw_vect last_rx;
	struct asr_rx_rate_stats rx_rate;
};

/*
 * Structure used to save information relative to the managed stations.
 */
struct asr_sta {
	struct list_head list;
	u16 aid;		/* association ID */
	u8 sta_idx;		/* Identifier of the station */
	u8 vif_idx;		/* Identifier of the VIF (fw id) the station
				   belongs to */
	u8 vlan_idx;		/* Identifier of the VLAN VIF (fw id) the station
				   belongs to (= vif_idx if no vlan in used) */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 1)
	enum nl80211_band band;	/* Band */
#else
	enum ieee80211_band band;	/* Band */
#endif
	enum nl80211_chan_width width;	/* Channel width */
	u16 center_freq;	/* Center frequency */
	u32 center_freq1;	/* Center frequency 1 */
	u32 center_freq2;	/* Center frequency 2 */
	u8 ch_idx;		/* Identifier of the channel
				   context the station belongs to */
	bool qos;		/* Flag indicating if the station
				   supports QoS */
	u8 acm;			/* Bitfield indicating which queues
				   have AC mandatory */
	u16 uapsd_tids;		/* Bitfield indicating which tids are subject to
				   UAPSD */
	u8 mac_addr[ETH_ALEN];	/* MAC address of the station */
	struct asr_key key;
	bool valid;		/* Flag indicating if the entry is valid */
	struct asr_sta_ps ps;	/* Information when STA is in PS (AP only) */

	bool ht;		/* Flag indicating if the station
				   supports HT */
	u32 ac_param[AC_MAX];	/* EDCA parameters */
	struct asr_sta_stats stats;
	u8 fw_tx_pkt;
	u8 ps_tx_pkt;
};

struct stats_rate{
	unsigned long tx_bytes;
	unsigned long tx_times;
	unsigned long tx_rates;
	unsigned long rx_bytes;
	unsigned long rx_times;
	unsigned long rx_rates;
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 61)
#define IEEE80211_MAX_AMPDU_BUF IEEE80211_MAX_AMPDU_BUF_HE
#endif

struct asr_stats {
	int cfm_balance[NX_TXQ_CNT];
	unsigned long last_rx, last_tx;	/* jiffies */
	int ampdus_tx[IEEE80211_MAX_AMPDU_BUF];
	int ampdus_rx[IEEE80211_MAX_AMPDU_BUF];
	int ampdus_rx_map[4];
	int ampdus_rx_miss;
	int amsdus_rx[64];
	u32 tx_ctlpkts;
	u32 tx_ctlerrs;
	u32 rx_ctlpkts;
	u32 rx_ctlerrs;
#ifdef ASR_STATS_RATES_TIMER
	struct stats_rate txrx_rates[TXRX_RATES_NUM];
	unsigned long tx_bytes;
	unsigned long rx_bytes;
	unsigned long last_rx_times;
#endif
};

struct asr_sec_phy_chan {
	u16 prim20_freq;
	u16 center_freq1;
	u16 center_freq2;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 1)
	enum nl80211_band band;
#else
	enum ieee80211_band band;
#endif
	u8 type;
};

/* Structure that will contains all RoC information received from cfg80211 */
struct asr_roc_elem {
	struct wireless_dev *wdev;
	struct ieee80211_channel *chan;
	unsigned int duration;
	/* Used to avoid call of CFG80211 callback upon expiration of RoC */
	bool mgmt_roc;
	/* Indicate if we have switch on the RoC channel */
	bool on_chan;
};

/* Structure containing channel survey information received from MAC */
struct asr_survey_info {
	// Filled
	u32 filled;
	// Amount of time in ms the radio spent on the channel
	u32 chan_time_ms;
	// Amount of time the primary channel was sensed busy
	u32 chan_time_busy_ms;
	// Noise in dbm
	s8 noise_dbm;
};

#define ASR_CH_NOT_SET 0xFF

/* Structure containing channel context information */
struct asr_chanctx {
	struct cfg80211_chan_def chan_def;	/* channel description */
	u8 count;		/* number of vif using this ctxt */
};

struct asr_tx_agg {
	struct sk_buff *aggr_buf;
	u16 last_aggr_buf_next_idx;
	u16 cur_aggr_buf_next_idx;
	u8 *last_aggr_buf_next_addr;
	u8 *cur_aggr_buf_next_addr;
	u16 aggr_buf_cnt;
};
struct asr_skbuff_cb {
	struct asr_vif *asr_vif;
};
#define asr_skbcb(skb)        ((struct asr_skbuff_cb *)((skb)->cb))

/** main task source */
enum {
	SDIO_THREAD,
	SDIO_ISR
};

struct asr_global_param {
	bool dev_reset_start;
	bool dev_driver_remove;
#ifdef	CONFIG_ASR_USB
	int dev_driver_remove_cnt;
#endif
	struct completion reset_complete;
	struct asr_hw *asr_hw;
	struct device *dev;
	struct pinctrl *asr_pinctrl;

#ifdef ASR_MODULE_RESET_SUPPORT
#ifdef CONFIG_PINCTRL
	struct pinctrl_state *reset_pins_on;
	struct pinctrl_state *reset_pins_off;
#endif
	int reset_pin;
#endif

#ifdef ASR_MODULE_POWER_PIN_SUPPORT
#ifdef CONFIG_PINCTRL
	struct pinctrl_state *power_pins_on;
	struct pinctrl_state *power_pins_off;
#endif
	int power_pin;
#endif

#ifdef OOB_INTR_ONLY
	int oob_intr_pin;
#endif

#ifdef ASR_BOOT_TO_RTS_PIN_SUPPORT
	struct pinctrl_state *boot_pins_uart;
	struct pinctrl_state *boot_pins_gpio;
#endif

	/*for issue mmc card_detection interrupt */
	struct mmc_host *mmc;
	int sdio_send_times;
};

struct asr_work_ctx {
	struct work_struct real_work;
	struct asr_hw *asr_hw;
	struct asr_vif *asr_vif;
	struct asr_sta *sta;
	u32 parm1;
};

struct asr_hw {
	struct asr_mod_params *mod_params;

#ifdef CONFIG_ASR_SDIO
	u8 sdio_reg[68];
	u8 last_sdio_regs[SDIO_REG_READ_LENGTH];
	u8 sdio_ireg;
	u8 *sdio_reg_buff;

    // tx agg buf related.
	struct asr_tx_agg tx_agg_env;
	spinlock_t tx_agg_env_lock;

	// tx opt use tx_sk_list
	struct sk_buff_head tx_sk_list;             // use tx_lock to protect.
	struct sk_buff_head tx_hif_free_buf_list;
	struct sk_buff_head tx_hif_skb_list;
#endif

	bool use_phy_bw_tweaks;
	struct device *dev;
	struct wiphy *wiphy;
	struct list_head vifs;
	struct asr_vif *vif_table[NX_VIRT_DEV_MAX];	/* indexed with fw id */
	struct asr_sta sta_table[NX_REMOTE_STA_MAX + NX_VIRT_DEV_MAX];
	struct asr_survey_info survey[SCAN_CHANNEL_MAX];
	struct cfg80211_scan_request *scan_request;
	u8 scan_vif_index;
	struct asr_chanctx chanctx_table[NX_CHAN_CTXT_CNT];
	u8 cur_chanctx;

	/* RoC Management */
	struct asr_roc_elem *roc_elem;	/* Information provided by cfg80211 in its remain on channel request */
	u32 roc_cookie_cnt;	/* Counter used to identify RoC request sent by cfg80211 */

	struct asr_cmd_mgr cmd_mgr;

	unsigned long phy_flags;


#ifdef CONFIG_ASR_SDIO
	struct asr_plat *plat;
#else
	struct asr_usbdev_info *plat;
#endif

	struct workqueue_struct *asr_wq;
	struct work_struct datawork;

	struct ipc_host_env_tag *ipc_env;	/* store the IPC environment */

	spinlock_t tx_lock;
	spinlock_t cb_lock;

	volatile long unsigned int ulFlag;
	//struct task_struct *tx_thread;
	//wait_queue_head_t waitq_tx_thead;

	struct task_struct *main_task_thread;
	wait_queue_head_t waitq_main_task_thead;

	#ifdef OOB_INTR_ONLY
	struct task_struct *oob_intr_thread;
	wait_queue_head_t waitq_oob_intr_thead;
	#endif

	struct task_struct *rx_to_os_thread;
	wait_queue_head_t waitq_rx_to_os_thead;

	struct mm_version_cfm version_cfm;	/* Lower layers versions - obtained via MM_VERSION_REQ */

	u32 tcp_pacing_shift;
	struct asr_ipc_dbgdump_elem dbgdump_elem;
	struct asr_debugfs debugfs;

	struct asr_txq txq[NX_NB_TXQ];
	struct asr_hwq hwq[NX_TXQ_CNT];
	struct asr_sec_phy_chan sec_phy_chan;
	u8 phy_cnt;
	u8 chan_ctxt_req;
	u8 avail_idx_map;
	u8 vif_started;
	bool adding_sta;
	struct phy_cfg_tag phy_config;

	/* extended capabilities supported */
	u8 ext_capa[10];

	//extend
	char country[2];

#ifdef CFG_SNIFFER_SUPPORT
	uint8_t monitor_vif_idx;
#endif

	u8 mac_addr[ETH_ALEN];

	struct sk_buff_head rx_data_sk_list;   // sdio skb list for data rx, may not use when asr_sdio_rw_sg is enable.
	struct sk_buff_head rx_msg_sk_list;    // sdio skb list for msg rx.
	struct sk_buff_head rx_log_sk_list;    // sdio skb list for log rx.

	struct sk_buff_head rx_sk_split_list;

	#ifdef SDIO_DEAGGR
	struct sk_buff_head rx_sk_sdio_deaggr_list;
	struct sk_buff_head rx_sk_sdio_deaggr_amsdu_list;
	struct sk_buff_head rx_sdio_sg_list;
	#endif

	struct sk_buff_head rx_to_os_skb_list;
	struct sk_buff_head rx_pending_skb_list;


	volatile u16 tx_use_bitmap;
	volatile u8 tx_data_cur_idx;
	volatile u8 rx_data_cur_idx;
	volatile u16 tx_last_trans_bitmap;

	int ioport;

	struct timer_list rx_thread_timer;	/* reclaims TX buffers */
	struct timer_list rx_period_timer;

#ifdef CONFIG_ASR_KEY_DBG
	struct timer_list tx_status_timer;	//uesed to detect tx status
#endif

	struct hrtimer tx_evt_hrtimer;
	bool ps_on;		//just record power save state from user call
	bool usr_cmd_ps;// user cmd set ps mode

#if 1
	spinlock_t int_reg_lock;
	bool mlan_processing;
	u8 main_task_from_type;
	bool more_task_flag;
	bool restart_flag;
	spinlock_t pmain_proc_lock;
	spinlock_t tx_msg_lock;
#else
	atomic_t mlan_processing;
	atomic_t more_task_flag;
#endif

	bool host_int_upld;

#ifdef CONFIG_POWER_SAVE
	int wakeup_gpio;
#endif

#ifdef OOB_INTR_ONLY
    int oob_intr_gpio;
#endif

	struct wifi_mib *pmib;
	struct mm_fw_softversion_cfm fw_softversion_cfm;
	struct mm_hif_sdio_info_ind hif_sdio_info_ind;

#ifdef CONFIG_PM
    /** Device suspend flag */
	bool is_suspended;
    /** suspend notify flag */
	bool suspend_notify_req;
    /** Host Sleep activated flag */
	u8 hs_activated;
    /** Host Sleep activated event wait queue token */
	u16 hs_activate_wait_q_woken;
    /** Host Sleep activated event wait queue */
	wait_queue_head_t hs_activate_wait_q;
    /** hs skip count */
	u32 hs_skip_count;
    /** hs force count */
	u32 hs_force_count;
    /** suspend_fail flag */
	bool suspend_fail;
#endif

	struct asr_work_ctx rx_deauth_work;

#ifdef CONFIG_ASR_SDIO
#ifdef ASR_MODULE_RESET_SUPPORT
	struct asr_work_ctx dev_restart_work;
#endif
#endif

	struct mutex tx_msg_mutex;
	u8 vif_index;
	/** extend vif index, used for cfg . */
	u8 ext_vif_index;

#ifdef ASR_HEARTBEAT_DETECT
	struct timer_list heartbeat_timer;
	struct asr_work_ctx heartbeat_work;
#endif
	struct timer_list scan_cmd_timer;
	struct asr_work_ctx cmd_crash_work;
#ifdef ASR_STATS_RATES_TIMER
	struct timer_list statsrates_timer;
	struct asr_work_ctx statsrates_work;
#endif

#ifdef ASR_DRV_DEBUG_TIMER
	struct timer_list drvdebug_timer;
	struct asr_work_ctx drvdebug_work;
#endif

	struct task_struct *msgind_task_thread;
	wait_queue_head_t waitq_msgind_task_thead;
	struct sk_buff_head msgind_task_skb_list;

#ifdef CONFIG_ASR_SDIO
	struct timer_list txflow_timer;
#endif

	u8 vif_max_num;
	u8 sta_max_num;
	u8 driver_mode;

#ifdef CONFIG_ASR_USB
	u8 usb_remove_flag;
#endif

	struct asr_stats stats;

#ifdef ASR_REDUCE_TCP_ACK
	struct timer_list tcp_ack_timer;
	struct asr_work_ctx tcp_ack_work;
	uint8_t recvd_tcp_ack_count;
	struct sk_buff* saved_tcp_ack_sdk;
	uint8_t first_ack;
	spinlock_t tcp_ack_lock;
#endif
	struct timer_list ate_at_cmd_timer;
	struct asr_work_ctx ate_at_cmd_work;
	spinlock_t ate_at_cmd_lock;

	struct asr_work_ctx sta_deauth_work[NX_REMOTE_STA_MAX];
	bool scan_reverse;//reverse order scan

};

struct asr_traffic_status {
    bool send;
	struct asr_sta *asr_sta_ps;
	bool tx_ava;
	u8 ps_id_bits;
};

//extern param
extern struct asr_global_param g_asr_para;

u8 *asr_build_bcn(struct asr_bcn *bcn
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 4, 0)
		  , struct cfg80211_beacon_data *new
#else
		  , struct beacon_parameters *new
#endif
    );

void asr_chanctx_link(struct asr_vif *vif, u8 idx, struct cfg80211_chan_def *chandef);
void asr_chanctx_unlink(struct asr_vif *vif);
int asr_chanctx_valid(struct asr_hw *asr_hw, u8 idx);

static inline bool is_multicast_sta(struct asr_hw *asr_hw, int sta_idx)
{
	return (sta_idx >= asr_hw->sta_max_num);
}

static inline u8 master_vif_idx(struct asr_vif *vif)
{
	return vif->vif_index;
}

#define asr_atomic_inc(ptr_atomic_t)        atomic_inc(ptr_atomic_t)
#define asr_atomic_dec(ptr_atomic_t)        atomic_dec(ptr_atomic_t)
#define asr_atomic_read(ptr_atomic_t)        atomic_read(ptr_atomic_t)
#define asr_atomic_set(ptr_atomic_t, i)    atomic_set(ptr_atomic_t,i)

void asr_dev_restart_handler(struct asr_hw *asr_hw, u32 reason);
bool check_is_dhcp_package(struct asr_vif *asr_vif, bool is_tx, u16 eth_proto, u8 * type, u8 ** d_mac, u8 * data,
			   u32 data_len);

#endif /* _ASR_DEFS_H_ */
