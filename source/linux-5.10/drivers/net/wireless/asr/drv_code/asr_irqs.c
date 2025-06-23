/**
 ******************************************************************************
 *
 * @file asr_irqs.c
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */
#include <linux/interrupt.h>

#include "asr_defs.h"
#include "ipc_host.h"
#include "asr_hif.h"
#include <linux/ktime.h>
#include "asr_sdio.h"
#ifdef CONFIG_ASR_PM
#include "asr_pm.h"
#endif

#ifdef CONFIG_ASR_SDIO

extern int sss0;
extern int sss0_pre;
extern bool asr_xmit_opt;
extern bool sdio1thread;
extern bool sdio_restart;
long asr_sdio_times = 0;
long asr_oob_times = 0;
long ipc_host_irq_times = 0;
long asr_oob_irq_times =0;

//struct timespec irq_start, irq_stage1;
extern u32 bitcount(u32 num);

#ifdef OOB_INTR_ONLY
/**
 * asr_oob_irq_hdlr - out-of-band IRQ handler
 *
 * Handler registerd by the platform driver
 */
irqreturn_t asr_oob_irq_hdlr(int irq, void *dev_id)
{
	struct asr_hw *asr_hw = (struct asr_hw *)dev_id;

	asr_oob_irq_times++;

	//if (!asr_main_process_running(asr_hw))
	{
		set_bit(ASR_FLAG_OOB_INT_BIT, &asr_hw->ulFlag);
		wake_up_interruptible(&asr_hw->waitq_oob_intr_thead);
	}

	return IRQ_HANDLED;
}
#endif

/**
 * asr_main_task - Bottom half for IRQ handler
 *
 * Read irq status and process accordingly
 */

int asr_main_task_run_cnt;
int asr_main_task(struct asr_hw *asr_hw, u8 main_task_from_type)
{
	int type = HIF_INVALID_TYPE;
	unsigned long now = jiffies;
	struct asr_tx_agg *tx_agg_env = &(asr_hw->tx_agg_env);
	int ret = 0;
	//struct sdio_func *func;

	spin_lock_bh(&asr_hw->pmain_proc_lock);
	/* Check if already processing */
	if (asr_hw->mlan_processing) {
		asr_hw->more_task_flag = true;
		ret = -1;
		spin_unlock_bh(&asr_hw->pmain_proc_lock);
		goto exit_main_proc;
	} else {
		asr_hw->mlan_processing = true;
		//asr_hw->restart_flag = false;
		spin_unlock_bh(&asr_hw->pmain_proc_lock);
	}

process_start:

	do {
		/* Handle pending SDIO interrupts if any */
		asr_main_task_run_cnt++;
		if (asr_hw->sdio_ireg || (asr_hw->restart_flag && sdio_restart)) {
			asr_process_int_status(asr_hw->ipc_env);
		}
		// handle tx task
		if ( (asr_xmit_opt ? skb_queue_len(&asr_hw->tx_sk_list) : tx_agg_env->aggr_buf_cnt)
                    && bitcount(asr_hw->tx_use_bitmap))
                {
			if (asr_xmit_opt)  {
			    ret += asr_opt_tx_task(asr_hw);
			} else {
			    ret += asr_tx_task(asr_hw);
			}
		}
		//ipc_host_irq(asr_hw->ipc_env,&type);

		if (type == HIF_RX_DATA)
			asr_hw->stats.last_rx = now;

	} while (0);

	spin_lock_bh(&asr_hw->pmain_proc_lock);
	if (asr_hw->more_task_flag == true) {
		asr_hw->more_task_flag = false;
		asr_hw->restart_flag = true;
		spin_unlock_bh(&asr_hw->pmain_proc_lock);
		goto process_start;
	}
	asr_hw->mlan_processing = false;
	asr_hw->restart_flag = false;
	spin_unlock_bh(&asr_hw->pmain_proc_lock);

exit_main_proc:

	return ret;

}

