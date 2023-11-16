// SPDX-License-Identifier: GPL-2.0-only
/*
 * SD/MMC Host Controller driver of ArtInChip SoC
 *
 * Copyright (c) 2022, ArtInChip Technology Co., Ltd
 * Authors: matteo <mintao.duan@artinchip.com>
 */
#include <linux/blkdev.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/sdio.h>
#include <linux/bitops.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/mmc/slot-gpio.h>

#include "artinchip_mmc.h"


#define DESC_RING_BUF_SZ	PAGE_SIZE

struct idmac_desc {
	__le32		des0;	/* Control Descriptor */
#define IDMAC_DES0_DIC	BIT(1)
#define IDMAC_DES0_LD	BIT(2)
#define IDMAC_DES0_FD	BIT(3)
#define IDMAC_DES0_CH	BIT(4)
#define IDMAC_DES0_ER	BIT(5)
#define IDMAC_DES0_CES	BIT(30)
#define IDMAC_DES0_OWN	BIT(31)

	__le32		des1;	/* Buffer sizes */
#define IDMAC_SET_BUFFER1_SIZE(d, s) \
	((d)->des1 = ((d)->des1 & cpu_to_le32(0x03ffe000)) | (cpu_to_le32((s) & 0x1fff)))

	__le32		des2;	/* buffer 1 physical address */

	__le32		des3;	/* buffer 2 physical address */
};

#define IDMAC_OWN_CLR64(x) \
	!((x) & cpu_to_le32(IDMAC_DES0_OWN))

/* Each descriptor can transfer up to 4KB of data in chained mode */
#define ARTINCHIP_MMC_DESC_DATA_LENGTH	0x1000

static void artinchip_mmc_request_end(struct artinchip_mmc *host,
				      struct mmc_request *mrq);

