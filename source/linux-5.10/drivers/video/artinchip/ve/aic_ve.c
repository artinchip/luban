// SPDX-License-Identifier: GPL-2.0-only
/*
 * Artinchip video engine driver, the driver for hw video codec embedded in Artinchip SOCs.
 *
 * Copyright (C) 2022 ArtInChip Technology Co., Ltd.
 * Authors:  Jun <lijun.li@artinchip.com>
 */

#include <linux/compat.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/debugfs.h>

#ifdef CONFIG_DMA_SHARED_BUFFER
#include <linux/dma-buf.h>
#endif

#include <video/artinchip_ve.h>

#define VE_TIMEOUT_MS(x)	msecs_to_jiffies(x)

#define VE_CLK_REG 0x00
#define VE_RST_REG 0x04
#define VE_INIT_REG 0x08
#define VE_IRQ_REG 0x0C
#define AVC_RESET_REG 0x100
#define VE_STATUS_REG (0x128)

#define JPG_REG_OFFSET_ADDR 0x2000
#define PNG_REG_OFFSET_ADDR 0xc00

#define JPG_BUSY_REG    (JPG_REG_OFFSET_ADDR + 0x224)
#define JPG_STATUS_REG  (JPG_REG_OFFSET_ADDR + 0x04)
#define PNG_BUSY_REG    (PNG_REG_OFFSET_ADDR + 0x54)
#define PNG_STATUS_REG  (PNG_REG_OFFSET_ADDR + 0x04)

#define VE_AVC_EN_REG 0x10
#define VE_JPG_EN_REG 0x14
#define VE_PNG_EN_REG 0x18

#define VE_AVC_TYPE 0x01
#define VE_JPG_TYPE 0x02
#define VE_PNG_TYPE 0x04

#define AVC_CLEAR_IRQ 0x70000
#define JPG_CLEAR_IRQ 0xf
#define PNG_CLEAR_IRQ 0xf

#define VE_ENABLE_IRQ 1
#define VE_DISABLE_IRQ 0

struct aic_ve_client {
	struct list_head list_client;
	pid_t pid;			// process id of this client.
	struct mutex lock;		// lock of dma_buf list
	struct list_head list_dma_buf;	// list of dma_buf band with current client
};

struct aic_ve_service {
	struct mutex lock;
	struct list_head client;
	bool is_running;		// ve is used by a client
	pid_t running_pid;		// process id of current running
	atomic_t power_on;
	wait_queue_head_t wait;		// irq wait queue
	wait_queue_head_t client_wait;	// get client wait queue
	bool irq_flag;
	unsigned int irq_type;
	unsigned int irq_status;
	int client_count;
};

struct aic_ve_ctx {
	struct aic_ve_service	ve_service;
	struct device		*dev;
	struct device		*child_dev;
	struct cdev		cdev;
	struct class		*class;
	dev_t			ve_dev;
	struct platform_device	*plat_dev;
	int			irq;
	spinlock_t		lock;
	struct reset_control	*reset;
	struct clk		*ve_clk;
	struct resource		*res;
	resource_size_t		reg_phy;
	int			reg_size;
	void __iomem		*regs_base;
	ulong			mclk_rate;
};

struct aic_dma_buf_info {
	struct list_head list;
	int fd;
	dma_addr_t addr;
	struct dma_buf *dma_buf;
	struct dma_buf_attachment *attachment;
	struct sg_table	*sgt;
	int pid;
	int ref_cnt;
};

static int enable_ve_hw_clk(struct aic_ve_ctx *ctx)
{
	int ret;

	ret = reset_control_deassert(ctx->reset);
	if (ret) {
		dev_err(ctx->dev, "reset ve control deassert failed!\n");
		return ret;
	}

	ret = clk_set_rate(ctx->ve_clk, ctx->mclk_rate);
	if (ret) {
		dev_err(ctx->dev, "Failed to set CLK_VE %ld\n", ctx->mclk_rate);
		return ret;
	}

	if (!__clk_is_enabled(ctx->ve_clk)) {
		ret = clk_prepare_enable(ctx->ve_clk);
		if (ret) {
			dev_err(ctx->dev, "enable ve clk gating failed!\n");
			return ret;
		}
	}

	return 0;
}

