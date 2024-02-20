// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2022 ArtInChip Technology Co., Ltd.
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/miscdevice.h>
#include <linux/bits.h>
#include <linux/io.h>
#include <linux/types.h>

#include <video/artinchip_ge.h>

#ifdef CONFIG_DMA_SHARED_BUFFER
#include <linux/dma-buf.h>
#endif
#include "ge_reg.h"

#define AIC_GE_NAME "ge"

#define GE_TIMEOUT(x) msecs_to_jiffies((x) * 1000)

#define MAX_BATCH_NUM 8
#define CMD_BUF_SIZE  (32 * 1024)

#define END_ADDR(start, size) ((start) + (size) - 1)

#define ALIGN_8B(x) (((x) + (7)) & ~(7))
#define ALIGN_128B(x) (((x) + (127)) & ~(127))

#ifdef CONFIG_DMA_SHARED_BUFFER
struct ge_dmabuf {
	struct list_head list;
	int                       fd;
	struct dma_buf            *buf;
	struct dma_buf_attachment *attach;
	struct sg_table           *sgt;
	dma_addr_t                phy_addr;
};
#endif

struct aic_ge_client {
	struct list_head list;
	struct list_head buf_list;
	struct mutex buf_lock; /* dma buf list lock */
	int id;
	int batch_num;
};

struct aic_ge_batch {
	struct list_head list;
	int offset;
	int length;
	int client_id;
};

struct aic_ge_data {
	struct device        *dev;
	void __iomem         *regs;
	struct reset_control *reset;
	struct clk           *mclk;
	ulong                mclk_rate;
	int                  irq;
	struct mutex         lock; /* hardware resource lock */
	spinlock_t           hw_lock; /* hardware resource lock */
	int                  refs;
	wait_queue_head_t    wait;
	u32                  status;
	atomic_t             cur_id;
	bool                 hw_running;
	bool                 batch_full;
	struct list_head     free;
	struct list_head     ready;
	struct aic_ge_batch  *cur_batch;
	u8                   *cmd_ptr;
	dma_addr_t           cmd_phys;
	dma_addr_t           dither_phys;
	u8                   *base_ptr;
	dma_addr_t           base_phys;
	u32	     	     counter;
	u32                  total_size;
	int                  empty_size;
	int                  write_offset;
	struct list_head     client_list;
	enum ge_mode         ge_mode;
};

static struct aic_ge_data *g_data;
static int ge_clk_enable(struct aic_ge_data *data);
static int ge_clk_disable(struct aic_ge_data *data);

static inline struct aic_ge_client *to_get_client(struct file *file)
{
	return (struct aic_ge_client *)file->private_data;
}

#ifdef CONFIG_DMA_SHARED_BUFFER

static int ge_add_dma_buf(struct aic_ge_data *data,
			  struct aic_ge_client *client,
			  struct dma_buf_info *dma_info)
{
	struct ge_dmabuf *node;
	struct ge_dmabuf *dma_buf = NULL;
	int ret = 0;

	mutex_lock(&client->buf_lock);
	list_for_each_entry_safe(dma_buf, node, &client->buf_list, list) {
		if (dma_buf->fd == dma_info->fd) {
			dev_err(data->dev, "dma fd: %d already added\n",
				dma_info->fd);
			mutex_unlock(&client->buf_lock);
			return -EINVAL;
		}
	}
	mutex_unlock(&client->buf_lock);

	dma_buf = kzalloc(sizeof(*dma_buf), GFP_KERNEL);
	if (!dma_buf)
		return -ENOMEM;

	dma_buf->fd = dma_info->fd;
	dma_buf->buf = dma_buf_get(dma_buf->fd);

	if (IS_ERR(dma_buf->buf)) {
		dev_err(data->dev, "dma_buf_get(%d) failed\n",
			dma_buf->fd);
		ret = PTR_ERR(dma_buf->buf);
		goto EXIT;
	}

	dma_buf->attach = dma_buf_attach(dma_buf->buf, data->dev);
	if (IS_ERR(dma_buf->attach)) {
		dev_err(data->dev, "dma_buf_attach(%d) failed\n",
			dma_buf->fd);
			dma_buf_put(dma_buf->buf);
		ret = PTR_ERR(dma_buf->attach);
		goto EXIT;
	}

	dma_buf->sgt = dma_buf_map_attachment(dma_buf->attach,
					      DMA_BIDIRECTIONAL);

