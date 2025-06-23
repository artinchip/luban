/**
 ****************************************************************************************
 *
 * @file ipc_shared.h
 *
 * @brief Shared data between both IPC modules.
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef _IPC_SHARED_H_
#define _IPC_SHARED_H_

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "ipc_compat.h"
#include "lmac_mac.h"

/*
 * DEFINES AND MACROS
 ****************************************************************************************
 */
#define CO_BIT(pos) (1U<<(pos))

#define IPC_TXQUEUE_CNT     NX_TXQ_CNT
#define NX_TXDESC_CNT0      8
#define NX_TXDESC_CNT1      64
#define NX_TXDESC_CNT2      64
#define NX_TXDESC_CNT3      16
#if NX_TXQ_CNT == 5
#define NX_TXDESC_CNT4      8
#endif

/*
 * Number of Host buffers available for Data Rx handling
 */
#ifdef CONFIG_ASR_SDIO
#ifdef SDIO_DEAGGR
#define IPC_RXBUF_CNT       4   // 8   // 50	//15
#else
#define IPC_RXBUF_CNT       50   // 50	//15
#endif
#else
#define IPC_RXBUF_CNT       50	//15
#endif

/*
 * RX Data buffers size (in bytes) need adjust
 */
#ifdef CONFIG_ASR_SDIO
#define ASR_ALIGN_BLKSZ(x) (((x)+SDIO_BLOCK_SIZE-1)&(~(SDIO_BLOCK_SIZE-1)))
#else
#define ASR_ALIGN_BLKSZ(x)  (x)
#endif

#define SDIO_MAX_AGGR_PORT_NUM     8
#ifdef CONFIG_ASR_SDIO
#define IPC_RXBUF_SIZE      ASR_ALIGN_BLKSZ(1696)*(SDIO_MAX_AGGR_PORT_NUM)    //13568	//1696*8
#else
#define IPC_RXBUF_SIZE      (1696)	//1696*1
#endif

#define IPC_MSG_RXBUF_CNT       8
#define IPC_MSG_RXBUF_SIZE      ASR_ALIGN_BLKSZ(1696)

#define IPC_LOG_RXBUF_CNT       1
#define IPC_LOG_RXBUF_SIZE      ASR_ALIGN_BLKSZ(1696)

/*
 * RX AMSDU buffers size (in bytes) and cnt need adjust
 */
#define IPC_RXBUF_SIZE_SPLIT             (WLAN_AMSDU_RX_LEN)

#ifdef CONFIG_ASR_SDIO
#define IPC_RXBUF_CNT_SPLIT       2 // 8    // refine , only used for temp save part of mpdu when rx amsdu or ringbuf , others directly to ip stack.
#else
#define IPC_RXBUF_CNT_SPLIT       2
#endif


/*
 * RX deaggr buffers size (in bytes) and cnt need adjust
 */
#ifdef SDIO_DEAGGR
#ifdef CONFIG_ASR595X
// wifi6 rx ringbuf used.
#define IPC_RXBUF_CNT_SDIO_DEAGG_AMSDU   (15)
#define IPC_RXBUF_SIZE_SDIO_DEAGG_AMSDU  (WLAN_AMSDU_RX_LEN)

#ifdef SDIO_DEAGGR_AMSDU
#define IPC_RXBUF_CNT_SDIO_DEAGG         (30)    // 336   // (50-8)*8
#define IPC_RXBUF_SIZE_SDIO_DEAGG        (1696)
#else
#define IPC_RXBUF_CNT_SDIO_DEAGG         (30)    // 336   // (50-8)*8
#define IPC_RXBUF_SIZE_SDIO_DEAGG        IPC_RXBUF_SIZE_SDIO_DEAGG_AMSDU
#endif
#else
#define IPC_RXBUF_CNT_SDIO_DEAGG         (50)    // 336   // (50-8)*8
#define IPC_RXBUF_SIZE_SDIO_DEAGG        ASR_ALIGN_BLKSZ(1696)
#endif
#endif  // SDIO_DEAGGR

/*
 * Number of Host buffers available for Data Tx handling
 */
