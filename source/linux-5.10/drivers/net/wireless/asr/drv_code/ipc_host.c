/**
 ******************************************************************************
 *
 * @file ipc_host.c
 *
 * @brief IPC module.
 *
 * Copyright (C) 2021 ASR Micro Limited.
 * All Rights Reserved.
 *
 ******************************************************************************
 */

/*
 * INCLUDE FILES
 ******************************************************************************
 */
#include <linux/spinlock.h>
#include <linux/ktime.h>
#include "asr_defs.h"

#include "ipc_host.h"
#include "asr_hif.h"
#ifdef CONFIG_ASR_SDIO
#include "asr_sdio.h"
#endif
#include "asr_utils.h"
#include <linux/time.h>
#ifdef CONFIG_ASR_USB
#include "asr_usb.h"
#endif
#include "asr_irqs.h"
/*
 * TYPES DEFINITION
 ******************************************************************************
 */
extern bool txlogen;
extern bool rxlogen;
extern int rx_aggr;
extern int tx_status_debug;
extern int lalalaen;

const int nx_txdesc_cnt[] = {
	NX_TXDESC_CNT0,
	NX_TXDESC_CNT1,
	NX_TXDESC_CNT2,
	NX_TXDESC_CNT3,
#if NX_TXQ_CNT == 5
	NX_TXDESC_CNT4,
#endif
};

const int nx_txdesc_cnt_msk[] = {
	NX_TXDESC_CNT0 - 1,
	NX_TXDESC_CNT1 - 1,
	NX_TXDESC_CNT2 - 1,
	NX_TXDESC_CNT3 - 1,
#if NX_TXQ_CNT == 5
	NX_TXDESC_CNT4 - 1,
#endif
};

const int nx_txuser_cnt[] = {
	CONFIG_USER_MAX,
	CONFIG_USER_MAX,
	CONFIG_USER_MAX,
	CONFIG_USER_MAX,
#if NX_TXQ_CNT == 5
	1,
#endif
};

extern u32 bitcount(u32 num);

/*
 * FUNCTIONS DEFINITIONS
 ******************************************************************************
 */
/**
 ******************************************************************************
 * @brief Handle the reception of a Rx Descriptor
 *
 ******************************************************************************
 */
static void ipc_host_rxdata_handler(struct ipc_host_env_tag *env, struct sk_buff *skb)
{
	// asr_rxdataind
	env->cb.recv_data_ind(env->pthis, (void *)skb);	//asr_rxdataind
}

/**
 ******************************************************************************
 */
static void ipc_host_msg_handler(struct ipc_host_env_tag *env, struct sk_buff *skb)
{
	env->cb.recv_msg_ind(env->pthis, (void *)skb);	//asr_msgind
}

/**
 ******************************************************************************
 */
static void ipc_host_dbg_handler(struct ipc_host_env_tag *env, struct sk_buff *skb)
{
	env->cb.recv_dbg_ind(env->pthis, (void *)skb);	//asr_dbgind
}

bool asr_msgind_task_state = 0;
int asr_msgind_task_run_cnt = 0;
void asr_msgind_task(struct asr_hw *asr_hw)
{
	struct sk_buff *skb;
	asr_msgind_task_state = 1;
	asr_msgind_task_run_cnt++;

	while (!skb_queue_empty(&asr_hw->msgind_task_skb_list)
	       && (skb = skb_dequeue(&asr_hw->msgind_task_skb_list))) {
		ipc_host_msg_handler(asr_hw->ipc_env, skb);
	}
	asr_msgind_task_state = 0;
}

/**
 ******************************************************************************
 */
void ipc_host_init(struct ipc_host_env_tag *env, struct ipc_host_cb_tag *cb, void *pthis)
{
	// Reset the IPC Host environment
	memset(env, 0, sizeof(struct ipc_host_env_tag));

	// Save the callbacks in our own environment
	env->cb = *cb;

	// Save the pointer to the register base
	env->pthis = pthis;

	// Initialize buffers numbers and buffers sizes needed for DMA Receptions

	env->rx_bufnb = IPC_RXBUF_CNT;
	env->rx_bufsz = IPC_RXBUF_SIZE;

    #ifdef SDIO_DEAGGR
	env->rx_bufnb_sdio_deagg = IPC_RXBUF_CNT_SDIO_DEAGG;
	env->rx_bufsz_sdio_deagg = IPC_RXBUF_SIZE_SDIO_DEAGG;
	#endif

	env->rx_bufnb_split = IPC_RXBUF_CNT_SPLIT;
	env->rx_bufsz_split = IPC_RXBUF_SIZE_SPLIT;

}

int sss0_pre = 0;
int sss0 = 0;
int sss1 = 0;
int sss2 = 0;
int sss3 = 0;
int sss4 = 0;
int sss5 = 0;
bool asr_rx_to_os_task_state = 0;
int asr_rx_to_os_task_run_cnt;

void asr_sched_timeout(int delay_jitter)
{
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(delay_jitter);
}
#ifdef CONFIG_ASR_SDIO
void asr_rx_to_os_task(struct asr_hw *asr_hw)
{
	struct sk_buff *skb;
	asr_rx_to_os_task_state = 1;
	asr_rx_to_os_task_run_cnt++;

	while (!skb_queue_empty(&asr_hw->rx_to_os_skb_list)
	       && (skb = skb_dequeue(&asr_hw->rx_to_os_skb_list))) {
		ipc_host_rxdata_handler(asr_hw->ipc_env, skb);
	}
	asr_rx_to_os_task_state = 0;
}


