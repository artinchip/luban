// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ArtInChip Technology Co.,Ltd
 * Author: Dehuang Wu <dehuang.wu@artinchip.com>
 */

#include <common.h>
#include <cpu_func.h>
#include <dm.h>
#include <wdt.h>
#include <dm/uclass-internal.h>
#include <dm/uclass.h>
#include <ram.h>
#include <command.h>
#include <hang.h>
#include <linux/delay.h>
#include <wdt.h>
#include <asm/unaligned.h>

int arch_cpu_init(void)
{
	if (!icache_status())
		icache_enable();

	return 0;
}

int dram_init(void)
{
	const fdt64_t *val;
	int offset;
	int len;

	offset = fdt_path_offset(gd->fdt_blob, "/memory");
	if (offset < 0)
		return -EINVAL;

	val = fdt_getprop(gd->fdt_blob, offset, "reg", &len);
	if (len < sizeof(*val) * 2)
		return -EINVAL;

	gd->ram_size = get_unaligned_be64(&val[1]);

	return 0;
}

void enable_caches(void)
{
	if (!icache_status())
		icache_enable();

	if (!dcache_status())
		dcache_enable();
	pr_info("Cache enabled.\n");
}

int print_cpuinfo(void)
{
	return 0;
}

#ifndef CONFIG_SPL_BUILD
int do_reset(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	struct udevice __maybe_unused *dev;
	int ret = 0;

	printf("resetting ...\n");
#ifdef CONFIG_WDT_ARTINCHIP
	ret = uclass_get_device(UCLASS_WDT, 0, &dev);
	if (ret) {
		pr_err("Failed to get watchdog.\n");
		return ret;
	}

	ret = wdt_expire_now(dev, 0);
	if (ret) {
		printf("Expiring watchdog failed. ret = %d.\n", ret);
		ret = CMD_RET_FAILURE;
	} else {
		ret = CMD_RET_SUCCESS;
	}

	mdelay(1000);
#endif
	hang();
	return ret;
}
#endif
