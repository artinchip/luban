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

#define CMU_USB_HOST_REG(id)	((void *)(0x18020420UL + (id) * 4))
#define CMU_USB_PHY_REG(id)	((void *)(0x18020430UL + (id) * 4))

#define SYSCFG_USB0_CFG         ((void *)0x1800040CUL)
#define USB0_DEV_MODE           (0x01)

#define CMU_USB_HOST_MSK	(0x3000)
#define CMU_USB_PHY_MSK		(0x2100)

#define USB_HOST_BASE_REG(id)	((0x10210000UL + (id) * 0x10000))
#define USB_HOST_CTRL_REG(id)	((void *)(USB_HOST_BASE_REG(id) + 0x800))
#define USB_EHCI_HCOR_BASE(id)	((void *)(USB_HOST_BASE_REG(id) + 0x10))

/* USB Interrupt Enable. Paragraph 2.3.3 */

#define EHCI_INT_USBINT		BIT(0) /* Bit 0:  USB Interrupt */
#define EHCI_INT_USBERRINT	BIT(1) /* Bit 1:  USB Error Interrupt */
#define EHCI_INT_PORTSC		BIT(2) /* Bit 2:  Port Change Detect */
#define EHCI_INT_FLROLL		BIT(3) /* Bit 3:  Frame List Rollover */
#define EHCI_INT_SYSERROR	BIT(4) /* Bit 4:  Host System Error */
#define EHCI_INT_AAINT		BIT(5) /* Bit 5:  Interrupt on Async Advance */
#define EHCI_INT_ALLINTS	(0x3f) /* Bits 0-5:  All interrupts */

#define EHCI_USBSTS_HALTED	BIT(12) /* Bit 12: HC Halted */

/* USB Command. Paragraph 2.3.1 */

#define EHCI_USBCMD_RUN		BIT(0) /* Bit 0: Run/Stop */
#define EHCI_USBCMD_HCRESET	BIT(1) /* Bit 1: Host Controller Reset */
#define EHCI_USBCMD_FLSIZE_MASK	(0x0C)
#define EHCI_USBCMD_PSEN	BIT(4) /* Bit 4: Periodic Schedule Enable */
#define EHCI_USBCMD_ASEN	BIT(5) /* Bit 5: Asynchronous Schedule Enable */
#define EHCI_USBCMD_IAADB	BIT(6) /* Bit 6: Interrupt on Async Advance Doorbell */

/* Configured Flag Register. Paragraph 2.3.8 */

#define EHCI_CONFIGFLAG		BIT(0)

/* Port Status/Control, Port 1-n. Paragraph 2.3.9 */

#define EHCI_PORTSC_CCS		BIT(0) /* Bit 0: Current Connect Status */
#define EHCI_PORTSC_CSC		BIT(1) /* Bit 1: Connect Status Change */
#define EHCI_PORTSC_PE		BIT(2) /* Bit 2: Port Enable */
#define EHCI_PORTSC_PEC		BIT(3) /* Bit 3: Port Enable/Disable Change */
#define EHCI_PORTSC_OCA		BIT(4) /* Bit 4: Over-current Active */
#define EHCI_PORTSC_OCC		BIT(5) /* Bit 5: Over-current Change */
#define EHCI_PORTSC_RESUME	BIT(6) /* Bit 6: Force Port Resume */
#define EHCI_PORTSC_SUSPEND	BIT(7) /* Bit 7: Suspend */
#define EHCI_PORTSC_RESET	BIT(8) /* Bit 8: Port Reset */
#define EHCI_PORTSC_PP		BIT(12) /* Bit 12: Port Power */
#define EHCI_PORTSC_OWNER	BIT(13) /* Bit 13: Port Owner */

/* Host Controller Operational Registers.
 * This register block is positioned at an offset of 'caplength' from the
 * beginning of the Host Controller Capability Registers.
 */

struct ehci_hcor_s {
	u32 usbcmd;		/* 0x00: USB Command */
	u32 usbsts;		/* 0x04: USB Status */
	u32 usbintr;		/* 0x08: USB Interrupt Enable */
	u32 frindex;		/* 0x0c: USB Frame Index */
	u32 ctrldssegment;	/* 0x10: 4G Segment Selector */
	u32 periodiclistbase;	/* 0x14: Frame List Base Address */
	u32 asynclistaddr;	/* 0x18: Next Asynchronous List Address */
	u32 reserved[9];
	u32 configflag;		/* 0x40: Configured Flag Register */
	u32 portsc[15];		/* 0x44: Port Status/Control */
};

