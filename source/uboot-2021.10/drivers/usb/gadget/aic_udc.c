// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2021, ArtInChip Technology Co., Ltd
 */

#undef DEBUG
#include <common.h>
#include <clk.h>
#include <dm.h>
#include <generic-phy.h>
#include <log.h>
#include <malloc.h>
#include <reset.h>
#include <dm/device_compat.h>
#include <dm/devres.h>
#include <linux/bug.h>
#include <linux/delay.h>

#include <linux/errno.h>
#include <linux/list.h>

#include <linux/usb/ch9.h>
#include <linux/usb/otg.h>
#include <linux/usb/gadget.h>

#include <phys2bus.h>
#include <asm/byteorder.h>
#include <asm/unaligned.h>
#include <asm/io.h>
#include <cpu_func.h>
#include <linux/bug.h>
#include <power/regulator.h>

#include "aic_udc.h"

/***********************************************************/

#define DEBUG_SETUP 0
#define DEBUG_EP0 0
#define DEBUG_ISR 0
#define DEBUG_OUT_EP 0
#define DEBUG_IN_EP 0
#define DEBUG_DUMP 0

#define EP0_CON		0
#define EP_MASK		0xF

static char *state_names[] = {
	"WAIT_FOR_SETUP",
	"DATA_STATE_XMIT",
	"DATA_STATE_NEED_ZLP",
	"WAIT_FOR_OUT_STATUS",
	"DATA_STATE_RECV",
	"WAIT_FOR_COMPLETE",
	"WAIT_FOR_OUT_COMPLETE",
	"WAIT_FOR_IN_COMPLETE",
	"WAIT_FOR_NULL_COMPLETE",
};

static struct aic_udc	*the_controller;

static const char driver_name[] = "aic-udc";
static const char ep0name[] = "ep0-control";

/* Max packet size*/
static unsigned int ep0_fifo_size = 64;
static unsigned int ep_fifo_size =  512;
static unsigned int ep_fifo_size2 = 1024;
static int reset_available = 1;

static struct usb_ctrlrequest *usb_ctrl;
static dma_addr_t usb_ctrl_dma_addr;

/*
 * Local declarations.
 */
static int aic_ep_enable(struct usb_ep *ep,
			 const struct usb_endpoint_descriptor *);
static int aic_ep_disable(struct usb_ep *ep);
static struct usb_request *aic_alloc_request(struct usb_ep *ep,
					     gfp_t gfp_flags);
static void aic_free_request(struct usb_ep *ep, struct usb_request *);

static int aic_queue(struct usb_ep *ep, struct usb_request *, gfp_t gfp_flags);
static int aic_dequeue(struct usb_ep *ep, struct usb_request *);
static int aic_fifo_status(struct usb_ep *ep);
static void aic_fifo_flush(struct usb_ep *ep);
static void aic_ep0_read(struct aic_udc *dev);
static void aic_ep0_kick(struct aic_udc *dev, struct aic_ep *ep);
static void aic_handle_ep0(struct aic_udc *dev);
static int aic_ep0_write(struct aic_udc *dev);
static int write_fifo_ep0(struct aic_ep *ep, struct aic_request *req);
static void done(struct aic_ep *ep, struct aic_request *req, int status);
static void stop_activity(struct aic_udc *dev,
			  struct usb_gadget_driver *driver);
static int udc_enable(struct aic_udc *dev);
static void udc_set_address(struct aic_udc *dev, unsigned char address);
static void reconfig_usbd(struct aic_udc *dev);
static void set_max_pktsize(struct aic_udc *dev, enum usb_device_speed speed);
static void nuke(struct aic_ep *ep, int status);
static int aic_udc_set_halt(struct usb_ep *_ep, int value);
static void aic_udc_set_nak(struct aic_ep *ep);

void set_udc_gadget_private_data(void *p)
{
	debug_cond(DEBUG_SETUP != 0,
		   "%s: the_controller: 0x%p, p: 0x%p\n", __func__,
		   the_controller, p);
	the_controller->gadget.dev.device_data = p;
}

void *get_udc_gadget_private_data(struct usb_gadget *gadget)
{
	return gadget->dev.device_data;
}

static struct usb_ep_ops aic_ep_ops = {
	.enable = aic_ep_enable,
	.disable = aic_ep_disable,

	.alloc_request = aic_alloc_request,
	.free_request = aic_free_request,

	.queue = aic_queue,
	.dequeue = aic_dequeue,

	.set_halt = aic_udc_set_halt,
	.fifo_status = aic_fifo_status,
	.fifo_flush = aic_fifo_flush,
};

#define create_proc_files() do {} while (0)
#define remove_proc_files() do {} while (0)

/***********************************************************/

static struct aic_udc_reg *reg;

bool dfu_usb_get_reset(void)
{
	return !!(readl(&reg->usbintsts) & INT_RESET);
}

__weak void udc_phy_init(struct aic_udc *dev) {}
__weak void udc_phy_off(struct aic_udc *dev) {}

/***********************************************************/

static u8 clear_feature_num;
int clear_feature_flag;

/* Bulk-Only Mass Storage Reset (class-specific request) */
#define GET_MAX_LUN_REQUEST	0xFE
#define BOT_RESET_REQUEST	0xFF

static inline void aic_udc_ep0_zlp(struct aic_udc *dev)
{
	u32 ep_ctrl;

	writel(phys_to_bus((unsigned long)usb_ctrl_dma_addr),
	       &reg->inepdmaaddr[EP0_CON]);
	writel(DIEPT_SIZ_PKT_CNT(1), &reg->ineptsfsiz[EP0_CON]);

	ep_ctrl = readl(&reg->inepcfg[EP0_CON]);
	writel(ep_ctrl | DEPCTL_EPENA | DEPCTL_CNAK,
	       &reg->inepcfg[EP0_CON]);

	debug_cond(DEBUG_EP0 != 0, "%s:EP0 ZLP DIEPDMA0 = 0x%x\n",
		   __func__, readl(&reg->inepdmaaddr[EP0_CON]));
	debug_cond(DEBUG_EP0 != 0, "%s:EP0 ZLP DIEPCTL0 = 0x%x\n",
		   __func__, readl(&reg->inepcfg[EP0_CON]));
	dev->ep0state = WAIT_FOR_IN_COMPLETE;
}

static void aic_udc_pre_setup(void)
{
	u32 ep_ctrl;

	debug_cond(DEBUG_IN_EP,
		   "%s : Prepare Setup packets.\n", __func__);

	writel(DOEPT_SIZ_PKT_CNT(1) | sizeof(struct usb_ctrlrequest),
	       &reg->outeptsfsiz[EP0_CON]);
	writel(phys_to_bus((unsigned long)usb_ctrl_dma_addr),
	       &reg->outepdmaaddr[EP0_CON]);

	ep_ctrl = readl(&reg->outepcfg[EP0_CON]);
	writel(ep_ctrl | DEPCTL_EPENA, &reg->outepcfg[EP0_CON]);

	debug_cond(DEBUG_EP0 != 0, "%s:EP0 ZLP DOEPDMA0 = 0x%x\n",
		   __func__, readl(&reg->outepdmaaddr[EP0_CON]));
	debug_cond(DEBUG_EP0 != 0, "%s:EP0 ZLP DIEPCTL0 = 0x%x\n",
		   __func__, readl(&reg->inepcfg[EP0_CON]));
	debug_cond(DEBUG_EP0 != 0, "%s:EP0 ZLP DOEPCTL0 = 0x%x\n",
		   __func__, readl(&reg->outepcfg[EP0_CON]));
}

static inline void aic_ep0_complete_out(void)
{
	u32 ep_ctrl;

	debug_cond(DEBUG_EP0 != 0, "%s:EP0 ZLP DIEPCTL0 = 0x%x\n",
		   __func__, readl(&reg->inepcfg[EP0_CON]));
	debug_cond(DEBUG_EP0 != 0, "%s:EP0 ZLP DOEPCTL0 = 0x%x\n",
		   __func__, readl(&reg->outepcfg[EP0_CON]));

	debug_cond(DEBUG_IN_EP,
		   "%s : Prepare Complete Out packet.\n", __func__);

	writel(DOEPT_SIZ_PKT_CNT(1) | sizeof(struct usb_ctrlrequest),
	       &reg->outeptsfsiz[EP0_CON]);
	writel(phys_to_bus((unsigned long)usb_ctrl_dma_addr),
	       &reg->outepdmaaddr[EP0_CON]);

	ep_ctrl = readl(&reg->outepcfg[EP0_CON]);
	writel(ep_ctrl | DEPCTL_EPENA | DEPCTL_CNAK,
	       &reg->outepcfg[EP0_CON]);

	debug_cond(DEBUG_EP0 != 0, "%s:EP0 ZLP DIEPCTL0 = 0x%x\n",
		   __func__, readl(&reg->inepcfg[EP0_CON]));
	debug_cond(DEBUG_EP0 != 0, "%s:EP0 ZLP DOEPCTL0 = 0x%x\n",
		   __func__, readl(&reg->outepcfg[EP0_CON]));
}

static int setdma_rx(struct aic_ep *ep, struct aic_request *req)
{
	u32 *buf, ctrl;
	u32 length, pktcnt;
	u32 ep_num = ep_index(ep);

	buf = req->req.buf + req->req.actual;
	length = min_t(u32, req->req.length - req->req.actual,
		       ep_num ? DMA_BUFFER_SIZE : ep->ep.maxpacket);

	ep->len = length;
	ep->dma_buf = buf;

	if (ep_num == EP0_CON || length == 0)
		pktcnt = 1;
	else
		pktcnt = (length - 1) / (ep->ep.maxpacket) + 1;

	ctrl =  readl(&reg->outepcfg[ep_num]);

	invalidate_dcache_range((unsigned long)ep->dma_buf,
				(unsigned long)ep->dma_buf +
				ROUND(ep->len, CONFIG_SYS_CACHELINE_SIZE));

	writel(phys_to_bus((unsigned long)ep->dma_buf),
	       &reg->outepdmaaddr[ep_num]);
	writel(DOEPT_SIZ_PKT_CNT(pktcnt) | DOEPT_SIZ_XFER_SIZE(length),
	       &reg->outeptsfsiz[ep_num]);
	writel(DEPCTL_EPENA | DEPCTL_CNAK | ctrl, &reg->outepcfg[ep_num]);

	debug_cond(DEBUG_OUT_EP != 0,
		   "%s: EP%d RX DMA start : DOEPDMA = 0x%x,"
		   "DOEPTSIZ = 0x%x, DOEPCTL = 0x%x\n"
		   "\tbuf = 0x%p, pktcnt = %d, xfersize = %d\n",
		   __func__, ep_num,
		   readl(&reg->outepdmaaddr[ep_num]),
		   readl(&reg->outeptsfsiz[ep_num]),
		   readl(&reg->outepcfg[ep_num]),
		   buf, pktcnt, length);
	return 0;
}