static int disable_ve_hw_clk(struct aic_ve_ctx *ctx)
{
	if (__clk_is_enabled(ctx->ve_clk))
		clk_disable_unprepare(ctx->ve_clk);
	reset_control_assert(ctx->reset);

	return 0;
}

static void ve_service_power_on(struct aic_ve_ctx *ctx, struct aic_ve_service *service)
{
	if (service->client_count != 0) {
		return ;
	}
	enable_ve_hw_clk(ctx);
	dev_dbg(ctx->dev, "power on\n");
	return;
}

static void ve_service_power_off(struct aic_ve_ctx *ctx, struct aic_ve_service *service)
{
	if (service->client_count != 0) {
		return ;
	}
	if (service->is_running) {
		dev_warn(ctx->dev, "power off while service is running!\n");
	}
	disable_ve_hw_clk(ctx);
	dev_dbg(ctx->dev, "power off\n");
	return;
}

#ifdef CONFIG_DMA_SHARED_BUFFER

static int add_dma_buf(struct aic_ve_ctx *ctx, struct aic_ve_client *client,
				int fd, unsigned int *addr)
{
	struct aic_dma_buf_info *dma_buf_info = NULL;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	struct dma_buf *buf;
	int ret;

	// if we add the same dma-buf again,
	// ref count add one and return the physic address
	if (!list_empty(&client->list_dma_buf)) {
		struct aic_dma_buf_info *pos = NULL, *n = NULL;

		list_for_each_entry_safe(pos, n, &client->list_dma_buf, list) {
			if ((pos->fd == fd) && (pos->pid == current->tgid)) {
				pos->ref_cnt++;
				*addr = pos->addr;
				return 0;
			}
		}
	}

	dma_buf_info = kmalloc(sizeof(*dma_buf_info), GFP_KERNEL);
	if (!dma_buf_info) {
		dev_err(ctx->dev, "kmalloc dma_buf_info failed!\n");
		return -ENOMEM;
	}

	memset(dma_buf_info, 0, sizeof(struct aic_dma_buf_info));

	buf = dma_buf_get(fd);

	if (IS_ERR(buf)) {
		dev_err(ctx->dev, "dma_buf_get(%d) failed\n", fd);
		ret = PTR_ERR(buf);
		goto end;
	}

	attach = dma_buf_attach(buf, ctx->dev);
	if (IS_ERR(attach)) {
		dev_err(ctx->dev, "dma_buf_attach(%d) failed\n", fd);
		dma_buf_put(buf);
		ret = PTR_ERR(attach);
		goto end;
	}

	sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		dev_err(ctx->dev, "dma_buf_map_attachment(%d) failed\n", fd);
		dma_buf_detach(buf, attach);
		dma_buf_put(buf);
		ret = PTR_ERR(sgt);
		goto end;
	}

	dma_buf_info->dma_buf = buf;
	dma_buf_info->attachment = attach;
	dma_buf_info->sgt = sgt;
	dma_buf_info->addr = sg_dma_address(sgt->sgl);
	dma_buf_info->fd = fd;
	dma_buf_info->pid = current->tgid;
	dma_buf_info->ref_cnt++;

	*addr = dma_buf_info->addr;

	INIT_LIST_HEAD(&dma_buf_info->list);

	mutex_lock(&client->lock);
	list_add_tail(&dma_buf_info->list, &client->list_dma_buf);
	mutex_unlock(&client->lock);

	return 0;

end:
	kfree(dma_buf_info);
	return -1;
}

/*
 * remove dma-buf from ve driver in 2 cases:
 * 1) ve_device_release (ie. process crash), unmap_all=1.
 *    remove all the dma-buf in this process;
 * 2) ref_cnt of dma-buf is 0
 */