	if (IS_ERR(dma_buf->sgt)) {
		dev_err(data->dev,
			"dma_buf_map_attachment(%d) failed\n",
			dma_buf->fd);
			dma_buf_detach(dma_buf->buf, dma_buf->attach);
			dma_buf_put(dma_buf->buf);
			ret = PTR_ERR(dma_buf->sgt);
			goto EXIT;
	}

	dma_buf->phy_addr = sg_dma_address(dma_buf->sgt->sgl);
	dma_info->phy_addr = dma_buf->phy_addr;

	mutex_lock(&client->buf_lock);
	list_add_tail(&dma_buf->list, &client->buf_list);
	mutex_unlock(&client->buf_lock);

	return 0;
EXIT:
	kfree(dma_buf);
	return ret;
}

static int ge_rm_dma_buf(struct aic_ge_data *data,
			 struct aic_ge_client *client,
			 struct dma_buf_info *dma_info)

{
	struct ge_dmabuf *node;
	struct ge_dmabuf *dma_buf = NULL;
	bool match = false;

	mutex_lock(&client->buf_lock);
	list_for_each_entry_safe(dma_buf, node, &client->buf_list, list) {
		if (dma_buf->fd == dma_info->fd) {
			match = true;
			list_del(&dma_buf->list);
			dma_buf_unmap_attachment(dma_buf->attach,
			dma_buf->sgt, DMA_BIDIRECTIONAL);
			dma_buf_detach(dma_buf->buf, dma_buf->attach);
			dma_buf_put(dma_buf->buf);
			kfree(dma_buf);
			break;
		}
	}
	mutex_unlock(&client->buf_lock);

	if (!match)
		dev_warn(data->dev, "can't find fd:%d", dma_info->fd);

	return 0;
}

static int ge_clean_dma_buf(struct aic_ge_data *data,
		      struct aic_ge_client *client)

{
	struct ge_dmabuf *dma_buf, *node;

	mutex_lock(&client->buf_lock);
	list_for_each_entry_safe(dma_buf, node, &client->buf_list, list) {
		list_del(&dma_buf->list);
		dma_buf_unmap_attachment(dma_buf->attach,
					 dma_buf->sgt, DMA_BIDIRECTIONAL);
		dma_buf_detach(dma_buf->buf, dma_buf->attach);
		dma_buf_put(dma_buf->buf);
		kfree(dma_buf);
	}
	mutex_unlock(&client->buf_lock);

	return 0;
}

#endif

static void ge_reset(struct aic_ge_data *data)
{
	reset_control_assert(data->reset);
	reset_control_deassert(data->reset);
}

static int ge_wait_empty_buffer(struct aic_ge_data *data, int req_size)
{
	int ret = 0;

	dev_dbg(data->dev, "%s()\n", __func__);

	while (req_size > data->empty_size ||
	       data->batch_full) {
		ret = wait_event_interruptible_timeout(data->wait,
						       !data->batch_full,
						       GE_TIMEOUT(4));
		if (ret < 0)
			break;
	}

	dev_dbg(data->dev, "%s()\n", __func__);

	return ret;
}

static int ge_wait_idle(struct aic_ge_data *data)
{
	int ret = 0;

	dev_dbg(data->dev, "%s()\n", __func__);

	while (data->hw_running) {
		ret = wait_event_interruptible_timeout(data->wait,
						       !data->hw_running,
						       GE_TIMEOUT(4));

		if (ret < 0)
			break;
	}

	dev_dbg(data->dev, "%s()\n", __func__);

	return ret;
}

static int ge_client_sync(struct aic_ge_data *data,
			  struct aic_ge_client *client)
{
	int ret = 0;

	dev_dbg(data->dev, "%s()\n", __func__);

	while (client->batch_num) {
		ret = wait_event_interruptible_timeout(data->wait,
						       !client->batch_num,
						       GE_TIMEOUT(4));

		if (ret < 0)
			break;
	}

	dev_dbg(data->dev, "%s()\n", __func__);

	return ret;
}

