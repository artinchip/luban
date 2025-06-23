/**
 ****************************************************************************************
 *
 * @file lmac_msg.h
 *
 * @brief Main definitions for message exchanges with LMAC
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef LMAC_MSG_H_
#define LMAC_MSG_H_

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
// for MAC related elements (mac_addr, mac_ssid...)
#include "lmac_mac.h"

/// Maximum length of the additional ProbeReq IEs
#define SCANU_MAX_IE_LEN  200
/// Maximum size of a beacon frame
#define NX_BCNFRAME_LEN   512

#define WIFI_CONFIG_PWR_NUM			28

#define FRAM_CUSTOM_APPIE_LEN 	50

struct cca_config_info {
	bool cca_valid;
	s8 cca_threshold;
	s8 cca_prise_thr;
	s8 cca_pfall_thr;
	s8 cca_srise_thr;
	s8 cca_sfall_thr;
};

struct asr_wifi_config {
	u8 mac_addr[6];
	s8 tx_pwr[WIFI_CONFIG_PWR_NUM];
	bool pwr_config;
	struct cca_config_info cca_config;
	u32 edca_bk;
	u32 edca_be;
	u32 edca_vi;
	u32 edca_vo;
};

extern struct asr_wifi_config g_wifi_config;

/*
 ****************************************************************************************
 */
/////////////////////////////////////////////////////////////////////////////////
// COMMUNICATION WITH LMAC LAYER
/////////////////////////////////////////////////////////////////////////////////
/* Task identifiers for communication between LMAC and DRIVER */
enum {
	TASK_NONE = (u8) - 1,

	// MAC Management task.
	TASK_MM = 0,
	// DEBUG task
	TASK_DBG,
	/// SCAN task
	TASK_SCAN,
	/// TDLS task
	TASK_TDLS,
	/// SCANU task
	TASK_SCANU,
	/// ME task
	TASK_ME,
	/// SM task
	TASK_SM,
	/// APM task
	TASK_APM,
	/// BAM task
	TASK_BAM,
	/// MESH task
	TASK_MESH,
#if defined(CONFIG_ASR595X)
	/// TWT task
	TASK_TWT,
#endif
	/// RXU task
	TASK_RXU,
	// This is used to define the last task that is running on the EMB processor
	TASK_LAST_EMB = TASK_RXU,
	// nX API task
	TASK_API,
	TASK_MAX,
};

/// For MAC HW States copied from "hal_machw.h"
enum {
	/// MAC HW IDLE State.
	HW_IDLE = 0,
	/// MAC HW RESERVED State.
	HW_RESERVED,
	/// MAC HW DOZE State.
	HW_DOZE,
	/// MAC HW ACTIVE State.
	HW_ACTIVE
};

/// Power Save mode setting
enum mm_ps_mode_state {
	MM_PS_MODE_OFF,
	MM_PS_MODE_ON,
	MM_PS_MODE_ON_DYN,
};

/// Status/error codes used in the MAC software.
enum {
	CO_OK,
	CO_FAIL,
	CO_EMPTY,
	CO_FULL,
	CO_BAD_PARAM,
	CO_NOT_FOUND,
	CO_NO_MORE_ELT_AVAILABLE,
	CO_NO_ELT_IN_USE,
	CO_BUSY,
	CO_OP_IN_PROGRESS,
};

/// Remain on channel operation codes
enum mm_remain_on_channel_op {
	MM_ROC_OP_START = 0,
	MM_ROC_OP_CANCEL,
};

#define DRV_TASK_ID 100

/// Message Identifier. The number of messages is limited to 0xFFFF.
/// The message ID is divided in two parts:
/// - bits[15..10] : task index (no more than 64 tasks supported).
/// - bits[9..0] : message index (no more that 1024 messages per task).
typedef u16 lmac_msg_id_t;

typedef u16 lmac_task_id_t;

/// Build the first message ID of a task.
#define LMAC_FIRST_MSG(task) ((lmac_msg_id_t)((task) << 10))

#define MSG_T(msg) ((lmac_task_id_t)((msg) >> 10))
#define MSG_I(msg) ((msg) & ((1<<10)-1))

/// The two following definitions are needed for message structure consistency
/// Structure of a list element header
struct co_list_hdr {
	struct co_list_hdr *next;
};

/// Structure of a list
struct co_list {
	/// pointer to first element of the list
	struct co_list_hdr *first;
	/// pointer to the last element
	struct co_list_hdr *last;
	/// number of element in the list
	u32 cnt;

//      #if NX_DEBUG
//      /// max number of element in the list
//      u32 maxcnt;
//      /// min number of element in the list
//      u32 mincnt;
//      #endif // NX_DEBUG
};

/// Message structure.
struct lmac_msg {
	u32 dummy;
	lmac_msg_id_t id;	///< Message id.
	lmac_task_id_t dest_id;	///< Destination kernel identifier.
	lmac_task_id_t src_id;	///< Source kernel identifier.
	u16 param_len;		///< Parameter embedded struct length.
	u32 param[];		///< Parameter embedded struct. Must be word-aligned.
};

