/**
 ******************************************************************************
 *
 * @file usb.h
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */

#ifndef MAC_USB_H
#define MAC_USB_H

enum asr_usb_state {
	ASRMAC_USB_STATE_DOWN,
	ASRMAC_USB_STATE_DL_FAIL,
	ASRMAC_USB_STATE_DL_DONE,
	ASRMAC_USB_STATE_UP,
	ASRMAC_USB_STATE_SLEEP
};

#if 0
struct asr_stats {
	u32 tx_ctlpkts;
	u32 tx_ctlerrs;
	u32 rx_ctlpkts;
	u32 rx_ctlerrs;
};
#endif

struct asr_usbdev {
	struct asr_bus *bus;
	struct asr_usbdev_info *devinfo;
	volatile enum asr_usb_state state;
	struct asr_stats stats;
	int ntxq, nrxq, rxsize;
	u32 bus_mtu;
	int devid;
	int chiprev;		/* chip revsion number */
};

/* IO Request Block (IRB) */
struct asr_usbreq {
	struct list_head list;
	struct asr_usbdev_info *devinfo;
	struct urb *urb;
	struct sk_buff *skb;
};

/**
 * the members before urb must be same as struct asr_usbreq,
 * sometimes will use struct asr_usbreq instead of struct
 * asr_usbctlreq, such as asr_usb_deq and asr_usb_free_q
 */
struct asr_usbctlreq {
	struct list_head list;
	struct asr_usbdev_info *devinfo;
	struct urb *urb;
	struct usb_ctrlrequest ctl_req;
	int completed;
	wait_queue_head_t resp_wait;
};

#endif /* MAC_USB_H */
