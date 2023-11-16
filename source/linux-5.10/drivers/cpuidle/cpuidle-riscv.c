// SPDX-License-Identifier: GPL-2.0-only
/*
 *
 * RISCV generic CPU idle driver.
 *
 * Copyright (C) 2020 ALLWINNERTECH.
 * Author: liush <liush@allwinnertech.com>
 */

#include <linux/cpuidle.h>
#include <linux/cpumask.h>
#include <linux/cpu_pm.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <asm/sbi.h>

#include "dt_idle_states.h"

#define MAX_IDLE_STATES		1

static int riscv_enter_idle(struct cpuidle_device *dev,
			struct cpuidle_driver *drv, int index)
{
	struct sbiret ret;

	ret = sbi_ecall(SBI_EXT_HSM, SBI_EXT_HSM_HART_SUSPEND,
			0, 0, 0, 0, 0, 0);
	return ret.error;
}

static struct cpuidle_driver riscv_idle_driver = {
	.name = "riscv_idle",
	.owner = THIS_MODULE,
	.states[0] = {
		.enter = riscv_enter_idle,
		.exit_latency = 1,
		.target_residency = 1,
		.power_usage = UINT_MAX,
		.name = "WFI",
		.desc = "RISCV WFI",
	},
	.state_count = MAX_IDLE_STATES,
};

static int __init riscv_idle_init(void)
{
	int ret;
	struct cpuidle_driver *drv;

	drv = kmemdup(&riscv_idle_driver, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	drv->cpumask = (struct cpumask *)cpumask_of(0);

	ret = cpuidle_register(drv, NULL);
	if (ret)
		goto out_kfree_drv;

	return 0;

out_kfree_drv:
	kfree(drv);
	return ret;

}

device_initcall(riscv_idle_init);
