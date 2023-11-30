// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ArtInChip Technology Co.,Ltd
 * Author: Dehuang Wu <dehuang.wu@artinchip.com>
 */

#include <common.h>
#include <bootstage.h>
#include <linux/io.h>
#include <asm/arch/boot_param.h>
#include <asm/csr.h>

/*
 * Save boot parameters and context when save_boot_params is called.
 */
union boot_params boot_params_stash __section(".data");

enum boot_reason aic_get_boot_reason(void)
{
	enum boot_reason reason;

#ifdef CONFIG_SPL_BUILD
	/* SPL use a0 */
	reason = get_boot_reason(boot_params_stash.r.a[0]);
#else
	/* U-Boot use a3 */
	reason = get_boot_reason(boot_params_stash.r.a[3]);
#endif
	return reason;
}

static const char *const boot_device_name[] = {
	"BD_NONE",
	"BD_SDMC0",
	"BD_SDMC1",
	"BD_SDMC2",
	"BD_SPINOR",
	"BD_SPINAND",
	"BD_SDFAT32",
	"BD_USB",
};

#ifdef CONFIG_SPL_BUILD
static char *boot_stage = "[SPL]";
#else
static char *boot_stage = "[U-Boot]";
#endif

static void show_boot_device(u32 dev)
{
	static u32 show_flag = 1;

	if (show_flag) {
		/* Print once only */
		printf("%s: Boot device = %d(%s)\n", boot_stage, dev,
		       boot_device_name[dev]);
		show_flag = 0;
	}
}

enum boot_device aic_get_boot_device(void)
{
	enum boot_device dev;

#ifdef CONFIG_SPL_BUILD
	/* SPL use a0 */
	dev = get_boot_device(boot_params_stash.r.a[0]);
#else
	/* U-Boot use a3 */
	dev = get_boot_device(boot_params_stash.r.a[3]);
#endif
	show_boot_device(dev);
	return dev;
}

#ifdef CONFIG_SPL_BUILD
enum boot_controller aic_get_boot_controller(void)
{
	return get_boot_controller(boot_params_stash.r.a[0]);
}

int aic_get_boot_image_id(void)
{
	return get_boot_image_id(boot_params_stash.r.a[0]);
}
#endif

#ifdef CONFIG_ARTINCHIP_DEBUG_BOOT_TIME
unsigned long aic_timer_get_us(void)
{
	u64 tick = csr_read(CSR_TIME);
	return (unsigned long)(tick >> 2);
}
#endif