/// List of messages related to the task.
enum mm_msg_tag {
	/// RESET Request.
	MM_RESET_REQ = LMAC_FIRST_MSG(TASK_MM),
	/// RESET Confirmation.
	MM_RESET_CFM,
	/// START Request.
	MM_START_REQ,
	/// START Confirmation.
	MM_START_CFM,
	/// Read Version Request.
	MM_VERSION_REQ,
	/// Read Version Confirmation.
	MM_VERSION_CFM,
	/// ADD INTERFACE Request.
	MM_ADD_IF_REQ,
	/// ADD INTERFACE Confirmation.
	MM_ADD_IF_CFM,
	/// REMOVE INTERFACE Request.
	MM_REMOVE_IF_REQ,
	/// REMOVE INTERFACE Confirmation.
	MM_REMOVE_IF_CFM,
	/// STA ADD Request.
	MM_STA_ADD_REQ,		//-------10----------
	/// STA ADD Confirm.
	MM_STA_ADD_CFM,
	/// STA DEL Request.
	MM_STA_DEL_REQ,
	/// STA DEL Confirm.
	MM_STA_DEL_CFM,
	/// RX FILTER CONFIGURATION Request.
	MM_SET_FILTER_REQ,
	/// RX FILTER CONFIGURATION Confirmation.
	MM_SET_FILTER_CFM,
	/// CHANNEL CONFIGURATION Request.
	MM_SET_CHANNEL_REQ,
	/// CHANNEL CONFIGURATION Confirmation.
	MM_SET_CHANNEL_CFM,
	/// DTIM PERIOD CONFIGURATION Request.
	MM_SET_DTIM_REQ,
	/// DTIM PERIOD CONFIGURATION Confirmation.
	MM_SET_DTIM_CFM,
	/// BEACON INTERVAL CONFIGURATION Request.
	MM_SET_BEACON_INT_REQ,	//-------20----------
	/// BEACON INTERVAL CONFIGURATION Confirmation.
	MM_SET_BEACON_INT_CFM,
	/// BASIC RATES CONFIGURATION Request.
	MM_SET_BASIC_RATES_REQ,
	/// BASIC RATES CONFIGURATION Confirmation.
	MM_SET_BASIC_RATES_CFM,
	/// BSSID CONFIGURATION Request.
	MM_SET_BSSID_REQ,
	/// BSSID CONFIGURATION Confirmation.
	MM_SET_BSSID_CFM,
	/// EDCA PARAMETERS CONFIGURATION Request.
	MM_SET_EDCA_REQ,
	/// EDCA PARAMETERS CONFIGURATION Confirmation.
	MM_SET_EDCA_CFM,
	/// ABGN MODE CONFIGURATION Request.
	MM_SET_MODE_REQ,
	/// ABGN MODE CONFIGURATION Confirmation.
	MM_SET_MODE_CFM,
	/// Request setting the VIF active state (i.e associated or AP started)
	MM_SET_VIF_STATE_REQ,	//-------30----------
	/// Confirmation of the @ref MM_SET_VIF_STATE_REQ message.
	MM_SET_VIF_STATE_CFM,
	/// SLOT TIME PARAMETERS CONFIGURATION Request.
	MM_SET_SLOTTIME_REQ,
	/// SLOT TIME PARAMETERS CONFIGURATION Confirmation.
	MM_SET_SLOTTIME_CFM,
	/// Power Mode Change Request.
	MM_SET_IDLE_REQ,
	/// Power Mode Change Confirm.
	MM_SET_IDLE_CFM,
	/// KEY ADD Request.
	MM_KEY_ADD_REQ,
	/// KEY ADD Confirm.
	MM_KEY_ADD_CFM,
	/// KEY DEL Request.
	MM_KEY_DEL_REQ,
	/// KEY DEL Confirm.
	MM_KEY_DEL_CFM,
	/// Block Ack agreement info addition
	MM_BA_ADD_REQ,		//-------40----------
	/// Block Ack agreement info addition confirmation
	MM_BA_ADD_CFM,
	/// Block Ack agreement info deletion
	MM_BA_DEL_REQ,
	/// Block Ack agreement info deletion confirmation
	MM_BA_DEL_CFM,
	/// Indication of the primary TBTT to the upper MAC. Upon the reception of this
	// message the upper MAC has to push the beacon(s) to the beacon transmission queue.
	MM_PRIMARY_TBTT_IND,
	/// Indication of the secondary TBTT to the upper MAC. Upon the reception of this
	// message the upper MAC has to push the beacon(s) to the beacon transmission queue.
	MM_SECONDARY_TBTT_IND,
	/// Request for changing the TX power
	MM_SET_POWER_REQ,
	/// Confirmation of the TX power change
	MM_SET_POWER_CFM,
	/// Request to the LMAC to trigger the embedded logic analyzer and forward the debug
	/// dump.
	MM_DBG_TRIGGER_REQ,
	/// Set Power Save mode
	MM_SET_PS_MODE_REQ,
	/// Set Power Save mode confirmation
	MM_SET_PS_MODE_CFM,	//-------50----------
	/// Request to add a channel context
	MM_CHAN_CTXT_ADD_REQ,
	/// Confirmation of the channel context addition
	MM_CHAN_CTXT_ADD_CFM,
	/// Request to delete a channel context
	MM_CHAN_CTXT_DEL_REQ,
	/// Confirmation of the channel context deletion
	MM_CHAN_CTXT_DEL_CFM,
	/// Request to link a channel context to a VIF
	MM_CHAN_CTXT_LINK_REQ,
	/// Confirmation of the channel context link
	MM_CHAN_CTXT_LINK_CFM,
	/// Request to unlink a channel context from a VIF
	MM_CHAN_CTXT_UNLINK_REQ,
	/// Confirmation of the channel context unlink
	MM_CHAN_CTXT_UNLINK_CFM,
	/// Request to update a channel context
	MM_CHAN_CTXT_UPDATE_REQ,
	/// Confirmation of the channel context update
	MM_CHAN_CTXT_UPDATE_CFM,	//-------60----------
	/// Request to schedule a channel context
	MM_CHAN_CTXT_SCHED_REQ,
	/// Confirmation of the channel context scheduling
	MM_CHAN_CTXT_SCHED_CFM,
	/// Request to change the beacon template in LMAC
	MM_BCN_CHANGE_REQ,
	/// Confirmation of the beacon change
	MM_BCN_CHANGE_CFM,
	/// Request to update the TIM in the beacon (i.e to indicate traffic bufferized at AP)
	MM_TIM_UPDATE_REQ,
	/// Confirmation of the TIM update
	MM_TIM_UPDATE_CFM,
	/// Connection loss indication
	MM_CONNECTION_LOSS_IND,
	/// Channel context switch indication to the upper layers
	MM_CHANNEL_SWITCH_IND,
	/// Channel context pre-switch indication to the upper layers
	MM_CHANNEL_PRE_SWITCH_IND,
	/// Request to remain on channel or cancel remain on channel
	MM_REMAIN_ON_CHANNEL_REQ,	//-------70----------
	/// Confirmation of the (cancel) remain on channel request
	MM_REMAIN_ON_CHANNEL_CFM,
	/// Remain on channel expired indication
	MM_REMAIN_ON_CHANNEL_EXP_IND,
	/// Indication of a PS state change of a peer device
	MM_PS_CHANGE_IND,
	/// Indication that some buffered traffic should be sent to the peer device
	MM_TRAFFIC_REQ_IND,
	/// Request to modify the STA Power-save mode options
	MM_SET_PS_OPTIONS_REQ,
	/// Confirmation of the PS options setting
	MM_SET_PS_OPTIONS_CFM,
	/// Indication of PS state change for a P2P VIF
	MM_P2P_VIF_PS_CHANGE_IND,
	/// Indication that CSA counter has been updated
	MM_CSA_COUNTER_IND,
	/// Channel occupation report indication
	MM_CHANNEL_SURVEY_IND,
	/// Message containing Beamformer Information
	MM_BFMER_ENABLE_REQ,	//-------80----------
	/// Request to Start/Stop/Update NOA - GO Only
	MM_SET_P2P_NOA_REQ,
	/// Request to Start/Stop/Update Opportunistic PS - GO Only
	MM_SET_P2P_OPPPS_REQ,
	/// Start/Stop/Update NOA Confirmation
	MM_SET_P2P_NOA_CFM,
	/// Start/Stop/Update Opportunistic PS Confirmation
	MM_SET_P2P_OPPPS_CFM,
	/// P2P NoA Update Indication - GO Only
	MM_P2P_NOA_UPD_IND,
	/// Request to set RSSI threshold and RSSI hysteresis
	MM_CFG_RSSI_REQ,
	/// Indication that RSSI level is below or above the threshold
	MM_RSSI_STATUS_IND,
	/// Indication that CSA is done
	MM_CSA_FINISH_IND,
	/// Indication that CSA is in prorgess (resp. done) and traffic must be stopped (resp. restarted)
	MM_CSA_TRAFFIC_IND,
	/// Request to update the group information of a station
	MM_MU_GROUP_UPDATE_REQ,	//-------90----------
	/// Confirmation of the @ref MM_MU_GROUP_UPDATE_REQ message
	MM_MU_GROUP_UPDATE_CFM,
	// Request to enable RSSI level indication
	MM_CFG_RSSI_LEVEL_REQ,	//92
	// Indication that RSSI level is changed
	MM_RSSI_LEVEL_IND,	//93
	// request fw software version
	MM_FW_SOFTVERSION_REQ,
	// Confirmation fw software version
	MM_FW_SOFTVERSION_CFM,
	// request fw macaddr
	MM_FW_MAC_ADDR_REQ,
	// Confirmation fw macaddr
	MM_FW_MAC_ADDR_CFM,
	// request set fw macaddr
	MM_SET_FW_MAC_ADDR_REQ,
	// Confirmation set fw macaddr
	MM_SET_FW_MAC_ADDR_CFM,
#ifdef CONFIG_ASR_SDIO
	// request sdio info
	MM_HIF_SDIO_INFO_REQ,	//-------100----------
	// Confirmation sdio info
	MM_HIF_SDIO_INFO_IND,
#endif
	/// Request to initialize the antenna diversity algorithm
	MM_ANT_DIV_INIT_REQ,
	/// Request to stop the antenna diversity algorithm
	MM_ANT_DIV_STOP_REQ,
	/// Request to update the antenna switch status
	MM_ANT_DIV_UPDATE_REQ,
	/// Request to switch the antenna connected to path_0
	MM_SWITCH_ANTENNA_REQ,
#ifdef CONFIG_ASR595X
	/// Indication that a packet loss has occurred
	MM_PKTLOSS_IND,
	/// MU EDCA PARAMETERS Configuration Request.
	MM_SET_MU_EDCA_REQ,
	/// MU EDCA PARAMETERS Configuration Confirmation.
	MM_SET_MU_EDCA_CFM,
	/// UORA PARAMETERS Configuration Request.
	MM_SET_UORA_REQ,
	/// UORA PARAMETERS Configuration Confirmation.
	MM_SET_UORA_CFM,	//-------110----------
	/// TXOP RTS THRESHOLD Configuration Request.
	MM_SET_TXOP_RTS_THRES_REQ,
	/// TXOP RTS THRESHOLD Configuration Confirmation.
	MM_SET_TXOP_RTS_THRES_CFM,
	/// HE BSS Color Configuration Request.
	MM_SET_BSS_COLOR_REQ,
	/// HE BSS Color Configuration Confirmation.
	MM_SET_BSS_COLOR_CFM,
	/// Indication that station is outside of twt sp and traffic must be stopped
	MM_TWT_TRAFFIC_IND,	//-------115----------
#endif
	/// Request fw mm info
	MM_GET_INFO_REQ,
	/// Confirmation fw mm info
	MM_GET_INFO_CFM,
	/// Request set tx pwr of rate
	MM_SET_TX_PWR_RATE_REQ,
	/// Confirmation set tx pwr of rate
	MM_SET_TX_PWR_RATE_CFM,
	/// Request set cca thr
	MM_SET_GET_CCA_THR_REQ,
	/// Confirmation set cca thr
	MM_SET_GET_CCA_THR_CFM,
	// Request get rssi
	MM_GET_RSSI_REQ,
	// Confirmation get rssi
	MM_GET_RSSI_CFM,
	// Request set upload fram
	MM_SET_UPLOAD_FRAM_REQ,
	// Confirmation set upload fram
	MM_SET_UPLOAD_FRAM_CFM,
	// Request set fram appie
	MM_SET_FRAM_APPIE_REQ,
	// Confirmation set fram appie
	MM_SET_FRAM_APPIE_CFM,
	// Indication ap mode that station is disconnected
	MM_STA_LINK_LOSS_IND,
	// Request set efuse txpower
	MM_SET_EFUSE_TXPWR_REQ,
	// Confirmation set efuse txpower
	MM_SET_EFUSE_TXPWR_CFM,


#ifdef CONFIG_HW_MIB_TABLE
	/// Request reset HW MIB table
	MM_RESET_HW_MIB_TABLE_REQ,
	/// Confirmation reset HW MIB table
	MM_RESET_HW_MIB_TABLE_CFM,
	/// Request get HW MIB table
	MM_GET_HW_MIB_TABLE_REQ,
	/// Confirmation get HW MIB table
	MM_GET_HW_MIB_TABLE_CFM,
#endif
	/// MAX number of messages
	MM_MAX,
};

/// Interface types
enum {
	/// ESS STA interface
	MM_STA,
	/// IBSS STA interface
	MM_IBSS,
	/// AP interface
	MM_AP,
	// Mesh Point interface
	MM_MESH_POINT,
#if (defined CFG_SNIFFER_SUPPORT || defined CFG_CUS_FRAME)
	// Idle interface
	MM_MONITOR_MODE,
#endif
};

///BA agreement types
enum {
	///BlockAck agreement for TX
	BA_AGMT_TX,
	///BlockAck agreement for RX
	BA_AGMT_RX,
};

///BA agreement related status
enum {
	///Correct BA agreement establishment
	BA_AGMT_ESTABLISHED,
	///BA agreement already exists for STA+TID requested, cannot override it (should have been deleted first)
	BA_AGMT_ALREADY_EXISTS,
	///Correct BA agreement deletion
	BA_AGMT_DELETED,
	///BA agreement for the (STA, TID) doesn't exist so nothing to delete
	BA_AGMT_DOESNT_EXIST,
	/// No more BA agreement can be added for the specified type
	BA_AGMT_NO_MORE_BA_AGMT,
	/// BA agreement type not supported
	BA_AGMT_NOT_SUPPORTED
};

/// Features supported by LMAC - Positions
enum mm_features {
	/// Beaconing
	MM_FEAT_BCN_BIT = 0,
	/// Autonomous Beacon Transmission
	MM_FEAT_AUTOBCN_BIT,
	/// Scan in LMAC
	MM_FEAT_HWSCAN_BIT,
	/// Connection Monitoring
	MM_FEAT_CMON_BIT,
	/// Multi Role
	MM_FEAT_MROLE_BIT,
	/// Radar Detection
	MM_FEAT_RADAR_BIT,
	/// Power Save
	MM_FEAT_PS_BIT,
	/// UAPSD
	MM_FEAT_UAPSD_BIT,
	/// DPSM
	MM_FEAT_DPSM_BIT,
	/// A-MPDU
	MM_FEAT_AMPDU_BIT,
	/// A-MSDU
	MM_FEAT_AMSDU_BIT,
	/// Channel Context
	MM_FEAT_CHNL_CTXT_BIT,
	/// Packet reordering
	MM_FEAT_REORD_BIT,
	/// P2P
	MM_FEAT_P2P_BIT,
	/// P2P Go
	MM_FEAT_P2P_GO_BIT,
	/// UMAC Present
	MM_FEAT_UMAC_BIT,
	/// VHT support
	MM_FEAT_VHT_BIT,
	/// Beamformee
	MM_FEAT_BFMEE_BIT,
	/// Beamformer
	MM_FEAT_BFMER_BIT,
	/// WAPI
	MM_FEAT_WAPI_BIT,
	/// MFP
	MM_FEAT_MFP_BIT,
	/// Mu-MIMO RX support
	MM_FEAT_MU_MIMO_RX_BIT,
	/// Mu-MIMO TX support
	MM_FEAT_MU_MIMO_TX_BIT,
	/// Wireless Mesh Networking
	MM_FEAT_MESH_BIT,
	/// TDLS support
	MM_FEAT_TDLS_BIT,
	/// Antenna Diversity support
	MM_FEAT_ANT_DIV_BIT,
#ifdef CONFIG_ASR595X
	/// UF support
	MM_FEAT_UF_BIT,
	/// A-MSDU maximum size (bit0)
	MM_AMSDU_MAX_SIZE_BIT0,
	/// A-MSDU maximum size (bit1)
	MM_AMSDU_MAX_SIZE_BIT1,
	/// MON_DATA support
	MM_FEAT_MON_DATA_BIT,
	/// HE (802.11ax) support
	MM_FEAT_HE_BIT,
#endif
};

