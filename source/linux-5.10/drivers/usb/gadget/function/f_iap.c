/*
 * Gadget Function Driver for Android USB accessories
 *
 * Copyright (C) 2011 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* #define DEBUG */
/* #define VERBOSE_DEBUG */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/freezer.h>

#include <linux/types.h>
#include <linux/file.h>
#include <linux/device.h>
#include <linux/miscdevice.h>

#include <linux/usb.h>
#include <linux/usb/ch9.h>

#include <linux/configfs.h>
#include <linux/usb/composite.h>

#define MAX_INST_NAME_LEN        40
#define BULK_BUFFER_SIZE    4096
#define ACC_STRING_SIZE     256

/* String IDs */
#define INTERFACE_STRING_INDEX	0

/* number of tx and rx requests to allocate */
#define TX_REQ_MAX 4
#define RX_REQ_MAX 2

#define GET_IAP_STATE               _IOR('m', 1, int)

struct iap_dev {
	struct usb_function function;
	struct usb_composite_dev *cdev;
	spinlock_t lock;

	struct usb_ep *ep_in;
	struct usb_ep *ep_out;

	/* online indicates state of function_set_alt & function_unbind
	 * set to 1 when we connect
	 */
	int online:1;

	/* disconnected indicates state of open & release
	 * Set to 1 when we disconnect.
	 * Not cleared until our file is closed.
	 */
	int disconnected:1;
	
	/* for iap_complete_set_string */
	int string_index;

	/* set to 1 if we have a pending start request */
	int start_requested;

	int audio_mode;

	/* synchronize access to our device file */
	atomic_t open_excl;

	struct list_head tx_idle;

	wait_queue_head_t read_wq;
	wait_queue_head_t write_wq;
	struct usb_request *rx_req[RX_REQ_MAX];
	int rx_done;

	/* delayed work for handling IAP_START */
	struct delayed_work start_work;
};

static struct usb_interface_descriptor iap_interface_desc = {
	.bLength                = USB_DT_INTERFACE_SIZE,
	.bDescriptorType        = USB_DT_INTERFACE,
	.bInterfaceNumber       = 0,
	.bNumEndpoints          = 2,
	.bInterfaceClass        = 0xFF,
	.bInterfaceSubClass     = 0xF0,
	.bInterfaceProtocol     = 0,
};

static struct usb_endpoint_descriptor iap_highspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor iap_highspeed_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor iap_fullspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor iap_fullspeed_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *fs_iap_descs[] = {
	(struct usb_descriptor_header *) &iap_interface_desc,
	(struct usb_descriptor_header *) &iap_fullspeed_in_desc,
	(struct usb_descriptor_header *) &iap_fullspeed_out_desc,
	NULL,
};

static struct usb_descriptor_header *hs_iap_descs[] = {
	(struct usb_descriptor_header *) &iap_interface_desc,
	(struct usb_descriptor_header *) &iap_highspeed_in_desc,
	(struct usb_descriptor_header *) &iap_highspeed_out_desc,
	NULL,
};

static struct usb_string iap_string_defs[] = {
	[INTERFACE_STRING_INDEX].s	= "iAP Interface",
	{  },	/* end of list */
};

static struct usb_gadget_strings iap_string_table = {
	.language		= 0x0409,	/* en-US */
	.strings		= iap_string_defs,
};

static struct usb_gadget_strings *iap_strings[] = {
	&iap_string_table,
	NULL,
};

/* temporary variable used between iap_open() and iap_gadget_bind() */
static struct iap_dev *_iap_dev;

struct iap_instance {
	struct usb_function_instance func_inst;
	const char *name;
};

static inline struct iap_dev *func_to_dev(struct usb_function *f)
{
	return container_of(f, struct iap_dev, function);
}

static struct usb_request *iap_request_new(struct usb_ep *ep, int buffer_size)
{
	struct usb_request *req = usb_ep_alloc_request(ep, GFP_KERNEL);

	if (!req)
		return NULL;

	/* now allocate buffers for the requests */
	req->buf = kmalloc(buffer_size, GFP_KERNEL);
	if (!req->buf) {
		usb_ep_free_request(ep, req);
		return NULL;
	}

	return req;
}

static void iap_request_free(struct usb_request *req, struct usb_ep *ep)
{
	if (req) {
		kfree(req->buf);
		usb_ep_free_request(ep, req);
	}
}