u8 count_bits(u16 data, u8 start_bit)
{
	u8 num = 0;

	while (start_bit < 16) {
		if (data & (1 << start_bit)) {
			num++;
		}
		start_bit++;
	}

	return num;
}

void dump_sdio_info(struct asr_hw *asr_hw, const char *func, u32 line)
{
	u8 sdio_int_reg;
	u8 sdio_int_reg_l;
	u16 rd_bitmap, wr_bitmap;
	u16 rd_bitmap_l, wr_bitmap_l;
	struct asr_tx_agg *tx_agg_env = &(asr_hw->tx_agg_env);

	sdio_int_reg = asr_hw->sdio_reg[HOST_INT_STATUS];
	rd_bitmap = asr_hw->sdio_reg[RD_BITMAP_L] | asr_hw->sdio_reg[RD_BITMAP_U] << 8;
	wr_bitmap = asr_hw->sdio_reg[WR_BITMAP_L] | asr_hw->sdio_reg[WR_BITMAP_U] << 8;

	sdio_int_reg_l = asr_hw->last_sdio_regs[HOST_INT_STATUS];
	rd_bitmap_l = asr_hw->last_sdio_regs[RD_BITMAP_L] | asr_hw->sdio_reg[RD_BITMAP_U] << 8;
	wr_bitmap_l = asr_hw->last_sdio_regs[WR_BITMAP_L] | asr_hw->sdio_reg[WR_BITMAP_U] << 8;

	dev_err(asr_hw->dev,
		"%s,%d:trace sdio,%02X,(%04X,%02d,%02d),(%04X,%04X,%03d,%02d),(%02X,%04X,%04X)\n",
		func, line, sdio_int_reg, rd_bitmap, count_bits(rd_bitmap, 2),
		asr_hw->rx_data_cur_idx, wr_bitmap, asr_hw->tx_use_bitmap,
		tx_agg_env->aggr_buf_cnt, asr_hw->tx_data_cur_idx, sdio_int_reg_l, rd_bitmap_l, wr_bitmap_l);
}

unsigned int delta_time_max = 0;
extern int sdio_irq_sched_flag;
extern struct timespec irq_sched;
extern long tx_evt_irq_times;
int isr_write_clear_cnt;
int ipc_read_rx_bm_fail_cnt;
int ipc_read_tx_bm_fail_case2_cnt,ipc_read_tx_bm_fail_case0_cnt;
int ipc_read_tx_bm_fail_case3_cnt;
int ipc_isr_skip_main_task_cnt;

int sdio_isr_uplod_cnt;
extern u32 asr_data_tx_pkt_cnt;
extern u32 asr_rx_pkt_cnt;
extern int tx_debug;
extern bool txlogen;
extern bool rxlogen;
extern int tx_status_debug;
DEFINE_MUTEX(asr_process_int_mutex);

