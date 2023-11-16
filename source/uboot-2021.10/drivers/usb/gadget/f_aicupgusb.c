// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd
 */
#include <config.h>
#include <common.h>
#include <env.h>
#include <errno.h>
#include <malloc.h>
#include <memalign.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/composite.h>
#include <linux/compiler.h>
#include <version.h>
#include <g_dnl.h>
#include <artinchip/aicupg.h>

#ifdef CONFIG_ARTINCHIP_DEBUG_AICUPG
#undef debug
#define debug printf
#endif

#define USB_UPG_BUF_SIZE                (64 * 1024)
#define USB_CBW_LENGTH                  31
#define USB_CBW_SIGATURE                0x43425355      /* 'USBC' */
#define USB_DIRECT_IN_FLAG              0x80
#define USB_CSW_SIGNATURE               0x53425355      /* 'USBS' */
#define USB_CSW_LENGTH                  13
#define USB_CSW_STATUS_OK               0
#define USB_CSW_STATUS_FAIL             1

#define AICUPG_USB_CMD_WRITE_PKT        0x01
#define AICUPG_USB_CMD_READ_PKT         0x02

struct f_aicupgusb {
	struct usb_function usb_function;
	struct usb_ep *in_ep, *out_ep;
	struct usb_request *in_req, *out_req;
	u32    tag;
};

/* Command Block Wrapper */
struct aic_cbw {
	__le32	signature;		/* Contains 'USBC' */
	u32	tag;			/* Unique per command id */
	__le32	data_transfer_length;	/* Size of the data */
	u8	flags;			/* Direction in bit 7 */
	u8	lun;			/* LUN (normally 0) */
	u8	length;			/* Of the CDB, 0x01 */
	u8	cmd;			/* Command Data Block */
	u8	reserved[15];
};

/* Command status Wrapper */
struct aic_csw {
	__le32  signature;              /* Should = 'USBS' */
	u32     tag;                    /* Same as original command */
	__le32  residue;                /* Amount not transferred */
	u8      status;                 /* See below */
};

static struct f_aicupgusb *aicupg_func;
static struct usb_endpoint_descriptor fs_ep_in = {
	.bLength            = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType    = USB_DT_ENDPOINT,
	.bEndpointAddress   = USB_DIR_IN,
	.bmAttributes       = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize     = cpu_to_le16(64),
};

static struct usb_endpoint_descriptor fs_ep_out = {
	.bLength		= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bEndpointAddress	= USB_DIR_OUT,
	.bmAttributes		= USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize		= cpu_to_le16(64),
};

static struct usb_endpoint_descriptor hs_ep_in = {
	.bLength		= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bEndpointAddress	= USB_DIR_IN,
	.bmAttributes		= USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize		= cpu_to_le16(512),
};

static struct usb_endpoint_descriptor hs_ep_out = {
	.bLength		= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bEndpointAddress	= USB_DIR_OUT,
	.bmAttributes		= USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize		= cpu_to_le16(512),
};

static struct usb_interface_descriptor interface_desc = {
	.bLength		= USB_DT_INTERFACE_SIZE,
	.bDescriptorType	= USB_DT_INTERFACE,
	.bInterfaceNumber	= 0x00,
	.bAlternateSetting	= 0x00,
	.bNumEndpoints		= 0x02,
	.bInterfaceClass	= 0xFF,
	.bInterfaceSubClass	= 0xFF,
	.bInterfaceProtocol	= 0xFF,
};

static struct usb_descriptor_header *aicupg_fs_function[] = {
	(struct usb_descriptor_header *)&interface_desc,
	(struct usb_descriptor_header *)&fs_ep_in,
	(struct usb_descriptor_header *)&fs_ep_out,
};

static struct usb_descriptor_header *aicupg_hs_function[] = {
	(struct usb_descriptor_header *)&interface_desc,
	(struct usb_descriptor_header *)&hs_ep_in,
	(struct usb_descriptor_header *)&hs_ep_out,
	NULL,
};