/* add a request to the tail of a list */
static void req_put(struct iap_dev *dev, struct list_head *head,
		struct usb_request *req)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	list_add_tail(&req->list, head);
	spin_unlock_irqrestore(&dev->lock, flags);
}

/* remove a request from the head of a list */
static struct usb_request *req_get(struct iap_dev *dev, struct list_head *head)
{
	unsigned long flags;
	struct usb_request *req;

	spin_lock_irqsave(&dev->lock, flags);
	if (list_empty(head)) {
		req = 0;
	} else {
		req = list_first_entry(head, struct usb_request, list);
		list_del(&req->list);
	}
	spin_unlock_irqrestore(&dev->lock, flags);
	return req;
}

static void iap_set_disconnected(struct iap_dev *dev)
{
	printk(KERN_INFO "iap_set_disconnected\n");

	dev->disconnected = 1;
    dev->online = 0;
}

static void iap_complete_in(struct usb_ep *ep, struct usb_request *req)
{
	struct iap_dev *dev = _iap_dev;

	if (req->status == -ESHUTDOWN) {
		pr_debug("iap_complete_in set disconnected");
		iap_set_disconnected(dev);
	}

	req_put(dev, &dev->tx_idle, req);

	wake_up(&dev->write_wq);
}

static void iap_complete_out(struct usb_ep *ep, struct usb_request *req)
{
	struct iap_dev *dev = _iap_dev;

	dev->rx_done = 1;
	if (req->status == -ESHUTDOWN) {
		pr_debug("iap_complete_out set disconnected");
		iap_set_disconnected(dev);
	}

	wake_up(&dev->read_wq);
}

static int create_bulk_endpoints(struct iap_dev *dev,
				struct usb_endpoint_descriptor *in_desc,
				struct usb_endpoint_descriptor *out_desc)
{
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req;
	struct usb_ep *ep;
	int i;

	DBG(cdev, "create_bulk_endpoints dev: %p\n", dev);

