// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2019 Fraunhofer AISEC,
 * Lukas Auer <lukas.auer@aisec.fraunhofer.de>
 *
 * Based on common/spl/spl_atf.c
 */
#include <common.h>
#include <cpu_func.h>
#include <errno.h>
#include <hang.h>
#include <image.h>
#include <spl.h>
#include <linux/compiler.h>
#include <asm/global_data.h>
#include <asm/smp.h>
#include <opensbi.h>
#include <linux/libfdt.h>
#ifdef CONFIG_ARCH_ARTINCHIP
#include <asm/arch/boot_param.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

struct fw_dynamic_info opensbi_info;

#ifdef CONFIG_ARCH_RISCV_ARTINCHIP
static ulong g_uboot_entry;
ulong spl_opensbi_get_uboot_entry(void)
{
	return g_uboot_entry;
}

void spl_opensbi_set_uboot_entry(ulong entry)
{
	g_uboot_entry = entry;
}
#else
static int spl_opensbi_find_uboot_node(void *blob, int *uboot_node)
{
	int fit_images_node, node;
	const char *fit_os;

	fit_images_node = fdt_path_offset(blob, "/fit-images");
	if (fit_images_node < 0)
		return -ENODEV;

	fdt_for_each_subnode(node, blob, fit_images_node) {
		fit_os = fdt_getprop(blob, node, FIT_OS_PROP, NULL);
		if (!fit_os)
			continue;

		if (genimg_get_os_id(fit_os) == IH_OS_U_BOOT) {
			*uboot_node = node;
			return 0;
		}
	}

	return -ENODEV;
}
#endif

void spl_invoke_opensbi(struct spl_image_info *spl_image)
{
	ulong uboot_entry;
#ifdef CONFIG_ARCH_RISCV_ARTINCHIP
	ulong boot_param = 0;
	void (*opensbi_entry)(ulong hartid, ulong dtb, ulong info, ulong bp);

	uboot_entry = spl_opensbi_get_uboot_entry();
#else
	void (*opensbi_entry)(ulong hartid, ulong dtb, ulong info);
#endif
	if (spl_image->os != IH_OS_ARTINCHIP && !spl_image->fdt_addr) {
		pr_err("No device tree specified in SPL image\n");
		hang();
	}

#ifndef CONFIG_ARCH_RISCV_ARTINCHIP
	int ret, uboot_node;
	/* Find U-Boot image in /fit-images */
	ret = spl_opensbi_find_uboot_node(spl_image->fdt_addr, &uboot_node);
	if (ret) {
		pr_err("Can't find U-Boot node, %d\n", ret);
		hang();
	}

	/* Get U-Boot entry point */
	ret = fit_image_get_entry(spl_image->fdt_addr, uboot_node, &uboot_entry);
	if (ret)
		ret = fit_image_get_load(spl_image->fdt_addr, uboot_node, &uboot_entry);
#endif

	/* Prepare obensbi_info object */
	opensbi_info.magic = FW_DYNAMIC_INFO_MAGIC_VALUE;
	opensbi_info.version = FW_DYNAMIC_INFO_VERSION;
	opensbi_info.next_addr = uboot_entry;
	opensbi_info.next_mode = FW_DYNAMIC_INFO_NEXT_MODE_S;
#ifdef CONFIG_SPL_OPENSBI_NO_BOOT_PRINTS
	opensbi_info.options = SBI_SCRATCH_NO_BOOT_PRINTS;
#else
	opensbi_info.options = SBI_SCRATCH_DEBUG_PRINTS;
#endif
	opensbi_info.boot_hart = gd->arch.boot_hart;
	if (!spl_start_uboot())
		opensbi_info.options |= SBI_SCRATCH_FDT_NO_FIXUP;
	invalidate_icache_all();

#ifdef CONFIG_SPL_SMP
	/*
	 * Start OpenSBI on all secondary harts and wait for acknowledgment.
	 *
	 * OpenSBI first relocates itself to its link address. This is done by
	 * the main hart. To make sure no hart is still running U-Boot SPL
	 * during relocation, we wait for all secondary harts to acknowledge
	 * the call-function request before entering OpenSBI on the main hart.
	 * Otherwise, code corruption can occur if the link address ranges of
	 * U-Boot SPL and OpenSBI overlap.
	 */
	ret = smp_call_function((ulong)spl_image->entry_point,
				(ulong)spl_image->fdt_addr,
				(ulong)&opensbi_info, 1);
	if (ret)
		hang();
#endif
#ifdef CONFIG_ARTINCHIP_DEBUG_BOOT_TIME
	u32 *p = (u32 *)BOOT_TIME_SPL_EXIT;
	*p = aic_timer_get_us();
#endif
#ifdef CONFIG_ARCH_ARTINCHIP
	set_boot_device(boot_param, aic_get_boot_device());
	set_boot_reason(boot_param, aic_get_boot_reason());

	opensbi_entry =
		(void (*)(ulong, ulong, ulong, ulong))spl_image->entry_point;
	opensbi_entry(gd->arch.boot_hart, (ulong)spl_image->fdt_addr,
		      (ulong)&opensbi_info, boot_param);
#else
	opensbi_entry = (void (*)(ulong, ulong, ulong))spl_image->entry_point;
	opensbi_entry(gd->arch.boot_hart, (ulong)spl_image->fdt_addr,
		      (ulong)&opensbi_info);
#endif
}

#ifdef CONFIG_SPL_OS_BOOT
void __noreturn jump_to_image_linux(struct spl_image_info *spl_image)
	__attribute__((weak, alias("spl_invoke_opensbi")));
#endif
