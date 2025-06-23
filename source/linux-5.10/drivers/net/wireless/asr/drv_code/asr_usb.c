/**
 ******************************************************************************
 *
 * @file asr_usb.c
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/usb.h>
#include <linux/vmalloc.h>
#include <linux/ktime.h>
#include "asr_defs.h"
#include "asr_usb.h"
#include "asr_hif.h"
#include "asr_utils.h"
#include "asr_main.h"
#include "asr_bus.h"
#include "usb_rdl.h"
#include "usb.h"
#include "ipc_host.h"
#include "lmac_msg.h"
#include "asr_platform.h"

#define IOCTL_RESP_TIMEOUT  2000

#define ASR_USB_RESET_GETVER_SPINWAIT    100	/* in unit of ms */
#define ASR_USB_RESET_GETVER_LOOP_CNT    10

#define ASR_USB_NRXQ    50
#define ASR_USB_NTXQ    256

#define ASR_USB_NCTLQ	5

#define CONFIGDESC(usb)         (&((usb)->actconfig)->desc)
#define IFPTR(usb, idx)         ((usb)->actconfig->interface[(idx)])
#define IFALTS(usb, idx)        (IFPTR((usb), (idx))->altsetting[0])
#define IFDESC(usb, idx)        IFALTS((usb), (idx)).desc
#define IFEPDESC(usb, idx, ep)  (IFALTS((usb), (idx)).endpoint[(ep)]).desc

#define CONTROL_IF              0
#define BULK_IF                 0

#define ASR_USB_CBCTL_WRITE    0
#define ASR_USB_CBCTL_READ     1
#define ASR_USB_MAX_PKT_SIZE   1600

#define ASR_USB_5531_FW_NAME   ASR_MAC_FW_NAME

//#define DONT_ENUM_AFTER_DL_FW

extern int downloadATE;

struct asr_usb_image {
	struct list_head list;
	s8 *fwname;
	u8 *image;
	int image_len;
};
static struct list_head fw_image_list;

static void asr_usb_rx_refill(struct asr_usbdev_info *devinfo, struct asr_usbreq *req);

static struct asr_usbdev *asr_usb_get_buspub(struct device *dev)
{
	struct asr_bus *bus_if = dev_get_drvdata(dev);
	return bus_if->bus_priv.usb;
}

static struct asr_usbdev_info *asr_usb_get_businfo(struct device *dev)
{
	return asr_usb_get_buspub(dev)->devinfo;
}

static int asr_usb_ioctl_resp_wait(struct asr_usbdev_info *devinfo)
{
	return wait_event_timeout(devinfo->ioctl_resp_wait,
				  devinfo->ctl_completed, msecs_to_jiffies(IOCTL_RESP_TIMEOUT));
}

static void asr_usb_ioctl_resp_wake(struct asr_usbdev_info *devinfo)
{
	if (waitqueue_active(&devinfo->ioctl_resp_wait))
		wake_up(&devinfo->ioctl_resp_wait);
}

static void *asr_usb_deq(spinlock_t *qlock, struct list_head *q, int *counter)
{
	unsigned long flags;
	struct asr_usbreq *req;
	spin_lock_irqsave(qlock, flags);
	if (list_empty(q)) {
		spin_unlock_irqrestore(qlock, flags);
		return NULL;
	}
	req = list_entry(q->next, struct asr_usbreq, list);
	list_del_init(q->next);
	if (counter)
		(*counter)--;
	spin_unlock_irqrestore(qlock, flags);
	return req;
}

static void asr_usb_enq(spinlock_t *qlock, struct list_head *q, struct list_head *list, int *counter)
{
	unsigned long flags;
	spin_lock_irqsave(qlock, flags);
	list_add_tail(list, q);
	if (counter)
		(*counter)++;
	spin_unlock_irqrestore(qlock, flags);
}

static void asr_usb_del_fromq(spinlock_t *qlock, struct list_head *list)
{
	unsigned long flags;

	spin_lock_irqsave(qlock, flags);
	list_del_init(list);
	spin_unlock_irqrestore(qlock, flags);
}

static struct asr_usbctlreq *asr_usb_ctl_qinit(struct list_head *q, int qsize)
{
	int i;
	struct asr_usbctlreq *req, *reqs;

	reqs = kcalloc(qsize, sizeof(struct asr_usbctlreq), GFP_ATOMIC);
	if (reqs == NULL)
		return NULL;

	req = reqs;

	for (i = 0; i < qsize; i++) {
		req->urb = usb_alloc_urb(0, GFP_ATOMIC);
		if (!req->urb)
			goto fail;

		init_waitqueue_head(&req->resp_wait);

		INIT_LIST_HEAD(&req->list);
		list_add_tail(&req->list, q);
		req++;
	}
	return reqs;

fail:
	asr_err("fail!\n");
	while (!list_empty(q)) {
		req = list_entry(q->next, struct asr_usbctlreq, list);
		if (req && req->urb)
			usb_free_urb(req->urb);
		list_del(q->next);
	}
	return NULL;
}

#ifdef CONFIG_ASR_USB_PM
#include "asr_msg_tx.h"
static void usb_pm_cmd_work_func(struct work_struct *work)
{
	struct asr_usbdev_info *devinfo = container_of(work, struct asr_usbdev_info, pm_cmd_work.work);
	struct usb_device *udev = devinfo->usbdev;
	struct device *dev = &udev->dev;
	struct asr_hw *asr_hw = devinfo->bus_pub.bus->asr_hw;
	char *cmd = asr_hw->mod_params->usb_pm_cmd;
	int ret = 0;

	if (strlen(cmd)) {
		asr_dbg(TRACE, "at cmd: %s\n", cmd);

		if (strncmp(cmd, "enable", strlen("enable")) == 0) {
			usb_enable_autosuspend(udev);
		} else if (strncmp(cmd, "disable", strlen("disable")) == 0) {
			usb_disable_autosuspend(udev);
		} else if (strncmp(cmd, "suspend", strlen("suspend")) == 0) {
			pm_runtime_mark_last_busy(dev);
			ret = pm_runtime_put_sync_autosuspend(dev);
		} else if (strncmp(cmd, "resume", strlen("resume")) == 0) {
			ret = pm_runtime_get_sync(dev);
		} else if (strncmp(cmd, "version", strlen("version")) == 0) {
			if (!asr_send_fw_softversion_req(asr_hw, &asr_hw->fw_softversion_cfm))
				asr_dbg(TRACE, "fw version: %s\n", asr_hw->fw_softversion_cfm.fw_softversion);
		} else if (strncmp(cmd, "spnd_delay", strlen("spnd_delay")) == 0) {
			char *pmsecs = strchr(cmd, '=');
			if (pmsecs) {
				pmsecs++; //point to delay time
				devinfo->autosuspend_delay = simple_strtol(pmsecs, NULL, 0);
				asr_dbg(TRACE, "auto suspend delay %dms\n", devinfo->autosuspend_delay);
				pm_runtime_set_autosuspend_delay(&udev->dev, devinfo->autosuspend_delay);
			}
		}

		asr_dbg(TRACE, "runtime op result: %d", ret);
		asr_dbg(TRACE, "usage count: %d, runtime auto: %d, "
				"use_autosuspend: %d, autosuspend_delay: %d\n",
				atomic_read(&dev->power.usage_count),
				dev->power.runtime_auto,
				dev->power.use_autosuspend,
				dev->power.autosuspend_delay);
		memset(cmd, 0x00, 20);
	}

	schedule_delayed_work(&devinfo->pm_cmd_work,
		msecs_to_jiffies(ASR_ATE_AT_CMD_TIMER_OUT));
}

static void asr_usb_enable_autosuspend(struct asr_usbdev_info *devinfo)
{
	struct usb_device *udev = devinfo->usbdev;
	pm_runtime_mark_last_busy(&udev->dev);
	pm_runtime_set_autosuspend_delay(&udev->dev, devinfo->autosuspend_delay);
	usb_disable_autosuspend(udev);
}

static int asr_usb_pm_init(struct asr_usbdev_info *devinfo)
{
	/* Initialize delayed work for asr cmd */
	INIT_DELAYED_WORK(&devinfo->pm_cmd_work, usb_pm_cmd_work_func);
	schedule_delayed_work(&devinfo->pm_cmd_work,
		msecs_to_jiffies(ASR_ATE_AT_CMD_TIMER_OUT));

	init_waitqueue_head(&devinfo->pm_waitq);

	devinfo->autosuspend_delay = 2000;
	asr_usb_enable_autosuspend(devinfo);

	return 0;
}

static inline int asr_usb_pm_acquire(struct asr_usbdev_info *devinfo)
{
	int	status;
	struct usb_device *udev = devinfo->usbdev;

	status = pm_runtime_get(&udev->dev);
	if (status < 0 && status != -EINPROGRESS) {
		asr_err("pm runtime get error %d", status);
		return status;
	}

	wait_event_timeout(devinfo->pm_waitq,
						devinfo->bus_pub.state == ASRMAC_USB_STATE_UP,
						msecs_to_jiffies(10000));
	return 0;
}

static inline int asr_usb_pm_release(struct usb_device *udev)
{
	pm_runtime_mark_last_busy(&udev->dev);
	return pm_runtime_put_sync_autosuspend(&udev->dev);
}
#endif

static int asr_usb_ctl_resp_wait(struct asr_usbctlreq *req)
{
	return wait_event_timeout(req->resp_wait,
				  req->completed, msecs_to_jiffies(IOCTL_RESP_TIMEOUT));
}

static void asr_usb_ctl_resp_wake(struct asr_usbctlreq *req)
{
	if (waitqueue_active(&req->resp_wait))
		wake_up(&req->resp_wait);
}