// new tx list macro
#define IPC_HIF_TXBUF_SIZE   ASR_ALIGN_BLKSZ(1696)*(SDIO_MAX_AGGR_PORT_NUM)

// tx agg buf macro
#define TX_AGG_BUF_UNIT_CNT  (180)	//240
#ifdef CONFIG_ASR595X
#define TX_AGG_BUF_UNIT_SIZE (1632)  //  44 + (2+60) + 1500 + 4
#else
#define TX_AGG_BUF_UNIT_SIZE ASR_ALIGN_BLKSZ(1696)
#endif
#define TX_AGG_BUF_SIZE     (TX_AGG_BUF_UNIT_CNT*TX_AGG_BUF_UNIT_SIZE)

/*
 * Length used in MSGs structures
 */
#define IPC_A2E_MSG_BUF_SIZE    127	// size in 4-byte words

#define IPC_E2A_MSG_PARAM_SIZE   256	// size in 4-byte words

/*
 * Debug messages buffers size (in bytes)
 */
#define IPC_DBG_PARAM_SIZE       1024

/*
 * Define used for Rx hostbuf validity.
 * This value should appear only when hostbuf was used for a Reception.
 */
#define RX_DMA_OVER_PATTERN 0xAAAAAA00

/*
 * Define used for MSG buffers validity.
 * This value will be written only when a MSG buffer is used for sending from Emb to App.
 */
#define IPC_MSGE2A_VALID_PATTERN 0xADDEDE2A

/*
 * Define used for Debug messages buffers validity.
 * This value will be written only when a DBG buffer is used for sending from Emb to App.
 */
#define IPC_DBG_VALID_PATTERN 0x000CACA0

/*
 *  Length of the receive vectors, in bytes
 */
#define DMA_HDR_PHYVECT_LEN    36

/*
 * Maximum number of payload addresses and lengths present in the descriptor
 */
#define NX_TX_PAYLOAD_MAX      6

/*
 ****************************************************************************************
 */
// c.f LMAC/src/tx/tx_swdesc.h
#if 0
/// Descriptor filled by the Host
struct hostdesc {
#ifdef CONFIG_ASR_SPLIT_TX_BUF
	/// Pointers to packet payloads
	u32 packet_addr[NX_TX_PAYLOAD_MAX];
	/// Sizes of the MPDU/MSDU payloads
	u16 packet_len[NX_TX_PAYLOAD_MAX];
	/// Number of payloads forming the MPDU
	u8 packet_cnt;
#else
	/// Pointer to packet payload
	u32 packet_addr;
	/// Size of the payload
	u16 packet_len;
#endif				//(NX_AMSDU_TX)

	/// Address of the status descriptor in host memory (used for confirmation upload)
	u32 status_desc_addr;
	/// Destination Address
	struct mac_addr eth_dest_addr;
	/// Source Address
	struct mac_addr eth_src_addr;
	/// Ethernet Type
	u16 ethertype;
	/// Buffer containing the PN to be used for this packet
	u16 pn[4];
	/// Sequence Number used for transmission of this MPDU
	u16 sn;
	/// Timestamp of first transmission of this MPDU
	u16 timestamp;
	/// Packet TID (0xFF if not a QoS frame)
	u8 tid;
	/// Interface Id
	u8 vif_idx;
	/// Station Id (0xFF if station is unknown)
	u8 staid;
	/// TX flags
	u16 flags;
};
#else
/// Descriptor filled by the Host
struct hostdesc {
	// tx len filled by host when use tx aggregation mode
	u16 sdio_tx_len;
	// tx total len: used for host
	u16 sdio_tx_total_len;
	// queue id
	u8 queue_idx;		//VO VI BE BK
	/// Pointer to packet payload
	u8 packet_offset;
	/// Size of the payload
	u16 packet_len;
	/// Packet TID
	u8 tid;
	/// VIF index
	u8 vif_idx;
	/// Station Id (0xFF if station is unknown)
	u8 staid;
	// agg_num
	u8 agg_num;
	/// TX flags
	u16 flags;
	/// Timestamp of first transmission of this MPDU
	u16 timestamp;
	/// address of buffer from hostdesc in fw
	u32 packet_addr;
	/// PN that was used for first transmission of this MPDU
	u32 txq;
	u32 reserved;
	/// Sequence Number used for first transmission of this MPDU
	u16 sn;
	/// Ethernet Type
	u16 ethertype;
	/// Destination Address
	struct mac_addr eth_dest_addr;
	/// Source Address
	struct mac_addr eth_src_addr;
};
#endif