#if defined(CONFIG_DEBUG_FS)
static int artinchip_mmc_req_show(struct seq_file *s, void *v)
{
	struct artinchip_mmc_slot *slot = s->private;
	struct mmc_request *mrq;
	struct mmc_command *cmd;
	struct mmc_command *stop;
	struct mmc_data	*data;

	/* Make sure we get a consistent snapshot */
	spin_lock_bh(&slot->host->lock);
	mrq = slot->mrq;

	if (mrq) {
		cmd = mrq->cmd;
		data = mrq->data;
		stop = mrq->stop;

		if (cmd)
			seq_printf(s,
				   "CMD%u(0x%x) flg %x rsp %x %x %x %x err %d\n",
				   cmd->opcode, cmd->arg, cmd->flags,
				   cmd->resp[0], cmd->resp[1], cmd->resp[2],
				   cmd->resp[2], cmd->error);
		if (data)
			seq_printf(s, "DATA %u / %u * %u flg %x err %d\n",
				   data->bytes_xfered, data->blocks,
				   data->blksz, data->flags, data->error);
		if (stop)
			seq_printf(s,
				   "CMD%u(0x%x) flg %x rsp %x %x %x %x err %d\n",
				   stop->opcode, stop->arg, stop->flags,
				   stop->resp[0], stop->resp[1], stop->resp[2],
				   stop->resp[2], stop->error);
	}

	spin_unlock_bh(&slot->host->lock);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(artinchip_mmc_req);

static int artinchip_mmc_regs_show(struct seq_file *s, void *v)
{
	struct artinchip_mmc *host = s->private;

	pm_runtime_get_sync(host->dev);
	seq_printf(s, "SDMC_CTRST:\t0x%08x\n", mci_readl(host, SDMC_CTRST));
	seq_printf(s, "SDMC_OINTST:\t0x%08x\n", mci_readl(host, SDMC_OINTST));
	seq_printf(s, "SDMC_CMD:\t0x%08x\n", mci_readl(host, SDMC_CMD));
	seq_printf(s, "SDMC_HCTRL1:\t0x%08x\n", mci_readl(host, SDMC_HCTRL1));
	seq_printf(s, "SDMC_INTEN:\t0x%08x\n", mci_readl(host, SDMC_INTEN));
	seq_printf(s, "SDMC_CLKCTRL:\t0x%08x\n", mci_readl(host, SDMC_CLKCTRL));
	seq_printf(s, "SDMC_DLYCTRL:\t0x%08x\n", mci_readl(host, SDMC_DLYCTRL));

	pm_runtime_put_autosuspend(host->dev);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(artinchip_mmc_regs);

static void artinchip_mmc_init_debugfs(struct artinchip_mmc_slot *slot)
{
	struct mmc_host	*mmc = slot->mmc;
	struct artinchip_mmc *host = slot->host;
	struct dentry *root;

	root = mmc->debugfs_root;
	if (!root)
		return;

	debugfs_create_file("regs", 0400, root, host, &artinchip_mmc_regs_fops);
	debugfs_create_file("req", 0400, root, slot, &artinchip_mmc_req_fops);
	debugfs_create_u32("state", 0400, root, &host->state);
	debugfs_create_xul("pending_events", 0400, root,
			   &host->pending_events);
	debugfs_create_xul("completed_events", 0400, root,
			   &host->completed_events);
}
#endif /* defined(CONFIG_DEBUG_FS) */

static inline void aic_mmc_reg_set(struct artinchip_mmc *host, u32 offset,
				   u32 mask, u32 shift, u32 val)
{
	u32 cur = mci_readl(host, offset) & ~mask;

	mci_writel(host, offset, cur | (val << shift));
}

static ssize_t sample_phase_show(struct device *dev,
				 struct device_attribute *devattr, char *buf)
{
	struct artinchip_mmc *host = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", host->sample_phase);
}

static ssize_t sample_phase_store(struct device *dev,
				  struct device_attribute *devattr,
				  const char *buf, size_t count)
{
	int ret = 0;
	u32 val = 0;
	struct artinchip_mmc *host = dev_get_drvdata(dev);

	ret = kstrtouint(buf, 10, &val);
	if (ret || val > SDMC_DLYCTRL_CLK_SMP_PHA_270) {
		dev_warn(dev, "Invalid parameters: %s\n", buf);
		return -EINVAL;
	}

	host->sample_phase = val;
	aic_mmc_reg_set(host, SDMC_DLYCTRL, SDMC_DLYCTRL_CLK_SMP_PHA_MASK,
			SDMC_DLYCTRL_CLK_SMP_PHA_SHIFT, val);
	return count;
}
static DEVICE_ATTR_RW(sample_phase);

static ssize_t sample_delay_show(struct device *dev,
				 struct device_attribute *devattr, char *buf)
{
	struct artinchip_mmc *host = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", host->sample_delay);
}

static ssize_t sample_delay_store(struct device *dev,
				  struct device_attribute *devattr,
				  const char *buf, size_t count)
{
	int ret = 0;
	u32 val = 0;
	struct artinchip_mmc *host = dev_get_drvdata(dev);

	ret = kstrtouint(buf, 10, &val);
	if (ret || val > SDMC_DLYCTRL_CLK_SMP_DLY_MAX) {
		dev_warn(dev, "Invalid parameters: %s\n", buf);
		return -EINVAL;
	}

	host->sample_delay = val;
	aic_mmc_reg_set(host, SDMC_DLYCTRL, SDMC_DLYCTRL_CLK_SMP_DLY_MASK,
			SDMC_DLYCTRL_CLK_SMP_DLY_SHIFT, val);
	return count;
}
static DEVICE_ATTR_RW(sample_delay);

static ssize_t driver_phase_show(struct device *dev,
				 struct device_attribute *devattr, char *buf)
{
	struct artinchip_mmc *host = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", host->driver_phase);
}

static ssize_t driver_phase_store(struct device *dev,
				  struct device_attribute *devattr,
				  const char *buf, size_t count)
{
	int ret = 0;
	u32 val = 0;
	struct artinchip_mmc *host = dev_get_drvdata(dev);

	ret = kstrtouint(buf, 10, &val);
	if (ret || val > SDMC_DLYCTRL_CLK_DRV_PHA_270) {
		dev_warn(dev, "Invalid parameters: %s\n", buf);
		return -EINVAL;
	}

	host->driver_phase = val;
	aic_mmc_reg_set(host, SDMC_DLYCTRL, SDMC_DLYCTRL_CLK_DRV_PHA_MASK,
			SDMC_DLYCTRL_CLK_DRV_PHA_SHIFT, val);
	return count;
}
static DEVICE_ATTR_RW(driver_phase);

static ssize_t driver_delay_show(struct device *dev,
				 struct device_attribute *devattr, char *buf)
{
	struct artinchip_mmc *host = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", host->driver_delay);
}

static ssize_t driver_delay_store(struct device *dev,
				  struct device_attribute *devattr,
				  const char *buf, size_t count)
{
	int ret = 0;
	u32 val = 0;
	struct artinchip_mmc *host = dev_get_drvdata(dev);

	ret = kstrtouint(buf, 10, &val);
	if (ret || val > SDMC_DLYCTRL_CLK_DRV_DLY_MAX) {
		dev_warn(dev, "Invalid parameters: %s\n", buf);
		return -EINVAL;
	}

	host->driver_delay = val;
	aic_mmc_reg_set(host, SDMC_DLYCTRL, SDMC_DLYCTRL_CLK_DRV_DLY_MASK,
			SDMC_DLYCTRL_CLK_DRV_DLY_SHIFT, val);
	return count;
}
static DEVICE_ATTR_RW(driver_delay);


static struct attribute *aic_mmc_attr[] = {
	&dev_attr_sample_phase.attr,
	&dev_attr_sample_delay.attr,
	&dev_attr_driver_phase.attr,
	&dev_attr_driver_delay.attr,
	NULL
};

static void sdmc_reg_setbit(struct artinchip_mmc *host, u32 reg, u32 value)
{
	u32 temp = 0;

	temp = mci_readl(host, reg);
	temp |= value;
	mci_writel(host, reg, temp);
}

static void sdmc_reg_clrbit(struct artinchip_mmc *host, u32 reg, u32 mask)
{
	u32 temp = 0;

	temp = mci_readl(host, reg);
	temp &= ~mask;
	mci_writel(host, reg, temp);
}

static bool artinchip_mmc_ctrl_reset(struct artinchip_mmc *host, u32 reset)
{
	u32 ctrl;

	sdmc_reg_setbit(host, SDMC_HCTRL1, reset);

	/* wait till resets clear */
	if (readl_poll_timeout_atomic(host->regs + SDMC_HCTRL1, ctrl,
				      !(ctrl & reset),
				      1, 500 * USEC_PER_MSEC)) {
		dev_err(host->dev,
			"Timeout resetting block (ctrl reset %#x)\n",
			ctrl & reset);
		return false;
	}

	return true;
}

static void artinchip_mmc_wait_while_busy(struct artinchip_mmc *host, u32 cmd_flags)
{
	u32 status;

	/*
	 * before issuing a new data transfer command
	 * we need to check to see if the card is busy.  Data transfer commands
	 * or SDMC_CMD_VOLT_SWITCH we'll key off that.
	 */
	if ((cmd_flags & SDMC_CMD_PRV_DAT_WAIT) &&
	    !(cmd_flags & SDMC_CMD_VOLT_SWITCH)) {
		if (readl_poll_timeout_atomic(host->regs + SDMC_CTRST, status,
					      !(status & SDMC_CTRST_BUSY),
					      10, 500 * USEC_PER_MSEC))
			dev_err(host->dev, "Busy; trying anyway\n");
	}
}

static void mci_send_cmd(struct artinchip_mmc_slot *slot, u32 cmd, u32 arg)
{
	struct artinchip_mmc *host = slot->host;
	unsigned int cmd_status = 0;

	mci_writel(host, SDMC_CMDARG, arg);
	wmb(); /* drain writebuffer */
	artinchip_mmc_wait_while_busy(host, cmd);
	mci_writel(host, SDMC_CMD, SDMC_CMD_START | cmd);

	if (readl_poll_timeout_atomic(host->regs + SDMC_CMD, cmd_status,
				      !(cmd_status & SDMC_CMD_START),
				      1, 500 * USEC_PER_MSEC))
		dev_err(&slot->mmc->class_dev,
			"Timeout sending command (cmd %#x arg %#x status %#x)\n",
			cmd, arg, cmd_status);
}

static u32 artinchip_mmc_prepare_command(struct mmc_host *mmc, struct mmc_command *cmd)
{
	struct artinchip_mmc_slot *slot = mmc_priv(mmc);
	struct artinchip_mmc *host = slot->host;
	u32 cmdr;

	cmd->error = -EINPROGRESS;
	cmdr = cmd->opcode;

	if (cmd->opcode == MMC_STOP_TRANSMISSION ||
	    cmd->opcode == MMC_GO_IDLE_STATE ||
	    cmd->opcode == MMC_GO_INACTIVE_STATE ||
	    (cmd->opcode == SD_IO_RW_DIRECT &&
	     ((cmd->arg >> 9) & 0x1FFFF) == SDIO_CCCR_ABORT))
		cmdr |= SDMC_CMD_STOP;
	else if (cmd->opcode != MMC_SEND_STATUS && cmd->data)
		cmdr |= SDMC_CMD_PRV_DAT_WAIT;

	if (cmd->opcode == SD_SWITCH_VOLTAGE) {
		/* Special bit makes CMD11 not die */
		cmdr |= SDMC_CMD_VOLT_SWITCH;

		/* Change state to continue to handle CMD11 weirdness */
		WARN_ON(slot->host->state != STATE_SENDING_CMD);
		slot->host->state = STATE_SENDING_CMD11;

		/* We need to disable low power mode*/
		sdmc_reg_clrbit(host, SDMC_CLKCTRL, SDMC_CLKCTRL_LOW_PWR);

		mci_send_cmd(slot,
			     SDMC_CMD_UPD_CLK | SDMC_CMD_PRV_DAT_WAIT, 0);
	}

	if (cmd->flags & MMC_RSP_PRESENT) {
		/* We expect a response, so set this bit */
		cmdr |= SDMC_CMD_RESP_EXP;
		if (cmd->flags & MMC_RSP_136)
			cmdr |= SDMC_CMD_RESP_LEN;
	}

	if (cmd->flags & MMC_RSP_CRC)
		cmdr |= SDMC_CMD_RESP_CRC;

	if (cmd->data) {
		cmdr |= SDMC_CMD_DAT_EXP;
		if (cmd->data->flags & MMC_DATA_WRITE)
			cmdr |= SDMC_CMD_DAT_WR;
	}

	if (!test_bit(ARTINCHIP_MMC_CARD_NO_USE_HOLD, &slot->flags))
		cmdr |= SDMC_CMD_USE_HOLD_REG;

	return cmdr;
}

static u32 artinchip_mmc_prep_stop_abort(struct artinchip_mmc *host, struct mmc_command *cmd)
{
	struct mmc_command *stop;
	u32 cmdr;
	u32 temp;

	temp = 0x1;
	if (!cmd->data)
		return 0;

	stop = &host->stop_abort;
	cmdr = cmd->opcode;
	memset(stop, 0, sizeof(struct mmc_command));

	if (cmdr == MMC_READ_SINGLE_BLOCK ||
	    cmdr == MMC_READ_MULTIPLE_BLOCK ||
	    cmdr == MMC_WRITE_BLOCK ||
	    cmdr == MMC_WRITE_MULTIPLE_BLOCK ||
	    cmdr == MMC_SEND_TUNING_BLOCK ||
	    cmdr == MMC_SEND_TUNING_BLOCK_HS200) {
		stop->opcode = MMC_STOP_TRANSMISSION;
		stop->arg = 0;
		stop->flags = MMC_RSP_R1B | MMC_CMD_AC;
	} else if (cmdr == SD_IO_RW_EXTENDED) {
		stop->opcode = SD_IO_RW_DIRECT;

		stop->arg |= (temp << 31) | (0x0 << 28) | (SDIO_CCCR_ABORT << 9) |
			     ((cmd->arg >> 28) & 0x7);
		stop->flags = MMC_RSP_SPI_R5 | MMC_RSP_R5 | MMC_CMD_AC;
	} else {
		return 0;
	}

	cmdr = stop->opcode | SDMC_CMD_STOP |
		SDMC_CMD_RESP_CRC | SDMC_CMD_RESP_EXP;

	if (!test_bit(ARTINCHIP_MMC_CARD_NO_USE_HOLD, &host->slot->flags))
		cmdr |= SDMC_CMD_USE_HOLD_REG;

	return cmdr;
}

static inline void artinchip_mmc_set_cto(struct artinchip_mmc *host)
{
	unsigned int cto_clks;
	unsigned int cto_div;
	unsigned int cto_ms;
	unsigned long irqflags;

	cto_clks = SDMC_TTMC_RESP(mci_readl(host, SDMC_TTMC));
	cto_div = SDMC_GET_CLK_DIV(mci_readl(host, SDMC_CLKCTRL));
	if (cto_div == 0)
		cto_div = 1;

	cto_ms = DIV_ROUND_UP_ULL((u64)MSEC_PER_SEC * cto_clks * cto_div,
				  host->sclk_rate);

	/* add a bit spare time */
	cto_ms += 10;

	/*
	 * The durations we're working with are fairly short so we have to be
	 * extra careful about synchronization here.  Specifically in hardware a
	 * command timeout is _at most_ 5.1 ms, so that means we expect an
	 * interrupt (either command done or timeout) to come rather quickly
	 * after the mci_writel.  ...but just in case we have a long interrupt
	 * latency let's add a bit of paranoia.
	 *
	 * In general we'll assume that at least an interrupt will be asserted
	 * in hardware by the time the cto_timer runs.  ...and if it hasn't
	 * been asserted in hardware by that time then we'll assume it'll never
	 * come.
	 */
	spin_lock_irqsave(&host->irq_lock, irqflags);
	if (!test_bit(EVENT_CMD_COMPLETE, &host->pending_events))
		mod_timer(&host->cto_timer,
			jiffies + msecs_to_jiffies(cto_ms) + 1);
	spin_unlock_irqrestore(&host->irq_lock, irqflags);
}

static void artinchip_mmc_start_command(struct artinchip_mmc *host,
				 struct mmc_command *cmd, u32 cmd_flags)
{
	host->cmd = cmd;
	dev_vdbg(host->dev,
		 "start command: ARGR=0x%08x CMDR=0x%08x\n",
		 cmd->arg, cmd_flags);

	mci_writel(host, SDMC_CMDARG, cmd->arg);
	wmb(); /* drain writebuffer */
	artinchip_mmc_wait_while_busy(host, cmd_flags);
	mci_writel(host, SDMC_CMD, cmd_flags | SDMC_CMD_START);
	/* response expected command only */
	if (cmd_flags & SDMC_CMD_RESP_EXP)
		artinchip_mmc_set_cto(host);
}

static inline void send_stop_abort(struct artinchip_mmc *host, struct mmc_data *data)
{
	struct mmc_command *stop = &host->stop_abort;

	artinchip_mmc_start_command(host, stop, host->stop_cmdr);
}

/* DMA interface functions */
static void artinchip_mmc_stop_dma(struct artinchip_mmc *host)
{
	if (host->using_dma) {
		host->dma_ops->stop(host);
		host->dma_ops->cleanup(host);
	}

	/* Data transfer was stopped by the interrupt handler */
	set_bit(EVENT_XFER_COMPLETE, &host->pending_events);
}

static void artinchip_mmc_dma_cleanup(struct artinchip_mmc *host)
{
	struct mmc_data *data = host->data;

	if (data && data->host_cookie == COOKIE_MAPPED) {
		dma_unmap_sg(host->dev,
			     data->sg,
			     data->sg_len,
			     mmc_get_dma_dir(data));
		data->host_cookie = COOKIE_UNMAPPED;
	}
}

static void artinchip_mmc_idmac_stop_dma(struct artinchip_mmc *host)
{
	u32 temp;

	/* Disable and reset the IDMAC interface */
	temp = mci_readl(host, SDMC_HCTRL1);
	temp &= ~SDMC_HCTRL1_USE_IDMAC;
	temp |= SDMC_HCTRL1_DMA_RESET;
	mci_writel(host, SDMC_HCTRL1, temp);

	/* Stop the IDMAC running */
	temp = mci_readl(host, SDMC_PBUSCFG);
	temp &= ~(SDMC_PBUSCFG_IDMAC_EN | SDMC_PBUSCFG_IDMAC_FB);
	temp |= SDMC_PBUSCFG_IDMAC_SWR;
	mci_writel(host, SDMC_PBUSCFG, temp);
}

static void artinchip_mmc_dmac_complete_dma(void *arg)
{
	struct artinchip_mmc *host = arg;
	struct mmc_data *data = host->data;

	dev_vdbg(host->dev, "DMA complete\n");
	host->dma_ops->cleanup(host);
	/*
	 * If the card was removed, data will be NULL. No point in trying to
	 * send the stop command or waiting for NBUSY in this case.
	 */
	if (data) {
		set_bit(EVENT_XFER_COMPLETE, &host->pending_events);
		tasklet_schedule(&host->tasklet);
	}
}

static int artinchip_mmc_idmac_init(struct artinchip_mmc *host)
{
		int i;

		struct idmac_desc *p;
		/* Number of descriptors in the ring buffer */
		host->ring_size =
			DESC_RING_BUF_SZ / sizeof(struct idmac_desc);

		/* Forward link the descriptor list */
		for (i = 0, p = host->sg_cpu;
			 i < host->ring_size - 1;
			 i++, p++) {
			p->des3 = cpu_to_le32(host->sg_dma +
					(sizeof(struct idmac_desc) * (i + 1)));
			p->des0 = 0;
			p->des1 = 0;
		}

		/* Set the last descriptor as the end-of-ring descriptor */
		p->des3 = cpu_to_le32(host->sg_dma);
		p->des0 = cpu_to_le32(IDMAC_DES0_ER);

		/* Software reset of DMA */
		sdmc_reg_setbit(host, SDMC_PBUSCFG, SDMC_PBUSCFG_IDMAC_SWR);

		/* Mask out interrupts - get Tx & Rx complete only */
		mci_writel(host, SDMC_IDMAST, SDMC_IDMAC_INT_MASK);
		mci_writel(host, SDMC_IDMAINTEN, SDMC_IDMAC_INT_NIS |
				SDMC_IDMAC_INT_RI | SDMC_IDMAC_INT_TI);

		/* Set the descriptor base address */
		mci_writel(host, SDMC_IDMASADDR, host->sg_dma);

		return 0;
}

static inline int artinchip_mmc_prepare_desc(struct artinchip_mmc *host,
					 struct mmc_data *data,
					 unsigned int sg_len)
{
	unsigned int desc_len;
	u32 val;
	int i;
	struct idmac_desc *desc_first, *desc_last, *desc;

	desc_first = desc_last = desc = host->sg_cpu;

	for (i = 0; i < sg_len; i++) {
		unsigned int length = sg_dma_len(&data->sg[i]);
		u32 mem_addr = sg_dma_address(&data->sg[i]);

		for ( ; length ; desc++) {
			desc_len = (length <= ARTINCHIP_MMC_DESC_DATA_LENGTH) ?
				   length : ARTINCHIP_MMC_DESC_DATA_LENGTH;

			length -= desc_len;

			/*
			 * Wait for the former clear OWN bit operation
			 * of IDMAC to make sure that this descriptor
			 * isn't still owned by IDMAC as IDMAC's write
			 * ops and CPU's read ops are asynchronous.
			 */
			if (readl_poll_timeout_atomic(&desc->des0, val,
						      IDMAC_OWN_CLR64(val),
						      10,
						      100 * USEC_PER_MSEC))
				goto err_own_bit;

			/*
			 * Set the OWN bit and disable interrupts
			 * for this descriptor
			 */
			desc->des0 = cpu_to_le32(IDMAC_DES0_OWN |
						 IDMAC_DES0_DIC |
						 IDMAC_DES0_CH);

			/* Buffer length */
			IDMAC_SET_BUFFER1_SIZE(desc, desc_len);

			/* Physical address to DMA to/from */
			desc->des2 = cpu_to_le32(mem_addr);
			/* Update physical address for the next desc */
			mem_addr += desc_len;

			/* Save pointer to the last descriptor */
			desc_last = desc;
		}
	}

	/* Set first descriptor */
	desc_first->des0 |= cpu_to_le32(IDMAC_DES0_FD);

	/* Set last descriptor */
	desc_last->des0 &= cpu_to_le32(~(IDMAC_DES0_CH |
				       IDMAC_DES0_DIC));
	desc_last->des0 |= cpu_to_le32(IDMAC_DES0_LD);

	return 0;
err_own_bit:
	/* restore the descriptor chain as it's polluted */
	dev_dbg(host->dev, "descriptor is still owned by IDMAC.\n");
	memset(host->sg_cpu, 0, DESC_RING_BUF_SZ);
	artinchip_mmc_idmac_init(host);
	return -EINVAL;
}

static int artinchip_mmc_idmac_start_dma(struct artinchip_mmc *host, unsigned int sg_len)
{
	int ret;

	ret = artinchip_mmc_prepare_desc(host, host->data, sg_len);
	if (ret)
		goto out;

	/* drain writebuffer */
	wmb();
	/* Make sure to reset DMA in case we did PIO before this */
	artinchip_mmc_ctrl_reset(host, SDMC_HCTRL1_DMA_RESET);
	/* Software reset of DMA */
	sdmc_reg_setbit(host, SDMC_PBUSCFG, SDMC_PBUSCFG_IDMAC_SWR);
	/* Select IDMAC interface */
	sdmc_reg_setbit(host, SDMC_HCTRL1, SDMC_HCTRL1_USE_IDMAC);
	/* drain writebuffer */
	wmb();
	/* Enable the IDMAC */
	sdmc_reg_setbit(host, SDMC_PBUSCFG,
			SDMC_PBUSCFG_IDMAC_EN | SDMC_PBUSCFG_IDMAC_FB);
	/* Start it running */
	mci_writel(host, SDMC_IDMARCAP, 1);

out:
	return ret;
}

static const struct artinchip_mmc_dma_ops artinchip_mmc_idmac_ops = {
	.init = artinchip_mmc_idmac_init,
	.start = artinchip_mmc_idmac_start_dma,
	.stop = artinchip_mmc_idmac_stop_dma,
	.complete = artinchip_mmc_dmac_complete_dma,
	.cleanup = artinchip_mmc_dma_cleanup,
};

static int artinchip_mmc_pre_dma_transfer(struct artinchip_mmc *host,
				   struct mmc_data *data,
				   int cookie)
{
	struct scatterlist *sg;
	unsigned int i, sg_len;

	if (data->host_cookie == COOKIE_PRE_MAPPED)
		return data->sg_len;

	/*
	 * We don't do DMA on "complex" transfers, i.e. with
	 * non-word-aligned buffers or lengths. Also, we don't bother
	 * with all the DMA setup overhead for short transfers.
	 */
	if (data->blocks * data->blksz < ARTINCHIP_MMC_DMA_THRESHOLD)
		return -EINVAL;

	if (data->blksz & 3)
		return -EINVAL;

	for_each_sg(data->sg, sg, data->sg_len, i) {
		if (sg->offset & 3 || sg->length & 3)
			return -EINVAL;
	}

	sg_len = dma_map_sg(host->dev,
			    data->sg,
			    data->sg_len,
			    mmc_get_dma_dir(data));
	if (sg_len == 0)
		return -EINVAL;

	data->host_cookie = cookie;

	return sg_len;
}

static void artinchip_mmc_pre_req(struct mmc_host *mmc,
			   struct mmc_request *mrq)
{
	struct artinchip_mmc_slot *slot = mmc_priv(mmc);
	struct mmc_data *data = mrq->data;

	if (!slot->host->use_dma || !data)
		return;

	/* This data might be unmapped at this time */
	data->host_cookie = COOKIE_UNMAPPED;

	if (artinchip_mmc_pre_dma_transfer(slot->host, mrq->data,
				COOKIE_PRE_MAPPED) < 0)
		data->host_cookie = COOKIE_UNMAPPED;
}

static void artinchip_mmc_post_req(struct mmc_host *mmc,
			    struct mmc_request *mrq,
			    int err)
{
	struct artinchip_mmc_slot *slot = mmc_priv(mmc);
	struct mmc_data *data = mrq->data;

	if (!slot->host->use_dma || !data)
		return;

	if (data->host_cookie != COOKIE_UNMAPPED)
		dma_unmap_sg(slot->host->dev,
			     data->sg,
			     data->sg_len,
			     mmc_get_dma_dir(data));
	data->host_cookie = COOKIE_UNMAPPED;
}

static int artinchip_mmc_get_cd(struct mmc_host *mmc)
{
	int present;
	struct artinchip_mmc_slot *slot = mmc_priv(mmc);
	struct artinchip_mmc *host = slot->host;
	int gpio_cd = mmc_gpio_get_cd(mmc);

	/* Use platform get_cd function, else try onboard card detect */
	if (((mmc->caps & MMC_CAP_NEEDS_POLL)
				|| !mmc_card_is_removable(mmc))) {
		present = 1;

		if (!test_bit(ARTINCHIP_MMC_CARD_PRESENT, &slot->flags)) {
			if (mmc->caps & MMC_CAP_NEEDS_POLL) {
				dev_info(&mmc->class_dev,
					"card is polling.\n");
			} else {
				dev_info(&mmc->class_dev,
					"card is non-removable.\n");
			}
			set_bit(ARTINCHIP_MMC_CARD_PRESENT, &slot->flags);
		}

		return present;
	} else if (gpio_cd >= 0)
		present = gpio_cd;
	else
		present = ((mci_readl(host, SDMC_CDET) & SDMC_CDET_ABSENT)) ?
			   0 : 1;

	spin_lock(&host->lock);
	if (present && !test_and_set_bit(ARTINCHIP_MMC_CARD_PRESENT, &slot->flags))
		dev_dbg(&mmc->class_dev, "card is present\n");
	else if (!present &&
			!test_and_clear_bit(ARTINCHIP_MMC_CARD_PRESENT, &slot->flags))
		dev_dbg(&mmc->class_dev, "card is not present\n");
	spin_unlock(&host->lock);
	return present;
}

static void artinchip_mmc_adjust_fifoth(struct artinchip_mmc *host, struct mmc_data *data)
{
	unsigned int blksz = data->blksz;
	static const u32 mszs[] = {1, 4, 8, 16, 32, 64, 128, 256};
	u32 fifo_width = 1 << host->data_shift;
	u32 blksz_depth = blksz / fifo_width, fifoth_val;
	u32 msize = 0, rx_wmark = 1, tx_wmark, tx_wmark_invers;
	int idx = ARRAY_SIZE(mszs) - 1;

	/* pio should ship this scenario */
	if (!host->use_dma)
		return;

	tx_wmark = (host->fifo_depth) / 2;
	tx_wmark_invers = host->fifo_depth - tx_wmark;

	/*
	 * MSIZE is '1',
	 * if blksz is not a multiple of the FIFO width
	 */
	if (blksz % fifo_width)
		goto done;

	do {
		if (!((blksz_depth % mszs[idx]) ||
		     (tx_wmark_invers % mszs[idx]))) {
			msize = idx;
			rx_wmark = mszs[idx] - 1;
			break;
		}
	} while (--idx > 0);
	/*
	 * If idx is '0', it won't be tried
	 * Thus, initial values are uesed
	 */
done:
	fifoth_val = SDMC_FIFOCFG_SET_THD(msize, rx_wmark, tx_wmark);
	mci_writel(host, SDMC_FIFOCFG, fifoth_val);
}

static bool artinchip_mmc_check_hs400_mode(struct artinchip_mmc *host, struct mmc_data *data)
{
	if (data->flags & MMC_DATA_WRITE &&
		host->timing != MMC_TIMING_MMC_HS400)
		goto done;

	if (host->timing != MMC_TIMING_MMC_HS200 &&
		host->timing != MMC_TIMING_UHS_SDR104 &&
		host->timing != MMC_TIMING_MMC_HS400)
		goto done;

	if (data->blksz / (1 << host->data_shift) > host->fifo_depth)
		goto done;
	return true;

done:
	return false;
}

static void artinchip_mmc_ctrl_thld(struct artinchip_mmc *host, struct mmc_data *data)
{
	unsigned int blksz = data->blksz;
	u32 blksz_depth, fifo_depth;
	u16 thld_size;
	u8 enable;

	if (!artinchip_mmc_check_hs400_mode(host, data)) {
		mci_writel(host, SDMC_CTC, 0);
		return;
	}

	if (data->flags & MMC_DATA_WRITE)
		enable = SDMC_CTC_WR_THD_EN;
	else
		enable = SDMC_CTC_RD_THD_EN;

	blksz_depth = blksz / (1 << host->data_shift);
	fifo_depth = host->fifo_depth;
	/*
	 * If (blksz_depth) >= (fifo_depth >> 1), should be 'thld_size <= blksz'
	 * If (blksz_depth) <  (fifo_depth >> 1), should be thld_size = blksz
	 * Currently just choose blksz.
	 */
	thld_size = blksz;
	mci_writel(host, SDMC_CTC, SDMC_CTC_SET_THD(thld_size, enable));
}

static int artinchip_mmc_submit_data_dma(struct artinchip_mmc *host, struct mmc_data *data)
{
	unsigned long irqflags;
	int sg_len;

	host->using_dma = false;

	/* If don't have a channel, we can't do DMA */
	if (!host->use_dma)
		return -ENODEV;

	sg_len = artinchip_mmc_pre_dma_transfer(host, data, COOKIE_MAPPED);
	if (sg_len < 0) {
		host->dma_ops->stop(host);
		return sg_len;
	}

	host->using_dma = true;

	if (host->use_dma)
		dev_vdbg(host->dev,
			 "sd sg_cpu: %#lx sg_dma: %#lx sg_len: %d\n",
			 (unsigned long)host->sg_cpu,
			 (unsigned long)host->sg_dma,
			 sg_len);

	/*
	 * Decide the MSIZE and RX/TX Watermark.
	 * If current block size is same with previous size,
	 * no need to update fifoth.
	 */
	if (host->prev_blksz != data->blksz)
		artinchip_mmc_adjust_fifoth(host, data);

	/* Enable the DMA interface */
	sdmc_reg_setbit(host, SDMC_HCTRL1, SDMC_HCTRL1_DMA_EN);

	/* Disable RX/TX IRQs, let DMA handle it */
	spin_lock_irqsave(&host->irq_lock, irqflags);
	sdmc_reg_clrbit(host, SDMC_INTEN, SDMC_INT_RXDR | SDMC_INT_TXDR);

	spin_unlock_irqrestore(&host->irq_lock, irqflags);

	if (host->dma_ops->start(host, sg_len)) {
		host->dma_ops->stop(host);
		/* We can't do DMA, try PIO for this one */
		dev_dbg(host->dev,
			"%s: fall back to PIO mode for current transfer\n",
			__func__);
		return -ENODEV;
	}

	return 0;
}

static void artinchip_mmc_submit_data(struct artinchip_mmc *host,
				      struct mmc_data *data)
{
	unsigned long irqflags;
	int flags = SG_MITER_ATOMIC;

	data->error = -EINPROGRESS;

	WARN_ON(host->data);
	host->sg = NULL;
	host->data = data;

	if (data->flags & MMC_DATA_READ)
		host->dir_status = ARTINCHIP_MMC_RECV_STATUS;
	else
		host->dir_status = ARTINCHIP_MMC_SEND_STATUS;

	artinchip_mmc_ctrl_thld(host, data);

	if (artinchip_mmc_submit_data_dma(host, data)) {
		if (host->data->flags & MMC_DATA_READ)
			flags |= SG_MITER_TO_SG;
		else
			flags |= SG_MITER_FROM_SG;

		sg_miter_start(&host->sg_miter, data->sg, data->sg_len, flags);
		host->sg = data->sg;
		host->part_buf_start = 0;
		host->part_buf_count = 0;

		mci_writel(host, SDMC_OINTST, SDMC_INT_TXDR | SDMC_INT_RXDR);

		spin_lock_irqsave(&host->irq_lock, irqflags);
		sdmc_reg_setbit(host, SDMC_INTEN,
				SDMC_INT_TXDR | SDMC_INT_RXDR);
		spin_unlock_irqrestore(&host->irq_lock, irqflags);

		sdmc_reg_clrbit(host, SDMC_HCTRL1, SDMC_HCTRL1_DMA_EN);

		/*
		 * Use the initial fifoth_val for PIO mode. If wm_aligned
		 * is set, we set watermark same as data size.
		 * If next issued data may be transferred by DMA mode,
		 * prev_blksz should be invalidated.
		 */
		if (host->wm_aligned)
			artinchip_mmc_adjust_fifoth(host, data);
		else
			mci_writel(host, SDMC_FIFOCFG, host->fifoth_val);
		host->prev_blksz = 0;
	} else {
		/*
		 * Keep the current block size.
		 * It will be used to decide whether to update
		 * fifoth register next time.
		 */
		host->prev_blksz = data->blksz;
	}
}

static void aic_sdmc_set_ext_clk_mux(struct artinchip_mmc *host, u32 mux)
{
	u32 temp = mci_readl(host, SDMC_DLYCTRL);

	temp &= ~SDMC_DLYCTRL_EXT_CLK_MUX_MASK;
	switch (mux) {
	case 1:
		temp |= SDMC_DLYCTRL_EXT_CLK_MUX_1 <<
			SDMC_DLYCTRL_EXT_CLK_MUX_SHIFT;
		break;
	case 2:
		temp |= SDMC_DLYCTRL_EXT_CLK_MUX_2 <<
			SDMC_DLYCTRL_EXT_CLK_MUX_SHIFT;
		break;
	case 4:
	default:
		temp |= SDMC_DLYCTRL_EXT_CLK_MUX_4 <<
			SDMC_DLYCTRL_EXT_CLK_MUX_SHIFT;
		break;
	}

	mci_writel(host, SDMC_DLYCTRL, temp);
}

static u32 aic_sdmc_get_best_div(u32 sclk, u32 target_freq)
{
	u32 down, up, f_down, f_up;

	down = sclk / target_freq;
	up = DIV_ROUND_UP(sclk, target_freq);

	f_down = down == 0 ? sclk : sclk / down;
	f_up = up == 0 ? sclk : sclk / up;

	/* Select the closest div parameter */
	if ((f_down - target_freq) < (target_freq - f_up))
		return down;
	return up;
}

static void artinchip_mmc_setup_bus(struct artinchip_mmc_slot *slot,
				    bool force_clkinit)
{
	struct artinchip_mmc *host = slot->host;
	unsigned int clock = slot->clock;
	u32 mux, div, actual;
	u32 temp;
	u32 sdmc_cmd_bits = SDMC_CMD_UPD_CLK | SDMC_CMD_PRV_DAT_WAIT;

	/* We must continue to set bit 28 in CMD until the change is complete */
	if (host->state == STATE_WAITING_CMD11_DONE)
		sdmc_cmd_bits |= SDMC_CMD_VOLT_SWITCH;

	slot->mmc->actual_clock = 0;

	if (!clock) {
		sdmc_reg_clrbit(host, SDMC_CLKCTRL, SDMC_CLKCTRL_EN);
		mci_send_cmd(slot, sdmc_cmd_bits, 0);
	} else if (clock != host->current_speed || force_clkinit) {
		if (host->sclk_rate == clock) {
			mux = 1;
			div = 0;
		} else {
			div = aic_sdmc_get_best_div(host->sclk_rate, clock);
			if (div <= 4) {
				mux = DIV_ROUND_UP(div, 2);
			} else {
				if (div % 8)
					mux = 2;
				else
					mux = 4;
			}
			div /= mux * 2;
			if (div > SDMC_CLKCTRL_DIV_MAX)
				div = SDMC_CLKCTRL_DIV_MAX;
		}
		aic_sdmc_set_ext_clk_mux(host, mux);

		if (div)
			actual = host->sclk_rate / mux / div / 2;
		else
			/* div = 1 or 0 */
			actual = host->sclk_rate / mux;

		if ((clock != slot->__clk_old &&
			!test_bit(ARTINCHIP_MMC_CARD_NEEDS_POLL, &slot->flags)) ||
			force_clkinit) {
			/* Silent the verbose log if calling from PM context */
			if (!force_clkinit)
				dev_info(&slot->mmc->class_dev,
					 "sclk %d KHz, req %d KHz, "
					 "actual %d KHz, div %d-%d\n",
					 host->sclk_rate / 1000,
					 clock / 1000, actual / 1000, mux, div);

			/*
			 * If card is polling, display the message only
			 * one time at boot time.
			 */
			if (slot->mmc->caps & MMC_CAP_NEEDS_POLL &&
					slot->mmc->f_min == clock)
				set_bit(ARTINCHIP_MMC_CARD_NEEDS_POLL, &slot->flags);
		}

		/* disable clock */
		sdmc_reg_clrbit(host, SDMC_CLKCTRL, SDMC_CLKCTRL_EN);

		/* inform Card interface */
		mci_send_cmd(slot, sdmc_cmd_bits, 0);

		/* set clock to desired speed */
		temp = mci_readl(host, SDMC_CLKCTRL);
		temp &= ~SDMC_CLKCTRL_DIV_MASK;
		temp |= div << SDMC_CLKCTRL_DIV_SHIFT;
		mci_writel(host, SDMC_CLKCTRL, temp);

		/* inform Card interface */
		mci_send_cmd(slot, sdmc_cmd_bits, 0);

		/* enable clock; only low power if no SDIO */
		if (!test_bit(ARTINCHIP_MMC_CARD_NO_LOW_PWR, &slot->flags))
			sdmc_reg_setbit(host, SDMC_CLKCTRL,
					SDMC_CLKCTRL_LOW_PWR);

		sdmc_reg_setbit(host, SDMC_CLKCTRL, SDMC_CLKCTRL_EN);

		/* inform Card interface */
		mci_send_cmd(slot, sdmc_cmd_bits, 0);

		/* keep the last clock value that was requested from core */
		slot->__clk_old = clock;
		slot->mmc->actual_clock = actual;
	}

	host->current_speed = clock;

	/* Set the current slot bus width */
	temp = mci_readl(host, SDMC_HCTRL2);
	temp &= ~(0x3 << 28);
	temp |= (slot->ctype);
	mci_writel(host, SDMC_HCTRL2, temp);
}

static void __artinchip_mmc_start_request(struct artinchip_mmc *host,
						struct artinchip_mmc_slot *slot,
						struct mmc_command *cmd)
{
	struct mmc_request *mrq;
	struct mmc_data	*data;
	u32 cmdflags;

	mrq = slot->mrq;

	host->mrq = mrq;
	host->pending_events = 0;
	host->completed_events = 0;
	host->cmd_status = 0;
	host->data_status = 0;
	host->dir_status = 0;

	data = cmd->data;
	if (data) {
		mci_writel(host, SDMC_TTMC, 0xFFFFFFFF);
		mci_writel(host, SDMC_BLKCNT, data->blocks);
		mci_writel(host, SDMC_BLKSIZ, data->blksz);
	}

	cmdflags = artinchip_mmc_prepare_command(slot->mmc, cmd);

	/* this is the first command, send the initialization clock */
	if (test_and_clear_bit(ARTINCHIP_MMC_CARD_NEED_INIT, &slot->flags))
		cmdflags |= SDMC_CMD_INIT;

	if (data) {
		artinchip_mmc_submit_data(host, data);
		wmb(); /* drain writebuffer */
	}
	artinchip_mmc_start_command(host, cmd, cmdflags);

	if (cmd->opcode == SD_SWITCH_VOLTAGE) {
		unsigned long irqflags;

		/*
		 *the cmd11 interrupt fail after over 130ms
		 * We'll set to 500ms, plus an extra jiffy just in case jiffies
		 * is just about to roll over.
		 * We do this whole thing under spinlock and only if the
		 * command hasn't already completed
		 */
		spin_lock_irqsave(&host->irq_lock, irqflags);
		if (!test_bit(EVENT_CMD_COMPLETE, &host->pending_events))
			mod_timer(&host->cmd11_timer,
				jiffies + msecs_to_jiffies(500) + 1);
		spin_unlock_irqrestore(&host->irq_lock, irqflags);
	}

	host->stop_cmdr = artinchip_mmc_prep_stop_abort(host, cmd);
}

static void artinchip_mmc_start_request(struct artinchip_mmc *host,
				 struct artinchip_mmc_slot *slot)
{
	struct mmc_request *mrq = slot->mrq;
	struct mmc_command *cmd;

	cmd = mrq->sbc ? mrq->sbc : mrq->cmd;
	__artinchip_mmc_start_request(host, slot, cmd);
}

/* must be called with host->lock held */
static void artinchip_mmc_queue_request(struct artinchip_mmc *host, struct artinchip_mmc_slot *slot,
				 struct mmc_request *mrq)
{
	dev_vdbg(&slot->mmc->class_dev, "queue request: state=%d\n",
		 host->state);

	slot->mrq = mrq;

	if (host->state == STATE_WAITING_CMD11_DONE) {
		dev_warn(&slot->mmc->class_dev,
			 "Voltage change didn't complete\n");
		host->state = STATE_IDLE;
	}

	if (host->state == STATE_IDLE) {
		host->state = STATE_SENDING_CMD;
		artinchip_mmc_start_request(host, slot);
	} else {
		list_add_tail(&slot->queue_node, &host->queue);
	}
}

static void artinchip_mmc_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct artinchip_mmc_slot *slot = mmc_priv(mmc);
	struct artinchip_mmc *host = slot->host;

	WARN_ON(slot->mrq);
	/*
	 * The check for card presence and queueing of the request must be
	 * atomic, otherwise the card could be removed in between and the
	 * request wouldn't fail until another card was inserted.
	 */

	if (!artinchip_mmc_get_cd(mmc)) {
		mrq->cmd->error = -ENOMEDIUM;
		mmc_request_done(mmc, mrq);
		return;
	}

	spin_lock_bh(&host->lock);
	artinchip_mmc_queue_request(host, slot, mrq);

	spin_unlock_bh(&host->lock);
}

static void artinchip_mmc_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct artinchip_mmc_slot *slot = mmc_priv(mmc);
	u32 regs;
	int ret;

	switch (ios->bus_width) {
	case MMC_BUS_WIDTH_4:
		slot->ctype = SDMC_HCTRL2_BW_4BIT;
		break;
	case MMC_BUS_WIDTH_8:
		slot->ctype = SDMC_HCTRL2_BW_8BIT;
		break;
	default:
		/* set default 1 bit mode */
		slot->ctype = SDMC_HCTRL2_BW_1BIT;
	}

	regs = mci_readl(slot->host, SDMC_HCTRL2);

	/* DDR mode set */
	if (ios->timing == MMC_TIMING_MMC_DDR52 ||
	    ios->timing == MMC_TIMING_UHS_DDR50 ||
	    ios->timing == MMC_TIMING_MMC_HS400)
		regs |= SDMC_HCTRL2_DDR_MODE;
	else
		regs &= ~SDMC_HCTRL2_DDR_MODE;

	mci_writel(slot->host, SDMC_HCTRL2, regs);
	slot->host->timing = ios->timing;

	/*
	 * Use mirror of ios->clock to prevent race with mmc
	 * core ios update when finding the minimum.
	 */
	slot->clock = ios->clock;

	switch (ios->power_mode) {
	case MMC_POWER_UP:
		if (!IS_ERR(mmc->supply.vmmc)) {
			ret = mmc_regulator_set_ocr(mmc, mmc->supply.vmmc,
					ios->vdd);
			if (ret) {
				dev_err(slot->host->dev,
					"failed to enable vmmc regulator\n");
				/*return, if failed turn on vmmc*/
				return;
			}
		}
		set_bit(ARTINCHIP_MMC_CARD_NEED_INIT, &slot->flags);
		break;
	case MMC_POWER_ON:
		if (!slot->host->vqmmc_enabled) {
			if (!IS_ERR(mmc->supply.vqmmc)) {
				ret = regulator_enable(mmc->supply.vqmmc);
				if (ret < 0)
					dev_err(slot->host->dev,
						"failed to enable vqmmc\n");
				else
					slot->host->vqmmc_enabled = true;

			} else {
				/* Keep track so we don't reset again */
				slot->host->vqmmc_enabled = true;
			}

			/* Reset our state machine after powering on */
			artinchip_mmc_ctrl_reset(slot->host,
						 SDMC_HCTRL1_RESET_ALL);
		}

		/* Adjust clock / bus width after power is up */
		artinchip_mmc_setup_bus(slot, false);

		break;
	case MMC_POWER_OFF:
		/* Turn clock off before power goes down */
		artinchip_mmc_setup_bus(slot, false);

		if (!IS_ERR(mmc->supply.vmmc))
			mmc_regulator_set_ocr(mmc, mmc->supply.vmmc, 0);

		if (!IS_ERR(mmc->supply.vqmmc) && slot->host->vqmmc_enabled)
			regulator_disable(mmc->supply.vqmmc);
		slot->host->vqmmc_enabled = false;
		break;
	default:
		break;
	}

	if (slot->host->state == STATE_WAITING_CMD11_DONE && ios->clock != 0)
		slot->host->state = STATE_IDLE;
}

