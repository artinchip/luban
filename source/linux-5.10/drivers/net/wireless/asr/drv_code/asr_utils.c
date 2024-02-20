/**
 ****************************************************************************************
 *
 * @file asr_utils.c
 *
 * @brief sdio rx buffer
 *
 * Copyright (C) 2021 ASR Micro Limited.
 * All Rights Reserved.
 *
 ****************************************************************************************
 */
#include <linux/device.h>
#include "asr_utils.h"
#include "asr_rx.h"
#include "asr_tx.h"
#include "ipc_host.h"
#include "asr_msg_rx.h"
#include "asr_defs.h"
#include "asr_main.h"
#include "asr_ate.h"

//#ifdef LONG_SIZE_DBG
u8 asr_snprintf_buf[ASR_SNPRINTF_BUFF + 32] = { 0 };

u32 asr_snprintf_idx = 0;
u32 asr_sprintf_item_array[ASR_SPRINTF_ITEM_ARR_SIZE + 1] = { 0 };

u32 asr_sprintf_item_idx = 0;
//#endif

//#ifdef RING_DBG
ASR_DBG_RING_T asr_dbg_ring;
//#endif

extern int asr_msg_level;

/*
*/
int asr_rxbuff_alloc(struct asr_hw *asr_hw, u32 len, struct sk_buff **skb)
{
	struct sk_buff *skb_new;

	skb_new = dev_alloc_skb(len);

	if (unlikely(!skb_new)) {
		dev_err(asr_hw->dev, "%s:%d: skb alloc of size %u failed\n\n", __func__, __LINE__, len);
		return -ENOMEM;
	}

	*skb = skb_new;

	return 0;
}

/**
 * Deallocs all sk_buff structures in the rxbuff_table
 */
static void asr_rxbuffs_dealloc(struct asr_hw *asr_hw)
{
	// Get first element
	struct sk_buff *skb;

#ifdef CONFIG_ASR_SDIO
    #ifndef SDIO_RXBUF_SPLIT
	while (!skb_queue_empty(&asr_hw->rx_sk_list)) {
		skb = __skb_dequeue(&asr_hw->rx_sk_list);
		if (skb) {
			dev_kfree_skb(skb);
			skb = NULL;
		}
	}
	#else
	while (!skb_queue_empty(&asr_hw->rx_data_sk_list)) {
		skb = __skb_dequeue(&asr_hw->rx_data_sk_list);
		if (skb) {
			dev_kfree_skb(skb);
			skb = NULL;
		}
	}

	while (!skb_queue_empty(&asr_hw->rx_msg_sk_list)) {
		skb = __skb_dequeue(&asr_hw->rx_msg_sk_list);
		if (skb) {
			dev_kfree_skb(skb);
			skb = NULL;
		}
	}
	#endif

    #ifdef SDIO_DEAGGR
	while (!skb_queue_empty(&asr_hw->rx_sk_sdio_deaggr_list)) {
		skb = skb_dequeue(&asr_hw->rx_sk_sdio_deaggr_list);
		if (skb) {
			dev_kfree_skb(skb);
			skb = NULL;
		}
	}
	#endif
#endif

	while (!skb_queue_empty(&asr_hw->rx_sk_split_list)) {
		skb = skb_dequeue(&asr_hw->rx_sk_split_list);
		if (skb) {
			dev_kfree_skb(skb);
			skb = NULL;
		}
	}
}

/**
 * @brief Deallocate storage elements.
 *
 *  This function deallocates all the elements required for communications with LMAC,
 *  such as Rx Data elements, MSGs elements, ...
 *
 * This function should be called in correspondence with the allocation function.
 *
 * @param[in]   asr_hw   Pointer to main structure storing all the relevant information
 */
static void asr_elems_deallocs(struct asr_hw *asr_hw)
{
	ASR_DBG(ASR_FN_ENTRY_STR);

	asr_rxbuffs_dealloc(asr_hw);
}