static int setdma_tx(struct aic_ep *ep, struct aic_request *req)
{
	u32 *buf, ctrl = 0;
	u32 length, pktcnt;
	u32 ep_num = ep_index(ep);
	u32 uTemp;

	buf = req->req.buf + req->req.actual;
	length = req->req.length - req->req.actual;

	if (ep_num == EP0_CON)
		length = min(length, (u32)ep_maxpacket(ep));

	ep->len = length;
	ep->dma_buf = buf;

	flush_dcache_range((unsigned long)ep->dma_buf,
			   (unsigned long)ep->dma_buf +
			   ROUND(ep->len, CONFIG_SYS_CACHELINE_SIZE));

	if (length == 0)
		pktcnt = 1;
	else
		pktcnt = (length - 1) / (ep->ep.maxpacket) + 1;

	/* Flush the endpoint's Tx FIFO */
	uTemp = readl(&reg->usbdevinit);
	writel(uTemp | USBDEVINIT_TXFNUM(ep->fifo_num), &reg->usbdevinit);
	writel(uTemp | USBDEVINIT_TXFNUM(ep->fifo_num) | USBDEVINIT_TXFFLSH,
	       &reg->usbdevinit);
	while (readl(&reg->usbdevinit) & USBDEVINIT_TXFFLSH)
		;

	writel(phys_to_bus((unsigned long)ep->dma_buf),
	       &reg->inepdmaaddr[ep_num]);
	writel(DIEPT_SIZ_PKT_CNT(pktcnt) | DIEPT_SIZ_XFER_SIZE(length),
	       &reg->ineptsfsiz[ep_num]);

	ctrl = readl(&reg->inepcfg[ep_num]);

	/* Write the FIFO number to be used for this endpoint */
	ctrl &= DIEPCTL_TX_FIFO_NUM_MASK;
	ctrl |= DIEPCTL_TX_FIFO_NUM(ep->fifo_num);

	debug_cond(DEBUG_IN_EP, "%s, DIEPCTL <= 0x%x\n", __func__,
		   DEPCTL_EPENA | DEPCTL_CNAK | ctrl);
	writel(DEPCTL_EPENA | DEPCTL_CNAK | ctrl, &reg->inepcfg[ep_num]);

	debug_cond(DEBUG_IN_EP,
		"%s:EP%d TX DMA start : DIEPDMA0 = 0x%x,"
		"DIEPTSIZ0 = 0x%x, DIEPCTL0 = 0x%x\n"
		"\tbuf = 0x%p, pktcnt = %d, xfersize = %d\n",
		__func__, ep_num,
		readl(&reg->inepdmaaddr[ep_num]),
		readl(&reg->ineptsfsiz[ep_num]),
		readl(&reg->inepcfg[ep_num]),
		buf, pktcnt, length);

	return length;
}

static void complete_rx(struct aic_udc *dev, u8 ep_num)
{
	struct aic_ep *ep = &dev->ep[ep_num];
	struct aic_request *req = NULL;
	u32 ep_tsr = 0, xfer_size = 0, is_short = 0;

	if (list_empty(&ep->queue)) {
		debug_cond(DEBUG_OUT_EP != 0,
			   "%s: RX DMA done : NULL REQ on OUT EP-%d\n",
			   __func__, ep_num);
		return;
	}

	req = list_entry(ep->queue.next, struct aic_request, queue);
	ep_tsr = readl(&reg->outeptsfsiz[ep_num]);

	if (ep_num == EP0_CON)
		xfer_size = (ep_tsr & DOEPT_SIZ_XFER_SIZE_MAX_EP0);
	else
		xfer_size = (ep_tsr & DOEPT_SIZ_XFER_SIZE_MAX_EP);

	xfer_size = ep->len - xfer_size;

	/*
	 * NOTE:
	 *
	 * Please be careful with proper buffer allocation for USB request,
	 * which needs to be aligned to CONFIG_SYS_CACHELINE_SIZE, not only
	 * with starting address, but also its size shall be a cache line
	 * multiplication.
	 *
	 * This will prevent from corruption of data allocated immediately
	 * before or after the buffer.
	 *
	 * For armv7, the cache_v7.c provides proper code to emit "ERROR"
	 * message to warn users.
	 */
	invalidate_dcache_range((unsigned long)ep->dma_buf,
				(unsigned long)ep->dma_buf +
				ROUND(xfer_size, CONFIG_SYS_CACHELINE_SIZE));

	req->req.actual += min(xfer_size, req->req.length - req->req.actual);
	is_short = !!(xfer_size % ep->ep.maxpacket);

	debug_cond(DEBUG_OUT_EP != 0,
		   "%s: RX DMA done : ep = %d, rx bytes = %d/%d, "
		   "is_short = %d, DOEPTSIZ = 0x%x, remained bytes = %d\n",
		   __func__, ep_num, req->req.actual, req->req.length,
		   is_short, ep_tsr, req->req.length - req->req.actual);

	if (is_short || req->req.actual == req->req.length) {
		if (ep_num == EP0_CON && dev->ep0state == DATA_STATE_RECV) {
			debug_cond(DEBUG_OUT_EP != 0, "	=> Send ZLP\n");
			aic_udc_ep0_zlp(dev);
			/* packet will be completed in complete_tx() */
			dev->ep0state = WAIT_FOR_IN_COMPLETE;
		} else {
			done(ep, req, 0);

			if (!list_empty(&ep->queue)) {
				req = list_entry(ep->queue.next,
						 struct aic_request, queue);
				debug_cond(DEBUG_OUT_EP != 0,
					   "%s: Next Rx request start...\n",
					   __func__);
				setdma_rx(ep, req);
			}
		}
	} else {
		setdma_rx(ep, req);
	}
}

static void complete_tx(struct aic_udc *dev, u8 ep_num)
{
	struct aic_ep *ep = &dev->ep[ep_num];
	struct aic_request *req;
	u32 ep_tsr = 0, xfer_size = 0, is_short = 0;
	u32 last;

	if (dev->ep0state == WAIT_FOR_NULL_COMPLETE) {
		dev->ep0state = WAIT_FOR_OUT_COMPLETE;
		aic_ep0_complete_out();
		return;
	}

	if (list_empty(&ep->queue)) {
		debug_cond(DEBUG_IN_EP,
			   "%s: TX DMA done : NULL REQ on IN EP-%d\n",
			   __func__, ep_num);
		return;
	}

	req = list_entry(ep->queue.next, struct aic_request, queue);

	ep_tsr = readl(&reg->ineptsfsiz[ep_num]);

	xfer_size = ep->len;
	is_short = (xfer_size < ep->ep.maxpacket);
	req->req.actual += min(xfer_size, req->req.length - req->req.actual);

	debug_cond(DEBUG_IN_EP,
		   "%s: TX DMA done : ep = %d, tx bytes = %d/%d, "
		   "is_short = %d, DIEPTSIZ = 0x%x, remained bytes = %d\n",
		   __func__, ep_num, req->req.actual, req->req.length,
		   is_short, ep_tsr, req->req.length - req->req.actual);

	if (ep_num == 0) {
		if (dev->ep0state == DATA_STATE_XMIT) {
			debug_cond(DEBUG_IN_EP,
				   "%s: ep_num = %d, ep0stat =="
				   "DATA_STATE_XMIT\n",
				   __func__, ep_num);
			last = write_fifo_ep0(ep, req);
			if (last)
				dev->ep0state = WAIT_FOR_COMPLETE;
		} else if (dev->ep0state == WAIT_FOR_IN_COMPLETE) {
			debug_cond(DEBUG_IN_EP,
				   "%s: ep_num = %d, completing request\n",
				    __func__, ep_num);
			done(ep, req, 0);
			dev->ep0state = WAIT_FOR_SETUP;
		} else if (dev->ep0state == WAIT_FOR_COMPLETE) {
			debug_cond(DEBUG_IN_EP,
				   "%s: ep_num = %d, completing request\n",
				   __func__, ep_num);
			done(ep, req, 0);
			dev->ep0state = WAIT_FOR_OUT_COMPLETE;
			aic_ep0_complete_out();
		} else {
			debug_cond(DEBUG_IN_EP,
				   "%s: ep_num = %d, invalid ep state\n",
				   __func__, ep_num);
		}
		return;
	}

	if (req->req.actual == req->req.length)
		done(ep, req, 0);

	if (!list_empty(&ep->queue)) {
		req = list_entry(ep->queue.next, struct aic_request, queue);
		debug_cond(DEBUG_IN_EP,
			   "%s: Next Tx request start...\n", __func__);
		setdma_tx(ep, req);
	}
}

static inline void aic_udc_check_tx_queue(struct aic_udc *dev, u8 ep_num)
{
	struct aic_ep *ep = &dev->ep[ep_num];
	struct aic_request *req;

	debug_cond(DEBUG_IN_EP,
		   "%s: Check queue, ep_num = %d\n", __func__, ep_num);

	if (!list_empty(&ep->queue)) {
		req = list_entry(ep->queue.next, struct aic_request, queue);
		debug_cond(DEBUG_IN_EP,
			   "%s: Next Tx request(0x%p) start...\n",
			   __func__, req);

		if (ep_is_in(ep))
			setdma_tx(ep, req);
		else
			setdma_rx(ep, req);
	} else {
		debug_cond(DEBUG_IN_EP,
			   "%s: NULL REQ on IN EP-%d\n", __func__, ep_num);

		return;
	}
}

static void process_ep_in_intr(struct aic_udc *dev)
{
	u32 ep_intr, ep_intr_status;
	u8 ep_num = 0;

	ep_intr = readl(&reg->usbepint);
	debug_cond(DEBUG_IN_EP,
		"*** %s: EP In interrupt : DAINT = 0x%x\n", __func__, ep_intr);

	ep_intr &= DAINT_MASK;

	while (ep_intr) {
		if (ep_intr & DAINT_IN_EP_INT(1)) {
			ep_intr_status = readl(&reg->inepint[ep_num]);
			debug_cond(DEBUG_IN_EP,
				   "\tEP%d-IN : DIEPINT = 0x%x\n",
				   ep_num, ep_intr_status);

			/* Interrupt Clear */
			writel(ep_intr_status, &reg->inepint[ep_num]);

			if (ep_intr_status & TRANSFER_DONE) {
				complete_tx(dev, ep_num);

				if (ep_num == 0) {
					if (dev->ep0state ==
					    WAIT_FOR_IN_COMPLETE)
						dev->ep0state = WAIT_FOR_SETUP;

					if (dev->ep0state == WAIT_FOR_SETUP)
						aic_udc_pre_setup();

					/* continue transfer after
					 * set_clear_halt for DMA mode */
					if (clear_feature_flag == 1) {
						aic_udc_check_tx_queue(dev,
							clear_feature_num);
						clear_feature_flag = 0;
					}
				}
			}
		}
		ep_num++;
		ep_intr >>= 1;
	}
}

static void process_ep_out_intr(struct aic_udc *dev)
{
	u32 ep_intr, ep_intr_status;
	u8 ep_num = 0;
	u32 ep_tsr = 0, xfer_size = 0;
	u32 epsiz_reg = reg->outeptsfsiz[ep_num];
	u32 req_size = sizeof(struct usb_ctrlrequest);

	ep_intr = readl(&reg->usbepint);
	debug_cond(DEBUG_OUT_EP != 0,
		   "*** %s: EP OUT interrupt : DAINT = 0x%x\n",
		   __func__, ep_intr);

	ep_intr = (ep_intr >> DAINT_OUT_BIT) & DAINT_MASK;

	while (ep_intr) {
		if (ep_intr & 0x1) {
			ep_intr_status = readl(&reg->outepint[ep_num]);
			debug_cond(DEBUG_OUT_EP != 0,
				   "\tEP%d-OUT : DOEPINT = 0x%x\n",
				   ep_num, ep_intr_status);

			/* Interrupt Clear */
			writel(ep_intr_status, &reg->outepint[ep_num]);

			if (ep_num == 0) {
				if (ep_intr_status & TRANSFER_DONE) {
					ep_tsr = readl(&epsiz_reg);
					xfer_size = ep_tsr &
						   DOEPT_SIZ_XFER_SIZE_MAX_EP0;

					if (xfer_size == req_size &&
					    dev->ep0state == WAIT_FOR_SETUP) {
						aic_udc_pre_setup();
					} else if (dev->ep0state !=
						   WAIT_FOR_OUT_COMPLETE) {
						complete_rx(dev, ep_num);
					} else {
						dev->ep0state = WAIT_FOR_SETUP;
						aic_udc_pre_setup();
					}
				}

				if (ep_intr_status &
				    CTRL_OUT_EP_SETUP_PHASE_DONE) {
					debug_cond(DEBUG_OUT_EP != 0,
						   "SETUP packet arrived\n");
					aic_handle_ep0(dev);
				}
			} else {
				if (ep_intr_status & TRANSFER_DONE)
					complete_rx(dev, ep_num);
			}
		}
		ep_num++;
		ep_intr >>= 1;
	}
}