	ep = usb_ep_autoconfig(cdev->gadget, in_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for ep_in failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for ep_in got %s\n", ep->name);
	ep->driver_data = dev;		/* claim the endpoint */
	dev->ep_in = ep;

	ep = usb_ep_autoconfig(cdev->gadget, out_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for ep_out failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for ep_out got %s\n", ep->name);
	ep->driver_data = dev;		/* claim the endpoint */
	dev->ep_out = ep;

	/* now allocate requests for our endpoints */
	for (i = 0; i < TX_REQ_MAX; i++) {
		req = iap_request_new(dev->ep_in, BULK_BUFFER_SIZE);
		if (!req)
			goto fail;
		req->complete = iap_complete_in;
		req_put(dev, &dev->tx_idle, req);
	}
	for (i = 0; i < RX_REQ_MAX; i++) {
		req = iap_request_new(dev->ep_out, BULK_BUFFER_SIZE);
		if (!req)
			goto fail;
		req->complete = iap_complete_out;
		dev->rx_req[i] = req;
	}

	return 0;

fail:
	pr_err("iap_bind() could not allocate requests\n");
	while ((req = req_get(dev, &dev->tx_idle)))
		iap_request_free(req, dev->ep_in);
	for (i = 0; i < RX_REQ_MAX; i++)
		iap_request_free(dev->rx_req[i], dev->ep_out);
	return -1;
}

static ssize_t iap_read(struct file *fp, char __user *buf,
	size_t count, loff_t *pos)
{
	struct iap_dev *dev = fp->private_data;
	struct usb_request *req;
	ssize_t r = count;
	unsigned xfer;
	int ret = 0;

	pr_debug("iap_read(%zu)\n", count);

	if (dev->disconnected) {
		pr_debug("iap_read disconnected");
		return -ENODEV;
	}

	if (count > BULK_BUFFER_SIZE)
		count = BULK_BUFFER_SIZE;

	/* we will block until we're online */
	pr_debug("iap_read: waiting for online\n");
	ret = wait_event_interruptible(dev->read_wq, dev->online);

	if (ret < 0) {
		r = ret;
		goto done;
	}

	if (dev->rx_done) {
		// last req cancelled. try to get it.
		req = dev->rx_req[0];
		goto copy_data;
	}

requeue_req:
	/* queue a request */
	req = dev->rx_req[0];
	req->length = count;
	dev->rx_done = 0;
	ret = usb_ep_queue(dev->ep_out, req, GFP_KERNEL);
	if (ret < 0) {
		r = -EIO;
		goto done;
	} else {
		pr_debug("rx %p queue\n", req);
	}

	/* wait for a request to complete */
	//ret = wait_event_interruptible(dev->read_wq, dev->rx_done);
	ret = wait_event_interruptible_timeout(dev->read_wq, dev->rx_done, msecs_to_jiffies(3000));

	if (ret < 0) {
		r = ret;
		ret = usb_ep_dequeue(dev->ep_out, req);
		if (ret != 0) {
			// cancel failed. There can be a data already received.
			// it will be retrieved in the next read.
			pr_debug("iap_read: cancelling failed %d", ret);
		}
		goto done;
	}
	else if (0 == ret)
    {
		ret = usb_ep_dequeue(dev->ep_out, req);
		r = -ETIME;
		goto done;
    }

copy_data:
	dev->rx_done = 0;
	if (dev->online) {
		/* If we got a 0-len packet, throw it back and try again. */
		if (req->actual == 0)
			goto requeue_req;

		pr_debug("rx %p %u\n", req, req->actual);
		xfer = (req->actual < count) ? req->actual : count;
		r = xfer;
		if (copy_to_user(buf, req->buf, xfer))
			r = -EFAULT;
	} else
		r = -EIO;

done:
	pr_debug("iap_read returning %zd\n", r);
	return r;
}

static ssize_t iap_write(struct file *fp, const char __user *buf,
	size_t count, loff_t *pos)
{
	struct iap_dev *dev = fp->private_data;
	struct usb_request *req = 0;
	ssize_t r = count;
	unsigned xfer;
	int ret;

	pr_debug("iap_write(%zu)\n", count);

	if (!dev->online || dev->disconnected) {
		pr_debug("iap_write disconnected or not online");
		return -ENODEV;
	}

	while (count > 0) {
		if (!dev->online) {
			pr_debug("iap_write dev->error\n");
			r = -EIO;
			break;
		}

		/* get an idle tx request to use */
		req = 0;
		ret = wait_event_interruptible(dev->write_wq,
			((req = req_get(dev, &dev->tx_idle)) || !dev->online));
		if (!req) {
			r = ret;
			break;
		}

		if (count > BULK_BUFFER_SIZE) {
			xfer = BULK_BUFFER_SIZE;
			/* ZLP, They will be more TX requests so not yet. */
			req->zero = 0;
		} else {
			xfer = count;
			/* If the data length is a multple of the
			 * maxpacket size then send a zero length packet(ZLP).
			*/
			req->zero = ((xfer % dev->ep_in->maxpacket) == 0);
		}
		if (copy_from_user(req->buf, buf, xfer)) {
			r = -EFAULT;
			break;
		}

		req->length = xfer;
		ret = usb_ep_queue(dev->ep_in, req, GFP_KERNEL);
		if (ret < 0) {
			pr_debug("iap_write: xfer error %d\n", ret);
			r = -EIO;
			break;
		}

		buf += xfer;
		count -= xfer;

		/* zero this so we don't try to free it on error exit */
		req = 0;
	}

	if (req)
		req_put(dev, &dev->tx_idle, req);

	pr_debug("iap_write returning %zd\n", r);
	return r;
}

static long iap_ioctl(struct file *fp, unsigned code, unsigned long value)
{
	struct iap_dev *dev = fp->private_data;
	int ret = -EINVAL;

	printk(KERN_INFO "iap_ioctl from ltlink\n");
	
	if (GET_IAP_STATE == code)
	{
		int status = dev->online ? 0x1 : 0x0;

		if (copy_to_user((void __user *)value, &status, sizeof(status)))
		{
			ret = -EFAULT;
		}
		else
		{
			ret = sizeof(status);
		}
	}

	return ret;
}

static int iap_open(struct inode *ip, struct file *fp)
{
	printk(KERN_INFO "iap_open from ltlink\n");

	if (atomic_xchg(&_iap_dev->open_excl, 1))
		return -EBUSY;
	
	_iap_dev->disconnected = 0;
	fp->private_data = _iap_dev;

	return 0;
}

static int iap_release(struct inode *ip, struct file *fp)
{
	printk(KERN_INFO "iap_release from ltlink\n");

	//WARN_ON(!atomic_xchg(&_iap_dev->open_excl, 0));
	atomic_xchg(&_iap_dev->open_excl, 0);
	/* indicate that we are disconnected
	 * still could be online so don't touch online flag
	 */
	_iap_dev->disconnected = 1;
	return 0;
}

/* file operations for /dev/usb_iap */
static const struct file_operations iap_fops = {
	.owner = THIS_MODULE,
	.read = iap_read,
	.write = iap_write,
	.unlocked_ioctl = iap_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = iap_ioctl,
#endif
	.open = iap_open,
	.release = iap_release,
};

static struct miscdevice iap_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "usb_iap",
	.fops = &iap_fops,
};

static int
__iap_function_bind(struct usb_configuration *c,
			struct usb_function *f, bool configfs)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct iap_dev	*dev = func_to_dev(f);
	int			id;
	int			ret;

