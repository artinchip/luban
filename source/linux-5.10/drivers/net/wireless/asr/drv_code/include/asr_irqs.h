/**
 ******************************************************************************
 *
 * @file asr_irqs.h
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */
#ifndef _ASR_IRQS_H_
#define _ASR_IRQS_H_

#include <linux/interrupt.h>

/* IRQ handler to be registered by platform driver */

void asr_sdio_isr(struct sdio_func *sdio_func);
#ifdef OOB_INTR_ONLY
irqreturn_t asr_oob_irq_hdlr(int irq, void *dev_id);
void asr_oob_isr(struct asr_hw *asr_hw);
#endif
void asr_sdio_dataworker(struct work_struct *work);
void asr_main_task(struct asr_hw *asr_hw, u8 main_task_from_type);
//void asr_rx_tx_task(struct asr_hw *asr_hw);
#ifdef CONFIG_ASR_SDIO
void dump_sdio_info(struct asr_hw *asr_hw, const char *func, u32 line);
#endif
u8 count_bits(u16 data, u8 start_bit);
#endif /* _ASR_IRQS_H_ */