// deaggr skb here, when enable ooo, skb contain data and rxdesc.
#ifdef SDIO_DEAGGR
extern int rx_ooo_upload;
u32 pending_data_cnt;
u32 direct_fwd_cnt;
u32 rxdesc_refwd_cnt;
u32 rxdesc_del_cnt;
u32 min_deaggr_free_cnt = 336;
#define ASR_MAX_AMSDU_RX 4096
u8 asr_sdio_deaggr(struct asr_hw *asr_hw,struct sk_buff *skb_head,u8 aggr_num)
{
    struct sk_buff *skb = skb_head;
    struct host_rx_desc *host_rx_desc = (struct host_rx_desc*)skb->data;

    int msdu_offset = 0;
    int frame_len = 0;
	u16 sdio_rx_len = 0;
    int num = aggr_num;
	bool need_wakeup_rxos_thread = false;
    #ifdef CFG_OOO_UPLOAD
    bool forward_skb = false;
    #ifdef CONFIG_ASR_KEY_DBG
    int free_deaggr_skb_cnt;
    #endif
    #endif

    while(num--)
    {
            #ifdef CFG_OOO_UPLOAD
            if (host_rx_desc->id == HOST_RX_DATA_ID)
			#endif
			{
					sdio_rx_len = host_rx_desc->sdio_rx_len;
				    if (!skb_queue_empty(&asr_hw->rx_sk_sdio_deaggr_list) && (skb = skb_dequeue(&asr_hw->rx_sk_sdio_deaggr_list)))
					{
			            // get data skb.
			            msdu_offset = host_rx_desc->pld_offset;
						frame_len = host_rx_desc->frmlen;
						
						if (host_rx_desc->totol_frmlen > ASR_MAX_AMSDU_RX)
						    dev_err(asr_hw->dev, "unlikely host_rx_desc : %d, %d , %d  \n", msdu_offset,frame_len,host_rx_desc->totol_frmlen);
						
			            memcpy(skb->data, host_rx_desc, msdu_offset + frame_len);

	                    #ifdef CFG_OOO_UPLOAD
						// ooo refine : check if pkt is forward.
						if (rx_ooo_upload) {
								struct host_rx_desc *host_rx_desc = (struct host_rx_desc *)skb->data;
								//dev_err(asr_hw->dev, "RX_DATA_ID : host_rx_desc =%p , rx_status=0x%x \n", host_rx_desc,host_rx_desc->rx_status);

								if (host_rx_desc && (host_rx_desc->rx_status & RX_STAT_FORWARD)){
									forward_skb = true;
									direct_fwd_cnt++;
								} else if (host_rx_desc && (host_rx_desc->rx_status & RX_STAT_ALLOC)){
									skb_queue_tail(&asr_hw->rx_pending_skb_list, skb);
									forward_skb = false;
									pending_data_cnt++;

									if (lalalaen)
										dev_err(asr_hw->dev, "RX_DATA_ID pending: host_rx_desc =%p , rx_status=0x%x (%d %d %d %d)\n", host_rx_desc,
																	host_rx_desc->rx_status,
																	host_rx_desc->sta_idx,
																	host_rx_desc->tid,
																	host_rx_desc->seq_num,
																	host_rx_desc->fn_num);
								} else {
									dev_err(asr_hw->dev, "unlikely: host_rx_desc =%p , rx_status=0x%x\n", host_rx_desc,host_rx_desc->rx_status);
									//dev_kfree_skb(skb);
									memset(skb->data, 0, asr_hw->ipc_env->rx_bufsz_sdio_deagg);
									// Add the sk buffer structure in the table of rx buffer
									skb_queue_tail(&asr_hw->rx_sk_sdio_deaggr_list, skb);	
								}

						} else {
							forward_skb = true;
						}

						if (forward_skb)
		                #endif  //CFG_OOO_UPLOAD
						{
							skb_queue_tail(&asr_hw->rx_to_os_skb_list, skb);
							need_wakeup_rxos_thread = true;
						}

		            }else {
		                pr_err("%s: unlikely, rx_sk_sdio_deaggr_list empty\n", __func__);
		            }
            }
            #ifdef CFG_OOO_UPLOAD
			else if ((host_rx_desc->id == HOST_RX_DESC_ID) && rx_ooo_upload) {
				// ooo refine: parse ooo rxdesc
				//	   RX_STAT_FORWARD : move pkt from rx_pending_skb_list to rx_to_os_skb_list and trigger
				//	   RX_STAT_DELETE  : move pkt from rx_pending_skb_list and free.

				struct ipc_ooo_rxdesc_buf_hdr *ooo_rxdesc = (struct ipc_ooo_rxdesc_buf_hdr *)host_rx_desc;
				uint8_t rxu_stat_desc_cnt = ooo_rxdesc->rxu_stat_desc_cnt;
				struct rxu_stat_val *rxu_stat_handle;
				uint8_t * start_addr = (void *)ooo_rxdesc + sizeof(struct ipc_ooo_rxdesc_buf_hdr);
				int forward_pending_skb_cnt = 0;
				struct sk_buff *skb_pending;
				struct sk_buff *pnext;
				struct host_rx_desc *host_rx_desc_pending = NULL;
				uint16_t ooo_match_cnt = 0;

			    sdio_rx_len = ooo_rxdesc->sdio_rx_len;

				if (lalalaen)
					dev_err(asr_hw->dev, "RX_DESC_ID : ooo_rxdesc =%p , rxu_stat_desc_cnt=%d ,start_addr=%p,hdr_len=%u\n", ooo_rxdesc,rxu_stat_desc_cnt,start_addr,
															 (unsigned int)sizeof(struct ipc_ooo_rxdesc_buf_hdr));
				if (ooo_rxdesc && rxu_stat_desc_cnt) {

					while (rxu_stat_desc_cnt > 0){
						   rxu_stat_handle = (struct rxu_stat_val *)start_addr;

							if (lalalaen)
								dev_err(asr_hw->dev, "RX_DESC_ID search: ooo_rxdesc =%p cnt=%d, rxu_stat_handle status=0x%x (%d %d %d %d)\n", ooo_rxdesc,rxu_stat_desc_cnt,
														rxu_stat_handle->status,
														rxu_stat_handle->sta_idx,
														rxu_stat_handle->tid,
														rxu_stat_handle->seq_num,
														rxu_stat_handle->amsdu_fn_num);

						   // search and get from asr_hw->rx_pending_skb_list.
						   skb_queue_walk_safe(&asr_hw->rx_pending_skb_list, skb_pending, pnext) {
							   host_rx_desc_pending = (struct host_rx_desc *)skb_pending->data;
							   ooo_match_cnt = 0;

								if (lalalaen)
									dev_err(asr_hw->dev, "RX_DESC_ID walk: pending host_rx_desc rx_status=0x%x (%d %d %d %d)\n",
														host_rx_desc_pending->rx_status,
														host_rx_desc_pending->sta_idx,
														host_rx_desc_pending->tid,
														host_rx_desc_pending->seq_num,
														host_rx_desc_pending->fn_num);

							   if ((host_rx_desc_pending->sta_idx == rxu_stat_handle->sta_idx) &&
								   (host_rx_desc_pending->tid	  == rxu_stat_handle->tid) &&
								   (host_rx_desc_pending->seq_num == rxu_stat_handle->seq_num) &&
								   (host_rx_desc_pending->fn_num  <= rxu_stat_handle->amsdu_fn_num)) {
			
								   skb_unlink(skb_pending, &asr_hw->rx_pending_skb_list);
								   ooo_match_cnt++;

								   if (lalalaen)
									   dev_err(asr_hw->dev, "RX_DESC_ID hit: ooo_rxdesc =%p cnt=%d, rx_status=0x%x (%d %d %d %d)\n", ooo_rxdesc,rxu_stat_desc_cnt,
														host_rx_desc_pending->rx_status,
														host_rx_desc_pending->sta_idx,
														host_rx_desc_pending->tid,
														host_rx_desc_pending->seq_num,
														host_rx_desc_pending->fn_num);

								   if (rxu_stat_handle->status & RX_STAT_FORWARD){
									   host_rx_desc_pending->rx_status |= RX_STAT_FORWARD;
									   skb_queue_tail(&asr_hw->rx_to_os_skb_list, skb_pending);
									   forward_pending_skb_cnt++;
									   rxdesc_refwd_cnt++;
								   } else if (rxu_stat_handle->status & RX_STAT_DELETE) {
									   //dev_kfree_skb(skb_pending);
									   memset(skb_pending->data, 0, asr_hw->ipc_env->rx_bufsz_sdio_deagg);
									   // Add the sk buffer structure in the table of rx buffer
									   skb_queue_tail(&asr_hw->rx_sk_sdio_deaggr_list, skb_pending);
                                       rxdesc_del_cnt++;
								   } else {
									   dev_err(asr_hw->dev, "unlikely: status =0x%x\n", rxu_stat_handle->status);
								   }
							   }
							   if (ooo_match_cnt == (rxu_stat_handle->amsdu_fn_num + 1))
								   break;
						   }
			
						   //get next rxu_stat_val
						   start_addr += sizeof(struct rxu_stat_val);
						   rxu_stat_desc_cnt--;
					}
			
					// trigger
					if (forward_pending_skb_cnt) {
						need_wakeup_rxos_thread = true;
					}
				}			
			}else {
				dev_err(asr_hw->dev, "id = 0x%x, not support! \n", host_rx_desc->id);
			}
            #endif	//CFG_OOO_UPLOAD

            if(num)
            {
                // next rx data or rxdesc.
                host_rx_desc = (struct host_rx_desc*)((u8*)host_rx_desc + sdio_rx_len);
            }
    }

    #ifdef CONFIG_ASR_KEY_DBG
	free_deaggr_skb_cnt  = skb_queue_len(&asr_hw->rx_sk_sdio_deaggr_list);
	if (free_deaggr_skb_cnt < min_deaggr_free_cnt)
		min_deaggr_free_cnt = free_deaggr_skb_cnt;
	#endif

    if (need_wakeup_rxos_thread) {
		set_bit(ASR_FLAG_RX_TO_OS_BIT, &asr_hw->ulFlag);
		wake_up_interruptible(&asr_hw->waitq_rx_to_os_thead);
    }

    // deagg sdio buf done, free skb_head here.
	memset(skb_head->data, 0, asr_hw->ipc_env->rx_bufsz);
	// Add the sk buffer structure in the table of rx buffer
	#ifndef SDIO_RXBUF_SPLIT
	skb_queue_tail(&asr_hw->rx_sk_list, skb_head);
	#else
	skb_queue_tail(&asr_hw->rx_data_sk_list, skb_head);
	#endif

    return 0;
}
#endif