static struct usb_endpoint_descriptor *
aicupg_ep_desc(struct usb_gadget *g, struct usb_endpoint_descriptor *fs,
	       struct usb_endpoint_descriptor *hs)
{
	if (gadget_is_dualspeed(g) && g->speed == USB_SPEED_HIGH)
		return hs;
	return fs;
}

static const char aicupg_manu_name[] = "ArtInChip";
static const char aicupg_prod_name[] = "AIC Device";

static struct usb_string aicupg_string_defs[] = {
	[0].s = aicupg_manu_name,
	[1].s = aicupg_prod_name,
	{  }			/* end of list */
};

static struct usb_gadget_strings stringtab_aicupg = {
	.language	= 0x0409,	/* en-us */
	.strings	= aicupg_string_defs,
};

static struct usb_gadget_strings *aicupg_strings[] = {
	&stringtab_aicupg,
	NULL,
};

static void aicupg_trans_layer_rx_cbw(struct usb_ep *ep,
					      struct usb_request *req);
static void aicupg_tx_complete(struct usb_ep *ep, struct usb_request *req);

static inline struct f_aicupgusb *func_to_aicupgusb(struct usb_function *f)
{
	return container_of(f, struct f_aicupgusb, usb_function);
}

static int aicupg_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct f_aicupgusb *f_upg = func_to_aicupgusb(f);
	struct usb_gadget *gadget = c->cdev->gadget;
	const char *s;
	int id;

	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	interface_desc.bInterfaceNumber = id;

	id = usb_string_id(c->cdev);
	if (id < 0)
		return id;
	aicupg_string_defs[0].id = id;
	interface_desc.iInterface = id;

	f_upg->in_ep = usb_ep_autoconfig(gadget, &fs_ep_in);
	if (!f_upg->in_ep) {
		pr_err("%s, in ep config failed.\n", __func__);
		return -ENODEV;
	}
	f_upg->in_ep->driver_data = c->cdev;

	f_upg->out_ep = usb_ep_autoconfig(gadget, &fs_ep_out);
	if (!f_upg->out_ep) {
		pr_err("%s, out ep config failed.\n", __func__);
		return -ENODEV;
	}
	f_upg->out_ep->driver_data = c->cdev;

	f->descriptors = aicupg_fs_function;

	if (gadget_is_dualspeed(gadget)) {
		hs_ep_in.bEndpointAddress = fs_ep_in.bEndpointAddress;
		hs_ep_out.bEndpointAddress = fs_ep_out.bEndpointAddress;
		f->hs_descriptors = aicupg_hs_function;
	}

	s = env_get("serial#");
	if (s)
		g_dnl_set_serialnumber((char *)s);

	return 0;
}

static void aicupg_unbind(struct usb_configuration *c, struct usb_function *f)
{
	memset(aicupg_func, 0, sizeof(*aicupg_func));
}

static void aicupg_disable(struct usb_function *f)
{
	struct f_aicupgusb *f_upg = func_to_aicupgusb(f);

	usb_ep_disable(f_upg->out_ep);
	usb_ep_disable(f_upg->in_ep);

	if (f_upg->out_req) {
		free(f_upg->out_req->buf);
		usb_ep_free_request(f_upg->out_ep, f_upg->out_req);
		f_upg->out_req = NULL;
	}
	if (f_upg->in_req) {
		free(f_upg->in_req->buf);
		usb_ep_free_request(f_upg->in_ep, f_upg->in_req);
		f_upg->in_req = NULL;
	}
}

static struct usb_request *aicupg_start_ep(struct usb_ep *ep)
{
	struct usb_request *req;

	req = usb_ep_alloc_request(ep, 0);
	if (!req)
		return NULL;