/*
 *	usb client interrupt handler.
 */
static int aic_udc_irq(int irq, void *_dev)
{
	struct aic_udc *dev = _dev;
	u32 intr_status;
	u32 usb_status, gintmsk;
	unsigned long flags = 0;

	spin_lock_irqsave(&dev->lock, flags);

	intr_status = readl(&reg->usbintsts);
	gintmsk = readl(&reg->usbintmsk);

	debug_cond(DEBUG_ISR,
		  "\n*** %s : GINTSTS=0x%x(on state %s), GINTMSK : 0x%x,"
		  "DAINT : 0x%x, DAINTMSK : 0x%x\n",
		  __func__, intr_status, state_names[dev->ep0state], gintmsk,
		  readl(&reg->usbepint), readl(&reg->usbepintmsk));

	if (!intr_status) {
		spin_unlock_irqrestore(&dev->lock, flags);
		return IRQ_HANDLED;
	}

	if (intr_status & INT_ENUMDONE) {
		debug_cond(DEBUG_ISR, "\tSpeed Detection interrupt\n");

		writel(INT_ENUMDONE, &reg->usbintsts);
		usb_status = (readl(&reg->usblinests) & 0x6);

		if (usb_status & (USB_FULL_30_60MHZ | USB_FULL_48MHZ)) {
			debug_cond(DEBUG_ISR,
				   "\t\tFull Speed Detection\n");
			set_max_pktsize(dev, USB_SPEED_FULL);

		} else {
			debug_cond(DEBUG_ISR,
				   "\t\tHigh Speed Detection : 0x%x\n",
				usb_status);
			set_max_pktsize(dev, USB_SPEED_HIGH);
		}
	}

	if (intr_status & INT_EARLY_SUSPEND) {
		debug_cond(DEBUG_ISR, "\tEarly suspend interrupt\n");
		writel(INT_EARLY_SUSPEND, &reg->usbintsts);
	}

	if (intr_status & INT_SUSPEND) {
		usb_status = readl(&reg->usblinests);
		debug_cond(DEBUG_ISR,
			   "\tSuspend interrupt :(DSTS):0x%x\n", usb_status);
		writel(INT_SUSPEND, &reg->usbintsts);

		if (dev->gadget.speed != USB_SPEED_UNKNOWN &&
		    dev->driver) {
			if (dev->driver->suspend)
				dev->driver->suspend(&dev->gadget);
		}
	}

	if (intr_status & INT_RESUME) {
		debug_cond(DEBUG_ISR, "\tResume interrupt\n");
		writel(INT_RESUME, &reg->usbintsts);

		if (dev->gadget.speed != USB_SPEED_UNKNOWN &&
		    dev->driver &&
		    dev->driver->resume) {
			dev->driver->resume(&dev->gadget);
		}
	}

	if (intr_status & INT_RESET) {
		debug_cond(DEBUG_ISR,
			"\tReset interrupt - (intr_status):0x%x\n",
			intr_status);
		writel(INT_RESET, &reg->usbintsts);
		if (reset_available) {
			debug_cond(DEBUG_ISR,
				   "\t\tUDC core got reset (%d)!!\n",
				   reset_available);
			reconfig_usbd(dev);
			dev->ep0state = WAIT_FOR_SETUP;
			reset_available = 0;
			aic_udc_pre_setup();
		} else {
			reset_available = 1;
		}
	}

	if (intr_status & INT_IN_EP)
		process_ep_in_intr(dev);

	if (intr_status & INT_OUT_EP)
		process_ep_out_intr(dev);

	spin_unlock_irqrestore(&dev->lock, flags);

	return IRQ_HANDLED;
}

static int aic_handle_unaligned_req(struct aic_ep *a_ep,
				    struct aic_request *a_req)
{
	void *req_buf = a_req->req.buf;

	if (!((long)req_buf % CONFIG_SYS_CACHELINE_SIZE) &&
	    !(a_req->req.length % CONFIG_SYS_CACHELINE_SIZE))
		return 0;

	a_req->req.buf = memalign(CONFIG_SYS_CACHELINE_SIZE,
				  ROUND(a_req->req.length,
					CONFIG_SYS_CACHELINE_SIZE));

	if (!a_req->req.buf) {
		a_req->req.buf = req_buf;
		debug("%s: unable to allocate memory for bounce buffer\n",
		      __func__);
		return -ENOMEM;
	}

	/* Save actual buffer */
	a_req->saved_req_buf = req_buf;

	if (ep_is_in(a_ep))
		memcpy(a_req->req.buf, req_buf, a_req->req.length);
	return 0;
}

static void aic_handle_unaligned_req_complete(struct aic_ep *a_ep,
					      struct aic_request *a_req)
{
	/* If buffer was aligned */
	if (!a_req->saved_req_buf)
		return;

	debug("%s: %s: status=%d actual-length=%d\n", __func__,
	      a_ep->ep.name, a_req->req.status, a_req->req.actual);

	/* Copy data from bounce buffer on successful out transfer */
	if (!ep_is_in(a_ep) && !a_req->req.status)
		memcpy(a_req->saved_req_buf, a_req->req.buf,
		       a_req->req.actual);

	/* Free bounce buffer */
	kfree(a_req->req.buf);

	a_req->req.buf = a_req->saved_req_buf;
	a_req->saved_req_buf = NULL;
}

/** Queue one request
 *  Kickstart transfer if needed
 */
static int aic_queue(struct usb_ep *_ep, struct usb_request *_req,
		     gfp_t gfp_flags)
{
	struct aic_request *req;
	struct aic_ep *ep;
	struct aic_udc *dev;
	unsigned long flags = 0;
	u32 ep_num, gintsts;

	req = container_of(_req, struct aic_request, req);
	if (unlikely(!_req || !_req->complete || !_req->buf ||
		     !list_empty(&req->queue))) {
		debug("%s: bad params\n", __func__);
		return -EINVAL;
	}

	ep = container_of(_ep, struct aic_ep, ep);

	if (unlikely(!_ep || (!ep->desc && ep->ep.name != ep0name))) {
		debug("%s: bad ep: %s, %d, %p\n", __func__,
		      ep->ep.name, !ep->desc, _ep);
		return -EINVAL;
	}

	ep_num = ep_index(ep);
	dev = ep->dev;
	if (unlikely(!dev->driver || dev->gadget.speed == USB_SPEED_UNKNOWN)) {
		debug("%s: bogus device state %p\n", __func__, dev->driver);
		return -ESHUTDOWN;
	}

	spin_lock_irqsave(&dev->lock, flags);

	aic_handle_unaligned_req(ep, req);

	_req->status = -EINPROGRESS;
	_req->actual = 0;

	/* kickstart this i/o queue? */
	debug("\n*** %s: %s-%s req = %p, len = %d, buf = %p"
		"Q empty = %d, stopped = %d\n",
		__func__, _ep->name, ep_is_in(ep) ? "in" : "out",
		_req, _req->length, _req->buf,
		list_empty(&ep->queue), ep->stopped);

#ifdef DEBUG
	{
		int i, len = _req->length;

		printf("pkt = ");
		if (len > 64)
			len = 64;
		for (i = 0; i < len; i++) {
			printf("%02x", ((u8 *)_req->buf)[i]);
			if ((i & 7) == 7)
				printf(" ");
		}
		printf("\n");
	}
#endif

	if (list_empty(&ep->queue) && !ep->stopped) {
		if (ep_num == 0) {
			/* EP0 */
			list_add_tail(&req->queue, &ep->queue);
			aic_ep0_kick(dev, ep);
			req = 0;
		} else if (ep_is_in(ep)) {
			gintsts = readl(&reg->usbintsts);
			debug_cond(DEBUG_IN_EP,
				   "%s: ep_is_in, AIC_UDC_GINTSTS=0x%x\n",
				   __func__, gintsts);

			setdma_tx(ep, req);
		} else {
			gintsts = readl(&reg->usbintsts);
			debug_cond(DEBUG_OUT_EP != 0,
				   "%s:ep_is_out, AIC_UDC_GINTSTS=0x%x\n",
				   __func__, gintsts);

			setdma_rx(ep, req);
		}
	}

	/* pio or dma irq handler advances the queue. */
	if (likely(req != 0))
		list_add_tail(&req->queue, &ep->queue);

	spin_unlock_irqrestore(&dev->lock, flags);

	return 0;
}

/****************************************************************/
/* End Point 0 related functions                                */
/****************************************************************/

/* return:  0 = still running, 1 = completed, negative = errno */
static int write_fifo_ep0(struct aic_ep *ep, struct aic_request *req)
{
	u32 max;
	unsigned int count;
	int is_last;

	max = ep_maxpacket(ep);

	debug_cond(DEBUG_EP0 != 0, "%s: max = %d\n", __func__, max);

	count = setdma_tx(ep, req);

	/* last packet is usually short (or a zlp) */
	if (likely(count != max)) {
		is_last = 1;
	} else {
		if (likely(req->req.length != req->req.actual + count) ||
		    req->req.zero)
			is_last = 0;
		else
			is_last = 1;
	}

	debug_cond(DEBUG_EP0 != 0,
		   "%s: wrote %s %d bytes%s %d left %p\n", __func__,
		   ep->ep.name, count,
		   is_last ? "/L" : "",
		   req->req.length - req->req.actual - count, req);

	/* requests complete when all IN data is in the FIFO */
	if (is_last) {
		ep->dev->ep0state = WAIT_FOR_SETUP;
		return 1;
	}

	return 0;
}

static int aic_fifo_read(struct aic_ep *ep, void *cp, int max)
{
	invalidate_dcache_range((unsigned long)cp, (unsigned long)cp +
				ROUND(max, CONFIG_SYS_CACHELINE_SIZE));

	debug_cond(DEBUG_EP0 != 0,
		   "%s: bytes=%d, ep_index=%d 0x%p\n", __func__,
		   max, ep_index(ep), cp);

	return max;
}

/**
 * udc_set_address - set the USB address for this device
 * @address:
 *
 * Called from control endpoint function
 * after it decodes a set address setup packet.
 */
static void udc_set_address(struct aic_udc *dev, unsigned char address)
{
	u32 ctrl = readl(&reg->usbdevconf);

	writel(DEVICE_ADDRESS(address) | ctrl, &reg->usbdevconf);

	aic_udc_ep0_zlp(dev);

	debug_cond(DEBUG_EP0 != 0,
		   "%s: USB 2.0 Device address=%d, DCFG=0x%x\n",
		   __func__, address, readl(&reg->usbdevconf));

	dev->usb_address = address;
}

static inline void aic_udc_ep0_set_stall(struct aic_ep *ep)
{
	struct aic_udc *dev;
	u32		ep_ctrl = 0;

	dev = ep->dev;
	ep_ctrl = readl(&reg->inepcfg[EP0_CON]);

	/* set the disable and stall bits */
	if (ep_ctrl & DEPCTL_EPENA)
		ep_ctrl |= DEPCTL_EPDIS;

	ep_ctrl |= DEPCTL_STALL;

	writel(ep_ctrl, &reg->inepcfg[EP0_CON]);

	debug_cond(DEBUG_EP0 != 0,
		   "%s: set ep%d stall, DIEPCTL0 = 0x%p\n",
		   __func__, ep_index(ep), &reg->inepcfg[EP0_CON]);
	/*
	 * The application can only set this bit, and the core clears it,
	 * when a SETUP token is received for this endpoint
	 */
	dev->ep0state = WAIT_FOR_SETUP;

	aic_udc_pre_setup();
}