static void remove_dma_buf(struct aic_ve_ctx *ctx, struct aic_ve_client *client,
					int fd, int unmap_all)
{
	struct aic_dma_buf_info *dma_buf_info = NULL;

	mutex_lock(&client->lock);

	if (!list_empty(&client->list_dma_buf)) {
		struct aic_dma_buf_info *pos = NULL, *n = NULL;

		list_for_each_entry_safe(pos, n, &client->list_dma_buf, list) {
			int find_fd = unmap_all || ((pos->fd == fd) && (pos->ref_cnt == 1));

			if ((pos->fd == fd) && (pos->pid == current->tgid))
				pos->ref_cnt--;
			if (find_fd && (pos->pid == current->tgid)) {
				dma_buf_info = pos;
				dma_buf_unmap_attachment(dma_buf_info->attachment,
							dma_buf_info->sgt, DMA_BIDIRECTIONAL);
				dma_buf_detach(dma_buf_info->dma_buf, dma_buf_info->attachment);
				dma_buf_put(dma_buf_info->dma_buf);

				dma_buf_info->dma_buf = NULL;
				dma_buf_info->attachment = NULL;
				dma_buf_info->sgt = NULL;
				dma_buf_info->addr = 0;
				dma_buf_info->fd = -1;

				list_del_init(&dma_buf_info->list);
				kfree(dma_buf_info);
			}
		}
	}

	mutex_unlock(&client->lock);
}

#endif

static irqreturn_t ve_irq_handler(int irq, void *priv)
{
	struct aic_ve_ctx *ctx = (struct aic_ve_ctx *)priv;
	struct aic_ve_service *service = &ctx->ve_service;
	unsigned int status;
	unsigned int enable;

	// video
	enable = readl(ctx->regs_base + VE_AVC_EN_REG);
	if (enable) {
		// read video status
		status = readl(ctx->regs_base + VE_STATUS_REG);
		service->irq_type = VE_AVC_TYPE;
		service->irq_status = status;
		// clear video interrupt
		status |= AVC_CLEAR_IRQ;
		writel(status, ctx->regs_base + VE_STATUS_REG);
	}
	// jpg
	enable = readl(ctx->regs_base + VE_JPG_EN_REG);
	if (enable) {
		// read jpg status
		status = readl(ctx->regs_base + JPG_STATUS_REG);
		service->irq_type = VE_JPG_TYPE;
		service->irq_status = status;
		// clear jpg interrupt
		status |= JPG_CLEAR_IRQ;
		writel(status, ctx->regs_base + JPG_STATUS_REG);
	}
	// png
	enable = readl(ctx->regs_base + VE_PNG_EN_REG);
	if (enable) {
		// read png status
		status = readl(ctx->regs_base + PNG_STATUS_REG);
		service->irq_type = VE_PNG_TYPE;
		service->irq_status = status;
		// clear png interrupt
		status |= PNG_CLEAR_IRQ;
		writel(status, ctx->regs_base + PNG_STATUS_REG);
	}
	writel(VE_DISABLE_IRQ, ctx->regs_base + VE_IRQ_REG);
	service->irq_flag = true;
	wake_up(&service->wait);

	return IRQ_HANDLED;
}

static int aic_ve_cdev_open(struct inode *inode, struct file *filp)
{
	struct aic_ve_ctx *ctx = container_of(inode->i_cdev, struct aic_ve_ctx, cdev);
	struct aic_ve_service *service = &ctx->ve_service;
	struct aic_ve_client *client = NULL;

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return -ENOMEM;

	client->pid = current->pid;
	INIT_LIST_HEAD(&client->list_client);
	INIT_LIST_HEAD(&client->list_dma_buf);
	mutex_init(&client->lock);
	filp->private_data = (void *)client;

	mutex_lock(&service->lock);
	ve_service_power_on(ctx, service);
	service->client_count++;
	list_add_tail(&client->list_client, &service->client);
	dev_dbg(ctx->dev, "ve client count %d pid %d open\n", service->client_count, client->pid);
	mutex_unlock(&service->lock);

	return nonseekable_open(inode, filp);
}

static void ve_check_idle(struct aic_ve_ctx *ctx)
{
	int i;

	// H264
	if (readl(ctx->regs_base + VE_AVC_EN_REG)) {
		for (i = 0; i < 100; i++) {
			if ((readl(ctx->regs_base + VE_STATUS_REG) & 0x1ff) == 0)
				break;
			usleep_range(1000, 2000);
		}
	}
	// jpg
	if (readl(ctx->regs_base + VE_JPG_EN_REG)) {
		for (i = 0; i < 100; i++) {
			if (readl(ctx->regs_base + JPG_BUSY_REG) == 0)
				break;
			usleep_range(1000, 2000);
		}
	}

	// png
	if (readl(ctx->regs_base + VE_PNG_EN_REG)) {
		for (i = 0; i < 100; i++) {
			if (readl(ctx->regs_base + PNG_BUSY_REG) == 0)
				break;
			usleep_range(1000, 2000);
		}
	}
}