/**
 * @brief Allocate storage elements.
 *
 *  This function allocates all the elements required for communications with LMAC,
 *  such as Rx Data elements, MSGs elements, ...
 *
 * This function should be called in correspondence with the deallocation function.
 *
 * @param[in]   asr_hw   Pointer to main structure storing all the relevant information
 */
static int asr_elems_allocs(struct asr_hw *asr_hw)
{
	struct sk_buff *skb;
	int i;

	ASR_DBG(ASR_FN_ENTRY_STR);

#ifdef CONFIG_ASR_SDIO

    #ifndef SDIO_RXBUF_SPLIT
	for (i = 0; i < asr_hw->ipc_env->rx_bufnb; i++) {

		// Allocate a new sk buff
		if (asr_rxbuff_alloc(asr_hw, asr_hw->ipc_env->rx_bufsz, &skb)) {
			dev_err(asr_hw->dev, "%s:%d: MEM ALLOC FAILED\n", __func__, __LINE__);
			goto err_alloc;
		}

		memset(skb->data, 0, asr_hw->ipc_env->rx_bufsz);
		// Add the sk buffer structure in the table of rx buffer
		skb_queue_tail(&asr_hw->rx_sk_list, skb);
	}
	#else	
	// data rxbuf alloc
	for (i = 0; i < asr_hw->ipc_env->rx_bufnb; i++) {

		// Allocate a new sk buff
		if (asr_rxbuff_alloc(asr_hw, asr_hw->ipc_env->rx_bufsz, &skb)) {
			dev_err(asr_hw->dev, "%s:%d: DATA MEM ALLOC FAILED\n", __func__, __LINE__);
			goto err_alloc;
		}

		memset(skb->data, 0, asr_hw->ipc_env->rx_bufsz);
		// Add the sk buffer structure in the table of rx buffer
		skb_queue_tail(&asr_hw->rx_data_sk_list, skb);
	}

    // msg rxbuf alloc, only one-port size.
	for (i = 0; i < IPC_MSG_RXBUF_CNT; i++) {

		// Allocate a new sk buff
		if (asr_rxbuff_alloc(asr_hw, IPC_MSG_RXBUF_SIZE, &skb)) {
			dev_err(asr_hw->dev, "%s:%d: MSG MEM ALLOC FAILED\n", __func__, __LINE__);
			goto err_alloc;
		}

		memset(skb->data, 0, IPC_MSG_RXBUF_SIZE);
		// Add the sk buffer structure in the table of rx buffer
		skb_queue_tail(&asr_hw->rx_msg_sk_list, skb);
	}

	#endif // SDIO_RXBUF_SPLIT

    #ifdef SDIO_DEAGGR
	for (i = 0; i < asr_hw->ipc_env->rx_bufnb_sdio_deagg; i++) {

		// Allocate a new sk buff
		if (asr_rxbuff_alloc(asr_hw, asr_hw->ipc_env->rx_bufsz_sdio_deagg, &skb)) {
			dev_err(asr_hw->dev, "%s:%d: MEM ALLOC FAILED\n", __func__, __LINE__);
			goto err_alloc;
		}

		memset(skb->data, 0, asr_hw->ipc_env->rx_bufsz_sdio_deagg);
		// Add the sk buffer structure in the table of rx buffer
		skb_queue_tail(&asr_hw->rx_sk_sdio_deaggr_list, skb);
	}
	#endif
	
#endif

    // rx_sk_split_list used for amsdu rx.
	for (i = 0; i < asr_hw->ipc_env->rx_bufnb_split; i++) {

		// Allocate a new sk buff
		if (asr_rxbuff_alloc(asr_hw, asr_hw->ipc_env->rx_bufsz_split, &skb)) {
			dev_err(asr_hw->dev, "%s:%d: MEM ALLOC FAILED\n", __func__, __LINE__);
			goto err_alloc;
		}

		memset(skb->data, 0, asr_hw->ipc_env->rx_bufsz_split);
		// Add the sk buffer structure in the table of rx buffer
		skb_queue_tail(&asr_hw->rx_sk_split_list, skb);
	}

	return 0;

err_alloc:
	asr_elems_deallocs(asr_hw);
	return -ENOMEM;
}