static void aic_ep0_read(struct aic_udc *dev)
{
	struct aic_request *req;
	struct aic_ep *ep = &dev->ep[0];

	if (!list_empty(&ep->queue)) {
		req = list_entry(ep->queue.next, struct aic_request, queue);

	} else {
		debug("%s: ---> BUG\n", __func__);
		BUG();
		return;
	}

	debug_cond(DEBUG_EP0 != 0,
		   "%s: req = %p, req.length = 0x%x, req.actual = 0x%x\n",
		   __func__, req, req->req.length, req->req.actual);

	if (req->req.length == 0) {
		/* zlp for Set_configuration, Set_interface,
		 * or Bulk-Only mass storge reset
		 */

		ep->len = 0;
		aic_udc_ep0_zlp(dev);

		debug_cond(DEBUG_EP0 != 0,
			   "%s: req.length = 0, bRequest = %d\n",
			   __func__, usb_ctrl->bRequest);
		return;
	}

	setdma_rx(ep, req);
}

/*
 * DATA_STATE_XMIT
 */
static int aic_ep0_write(struct aic_udc *dev)
{
	struct aic_request *req;
	struct aic_ep *ep = &dev->ep[0];
	int ret, need_zlp = 0;

	if (list_empty(&ep->queue))
		req = 0;
	else
		req = list_entry(ep->queue.next, struct aic_request, queue);

	if (!req) {
		debug_cond(DEBUG_EP0 != 0, "%s: NULL REQ\n", __func__);
		return 0;
	}

	debug_cond(DEBUG_EP0 != 0,
		   "%s: req = %p, req.length = 0x%x, req.actual = 0x%x\n",
		   __func__, req, req->req.length, req->req.actual);

	if (req->req.length - req->req.actual == ep0_fifo_size) {
		/* Next write will end with the packet size, */
		/* so we need Zero-length-packet */
		need_zlp = 1;
	}

	ret = write_fifo_ep0(ep, req);

	if (ret == 1 && !need_zlp) {
		/* Last packet */
		dev->ep0state = WAIT_FOR_COMPLETE;
		debug_cond(DEBUG_EP0 != 0,
			   "%s: finished, waiting for status\n", __func__);

	} else {
		dev->ep0state = DATA_STATE_XMIT;
		debug_cond(DEBUG_EP0 != 0,
			   "%s: not finished\n", __func__);
	}

	return 1;
}

static int aic_udc_get_status(struct aic_udc *dev,
			      struct usb_ctrlrequest *crq)
{
	u8 ep_num = crq->wIndex & 0x7F;
	u16 g_status = 0;
	u32 ep_ctrl;

	debug_cond(DEBUG_SETUP != 0,
		   "%s: *** USB_REQ_GET_STATUS\n", __func__);
	debug("crq->brequest:0x%x\n", crq->bRequestType & USB_RECIP_MASK);
	switch (crq->bRequestType & USB_RECIP_MASK) {
	case USB_RECIP_INTERFACE:
		g_status = 0;
		debug_cond(DEBUG_SETUP != 0,
			   "\tGET_STATUS:USB_RECIP_INTERFACE, g_stauts = %d\n",
			   g_status);
		break;

	case USB_RECIP_DEVICE:
		g_status = 0x1; /* Self powered */
		debug_cond(DEBUG_SETUP != 0,
			   "\tGET_STATUS: USB_RECIP_DEVICE, g_stauts = %d\n",
			   g_status);
		break;

	case USB_RECIP_ENDPOINT:
		if (crq->wLength > 2) {
			debug_cond(DEBUG_SETUP != 0,
				   "\tGET_STATUS:Not support EP or wLength\n");
			return 1;
		}

		g_status = dev->ep[ep_num].stopped;
		debug_cond(DEBUG_SETUP != 0,
			   "\tGET_STATUS: USB_RECIP_ENDPOINT, g_stauts = %d\n",
			   g_status);

		break;

	default:
		return 1;
	}

	memcpy(usb_ctrl, &g_status, sizeof(g_status));

	flush_dcache_range((unsigned long)usb_ctrl,
			   (unsigned long)usb_ctrl +
			   ROUND(sizeof(g_status), CONFIG_SYS_CACHELINE_SIZE));

	writel(phys_to_bus(usb_ctrl_dma_addr), &reg->inepdmaaddr[EP0_CON]);
	writel(DIEPT_SIZ_PKT_CNT(1) | DIEPT_SIZ_XFER_SIZE(2),
	       &reg->ineptsfsiz[EP0_CON]);

	ep_ctrl = readl(&reg->inepcfg[EP0_CON]);
	writel(ep_ctrl | DEPCTL_EPENA | DEPCTL_CNAK,
	       &reg->inepcfg[EP0_CON]);
	dev->ep0state = WAIT_FOR_NULL_COMPLETE;

	return 0;
}

static void aic_udc_set_nak(struct aic_ep *ep)
{
	u8		ep_num;
	u32		ep_ctrl = 0;

	ep_num = ep_index(ep);
	debug("%s: ep_num = %d, ep_type = %d\n", __func__, ep_num, ep->ep_type);

	if (ep_is_in(ep)) {
		ep_ctrl = readl(&reg->inepcfg[ep_num]);
		ep_ctrl |= DEPCTL_SNAK;
		writel(ep_ctrl, &reg->inepcfg[ep_num]);
		debug("%s: set NAK, DIEPCTL%d = 0x%x\n",
		      __func__, ep_num, readl(&reg->inepcfg[ep_num]));
	} else {
		ep_ctrl = readl(&reg->outepcfg[ep_num]);
		ep_ctrl |= DEPCTL_SNAK;
		writel(ep_ctrl, &reg->outepcfg[ep_num]);
		debug("%s: set NAK, DOEPCTL%d = 0x%x\n",
		      __func__, ep_num, readl(&reg->outepcfg[ep_num]));
	}
}

static void aic_udc_ep_set_stall(struct aic_ep *ep)
{
	u8		ep_num;
	u32		ep_ctrl = 0;

	ep_num = ep_index(ep);
	debug("%s: ep_num = %d, ep_type = %d\n", __func__, ep_num, ep->ep_type);

	if (ep_is_in(ep)) {
		ep_ctrl = readl(&reg->inepcfg[ep_num]);

		/* set the disable and stall bits */
		if (ep_ctrl & DEPCTL_EPENA)
			ep_ctrl |= DEPCTL_EPDIS;

		ep_ctrl |= DEPCTL_STALL;

		writel(ep_ctrl, &reg->inepcfg[ep_num]);
		debug("%s: set stall, DIEPCTL%d = 0x%x\n",
		      __func__, ep_num, readl(&reg->inepcfg[ep_num]));

	} else {
		ep_ctrl = readl(&reg->outepcfg[ep_num]);

		/* set the stall bit */
		ep_ctrl |= DEPCTL_STALL;

		writel(ep_ctrl, &reg->outepcfg[ep_num]);
		debug("%s: set stall, DOEPCTL%d = 0x%x\n",
		      __func__, ep_num, readl(&reg->outepcfg[ep_num]));
	}
}

static void aic_udc_ep_clear_stall(struct aic_ep *ep)
{
	u8		ep_num;
	u32		ep_ctrl = 0;

	ep_num = ep_index(ep);
	debug("%s: ep_num = %d, ep_type = %d\n", __func__, ep_num, ep->ep_type);

	if (ep_is_in(ep)) {
		ep_ctrl = readl(&reg->inepcfg[ep_num]);

		/* clear stall bit */
		ep_ctrl &= ~DEPCTL_STALL;

		/*
		 * USB Spec 9.4.5: For endpoints using data toggle, regardless
		 * of whether an endpoint has the Halt feature set, a
		 * ClearFeature(ENDPOINT_HALT) request always results in the
		 * data toggle being reinitialized to DATA0.
		 */
		if (ep->bmAttributes == USB_ENDPOINT_XFER_INT ||
		    ep->bmAttributes == USB_ENDPOINT_XFER_BULK) {
		    ep_ctrl |= DEPCTL_SETD0PID; /* DATA0 */
		}

		writel(ep_ctrl, &reg->inepcfg[ep_num]);
		debug("%s: cleared stall, DIEPCTL%d = 0x%x\n",
		      __func__, ep_num, readl(&reg->inepcfg[ep_num]));

	} else {
		ep_ctrl = readl(&reg->outepcfg[ep_num]);

		/* clear stall bit */
		ep_ctrl &= ~DEPCTL_STALL;

		if (ep->bmAttributes == USB_ENDPOINT_XFER_INT ||
		    ep->bmAttributes == USB_ENDPOINT_XFER_BULK) {
		    ep_ctrl |= DEPCTL_SETD0PID; /* DATA0 */
		}

		writel(ep_ctrl, &reg->outepcfg[ep_num]);
		debug("%s: cleared stall, DOEPCTL%d = 0x%x\n",
		      __func__, ep_num, readl(&reg->outepcfg[ep_num]));
	}
}

static int aic_udc_set_halt(struct usb_ep *_ep, int value)
{
	struct aic_ep	*ep;
	struct aic_udc	*dev;
	unsigned long	flags = 0;
	u8		ep_num;

	ep = container_of(_ep, struct aic_ep, ep);
	ep_num = ep_index(ep);

	if (unlikely(!_ep || !ep->desc || ep_num == EP0_CON ||
		     ep->desc->bmAttributes == USB_ENDPOINT_XFER_ISOC)) {
		debug("%s: %s bad ep or descriptor\n", __func__, ep->ep.name);
		return -EINVAL;
	}

	/* Attempt to halt IN ep will fail if any transfer requests
	 * are still queue
	 */
	if (value && ep_is_in(ep) && !list_empty(&ep->queue)) {
		debug("%s: %s queue not empty, req = %p\n",
		      __func__, ep->ep.name,
		      list_entry(ep->queue.next, struct aic_request, queue));

		return -EAGAIN;
	}

	dev = ep->dev;
	debug("%s: ep_num = %d, value = %d\n", __func__, ep_num, value);

	spin_lock_irqsave(&dev->lock, flags);

	if (value == 0) {
		ep->stopped = 0;
		aic_udc_ep_clear_stall(ep);
	} else {
		if (ep_num == 0)
			dev->ep0state = WAIT_FOR_SETUP;

		ep->stopped = 1;
		aic_udc_ep_set_stall(ep);
	}

	spin_unlock_irqrestore(&dev->lock, flags);

	return 0;
}

static void aic_udc_ep_activate(struct aic_ep *ep)
{
	u8 ep_num;
	u32 ep_ctrl = 0, daintmsk = 0;

	ep_num = ep_index(ep);

	/* Read DEPCTLn register */
	if (ep_is_in(ep)) {
		ep_ctrl = readl(&reg->inepcfg[ep_num]);
		daintmsk = 1 << ep_num;
	} else {
		ep_ctrl = readl(&reg->outepcfg[ep_num]);
		daintmsk = (1 << ep_num) << DAINT_OUT_BIT;
	}

	debug("%s: EPCTRL%d = 0x%x, ep_is_in = %d\n",
	      __func__, ep_num, ep_ctrl, ep_is_in(ep));

	/* If the EP is already active don't change the EP Control
	 * register.
	 */
	if (!(ep_ctrl & DEPCTL_USBACTEP)) {
		ep_ctrl = (ep_ctrl & ~DEPCTL_TYPE_MASK) |
			(ep->bmAttributes << DEPCTL_TYPE_BIT);
		ep_ctrl = (ep_ctrl & ~DEPCTL_MPS_MASK) |
			(ep->ep.maxpacket << DEPCTL_MPS_BIT);
		ep_ctrl |= (DEPCTL_SETD0PID | DEPCTL_USBACTEP | DEPCTL_SNAK);

		if (ep_is_in(ep)) {
			writel(ep_ctrl, &reg->inepcfg[ep_num]);
			debug("%s: USB Ative EP%d, DIEPCTRL%d = 0x%x\n",
			      __func__, ep_num, ep_num,
			      readl(&reg->inepcfg[ep_num]));
		} else {
			writel(ep_ctrl, &reg->outepcfg[ep_num]);
			debug("%s: USB Ative EP%d, DOEPCTRL%d = 0x%x\n",
			      __func__, ep_num, ep_num,
			      readl(&reg->outepcfg[ep_num]));
		}
	}

	/* Unmask EP Interrtupt */
	writel(readl(&reg->usbepintmsk) | daintmsk, &reg->usbepintmsk);
	debug("%s: DAINTMSK = 0x%x\n", __func__, readl(&reg->usbepintmsk));
}