static int read_sdio_block_int(struct asr_hw *asr_hw, u8 *sdio_int_reg)
{
	u16 rd_bitmap, wr_bitmap, diff_bitmap = 0;
	u8 sdio_regs[SDIO_REG_READ_LENGTH] = {0};
	int ret = 0;

	ret = read_sdio_block(asr_hw, sdio_regs, 0);
	if (ret) {
		dev_err(asr_hw->dev, "[%s] reg rd fail (%d)!!! \n", __func__, ret);

		cmd_queue_crash_handle(asr_hw, __func__, __LINE__, ASR_RESTART_REASON_SDIO_ERR);

		return -1;
	}

	spin_lock_bh(&asr_hw->int_reg_lock);
	memcpy(asr_hw->sdio_reg, sdio_regs, SDIO_REG_READ_LENGTH);

	*sdio_int_reg = asr_hw->sdio_reg[HOST_INT_STATUS];
	rd_bitmap = asr_hw->sdio_reg[RD_BITMAP_L] | asr_hw->sdio_reg[RD_BITMAP_U] << 8;
	wr_bitmap = asr_hw->sdio_reg[WR_BITMAP_L] | asr_hw->sdio_reg[WR_BITMAP_U] << 8;
	diff_bitmap = ((asr_hw->tx_use_bitmap) ^ wr_bitmap);

	//dump_sdio_info(asr_hw, __func__, __LINE__);

	if (!(*sdio_int_reg & HOST_INT_UPLD_ST) && ((rd_bitmap & 0x1)
						   || count_bits(rd_bitmap, 2) >= SDIO_RX_AGG_TRI_CNT)) {

		//dev_err(asr_hw->dev,"%s: recover rx,0x%x\n", __func__, rd_bitmap);

		//dump_sdio_info(asr_hw, __func__, __LINE__);

		*sdio_int_reg |= HOST_INT_UPLD_ST;
		asr_hw->sdio_reg[HOST_INT_STATUS] |= HOST_INT_UPLD_ST;
	}
	if (!(*sdio_int_reg & HOST_INT_DNLD_ST) && ((diff_bitmap & 0x1)
						   || count_bits(diff_bitmap, 1) >= SDIO_TX_AGG_TRI_CNT)) {

		//dev_err(asr_hw->dev,"%s: recover tx,0x%X\n", __func__, diff_bitmap);

		*sdio_int_reg |= HOST_INT_DNLD_ST;
		asr_hw->sdio_reg[HOST_INT_STATUS] |= HOST_INT_DNLD_ST;
	}

	if (*sdio_int_reg) {
		/*
		 * HOST_INT_DNLD_ST and/or HOST_INT_UPLD_ST
		 * Clear the interrupt status register
		 */
		if (txlogen || rxlogen)
			dev_err(asr_hw->dev, "[%s]int:(0x%x 0x%x 0x%x)\n", __func__, *sdio_int_reg, wr_bitmap,
				rd_bitmap);

		asr_hw->sdio_ireg |= *sdio_int_reg;
		memcpy(asr_hw->last_sdio_regs, asr_hw->sdio_reg, SDIO_REG_READ_LENGTH);
	}
	spin_unlock_bh(&asr_hw->int_reg_lock);

	return 0;
}