#ifdef CONFIG_ASR_SDIO
// tx hif buf
int asr_txhifbuffs_alloc(struct asr_hw *asr_hw, u32 len, struct sk_buff **skb)
{
	struct sk_buff *skb_new;

	skb_new = dev_alloc_skb(len);

	if (unlikely(!skb_new)) {
		dev_err(asr_hw->dev, "%s:%d: skb alloc of size %u failed\n\n", __func__, __LINE__, len);
		return -ENOMEM;
	}

	*skb = skb_new;

	return 0;
}

void asr_txhifbuffs_dealloc(struct asr_hw *asr_hw)
{
	// Get first element
	struct sk_buff *skb;

	while (!skb_queue_empty(&asr_hw->tx_sk_list)) {
		skb = __skb_dequeue(&asr_hw->tx_sk_list);
		if (skb) {
			dev_kfree_skb(skb);
			skb = NULL;
		}
	}

	while (!skb_queue_empty(&asr_hw->tx_hif_skb_list)) {
		skb = __skb_dequeue(&asr_hw->tx_hif_skb_list);
		if (skb) {
			dev_kfree_skb(skb);
			skb = NULL;
		}
	}

	while (!skb_queue_empty(&asr_hw->tx_hif_free_buf_list)) {
		skb = __skb_dequeue(&asr_hw->tx_hif_free_buf_list);
		if (skb) {
			dev_kfree_skb(skb);
			skb = NULL;
		}
	}
}
#endif

// return true if equal
bool is_equal_mac_addr(const u8 *addr1,const u8 *addr2)
{
	const u16 *a = (const u16*)addr1;
	const u16 *b = (const u16*)addr2;
	return ((a[0]^b[0])|(a[1]^b[1])|(a[2]^b[2])) == 0;
}

bool check_is_dhcp_package(struct asr_vif *asr_vif, bool is_tx, u16 eth_proto, u8 * type, u8 ** d_mac, u8 * data,
			   u32 data_len)
{
	uint16_t sport = 0, dport = 0;
	int index = 0;
	u8 direct_ps = 0;

	if (!data || data_len <= 270) {
		return false;
	}

	if (eth_proto == 0x800 && data[0] == 0x45 && data[9] == 0x11) {
		sport = ntohs(*(u16 *) (data + 20));
		dport = ntohs(*(u16 *) (data + 22));

		if (sport == 67 && dport == 68) {
			direct_ps = 2;
		} else if (sport == 68 && dport == 67) {
			direct_ps = 1;
		}

		if (direct_ps != 0) {
			index = 44;
			if (data[44] == 0 && data_len > 285 && data[270] == 3) {
				if(data[40] == 0){
					if(data[280]==50)
						index = 282;
					else if(data[277]==50)
						index = 279;
					else if(data[271]==50)
						index = 273;
				}
				else
					index = 40;
			}

			if (type) {
				*type = data[270];
			}

			if (d_mac) {
				*d_mac = data + 56;
			}

			if((ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_STATION)&& !is_equal_mac_addr(asr_vif->asr_hw->mac_addr,*d_mac))
				return false;

			dev_info(asr_vif->asr_hw->dev, "DHCP:%d,%d,%d,%d,%d,%d:%d:%d:%d,%02X:%02X:%02X:%02X:%02X:%02X\n",
				 is_tx, data[28], data[270], sport, dport, data[index], data[index + 1],
				 data[index + 2], data[index + 3], data[56], data[57], data[58], data[59], data[60],
				 data[61]);

			return true;
		}
	}

	return false;
}

/**
 * WLAN driver call-back function for message reception indication
 */