static int aic_udc_clear_feature(struct usb_ep *_ep)
{
	struct aic_udc	*dev;
	struct aic_ep	*ep;
	u8		ep_num;

	ep = container_of(_ep, struct aic_ep, ep);
	ep_num = ep_index(ep);

	dev = ep->dev;
	debug_cond(DEBUG_SETUP != 0,
		   "%s: ep_num = %d, is_in = %d, clear_feature_flag = %d\n",
		   __func__, ep_num, ep_is_in(ep), clear_feature_flag);

	if (usb_ctrl->wLength != 0) {
		debug_cond(DEBUG_SETUP != 0,
			   "\tCLEAR_FEATURE: wLength is not zero.....\n");
		return 1;
	}

	switch (usb_ctrl->bRequestType & USB_RECIP_MASK) {
	case USB_RECIP_DEVICE:
		switch (usb_ctrl->wValue) {
		case USB_DEVICE_REMOTE_WAKEUP:
			debug_cond(DEBUG_SETUP != 0,
				   "\tOFF:USB_DEVICE_REMOTE_WAKEUP\n");
			break;

		case USB_DEVICE_TEST_MODE:
			debug_cond(DEBUG_SETUP != 0,
				   "\tCLEAR_FEATURE: USB_DEVICE_TEST_MODE\n");
			/** @todo Add CLEAR_FEATURE for TEST modes. */
			break;
		}

		aic_udc_ep0_zlp(dev);
		break;

	case USB_RECIP_ENDPOINT:
		debug_cond(DEBUG_SETUP != 0,
			   "\tCLEAR_FEATURE:USB_RECIP_ENDPOINT, wValue = %d\n",
			   usb_ctrl->wValue);

		if (usb_ctrl->wValue == USB_ENDPOINT_HALT) {
			if (ep_num == 0) {
				aic_udc_ep0_set_stall(ep);
				return 0;
			}

			aic_udc_ep0_zlp(dev);

			aic_udc_ep_clear_stall(ep);
			aic_udc_ep_activate(ep);
			ep->stopped = 0;

			clear_feature_num = ep_num;
			clear_feature_flag = 1;
		}
		break;
	}

	return 0;
}

static int aic_udc_set_feature(struct usb_ep *_ep)
{
	struct aic_udc	*dev;
	struct aic_ep	*ep;
	u8		ep_num;

	ep = container_of(_ep, struct aic_ep, ep);
	ep_num = ep_index(ep);
	dev = ep->dev;

	debug_cond(DEBUG_SETUP != 0,
		   "%s: *** USB_REQ_SET_FEATURE , ep_num = %d\n",
		    __func__, ep_num);

	if (usb_ctrl->wLength != 0) {
		debug_cond(DEBUG_SETUP != 0,
			   "\tSET_FEATURE: wLength is not zero.....\n");
		return 1;
	}

	switch (usb_ctrl->bRequestType & USB_RECIP_MASK) {
	case USB_RECIP_DEVICE:
		switch (usb_ctrl->wValue) {
		case USB_DEVICE_REMOTE_WAKEUP:
			debug_cond(DEBUG_SETUP != 0,
				   "\tSET_FEATURE:USB_DEVICE_REMOTE_WAKEUP\n");
			break;
		case USB_DEVICE_B_HNP_ENABLE:
			debug_cond(DEBUG_SETUP != 0,
				   "\tSET_FEATURE: USB_DEVICE_B_HNP_ENABLE\n");
			break;

		case USB_DEVICE_A_HNP_SUPPORT:
			/* RH port supports HNP */
			debug_cond(DEBUG_SETUP != 0,
				   "\tSET_FEATURE:USB_DEVICE_A_HNP_SUPPORT\n");
			break;

		case USB_DEVICE_A_ALT_HNP_SUPPORT:
			/* other RH port does */
			debug_cond(DEBUG_SETUP != 0,
				   "\tSET: USB_DEVICE_A_ALT_HNP_SUPPORT\n");
			break;
		}

		aic_udc_ep0_zlp(dev);
		return 0;

	case USB_RECIP_INTERFACE:
		debug_cond(DEBUG_SETUP != 0,
			   "\tSET_FEATURE: USB_RECIP_INTERFACE\n");
		break;

	case USB_RECIP_ENDPOINT:
		debug_cond(DEBUG_SETUP != 0,
			   "\tSET_FEATURE: USB_RECIP_ENDPOINT\n");
		if (usb_ctrl->wValue == USB_ENDPOINT_HALT) {
			if (ep_num == 0) {
				aic_udc_ep0_set_stall(ep);
				return 0;
			}
			ep->stopped = 1;
			aic_udc_ep_set_stall(ep);
		}

		aic_udc_ep0_zlp(dev);
		return 0;
	}

	return 1;
}

/*
 * WAIT_FOR_SETUP (OUT_PKT_RDY)
 */
static void aic_ep0_setup(struct aic_udc *dev)
{
	struct aic_ep *ep = &dev->ep[0];
	int i;
	u8 ep_num;

	/* Nuke all previous transfers */
	nuke(ep, -EPROTO);

	/* read control req from fifo (8 bytes) */
	aic_fifo_read(ep, usb_ctrl, 8);

	debug_cond(DEBUG_SETUP != 0,
		   "%s: bRequestType = 0x%x(%s), bRequest = 0x%x"
		   "\twLength = 0x%x, wValue = 0x%x, wIndex= 0x%x\n",
		   __func__, usb_ctrl->bRequestType,
		   (usb_ctrl->bRequestType & USB_DIR_IN) ? "IN" : "OUT",
		   usb_ctrl->bRequest,
		   usb_ctrl->wLength, usb_ctrl->wValue, usb_ctrl->wIndex);

#ifdef DEBUG
	{
		int i, len = sizeof(*usb_ctrl);
		char *p = (char *)usb_ctrl;

		printf("pkt = ");
		for (i = 0; i < len; i++) {
			printf("%02x", ((u8 *)p)[i]);
			if ((i & 7) == 7)
				printf(" ");
		}
		printf("\n");
	}
#endif

	if (usb_ctrl->bRequest == GET_MAX_LUN_REQUEST &&
	    usb_ctrl->wLength != 1) {
		debug_cond(DEBUG_SETUP != 0,
			   "\t%s:GET_MAX_LUN_REQUEST:invalid",
			   __func__);
		debug_cond(DEBUG_SETUP != 0,
			   "wLength = %d, setup returned\n",
			   usb_ctrl->wLength);

		aic_udc_ep0_set_stall(ep);
		dev->ep0state = WAIT_FOR_SETUP;

		return;
	} else if (usb_ctrl->bRequest == BOT_RESET_REQUEST &&
		 usb_ctrl->wLength != 0) {
		/* Bulk-Only *mass storge reset of class-specific request */
		debug_cond(DEBUG_SETUP != 0,
			   "%s:BOT Rest:invalid wLength =%d, setup returned\n",
			   __func__, usb_ctrl->wLength);

		aic_udc_ep0_set_stall(ep);
		dev->ep0state = WAIT_FOR_SETUP;

		return;
	}

	/* Set direction of EP0 */
	if (likely(usb_ctrl->bRequestType & USB_DIR_IN))
		ep->bEndpointAddress |= USB_DIR_IN;
	else
		ep->bEndpointAddress &= ~USB_DIR_IN;

	/* cope with automagic for some standard requests. */
	dev->req_std = (usb_ctrl->bRequestType & USB_TYPE_MASK)
			== USB_TYPE_STANDARD;

	dev->req_pending = 1;

	/* Handle some SETUP packets ourselves */
	if (dev->req_std) {
		switch (usb_ctrl->bRequest) {
		case USB_REQ_SET_ADDRESS:
		debug_cond(DEBUG_SETUP != 0,
			   "%s: *** USB_REQ_SET_ADDRESS (%d)\n",
			   __func__, usb_ctrl->wValue);
			if (usb_ctrl->bRequestType
				!= (USB_TYPE_STANDARD | USB_RECIP_DEVICE))
				break;

			udc_set_address(dev, usb_ctrl->wValue);
			return;

		case USB_REQ_SET_CONFIGURATION:
			debug_cond(DEBUG_SETUP != 0,
				   "=====================================\n");
			debug_cond(DEBUG_SETUP != 0,
				   "%s: USB_REQ_SET_CONFIGURATION (%d)\n",
				   __func__, usb_ctrl->wValue);

			if (usb_ctrl->bRequestType == USB_RECIP_DEVICE)
				reset_available = 1;

			break;

		case USB_REQ_GET_DESCRIPTOR:
			debug_cond(DEBUG_SETUP != 0,
				   "%s: *** USB_REQ_GET_DESCRIPTOR\n",
				   __func__);
			break;

		case USB_REQ_SET_INTERFACE:
			debug_cond(DEBUG_SETUP != 0,
				   "%s: *** USB_REQ_SET_INTERFACE (%d)\n",
				   __func__, usb_ctrl->wValue);

			if (usb_ctrl->bRequestType == USB_RECIP_INTERFACE)
				reset_available = 1;

			break;

		case USB_REQ_GET_CONFIGURATION:
			debug_cond(DEBUG_SETUP != 0,
				   "%s: *** USB_REQ_GET_CONFIGURATION\n",
				   __func__);
			break;

		case USB_REQ_GET_STATUS:
			if (!aic_udc_get_status(dev, usb_ctrl))
				return;

			break;

		case USB_REQ_CLEAR_FEATURE:
			ep_num = usb_ctrl->wIndex & 0x7f;

			if (!aic_udc_clear_feature(&dev->ep[ep_num].ep))
				return;

			break;

		case USB_REQ_SET_FEATURE:
			ep_num = usb_ctrl->wIndex & 0x7f;

			if (!aic_udc_set_feature(&dev->ep[ep_num].ep))
				return;

			break;

		default:
			debug_cond(DEBUG_SETUP != 0,
				   "%s: *** Default of usb_ctrl->bRequest=0x%x "
				   "happened.\n", __func__, usb_ctrl->bRequest);
			break;
		}
	}

	if (likely(dev->driver)) {
		/* device-2-host (IN) or no data setup command,
		 * process immediately
		 */
		debug_cond(DEBUG_SETUP != 0,
			   "%s:usb_ctrlreq will be passed to fsg_setup()\n",
			    __func__);

		spin_unlock(&dev->lock);
		i = dev->driver->setup(&dev->gadget, usb_ctrl);
		spin_lock(&dev->lock);

		if (i < 0) {
			/* setup processing failed, force stall */
			aic_udc_ep0_set_stall(ep);
			dev->ep0state = WAIT_FOR_SETUP;

			debug_cond(DEBUG_SETUP != 0,
				   "\tdev->driver->setup failed (%d),"
				    " bRequest = %d\n",
				i, usb_ctrl->bRequest);

		} else if (dev->req_pending) {
			dev->req_pending = 0;
			debug_cond(DEBUG_SETUP != 0,
				   "\tdev->req_pending...\n");
		}

		debug_cond(DEBUG_SETUP != 0,
			   "\tep0state = %s\n", state_names[dev->ep0state]);
	}
}