static void asr_usb_ctl_complete(struct asr_usbdev_info *devinfo, int type, int status)
{
	asr_dbg(USB, "Enter, type=%d,status=%d\n", type, status);

	if (unlikely(devinfo == NULL))
		return;

	if (type == ASR_USB_CBCTL_READ) {
		if (status == 0)
			devinfo->bus_pub.stats.rx_ctlpkts++;
		else
			devinfo->bus_pub.stats.rx_ctlerrs++;
	} else if (type == ASR_USB_CBCTL_WRITE) {
		if (status == 0)
			devinfo->bus_pub.stats.tx_ctlpkts++;
		else
			devinfo->bus_pub.stats.tx_ctlerrs++;
	}

	devinfo->ctl_urb_status = status;
	devinfo->ctl_completed = true;
	asr_usb_ioctl_resp_wake(devinfo);
}

static void asr_usb_ctlread_complete(struct urb *urb)
{
	struct asr_usbdev_info *devinfo = (struct asr_usbdev_info *)urb->context;

	asr_dbg(USB, "Enter\n");
	devinfo->ctl_urb_actual_length = urb->actual_length;
	asr_usb_ctl_complete(devinfo, ASR_USB_CBCTL_READ, urb->status);
}

static int asr_usb_recv_ctl(struct asr_usbdev_info *devinfo, u8 * buf, int len)
{
	int ret;
	u16 size;

	asr_dbg(USB, "Enter\n");
	if ((devinfo == NULL) || (buf == NULL) || (len == 0)
	    || (devinfo->ctl_urb == NULL))
		return -EINVAL;

	size = len;
	devinfo->ctl_read.wLength = cpu_to_le16p(&size);
	devinfo->ctl_urb->transfer_buffer_length = size;

	devinfo->ctl_read.bRequestType = USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE;
	devinfo->ctl_read.bRequest = 1;

	usb_fill_control_urb(devinfo->ctl_urb,
			     devinfo->usbdev,
			     devinfo->ctl_in_pipe,
			     (unsigned char *)&devinfo->ctl_read,
			     buf, size, (usb_complete_t) asr_usb_ctlread_complete, devinfo);

	ret = usb_submit_urb(devinfo->ctl_urb, GFP_ATOMIC);
	if (ret < 0)
		asr_err("usb_submit_urb failed %d\n", ret);

	return ret;
}

static void asr_usb_ctlwrite_complete(struct urb *urb)
{
	struct asr_usbctlreq *req = (struct asr_usbctlreq *)urb->context;
	struct asr_usbdev_info *devinfo;

	asr_dbg(USB, "Enter, status=%d\n", urb->status);

	if (unlikely(req == NULL))
		return;

	devinfo = req->devinfo;
	asr_usb_del_fromq(&devinfo->ctl_qlock, &req->list);

	if (urb->status == 0)
		devinfo->bus_pub.stats.tx_ctlpkts++;
	else
		devinfo->bus_pub.stats.tx_ctlerrs++;

	asr_usb_enq(&devinfo->ctl_qlock, &devinfo->ctl_tx_freeq, &req->list, NULL);
	req->completed = true;
	asr_usb_ctl_resp_wake(req);
}

static int asr_usb_tx_ctlpkt(struct device *dev, u8 * buf, u32 len)
{
	int ret = 0;
	u16 size;
	struct asr_usbdev_info *devinfo = asr_usb_get_businfo(dev);
	struct asr_usbctlreq *req;

	asr_dbg(USB, "Enter\n");
#ifdef CONFIG_ASR_USB_PM
	asr_usb_pm_acquire(devinfo);
#endif

	if (devinfo->bus_pub.state != ASRMAC_USB_STATE_UP) {
		asr_err("usb tx bus state:%d\n", devinfo->bus_pub.state);
		ret = -EIO;
		goto exit;
	}

	req = asr_usb_deq(&devinfo->ctl_qlock, &devinfo->ctl_tx_freeq, NULL);
	if (req == NULL) {
		asr_err("no req to send ctlpkt freecount\n");
		ret = -ENOMEM;
		goto exit;
	}

	size = len;
	req->devinfo = devinfo;
	memcpy(&req->ctl_req, &devinfo->ctl_write, sizeof(struct usb_ctrlrequest));
	req->ctl_req.wLength = cpu_to_le16p(&size);
	req->completed = false;

	usb_fill_control_urb(req->urb, devinfo->usbdev, devinfo->ctl_out_pipe,
						(unsigned char *) &req->ctl_req, buf, size,
						asr_usb_ctlwrite_complete, req);
	asr_usb_enq(&devinfo->ctl_qlock, &devinfo->ctl_tx_postq, &req->list, NULL);
	ret = usb_submit_urb(req->urb, GFP_ATOMIC);
	if (ret) {
		asr_err("asr_usb_tx_ctlpkt usb_submit_urb FAILED: %d", ret);
		asr_usb_del_fromq(&devinfo->ctl_qlock, &req->list);
		asr_usb_enq(&devinfo->ctl_qlock, &devinfo->ctl_tx_freeq, &req->list, NULL);
		goto exit;
	}

	ret = asr_usb_ctl_resp_wait(req);
	if (!ret) {
		asr_err("Txctl wait timed out\n");
		ret = -EIO;
	}

exit:
#ifdef CONFIG_ASR_USB_PM
	asr_usb_pm_release(devinfo->usbdev);
#endif
	return ret;
}

static int asr_usb_rx_ctlpkt(struct device *dev, u8 * buf, u32 len)
{
	int err = 0;
	int timeout = 0;
	struct asr_usbdev_info *devinfo = asr_usb_get_businfo(dev);

	asr_dbg(USB, "Enter\n");
	if (devinfo->bus_pub.state != ASRMAC_USB_STATE_UP)
		return -EIO;

	if (test_and_set_bit(0, &devinfo->ctl_op))
		return -EIO;

	devinfo->ctl_completed = false;
	err = asr_usb_recv_ctl(devinfo, buf, len);
	if (err) {
		asr_err("fail %d bytes: %d\n", err, len);
		clear_bit(0, &devinfo->ctl_op);
		return err;
	}
	timeout = asr_usb_ioctl_resp_wait(devinfo);
	err = devinfo->ctl_urb_status;
	clear_bit(0, &devinfo->ctl_op);
	if (!timeout) {
		asr_err("rxctl wait timed out\n");
		err = -EIO;
	}
	if (!err)
		return devinfo->ctl_urb_actual_length;
	else
		return err;
}

static struct asr_usbreq *asr_usbdev_qinit(struct list_head *q, int qsize)
{
	int i;
	struct asr_usbreq *req, *reqs;

	reqs = kcalloc(qsize, sizeof(struct asr_usbreq), GFP_ATOMIC);
	if (reqs == NULL)
		return NULL;

	req = reqs;

	for (i = 0; i < qsize; i++) {
		req->urb = usb_alloc_urb(0, GFP_ATOMIC);
		if (!req->urb)
			goto fail;

		INIT_LIST_HEAD(&req->list);
		list_add_tail(&req->list, q);
		req++;
	}
	return reqs;
fail:
	asr_err("fail!\n");
	while (!list_empty(q)) {
		req = list_entry(q->next, struct asr_usbreq, list);
		if (req && req->urb)
			usb_free_urb(req->urb);
		list_del(q->next);
	}
	return NULL;

}

static void asr_usb_free_q(struct list_head *q, bool pending)
{
	struct asr_usbreq *req, *next;
	int i = 0;
	list_for_each_entry_safe(req, next, q, list) {
		if (!req->urb) {
			asr_err("bad req\n");
			break;
		}
		i++;
		if (pending) {
			usb_kill_urb(req->urb);
		} else {
			usb_free_urb(req->urb);
			list_del_init(&req->list);
		}
	}
}

void asr_txcomplete(struct device *dev, struct sk_buff *txp, bool success)
{
	//struct asr_bus *bus_if = dev_get_drvdata(dev);

	int headroom = sizeof(struct hostdesc) + HOST_PAD_USB_LEN;
	skb_pull(txp, headroom);

	//if (!success)
	//    ifp->stats.tx_errors++;

	dev_kfree_skb_any(txp);
}

void asr_txflowblock_if(struct asr_vif *ifp, bool state)
{
	if (!ifp)
		return;

	asr_dbg(TRACE, "enter: state=%d\n", state);

	if (state) {
        netif_tx_stop_all_queues(ifp->ndev);
	} else {
        netif_tx_wake_all_queues(ifp->ndev);
	}
}

int tx_block_cnt;
int tx_unblock_cnt;
void asr_txflowblock(struct device *dev, bool state)
{
	struct asr_bus *bus_if = dev_get_drvdata(dev);
	struct asr_hw *asr_hw = bus_if->asr_hw;

	int i;

	asr_dbg(TRACE, "Enter\n");

	if (state == true)
		tx_block_cnt++;
	else if (state == false)
		tx_unblock_cnt++;
	asr_dbg(USB, "txq %d:%d\n",tx_block_cnt,tx_unblock_cnt);
	for (i = 0; i < asr_hw->vif_max_num; i++)
		asr_txflowblock_if(asr_hw->vif_table[i], state);
}

static void asr_usb_tx_complete(struct urb *urb)
{
	struct asr_usbreq *req;
	struct asr_usbdev_info *devinfo;

	req  = (struct asr_usbreq *)urb->context;
	if (req == NULL) {
		asr_err("tx req is null\n");
		return;
	}
	devinfo = req->devinfo;
#ifdef ASR_DRV_DEBUG_TIMER
	extern u32 g_usb_tx_cnt;
	g_usb_tx_cnt++;
#endif

	asr_dbg(USB, "Enter, urb->status=%d, skb=%p\n", urb->status, req->skb);
	asr_usb_del_fromq(&devinfo->qlock, &req->list);

	asr_txcomplete(devinfo->dev, req->skb, urb->status == 0);
	req->skb = NULL;
	urb->context = NULL;
	asr_usb_enq(&devinfo->qlock, &devinfo->tx_freeq, &req->list, &devinfo->tx_freecount);
	if (devinfo->tx_freecount > devinfo->tx_high_watermark && devinfo->tx_flowblock) {
		asr_dbg(USB, "txq enable %d\n",devinfo->tx_freecount);
		asr_txflowblock(devinfo->dev, false);
		devinfo->tx_flowblock = false;
	}

#ifdef CONFIG_ASR_USB_PM
	asr_usb_pm_release(devinfo->usbdev);
#endif
}