static int artinchip_mmc_card_busy(struct mmc_host *mmc)
{
	struct artinchip_mmc_slot *slot = mmc_priv(mmc);
	u32 status;

	status = mci_readl(slot->host, SDMC_CTRST);

	return !!(status & SDMC_CTRST_BUSY);
}

static int artinchip_mmc_switch_voltage(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct artinchip_mmc_slot *slot = mmc_priv(mmc);
	struct artinchip_mmc *host = slot->host;
	u32 uhs;
	int ret;

	/* Program the voltage.*/
	uhs = mci_readl(host, SDMC_HCTRL2);
	if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_330)
		uhs &= ~SDMC_HCTRL2_VOLT_18V;
	else
		uhs |= SDMC_HCTRL2_VOLT_18V;

	if (!IS_ERR(mmc->supply.vqmmc)) {
		ret = mmc_regulator_set_vqmmc(mmc, ios);
		if (ret < 0) {
			dev_dbg(&mmc->class_dev,
				"Regulator set error %d - %s V\n", ret,
				uhs & SDMC_HCTRL2_VOLT_18V ? "1.8" : "3.3");
			return ret;
		}
	}
	mci_writel(host, SDMC_HCTRL2, uhs);

	return 0;
}

static int artinchip_mmc_get_ro(struct mmc_host *mmc)
{
	/* Not support protect*/
	return 0;
}