/// Maximum number of words in the configuration buffer
#define PHY_CFG_BUF_SIZE     16

/// Structure containing the parameters of the PHY configuration
struct phy_cfg_tag {
	/// Buffer containing the parameters specific for the PHY used
	u32 parameters[PHY_CFG_BUF_SIZE];
};

/// Structure containing the parameters of the Trident PHY configuration
struct phy_trd_cfg_tag {
	/// MDM type(nxm)(upper nibble) and MDM2RF path mapping(lower nibble)
	u8 path_mapping;
	/// TX DC offset compensation
	u32 tx_dc_off_comp;
};

/// Structure containing the parameters of the Karst PHY configuration
struct phy_karst_cfg_tag {
	/// TX IQ mismatch compensation in 2.4GHz
	u32 tx_iq_comp_2_4G[2];
	/// RX IQ mismatch compensation in 2.4GHz
	u32 rx_iq_comp_2_4G[2];
	/// TX IQ mismatch compensation in 5GHz
	u32 tx_iq_comp_5G[2];
	/// RX IQ mismatch compensation in 5GHz
	u32 rx_iq_comp_5G[2];
	/// RF path used by default (0 or 1)
	u8 path_used;
};

/// Structure containing the parameters of the @ref MM_START_REQ message
struct mm_start_req {
	/// PHY configuration
	struct phy_cfg_tag phy_cfg;
	/// UAPSD timeout
	u32 uapsd_timeout;
	/// Local LP clock accuracy (in ppm)
	u16 lp_clk_accuracy;
};

/// Structure containing the parameters of the @ref MM_SET_CHANNEL_REQ message
#ifdef CONFIG_ASR595X
/// Channel Flag
enum mac_chan_flags {
	/// Cannot initiate radiation on this channel
	CHAN_NO_IR = BIT(0),
	/// Channel is not allowed
	CHAN_DISABLED = BIT(1),
	/// Radar detection required on this channel
	CHAN_RADAR = BIT(2),
};
/// Operating Channel
struct mac_chan_op {
	/// Band (@ref mac_chan_band)
	uint8_t band;
	/// Channel type (@ref mac_chan_bandwidth)
	uint8_t type;
	/// Frequency for Primary 20MHz channel (in MHz)
	uint16_t prim20_freq;
	/// Frequency center of the contiguous channel or center of Primary 80+80 (in MHz)
	uint16_t center1_freq;
	/// Frequency center of the non-contiguous secondary 80+80 (in MHz)
	uint16_t center2_freq;
	/// Max transmit power allowed on this channel (dBm)
	int8_t tx_power;
	/// Additional information (@ref mac_chan_flags)
	uint8_t flags;
};
struct mm_set_channel_req {
	/// Channel information
	struct mac_chan_op chan;
	/// Index of the RF for which the channel has to be set (0: operating (primary), 1: secondary
	/// RF (used for additional radar detection). This parameter is reserved if no secondary RF
	/// is available in the system
	uint8_t index;
};
#else
struct mm_set_channel_req {
	/// Band (2.4GHz or 5GHz)
	u8 band;
	/// Channel type: 20,40,80,160 or 80+80 MHz
	u8 type;
	/// Frequency for Primary 20MHz channel (in MHz)
	u16 prim20_freq;
	/// Frequency for Center of the contiguous channel or center of Primary 80+80
	u16 center1_freq;
	/// Frequency for Center of the non-contiguous secondary 80+80
	u16 center2_freq;
	/// Index of the RF for which the channel has to be set (0: operating (primary), 1: secondary
	/// RF (used for additional radar detection). This parameter is reserved if no secondary RF
	/// is available in the system
	u8 index;
	/// Max tx power for this channel
	s8 tx_power;
};
#endif

/// Structure containing the parameters of the @ref MM_SET_CHANNEL_CFM message
struct mm_set_channel_cfm {
	/// Radio index to be used in policy table
	u8 radio_idx;
	/// TX power configured (in dBm)
	s8 power;
};

/// Structure containing the parameters of the @ref MM_SET_DTIM_REQ message
struct mm_set_dtim_req {
	/// DTIM period
	u8 dtim_period;
};

/// Structure containing the parameters of the @ref MM_SET_POWER_REQ message
struct mm_set_power_req {
	/// Index of the interface for which the parameter is configured
	u8 inst_nbr;
	/// TX power (in dBm)
	s8 power;
};

/// Structure containing the parameters of the @ref MM_SET_POWER_CFM message
struct mm_set_power_cfm {
	/// Radio index to be used in policy table
	u8 radio_idx;
	/// TX power configured (in dBm)
	s8 power;
};

/// Structure containing the parameters of the @ref MM_SET_BEACON_INT_REQ message
struct mm_set_beacon_int_req {
	/// Beacon interval
	u16 beacon_int;
	/// Index of the interface for which the parameter is configured
	u8 inst_nbr;
};

/// Structure containing the parameters of the @ref MM_SET_BASIC_RATES_REQ message
struct mm_set_basic_rates_req {
	/// Basic rate set (as expected by bssBasicRateSet field of Rates MAC HW register)
	u32 rates;
	/// Index of the interface for which the parameter is configured
	u8 inst_nbr;
	/// Band on which the interface will operate
	u8 band;
};

/// Structure containing the parameters of the @ref MM_SET_BSSID_REQ message
struct mm_set_bssid_req {
	/// BSSID to be configured in HW
	struct mac_addr bssid;
	/// Index of the interface for which the parameter is configured
	u8 inst_nbr;
};

/// Structure containing the parameters of the @ref MM_SET_FILTER_REQ message
struct mm_set_filter_req {
	/// RX filter to be put into rxCntrlReg HW register
	u32 filter;
};

/// Structure containing the parameters of the @ref MM_ADD_IF_REQ message.
struct mm_add_if_req {
	/// Type of the interface (AP, STA, ADHOC, ...)
	u8 type;
	/// MAC ADDR of the interface to start
	struct mac_addr addr;
	/// P2P Interface
	bool p2p;
};

/// Structure containing the parameters of the @ref MM_SET_EDCA_REQ message
struct mm_set_edca_req {
	/// EDCA parameters of the queue (as expected by edcaACxReg HW register)
	u32 ac_param;
	/// Flag indicating if UAPSD can be used on this queue
	bool uapsd;
	/// HW queue for which the parameters are configured
	u8 hw_queue;
	/// Index of the interface for which the parameters are configured
	u8 inst_nbr;
};

struct mm_set_idle_req {
	u8 hw_idle;
};

/// Structure containing the parameters of the @ref MM_SET_SLOTTIME_REQ message
struct mm_set_slottime_req {
	/// Slot time expressed in us
	u8 slottime;
};

/// Structure containing the parameters of the @ref MM_SET_MODE_REQ message
struct mm_set_mode_req {
	/// abgnMode field of macCntrl1Reg register
	u8 abgnmode;
};

/// Structure containing the parameters of the @ref MM_SET_VIF_STATE_REQ message
struct mm_set_vif_state_req {
	/// Association Id received from the AP (valid only if the VIF is of STA type)
	u16 aid;
	/// Flag indicating if the VIF is active or not
	bool active;
	/// Interface index
	u8 inst_nbr;
};

/// Structure containing the parameters of the @ref MM_ADD_IF_CFM message.
struct mm_get_info_cfm {
	/// Status of operation (different from 0 if unsuccessful)
	u8 status;
	u8 vif_num;
	u8 sta_num;
	u8 reserved1;
	u32 reserved2;
	u32 reserved3;
};

/// Structure containing the parameters of the @ref MM_ADD_IF_CFM message.
struct mm_add_if_cfm {
	/// Status of operation (different from 0 if unsuccessful)
	u8 status;
	/// Interface index assigned by the LMAC
	u8 inst_nbr;
};

/// Structure containing the parameters of the @ref MM_REMOVE_IF_REQ message.
struct mm_remove_if_req {
	/// Interface index assigned by the LMAC
	u8 inst_nbr;
};

/// Structure containing the parameters of the @ref MM_VERSION_CFM message.
struct mm_version_cfm {
	/// Version of the LMAC FW
	u32 version_lmac;
	/// Version1 of the MAC HW (as encoded in version1Reg MAC HW register)
	u32 version_machw_1;
	/// Version2 of the MAC HW (as encoded in version2Reg MAC HW register)
	u32 version_machw_2;
	/// Version1 of the PHY (depends on actual PHY)
	u32 version_phy_1;
	/// Version2 of the PHY (depends on actual PHY)
	u32 version_phy_2;
	/// Supported Features
	u32 features;
	/// Maximum number of supported stations
	uint16_t max_sta_nb;
	/// Maximum number of supported virtual interfaces
	uint8_t max_vif_nb;
};

/// Structure containing the parameters of the @ref MM_STA_ADD_REQ message.
struct mm_sta_add_req {
#ifdef CONFIG_ASR595X
	/// Maximum A-MPDU size, in bytes, for HE frames
	uint32_t ampdu_size_max_he;
#endif
	/// Maximum A-MPDU size, in bytes, for VHT frames
	u32 ampdu_size_max_vht;
	/// PAID/GID
	u32 paid_gid;
	/// Maximum A-MPDU size, in bytes, for HT frames
	u16 ampdu_size_max_ht;
	/// MAC address of the station to be added
	struct mac_addr mac_addr;
	/// A-MPDU spacing, in us
	u8 ampdu_spacing_min;
	/// Interface index
	u8 inst_nbr;
	/// TDLS station
	bool tdls_sta;
#ifdef CONFIG_ASR595X
	/// TDLS link initiator
	bool tdls_initiator;
	/// Flag to indicate if TDLS Channel Switch is allowed
	bool tdls_chsw_allowed;
	/// nonTransmitted BSSID index, set to the BSSID index in case the STA added is an AP
	/// that is a nonTransmitted BSSID. Should be set to 0 otherwise
	uint8_t bssid_index;
	/// Maximum BSSID indicator, valid if the STA added is an AP that is a nonTransmitted
	/// BSSID
	uint8_t max_bssid_ind;
#endif
};

/// Structure containing the parameters of the @ref MM_STA_ADD_CFM message.
struct mm_sta_add_cfm {
	/// Status of the operation (different from 0 if unsuccessful)
	u8 status;
	/// Index assigned by the LMAC to the newly added station
	u8 sta_idx;
	/// MAC HW index of the newly added station
	u8 hw_sta_idx;
};

/// Structure containing the parameters of the @ref MM_STA_DEL_REQ message.
struct mm_sta_del_req {
	/// Index of the station to be deleted
	u8 sta_idx;
};