void asr_rx_frames(struct device *dev, struct sk_buff_head *skb_list)
{
	struct sk_buff *skb, *pnext;

	struct asr_bus *bus_if = dev_get_drvdata(dev);

	struct asr_hw *asr_hw = bus_if->asr_hw;

	u16 id = 0;		//MM_RESET_REQ id =0 and host can't recv from card.

	asr_dbg(USB, "Enter\n");

	skb_queue_walk_safe(skb_list, skb, pnext) {
		skb_unlink(skb, skb_list);

		// handle msg/data/log here.
		memcpy(&id, skb->data, sizeof(id));
		if ((id != RX_DATA_ID) && (id != RX_LOG_ID) && (id != RX_DESC_ID)) {
			asr_dbg(USB,
				"(id:0x%x,len:%d),skb_data(0x%x 0x%x 0x%x 0x%x 0x%x 0x%x)\n",
				id, skb->len, *(skb->data), *(skb->data + 1),
				*(skb->data + 2), *(skb->data + 3), *(skb->data + 4), *(skb->data + 5));
			//dev_err(asr_hw->dev, ("(id:0x%x,len:%d),skb_data(0x%x 0x%x 0x%x 0x%x 0x%x 0x%x)\n", id,skb->len,*(skb->data),*(skb->data+1),*(skb->data+2),*(skb->data+3),*(skb->data+4),*(skb->data+5));
		}

		if (id && asr_hw)
			ipc_host_process_rx_usb(id, asr_hw, skb);
	}
}

static void asr_usb_rx_complete(struct urb *urb)
{
	struct asr_usbreq *req;
	struct asr_usbdev_info *devinfo;
	struct sk_buff *skb;
	struct sk_buff_head skbq;
#ifdef ASR_DRV_DEBUG_TIMER
	extern u32 g_usb_rx_cnt;
#endif
	req = (struct asr_usbreq *)urb->context;
	if (req == NULL) {
		asr_err("rx req is null\n");
		return ;
	}
	devinfo = req->devinfo;

	asr_dbg(USB, "Enter, urb->status=%d\n", urb->status);
	asr_usb_del_fromq(&devinfo->qlock, &req->list);
	skb = req->skb;
	req->skb = NULL;
	urb->context = NULL;

	/* zero lenght packets indicate usb "failure". Do not refill */
	if (urb->status != 0 || !urb->actual_length) {
		if (skb)
			dev_kfree_skb_any(skb);
		//if (skb)
		//    skb_queue_tail(&asr_hw->rx_sk_list, skb);
		asr_dbg(USB, "rx failure, status(%d) len(%d)\n", urb->status, urb->actual_length);
		asr_usb_enq(&devinfo->qlock, &devinfo->rx_freeq, &req->list, &devinfo->rx_freecount);
		return;
	}

	if (devinfo->bus_pub.state == ASRMAC_USB_STATE_UP) {
		if (!skb) {
			asr_usb_enq(&devinfo->qlock, &devinfo->rx_freeq, &req->list, &devinfo->rx_freecount);
			asr_err("%d skb is null\r\n", __LINE__);
			return;
		}

#ifdef CONFIG_ASR_USB_PM
		asr_usb_pm_acquire(devinfo);
#endif
		skb_queue_head_init(&skbq);
		skb_queue_tail(&skbq, skb);
		asr_dbg(USB, "urb:pipe=0x%x,x_len=%d,actual_length=%d\n",
			urb->pipe, urb->transfer_buffer_length, urb->actual_length);
#ifdef ASR_DRV_DEBUG_TIMER
		g_usb_rx_cnt++;
#endif
		// no need to move tail pointer bcz skb will rechain to rx_sk_list.
        // new rx move skb_put to asr_rxdataind.
		//skb_put(skb, urb->actual_length);

		asr_rx_frames(devinfo->dev, &skbq);
		asr_usb_rx_refill(devinfo, req);
#ifdef CONFIG_ASR_USB_PM
		asr_usb_pm_release(devinfo->usbdev);
#endif
	} else {
		if (skb)
			dev_kfree_skb_any(skb);
		asr_dbg(USB, "%s rx state off %d\n", __func__, devinfo->bus_pub.state);
		asr_usb_enq(&devinfo->qlock, &devinfo->rx_freeq, &req->list, &devinfo->rx_freecount);
	}
	return;
}

static void asr_usb_rx_refill(struct asr_usbdev_info *devinfo, struct asr_usbreq *req)
{
	struct sk_buff *skb;
	int ret;
#ifdef ASR_DRV_DEBUG_TIMER
	extern u32 g_rx_failed_cnt, g_rx_all_cnt;
#endif

	if (!req || !devinfo)
		goto fail;

#if 1
	skb = dev_alloc_skb(devinfo->bus_pub.bus_mtu);
	if (!skb) {
		asr_dbg(USB, "skb alloc fail!! len=%d\n", devinfo->bus_pub.bus_mtu);
		asr_usb_enq(&devinfo->qlock, &devinfo->rx_freeq, &req->list, &devinfo->rx_freecount);
		goto fail;
	}
#else
	skb = skb_dequeue(&asr_hw->rx_sk_list);
	if (!skb) {
		dev_err(asr_hw->dev, "%s no more skb buffer\n", __func__);
		asr_usb_enq(&devinfo->qlock, &devinfo->rx_freeq, &req->list, NULL);
		return;
	}
#endif

	req->skb = skb;

	usb_fill_bulk_urb(req->urb, devinfo->usbdev, devinfo->rx_pipe,
			  skb->data, skb_tailroom(skb), asr_usb_rx_complete, req);
	req->devinfo = devinfo;
	asr_usb_enq(&devinfo->qlock, &devinfo->rx_postq, &req->list, NULL);

	ret = usb_submit_urb(req->urb, GFP_ATOMIC);
	if (ret) {
		asr_usb_del_fromq(&devinfo->qlock, &req->list);

		if (req->skb)
			dev_kfree_skb_any(req->skb);

		//if (req->skb)
		//    skb_queue_tail(&asr_hw->rx_sk_list, skb);
		asr_dbg(USB, "%s submit failed\n", __func__);

		req->skb = NULL;
		req->urb->context = NULL;
		asr_usb_enq(&devinfo->qlock, &devinfo->rx_freeq, &req->list, &devinfo->rx_freecount);
	}
#ifdef ASR_DRV_DEBUG_TIMER
	g_rx_all_cnt++;
#endif

	return;
fail:
#ifdef ASR_DRV_DEBUG_TIMER
	g_rx_failed_cnt++;
#endif
	return;
}

static void asr_usb_rx_fill_all(struct asr_usbdev_info *devinfo)
{
	struct asr_usbreq *req;
	int usb_rx_refill_cnt = 0;
	asr_dbg(USB, "Enter freecount %d\n", devinfo->rx_freecount);

	if (devinfo->bus_pub.state != ASRMAC_USB_STATE_UP) {
		asr_err("bus is not up=%d\n", devinfo->bus_pub.state);
		return;
	}
	while ((req = asr_usb_deq(&devinfo->qlock, &devinfo->rx_freeq, &devinfo->rx_freecount)) != NULL) {

		asr_usb_rx_refill(devinfo, req);

		if (++usb_rx_refill_cnt > 1000) {
			asr_dbg(USB, "refill time overflow,break\n");
			break;
		}
	}

	asr_dbg(USB, "done\n");
}

static void asr_usb_state_change(struct asr_usbdev_info *devinfo, int state)
{
	struct asr_bus *bcmf_bus = devinfo->bus_pub.bus;
	int old_state;

	asr_dbg(USB, "Enter, current state=%d, new state=%d\n", devinfo->bus_pub.state, state);

	if (devinfo->bus_pub.state == state)
		return;

	old_state = devinfo->bus_pub.state;
	devinfo->bus_pub.state = state;

	/* update state of upper layer */
	if (state == ASRMAC_USB_STATE_DOWN) {
		asr_dbg(USB, "DBUS is down\n");
		bcmf_bus->state = ASR_BUS_DOWN;
	} else if (state == ASRMAC_USB_STATE_UP) {
		asr_dbg(USB, "DBUS is up\n");
		bcmf_bus->state = ASR_BUS_DATA;
	} else {
		asr_dbg(USB, "DBUS current state=%d\n", state);
	}
}

/*static void asr_usb_intr_complete(struct urb *urb)
{
	struct asr_usbdev_info *devinfo = (struct asr_usbdev_info *)urb->context;
	int err;

	asr_dbg(USB, "Enter, urb->status=%d\n", urb->status);

	if (devinfo == NULL)
		return;

	if (unlikely(urb->status)) {
		if (urb->status == -ENOENT || urb->status == -ESHUTDOWN || urb->status == -ENODEV) {
			asr_usb_state_change(devinfo, ASRMAC_USB_STATE_DOWN);
		}
	}

	if (devinfo->bus_pub.state == ASRMAC_USB_STATE_DOWN) {
		asr_err("intr cb when DBUS down, ignoring\n");
		return;
	}

	if (devinfo->bus_pub.state == ASRMAC_USB_STATE_UP) {
		err = usb_submit_urb(devinfo->intr_urb, GFP_ATOMIC);
		if (err)
			asr_err("usb_submit_urb, err=%d\n", err);
	}
}*/