int ipc_host_process_rx_sdio(int port, struct asr_hw *asr_hw, struct sk_buff *skb,u8 aggr_num)
{
	u8 type;
	ASR_DBG(ASR_FN_ENTRY_STR);

	if (port == SDIO_PORT_IDX(0))	//msg
	{
		skb_queue_tail(&asr_hw->msgind_task_skb_list, skb);
		set_bit(ASR_FLAG_MSGIND_TASK_BIT, &asr_hw->ulFlag);
		wake_up_interruptible(&asr_hw->waitq_msgind_task_thead);
		type = HIF_RX_MSG;
	}
	else if (port == SDIO_PORT_IDX(1))	//log
	{
		ipc_host_dbg_handler(asr_hw->ipc_env, skb);
		type = HIF_RX_LOG;
	}
	else {

		#ifndef SDIO_DEAGGR
		skb_queue_tail(&asr_hw->rx_to_os_skb_list, skb);
		set_bit(ASR_FLAG_RX_TO_OS_BIT, &asr_hw->ulFlag);
		wake_up_interruptible(&asr_hw->waitq_rx_to_os_thead);
		#else

		// start_port > 1, de-agg sdio multi-port read here.
		// when enable ooo, sdio buf contain data or rxdesc.
                // like ipc_host_process_rx_usb , process ooo here.
		asr_sdio_deaggr(asr_hw,skb,aggr_num);
		#endif
		type = HIF_RX_DATA;
	}

	return type;
}

extern bool test_host_re_read;

static int port_dispatch(struct asr_hw *asr_hw, u16 port_size,
			 u8 start_port, u8 end_port, u8 io_addr_bmp, u8 aggr_num, const u8 asr_sdio_reg[])
{
#define ASR_IOPORT(hw) ((hw)->ioport & (0xffff0000))
	//u8 reg_val;
	int type;
	int ret = 0;
	u32 addr;
	struct sk_buff *skb = NULL;
	struct sdio_func *func = asr_hw->plat->func;
	//int seconds;

	ASR_DBG(ASR_FN_ENTRY_STR);

	//adjust port_size, must be block size blocks, guarantee by FW
	//if(port_size%SDIO_BLOCK_SIZE)
	//    port_size = port_size+(SDIO_BLOCK_SIZE - (port_size%SDIO_BLOCK_SIZE));

	if (start_port == end_port) {
		port_size = asr_sdio_reg[RD_LEN_L(start_port)];
		port_size |= asr_sdio_reg[RD_LEN_U(start_port)] << 8;
		if (port_size == 0) {
			dev_err(asr_hw->dev, "%s read bitmap port %d value:0x%x\n", __func__, start_port, port_size);
			return -ENODEV;
		}
		//dev_err(asr_hw->dev, "!!!!len2 %d\n",port_size);

		addr = ASR_IOPORT(asr_hw) | start_port;
	} else {		//aggregation mode
		addr = ASR_IOPORT(asr_hw) | 0x1000 | start_port | (io_addr_bmp << 4);
	}

    #ifndef SDIO_RXBUF_SPLIT
	skb = skb_dequeue(&asr_hw->rx_sk_list);
	#else
    if (start_port == SDIO_PORT_IDX(0))
	    skb = skb_dequeue(&asr_hw->rx_msg_sk_list);
    else
	    skb = skb_dequeue(&asr_hw->rx_data_sk_list);
	#endif
	if (!skb) {
		dev_err(asr_hw->dev, "%s no more skb buffer\n", __func__);
		return -ENOMEM;
	}
	//seconds = ktime_to_us(ktime_get());
	//dbg("rdb %d\n%s",seconds, "\0");

#ifndef SDIO_RXBUF_SPLIT
	if (port_size > IPC_RXBUF_SIZE) {
		dev_err(asr_hw->dev, "[%s][%d %d]!!!!len2 %d\n", __func__, start_port, end_port, port_size);
	}
#else
	if (((port_size > IPC_RXBUF_SIZE) && (start_port != SDIO_PORT_IDX(0))) ||
		((port_size > IPC_MSG_RXBUF_SIZE) && (start_port == SDIO_PORT_IDX(0)))) {
		dev_err(asr_hw->dev, "[%s][%d %d]!!!!len2 %d\n", __func__, start_port, end_port, port_size);
	}
#endif

	if (test_host_re_read) {
		ret = -84;
	} else {
		sdio_claim_host(func);
		ret = sdio_readsb(func, skb->data, addr, port_size);
		sdio_release_host(func);
	}

	//sss5 = ktime_to_us(ktime_get());

	//seconds = ktime_to_us(ktime_get());
	//dbg("rda %d\n%s",((int)ktime_to_us(ktime_get())-seconds), "\0");

	if (ret) {
		dev_err(asr_hw->dev, "%s read data failed %x %x\n", __func__, addr, port_size);

        #ifndef SDIO_RXBUF_SPLIT
		memset(skb->data, 0, asr_hw->ipc_env->rx_bufsz);
		skb_queue_tail(&asr_hw->rx_sk_list, skb);
		#else
        if (start_port == SDIO_PORT_IDX(0)) {
		    memset(skb->data, 0, IPC_MSG_RXBUF_SIZE);
		    skb_queue_tail(&asr_hw->rx_msg_sk_list, skb);
		} else {
		    memset(skb->data, 0, asr_hw->ipc_env->rx_bufsz);
		    skb_queue_tail(&asr_hw->rx_data_sk_list, skb);
		}
		#endif
		return ret;
	}

	#ifndef SDIO_DEAGGR
	if (start_port > 1) {
		((struct host_rx_desc *)skb->data)->num = aggr_num;
	}

	//had read data,call back function
	type = ipc_host_process_rx_sdio(start_port, asr_hw, skb,aggr_num);
	#else

    // start_port > 1, de-agg sdio multi-port read here.
    // add new list for multi-port.   data or rxdesc.

	type = ipc_host_process_rx_sdio(start_port, asr_hw, skb,aggr_num);

	#endif

	if (type == HIF_RX_DATA)
		asr_hw->stats.last_rx = jiffies;

	return 0;
}