	req->length = USB_UPG_BUF_SIZE;
	req->buf = memalign(CONFIG_SYS_CACHELINE_SIZE, USB_UPG_BUF_SIZE);
	if (!req->buf) {
		usb_ep_free_request(ep, req);
		return NULL;
	}

	memset(req->buf, 0, req->length);
	return req;
}

static int aicupg_set_alt(struct usb_function *f, unsigned interface,
			  unsigned alt)
{
	struct f_aicupgusb *f_upg = func_to_aicupgusb(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_gadget *gadget = cdev->gadget;
	const struct usb_endpoint_descriptor *d;
	int ret;

	debug("%s: func: %s intf: %d alt: %d\n",
	      __func__, f->name, interface, alt);

	d = aicupg_ep_desc(gadget, &fs_ep_out, &hs_ep_out);
	ret = usb_ep_enable(f_upg->out_ep, d);
	if (ret) {
		puts("failed to enable out ep\n");
		return ret;
	}

	f_upg->out_req = aicupg_start_ep(f_upg->out_ep);
	if (!f_upg->out_req) {
		puts("failed to alloc out req\n");
		ret = -EINVAL;
		goto err;
	}
	f_upg->out_req->complete = aicupg_trans_layer_rx_cbw;

	d = aicupg_ep_desc(gadget, &fs_ep_in, &hs_ep_in);
	ret = usb_ep_enable(f_upg->in_ep, d);
	if (ret) {
		puts("failed to enable in ep\n");
		goto err;
	}

	f_upg->in_req = aicupg_start_ep(f_upg->in_ep);
	if (!f_upg->in_req) {
		puts("failed alloc req in\n");
		ret = -EINVAL;
		goto err;
	}
	f_upg->in_req->complete = aicupg_tx_complete;

	ret = usb_ep_queue(f_upg->out_ep, f_upg->out_req, 0);
	if (ret)
		goto err;

	return 0;
err:
	aicupg_disable(f);
	return ret;
}

static void aicupg_tx_complete(struct usb_ep *in_ep, struct usb_request *in_req)
{
	int status = in_req->status;

	if (!status)
		return;
	debug("status: %d ep '%s' trans: %d\n", status, in_ep->name,
	       in_req->actual);
}

static int aicupg_trans_layer_send_csw(u32 tag, int residue, u8 status)
{
	struct usb_request *in_req = aicupg_func->in_req;
	struct usb_ep *in_ep = aicupg_func->in_ep;
	struct aic_csw csw;
	int ret;

	debug("%s, tag = 0x%x, status = %d\n", __func__, tag, status);
	csw.signature = cpu_to_le32(USB_CSW_SIGNATURE);
	csw.tag = tag;
	csw.residue = cpu_to_be32(residue);
	csw.status = status;

	memcpy(in_req->buf, &csw, USB_CSW_LENGTH);
	in_req->length = USB_CSW_LENGTH;
	in_req->complete = aicupg_tx_complete;
	usb_ep_dequeue(in_ep, in_req);
	ret = usb_ep_queue(in_ep, in_req, 0);
	if (ret)
		pr_err("Error %d on queue\n", ret);
	return ret;
}

static void aicupg_trans_layer_tx_csw(struct usb_ep *in_ep,
				      struct usb_request *in_req)
{
	aicupg_trans_layer_send_csw(aicupg_func->tag, 0, USB_CSW_STATUS_OK);
}

static void aicupg_trans_layer_write_pkt(struct usb_ep *out_ep,
					 struct usb_request *out_req)
{
	debug("%s, actual %d\n", __func__, out_req->actual);

	aicupg_data_packet_write(out_req->buf, out_req->actual);
	/*
	 * Host write data packet is ok, now should send out csw.
	 */
	aicupg_trans_layer_send_csw(aicupg_func->tag, 0, USB_CSW_STATUS_OK);
	/*
	 * Data packet seq is:
	 *  {CBW} {DATA PKT} {CSW} ... {CBW} {DATA PKT} {CSW}
	 *
	 * Next rx data should be CBW again
	 */
	out_req->complete = aicupg_trans_layer_rx_cbw;
	out_req->actual = 0;
	usb_ep_queue(out_ep, out_req, 0);
}

static void aicupg_trans_layer_read_pkt(struct usb_ep *in_ep,
					struct usb_request *in_req)
{
	debug("%s, length %d\n", __func__, in_req->length);
	aicupg_data_packet_read(in_req->buf, in_req->length);
	/* Send data packet, and csw should be sent when data packet is done */
	in_req->complete = aicupg_trans_layer_tx_csw;
	usb_ep_dequeue(in_ep, in_req);
	usb_ep_queue(in_ep, in_req, 0);
}

static void aicupg_trans_layer_rx_cbw(struct usb_ep *out_ep,
				      struct usb_request *out_req)
{
	unsigned int maxpacket, rem;
	struct aic_cbw cbw;

	if (out_req->status || out_req->length == 0)
		return;
	memcpy(&cbw, out_req->buf, USB_CBW_LENGTH);

	debug("\n%s, tag = 0x%x, data_transfer_length = %d\n", __func__, cbw.tag,
	      cbw.data_transfer_length);
	if (cbw.cmd == AICUPG_USB_CMD_WRITE_PKT) {
		/*
		 * Host write data packet to device, next is going to receive
		 * one data packet.
		 */
		if (cbw.data_transfer_length > 0) {
			aicupg_func->tag = cbw.tag;
			out_req->complete = aicupg_trans_layer_write_pkt;
		}
	} else if (cbw.cmd == AICUPG_USB_CMD_READ_PKT) {
		/*
		 * Host read data packet from device, here read one pakcet
		 * data and setup to send out.
		 */
		if (cbw.data_transfer_length > 0) {
			aicupg_func->tag = cbw.tag;
			aicupg_func->in_req->length = cbw.data_transfer_length;
			aicupg_trans_layer_read_pkt(aicupg_func->in_ep,
						    aicupg_func->in_req);
		}
	} else {
		pr_err("%s() Invalid CBW.tag %#x, CBW.cmd %#x\n", __func__,
			cbw.tag, cbw.cmd);
		return;
	}

	out_req->actual = 0;
	/*
	 * Out ep transfer always expect the length is integral multiple of
	 * maxpackets.
	 */
	maxpacket = usb_endpoint_maxp(out_ep->desc);
	rem = cbw.data_transfer_length % maxpacket;
	out_req->length = cbw.data_transfer_length;
	if (rem)
		out_req->length += (maxpacket - rem);
	if (out_req->length > USB_UPG_BUF_SIZE)
		out_req->length = USB_UPG_BUF_SIZE;
	usb_ep_queue(out_ep, out_req, 0);
}

static int aicupg_add(struct usb_configuration *c)
{
	struct f_aicupgusb *f_upg = aicupg_func;
	int ret;

	if (!f_upg) {
		f_upg = memalign(CONFIG_SYS_CACHELINE_SIZE, sizeof(*f_upg));
		if (!f_upg)
			return -ENOMEM;

		aicupg_func = f_upg;
		memset(f_upg, 0, sizeof(*f_upg));
	}

	f_upg->usb_function.name = "f_aicupg";
	f_upg->usb_function.bind = aicupg_bind;
	f_upg->usb_function.unbind = aicupg_unbind;
	f_upg->usb_function.set_alt = aicupg_set_alt;
	f_upg->usb_function.disable = aicupg_disable;
	f_upg->usb_function.strings = aicupg_strings;

	ret = usb_add_function(c, &f_upg->usb_function);
	if (ret) {
		free(f_upg);
		aicupg_func = NULL;
	}
	return ret;
}

DECLARE_GADGET_BIND_CALLBACK(usb_dnl_aicupg, aicupg_add);