static int asr_usb_tx(struct device *dev, struct sk_buff *skb)
{
	struct asr_usbdev_info *devinfo = asr_usb_get_businfo(dev);
	struct asr_usbreq *req;
	int ret;
#ifdef ASR_DRV_DEBUG_TIMER
	extern u32 g_tx_failed_cnt, g_tx_all_cnt;
#endif

	asr_dbg(USB, "Enter, skb=%p %d\n", skb, skb->len);
#ifdef CONFIG_ASR_USB_PM
	asr_usb_pm_acquire(devinfo);
#endif

	if (devinfo->bus_pub.state != ASRMAC_USB_STATE_UP) {
		asr_err("state is not up, state:%d\n", devinfo->bus_pub.state);
		ret = -EIO;
		goto fail;
	}

	req = asr_usb_deq(&devinfo->qlock, &devinfo->tx_freeq, &devinfo->tx_freecount);
	if (!req) {
		asr_err("no req to send freecnt:%d\n", devinfo->tx_freecount);
		ret = -ENOMEM;
		goto fail;
	}

	req->skb = skb;
	req->devinfo = devinfo;
	//debug usb tx fail, temp codes
	if (skb->len > 2000)
		asr_err("skb->len:%u\n", skb->len);
	usb_fill_bulk_urb(req->urb, devinfo->usbdev, devinfo->tx_pipe, skb->data, skb->len, asr_usb_tx_complete, req);
	req->urb->transfer_flags |= URB_ZERO_PACKET;
	asr_usb_enq(&devinfo->qlock, &devinfo->tx_postq, &req->list, NULL);
	ret = usb_submit_urb(req->urb, GFP_ATOMIC);
	if (ret) {
		asr_err("asr_usb_tx usb_submit_urb FAILED, ret:%d, skb:%p, skb->data:%p, skb->len:%u, urb:%p, urb->pipe:%u urb->transfer_buffer:%p urb->transfer_buffer_length:%u ep->desc:%p (%u %u %u %u %u %u %u %u)\n",
				ret, skb, skb->data, skb->len, req->urb, req->urb->pipe, req->urb->transfer_buffer, req->urb->transfer_buffer_length, &(req->urb->ep->desc),
				req->urb->ep->desc.bLength, req->urb->ep->desc.bDescriptorType, req->urb->ep->desc.bEndpointAddress, req->urb->ep->desc.bmAttributes,
				req->urb->ep->desc.wMaxPacketSize, req->urb->ep->desc.bInterval, req->urb->ep->desc.bRefresh, req->urb->ep->desc.bSynchAddress);
		asr_usb_del_fromq(&devinfo->qlock, &req->list);
		req->skb = NULL;
		req->urb->context = NULL;
		asr_usb_enq(&devinfo->qlock, &devinfo->tx_freeq, &req->list, &devinfo->tx_freecount);
		goto fail;
	}
#ifdef ASR_DRV_DEBUG_TIMER
	g_tx_all_cnt++;
#endif

	if (devinfo->tx_freecount < devinfo->tx_low_watermark && !devinfo->tx_flowblock) {
		asr_dbg(USB, "txq stop fre:%d\n",devinfo->tx_freecount);
		asr_txflowblock(dev, true);
		devinfo->tx_flowblock = true;
	}
	return 0;

fail:
#ifdef ASR_DRV_DEBUG_TIMER
	g_tx_failed_cnt++;
#endif
	skb_pull(skb, sizeof(struct hostdesc) + HOST_PAD_USB_LEN); //if usb_submit_urb fail, the skb will freeed by _dev_queue_xmit which will call kfree_skb_list(skb)
	//asr_txcomplete(dev, skb, false);
#ifdef CONFIG_ASR_USB_PM
	asr_usb_pm_release(devinfo->usbdev);
#endif
	return ret;
}

static int asr_usb_up(struct device *dev)
{
	struct asr_usbdev_info *devinfo = asr_usb_get_businfo(dev);
	u16 ifnum;
	//int ret;

	asr_dbg(USB, "Enter\n");
	if (devinfo->bus_pub.state == ASRMAC_USB_STATE_UP)
		return 0;

	/* Success, indicate devinfo is fully up */
	asr_usb_state_change(devinfo, ASRMAC_USB_STATE_UP);

	/*if (devinfo->intr_urb) {
		usb_fill_int_urb(devinfo->intr_urb, devinfo->usbdev,
				 devinfo->intr_pipe,
				 &devinfo->intr,
				 devinfo->intr_size,
				 (usb_complete_t) asr_usb_intr_complete, devinfo, devinfo->interval);

		ret = usb_submit_urb(devinfo->intr_urb, GFP_ATOMIC);
		if (ret) {
			asr_err("USB_SUBMIT_URB failed with status %d\n", ret);
			return -EINVAL;
		}
	}*/

	if (devinfo->ctl_urb) {
		devinfo->ctl_in_pipe = usb_rcvctrlpipe(devinfo->usbdev, 0);
		devinfo->ctl_out_pipe = usb_sndctrlpipe(devinfo->usbdev, 0);

		ifnum = IFDESC(devinfo->usbdev, CONTROL_IF).bInterfaceNumber;

		/* CTL Write */
		devinfo->ctl_write.bRequestType = USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE;
		devinfo->ctl_write.bRequest = 0;
		devinfo->ctl_write.wValue = cpu_to_le16(0);
		devinfo->ctl_write.wIndex = cpu_to_le16p(&ifnum);

		/* CTL Read */
		devinfo->ctl_read.bRequestType = USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE;
		devinfo->ctl_read.bRequest = 1;
		devinfo->ctl_read.wValue = cpu_to_le16(0);
		devinfo->ctl_read.wIndex = cpu_to_le16p(&ifnum);
	}
	asr_usb_rx_fill_all(devinfo);

#ifdef CONFIG_ASR_USB_PM
	asr_usb_pm_init(devinfo);
#endif

	return 0;
}

static void asr_usb_down(struct device *dev)
{
	struct asr_usbdev_info *devinfo = asr_usb_get_businfo(dev);

	asr_dbg(USB, "Enter\n");
	if (devinfo == NULL)
		return;

	if (devinfo->bus_pub.state == ASRMAC_USB_STATE_DOWN)
		return;

	asr_usb_state_change(devinfo, ASRMAC_USB_STATE_DOWN);

	if (devinfo->intr_urb)
		usb_kill_urb(devinfo->intr_urb);

	if (devinfo->ctl_urb)
		usb_kill_urb(devinfo->ctl_urb);

	if (devinfo->bulk_urb)
		usb_kill_urb(devinfo->bulk_urb);

	asr_usb_free_q(&devinfo->tx_postq, true);
	asr_usb_free_q(&devinfo->rx_postq, true);
	asr_usb_free_q(&devinfo->ctl_tx_postq, true);

#ifdef CONFIG_ASR_USB_PM
	cancel_delayed_work_sync(&devinfo->pm_cmd_work);
	if (waitqueue_active(&devinfo->pm_waitq))
		wake_up(&devinfo->pm_waitq);
#endif
}

static void asr_usb_sync_complete(struct urb *urb)
{
	struct asr_usbdev_info *devinfo = (struct asr_usbdev_info *)urb->context;

	devinfo->ctl_completed = true;
	asr_usb_ioctl_resp_wake(devinfo);
}

static bool asr_usb_dl_cmd(struct asr_usbdev_info *devinfo, u8 cmd, void *buffer, int buflen)
{
	int ret = 0;
	char *tmpbuf;
	u16 size;

	if ((!devinfo) || (devinfo->ctl_urb == NULL))
		return false;

	tmpbuf = kmalloc(buflen, GFP_ATOMIC);
	if (!tmpbuf)
		return false;

	memset(tmpbuf, 0, buflen);

	size = buflen;
	devinfo->ctl_urb->transfer_buffer_length = size;

	devinfo->ctl_read.wLength = cpu_to_le16p(&size);
	devinfo->ctl_read.bRequestType = USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_INTERFACE;
	devinfo->ctl_read.bRequest = cmd;

	usb_fill_control_urb(devinfo->ctl_urb,
			     devinfo->usbdev,
			     usb_rcvctrlpipe(devinfo->usbdev, 0),
			     (unsigned char *)&devinfo->ctl_read,
			     (void *)tmpbuf, size, (usb_complete_t) asr_usb_sync_complete, devinfo);

	devinfo->ctl_completed = false;

	if (!devinfo->ctl_urb || !devinfo->ctl_urb->complete)
		return -EINVAL;
	if (devinfo->ctl_urb->hcpriv) {
		asr_dbg(USB, "urb bussy \n");
		kfree(tmpbuf);
		return -EBUSY;
	}
	ret = usb_submit_urb(devinfo->ctl_urb, GFP_ATOMIC);
	if (ret < 0) {
		asr_err("usb_submit_urb failed %d\n", ret);
		kfree(tmpbuf);
		return false;
	}

	ret = asr_usb_ioctl_resp_wait(devinfo);
	memcpy(buffer, tmpbuf, buflen);
	kfree(tmpbuf);

	return ret;
}