/// Structure containing the parameters of the @ref MM_STA_DEL_CFM message.
struct mm_sta_del_cfm {
	/// Status of the operation (different from 0 if unsuccessful)
	u8 status;
};

/// Structure containing the parameters of the SET_POWER_MODE REQ message.
struct mm_setpowermode_req {
	u8 mode;
	u8 sta_idx;
};

/// Structure containing the parameters of the SET_POWER_MODE CFM message.
struct mm_setpowermode_cfm {
	u8 status;
};

/// Structure containing the parameters of the @ref MM_KEY_ADD REQ message.
struct mm_key_add_req {
	/// Key index (valid only for default keys)
	u8 key_idx;
	/// STA index (valid only for pairwise or mesh group keys)
	u8 sta_idx;
	/// Key material
	struct mac_sec_key key;
	/// Cipher suite (WEP64, WEP128, TKIP, CCMP)
	u8 cipher_suite;
	/// Index of the interface for which the key is set (valid only for default keys or mesh group keys)
	u8 inst_nbr;
	/// A-MSDU SPP parameter
	u8 spp;
	/// Indicate if provided key is a pairwise key or not
	bool pairwise;
};

/// Structure containing the parameters of the @ref MM_KEY_ADD_CFM message.
struct mm_key_add_cfm {
	/// Status of the operation (different from 0 if unsuccessful)
	u8 status;
	/// HW index of the key just added
	u8 hw_key_idx;
};

/// Structure containing the parameters of the @ref MM_KEY_DEL_REQ message.
struct mm_key_del_req {
	/// HW index of the key to be deleted
	u8 hw_key_idx;
};

/// Structure containing the parameters of the @ref MM_BA_ADD_REQ message.
struct mm_ba_add_req {
	///Type of agreement (0: TX, 1: RX)
	u8 type;
	///Index of peer station with which the agreement is made
	u8 sta_idx;
	///TID for which the agreement is made with peer station
	u8 tid;
	///Buffer size - number of MPDUs that can be held in its buffer per TID
	u8 bufsz;
	/// Start sequence number negotiated during BA setup - the one in first aggregated MPDU counts more
	u16 ssn;
};

/// Structure containing the parameters of the @ref MM_BA_ADD_CFM message.
struct mm_ba_add_cfm {
	///Index of peer station for which the agreement is being confirmed
	u8 sta_idx;
	///TID for which the agreement is being confirmed
	u8 tid;
	/// Status of ba establishment
	u8 status;
};

/// Structure containing the parameters of the @ref MM_BA_DEL_REQ message.
struct mm_ba_del_req {
	///Type of agreement (0: TX, 1: RX)
	u8 type;
	///Index of peer station for which the agreement is being deleted
	u8 sta_idx;
	///TID for which the agreement is being deleted
	u8 tid;
};

/// Structure containing the parameters of the @ref MM_BA_DEL_CFM message.
struct mm_ba_del_cfm {
	///Index of peer station for which the agreement deletion is being confirmed
	u8 sta_idx;
	///TID for which the agreement deletion is being confirmed
	u8 tid;
	/// Status of ba deletion
	u8 status;
};

/// Structure containing the parameters of the @ref MM_CHAN_CTXT_ADD_REQ message
#ifdef CONFIG_ASR595X
struct mm_chan_ctxt_add_req {
	/// Operating channel
	struct mac_chan_op chan;
};
#else
struct mm_chan_ctxt_add_req {
	/// Band (2.4GHz or 5GHz)
	u8 band;
	/// Channel type: 20,40,80,160 or 80+80 MHz
	u8 type;
	/// Frequency for Primary 20MHz channel (in MHz)
	u16 prim20_freq;
	/// Frequency for Center of the contiguous channel or center of Primary 80+80
	u16 center1_freq;
	/// Frequency for Center of the non-contiguous secondary 80+80
	u16 center2_freq;
	/// Max tx power for this channel
	s8 tx_power;
};
#endif

/// Structure containing the parameters of the @ref MM_CHAN_CTXT_ADD_REQ message
struct mm_chan_ctxt_add_cfm {
	/// Status of the addition
	u8 status;
	/// Index of the new channel context
	u8 index;
};

/// Structure containing the parameters of the @ref MM_CHAN_CTXT_DEL_REQ message
struct mm_chan_ctxt_del_req {
	/// Index of the new channel context to be deleted
	u8 index;
};

/// Structure containing the parameters of the @ref MM_CHAN_CTXT_LINK_REQ message
struct mm_chan_ctxt_link_req {
	/// VIF index
	u8 vif_index;
	/// Channel context index
	u8 chan_index;
	/// Indicate if this is a channel switch (unlink current ctx first if true)
	u8 chan_switch;
};

/// Structure containing the parameters of the @ref MM_CHAN_CTXT_UNLINK_REQ message
struct mm_chan_ctxt_unlink_req {
	/// VIF index
	u8 vif_index;
};

/// Structure containing the parameters of the @ref MM_CHAN_CTXT_UPDATE_REQ message
#ifdef CONFIG_ASR595X
struct mm_chan_ctxt_update_req {
	/// Channel context index
	uint8_t chan_index;
	/// New channel information
	struct mac_chan_op chan;
};
#else
struct mm_chan_ctxt_update_req {
	/// Channel context index
	u8 chan_index;
	/// Band (2.4GHz or 5GHz)
	u8 band;
	/// Channel type: 20,40,80,160 or 80+80 MHz
	u8 type;
	/// Frequency for Primary 20MHz channel (in MHz)
	u16 prim20_freq;
	/// Frequency for Center of the contiguous channel or center of Primary 80+80
	u16 center1_freq;
	/// Frequency for Center of the non-contiguous secondary 80+80
	u16 center2_freq;
};
#endif

/// Structure containing the parameters of the @ref MM_CHAN_CTXT_SCHED_REQ message
struct mm_chan_ctxt_sched_req {
	/// VIF index
	u8 vif_index;
	/// Channel context index
	u8 chan_index;
	/// Type of the scheduling request (0: normal scheduling, 1: derogatory
	/// scheduling)
	u8 type;
};

/// Structure containing the parameters of the @ref MM_CHANNEL_SWITCH_IND message
struct mm_channel_switch_ind {
	/// Index of the channel context we will switch to
	u8 chan_index;
	/// Indicate if the switch has been triggered by a Remain on channel request
	bool roc;
	/// VIF on which remain on channel operation has been started (if roc == 1)
	u8 vif_index;
	/// Indicate if the switch has been triggered by a TDLS Remain on channel request
	bool roc_tdls;
};

/// Structure containing the parameters of the @ref MM_CHANNEL_PRE_SWITCH_IND message
struct mm_channel_pre_switch_ind {
	/// Index of the channel context we will switch to
	u8 chan_index;
};

/// Structure containing the parameters of the @ref MM_CONNECTION_LOSS_IND message.
struct mm_connection_loss_ind {
	/// VIF instance number
	u8 inst_nbr;
};

/// Structure containing the parameters of the @ref MM_DBG_TRIGGER_REQ message.
struct mm_dbg_trigger_req {
	/// Error trace to be reported by the LMAC
	char error[64];
};

/// Structure containing the parameters of the @ref MM_SET_PS_MODE_REQ message.
struct mm_set_ps_mode_req {
	/// Power Save is activated or deactivated
	u8 new_state;
};

/// Structure containing the parameters of the @ref MM_BCN_CHANGE_REQ message.
#define BCN_MAX_CSA_CPT 2
struct mm_bcn_change_req {
	/// bcn
	u8 bcn_ptr[NX_BCNFRAME_LEN];
	/// Length of the beacon template
	u16 bcn_len;
	/// Offset of the TIM IE in the beacon
	u16 tim_oft;
	/// Length of the TIM IE
	u8 tim_len;
	/// Index of the VIF for which the beacon is updated
	u8 inst_nbr;
	/// Offset of CSA (channel switch announcement) counters (0 means no counter)
	u8 csa_oft[BCN_MAX_CSA_CPT];
};

/// Structure containing the parameters of the @ref MM_TIM_UPDATE_REQ message.
struct mm_tim_update_req {
	/// Association ID of the STA the bit of which has to be updated (0 for BC/MC traffic)
	u16 aid;
	/// Flag indicating the availability of data packets for the given STA
	u8 tx_avail;
	/// Index of the VIF for which the TIM is updated
	u8 inst_nbr;
};

/// Structure containing the parameters of the @ref MM_REMAIN_ON_CHANNEL_REQ message.
#ifdef CONFIG_ASR595X
struct mm_remain_on_channel_req {
	/// Operation Code
	uint8_t op_code;
	/// VIF Index
	uint8_t vif_index;
	/// Channel parameter
	struct mac_chan_op chan;
	/// Duration
	uint32_t duration_ms;
};
#else
struct mm_remain_on_channel_req {
	/// Operation Code
	u8 op_code;
	/// VIF Index
	u8 vif_index;
	/// Band (2.4GHz or 5GHz)
	u8 band;
	/// Channel type: 20,40,80,160 or 80+80 MHz
	u8 type;
	/// Frequency for Primary 20MHz channel (in MHz)
	u16 prim20_freq;
	/// Frequency for Center of the contiguous channel or center of Primary 80+80
	u16 center1_freq;
	/// Frequency for Center of the non-contiguous secondary 80+80
	u16 center2_freq;
	/// Duration (in ms)
	u32 duration_ms;
	/// TX power (in dBm)
	s8 tx_power;
};
#endif

/// Structure containing the parameters of the @ref MM_REMAIN_ON_CHANNEL_CFM message
struct mm_remain_on_channel_cfm {
	/// Operation Code
	u8 op_code;
	/// Status of the operation
	u8 status;
	/// Channel Context index
	u8 chan_ctxt_index;
};

/// Structure containing the parameters of the @ref MM_REMAIN_ON_CHANNEL_EXP_IND message
struct mm_remain_on_channel_exp_ind {
	/// VIF Index
	u8 vif_index;
	/// Channel Context index
	u8 chan_ctxt_index;
};

/// Structure containing the parameters of the @ref MM_SET_UAPSD_TMR_REQ message.
struct mm_set_uapsd_tmr_req {
	/// action: Start or Stop the timer
	u8 action;
	/// timeout value, in milliseconds
	u32 timeout;
};

/// Structure containing the parameters of the @ref MM_SET_UAPSD_TMR_CFM message.
struct mm_set_uapsd_tmr_cfm {
	/// Status of the operation (different from 0 if unsuccessful)
	u8 status;
};

/// Structure containing the parameters of the @ref MM_PS_CHANGE_IND message
struct mm_ps_change_ind {
	/// Index of the peer device that is switching its PS state
	u8 sta_idx;
	/// New PS state of the peer device (0: active, 1: sleeping)
	u8 ps_state;
	u8 tx_pkt;
};

