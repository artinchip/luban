// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (c) 2021, Artinchip Technology Co., Ltd
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/of_device.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/phy/phy.h>
#include <linux/reset.h>
#include <linux/usb/of.h>

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/phy.h>
#include <linux/usb/composite.h>

#include <linux/spinlock.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include "aic_udc.h"

static inline u32 aic_readl(struct aic_usb_gadget *gg, u32 offset);
static inline void aic_writel(struct aic_usb_gadget *gg, u32 value,
			      u32 offset);
static void aic_ep0_enqueue_setup(struct aic_usb_gadget *gg);
static struct usb_request *aic_ep_alloc_request(struct usb_ep *ep,
						gfp_t flags);
static void aic_ep_free_request(struct usb_ep *ep,
				struct usb_request *req);
static void aic_ep_complete_request(struct aic_usb_gadget *gg,
				    struct aic_usb_ep *a_ep,
				    struct aic_usb_req *a_req,
				    int result);
static int aic_ep_queue_request_nolock(struct usb_ep *ep,
				       struct usb_request *req,
				       gfp_t gfp_flags);
static int aic_ep_sethalt_nolock(struct usb_ep *ep, int value, bool now);
static void aic_ep_start_req(struct aic_usb_gadget *gg,
			     struct aic_usb_ep *a_ep,
			     struct aic_usb_req *a_req,
			     bool continuing);
static void aic_ep_start_next_request(struct aic_usb_ep *a_ep);
static void aic_stall_ep0(struct aic_usb_gadget *gg);
static void aic_epint_handle_complete_in(struct aic_usb_gadget *gg,
					 struct aic_usb_ep *a_ep);
static void aic_kill_ep_reqs(struct aic_usb_gadget *gg,
			     struct aic_usb_ep *ep,
			     int result);
static int aic_ep_disable(struct usb_ep *ep);
static int aic_ep_disable_nolock(struct usb_ep *ep);
static int aic_set_test_mode(struct aic_usb_gadget *gg, int testmode);

#ifdef CONFIG_DEBUG_FS