	DBG(cdev, "iap_function_bind dev: %p\n", dev);
	printk(KERN_INFO "iap_function_bind");

	/*if (configfs) {
		if (iap_string_defs[INTERFACE_STRING_INDEX].id == 0) {
			ret = usb_string_id(c->cdev);
			if (ret < 0)
				return ret;
			iap_string_defs[INTERFACE_STRING_INDEX].id = ret;
			iap_interface_desc.iInterface = ret;
		}
		dev->cdev = c->cdev;
	}*/
	dev->cdev = c->cdev;
	dev->start_requested = 0;

	/* allocate interface ID(s) */
	id = usb_interface_id(c, f);
	if (id < 0)
		return id;

	iap_interface_desc.bInterfaceNumber = id;

	ret = usb_string_id(c->cdev);
	if (ret < 0)
		return ret;
	iap_string_defs[INTERFACE_STRING_INDEX].id = ret;
	iap_interface_desc.iInterface = ret;

	/* allocate endpoints */
	ret = create_bulk_endpoints(dev, &iap_fullspeed_in_desc,
			&iap_fullspeed_out_desc);
	if (ret)
		return ret;

	/* support high speed hardware */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		iap_highspeed_in_desc.bEndpointAddress =
			iap_fullspeed_in_desc.bEndpointAddress;
		iap_highspeed_out_desc.bEndpointAddress =
			iap_fullspeed_out_desc.bEndpointAddress;
	}

	DBG(cdev, "%s speed %s: IN/%s, OUT/%s\n",
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
			f->name, dev->ep_in->name, dev->ep_out->name);
	return 0;
}

static int
iap_function_bind_configfs(struct usb_configuration *c,
			struct usb_function *f) {
	return __iap_function_bind(c, f, true);
}

static void
iap_function_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct iap_dev	*dev = func_to_dev(f);
	struct usb_request *req;
	int i;
	
	printk(KERN_INFO "iap_function_unbind\n");

	dev->online = 0;		/* clear online flag */
	wake_up(&dev->read_wq);		/* unblock reads on closure */
	wake_up(&dev->write_wq);	/* likewise for writes */

	while ((req = req_get(dev, &dev->tx_idle)))
		iap_request_free(req, dev->ep_in);
	for (i = 0; i < RX_REQ_MAX; i++)
		iap_request_free(dev->rx_req[i], dev->ep_out);
}

static void iap_start_work(struct work_struct *data)
{
	char *envp[2] = { "IAP=START", NULL };
	
	printk(KERN_INFO "iap_start_work from ltlink\n");

	kobject_uevent_env(&iap_device.this_device->kobj, KOBJ_CHANGE, envp);
}

static int iap_function_set_alt(struct usb_function *f,
		unsigned intf, unsigned alt)
{
	struct iap_dev	*dev = func_to_dev(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	int ret;

	//DBG(cdev, "iap_function_set_alt intf: %d alt: %d\n", intf, alt);
	printk(KERN_INFO "iap_function_set_alt intf: %d alt: %d from ltlink\n", intf, alt);

	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_in);
	if (ret)
		return ret;

	ret = usb_ep_enable(dev->ep_in);
	if (ret)
		return ret;

	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_out);
	if (ret)
		return ret;

	ret = usb_ep_enable(dev->ep_out);
	if (ret) {
		usb_ep_disable(dev->ep_in);
		return ret;
	}

	dev->online = 1;
	dev->disconnected = 0; /* if online then not disconnected */

	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);

	schedule_delayed_work(
	&dev->start_work, msecs_to_jiffies(10));

	return 0;
}

static void iap_function_disable(struct usb_function *f)
{
	struct iap_dev	*dev = func_to_dev(f);
	struct usb_composite_dev	*cdev = dev->cdev;

	DBG(cdev, "iap_function_disable\n");
	printk(KERN_INFO "iap_function_disable\n");
	iap_set_disconnected(dev); /* this now only sets disconnected */
	dev->online = 0; /* so now need to clear online flag here too */
	usb_ep_disable(dev->ep_in);
	usb_ep_disable(dev->ep_out);

	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);

	VDBG(cdev, "%s disabled\n", dev->function.name);
}