/*
 * handle ep0 interrupt
 */
static void aic_handle_ep0(struct aic_udc *dev)
{
	if (dev->ep0state == WAIT_FOR_SETUP) {
		debug_cond(DEBUG_OUT_EP != 0,
			   "%s: WAIT_FOR_SETUP\n", __func__);
		aic_ep0_setup(dev);

	} else {
		debug_cond(DEBUG_OUT_EP != 0,
			   "%s: strange state!!(state = %s)\n",
			__func__, state_names[dev->ep0state]);
	}
}

static void aic_ep0_kick(struct aic_udc *dev, struct aic_ep *ep)
{
	debug_cond(DEBUG_EP0 != 0,
		   "%s: ep_is_in = %d\n", __func__, ep_is_in(ep));
	if (ep_is_in(ep)) {
		dev->ep0state = DATA_STATE_XMIT;
		aic_ep0_write(dev);

	} else {
		dev->ep0state = DATA_STATE_RECV;
		aic_ep0_read(dev);
	}
}

/*
 *	udc_disable - disable USB device controller
 */
static void udc_disable(struct aic_udc *dev)
{
	debug_cond(DEBUG_SETUP != 0, "%s: %p\n", __func__, dev);

	udc_set_address(dev, 0);

	dev->ep0state = WAIT_FOR_SETUP;
	dev->gadget.speed = USB_SPEED_UNKNOWN;
	dev->usb_address = 0;

	udc_phy_off(dev);
}

/*
 *	udc_reinit - initialize software state
 */
static void udc_reinit(struct aic_udc *dev)
{
	unsigned int i;

	debug_cond(DEBUG_SETUP != 0, "%s: %p\n", __func__, dev);

	/* device/ep0 records init */
	INIT_LIST_HEAD(&dev->gadget.ep_list);
	INIT_LIST_HEAD(&dev->gadget.ep0->ep_list);
	dev->ep0state = WAIT_FOR_SETUP;

	/* basic endpoint records init */
	for (i = 0; i < AIC_MAX_ENDPOINTS; i++) {
		struct aic_ep *ep = &dev->ep[i];

		if (i != 0)
			list_add_tail(&ep->ep.ep_list, &dev->gadget.ep_list);

		ep->desc = 0;
		ep->stopped = 0;
		INIT_LIST_HEAD(&ep->queue);
		ep->pio_irqs = 0;
	}

	/* the rest was statically initialized, and is read-only */
}

#define BYTES2MAXP(x)	(x / 8)
#define MAXP2BYTES(x)	(x * 8)

/* until it's enabled, this UDC should be completely invisible
 * to any USB host.
 */
static int udc_enable(struct aic_udc *dev)
{
	debug_cond(DEBUG_SETUP != 0, "%s: %p\n", __func__, dev);

	udc_phy_init(dev);
	reconfig_usbd(dev);

	debug_cond(DEBUG_SETUP != 0,
		   "AIC USB 2.0 Device Controller Core Initialized : 0x%x\n",
		    readl(&reg->usbintmsk));

	dev->gadget.speed = USB_SPEED_UNKNOWN;

	return 0;
}

#if !CONFIG_IS_ENABLED(DM_USB_GADGET)
/*
  Register entry point for the peripheral controller driver.
*/
int usb_gadget_register_driver(struct usb_gadget_driver *driver)
{
	struct aic_udc *dev = the_controller;
	int retval = 0;
	unsigned long flags = 0;

	debug_cond(DEBUG_SETUP != 0, "%s: %s\n", __func__, "no name");

	if (!driver || driver->speed < USB_SPEED_FULL ||
	    !driver->bind || !driver->disconnect || !driver->setup)
		return -EINVAL;
	if (!dev)
		return -ENODEV;
	if (dev->driver)
		return -EBUSY;

	spin_lock_irqsave(&dev->lock, flags);
	/* first hook up the driver ... */
	dev->driver = driver;
	spin_unlock_irqrestore(&dev->lock, flags);

	if (retval) { /* TODO */
		printf("target device_add failed, error %d\n", retval);
		return retval;
	}

	retval = driver->bind(&dev->gadget);
	if (retval) {
		debug_cond(DEBUG_SETUP != 0,
			   "%s: bind to driver --> error %d\n",
			    dev->gadget.name, retval);
		dev->driver = 0;
		return retval;
	}

	enable_irq(IRQ_OTG);

	debug_cond(DEBUG_SETUP != 0,
		   "Registered gadget driver %s\n", dev->gadget.name);
	udc_enable(dev);

	return 0;
}

/*
 * Unregister entry point for the peripheral controller driver.
 */
int usb_gadget_unregister_driver(struct usb_gadget_driver *driver)
{
	struct aic_udc *dev = the_controller;
	unsigned long flags = 0;

	if (!dev)
		return -ENODEV;
	if (!driver || driver != dev->driver)
		return -EINVAL;

	spin_lock_irqsave(&dev->lock, flags);
	dev->driver = 0;
	stop_activity(dev, driver);
	spin_unlock_irqrestore(&dev->lock, flags);

	driver->unbind(&dev->gadget);

	disable_irq(IRQ_OTG);

	udc_disable(dev);
	return 0;
}
#else /* !CONFIG_IS_ENABLED(DM_USB_GADGET) */

static int aic_gadget_start(struct usb_gadget *g,
			     struct usb_gadget_driver *driver)
{
	struct aic_udc *dev = the_controller;

	debug_cond(DEBUG_SETUP != 0, "%s: %s\n", __func__, "no name");

	if (!driver || driver->speed < USB_SPEED_FULL ||
	    !driver->bind || !driver->disconnect || !driver->setup)
		return -EINVAL;

	if (!dev)
		return -ENODEV;

	if (dev->driver)
		return -EBUSY;

	/* first hook up the driver ... */
	dev->driver = driver;

	debug_cond(DEBUG_SETUP != 0,
		   "Registered gadget driver %s\n", dev->gadget.name);
	return udc_enable(dev);
}

static int aic_gadget_stop(struct usb_gadget *g)
{
	struct aic_udc *dev = the_controller;

	if (!dev)
		return -ENODEV;

	if (!dev->driver)
		return -EINVAL;

	dev->driver = 0;
	stop_activity(dev, dev->driver);

	udc_disable(dev);

	return 0;
}

#endif /* !CONFIG_IS_ENABLED(DM_USB_GADGET) */

/*
 *	done - retire a request; caller blocked irqs
 */
static void done(struct aic_ep *ep, struct aic_request *req, int status)
{
	unsigned int stopped = ep->stopped;

	debug("%s: %s %p, req = %p, stopped = %d\n",
	      __func__, ep->ep.name, ep, &req->req, stopped);

	list_del_init(&req->queue);

	if (likely(req->req.status == -EINPROGRESS))
		req->req.status = status;
	else
		status = req->req.status;

	if (status && status != -ESHUTDOWN) {
		debug("complete %s req %p stat %d len %u/%u\n",
		      ep->ep.name, &req->req, status,
		      req->req.actual, req->req.length);
	}

	/* don't modify queue heads during completion callback */
	ep->stopped = 1;

#ifdef DEBUG
	printf("calling complete callback\n");
	{
		int i, len = req->req.length;

		printf("pkt[%d] = ", req->req.length);
		if (len > 64)
			len = 64;
		for (i = 0; i < len; i++) {
			printf("%02x", ((u8 *)req->req.buf)[i]);
			if ((i & 7) == 7)
				printf(" ");
		}
		printf("\n");
	}
#endif
	spin_unlock(&ep->dev->lock);
	aic_handle_unaligned_req_complete(ep, req);
	req->req.complete(&ep->ep, &req->req);
	spin_lock(&ep->dev->lock);

	debug("callback completed\n");

	ep->stopped = stopped;
}

/*
 *	nuke - dequeue ALL requests
 */
static void nuke(struct aic_ep *ep, int status)
{
	struct aic_request *req;

	debug("%s: %s %p\n", __func__, ep->ep.name, ep);

	/* called with irqs blocked */
	while (!list_empty(&ep->queue)) {
		req = list_entry(ep->queue.next, struct aic_request, queue);
		done(ep, req, status);
	}
}

static void stop_activity(struct aic_udc *dev,
			  struct usb_gadget_driver *driver)
{
	int i;

	/* don't disconnect drivers more than once */
	if (dev->gadget.speed == USB_SPEED_UNKNOWN)
		driver = 0;
	dev->gadget.speed = USB_SPEED_UNKNOWN;

	/* prevent new request submissions, kill any outstanding requests  */
	for (i = 0; i < AIC_MAX_ENDPOINTS; i++) {
		struct aic_ep *ep = &dev->ep[i];

		ep->stopped = 1;
		nuke(ep, -ESHUTDOWN);
	}

	/* report disconnect; the driver is already quiesced */
	if (driver) {
		spin_unlock(&dev->lock);
		driver->disconnect(&dev->gadget);
		spin_lock(&dev->lock);
	}

	/* re-init driver-visible data structures */
	udc_reinit(dev);
}