extern struct asr_traffic_status g_asr_traffic_sts;
u8 asr_msgind(void *pthis, void *hostid)
{
	struct asr_hw *asr_hw = (struct asr_hw *)pthis;
	struct sk_buff *skb = (struct sk_buff *)hostid;
	u8 ret = 0;
	struct ipc_e2a_msg *msg;
        bool is_ps_change_ind_msg = false;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Retrieve the message structure */
	msg = (struct ipc_e2a_msg *)skb->data;

	/* Relay further actions to the msg parser */
	if (driver_mode == DRIVER_MODE_NORMAL) {
		asr_rx_handle_msg(asr_hw, msg);
		is_ps_change_ind_msg = ((MSG_T(msg->id) == TASK_MM) && (MSG_I(msg->id) == MM_PS_CHANGE_IND));
	} else if (driver_mode == DRIVER_MODE_ATE) {
		asr_rx_ate_handle(asr_hw, msg);
	}
#ifdef CONFIG_ASR_SDIO
    #ifndef SDIO_RXBUF_SPLIT
	memset(skb->data, 0, asr_hw->ipc_env->rx_bufsz);
	// Add the sk buffer structure in the table of rx buffer
	skb_queue_tail(&asr_hw->rx_sk_list, skb);
	#else
	memset(skb->data, 0, IPC_MSG_RXBUF_SIZE);
	// Add the sk buffer structure in the table of rx buffer
	skb_queue_tail(&asr_hw->rx_msg_sk_list, skb);
	#endif
#else
	dev_kfree_skb(skb);
#endif

    if (is_ps_change_ind_msg == true) {
           // move traffic sts msg send here.
		   if (g_asr_traffic_sts.send) {
                       struct asr_sta *asr_sta_tmp = g_asr_traffic_sts.asr_sta_ps;

                        // send msg may schedule.
				if (g_asr_traffic_sts.ps_id_bits & LEGACY_PS_ID)
					asr_set_traffic_status(asr_hw, asr_sta_tmp, g_asr_traffic_sts.tx_ava, LEGACY_PS_ID);

				if (g_asr_traffic_sts.ps_id_bits & UAPSD_ID)
					asr_set_traffic_status(asr_hw, asr_sta_tmp, g_asr_traffic_sts.tx_ava, UAPSD_ID);

				dev_err(asr_hw->dev," [ps]tx_ava=%d:sta-%d, uapsd=0x%x, (%d , %d) \r\n",
				                                        g_asr_traffic_sts.tx_ava,
				                                        asr_sta_tmp->sta_idx,asr_sta_tmp->uapsd_tids,
														asr_sta_tmp->ps.pkt_ready[LEGACY_PS_ID],
														asr_sta_tmp->ps.pkt_ready[UAPSD_ID]);
		   }
	}

	return ret;
}

/**
 * WLAN driver call-back function for Debug message reception indication
 */
u8 asr_dbgind(void *pthis, void *hostid)
{
#ifdef CONFIG_ASR_SDIO
	struct asr_hw *asr_hw = (struct asr_hw *)pthis;
#endif
	struct sk_buff *skb = (struct sk_buff *)hostid;
	struct ipc_dbg_msg *dbg_msg;
	u8 ret = 0;

	/* Retrieve the message structure */
	dbg_msg = (struct ipc_dbg_msg *)skb->data;

	/* Display the LMAC string */
	printk("lmac--%s\n", (char *)dbg_msg->string);

#ifdef CONFIG_ASR_SDIO
	memset(skb->data, 0, asr_hw->ipc_env->rx_bufsz);
	// Add the sk buffer structure in the table of rx buffer
	#ifndef SDIO_RXBUF_SPLIT
	skb_queue_tail(&asr_hw->rx_sk_list, skb);
	#else
	skb_queue_tail(&asr_hw->rx_data_sk_list, skb);
	#endif
#else
	dev_kfree_skb(skb);
#endif

	return ret;
}

/**
 *
 */
int asr_ipc_init(struct asr_hw *asr_hw)
{
	struct ipc_host_cb_tag cb;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* initialize the API interface */
	cb.recv_data_ind = asr_rxdataind;
	cb.recv_msg_ind = asr_msgind;
	cb.recv_dbg_ind = asr_dbgind;

	/* set the IPC environment */
	asr_hw->ipc_env = (struct ipc_host_env_tag *)
	    kzalloc(sizeof(struct ipc_host_env_tag), GFP_KERNEL);

	/* call the initialization of the IPC */
	ipc_host_init(asr_hw->ipc_env, &cb, asr_hw);

	asr_cmd_mgr_init(&asr_hw->cmd_mgr);

	return asr_elems_allocs(asr_hw);
}

