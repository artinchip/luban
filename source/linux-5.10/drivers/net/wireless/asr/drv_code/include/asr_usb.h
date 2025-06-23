/**
 ******************************************************************************
 *
 * @file asr_usb.h
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */

#ifndef _ASR_USB_H_
#define _ASR_USB_H_

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/usb.h>
#include <linux/vmalloc.h>
#include <linux/ktime.h>
#include "asr_defs.h"
#include "asr_hif.h"
#include "asr_utils.h"

#include "asr_bus.h"
#include "usb_rdl.h"
#include "usb.h"

struct intr_transfer_buf {
	u32 notification;
	u32 reserved;
};

struct asr_usbdev_info {
	struct asr_usbdev bus_pub;	/* MUST BE FIRST */
	spinlock_t qlock;
	struct list_head rx_freeq;
	struct list_head rx_postq;
	struct list_head tx_freeq;
	struct list_head tx_postq;
	uint rx_pipe, tx_pipe, intr_pipe, rx_pipe2;

	int rx_low_watermark;
	int tx_low_watermark;
	int tx_high_watermark;
	int tx_freecount;
	int rx_freecount;
	bool tx_flowblock;

	struct asr_usbreq *tx_reqs;
	struct asr_usbreq *rx_reqs;

	spinlock_t ctl_qlock;
	struct list_head ctl_tx_freeq;
	struct list_head ctl_tx_postq;

	struct asr_usbctlreq *ctl_tx_reqs;

	u8 *image;		/* buffer for combine fw and nvram */
	int image_len;

	struct usb_device *usbdev;
	struct device *dev;

	int ctl_in_pipe, ctl_out_pipe;
	struct urb *ctl_urb;	/* URB for control endpoint */
	struct usb_ctrlrequest ctl_write;
	struct usb_ctrlrequest ctl_read;
	u32 ctl_urb_actual_length;
	int ctl_urb_status;
	int ctl_completed;
	wait_queue_head_t ioctl_resp_wait;
	ulong ctl_op;

	struct urb *bulk_urb;	/* used for FW download */
	struct urb *intr_urb;	/* URB for interrupt endpoint */
	int intr_size;		/* Size of interrupt message */
	int interval;		/* Interrupt polling interval */
	struct intr_transfer_buf intr;	/* Data buffer for interrupt endpoint */
#ifdef CONFIG_ASR_USB_PM
	int autosuspend_delay; /* auto suspend delay in milliseconds */
	struct delayed_work pm_cmd_work;
	wait_queue_head_t pm_waitq;
#endif
};

// id num used in usb
#define RX_DATA_ID (0xFFFF)
#define RX_LOG_ID  (0x7FFF)
#define RX_DESC_ID (0x7FFE)

int asr_usb_send_cmd(struct asr_hw *asr_hw, u8 * src, u16 len, bool set);
int asr_usb_register_drv(void);
void asr_usb_exit(void);

#endif /* _ASR_USB_H_ */