/* This is the set of interrupts handled by this driver */
#define EHCI_HANDLED_INTS                                                      \
	(EHCI_INT_USBINT | EHCI_INT_USBERRINT | EHCI_INT_PORTSC |              \
	 EHCI_INT_SYSERROR | EHCI_INT_AAINT)

static int usb_ehci_reset(int id);
static int usb_ehci_wait_usbsts(int id, u32 maskbits, u32 donebits,
				u32 delay);

static void usb_hc_low_level_init(int id)
{
	u32 val;

	if (id == 0) {
		/* Switch to HOST mode */
		writel(0, SYSCFG_USB0_CFG);
	}

	writel(0, CMU_USB_HOST_REG(id));
	writel(0, CMU_USB_PHY_REG(id));
	udelay(100);
	/* Enable clock */
	writel(CMU_USB_HOST_MSK, CMU_USB_HOST_REG(id));
	writel(CMU_USB_PHY_MSK, CMU_USB_PHY_REG(id));
	udelay(100);
	/* set phy type: UTMI/ULPI */
	val = readl(USB_HOST_CTRL_REG(id));
	writel((val | 0x1), USB_HOST_CTRL_REG(id));
}

static int usb_hc_hw_init(int id)
{
	volatile struct ehci_hcor_s *hcor;
	int ret;
	u32 regval;

	hcor = (struct ehci_hcor_s *)USB_EHCI_HCOR_BASE(id);
	usb_hc_low_level_init(id);

	/* Reset the EHCI hardware */
	ret = usb_ehci_reset(id);
	if (ret < 0) {
		pr_err("ehci reset failed.\n");
		return -1;
	}

	/* Disable all interrupts */
	writel(0, &hcor->usbintr);

	/* Clear pending interrupts.  Bits in the USBSTS register are cleared by
	 * writing a '1' to the corresponding bit.
	 */
	writel(EHCI_INT_ALLINTS, &hcor->usbsts);

	/* Start the host controller by setting the RUN bit in the USBCMD register. */
	regval = readl(&hcor->usbcmd);
	regval |= EHCI_USBCMD_RUN;
	writel(regval, &hcor->usbcmd);

	/* Route all ports to this host controller by setting the CONFIG flag. */
	regval = readl(&hcor->configflag);
	regval |= EHCI_CONFIGFLAG;
	writel(regval, &hcor->configflag);

	/* Wait for the EHCI to run (i.e., no longer report halted) */
	ret = usb_ehci_wait_usbsts(id, EHCI_USBSTS_HALTED, 0, 50);
	if (ret < 0) {
		pr_err("Wait ehci run timeout.\n");
		return -ETIMEDOUT;
	}

	/* Enable port power */
	regval = readl(&hcor->portsc[0]);
	regval |= EHCI_PORTSC_PP;
	writel(regval, &hcor->portsc[0]);

	/* Enable EHCI interrupts.  Interrupts are still disabled at the level of
	 * the interrupt controller.
	 */
	writel(EHCI_HANDLED_INTS, &hcor->usbintr);

	return ret;
}

static void usb_hc_hw_deinit(int id)
{
	if (id == 0)
		writel(USB0_DEV_MODE, SYSCFG_USB0_CFG);

	writel(0, CMU_USB_HOST_REG(id));
	writel(0, CMU_USB_PHY_REG(id));
}

static int usbh_portchange_wait(int id)
{
	u32 usbsts, pending, regval, retry = 1000;
	volatile struct ehci_hcor_s *hcor;

	hcor = (struct ehci_hcor_s *)USB_EHCI_HCOR_BASE(id);
	do {
		/* Read Interrupt Status and mask out interrupts that are not enabled. */
		usbsts = readl(&hcor->usbsts);
		regval = readl(&hcor->usbintr);

		/* Handle all unmasked interrupt sources */
		pending = usbsts & regval;

		/* Clear all pending interrupts */
		writel(usbsts & EHCI_INT_ALLINTS, &hcor->usbsts);

		if ((pending & EHCI_INT_PORTSC) != 0) {
			pr_info("USB host port status is changed. retry %d\n",
				retry);
			return 0;
		}
		udelay(10);
	} while (--retry);

	pr_info("USB host port status is not changed. retry cnt left: %d\n",
		retry);
	return -1;
}