/**
 *
 */
void asr_ipc_deinit(struct asr_hw *asr_hw)
{
	ASR_DBG(ASR_FN_ENTRY_STR);

	asr_cmd_mgr_deinit(&asr_hw->cmd_mgr);
	asr_elems_deallocs(asr_hw);
	kfree(asr_hw->ipc_env);
	asr_hw->ipc_env = NULL;
}

/**
 *
 */
void asr_error_ind(struct asr_hw *asr_hw)
{
	dev_err(asr_hw->dev, "%s\n", __func__);
}

void __asr_dbg(u32 level, const char *func, const char *fmt, ...)
{
	struct va_format vaf = {
		.fmt = fmt,
	};
	va_list args;

	va_start(args, fmt);
	vaf.va = &args;
	if (asr_msg_level & level)
		pr_info("%s %pV", func, &vaf);
	va_end(args);
}

#ifdef ASR_STATS_RATES_TIMER
void asr_stats_update_txrx_rates(struct asr_hw *asr_hw)
{
	unsigned long avg_tx_rates = 0;
	unsigned long avg_rx_rates = 0;
	int index = 0;

	for (index = TXRX_RATES_NUM - 1; index > 0; index--) {
		asr_hw->stats.txrx_rates[index] = asr_hw->stats.txrx_rates[index - 1];
	}


	if (jiffies - asr_hw->stats.txrx_rates[0].tx_times > 0) {
		avg_tx_rates = (asr_hw->stats.tx_bytes - asr_hw->stats.txrx_rates[0].tx_bytes) * 8 * 1000 /
			jiffies_to_msecs(jiffies - asr_hw->stats.txrx_rates[0].tx_times);
		asr_hw->stats.txrx_rates[0].tx_bytes = asr_hw->stats.tx_bytes;
		asr_hw->stats.txrx_rates[0].tx_times = asr_hw->stats.last_tx;
	} else {
		avg_tx_rates = 0;
	}

	if (jiffies - asr_hw->stats.txrx_rates[0].rx_times > 0) {
		avg_rx_rates = (asr_hw->stats.rx_bytes - asr_hw->stats.txrx_rates[0].rx_bytes) * 8 * 1000 /
			jiffies_to_msecs(jiffies - asr_hw->stats.txrx_rates[0].rx_times);
		asr_hw->stats.txrx_rates[0].rx_bytes = asr_hw->stats.rx_bytes;
		asr_hw->stats.txrx_rates[0].rx_times = asr_hw->stats.last_rx_times;;
	} else {
		avg_rx_rates = 0;
	}


	if (avg_tx_rates == 0 && jiffies_to_msecs(jiffies - asr_hw->stats.txrx_rates[0].tx_times) > 2000) {
		asr_hw->stats.txrx_rates[0].tx_rates = 0;
	} else {
		for (index = TXRX_RATES_NUM - 1; index > 0; index--) {
			avg_tx_rates += asr_hw->stats.txrx_rates[index].tx_rates;
		}

		asr_hw->stats.txrx_rates[0].tx_rates = avg_tx_rates / TXRX_RATES_NUM;
	}

	if (avg_rx_rates == 0 && jiffies_to_msecs(jiffies - asr_hw->stats.txrx_rates[0].rx_times) > 2000) {
		asr_hw->stats.txrx_rates[0].rx_rates = 0;
	} else {
		for (index = TXRX_RATES_NUM - 1; index > 0; index--) {
			avg_rx_rates += asr_hw->stats.txrx_rates[index].rx_rates;
		}

		asr_hw->stats.txrx_rates[0].rx_rates = avg_rx_rates / TXRX_RATES_NUM;
	}

}
#endif