void asr_sdio_corner_case(struct asr_hw *asr_hw,u8 sdio_int_reg,u32 tx_len,bool is_tx_send,bool is_rx_receive,
			u16 rd_bitmap,u16 wr_bitmap,bool skip_main_task_flag,int tx_pkt_cnt_in_isr)
{
	bool write_clear_case1, write_clear_case2, write_clear_case3, write_clear_case4,write_clear_case0;
	bool is_tx=false,is_rx=false;
	int ret = -1;
	struct sdio_func *func = asr_hw->plat->func;


	is_tx = (sdio_int_reg & HOST_INT_DNLD_ST)?true:false;
	is_rx = (sdio_int_reg & HOST_INT_UPLD_ST)?true:false;


	// case 0:current 1,tx ; 2,only sdio isr send;3,bitmap data is 0
	//write_clear_case0 = is_tx && !skip_main_task_flag && !(wr_bitmap >> 1)&& tx_len && !is_tx_send;
	//write_clear_case0 = is_tx && !skip_main_task_flag && (!(asr_hw->tx_use_bitmap>> 1)||!(wr_bitmap >> 1))&& tx_len && !is_tx_send;
	write_clear_case0 = is_tx && (!(asr_hw->tx_use_bitmap>> 1)||!(wr_bitmap >> 1))&& tx_len && !is_tx_send;


	// special case for write clear int status
	// corner case 1: last int read all bitmap.
	write_clear_case1 = is_rx && (!(rd_bitmap >> 2)) && !is_rx_receive;

	// corner case 2: may wr_bitmap = 0x1, but no data tx cmd53 will also cause clear fail.
	write_clear_case2 = is_tx && !skip_main_task_flag && !(wr_bitmap >> 1) && tx_len;

	// corner case 3: this case cmd53 write pkt cnt==0(maybe aggr_buf_cnt==0), use write clear
	write_clear_case3 = is_tx && !skip_main_task_flag && (wr_bitmap >> 1)
		&& !tx_pkt_cnt_in_isr && tx_len;

	// corner case 4: this case skip cmd53 write/read, use write clear // may removed
	write_clear_case4 = (is_tx|| is_rx) && skip_main_task_flag;

	// interrupt and bitmap sync fail.
	if (write_clear_case1)
		ipc_read_rx_bm_fail_cnt++;

	if (write_clear_case0)
		ipc_read_tx_bm_fail_case0_cnt++;


	if (write_clear_case2)
		ipc_read_tx_bm_fail_case2_cnt++;

	if (write_clear_case3)
		ipc_read_tx_bm_fail_case3_cnt++;

	if (write_clear_case4)
		ipc_isr_skip_main_task_cnt++;

	// if no cmd53 write/read , clear int status will fail,use write-clear .
	if (write_clear_case1|| write_clear_case0||
		(write_clear_case2 && (tx_debug <= 0)) ||
		(write_clear_case3 && (tx_debug <= 0)) ||
		(write_clear_case4 && (tx_debug <= 0)) ||
		(tx_debug == 256)
		) {
		if(write_clear_case1){
			//gpio_direction_output(6, 1);
			//gpio_direction_output(6, 0);
		}
		//gpio_direction_output(6, 1);
		sdio_claim_host(func);
		sdio_writeb(func, 0x0, HOST_INT_STATUS, &ret);
		sdio_release_host(func);
		//gpio_direction_output(6, 0);

		if (ret) {
			dev_err(asr_hw->dev, "%s write clear HOST_INT_STATUS fail!!! (%d)\n", __func__, ret);
			return;
		}

		isr_write_clear_cnt++;

	} else {

		if (tx_status_debug == 13) {
			if (!is_tx_send && is_tx)
				dev_err(asr_hw->dev, "%s tx :(0x%x 0x%x %d %d 0x%x %d)!!! \n",
					__func__, sdio_int_reg, wr_bitmap,
					skip_main_task_flag,
					tx_len,
					asr_hw->tx_use_bitmap, asr_hw->tx_data_cur_idx);

			if (!is_rx_receive && is_rx)
				dev_err(asr_hw->dev, "%s rx :(0x%x 0x%x %d)!!! \n",
					__func__, sdio_int_reg, rd_bitmap, skip_main_task_flag);
		}
	}

}

void asr_sdio_dataworker(struct work_struct *work)
{
	struct asr_hw *asr_hw = container_of(work, struct asr_hw, datawork);
	struct sdio_func *func;
	u8 sdio_int_reg;
	
	u16 rd_bitmap, wr_bitmap;
	u32 pre_tx_pkt_cnt, after_tx_pkt_cnt;
	u32 pre_rx_pkt_cnt, after_rx_pkt_cnt;
	bool skip_main_task_flag = false;
	struct asr_tx_agg *tx_agg_env;

	int main_task_ret = 0;
	int tx_pkt_cnt_in_isr = 0;
	bool is_tx_send=false,is_rx_receive=false;
	u32 tx_len = 0;

	/* interrupt to read int status */
	if (!asr_hw || !asr_hw->plat) {
		return;
	}

	func = asr_hw->plat->func;
	if (!func)
		return;
	//asr_hw = sdio_get_drvdata(func);
	tx_agg_env = &(asr_hw->tx_agg_env);


	if (read_sdio_block_int(asr_hw, &sdio_int_reg)) {
		return;
	}

	rd_bitmap = asr_hw->sdio_reg[RD_BITMAP_L] | asr_hw->sdio_reg[RD_BITMAP_U] << 8;
	wr_bitmap = asr_hw->sdio_reg[WR_BITMAP_L] | asr_hw->sdio_reg[WR_BITMAP_U] << 8;

	// main process to update tx/rx bitmap and handle tx/rx task.
	tx_evt_irq_times++;

	pre_tx_pkt_cnt = asr_data_tx_pkt_cnt;
	pre_rx_pkt_cnt = asr_rx_pkt_cnt;

	// asr main task in isr may skip .
	main_task_ret = asr_main_task(asr_hw, SDIO_ISR);
	if (main_task_ret < 0)
		skip_main_task_flag = true;
	else
		tx_pkt_cnt_in_isr = main_task_ret;

	after_tx_pkt_cnt = asr_data_tx_pkt_cnt;
	after_rx_pkt_cnt = asr_rx_pkt_cnt;



	is_tx_send = (pre_tx_pkt_cnt == after_tx_pkt_cnt)?false:true;
	is_rx_receive = (pre_rx_pkt_cnt == after_rx_pkt_cnt)?false:true;

	tx_len = asr_xmit_opt ? skb_queue_len(&asr_hw->tx_sk_list) : tx_agg_env->aggr_buf_cnt;

	asr_sdio_corner_case(asr_hw,sdio_int_reg,tx_len,is_tx_send,is_rx_receive,
						rd_bitmap,wr_bitmap,skip_main_task_flag,tx_pkt_cnt_in_isr);


}

