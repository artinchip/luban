// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 ArtInChip Technology Co., Ltd
 */

#include <common.h>
#include <command.h>
#include <console.h>
#include <malloc.h>
#include <env.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <asm/arch/usb_detect.h>
#include <userid.h>
#include <dt-bindings/gpio/gpio.h>

#include <dm.h>
#include <fdtdec.h>
#include <fdt_support.h>
#include <asm/global_data.h>

DECLARE_GLOBAL_DATA_PTR;

#define RTC_CMU_REG               ((void *)0x18020908)
#define RTC_CMU_BUS_EN_MSK        (0x1000)

#define BASE_RTC                  ((void *)0x19030000)
#define RTC_WRITE_KEY_REG         (BASE_RTC + 0x0FC)
#define RTC_WRITE_KEY_VALUE       (0xAC)

#define RTC_BOOTINFO1_REG         (BASE_RTC + 0x100)
#define BOOTINFO1_REASON_OFF      (4)
#define BOOTINFO1_REASON_MSK      (0xF << 4)
#define RTC_REBOOT_REASON_UPGRADE (4)

#define RTC_SYS_BAK_BASE	  (BASE_RTC + 0x104)
#define RTC_SYS_BAK_REG(id)	  (RTC_SYS_BAK_BASE + (id) * 0x04)

#define GPIO_IE                   (1 << 16)
#define GPIO_PULLUP               (3 << 8)
#define GPIO_PULLDOWN             (2 << 8)
#define GPIO_DRV_2                (2 << 4)

#define GPIO_PIN_INPUT_CFG (GPIO_IE | GPIO_DRV_2 | 1)
#define PIN_CHECK_CNT 4

static int usbupg_boot_pin_check(void)
{
	unsigned long base, addr;
	u32 bak, val, status = 0;
	ofnode config_node, upgpin_node;
	u32 regs[5], pin_idx, active_lvl, reg_cfg, reg_in;
	struct ofnode_phandle_args args;
	int ret, cnt;

	if (!gd->fdt_blob || ((uintptr_t)gd->fdt_blob & 3) ||
	    fdt_check_header(gd->fdt_blob)) {
		pr_info(" No valid DTB\n");
		return 0;
	}
	config_node = ofnode_path("/config");
	if (!ofnode_valid(config_node))
		return 0;

	ret = ofnode_parse_phandle_with_args(config_node, "aic,upgmode-gpio",
					     NULL, 2, 0, &args);
	if (ret) {
		pr_info("Get aic,upgmode-gpio failed.\n");
		return 0;
	}
	pin_idx = args.args[0];
	active_lvl = args.args[1];
	upgpin_node = args.node;

	/* Get register offset */
	ret = ofnode_read_u32_array(upgpin_node, "gpio_regs", regs, 5);
	if (ret) {
		pr_info("Read regs failed.\n");
		return 0;
	}
	reg_in = regs[0];
	reg_cfg = regs[4];
	ret = ofnode_parse_phandle_with_args(upgpin_node, "gpio-ranges", NULL,
					     3, 0, &args);
	if (ret) {
		pr_info("Get pinctrl failed.\n");
		return 0;
	}

	/* Get pinctrl's base address */
	base = ofnode_get_addr(args.node);

	/* aic,upgmode-gpio PINCFG reg address */
	addr = base + reg_cfg + pin_idx * 4;

	bak = readl((void *)addr);
	if (active_lvl == GPIO_ACTIVE_LOW)
		val = GPIO_PIN_INPUT_CFG | GPIO_PULLUP;
	else
		val = GPIO_PIN_INPUT_CFG | GPIO_PULLDOWN;
	writel(val, (void *)addr);

	/* aic,upgmode-gpio input status reg address */
	cnt = PIN_CHECK_CNT;
	while (cnt) {
		addr = base + reg_in;
		val = readl((void *)addr);
		status += (val >> pin_idx) & 0x01;
		cnt--;
		udelay(250);
	}

	ret = 0;
	/*
	 * if GPIO_ACTIVE_LOW: input value should be all 0
	 * if GPIO_ACTIVE_HIGH: input value should be all 1
	 */
	if (active_lvl != GPIO_ACTIVE_LOW)
		status = PIN_CHECK_CNT - status;
	if (status == 0) {
		ret = 1;
		goto out;
	}

out:
	/* Reset to previous state */
	addr = base + reg_cfg + pin_idx * 4;
	writel(bak, (void *)addr);

	if (ret) {
		printf("aic,upgmode-gpio pin is pressed, enter upgrading mode.\n");
	}

	return ret;
}