static void artinchip_mmc_hw_reset(struct mmc_host *mmc)
{
	struct artinchip_mmc_slot *slot = mmc_priv(mmc);
	struct artinchip_mmc *host = slot->host;

	if (host->use_dma)
		/* Software reset of DMA */
		sdmc_reg_setbit(host, SDMC_PBUSCFG, SDMC_PBUSCFG_IDMAC_SWR);

	if (!artinchip_mmc_ctrl_reset(host, SDMC_HCTRL1_DMA_RESET |
				     SDMC_HCTRL1_FIFO_RESET))
		return;

	/*
	 * According to eMMC spec, card reset procedure:
	 * pulse width >= 1us
	 * Command time >= 200us
	 * high period >= 1us
	 */
	sdmc_reg_clrbit(host, SDMC_HCTRL1, SDMC_HCTRL1_CARD_RESET);
	usleep_range(1, 2);

	sdmc_reg_setbit(host, SDMC_HCTRL1, SDMC_HCTRL1_CARD_RESET);
	usleep_range(200, 300);
}

static void artinchip_mmc_init_card(struct mmc_host *mmc, struct mmc_card *card)
{
	struct artinchip_mmc_slot *slot = mmc_priv(mmc);
	struct artinchip_mmc *host = slot->host;

	/*
	 * Low power mode will stop the card clock when idle.  According to the
	 * description of the SDMC_CLKCTRL register we should disable
	 * low power mode for SDIO cards if we need SDIO interrupts to work.
	 */
	if (mmc->caps & MMC_CAP_SDIO_IRQ) {
		const u32 clken_low_pwr = SDMC_CLKCTRL_LOW_PWR;
		u32 clk_en_a_old;
		u32 clk_en_a;

		clk_en_a_old = mci_readl(host, SDMC_CLKCTRL);

		if (card->type == MMC_TYPE_SDIO ||
		    card->type == MMC_TYPE_SD_COMBO) {
			set_bit(ARTINCHIP_MMC_CARD_NO_LOW_PWR, &slot->flags);
			clk_en_a = clk_en_a_old & ~clken_low_pwr;
		} else {
			clear_bit(ARTINCHIP_MMC_CARD_NO_LOW_PWR, &slot->flags);
			clk_en_a = clk_en_a_old | clken_low_pwr;
		}
		if (clk_en_a != clk_en_a_old) {
			mci_writel(host, SDMC_CLKCTRL, clk_en_a);
			mci_send_cmd(slot, SDMC_CMD_UPD_CLK |
				     SDMC_CMD_PRV_DAT_WAIT, 0);
		}
	}
}

static void __artinchip_mmc_enable_sdio_irq(struct artinchip_mmc_slot *slot, int enb)
{
	struct artinchip_mmc *host = slot->host;
	unsigned long irqflags;

	spin_lock_irqsave(&host->irq_lock, irqflags);
	/* Enable/disable Slot Specific SDIO interrupt */
	if (enb)
		sdmc_reg_setbit(host, SDMC_INTEN, SDMC_INT_SDIO);
	else
		sdmc_reg_clrbit(host, SDMC_INTEN, SDMC_INT_SDIO);

	spin_unlock_irqrestore(&host->irq_lock, irqflags);
}

static void artinchip_mmc_enable_sdio_irq(struct mmc_host *mmc, int enb)
{
	struct artinchip_mmc_slot *slot = mmc_priv(mmc);
	struct artinchip_mmc *host = slot->host;

	__artinchip_mmc_enable_sdio_irq(slot, enb);

	/* Avoid runtime suspending the device when SDIO IRQ is enabled */
	if (enb)
		pm_runtime_get_noresume(host->dev);
	else
		pm_runtime_put_noidle(host->dev);
}

static void artinchip_mmc_ack_sdio_irq(struct mmc_host *mmc)
{
	struct artinchip_mmc_slot *slot = mmc_priv(mmc);

	__artinchip_mmc_enable_sdio_irq(slot, 1);
}

static int artinchip_mmc_execute_tuning(struct mmc_host *mmc, u32 opcode)
{
	struct artinchip_mmc_slot *slot = mmc_priv(mmc);
	struct artinchip_mmc *host = slot->host;
	const struct artinchip_mmc_drv_data *drv_data = host->drv_data;
	int err = -EINVAL;

	if (drv_data && drv_data->execute_tuning)
		err = drv_data->execute_tuning(slot, opcode);
	return err;
}

static int artinchip_mmc_prepare_hs400_tuning(struct mmc_host *mmc,
				       struct mmc_ios *ios)
{
	struct artinchip_mmc_slot *slot = mmc_priv(mmc);
	struct artinchip_mmc *host = slot->host;
	const struct artinchip_mmc_drv_data *drv_data = host->drv_data;

	if (drv_data && drv_data->prepare_hs400_tuning)
		return drv_data->prepare_hs400_tuning(host, ios);

	return 0;
}

static void aic_sdmc_card_event(struct mmc_host *mmc)
{
	struct artinchip_mmc_slot *slot = mmc_priv(mmc);
	struct artinchip_mmc *host = slot->host;
	unsigned long flags;
	int present;

	present = mmc->ops->get_cd(mmc);

	spin_lock_irqsave(&host->lock, flags);

	/* Check requests first in case we are runtime */
	if (host->cmd && !present) {
		host->cmd->error = -ENOMEDIUM;
		mmc_request_done(mmc, slot->mrq);
		artinchip_mmc_hw_reset(mmc);
		dev_err(host->dev, "Card removed during transfer!\n");
		dev_err(host->dev, "Resetting controller.\n");
	}

	spin_unlock_irqrestore(&host->lock, flags);
}

static bool artinchip_mmc_reset(struct artinchip_mmc *host)
{
	u32 flags = SDMC_HCTRL1_RESET | SDMC_HCTRL1_FIFO_RESET;
	bool ret = false;
	u32 status = 0;

	/*
	 * Resetting generates a block interrupt, hence setting
	 * the scatter-gather pointer to NULL.
	 */
	if (host->sg) {
		sg_miter_stop(&host->sg_miter);
		host->sg = NULL;
	}

	if (host->use_dma)
		flags |= SDMC_HCTRL1_DMA_RESET;

	if (artinchip_mmc_ctrl_reset(host, flags)) {
		/*
		 * In all cases we clear the RAWINTS
		 * register to clear any interrupts.
		 */
		mci_writel(host, SDMC_OINTST, 0xFFFFFFFF);

		if (!host->use_dma) {
			ret = true;
			goto out;
		}

		/* Wait for dma_req to be cleared */
		if (readl_poll_timeout_atomic(host->regs + SDMC_CTRST,
					      status,
					      !(status & SDMC_CTRST_DMA_REQ),
					      1, 500 * USEC_PER_MSEC)) {
			dev_err(host->dev,
				"%s: Timeout waiting for dma_req to be cleared\n",
				__func__);
			goto out;
		}

		/* when using DMA next we reset the fifo again */
		if (!artinchip_mmc_ctrl_reset(host, SDMC_HCTRL1_FIFO_RESET))
			goto out;
	} else {
		/* if the controller reset bit did clear, then set clock regs */
		if (!(mci_readl(host, SDMC_HCTRL1) & SDMC_HCTRL1_RESET)) {
			dev_err(host->dev,
				"%s: fifo/dma reset bits didn't clear but cif was reset, doing clock update\n",
				__func__);
			goto out;
		}
	}

	if (host->use_dma)
		/* It is also required that we reinit idmac */
		artinchip_mmc_idmac_init(host);

	ret = true;

out:
	/* After a CTRL reset we need to set clock registers  */
	mci_send_cmd(host->slot, SDMC_CMD_UPD_CLK, 0);

	return ret;
}

static const struct mmc_host_ops artinchip_mmc_ops = {
	.request		= artinchip_mmc_request,
	.pre_req		= artinchip_mmc_pre_req,
	.post_req		= artinchip_mmc_post_req,
	.set_ios		= artinchip_mmc_set_ios,
	.get_ro			= artinchip_mmc_get_ro,
	.get_cd			= artinchip_mmc_get_cd,
	.hw_reset               = artinchip_mmc_hw_reset,
	.enable_sdio_irq	= artinchip_mmc_enable_sdio_irq,
	.ack_sdio_irq		= artinchip_mmc_ack_sdio_irq,
	.execute_tuning		= artinchip_mmc_execute_tuning,
	.card_busy		= artinchip_mmc_card_busy,
	.start_signal_voltage_switch = artinchip_mmc_switch_voltage,
	.init_card		= artinchip_mmc_init_card,
	.prepare_hs400_tuning	= artinchip_mmc_prepare_hs400_tuning,
	.card_event		= aic_sdmc_card_event,
};