u8 asr_get_rx_aggr_end_port(struct asr_hw *asr_hw, u8 start_port,
			    u16 * hw_bitmap, u8 * io_addr_bitmap, u8 * num, u16 * len, const u8 asr_sdio_reg[])
{
	u8 end_port = start_port;
	u16 port_size;

	*io_addr_bitmap = 0;
	*num = 0;
	*len = 0;

	//while(bitcount(*hw_bitmap)>1)
	while (*hw_bitmap) {
		if (*hw_bitmap & (1 << end_port)) {
			*hw_bitmap &= ~(1 << end_port);

			port_size = asr_sdio_reg[RD_LEN_L(end_port)];
			port_size |= asr_sdio_reg[RD_LEN_U(end_port)] << 8;
			if (port_size == 0) {
				dev_err(asr_hw->dev, "%s read bitmap port %d value:0x%x\n", __func__, end_port,
					port_size);
			}
			//if(lalalaen)
			//    dev_err(asr_hw->dev, "len %d %d %d",port_size,asr_hw->sdio_reg[RD_LEN_L(end_port)],asr_hw->sdio_reg[RD_LEN_U(end_port)]);
			
			// add max len check.
			if ((*len + port_size) > IPC_RXBUF_SIZE) {
			    dev_err(asr_hw->dev, "len=%d oversize,max=%d ,%d %d",*len,IPC_RXBUF_SIZE,start_port,end_port);
			
			    if (end_port == 2)
				    end_port = 15;
				else
				    end_port--;
					
				return end_port;
			}
			
			*len += port_size;

			if (end_port < start_port) {
				*io_addr_bitmap |= (1 << (*num + 2));
				(*num)++;
				if (*num >= 6)
					return end_port;
				end_port++;
			} else {
				*io_addr_bitmap |= (1 << *num);
				(*num)++;
				if (*num >= 8)
					return end_port;
				if (++end_port == 16) {
					if (*num >= 6) {
						end_port--;
						return end_port;
					} else
						end_port = 2;
				}
			}
		} else {
			dev_err(asr_hw->dev, "[%s]lalala cur_port_idx %d no data,start_port=%d\n", __func__, end_port, start_port);

			if (end_port == 2)
				end_port = 15;
			else
				end_port--;

			return end_port;
		}
	}

	/* can we arrive here? */
	if (end_port == 2)
		end_port = 15;
	else
		end_port--;

	return end_port;
}

/**
 ******************************************************************************
 */
uint16_t same_tx_bitmap = 0;
uint32_t same_tx_bitmap_cnt = 0;
#ifdef CONFIG_ASR_KEY_DBG
uint32_t diff_bitmap_no_cnt = 0;
uint32_t diff_bitmap_cnt = 0;
extern int no_avail_port_flag;
#endif

#ifdef CONFIG_ASR_KEY_DBG
u32 rx_agg_port_num[8] = { 0 };

u32 aggr_buf_cnt_100 = 0;
u32 aggr_buf_cnt_80 = 0;
u32 aggr_buf_cnt_40 = 0;
u32 aggr_buf_cnt_8 = 0;
u32 aggr_buf_cnt_less_8 = 0;

static void tx_aggr_stat(struct asr_tx_agg *tx_agg_env, u16 diff_bitmap)
{
	if (tx_status_debug) {
		if ((diff_bitmap == 0)) {
			diff_bitmap_no_cnt++;
			no_avail_port_flag = 0;
		} else if ((diff_bitmap)) {
			diff_bitmap_cnt++;
			/* ?? =1 */
			no_avail_port_flag = 0;
		}

		if (tx_agg_env->aggr_buf_cnt > 100) {
			aggr_buf_cnt_100++;
		} else if (tx_agg_env->aggr_buf_cnt > 80) {
			aggr_buf_cnt_80++;
		} else if (tx_agg_env->aggr_buf_cnt > 40) {
			aggr_buf_cnt_40++;
		} else if (tx_agg_env->aggr_buf_cnt >= 8) {
			aggr_buf_cnt_8++;
		} else if (tx_agg_env->aggr_buf_cnt) {
			aggr_buf_cnt_less_8++;
		}
	}
}

static void rx_aggr_stat(struct asr_hw *asr_hw, u8 aggr_num)
{
	// legal value: 1~8
	if (aggr_num > 0 && aggr_num < 9) {
		rx_agg_port_num[aggr_num - 1]++;
	} else {
		dev_err(asr_hw->dev, "wrong aggr_num %d\n", aggr_num);
	}
}
#else // CONFIG_ASR_KEY_DBG
static void tx_aggr_stat(struct asr_tx_agg *tx_agg_env, u16 diff_bitmap)
{
}

static void rx_aggr_stat(struct asr_hw *asr_hw, u8 aggr_num)
{
}
#endif // CONFIG_ASR_KEY_DBG

extern struct timespec irq_start, irq_stage1;

extern int tx_aggr;
extern int tx_debug;
extern bool irq_double_edge_en;

int ipc_read_sdio_reg_cnt;
int ipc_read_sdio_data_cnt;
int ipc_write_sdio_data_cnt;
int ipc_read_sdio_data_restart_cnt;

long tx_evt_irq_times;
int tx_bitmap_change_cnt[16];

//#define SDIO_IRQ_BOTH_TRI
extern int rx_thread_timer;
extern bool flag_rx_thread_timer;
extern bool main_task_clr_irq_en;
extern bool asr_xmit_opt;

u32 asr_rx_pkt_cnt;
int more_task_update_bm_cnt;
extern int asr_rxdataind_run_cnt;
extern bool asr_rx_to_os_task_state;
extern int main_task_from_thread_cnt;


#ifndef SDIO_RXBUF_SPLIT
int asr_wait_rx_sk_buf(struct asr_hw *asr_hw)
{
	int wait_mem_cnt = 0;
	int ret = 0;

	do {
		wait_mem_cnt++;
		asr_sched_timeout(2);	// wait 20ms

	} while ((skb_queue_empty(&asr_hw->rx_sk_list))
		 && (wait_mem_cnt < 100));

	if (skb_queue_empty(&asr_hw->rx_sk_list))
		ret = -1;

	dev_err(asr_hw->dev, "%s ,ret=%d \n", __func__,ret);

	return ret;
}
#else
int asr_wait_rx_data_sk_buf(struct asr_hw *asr_hw)
{
	int wait_mem_cnt = 0;
	int ret = 0;

	do {
		wait_mem_cnt++;
		asr_sched_timeout(2);	// wait 20ms

	} while ((skb_queue_empty(&asr_hw->rx_data_sk_list))
		 && (wait_mem_cnt < 100));

	if (skb_queue_empty(&asr_hw->rx_data_sk_list))
		ret = -1;

	dev_err(asr_hw->dev, "%s ,ret=%d \n", __func__,ret);

	return ret;
}

int asr_wait_rx_msg_sk_buf(struct asr_hw *asr_hw)
{
	int wait_mem_cnt = 0;
	int ret = 0;

	do {
		wait_mem_cnt++;
		asr_sched_timeout(2);	// wait 20ms

	} while ((skb_queue_empty(&asr_hw->rx_msg_sk_list))
		 && (wait_mem_cnt < 100));

	if (skb_queue_empty(&asr_hw->rx_msg_sk_list))
		ret = -1;

	dev_err(asr_hw->dev, "%s ,ret=%d \n", __func__,ret);

	return ret;
}
#endif

extern int min_rx_skb_free_cnt;
extern struct mutex asr_process_int_mutex;

int read_sdio_block(struct asr_hw *asr_hw, u8 * dst, unsigned int addr)
{
	int ret;

	if (!asr_hw->sdio_reg_buff) {
		dev_err(asr_hw->dev, "%s: sdio_reg_buff is null\n", __func__);
		return -1;
	}

	/*  read newest wr_bitmap/rd_bitmap/rd_len to update sdio_reg */
	sdio_claim_host(asr_hw->plat->func);
	ret = sdio_readsb(asr_hw->plat->func, asr_hw->sdio_reg_buff, addr, SDIO_REG_READ_LENGTH);
	sdio_release_host(asr_hw->plat->func);

	spin_lock_bh(&asr_hw->int_reg_lock);
	memcpy(dst, asr_hw->sdio_reg_buff, SDIO_REG_READ_LENGTH);
	spin_unlock_bh(&asr_hw->int_reg_lock);

	return ret;
}

