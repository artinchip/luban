/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Artinchip Technology Co.,Ltd
 * Author: Dehuang Wu <dehuang.wu@artinchip.com>
 */
#ifndef __BOOT_PARAM_H__
#define __BOOT_PARAM_H__

#define set_boot_device(var, device)                                           \
	{                                                                      \
		(var) &= ~(0xF);                                               \
		(var) |= (device)&0xF;                                         \
	}

#define set_boot_reason(var, reason)                                           \
	{                                                                      \
		(var) &= ~(0xF << 4);                                          \
		(var) |= (reason) & (0xF << 8);                                \
	}


#define BOOT_TIME_SPL_START			(CONFIG_ARTINCHIP_DEBUG_SPL_BOOT_TIME)
#define BOOT_TIME_SPL_EXIT			(CONFIG_ARTINCHIP_DEBUG_SPL_BOOT_TIME + 4)
#define BOOT_TIME_UBOOT_START			(CONFIG_ARTINCHIP_DEBUG_SPL_BOOT_TIME + 8)
#define BOOT_TIME_UBOOT_RELOCATE_DONE		(CONFIG_ARTINCHIP_DEBUG_SPL_BOOT_TIME + 12)
#define BOOT_TIME_UBOOT_LOAD_KERNEL_START	(CONFIG_ARTINCHIP_DEBUG_SPL_BOOT_TIME + 16)
#define BOOT_TIME_UBOOT_LOAD_KERNEL_DONE	(CONFIG_ARTINCHIP_DEBUG_SPL_BOOT_TIME + 20)
#define BOOT_TIME_UBOOT_START_KERNEL		(CONFIG_ARTINCHIP_DEBUG_SPL_BOOT_TIME + 24)

union boot_params {
	/*
	 * Save r0~r15 registers when save_boot_params is called.
	 */
	u32 regs[16];
	struct {
		u32 device   : 4;
		u32 reason   : 4;
		u32 r0reserved : 24;
		u32 reserved[15];
	} boot;
};

enum boot_reason {
	BR_COLD_BOOT = 0,
	BR_WARM_BOOT,
};

enum boot_device {
	BD_NONE,
	BD_SDMC0,
	BD_SDMC1,
	BD_SDMC2,
	BD_SPINOR,
	BD_SPINAND,
	BD_SDFAT32,
	BD_BOOTROM,
};

enum boot_reason aic_get_boot_reason(void);
enum boot_device aic_get_boot_device(void);
u32 aic_get_boot_time_us(void);
void aic_boot_time_trace(char *msg);
unsigned long aic_timer_get_us(void);

#endif /* __BOOT_PARAM_H__ */