static bool asr_usb_dl_cmd_set(struct asr_usbdev_info *devinfo, u8 cmd, void *buffer, int buflen)
{
	int ret = 0;
	char *tmpbuf;
	u16 size;

	if ((!devinfo) || (devinfo->ctl_urb == NULL))
		return false;

	tmpbuf = kmalloc(buflen, GFP_ATOMIC);
	if (!tmpbuf)
		return false;

	memcpy(tmpbuf, buffer, buflen);

	size = buflen;
	devinfo->ctl_urb->transfer_buffer_length = size;

	devinfo->ctl_write.wLength = cpu_to_le16p(&size);
	devinfo->ctl_write.bRequestType = USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_INTERFACE;
	devinfo->ctl_write.bRequest = cmd;

	usb_fill_control_urb(devinfo->ctl_urb,
			     devinfo->usbdev,
			     usb_sndctrlpipe(devinfo->usbdev, 0),
			     (unsigned char *)&devinfo->ctl_write,
			     (void *)tmpbuf, size, (usb_complete_t) asr_usb_sync_complete, devinfo);

	devinfo->ctl_completed = false;
	if (!devinfo->ctl_urb || !devinfo->ctl_urb->complete)
		return -EINVAL;

	if (devinfo->ctl_urb->hcpriv) {
		asr_dbg(USB, "urb bussy \n");
		kfree(tmpbuf);
		return -EBUSY;
	}

	ret = usb_submit_urb(devinfo->ctl_urb, GFP_ATOMIC);
	if (ret < 0) {
		asr_err("usb_submit_urb failed %d\n", ret);
		kfree(tmpbuf);
		return false;
	}

	ret = asr_usb_ioctl_resp_wait(devinfo);
	kfree(tmpbuf);

	return ret;
}

#if 1				//def USB_CDC_CMD //lalala_todo
/*
/// Message structure.
struct lmac_msg
{
    u32 dummy;
    lmac_msg_id_t     id;         ///< Message id.
    lmac_task_id_t    dest_id;    ///< Destination kernel identifier.
    lmac_task_id_t    src_id;     ///< Source kernel identifier.
    u16        param_len;  ///< Parameter embedded struct length.
    u32        param[];   ///< Parameter embedded struct. Must be word-aligned.
};

*/
#define CDC_MAX_MSG_SIZE    (ETH_FRAME_LEN+ETH_FCS_LEN)
#define RETRIES 2		/* # of retries to retrieve matching dcmd response */
#define ASR_DCMD_MAXLEN    8192

static int asr_proto_cdc_msg(struct asr_hw *asr_hw, u8 * src, u16 len)
{
	struct asr_usbdev_info *asr_plat = asr_hw->plat;
	struct asr_bus *bus_if = asr_plat->bus_pub.bus;

	asr_dbg(USB, "Enter\n");

	/* NOTE : cdc->msg.len holds the desired length of the buffer to be
	 *        returned. Only up to CDC_MAX_MSG_SIZE of this buffer area
	 *        is actually sent to the dongle
	 */
	if (len > CDC_MAX_MSG_SIZE)
		len = CDC_MAX_MSG_SIZE;

	/* Send request */
	return asr_bus_txctl(bus_if, src, len);	//asr_usb_tx_ctlpkt
}

static int asr_proto_cdc_cmplt(struct asr_hw *asr_hw, u16 id, u8 * src, u16 len)
{
	int ret = -1;
	struct asr_usbdev_info *asr_plat = asr_hw->plat;
	struct asr_bus *bus_if = asr_plat->bus_pub.bus;
	lmac_msg_id_t id_cmplt;

	asr_dbg(USB, "Enter\n");

	do {
		ret = asr_bus_rxctl(bus_if, src, len);	//asr_usb_rx_ctlpkt
		if (ret < 0)
			break;

		id_cmplt = ((struct lmac_msg *)src)->id;
	} while (id_cmplt != id);

	return ret;
}

int asr_proto_cdc_set_dcmd(struct asr_hw *asr_hw, u8 * buf, uint len)
{
	//struct asr_usbdev_info *asr_plat = asr_hw->plat;
	//struct asr_bus *bus_if =  asr_plat->bus_pub.bus;
	int ret = -1;
	//u32 flags;
	lmac_msg_id_t reqid;
	//lmac_msg_id_t id;
	struct lmac_msg *a2e_msg = (struct lmac_msg *)buf;

	reqid = ((struct lmac_msg *)buf)->id;
	asr_dbg(USB, "Enter, cmd %d %d %d %d,len %d\n", reqid,
		a2e_msg->dest_id, a2e_msg->src_id, a2e_msg->param_len, len);

	ret = asr_proto_cdc_msg(asr_hw, buf, len);
	if (ret < 0)
		goto done;

#if 0
	ret = asr_proto_cdc_cmplt(asr_hw, reqid, buf, len);
	if (ret < 0)
		goto done;

	id = ((struct lmac_msg *)buf)->id;

	if (id != reqid) {
		asr_err("unexpected request id %d (expected %d)\n", id, reqid);
		ret = -EINVAL;
		goto done;
	}
#endif

	/* Check the ERROR flag */
	//if (flags & CDC_DCMD_ERROR)
	//    ret = le32_to_cpu(msg->status);

done:
	return ret;
}

int asr_proto_cdc_query_dcmd(struct asr_hw *asr_hw, u8 * buf, uint len)
{
	//struct asr_usbdev_info *asr_plat = asr_hw->plat;
	//struct asr_bus *bus_if =  asr_plat->bus_pub.bus;
	lmac_msg_id_t reqid, id;

	//void *info;
	int ret = 0, retries = 0;

	reqid = ((struct lmac_msg *)buf)->id;
	asr_dbg(USB, "Enter, cmd %d len %d\n", reqid, len);

	ret = asr_proto_cdc_msg(asr_hw, buf, len);
	if (ret < 0) {
		asr_err("asr_proto_cdc_msg failed w/status %d\n", ret);
		goto done;
	}

retry:
	/* wait for interrupt and get first fragment */
	ret = asr_proto_cdc_cmplt(asr_hw, reqid, buf, len);
	if (ret < 0)
		goto done;

	id = ((struct lmac_msg *)buf)->id;

	if ((id < reqid) && (++retries < RETRIES))
		goto retry;
	if (id != reqid) {
		asr_err("unexpected request id %d (expected %d)\n", id, reqid);
		ret = -EINVAL;
		goto done;
	}

	/* Check the ERROR flag */
	//if (flags & CDC_DCMD_ERROR)
	//    ret = le32_to_cpu(msg->status);

done:
	return ret;
}

int asr_usb_send_cmd(struct asr_hw *asr_hw, u8 * src, u16 len, bool set)
{
	struct asr_usbdev_info *asr_plat = asr_hw->plat;
	struct asr_bus *bus_if = asr_plat->bus_pub.bus;
	struct lmac_msg *tx_msg = (struct lmac_msg *)src;
	int err;

	asr_dbg(USB, "Enter\n");

	dev_info(asr_hw->dev, "%s: tx msg %d,%d\n", __func__, MSG_T(tx_msg->id), MSG_I(tx_msg->id));

	if (bus_if->state != ASR_BUS_DATA) {
		asr_err("bus is down. we have nothing to do.state:%d\n", bus_if->state);
		return -EIO;
	}
	//adjust len
	len = len + 4;		//  4 is for end token

	if (src != NULL)
		len = min_t(uint, len, ASR_DCMD_MAXLEN);

	if (set)
		err = asr_proto_cdc_set_dcmd(asr_hw, src, len);
	else
		err = asr_proto_cdc_query_dcmd(asr_hw, src, len);

	if (err >= 0)
		err = 0;
	else
		asr_err("Failed err=%d\n", err);

	return err;

}

#endif

static bool asr_usb_dlneeded(struct asr_usbdev_info *devinfo)
{
	struct bootrom_id_le id;
	u32 chipid, chiprev, fwver;

	asr_dbg(USB, "Enter\n");

	if (devinfo == NULL){
		asr_err("devinfo is NULL\n");
		return false;
	}

	/* Check if firmware downloaded already by querying runtime ID */
	id.chip = cpu_to_le32(0xDEAD);
	asr_usb_dl_cmd(devinfo, DL_GETVER, &id, sizeof(id));

	if (le32_to_cpu(id.chip) && (le32_to_cpu(id.chip) < 0xFFFF))	//todolalala
	{
		chipid = le32_to_cpu(id.chip);
		chiprev = le32_to_cpu(id.chiprev);
		fwver = le32_to_cpu(id.fwver);
	} else {
		asr_err("DL_GETVER failed , use default chip id\n");
		chipid = 0x5531;
		chiprev = 0x1;
		fwver = 0;
	}

	asr_dbg(USB, "chip %x rev 0x%x fw 0x%x\n", chipid, chiprev, fwver);

	if (fwver & 0xFFFF0000) {
		asr_usb_dl_cmd(devinfo, DL_RESETCFG, &id, sizeof(id));
		asr_err("firmware already downloaded\n");
		return false;
	} else {
		devinfo->bus_pub.devid = chipid;
		devinfo->bus_pub.chiprev = chiprev;
		asr_dbg(USB, "chip %x rev 0x%x\n", chipid, chiprev);
	}
	return true;
}

#ifdef DONT_ENUM_AFTER_DL_FW
static int asr_usb_resetcfg(struct asr_usbdev_info *devinfo)
{
	struct bootrom_id_le id;
	u32 loop_cnt;
	bool ret;

	asr_dbg(USB, "Enter\n");

	loop_cnt = 0;
	do {
		mdelay(ASR_USB_RESET_GETVER_SPINWAIT);
		loop_cnt++;
		id.chip = cpu_to_le32(0xDEAD);	/* Get the ID */
		ret = asr_usb_dl_cmd(devinfo, DL_GETVER, &id, sizeof(id));
		if (ret) {
			asr_err("Cannot talk to Dongle. Firmware is not UP, %d ms ret:%d\n",
				ASR_USB_RESET_GETVER_SPINWAIT * loop_cnt, ret);
			return -EINVAL;
		}
		if (id.fwver & 0xFFFF0000)
			break;
	} while (loop_cnt < ASR_USB_RESET_GETVER_LOOP_CNT);

	if (id.fwver & 0xFFFF0000) {
		asr_dbg(USB, "postboot chip 0x%x/rev 0x%x/fw 0x%x\n",
			le32_to_cpu(id.chip), le32_to_cpu(id.chiprev), le32_to_cpu(id.fwver));

		ret = asr_usb_dl_cmd(devinfo, DL_RESETCFG, &id, sizeof(id));
		if (ret) {
			asr_err("Cannot talk to Dongle. Firmware is not UP, %d ms ret:%d\n",
				ASR_USB_RESET_GETVER_SPINWAIT * loop_cnt, ret);
			return -EINVAL;
		}
		return 0;
	} else {
		asr_err("Cannot talk to Dongle. Firmware is not UP, %d ms\n", ASR_USB_RESET_GETVER_SPINWAIT * loop_cnt);
		return -EINVAL;
	}
}
#endif

