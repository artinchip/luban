/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2017 Andes Technology Corporation
 * Rick Chen, Andes Technology Corporation <rick@andestech.com>
 */

#ifndef __ASM_RISCV_SYSTEM_H
#define __ASM_RISCV_SYSTEM_H

#include <asm/csr.h>

/*
 * Interupt configuration macros
 */

#define local_irq_save(__flags)                                 \
	do {                                                           \
		__flags = csr_read_clear(CSR_SSTATUS, SR_SIE) & SR_SIE; \
	} while (0)

#define local_irq_restore(__flags)             \
	do {                                          \
		csr_set(CSR_SSTATUS, __flags &SR_SIE); \
	} while (0)

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

#endif /* __ASM_RISCV_SYSTEM_H */