static void artinchip_mmc_request_end(struct artinchip_mmc *host, struct mmc_request *mrq)
	__releases(&host->lock)
	__acquires(&host->lock)
{
	struct artinchip_mmc_slot *slot;
	struct mmc_host	*prev_mmc = host->slot->mmc;

	WARN_ON(host->cmd || host->data);

	host->slot->mrq = NULL;
	host->mrq = NULL;
	if (!list_empty(&host->queue)) {
		slot = list_entry(host->queue.next,
				  struct artinchip_mmc_slot, queue_node);
		list_del(&slot->queue_node);
		dev_vdbg(host->dev, "list not empty: %s is next\n",
			 mmc_hostname(slot->mmc));
		host->state = STATE_SENDING_CMD;
		artinchip_mmc_start_request(host, slot);
	} else {
		dev_vdbg(host->dev, "list empty\n");

		if (host->state == STATE_SENDING_CMD11)
			host->state = STATE_WAITING_CMD11_DONE;
		else
			host->state = STATE_IDLE;
	}

	mmc_request_done(prev_mmc, mrq);
}

static int artinchip_mmc_command_complete(struct artinchip_mmc *host, struct mmc_command *cmd)
{
	u32 status = host->cmd_status;

	host->cmd_status = 0;

	/* Read the response from the card (up to 16 bytes) */
	if (cmd->flags & MMC_RSP_PRESENT) {
		if (cmd->flags & MMC_RSP_136) {
			cmd->resp[3] = mci_readl(host, SDMC_RESP0);
			cmd->resp[2] = mci_readl(host, SDMC_RESP1);
			cmd->resp[1] = mci_readl(host, SDMC_RESP2);
			cmd->resp[0] = mci_readl(host, SDMC_RESP3);
		} else {
			cmd->resp[0] = mci_readl(host, SDMC_RESP0);
			cmd->resp[1] = 0;
			cmd->resp[2] = 0;
			cmd->resp[3] = 0;
		}
	}
	if (status & SDMC_INT_RTO)
		cmd->error = -ETIMEDOUT;
	else if ((cmd->flags & MMC_RSP_CRC) && (status & SDMC_INT_RCRC))
		cmd->error = -EILSEQ;
	else if (status & SDMC_INT_RESP_ERR)
		cmd->error = -EIO;
	else
		cmd->error = 0;

	return cmd->error;
}

static int artinchip_mmc_data_complete(struct artinchip_mmc *host, struct mmc_data *data)
{
	u32 status = host->data_status;

	if (status & SDMC_DAT_ERROR_FLAGS) {
		if (status & SDMC_INT_DRTO) {
			data->error = -ETIMEDOUT;
		} else if (status & SDMC_INT_DCRC) {
			data->error = -EILSEQ;
		} else if (status & SDMC_INT_EBE) {
			if (host->dir_status ==
				ARTINCHIP_MMC_SEND_STATUS) {
				/*
				 * No data CRC status was returned.
				 * The number of bytes transferred
				 * will be exaggerated in PIO mode.
				 */
				data->bytes_xfered = 0;
				data->error = -ETIMEDOUT;
			} else if (host->dir_status ==
					ARTINCHIP_MMC_RECV_STATUS) {
				data->error = -EILSEQ;
			}
		} else {
			/* SDMC_INT_SBE is included */
			data->error = -EILSEQ;
		}

		dev_dbg(host->dev, "data error, status 0x%08x\n", status);

		/*
		 * After an error, there may be data lingering
		 * in the FIFO
		 */
		artinchip_mmc_reset(host);
	} else {
		data->bytes_xfered = data->blocks * data->blksz;
		data->error = 0;
	}
	return data->error;
}

static void artinchip_mmc_set_drto(struct artinchip_mmc *host)
{
	unsigned int drto_clks;
	unsigned int drto_div;
	unsigned int drto_ms;
	unsigned long irqflags;

	drto_clks = mci_readl(host, SDMC_TTMC) >> 8;
	drto_div = SDMC_GET_CLK_DIV((mci_readl(host, SDMC_CLKCTRL)));
	if (drto_div == 0)
		drto_div = 1;

	drto_ms = DIV_ROUND_UP_ULL((u64)MSEC_PER_SEC * drto_clks * drto_div,
				   host->sclk_rate);

	/* add a bit spare time */
	drto_ms += 10;

	spin_lock_irqsave(&host->irq_lock, irqflags);
	if (!test_bit(EVENT_DATA_COMPLETE, &host->pending_events))
		mod_timer(&host->dto_timer,
			  jiffies + msecs_to_jiffies(drto_ms));
	spin_unlock_irqrestore(&host->irq_lock, irqflags);
}

static bool artinchip_mmc_clear_pending_cmd_complete(struct artinchip_mmc *host)
{
	if (!test_bit(EVENT_CMD_COMPLETE, &host->pending_events))
		return false;

	/*
	 * Really be certain that the timer has stopped.  This is a bit of
	 * paranoia and could only really happen if we had really bad
	 * interrupt latency and the interrupt routine and timeout were
	 * running concurrently so that the del_timer() in the interrupt
	 * handler couldn't run.
	 */
	WARN_ON(del_timer_sync(&host->cto_timer));
	clear_bit(EVENT_CMD_COMPLETE, &host->pending_events);

	return true;
}

static bool artinchip_mmc_clear_pending_data_complete(struct artinchip_mmc *host)
{
	if (!test_bit(EVENT_DATA_COMPLETE, &host->pending_events))
		return false;

	/* Extra paranoia just like artinchip_mmc_clear_pending_cmd_complete() */
	WARN_ON(del_timer_sync(&host->dto_timer));
	clear_bit(EVENT_DATA_COMPLETE, &host->pending_events);

	return true;
}

static void artinchip_mmc_tasklet_func(unsigned long priv)
{
	struct artinchip_mmc *host = (struct artinchip_mmc *)priv;
	struct mmc_data	*data;
	struct mmc_command *cmd;
	struct mmc_request *mrq;
	enum artinchip_mmc_state state;
	enum artinchip_mmc_state prev_state;
	unsigned int err;

	spin_lock(&host->lock);

	state = host->state;
	data = host->data;
	mrq = host->mrq;

	do {
		prev_state = state;

		switch (state) {
		case STATE_IDLE:
		case STATE_WAITING_CMD11_DONE:
			break;

		case STATE_SENDING_CMD11:
		case STATE_SENDING_CMD:
			if (!artinchip_mmc_clear_pending_cmd_complete(host))
				break;

			cmd = host->cmd;
			host->cmd = NULL;
			set_bit(EVENT_CMD_COMPLETE, &host->completed_events);
			err = artinchip_mmc_command_complete(host, cmd);
			if (cmd == mrq->sbc && !err) {
				__artinchip_mmc_start_request(host, host->slot, cmd);
				goto unlock;
			}

			if (cmd->data && err) {
				/*
				 * During UHS tuning sequence, sending the stop
				 * command after the response CRC error would
				 * throw the system into a confused state
				 * causing all future tuning phases to report
				 * failure.
				 *
				 * In such case controller will move into a data
				 * transfer state after a response error or
				 * response CRC error. Let's let that finish
				 * before trying to send a stop, so we'll go to
				 * STATE_SENDING_DATA.
				 *
				 * Although letting the data transfer take place
				 * will waste a bit of time (we already know
				 * the command was bad), it can't cause any
				 * errors since it's possible it would have
				 * taken place anyway if this tasklet got
				 * delayed. Allowing the transfer to take place
				 * avoids races and keeps things simple.
				 */
				if (err != -ETIMEDOUT) {
					state = STATE_SENDING_DATA;
					continue;
				}

				artinchip_mmc_stop_dma(host);
				send_stop_abort(host, data);
				state = STATE_SENDING_STOP;
				break;
			}

			if (!cmd->data || err) {
				artinchip_mmc_request_end(host, mrq);
				goto unlock;
			}

			prev_state = STATE_SENDING_DATA;
			state = STATE_SENDING_DATA;
			fallthrough;

		case STATE_SENDING_DATA:
			/*
			 * We could get a data error and never a transfer
			 * complete so we'd better check for it here.
			 *
			 * Note that we don't really care if we also got a
			 * transfer complete; stopping the DMA and sending an
			 * abort won't hurt.
			 */
			if (test_and_clear_bit(EVENT_DATA_ERROR,
					       &host->pending_events)) {
				artinchip_mmc_stop_dma(host);
				if (!(host->data_status & (SDMC_INT_DRTO |
							   SDMC_INT_EBE)))
					send_stop_abort(host, data);
				state = STATE_DATA_ERROR;
				break;
			}

			if (!test_and_clear_bit(EVENT_XFER_COMPLETE,
						&host->pending_events)) {
				/*
				 * If all data-related interrupts don't come
				 * within the given time in reading data state.
				 */
				if (host->dir_status == ARTINCHIP_MMC_RECV_STATUS)
					artinchip_mmc_set_drto(host);
				break;
			}

			set_bit(EVENT_XFER_COMPLETE, &host->completed_events);

			/*
			 * Handle an EVENT_DATA_ERROR that might have shown up
			 * before the transfer completed.  This might not have
			 * been caught by the check above because the interrupt
			 * could have gone off between the previous check and
			 * the check for transfer complete.
			 *
			 * Technically this ought not be needed assuming we
			 * get a DATA_COMPLETE eventually (we'll notice the
			 * error and end the request), but it shouldn't hurt.
			 *
			 * This has the advantage of sending the stop command.
			 */
			if (test_and_clear_bit(EVENT_DATA_ERROR,
					       &host->pending_events)) {
				artinchip_mmc_stop_dma(host);
				if (!(host->data_status & (SDMC_INT_DRTO |
							   SDMC_INT_EBE)))
					send_stop_abort(host, data);
				state = STATE_DATA_ERROR;
				break;
			}
			prev_state = STATE_DATA_BUSY;
			state = STATE_DATA_BUSY;

			fallthrough;

		case STATE_DATA_BUSY:
			if (!artinchip_mmc_clear_pending_data_complete(host)) {
				/*
				 * If data error interrupt comes but data over
				 * interrupt doesn't come within the given time.
				 * in reading data state.
				 */
				if (host->dir_status == ARTINCHIP_MMC_RECV_STATUS)
					artinchip_mmc_set_drto(host);
				break;
			}

			host->data = NULL;
			set_bit(EVENT_DATA_COMPLETE, &host->completed_events);
			err = artinchip_mmc_data_complete(host, data);

			if (!err) {
				if (!data->stop || mrq->sbc) {
					if (mrq->sbc && data->stop)
						data->stop->error = 0;
					artinchip_mmc_request_end(host, mrq);
					goto unlock;
				}

				/* stop command for open-ended transfer*/
				if (data->stop)
					send_stop_abort(host, data);
			} else {
				/*
				 * If we don't have a command complete now we'll
				 * never get one since we just reset everything;
				 * better end the request.
				 *
				 * If we do have a command complete we'll fall
				 * through to the SENDING_STOP command and
				 * everything will be peachy keen.
				 */
				if (!test_bit(EVENT_CMD_COMPLETE,
					      &host->pending_events)) {
					host->cmd = NULL;
					artinchip_mmc_request_end(host, mrq);
					goto unlock;
				}
			}

			/*
			 * If err has non-zero,
			 * stop-abort command has been already issued.
			 */
			prev_state = STATE_SENDING_STOP;
			state = STATE_SENDING_STOP;

			fallthrough;

		case STATE_SENDING_STOP:
			if (!artinchip_mmc_clear_pending_cmd_complete(host))
				break;

			/* CMD error in data command */
			if (mrq->cmd->error && mrq->data)
				artinchip_mmc_reset(host);

			host->cmd = NULL;
			host->data = NULL;

			if (!mrq->sbc && mrq->stop)
				artinchip_mmc_command_complete(host, mrq->stop);
			else
				host->cmd_status = 0;

			artinchip_mmc_request_end(host, mrq);
			goto unlock;

		case STATE_DATA_ERROR:
			if (!test_and_clear_bit(EVENT_XFER_COMPLETE,
						&host->pending_events))
				break;

			state = STATE_DATA_BUSY;
			break;
		}
	} while (state != prev_state);

	host->state = state;
unlock:
	spin_unlock(&host->lock);
}

/* pull first bytes from part_buf, only use during pull */
static int artinchip_mmc_pull_part_bytes(struct artinchip_mmc *host, void *buf, int cnt)
{
	cnt = min_t(int, cnt, host->part_buf_count);
	if (cnt) {
		memcpy(buf, (void *)&host->part_buf + host->part_buf_start,
		       cnt);
		host->part_buf_count -= cnt;
		host->part_buf_start += cnt;
	}
	return cnt;
}

/* pull final bytes from the part_buf, assuming it's just been filled */
static void artinchip_mmc_pull_final_bytes(struct artinchip_mmc *host, void *buf, int cnt)
{
	memcpy(buf, &host->part_buf, cnt);
	host->part_buf_start = cnt;
	host->part_buf_count = (1 << host->data_shift) - cnt;
}

static void artinchip_mmc_fifo_write_part(struct artinchip_mmc *host)
{
	switch (host->data_width) {
	case DATA_WIDTH_16BIT:
		mci_fifo_writew(host->fifo_reg, host->part_buf16);
		break;
	case DATA_WIDTH_32BIT:
		mci_fifo_writel(host->fifo_reg, host->part_buf32);
		break;
	case DATA_WIDTH_64BIT:
		mci_fifo_writeq(host->fifo_reg, host->part_buf);
		break;
	}
}

static void artinchip_mmc_fifo_read_part(struct artinchip_mmc *host)
{
	switch (host->data_width) {
	case DATA_WIDTH_16BIT:
		host->part_buf16 = mci_fifo_readw(host->fifo_reg);
		break;
	case DATA_WIDTH_32BIT:
		host->part_buf32 = mci_fifo_readl(host->fifo_reg);
		break;
	case DATA_WIDTH_64BIT:
		host->part_buf = mci_fifo_readq(host->fifo_reg);
		break;
	}
}

#ifndef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
static int artinchip_mmc_write_fifo_aligned(struct artinchip_mmc *host,
									void *buf,
									int cnt,
									int cnt_index)
{
	int i;
	int ret_len = 0;

	if (host->data_width == DATA_WIDTH_16BIT) {
		u16 aligned_buf_16[64];
		int len = min(cnt & -cnt_index, (int)sizeof(aligned_buf_16));
		int items = len >> host->data_shift;

		ret_len = len;
		/* memcpy from input buffer into aligned buffer */
		memcpy(aligned_buf_16, buf, len);
		/* push data from aligned buffer into fifo */
		for (i = 0; i < items; ++i)
			mci_fifo_writew(host->fifo_reg, aligned_buf_16[i]);
	} else if (host->data_width == DATA_WIDTH_32BIT) {
		u32 aligned_buf_32[32];
		int len = min(cnt & -cnt_index, (int)sizeof(aligned_buf_32));
		int items = len >> host->data_shift;

		ret_len = len;
		/* memcpy from input buffer into aligned buffer */
		memcpy(aligned_buf_32, buf, len);
		/* push data from aligned buffer into fifo */
		for (i = 0; i < items; ++i)
			mci_fifo_writel(host->fifo_reg, aligned_buf_32[i]);
	} else if (host->data_width == DATA_WIDTH_64BIT) {
		u64 aligned_buf_64[16];
		int len = min(cnt & -cnt_index, (int)sizeof(aligned_buf_64));
		int items = len >> host->data_shift;

		ret_len = len;
		/* memcpy from input buffer into aligned buffer */
		memcpy(aligned_buf_64, buf, len);
		/* push data from aligned buffer into fifo */
		for (i = 0; i < items; ++i)
			mci_fifo_writeq(host->fifo_reg, aligned_buf_64[i]);
	}
	return ret_len;
}