static int asr_usb_dl_send_bulk(struct asr_usbdev_info *devinfo, void *buffer, int len)
{
	int ret;

	if ((devinfo == NULL) || (devinfo->bulk_urb == NULL))
		return -EINVAL;

	/* Prepare the URB */
	usb_fill_bulk_urb(devinfo->bulk_urb, devinfo->usbdev,
			  devinfo->tx_pipe, buffer, len, (usb_complete_t) asr_usb_sync_complete, devinfo);

	devinfo->bulk_urb->transfer_flags |= URB_ZERO_PACKET;

	devinfo->ctl_completed = false;

	if (devinfo->bulk_urb->hcpriv) {
		asr_dbg(USB, "urb bussy \n");
		return -EBUSY;
	}

	ret = usb_submit_urb(devinfo->bulk_urb, GFP_ATOMIC);
	if (ret) {
		asr_err("usb_submit_urb failed %d\n", ret);
		return ret;
	}
	ret = asr_usb_ioctl_resp_wait(devinfo);
	return (ret == 0);
}

static int asr_usb_dl_writeimage(struct asr_usbdev_info *devinfo, struct sec_header_le *sec_hdr, u8 * fw, int fwlen)
{
	unsigned int sendlen, sent;
	char *bulkchunk = NULL, *dlpos;
	struct rdl_state_le state;
	u32 rdlstate, rdlbytes;
	int err = 0;

	asr_dbg(USB, "Enter, fw %p, len %d\n", fw, fwlen);

	bulkchunk = kmalloc(RDL_CHUNK, GFP_ATOMIC);
	if (bulkchunk == NULL) {
		asr_err("Failed to kmalloc RDL_CHUNK\n");
		err = -ENOMEM;
		goto fail;
	}

	/* 1) Prepare USB boot loader for runtime image */
	if (!asr_usb_dl_cmd_set(devinfo, DL_START, sec_hdr, sizeof(struct sec_header_le))) {
		asr_err("Failed to DL_START\n");
		err = -EINVAL;
		goto fail;
	}

	/* 2) Check we are in the ready state */
	if (!asr_usb_dl_cmd(devinfo, DL_GETSTATE, &state, sizeof(struct rdl_state_le))) {
		asr_err("DL_GETSTATE Failed\n");
		err = -EINVAL;
		goto fail;
	}

	rdlstate = le32_to_cpu(state.state);
	if (rdlstate != DL_READY) {
		asr_err("Failed to DL_START\n");
		err = -EINVAL;
		goto fail;
	}

	sent = 0;
	dlpos = fw;

	while (sent < fwlen) {
		sendlen = fwlen - sent;
		if (sendlen > RDL_CHUNK)
			sendlen = RDL_CHUNK;

		memcpy(bulkchunk, dlpos, sendlen);
		if (asr_usb_dl_send_bulk(devinfo, bulkchunk, sendlen)) {
			asr_err("send_bulk failed\n");
			err = -EINVAL;
			goto fail;
		}

		dlpos += sendlen;
		sent += sendlen;
	}

	do {
		if (!asr_usb_dl_cmd(devinfo, DL_GETSTATE, &state, sizeof(struct rdl_state_le))) {
			asr_err("DL_GETSTATE Failed\n");
			err = -EINVAL;
			goto fail;
		}

		rdlstate = le32_to_cpu(state.state);
		rdlbytes = le32_to_cpu(state.bytes);
	} while (rdlbytes != sent);

	if (rdlstate == DL_BAD_CRC) {
		asr_err("Bad state %d and length %d\n", rdlstate, rdlbytes);
		err = -EINVAL;
	}

fail:
	kfree(bulkchunk);
	asr_dbg(USB, "Exit, err=%d\n", err);
	return err;
}

static int asr_usb_dlstart(struct asr_usbdev_info *devinfo, u8 * fw, int len)
{
	int err, offset;
	int sec_idx = 0;
	int retry_times = 3;
	struct rdl_state_le state;
	struct fw_header_le *fw_hdr = (struct fw_header_le *)fw;
	struct sec_header_le *sec_hdr;

	asr_dbg(USB, "Enter dl start\n");

	if (devinfo == NULL)
		return -EINVAL;

	/* magic + sec num + [len + crc + chip addr] */
	offset = 8 + fw_hdr->sec_num * sizeof(struct sec_header_le);
	asr_dbg(USB, "fw section number %d, fw offset %d\n", fw_hdr->sec_num, offset);

	do {
		sec_hdr = &fw_hdr->sec_hdr[sec_idx];
		asr_dbg(USB,
			"fw section length %d, section crc 0x%x, chip address 0x%x\n",
			sec_hdr->sec_len, sec_hdr->sec_crc, sec_hdr->chip_addr);

		/* reset retry times to 3 */
		retry_times = 3;

		do {
			err = asr_usb_dl_writeimage(devinfo, sec_hdr, fw + offset, sec_hdr->sec_len);
			if (err == 0)
				break;
			/* if error, retry to download */
			retry_times--;
		} while (retry_times > 0);

		offset += sec_hdr->sec_len;
		sec_idx++;
	} while (sec_idx < fw_hdr->sec_num);

	if (err == 0 && sec_idx == fw_hdr->sec_num)
		devinfo->bus_pub.state = ASRMAC_USB_STATE_DL_DONE;
	else
		devinfo->bus_pub.state = ASRMAC_USB_STATE_DL_FAIL;

	asr_err("Exit, state=%d retry_times=%d\n", state.state,retry_times);
	return err;
}

static int asr_usb_dlrun(struct asr_usbdev_info *devinfo)
{
	struct rdl_state_le state;
	struct fw_header_le *fw_hdr;
	struct sec_header_le *sec_hdr;

	asr_dbg(USB, "Enter\n");
	if (!devinfo)
		return -EINVAL;

	if (devinfo->bus_pub.devid == 0xDEAD)
		return -EINVAL;

	/* Check we are runnable */
	asr_usb_dl_cmd(devinfo, DL_GETSTATE, &state, sizeof(struct rdl_state_le));

	/* Start the image */
	if (state.state == cpu_to_le32(DL_RUNNABLE)) {
		/* get the chip address of the first section, then jump to this address to run */
		fw_hdr = (struct fw_header_le *) devinfo->image;
		sec_hdr = &fw_hdr->sec_hdr[0];
		if (!asr_usb_dl_cmd_set(devinfo, DL_GO, &sec_hdr->chip_addr, sizeof(__le32)))
			return -ENODEV;
#ifdef DONT_ENUM_AFTER_DL_FW
		if (asr_usb_resetcfg(devinfo))
			return -ENODEV;
		/* The Dongle may go for re-enumeration. */
#else
		asr_err("Dongle will go for re-enumeration\n");
		return -ENODEV;
#endif
	} else {
		asr_err("Dongle not runnable\n");
		return -EINVAL;
	}
	asr_dbg(USB, "Exit\n");
	return 0;
}

static bool asr_usb_chip_support(int chipid, int chiprev)
{
	switch (chipid) {
	case 0x5531:
		return true;
	default:
		break;
	}
	return false;
}

static int asr_usb_fw_download(struct asr_usbdev_info *devinfo)
{
	int devid, chiprev;
	int err;

	asr_dbg(USB, "Enter\n");
	if (devinfo == NULL){
		asr_err("devinfo is NULL\n");
		return -ENODEV;
	}

	devid = devinfo->bus_pub.devid;
	chiprev = devinfo->bus_pub.chiprev;

	if (!asr_usb_chip_support(devid, chiprev)) {
		asr_err("unsupported chip %d rev %d\n", devid, chiprev);
		return -EINVAL;
	}

	if (!devinfo->image) {
		asr_err("No firmware!\n");
		return -ENOENT;
	}

	err = asr_usb_dlstart(devinfo, devinfo->image, devinfo->image_len);
	return err;
}

static void asr_usb_detach(struct asr_usbdev_info *devinfo)
{
	asr_dbg(USB, "Enter, devinfo %p\n", devinfo);

	/* free the URBS */
	asr_usb_free_q(&devinfo->rx_freeq, false);
	asr_usb_free_q(&devinfo->tx_freeq, false);
	asr_usb_free_q(&devinfo->ctl_tx_freeq, false);

	usb_free_urb(devinfo->intr_urb);
	usb_free_urb(devinfo->ctl_urb);
	usb_free_urb(devinfo->bulk_urb);

	kfree(devinfo->tx_reqs);
	kfree(devinfo->rx_reqs);
	kfree(devinfo->ctl_tx_reqs);
}

void asr_detach(struct device *dev)
{
	struct asr_bus *bus_if = dev_get_drvdata(dev);
	struct asr_hw *asr_hw;

	asr_dbg(TRACE, "Enter\n");

	asr_hw = bus_if->asr_hw;
#ifdef CONFIG_ASR_USB
	asr_hw->usb_remove_flag = 1;
#endif
	//asr_cfg80211_deinit(asr_hw);
	asr_platform_deinit(asr_hw);

	asr_bus_stop(bus_if);

}

#define TRX_MAGIC       0x30524448	/* "HDR0" */
#define TRX_VERSION     1	/* Version 1 */
#define TRX_MAX_LEN     0x3B0000	/* Max length */
#define TRX_NO_HEADER   1	/* Do not write TRX header */
#define TRX_MAX_OFFSET  3	/* Max number of individual files */
#define TRX_UNCOMP_IMAGE        0x20	/* Trx contains uncompressed image */

