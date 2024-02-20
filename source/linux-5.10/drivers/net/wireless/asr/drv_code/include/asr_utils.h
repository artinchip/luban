/**
 ******************************************************************************
 *
 * @file asr_utils.h
 *
 * @brief IPC utility function declarations
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */

#ifndef _ASR_UTILS_H_
#define _ASR_UTILS_H_

#include <linux/dma-mapping.h>
#include <linux/skbuff.h>
#include <linux/printk.h>

#include "lmac_msg.h"
#include "asr_cmds.h"

#ifdef CONFIG_ASR_DBG

/* message levels */
#define ASR_TRACE_VAL    0x00000002
#define ASR_INFO_VAL    0x00000004
#define ASR_DATA_VAL    0x00000008
#define ASR_CTL_VAL    0x00000010
#define ASR_TIMER_VAL    0x00000020
#define ASR_HDRS_VAL    0x00000040
#define ASR_BYTES_VAL    0x00000080
#define ASR_INTR_VAL    0x00000100
#define ASR_GLOM_VAL    0x00000200
#define ASR_EVENT_VAL    0x00000400
#define ASR_BTA_VAL    0x00000800
#define ASR_FIL_VAL    0x00001000
#define ASR_USB_VAL    0x00002000
#define ASR_SCAN_VAL    0x00004000
#define ASR_CONN_VAL    0x00008000
#define ASR_CDC_VAL    0x00010000
#define ASR_SDIO_VAL    0x00020000

#define asr_err(fmt, ...)    pr_err("%s: " fmt, __func__, ##__VA_ARGS__)