static int artinchip_mmc_read_fifo_aligned(struct artinchip_mmc *host,
									void *buf,
									int cnt,
									int cnt_index)
{
	int i;
	int ret_len = 0;

	if (host->data_width == DATA_WIDTH_16BIT) {
		u16 aligned_buf[64];
		int len = min(cnt & (-cnt_index), (int)sizeof(aligned_buf));
		int items = len >> host->data_shift;

		ret_len = len;
		for (i = 0; i < items; ++i)
			aligned_buf[i] = mci_fifo_readw(host->fifo_reg);
		/* memcpy from aligned buffer into output buffer */
		memcpy(buf, aligned_buf, len);
	} else if (host->data_width == DATA_WIDTH_32BIT) {
		u32 aligned_buf[32];
		int len = min(cnt & (-cnt_index), (int)sizeof(aligned_buf));
		int items = len >> host->data_shift;

		ret_len = len;
		for (i = 0; i < items; ++i)
			aligned_buf[i] = mci_fifo_readl(host->fifo_reg);
		/* memcpy from aligned buffer into output buffer */
		memcpy(buf, aligned_buf, len);
	} else if (host->data_width == DATA_WIDTH_64BIT) {
		u64 aligned_buf[16];
		int len = min(cnt & (-cnt_index), (int)sizeof(aligned_buf));
		int items = len >> host->data_shift;

		ret_len = len;
		for (i = 0; i < items; ++i)
			aligned_buf[i] = mci_fifo_readq(host->fifo_reg);
		/* memcpy from aligned buffer into output buffer */
		memcpy(buf, aligned_buf, len);
	}
	return ret_len;
}
#endif

static int artinchip_mmc_write_fifo(struct artinchip_mmc *host,
							void *buf,
							int cnt,
							int cnt_index)
{
	int cnt_temp = cnt;

	if (host->data_width == DATA_WIDTH_16BIT) {
		u16 *pdata = buf;

		for (; cnt_temp >= cnt_index; cnt_temp -= cnt_index)
			mci_fifo_writew(host->fifo_reg, *pdata++);
		buf = pdata;
	} else if (host->data_width == DATA_WIDTH_32BIT) {
		u32 *pdata = buf;

		for (; cnt_temp >= cnt_index; cnt_temp -= cnt_index)
			mci_fifo_writel(host->fifo_reg, *pdata++);
		buf = pdata;
	} else if (host->data_width == DATA_WIDTH_64BIT) {
		u64 *pdata = buf;

		for (; cnt_temp >= cnt_index; cnt_temp -= cnt_index)
			mci_fifo_writeq(host->fifo_reg, *pdata++);
		buf = pdata;
	}
	return cnt_temp;
}

static int artinchip_mmc_read_fifo(struct artinchip_mmc *host,
							void *buf,
							int cnt,
							int cnt_index)
{
	int cnt_temp = cnt;

	if (host->data_width == DATA_WIDTH_16BIT) {
		u16 *pdata = buf;

		for (; cnt_temp >= cnt_index; cnt_temp -= cnt_index)
			*pdata++ = mci_fifo_readw(host->fifo_reg);
		buf = pdata;
	} else if (host->data_width == DATA_WIDTH_32BIT) {
		u32 *pdata = buf;

		for (; cnt_temp >= cnt_index; cnt_temp -= cnt_index)
			*pdata++ = mci_fifo_readl(host->fifo_reg);
		buf = pdata;
	} else if (host->data_width == DATA_WIDTH_64BIT) {
		u64 *pdata = buf;

		for (; cnt_temp >= cnt_index; cnt_temp -= cnt_index)
			*pdata++ = mci_fifo_readq(host->fifo_reg);
		buf = pdata;
	}
	return cnt_temp;
}

static void artinchip_mmc_push_data(struct artinchip_mmc *host, void *buf, int cnt)
{
	struct mmc_data *data = host->data;
	int init_cnt = cnt;
	int cnt_index;
	int temp_len = 0;

	cnt_index = 0x1 << host->data_shift;

	/* try and push anything in the part_buf */
	if (unlikely(host->part_buf_count)) {
		/* append bytes to part_buf, only use during push */
		int len = min(cnt, (1 << host->data_shift) - host->part_buf_count);

		memcpy((void *)&host->part_buf + host->part_buf_count, buf, cnt);
		host->part_buf_count += cnt;
		buf += len;
		cnt -= len;

		if (host->part_buf_count == cnt_index) {
			artinchip_mmc_fifo_write_part(host);
			host->part_buf_count = 0;
		}
	}
#ifndef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
	if (unlikely((unsigned long)buf & (0x7 >> (3 - host->data_shift)))) {
		while (cnt >= cnt_index) {
			temp_len = artinchip_mmc_write_fifo_aligned(host, buf, cnt, cnt_index);
			buf += temp_len;
			cnt -= temp_len;
		}
	} else
#endif
	{
		cnt = artinchip_mmc_write_fifo(host, buf, cnt, cnt_index);
	}
	/* put anything remaining in the part_buf */
	if (cnt) {
		/* push final bytes to part_buf, only use during push */
		memcpy((void *)&host->part_buf, buf, cnt);
		host->part_buf_count = cnt;
		 /* Push data if we have reached the expected data length */
		if ((data->bytes_xfered + init_cnt) ==
			(data->blksz * data->blocks))
			artinchip_mmc_fifo_write_part(host);
	}
}

static void artinchip_mmc_pull_data(struct artinchip_mmc *host, void *buf, int cnt)
{
	int len;
	int cnt_index;
	int temp_len = 0;

	/* get remaining partial bytes */
	len = artinchip_mmc_pull_part_bytes(host, buf, cnt);
	if (unlikely(len == cnt))
		return;
	buf += len;
	cnt -= len;

	cnt_index = 0x1 << host->data_shift;
	/* get the rest of the data */
#ifndef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
		if (unlikely((unsigned long)buf & (0x7 >> (3 - host->data_shift)))) {
			while (cnt >= cnt_index) {
				/* pull data from fifo into aligned buffer */
				temp_len = artinchip_mmc_read_fifo_aligned(host,
						buf, cnt, cnt_index);
				buf += temp_len;
				cnt -= temp_len;
			}
		} else
#endif
		{
			cnt = artinchip_mmc_read_fifo(host, buf, cnt, cnt_index);
		}
		if (cnt) {
			artinchip_mmc_fifo_read_part(host);
			artinchip_mmc_pull_final_bytes(host, buf, cnt);
		}
}

static void artinchip_mmc_read_data_pio(struct artinchip_mmc *host, bool dto)
{
	struct sg_mapping_iter *sg_miter = &host->sg_miter;
	void *buf;
	unsigned int offset;
	struct mmc_data	*data = host->data;
	int shift = host->data_shift;
	u32 status;
	unsigned int len;
	unsigned int remain, fcnt;

	do {
		if (!sg_miter_next(sg_miter))
			goto done;

		host->sg = sg_miter->piter.sg;
		buf = sg_miter->addr;
		remain = sg_miter->length;
		offset = 0;
		do {
			fcnt = (SDMC_CTRST_FCNT(mci_readl(host, SDMC_CTRST))
					<< shift) + host->part_buf_count;
			len = min(remain, fcnt);
			if (!len)
				break;
			artinchip_mmc_pull_data(host, (void *)(buf + offset), len);
			data->bytes_xfered += len;
			offset += len;
			remain -= len;
		} while (remain);
		sg_miter->consumed = offset;
		status = mci_readl(host, SDMC_INTST);
		mci_writel(host, SDMC_OINTST, SDMC_INT_RXDR);
	/* if the RXDR is ready read again */
	} while ((status & SDMC_INT_RXDR) ||
		 (dto && SDMC_CTRST_FCNT(mci_readl(host, SDMC_CTRST))));
	if (!remain) {
		if (!sg_miter_next(sg_miter))
			goto done;
		sg_miter->consumed = 0;
	}
	sg_miter_stop(sg_miter);
	return;

done:
	sg_miter_stop(sg_miter);
	host->sg = NULL;
	smp_wmb(); /* drain writebuffer */
	set_bit(EVENT_XFER_COMPLETE, &host->pending_events);
}

static void artinchip_mmc_write_data_pio(struct artinchip_mmc *host)
{
	struct sg_mapping_iter *sg_miter = &host->sg_miter;
	void *buf;
	unsigned int offset;
	struct mmc_data	*data = host->data;
	int shift = host->data_shift;
	u32 status;
	unsigned int len;
	unsigned int fifo_depth = host->fifo_depth;
	unsigned int remain, fcnt;

	do {
		if (!sg_miter_next(sg_miter))
			goto done;

		host->sg = sg_miter->piter.sg;
		buf = sg_miter->addr;
		remain = sg_miter->length;
		offset = 0;

		do {
			fcnt = ((fifo_depth -
				 SDMC_CTRST_FCNT(mci_readl(host, SDMC_CTRST)))
					<< shift) - host->part_buf_count;
			len = min(remain, fcnt);
			if (!len)
				break;
			artinchip_mmc_push_data(host, (void *)(buf + offset), len);
			data->bytes_xfered += len;
			offset += len;
			remain -= len;
		} while (remain);

		sg_miter->consumed = offset;
		status = mci_readl(host, SDMC_INTST);
		mci_writel(host, SDMC_OINTST, SDMC_INT_TXDR);
	} while (status & SDMC_INT_TXDR); /* if TXDR write again */

	if (!remain) {
		if (!sg_miter_next(sg_miter))
			goto done;
		sg_miter->consumed = 0;
	}
	sg_miter_stop(sg_miter);
	return;

done:
	sg_miter_stop(sg_miter);
	host->sg = NULL;
	smp_wmb(); /* drain writebuffer */
	set_bit(EVENT_XFER_COMPLETE, &host->pending_events);
}

static void artinchip_mmc_cmd_interrupt(struct artinchip_mmc *host, u32 status)
{
	del_timer(&host->cto_timer);

	if (!host->cmd_status)
		host->cmd_status = status;

	smp_wmb(); /* drain writebuffer */

	set_bit(EVENT_CMD_COMPLETE, &host->pending_events);
	tasklet_schedule(&host->tasklet);
}

static void artinchip_mmc_handle_cd(struct artinchip_mmc *host)
{
	struct artinchip_mmc_slot *slot = host->slot;

	if (slot->mmc->ops->card_event)
		slot->mmc->ops->card_event(slot->mmc);
	mmc_detect_change(slot->mmc,
		msecs_to_jiffies(host->pdata->detect_delay_ms));
}

static irqreturn_t artinchip_mmc_interrupt(int irq, void *dev_id)
{
	struct artinchip_mmc *host = dev_id;
	u32 pending;
	struct artinchip_mmc_slot *slot = host->slot;
	unsigned long irqflags;

	pending = mci_readl(host, SDMC_INTST); /* read-only mask reg */
	if (pending) {
		if (pending & SDMC_INT_CD) {
			mci_writel(host, SDMC_OINTST, SDMC_INT_CD);
			artinchip_mmc_handle_cd(host);
		}

		/* Check volt switch first, since it can look like an error */
		if ((host->state == STATE_SENDING_CMD11) &&
		    (pending & SDMC_INT_VOLT_SWITCH)) {
			mci_writel(host, SDMC_OINTST, SDMC_INT_VOLT_SWITCH);
			pending &= ~SDMC_INT_VOLT_SWITCH;

			/*
			 * Hold the lock; we know cmd11_timer can't be kicked
			 * off after the lock is released, so safe to delete.
			 */
			spin_lock_irqsave(&host->irq_lock, irqflags);
			artinchip_mmc_cmd_interrupt(host, pending);
			spin_unlock_irqrestore(&host->irq_lock, irqflags);

			del_timer(&host->cmd11_timer);
		}

		if (pending & SDMC_CMD_ERROR_FLAGS) {
			spin_lock_irqsave(&host->irq_lock, irqflags);

			del_timer(&host->cto_timer);
			mci_writel(host, SDMC_OINTST, SDMC_CMD_ERROR_FLAGS);
			host->cmd_status = pending;
			smp_wmb(); /* drain writebuffer */
			set_bit(EVENT_CMD_COMPLETE, &host->pending_events);

			spin_unlock_irqrestore(&host->irq_lock, irqflags);
		}

		if (pending & SDMC_DAT_ERROR_FLAGS) {
			/* if there is an error report DATA_ERROR */
			mci_writel(host, SDMC_OINTST, SDMC_DAT_ERROR_FLAGS);
			host->data_status = pending;
			smp_wmb(); /* drain writebuffer */
			set_bit(EVENT_DATA_ERROR, &host->pending_events);
			tasklet_schedule(&host->tasklet);
		}

		if (pending & SDMC_INT_DAT_DONE) {
			spin_lock_irqsave(&host->irq_lock, irqflags);
			del_timer(&host->dto_timer);

			mci_writel(host, SDMC_OINTST, SDMC_INT_DAT_DONE);
			if (!host->data_status)
				host->data_status = pending;
			smp_wmb(); /* drain writebuffer */
			if (host->dir_status == ARTINCHIP_MMC_RECV_STATUS) {
				if (host->sg != NULL)
					artinchip_mmc_read_data_pio(host, true);
			}
			set_bit(EVENT_DATA_COMPLETE, &host->pending_events);
			tasklet_schedule(&host->tasklet);

			spin_unlock_irqrestore(&host->irq_lock, irqflags);
		}

		if (pending & SDMC_INT_RXDR) {
			mci_writel(host, SDMC_OINTST, SDMC_INT_RXDR);
			if (host->dir_status == ARTINCHIP_MMC_RECV_STATUS && host->sg)
				artinchip_mmc_read_data_pio(host, false);
		}

		if (pending & SDMC_INT_TXDR) {
			mci_writel(host, SDMC_OINTST, SDMC_INT_TXDR);
			if (host->dir_status == ARTINCHIP_MMC_SEND_STATUS && host->sg)
				artinchip_mmc_write_data_pio(host);
		}

		if (pending & SDMC_INT_CMD_DONE) {
			spin_lock_irqsave(&host->irq_lock, irqflags);

			mci_writel(host, SDMC_OINTST, SDMC_INT_CMD_DONE);
			artinchip_mmc_cmd_interrupt(host, pending);

			spin_unlock_irqrestore(&host->irq_lock, irqflags);
		}

		if (pending & SDMC_INT_SDIO) {
			mci_writel(host, SDMC_OINTST,
				   SDMC_INT_SDIO);
			__artinchip_mmc_enable_sdio_irq(slot, 0);
			sdio_signal_irq(slot->mmc);
		}

	}

	if (!host->use_dma)
		return IRQ_HANDLED;

	/* Handle IDMA interrupts */
	pending = mci_readl(host, SDMC_IDMAST);
	if (pending & (SDMC_IDMAC_INT_TI | SDMC_IDMAC_INT_RI)) {
		mci_writel(host, SDMC_IDMAST, SDMC_IDMAC_INT_TI |
						SDMC_IDMAC_INT_RI);
		mci_writel(host, SDMC_IDMAST, SDMC_IDMAC_INT_NIS);
		if (!test_bit(EVENT_DATA_ERROR, &host->pending_events))
			host->dma_ops->complete((void *)host);
	}

	return IRQ_HANDLED;
}