static void reconfig_usbd(struct aic_udc *dev)
{
	/* 2. Soft-reset UDC Core and then unreset again. */
	int i;
	unsigned int uTemp;
	uint32_t dflt_gusbcfg;
	uint32_t rx_fifo_sz, tx_fifo_sz, np_tx_fifo_sz, offset;
	u32 max_hw_ep;
	int pdata_hw_ep;

	debug("Resetting UDC controller\n");

	uTemp = readl(&reg->usbdevinit);
	writel(uTemp | USBDEVINIT_CSFTRST, &reg->usbdevinit);
	while (readl(&reg->usbdevinit) & USBDEVINIT_CSFTRST)
		debug("%s: waiting for core_reset\n", __func__);
	while (!(readl(&reg->ahbbasic) & AHBBASIC_AHBIDLE))
		debug("%s: waiting for ahb_idle\n", __func__);

	dflt_gusbcfg =
		0 << 15		/* PHY Low Power Clock sel*/
		| 1 << 14	/* Non-Periodic TxFIFO Rewind Enable*/
		| 0x5 << 10	/* Turnaround time*/
		| 0 << 9 | 0 << 8/*[0:HNP disable,1:HNP enable][0:SRP disable*/
				/* 1:SRP enable] H1= 1,1*/
		| 0 << 7	/* Ulpi DDR sel*/
		| 0 << 6	/* 0: high speed utmi+, 1: full speed serial*/
		| 0 << 4	/* 0: utmi+, 1:ulpi*/
#ifdef CONFIG_PHY_BUS_WIDTH_8
		| 0 << 3	/* phy i/f  0:8bit, 1:16bit*/
#else
		| 1 << 3	/* phy i/f  0:8bit, 1:16bit*/
#endif
		| 0x7 << 0;	/* HS/FS Timeout**/

	if (dev->pdata->usb_gusbcfg) {
		dflt_gusbcfg = dev->pdata->usb_gusbcfg;
		debug("dflt_gusbcfg override by pdata = 0x%x\n", dflt_gusbcfg);
	}

	writel(dflt_gusbcfg, &reg->usbphyif);

	/* 3. Put the UDC device core in the disconnected state.*/
	uTemp = readl(&reg->usbdevfunc);
	uTemp |= SOFT_DISCONNECT;
	writel(uTemp, &reg->usbdevfunc);

	udelay(20);

	/* 5. Configure UDC Core to initial settings of device mode.*/
	/* [][1: full speed(30Mhz) 0:high speed]*/
	writel(EP_MISS_CNT(1) | DEV_SPEED_HIGH_SPEED_20, &reg->usbdevconf);

	mdelay(1);

	/* 6. Unmask the core interrupts*/
	writel(GINTMSK_INIT, &reg->usbintmsk);

	/* 7. Set NAK bit of EP0, EP1, EP2*/
	writel(DEPCTL_EPDIS | DEPCTL_SNAK, &reg->outepcfg[EP0_CON]);
	writel(DEPCTL_EPDIS | DEPCTL_SNAK | (1 << DEPCTL_NEXT_EP_BIT),
	       &reg->inepcfg[EP0_CON]);

	for (i = 1; i < AIC_MAX_ENDPOINTS; i++) {
		writel(DEPCTL_EPDIS | DEPCTL_SNAK, &reg->outepcfg[i]);
		writel(DEPCTL_EPDIS | DEPCTL_SNAK, &reg->inepcfg[i]);
	}

	/* 8. Unmask EPO interrupts*/
	writel(((1 << EP0_CON) << DAINT_OUT_BIT)
	       | (1 << EP0_CON), &reg->usbepintmsk);

	/* 9. Unmask device OUT EP common interrupts*/
	writel(DOEPMSK_INIT, &reg->outepintmsk);

	/* 10. Unmask device IN EP common interrupts*/
	writel(DIEPMSK_INIT, &reg->inepintmsk);

	rx_fifo_sz = RX_FIFO_SIZE;
	np_tx_fifo_sz = NPTX_FIFO_SIZE;
	tx_fifo_sz = PTX_FIFO_SIZE;

	if (dev->pdata->rx_fifo_sz)
		rx_fifo_sz = dev->pdata->rx_fifo_sz;
	if (dev->pdata->np_tx_fifo_sz)
		np_tx_fifo_sz = dev->pdata->np_tx_fifo_sz;
	if (dev->pdata->tx_fifo_sz)
		tx_fifo_sz = dev->pdata->tx_fifo_sz;

	/* 11. Set Rx FIFO Size (in 32-bit words) */
	writel(rx_fifo_sz, &reg->rxfifosiz);

	/* 12. Set Non Periodic Tx FIFO Size */
	writel((np_tx_fifo_sz << 16) | rx_fifo_sz,
	       &reg->nptxfifosiz);

	/* retrieve the number of IN Endpoints (excluding ep0) */
	max_hw_ep = PERIOD_IN_EP_NUM;
	pdata_hw_ep = dev->pdata->tx_fifo_sz_nb;

	/* tx_fifo_sz_nb should equal to number of IN Endpoint */
	if (pdata_hw_ep && max_hw_ep != pdata_hw_ep)
		pr_warn("Got %d hw endpoint but %d tx-fifo-size in array !!\n",
			max_hw_ep, pdata_hw_ep);

	offset = rx_fifo_sz + np_tx_fifo_sz;
	for (i = 0; i < max_hw_ep; i++) {
		if (pdata_hw_ep)
			tx_fifo_sz = dev->pdata->tx_fifo_sz_array[i];

		writel((offset | tx_fifo_sz << 16), &reg->txfifosiz[i]);
		offset += tx_fifo_sz;
	}
	/* Flush the RX FIFO */
	uTemp = readl(&reg->usbdevinit);
	writel(uTemp | USBDEVINIT_RXFFLSH, &reg->usbdevinit);
	while (readl(&reg->usbdevinit) & USBDEVINIT_RXFFLSH)
		debug("%s: waiting for rx_fifo_flush\n", __func__);

	/* Flush all the Tx FIFO's */
	uTemp = readl(&reg->usbdevinit);
	writel(uTemp | USBDEVINIT_TXFNUM(0x10), &reg->usbdevinit);
	writel(uTemp | USBDEVINIT_TXFNUM(0x10) | USBDEVINIT_TXFFLSH,
	       &reg->usbdevinit);
	while (readl(&reg->usbdevinit) & USBDEVINIT_TXFFLSH)
		debug("%s: waiting for tx_fifo_flush\n", __func__);

	/* 13. Clear NAK bit of EP0, EP1, EP2*/
	/* For Slave mode*/
	/* EP0: Control OUT */
	writel(DEPCTL_EPDIS | DEPCTL_CNAK,
	       &reg->outepcfg[EP0_CON]);

	/* 14. Initialize UDC Link Core.*/
	writel(GAHBCFG_INIT, &reg->usbdevinit);

	/* 4. Make the UDC device core exit from the disconnected state.*/
	uTemp = readl(&reg->usbdevfunc);
	uTemp = uTemp & ~SOFT_DISCONNECT;
	writel(uTemp, &reg->usbdevfunc);

	/* dump register */
	if (DEBUG_DUMP) {
		debug("ahbbasic = 0x%x\n", readl(&reg->ahbbasic));
		debug("usbdevinit = 0x%x\n", readl(&reg->usbdevinit));
		debug("usbphyif = 0x%x\n", readl(&reg->usbphyif));
		debug("usbulpiphy = 0x%x\n", readl(&reg->usbulpiphy));
		debug("usbintsts = 0x%x\n", readl(&reg->usbintsts));
		debug("usbintmsk = 0x%x\n", readl(&reg->usbintmsk));
		debug("rxfifosiz = 0x%x\n", readl(&reg->rxfifosiz));
		debug("rxfifosts = 0x%x\n", readl(&reg->rxfifosts));
		debug("nptxfifosiz = 0x%x\n", readl(&reg->nptxfifosiz));
		debug("nptxfifosts = 0x%x\n", readl(&reg->nptxfifosts));
		debug("txfifosiz[0] = 0x%x\n", readl(&reg->txfifosiz[0]));
		debug("txfifosiz[1] = 0x%x\n", readl(&reg->txfifosiz[1]));
		debug("usbdevconf = 0x%x\n", readl(&reg->usbdevconf));
		debug("usbdevfunc = 0x%x\n", readl(&reg->usbdevfunc));
		debug("usblinests = 0x%x\n", readl(&reg->usblinests));
		debug("inepintmsk = 0x%x\n", readl(&reg->inepintmsk));
		debug("outepintmsk = 0x%x\n", readl(&reg->outepintmsk));
		debug("usbepint = 0x%x\n", readl(&reg->usbepint));
		debug("usbepintmsk = 0x%x\n", readl(&reg->usbepintmsk));
		debug("inepcfg[0] = 0x%x\n", readl(&reg->inepcfg[0]));
		debug("outepcfg[0] = 0x%x\n", readl(&reg->outepcfg[0]));
		debug("ineptsfsiz[0] = 0x%x\n", readl(&reg->ineptsfsiz[0]));
		debug("outeptsfsiz[0] = 0x%x\n", readl(&reg->outeptsfsiz[0]));
		debug("inepdmaaddr[0] = 0x%x\n", readl(&reg->inepdmaaddr[0]));
		debug("outepdmaaddr[0] =0x%x\n", readl(&reg->outepdmaaddr[0]));
		debug("inepint[0] = 0x%x\n", readl(&reg->inepint[0]));
		debug("outepint[0] = 0x%x\n", readl(&reg->outepint[0]));
	}
}

static void set_max_pktsize(struct aic_udc *dev, enum usb_device_speed speed)
{
	unsigned int ep_ctrl;
	int i;

	if (speed == USB_SPEED_HIGH) {
		ep0_fifo_size = 64;
		ep_fifo_size = 512;
		ep_fifo_size2 = 1024;
		dev->gadget.speed = USB_SPEED_HIGH;
	} else {
		ep0_fifo_size = 64;
		ep_fifo_size = 64;
		ep_fifo_size2 = 64;
		dev->gadget.speed = USB_SPEED_FULL;
	}

	dev->ep[0].ep.maxpacket = ep0_fifo_size;
	for (i = 1; i < AIC_MAX_ENDPOINTS; i++)
		dev->ep[i].ep.maxpacket = ep_fifo_size;

	/* EP0 - Control IN (64 bytes)*/
	ep_ctrl = readl(&reg->inepcfg[EP0_CON]);
	writel(ep_ctrl | (0 <<  0), &reg->inepcfg[EP0_CON]);

	/* EP0 - Control OUT (64 bytes)*/
	ep_ctrl = readl(&reg->outepcfg[EP0_CON]);
	writel(ep_ctrl | (0 << 0), &reg->outepcfg[EP0_CON]);
}

static int aic_ep_enable(struct usb_ep *_ep,
			 const struct usb_endpoint_descriptor *desc)
{
	struct aic_ep *ep;
	struct aic_udc *dev;
	unsigned long flags = 0;

	debug("%s: %p\n", __func__, _ep);

	ep = container_of(_ep, struct aic_ep, ep);
	if (!_ep || !desc || ep->desc || _ep->name == ep0name ||
	    desc->bDescriptorType != USB_DT_ENDPOINT ||
	    ep->bEndpointAddress != desc->bEndpointAddress ||
	    ep_maxpacket(ep) <
	    le16_to_cpu(get_unaligned(&desc->wMaxPacketSize))) {
		debug("%s: bad ep or descriptor\n", __func__);
		return -EINVAL;
	}

	/* xfer types must match, except that interrupt ~= bulk */
	if (ep->bmAttributes != desc->bmAttributes &&
	    ep->bmAttributes != USB_ENDPOINT_XFER_BULK &&
	    desc->bmAttributes != USB_ENDPOINT_XFER_INT) {
		debug("%s: %s type mismatch\n", __func__, _ep->name);
		return -EINVAL;
	}

	/* hardware _could_ do smaller, but driver doesn't */
	if ((desc->bmAttributes == USB_ENDPOINT_XFER_BULK &&
	     le16_to_cpu(get_unaligned(&desc->wMaxPacketSize)) >
	     ep_maxpacket(ep)) || !get_unaligned(&desc->wMaxPacketSize)) {
		debug("%s: bad %s maxpacket\n", __func__, _ep->name);
		return -ERANGE;
	}

	dev = ep->dev;
	if (!dev->driver || dev->gadget.speed == USB_SPEED_UNKNOWN) {
		debug("%s: bogus device state\n", __func__);
		return -ESHUTDOWN;
	}

	ep->stopped = 0;
	ep->desc = desc;
	ep->ep.desc = desc;
	ep->pio_irqs = 0;
	ep->ep.maxpacket = le16_to_cpu(get_unaligned(&desc->wMaxPacketSize));

	/* Reset halt state */
	aic_udc_set_nak(ep);
	aic_udc_set_halt(_ep, 0);

	spin_lock_irqsave(&ep->dev->lock, flags);
	aic_udc_ep_activate(ep);
	spin_unlock_irqrestore(&ep->dev->lock, flags);

	debug("%s: enabled %s, stopped = %d, maxpacket = %d\n",
	      __func__, _ep->name, ep->stopped, ep->ep.maxpacket);
	return 0;
}

/*
 * Disable EP
 */
static int aic_ep_disable(struct usb_ep *_ep)
{
	struct aic_ep *ep;
	unsigned long flags = 0;

	debug("%s: %p\n", __func__, _ep);

	ep = container_of(_ep, struct aic_ep, ep);
	if (!_ep || !ep->desc) {
		debug("%s: %s not enabled\n", __func__,
		      _ep ? ep->ep.name : NULL);
		return -EINVAL;
	}

	spin_lock_irqsave(&ep->dev->lock, flags);

	/* Nuke all pending requests */
	nuke(ep, -ESHUTDOWN);

	ep->desc = 0;
	ep->ep.desc = 0;
	ep->stopped = 1;

	spin_unlock_irqrestore(&ep->dev->lock, flags);

	debug("%s: disabled %s\n", __func__, _ep->name);
	return 0;
}

static struct usb_request *aic_alloc_request(struct usb_ep *ep,
					     gfp_t gfp_flags)
{
	struct aic_request *req;

	debug("%s: %s %p\n", __func__, ep->name, ep);

	req = memalign(CONFIG_SYS_CACHELINE_SIZE, sizeof(*req));
	if (!req)
		return 0;

	memset(req, 0, sizeof(*req));
	INIT_LIST_HEAD(&req->queue);

	return &req->req;
}