static int iap_setup(void)
{
	struct iap_dev *dev;
	int ret;
	
	printk(KERN_INFO "iap_setup\n");

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	spin_lock_init(&dev->lock);
	init_waitqueue_head(&dev->read_wq);
	init_waitqueue_head(&dev->write_wq);
	atomic_set(&dev->open_excl, 0);
	INIT_LIST_HEAD(&dev->tx_idle);

	INIT_DELAYED_WORK(&dev->start_work, iap_start_work);

	/* _iap_dev must be set before calling usb_gadget_register_driver */
	_iap_dev = dev;

	ret = misc_register(&iap_device);
	if (ret)
		goto err;

	return 0;

err:
	kfree(dev);
	pr_err("USB iap gadget driver failed to initialize\n");
	return ret;
}

void iap_disconnect(void)
{
	printk(KERN_INFO "iap_disconnect\n");
}
EXPORT_SYMBOL_GPL(iap_disconnect);

static void iap_cleanup(void)
{
	misc_deregister(&iap_device);
	kfree(_iap_dev);
	_iap_dev = NULL;
}
static struct iap_instance *to_iap_instance(struct config_item *item)
{
	return container_of(to_config_group(item), struct iap_instance,
		func_inst.group);
}

static void iap_attr_release(struct config_item *item)
{
	struct iap_instance *fi_acc = to_iap_instance(item);

	usb_put_function_instance(&fi_acc->func_inst);
}

static struct configfs_item_operations iap_item_ops = {
	.release        = iap_attr_release,
};

static struct config_item_type iap_func_type = {
	.ct_item_ops    = &iap_item_ops,
	.ct_owner       = THIS_MODULE,
};

static struct iap_instance *to_fi_acc(struct usb_function_instance *fi)
{
	return container_of(fi, struct iap_instance, func_inst);
}

static int iap_set_inst_name(struct usb_function_instance *fi, const char *name)
{
	struct iap_instance *fi_acc;
	char *ptr;
	int name_len;

	name_len = strlen(name) + 1;
	if (name_len > MAX_INST_NAME_LEN)
		return -ENAMETOOLONG;

	ptr = kstrndup(name, name_len, GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	fi_acc = to_fi_acc(fi);
	fi_acc->name = ptr;
	return 0;
}

static void iap_free_inst(struct usb_function_instance *fi)
{
	struct iap_instance *fi_acc;

	fi_acc = to_fi_acc(fi);
	kfree(fi_acc->name);
	iap_cleanup();
}

static struct usb_function_instance *iap_alloc_inst(void)
{
	struct iap_instance *fi_acc;
	struct iap_dev *dev;
	int err;

	fi_acc = kzalloc(sizeof(*fi_acc), GFP_KERNEL);
	if (!fi_acc)
		return ERR_PTR(-ENOMEM);
	fi_acc->func_inst.set_inst_name = iap_set_inst_name;
	fi_acc->func_inst.free_func_inst = iap_free_inst;

	err = iap_setup();
	if (err) {
		kfree(fi_acc);
		pr_err("Error setting IAP\n");
		return ERR_PTR(err);
	}

	config_group_init_type_name(&fi_acc->func_inst.group,
					"", &iap_func_type);
	dev = _iap_dev;
	return  &fi_acc->func_inst;
}

static void iap_free(struct usb_function *f)
{
/*NO-OP: no function specific resource allocation in mtp_alloc*/
}

static struct usb_function *iap_alloc(struct usb_function_instance *fi)
{
	struct iap_dev *dev = _iap_dev;

	pr_info("iap_alloc\n");

	dev->function.name = "iap";
	dev->function.strings = iap_strings,
	dev->function.fs_descriptors = fs_iap_descs;
	dev->function.hs_descriptors = hs_iap_descs;
	dev->function.bind = iap_function_bind_configfs;
	dev->function.unbind = iap_function_unbind;
	dev->function.set_alt = iap_function_set_alt;
	dev->function.disable = iap_function_disable;
	dev->function.free_func = iap_free;

	return &dev->function;
}
DECLARE_USB_FUNCTION_INIT(iap, iap_alloc_inst, iap_alloc);
MODULE_LICENSE("GPL");