static int artinchip_mmc_init_slot_caps(struct artinchip_mmc_slot *slot)
{
	struct artinchip_mmc *host = slot->host;
	const struct artinchip_mmc_drv_data *drv_data = host->drv_data;
	struct mmc_host *mmc = slot->mmc;
	int ctrl_id;

	if (host->pdata->caps)
		mmc->caps = host->pdata->caps;

	if (host->pdata->pm_caps)
		mmc->pm_caps = host->pdata->pm_caps;

	if (host->dev->of_node) {
		ctrl_id = of_alias_get_id(host->dev->of_node, "mshc");
		if (ctrl_id < 0)
			ctrl_id = 0;
	} else {
		ctrl_id = to_platform_device(host->dev)->id;
	}

	if (drv_data && drv_data->caps) {
		if (ctrl_id >= drv_data->num_caps) {
			dev_err(host->dev, "invalid controller id %d\n",
				ctrl_id);
			return -EINVAL;
		}
		mmc->caps |= drv_data->caps[ctrl_id];
	}

	if (host->pdata->caps2)
		mmc->caps2 = host->pdata->caps2;

	mmc->f_min = ARTINCHIP_MMC_FREQ_MIN;
	if (!mmc->f_max)
		mmc->f_max = ARTINCHIP_MMC_FREQ_MAX;

	/* Process SDIO IRQs through the sdio_irq_work. */
	if (mmc->caps & MMC_CAP_SDIO_IRQ)
		mmc->caps2 |= MMC_CAP2_SDIO_IRQ_NOTHREAD;

	return 0;
}

static int artinchip_mmc_init_slot(struct artinchip_mmc *host)
{
	struct mmc_host *mmc;
	struct artinchip_mmc_slot *slot;
	int ret;

	mmc = mmc_alloc_host(sizeof(struct artinchip_mmc_slot), host->dev);
	if (!mmc)
		return -ENOMEM;

	slot = mmc_priv(mmc);
	slot->mmc = mmc;
	slot->host = host;
	host->slot = slot;

	mmc->ops = &artinchip_mmc_ops;

	/*if there are external regulators, get them*/
	ret = mmc_regulator_get_supply(mmc);
	if (ret)
		goto err_host_allocated;

	if (!mmc->ocr_avail)
		mmc->ocr_avail = MMC_VDD_32_33 | MMC_VDD_33_34;

	ret = mmc_of_parse(mmc);
	if (ret)
		goto err_host_allocated;

	ret = artinchip_mmc_init_slot_caps(slot);
	if (ret)
		goto err_host_allocated;

	/* Useful defaults if platform data is unset. */
	if (host->use_dma) {
		mmc->max_segs = host->ring_size;
		mmc->max_blk_size = 65535;
		mmc->max_seg_size = 0x1000;
		mmc->max_req_size = mmc->max_seg_size * host->ring_size;
		mmc->max_blk_count = mmc->max_req_size / 512;
	} else {
		/* TRANS_MODE_PIO */
		mmc->max_segs = 64;
		mmc->max_blk_size = 65535; /* SDMC_BLKSIZ is 16 bits */
		mmc->max_blk_count = 512;
		mmc->max_req_size = mmc->max_blk_size *
				    mmc->max_blk_count;
		mmc->max_seg_size = mmc->max_req_size;
	}

	artinchip_mmc_get_cd(mmc);

	ret = mmc_add_host(mmc);
	if (ret)
		goto err_host_allocated;

#if defined(CONFIG_DEBUG_FS)
	artinchip_mmc_init_debugfs(slot);
#endif

	return 0;

err_host_allocated:
	mmc_free_host(mmc);
	return ret;
}

static void artinchip_mmc_cleanup_slot(struct artinchip_mmc_slot *slot)
{
	/* Debugfs stuff is cleaned up by mmc core */
	mmc_remove_host(slot->mmc);
	slot->host->slot = NULL;
	mmc_free_host(slot->mmc);
}

static void artinchip_mmc_init_dma(struct artinchip_mmc *host)
{
	u32 dma_mode;
	struct device *dev = host->dev;

	/*
	 * Check transfer mode from SDMC_HINFO[17:16]
	 * 2b'00: No DMA Interface -> Actually means using Internal DMA block
	 * 2b'11: Non AIC DMA Interface -> pio only
	 * we currently do not support external dma
	 */
	dma_mode = SDMC_HINFO_TRANS_MODE(mci_readl(host, SDMC_HINFO));
	if (dma_mode == SDMC_HINFO_IF_IDMA)
		host->use_dma = true;
	else if (dma_mode == SDMC_HINFO_IF_NODMA)
		goto no_dma;

	/* Determine which DMA interface to use */
	if (host->use_dma) {
		/* Alloc memory for sg translation */
		host->sg_cpu = dmam_alloc_coherent(dev, DESC_RING_BUF_SZ,
						   &host->sg_dma, GFP_KERNEL);
		if (!host->sg_cpu) {
			dev_err(dev, "Failed to alloc DMA memory\n");
			goto no_dma;
		}

		host->dma_ops = &artinchip_mmc_idmac_ops;
		dev_dbg(dev, "Using internal DMA controller.\n");
	}

	if (host->dma_ops->init && host->dma_ops->start &&
	    host->dma_ops->stop && host->dma_ops->cleanup) {
		if (host->dma_ops->init(host)) {
			dev_err(dev, "Failed to init DMA Controller.\n");
			goto no_dma;
		}
	} else {
		dev_err(dev, "DMA ops is unavailable.\n");
		goto no_dma;
	}

	return;

no_dma:
	dev_info(host->dev, "Using PIO mode.\n");
	host->use_dma = false;
}

static void artinchip_mmc_cmd11_timer(struct timer_list *t)
{
	struct artinchip_mmc *host = from_timer(host, t, cmd11_timer);

	if (host->state != STATE_SENDING_CMD11) {
		dev_warn(host->dev, "Unexpected CMD11 timeout\n");
		return;
	}

	host->cmd_status = SDMC_INT_RTO;
	set_bit(EVENT_CMD_COMPLETE, &host->pending_events);
	tasklet_schedule(&host->tasklet);
}

static void artinchip_mmc_cto_timer(struct timer_list *t)
{
	struct artinchip_mmc *host = from_timer(host, t, cto_timer);
	unsigned long irqflags;
	u32 pending;

	spin_lock_irqsave(&host->irq_lock, irqflags);

	/*
	 * If somehow we have very bad interrupt latency it's remotely possible
	 * that the timer could fire while the interrupt is still pending or
	 * while the interrupt is midway through running.  Let's be paranoid
	 * and detect those two cases.  Note that this is paranoia is somewhat
	 * justified because in this function we don't actually cancel the
	 * pending command in the controller--we just assume it will never come.
	 */
	pending = mci_readl(host, SDMC_INTST); /* read-only mask reg */
	if (pending & (SDMC_CMD_ERROR_FLAGS | SDMC_INT_CMD_DONE)) {
		/* The interrupt should fire; no need to act but we can warn */
		dev_warn(host->dev, "Unexpected interrupt latency\n");
		goto exit;
	}
	if (test_bit(EVENT_CMD_COMPLETE, &host->pending_events)) {
		/* Presumably interrupt handler couldn't delete the timer */
		dev_warn(host->dev, "CTO timeout when already completed\n");
		goto exit;
	}

	/*
	 * Continued paranoia to make sure we're in the state we expect.
	 * This paranoia isn't really justified but it seems good to be safe.
	 */
	switch (host->state) {
	case STATE_SENDING_CMD11:
	case STATE_SENDING_CMD:
	case STATE_SENDING_STOP:
		/*
		 * If CMD_DONE interrupt does NOT come in sending command
		 * state, we should notify the driver to terminate current
		 * transfer and report a command timeout to the core.
		 */
		host->cmd_status = SDMC_INT_RTO;
		set_bit(EVENT_CMD_COMPLETE, &host->pending_events);
		tasklet_schedule(&host->tasklet);
		break;
	default:
		dev_warn(host->dev, "Unexpected command timeout, state %d\n",
			 host->state);
		break;
	}

exit:
	spin_unlock_irqrestore(&host->irq_lock, irqflags);
}

static void artinchip_mmc_dto_timer(struct timer_list *t)
{
	struct artinchip_mmc *host = from_timer(host, t, dto_timer);
	unsigned long irqflags;
	u32 pending;

	spin_lock_irqsave(&host->irq_lock, irqflags);

	/*
	 * The DTO timer is much longer than the CTO timer, so it's even less
	 * likely that we'll these cases, but it pays to be paranoid.
	 */
	pending = mci_readl(host, SDMC_INTST); /* read-only mask reg */
	if (pending & SDMC_INT_DAT_DONE) {
		/* The interrupt should fire; no need to act but we can warn */
		dev_warn(host->dev, "Unexpected data interrupt latency\n");
		goto exit;
	}
	if (test_bit(EVENT_DATA_COMPLETE, &host->pending_events)) {
		/* Presumably interrupt handler couldn't delete the timer */
		dev_warn(host->dev, "DTO timeout when already completed\n");
		goto exit;
	}

	/*
	 * Continued paranoia to make sure we're in the state we expect.
	 * This paranoia isn't really justified but it seems good to be safe.
	 */
	switch (host->state) {
	case STATE_SENDING_DATA:
	case STATE_DATA_BUSY:
		/*
		 * If DTO interrupt does NOT come in sending data state,
		 * we should notify the driver to terminate current transfer
		 * and report a data timeout to the core.
		 */
		host->data_status = SDMC_INT_DRTO;
		set_bit(EVENT_DATA_ERROR, &host->pending_events);
		set_bit(EVENT_DATA_COMPLETE, &host->pending_events);
		tasklet_schedule(&host->tasklet);
		break;
	default:
		dev_warn(host->dev, "Unexpected data timeout, state %d\n",
			 host->state);
		break;
	}

exit:
	spin_unlock_irqrestore(&host->irq_lock, irqflags);
}

#ifdef CONFIG_OF
static struct artinchip_mmc_board *artinchip_mmc_parse_dt(struct artinchip_mmc *host)
{
	struct artinchip_mmc_board *pdata;
	struct device *dev = host->dev;
	const struct artinchip_mmc_drv_data *drv_data = host->drv_data;
	int ret;
	u32 val;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	if (device_property_read_u32(dev, "aic,fifo-depth", &pdata->fifo_depth))
		dev_info(dev,
			 "fifo-depth property not found\n");

	if (device_property_present(dev, "aic,fifo-watermark-aligned"))
		host->wm_aligned = true;

	if (!device_property_read_u32(dev, "max-frequency", &val))
		pdata->sclk_rate = val;

	ret = device_property_read_u32(dev, "aic,sample-phase", &val);
	if (!ret)
		host->sample_phase = val;
	ret = device_property_read_u32(dev, "aic,sample-delay", &val);
	if (!ret)
		host->sample_delay = val;
	ret = device_property_read_u32(dev, "aic,driver-phase", &val);
	if (!ret)
		host->driver_phase = val;
	ret = device_property_read_u32(dev, "aic,driver-delay", &val);
	if (!ret)
		host->driver_delay = val;

	pdata->detect_delay_ms = 200;

	if (drv_data && drv_data->parse_dt) {
		ret = drv_data->parse_dt(host);
		if (ret)
			return ERR_PTR(ret);
	}

	return pdata;
}

#else /* CONFIG_OF */
static struct artinchip_mmc_board *artinchip_mmc_parse_dt(struct artinchip_mmc *host)
{
	return ERR_PTR(-EINVAL);
}
#endif /* CONFIG_OF */

static void artinchip_mmc_enable_cd(struct artinchip_mmc *host)
{
	unsigned long irqflags;
	/*
	 * No need for CD if all slots have a non-error GPIO
	 * as well as broken card detection is found.
	 */
	if (host->slot->mmc->caps & MMC_CAP_NEEDS_POLL)
		return;

	if (mmc_gpio_get_cd(host->slot->mmc) < 0) {
		spin_lock_irqsave(&host->irq_lock, irqflags);
		sdmc_reg_setbit(host, SDMC_INTEN, SDMC_INT_CD);
		spin_unlock_irqrestore(&host->irq_lock, irqflags);
	}
}

static int artinchip_mmc_clk_enable(struct artinchip_mmc *host)
{
	int ret = 0;
	struct device *dev = host->dev;

	host->hif_clk = devm_clk_get(dev, NULL);
	if (IS_ERR(host->hif_clk)) {
		dev_err(dev, "SDMC host clock is not available\n");
		host->sclk_rate = host->pdata->sclk_rate / 2;
		goto err;
	}

	ret = clk_prepare_enable(host->hif_clk);
	if (ret) {
		dev_err(dev, "Failed to enable SDMC Host clock\n");
		goto err;
	}

	if (host->pdata->sclk_rate) {
		ret = clk_set_rate(host->hif_clk, host->pdata->sclk_rate);
		if (ret)
			dev_warn(dev, "Unable to set SDMC clk rate to %uHz\n",
				 host->pdata->sclk_rate);
	}
	host->sclk_rate = clk_get_rate(host->hif_clk) / 2;
	if (!host->sclk_rate) {
		dev_err(dev, "Platform must supply SDMC clk\n");
		goto err;
	}

	host->reset = devm_reset_control_get_optional_exclusive(dev, NULL);
	if (IS_ERR(host->reset)) {
		dev_err(dev, "Failed to find reset of SDMC Host\n");
		goto err;
	}
	reset_control_assert(host->reset);
	usleep_range(10, 50);
	reset_control_deassert(host->reset);

	ret = mci_readl(host, SDMC_DLYCTRL);
	ret &= ~SDMC_DLYCTRL_CLK_DRV_PHA_MASK;
	ret &= ~SDMC_DLYCTRL_CLK_SMP_PHA_MASK;
	ret &= ~SDMC_DLYCTRL_CLK_DRV_DLY_MASK;
	ret &= ~SDMC_DLYCTRL_CLK_SMP_DLY_MASK;
	mci_writel(host, SDMC_DLYCTRL, ret |
		   host->driver_phase << SDMC_DLYCTRL_CLK_DRV_PHA_SHIFT |
		   host->driver_delay << SDMC_DLYCTRL_CLK_DRV_DLY_SHIFT |
		   host->sample_phase << SDMC_DLYCTRL_CLK_SMP_PHA_SHIFT |
		   host->sample_delay << SDMC_DLYCTRL_CLK_SMP_DLY_SHIFT);
	return 0;
err:
	return -ENODEV;
}