/// Structure containing the parameters of the @ref MM_P2P_VIF_PS_CHANGE_IND message
struct mm_p2p_vif_ps_change_ind {
	/// Index of the P2P VIF that is switching its PS state
	u8 vif_index;
	/// New PS state of the P2P VIF interface (0: active, 1: sleeping)
	u8 ps_state;
};

/// Structure containing the parameters of the @ref MM_TRAFFIC_REQ_IND message
struct mm_traffic_req_ind {
	/// Index of the peer device that needs traffic
	u8 sta_idx;
	/// Number of packets that need to be sent (if 0, all buffered traffic shall be sent)
	u8 pkt_cnt;
	/// Flag indicating if the traffic request concerns U-APSD queues or not
	bool uapsd;
};

/// Structure containing the parameters of the @ref MM_SET_PS_OPTIONS_REQ message.
struct mm_set_ps_options_req {
	/// VIF Index
	u8 vif_index;
	/// Listen interval (0 if wake up shall be based on DTIM period)
	u16 listen_interval;
	/// Flag indicating if we shall listen the BC/MC traffic or not
	bool dont_listen_bc_mc;
};

/// Structure containing the parameters of the @ref MM_CSA_COUNTER_IND message
struct mm_csa_counter_ind {
	/// Index of the VIF
	u8 vif_index;
	/// Updated CSA counter value
	u8 csa_count;
};

/// Structure containing the parameters of the @ref MM_CHANNEL_SURVEY_IND message
struct mm_channel_survey_ind {
	/// Frequency of the channel
	u16 freq;
	/// Noise in dbm
	s8 noise_dbm;
	/// Amount of time spent of the channel (in ms)
	u32 chan_time_ms;
	/// Amount of time the primary channel was sensed busy
	u32 chan_time_busy_ms;
};

/// Structure containing the parameters of the @ref MM_BFMER_ENABLE_REQ message.
struct mm_bfmer_enable_req {
    /**
     * Address of Beamforming Report in host memory
     * (Valid only if vht_su_bfmee is true)
     */
	u32 host_bfr_addr;
	/// AID
	u16 aid;
	/// Station Index
	u8 sta_idx;
	/// Maximum number of spatial streams the station can receive
	u8 rx_nss;
    /**
     * Indicate if peer STA is MU Beamformee (VHT) capable
     * (Valid only if vht_su_bfmee is true)
     */
	bool vht_mu_bfmee;
};

/// Structure containing the parameters of the @ref MM_SET_P2P_NOA_REQ message.
struct mm_set_p2p_noa_req {
	/// VIF Index
	u8 vif_index;
	/// Allocated NOA Instance Number - Valid only if count = 0
	u8 noa_inst_nb;
	/// Count
	u8 count;
	/// Indicate if NoA can be paused for traffic reason
	bool dyn_noa;
	/// Duration (in us)
	u32 duration_us;
	/// Interval (in us)
	u32 interval_us;
	/// Start Time offset from next TBTT (in us)
	u32 start_offset;
};

/// Structure containing the parameters of the @ref MM_SET_P2P_OPPPS_REQ message.
struct mm_set_p2p_oppps_req {
	/// VIF Index
	u8 vif_index;
	/// CTWindow
	u8 ctwindow;
};

/// Structure containing the parameters of the @ref MM_SET_P2P_NOA_CFM message.
struct mm_set_p2p_noa_cfm {
	/// Request status
	u8 status;
};

/// Structure containing the parameters of the @ref MM_SET_P2P_OPPPS_CFM message.
struct mm_set_p2p_oppps_cfm {
	/// Request status
	u8 status;
};

/// Structure containing the parameters of the @ref MM_P2P_NOA_UPD_IND message.
struct mm_p2p_noa_upd_ind {
	/// VIF Index
	u8 vif_index;
	/// NOA Instance Number
	u8 noa_inst_nb;
	/// NoA Type
	u8 noa_type;
	/// Count
	u8 count;
	/// Duration (in us)
	u32 duration_us;
	/// Interval (in us)
	u32 interval_us;
	/// Start Time
	u32 start_time;
};

/// Structure containing the parameters of the @ref MM_CFG_RSSI_REQ message
struct mm_cfg_rssi_req {
	/// Index of the VIF
	u8 vif_index;
	/// RSSI threshold
	s8 rssi_thold;
	/// RSSI hysteresis
	u8 rssi_hyst;
};

/// Structure containing the parameters of the @ref MM_RSSI_STATUS_IND message
struct mm_rssi_status_ind {
	/// Index of the VIF
	u8 vif_index;
	/// Status of the RSSI
	bool rssi_status;
};

/// Structure containing the parameters of the @ref MM_PKTLOSS_IND message
struct mm_pktloss_ind
{
    /// Index of the VIF
    u8 vif_index;
    /// Address of the STA for which there is a packet loss
    struct mac_addr mac_addr;
    /// Number of packets lost
    u32 num_packets;
};

/// Structure containing the parameters of the @ref MM_CSA_FINISH_IND message
struct mm_csa_finish_ind {
	/// Index of the VIF
	u8 vif_index;
	/// Status of the operation
	u8 status;
	/// New channel ctx index
	u8 chan_idx;
};

/// Structure containing the parameters of the @ref MM_CSA_TRAFFIC_IND message
struct mm_csa_traffic_ind {
	/// Index of the VIF
	u8 vif_index;
	/// Is tx traffic enable or disable
	bool enable;
};

#ifdef CONFIG_TWT
/// Structure containing the parameters of the @ref MM_TWT_TRAFFIC_REQ_IND message
struct mm_twt_traffic_ind {
	/// Index of the peer device that needs traffic
	u8 sta_idx;
	/// VIF Index
	u8 vif_index;
	/// Flag indicating if the twt traffic is enable or not
	bool enable;
};
#endif

/// Structure containing the parameters of the @ref MM_MU_GROUP_UPDATE_REQ message.
/// Size allocated for the structure depends of the number of group
struct mm_mu_group_update_req {
	/// Station index
	u8 sta_idx;
	/// Number of groups the STA belongs to
	u8 group_cnt;
	/// Group information
	struct {
		/// Group Id
		u8 group_id;
		/// User position
		u8 user_pos;
	} groups[0];
};

struct mm_fw_softversion_cfm {
	u8 fw_softversion[32];
};

struct mm_fw_macaddr_cfm {
	u8 mac[MAC_ADDR_LEN];
	u8 status;
};

struct mm_set_fw_macaddr_req {
	u8 mac[MAC_ADDR_LEN];
};

struct mm_hif_sdio_info_ind {
	u8 flag;
	u8 host_tx_data_cur_idx;
	u8 rx_data_cur_idx;
};

struct mm_set_tx_pwr_rate_req {
	s8 pwr_rate[WIFI_CONFIG_PWR_NUM];
};

struct mm_set_tx_pwr_rate_cfm {
	u8 status;
};
#ifdef CONFIG_HW_MIB_TABLE
struct mm_reset_hw_mib_table_req {
	u8 reserved;
}
struct mm_reset_hw_mib_table_cfm {
	u8 status;
}
struct mm_get_hw_mib_table_req {
	u8 reserved;
}
struct mm_get_hw_mib_table_cfm {
	u8 status;
	uint32_t mib_info[255];
}
#endif

struct mm_set_get_cca_req {
	bool is_set_cca;
	s8 cca_threshold;
	s8 p_cca_rise;
	s8 p_cca_fall;
	s8 s_cca_rise;
	s8 s_cca_fall;
};

struct mm_set_get_cca_cfm {
	u8 status;
	s8 p_cca_rise;
	s8 p_cca_fall;
	s8 s_cca_rise;
	s8 s_cca_fall;
};

struct mm_get_rssi_cfm {
	u8 status;
	u8 staid;
	s8 rssi;
};

struct mm_set_upload_fram_cfm {
	u8 status;
	u8 vif_idx;
	u8 fram_type;
	u8 enable;
};

struct mm_set_fram_appie_req {
	u8 status;
	u8 vif_idx;
	u8 fram_type;
	u8 ie_len;
	u8 appie[FRAM_CUSTOM_APPIE_LEN];
};

struct mm_sta_link_loss_ind
{
    /// Index of the VIF
    u8 vif_idx;
    u8 staid;
};

struct mm_efuse_txpwr_info
{
    u8 status;
    u8 iswrite;
    u8 index;
    u8 txpwr[6];
    u8 txevm[6];
    u8 freq_err;
};
///////////////////////////////////////////////////////////////////////////////
/////////// For Scan messages
///////////////////////////////////////////////////////////////////////////////
enum scan_msg_tag {
	/// Scanning start Request.
	SCAN_START_REQ = LMAC_FIRST_MSG(TASK_SCAN),
	/// Scanning start Confirmation.
	SCAN_START_CFM,
	/// End of scanning indication.
	SCAN_DONE_IND,
	/// Cancel scan request
	SCAN_CANCEL_REQ,
	/// Cancel scan confirmation
	SCAN_CANCEL_CFM,

	/// MAX number of messages
	SCAN_MAX,
};

/// Maximum number of SSIDs in a scan request
#define SCAN_SSID_MAX   2

/// Maximum number of 2.4GHz channels
#define SCAN_CHANNEL_2G4 14

/// Maximum number of 5GHz channels
#define SCAN_CHANNEL_5G  28

/// scan additional times on the base 1 time
#define SCAN_ADDITIONAL_TIMES 1

/// Maximum number of channels in a scan request
#define SCAN_CHANNEL_MAX (SCAN_CHANNEL_2G4)

/// Flag bits
#define SCAN_PASSIVE_BIT BIT(0)
#define SCAN_DISABLED_BIT BIT(1)

/// Maximum number of PHY bands supported
#define SCAN_BAND_MAX 2

/// Definition of a channel to be scanned
struct scan_chan_tag {
	/// Frequency of the channel
	u16 freq;
	/// RF band (0: 2.4GHz, 1: 5GHz)
	u8 band;
	/// Bit field containing additional information about the channel
	u8 flags;
	/// Max tx_power for this channel (dBm)
	s8 tx_power;
};

#ifdef CONFIG_ASR595X
/// Primary Channel definition
struct mac_chan_def {
	/// Frequency of the channel (in MHz)
	uint16_t freq;
	/// RF band (@ref mac_chan_band)
	uint8_t band;
	/// Additional information (@ref mac_chan_flags)
	uint8_t flags;
	/// Max transmit power allowed on this channel (dBm)
	int8_t tx_power;
	/// Number of additional times in one channel to scan
	uint8_t scan_additional_cnt;
};
#endif