static bool usbh_get_port_connect_status(int id, int port)
{
	u32 portsc;
	bool connected = false;
	volatile struct ehci_hcor_s *hcor;

	hcor = (struct ehci_hcor_s *)USB_EHCI_HCOR_BASE(id);

	if (port <= 0) {
		pr_err("port id not correct %d\n", port);
		return false;
	}

	portsc = readl(&hcor->portsc[port - 1]);

	/* Handle port connection status change (CSC) events */
	if ((portsc & EHCI_PORTSC_CSC) != 0) {
		if ((portsc & EHCI_PORTSC_CCS) == EHCI_PORTSC_CCS) {
			/* Connected ... Did we just become connected? */
			pr_info("port connected\n");
			connected = true;
		} else {
			pr_info("port disconnected\n");
			connected = false;
		}
	}

	/* Clear all pending port interrupt sources by writing a '1' to the
	 * corresponding bit in the PORTSC register.  In addition, we need
	 * to preserve the values of all R/W bits (RO bits don't matter)
	 */
	writel(portsc, &hcor->portsc[port - 1]);

	return connected;
}

static int usb_ehci_reset(int id)
{
	u32 regval;
	u32  timeout;
	volatile struct ehci_hcor_s *hcor;

	hcor = (struct ehci_hcor_s *)USB_EHCI_HCOR_BASE(id);

	/* Make sure that the EHCI is halted:  "When [the Run/Stop] bit is set to
	 * 0, the Host Controller completes the current transaction on the USB and
	 * then halts. The HC Halted bit in the status register indicates when the
	 * Host Controller has finished the transaction and has entered the
	 * stopped state..."
	 */

	writel(0, &hcor->usbcmd);

	/* "... Software should not set [HCRESET] to a one when the HCHalted bit in
	 *  the USBSTS register is a zero. Attempting to reset an actively running
	 *  host controller will result in undefined behavior."
	 */

	timeout = 0;
	do {
		/* Wait and update the timeout counter */

		udelay(100);
		timeout++;

		/* Get the current value of the USBSTS register.  This loop will
		 * terminate when either the timeout exceeds one millisecond or when
		 * the HCHalted bit is no longer set in the USBSTS register.
		 */

		regval = readl(&hcor->usbsts);
	} while (((regval & EHCI_USBSTS_HALTED) == 0) && (timeout < 1000));

	/* Is the EHCI still running?  Did we timeout? */
	if ((regval & EHCI_USBSTS_HALTED) == 0) {
		pr_err("Wait ehci halt timeout.\n");
		return -ETIMEDOUT;
	}

	/* Now we can set the HCReset bit in the USBCMD register to
	 * initiate the reset
	 */

	regval = readl(&hcor->usbcmd);
	regval |= EHCI_USBCMD_HCRESET;
	writel(regval, &hcor->usbcmd);

	/* Wait for the HCReset bit to become clear */

	do {
		/* Wait and update the timeout counter */

		udelay(100);
		timeout += 1;

		/* Get the current value of the USBCMD register.  This loop will
		 * terminate when either the timeout exceeds one second or when the
		 * HCReset bit is no longer set in the USBSTS register.
		 */

		regval = readl(&hcor->usbcmd);
	} while (((regval & EHCI_USBCMD_HCRESET) != 0) && (timeout < 1000));

	/* Return either success or a timeout */

	return (regval & EHCI_USBCMD_HCRESET) != 0 ? -ETIMEDOUT : 0;
}

static int usb_ehci_wait_usbsts(int id, u32 maskbits, u32 donebits,
				u32 delay_ms)
{
	u32 regval;
	u32 timeout, tmo_us = delay_ms * 1000;
	volatile struct ehci_hcor_s *hcor;

	hcor = (struct ehci_hcor_s *)USB_EHCI_HCOR_BASE(id);

	timeout = 0;
	do {
		/* Wait 10usec before trying again */
		udelay(10);
		timeout += 10;

		/* Read the USBSTS register and check for a system error */
		regval = readl(&hcor->usbsts);
		if ((regval & EHCI_INT_SYSERROR) != 0)
			return -EIO;

		/* Mask out the bits of interest */
		regval &= maskbits;

		/* Loop until the masked bits take the specified value or until a
		 * timeout occurs.
		 */
	} while (regval != donebits && timeout < tmo_us);

	/* We got here because either the waited for condition or a timeout
	 * occurred.  Return a value to indicate which.
	 */
	return (regval == donebits) ? 0 : -ETIMEDOUT;
}

int usb_host_udisk_connection_check(void)
{
	int ret, id = 0;

	id = CONFIG_UPDATE_USB_CONTROLLER_ID_ARTINCHIP;
	if (usb_hc_hw_init(id)) {
		pr_err("usb_hc_hw_init failed.\n");
		ret = 0;
		goto out;
	}

	if (usbh_portchange_wait(id)) {
		ret = 0;
		goto out;
	}

	/* Only one root hub port */
	ret = usbh_get_port_connect_status(id, 1);
out:
	if (!ret && id == 0)
		usb_hc_hw_deinit(id);

	return ret;
}