struct txdesc_api {
	/// Information provided by Host
	struct hostdesc host;
};

struct txdesc_host {
	u32 ready;

	/// API of the embedded part
	struct txdesc_api api;
};

/// Comes from ipc_dma.h
/// Element in the pool of TX DMA bridge descriptors.
struct dma_desc {
    /** Application subsystem address which is used as source address for DMA payload
      * transfer*/
	u32 src;
    /** Points to the start of the embedded data buffer associated with this descriptor.
     *  This address acts as the destination address for the DMA payload transfer*/
	u32 dest;
	/// Complete length of the buffer in memory
	u16 length;
	/// Control word for the DMA engine (e.g. for interrupt generation)
	u16 ctrl;
	/// Pointer to the next element of the chained list
	u32 next;
};

// Comes from la.h
/// Length of the configuration data of a logic analyzer
#define LA_CONF_LEN          10

/// Structure containing the configuration data of a logic analyzer
struct la_conf_tag {
	u32 conf[LA_CONF_LEN];
	u32 trace_len;
	u32 diag_conf;
};

/// Size of a logic analyzer memory
#define LA_MEM_LEN       (1024 * 1024)

/// Type of errors
enum {
	/// Recoverable error, not requiring any action from Upper MAC
	DBG_ERROR_RECOVERABLE = 0,
	/// Fatal error, requiring Upper MAC to reset Lower MAC and HW and restart operation
	DBG_ERROR_FATAL
};

/// Maximum length of the SW diag trace
#define DBG_SW_DIAG_MAX_LEN   1024

/// Maximum length of the error trace
#define DBG_ERROR_TRACE_SIZE  256

/// Number of MAC diagnostic port banks
#define DBG_DIAGS_MAC_MAX     48

/// Number of PHY diagnostic port banks
#define DBG_DIAGS_PHY_MAX     32

/// Maximum size of the RX header descriptor information in the debug dump
#define DBG_RHD_MEM_LEN      (5 * 1024)

/// Maximum size of the RX buffer descriptor information in the debug dump
#define DBG_RBD_MEM_LEN      (5 * 1024)

/// Maximum size of the TX header descriptor information in the debug dump
#define DBG_THD_MEM_LEN      (10 * 1024)

/// Structure containing the information about the PHY channel that is used
struct phy_channel_info {
	/// PHY channel information 1
	u32 info1;
	/// PHY channel information 2
	u32 info2;
};

/// Debug information forwarded to host when an error occurs
struct dbg_debug_info_tag {
	/// Type of error (0: recoverable, 1: fatal)
	u32 error_type;
	/// Pointer to the first RX Header Descriptor chained to the MAC HW
	u32 rhd;
	/// Size of the RX header descriptor buffer
	u32 rhd_len;
	/// Pointer to the first RX Buffer Descriptor chained to the MAC HW
	u32 rbd;
	/// Size of the RX buffer descriptor buffer
	u32 rbd_len;
	/// Pointer to the first TX Header Descriptors chained to the MAC HW
	u32 thd[NX_TXQ_CNT];
	/// Size of the TX header descriptor buffer
	u32 thd_len[NX_TXQ_CNT];
	/// MAC HW diag configuration
	u32 hw_diag;
	/// Error message
	u32 error[DBG_ERROR_TRACE_SIZE / 4];
	/// SW diag configuration length
	u32 sw_diag_len;
	/// SW diag configuration
	u32 sw_diag[DBG_SW_DIAG_MAX_LEN / 4];
	/// PHY channel information
	struct phy_channel_info chan_info;
	/// Embedded LA configuration
	struct la_conf_tag la_conf;
	/// MAC diagnostic port state
	u16 diags_mac[DBG_DIAGS_MAC_MAX];
	/// PHY diagnostic port state
	u16 diags_phy[DBG_DIAGS_PHY_MAX];
	/// MAC HW RX Header descriptor pointer
	u32 rhd_hw_ptr;
	/// MAC HW RX Buffer descriptor pointer
	u32 rbd_hw_ptr;
};

