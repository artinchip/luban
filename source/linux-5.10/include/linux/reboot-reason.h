/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __REBOOT_REASON_H__
#define __REBOOT_REASON_H__

enum aic_reboot_reason {
	REBOOT_REASON_COLD = 0,
	REBOOT_REASON_CMD_REBOOT = 1,
	REBOOT_REASON_CMD_SHUTDOWN = 2,
	REBOOT_REASON_SUSPEND = 3,
	REBOOT_REASON_UPGRADE = 4,
	REBOOT_REASON_FASTBOOT = 5,

	/* Some software exception reason */
	REBOOT_REASON_SW_LOCKUP = 8,
	REBOOT_REASON_HW_LOCKUP = 9,
	REBOOT_REASON_PANIC = 10,
	REBOOT_REASON_RAMDUMP = 11,

	/* Some hardware exception reason */
	REBOOT_REASON_RTC = 17,
	REBOOT_REASON_EXTEND = 18,
	REBOOT_REASON_DM = 19,
	REBOOT_REASON_OTP = 20,
	REBOOT_REASON_UNDER_VOL = 21,

	REBOOT_REASON_INVALID = 0xff,
};

/* Defined in ArtInChip WRI driver */

#if defined(CONFIG_RTC_DRV_ARTINCHIP_V01) || defined(CONFIG_ARTINCHIP_WRI)
void aic_set_reboot_reason(enum aic_reboot_reason reason);
enum aic_reboot_reason aic_get_reboot_reason(void);
#else
void __weak aic_set_reboot_reason(enum aic_reboot_reason reason)
{
	(void)reason; // Unused
}

enum aic_reboot_reason __weak aic_get_reboot_reason(void)
{
	return REBOOT_REASON_COLD;
}
#endif

#endif
