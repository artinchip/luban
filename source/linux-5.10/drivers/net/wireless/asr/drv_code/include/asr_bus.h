/**
 ******************************************************************************
 *
 * @file asr_bus.h
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */

#ifndef _ASR_BUS_H_
#define _ASR_BUS_H_

/* The level of bus communication with the dongle */
enum asr_bus_state {
	ASR_BUS_DOWN,		/* Not ready for frame transfers */
	ASR_BUS_LOAD,		/* Download access only (CPU reset) */
	ASR_BUS_DATA		/* Ready for frame transfers */
};

struct asr_bus_dcmd {
	char *name;
	char *param;
	int param_len;
	struct list_head list;
};

/**
 * struct asr_bus_ops - bus callback operations.
 *
 * @init: prepare for communication with dongle.
 * @stop: clear pending frames, disable data flow.
 * @txdata: send a data frame to the dongle (callee disposes skb).
 * @txctl: transmit a control request message to dongle.
 * @rxctl: receive a control response message from dongle.
 * @gettxq: obtain a reference of bus transmit queue (optional).
 *
 * This structure provides an abstract interface towards the
 * bus specific driver. For control messages to common driver
 * will assure there is only one active transaction. Unless
 * indicated otherwise these callbacks are mandatory.
 */
struct asr_bus_ops {
	int (*init) (struct device * dev);
	void (*stop) (struct device * dev);
	int (*txdata) (struct device * dev, struct sk_buff * skb);
	int (*txctl) (struct device * dev, unsigned char *msg, uint len);
	int (*rxctl) (struct device * dev, unsigned char *msg, uint len);
	struct pktq *(*gettxq) (struct device * dev);
};

/**
 * struct asr_bus - interface structure between common and bus layer
 *
 * @bus_priv: pointer to private bus device.
 * @dev: device pointer of bus device.
 * @drvr: public driver information.
 * @state: operational state of the bus interface.
 * @maxctl: maximum size for rxctl request message.
 * @tx_realloc: number of tx packets realloced for headroom.
 * @dstats: dongle-based statistical data.
 * @align: alignment requirement for the bus.
 * @dcmd_list: bus/device specific dongle initialization commands.
 * @chip: device identifier of the dongle chip.
 * @chiprev: revision of the dongle chip.
 */
struct asr_bus {
	union {
		//struct asr_sdio_dev *sdio;
		struct asr_usbdev *usb;
	} bus_priv;
	struct device *dev;
	//struct asr_pub *drvr;
	struct asr_hw *asr_hw;
	enum asr_bus_state state;
	uint maxctl;
	unsigned long tx_realloc;
	u8 align;
	u32 chip;
	u32 chiprev;
	struct list_head dcmd_list;

	struct asr_bus_ops *ops;
};

/*
 * callback wrappers
 */
static inline int asr_bus_init(struct asr_bus *bus)
{
	return bus->ops->init(bus->dev);
}

static inline void asr_bus_stop(struct asr_bus *bus)
{
	bus->ops->stop(bus->dev);
}

static inline int asr_bus_txdata(struct asr_bus *bus, struct sk_buff *skb)
{
	return bus->ops->txdata(bus->dev, skb);	//asr_usb_tx
}

static inline int asr_bus_txctl(struct asr_bus *bus, unsigned char *msg, uint len)
{
	return bus->ops->txctl(bus->dev, msg, len);
}

static inline int asr_bus_rxctl(struct asr_bus *bus, unsigned char *msg, uint len)
{
	return bus->ops->rxctl(bus->dev, msg, len);
}

static inline struct pktq *asr_bus_gettxq(struct asr_bus *bus)
{
	if (!bus->ops->gettxq)
		return ERR_PTR(-ENOENT);

	return bus->ops->gettxq(bus->dev);
}

/*
 * interface functions from common layer
 */

extern bool asr_c_prec_enq(struct device *dev, struct pktq *q, struct sk_buff *pkt, int prec);

/* Receive frame for delivery to OS.  Callee disposes of rxp. */
extern void asr_rx_frames(struct device *dev, struct sk_buff_head *rxlist);

/* Indication from bus module regarding presence/insertion of dongle. */
extern int asr_attach(uint bus_hdrlen, struct device *dev);
/* Indication from bus module regarding removal/absence of dongle */
extern void asr_detach(struct device *dev);
/* Indication from bus module that dongle should be reset */
extern void asr_dev_reset(struct device *dev);
/* Indication from bus module to change flow-control state */
extern void asr_txflowblock(struct device *dev, bool state);

/* Notify the bus has transferred the tx packet to firmware */
extern void asr_txcomplete(struct device *dev, struct sk_buff *txp, bool success);

extern int asr_bus_start(struct device *dev);

#ifdef CONFIG_ASRMAC_SDIO
extern void asr_sdio_exit(void);
extern void asr_sdio_init(void);
extern void asr_sdio_register(void);
#endif
#ifdef CONFIG_ASRMAC_USB
extern void asr_usb_exit(void);
extern void asr_usb_register(void);
#endif

#endif /* _ASR_BUS_H_ */