void asr_sdio_isr(struct sdio_func *sdio_func)
{
	struct asr_hw *asr_hw;

	asr_hw = sdio_get_drvdata(sdio_func);

	asr_sdio_times++;

#ifdef ASR_SDIO_ISR_DATAWORKER
	queue_work(asr_hw->asr_wq, &asr_hw->datawork);
#else
	if (!sdio1thread) {
		int state;

#ifdef CONFIG_ASR_PM
		state = asr_pm_acquire(asr_hw->plat, PM_NONBLOCK | PM_FROMISR);
#else
		state = asr_sdio_get_state(asr_hw->plat);
#endif

		if (state == SDIO_STATE_ACTIVE)
			asr_sdio_dataworker(&asr_hw->datawork);
#ifdef CONFIG_ASR_PM
		else if (state == SDIO_STATE_RESUMING)
			set_bit(ASR_FLAG_SDIO_INT_BIT, &asr_hw->ulFlag);
		else if (state != SDIO_STATE_SUSPENDING && state != SDIO_STATE_SUSPENDED)
#else
		else
#endif
			asr_err("sdio state error: %d", state);

#ifdef CONFIG_ASR_PM
		asr_pm_release(asr_hw->plat);
#endif
	} else {
		set_bit(ASR_FLAG_SDIO_INT_BIT, &asr_hw->ulFlag);
		wake_up_interruptible(&asr_hw->waitq_main_task_thead);
	}
#endif
}

#ifdef OOB_INTR_ONLY
void asr_oob_isr(struct asr_hw *asr_hw)
{
    asr_oob_times++;

#ifdef ASR_SDIO_ISR_DATAWORKER
	queue_work(asr_hw->asr_wq, &asr_hw->datawork);
#else
	if (!sdio1thread) {
		int state;

#ifdef CONFIG_ASR_PM
		state = asr_pm_acquire(asr_hw->plat, PM_NONBLOCK | PM_FROMISR);
#else
		state = asr_sdio_get_state(asr_hw->plat);
#endif

		if (state == SDIO_STATE_ACTIVE)
			asr_sdio_dataworker(&asr_hw->datawork);
#ifdef CONFIG_ASR_PM
		else if (state == SDIO_STATE_RESUMING)
			set_bit(ASR_FLAG_SDIO_INT_BIT, &asr_hw->ulFlag);
		else if (state != SDIO_STATE_SUSPENDING && state != SDIO_STATE_SUSPENDED)
#else
		else
#endif
			asr_err("sdio state error: %d", state);

#ifdef CONFIG_ASR_PM
		asr_pm_release(asr_hw->plat);
#endif
	} else {
		set_bit(ASR_FLAG_SDIO_INT_BIT, &asr_hw->ulFlag);
		wake_up_interruptible(&asr_hw->waitq_main_task_thead);
	}
#endif
}
#endif  // OOB_INTR_ONLY

#endif