/// Structure containing the parameters of the @ref SCAN_START_REQ message
struct scan_start_req {
	/// List of channel to be scanned
#ifdef CONFIG_ASR595X
	struct mac_chan_def chan[SCAN_CHANNEL_MAX];
#else
	struct scan_chan_tag chan[SCAN_CHANNEL_MAX];
#endif
	/// List of SSIDs to be scanned
	struct mac_ssid ssid[SCAN_SSID_MAX];
	/// BSSID to be scanned
	struct mac_addr bssid;
	/// Pointer (in host memory) to the additional IEs that need to be added to the ProbeReq
	/// (following the SSID element)
	u32 add_ies;
	/// Length of the additional IEs
	u16 add_ie_len;
	/// Index of the VIF that is scanning
	u8 vif_idx;
	/// Number of channels to scan
	u8 chan_cnt;
	/// Number of SSIDs to scan for
	u8 ssid_cnt;
	/// no CCK - For P2P frames not being sent at CCK rate in 2GHz band.
	bool no_cck;
};

/// Structure containing the parameters of the @ref SCAN_START_CFM message
struct scan_start_cfm {
	/// Status of the request
	u8 status;
};

/// Structure containing the parameters of the @ref SCAN_CANCEL_REQ message
struct scan_cancel_req {
};

/// Structure containing the parameters of the @ref SCAN_START_CFM message
struct scan_cancel_cfm {
	/// Status of the request
	u8 status;
};

///////////////////////////////////////////////////////////////////////////////
/////////// For Scanu messages
///////////////////////////////////////////////////////////////////////////////
/// Messages that are logically related to the task.
enum {
	/// Scan request from host.
	SCANU_START_REQ = LMAC_FIRST_MSG(TASK_SCANU),
	/// Scanning start Confirmation.
	SCANU_START_CFM,
	/// Join request
	SCANU_JOIN_REQ,
	/// Join confirmation.
	SCANU_JOIN_CFM,
	/// Scan result indication.
	SCANU_RESULT_IND,
	/// Fast scan request from any other module.
	SCANU_FAST_REQ,
	/// Confirmation of fast scan request.
	SCANU_FAST_CFM,

	/// MAX number of messages
	SCANU_MAX,
};

/// Structure containing the parameters of the @ref SCANU_START_REQ message
struct scanu_start_req {
	/// List of channel to be scanned
#ifdef CONFIG_ASR595X
	struct mac_chan_def chan[SCAN_CHANNEL_MAX];
#else
	struct scan_chan_tag chan[SCAN_CHANNEL_MAX];
#endif
	/// List of SSIDs to be scanned
	struct mac_ssid ssid[SCAN_SSID_MAX];
	/// BSSID to be scanned (or WILDCARD BSSID if no BSSID is searched in particular)
	struct mac_addr bssid;
	/// Length of the additional IEs
	u16 add_ie_len;
	/// Index of the VIF that is scanning
	u8 vif_idx;
	/// Number of channels to scan
	u8 chan_cnt;
	/// Number of SSIDs to scan for
	u8 ssid_cnt;
	/// no CCK - For P2P frames not being sent at CCK rate in 2GHz band.
	bool no_cck;
	/// add ie
	u8 add_ie[SCANU_MAX_IE_LEN];
};

/// Structure containing the parameters of the @ref SCANU_START_CFM message
struct scanu_start_cfm {
	/// Status of the request
	u8 status;
};

/// Parameters of the @SCANU_RESULT_IND message
struct scanu_result_ind {
	/// Length of the frame
	u16 length;
	/// Frame control field of the frame.
	u16 framectrl;
	/// Center frequency on which we received the packet
	u16 center_freq;
	/// PHY band
	u8 band;
	/// Index of the station that sent the frame. 0xFF if unknown.
	u8 sta_idx;
	/// Index of the VIF that received the frame. 0xFF if unknown.
	u8 inst_nbr;
	/// RSSI of the received frame.
	s8 rssi;
	/// Frame payload.
	u32 payload[];
};

/// Structure containing the parameters of the message.
struct scanu_fast_req {
	/// The SSID to scan in the channel.
	struct mac_ssid ssid;
	/// BSSID.
	struct mac_addr bssid;
	/// Probe delay.
	u16 probe_delay;
	/// Minimum channel time.
	u16 minch_time;
	/// Maximum channel time.
	u16 maxch_time;
	/// The channel number to scan.
	u16 ch_nbr;
};

///////////////////////////////////////////////////////////////////////////////
/////////// For ME messages
///////////////////////////////////////////////////////////////////////////////
/// Messages that are logically related to the task.
enum {
	/// Configuration request from host.
	ME_CONFIG_REQ = LMAC_FIRST_MSG(TASK_ME),
	/// Configuration confirmation.
	ME_CONFIG_CFM,
	/// Configuration request from host.
	ME_CHAN_CONFIG_REQ,
	/// Configuration confirmation.
	ME_CHAN_CONFIG_CFM,
	/// Set control port state for a station.
	ME_SET_CONTROL_PORT_REQ,
	/// Control port setting confirmation.
	ME_SET_CONTROL_PORT_CFM,
	/// TKIP MIC failure indication.
	ME_TKIP_MIC_FAILURE_IND,
	/// Add a station to the FW (AP mode)
	ME_STA_ADD_REQ,
	/// Confirmation of the STA addition
	ME_STA_ADD_CFM,
	/// Delete a station from the FW (AP mode)
	ME_STA_DEL_REQ,
	/// Confirmation of the STA deletion
	ME_STA_DEL_CFM,
	/// Indication of a TX RA/TID queue credit update
	ME_TX_CREDITS_UPDATE_IND,
	/// Request indicating to the FW that there is traffic buffered on host
	ME_TRAFFIC_IND_REQ,
	/// Confirmation that the @ref ME_TRAFFIC_IND_REQ has been executed
	ME_TRAFFIC_IND_CFM,
	/// Request of RC statistics to a station
	ME_RC_STATS_REQ,
	/// RC statistics confirmation
	ME_RC_STATS_CFM,
	/// RC fixed rate request
	ME_RC_SET_RATE_REQ,
	/// Request disable tx_aggr
	ME_HOST_DBG_CMD_REQ,
#ifdef CONFIG_ASR595X
	/// Configure monitor interface
	ME_CONFIG_MONITOR_REQ,
	/// Configure monitor interface response
	ME_CONFIG_MONITOR_CFM,
	/// Setting Power Save mode request from host
	ME_SET_PS_MODE_REQ,
	/// Set Power Save mode confirmation
	ME_SET_PS_MODE_CFM,
	/// Setting individual twt request from host
	ME_ITWT_CONFIG_REQ,	//21
	/// del individual twt request from host
	ME_ITWT_DEL_REQ,	//22
#endif
	/// MAX number of messages
	ME_MAX,
};

/// Structure containing the parameters of the @ref ME_START_REQ message
struct me_config_req {
	/// HT Capabilities
	struct mac_htcapability ht_cap;
	/// VHT Capabilities
	struct mac_vhtcapability vht_cap;
#ifdef CONFIG_ASR595X
	/// HE capabilities
	struct mac_hecapability he_cap;
#endif
	/// Lifetime of packets sent under a BlockAck agreement (expressed in TUs)
	u16 tx_lft;
#ifdef CONFIG_ASR595X
	/// Maximum supported BW
	uint8_t phy_bw_max;
#endif
	/// Boolean indicating if HT is supported or not
	bool ht_supp;
	/// Boolean indicating if VHT is supported or not
	bool vht_supp;
#ifdef CONFIG_ASR595X
	/// Boolean indicating if HE is supported or not
	bool he_supp;
	/// Boolean indicating if HE OFDMA UL is enabled or not
	bool he_ul_on;
#endif
	/// Boolean indicating if PS mode shall be enabled or not
	bool ps_on;
#ifdef CONFIG_ASR5531
	/// Boolean indicating if Antenna Diversity shall be enabled or not
	bool ant_div_on;
#endif
};

/// Structure containing the parameters of the @ref ME_CHAN_CONFIG_REQ message
#ifdef CONFIG_ASR595X
struct me_chan_config_req {
	/// List of 2.4GHz supported channels
	struct mac_chan_def chan2G4[MAC_DOMAINCHANNEL_24G_MAX];
        //#ifndef BASS_SUPPORT
	/// List of 5GHz supported channels
	struct mac_chan_def chan5G[MAC_DOMAINCHANNEL_5G_MAX];
        //#endif
	/// Number of 2.4GHz channels in the list
	uint8_t chan2G4_cnt;
	/// Number of 5GHz channels in the list
	uint8_t chan5G_cnt;
};
#else
struct me_chan_config_req {
	/// List of 2.4GHz supported channels
	struct scan_chan_tag chan2G4[SCAN_CHANNEL_2G4];
	/// List of 5GHz supported channels
	struct scan_chan_tag chan5G[SCAN_CHANNEL_5G];
	/// Number of 2.4GHz channels in the list
	u8 chan2G4_cnt;
	/// Number of 5GHz channels in the list
	u8 chan5G_cnt;
};
#endif

/// Structure containing the parameters of the @ref ME_SET_CONTROL_PORT_REQ message
struct me_set_control_port_req {
	/// Index of the station for which the control port is opened
	u8 sta_idx;
	/// Control port state
	bool control_port_open;
};

/// Structure containing the parameters of the @ref ME_TKIP_MIC_FAILURE_IND message
struct me_tkip_mic_failure_ind {
	/// Address of the sending STA
	struct mac_addr addr;
	/// TSC value
	u64 tsc;
	/// Boolean indicating if the packet was a group or unicast one (true if group)
	bool ga;
	/// Key Id
	u8 keyid;
	/// VIF index
	u8 vif_idx;
};

/// Structure containing the parameters of the @ref ME_STA_ADD_REQ message
struct me_sta_add_req {
	/// MAC address of the station to be added
	struct mac_addr mac_addr;
	/// Supported legacy rates
	struct mac_rateset rate_set;
	/// HT Capabilities
	struct mac_htcapability ht_cap;
	/// VHT Capabilities
	struct mac_vhtcapability vht_cap;
#ifdef CONFIG_ASR595X
	/// HE capabilities
	struct mac_hecapability he_cap;
#endif
	/// Flags giving additional information about the station
	u32 flags;
	/// Association ID of the station
	u16 aid;
	/// Bit field indicating which queues have U-APSD enabled
	u8 uapsd_queues;
	/// Maximum size, in frames, of a APSD service period
	u8 max_sp_len;
	/// Operation mode information (valid if bit @ref STA_OPMOD_NOTIF is
	/// set in the flags)
	u8 opmode;
	/// Index of the VIF the station is attached to
	u8 vif_idx;
	/// Whether the the station is TDLS station
	bool tdls_sta;
};

/// Structure containing the parameters of the @ref ME_STA_ADD_CFM message
struct me_sta_add_cfm {
	/// Station index
	u8 sta_idx;
	/// Status of the station addition
	u8 status;
	/// PM state of the station
	u8 pm_state;
};

/// Structure containing the parameters of the @ref ME_STA_DEL_REQ message.
struct me_sta_del_req {
	/// Index of the station to be deleted
	u8 sta_idx;
	/// Whether the the station is TDLS station
	bool tdls_sta;
};

