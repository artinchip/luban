// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd
 * Author: Dehuang Wu <dehuang.wu@artinchip.com>
 */

#include <common.h>
#include <cache.h>
#include <asm/csr.h>

#define sync_is()   do { asm volatile (".long 0x01b0000b"); } while (0)
#define MHCR  0x7C1
#define MCOR  0x7C2
#define MHINT 0x7C5

#define MCOR_SEL_ICACHE    (1 << 0)
#define MCOR_SEL_DCACHE    (1 << 1)
#define MCOR_SEL_ALL_CACHE (0x3)

#define MHCR_ICACHE_EN     (1 << 0)
#define MHCR_DCACHE_EN     (1 << 1)
#define MHCR_WRITE_ALLOC   (1 << 2)
#define MHCR_WRITE_BACK    (1 << 3)
#define MHCR_RS            (1 << 4)
#define MHCR_BPE           (1 << 5)
#define MHCR_BTB           (1 << 6)
#define MHCR_WBR           (1 << 8)

#define MHINT_DPLD         (1 << 2)
#define MHINT_AMR_1        (1 << 3)
#define MHINT_AMR_3        (3 << 3)
#define MHINT_IPLD         (1 << 8)
#define MHINT_IWPE         (1 << 10)
#define MHINT_DDIS_3       (3 << 13)

#define MHCR_ICACHE_OPTIONS (MHCR_ICACHE_EN | MHCR_RS | MHCR_BPE | MHCR_BTB)
#define MHCR_DCACHE_OPTIONS (MHCR_DCACHE_EN | MHCR_WRITE_ALLOC | MHCR_WRITE_BACK | MHCR_WBR)
#define MHINT_ICACHE_OPTIONS (MHINT_IPLD | MHINT_IWPE)
#define MHINT_DCACHE_OPTIONS (MHINT_DPLD | MHINT_AMR_1 |MHINT_DDIS_3)

void flush_dcache_all(void)
{
	asm volatile(".long 0x0010000b");  /* dcache.call */
	sync_is();
}

void flush_dcache_range(unsigned long start, unsigned long end)
{
	register unsigned long i asm("a0") =
		start & ~(CONFIG_SYS_CACHELINE_SIZE - 1);

	for (; i < end; i += CONFIG_SYS_CACHELINE_SIZE)
		asm volatile(".long 0x0295000b");  /* dcache.cpa a0 */

	sync_is();

}

void invalidate_dcache_range(unsigned long start, unsigned long end)
{
	register unsigned long i asm("a0") =
		start & ~(CONFIG_SYS_CACHELINE_SIZE - 1);

	for (; i < end; i += CONFIG_SYS_CACHELINE_SIZE)
		asm volatile(".long 0x02b5000b");  /* dcache.cipa a0 */

	sync_is();
}

void icache_enable(void)
{
#ifdef CONFIG_SPL_BUILD
#if !CONFIG_IS_ENABLED(SYS_ICACHE_OFF)
#if CONFIG_IS_ENABLED(RISCV_MMODE)
	csr_set(MHCR, MHCR_ICACHE_OPTIONS);
	csr_set(MHINT, MHINT_ICACHE_OPTIONS);
#endif
#endif
#endif
}

void icache_disable(void)
{
#ifdef CONFIG_SPL_BUILD
#if !CONFIG_IS_ENABLED(SYS_ICACHE_OFF)
#if CONFIG_IS_ENABLED(RISCV_MMODE)
	csr_clear(MHCR, MHCR_ICACHE_OPTIONS);
	csr_clear(MHINT, MHINT_ICACHE_OPTIONS);
#endif
#endif
#endif
}

void dcache_enable(void)
{
#ifdef CONFIG_SPL_BUILD
#if !CONFIG_IS_ENABLED(SYS_ICACHE_OFF)
#if CONFIG_IS_ENABLED(RISCV_MMODE)
	csr_set(MHCR, MHCR_DCACHE_OPTIONS);
	csr_set(MHINT, MHINT_DCACHE_OPTIONS);
#endif
#endif
#endif
}

void dcache_disable(void)
{
#ifdef CONFIG_SPL_BUILD
#if !CONFIG_IS_ENABLED(SYS_DCACHE_OFF)
#if CONFIG_IS_ENABLED(RISCV_MMODE)
	csr_clear(MHCR, MHCR_DCACHE_OPTIONS);
	csr_clear(MHINT, MHINT_DCACHE_OPTIONS);
#endif
#endif
#endif
}

int icache_status(void)
{
	int ret = 0;
#ifdef CONFIG_SPL_BUILD
#if !CONFIG_IS_ENABLED(SYS_ICACHE_OFF)
#if CONFIG_IS_ENABLED(RISCV_MMODE)
	ret = ((csr_read(MHCR) & MHCR_ICACHE_EN) == MHCR_ICACHE_EN);
#endif
#endif
#endif
	return ret;
}

int dcache_status(void)
{
	int ret = 0;
#ifdef CONFIG_SPL_BUILD
#if !CONFIG_IS_ENABLED(SYS_DCACHE_OFF)
#if CONFIG_IS_ENABLED(RISCV_MMODE)
	ret = ((csr_read(MHCR) & MHCR_DCACHE_EN) == MHCR_DCACHE_EN);
#endif
#endif
#endif
	return ret;
}