static void run_hw(struct aic_ge_data *data)
{
	struct aic_ge_batch *batch;

	dev_dbg(data->dev, "%s()\n", __func__);

	WARN_ON(list_empty(&data->ready));
	WARN_ON(data->cur_batch);

	/* dequeue batch from ready list */
	batch = list_first_entry(&data->ready,
				 struct aic_ge_batch, list);

	list_del(&batch->list);

#ifdef CONFIG_CTRL_GE_CLK_IN_FRAME
	ge_clk_enable(data);
#endif

	/* config cmd queue ring buffer */
	writel(data->cmd_phys, data->regs + CMD_BUF_START_ADDR);
	writel(END_ADDR(data->cmd_phys, data->total_size),
	       data->regs + CMD_BUF_END_ADDR);
	writel(batch->offset, data->regs + CMD_BUF_ADDR_OFFSET);
	writel(batch->length, data->regs + CMD_BUF_VALID_LENGTH);

	/* enable interrupt*/
	writel(GE_CTRL_FINISH_IRQ_EN | GE_CTRL_HW_ERR_IRQ_EN,
	       data->regs + GE_CTRL);

	/* set dither*/
	writel(data->dither_phys, data->regs + DITHER_BGN_ADDR);

	/* run cmd queue */
	writel(GE_SW_RESET | GE_CMD_EN | GE_START_EN, data->regs + GE_START);

	data->hw_running = true;
	data->cur_batch = batch;
}

static irqreturn_t aic_ge_handler(int irq, void *ctx)
{
	unsigned long flags;
	u32 status;
	struct aic_ge_client *node;
	struct aic_ge_client *cur_client = NULL;
	struct aic_ge_data *data = (struct aic_ge_data *)ctx;

	dev_dbg(data->dev, "%s()\n", __func__);

	spin_lock_irqsave(&data->hw_lock, flags);

	/* read interrupt status */
	status = readl(data->regs + GE_STATUS);

	/* clear interrupt status */
	writel(status, data->regs + GE_STATUS);

	/* disable interrupt*/
	writel(0, data->regs + GE_CTRL);

	WARN_ON(!status);

	if (status & GE_CTRL_HW_ERR_IRQ_EN) {
		dev_err(data->dev, "ge error status:%08x\n", status);
		ge_reset(data);
	}

	data->empty_size += ALIGN_8B(data->cur_batch->length);

	list_add_tail(&data->cur_batch->list, &data->free);
	data->batch_full = false;

	list_for_each_entry_safe(cur_client, node, &data->client_list, list) {
		if (cur_client->id == data->cur_batch->client_id) {
			cur_client->batch_num--;
			break;
		}
	}

	dev_dbg(data->dev, "client id:%d, task num:%d, empty size: %08x\n",
		cur_client->id, (int)(status >> 16), data->empty_size);
#ifdef CONFIG_ARTINCHIP_GE_DEBUG
	data->counter = readl(data->regs + GE_COUNTER);
	dev_dbg(data->dev, " ge counter = 0x%08x\n", data->counter);
#endif

	data->cur_batch = NULL;

#ifdef CONFIG_CTRL_GE_CLK_IN_FRAME
	ge_clk_disable(data);
#endif

	if (!list_empty(&data->ready)) {
		run_hw(data);
	} else {
		/*idle*/
		data->hw_running = false;
	}

	wake_up_all(&data->wait);

	spin_unlock_irqrestore(&data->hw_lock, flags);

	return IRQ_HANDLED;
}

static int aic_ge_request_irq(struct device *dev,
			      struct aic_ge_data *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	int irq, ret;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "Couldn't get disp engine interrupt\n");
		return irq;
	}

	data->irq = irq;
	ret = devm_request_irq(dev, irq, aic_ge_handler, 0,
			       dev_name(dev), data);
	if (ret) {
		dev_err(dev, "Couldn't request the IRQ\n");
		return ret;
	}

	return 0;
}