/// Structure containing the parameters of the @ref ME_TX_CREDITS_UPDATE_IND message.
struct me_tx_credits_update_ind {
	/// Index of the station for which the credits are updated
	u8 sta_idx;
	/// TID for which the credits are updated
	u8 tid;
	/// Offset to be applied on the credit count
	s8 credits;
};

/// Structure containing the parameters of the @ref ME_TRAFFIC_IND_REQ message.
struct me_traffic_ind_req {
	/// Index of the station for which UAPSD traffic is available on host
	u8 sta_idx;
	/// Flag indicating the availability of UAPSD packets for the given STA
	u8 tx_avail;
	/// Indicate if traffic is on uapsd-enabled queues
	bool uapsd;
};

/// Structure containing the parameters of the @ref ME_RC_STATS_REQ message.
struct me_rc_stats_req {
	/// Index of the station for which the RC statistics are requested
	u8 sta_idx;
};

/// Number of rate control steps in the policy table
#define RATE_CONTROL_STEPS      4
/// Structure containing the rate control statistics
#ifdef CONFIG_ASR595X
/// Maximum number of samples to be maintained in the statistics structure
#define RC_MAX_N_SAMPLE          (10)
#define RC_HE_STATS_IDX   RC_MAX_N_SAMPLE


struct rc_rate_stats {
	/// Number of attempts (per sampling interval)
	uint16_t attempts;
	/// Number of success (per sampling interval)
	uint16_t success;
	/// Estimated probability of success (EWMA)
	uint16_t probability;
	/// Rate configuration of the sample
	uint16_t rate_config;
	union {
		struct {
			/// Number of times the sample has been skipped (per sampling interval)
			uint8_t sample_skipped;
			/// Whether the old probability is available
			bool old_prob_available;
			/// Whether the rate can be used in the retry chain
			bool rate_allowed;
		};
		struct {
			/// RU size and UL length received in the latest HE trigger frame
			uint16_t ru_and_length;
		};
	};
};

/// Structure containing the parameters of the @ref ME_RC_STATS_CFM message.
struct me_rc_stats_cfm {
	/// Index of the station for which the RC statistics are provided
	uint8_t sta_idx;
	/// Number of samples used in the RC algorithm
	uint16_t no_samples;
	/// Number of MPDUs transmitted (per sampling interval)
	uint16_t ampdu_len;
	/// Number of AMPDUs transmitted (per sampling interval)
	uint16_t ampdu_packets;
	/// Average number of MPDUs in each AMPDU frame (EWMA)
	uint32_t avg_ampdu_len;
	/// Current step 0 of the retry chain
	uint8_t sw_retry_step;
	/// Trial transmission period
	uint8_t sample_wait;
	/// Retry chain steps
	uint16_t retry_step_idx[RATE_CONTROL_STEPS];
	/// RC statistics - Max number of RC samples, plus one for the HE TB statistics
	struct rc_rate_stats rate_stats[RC_MAX_N_SAMPLE + 1];
	/// Throughput - Max number of RC samples, plus one for the HE TB statistics
	uint32_t tp[RC_MAX_N_SAMPLE + 1];
};
#else
/// Structure containing the rate control statistics
struct rc_rate_stats {
	/// Number of attempts (per sampling interval)
	u16 attempts;
	/// Number of success (per sampling interval)
	u16 success;
	/// Estimated probability of success (EWMA)
	u16 probability;
	/// Rate configuration of the sample
	u16 rate_config;
	/// Number of times the sample has been skipped (per sampling interval)
	u8 sample_skipped;
	/// Whether the old probability is available
	bool old_prob_available;
	// Whether the rate can be used in the retry chain
	bool rate_allowed;
};

/// Structure containing the parameters of the @ref ME_RC_STATS_CFM message.
struct me_rc_stats_cfm {
	/// Index of the station for which the RC statistics are provided
	u8 sta_idx;
	/// Number of samples used in the RC algorithm
	u16 no_samples;
	/// Number of MPDUs transmitted (per sampling interval)
	u16 ampdu_len;
	/// Number of AMPDUs transmitted (per sampling interval)
	u16 ampdu_packets;
	/// Average number of MPDUs in each AMPDU frame (EWMA)
	u32 avg_ampdu_len;
	// Current step 0 of the retry chain
	u8 sw_retry_step;
	/// Trial transmission period
	u8 sample_wait;
	/// Retry chain steps
	uint16_t retry_step_idx[RATE_CONTROL_STEPS];
	/// RC statistics
	struct rc_rate_stats rate_stats[10];
	/// Throughput
	u32 tp[10];
};
#endif

/// Structure containing the parameters of the @ref ME_RC_SET_RATE_REQ message.
struct me_rc_set_rate_req {
	/// Index of the station for which the fixed rate is set
	u8 sta_idx;
	/// Rate configuration to be set
	u16 fixed_rate_cfg;
};

struct me_host_dbg_cmd_req {
    /// host dbg cmd send to fw.
    u32  host_dbg_cmd;
    u32  host_dbg_reg;
    u32  host_dbg_value;
};

///////////////////////////////////////////////////////////////////////////////
/////////// For SM messages
///////////////////////////////////////////////////////////////////////////////
/// Message API of the SM task
enum sm_msg_tag {
	/// Request to connect to an AP
	SM_CONNECT_REQ = LMAC_FIRST_MSG(TASK_SM),
	/// Confirmation of connection
	SM_CONNECT_CFM,
	/// Indicates that the SM associated to the AP
	SM_CONNECT_IND,
	/// Request to disconnect
	SM_DISCONNECT_REQ,
	/// Confirmation of disconnection
	SM_DISCONNECT_CFM,
	/// Indicates that the SM disassociated the AP
	SM_DISCONNECT_IND,
#ifdef CONFIG_ASR595X
	/// Request to start external authentication
	SM_EXTERNAL_AUTH_REQUIRED_IND,
	/// Response to external authentication request
	SM_EXTERNAL_AUTH_REQUIRED_RSP,
#endif

	/// Timeout message for procedures requiring a response from peer
	SM_RSP_TIMEOUT_IND,
#ifdef CONFIG_SME
	/// Request to auth to an AP
	SM_AUTH_REQ,
	/// Confirmation of auth
	SM_AUTH_CFM,
	/// Indicates that the SM authed to the AP
	SM_AUTH_IND,
	/// Request to assoc to an AP
	SM_ASSOC_REQ,		//10
	/// Confirmation of assoc
	SM_ASSOC_CFM,
	/// Indicates that the SM assoced to the AP
	SM_ASSOC_IND,
#endif

	/// MAX number of messages
	SM_MAX,
};

#ifdef CONFIG_SME
/// Structure containing the parameters of @ref SM_AUTH_REQ message.
#define SAE_MAX_PRIME_LEN 512
#define SAE_COMMIT_MAX_LEN (2 + 3 * SAE_MAX_PRIME_LEN)
#define SAE_CONFIRM_MAX_LEN (2 + SAE_MAX_PRIME_LEN)

struct sm_auth_req {
	/// SSID to connect to
	struct mac_ssid ssid;	//auth stage
	/// BSSID to connect to (if not specified, set this field to WILDCARD BSSID)
	struct mac_addr bssid;	//auth stage
	/// Channel on which we have to connect (if not specified, set -1 in the chan.freq field)
	struct scan_chan_tag chan;	// auth stage
	/// Connection flags (see @ref sm_connect_flags)
	u32 flags;		//(con_par->flags & DISABLE_HT)  auth stage
	/// Authentication type
	u8 auth_type;		//auth stage
	/// VIF index
	u8 vif_idx;		//auth stage,all
	/// Extra IEs to add to Authentication frame or %NULL
	u32 ie_buf[64];
	/// Length of ie buffer in octets
	u16 ie_len;		//both stage
#if 1 //def CONFIG_SAE
	//Non-IE data to use with SAE or %NULL. This starts with Authentication transaction sequence number field.
	u8 sae_data[512];	// SAE_COMMIT_MAX_LEN = 1538, use group19 sae_data length
	//Length of sae_data buffer in octets
	u16 sae_data_len;
#endif
};

/// Structure containing the parameters of @ref SM_ASSOC_REQ message.
struct sm_assoc_req {
	/// BSSID to connect to (if not specified, set this field to WILDCARD BSSID)
	struct mac_addr bssid;	//assoc stage
	/// Listen interval to be used for this connection
	u16 listen_interval;	//assoc stage
	/// Flag indicating if the we have to wait for the BC/MC traffic after beacon or not
	bool dont_wait_bcmc;	//assoc stage
	/// Control port Ethertype
	u16 ctrl_port_ethertype;	//assoc stage
	/// UAPSD queues (bit0: VO, bit1: VI, bit2: BE, bit3: BK)
	u8 uapsd_queues;	//assoc stage
	/// associate flags
	u32 flags;		//(con_par->flags & DISABLE_HT)  assoc stage
	//Use management frame protection (IEEE 802.11w) in this association
	bool use_mfp;
	/// VIF index
	u8 vif_idx;		//auth stage,all
	/// Buffer containing the additional information elements to be put in the association request
	u32 ie_buf[64];		//all
	/// Length of the association request IEs
	u16 ie_len;		//both stage
};
#endif

#ifdef CONFIG_TWT

typedef struct {
	u8 setup_cmd;
	u8 flow_type;
	u8 wake_interval_exponent;
	u16 wake_interval_mantissa;
	u8 monimal_min_wake_duration;
	bool twt_upload_wait_trigger;
} wifi_twt_config_t;

/// Structure containing the parameters of the @ref ME_ITWT_CONFIG_REQ message.
struct me_itwt_config_req {
	/// setup_cmd
	u8 setup_cmd;
	/// Is trigger-enabled TWT.
	u8 trigger;
	/// Is implicit TWT.
	u8 implicit;
	/// Is announced TWT.
	u8 flow_type;
	/// TWT channel
	u8 twt_channel;
	/// TWT protection
	u8 twt_prot;
	/// wake interval exponent
	u8 wake_interval_exponent;
	/// wake interval mantissa
	u16 wake_interval_mantissa;
	/// target wakeup time low(us)
	u32 target_wake_time_lo;
	/// minial wake duration after twt sp start time
	u8 monimal_min_wake_duration;
	//  indicate upload should enable after rx trigger
	bool twt_upload_wait_trigger;
};

struct me_itwt_del_req {
	u8 flow_id;
};
#endif

/// Structure containing the parameters of @ref SM_CONNECT_REQ message.
struct sm_connect_req {
	/// SSID to connect to
	struct mac_ssid ssid;
	/// BSSID to connect to (if not specified, set this field to WILDCARD BSSID)
	struct mac_addr bssid;
	/// Channel on which we have to connect (if not specified, set -1 in the chan.freq field)
#ifdef CONFIG_ASR595X
	struct mac_chan_def chan;
#else
	struct scan_chan_tag chan;
#endif
	/// Connection flags (see @ref sm_connect_flags)
	u32 flags;
	/// Control port Ethertype
	u16 ctrl_port_ethertype;
	/// Length of the association request IEs
	u16 ie_len;
	/// Listen interval to be used for this connection
	u16 listen_interval;
	/// Flag indicating if the we have to wait for the BC/MC traffic after beacon or not
	bool dont_wait_bcmc;
	/// Authentication type
	u8 auth_type;
	/// UAPSD queues (bit0: VO, bit1: VI, bit2: BE, bit3: BK)
	u8 uapsd_queues;
	/// VIF index
	u8 vif_idx;
	/// Buffer containing the additional information elements to be put in the
	/// association request
	u32 ie_buf[64];
};