int artinchip_mmc_probe(struct artinchip_mmc *host)
{
	int width, i, ret = 0;
	u32 fifo_size;

	if (!host->pdata) {
		host->pdata = artinchip_mmc_parse_dt(host);
		if (IS_ERR(host->pdata))
			return dev_err_probe(host->dev, PTR_ERR(host->pdata),
					     "platform data not available\n");
	}

	ret = artinchip_mmc_clk_enable(host);
	if (ret)
		goto err_clk;

	timer_setup(&host->cmd11_timer, artinchip_mmc_cmd11_timer, 0);
	timer_setup(&host->cto_timer, artinchip_mmc_cto_timer, 0);
	timer_setup(&host->dto_timer, artinchip_mmc_dto_timer, 0);

	spin_lock_init(&host->lock);
	spin_lock_init(&host->irq_lock);
	INIT_LIST_HEAD(&host->queue);

	/*
	 * Get the host data width -
	 * assumes that SDMC_HINFO has been set with the correct values.
	 */
	i = SDMC_HINFO_HDATA_WIDTH(mci_readl(host, SDMC_HINFO));
	if (!i) {
		host->data_width = DATA_WIDTH_16BIT;
		width = 16;
		host->data_shift = 1;
	} else if (i == 2) {
		host->data_width = DATA_WIDTH_64BIT;
		width = 64;
		host->data_shift = 3;
	} else {
		/* Check for a reserved value, and warn if it is */
		WARN((i != 1),
		     "SDMC_HINFO reports a reserved host data width!\n"
		     "Defaulting to 32-bit access.\n");
		host->data_width = DATA_WIDTH_32BIT;
		width = 32;
		host->data_shift = 2;
	}

	/* Reset all blocks */
	if (!artinchip_mmc_ctrl_reset(host, SDMC_HCTRL1_RESET_ALL)) {
		ret = -ENODEV;
		goto err_clk;
	}

	host->dma_ops = host->pdata->dma_ops;
	artinchip_mmc_init_dma(host);

	/* Clear the interrupts for the host controller */
	mci_writel(host, SDMC_OINTST, 0xFFFFFFFF);
	mci_writel(host, SDMC_INTEN, 0); /* disable all mmc interrupt first */

	/* Put in max timeout */
	mci_writel(host, SDMC_TTMC, 0xFFFFFFFF);

	/*
	 * FIFO threshold settings  RxMark  = fifo_size / 2 - 1,
	 *                          Tx Mark = fifo_size / 2 DMA Size = 8
	 */
	if (!host->pdata->fifo_depth) {
		/*
		 * Power-on value of RX_WMark is FIFO_DEPTH-1, but this may
		 * have been overwritten by the bootloader, just like we're
		 * about to do, so if you know the value for your hardware, you
		 * should put it in the platform data.
		 */
		fifo_size = mci_readl(host, SDMC_FIFOCFG);
		fifo_size = 1 + ((fifo_size >> 16) & 0xfff);
	} else {
		fifo_size = host->pdata->fifo_depth;
	}
	host->fifo_depth = fifo_size;
	host->fifoth_val =
		SDMC_FIFOCFG_SET_THD(0x2, fifo_size / 2 - 1, fifo_size / 2);
	mci_writel(host, SDMC_FIFOCFG, host->fifoth_val);

	/* disable clock */
	sdmc_reg_clrbit(host, SDMC_CLKCTRL, SDMC_CLKCTRL_EN);

	host->verid = SDMC_VERID_GET(mci_readl(host, SDMC_VERID));
	dev_info(host->dev, "Version ID is %04x\n", host->verid);

	host->fifo_reg = host->regs + SDMC_FIFO_DATA;

	tasklet_init(&host->tasklet, artinchip_mmc_tasklet_func, (unsigned long)host);
	ret = devm_request_irq(host->dev, host->irq, artinchip_mmc_interrupt,
			       host->irq_flags, "aic-mmc", host);
	if (ret)
		goto err_dmaunmap;

	/*
	 * Enable interrupts for command done, data over, data empty,
	 * receive ready and error such as transmit, receive timeout, crc error
	 */
	mci_writel(host, SDMC_INTEN, SDMC_INT_CMD_DONE | SDMC_INT_DAT_DONE |
		   SDMC_INT_TXDR | SDMC_INT_RXDR | SDMC_ERROR_FLAGS);
	/* Enable mci interrupt */
	sdmc_reg_setbit(host, SDMC_HCTRL1, SDMC_HCTRL1_INT_EN);

	dev_info(host->dev,
		 "SDMC at irq %d,%d bit host data width,%u deep fifo\n",
		 host->irq, width, fifo_size);

	/* We need at least one slot to succeed */
	ret = artinchip_mmc_init_slot(host);
	if (ret) {
		dev_dbg(host->dev, "slot %d init failed\n", i);
		goto err_dmaunmap;
	}

	/* Now that slots are all setup, we can enable card detect */
	artinchip_mmc_enable_cd(host);

	host->attrs.attrs = aic_mmc_attr;
	return sysfs_create_group(&host->dev->kobj, &host->attrs);

err_dmaunmap:
	if (host->use_dma && host->dma_ops->exit)
		host->dma_ops->exit(host);

	if (!IS_ERR(host->reset))
		reset_control_assert(host->reset);

err_clk:
	clk_disable_unprepare(host->hif_clk);

	return ret;
}

void artinchip_mmc_remove(struct artinchip_mmc *host)
{
	dev_dbg(host->dev, "remove slot\n");
	if (host->slot)
		artinchip_mmc_cleanup_slot(host->slot);

	mci_writel(host, SDMC_OINTST, 0xFFFFFFFF);
	mci_writel(host, SDMC_INTEN, 0); /* disable all mmc interrupt first */

	/* disable clock*/
	sdmc_reg_clrbit(host, SDMC_CLKCTRL, SDMC_CLKCTRL_EN);


	if (host->use_dma && host->dma_ops->exit)
		host->dma_ops->exit(host);

	if (!IS_ERR(host->reset))
		reset_control_assert(host->reset);

	clk_disable_unprepare(host->hif_clk);
}
EXPORT_SYMBOL(artinchip_mmc_remove);

#ifdef CONFIG_PM
int artinchip_mmc_runtime_suspend(struct device *dev)
{
	struct artinchip_mmc *host = dev_get_drvdata(dev);

	if (host->use_dma && host->dma_ops->exit)
		host->dma_ops->exit(host);

	clk_disable_unprepare(host->hif_clk);

	return 0;
}
EXPORT_SYMBOL(artinchip_mmc_runtime_suspend);

int artinchip_mmc_runtime_resume(struct device *dev)
{
	int ret = 0;
	struct artinchip_mmc *host = dev_get_drvdata(dev);

	ret = clk_prepare_enable(host->hif_clk);
	if (ret)
		goto err;

	if (!artinchip_mmc_ctrl_reset(host, SDMC_HCTRL1_RESET_ALL)) {
		clk_disable_unprepare(host->hif_clk);
		ret = -ENODEV;
		goto err;
	}

	if (host->use_dma && host->dma_ops->init)
		host->dma_ops->init(host);

	/*
	 * Restore the initial value at FIFOTH register
	 * And Invalidate the prev_blksz with zero
	 */
	mci_writel(host, SDMC_FIFOCFG, host->fifoth_val);
	host->prev_blksz = 0;

	/* Put in max timeout */
	mci_writel(host, SDMC_TTMC, 0xFFFFFFFF);

	mci_writel(host, SDMC_OINTST, 0xFFFFFFFF);
	mci_writel(host, SDMC_INTEN, SDMC_INT_CMD_DONE | SDMC_INT_DAT_DONE |
		   SDMC_INT_TXDR | SDMC_INT_RXDR | SDMC_ERROR_FLAGS);

	sdmc_reg_setbit(host, SDMC_HCTRL1, SDMC_HCTRL1_INT_EN);

	if (host->slot->mmc->pm_flags & MMC_PM_KEEP_POWER)
		artinchip_mmc_set_ios(host->slot->mmc, &host->slot->mmc->ios);

	/* Force setup bus to guarantee available clock output */
	artinchip_mmc_setup_bus(host->slot, true);

	/* Re-enable SDIO interrupts. */
	if (sdio_irq_claimed(host->slot->mmc))
		__artinchip_mmc_enable_sdio_irq(host->slot, 1);

	/* Now that slots are all setup, we can enable card detect */
	artinchip_mmc_enable_cd(host);

	return 0;

err:
	if (host->slot &&
	    (mmc_can_gpio_cd(host->slot->mmc) ||
	     !mmc_card_is_removable(host->slot->mmc)))
		clk_disable_unprepare(host->hif_clk);

	return ret;
}
EXPORT_SYMBOL(artinchip_mmc_runtime_resume);
#endif /* CONFIG_PM */

struct artinchip_mmc_aic_priv_data {
	struct clk		*drv_clk;
	struct clk		*sample_clk;
	int			default_sample_phase;
	int			num_phases;
	u8			tuned_sample;
};

static void artinchip_mmc_aic_set_clksmpl(struct artinchip_mmc *host, u8 sample)
{
	u32 clksel;

	clksel = mci_readl(host, SDMC_DLYCTRL);
	clksel &= ~(0x3 << 28);
	clksel |= sample << 28;
	mci_writel(host, SDMC_DLYCTRL, clksel);
}

static u8 artinchip_mmc_aic_move_next_clksmpl(struct artinchip_mmc *host)
{
	u32 clksel, temp;
	u8 sample;

	clksel = mci_readl(host, SDMC_DLYCTRL);
	temp = (clksel >> 28) & 0x3;
	if (temp < 3)
		temp++;
	sample = (u8)temp;
	temp = temp << 28;
	clksel &= ~(0x3 << 28);
	clksel |= temp;

	mci_writel(host, SDMC_DLYCTRL, clksel);

	return sample;
}

static s8 artinchip_mmc_aic_get_best_clksmpl(u8 candiates)
{
	const u8 iter = 8;
	u8 __c;
	s8 i, loc = -1;

	for (i = 0; i < iter; i++) {
		__c = ror8(candiates, i);
		if ((__c & 0xc7) == 0xc7) {
			loc = i;
			goto out;
		}
	}

	for (i = 0; i < iter; i++) {
		__c = ror8(candiates, i);
		if ((__c & 0x83) == 0x83) {
			loc = i;
			goto out;
		}
	}

out:
	return loc;
}

static int artinchip_mmc_aic_execute_tuning(struct artinchip_mmc_slot *slot, u32 opcode)
{
	struct artinchip_mmc *host = slot->host;
	struct mmc_host *mmc = slot->mmc;
	struct artinchip_mmc_aic_priv_data *priv = host->priv;
	u8 start_smpl, smpl, candiates = 0;
	s8 best_clksmpl = -1;
	int ret;
	/*set 0*/
	start_smpl = (mci_readl(host, SDMC_DLYCTRL) >> 28) & 0x3;
	do {
		mci_writel(host, SDMC_TTMC, 0xfffffff);
		smpl = artinchip_mmc_aic_move_next_clksmpl(host);
		/*send tuning cmd*/
		if (!mmc_send_tuning(mmc, opcode, NULL))
			candiates |= (1 << smpl);
	} while (start_smpl != smpl);

	best_clksmpl = artinchip_mmc_aic_get_best_clksmpl(candiates);
	if (best_clksmpl >= 0) {
		artinchip_mmc_aic_set_clksmpl(host, best_clksmpl);
		priv->tuned_sample = best_clksmpl;
	} else {
		ret = -EIO;
	}
	return 0;
}

static int artinchip_mmc_aic_parse_dt(struct artinchip_mmc *host)
{
	struct artinchip_mmc_aic_priv_data *priv;

	priv = devm_kzalloc(host->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	host->priv = priv;

	return 0;
}

static int artinchip_mmc_aic_init(struct artinchip_mmc *host)
{
	return 0;
}

static const struct artinchip_mmc_drv_data aic_drv_data = {
	.caps			= NULL,
	.num_caps		= 0,
	.execute_tuning		= artinchip_mmc_aic_execute_tuning,
	.parse_dt		= artinchip_mmc_aic_parse_dt,
	.init			= artinchip_mmc_aic_init,
};

static const struct of_device_id artinchip_mmc_aic_match[] = {
	{ .compatible = "artinchip,aic-sdmc", .data = &aic_drv_data},
	{},
};
MODULE_DEVICE_TABLE(of, artinchip_mmc_aic_match);

static int artinchip_mmc_aic_probe(struct platform_device *pdev)
{
	const struct artinchip_mmc_drv_data *drv_data;
	const struct of_device_id *match;
	struct artinchip_mmc *host;
	struct resource	*regs;

	if (!pdev->dev.of_node)
		return -ENODEV;

	match = of_match_node(artinchip_mmc_aic_match, pdev->dev.of_node);
	drv_data = match->data;
	host = devm_kzalloc(&pdev->dev, sizeof(struct artinchip_mmc), GFP_KERNEL);
	if (!host)
		return -ENOMEM;

	host->irq = platform_get_irq(pdev, 0);
	if (host->irq < 0)
		return host->irq;

	host->drv_data = drv_data;
	host->dev = &pdev->dev;
	host->irq_flags = 0;
	host->pdata = pdev->dev.platform_data;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	host->regs = devm_ioremap_resource(&pdev->dev, regs);
	if (IS_ERR(host->regs))
		return PTR_ERR(host->regs);

	/* Get registers' physical base address */
	host->phy_regs = regs->start;

	platform_set_drvdata(pdev, host);
	return artinchip_mmc_probe(host);

}

static int artinchip_mmc_aic_remove(struct platform_device *pdev)
{
	struct artinchip_mmc *host = platform_get_drvdata(pdev);

	artinchip_mmc_remove(host);
	return 0;
}

static const struct dev_pm_ops artinchip_mmc_aic_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(artinchip_mmc_runtime_suspend,
			   artinchip_mmc_runtime_resume,
			   NULL)
};

static struct platform_driver artinchip_mmc_aic_pltfm_driver = {
	.probe		= artinchip_mmc_aic_probe,
	.remove		= artinchip_mmc_aic_remove,
	.driver		= {
		.name		= "aic_sdmc",
		.of_match_table	= artinchip_mmc_aic_match,
		.pm		= &artinchip_mmc_aic_dev_pm_ops,
	},
};

module_platform_driver(artinchip_mmc_aic_pltfm_driver);

MODULE_AUTHOR("matteo <mintao.duan@artinchip.com>");
MODULE_DESCRIPTION("Artinchip SDMC Driver Extension");
MODULE_ALIAS("platform:aic-sdmc");
MODULE_LICENSE("GPL v2");
