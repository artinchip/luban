/**
 ******************************************************************************
 *
 * @file ipc_host.h
 *
 * @brief IPC module.
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */
#ifndef _IPC_HOST_H_
#define _IPC_HOST_H_

/*
 * INCLUDE FILES
 ******************************************************************************
 */
#include "ipc_shared.h"
#ifndef __KERNEL__
#include "arch.h"
#else
#include "ipc_compat.h"
#endif

/*
 * ENUMERATION
 ******************************************************************************
 */

/**
 ******************************************************************************
 * @brief This structure is used to initialize the MAC SW
 *
 * The WLAN device driver provides functions call-back with this structure
 ******************************************************************************
 */
struct ipc_host_cb_tag {
	/// WLAN driver call-back function: recv_data_ind
	u8(*recv_data_ind) (void *pthis, void *host_id);

	/// WLAN driver call-back function: recv_msg_ind
	u8(*recv_msg_ind) (void *pthis, void *host_id);

	/// WLAN driver call-back function: recv_dbg_ind
	u8(*recv_dbg_ind) (void *pthis, void *host_id);
};

/// Definition of the IPC Host environment structure.
struct ipc_host_env_tag {
	/// Structure containing the callback pointers
	struct ipc_host_cb_tag cb;

	// Store the number of Rx Data buffers
	u32 rx_bufnb;
	// Store the size of the Rx Data buffers
	u32 rx_bufsz;
	// Store the number of Rx Data buffers
	u32 rx_bufnb_split;
	// Store the size of the Rx Data buffers
	u32 rx_bufsz_split;

    #ifdef SDIO_DEAGGR
	// Store the number of Rx Data buffers
	u32 rx_bufnb_sdio_deagg;
	// Store the size of the Rx Data buffers
	u32 rx_bufsz_sdio_deagg;
	#endif

	/// Pointer to the attached object (used in callbacks and register accesses)
	void *pthis;
};

extern const int nx_txdesc_cnt[];
extern const int nx_txuser_cnt[];

/**
 ******************************************************************************
 * @brief Initialize the IPC running on the Application CPU.
 *
 * This function:
 *   - initializes the IPC software environments
 *   - enables the interrupts in the IPC block
 *
 * @param[in]   env   Pointer to the IPC host environment
 *
 * @warning Since this function resets the IPC Shared memory, it must be called
 * before the LMAC FW is launched because LMAC sets some init values in IPC
 * Shared memory at boot.
 *
 ******************************************************************************
 */
void ipc_host_init(struct ipc_host_env_tag *env, struct ipc_host_cb_tag *cb, void *pthis);
/**
 ******************************************************************************
 * @brief Handle all IPC interrupts on the host side.
 *
 * The following interrupts should be handled:
 * Tx confirmation, Rx buffer requests, Rx packet ready and kernel messages
 *
 * @param[in]   env   Pointer to the IPC host environment
 *
 ******************************************************************************
 */
int ipc_host_irq(struct ipc_host_env_tag *env, int *type);
int asr_process_int_status(struct ipc_host_env_tag *env, int *type, bool restart_flag);

/**
 ******************************************************************************
 * @brief Send a message to the embedded side
 *
 * @param[in]   env      Pointer to the IPC host environment
 * @param[in]   msg_buf  Pointer to the message buffer
 * @param[in]   msg_len  Length of the message to be transmitted
 *
 * @return      Non-null value on failure
 *
 ******************************************************************************
 */
int ipc_host_msg_push(struct ipc_host_env_tag *env, void *msg_buf, u16 len);

/// @} IPC_MISC

int ipc_host_tx_data_push(struct ipc_host_env_tag *env, u8 * src, u16 len);
int ipc_host_process_rx_sdio(int port, struct asr_hw *asr_hw, struct sk_buff *skb,u8 aggr_num);
int ipc_host_process_rx_usb(u16 id, struct asr_hw *asr_hw, struct sk_buff *skb);
void asr_rx_to_os_task(struct asr_hw *asr_hw);
void asr_sched_timeout(int delay_jitter);

#ifdef CONFIG_ASR_SDIO
int read_sdio_block(struct asr_hw *asr_hw, u8 * dst, unsigned int addr);
int read_sdio_block_retry(struct asr_hw *asr_hw, u8 * sdio_regs, u8 * sdio_ireg);
#endif
void asr_msgind_task(struct asr_hw *asr_hw);

#endif // _IPC_HOST_H_