struct trx_header_le {
	__le32 magic;		/* "HDR0" */
	__le32 len;		/* Length of file including header */
	__le32 crc32;		/* CRC from flag_version to end of file */
	__le32 flag_version;	/* 0:15 flags, 16:31 version */
	__le32 offsets[TRX_MAX_OFFSET];	/* Offsets of partitions from start of
					 * header */
};

static int check_file(const u8 * headers, int size)
{
	struct fw_header_le *fw_hdr = (struct fw_header_le *)headers;
	int actual_len = -1;

	asr_dbg(USB, "Enter\n");

	/* check the magic number of fw header */
	if (fw_hdr->magic != cpu_to_le32(FW_HEADER_MAGIC))
		return -1;

	/* magic + sec num + [len + crc + chip addr] */
	actual_len = size - (4 + 4 + fw_hdr->sec_num * sizeof(struct sec_header_le));
	return actual_len;
}

static int asr_usb_get_fw(struct asr_usbdev_info *devinfo)
{
	s8 *fwname;
	const struct firmware *fw;
	struct asr_usb_image *fw_image;
	int err;

	asr_dbg(USB, "Enter\n");
	switch (devinfo->bus_pub.devid) {
	case 0x5531:
		if (downloadATE) {
			fwname = ASR_ATE_FW_NAME;
		} else if (driver_mode == DRIVER_MODE_ATE) {
			fwname = ASR_DRIVER_ATE_FW_NAME;
		} else {
			fwname = ASR_MAC_FW_NAME;
		}
		break;
	default:
		//ignore devid , decide lega devid later.
		fwname = ASR_MAC_FW_NAME;
		break;
	}
	asr_dbg(USB, "Loading FW %s\n", fwname);
	list_for_each_entry(fw_image, &fw_image_list, list) {
		if (fw_image->fwname == fwname) {
			devinfo->image = fw_image->image;
			devinfo->image_len = fw_image->image_len;
			return 0;
		}
	}
	/* fw image not yet loaded. Load it now and add to list */
	err = request_firmware(&fw, fwname, devinfo->dev);
	if (!fw) {
		asr_err("fail to request firmware %s\n", fwname);
		return err;
	}
	if (check_file(fw->data, fw->size) < 0) {
		asr_err("invalid firmware %s\n", fwname);
		return -EINVAL;
	}

	fw_image = kzalloc(sizeof(*fw_image), GFP_ATOMIC);
	if (!fw_image){
		asr_err("fw_image malloc failed\n");
		return -ENOMEM;
	}
	INIT_LIST_HEAD(&fw_image->list);
	list_add_tail(&fw_image->list, &fw_image_list);
	fw_image->fwname = fwname;
	fw_image->image = vmalloc(fw->size);
	if (!fw_image->image){
		asr_err("fw_image->image is NULL\n");
		return -ENOMEM;
	}

	memcpy(fw_image->image, fw->data, fw->size);
	fw_image->image_len = fw->size;

	release_firmware(fw);

	devinfo->image = fw_image->image;
	devinfo->image_len = fw_image->image_len;

	return 0;
}

static
struct asr_usbdev *asr_usb_attach(struct asr_usbdev_info *devinfo, int nrxq, int ntxq)
{
	asr_dbg(USB, "Enter\n");

	devinfo->bus_pub.nrxq = nrxq;
	devinfo->rx_low_watermark = nrxq / 2;
	devinfo->bus_pub.devinfo = devinfo;
	devinfo->bus_pub.ntxq = ntxq;
	devinfo->bus_pub.state = ASRMAC_USB_STATE_DOWN;

	/* flow control when too many tx urbs posted */
	devinfo->tx_low_watermark = ntxq / 4;
	devinfo->tx_high_watermark = devinfo->tx_low_watermark * 3;
	devinfo->bus_pub.bus_mtu = ASR_USB_MAX_PKT_SIZE + HOST_RX_DESC_SIZE;

	/* Initialize other structure content */
	init_waitqueue_head(&devinfo->ioctl_resp_wait);

	/* Initialize the spinlocks */
	spin_lock_init(&devinfo->qlock);

	INIT_LIST_HEAD(&devinfo->rx_freeq);
	INIT_LIST_HEAD(&devinfo->rx_postq);

	INIT_LIST_HEAD(&devinfo->tx_freeq);
	INIT_LIST_HEAD(&devinfo->tx_postq);

	devinfo->tx_flowblock = false;

	devinfo->rx_reqs = asr_usbdev_qinit(&devinfo->rx_freeq, nrxq);
	if (!devinfo->rx_reqs)
		goto error;
	devinfo->rx_freecount = nrxq;

	devinfo->tx_reqs = asr_usbdev_qinit(&devinfo->tx_freeq, ntxq);
	if (!devinfo->tx_reqs)
		goto error;
	devinfo->tx_freecount = ntxq;

	/* Initialize the spinlock for control queues */
	spin_lock_init(&devinfo->ctl_qlock);
	/* Initialize the free and post queue head of control tx requests*/
	INIT_LIST_HEAD(&devinfo->ctl_tx_freeq);
	INIT_LIST_HEAD(&devinfo->ctl_tx_postq);
	/* Initialize the requests and push to free queue */
	devinfo->ctl_tx_reqs = asr_usb_ctl_qinit(&devinfo->ctl_tx_freeq, ASR_USB_NCTLQ);
	if (!devinfo->ctl_tx_reqs)
		goto error;

	devinfo->intr_urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!devinfo->intr_urb) {
		asr_err("usb_alloc_urb (intr) failed\n");
		goto error;
	}
	devinfo->ctl_urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!devinfo->ctl_urb) {
		asr_err("usb_alloc_urb (ctl) failed\n");
		goto error;
	}
	devinfo->bulk_urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!devinfo->bulk_urb) {
		asr_err("usb_alloc_urb (bulk) failed\n");
		goto error;
	}

	if (!asr_usb_dlneeded(devinfo)){
		asr_err("fw download no need\n");
		return &devinfo->bus_pub;
	}

	asr_dbg(USB, "Start fw downloading\n");
	if (asr_usb_get_fw(devinfo))
		goto error;

	if (asr_usb_fw_download(devinfo)) {
		asr_err("fw_download failed!, temp continue probe flow!\n");
		goto error;
	}

	if (asr_usb_dlrun(devinfo)) {
		asr_err("Dongle is not running now!\n");
		goto error;
	}

	return &devinfo->bus_pub;

error:
	asr_err("failed!\n");
	asr_usb_detach(devinfo);
	return NULL;
}

static struct asr_bus_ops asr_usb_bus_ops = {
	.txdata = asr_usb_tx,
	.init = asr_usb_up,
	.stop = asr_usb_down,
	.txctl = asr_usb_tx_ctlpkt,
	.rxctl = asr_usb_rx_ctlpkt,
};

static int asr_usb_probe_cb(struct asr_usbdev_info *devinfo)
{
	struct asr_bus *bus = NULL;
	struct asr_usbdev *bus_pub = NULL;
	int ret;
	struct device *dev = devinfo->dev;
	struct asr_bus *bus_if = NULL;
	void *drvdata;

	asr_dbg(USB, "Enter\n");
	// usb init and fw download.
	bus_pub = asr_usb_attach(devinfo, ASR_USB_NRXQ, ASR_USB_NTXQ);
	if (!bus_pub)
		return -ENODEV;

	bus = kzalloc(sizeof(struct asr_bus), GFP_ATOMIC);
	if (!bus) {
		ret = -ENOMEM;
		goto fail;
	}

	bus->dev = dev;
	bus_pub->bus = bus;
	bus->bus_priv.usb = bus_pub;
	dev_set_drvdata(dev, bus);
	bus->ops = &asr_usb_bus_ops;
	bus->chip = bus_pub->devid;
	bus->chiprev = bus_pub->chiprev;

	// bus_start
	// now bus_if is allocated bus.
	bus_if = dev_get_drvdata(dev);

	/* Bring up the bus */
	ret = asr_bus_init(bus_if);	//asr_usb_up
	if (ret != 0) {
		asr_err("bus_init failed %d\n", ret);
		goto fail;
	}

	/* signal bus ready */
	bus_if->state = ASR_BUS_DATA;

	asr_dbg(USB, "asr_usb_platform_init started\n");
	ret = asr_usb_platform_init(devinfo, &drvdata);
	if (ret != 0) {
		asr_err("usb_platform_init failed %d\n", ret);
		asr_bus_stop(bus_if);
		goto fail;
	}

	return 0;
fail:
	/* Release resources in reverse order */
	kfree(bus);
	asr_usb_detach(devinfo);
	return ret;
}

static void asr_usb_disconnect_cb(struct asr_usbdev_info *devinfo)
{
	if (!devinfo)
		return;
	asr_dbg(USB, "Enter, bus_pub %p\n", devinfo);

	asr_detach(devinfo->dev);
	asr_usb_detach(devinfo);
	kfree(devinfo->bus_pub.bus);
}