#define ASR_DBG(fmt,...) \
do { \
    __asr_dbg(ASR_TRACE_VAL, __func__,        \
            fmt, ##__VA_ARGS__);            \
} while(0)

__printf(3, 4)
void __asr_dbg(u32 level, const char *func, const char *fmt, ...);
#define asr_dbg(level, fmt, ...)                \
do {                                \
    __asr_dbg(ASR_##level##_VAL, __func__,        \
            fmt, ##__VA_ARGS__);            \
} while (0)

#else
#define asr_err(fmt, ...)    pr_err("%s: " fmt, __func__, ##__VA_ARGS__)
#define ASR_DBG(a...) do {} while (0)
#define asr_dbg(level, fmt, ...) no_printk(fmt, ##__VA_ARGS__)
#endif

#define ASR_FN_ENTRY_STR ">>> %s()\n", __func__


#if 1
// phy_flag saved in asr_hw.
enum asr_phy_flag {
	ASR_DEV_INITED,
    ASR_DEV_RESTARTING,
	ASR_DEV_STARTED,	
	ASR_DEV_PRE_RESTARTING,
};

// dev_flag saved in asr_vif
enum asr_dev_flag {
	ASR_DEV_PRECLOSEING,
	ASR_DEV_CLOSEING,
	ASR_DEV_SCANING,
	ASR_DEV_STA_CONNECTING,
	ASR_DEV_STA_CONNECTED,
	ASR_DEV_STA_DISCONNECTING,
	ASR_DEV_STA_DHCPEND,
	ASR_DEV_STA_DEL_KEY,
	ASR_DEV_STA_OUT_TWTSP,
	ASR_DEV_TXQ_STOP_CSA,
	ASR_DEV_TXQ_STOP_CHAN,
	ASR_DEV_TXQ_STOP_VIF_PS,
};

#else
enum asr_dev_flag {
	ASR_DEV_INITED,
	ASR_DEV_RESTARTING,
	ASR_DEV_STACK_RESTARTING,
	ASR_DEV_STARTED,
	ASR_DEV_PRECLOSEING,
	ASR_DEV_CLOSEING,
	ASR_DEV_SCANING,
	ASR_DEV_STA_CONNECTING,
	ASR_DEV_STA_CONNECTED,
	ASR_DEV_STA_DISCONNECTING,
	ASR_DEV_STA_DHCPEND,
	ASR_DEV_STA_DEL_KEY,
	ASR_DEV_STA_OUT_TWTSP,
	ASR_DEV_TXQ_STOP_CSA,
	ASR_DEV_TXQ_STOP_CHAN,
	ASR_DEV_PRE_RESTARTING,
};
#endif

struct asr_dma_elem {
	u8 *buf;
	dma_addr_t dma_addr;
	int len;
};

struct asr_patternbuf {
	u32 *buf;
	dma_addr_t dma_addr;
	int bufsz;
};

struct asr_labuf {
	u8 *buf;
	dma_addr_t dma_addr;
	int bufsz;
};

struct asr_dbginfo {
	struct mutex mutex;
	struct dbg_debug_dump_tag *buf;
	dma_addr_t dma_addr;
	int bufsz;
};

#define ASR_RXBUFF_PATTERN     (0xCAFEFADE)

/* Maximum number of rx buffer the fw may use at the same time */
#define ASR_RXBUFF_MAX (64 * 10)

#define ASR_RXBUFF_VALID_IDX(idx) ((idx) < ASR_RXBUFF_MAX)

/* Used to ensure that hostid set to fw is never 0 */
#define ASR_RXBUFF_IDX_TO_HOSTID(idx) ((idx) + 1)
#define ASR_RXBUFF_HOSTID_TO_IDX(hostid) ((hostid) - 1)

struct asr_e2arxdesc_elem {
	struct rxdesc_tag *rxdesc_ptr;
	dma_addr_t dma_addr;
};

/*
 * Structure used to store information regarding E2A msg buffers in the driver
 */
struct asr_e2amsg_elem {
	struct ipc_e2a_msg *msgbuf_ptr;
	dma_addr_t dma_addr;
};

/*
 * Structure used to store information regarding Debug msg buffers in the driver
 */
struct asr_dbg_elem {
	struct ipc_dbg_msg *dbgbuf_ptr;
	dma_addr_t dma_addr;
};

/**
 * struct asr_ipc_dbgdump_elem - IPC buffer for debug dump
 *
 * @mutex: Mutex to protect access to debug dump
 * @buf: IPC buffer
 */
struct asr_ipc_dbgdump_elem {
	struct mutex mutex;
	//struct asr_ipc_elem_var buf;
};

//#define LONG_SIZE_DBG
#define RING_DBG
#define LONG_SIZE_DBG_TYEP 2
#define RING_DBG_TYPE 1

#ifdef LONG_SIZE_DBG
#define ASR_SNPRINTF_BUFF 122880
#define ASR_SPRINTF_ITEM_ARR_SIZE  50000
#else
#define ASR_SNPRINTF_BUFF 1
#define ASR_SPRINTF_ITEM_ARR_SIZE  1
#endif
extern u8 asr_snprintf_buf[ASR_SNPRINTF_BUFF + 32];
extern u32 asr_snprintf_idx;
extern u32 asr_sprintf_item_array[ASR_SPRINTF_ITEM_ARR_SIZE + 1];
extern u32 asr_sprintf_item_idx;

#ifdef RING_DBG
#define DBG_RING_BUFF_CNT  3000
#define DBG_RING_BUFF_SIZE 50
#else
#define DBG_RING_BUFF_CNT  1
#define DBG_RING_BUFF_SIZE 1
#endif
typedef struct {
	u8 dbg_ring_buf[DBG_RING_BUFF_CNT][DBG_RING_BUFF_SIZE];
	u32 ring_idx;
} ASR_DBG_RING_T;
extern ASR_DBG_RING_T asr_dbg_ring;

extern int dbg_type;

#if 0
#define dbg(Fmt, ...)\
    do {\
            if(dbg_type == LONG_SIZE_DBG_TYEP) \
            { \
                u32 index;\
                if((asr_snprintf_idx < ASR_SNPRINTF_BUFF) && (asr_sprintf_item_idx < ASR_SPRINTF_ITEM_ARR_SIZE))\
                {\
                    index = sprintf(&asr_snprintf_buf[asr_snprintf_idx], Fmt, ##__VA_ARGS__);\
                    asr_snprintf_idx += index;\
                    asr_sprintf_item_array[asr_sprintf_item_idx] = asr_snprintf_idx;\
                    asr_sprintf_item_idx++;\
                }\
            } \
            else if(dbg_type == RING_DBG_TYPE) \
            { \
                sprintf(asr_dbg_ring.dbg_ring_buf[asr_dbg_ring.ring_idx++], Fmt, ##__VA_ARGS__);\
                if(asr_dbg_ring.ring_idx >= DBG_RING_BUFF_CNT) \
                    asr_dbg_ring.ring_idx = 0; \
            } \
    }while(0)
#endif

#define print_dbg() \
    do {\
            if(dbg_type == LONG_SIZE_DBG_TYEP) \
            { \
                u32 i = 0;\
                for(i = 0; i < asr_sprintf_item_idx; i++)\
                {\
                    printk("lalala:%s",&(asr_snprintf_buf[asr_sprintf_item_array[i]]));\
                }\
                printk("lalala end %u %u\n",asr_snprintf_idx,asr_sprintf_item_idx);\
            } \
            else if(dbg_type == RING_DBG_TYPE) \
            { \
                u32 i = 0;\
                for(i = 0; i < DBG_RING_BUFF_CNT; i++)\
                {\
                    printk("%u lalala:%s",i, asr_dbg_ring.dbg_ring_buf[i]);\
                }\
                printk("lalala end ring_idx: %u\n",asr_dbg_ring.ring_idx);\
            } \
    }while(0)

/**
 ******************************************************************************
 * @brief Initialize IPC interface.
 *
 * This function initializes IPC interface by registering callbacks, setting
 * shared memory area and calling IPC Init function.
 *
 * This function should be called only once during driver's lifetime.
 *
 * @param[in]   asr_hw        Pointer to main structure storing all the
 *                             relevant information
 * @param[in]   ipc_shared_mem
 *
 ******************************************************************************
 */
int asr_ipc_init(struct asr_hw *asr_hw);

/**
 ******************************************************************************
 * @brief Release IPC interface.
 *
 * @param[in]   asr_hw   Pointer to main structure storing all the relevant
 *                        information
 *
 ******************************************************************************
 */
void asr_ipc_deinit(struct asr_hw *asr_hw);

/**
 ******************************************************************************
 * @brief Function called upon DBG_ERROR_IND message reception.
 * This function triggers the UMH script call that will indicate to the user
 * space the error that occurred and stored the debug dump. Once the UMH script
 *  is executed, the asr_umh_done function has to be called.
 *
 * @param[in]   asr_hw   Pointer to main structure storing all the relevant
 *                        information
 *
 ******************************************************************************
 */
void asr_error_ind(struct asr_hw *asr_hw);

/**
 * asr_rxbuff_alloc - Allocate a new skb for RX
 *
 * @asr_hw: Main driver data
 * @len: Length of the buffer to allocate
 * @skb: Pointer to store skb allocated
 *
 * Allocate a skb and map its data buffer for DMA transmission. The dma address
 * is stored in the control buffer of the skb.
 *
 * @return 0 if allocation succeed, !=0 if error occured
 */
int asr_rxbuff_alloc(struct asr_hw *asr_hw, u32 len, struct sk_buff **skb);

struct asr_sta *asr_get_sta(struct asr_hw *asr_hw, const u8 * mac_addr);

/*when use monitor mode, user should register this type of callback function to get the received MPDU*/
typedef void (*monitor_cb_t) (u8 * data, int len);

void asr_stats_update_txrx_rates(struct asr_hw *asr_hw);

#endif /* _ASR_UTILS_H_ */