int read_sdio_block_retry(struct asr_hw *asr_hw, u8 * sdio_regs, u8 * sdio_ireg)
{
	u16 rd_bitmap = 0;
	u16 wr_bitmap = 0;
	u16 diff_bitmap = 0;
	int ret = 0;

	if (read_sdio_block(asr_hw, sdio_regs, 0)) {
		dev_err(asr_hw->dev, "%s isr read sdio_reg fail!!! \n", __func__);
		return -1;
	}

	if (sdio_regs[HOST_INT_STATUS]) {
		sdio_claim_host(asr_hw->plat->func);
		sdio_writeb(asr_hw->plat->func, 0x0, HOST_INT_STATUS, &ret);
		sdio_release_host(asr_hw->plat->func);
		if (ret) {
			return -1;
		}
	}

	spin_lock_bh(&asr_hw->int_reg_lock);

	*sdio_ireg = asr_hw->sdio_ireg;
	*sdio_ireg |= sdio_regs[HOST_INT_STATUS];

	rd_bitmap = sdio_regs[RD_BITMAP_L] | sdio_regs[RD_BITMAP_U] << 8;

	if (!(*sdio_ireg & HOST_INT_UPLD_ST)
	    && ((rd_bitmap & 0x1) || count_bits(rd_bitmap, 2) >= SDIO_RX_AGG_TRI_CNT)) {

		//dev_err(asr_hw->dev,"%s: recover rx\n", __func__);

		*sdio_ireg |= HOST_INT_UPLD_ST;
		asr_hw->sdio_reg[HOST_INT_STATUS] |= HOST_INT_UPLD_ST;
	}

	wr_bitmap = sdio_regs[WR_BITMAP_L] | sdio_regs[WR_BITMAP_U] << 8;
	diff_bitmap = ((asr_hw->tx_use_bitmap) ^ wr_bitmap);

	if (!(*sdio_ireg & HOST_INT_DNLD_ST)
	    && (count_bits(diff_bitmap, 1) >= SDIO_TX_AGG_TRI_CNT)) {

		//dev_err(asr_hw->dev,"%s: recover tx\n", __func__);

		*sdio_ireg |= HOST_INT_DNLD_ST;
		asr_hw->sdio_reg[HOST_INT_STATUS] |= HOST_INT_DNLD_ST;
	}
	if (*sdio_ireg & HOST_INT_DNLD_ST || *sdio_ireg & HOST_INT_UPLD_ST) {
		memcpy(asr_hw->sdio_reg, sdio_regs, SDIO_REG_READ_LENGTH);
		memcpy(asr_hw->last_sdio_regs, asr_hw->sdio_reg, SDIO_REG_READ_LENGTH);
	}

	//dev_err(asr_hw->dev, "%s:0x%X,0x%X,0x%X\n", __func__, asr_hw->sdio_ireg, sdio_regs[HOST_INT_STATUS], *sdio_ireg);

	asr_hw->sdio_ireg = 0;

	spin_unlock_bh(&asr_hw->int_reg_lock);

	return 0;
}

static bool host_restart(struct asr_hw *asr_hw, u8 aggr_num)
{
	int err_ret;
	u8 fw_reg_idx, fw_reg_num;
	int wait_fw_cnt = 0;
	bool re_read_done_flag = false;
	struct sdio_func *func = asr_hw->plat->func;

	dev_err(asr_hw->dev, "H2C_INTEVENT(0x0):0x4 firstly terminate the cmd53! (%d %d) \n", asr_hw->rx_data_cur_idx,
		aggr_num);

	//step 1 : host set aggr parameter and finish cmd53.
	sdio_claim_host(func);
        // clear fw notify reg first.
	sdio_writeb(func, 0, SCRATCH1_2, &err_ret);
	sdio_writeb(func, 0, SCRATCH1_3, &err_ret);

	sdio_writeb(func, asr_hw->rx_data_cur_idx, SCRATCH1_0, &err_ret);
	sdio_writeb(func, aggr_num, SCRATCH1_1, &err_ret);

	sdio_writeb(func, 0x4, H2C_INTEVENT, &err_ret);
	sdio_release_host(func);

	// step 2: trigger h2c event.
	sdio_claim_host(func);
	sdio_writeb(func, 0x8, H2C_INTEVENT, &err_ret);	// trigger host to card event for test
	sdio_release_host(func);

	//wait for fw ready for this idx
	do {
		int ret;
		asr_sched_timeout(2);
		wait_fw_cnt++;

		sdio_claim_host(func);
		fw_reg_idx = sdio_readb(func, SCRATCH1_2, &ret);
		fw_reg_num = sdio_readb(func, SCRATCH1_3, &ret);
		sdio_release_host(func);

		dev_err(asr_hw->dev, "H2C host restart check:%d %d \n", fw_reg_idx, fw_reg_num);

		if ((fw_reg_idx == asr_hw->rx_data_cur_idx)
		    && (fw_reg_num == aggr_num)) {
			re_read_done_flag = true;
			break;
		}

		dev_err(asr_hw->dev, "host restart retry:wait_cnt=%d \n", wait_fw_cnt);

	} while (wait_fw_cnt < 5);

	dev_err(asr_hw->dev, "HOST_RESTART(0x28):0x2 secondly restart upload,wait_cnt=%d \n", wait_fw_cnt);
	return re_read_done_flag;
}

extern int port_dispatch_nomem_retry(struct asr_hw *asr_hw, u16 size,
				     u8 start, u8 end, u8 io_addr_bmp, u8 aggr_num, const u8 regs[])
{
	int wait_mem_cnt = 0;

	do {
		int rx_skb_free_cnt, rx_skb_to_os_cnt, rx_sk_split_cnt;

		int ret = port_dispatch(asr_hw, size, start, end,
					io_addr_bmp, aggr_num, regs);

		if (ret != -ENOMEM)
			return ret;

		asr_sched_timeout(2);	// wait 20ms

		#ifndef SDIO_RXBUF_SPLIT
		rx_skb_free_cnt = skb_queue_len(&asr_hw->rx_sk_list);
		#else
		rx_skb_free_cnt = skb_queue_len(&asr_hw->rx_data_sk_list);
		#endif
		rx_skb_to_os_cnt = skb_queue_len(&asr_hw->rx_to_os_skb_list);
		rx_sk_split_cnt = skb_queue_len(&asr_hw->rx_sk_split_list);
		dev_err(asr_hw->dev,
			"lalala rx fail ENOMEM,wait & re-read: %d %d ,free skb_cnt=%d,to_os_cnt=%d %d, (0x%x) %d %d %d\n",
			ret, wait_mem_cnt, rx_skb_free_cnt, rx_skb_to_os_cnt, rx_sk_split_cnt,
			(unsigned int)asr_hw->ulFlag, asr_rxdataind_run_cnt, asr_rx_to_os_task_state,
			main_task_from_thread_cnt);

	} while (wait_mem_cnt++ < 100);

	return -ENOMEM;
}