static int aic_ve_cdev_release(struct inode *inode, struct file *filp)
{
	struct aic_ve_ctx *ctx = container_of(inode->i_cdev, struct aic_ve_ctx, cdev);
	struct aic_ve_service *service = &ctx->ve_service;
	struct aic_ve_client *client = (struct aic_ve_client *)filp->private_data;

	if (!client)
		return -EINVAL;

	dev_dbg(ctx->dev, "ve client count %d pid %d release\n", service->client_count, client->pid);

	mutex_lock(&service->lock);
	service->client_count--;
	ve_service_power_off(ctx, service);
	// we should release ve resource, if current client crash but still
	// occupy the resources
	if ((service->running_pid == client->pid) && service->is_running) {
		dev_warn(ctx->dev, "process crash, release ve resources!\n");
		ve_check_idle(ctx);

		service->is_running = false;
		service->running_pid = -1;
		wake_up(&service->client_wait);
	}
	list_del_init(&client->list_client);
	mutex_unlock(&service->lock);

#ifdef CONFIG_DMA_SHARED_BUFFER
	// remove all the dma-bufs if the process crash
	remove_dma_buf(ctx, client, 0, 1);
#endif

	list_del_init(&client->list_dma_buf);
	kfree(client);
	filp->private_data = NULL;

	return 0;
}

static long aic_ve_cdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct aic_ve_ctx *ctx = container_of(filp->f_path.dentry->d_inode->i_cdev,
					      struct aic_ve_ctx, cdev);
	struct aic_ve_service *service = &ctx->ve_service;
	struct aic_ve_client *client = (struct aic_ve_client *)filp->private_data;
	int ret;

	if (!client)
		return -EINVAL;

	switch (cmd) {
	case IOC_VE_WAIT: {
		struct wait_info info;

		if (copy_from_user(&info, (void __user *)arg, sizeof(struct wait_info)))
			return -EFAULT;

		ret = wait_event_timeout(service->wait, service->irq_flag,
			VE_TIMEOUT_MS(info.wait_time));
		service->irq_flag = false;

		if (ret <= 0) {
			dev_warn(ctx->dev, "client pid %d wait timeout!\n", client->pid);
			return -ETIMEDOUT;
		}

		info.reg_status = service->irq_status;

		if (copy_to_user((void __user *)arg, &info, sizeof(struct wait_info)))
			return -EFAULT;

		break;
	}
	case IOC_VE_GET_CLIENT: {
		int ret;

		dev_dbg(ctx->dev, "pid %d VE_CMD_GET_CLIENT\n", client->pid);

		ret = wait_event_timeout(service->client_wait, !service->is_running,
						 VE_TIMEOUT_MS(2000));
		if (ret <= 0) {
			dev_warn(ctx->dev, "get client pid %d wait timeout!\n",
				client->pid);
			return -ETIMEDOUT;
		}
		service->running_pid = client->pid;
		service->is_running = true;

		break;
	}
	case IOC_VE_PUT_CLIENT: {
		dev_dbg(ctx->dev, "pid %d VE_CMD_PUT_CLIENT\n", client->pid);

		service->is_running = false;
		service->running_pid = -1;
		wake_up(&service->client_wait);

		break;
	}
	case IOC_VE_SET_INFO: {

		break;
	}
	case IOC_VE_GET_INFO: {
		struct ve_info info;

		info.reg_size = ctx->reg_size;
		if (copy_to_user((char *)arg, &info, sizeof(struct ve_info)))
			return -EFAULT;

		break;
	}
	case IOC_VE_RESET: {
		dev_dbg(ctx->dev, "pid %d VE_CMD_RESET\n", client->pid);

		reset_control_assert(ctx->reset);
		reset_control_deassert(ctx->reset);

		break;
	}
#ifdef CONFIG_DMA_SHARED_BUFFER
	case IOC_VE_ADD_DMA_BUF: {
		struct dma_buf_info info;

		if (copy_from_user(&info, (void __user *)arg, sizeof(struct dma_buf_info)))
			return -EFAULT;

		if (add_dma_buf(ctx, client, info.fd, &info.phy_addr) != 0) {
			dev_err(ctx->dev, "add dma buf failed!\n");
			return -EINVAL;
		}

		if (copy_to_user((void __user *)arg, &info, sizeof(struct dma_buf_info)))
			return -EFAULT;

		break;
	}
	case IOC_VE_RM_DMA_BUF: {
		struct dma_buf_info info;

		if (copy_from_user(&info, (void __user *)arg, sizeof(struct dma_buf_info)))
			return -EFAULT;

		remove_dma_buf(ctx, client, info.fd, 0);

		break;
	}
#endif
	default: {
		dev_warn(ctx->dev, "warning: unknown ve cmd %x\n", cmd);
		return -ENOIOCTLCMD;
	}
	}

	return 0;
}