static long ge_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct aic_ge_data *data = g_data;
	struct aic_ge_client *client = to_get_client(file);

	switch (cmd) {
	case IOC_GE_VERSION:
	{
		u32 version = readl(data->regs + GE_VERSION_ID);

		if (copy_to_user((void __user *)arg, &version, sizeof(u32)))
			ret = -EFAULT;

		break;
	}
	case IOC_GE_MODE:
		if (copy_to_user((void __user *)arg, &data->ge_mode,
				 sizeof(enum ge_mode)))
			return -EFAULT;

		break;
	case IOC_GE_SYNC:
		ret = ge_client_sync(data, to_get_client(file));
		break;
	case IOC_GE_CMD_BUF_SIZE:
		if (copy_to_user((void __user *)arg,
				 &data->total_size, sizeof(u32)))
			ret = -EFAULT;

		break;
	case IOC_GE_FILLRECT:
	case IOC_GE_BITBLT:
	case IOC_GE_ROTATE:
		dev_err(data->dev,
			"cmd queue mode does't support normal ioctl : %08x\n",
			cmd);

		ret = -EINVAL;
		break;

#ifdef CONFIG_DMA_SHARED_BUFFER
	case IOC_GE_ADD_DMA_BUF:
	{
		struct dma_buf_info dma_buf;
		int ret;

		if (copy_from_user(&dma_buf, (void __user *)arg,
				   sizeof(struct dma_buf_info)))
			return -EFAULT;

		ret = ge_add_dma_buf(data, client, &dma_buf);
		if (ret < 0) {
			dev_err(data->dev, "map dma buf failed!\n");
			return ret;
		}

		if (copy_to_user((void __user *)arg, &dma_buf,
				 sizeof(struct dma_buf_info)))
			return -EFAULT;

		break;
	}
	case IOC_GE_RM_DMA_BUF:
	{
		struct dma_buf_info dma_buf;

		if (copy_from_user(&dma_buf, (void __user *)arg,
				   sizeof(struct dma_buf_info)))
			return -EFAULT;

		ge_rm_dma_buf(data, client, &dma_buf);
		break;
	}
#endif
	default:
		dev_err(data->dev, "Invalid ioctl: %08x\n", cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ge_clk_enable(struct aic_ge_data *data)
{
	int ret;

	ret = clk_set_rate(data->mclk, data->mclk_rate);
	if (ret) {
		dev_err(data->dev, "Failed to set CLK_DE %ld\n",
			data->mclk_rate);
	}

	ret = reset_control_deassert(data->reset);
	if (ret) {
		dev_err(data->dev, "%s() - Couldn't deassert\n", __func__);
		return ret;
	}

	ret = clk_prepare_enable(data->mclk);
	if (ret) {
		dev_err(data->dev, "%s() - Couldn't enable mclk\n", __func__);
		return ret;
	}
	return 0;
}

static int ge_clk_disable(struct aic_ge_data *data)
{
	clk_disable_unprepare(data->mclk);
	reset_control_assert(data->reset);
	return 0;
}

static ssize_t ge_write(struct file *file, const char *buff,
			size_t count, loff_t *offp)
{
	int ret;
	unsigned long flags;
	int write_offset;
	int aligned_count;
	struct aic_ge_client *client = to_get_client(file);
	struct aic_ge_data *data = g_data;
	struct aic_ge_batch *batch;

	aligned_count = ALIGN_8B(count);

	if (unlikely(aligned_count > (int)data->total_size))
		return -E2BIG;

	mutex_lock(&data->lock);

	/* wait for enough buffer */
	if (ge_wait_empty_buffer(data, aligned_count) < 0) {
		dev_err(data->dev, "failed to get enough buffer\n");
		return -EFAULT;
	}

	spin_lock_irqsave(&data->hw_lock, flags);

	/* dequeue free batch */
	batch = list_first_entry(&data->free,
				 struct aic_ge_batch, list);
	list_del(&batch->list);

	if (list_empty(&data->free))
		data->batch_full = true;

	write_offset = data->write_offset;
	data->write_offset += aligned_count;
	data->empty_size -= aligned_count;

	if (write_offset + aligned_count >= data->total_size)
		data->write_offset -= data->total_size;

	batch->client_id = client->id;
	batch->length = count;
	batch->offset = write_offset;
	client->batch_num++;

	spin_unlock_irqrestore(&data->hw_lock, flags);

	/* copy cmd from user space to ring buffer */
	if (write_offset + aligned_count <= data->total_size) {
		if (copy_from_user(data->cmd_ptr + write_offset,
				   buff, count))
			ret = -EFAULT;
	} else {
		int len0;
		int len1;

		len1 = write_offset + count - data->total_size;
		len0 = count - len1;

		if (copy_from_user(data->cmd_ptr + write_offset,
				   buff, len0))
			ret = -EFAULT;

		if (copy_from_user(data->cmd_ptr, buff + len0, len1))
			ret = -EFAULT;
	}

	/* enqueue ready batch */
	spin_lock_irqsave(&data->hw_lock, flags);
	list_add_tail(&batch->list, &data->ready);
	if (!data->hw_running)
		run_hw(data);

	spin_unlock_irqrestore(&data->hw_lock, flags);

	mutex_unlock(&data->lock);

	return count;
}

#ifndef CONFIG_CTRL_GE_CLK_IN_FRAME
static void ge_power_on(struct aic_ge_data *data)
{
	mutex_lock(&data->lock);
	if (data->refs == 0)
		ge_clk_enable(data);

	data->refs++;
	mutex_unlock(&data->lock);
}

static void ge_power_off(struct aic_ge_data *data)
{
	mutex_lock(&data->lock);
	if (data->refs == 1)
		ge_clk_disable(data);

	data->refs--;
	mutex_unlock(&data->lock);
}
#endif

static int ge_open(struct inode *inode, struct file *file)
{
	unsigned long flags;
	struct aic_ge_client *client = NULL;
	struct aic_ge_data *data = g_data;

	dev_dbg(data->dev, "%s()\n", __func__);

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return -ENOMEM;

	INIT_LIST_HEAD(&client->buf_list);
	mutex_init(&client->buf_lock);

#ifndef CONFIG_CTRL_GE_CLK_IN_FRAME
	ge_power_on(data);
#endif
	client->id = atomic_inc_return(&data->cur_id);

	spin_lock_irqsave(&data->hw_lock, flags);
	list_add_tail(&client->list, &data->client_list);
	spin_unlock_irqrestore(&data->hw_lock, flags);

	file->private_data = (void *)client;

	dev_dbg(data->dev, "%s()\n", __func__);
	return nonseekable_open(inode, file);
}

static int ge_release(struct inode *inode, struct file *file)
{
	struct aic_ge_client *client = to_get_client(file);
	struct aic_ge_data *data = g_data;
	unsigned long flags;

	if (!client)
		return -EINVAL;

	spin_lock_irqsave(&data->hw_lock, flags);
	list_del(&client->list);
	spin_unlock_irqrestore(&data->hw_lock, flags);

	ge_clean_dma_buf(data, client);
	kfree(client);
#ifndef CONFIG_CTRL_GE_CLK_IN_FRAME
	ge_power_off(data);
#endif
	return 0;
}

static const struct file_operations ge_fops = {
	.owner          = THIS_MODULE,
	.open           = ge_open,
	.write          = ge_write,
	.release        = ge_release,
	.unlocked_ioctl = ge_ioctl,
};

static struct miscdevice ge_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = AIC_GE_NAME,
	.fops  = &ge_fops,
};

static int ge_alloc_batch_buffer(struct device *dev,
				 struct aic_ge_data *data)
{
	struct aic_ge_batch *batch;

	batch = devm_kzalloc(dev, sizeof(struct aic_ge_batch), GFP_KERNEL);
	if (!batch)
		return -ENOMEM;

	list_add_tail(&batch->list, &data->free);
	return 0;
}

static ssize_t reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	bool enable;
	int ret;
	struct aic_ge_data *data = g_data;

	ret = kstrtobool(buf, &enable);
	if (ret)
		return ret;

	if (enable) {
		ge_reset(data);
		dev_info(data->dev, "GE reset\n");
	}

	return size;
}
static DEVICE_ATTR_WO(reset);

