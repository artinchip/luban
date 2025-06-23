// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024, ArtInChip Co., Ltd
 * Author: dwj <weijie.ding@artinchip.com>
 */

#include <linux/init.h>
#include <linux/suspend.h>
#include <asm/sbi.h>

#define SBI_HSM_SUSPEND_RET_PLATFORM		0x10000000

static int aic_pm_state_enter(suspend_state_t state)
{
	struct sbiret ret;

	ret = sbi_ecall(SBI_EXT_HSM, SBI_EXT_HSM_HART_SUSPEND,
			SBI_HSM_SUSPEND_RET_PLATFORM, 0, 0, 0, 0, 0);

	return (ret.error) ? sbi_err_map_linux_errno(ret.error) : 0;
}

static const struct platform_suspend_ops aic_pm_ops = {
	.valid	= suspend_valid_only_mem,
	.enter	= aic_pm_state_enter,
};

static int __init aic_suspend_init(void)
{
	suspend_set_ops(&aic_pm_ops);

	return 0;
}

subsys_initcall(aic_suspend_init);