static void aic_free_request(struct usb_ep *ep, struct usb_request *_req)
{
	struct aic_request *req;

	debug("%s: %p\n", __func__, ep);

	req = container_of(_req, struct aic_request, req);
	WARN_ON(!list_empty(&req->queue));
	kfree(req);
}

/* dequeue JUST ONE request */
static int aic_dequeue(struct usb_ep *_ep, struct usb_request *_req)
{
	struct aic_ep *ep;
	struct aic_request *req;
	unsigned long flags = 0;

	debug("%s: %p\n", __func__, _ep);

	ep = container_of(_ep, struct aic_ep, ep);
	if (!_ep || ep->ep.name == ep0name)
		return -EINVAL;

	spin_lock_irqsave(&ep->dev->lock, flags);

	/* make sure it's actually queued on this endpoint */
	list_for_each_entry(req, &ep->queue, queue) {
		if (&req->req == _req)
			break;
	}
	if (&req->req != _req) {
		spin_unlock_irqrestore(&ep->dev->lock, flags);
		return -EINVAL;
	}

	done(ep, req, -ECONNRESET);

	spin_unlock_irqrestore(&ep->dev->lock, flags);
	return 0;
}

/*
 * Return bytes in EP FIFO
 */
static int aic_fifo_status(struct usb_ep *_ep)
{
	int count = 0;
	struct aic_ep *ep;

	ep = container_of(_ep, struct aic_ep, ep);
	if (!_ep) {
		debug("%s: bad ep\n", __func__);
		return -ENODEV;
	}

	debug("%s: %d\n", __func__, ep_index(ep));

	/* LPD can't report unclaimed bytes from IN fifos */
	if (ep_is_in(ep))
		return -EOPNOTSUPP;

	return count;
}

/*
 * Flush EP FIFO
 */
static void aic_fifo_flush(struct usb_ep *_ep)
{
	struct aic_ep *ep;

	ep = container_of(_ep, struct aic_ep, ep);
	if (unlikely(!_ep || (!ep->desc && ep->ep.name != ep0name))) {
		debug("%s: bad ep\n", __func__);
		return;
	}

	debug("%s: %d\n", __func__, ep_index(ep));
}

static const struct usb_gadget_ops aic_udc_ops = {
	/* current versions must always be self-powered */
#if CONFIG_IS_ENABLED(DM_USB_GADGET)
	.udc_start		= aic_gadget_start,
	.udc_stop		= aic_gadget_stop,
#endif
};

static struct aic_udc memory = {
	.usb_address = 0,
	.gadget = {
		.ops = &aic_udc_ops,
		.ep0 = &memory.ep[0].ep,
		.name = driver_name,
	},

	/* control endpoint */
	.ep[0] = {
		.ep = {
			.name = ep0name,
			.ops = &aic_ep_ops,
			.maxpacket = EP0_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = 0,
		.bmAttributes = 0,

		.ep_type = ep_control,
	},

	/* first group of endpoints */
	.ep[1] = {
		.ep = {
			.name = "ep1in-bulk",
			.ops = &aic_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_IN | 1,
		.bmAttributes = USB_ENDPOINT_XFER_BULK,

		.ep_type = ep_bulk_in,
	},

	.ep[2] = {
		.ep = {
			.name = "ep2out-bulk",
			.ops = &aic_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_OUT | 2,
		.bmAttributes = USB_ENDPOINT_XFER_BULK,

		.ep_type = ep_bulk_out,
	},

	.ep[3] = {
		.ep = {
			.name = "ep3in-int",
			.ops = &aic_ep_ops,
			.maxpacket = EP_FIFO_SIZE,
		},
		.dev = &memory,

		.bEndpointAddress = USB_DIR_IN | 3,
		.bmAttributes = USB_ENDPOINT_XFER_INT,

		.ep_type = ep_interrupt,
		.fifo_num = 1,
	},
};

/*
 *	probe - binds to the platform device
 */

int aic_udc_probe(struct aic_plat_udc_data *pdata)
{
	struct aic_udc *dev = &memory;
	int retval = 0;

	debug("%s: %p\n", __func__, pdata);

	dev->pdata = pdata;

	reg = (struct aic_udc_reg *)pdata->regs_udc;

	dev->gadget.is_dualspeed = 1;	/* Hack only*/
	dev->gadget.is_otg = 0;
	dev->gadget.is_a_peripheral = 0;
	dev->gadget.b_hnp_enable = 0;
	dev->gadget.a_hnp_support = 0;
	dev->gadget.a_alt_hnp_support = 0;

	the_controller = dev;

	usb_ctrl = memalign(CONFIG_SYS_CACHELINE_SIZE,
			    ROUND(sizeof(struct usb_ctrlrequest),
				  CONFIG_SYS_CACHELINE_SIZE));
	if (!usb_ctrl) {
		pr_err("No memory available for UDC!\n");
		return -ENOMEM;
	}

	usb_ctrl_dma_addr = (dma_addr_t)usb_ctrl;

	debug("usb_ctrl = 0x%p, usb_ctrl_dma_addr = 0x%llx\n",
	      usb_ctrl, usb_ctrl_dma_addr);

	udc_reinit(dev);

	return retval;
}

int aic_udc_handle_interrupt(void)
{
	u32 intr_status = readl(&reg->usbintsts);
	u32 gintmsk = readl(&reg->usbintmsk);

	if (intr_status & gintmsk)
		return aic_udc_irq(1, (void *)the_controller);

	return 0;
}

#if !CONFIG_IS_ENABLED(DM_USB_GADGET)

int usb_gadget_handle_interrupts(int index)
{
	return aic_udc_handle_interrupt();
}

#else /* CONFIG_IS_ENABLED(DM_USB_GADGET) */
struct aic_priv_data {
	struct clk_bulk		clks;
	struct reset_ctl_bulk	resets;
	struct phy_bulk phys;
	struct udevice *usb33d_supply;
};

int dm_usb_gadget_handle_interrupts(struct udevice *dev)
{
	return aic_udc_handle_interrupt();
}

static int aic_phy_setup(struct udevice *dev, struct phy_bulk *phys)
{
	int ret;

	ret = generic_phy_get_bulk(dev, phys);
	if (ret)
		return ret;

	ret = generic_phy_init_bulk(phys);
	if (ret)
		return ret;

	ret = generic_phy_power_on_bulk(phys);
	if (ret)
		generic_phy_exit_bulk(phys);

	return ret;
}

static void aic_phy_shutdown(struct udevice *dev, struct phy_bulk *phys)
{
	generic_phy_power_off_bulk(phys);
	generic_phy_exit_bulk(phys);
}

static int aic_udc_of_to_plat(struct udevice *dev)
{
	struct aic_plat_udc_data *plat = dev_get_plat(dev);
	ulong drvdata;
	void (*set_params)(struct aic_plat_udc_data *data);

	plat->regs_udc = dev_read_addr(dev);

	plat->rx_fifo_sz = AIC_RX_FIFO_SIZE;
	plat->np_tx_fifo_sz = AIC_NP_TX_FIFO_SIZE;
	plat->tx_fifo_sz_nb = 2;
	plat->tx_fifo_sz_array[0] = AIC_PERIOD_TX_FIFO1_SIZE;
	plat->tx_fifo_sz_array[1] = AIC_PERIOD_TX_FIFO2_SIZE;

	plat->force_b_session_valid =
		dev_read_bool(dev, "u-boot,force-b-session-valid");

	plat->force_vbus_detection =
		dev_read_bool(dev, "u-boot,force-vbus-detection");

	/* force plat according compatible */
	drvdata = dev_get_driver_data(dev);
	if (drvdata) {
		set_params = (void *)drvdata;
		set_params(plat);
	}

	return 0;
}

static void aic_udc_v10_params(struct aic_plat_udc_data *p)
{
	p->usb_gusbcfg =
		0 << 15		/* PHY Low Power Clock sel*/
		| 0x5 << 10	/* USB Turnaround time (0x5 for HS phy) */
		| 0 << 9	/* [0:HNP disable,1:HNP enable]*/
		| 0 << 8	/* [0:SRP disable 1:SRP enable]*/
		| 0 << 6	/* 0: high speed utmi+, 1: full speed serial*/
#ifdef CONFIG_FPGA_BOARD_ARTINCHIP
		| 1 << 4	/* 0: utmi+, 1:ulpi*/
#else
		| 0 << 4	/* 0: utmi+, 1:ulpi*/
#endif
		| 0x7 << 0;	/* FS timeout calibration**/
}

static int aic_udc_gg_reset_init(struct udevice *dev,
				 struct reset_ctl_bulk *resets)
{
	int ret;

	ret = reset_get_bulk(dev, resets);
	if (ret == -ENOTSUPP || ret == -ENOENT)
		return 0;

	if (ret)
		return ret;

	ret = reset_assert_bulk(resets);

	if (!ret) {
		udelay(600);
		ret = reset_deassert_bulk(resets);
	}
	udelay(600);
	if (ret) {
		reset_release_bulk(resets);
		return ret;
	}

	return 0;
}

static int aic_udc_gg_clk_init(struct udevice *dev,
			       struct clk_bulk *clks)
{
	int ret;

	ret = clk_get_bulk(dev, clks);
	if (ret == -ENOSYS)
		return 0;

	if (ret)
		return ret;

	ret = clk_enable_bulk(clks);
	if (ret) {
		clk_release_bulk(clks);
		return ret;
	}

	/* wait USB-PHY work stable */
	udelay(600);

	return 0;
}

static int aic_udc_gg_probe(struct udevice *dev)
{
	struct aic_plat_udc_data *plat = dev_get_plat(dev);
	struct aic_priv_data *priv = dev_get_priv(dev);
	int ret;

	ret = aic_udc_gg_clk_init(dev, &priv->clks);
	if (ret)
		return ret;

	ret = aic_udc_gg_reset_init(dev, &priv->resets);
	if (ret)
		return ret;

	ret = aic_phy_setup(dev, &priv->phys);
	if (ret)
		return ret;

	if (plat->activate_stm_id_vb_detection) {
		if (CONFIG_IS_ENABLED(DM_REGULATOR) &&
		    (!plat->force_b_session_valid ||
		     plat->force_vbus_detection)) {
			ret = device_get_supply_regulator(dev, "usb33d-supply",
							  &priv->usb33d_supply);
			if (ret) {
				dev_err(dev, "can't get voltage level detector supply\n");
				return ret;
			}
			ret = regulator_set_enable(priv->usb33d_supply, true);
			if (ret) {
				dev_err(dev, "can't enable voltage level detector supply\n");
				return ret;
			}
		}
	}

	ret = aic_udc_probe(plat);
	if (ret)
		return ret;

	the_controller->driver = 0;

	ret = usb_add_gadget_udc((struct device *)dev, &the_controller->gadget);

	return ret;
}

static int aic_udc_gg_remove(struct udevice *dev)
{
	struct aic_priv_data *priv = dev_get_priv(dev);

	usb_del_gadget_udc(&the_controller->gadget);

	reset_release_bulk(&priv->resets);

	clk_release_bulk(&priv->clks);

	aic_phy_shutdown(dev, &priv->phys);

	return dm_scan_fdt_dev(dev);
}

static const struct udevice_id aic_udc_ids[] = {
	{ .compatible = "artinchip,aic-udc-v1.0",
		.data = (ulong)aic_udc_v10_params },
	{},
};

U_BOOT_DRIVER(aic_udc) = {
	.name	= "aic-udc",
	.id	= UCLASS_USB_GADGET_GENERIC,
	.of_match = aic_udc_ids,
	.of_to_plat = aic_udc_of_to_plat,
	.probe = aic_udc_gg_probe,
	.remove = aic_udc_gg_remove,
	.plat_auto	= sizeof(struct aic_plat_udc_data),
	.priv_auto	= sizeof(struct aic_priv_data),
};
#endif /* CONFIG_IS_ENABLED(DM_USB_GADGET) */