static int rtc_upg_flag_check(void)
{
	u32 val;

	val = readl((void *)RTC_BOOTINFO1_REG);
	val = (val & BOOTINFO1_REASON_MSK) >> BOOTINFO1_REASON_OFF;

	if (val == RTC_REBOOT_REASON_UPGRADE) {
		printf("Software reboot to enter upgrading mode.\n");
		return 1;
	}

	return 0;
}

void rtc_upg_succ_cnt(void)
{
	u32 val;

	val = readl(RTC_CMU_REG);
	if (!(val & RTC_CMU_BUS_EN_MSK))
		writel(RTC_CMU_BUS_EN_MSK, RTC_CMU_REG);

	writel(RTC_WRITE_KEY_VALUE, RTC_WRITE_KEY_REG);
	val = readl(RTC_SYS_BAK_REG(14));
	val += 1;
	writel(val, RTC_SYS_BAK_REG(14));
	writel(0, RTC_WRITE_KEY_REG);

	pr_info("Successfully burned %d times.\n", val);
}

int aic_upg_mode_detect(void)
{
	int ret;

	ret = rtc_upg_flag_check();
	if (ret)
		return ret;
	return usbupg_boot_pin_check();
}

#ifndef CONFIG_SPL_BUILD /* In U-Boot */
static int check_sd_fat32_upg(void)
{
	char *p;

	p = env_get("boot_device");
	if (!p)
		return 0;
	if (!strcmp(p, "fat"))
		return 1;
	return 0;
}

static int check_usb_upg(void)
{
	char *p;

	p = env_get("boot_device");
	if (!p)
		return 0;
	if (!strcmp(p, "usb"))
		return 1;

	if (aic_upg_mode_detect())
		return 1;
	return 0;
}

static void rtc_upg_flag_clear(void)
{
	u32 val;

	val = readl(RTC_CMU_REG);
	if (!(val & RTC_CMU_BUS_EN_MSK))
		writel(RTC_CMU_BUS_EN_MSK, RTC_CMU_REG);

	writel(RTC_WRITE_KEY_VALUE, RTC_WRITE_KEY_REG);
	val = readl(RTC_BOOTINFO1_REG);
	val &= ~BOOTINFO1_REASON_MSK;
	writel(val, RTC_BOOTINFO1_REG);
	writel(0, RTC_WRITE_KEY_REG);
}

#ifdef CONFIG_USERID_SUPPORT
static int usb_burn_userid_mode(void)
{
	int ret;
	u32 flag = 0;

	ret = userid_read("lock", 0, (void *)&flag, 4);

	if (ret <= 0) {
		/* userid lock is not exist */
		return 1;
	}
	if (flag == 0) {
		/* userid is in unlocked state */
		return 1;
	}
	/* userid is in locked state, don't goto burn userid mode agian */
	return 0;
}
#endif

static int do_upgrade_detect(struct cmd_tbl *cmdtp, int flag, int argc,
			     char *const argv[])
{
	int ret = 0;

	env_set("upg_type", "");
	if (check_usb_upg()) {
		rtc_upg_flag_clear();
		ret = 1;
		env_set("upg_type", "usb");
		goto out;
	}

	if (check_sd_fat32_upg()) {
		ret = 1;
		env_set("upg_type", "sdcard");
		goto out;
	}

#ifdef CONFIG_UPDATE_UDISK_FATFS_ARTINCHIP
	if (usb_host_udisk_connection_check()) {
		ret = 1;
		env_set("upg_type", "udisk");
		goto out;
	}
#endif
#ifdef CONFIG_USERID_SUPPORT
	if (usb_burn_userid_mode()) {
		ret = 1;
		env_set("upg_type", "usb");
		env_set("upg_mode", "userid");
		goto out;
	}
#endif
#ifdef CONFIG_AICUPG_FORCE_USBUPG_SUPPORT
	/* Force USB upgrading mode, jump into USB loop and checking
	 * This checking should behind of USERID checking
	 */
	{
		ret = 1;
		env_set("upg_type", "usb");
		env_set("upg_mode", "force");
		goto out;
	}
#endif

out:
	return ret;
}

U_BOOT_CMD(upg_detect, 1, 0, do_upgrade_detect,
	"ArtInChip upgrade detect command",
	"Return code:\n"
	"  - 0: No need to enter upgrade mode\n"
	"  - 1: Need to enter upgrade mode\n"
);

#endif
