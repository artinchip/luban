/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2017 Andes Technology Corporation
 * Rick Chen, Andes Technology Corporation <rick@andestech.com>
 */

#ifndef __ASM_RISCV_SYSTEM_H
#define __ASM_RISCV_SYSTEM_H

/*
 * Save the current interrupt enable state & disable IRQs
 */

/*
 * Save the current interrupt enable state
 * and disable IRQs/FIQs
 */
void __attribute__((weak)) local_irq_save(unsigned int flag)
{
	;
}

/*
 * Enable IRQs
 */
void __attribute__((weak)) local_irq_enable(unsigned int flag)
{
	;
}

/*
 * Disable IRQs
 */
void __attribute__((weak)) local_irq_disable(unsigned int flag)
{
	;
}

/*
 * Enable FIQs
 */
void __attribute__((weak)) __stf(void)
{
	;
}

/*
 * Disable FIQs
 */
void __attribute__((weak)) __clf(void)
{
	;
}

/*
 * Save the current interrupt enable state.
 */
void __attribute__((weak)) local_save_flags(unsigned int flag)
{
	;
}

/*
 * restore saved IRQ & FIQ state
 */
void __attribute__((weak)) local_irq_restore(unsigned int flag)
{
	;
}

#endif	/* __ASM_RISCV_SYSTEM_H */