static void ve_cdev_vma_open(struct vm_area_struct *vma)
{
}

static void ve_cdev_vma_close(struct vm_area_struct *vma)
{
}

static const struct vm_operations_struct ve_cdev_remap_vm_ops = {
	.open  = ve_cdev_vma_open,
	.close = ve_cdev_vma_close,
};

static int aic_ve_cdev_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct aic_ve_ctx *ctx = container_of(filp->f_path.dentry->d_inode->i_cdev,
					struct aic_ve_ctx, cdev);
	unsigned long temp_pfn;
	resource_size_t ve_reg_base = ctx->reg_phy;

	if (vma->vm_end - vma->vm_start == 0)
		return 0;

	if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT))
		return -EINVAL;

	temp_pfn = ve_reg_base >> 12;

	/* Set reserved and I/O flag for the area. */
	vma->vm_flags |= /*VM_RESERVED | */VM_IO;
	/* Select uncached access. */
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	if (io_remap_pfn_range(vma, vma->vm_start, temp_pfn, vma->vm_end - vma->vm_start,
				vma->vm_page_prot))
		return -EAGAIN;

	vma->vm_ops = &ve_cdev_remap_vm_ops;
	ve_cdev_vma_open(vma);

	return 0;
}

static const struct file_operations aic_ve_cdev_fops = {
	.mmap = aic_ve_cdev_mmap,
	.open = aic_ve_cdev_open,
	.release = aic_ve_cdev_release,
	.llseek	 = no_llseek,
	.unlocked_ioctl = aic_ve_cdev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = aic_ve_cdev_ioctl,
#endif
};

static int aic_ve_parse_dt(struct device *dev, struct aic_ve_ctx *ctx)
{
	int ret;
	struct device_node *np = dev->of_node;

	ret = of_property_read_u32(np, "mclk", (u32 *)&ctx->mclk_rate);
	if (ret) {
		dev_err(dev, "Can't parse CLK_VE rate\n");
		return ret;
	}

	return 0;
}