/// Structure containing the parameters of the @ref SM_CONNECT_CFM message.
struct sm_connect_cfm {
	/// Status. If 0, it means that the connection procedure will be performed and that
	/// a subsequent @ref SM_CONNECT_IND message will be forwarded once the procedure is
	/// completed
	u8 status;
};
#ifdef CONFIG_SME
/// Structure containing the parameters of the @ref SM_AUTH_CFM message.
struct sm_auth_cfm {
	/// Status. If 0, it means that the auth procedure will be performed
	u8 status;
};

/// Structure containing the parameters of the @ref SM_ASSOC_CFM message.
struct sm_assoc_cfm {
	/// Status. If 0, it means that the assoc procedure will be performed
	u8 status;
};
#endif

#define SM_ASSOC_IE_LEN   800
/// Structure containing the parameters of the @ref SM_CONNECT_IND message.
struct sm_connect_ind {
	/// Status code of the connection procedure
	u16 status_code;
	/// BSSID
	struct mac_addr bssid;
	/// Flag indicating if the indication refers to an internal roaming or from a host request
	bool roamed;
	/// Index of the VIF for which the association process is complete
	u8 vif_idx;
	/// Index of the STA entry allocated for the AP
	u8 ap_idx;
	/// Index of the LMAC channel context the connection is attached to
	u8 ch_idx;
	/// Flag indicating if the AP is supporting QoS
	bool qos;
	/// ACM bits set in the AP WMM parameter element
	u8 acm;
	/// Length of the AssocReq IEs
	u16 assoc_req_ie_len;
	/// Length of the AssocRsp IEs
	u16 assoc_rsp_ie_len;
	/// IE buffer
	u32 assoc_ie_buf[SM_ASSOC_IE_LEN / 4];
	/// Association Id allocated by the AP for this connection
	u16 aid;
#ifdef CONFIG_ASR595X
	/// AP operating channel
	struct mac_chan_op chan;
#else
	u8 band;
	u16 center_freq;
	u8 width;
	u32 center_freq1;
	u32 center_freq2;
#endif
	/// EDCA parameters
	u32 ac_param[AC_MAX];
};

/// Structure containing the parameters of the @ref SM_DISCONNECT_REQ message.
struct sm_disconnect_req {
	/// Reason of the deauthentication.
	u16 reason_code;
	/// Index of the VIF.
	u8 vif_idx;
};

/// Structure containing the parameters of SM_ASSOCIATION_IND the message
struct sm_association_ind {
	// MAC ADDR of the STA
	struct mac_addr me_mac_addr;
};

/// Structure containing the parameters of the @ref SM_DISCONNECT_IND message.
struct sm_disconnect_ind {
	/// Reason of the disconnection.
	u16 reason_code;
	/// Index of the VIF.
	u8 vif_idx;
	/// FT over DS is ongoing
	bool ft_over_ds;
	/// FT over DS is ongoing
	bool ft_over_air;
	/// non-ft roaming is ongoing
	bool normal_roam;
};

/// Structure containing the parameters of the @ref SM_AUTH_IND message.
struct sm_auth_ind {
	/// Reason of the auth.
	u16 reason_code;
	/// Index of the VIF.
	u8 vif_idx;
	/// FT over DS is ongoing
	bool ft_over_ds;
};

///////////////////////////////////////////////////////////////////////////////
/////////// For SM messages
///////////////////////////////////////////////////////////////////////////////
/// Message API of the APM task
enum apm_msg_tag {
	/// Request to start the AP.
	APM_START_REQ = LMAC_FIRST_MSG(TASK_APM),
	/// Confirmation of the AP start.
	APM_START_CFM,
	/// Request to stop the AP.
	APM_STOP_REQ,
	/// Confirmation of the AP stop.
	APM_STOP_CFM,
	/// Request to start CAC
	APM_START_CAC_REQ,
	/// Confirmation of the CAC start
	APM_START_CAC_CFM,
	/// Request to stop CAC
	APM_STOP_CAC_REQ,
	/// Confirmation of the CAC stop
	APM_STOP_CAC_CFM,

	/// MAX number of messages
	APM_MAX,
};

/// Structure containing the parameters of the @ref APM_START_REQ message.
struct apm_start_req {
	/// Basic rate set
	struct mac_rateset basic_rates;
#ifdef CONFIG_ASR595X
	/// Operating channel on which we have to enable the AP
	struct mac_chan_op chan;
#else
	/// Control channel on which we have to enable the AP
	struct scan_chan_tag chan;
	/// Center frequency of the first segment
	u32 center_freq1;
	/// Center frequency of the second segment (only in 80+80 configuration)
	u32 center_freq2;
	/// Width of channel
	u8 ch_width;
#endif
	/// Length of the beacon template
	u16 bcn_len;
	/// Offset of the TIM IE in the beacon
	u16 tim_oft;
	/// Beacon interval
	u16 bcn_int;
	/// Flags
	u32 flags;
	/// Control port Ethertype
	u16 ctrl_port_ethertype;
	/// Length of the TIM IE
	u8 tim_len;
	/// Index of the VIF for which the AP is started
	u8 vif_idx;
	/// bcn
	u8 bcn[NX_BCNFRAME_LEN];
#ifdef CFG_AP_HIDDEN_SSID
	struct mac_ssid ssid; // ap ssid len
#endif
};

/// Structure containing the parameters of the @ref APM_START_CFM message.
struct apm_start_cfm {
	/// Status of the AP starting procedure
	u8 status;
	/// Index of the VIF for which the AP is started
	u8 vif_idx;
	/// Index of the channel context attached to the VIF
	u8 ch_idx;
	/// Index of the STA used for BC/MC traffic
	u8 bcmc_idx;
};

/// Structure containing the parameters of the @ref APM_STOP_REQ message.
struct apm_stop_req {
	/// Index of the VIF for which the AP has to be stopped
	u8 vif_idx;
};

/// Structure containing the parameters of the @ref APM_START_CAC_REQ message.
struct apm_start_cac_req {
#ifdef CONFIG_ASR595X
	/// Channel configuration
	struct mac_chan_op chan;
#else
	/// Control channel on which we have to start the CAC
	struct scan_chan_tag chan;
	/// Center frequency of the first segment
	u32 center_freq1;
	/// Center frequency of the second segment (only in 80+80 configuration)
	u32 center_freq2;
	/// Width of channel
	u8 ch_width;
#endif
	/// Index of the VIF for which the CAC is started
	u8 vif_idx;
};

/// Structure containing the parameters of the @ref APM_START_CAC_CFM message.
struct apm_start_cac_cfm {
	/// Status of the CAC starting procedure
	u8 status;
	/// Index of the channel context attached to the VIF for CAC
	u8 ch_idx;
};

/// Structure containing the parameters of the @ref APM_STOP_CAC_REQ message.
struct apm_stop_cac_req {
	/// Index of the VIF for which the CAC has to be stopped
	u8 vif_idx;
};

///////////////////////////////////////////////////////////////////////////////
/////////// For Debug messages
///////////////////////////////////////////////////////////////////////////////

/// Messages related to Debug Task
enum dbg_msg_tag {
	/// Memory read request
	DBG_MEM_READ_REQ = LMAC_FIRST_MSG(TASK_DBG),
	/// Memory read confirm
	DBG_MEM_READ_CFM,
	/// Memory write request
	DBG_MEM_WRITE_REQ,
	/// Memory write confirm
	DBG_MEM_WRITE_CFM,
	/// Module filter request
	DBG_SET_MOD_FILTER_REQ,
	/// Module filter confirm
	DBG_SET_MOD_FILTER_CFM,
	/// Severity filter request
	DBG_SET_SEV_FILTER_REQ,
	/// Severity filter confirm
	DBG_SET_SEV_FILTER_CFM,
	/// LMAC/MAC HW fatal error indication
	DBG_ERROR_IND,
	/// Request to get system statistics
	DBG_GET_SYS_STAT_REQ,
	/// COnfirmation of system statistics
	DBG_GET_SYS_STAT_CFM,
	/// Max number of Debug messages
	DBG_MAX,
};

/// Structure containing the parameters of the @ref DBG_MEM_READ_REQ message.
struct dbg_mem_read_req {
	u32 memaddr;
};

/// Structure containing the parameters of the @ref DBG_MEM_READ_CFM message.
struct dbg_mem_read_cfm {
	u32 memaddr;
	u32 memdata;
};

/// Structure containing the parameters of the @ref DBG_MEM_WRITE_REQ message.
struct dbg_mem_write_req {
	u32 memaddr;
	u32 memdata;
};

/// Structure containing the parameters of the @ref DBG_MEM_WRITE_CFM message.
struct dbg_mem_write_cfm {
	u32 memaddr;
	u32 memdata;
};

/// Structure containing the parameters of the @ref DBG_SET_MOD_FILTER_REQ message.
struct dbg_set_mod_filter_req {
	/// Bit field indicating for each module if the traces are enabled or not
	u32 mod_filter;
};

/// Structure containing the parameters of the @ref DBG_SEV_MOD_FILTER_REQ message.
struct dbg_set_sev_filter_req {
	/// Bit field indicating the severity threshold for the traces
	u32 sev_filter;
};

/// Structure containing the parameters of the @ref DBG_GET_SYS_STAT_CFM message.
struct dbg_get_sys_stat_cfm {
	/// Time spent in CPU sleep since last reset of the system statistics
	u32 cpu_sleep_time;
	/// Time spent in DOZE since last reset of the system statistics
	u32 doze_time;
	/// Total time spent since last reset of the system statistics
	u32 stats_time;
};

/// Power Save mode setting
enum {
	/// Power-save off
	PS_MODE_OFF,
	/// Power-save on - Normal mode
	PS_MODE_ON,
	/// Power-save on - Dynamic mode
	PS_MODE_ON_DYN,


	/// modem sleep with xtal not gate,no gpio pin
	PS_MODE_MODEMSLEEP_0_PIN,
	/// modem sleep with xtal gate,2 gpio pin
	PS_MODE_MODEMSLEEP_2_PIN,
	/// light sleep with 1 pin,host wakeup card,for jichegn
	PS_MODE_LIGHTSLEEP_1_PIN,
	/// light sleep with 2 pin for sdio ,or no pin for usb
	PS_MODE_LIGHTSLEEP,
};

#endif // LMAC_MSG_H_