/// Full debug dump that is forwarded to host in case of error
struct dbg_debug_dump_tag {
	/// Debug information
	struct dbg_debug_info_tag dbg_info;

	/// RX header descriptor memory
	u32 rhd_mem[DBG_RHD_MEM_LEN / 4];

	/// RX buffer descriptor memory
	u32 rbd_mem[DBG_RBD_MEM_LEN / 4];

	/// TX header descriptor memory
	u32 thd_mem[NX_TXQ_CNT][DBG_THD_MEM_LEN / 4];

	/// Logic analyzer memory
	u32 la_mem[LA_MEM_LEN / 4];
};

///
struct rxdesc_tag {
	/// Host Buffer Address
	u32 host_id;
	/// Length
	u32 frame_len;
	/// Status
	u8 status;
};

/**
 ****************************************************************************************
 *  @defgroup IPC IPC
 *  @ingroup NXMAC
 *  @brief Inter Processor Communication module.
 *
 * The IPC module implements the protocol to communicate between the Host CPU
 * and the Embedded CPU.
 *
 * @see http://en.wikipedia.org/wiki/Circular_buffer
 * For more information about the ring buffer typical use and difficulties.
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup IPC_TX IPC Tx path
 *  @ingroup IPC
 *  @brief IPC Tx path structures and functions
 *
 * A typical use case of the IPC Tx path API:
 * @msc
 * hscale = "2";
 *
 * a [label=Driver],
 * b [label="IPC host"],
 * c [label="IPC emb"],
 * d [label=Firmware];
 *
 * ---   [label="Tx descriptor queue example"];
 * a=>a  [label="Driver receives a Tx packet from OS"];
 * a=>b  [label="ipc_host_txdesc_get()"];
 * a<<b  [label="struct txdesc_host *"];
 * a=>a  [label="Driver fill the descriptor"];
 * a=>b  [label="ipc_host_txdesc_push()"];
 * ...   [label="(several Tx desc can be pushed)"];
 * b:>c  [label="Tx desc queue filled IRQ"];
 * c=>>d [label="EDCA sub-scheduler callback"];
 * c<<d  [label="Tx desc queue to pop"];
 * c=>>d [label="UMAC Tx desc callback"];
 * ...   [label="(several Tx desc can be popped)"];
 * d=>d  [label="Packets are sent or discarded"];
 * ---   [label="Tx confirm queue example"];
 * c<=d  [label="ipc_emb_txcfm_push()"];
 * c>>d  [label="Request accepted"];
 * ...   [label="(several Tx cfm can be pushed)"];
 * b<:c  [label="Tx cfm queue filled IRQ"];
 * a<<=b [label="Driver's Tx Confirm callback"];
 * a=>b  [label="ipc_host_txcfm_pop()"];
 * a<<b  [label="struct ipc_txcfm"];
 * a<=a  [label="Packets are freed by the driver"];
 * @endmsc
 *
 * @{
 ****************************************************************************************
 */

/// @} IPC_TX

