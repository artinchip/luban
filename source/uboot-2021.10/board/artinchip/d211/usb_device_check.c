// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 ArtInChip Technology Co., Ltd
 *
 * Wu Dehuang
 */

#include <common.h>
#include <command.h>
#include <console.h>
#include <malloc.h>
#include <env.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <asm/arch/usb_detect.h>

#define SYSCFG_USB0_CFG         ((void *)0x1800040CUL)
#define USB0_DEV_MODE           (0x01)

#define CMU_MOD_AHB0_USB_DEV           ((void *)0x1802041CUL)
#define CMU_MOD_AHB0_USB_PHY0          ((void *)0x18020430UL)
#define USB_DEV_CONF                   ((void *)0x10200200UL)
#define USB_DEV_FUNC                   ((void *)0x10200204UL)
#define USB_DEV_INTSTS                 ((void *)0x10200010UL)
#define USB_DEV_INTMSK                 ((void *)0x10200014UL)
#define USB_DEV_INTSTS_ENUMDNE         (0x1U << 13)

#define USB_DEV_INIT        ((void *)0x10200004UL)
#define USB_DEV_DIEPMSK     ((void *)0x1020020CUL)
#define USB_DEV_DOEPMSK     ((void *)0x10200210UL)
#define USB_DEV_DAINT       ((void *)0x10200214UL)
#define USB_DEV_DAINTMSK    ((void *)0x10200218UL)

#define USB_DEV_SPEED_HIGH  0x00
#define USB_DEV_SPEED_FULL2 0x01
#define USB_DEV_SPEED_LOW   0x10
#define USB_DEV_SPEED_FULL1 0x11

#define USB_DEV_GINTMSK_ENUMDNE         (0x1U << 13)

static void usb_dev_init(void)
{
	u32 val;

	/* USB0 Switch to Device mode */
	val = readl(SYSCFG_USB0_CFG);
	val |= USB0_DEV_MODE;

	writel(0x0, CMU_MOD_AHB0_USB_DEV);
	writel(0x0, CMU_MOD_AHB0_USB_PHY0);
	udelay(100);
	/* Enable Device and Phy
	 *      1. Enable device and phy's clock
	 *      2. Delay more than 40us
	 *      3. Release the reset signal
	 */
	writel(0x1100, CMU_MOD_AHB0_USB_DEV);
	writel(0x1100, CMU_MOD_AHB0_USB_PHY0);
	udelay(100);
	writel(0x3100, CMU_MOD_AHB0_USB_PHY0);
	udelay(1);
	writel(0x3100, CMU_MOD_AHB0_USB_DEV);
	udelay(1);

	val = 0xFFFFFFFF;
	writel(val, USB_DEV_INTSTS);

	val = readl(USB_DEV_INTMSK);
	val |= USB_DEV_GINTMSK_ENUMDNE;
	writel(val, USB_DEV_INTMSK);

	/*
	 * Ummask/Enable the global interrupt
	 */
	val = readl(USB_DEV_INIT);
	val |= (0x1U << 9);
	writel(val, USB_DEV_INIT);

	/* Set DCFG Register bit[1:0], Device Speed to High speed  */
	val = readl(USB_DEV_CONF);

	val &= ~(0x3 << 0);
	val |= USB_DEV_SPEED_HIGH;
	writel(val, USB_DEV_CONF);
}

void usb_dev_soft_disconnect(void)
{
	u32 val;

	/* Set Soft Disconnect bit to signal the usb device controller to do a
	 * soft disconnect.
	 *
	 * Bit[1]: Default is 1, disconnect.
	 */
	val = readl(USB_DEV_FUNC);
	val |= (0x1U << 1);
	writel(val, USB_DEV_FUNC);
}

static void usb_dev_soft_connect(void)
{
	u32 val;

	val = readl(USB_DEV_FUNC);
	val &= ~(0x1U << 1);
	writel(val, USB_DEV_FUNC);
}

static u32 usb_dev_get_intsts(void)
{
	u32 val, msk;

	val = readl(USB_DEV_INTSTS);
	msk = readl(USB_DEV_INTMSK);

	return val & msk;
}

/*
 * When the USB0 works as device, checking if it is connecting with PC will
 * spend about 200ms, it is too long if we just waiting here.
 *
 * To save the waiting time, we split the checking to 2 parts:
 *   1. Initialize it when bus clock is set
 *   2. Double check after Linux image is loaded(for Falcon mode)
 *   3. If the status is connecting, than force to load u-boot again to force
 *      run u-boot
 */
void usb_dev_connection_check_start(int id)
{
	usb_dev_init();
	usb_dev_soft_connect();
}

void usb_dev_connection_check_end(int id)
{
	usb_dev_soft_disconnect();
	writel(0x0, CMU_MOD_AHB0_USB_DEV);
	writel(0x0, CMU_MOD_AHB0_USB_PHY0);
}

int usb_dev_connection_check_status(int id)
{
	u32 sts;
	int ret = 0;

	sts = usb_dev_get_intsts();
	if (sts & USB_DEV_INTSTS_ENUMDNE)
		ret = 1;
	return ret;
}