static int host_int_tx(struct asr_hw *asr_hw, u8 * regs)
{
	u16 wr_bitmap, diff_bitmap;

	//read tx available port
	wr_bitmap = regs[WR_BITMAP_L];
	wr_bitmap |= regs[WR_BITMAP_U] << 8;

	//update tx available port
#ifdef TXBM_DELAY_UPDATE
	//reserve 1 bit to delay updating
	wr_bitmap &= ~(asr_hw->tx_last_trans_bitmap);
#endif
	spin_lock_bh(&asr_hw->tx_msg_lock);
	diff_bitmap = ((asr_hw->tx_use_bitmap) ^ wr_bitmap);
	asr_hw->tx_use_bitmap = wr_bitmap;
	spin_unlock_bh(&asr_hw->tx_msg_lock);

#ifdef CONFIG_ASR_KEY_DBG
	if (txlogen) {
		dev_info(asr_hw->dev, "[%d]wbm 0x%x 0x%x, %d\n", 0, wr_bitmap, diff_bitmap, asr_hw->tx_data_cur_idx);
	}
#endif
    if (asr_xmit_opt == false)
	    tx_aggr_stat(&(asr_hw->tx_agg_env), diff_bitmap);

	if (diff_bitmap && (bitcount(diff_bitmap) <= 16)) {
		tx_bitmap_change_cnt[bitcount(diff_bitmap) - 1]++;
	}

	#if 0
	if (asr_hw->last_sdio_regs[WR_BITMAP_L] != regs[WR_BITMAP_L]
	    || asr_hw->last_sdio_regs[WR_BITMAP_U] != regs[WR_BITMAP_U]) {

		asr_hw->sdio_reg[WR_BITMAP_L] = regs[WR_BITMAP_L];
		asr_hw->sdio_reg[WR_BITMAP_U] = regs[WR_BITMAP_U];
		asr_hw->last_sdio_regs[WR_BITMAP_L] = regs[WR_BITMAP_L];
		asr_hw->last_sdio_regs[WR_BITMAP_U] = regs[WR_BITMAP_U];
	}
	#endif

	ipc_write_sdio_data_cnt++;
	return 0;
}

static u16 rx_aggregate(struct asr_hw *asr_hw, u16 rd_bitmap, u8 * regs)
{
	u8 aggr_num = 0;
	u16 rd_bitmap_ori = regs[RD_BITMAP_L] | regs[RD_BITMAP_U] << 8;
	//struct sdio_func *func = asr_hw->plat->func;
	u8 start_port = asr_hw->rx_data_cur_idx;
	u8 end_port;
	u8 io_addr_bmp;
	u16 len;
	u16 rd_bitmap_before_get = rd_bitmap;
	int ret;

	//while(bitcount(rd_bitmap) > 1) // bitmap cnt <1 , remian 1 bitmap.
	if (!(rd_bitmap & (1 << start_port))) {
		dev_err(asr_hw->dev, "%s:cur_rx_idx %d no data(0x%x,0x%x)\n", __func__, start_port, rd_bitmap,
			rd_bitmap_ori);

		return 0;
	}

	end_port = asr_get_rx_aggr_end_port(asr_hw, start_port, &rd_bitmap, &io_addr_bmp, &aggr_num, &len, regs);
	if (rxlogen)
		dev_err(asr_hw->dev, "R: (%d %d) 0x%x -> 0x%x , 0x%x %d %d\n",
			start_port, end_port, rd_bitmap_before_get, rd_bitmap, rd_bitmap_ori, aggr_num, len);
	if (len > IPC_RXBUF_SIZE) {
		dev_err(asr_hw->dev, "[%s]!!!![%d %d] len2 =%d\n", __func__, start_port, end_port, len);
	}
	rx_aggr_stat(asr_hw, aggr_num);

	ret = port_dispatch_nomem_retry(asr_hw, len, start_port, end_port, io_addr_bmp, aggr_num, regs);

	if (ret == 0) {
		asr_rx_pkt_cnt += aggr_num;
	} else {
		dev_err(asr_hw->dev, "lalala rx DATA agg err %d(%d %d)(%x)(%x) (%d %d %d) ret:%d len:%d\n",
			ret, start_port, end_port, rd_bitmap, (unsigned int)
			asr_hw->ulFlag, asr_rxdataind_run_cnt,
			asr_rx_to_os_task_state, main_task_from_thread_cnt, ret, len);

		if (test_bit(ASR_DEV_RESTARTING, &asr_hw->phy_flags)) {

			dev_err(asr_hw->dev, "%s: break phy_flags=0X%lX\n", __func__, asr_hw->phy_flags);

		} else if (host_restart(asr_hw, aggr_num)) {
			// cpeng: TODO: a macro or function
			//sdio_claim_host(func);
			//sdio_writeb(func, 0x2, HOST_RESTART, &ret);	//upload restart
			//sdio_release_host(func);

			dev_err(asr_hw->dev, "thirdly do a read again \n");

			ret = port_dispatch_nomem_retry(asr_hw, len, start_port, end_port, io_addr_bmp, aggr_num, regs);
			if (ret) {
				dev_err(asr_hw->dev, "re-read failed rx DATA err %d - 0x%x - cur-idx:%d aggr_num:%d\n",
					ret, rd_bitmap, asr_hw->rx_data_cur_idx, aggr_num);
			}
			// cpeng: TODO: a macro or function
			//sdio_claim_host(func);
			//sdio_writeb(func, 0x0, HOST_RESTART, &ret);	//upload restart
			//sdio_release_host(func);
		} else {
			dev_err(asr_hw->dev, "HOST_RESTART(0x28): FAIL !!!!! \n");
		}
	}

	start_port = end_port + 1;
	if (start_port >= 16)
		start_port = 2;
	asr_hw->rx_data_cur_idx = start_port;

	return rd_bitmap;
}

static u16 rx_once(struct asr_hw *asr_hw, u8 rd_bitmap, u8 * regs)
{
	int ret;
	u8 start_port = asr_hw->rx_data_cur_idx;

	if ((rd_bitmap & (1 << start_port)) == 0) {
		dev_err(asr_hw->dev, "lalala non_agg cur_rx_idx %d no data\n", start_port);
		return 0;
	}

	ret = port_dispatch_nomem_retry(asr_hw, 0, start_port, start_port, 0, 0, regs);
	if (ret) {
		dev_err(asr_hw->dev, "lalala rx noagg DATA err %d\n", ret);
	}

	rd_bitmap &= ~(1 << start_port);
	start_port++;
	if (start_port >= 16)
		start_port = 2;
	asr_hw->rx_data_cur_idx = start_port;

	return rd_bitmap;
}