#define print_param(_seq, _ptr, _param) \
seq_printf((_seq), "%-30s: %d\n", #_param, (_ptr)->_param)

#define print_param_hex(_seq, _ptr, _param) \
seq_printf((_seq), "%-30s: 0x%x\n", #_param, (_ptr)->_param)

#define dump_register(nm)	\
{				\
	.name	= #nm,		\
	.offset	= nm,		\
}

static const struct debugfs_reg32 aic_udc_regs[] = {
	dump_register(AHBBASIC),
	dump_register(USBDEVINIT),
	dump_register(USBPHYIF),
	dump_register(USBINTSTS),
	dump_register(USBINTMSK),
	dump_register(RXFIFOSTS_DBG),
	dump_register(RXFIFOSIZ),
	dump_register(NPTXFIFOSIZ),
	dump_register(NPTXFIFOSTS),
	dump_register(USBULPIPHY),
	dump_register(TXFIFOSIZ(1)),
	dump_register(TXFIFOSIZ(2)),
	dump_register(USBDEVCONF),
	dump_register(USBDEVFUNC),
	dump_register(USBLINESTS),
	dump_register(INEPINTMSK),
	dump_register(OUTEPINTMSK),
	dump_register(USBEPINT),
	dump_register(USBEPINTMSK),
	dump_register(DTKNQR1),
	dump_register(DTKNQR2),
	dump_register(DTKNQR3),
	dump_register(DTKNQR4),
	dump_register(INEPCFG(0)),
	dump_register(INEPCFG(1)),
	dump_register(INEPCFG(2)),
	dump_register(INEPCFG(3)),
	dump_register(INEPCFG(4)),
	dump_register(OUTEPCFG(0)),
	dump_register(OUTEPCFG(1)),
	dump_register(OUTEPCFG(2)),
	dump_register(OUTEPCFG(3)),
	dump_register(OUTEPCFG(4)),
	dump_register(INEPINT(0)),
	dump_register(INEPINT(1)),
	dump_register(INEPINT(2)),
	dump_register(INEPINT(3)),
	dump_register(INEPINT(4)),
	dump_register(OUTEPINT(0)),
	dump_register(OUTEPINT(1)),
	dump_register(OUTEPINT(2)),
	dump_register(OUTEPINT(3)),
	dump_register(OUTEPINT(4)),
	dump_register(INEPTSFSIZ(0)),
	dump_register(INEPTSFSIZ(1)),
	dump_register(INEPTSFSIZ(2)),
	dump_register(INEPTSFSIZ(3)),
	dump_register(INEPTSFSIZ(4)),
	dump_register(OUTEPTSFSIZ(0)),
	dump_register(OUTEPTSFSIZ(1)),
	dump_register(OUTEPTSFSIZ(2)),
	dump_register(OUTEPTSFSIZ(3)),
	dump_register(OUTEPTSFSIZ(4)),
	dump_register(INEPDMAADDR(0)),
	dump_register(INEPDMAADDR(1)),
	dump_register(INEPDMAADDR(2)),
	dump_register(INEPDMAADDR(3)),
	dump_register(INEPDMAADDR(4)),
	dump_register(OUTEPDMAADDR(0)),
	dump_register(OUTEPDMAADDR(1)),
	dump_register(OUTEPDMAADDR(2)),
	dump_register(OUTEPDMAADDR(3)),
	dump_register(OUTEPDMAADDR(4)),
	dump_register(INEPTXSTS(0)),
	dump_register(INEPTXSTS(1)),
	dump_register(INEPTXSTS(2)),
	dump_register(INEPTXSTS(3)),
	dump_register(INEPTXSTS(4)),
	dump_register(PCGCTL),
	dump_register(UDCVERSION),
};

static ssize_t testmode_write(struct file *file, const char __user *ubuf, size_t
		count, loff_t *ppos)
{
	struct seq_file		*s = file->private_data;
	struct aic_usb_gadget *gg = s->private;
	unsigned long		flags;
	u32			testmode = 0;
	char			buf[32];

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (!strncmp(buf, "test_j", 6))
		testmode = USB_TEST_J;
	else if (!strncmp(buf, "test_k", 6))
		testmode = USB_TEST_K;
	else if (!strncmp(buf, "test_se0_nak", 12))
		testmode = USB_TEST_SE0_NAK;
	else if (!strncmp(buf, "test_packet", 11))
		testmode = USB_TEST_PACKET;
	else if (!strncmp(buf, "test_force_enable", 17))
		testmode = USB_TEST_FORCE_ENABLE;
	else
		testmode = 0;

	spin_lock_irqsave(&gg->lock, flags);
	aic_set_test_mode(gg, testmode);
	spin_unlock_irqrestore(&gg->lock, flags);
	return count;
}

static int testmode_show(struct seq_file *s, void *unused)
{
	struct aic_usb_gadget *gg = s->private;
	unsigned long flags;
	int dctl;

	spin_lock_irqsave(&gg->lock, flags);
	dctl = aic_readl(gg, USBDEVFUNC);
	dctl &= USBDEVFUNC_TSTCTL_MASK;
	dctl >>= USBDEVFUNC_TSTCTL_SHIFT;
	spin_unlock_irqrestore(&gg->lock, flags);

	switch (dctl) {
	case 0:
		seq_puts(s, "no test\n");
		break;
	case USB_TEST_J:
		seq_puts(s, "test_j\n");
		break;
	case USB_TEST_K:
		seq_puts(s, "test_k\n");
		break;
	case USB_TEST_SE0_NAK:
		seq_puts(s, "test_se0_nak\n");
		break;
	case USB_TEST_PACKET:
		seq_puts(s, "test_packet\n");
		break;
	case USB_TEST_FORCE_ENABLE:
		seq_puts(s, "test_force_enable\n");
		break;
	default:
		seq_printf(s, "UNKNOWN %d\n", dctl);
	}

	return 0;
}

static int testmode_open(struct inode *inode, struct file *file)
{
	return single_open(file, testmode_show, inode->i_private);
}

static const struct file_operations testmode_fops = {
	.owner		= THIS_MODULE,
	.open		= testmode_open,
	.write		= testmode_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int ep_show(struct seq_file *seq, void *v)
{
	struct aic_usb_ep *ep = seq->private;
	struct aic_usb_gadget *gg = ep->parent;
	struct aic_usb_req *req = NULL;
	int index = ep->index;
	int show_limit = 15;
	unsigned long flags;

	seq_printf(seq, "Endpoint index %d, named %s,  dir %s:\n",
		   ep->index, ep->ep.name, (ep->dir_in ? "in" : "out"));

	/* first show the register state */
	seq_printf(seq, "\tINEPCFG=0x%08x, OUTEPCFG=0x%08x\n",
		   aic_readl(gg, INEPCFG(index)),
		   aic_readl(gg, OUTEPCFG(index)));

	seq_printf(seq, "\tINEPDMAADDR=0x%08x, OUTEPDMAADDR=0x%08x\n",
		   aic_readl(gg, INEPDMAADDR(index)),
		   aic_readl(gg, OUTEPDMAADDR(index)));

	seq_printf(seq, "\tINEPINT=0x%08x, OUTEPINT=0x%08x\n",
		   aic_readl(gg, INEPINT(index)),
		   aic_readl(gg, OUTEPINT(index)));

	seq_printf(seq, "\tINEPTSFSIZ=0x%08x, OUTEPTSFSIZ=0x%08x\n",
		   aic_readl(gg, INEPTSFSIZ(index)),
		   aic_readl(gg, OUTEPTSFSIZ(index)));

	seq_puts(seq, "\n");
	seq_printf(seq, "mps %d\n", ep->ep.maxpacket);
	seq_printf(seq, "total_data=%ld\n", ep->total_data);

	seq_printf(seq, "request list (%p,%p):\n",
		   ep->queue.next, ep->queue.prev);

	spin_lock_irqsave(&gg->lock, flags);

	list_for_each_entry(req, &ep->queue, queue) {
		if (--show_limit < 0) {
			seq_puts(seq, "not showing more requests...\n");
			break;
		}

		seq_printf(seq, "%c req %p: %d bytes @%p, ",
			   req == ep->req ? '*' : ' ',
			   req, req->req.length, req->req.buf);
		seq_printf(seq, "%d done, res %d\n",
			   req->req.actual, req->req.status);
	}

	spin_unlock_irqrestore(&gg->lock, flags);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(ep);

static int fifo_show(struct seq_file *seq, void *v)
{
	struct aic_usb_gadget *gg = seq->private;
	u32 val;
	int idx;

	seq_puts(seq, "Non-periodic FIFOs:\n");
	seq_printf(seq, "RXFIFO: Size %d\n", aic_readl(gg, RXFIFOSIZ));

	val = aic_readl(gg, NPTXFIFOSIZ);
	seq_printf(seq, "NPTXFIFO: Size %d, Start 0x%08x\n",
		   val >> FIFOSIZE_DEPTH_SHIFT,
		   val & FIFOSIZE_STARTADDR_MASK);

	seq_puts(seq, "\nPeriodic TXFIFOs:\n");

	for (idx = 1; idx <= gg->params.num_perio_in_ep; idx++) {
		val = aic_readl(gg, TXFIFOSIZ(idx));

		seq_printf(seq, "\tDPTXFIFO%2d: Size %d, Start 0x%08x\n", idx,
			   val >> FIFOSIZE_DEPTH_SHIFT,
			   val & FIFOSIZE_STARTADDR_MASK);
	}

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(fifo);

static int state_show(struct seq_file *seq, void *v)
{
	struct aic_usb_gadget *gg = seq->private;
	int idx;

	seq_printf(seq, "USBDEVCONF=0x%08x, USBDEVFUNC=0x%08x, USBLINESTS=0x%08x\n",
		   aic_readl(gg, USBDEVCONF),
		 aic_readl(gg, USBDEVFUNC),
		 aic_readl(gg, USBLINESTS));

	seq_printf(seq, "INEPINTMSK=0x%08x, DOEPMASK=0x%08x\n",
		   aic_readl(gg, INEPINTMSK), aic_readl(gg, OUTEPINTMSK));

	seq_printf(seq, "USBINTMSK=0x%08x, USBINTSTS=0x%08x\n",
		   aic_readl(gg, USBINTMSK),
		   aic_readl(gg, USBINTSTS));

	seq_printf(seq, "USBEPINTMSK=0x%08x, USBEPINT=0x%08x\n",
		   aic_readl(gg, USBEPINTMSK),
		   aic_readl(gg, USBEPINT));

	seq_printf(seq, "NPTXFIFOSTS=0x%08x, RXFIFOSTS_DBG=%08x\n",
		   aic_readl(gg, NPTXFIFOSTS),
		   aic_readl(gg, RXFIFOSTS_DBG));

	seq_puts(seq, "\nEndpoint status:\n");

	for (idx = 0; idx < gg->params.num_ep; idx++) {
		u32 in, out;

		in = aic_readl(gg, INEPCFG(idx));
		out = aic_readl(gg, OUTEPCFG(idx));

		seq_printf(seq, "ep%d: INEPCFG=0x%08x, OUTEPCFG=0x%08x",
			   idx, in, out);

		in = aic_readl(gg, INEPTSFSIZ(idx));
		out = aic_readl(gg, OUTEPTSFSIZ(idx));

		seq_printf(seq, ", INEPTSFSIZ=0x%08x, OUTEPTSFSIZ=0x%08x",
			   in, out);

		seq_puts(seq, "\n");
	}

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(state);

static int params_show(struct seq_file *seq, void *v)
{
	struct aic_usb_gadget *gg = seq->private;
	struct aic_gadget_params *p = &gg->params;
	int i;

	print_param(seq, p, num_ep);
	print_param(seq, p, num_perio_in_ep);
	print_param(seq, p, total_fifo_size);
	print_param(seq, p, rx_fifo_size);
	print_param(seq, p, np_tx_fifo_size);

	for (i = 1; i <= PERIOD_IN_EP_NUM; i++) {
		char str[32];

		snprintf(str, 32, "p_tx_fifo_size[%d]", i);
		seq_printf(seq, "%-30s: %d\n", str, p->p_tx_fifo_size[i]);
	}

	print_param(seq, p, ep_dirs);
	print_param(seq, p, speed);
	print_param(seq, p, phy_type);
	print_param(seq, p, phy_ulpi_ddr);
	print_param(seq, p, phy_utmi_width);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(params);

int aic_udc_debugfs_init(struct aic_usb_gadget *gg)
{
	struct dentry *root = NULL;
	int i = 0;
	int ret = 0;

	root = debugfs_create_dir(dev_name(gg->dev), NULL);
	gg->debug_root = root;

	debugfs_create_file("params", 0444, root, gg, &params_fops);
	/* create general state file */
	debugfs_create_file("state", 0444, root, gg, &state_fops);
	debugfs_create_file("fifo", 0444, root, gg, &fifo_fops);
	debugfs_create_file("testmode", 0644, root, gg, &testmode_fops);

	/* Create one file for each out endpoint */
	for (i = 0; i < gg->params.num_ep; i++) {
		struct aic_usb_ep *ep;

		ep = gg->eps_out[i];
		if (ep)
			debugfs_create_file(ep->name, 0444, root, ep, &ep_fops);
	}

	/* Create one file for each in endpoint. EP0 is handled with out eps */
	for (i = 1; i < gg->params.num_ep; i++) {
		struct aic_usb_ep *ep;

		ep = gg->eps_in[i];
		if (ep)
			debugfs_create_file(ep->name, 0444, root, ep, &ep_fops);
	}

	/* regdump */
	gg->regset = devm_kzalloc(gg->dev, sizeof(*gg->regset),
				  GFP_KERNEL);
	if (!gg->regset) {
		ret = -ENOMEM;
		goto err;
	}
	gg->regset->regs = aic_udc_regs;
	gg->regset->nregs = ARRAY_SIZE(aic_udc_regs);
	gg->regset->base = gg->regs;
	debugfs_create_regset32("regdump", 0444, root, gg->regset);

	return 0;
err:
	debugfs_remove_recursive(gg->debug_root);
	return ret;
}

void aic_udc_debugfs_exit(struct aic_usb_gadget *gg)
{
	debugfs_remove_recursive(gg->debug_root);
	gg->debug_root = NULL;
}
#else
static inline int aic_udc_debugfs_init(struct aic_usb_gadget *gg)
{  return 0;  }
static inline void aic_udc_debugfs_exit(struct aic_usb_gadget *gg)
{  }
#endif

static inline struct aic_usb_req *our_req(struct usb_request *req)
{
	return container_of(req, struct aic_usb_req, req);
}

static inline struct aic_usb_ep *our_ep(struct usb_ep *ep)
{
	return container_of(ep, struct aic_usb_ep, ep);
}

static inline struct aic_usb_gadget *our_gadget(struct usb_gadget *gadget)
{
	return container_of(gadget, struct aic_usb_gadget, gadget);
}

static inline u32 aic_readl(struct aic_usb_gadget *gg, u32 offset)
{
	return readl(gg->regs + offset);
}

static inline void aic_writel(struct aic_usb_gadget *gg, u32 value,
			      u32 offset)
{
	writel(value, gg->regs + offset);
}

static inline void aic_readl_rep(struct aic_usb_gadget *gg, u32 offset,
				 void *buffer, unsigned int count)
{
	if (count) {
		u32 *buf = buffer;

		do {
			u32 x = aic_readl(gg, offset);
			*buf++ = x;
		} while (--count);
	}
}

static inline void aic_writel_rep(struct aic_usb_gadget *gg, u32 offset,
				  const void *buffer, unsigned int count)
{
	if (count) {
		const u32 *buf = buffer;

		do {
			aic_writel(gg, *buf++, offset);
		} while (--count);
	}
}

static inline void aic_set_bit(struct aic_usb_gadget *gg, u32 offset, u32 val)
{
	aic_writel(gg, aic_readl(gg, offset) | val, offset);
}

static inline void aic_clear_bit(struct aic_usb_gadget *gg, u32 offset, u32 val)
{
	aic_writel(gg, aic_readl(gg, offset) & ~val, offset);
}

int aic_wait_bit_clear(struct aic_usb_gadget *gg, u32 offset, u32 mask,
		       u32 timeout)
{
	u32 i;

	for (i = 0; i < timeout; i++) {
		if (!(aic_readl(gg, offset) & mask))
			return 0;
		udelay(1);
	}

	return -ETIMEDOUT;
}

int aic_wait_bit_set(struct aic_usb_gadget *gg, u32 offset, u32 mask,
		     u32 timeout)
{
	u32 i;

	for (i = 0; i < timeout; i++) {
		if (aic_readl(gg, offset) & mask)
			return 0;
		udelay(1);
	}

	return -ETIMEDOUT;
}

static void aic_en_gsint(struct aic_usb_gadget *gg, u32 ints)
{
	u32 gsintmsk = aic_readl(gg, USBINTMSK);
	u32 new_gsintmsk;

	new_gsintmsk = gsintmsk | ints;

	if (new_gsintmsk != gsintmsk) {
		dev_dbg(gg->dev, "gsintmsk now 0x%08x\n", new_gsintmsk);
		aic_writel(gg, new_gsintmsk, USBINTMSK);
	}
}

static void aic_dis_gsint(struct aic_usb_gadget *gg, u32 ints)
{
	u32 gsintmsk = aic_readl(gg, USBINTMSK);
	u32 new_gsintmsk;

	new_gsintmsk = gsintmsk & ~ints;

	if (new_gsintmsk != gsintmsk)
		aic_writel(gg, new_gsintmsk, USBINTMSK);
}

static void aic_ctrl_epint(struct aic_usb_gadget *gg,
			   unsigned int ep, unsigned int dir_in,
			   unsigned int en)
{
	unsigned long flags;
	u32 bit = 1 << ep;
	u32 daint;

	if (!dir_in)
		bit <<= 16;

	local_irq_save(flags);
	daint = aic_readl(gg, USBEPINTMSK);
	if (en)
		daint |= bit;
	else
		daint &= ~bit;
	aic_writel(gg, daint, USBEPINTMSK);
	local_irq_restore(flags);
}

static int aic_low_hw_enable(struct aic_usb_gadget *gg)
{
	int ret = 0;
	int i = 0;

	/* enable regulator */
	/* enable clock */
	for (i = 0; i < USB_MAX_CLKS_RSTS && gg->clks[i]; i++)
		ret = clk_prepare_enable(gg->clks[i]);
	/* wait USB-PHY work stable */
	udelay(200);

	/* reset deassert */
	reset_control_deassert(gg->reset);
	reset_control_deassert(gg->reset_ecc);
	udelay(200);

	/* enable phy */
	if (gg->uphy) {
		ret = usb_phy_init(gg->uphy);
	} else {
		ret = phy_power_on(gg->phy);
		if (ret == 0)
			ret = phy_init(gg->phy);
	}

	return ret;
}

static int aic_low_hw_disable(struct aic_usb_gadget *gg)
{
	int ret = 0;
	int i = 0;

	/* disable phy */
	if (gg->uphy) {
		usb_phy_shutdown(gg->uphy);
	} else {
		ret = phy_exit(gg->phy);
		if (ret == 0)
			ret = phy_power_off(gg->phy);
		else
			return ret;
	}

	/* reset assert */
	reset_control_assert(gg->reset);
	reset_control_assert(gg->reset_ecc);

	/* disable clock */
	for (i = 0; i < USB_MAX_CLKS_RSTS && gg->clks[i]; i++)
		clk_disable_unprepare(gg->clks[i]);

	/* disable regulator */

	return ret;
}

static u32 aic_read_frameno(struct aic_usb_gadget *gg)
{
	u32 reg = 0;

	reg = aic_readl(gg, USBLINESTS);
	reg &= USBLINESTS_SOFFN_MASK;
	reg >>= USBLINESTS_SOFFN_SHIFT;

	return reg;
}

static bool aic_target_frame_elapsed(struct aic_usb_ep *a_ep)
{
	struct aic_usb_gadget *gg = a_ep->parent;
	u32 target_frame = a_ep->target_frame;
	u32 current_frame = gg->frame_number;
	bool frame_overrun = a_ep->frame_overrun;

	if (!frame_overrun && current_frame >= target_frame)
		return true;

	if (frame_overrun && current_frame >= target_frame &&
	    ((current_frame - target_frame) < USBLINESTS_SOFFN_LIMIT / 2))
		return true;

	return false;
}

static inline void aic_incr_frame_num(struct aic_usb_ep *a_ep)
{
	a_ep->target_frame += a_ep->interval;
	if (a_ep->target_frame > USBLINESTS_SOFFN_LIMIT) {
		a_ep->frame_overrun = true;
		a_ep->target_frame &= USBLINESTS_SOFFN_LIMIT;
	} else {
		a_ep->frame_overrun = false;
	}
}

static void aic_set_nextep(struct aic_usb_gadget *gg)
{
	int i;
	u32 reg = 0;

	/* dma to set the next-endpoint pointer. */
	for (i = 0; i < gg->params.num_ep; i++) {
		u32 next = EPCTL_NEXTEP((i + 1) % 15);

		if (gg->eps_in[i]) {
			reg = aic_readl(gg, INEPCFG(i));
			reg &= ~EPCTL_NEXTEP_MASK;
			reg |= next;
			aic_writel(gg, reg, INEPCFG(i));
		}
	}
}

static int aic_core_rst(struct aic_usb_gadget *gg)
{
	u32 reset;

	/* Core Soft Reset */
	reset = aic_readl(gg, USBDEVINIT);
	reset |= USBDEVINIT_CSFTRST;
	aic_writel(gg, reset, USBDEVINIT);

	if (aic_wait_bit_clear(gg, USBDEVINIT, USBDEVINIT_CSFTRST, 10000)) {
		dev_warn(gg->dev, "%s: HANG! Soft Reset timeout GRSTCTL GRSTCTL_CSFTRST\n",
			 __func__);
		return -EBUSY;
	}

	/* Wait for AHB master IDLE state */
	if (aic_wait_bit_set(gg, AHBBASIC, AHBBASIC_AHBIDLE, 10000)) {
		dev_warn(gg->dev, "%s: HANG! AHB Idle timeout GRSTCTL GRSTCTL_AHBIDLE\n",
			 __func__);
		return -EBUSY;
	}

	return 0;
}

static int aic_hs_phy_init(struct aic_usb_gadget *gg)
{
	u32 phyif, phyif_old;
	int ret = 0;

	phyif = aic_readl(gg, USBPHYIF);
	phyif_old = phyif;

	switch (gg->params.phy_type) {
	case AIC_PHY_TYPE_PARAM_ULPI:
		/* ULPI interface */
		dev_dbg(gg->dev, "HS ULPI PHY selected\n");
		phyif |= USBPHYIF_ULPI_UTMI_SEL;
		phyif &= ~(USBPHYIF_PHYIF16 | USBPHYIF_DDRSEL);
		if (gg->params.phy_ulpi_ddr)
			phyif |= USBPHYIF_DDRSEL;
		break;
	case AIC_PHY_TYPE_PARAM_UTMI:
		/* UTMI+ interface */
		dev_dbg(gg->dev, "HS UTMI+ PHY selected\n");
		phyif &= ~(USBPHYIF_ULPI_UTMI_SEL | USBPHYIF_PHYIF16);
		if (gg->params.phy_utmi_width == 16)
			phyif |= USBPHYIF_PHYIF16;

		phyif &= ~USBPHYIF_USBTRDTIM_MASK;
		if (gg->params.phy_utmi_width == 16)
			phyif |= 5 << USBPHYIF_USBTRDTIM_SHIFT;
		else
			phyif |= 9 << USBPHYIF_USBTRDTIM_SHIFT;
		break;
	default:
		dev_err(gg->dev, "FS PHY selected at HS!\n");
		break;
	}

	if (phyif != phyif_old) {
		aic_writel(gg, phyif, USBPHYIF);

		/* Reset after setting the PHY parameters */
		ret = aic_core_rst(gg);
		if (ret) {
			dev_err(gg->dev,
				"%s: Reset failed, aborting", __func__);
			return ret;
		}
	}

	return ret;
}

static int aic_init_fifo(struct aic_usb_gadget *gg)
{
	u32 addr = 0;
	u32 val = 0;
	int i = 0;

	/* reset fifo map of endpoints */
	WARN_ON(gg->fifo_map);
	gg->fifo_map = 0;

	/* rx fifo */
	aic_writel(gg, gg->params.rx_fifo_size, RXFIFOSIZ);

	/* tx no-period fifo */
	aic_writel(gg, (gg->params.rx_fifo_size <<
		    FIFOSIZE_STARTADDR_SHIFT) |
		    (gg->params.np_tx_fifo_size << FIFOSIZE_DEPTH_SHIFT),
		    NPTXFIFOSIZ);

	/* tx period fifos */
	addr = gg->params.rx_fifo_size + gg->params.np_tx_fifo_size;
	for (i = 1; i <= gg->params.num_perio_in_ep; i++) {
		val = addr;
		val |= gg->params.p_tx_fifo_size[i] << FIFOSIZE_DEPTH_SHIFT;
		aic_writel(gg, val, TXFIFOSIZ(i));
		WARN_ONCE(addr + gg->params.p_tx_fifo_size[i] >
				gg->params.total_fifo_size,
				"insufficient fifo memory");
		addr += gg->params.p_tx_fifo_size[i];
	}

	/* flush all fifo */
	val = aic_readl(gg, USBDEVINIT);
	val &= ~USBDEVINIT_TXFNUM_MASK;
	val |= USBDEVINIT_TXFNUM(0x10);
	val |= USBDEVINIT_TXFFLSH | USBDEVINIT_RXFFLSH;
	aic_writel(gg, val,  USBDEVINIT);
	if (aic_wait_bit_clear(gg, USBDEVINIT,
			       (USBDEVINIT_TXFFLSH | USBDEVINIT_RXFFLSH),
			       100)) {
		dev_warn(gg->dev, "%s: flush fifo timeout\n", __func__);
		return -EBUSY;
	}

	dev_dbg(gg->dev, "FIFOs reset.\n");
	return 0;
}

void aic_flush_tx_fifo(struct aic_usb_gadget *gg, const int num)
{
	u32 rst;

	dev_dbg(gg->dev, "Flush Tx FIFO %d\n", num);

	/* Wait for AHB master IDLE state */
	if (aic_wait_bit_set(gg, AHBBASIC, AHBBASIC_AHBIDLE, 10000))
		dev_warn(gg->dev, "%s:  HANG! AHB Idle GRSCTL\n",
			 __func__);

	rst = aic_readl(gg, USBDEVINIT);
	rst &= ~USBDEVINIT_TXFNUM_MASK;
	rst |= (num << USBDEVINIT_TXFNUM_SHIFT) & USBDEVINIT_TXFNUM_MASK;
	rst |= USBDEVINIT_TXFFLSH;
	aic_writel(gg, rst, USBDEVINIT);

	if (aic_wait_bit_clear(gg, USBDEVINIT, USBDEVINIT_TXFFLSH, 10000))
		dev_warn(gg->dev, "%s:  HANG! timeout GRSTCTL GRSTCTL_TXFFLSH\n",
			 __func__);

	/* Wait for at least 3 PHY Clocks */
	udelay(1);
}

void aic_flush_rx_fifo(struct aic_usb_gadget *gg)
{
	u32 rst;

	dev_dbg(gg->dev, "%s()\n", __func__);

	/* Wait for AHB master IDLE state */
	if (aic_wait_bit_set(gg, AHBBASIC, AHBBASIC_AHBIDLE, 10000))
		dev_warn(gg->dev, "%s:  HANG! AHB Idle GRSCTL\n",
			 __func__);

	rst = aic_readl(gg, USBDEVINIT);
	rst |= USBDEVINIT_RXFFLSH;
	aic_writel(gg, rst, USBDEVINIT);

	/* Wait for RxFIFO flush done */
	if (aic_wait_bit_clear(gg, USBDEVINIT, USBDEVINIT_RXFFLSH, 10000))
		dev_warn(gg->dev, "%s: HANG! timeout GRSTCTL GRSTCTL_RXFFLSH\n",
			 __func__);

	/* Wait for at least 3 PHY Clocks */
	udelay(1);
}

static void aic_soft_disconnect(struct aic_usb_gadget *gg)
{
	/* set the soft-disconnect bit */
	aic_set_bit(gg, USBDEVFUNC, USBDEVFUNC_SFTDISCON);
}

static void aic_soft_connect(struct aic_usb_gadget *gg)
{
	/* remove the soft-disconnect and let's go */
	aic_clear_bit(gg, USBDEVFUNC, USBDEVFUNC_SFTDISCON);
}

static u32 aic_read_ep_interrupts(struct aic_usb_gadget *gg,
				  unsigned int idx, int dir_in)
{
	u32 msk_addr = dir_in ? INEPINTMSK : OUTEPINTMSK;
	u32 int_addr = dir_in ? INEPINT(idx) : OUTEPINT(idx);
	u32 ints;
	u32 mask;

	mask = aic_readl(gg, msk_addr);
	mask |= EPINT_SETUP_RCVD;

	ints = aic_readl(gg, int_addr);
	ints &= mask;

	return ints;
}

static int aic_set_test_mode(struct aic_usb_gadget *gg, int testmode)
{
	int ctl = aic_readl(gg, USBDEVFUNC);

	ctl &= ~USBDEVFUNC_TSTCTL_MASK;
	switch (testmode) {
	case USB_TEST_J:
	case USB_TEST_K:
	case USB_TEST_SE0_NAK:
	case USB_TEST_PACKET:
	case USB_TEST_FORCE_ENABLE:
		ctl |= testmode << USBDEVFUNC_TSTCTL_SHIFT;
		break;
	default:
		return -EINVAL;
	}
	aic_writel(gg, ctl, USBDEVFUNC);
	return 0;
}

static u32 aic_ep0_mps(unsigned int mps)
{
	switch (mps) {
	case 64:
		return EP0CTL_MPS_64;
	case 32:
		return EP0CTL_MPS_32;
	case 16:
		return EP0CTL_MPS_16;
	case 8:
		return EP0CTL_MPS_8;
	}

	/* bad max packet size, warn and return invalid result */
	WARN_ON(1);
	return (u32)-1;
}

static inline struct aic_usb_ep *index_to_ep(struct aic_usb_gadget *gg,
					     u32 index, u32 dir_in)
{
	if (dir_in)
		return gg->eps_in[index];
	else
		return gg->eps_out[index];
}

static struct aic_usb_ep *windex_to_ep(struct aic_usb_gadget *gg,
				       u32 windex)
{
	struct aic_usb_ep *a_ep;
	int dir = (windex & USB_DIR_IN) ? 1 : 0;
	int idx = windex & 0x7F;

	if (windex >= 0x100)
		return NULL;

	if (idx > gg->params.num_ep)
		return NULL;

	a_ep = index_to_ep(gg, idx, dir);

	if (idx && a_ep->dir_in != dir)
		return NULL;

	return a_ep;
}

static bool on_list(struct aic_usb_ep *ep, struct aic_usb_req *test)
{
	struct aic_usb_req *req = NULL, *treq;

	list_for_each_entry_safe(req, treq, &ep->queue, queue) {
		if (req == test)
			return true;
	}

	return false;
}

static struct aic_usb_req *get_ep_head(struct aic_usb_ep *a_ep)
{
	return list_first_entry_or_null(&a_ep->queue, struct aic_usb_req,
					queue);
}

static unsigned int get_ep_limit(struct aic_usb_ep *a_ep)
{
	int index = a_ep->index;
	unsigned int maxsize;
	unsigned int maxpkt;

	if (index != 0) {
		maxsize = EPTSIZ_XFERSIZE_LIMIT + 1;
		maxpkt = EPTSIZ_PKTCNT_LIMIT + 1;
	} else {
		maxsize = 64 + 64;
		if (a_ep->dir_in)
			maxpkt = INEPTSFSIZ0_PKTCNT_LIMIT + 1;
		else
			maxpkt = 2;
	}

	/* we made the constant loading easier above by using +1 */
	maxpkt--;
	maxsize--;

	/*
	 * constrain by packet count if maxpkts*pktsize is greater
	 * than the length register size.
	 */

	if ((maxpkt * a_ep->ep.maxpacket) < maxsize)
		maxsize = maxpkt * a_ep->ep.maxpacket;

	return maxsize;
}

static void aic_set_ep_maxpacket(struct aic_usb_gadget *gg,
				 unsigned int ep, unsigned int mps,
				 unsigned int mc, unsigned int dir_in)
{
	struct aic_usb_ep *a_ep;
	u32 reg;

	a_ep = index_to_ep(gg, ep, dir_in);
	if (!a_ep)
		return;

	if (ep == 0) {
		u32 mps_bytes = mps;

		/* EP0 is a special case */
		mps = aic_ep0_mps(mps_bytes);
		if (mps > 3)
			goto bad_mps;
		a_ep->ep.maxpacket = mps_bytes;
		a_ep->mc = 1;
	} else {
		if (mps > 1024)
			goto bad_mps;
		a_ep->mc = mc;
		if (mc > 3)
			goto bad_mps;
		a_ep->ep.maxpacket = mps;
	}

	if (dir_in) {
		reg = aic_readl(gg, INEPCFG(ep));
		reg &= ~EPCTL_MPS_MASK;
		reg |= mps;
		aic_writel(gg, reg, INEPCFG(ep));
	} else {
		reg = aic_readl(gg, OUTEPCFG(ep));
		reg &= ~EPCTL_MPS_MASK;
		reg |= mps;
		aic_writel(gg, reg, OUTEPCFG(ep));
	}

	return;

bad_mps:
	dev_err(gg->dev, "ep%d: bad mps of %d\n", ep, mps);
}

static int aic_core_hw_init1(struct aic_usb_gadget *gg, bool is_usb_reset)
{
	u32 reg = 0;
	int i = 0;
	int ret = 0;

	if (!is_usb_reset) {
		/* unmask subset of endpoint interrupts */
		aic_writel(gg, INEPINTMSK_TIMEOUTMSK | INEPINTMSK_AHBERRMSK |
			    INEPINTMSK_EPDISBLDMSK | INEPINTMSK_XFERCOMPLMSK,
			    INEPINTMSK);
		aic_writel(gg, OUTEPINTMSK_SETUPMSK | OUTEPINTMSK_AHBERRMSK |
			    OUTEPINTMSK_EPDISBLDMSK | OUTEPINTMSK_XFERCOMPLMSK,
			    OUTEPINTMSK);

		/* mask all endpoint interrupt */
		aic_writel(gg, 0, USBEPINTMSK);

		/* soft disconnect */
		aic_writel(gg, USBDEVFUNC, USBDEVFUNC_SFTDISCON);

		/* enable DMA */
		aic_set_bit(gg, USBDEVINIT, USBDEVINIT_DMA_EN);
	}

	/* Kill any ep0 requests as controller will be reinitialized */
	aic_kill_ep_reqs(gg, gg->eps_out[0], -ECONNRESET);

	if (!is_usb_reset) {
		/* core reset */
		ret = aic_core_rst(gg);
		if (ret) {
			dev_err(gg->dev,
				"%s: Reset failed, aborting", __func__);
			return ret;
		}
	} else {
		/* all endpoints should be shutdown */
		for (i = 1; i < gg->params.num_ep; i++) {
			if (gg->eps_in[i])
				aic_ep_disable_nolock(&gg->eps_in[i]->ep);
			if (gg->eps_out[i])
				aic_ep_disable_nolock(&gg->eps_out[i]->ep);
		}
	}

	/* HS/FS timeout calibration */
	reg = aic_readl(gg, USBPHYIF);
	reg &= ~USBPHYIF_TOUTCAL_MASK;
	reg |= USBPHYIF_TOUTCAL(7);
	aic_writel(gg, reg, USBPHYIF);

	return ret;
}

static void aic_core_hw_init2(struct aic_usb_gadget *gg, bool is_usb_reset)
{
	u32 reg = 0;

	if (!is_usb_reset) {
		/* soft disconnect */
		aic_set_bit(gg, USBDEVFUNC, USBDEVFUNC_SFTDISCON);
	}

	/* device speed */
	reg = USBDEVCONF_EPMISCNT(1);
	switch (gg->params.speed) {
	case AIC_SPEED_PARAM_LOW:
		reg |= USBDEVCONF_DEVSPD_LS;
		break;
	case AIC_SPEED_PARAM_FULL:
		if (gg->params.phy_type == AIC_PHY_TYPE_PARAM_FS)
			reg |= USBDEVCONF_DEVSPD_FS48;
		else
			reg |= USBDEVCONF_DEVSPD_FS;
		break;
	default:
		reg |= USBDEVCONF_DEVSPD_HS;
	}
	aic_writel(gg, reg, USBDEVCONF);

	/* clear any pending interrupts */
	aic_writel(gg, 0xffffffff, USBINTSTS);

	/* set global interrupt mask */
	reg = USBINTSTS_ERLYSUSP |
		USBINTSTS_GOUTNAKEFF | USBINTSTS_GINNAKEFF |
		USBINTSTS_USBRST | USBINTSTS_ENUMDONE |
		USBINTSTS_USBSUSP | USBINTSTS_WKUPINT;
	reg |= USBINTSTS_INCOMPL_SOIN | USBINTSTS_INCOMPL_SOOUT;
	aic_writel(gg, reg, USBINTMSK);

	/* AHB config */
	reg = USBDEVINIT_GLBL_INTR_EN | USBDEVINIT_DMA_EN;
	reg |= USBDEVINIT_HBSTLEN_INCR4 << USBDEVINIT_HBSTLEN_SHIFT;
	aic_writel(gg, reg, USBDEVINIT);

	/* set IN endpoint interrupt mask */
	reg = INEPINTMSK_EPDISBLDMSK | INEPINTMSK_XFERCOMPLMSK |
		INEPINTMSK_TIMEOUTMSK | INEPINTMSK_AHBERRMSK;
	aic_writel(gg, reg, INEPINTMSK);

	/* set OUT endpoint interrupt mask */
	reg = OUTEPINTMSK_XFERCOMPLMSK | OUTEPINTMSK_STSPHSERCVDMSK |
		OUTEPINTMSK_EPDISBLDMSK | OUTEPINTMSK_AHBERRMSK |
		OUTEPINTMSK_SETUPMSK;
	aic_writel(gg, reg, OUTEPINTMSK);

	/* mask all endpoint interrupt */
	aic_writel(gg, 0, USBEPINTMSK);

	dev_dbg(gg->dev, "EP0: INEPCFG0=0x%08x, OUTEPCFG0=0x%08x\n",
		aic_readl(gg, INEPCFG0),
		aic_readl(gg, OUTEPCFG0));

	/* enable in and out endpoint interrupts */
	aic_en_gsint(gg, USBINTSTS_OEPINT | USBINTSTS_IEPINT);

	/* Enable interrupts for EP0 in and out */
	aic_ctrl_epint(gg, 0, 0, 1);
	aic_ctrl_epint(gg, 0, 1, 1);

	if (!is_usb_reset) {
		/* power-on programming done */
		aic_set_bit(gg, USBDEVFUNC, USBDEVFUNC_PWRONPRGDONE);
		udelay(10);
		aic_clear_bit(gg, USBDEVFUNC, USBDEVFUNC_PWRONPRGDONE);
	}
	dev_dbg(gg->dev, "USBDEVFUNC=0x%08x\n", aic_readl(gg, USBDEVFUNC));

	/* ep0 OUT: set to read 1 8byte packet */
	aic_writel(gg, EPTSIZ_MC(1) | EPTSIZ_PKTCNT(1) |
	       EPTSIZ_XFERSIZE(8), OUTEPTSFSIZ0);
	/* ep0 OUT: enable + active */
	aic_writel(gg, aic_ep0_mps(gg->eps_out[0]->ep.maxpacket) |
	       EPCTL_CNAK | EPCTL_EPENA |
	       EPCTL_USBACTEP,
	       OUTEPCFG0);
	/* ep0 IN: disable + active */
	aic_writel(gg, aic_ep0_mps(gg->eps_out[0]->ep.maxpacket) |
			EPCTL_USBACTEP, INEPCFG0);

	/* clear global NAKs */
	reg = USBDEVFUNC_CGOUTNAK | USBDEVFUNC_CGNPINNAK;
	if (!is_usb_reset)
		reg |= USBDEVFUNC_SFTDISCON;
	aic_set_bit(gg, USBDEVFUNC, reg);

	mdelay(3);

	dev_dbg(gg->dev, "EP0: INEPCFG0=0x%08x, OUTEPCFG0=0x%08x\n",
		aic_readl(gg, INEPCFG0),
		aic_readl(gg, OUTEPCFG0));
}

static int aic_core_init(struct aic_usb_gadget *gg,
			  bool is_usb_reset)
{
	int ret = 0;

	ret = aic_core_hw_init1(gg, is_usb_reset);
	if (ret)
		return ret;

	ret = aic_hs_phy_init(gg);
	if (ret)
		return ret;

	ret = aic_init_fifo(gg);
	if (ret)
		return ret;

	aic_core_hw_init2(gg, is_usb_reset);

	aic_set_nextep(gg);

	aic_ep0_enqueue_setup(gg);

	return 0;
}

static void aic_kill_ep_reqs(struct aic_usb_gadget *gg,
			     struct aic_usb_ep *ep,
			     int result)
{
	unsigned int size;

	ep->req = NULL;

	while (!list_empty(&ep->queue)) {
		struct aic_usb_req *req = get_ep_head(ep);

		aic_ep_complete_request(gg, ep, req, result);
	}

	size = (aic_readl(gg, INEPTXSTS(ep->fifo_index)) & 0xffff) * 4;
	if (size < ep->fifo_size)
		aic_flush_tx_fifo(gg, ep->fifo_index);
}

void aic_core_disconnect(struct aic_usb_gadget *gg)
{
	unsigned int i;

	if (!gg->connected)
		return;

	gg->connected = 0;
	gg->test_mode = 0;

	/* all endpoints should be shutdown */
	for (i = 0; i < gg->params.num_ep; i++) {
		if (gg->eps_in[i])
			aic_kill_ep_reqs(gg, gg->eps_in[i],
					 -ESHUTDOWN);
		if (gg->eps_out[i])
			aic_kill_ep_reqs(gg, gg->eps_out[i],
					 -ESHUTDOWN);
	}

	aic_gadget_driver_cb(gg, disconnect);

	usb_gadget_set_state(&gg->gadget, USB_STATE_NOTATTACHED);
}

static void aic_ep_complete_empty(struct usb_ep *ep,
				  struct usb_request *req)
{
	struct aic_usb_ep *a_ep = our_ep(ep);
	struct aic_usb_gadget *gg = a_ep->parent;

	dev_dbg(gg->dev, "%s: ep %p, req %p\n", __func__, ep, req);

	aic_ep_free_request(ep, req);
}

static int aic_ep0_enqueue_reply(struct aic_usb_gadget *gg,
				 struct aic_usb_ep *ep,
				void *buff,
				int length)
{
	struct usb_request *req;
	int ret;

	dev_dbg(gg->dev, "%s: buff %p, len %d\n", __func__, buff, length);

	req = aic_ep_alloc_request(&ep->ep, GFP_ATOMIC);
	gg->ep0_reply = req;
	if (!req) {
		dev_warn(gg->dev, "%s: cannot alloc req\n", __func__);
		return -ENOMEM;
	}

	req->buf = gg->ep0_buff;
	req->length = length;
	/*
	 * zero flag is for sending zlp in DATA IN stage. It has no impact on
	 * STATUS stage.
	 */
	req->zero = 0;
	req->complete = aic_ep_complete_empty;

	if (length)
		memcpy(req->buf, buff, length);

	ret = aic_ep_queue_request_nolock(&ep->ep, req, GFP_ATOMIC);
	if (ret) {
		dev_warn(gg->dev, "%s: cannot queue req\n", __func__);
		return ret;
	}

	return 0;
}

static int aic_ep0_process_req_feature(struct aic_usb_gadget *gg,
				       struct usb_ctrlrequest *ctrl)
{
	struct aic_usb_ep *ep0 = gg->eps_out[0];
	struct aic_usb_req *a_req;
	bool set = (ctrl->bRequest == USB_REQ_SET_FEATURE);
	struct aic_usb_ep *ep;
	int ret;
	bool halted;
	u32 recip;
	u32 wValue;
	u32 wIndex;

	dev_dbg(gg->dev, "%s: %s_FEATURE\n",
		__func__, set ? "SET" : "CLEAR");

	wValue = le16_to_cpu(ctrl->wValue);
	wIndex = le16_to_cpu(ctrl->wIndex);
	recip = ctrl->bRequestType & USB_RECIP_MASK;

	switch (recip) {
	case USB_RECIP_DEVICE:
		switch (wValue) {
		case USB_DEVICE_REMOTE_WAKEUP:
			if (set)
				gg->remote_wakeup_allowed = 1;
			else
				gg->remote_wakeup_allowed = 0;
			break;

		case USB_DEVICE_TEST_MODE:
			if ((wIndex & 0xff) != 0)
				return -EINVAL;
			if (!set)
				return -EINVAL;

			gg->test_mode = wIndex >> 8;
			break;
		default:
			return -ENOENT;
		}

		ret = aic_ep0_enqueue_reply(gg, ep0, NULL, 0);
		if (ret) {
			dev_err(gg->dev,
				"%s: failed to send reply\n", __func__);
			return ret;
		}
		break;

	case USB_RECIP_ENDPOINT:
		ep = windex_to_ep(gg, wIndex);
		if (!ep) {
			dev_dbg(gg->dev, "%s: no endpoint for 0x%04x\n",
				__func__, wIndex);
			return -ENOENT;
		}

		switch (wValue) {
		case USB_ENDPOINT_HALT:
			halted = ep->halted;

			aic_ep_sethalt_nolock(&ep->ep, set, true);

			ret = aic_ep0_enqueue_reply(gg, ep0, NULL, 0);
			if (ret) {
				dev_err(gg->dev,
					"%s: failed to send reply\n", __func__);
				return ret;
			}
			if (!set && halted) {
				if (ep->req) {
					a_req = ep->req;
					ep->req = NULL;
					list_del_init(&a_req->queue);
					if (a_req->req.complete) {
						spin_unlock(&gg->lock);
						usb_gadget_giveback_request
							(&ep->ep, &a_req->req);
						spin_lock(&gg->lock);
					}
				}

				/* If we have pending request, then start it */
				if (!ep->req)
					aic_ep_start_next_request(ep);
			}

			break;

		default:
			return -ENOENT;
		}
		break;
	default:
		return -ENOENT;
	}
	return 1;
}

static int aic_ep0_process_get_status(struct aic_usb_gadget *gg,
				      struct usb_ctrlrequest *ctrl)
{
	struct aic_usb_ep *ep0 = gg->eps_out[0];
	struct aic_usb_ep *ep;
	__le16 reply;
	u16 status;
	int ret;

	dev_dbg(gg->dev, "%s: USB_REQ_GET_STATUS\n", __func__);

	if (!ep0->dir_in) {
		dev_warn(gg->dev, "%s: direction out?\n", __func__);
		return -EINVAL;
	}

	switch (ctrl->bRequestType & USB_RECIP_MASK) {
	case USB_RECIP_DEVICE:
		status = 1 << USB_DEVICE_SELF_POWERED;
		status |= gg->remote_wakeup_allowed <<
			  USB_DEVICE_REMOTE_WAKEUP;
		reply = cpu_to_le16(status);
		break;

	case USB_RECIP_INTERFACE:
		/* currently, the data result should be zero */
		reply = cpu_to_le16(0);
		break;

	case USB_RECIP_ENDPOINT:
		ep = windex_to_ep(gg, le16_to_cpu(ctrl->wIndex));
		if (!ep)
			return -ENOENT;
		reply = cpu_to_le16(ep->halted ? 1 : 0);
		break;

	default:
		return 0;
	}

	if (le16_to_cpu(ctrl->wLength) != 2)
		return -EINVAL;

	ret = aic_ep0_enqueue_reply(gg, ep0, &reply, 2);
	if (ret) {
		dev_err(gg->dev, "%s: failed to send reply\n", __func__);
		return ret;
	}

	return 1;
}

static void aic_ep0_process_control(struct aic_usb_gadget *gg,
				    struct usb_ctrlrequest *ctrl)
{
	struct aic_usb_ep *ep0 = gg->eps_out[0];
	u32 reg = 0;
	int ret = 0;

	dev_dbg(gg->dev,
		"ctrl Type=%02x, Req=%02x, V=%04x, I=%04x, L=%04x\n",
		ctrl->bRequestType, ctrl->bRequest, ctrl->wValue,
		ctrl->wIndex, ctrl->wLength);

	if (ctrl->wLength == 0) {
		ep0->dir_in = 1;
		gg->ep0_state = AIC_EP0_STATUS_IN;
	} else if (ctrl->bRequestType & USB_DIR_IN) {
		ep0->dir_in = 1;
		gg->ep0_state = AIC_EP0_DATA_IN;
	} else {
		ep0->dir_in = 0;
		gg->ep0_state = AIC_EP0_DATA_OUT;
	}

	if ((ctrl->bRequestType & USB_TYPE_MASK) == USB_TYPE_STANDARD) {
		switch (ctrl->bRequest) {
		case USB_REQ_SET_ADDRESS:
			gg->connected = 1;
			reg = aic_readl(gg, USBDEVCONF);
			reg &= ~USBDEVCONF_DEVADDR_MASK;
			reg |= (le16_to_cpu(ctrl->wValue) <<
				 USBDEVCONF_DEVADDR_SHIFT) &
				 USBDEVCONF_DEVADDR_MASK;
			aic_writel(gg, reg, USBDEVCONF);

			dev_info(gg->dev, "new address %d\n", ctrl->wValue);

			ret = aic_ep0_enqueue_reply(gg, ep0, NULL, 0);
			return;

		case USB_REQ_GET_STATUS:
			ret = aic_ep0_process_get_status(gg, ctrl);
			break;

		case USB_REQ_CLEAR_FEATURE:
		case USB_REQ_SET_FEATURE:
			ret = aic_ep0_process_req_feature(gg, ctrl);
			break;
		}
	}

	/* driver's setup() callback */
	if (ret == 0 && gg->driver) {
		spin_unlock(&gg->lock);
		ret = gg->driver->setup(&gg->gadget, ctrl);
		spin_lock(&gg->lock);
		if (ret < 0)
			dev_dbg(gg->dev, "driver->setup() ret %d\n", ret);
	}

	gg->delayed_status = false;
	if (ret == USB_GADGET_DELAYED_STATUS)
		gg->delayed_status = true;

	if (ret < 0)
		aic_stall_ep0(gg);
}

static void aic_ep0_complete_setup(struct usb_ep *ep,
				   struct usb_request *req)
{
	struct aic_usb_ep *hs_ep = our_ep(ep);
	struct aic_usb_gadget *gg = hs_ep->parent;

	if (req->status < 0) {
		dev_dbg(gg->dev, "%s: failed %d\n", __func__, req->status);
		return;
	}

	spin_lock(&gg->lock);
	if (req->actual == 0)
		aic_ep0_enqueue_setup(gg);
	else
		aic_ep0_process_control(gg, req->buf);
	spin_unlock(&gg->lock);
}

static void aic_ep0_enqueue_setup(struct aic_usb_gadget *gg)
{
	struct usb_request *req = gg->ctrl_req;
	struct aic_usb_req *a_req = our_req(req);
	int ret;

	dev_dbg(gg->dev, "%s: queueing setup request\n", __func__);

	req->zero = 0;
	req->length = 8;
	req->buf = gg->ctrl_buff;
	req->complete = aic_ep0_complete_setup;

	if (!list_empty(&a_req->queue)) {
		dev_dbg(gg->dev, "%s already queued???\n", __func__);
		return;
	}

	gg->eps_out[0]->dir_in = 0;
	gg->eps_out[0]->send_zlp = 0;
	gg->ep0_state = AIC_EP0_SETUP;

	ret = aic_ep_queue_request_nolock(&gg->eps_out[0]->ep, req, GFP_ATOMIC);
	if (ret < 0)
		dev_err(gg->dev, "%s: failed queue (%d)\n", __func__, ret);
}

static void aic_ep_program_zlp(struct aic_usb_gadget *gg,
			       struct aic_usb_ep *a_ep)
{
	u32 ctrl;
	u8 index = a_ep->index;
	u32 ctl_addr = a_ep->dir_in ? INEPCFG(index) : OUTEPCFG(index);
	u32 siz_addr = a_ep->dir_in ? INEPTSFSIZ(index) : OUTEPTSFSIZ(index);

	if (a_ep->dir_in)
		dev_dbg(gg->dev, "Sending zero-length packet on ep%d\n",
			index);
	else
		dev_dbg(gg->dev, "Receiving zero-length packet on ep%d\n",
			index);

	aic_writel(gg, EPTSIZ_MC(1) | EPTSIZ_PKTCNT(1) |
			    EPTSIZ_XFERSIZE(0),
			    siz_addr);

	ctrl = aic_readl(gg, ctl_addr);
	ctrl |= EPCTL_CNAK;  /* clear NAK set by core */
	ctrl |= EPCTL_EPENA; /* ensure ep enabled */
	ctrl |= EPCTL_USBACTEP;
	aic_writel(gg, ctrl, ctl_addr);
}

static void aic_ep0_program_zlp(struct aic_usb_gadget *gg, bool dir_in)
{
	/* eps_out[0] is used in both directions */
	gg->eps_out[0]->dir_in = dir_in;
	gg->ep0_state = dir_in ? AIC_EP0_STATUS_IN : AIC_EP0_STATUS_OUT;

	aic_ep_program_zlp(gg, gg->eps_out[0]);
}

static void aic_epint_handle_nak(struct aic_usb_ep *a_ep)
{
	struct aic_usb_gadget *gg = a_ep->parent;
	int dir_in = a_ep->dir_in;

	if (!dir_in || !a_ep->isochronous)
		return;

	if (a_ep->target_frame == TARGET_FRAME_INITIAL) {
		a_ep->target_frame = gg->frame_number;
		if (a_ep->interval > 1) {
			u32 ctrl = aic_readl(gg,
					      INEPCFG(a_ep->index));
			if (a_ep->target_frame & 0x1)
				ctrl |= EPCTL_SETODDFR;
			else
				ctrl |= EPCTL_SETEVENFR;

			aic_writel(gg, ctrl, INEPCFG(a_ep->index));
		}
		aic_ep_complete_request(gg, a_ep,
					get_ep_head(a_ep), 0);
	}

	aic_incr_frame_num(a_ep);
}

static void aic_epint_handle_outtoken_ep_disabled(struct aic_usb_ep *ep)
{
	struct aic_usb_gadget *gg = ep->parent;
	int dir_in = ep->dir_in;
	u32 doepmsk;

	if (dir_in || !ep->isochronous)
		return;

	if (ep->interval > 1 &&
	    ep->target_frame == TARGET_FRAME_INITIAL) {
		u32 ctrl;

		ep->target_frame = gg->frame_number;
		aic_incr_frame_num(ep);

		ctrl = aic_readl(gg, OUTEPCFG(ep->index));
		if (ep->target_frame & 0x1)
			ctrl |= EPCTL_SETODDFR;
		else
			ctrl |= EPCTL_SETEVENFR;

		aic_writel(gg, ctrl, OUTEPCFG(ep->index));
	}

	aic_ep_start_next_request(ep);
	doepmsk = aic_readl(gg, OUTEPINTMSK);
	doepmsk &= ~OUTEPINTMSK_OUTTKNEPDISMSK;
	aic_writel(gg, doepmsk, OUTEPINTMSK);
}

static void aic_epint_handle_ep_disabled(struct aic_usb_ep *a_ep)
{
	struct aic_usb_gadget *gg = a_ep->parent;
	struct aic_usb_req *a_req;
	unsigned char idx = a_ep->index;
	int dir_in = a_ep->dir_in;
	u32 epctl_addr = dir_in ? INEPCFG(idx) : OUTEPCFG(idx);
	int dctl = aic_readl(gg, USBDEVFUNC);

	dev_dbg(gg->dev, "%s: EPDisbld\n", __func__);

	if (dir_in) {
		int epctl = aic_readl(gg, epctl_addr);

		aic_flush_tx_fifo(gg, a_ep->fifo_index);

		if (a_ep->isochronous) {
			aic_epint_handle_complete_in(gg, a_ep);
			return;
		}

		if ((epctl & EPCTL_STALL) && (epctl & EPCTL_EPTYPE_BULK)) {
			int dctl = aic_readl(gg, USBDEVFUNC);

			dctl |= USBDEVFUNC_CGNPINNAK;
			aic_writel(gg, dctl, USBDEVFUNC);
		}
		return;
	}

	if (dctl & USBDEVFUNC_GOUTNAKSTS) {
		dctl |= USBDEVFUNC_CGOUTNAK;
		aic_writel(gg, dctl, USBDEVFUNC);
	}

	if (!a_ep->isochronous)
		return;

	if (list_empty(&a_ep->queue)) {
		dev_dbg(gg->dev, "%s: complete_ep 0x%p, ep->queue empty!\n",
			__func__, a_ep);
		return;
	}

	do {
		a_req = get_ep_head(a_ep);
		if (a_req)
			aic_ep_complete_request(gg, a_ep, a_req,
						-ENODATA);
		aic_incr_frame_num(a_ep);
		/* Update current frame number value. */
		gg->frame_number = aic_read_frameno(gg);
	} while (aic_target_frame_elapsed(a_ep));

	aic_ep_start_next_request(a_ep);
}

static void aic_epint_handle_complete_in(struct aic_usb_gadget *gg,
					 struct aic_usb_ep *a_ep)
{
	struct aic_usb_req *a_req = a_ep->req;
	u32 epsize = aic_readl(gg, INEPTSFSIZ(a_ep->index));
	int size_left, size_done;

	if (!a_req) {
		dev_dbg(gg->dev, "XferCompl but no req\n");
		return;
	}

	/* Finish ZLP handling for IN EP0 transactions */
	if (a_ep->index == 0 && gg->ep0_state == AIC_EP0_STATUS_IN) {
		dev_dbg(gg->dev, "zlp packet sent\n");

		/*
		 * While send zlp for DWC2_EP0_STATUS_IN EP direction was
		 * changed to IN. Change back to complete OUT transfer request
		 */
		a_ep->dir_in = 0;

		aic_ep_complete_request(gg, a_ep, a_req, 0);
		if (gg->test_mode) {
			int ret;

			ret = aic_set_test_mode(gg, gg->test_mode);
			if (ret < 0) {
				dev_dbg(gg->dev, "Invalid Test #%d\n",
					gg->test_mode);
				aic_stall_ep0(gg);
				return;
			}
		}
		aic_ep0_enqueue_setup(gg);
		return;
	}

	/* calculate xfer length */
	size_left = EPTSIZ_XFERSIZE_GET(epsize);
	size_done = a_ep->size_loaded - size_left;
	size_done += a_ep->last_load;

	/* update req.actual */
	if (a_req->req.actual != size_done)
		dev_dbg(gg->dev, "%s: adjusting size done %d => %d\n",
			__func__, a_req->req.actual, size_done);
	a_req->req.actual = size_done;
	dev_dbg(gg->dev, "req->length:%d req->actual:%d req->zero:%d\n",
		a_req->req.length, a_req->req.actual, a_req->req.zero);

	/* request remain data, continue xfer */
	if (!size_left && a_req->req.actual < a_req->req.length) {
		dev_dbg(gg->dev, "%s trying more for req...\n", __func__);
		aic_ep_start_req(gg, a_ep, a_req, true);
		return;
	}

	/* Zlp for all endpoints, for ep0 only in DATA IN stage */
	if (a_ep->send_zlp) {
		aic_ep_program_zlp(gg, a_ep);
		a_ep->send_zlp = 0;
		/* transfer will be completed on next complete interrupt */
		return;
	}

	if (a_ep->index == 0 && gg->ep0_state == AIC_EP0_DATA_IN) {
		/* Move to STATUS OUT */
		aic_ep0_program_zlp(gg, false);
		return;
	}

	aic_ep_complete_request(gg, a_ep, a_req, 0);
}

static void aic_epint_handle_outdone(struct aic_usb_gadget *gg, int epnum)
{
	u32 epsize = aic_readl(gg, OUTEPTSFSIZ(epnum));
	struct aic_usb_ep *a_ep = gg->eps_out[epnum];
	struct aic_usb_req *a_req = a_ep->req;
	struct usb_request *req = &a_req->req;
	unsigned int size_left = EPTSIZ_XFERSIZE_GET(epsize);
	unsigned int size_done = 0;
	int ret = 0;

	if (!a_req) {
		dev_dbg(gg->dev, "%s: no request active\n", __func__);
		return;
	}

	if (epnum == 0 && gg->ep0_state == AIC_EP0_STATUS_OUT) {
		dev_dbg(gg->dev, "zlp packet received\n");
		aic_ep_complete_request(gg, a_ep, a_req, 0);
		aic_ep0_enqueue_setup(gg);
		return;
	}

	size_done = a_ep->size_loaded - size_left;
	size_done += a_ep->last_load;
	req->actual = size_done;

	/* request remain data, continue xfer */
	if (req->actual < req->length && size_left == 0) {
		aic_ep_start_req(gg, a_ep, a_req, true);
		return;
	}

	if (req->actual < req->length && req->short_not_ok) {
		dev_dbg(gg->dev, "%s: got %d/%d (short not ok) => error\n",
			__func__, req->actual, req->length);
		/*
		 * todo - what should we return here? there's no one else
		 * even bothering to check the status.
		 */
	}

	if (epnum == 0 && gg->ep0_state == AIC_EP0_DATA_OUT) {
		/* Move to STATUS IN */
		if (!gg->delayed_status)
			aic_ep0_program_zlp(gg, true);
	}

	/* Set actual frame number for completed transfers */
	if (a_ep->isochronous)
		req->frame_number = gg->frame_number;

	aic_ep_complete_request(gg, a_ep, a_req, ret);
}

static void aic_epint_irq(struct aic_usb_gadget *gg, unsigned int idx,
			  int dir_in)
{
	struct aic_usb_ep *a_ep = index_to_ep(gg, idx, dir_in);
	u32 int_addr = dir_in ? INEPINT(idx) : OUTEPINT(idx);
	u32 ctl_addr = dir_in ? INEPCFG(idx) : OUTEPCFG(idx);
	u32 siz_addr = dir_in ? INEPTSFSIZ(idx) : OUTEPTSFSIZ(idx);
	u32 ints;
	u32 ctrl;

	if (!a_ep) {
		dev_err(gg->dev, "%s:Interrupt for unconfigured ep%d(%s)\n",
			__func__, idx, dir_in ? "in" : "out");
		return;
	}

	/* read interrupt status */
	ints = aic_read_ep_interrupts(gg, idx, dir_in);
	ctrl = aic_readl(gg, ctl_addr);
	dev_dbg(gg->dev, "%s: ep%d(%s) DxEPINT=0x%08x\n",
		__func__, idx, dir_in ? "in" : "out", ints);

	/* clear interrupt status */
	aic_writel(gg, ints, int_addr);

	/* Don't process XferCompl interrupt if it is a setup packet */
	if (idx == 0 && (ints & (EPINT_SETUP | EPINT_SETUP_RCVD)))
		ints &= ~EPINT_XFERCOMPL;

	if (ints & EPINT_XFERCOMPL) {
		dev_dbg(gg->dev,
			"%s: XferCompl: DxEPCTL=0x%08x, EPTSIZ=%08x\n",
			__func__, aic_readl(gg, ctl_addr),
			aic_readl(gg, siz_addr));

		if (dir_in) {
			/*
			 * We get OutDone from the FIFO, so we only
			 * need to look at completing IN requests here
			 * if operating slave mode
			 */
			if (a_ep->isochronous && a_ep->interval > 1)
				aic_incr_frame_num(a_ep);

			aic_epint_handle_complete_in(gg, a_ep);
			if (ints & EPINT_NAKINTRPT)
				ints &= ~EPINT_NAKINTRPT;

			if (idx == 0 && !a_ep->req)
				aic_ep0_enqueue_setup(gg);
		} else {
			/*
			 * We're using DMA, we need to fire an OutDone here
			 * as we ignore the RXFIFO.
			 */
			if (a_ep->isochronous && a_ep->interval > 1)
				aic_incr_frame_num(a_ep);

			aic_epint_handle_outdone(gg, idx);
		}
	}

	if (ints & EPINT_EPDISBLD)
		aic_epint_handle_ep_disabled(a_ep);

	if (ints & EPINT_OUTTKNEPDIS)
		aic_epint_handle_outtoken_ep_disabled(a_ep);

	if (ints & EPINT_NAKINTRPT)
		aic_epint_handle_nak(a_ep);

	if (ints & EPINT_AHBERR)
		dev_dbg(gg->dev, "%s: AHBErr\n", __func__);

	if (ints & EPINT_SETUP) {  /* Setup or Timeout */
		dev_dbg(gg->dev, "%s: Setup/Timeout\n",  __func__);

		if (idx == 0) {
			/*
			 * this is the notification we've received a
			 * setup packet. In non-DMA mode we'd get this
			 * from the RXFIFO, instead we need to process
			 * the setup here.
			 */

			if (dir_in)
				WARN_ON_ONCE(1);
			else
				aic_epint_handle_outdone(gg, 0);
		}
	}

	if (ints & EPINT_STSPHSERCVD)
		dev_dbg(gg->dev, "%s: StsPhseRcvd\n", __func__);

	if (ints & EPINT_BACK2BACKSETUP)
		dev_dbg(gg->dev, "%s: B2BSetup/INEPNakEff\n", __func__);

	if (dir_in && !a_ep->isochronous) {
		/* not sure if this is important, but we'll clear it anyway */
		if (ints & EPINT_INTKNTXFEMP) {
			dev_dbg(gg->dev, "%s: ep%d: INTknTXFEmpMsk\n",
				__func__, idx);
		}

		/* this probably means something bad is happening */
		if (ints & EPINT_INTKNEPMIS) {
			dev_warn(gg->dev, "%s: ep%d: INTknEP\n",
				 __func__, idx);
		}
	}
}

static void aic_irq_handle_epint(struct aic_usb_gadget *gg)
{
	u32 int_val = aic_readl(gg, USBEPINT);
	u32 mask = aic_readl(gg, USBEPINTMSK);
	u32 int_out, int_in;
	int i;

	int_val &= mask;
	int_out = int_val >> USBEPINT_OUTEP_SHIFT;
	int_in = int_val & 0xFFFF;

	dev_dbg(gg->dev, "%s: int=%08x\n", __func__, int_val);

	for (i = 0; i < gg->params.num_ep && int_out;
					i++, int_out >>= 1) {
		if (int_out & 1)
			aic_epint_irq(gg, i, 0);
	}

	for (i = 0; i < gg->params.num_ep  && int_in;
					i++, int_in >>= 1) {
		if (int_in & 1)
			aic_epint_irq(gg, i, 1);
	}
}

static void aic_irq_handle_enumdone(struct aic_usb_gadget *gg)
{
	u32 reg = 0;
	int ep0_mps = 0;
	int ep_mps = 8;

	/* clear interrupt status */
	aic_writel(gg, USBINTSTS_ENUMDONE, USBINTSTS);

	/* read enumerated speed */
	reg = aic_readl(gg, USBLINESTS);
	dev_dbg(gg->dev, "EnumDone (USBLINESTS=0x%08x)\n", reg);

	switch ((reg & USBLINESTS_ENUMSPD_MASK) >> USBLINESTS_ENUMSPD_SHIFT) {
	case USBLINESTS_ENUMSPD_FS:
	case USBLINESTS_ENUMSPD_FS48:
		gg->gadget.speed = USB_SPEED_FULL;
		ep0_mps = EP0_MPS_LIMIT;
		ep_mps = 1023;
		break;

	case USBLINESTS_ENUMSPD_HS:
		gg->gadget.speed = USB_SPEED_HIGH;
		ep0_mps = EP0_MPS_LIMIT;
		ep_mps = 1024;
		break;

	case USBLINESTS_ENUMSPD_LS:
		gg->gadget.speed = USB_SPEED_LOW;
		ep0_mps = 8;
		ep_mps = 8;
		break;
	}
	dev_info(gg->dev, "new device is %s\n",
		 usb_speed_string(gg->gadget.speed));

	/* update endpoint maxpacket */
	if (ep0_mps) {
		int i;
		/* Initialize ep0 for both in and out directions */
		aic_set_ep_maxpacket(gg, 0, ep0_mps, 0, 1);
		aic_set_ep_maxpacket(gg, 0, ep0_mps, 0, 0);
		for (i = 1; i < gg->params.num_ep; i++) {
			if (gg->eps_in[i])
				aic_set_ep_maxpacket(gg, i, ep_mps,
						     0, 1);
			if (gg->eps_out[i])
				aic_set_ep_maxpacket(gg, i, ep_mps,
						     0, 0);
		}
	}

	/* ep0 request */
	aic_ep0_enqueue_setup(gg);
	dev_dbg(gg->dev, "EP0: INEPCFG0=0x%08x, OUTEPCFG0=0x%08x\n",
		aic_readl(gg, INEPCFG0),
		aic_readl(gg, OUTEPCFG0));
}

static void aic_irq_handle_usbrst(struct aic_usb_gadget *gg)
{
	/* clear interrupt status */
	aic_writel(gg, USBINTSTS_USBRST, USBINTSTS);

	dev_dbg(gg->dev, "%s: USBRst\n", __func__);
	dev_dbg(gg->dev, "NPTXFIFOSTS=%08x\n",
		aic_readl(gg, NPTXFIFOSTS));

	/* Reset device address to zero */
	aic_clear_bit(gg, USBDEVCONF, USBDEVCONF_DEVADDR_MASK);

	if (gg->connected) {
		/* cleanup endpoints request */
		aic_core_disconnect(gg);
		/* reinit hardware core register */
		aic_core_init(gg, true);
	}
}

static void aic_irq_handle_goutnak(struct aic_usb_gadget *gg)
{
	u8 idx;
	u32 epctrl;
	u32 gintmsk;
	u32 daintmsk;
	struct aic_usb_ep *a_ep;

	daintmsk = aic_readl(gg, USBEPINTMSK);
	daintmsk >>= USBEPINT_OUTEP_SHIFT;
	/* Mask this interrupt */
	gintmsk = aic_readl(gg, USBINTMSK);
	gintmsk &= ~USBINTSTS_GOUTNAKEFF;
	aic_writel(gg, gintmsk, USBINTMSK);

	dev_dbg(gg->dev, "GOUTNakEff triggered\n");
	for (idx = 1; idx < gg->params.num_ep; idx++) {
		a_ep = gg->eps_out[idx];
		/* Proceed only unmasked ISOC EPs */
		if ((BIT(idx) & ~daintmsk) || !a_ep->isochronous)
			continue;

		epctrl = aic_readl(gg, OUTEPCFG(idx));

		if (epctrl & EPCTL_EPENA) {
			epctrl |= EPCTL_SNAK;
			epctrl |= EPCTL_EPDIS;
			aic_writel(gg, epctrl, OUTEPCFG(idx));
		}
	}
}

static void aic_irq_handle_incomplete_isoc_in(struct aic_usb_gadget *gg)
{
	struct aic_usb_ep *a_ep;
	u32 epctrl;
	u32 daintmsk;
	u32 idx;

	dev_dbg(gg->dev, "Incomplete isoc in interrupt received:\n");

	/* Clear interrupt */
	aic_writel(gg, USBINTSTS_INCOMPL_SOIN, USBINTSTS);

	daintmsk = aic_readl(gg, USBEPINTMSK);

	for (idx = 1; idx < gg->params.num_ep; idx++) {
		a_ep = gg->eps_in[idx];
		/* Proceed only unmasked ISOC EPs */
		if ((BIT(idx) & ~daintmsk) || !a_ep->isochronous)
			continue;

		epctrl = aic_readl(gg, INEPCFG(idx));
		if ((epctrl & EPCTL_EPENA) &&
		    aic_target_frame_elapsed(a_ep)) {
			epctrl |= EPCTL_SNAK;
			epctrl |= EPCTL_EPDIS;
			aic_writel(gg, epctrl, INEPCFG(idx));
		}
	}
}

static void aic_irq_handle_incomplete_isoc_out(struct aic_usb_gadget *gg)
{
	u32 gintsts;
	u32 gintmsk;
	u32 daintmsk;
	u32 epctrl;
	struct aic_usb_ep *a_ep;
	int idx;

	dev_dbg(gg->dev, "%s: USBINTSTS_INCOMPL_SOOUT\n", __func__);

	/* Clear interrupt */
	aic_writel(gg, USBINTSTS_INCOMPL_SOOUT, USBINTSTS);

	daintmsk = aic_readl(gg, USBEPINTMSK);
	daintmsk >>= USBEPINT_OUTEP_SHIFT;

	for (idx = 1; idx < gg->params.num_ep; idx++) {
		a_ep = gg->eps_out[idx];
		/* Proceed only unmasked ISOC EPs */
		if ((BIT(idx) & ~daintmsk) || !a_ep->isochronous)
			continue;

		epctrl = aic_readl(gg, OUTEPCFG(idx));
		if ((epctrl & EPCTL_EPENA) &&
		    aic_target_frame_elapsed(a_ep)) {
			/* Unmask GOUTNAKEFF interrupt */
			gintmsk = aic_readl(gg, USBINTMSK);
			gintmsk |= USBINTSTS_GOUTNAKEFF;
			aic_writel(gg, gintmsk, USBINTMSK);

			gintsts = aic_readl(gg, USBINTSTS);
			if (!(gintsts & USBINTSTS_GOUTNAKEFF)) {
				aic_set_bit(gg, USBDEVFUNC,
					    USBDEVFUNC_SGOUTNAK);
				break;
			}
		}
	}
}

static irqreturn_t aic_udc_irq(int irq, void *data)
{
	struct aic_usb_gadget *gg = data;
	u32 intsts = 0;
	u32 intmsk = 0;
	int retry = 8;

	spin_lock(&gg->lock);

	intsts = aic_readl(gg, USBINTSTS);
	intmsk = aic_readl(gg, USBINTMSK);

	dev_dbg(gg->dev, "%s: %08x %08x (%08x) retry %d\n",
		__func__, intsts, intsts & intmsk, intmsk, retry);

	if (intsts & USBINTSTS_USBRST)
		aic_irq_handle_usbrst(gg);

	if (intsts & USBINTSTS_ENUMDONE)
		aic_irq_handle_enumdone(gg);

	if (intsts & (USBINTSTS_OEPINT | USBINTSTS_IEPINT))
		aic_irq_handle_epint(gg);

	if (intsts & USBINTSTS_NPTXFEMP) {
		dev_dbg(gg->dev, "NPTxFEmp\n");
		aic_dis_gsint(gg, USBINTSTS_NPTXFEMP);
	}

	if (intsts & USBINTSTS_RXFLVL)
		dev_dbg(gg->dev, "RxFLVL\n");

	if (intsts & USBINTSTS_ERLYSUSP) {
		dev_dbg(gg->dev, "ErlySusp\n");
		aic_writel(gg, USBINTSTS_ERLYSUSP, USBINTSTS);
	}

	if (intsts & USBINTSTS_GOUTNAKEFF)
		aic_irq_handle_goutnak(gg);

	if (intsts & USBINTSTS_GINNAKEFF) {
		dev_info(gg->dev, "GINNakEff triggered\n");
		aic_set_bit(gg, USBDEVFUNC, USBDEVFUNC_CGNPINNAK);
	}

	if (intsts & USBINTSTS_INCOMPL_SOIN)
		aic_irq_handle_incomplete_isoc_in(gg);

	if (intsts & USBINTSTS_INCOMPL_SOOUT)
		aic_irq_handle_incomplete_isoc_out(gg);

	if (intsts & USBINTSTS_WKUPINT) {
		dev_dbg(gg->dev, "WKUP\n");
		aic_writel(gg, USBINTSTS_WKUPINT, USBINTSTS);
	}

	if (intsts & USBINTSTS_USBSUSP) {
		dev_dbg(gg->dev, "USB SUSPEND\n");
		aic_writel(gg, USBINTSTS_USBSUSP, USBINTSTS);
	}

	spin_unlock(&gg->lock);

	return IRQ_HANDLED;
}

static int aic_handle_unaligned_req(struct aic_usb_gadget *gg,
				    struct aic_usb_ep *a_ep,
				    struct aic_usb_req *a_req)
{
	void *req_buf = a_req->req.buf;

	/* If buffer is aligned */
	if ((!((long)req_buf % L1_CACHE_BYTES) &&
	    !(a_req->req.length % L1_CACHE_BYTES))
	    || (a_ep->index == 0))
		return 0;

	WARN_ON(a_req->saved_req_buf);

	dev_dbg(gg->dev, "%s: %s: buf=%p length=%d\n", __func__,
		a_ep->ep.name, req_buf, a_req->req.length);

	a_req->req.buf = kmalloc(ALIGN(a_req->req.length, L1_CACHE_BYTES),
				 GFP_ATOMIC);
	if (!a_req->req.buf) {
		a_req->req.buf = req_buf;
		dev_err(gg->dev,
			"%s: unable to allocate memory for bounce buffer\n",
			__func__);
		return -ENOMEM;
	}

	/* Save actual buffer */
	a_req->saved_req_buf = req_buf;

	if (a_ep->dir_in)
		memcpy(a_req->req.buf, req_buf, a_req->req.length);
	return 0;
}

static void
aic_handle_unaligned_req_complete(struct aic_usb_gadget *gg,
				  struct aic_usb_ep *a_ep,
				  struct aic_usb_req *a_req)
{
	/* If buffer was aligned */
	if (!a_req->saved_req_buf)
		return;

	dev_dbg(gg->dev, "%s: %s: status=%d actual-length=%d\n", __func__,
		a_ep->ep.name, a_req->req.status, a_req->req.actual);

	/* Copy data from bounce buffer on successful out transfer */
	if (!a_ep->dir_in && !a_req->req.status)
		memcpy(a_req->saved_req_buf, a_req->req.buf,
		       a_req->req.actual);

	/* Free bounce buffer */
	kfree(a_req->req.buf);

	a_req->req.buf = a_req->saved_req_buf;
	a_req->saved_req_buf = NULL;
}

static void aic_ep_stop_xfer(struct aic_usb_gadget *gg,
			     struct aic_usb_ep *a_ep)
{
	u32 ctrl_addr;
	u32 int_addr;

	ctrl_addr = a_ep->dir_in ? INEPCFG(a_ep->index) :
		OUTEPCFG(a_ep->index);
	int_addr = a_ep->dir_in ? INEPINT(a_ep->index) :
		OUTEPINT(a_ep->index);

	dev_dbg(gg->dev, "%s: stopping transfer on %s\n", __func__,
		a_ep->name);

	if (a_ep->dir_in) {
		if (a_ep->periodic) {
			aic_set_bit(gg, ctrl_addr, EPCTL_SNAK);
			/* Wait for Nak effect */
			if (aic_wait_bit_set(gg, int_addr,
					     EPINT_INEPNAKEFF, 100))
				dev_warn(gg->dev,
					 "%s: timeout INEPINT.NAKEFF\n",
					 __func__);
		} else {
			aic_set_bit(gg, USBDEVFUNC, USBDEVFUNC_SGNPINNAK);
			/* Wait for Nak effect */
			if (aic_wait_bit_set(gg, USBINTSTS,
					     USBINTSTS_GINNAKEFF, 100))
				dev_warn(gg->dev,
					 "%s: timeout USBINTSTS.GINNAKEFF\n",
					 __func__);
		}
	} else {
		if (!(aic_readl(gg, USBINTSTS) & USBINTSTS_GOUTNAKEFF))
			aic_set_bit(gg, USBDEVFUNC, USBDEVFUNC_SGOUTNAK);

		/* Wait for global nak to take effect */
		if (aic_wait_bit_set(gg, USBINTSTS,
				     USBINTSTS_GOUTNAKEFF, 100))
			dev_warn(gg->dev, "%s: timeout USBINTSTS.GOUTNAKEFF\n",
				 __func__);
	}

	/* Disable ep */
	aic_set_bit(gg, ctrl_addr, EPCTL_EPDIS | EPCTL_SNAK);

	/* Wait for ep to be disabled */
	if (aic_wait_bit_set(gg, int_addr, EPINT_EPDISBLD, 100))
		dev_warn(gg->dev,
			 "%s: timeout OUTEPCFG.EPDisable\n", __func__);

	/* Clear EPDISBLD interrupt */
	aic_set_bit(gg, int_addr, EPINT_EPDISBLD);

	if (a_ep->dir_in) {
		unsigned short fifo_index;

		if (a_ep->periodic)
			fifo_index = a_ep->fifo_index;
		else
			fifo_index = 0;

		/* Flush TX FIFO */
		aic_flush_tx_fifo(gg, fifo_index);

		/* Clear Global In NP NAK in Shared FIFO for non periodic ep */
		if (!a_ep->periodic)
			aic_set_bit(gg, USBDEVFUNC, USBDEVFUNC_CGNPINNAK);

	} else {
		/* Remove global NAKs */
		aic_set_bit(gg, USBDEVFUNC, USBDEVFUNC_CGOUTNAK);
	}
}

static void aic_ep_start_req(struct aic_usb_gadget *gg,
			     struct aic_usb_ep *a_ep,
			     struct aic_usb_req *a_req,
			     bool continuing)
{
	struct usb_request *ureq = &a_req->req;
	int index = a_ep->index;
	int dir_in = a_ep->dir_in;
	u32 dma_reg;
	u32 epctrl_reg;
	u32 epsize_reg;
	u32 epsize;
	u32 ctrl;
	unsigned int length;
	unsigned int packets;
	unsigned int maxreq;

	if (index != 0) {
		if (a_ep->req && !continuing) {
			dev_err(gg->dev, "%s: active request\n", __func__);
			WARN_ON(1);
			return;
		} else if (a_ep->req != a_req && continuing) {
			dev_err(gg->dev,
				"%s: continue different req\n", __func__);
			WARN_ON(1);
			return;
		}
	}

	dma_reg = dir_in ? INEPDMAADDR(index) : OUTEPDMAADDR(index);
	epctrl_reg = dir_in ? INEPCFG(index) : OUTEPCFG(index);
	epsize_reg = dir_in ? INEPTSFSIZ(index) : OUTEPTSFSIZ(index);

	/* stall ignore */
	ctrl = aic_readl(gg, epctrl_reg);
	if (index && ctrl & EPCTL_STALL) {
		dev_warn(gg->dev, "%s: ep%d is stalled\n", __func__, index);
		return;
	}
	dev_dbg(gg->dev, "%s: DxEPCTL=0x%08x, ep %d, dir %s\n",
		__func__, ctrl, index,
		a_ep->dir_in ? "in" : "out");

	/* length */
	length = ureq->length - ureq->actual;
	maxreq = get_ep_limit(a_ep);
	dev_dbg(gg->dev, "ureq->length:%d ureq->actual:%d\n",
		ureq->length, ureq->actual);
	if (length > maxreq) {
		int round = maxreq % a_ep->ep.maxpacket;

		dev_dbg(gg->dev, "%s: length %d, max-req %d, r %d\n",
			__func__, length, maxreq, round);
		if (round)
			maxreq -= round;
		length = maxreq;
	}

	/* multi count */
	if (length)
		packets = DIV_ROUND_UP(length, a_ep->ep.maxpacket);
	else
		packets = 1;	/* send one packet if length is zero. */
	if (dir_in && index != 0)
		if (a_ep->isochronous)
			epsize = EPTSIZ_MC(packets);
		else
			epsize = EPTSIZ_MC(1);
	else
		epsize = 0;

	/* zero length packet */
	if (dir_in && ureq->zero && !continuing) {
		/* Test if zlp is actually required. */
		if ((ureq->length >= a_ep->ep.maxpacket) &&
		    !(ureq->length % a_ep->ep.maxpacket))
			a_ep->send_zlp = 1;
	}

	/* update size & dma */
	epsize |= EPTSIZ_PKTCNT(packets);
	epsize |= EPTSIZ_XFERSIZE(length);
	aic_writel(gg, epsize, epsize_reg);
	dev_dbg(gg->dev, "%s: %d@%d/%d, 0x%08x => 0x%08x\n",
		__func__, packets, length, ureq->length, epsize, epsize_reg);
	if (!continuing && (length != 0)) {
		aic_writel(gg, ureq->dma, dma_reg);
		dev_dbg(gg->dev, "%s: %pad => 0x%08x\n",
			__func__, &ureq->dma, dma_reg);
	}
	a_ep->req = a_req;

	/* update ctrl */
	if (a_ep->isochronous && a_ep->interval == 1) {
		a_ep->target_frame = aic_read_frameno(gg);
		aic_incr_frame_num(a_ep);
		if (a_ep->target_frame & 0x1)
			ctrl |= EPCTL_SETODDFR;
		else
			ctrl |= EPCTL_SETEVENFR;
	}
	dev_dbg(gg->dev, "ep0 state:%d\n", gg->ep0_state);
	if (!(index == 0 && gg->ep0_state == AIC_EP0_SETUP))
		ctrl |= EPCTL_CNAK;	/* clear NAK set by core */
	ctrl |= EPCTL_EPENA;	/* ensure ep enabled */
	dev_dbg(gg->dev, "%s: DxEPCTL=0x%08x\n", __func__, ctrl);
	aic_writel(gg, ctrl, epctrl_reg);

	/* update counter */
	a_ep->size_loaded = length;
	a_ep->last_load = ureq->actual;

	/* check ep is enabled */
	if (!(aic_readl(gg, epctrl_reg) & EPCTL_EPENA))
		dev_dbg(gg->dev,
			"ep%d: failed to become enabled (EPCTL=0x%08x)?\n",
			 index, aic_readl(gg, epctrl_reg));
	dev_dbg(gg->dev, "%s: DxEPCTL=0x%08x\n",
		__func__, aic_readl(gg, epctrl_reg));

	/* enable ep interrupts */
	aic_ctrl_epint(gg, a_ep->index, a_ep->dir_in, 1);
}

static void aic_ep_start_next_request(struct aic_usb_ep *a_ep)
{
	struct aic_usb_gadget *gg = a_ep->parent;
	struct aic_usb_req *a_req;
	int dir_in = a_ep->dir_in;
	u32 epmsk_reg = dir_in ? INEPINTMSK : OUTEPINTMSK;
	u32 mask;

	if (!list_empty(&a_ep->queue)) {
		a_req = get_ep_head(a_ep);
		aic_ep_start_req(gg, a_ep, a_req, false);
		return;
	}
	if (!a_ep->isochronous)
		return;

	if (dir_in) {
		dev_dbg(gg->dev, "%s: No more ISOC-IN requests\n",
			__func__);
	} else {
		dev_dbg(gg->dev, "%s: No more ISOC-OUT requests\n",
			__func__);
		mask = aic_readl(gg, epmsk_reg);
		mask |= OUTEPINTMSK_OUTTKNEPDISMSK;
		aic_writel(gg, mask, epmsk_reg);
	}
}

static void aic_ep_complete_request(struct aic_usb_gadget *gg,
				    struct aic_usb_ep *a_ep,
				    struct aic_usb_req *a_req,
				    int result)
{
	if (!a_req) {
		dev_dbg(gg->dev, "%s: nothing to complete?\n", __func__);
		return;
	}

	dev_dbg(gg->dev, "complete: ep %p %s, req %p, %d => %p\n",
		a_ep, a_ep->ep.name, a_req, result, a_req->req.complete);

	if (a_req->req.status == -EINPROGRESS)
		a_req->req.status = result;

	/* unmap dma */
	usb_gadget_unmap_request(&gg->gadget, &a_req->req, a_ep->dir_in);

	/* unalign clear */
	aic_handle_unaligned_req_complete(gg, a_ep, a_req);

	/* dequeue */
	a_ep->req = NULL;
	list_del_init(&a_req->queue);

	/* call complete function */
	if (a_req->req.complete) {
		spin_unlock(&gg->lock);
		usb_gadget_giveback_request(&a_ep->ep, &a_req->req);
		spin_lock(&gg->lock);
	}

	/* restart request */
	if (!a_ep->req && result >= 0)
		aic_ep_start_next_request(a_ep);
}

static void aic_stall_ep0(struct aic_usb_gadget *gg)
{
	struct aic_usb_ep *ep0 = gg->eps_out[0];
	u32 reg;
	u32 ctrl;

	dev_dbg(gg->dev, "ep0 stall (dir=%d)\n", ep0->dir_in);
	reg = (ep0->dir_in) ? INEPCFG0 : OUTEPCFG0;

	/*
	 * DxEPCTL_Stall will be cleared by EP once it has
	 * taken effect, so no need to clear later.
	 */

	ctrl = aic_readl(gg, reg);
	ctrl |= EPCTL_STALL;
	ctrl |= EPCTL_CNAK;
	aic_writel(gg, ctrl, reg);

	dev_dbg(gg->dev,
		"written EPCTL=0x%08x to %08x (EPCTL=0x%08x)\n",
		ctrl, reg, aic_readl(gg, reg));

	 /* ep0 request */
	 aic_ep0_enqueue_setup(gg);
}

static int aic_ep_sethalt_nolock(struct usb_ep *ep, int value, bool now)
{
	struct aic_usb_ep *a_ep = our_ep(ep);
	struct aic_usb_gadget *gg = a_ep->parent;
	int index = a_ep->index;
	u32 epreg;
	u32 epctl;
	u32 xfertype;

	dev_info(gg->dev, "%s(ep %p %s, %d)\n", __func__, ep, ep->name, value);

	if (index == 0) {
		if (value)
			aic_stall_ep0(gg);
		else
			dev_warn(gg->dev,
				 "%s: can't clear halt on ep0\n", __func__);
		return 0;
	}

	if (a_ep->isochronous) {
		dev_err(gg->dev, "%s is Isochronous Endpoint\n", ep->name);
		return -EINVAL;
	}

	if (!now && value && !list_empty(&a_ep->queue)) {
		dev_dbg(gg->dev, "%s request is pending, cannot halt\n",
			ep->name);
		return -EAGAIN;
	}

	if (a_ep->dir_in) {
		epreg = INEPCFG(index);
		epctl = aic_readl(gg, epreg);

		if (value) {
			epctl |= EPCTL_STALL | EPCTL_SNAK;
			if (epctl & EPCTL_EPENA)
				epctl |= EPCTL_EPDIS;
		} else {
			epctl &= ~EPCTL_STALL;
			xfertype = epctl & EPCTL_EPTYPE_MASK;
			if (xfertype == EPCTL_EPTYPE_BULK ||
			    xfertype == EPCTL_EPTYPE_INTERRUPT)
				epctl |= EPCTL_SETD0PID;
		}
		aic_writel(gg, epctl, epreg);
	} else {
		epreg = OUTEPCFG(index);
		epctl = aic_readl(gg, epreg);

		if (value) {
			epctl |= EPCTL_STALL;
		} else {
			epctl &= ~EPCTL_STALL;
			xfertype = epctl & EPCTL_EPTYPE_MASK;
			if (xfertype == EPCTL_EPTYPE_BULK ||
			    xfertype == EPCTL_EPTYPE_INTERRUPT)
				epctl |= EPCTL_SETD0PID;
		}
		aic_writel(gg, epctl, epreg);
	}

	a_ep->halted = value;

	return 0;
}

static int aic_ep_sethalt(struct usb_ep *ep, int value)
{
	struct aic_usb_ep *a_ep = our_ep(ep);
	struct aic_usb_gadget *gg = a_ep->parent;
	unsigned long flags = 0;
	int ret = 0;

	/* Sometime ADBD set ep0 halt for unknown reason, and no release halt,
	   mask this operation temporary.
	 */
	return 0;

	spin_lock_irqsave(&gg->lock, flags);
	ret = aic_ep_sethalt_nolock(ep, value, false);
	spin_unlock_irqrestore(&gg->lock, flags);

	return ret;
}

static int aic_ep_dequeue_request(struct usb_ep *ep, struct usb_request *req)
{
	struct aic_usb_req *a_req = our_req(req);
	struct aic_usb_ep *a_ep = our_ep(ep);
	struct aic_usb_gadget *gg = a_ep->parent;
	unsigned long flags;

	dev_dbg(gg->dev, "ep_dequeue(%p,%p)\n", ep, req);

	spin_lock_irqsave(&gg->lock, flags);

	if (!on_list(a_ep, a_req)) {
		spin_unlock_irqrestore(&gg->lock, flags);
		return -EINVAL;
	}

	/* Dequeue already started request */
	if (req == &a_ep->req->req)
		aic_ep_stop_xfer(gg, a_ep);

	aic_ep_complete_request(gg, a_ep, a_req, -ECONNRESET);
	spin_unlock_irqrestore(&gg->lock, flags);

	return 0;
}

static int aic_ep_queue_request_nolock(struct usb_ep *ep,
				       struct usb_request *req,
				       gfp_t gfp_flags)
{
	struct aic_usb_req *a_req = our_req(req);
	struct aic_usb_ep *a_ep = our_ep(ep);
	struct aic_usb_gadget *gg = a_ep->parent;
	bool first;
	int ret = 0;

	/* Don't queue ISOC request if length greater than mps*mc */
	if (a_ep->isochronous &&
	    req->length > (a_ep->mc * a_ep->ep.maxpacket)) {
		dev_err(gg->dev, "req length > maxpacket*mc\n");
		return -EINVAL;
	}

	dev_dbg(gg->dev, "%s: req %p: %d@%p, noi=%d, zero=%d, snok=%d\n",
		ep->name, req, req->length, req->buf, req->no_interrupt,
		req->zero, req->short_not_ok);

	/* init request */
	INIT_LIST_HEAD(&a_req->queue);
	req->actual = 0;
	req->status = -EINPROGRESS;

	/* unalign address */
	ret = aic_handle_unaligned_req(gg, a_ep, a_req);
	if (ret)
		return ret;

	/* map dma */
	ret = usb_gadget_map_request(&gg->gadget, req, a_ep->dir_in);
	if (ret) {
		dev_err(gg->dev, "%s: failed to map buffer %p, %d bytes\n",
			__func__, req->buf, req->length);
		return ret;
	}

	/* enqueue */
	first = list_empty(&a_ep->queue);
	list_add_tail(&a_req->queue, &a_ep->queue);

	/* Change EP0 direction if status phase request is after data out */
	if (!a_ep->index && !req->length && !a_ep->dir_in &&
	    gg->ep0_state == AIC_EP0_DATA_OUT)
		a_ep->dir_in = 1;

	/* start transfer */
	if (first) {
		if (!a_ep->isochronous) {
			aic_ep_start_req(gg, a_ep, a_req, false);
			return 0;
		}

		/* Update current frame number value. */
		gg->frame_number = aic_read_frameno(gg);
		while (aic_target_frame_elapsed(a_ep)) {
			aic_incr_frame_num(a_ep);
			/* Update current frame number value once more as it
			 * changes here.
			 */
			gg->frame_number = aic_read_frameno(gg);
		}

		if (a_ep->target_frame != TARGET_FRAME_INITIAL)
			aic_ep_start_req(gg, a_ep, a_req, false);
	}
	return 0;
}

static int aic_ep_queue_request(struct usb_ep *ep, struct usb_request *req,
				gfp_t gfp_flags)
{
	struct aic_usb_ep *a_ep = our_ep(ep);
	struct aic_usb_gadget *gg = a_ep->parent;
	unsigned long flags = 0;
	int ret = 0;

	spin_lock_irqsave(&gg->lock, flags);
	ret = aic_ep_queue_request_nolock(ep, req, gfp_flags);
	spin_unlock_irqrestore(&gg->lock, flags);

	return ret;
}

static int aic_ep_disable_nolock(struct usb_ep *ep)
{
	struct aic_usb_ep *a_ep = our_ep(ep);
	struct aic_usb_gadget *gg = a_ep->parent;
	unsigned int index = a_ep->index;
	u32 dir_in = a_ep->dir_in;
	u32 ctrl_addr;
	u32 ctrl;

	dev_dbg(gg->dev, "%s(ep %p)\n", __func__, ep);

	/* ep0 don't use by app driver */
	if (ep == &gg->eps_out[0]->ep) {
		dev_err(gg->dev, "%s: called for ep0\n", __func__);
		return -EINVAL;
	}

	ctrl_addr = dir_in ? INEPCFG(index) : OUTEPCFG(index);
	ctrl = aic_readl(gg, ctrl_addr);

	if (ctrl & EPCTL_EPENA)
		aic_ep_stop_xfer(gg, a_ep);

	ctrl &= ~EPCTL_EPENA;
	ctrl &= ~EPCTL_USBACTEP;
	ctrl |= EPCTL_SNAK;
	aic_writel(gg, ctrl, ctrl_addr);
	dev_dbg(gg->dev, "%s: DxEPCTL=0x%08x\n", __func__, ctrl);

	/* disable endpoint interrupts */
	aic_ctrl_epint(gg, a_ep->index, a_ep->dir_in, 0);

	/* terminate all requests with shutdown */
	aic_kill_ep_reqs(gg, a_ep, -ESHUTDOWN);

	/* free fifo */
	gg->fifo_map &= ~(1 << a_ep->fifo_index);
	a_ep->fifo_index = 0;
	a_ep->fifo_size = 0;

	return 0;
}

static int aic_ep_disable(struct usb_ep *ep)
{
	struct aic_usb_ep *a_ep = our_ep(ep);
	struct aic_usb_gadget *gg = a_ep->parent;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&gg->lock, flags);
	ret = aic_ep_disable_nolock(ep);
	spin_unlock_irqrestore(&gg->lock, flags);
	return ret;
}

static int aic_ep_enable(struct usb_ep *ep,
			 const struct usb_endpoint_descriptor *desc)
{
	struct aic_usb_ep *a_ep = our_ep(ep);
	struct aic_usb_gadget *gg = a_ep->parent;
	unsigned int index = a_ep->index;
	unsigned long flags;
	u32 dir_in;
	u32 mps;
	u32 mc;
	u32 ep_type;
	u32 ctrl_addr;
	u32 ctrl;
	u32 mask;
	int i;
	int ret = 0;

	dev_dbg(gg->dev,
		"%s: ep %s: a 0x%02x, attr 0x%02x, mps 0x%04x, intr %d\n",
		__func__, ep->name, desc->bEndpointAddress, desc->bmAttributes,
		desc->wMaxPacketSize, desc->bInterval);

	/* ep0 don't use by app driver */
	if (index == 0) {
		dev_err(gg->dev, "%s: called for EP0\n", __func__);
		return -EINVAL;
	}

	dir_in = (desc->bEndpointAddress & USB_ENDPOINT_DIR_MASK) ? 1 : 0;
	if (dir_in != a_ep->dir_in) {
		dev_err(gg->dev, "%s: direction mismatch!\n", __func__);
		return -EINVAL;
	}

	spin_lock_irqsave(&gg->lock, flags);

	/* (0) read ep ctrl */
	ctrl_addr = dir_in ? INEPCFG(index) : OUTEPCFG(index);
	ctrl = aic_readl(gg, ctrl_addr);
	dev_dbg(gg->dev, "%s: read DxEPCTL=0x%08x from 0x%08x\n",
		__func__, ctrl, ctrl_addr);

	/* (1) max packet */
	mps = usb_endpoint_maxp(desc);
	mc = usb_endpoint_maxp_mult(desc);

	aic_set_ep_maxpacket(gg, a_ep->index, mps, mc, dir_in);

	ctrl &= ~(EPCTL_EPTYPE_MASK | EPCTL_MPS_MASK);
	ctrl |= EPCTL_MPS(mps);
	ctrl |= EPCTL_USBACTEP;

	/* (2) endpoint type */
	ep_type = desc->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK;
	a_ep->isochronous = 0;
	a_ep->periodic = 0;
	a_ep->halted = 0;
	a_ep->interval = desc->bInterval;
	switch (ep_type) {
	case USB_ENDPOINT_XFER_ISOC:
		ctrl |= EPCTL_EPTYPE_ISO;
		ctrl |= EPCTL_SETEVENFR;
		a_ep->isochronous = 1;
		a_ep->interval = 1 << (desc->bInterval - 1);
		a_ep->target_frame = TARGET_FRAME_INITIAL;
		if (dir_in) {
			a_ep->periodic = 1;
			mask = aic_readl(gg, INEPINTMSK);
			mask |= INEPINTMSK_NAKMSK;
			aic_writel(gg, mask, INEPINTMSK);
		} else {
			mask = aic_readl(gg, OUTEPINTMSK);
			mask |= OUTEPINTMSK_OUTTKNEPDISMSK;
			aic_writel(gg, mask, OUTEPINTMSK);
		}
		break;

	case USB_ENDPOINT_XFER_BULK:
		ctrl |= EPCTL_EPTYPE_BULK;
		break;

	case USB_ENDPOINT_XFER_INT:
		if (dir_in)
			a_ep->periodic = 1;

		if (gg->gadget.speed == USB_SPEED_HIGH)
			a_ep->interval = 1 << (desc->bInterval - 1);

		ctrl |= EPCTL_EPTYPE_INTERRUPT;
		break;

	case USB_ENDPOINT_XFER_CONTROL:
		ctrl |= EPCTL_EPTYPE_CONTROL;
		break;
	}

	/* (3) period IN ep alloc fifo */
	if (dir_in && ((ep_type == USB_ENDPOINT_XFER_INT) ||
		       (ep_type == USB_ENDPOINT_XFER_ISOC))) {
		u32 fifo_index = 0;
		u32 fifo_size = UINT_MAX;
		u32 val = 0;
		u32 size = 0;

		size = a_ep->ep.maxpacket * a_ep->mc;
		for (i = 1; i <= gg->params.num_perio_in_ep; i++) {
			if (gg->fifo_map & (1 << i))
				continue;
			val = aic_readl(gg, TXFIFOSIZ(i));
			val = (val >> FIFOSIZE_DEPTH_SHIFT) * 4;
			if (val < size)
				continue;
			/* Search for smallest acceptable fifo */
			if (val < fifo_size) {
				fifo_size = val;
				fifo_index = i;
			}
		}
		if (!fifo_index) {
			dev_err(gg->dev,
				"%s: No suitable fifo found\n", __func__);
			ret = -ENOMEM;
			goto out;
		}
		ctrl &= ~(EPCTL_TXFNUM_LIMIT << EPCTL_TXFNUM_SHIFT);
		ctrl |= EPCTL_TXFNUM(fifo_index);
		gg->fifo_map |= 1 << fifo_index;
		a_ep->fifo_index = fifo_index;
		a_ep->fifo_size = fifo_size;
	}

	/* (4) for non iso endpoints, set PID to D0 */
	if (index && !a_ep->isochronous)
		ctrl |= EPCTL_SETD0PID;

	/* (5) clear NAK */
	if (gg->gadget.speed == USB_SPEED_FULL &&
	    a_ep->isochronous && dir_in) {
		ctrl |= EPCTL_CNAK;
	}

	/* (6) write-back ep ctrl */
	dev_dbg(gg->dev, "%s: write DxEPCTL=0x%08x\n",
		__func__, ctrl);
	aic_writel(gg, ctrl, ctrl_addr);
	dev_dbg(gg->dev, "%s: read DxEPCTL=0x%08x\n",
		__func__, aic_readl(gg, ctrl_addr));

	/* (7) enable the endpoint interrupt */
	aic_ctrl_epint(gg, index, dir_in, 1);

out:
	spin_unlock_irqrestore(&gg->lock, flags);

	return ret;
}

static struct usb_request *aic_ep_alloc_request(struct usb_ep *ep,
						gfp_t flags)
{
	struct aic_usb_req *req;

	req = kzalloc(sizeof(*req), flags);
	if (!req)
		return NULL;

	INIT_LIST_HEAD(&req->queue);

	return &req->req;
}

static void aic_ep_free_request(struct usb_ep *ep,
				struct usb_request *req)
{
	struct aic_usb_req *aic_req = our_req(req);

	kfree(aic_req);
}

static const struct usb_ep_ops aic_usb_ep_ops = {
	.enable			= aic_ep_enable,
	.disable		= aic_ep_disable,
	.alloc_request		= aic_ep_alloc_request,
	.free_request		= aic_ep_free_request,
	.queue			= aic_ep_queue_request,
	.dequeue		= aic_ep_dequeue_request,
	.set_halt		= aic_ep_sethalt,
};

static int aic_gg_getframe(struct usb_gadget *gadget)
{
	return aic_read_frameno(our_gadget(gadget));
}

static int aic_gg_vbus_draw(struct usb_gadget *gadget, unsigned int mA)
{
	struct aic_usb_gadget *gg = our_gadget(gadget);

	if (IS_ERR_OR_NULL(gg->uphy))
		return -ENOTSUPP;
	return usb_phy_set_power(gg->uphy, mA);
}

static int aic_gg_vbus_session(struct usb_gadget *gadget, int is_active)
{
	struct aic_usb_gadget *gg = our_gadget(gadget);
	unsigned long flags = 0;
	int ret = 0;

	if (!gg) {
		pr_err("%s: called with no device\n", __func__);
		return -ENODEV;
	}

	dev_dbg(gg->dev, "%s: is_active: %d\n", __func__, is_active);

	spin_lock_irqsave(&gg->lock, flags);

	if (is_active) {
		ret = aic_core_init(gg, false);
		if (ret) {
			dev_err(gg->dev, "%s: aic_core_init %d\n", __func__, ret);
			goto err;
		}
		if (gg->enabled)
			aic_soft_connect(gg);
	} else {
		aic_soft_disconnect(gg);
		aic_core_disconnect(gg);
	}

	spin_unlock_irqrestore(&gg->lock, flags);
	return 0;
err:
	spin_unlock_irqrestore(&gg->lock, flags);
	return ret;
}

static int aic_gg_pullup(struct usb_gadget *gadget, int is_on)
{
	struct aic_usb_gadget *gg = our_gadget(gadget);
	unsigned long flags = 0;
	int ret = 0;

	if (!gg) {
		pr_err("%s: called with no device\n", __func__);
		return -ENODEV;
	}

	dev_dbg(gg->dev, "%s: is_on: %d\n", __func__, is_on);

	spin_lock_irqsave(&gg->lock, flags);

	if (is_on) {
		gg->enabled = 1;
		ret = aic_core_init(gg, false);
		if (ret) {
			dev_err(gg->dev, "%s: aic_core_init %d\n", __func__, ret);
			goto err;
		}
		aic_soft_connect(gg);
	} else {
		aic_soft_disconnect(gg);
		aic_core_disconnect(gg);
		gg->enabled = 0;
	}
	gg->gadget.speed = USB_SPEED_UNKNOWN;

	spin_unlock_irqrestore(&gg->lock, flags);
	return 0;
err:
	spin_unlock_irqrestore(&gg->lock, flags);
	return ret;
}

static int aic_gg_udc_stop(struct usb_gadget *gadget)
{
	struct aic_usb_gadget *gg = our_gadget(gadget);
	unsigned long flags = 0;
	int i = 0;
	int ret = 0;

	if (!gg) {
		pr_err("%s: called with no device\n", __func__);
		return -ENODEV;
	}

	/* all endpoints should be shutdown */
	for (i = 1; i < gg->params.num_ep; i++) {
		if (gg->eps_in[i])
			aic_ep_disable(&gg->eps_in[i]->ep);
		if (gg->eps_out[i])
			aic_ep_disable(&gg->eps_out[i]->ep);
	}

	spin_lock_irqsave(&gg->lock, flags);

	gg->driver = NULL;
	gg->gadget.speed = USB_SPEED_UNKNOWN;
	gg->enabled = 0;

	spin_unlock_irqrestore(&gg->lock, flags);

	ret = aic_low_hw_disable(gg);
	if (ret)  {
		dev_err(gg->dev, "%s: aic_low_hw_disable %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static int aic_gg_udc_start(struct usb_gadget *gadget,
			    struct usb_gadget_driver *driver)
{
	struct aic_usb_gadget *gg = our_gadget(gadget);
	unsigned long flags = 0;
	int ret = 0;

	if (!gg) {
		pr_err("%s: called with no device\n", __func__);
		return -ENODEV;
	}

	if (!driver) {
		dev_err(gg->dev, "%s: no driver\n", __func__);
		return -EINVAL;
	}

	if (driver->max_speed < USB_SPEED_FULL)
		dev_err(gg->dev, "%s: bad speed\n", __func__);

	if (!driver->setup) {
		dev_err(gg->dev, "%s: missing entry points\n", __func__);
		return -EINVAL;
	}

	WARN_ON(gg->driver);
	driver->driver.bus = NULL;
	gg->driver = driver;
	gg->gadget.dev.of_node = gg->dev->of_node;
	gg->gadget.speed = USB_SPEED_UNKNOWN;

	spin_lock_irqsave(&gg->lock, flags);

	ret = aic_core_init(gg, false);
	if (ret) {
		dev_err(gg->dev, "%s: aic_core_init %d\n", __func__, ret);
		goto err;
	}
	gg->enabled = 0;

	spin_unlock_irqrestore(&gg->lock, flags);

	dev_info(gg->dev, " bound driver %s\n", driver->driver.name);

	return 0;

err:
	gg->driver = NULL;
	return ret;
}

static const struct usb_gadget_ops aic_usb_gadget_ops = {
	.get_frame		= aic_gg_getframe,
	.udc_start		= aic_gg_udc_start,
	.udc_stop		= aic_gg_udc_stop,
	.pullup			= aic_gg_pullup,
	.vbus_session		= aic_gg_vbus_session,
	.vbus_draw		= aic_gg_vbus_draw,
};

static void aic_init_ep(struct aic_usb_gadget *gg,
			struct aic_usb_ep *ep,
			int epnum,
			bool dir_in)
{
	char *dir;
	u32 next = 0;

	ep->dir_in = dir_in;
	ep->index = epnum;
	ep->parent = gg;

	if (epnum == 0)
		dir = "";
	else if (dir_in)
		dir = "in";
	else
		dir = "out";
	snprintf(ep->name, sizeof(ep->name), "ep%d%s", epnum, dir);
	ep->ep.name = ep->name;

	INIT_LIST_HEAD(&ep->queue);
	INIT_LIST_HEAD(&ep->ep.ep_list);
	/* add ep1 ~ epN to the list of endpoints known by the gadget driver */
	if (epnum)
		list_add_tail(&ep->ep.ep_list, &gg->gadget.ep_list);

	if (gg->params.speed == AIC_SPEED_PARAM_LOW)
		usb_ep_set_maxpacket_limit(&ep->ep, 8);
	else
		usb_ep_set_maxpacket_limit(&ep->ep,
					   epnum ? 1024 : EP0_MPS_LIMIT);
	ep->ep.ops = &aic_usb_ep_ops;

	if (epnum == 0) {
		ep->ep.caps.type_control = true;
	} else {
		if (gg->params.speed != AIC_SPEED_PARAM_LOW) {
			ep->ep.caps.type_iso = true;
			ep->ep.caps.type_bulk = true;
		}
		ep->ep.caps.type_int = true;
	}

	if (dir_in)
		ep->ep.caps.dir_in = true;
	else
		ep->ep.caps.dir_out = true;

	/* set the next-endpoint pointer */
	next = EPCTL_NEXTEP((epnum + 1) % 15);
	if (dir_in)
		aic_writel(gg, next, INEPCFG(epnum));
	else
		aic_writel(gg, next, OUTEPCFG(epnum));
}

static int aic_gadget_core_init(struct aic_usb_gadget *gg)
{
	struct device *dev = gg->dev;
	u32 cfg = gg->params.ep_dirs;
	u32 ep_type = 0;
	int i = 0;

	spin_lock_init(&gg->lock);

	/* alloc ep0 */
	gg->eps_in[0] = devm_kzalloc(gg->dev,
				     sizeof(struct aic_usb_ep),
				     GFP_KERNEL);
	if (!gg->eps_in[0])
		return -ENOMEM;
	gg->eps_out[0] = gg->eps_in[0];

	/* alloc ep0 buffer */
	gg->ctrl_buff = devm_kzalloc(gg->dev,
				     ALIGN(CTRL_BUFF_SIZE, L1_CACHE_BYTES),
				     GFP_KERNEL);
	if (!gg->ctrl_buff)
		return -ENOMEM;
	gg->ep0_buff = devm_kzalloc(gg->dev,
				    ALIGN(CTRL_BUFF_SIZE, L1_CACHE_BYTES),
				    GFP_KERNEL);
	if (!gg->ep0_buff)
		return -ENOMEM;

	/* alloc ep0 request */
	gg->ctrl_req = aic_ep_alloc_request(&gg->eps_out[0]->ep,
					    GFP_KERNEL);
	if (!gg->ctrl_req) {
		dev_err(dev, "failed to allocate ctrl req\n");
		return -ENOMEM;
	}

	/* alloc other ep */
	for (i = 1, cfg >>= 2; i < gg->params.num_ep; i++, cfg >>= 2) {
		ep_type = cfg & 3;
		/* Direction in or both */
		if (!(ep_type & 2)) {
			gg->eps_in[i] = devm_kzalloc(gg->dev,
						     sizeof(struct aic_usb_ep),
						     GFP_KERNEL);
			if (!gg->eps_in[i])
				return -ENOMEM;
		}
		/* Direction out or both */
		if (!(ep_type & 1)) {
			gg->eps_out[i] = devm_kzalloc(gg->dev,
						     sizeof(struct aic_usb_ep),
						     GFP_KERNEL);
			if (!gg->eps_out[i])
				return -ENOMEM;
		}
	}

	/* init eps */
	INIT_LIST_HEAD(&gg->gadget.ep_list);
	for (i = 0; i < gg->params.num_ep; i++) {
		if (gg->eps_in[i])
			aic_init_ep(gg, gg->eps_in[i], i, 1);
		if (gg->eps_out[i])
			aic_init_ep(gg, gg->eps_out[i], i, 0);
	}

	/* gadget member */
	gg->gadget.max_speed = USB_SPEED_HIGH;
	gg->gadget.ops = &aic_usb_gadget_ops;
	gg->gadget.name = dev_name(dev);
	gg->gadget.ep0 = &gg->eps_out[0]->ep;

	return 0;
}

static int aic_param_init(struct aic_usb_gadget *gg)
{
	gg->params.num_ep = EPS_NUM;
	gg->params.num_perio_in_ep = PERIOD_IN_EP_NUM;
	gg->params.total_fifo_size = TOTAL_FIFO_SIZE;

	gg->params.rx_fifo_size = RX_FIFO_SIZE;
	gg->params.np_tx_fifo_size = NP_TX_FIFO_SIZE;
	gg->params.p_tx_fifo_size[1] = PERIOD_TX_FIFO1_SIZE;
	gg->params.p_tx_fifo_size[2] = PERIOD_TX_FIFO2_SIZE;

	gg->params.ep_dirs = EP_DIRS;
#ifdef CONFIG_DEBUG_ON_FPGA_BOARD_ARTINCHIP
	gg->params.phy_type = AIC_PHY_TYPE_PARAM_ULPI;
#else
	gg->params.phy_type = AIC_PHY_TYPE_PARAM_UTMI;
#endif
	gg->params.phy_ulpi_ddr = 0;
	gg->params.speed = AIC_SPEED_PARAM_HIGH;

	return 0;
}

static int aic_gadget_init(struct aic_usb_gadget *gg)
{
	int ret = 0;

	ret = aic_param_init(gg);
	if (ret) {
		dev_err(gg->dev, "call aic_param_init fail: %d\n", ret);
		return ret;
	}

	ret = aic_low_hw_enable(gg);
	if (ret) {
		dev_err(gg->dev, "call aic_low_hw_enable fail: %d\n", ret);
		return ret;
	}

	ret = aic_core_rst(gg);
	if (ret) {
		dev_err(gg->dev, "call aic_core_rst fail: %d\n", ret);
		return ret;
	}

	ret = aic_gadget_core_init(gg);
	if (ret) {
		dev_err(gg->dev, "call aic_gadget_core_init fail: %d\n", ret);
		return ret;
	}

	return ret;
}

static int aic_udc_remove(struct platform_device *dev)
{
	struct aic_usb_gadget *gg = platform_get_drvdata(dev);

	aic_udc_debugfs_exit(gg);

	usb_del_gadget_udc(&gg->gadget);
	aic_ep_free_request(&gg->eps_out[0]->ep, gg->ctrl_req);

	aic_low_hw_disable(gg);

	reset_control_assert(gg->reset);
	reset_control_assert(gg->reset_ecc);

	return 0;
}

static void aic_udc_shutdown(struct platform_device *dev)
{
	struct aic_usb_gadget *gg = platform_get_drvdata(dev);

	disable_irq(gg->irq);
}

static int aic_udc_probe(struct platform_device *dev)
{
	struct aic_usb_gadget *gg = NULL;
	struct resource *res = NULL;
	int i, err;
	int ret = 0;

	if (of_property_read_bool(dev->dev.of_node, "aic,only-uboot-use")) {
		dev_info(&dev->dev, "aic-udc only work in uboot.\n");
		return -EPERM;
	}

	gg = devm_kzalloc(&dev->dev, sizeof(*gg), GFP_KERNEL);
	if (!gg)
		return -ENOMEM;

	gg->dev = &dev->dev;

	if (!dev->dev.dma_mask)
		dev->dev.dma_mask = &dev->dev.coherent_dma_mask;
	ret = dma_set_coherent_mask(&dev->dev, DMA_BIT_MASK(32));
	if (ret) {
		dev_err(&dev->dev, "can't set coherent DMA mask: %d\n", ret);
		return ret;
	}

	/* register */
	res = platform_get_resource(dev, IORESOURCE_MEM, 0);
	gg->regs = devm_ioremap_resource(&dev->dev, res);
	if (IS_ERR(gg->regs)) {
		dev_err(&dev->dev, "ioremap reg fail!\n");
		return PTR_ERR(gg->regs);
	}
	dev_dbg(&dev->dev, "mapped physical addr %08lx to virtual addr %p\n",
		(unsigned long)res->start, gg->regs);

	/* reset */
	gg->reset = devm_reset_control_get_optional(gg->dev, "aicudc");
	if (IS_ERR(gg->reset)) {
		ret = PTR_ERR(gg->reset);
		dev_err(gg->dev, "error getting reset control %d\n", ret);
		return ret;
	}
	gg->reset_ecc = devm_reset_control_get_optional_shared(gg->dev,
							       "aicudc-ecc");
	if (IS_ERR(gg->reset_ecc)) {
		ret = PTR_ERR(gg->reset_ecc);
		dev_err(gg->dev, "error getting reset control for ecc %d\n",
			ret);
		return ret;
	}

	/* regulator */

	/* clock */
	for (i = 0; i < USB_MAX_CLKS_RSTS; i++) {
		gg->clks[i] = of_clk_get(gg->dev->of_node, i);
		if (IS_ERR(gg->clks[i])) {
			dev_err(gg->dev, "cannot get clock %d\n", i);
			return PTR_ERR(gg->clks[i]);

			err = PTR_ERR(gg->clks[i]);
			if (err == -EPROBE_DEFER)
				return err;
			gg->clks[i] = NULL;
			break;
		}
	}

	/* phy */
	gg->phy = devm_phy_get(gg->dev, "usb2-phy");
	if (IS_ERR(gg->phy)) {
		ret = PTR_ERR(gg->phy);
		switch (ret) {
		case -ENODEV:
		case -ENOSYS:
			gg->phy = NULL;
			break;
		case -EPROBE_DEFER:
			return ret;
		default:
			dev_err(gg->dev, "error getting phy %d\n", ret);
			return ret;
		}
	}
	if (!gg->phy) {
		gg->uphy = devm_usb_get_phy(gg->dev, USB_PHY_TYPE_USB2);
		if (IS_ERR(gg->uphy)) {
			ret = PTR_ERR(gg->uphy);
			switch (ret) {
			case -ENODEV:
			case -ENXIO:
				gg->uphy = NULL;
				break;
			case -EPROBE_DEFER:
				return ret;
			default:
				dev_err(gg->dev, "error getting usb phy %d\n",
					ret);
				return ret;
			}
		}
	}

	/* gadget init */
	ret = aic_gadget_init(gg);
	if (ret) {
		dev_err(&dev->dev, "udc init fail: %d\n", ret);
		return ret;
	}

	/* interrupt */
	res = platform_get_resource(dev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(&dev->dev, "No IRQ resource found!\n");
		return -ENODEV;
	}
	gg->irq = res->start;
	ret = devm_request_irq(gg->dev, gg->irq,
			       aic_udc_irq, IRQF_SHARED,
			       dev_name(gg->dev), gg);
	if (ret) {
		dev_err(&dev->dev, "request irq%d fail: %d\n", gg->irq, ret);
		return ret;
	}
	dev_dbg(gg->dev, "registering interrupt handler for irq%d\n",
		gg->irq);

	/* udc add */
	ret = usb_add_gadget_udc(gg->dev, &gg->gadget);
	if (ret) {
		dev_err(&dev->dev, "udc add fail: %d\n", ret);
		return ret;
	}

	/* debugfs */
	aic_udc_debugfs_init(gg);

	platform_set_drvdata(dev, gg);

	return 0;
}

const struct of_device_id aic_udc_match_table[] = {
	{ .compatible = "artinchip,aic-udc-v1.0",},
	{},
};

static int __maybe_unused aic_udc_suspend(struct device *dev)
{
	struct aic_usb_gadget *gg = dev_get_drvdata(dev);

	aic_low_hw_disable(gg);
	return 0;
}

static int __maybe_unused aic_udc_resume(struct device *dev)
{
	struct aic_usb_gadget *gg = dev_get_drvdata(dev);

	aic_low_hw_enable(gg);
	return 0;
}

static const struct dev_pm_ops aic_udc_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(aic_udc_suspend, aic_udc_resume)
};

static struct platform_driver aic_udc_driver = {
	.driver = {
		.name = "aic_udc",
		.of_match_table = aic_udc_match_table,
		.pm = &aic_udc_pm_ops,
	},
	.probe = aic_udc_probe,
	.remove = aic_udc_remove,
	.shutdown = aic_udc_shutdown,
};

module_platform_driver(aic_udc_driver);
MODULE_DESCRIPTION("USB Device Controller driver for aic");