static ssize_t debug_show(struct device *dev,
				 struct device_attribute *devattr,
				 char *buf)
{
	struct aic_ge_data *data = g_data;

	sprintf(buf, "Ring buffer address: \t\t%#llx/%#llx\n"
			"Ring buffer offset: \t\t%d\n"
			"Ring buffer total_size: \t%d\n"
			"GE counter: \t\t\t0x%08x\n",
		data->cmd_phys, (u64)data->cmd_ptr,
		data->write_offset, data->total_size,
		data->counter);

	return strlen(buf);
}
static DEVICE_ATTR_RO(debug);

static struct attribute *aic_ge_attr[] = {
	&dev_attr_debug.attr,
	&dev_attr_reset.attr,
	NULL
};

static int aic_ge_sysfs_create(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(aic_ge_attr); i++) {
		if (!aic_ge_attr[i])
			break;

		if (sysfs_add_file_to_group(&dev->kobj, aic_ge_attr[i], NULL)
			< 0) {
			dev_err(dev, "Failed to create ge attr.\n");
			return -1;
		}
	}
	return 0;
}

static int aic_ge_parse_dt(struct device *dev)
{
	int ret;
	struct device_node *np = dev->of_node;
	struct aic_ge_data *data = dev_get_drvdata(dev);

	ret = of_property_read_u32(np, "mclk", (u32 *)&data->mclk_rate);
	if (ret) {
		dev_err(dev, "Can't parse CLK_GE rate\n");
		return ret;
	}

	return 0;
}