// return reschedule
static int host_int_rx(struct asr_hw *asr_hw, u8 * regs, u8 * sdio_ireg)
{
	u16 rd_bitmap;
	u16 rd_bitmap_new = 0;
#ifdef CONFIG_ASR_KEY_DBG
	int rx_skb_free_cnt;
#endif
	int ret;

	rd_bitmap = regs[RD_BITMAP_L];
	rd_bitmap |= regs[RD_BITMAP_U] << 8;

#ifdef CONFIG_ASR_KEY_DBG
	if (rxlogen)
		dev_info(asr_hw->dev, "rbm 0x%x\n", rd_bitmap);
#endif

	if (rd_bitmap == 0)
		return 0;

#ifdef CONFIG_ASR_KEY_DBG
	if (rxlogen)
		dev_info(asr_hw->dev, "ri 0x%x\n", asr_hw->rx_data_cur_idx);

	/*
	   need consider fw provide available port method(2->3->4...>15->2?)
	 */
    #ifndef SDIO_RXBUF_SPLIT
	rx_skb_free_cnt = skb_queue_len(&asr_hw->rx_sk_list);
	#else
	rx_skb_free_cnt = skb_queue_len(&asr_hw->rx_data_sk_list);
	#endif
	if (rx_skb_free_cnt < min_rx_skb_free_cnt)
		min_rx_skb_free_cnt = rx_skb_free_cnt;
#endif

//int_rx_retry:

    #ifndef SDIO_RXBUF_SPLIT
	if (skb_queue_empty(&asr_hw->rx_sk_list)) {
		if (asr_wait_rx_sk_buf(asr_hw) < 0) {
			return 1;
		}
	}
	#else
	if (skb_queue_empty(&asr_hw->rx_msg_sk_list)) {
		if (asr_wait_rx_msg_sk_buf(asr_hw) < 0) {
			return 1;
		}
	}
	#endif

	//msg
	if (rd_bitmap & (1 << SDIO_PORT_IDX(0))) {
		if ((ret = port_dispatch(asr_hw, 0, 0, 0, 0, 0, regs)))
			dev_err(asr_hw->dev, "lalala rx MSG err %d\n", ret);
		else {
			asr_rx_pkt_cnt++;
		}
		rd_bitmap &= 0xFFFE;
	}

    #ifdef SDIO_RXBUF_SPLIT
	if (skb_queue_empty(&asr_hw->rx_data_sk_list)) {
		if (asr_wait_rx_data_sk_buf(asr_hw) < 0) {
			return 1;
		}
	}
	#endif
	
	//log
	if (rd_bitmap & (1 << SDIO_PORT_IDX(1))) {
		if ((ret = port_dispatch(asr_hw, 0, 1, 1, 0, 0, regs)))
			dev_err(asr_hw->dev, "lalala rx LOG err %d\n", ret);
		else {
			asr_rx_pkt_cnt++;
		}
		rd_bitmap &= 0xFFFD;
	}

	if (rd_bitmap == 0) {
		//dev_err(asr_hw->dev, "[%s %d]get rd_bitmap failed\n", __func__,__LINE__);
		return 0;
	}
	//data port start
	mutex_lock(&asr_process_int_mutex);
	if (!(rd_bitmap & (1 << asr_hw->rx_data_cur_idx))) {
		#if 0
		ret = read_sdio_block_retry(asr_hw, regs, sdio_ireg);

		rd_bitmap_new = regs[RD_BITMAP_L] | regs[RD_BITMAP_U] << 8;

		if (!ret && (rd_bitmap_new & (1 << asr_hw->rx_data_cur_idx))) {

			rd_bitmap = rd_bitmap_new;

			mutex_unlock(&asr_process_int_mutex);
			goto int_rx_retry;
		}
		#endif

		mutex_unlock(&asr_process_int_mutex);

		//if (rd_bitmap_new) {
			dev_err(asr_hw->dev, "%s:idx %d no data(0x%x,0x%x),%d,%d\n ",
				__func__, asr_hw->rx_data_cur_idx, rd_bitmap, rd_bitmap_new, ret, asr_hw->restart_flag);
		//}

		return 0;
	}

    #ifndef SDIO_RXBUF_SPLIT
	if (skb_queue_empty(&asr_hw->rx_sk_list)) {
		if (asr_wait_rx_sk_buf(asr_hw) < 0) {
			mutex_unlock(&asr_process_int_mutex);
			return 1;
		}
	}
	#else
	if (skb_queue_empty(&asr_hw->rx_data_sk_list)) {
		if (asr_wait_rx_data_sk_buf(asr_hw) < 0) {
			mutex_unlock(&asr_process_int_mutex);
			return 1;
		}
	}
	#endif

	ipc_read_sdio_data_cnt++;

	//data,like (0000 0000,0111,1100)
	while (rd_bitmap) {

		if (test_bit(ASR_DEV_RESTARTING, &asr_hw->phy_flags)) {

			dev_err(asr_hw->dev, "%s: break phy_flags=0X%lX\n", __func__, asr_hw->phy_flags);
			break;
		}

		if (rx_aggr)
			rd_bitmap = rx_aggregate(asr_hw, rd_bitmap, regs);
		else
			rd_bitmap = rx_once(asr_hw, rd_bitmap, regs);
	}
	mutex_unlock(&asr_process_int_mutex);

#ifdef CONFIG_ASR_KEY_DBG
	if (rxlogen) {
		dev_info(asr_hw->dev, "uri 0x%x\n", asr_hw->rx_data_cur_idx);
	}
#endif
	return 0;
}

int asr_process_int_status(struct ipc_host_env_tag *env, int *type, bool restart_flag)
{
	int ret = 0;
	u8 sdio_ireg;
	u8 sdio_regs[68] = { 0 };
	struct asr_hw *asr_hw = (struct asr_hw *)env->pthis;

	#if 1
	if (asr_hw->restart_flag) {
		spin_lock_bh(&asr_hw->pmain_proc_lock);
		asr_hw->restart_flag = false;
		spin_unlock_bh(&asr_hw->pmain_proc_lock);
		ret = read_sdio_block_retry(asr_hw, sdio_regs, &sdio_ireg);
		if (ret) {
			dev_err(asr_hw->dev, "%s:read sdio_reg fail %d!!! \n", __func__, ret);
			return -1;
		}
	} else
	#endif
	{
		// get int reg and clean.
		spin_lock_bh(&asr_hw->int_reg_lock);
		sdio_ireg = asr_hw->sdio_ireg;
		asr_hw->sdio_ireg = 0;
		memcpy(sdio_regs, asr_hw->last_sdio_regs, SDIO_REG_READ_LENGTH);
		spin_unlock_bh(&asr_hw->int_reg_lock);
	}


	if (sdio_ireg & HOST_INT_UPLD_ST) {
		ret = host_int_rx(asr_hw, sdio_regs, &sdio_ireg);
	}

	if (sdio_ireg & HOST_INT_DNLD_ST) {
		ret = host_int_tx(asr_hw, sdio_regs);
	}

	if (ret) {
		dev_err(asr_hw->dev, "unlikely:lalala rx fail ENOMEM, %d\n", ret);
		//set_bit(ASR_FLAG_MAIN_TASK_BIT,&asr_hw->ulFlag);
		//wake_up_interruptible(&asr_hw->waitq_rx_thead);
	}

	return 0;
}
#endif

#ifdef CONFIG_ASR_USB
void asr_rx_to_os_task(struct asr_hw *asr_hw)
{
	struct sk_buff *skb;
	u16 id = 0;

	asr_rx_to_os_task_state = 1;
	asr_rx_to_os_task_run_cnt++;

	while (!skb_queue_empty(&asr_hw->rx_to_os_skb_list)
	       && (skb = skb_dequeue(&asr_hw->rx_to_os_skb_list))) {
		memcpy(&id, skb->data, sizeof(id));
		switch (id) {
		case RX_DATA_ID:
			ipc_host_rxdata_handler(asr_hw->ipc_env, skb);
			break;
		case RX_LOG_ID:
			ipc_host_dbg_handler(asr_hw->ipc_env, skb);
			break;
		default:
			ipc_host_msg_handler(asr_hw->ipc_env, skb);
			break;
		}
	}
	asr_rx_to_os_task_state = 0;
}