/**
 ****************************************************************************************
 *  @defgroup IPC_RX IPC Rx path
 *  @ingroup IPC
 *  @brief IPC Rx path functions and structures
 *
 * A typical use case of the IPC Rx path API:
 * @msc
 * hscale = "2";
 *
 * a [label=Firmware],
 * b [label="IPC emb"],
 * c [label="IPC host"],
 * d [label=Driver];
 *
 * ---   [label="Rx buffer and desc queues usage example"];
 * d=>c  [label="ipc_host_rxbuf_push()"];
 * d=>c  [label="ipc_host_rxbuf_push()"];
 * d=>c  [label="ipc_host_rxbuf_push()"];
 * ...   [label="(several Rx buffer are pushed)"];
 * a=>a  [label=" Frame is received\n from the medium"];
 * a<<b  [label="struct ipc_rxbuf"];
 * a=>a  [label=" Firmware fill the buffer\n with received frame"];
 * a<<b  [label="Push accepted"];
 * ...   [label="(several Rx desc can be pushed)"];
 * b:>c  [label="Rx desc queue filled IRQ"];
 * c=>>d [label="Driver Rx packet callback"];
 * c<=d  [label="ipc_host_rxdesc_pop()"];
 * d=>d  [label="Rx packet is handed \nover to the OS "];
 * ...   [label="(several Rx desc can be poped)"];
 * ---   [label="Rx buffer request exemple"];
 * b:>c  [label="Low Rx buffer count IRQ"];
 * a<<b  [label="struct ipc_rxbuf"];
 * c=>>d [label="Driver Rx buffer callback"];
 * d=>c  [label="ipc_host_rxbuf_push()"];
 * d=>c  [label="ipc_host_rxbuf_push()"];
 * d=>c  [label="ipc_host_rxbuf_push()"];
 * ...   [label="(several Rx buffer are pushed)"];
 * @endmsc
 *
 * @addtogroup IPC_RX
 * @{
 ****************************************************************************************
 */

/// @} IPC_RX

/**
 ****************************************************************************************
 *  @defgroup IPC_MISC IPC Misc
 *  @ingroup IPC
 *  @brief IPC miscellaneous functions
 ****************************************************************************************
 */
/** IPC header structure.  This structure is stored at the beginning of every IPC message.
 * @warning This structure's size must NOT exceed 4 bytes in length.
 */
struct ipc_header {
	/// IPC message type.
	u16 type;
	/// IPC message size in number of bytes.
	u16 size;
};

struct ipc_msg_elt {
	/// Message header (alignment forced on word size, see allocation in shared env).
	struct ipc_header header __ALIGN4;
};

/// Message structure for MSGs from Emb to App
struct ipc_e2a_msg {
	u16 id;			///< Message id.
	u16 dummy_dest_id;	///<
	u16 dummy_src_id;	///<
	u16 param_len;		///< Parameter embedded struct length.
	u32 param[IPC_E2A_MSG_PARAM_SIZE];	///< Parameter embedded struct. Must be word-aligned.
};

/// Message structure for Debug messages from Emb to App
struct ipc_dbg_msg {
	u16 id;
	u8 string[IPC_DBG_PARAM_SIZE];	///< Debug string
};

/// Message structure for MSGs from App to Emb.
/// Actually a sub-structure will be used when filling the messages.
struct ipc_a2e_msg {
	u32 dummy_word;		// used to cope with kernel message structure
	u32 msg[IPC_A2E_MSG_BUF_SIZE];	// body of the msg
};

struct ipc_ooo_rxdesc_buf_hdr
{
    uint16_t           id;
	#ifdef CONFIG_ASR_SDIO
	uint16_t           sdio_rx_len;
	#endif
	uint8_t            rxu_stat_desc_cnt;
    /*The rxu_stat_val start here. */
} __packed;

/// RX status value structure (as expected by Upper Layers)
struct rxu_stat_val
{
    // out-of-order value.
	//struct rxu_cntrl_ooo_val ooo_val;
    // use (sta_idx/tid/seq_num/fn_num) to identify one out-of-order pkt.	
	uint8_t   sta_idx;
	uint8_t   tid;
	uint16_t  seq_num;
	uint16_t  amsdu_fn_num;	
    /// Status (@ref rx_status_bits)
    uint8_t status;
} __packed;


/*
 * TYPE and STRUCT DEFINITIONS
 ****************************************************************************************
 */
// FLAGS for RX desc
#define IPC_RX_FORWARD          CO_BIT(1)
#define IPC_RX_INTRABSS         CO_BIT(0)

// IPC message TYPE
enum {
	IPC_MSG_NONE = 0,
	IPC_MSG_WRAP,
	IPC_MSG_KMSG,
	IPC_DBG_STRING,
};

#endif // _IPC_SHARED_H_