static int aic_ge_probe(struct platform_device *pdev)
{
	int ret;
	int i;
	struct aic_ge_data *data;
	struct resource *res;
	void __iomem *regs;

	dev_dbg(&pdev->dev, "%s()\n", __func__);

	ret = misc_register(&ge_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "misc_register failed:%d\n", ret);
		return ret;
	}

	data = devm_kzalloc(&pdev->dev,
			    sizeof(struct aic_ge_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	init_waitqueue_head(&data->wait);
	dev_set_drvdata(&pdev->dev, data);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	data->regs = regs;

	data->mclk = devm_clk_get(&pdev->dev, "ge");
	if (IS_ERR(data->mclk)) {
		dev_err(&pdev->dev, "Couldn't get ge clock\n");
		return PTR_ERR(data->mclk);
	}

	data->reset = devm_reset_control_get(&pdev->dev, "ge");
	if (IS_ERR(data->reset)) {
		dev_err(&pdev->dev, "Couldn't get reset\n");
		return PTR_ERR(data->reset);
	}

	ret = aic_ge_parse_dt(&pdev->dev);
	if (ret) {
		return ret;
	}

	ret = aic_ge_request_irq(&pdev->dev, data);
	if (ret) {
		dev_err(&pdev->dev, "Couldn't request graphics engine IRQ\n");
		return ret;
	}

	INIT_LIST_HEAD(&data->free);
	INIT_LIST_HEAD(&data->ready);
	INIT_LIST_HEAD(&data->client_list);

	for (i = 0; i < MAX_BATCH_NUM; i++) {
		ret = ge_alloc_batch_buffer(&pdev->dev, data);
		if (ret) {
			dev_err(&pdev->dev,
				"failed to alloc batch buffer: %d\n", i);
			return ret;
		}
	}

	data->base_ptr = dma_alloc_coherent(&pdev->dev,
					    ALIGN_128B(CMD_BUF_SIZE),
					    &data->base_phys, GFP_KERNEL);
	if (!data->base_ptr)
		return -ENOMEM;

	/* 128 bytes aligned */
	if (data->base_phys & 127) {
#ifdef CONFIG_64BIT
		data->cmd_ptr = (u8 *)ALIGN_128B((u64)data->base_ptr);
#else
		data->cmd_ptr = (u8 *)ALIGN_128B((u32)data->base_ptr);
#endif
		data->cmd_phys = ALIGN_128B(data->base_phys);

		/* dither memory allocation */
		data->dither_phys = ALIGN_128B((data->cmd_phys + (CMD_BUF_SIZE / 2)));

		data->total_size = ALIGN_128B((CMD_BUF_SIZE / 2)) - 128;
		data->empty_size = data->total_size;
	} else {
		data->cmd_ptr = data->base_ptr;
		data->cmd_phys = data->base_phys;

		data->dither_phys = ALIGN_128B((data->cmd_phys + (CMD_BUF_SIZE / 2)));

		data->total_size = ALIGN_128B((CMD_BUF_SIZE / 2));
		data->empty_size = data->total_size;
	}

	data->ge_mode = GE_MODE_CMDQ;
	data->dev = &pdev->dev;

	aic_ge_sysfs_create(&pdev->dev);

	mutex_init(&data->lock);
	spin_lock_init(&data->hw_lock);

	g_data = data;
	return 0;
}

static int aic_ge_remove(struct platform_device *pdev)
{
	struct aic_ge_data *data = dev_get_drvdata(&pdev->dev);

	dev_dbg(&pdev->dev, "%s()\n", __func__);
	ge_wait_idle(data);
	dma_free_coherent(&pdev->dev, data->total_size,
			  data->base_ptr, data->base_phys);
	misc_deregister(&ge_dev);
	return 0;
}

static const struct of_device_id aic_ge_match_table[] = {
	{.compatible = "artinchip,aic-ge-v1.0",},
	{},
};

MODULE_DEVICE_TABLE(of, aic_ge_match_table);

static struct platform_driver aic_ge_driver = {
	.probe = aic_ge_probe,
	.remove = aic_ge_remove,
	.driver = {
		.name = AIC_GE_NAME,
		.of_match_table = aic_ge_match_table,
	},
};

module_platform_driver(aic_ge_driver);

MODULE_AUTHOR("Ning Fang <ning.fang@artinchip.com>");
MODULE_DESCRIPTION("Artinchip 2D Graphics Engine with CMD Queue Driver");
MODULE_ALIAS("platform:" AIC_GE_NAME);
MODULE_LICENSE("GPL v2");

