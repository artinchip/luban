/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 */

#include <platform_override.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/fdt/fdt_fixup.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_hsm.h>
#include <sbi/sbi_string.h>
#include <sbi/riscv_encoding.h>
#include <sbi/riscv_asm.h>
#include <sbi/riscv_io.h>

#define AIC_SRAM_BASE		0x00100000
extern void aic_suspend_resume();
extern u32 aic_suspend_resume_size;
static void (*aic_suspend_resume_fn)();

static u64 sifive_fu540_tlbr_flush_limit(const struct fdt_match *match)
{
	/*
	 * The sfence.vma by virtual address does not work on
	 * SiFive FU540 so we return remote TLB flush limit as zero.
	 */
	return 0;
}

static int sifive_fu540_fdt_fixup(void *fdt, const struct fdt_match *match)
{
	/*
	 * SiFive Freedom U540 has an erratum that prevents S-mode software
	 * to access a PMP protected region using 1GB page table mapping, so
	 * always add the no-map attribute on this platform.
	 */
	fdt_reserved_memory_nomap_fixup(fdt);

	return 0;
}

static inline void icache_invalid(void)
{
	__asm__ __volatile__("fence");
	__asm__ __volatile__("fence.i");
	__asm__ __volatile__(".long 0x0100000b");
	__asm__ __volatile__("fence");
	__asm__ __volatile__("fence.i");
}

static inline void dcache_clean_invalid(void)
{
	__asm__ __volatile__("fence");
	__asm__ __volatile__("fence.i");
	__asm__ __volatile__(".long 0x0030000b");
	__asm__ __volatile__("fence");
	__asm__ __volatile__("fence.i");
}

static int artinchip_hart_suspend(u32 suspend_type, ulong raddr)
{
	unsigned long saved_mie;

	if (suspend_type) {
		sbi_memcpy((void *)AIC_SRAM_BASE, aic_suspend_resume,
			   aic_suspend_resume_size);
		aic_suspend_resume_fn = (void *)AIC_SRAM_BASE;

		saved_mie = csr_read(CSR_MIE);
		//Only enable SEIE
		csr_write(CSR_MIE, MIP_SEIP);

		icache_invalid();
		dcache_clean_invalid();
		aic_suspend_resume_fn();

		csr_write(CSR_MIE, saved_mie);
	}

	return 0;
}

static const struct sbi_hsm_device artinchip_hsm_device = {
	.name 		= "aic_hsm_device",
	.hart_suspend 	= artinchip_hart_suspend,
};

int artinchip_early_init(bool cold_boot, const struct fdt_match *match)
{
	if (cold_boot)
		sbi_hsm_set_device(&artinchip_hsm_device);

	return 0;
}

static const struct fdt_match sifive_fu540_match[] = {
	{ .compatible = "artinchip,d211" },
	{ .compatible = "sifive,fu540" },
	{ .compatible = "sifive,fu540g" },
	{ .compatible = "sifive,fu540-c000" },
	{ .compatible = "sifive,hifive-unleashed-a00" },
	{ },
};

const struct platform_override sifive_fu540 = {
	.match_table = sifive_fu540_match,
	.tlbr_flush_limit = sifive_fu540_tlbr_flush_limit,
	.fdt_fixup = sifive_fu540_fdt_fixup,
	.early_init = artinchip_early_init,
};