static int asr_usb_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	int ep;
	struct usb_endpoint_descriptor *endpoint;
	int ret = 0;
	struct usb_device *usb = interface_to_usbdev(intf);
	int num_of_eps;
	u8 endpoint_num;
	struct asr_usbdev_info *devinfo;

	asr_dbg(USB, "Enter!!\n");

	devinfo = kzalloc(sizeof(*devinfo), GFP_ATOMIC);
	if (devinfo == NULL)
		return -ENOMEM;

	devinfo->usbdev = usb;
	devinfo->dev = &usb->dev;

	usb_set_intfdata(intf, devinfo);

	asr_err
	    ("Check device : bNumConfigurations %d, bDeviceClass %d,bNumInterfaces %d\n",
	     usb->descriptor.bNumConfigurations, usb->descriptor.bDeviceClass, CONFIGDESC(usb)->bNumInterfaces);

	/* Check that the device supports only one configuration */
	if (usb->descriptor.bNumConfigurations != 1) {
		ret = -1;
		goto fail;
	}

	if (usb->descriptor.bDeviceClass != USB_CLASS_VENDOR_SPEC) {
		ret = -1;
		goto fail;
	}

	/*
	 * Only the BDC interface configuration is supported:
	 *    Device class: USB_CLASS_VENDOR_SPEC
	 *    if0 class: USB_CLASS_VENDOR_SPEC
	 *    if0/ep0: control
	 *    if0/ep1: bulk in
	 *    if0/ep2: bulk out (ok if swapped with bulk in)
	 */
	if (CONFIGDESC(usb)->bNumInterfaces != 1) {
		ret = -1;
		goto fail;
	}

	asr_err("check control interface: class %d, subclass %d, proto %d\n",
		IFDESC(usb, CONTROL_IF).bInterfaceClass,
		IFDESC(usb, CONTROL_IF).bInterfaceSubClass, IFDESC(usb, CONTROL_IF).bInterfaceProtocol);

	/* Check interface */
	if (IFDESC(usb, CONTROL_IF).bInterfaceClass != USB_CLASS_VENDOR_SPEC ||
	    IFDESC(usb, CONTROL_IF).bInterfaceSubClass != 2 || IFDESC(usb, CONTROL_IF).bInterfaceProtocol != 0xff) {
		asr_err
		    ("invalid control interface: class %d, subclass %d, proto %d\n",
		     IFDESC(usb, CONTROL_IF).bInterfaceClass, IFDESC(usb,
								     CONTROL_IF).bInterfaceSubClass,
		     IFDESC(usb, CONTROL_IF).bInterfaceProtocol);
		ret = -1;
		goto fail;
	}

	/* Check control endpoint */
	endpoint = &IFEPDESC(usb, CONTROL_IF, 0);

	asr_err("check control endpoint %d\n", endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK);

	if ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
	    != USB_ENDPOINT_XFER_INT) {
		asr_err("invalid control endpoint %d\n", endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK);
		ret = -1;
		goto fail;
	}

	endpoint_num = endpoint->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
	devinfo->intr_pipe = usb_rcvintpipe(usb, endpoint_num);

	devinfo->rx_pipe = 0;
	devinfo->rx_pipe2 = 0;
	devinfo->tx_pipe = 0;
	num_of_eps = IFDESC(usb, BULK_IF).bNumEndpoints - 1;

	/* Check data endpoints and get pipes */
	for (ep = 1; ep <= num_of_eps; ep++) {
		endpoint = &IFEPDESC(usb, BULK_IF, ep);

		asr_err("[%d of %d]check data endpoint %d\n", ep, num_of_eps,
			(endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK));

		if ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) != USB_ENDPOINT_XFER_BULK) {
			asr_err("invalid data endpoint %d\n", ep);
			ret = -1;
			goto fail;
		}

		endpoint_num = endpoint->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;

		if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK)
		    == USB_DIR_IN) {
			if (!devinfo->rx_pipe) {
				devinfo->rx_pipe = usb_rcvbulkpipe(usb, endpoint_num);
			} else {
				devinfo->rx_pipe2 = usb_rcvbulkpipe(usb, endpoint_num);
			}
		} else {
			devinfo->tx_pipe = usb_sndbulkpipe(usb, endpoint_num);
		}
	}

	/* Allocate interrupt URB and data buffer */
	devinfo->intr_size = IFEPDESC(usb, CONTROL_IF, 0).wMaxPacketSize;
	devinfo->interval = IFEPDESC(usb, CONTROL_IF, 0).bInterval;

	if (usb->speed == USB_SPEED_HIGH)
		asr_dbg(USB, "asr high speed USB wireless device detected\n");
	else
		asr_dbg(USB, "asr full speed USB wireless device detected\n");

	ret = asr_usb_probe_cb(devinfo);
	if (ret)
		goto fail;

	/* Success */
	asr_dbg(USB, "asr_usb_probe done!!\n");
	return 0;

fail:
	asr_err("failed with errno %d\n", ret);
	kfree(devinfo);
	usb_set_intfdata(intf, NULL);
	return ret;
}

static void asr_usb_disconnect(struct usb_interface *intf)
{
	struct asr_usbdev_info *devinfo;

	asr_dbg(USB, "Enter\n");
	devinfo = (struct asr_usbdev_info *)usb_get_intfdata(intf);
	if (devinfo) {
		asr_usb_disconnect_cb(devinfo);
		kfree(devinfo);
	}
	asr_dbg(USB, "Exit\n");
}

/*
 * only need to signal the bus being down and update the state.
 */
static int asr_usb_suspend(struct usb_interface *intf, pm_message_t state)
{
	struct usb_device *usb = interface_to_usbdev(intf);
	struct asr_usbdev_info *devinfo = asr_usb_get_businfo(&usb->dev);

	asr_dbg(USB, "Enter\n");
	asr_usb_state_change(devinfo, ASRMAC_USB_STATE_SLEEP);
	//asr_detach(&usb->dev);
	return 0;
}

/*
 * (re-) start the bus.
 */
static int asr_usb_resume(struct usb_interface *intf)
{
	struct usb_device *usb = interface_to_usbdev(intf);
	struct asr_usbdev_info *devinfo = asr_usb_get_businfo(&usb->dev);
	//struct device *dev = devinfo->dev;
	//struct asr_bus *bus_if = NULL;
	//int ret;
	//void *drvdata;

	asr_dbg(USB, "Enter\n");

	asr_usb_state_change(devinfo, ASRMAC_USB_STATE_UP);
	asr_usb_rx_fill_all(devinfo);

#ifdef	CONFIG_ASR_USB_PM
	if (waitqueue_active(&devinfo->pm_waitq))
		wake_up(&devinfo->pm_waitq);
#endif
	#if 0
	//if (!asr_attach(0, devinfo->dev))
	//    return asr_bus_start(&usb->dev);

	{
		// bus_start
		// now bus_if is allocated bus.
		bus_if = dev_get_drvdata(dev);

		/* Bring up the bus */
		ret = asr_bus_init(bus_if);
		if (ret != 0) {
			asr_err("bus_init failed %d\n", ret);
			return ret;
		}

		/* signal bus ready */
		bus_if->state = ASR_BUS_DATA;

		ret = asr_usb_platform_init(devinfo, &drvdata);
		if (ret != 0) {
			asr_err("bus_init failed %d\n", ret);
			return ret;
		}
	}
	#endif

	return 0;
}

static int asr_usb_reset_resume(struct usb_interface *intf)
{
	struct usb_device *usb = interface_to_usbdev(intf);
	struct asr_usbdev_info *devinfo = asr_usb_get_businfo(&usb->dev);

	asr_dbg(USB, "Enter\n");

	if (!asr_usb_fw_download(devinfo))
		return asr_usb_resume(intf);

	return -EIO;
}

#define ASR_USB_VENDOR_ID_BROADCOM 0x7392
#define ASR_USB_DEVICE_ID_BCMFW    0xa822

#define ASR_USB_VENDOR_ID_ASR      0x2ecc	//ASR
#define ASR_USB_PRODUCT_ID_5000    0x5000	//for ASR5531

static struct usb_device_id asr_usb_devid_table[] = {
	/* special entry for device with firmware loaded and running */
	{USB_DEVICE(ASR_USB_VENDOR_ID_BROADCOM, ASR_USB_DEVICE_ID_BCMFW)},
	{USB_DEVICE(ASR_USB_VENDOR_ID_ASR, ASR_USB_PRODUCT_ID_5000)},
	{}
};

MODULE_DEVICE_TABLE(usb, asr_usb_devid_table);
MODULE_FIRMWARE(ASR_USB_5531_FW_NAME);

static struct usb_driver asr_usbdrvr = {
	.name = KBUILD_MODNAME,
	.probe = asr_usb_probe,
	.disconnect = asr_usb_disconnect,
	.id_table = asr_usb_devid_table,
	.suspend = asr_usb_suspend,
	.resume = asr_usb_resume,
	.reset_resume = asr_usb_reset_resume,
	.supports_autosuspend = 1,
	.disable_hub_initiated_lpm = 1,
};

static void asr_release_fw(struct list_head *q)
{
	struct asr_usb_image *fw_image, *next;

	list_for_each_entry_safe(fw_image, next, q, list) {
		vfree(fw_image->image);
		list_del_init(&fw_image->list);
	}
}

void asr_dev_reset(struct device *dev)
{
	/*
	   asr_fil_cmd_int_set(drvr->iflist[0], ASR_C_TERMINATED, 1);
	 */

}

static int asr_usb_reset_device(struct device *dev, void *notused)
{
	/* device past is the usb interface so we
	 * need to use parent here.
	 */
	asr_dev_reset(dev->parent);
	return 0;
}

void asr_usb_exit(void)
{
	struct device_driver *drv = &asr_usbdrvr.drvwrap.driver;
	int ret;

	asr_dbg(USB, "Enter\n");
	ret = driver_for_each_device(drv, NULL, NULL, asr_usb_reset_device);
	usb_deregister(&asr_usbdrvr);
	asr_release_fw(&fw_image_list);
}

int asr_usb_register_drv(void)
{
	int ret = 0;
	asr_dbg(USB, "Enter\n");
	INIT_LIST_HEAD(&fw_image_list);
	ret = usb_register(&asr_usbdrvr);
	if (ret) {
		asr_dbg(USB, "asr_usb_register_drv faile\n");
		//usb_deregister(&asr_usbdrvr); to be done later
	}
	return ret;
}