static int aic_ve_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct aic_ve_ctx *ctx = NULL;
	struct aic_ve_service *service;
	struct device *dev = &pdev->dev;
	int dev_major;
	struct resource *res;
	void __iomem *regs_base;

	dev_dbg(&pdev->dev, "%s\n", __func__);

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	memset(ctx, 0, sizeof(struct aic_ve_ctx));
	service = &ctx->ve_service;
	ctx->dev = dev;

	mutex_init(&service->lock);
	INIT_LIST_HEAD(&service->client);

	service->is_running = false;
	service->client_count = 0;

	atomic_set(&service->power_on, 0);

	init_waitqueue_head(&service->client_wait);
	init_waitqueue_head(&service->wait);
	service->irq_flag = false;
	spin_lock_init(&ctx->lock);

	ctx->irq = platform_get_irq(pdev, 0);

	ret = devm_request_irq(dev, ctx->irq, ve_irq_handler, 0, dev_name(dev), ctx);

	if (ret < 0) {
		dev_err(dev, "try to request irq failed!\n");
		return ret;
	}

	dev_dbg(&pdev->dev, "get irq %d\n", ctx->irq);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	regs_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(regs_base))
		return PTR_ERR(regs_base);

	ctx->res = res;
	ctx->regs_base = regs_base;
	ctx->reg_phy = res->start;
	ctx->reg_size = resource_size(res);

	dev_dbg(&pdev->dev, "regs_base %p\n", ctx->regs_base);

	ctx->ve_clk = devm_clk_get(dev, "ve_clk");
	if (IS_ERR(ctx->ve_clk)) {
		dev_err(dev, "try to get ve clk failed!\n");
		return -1;
	}

	ctx->reset = devm_reset_control_get(dev, "ve_rst");
	if (IS_ERR(ctx->reset)) {
		dev_err(dev, "try to reset control failed!\n");
		return -1;
	}

	ret = aic_ve_parse_dt(dev, ctx);
	if (ret < 0) {
		dev_err(dev, "parse dts failed!\n");
		return ret;
	}

	platform_set_drvdata(pdev, ctx);

	// create char device
	ret = alloc_chrdev_region(&ctx->ve_dev, 0, 1, "aic_ve");
	if (ret) {
		dev_err(dev, "alloc dev_t failed!\n");
		return ret;
	}

	dev_major = MAJOR(ctx->ve_dev);
	cdev_init(&ctx->cdev, &aic_ve_cdev_fops);
	ctx->cdev.owner = THIS_MODULE;
	ctx->cdev.ops = &aic_ve_cdev_fops;
	ret = cdev_add(&ctx->cdev, ctx->ve_dev, 1);
	if (ret) {
		dev_err(dev, "add dev_t failed!\n");
		goto cdev_init_err;
	}

	ctx->class = class_create(THIS_MODULE, "aic_ve");
	if (IS_ERR(ctx->class)) {
		ret = PTR_ERR(ctx->class);
		dev_err(dev, "create class error:%d\n", ret);
		goto class_create_err;
	}

	ctx->child_dev = device_create(ctx->class, NULL, MKDEV(dev_major, 0), NULL, "aic_ve");
	if (IS_ERR(ctx->child_dev)) {
		ret = PTR_ERR(ctx->child_dev);
		dev_err(dev, "create device error:%d\n", ret);
		goto device_create_err;
	}

	return 0;

device_create_err:
	class_destroy(ctx->class);
class_create_err:
	cdev_del(&ctx->cdev);
cdev_init_err:
	unregister_chrdev_region(ctx->ve_dev, 1);
	return ret;
}

static int aic_ve_remove(struct platform_device *pdev)
{
	struct aic_ve_ctx *ctx = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	dev_dbg(&pdev->dev, "%s\n", __func__);

	if (ctx->regs_base) {
		struct resource *res = ctx->res;

		devm_iounmap(ctx->dev, ctx->regs_base);
		devm_release_mem_region(ctx->dev, res->start, resource_size(res));
		ctx->regs_base = NULL;
	}

	device_destroy(ctx->class, ctx->ve_dev);
	class_destroy(ctx->class);
	cdev_del(&ctx->cdev);
	unregister_chrdev_region(ctx->ve_dev, 1);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(dev, ctx);
	return 0;
}

static void aic_ve_shutdown(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "%s\n", __func__);
}

#ifdef CONFIG_PM
static int aic_ve_suspend(struct device *dev)
{
	struct aic_ve_ctx *ctx = dev_get_drvdata(dev);
	disable_ve_hw_clk(ctx);
	dev_dbg(ctx->dev, "aic_ve_suspend\n");
	return 0;
}

static int aic_ve_resume(struct device *dev)
{
	struct aic_ve_ctx *ctx = dev_get_drvdata(dev);
	enable_ve_hw_clk(ctx);
	dev_dbg(ctx->dev, "aic_ve_resume\n");
	return 0;
}

static SIMPLE_DEV_PM_OPS(aic_ve_pm_ops, aic_ve_suspend, aic_ve_resume);
#endif

static const struct of_device_id aic_ve_match[] = {
	{ .compatible = "artinchip,aic-ve-v1.0",},
	{}
};
MODULE_DEVICE_TABLE(of, aic_ve_match);

static struct platform_driver aic_ve_driver = {
	.probe = aic_ve_probe,
	.remove = aic_ve_remove,
	.shutdown = aic_ve_shutdown,

	.driver = {
		.name = "aic_ve",
		.owner = THIS_MODULE,
		.of_match_table = aic_ve_match,
#ifdef CONFIG_PM
		.pm = &aic_ve_pm_ops,
#endif
	},
};

module_platform_driver(aic_ve_driver);

MODULE_DESCRIPTION("ArtInChip Video Engine Driver");
MODULE_AUTHOR("Jun <lijun.li@artinchip.com>");
MODULE_LICENSE("GPL v2");