extern int rx_ooo_upload;
int ipc_host_process_rx_usb(u16 id, struct asr_hw *asr_hw, struct sk_buff *skb)
{
	//u8 type;
	ASR_DBG(ASR_FN_ENTRY_STR);

#if 0
	if (id == RX_DATA_ID)	//data
	{
		skb_queue_tail(&asr_hw->rx_to_os_skb_list, skb);
		set_bit(ASR_FLAG_RX_TO_OS_BIT, &asr_hw->ulFlag);
		wake_up_interruptible(&asr_hw->waitq_rx_to_os_thead);
		type = HIF_RX_DATA;
	} else if (id == RX_LOG_ID)	//log
	{
		ipc_host_dbg_handler(asr_hw->ipc_env, skb);
		type = HIF_RX_LOG;
	} else			//msg
	{
		ipc_host_msg_handler(asr_hw->ipc_env, skb);
		type = HIF_RX_MSG;
	}

	return type;
#else
	if (id == RX_DATA_ID || id == RX_LOG_ID) {
		// asr_usb_rx_complete is in irq, wakeup thread to handle msg/log/data.
		// Note: if msg response late, better to setup seperate thread to handle msg.

        #ifdef CFG_OOO_UPLOAD
        // ooo refine : check if pkt is forward.
		bool forward_skb = false;

        if (rx_ooo_upload) {
			if (id == RX_LOG_ID) {
				forward_skb = true;
			} else {
				struct host_rx_desc *host_rx_desc = (struct host_rx_desc *)skb->data;
				//dev_err(asr_hw->dev, "RX_DATA_ID : host_rx_desc =%p , rx_status=0x%x \n", host_rx_desc,host_rx_desc->rx_status);

				if (host_rx_desc && (host_rx_desc->rx_status & RX_STAT_FORWARD)){
					forward_skb = true;
				} else if (host_rx_desc && (host_rx_desc->rx_status & RX_STAT_ALLOC)){
					skb_queue_tail(&asr_hw->rx_pending_skb_list, skb);
					forward_skb = false;
					if (lalalaen)
						dev_err(asr_hw->dev, "RX_DATA_ID pending: host_rx_desc =%p , rx_status=0x%x (%d %d %d %d)\n", host_rx_desc,
													host_rx_desc->rx_status,
													host_rx_desc->sta_idx,
													host_rx_desc->tid,
													host_rx_desc->seq_num,
													host_rx_desc->fn_num);
				} else {
					dev_err(asr_hw->dev, "unlikely: host_rx_desc =%p , rx_status=0x%x\n", host_rx_desc,host_rx_desc->rx_status);
					dev_kfree_skb(skb);
				}
			}
		} else {
			forward_skb = true;
		}

		if (forward_skb)
		#endif  //CFG_OOO_UPLOAD
		{
		
		    skb_queue_tail(&asr_hw->rx_to_os_skb_list, skb);
		    set_bit(ASR_FLAG_RX_TO_OS_BIT, &asr_hw->ulFlag);
		    wake_up_interruptible(&asr_hw->waitq_rx_to_os_thead);
		}
	}
	#ifdef CFG_OOO_UPLOAD
    else if ((id == RX_DESC_ID) && rx_ooo_upload) {
	    // ooo refine: parse ooo rxdesc
	    //     RX_STAT_FORWARD : move pkt from rx_pending_skb_list to rx_to_os_skb_list and trigger
	    //     RX_STAT_DELETE  : move pkt from rx_pending_skb_list and free.
		struct ipc_ooo_rxdesc_buf_hdr *ooo_rxdesc = (struct ipc_ooo_rxdesc_buf_hdr *)skb->data;
		uint8_t rxu_stat_desc_cnt = ooo_rxdesc->rxu_stat_desc_cnt;
		struct rxu_stat_val *rxu_stat_handle;
		uint8_t * start_addr = (void *)skb->data + sizeof(struct ipc_ooo_rxdesc_buf_hdr);
        int forward_pending_skb_cnt = 0;
	    struct sk_buff *skb_pending;
        struct sk_buff *pnext;
	    struct host_rx_desc *host_rx_desc_pending = NULL;
        uint16_t ooo_match_cnt = 0;

        if (lalalaen)
	        dev_err(asr_hw->dev, "RX_DESC_ID : ooo_rxdesc =%p , rxu_stat_desc_cnt=%d ,start_addr=%p,hdr_len=%ld\n", ooo_rxdesc,rxu_stat_desc_cnt,start_addr,
		                                             sizeof(struct ipc_ooo_rxdesc_buf_hdr));

        if (ooo_rxdesc && rxu_stat_desc_cnt) {

            while (rxu_stat_desc_cnt > 0){
                   rxu_stat_handle = (struct rxu_stat_val *)start_addr;

				    if (lalalaen)
				        dev_err(asr_hw->dev, "RX_DESC_ID search: ooo_rxdesc =%p cnt=%d, rxu_stat_handle status=0x%x (%d %d %d %d)\n", ooo_rxdesc,rxu_stat_desc_cnt,
					                            rxu_stat_handle->status,
				                                rxu_stat_handle->sta_idx,
												rxu_stat_handle->tid,
												rxu_stat_handle->seq_num,
												rxu_stat_handle->amsdu_fn_num);

				   // search and get from asr_hw->rx_pending_skb_list.
				   skb_queue_walk_safe(&asr_hw->rx_pending_skb_list, skb_pending, pnext) {
                       host_rx_desc_pending = (struct host_rx_desc *)skb_pending->data;
                       ooo_match_cnt = 0;

                        if (lalalaen)
				            dev_err(asr_hw->dev, "RX_DESC_ID walk: pending host_rx_desc rx_status=0x%x (%d %d %d %d)\n",
				                                host_rx_desc_pending->rx_status,
												host_rx_desc_pending->sta_idx,
												host_rx_desc_pending->tid,
												host_rx_desc_pending->seq_num,
												host_rx_desc_pending->fn_num);

                       if ((host_rx_desc_pending->sta_idx == rxu_stat_handle->sta_idx) &&
						   (host_rx_desc_pending->tid     == rxu_stat_handle->tid) &&
						   (host_rx_desc_pending->seq_num == rxu_stat_handle->seq_num) &&
						   (host_rx_desc_pending->fn_num  <= rxu_stat_handle->amsdu_fn_num)) {

					       skb_unlink(skb_pending, &asr_hw->rx_pending_skb_list);
                           ooo_match_cnt++;

                           if (lalalaen)
				               dev_err(asr_hw->dev, "RX_DESC_ID hit: ooo_rxdesc =%p cnt=%d, rx_status=0x%x (%d %d %d %d)\n", ooo_rxdesc,rxu_stat_desc_cnt,
				                                host_rx_desc_pending->rx_status,
												host_rx_desc_pending->sta_idx,
												host_rx_desc_pending->tid,
												host_rx_desc_pending->seq_num,
												host_rx_desc_pending->fn_num);

						   if (rxu_stat_handle->status & RX_STAT_FORWARD){
						   	   host_rx_desc_pending->rx_status |= RX_STAT_FORWARD;
						       skb_queue_tail(&asr_hw->rx_to_os_skb_list, skb_pending);
							   forward_pending_skb_cnt++;
						   } else if (rxu_stat_handle->status & RX_STAT_DELETE) {
						       dev_kfree_skb(skb_pending);
						   } else {
							   dev_err(asr_hw->dev, "unlikely: status =0x%x\n", rxu_stat_handle->status);
						   }
                       }
                       if (ooo_match_cnt == (rxu_stat_handle->amsdu_fn_num + 1))
                           break;
				   }

                   //get next rxu_stat_val
			       start_addr += sizeof(struct rxu_stat_val);
				   rxu_stat_desc_cnt--;
			}

			// trigger
			if (forward_pending_skb_cnt) {
			    set_bit(ASR_FLAG_RX_TO_OS_BIT, &asr_hw->ulFlag);
			    wake_up_interruptible(&asr_hw->waitq_rx_to_os_thead);
			}
		}

	    dev_kfree_skb(skb);

	}
	#endif  //CFG_OOO_UPLOAD
	else { //msg
		skb_queue_tail(&asr_hw->msgind_task_skb_list, skb);
		set_bit(ASR_FLAG_MSGIND_TASK_BIT, &asr_hw->ulFlag);
		wake_up_interruptible(&asr_hw->waitq_msgind_task_thead);
	}
	return 0;
#endif

}
#endif

/**
 ******************************************************************************
 */
int ipc_host_msg_push(struct ipc_host_env_tag *env, void *msg_buf, u16 len)
{
	struct asr_hw *asr_hw = (struct asr_hw *)env->pthis;
	u8 *src = (u8 *) ((struct asr_cmd *)msg_buf)->a2e_msg;

	/* call sdio cmd to send cmd data, need retry n times
	 *
	 */

#ifdef CONFIG_ASR_SDIO
	u8 type = HIF_TX_MSG;
	return asr_sdio_send_data(asr_hw, type, src, len, (asr_hw->ioport | 0x0), 0x1);
#endif

#ifdef CONFIG_ASR_USB
	/* Use bus module to send cmd */
	return asr_usb_send_cmd(asr_hw, src, len, 1);	//asr_usb_tx(bus->dev, skb)

#endif

}
