// SPDX-License-Identifier: GPL-2.0-only
/*
 * I2C driver of ArtInChip SoC
 *
 * Copyright (C) 2020-2023 ArtInChip Technology Co., Ltd.
 * Authors:  dwj <weijie.ding@artinchip.com>
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/device.h>

#include "i2c-artinchip.h"

static void i2c_config_fifo_master(struct aic_i2c_dev *i2c_dev)
{
	/* Configure Tx/Rx FIFO threshold levels */
	i2c_writel(i2c_dev, I2C_TXFIFO_THRESHOLD, I2C_TX_TL);
	i2c_writel(i2c_dev, I2C_RXFIFO_THRESHOLD, I2C_RX_TL);
}

static int i2c_set_timmings_master(struct aic_i2c_dev *i2c_dev)
{
	const char *mode_str;
	u32 ic_clk;
	int ret;

	if (!i2c_dev->scl_hcnt || !i2c_dev->scl_lcnt) {
		ic_clk = clk_get_rate(i2c_dev->clk);
		if (!ic_clk) {
			dev_err(i2c_dev->dev, "get i2c clock rate failed\n");
			return -EINVAL;
		}

		ret = i2c_scl_cnt(ic_clk / 1000, i2c_dev->i2c_speed,
				&i2c_dev->scl_hcnt, &i2c_dev->scl_lcnt);
		if (ret) {
			dev_err(i2c_dev->dev, "set i2c scl cnt error\n");
			return ret;
		}
	}

	switch (i2c_dev->i2c_speed) {
	case I2C_SPEED_STD:
		mode_str = "Standard Mode";
		break;
	case I2C_SPEED_FAST:
	default:
		mode_str = "Fast Mode";
		break;
	}

	dev_dbg(i2c_dev->dev, "%s HCNT: %d, LCNT: %d\n", mode_str,
		i2c_dev->scl_hcnt, i2c_dev->scl_lcnt);

	return 0;
}

static void i2c_init_master(struct aic_i2c_dev *i2c_dev)
{
	i2c_disable(i2c_dev);

	i2c_dev->master_cfg = I2C_ENABLE_MASTER_DISABLE_SLAVE |
			  I2C_CTL_RESTART_ENABLE;

	/* Configure SCL hcnt, lcnt and speed mode */
	if (i2c_dev->i2c_speed == I2C_SPEED_STD) {
		i2c_writel(i2c_dev, i2c_dev->scl_hcnt, I2C_SS_SCL_HCNT);
		i2c_writel(i2c_dev, i2c_dev->scl_lcnt, I2C_SS_SCL_LCNT);
		i2c_dev->master_cfg |= I2C_CTL_SPEED_MODE_SS;
	} else {
		i2c_writel(i2c_dev, i2c_dev->scl_hcnt, I2C_FS_SCL_HCNT);
		i2c_writel(i2c_dev, i2c_dev->scl_lcnt, I2C_FS_SCL_LCNT);
		i2c_dev->master_cfg |= I2C_CTL_SPEED_MODE_FS;
	}

	/* Configure Tx/Rx FIFO */
	i2c_config_fifo_master(i2c_dev);
	/* Configure I2C as master */
	i2c_writel(i2c_dev, i2c_dev->master_cfg, I2C_CTL);
}

/* Set the slave address and transmission mode(7bit or 10bit)
 * before enable I2C module.
 */
static void i2c_xfer_init(struct aic_i2c_dev *i2c_dev)
{
	u32 ic_ctl;

	/* If the slave address is ten bit address, enable 10ADDR */
	ic_ctl = i2c_readl(i2c_dev, I2C_CTL);
	if (i2c_dev->msg->flags & I2C_M_TEN)
		ic_ctl |= I2C_CTL_10BIT_SELECT_MASTER;
	else
		ic_ctl &= ~I2C_CTL_10BIT_SELECT_MASTER;

	i2c_writel(i2c_dev, ic_ctl, I2C_CTL);

	/* Set the slave address */
	i2c_writel(i2c_dev, i2c_dev->msg->addr, I2C_TAR);
}

/*
 * Set the state of the message to its initial value before
 * each message transmission
 */
static void i2c_xfer_msg_init(struct aic_i2c_dev *i2c_dev)
{
	/* configure transfer status */
	i2c_dev->buf_write_idx = 0;
	i2c_dev->buf_read_idx = 0;
	i2c_dev->msg_err = 0;
	i2c_dev->abort_source = 0;
	i2c_dev->msg_status = MSG_IDLE;

	/* clear and enable interrupts */
	i2c_writel(i2c_dev, 0xFFFF, I2C_INTR_CLR);
	i2c_writel(i2c_dev, I2C_INTR_MASTER_MASK, I2C_INTR_MASK);
}

static void i2c_handle_read(struct aic_i2c_dev *i2c_dev)
{
	struct i2c_msg *msg = i2c_dev->msg;
	int rx_valid;

	rx_valid = i2c_readl(i2c_dev, I2C_RXFLR);
	while (rx_valid--) {
		msg->buf[i2c_dev->buf_read_idx++] = i2c_readl(i2c_dev,
							I2C_DATA_CMD);
	}

	/* message transfer done if it is a read message */
	if (i2c_dev->buf_read_idx == msg->len) {
		i2c_disable_interrupts(i2c_dev);
		complete(&i2c_dev->cmd_complete);
	}
}

static void i2c_handle_write(struct aic_i2c_dev *i2c_dev)
{
	struct i2c_msg *msg = i2c_dev->msg;
	int rx_valid, tx_valid, buf_len;
	u32 intr_mask = I2C_INTR_MASTER_MASK;

	/* In case of detecting the bus which slave address is used. */
	/* The data transferred on I2C bus is: START ADDR Wr ACK STOP. */
	if (i2c_dev->msg_status == MSG_IDLE && msg->len == 0 &&
	    i2c_dev->is_first_message && i2c_dev->is_last_message) {
		u32 cmd = 0;

		cmd |= I2C_DATA_CMD_STOP;
		/* Write operation */
		i2c_writel(i2c_dev, cmd, I2C_DATA_CMD);
		i2c_disable_interrupts(i2c_dev);
		complete(&i2c_dev->cmd_complete);
	}

	i2c_dev->msg_status = MSG_IN_PROCESS;
	rx_valid = I2C_FIFO_DEPTH - i2c_readl(i2c_dev, I2C_RXFLR);
	tx_valid = I2C_FIFO_DEPTH - i2c_readl(i2c_dev, I2C_TXFLR);
	buf_len = msg->len - i2c_dev->buf_write_idx;

	while (buf_len > 0 && rx_valid > 0 && tx_valid > 0) {
		u32 cmd = 0;

		if (buf_len == 1 && i2c_dev->is_last_message)
			cmd |= I2C_DATA_CMD_STOP;

		if (!i2c_dev->is_first_message && !i2c_dev->buf_write_idx)
			cmd |= I2C_DATA_CMD_RESTART;

		if (msg->flags & I2C_M_RD) {
			/* Read operation, I2C read one byte when you set
			 * CMD bit to 1 in I2C_DATA_CMD register. Slave receive
			 * ACK signal and transmit next byte data when you
			 * set CMD bit to 1 in I2C_DATA_CMD register.
			 */
			i2c_writel(i2c_dev, cmd | I2C_DATA_CMD_READ,
					I2C_DATA_CMD);
			rx_valid--;
		} else {
			/* Write operation */
			i2c_writel(i2c_dev,
				cmd | msg->buf[i2c_dev->buf_write_idx],
				I2C_DATA_CMD);
		}

		i2c_dev->buf_write_idx++;
		tx_valid--;
		buf_len--;
		if (i2c_dev->buf_write_idx == msg->len) {
			intr_mask &= ~I2C_INTR_TX_EMPTY;
			i2c_writel(i2c_dev, intr_mask, I2C_INTR_MASK);
			/* message transfer done if it is a write message */
			if (!(msg->flags & I2C_M_RD)) {
				i2c_disable_interrupts(i2c_dev);
				complete(&i2c_dev->cmd_complete);
			}
		}
	}
}

static int i2c_xfer_msg(struct aic_i2c_dev *i2c_dev, struct i2c_msg *msg,
			bool is_first, bool is_last)
{
	unsigned long timeout;
	int ret = 0;

	i2c_dev->msg = msg;
	i2c_dev->is_first_message = is_first;
	i2c_dev->is_last_message = is_last;

	reinit_completion(&i2c_dev->cmd_complete);

	if (is_first) {
		i2c_xfer_init(i2c_dev);
		/* Enable the adapter */
		i2c_enable(i2c_dev);

		ret = i2c_wait_bus_free(i2c_dev);
		if (ret)
			return ret;
	}

	i2c_xfer_msg_init(i2c_dev);

	timeout = wait_for_completion_timeout(&i2c_dev->cmd_complete,
						i2c_dev->adap.timeout);

	if (!timeout) {
		dev_dbg(i2c_dev->dev, "message xfer timeout\n");
		ret = -ETIMEDOUT;
	}

	/* Because you need to send restart between different messages,
	 * So, disable I2C module between messages is prohibited
	 */

	if (i2c_dev->msg_err) {
		if (i2c_dev->msg_err & I2C_INTR_ERROR_ABRT)
			ret = i2c_handle_tx_abort(i2c_dev);
		else
			ret = -EIO;
	}

	return ret;
}

static int i2c_xfer(struct i2c_adapter *i2c_adap, struct i2c_msg msgs[],
		int num)
{
	struct aic_i2c_dev *i2c_dev = i2c_get_adapdata(i2c_adap);
	int ret = 0, i;

	pm_runtime_get_sync(i2c_dev->dev);

	for (i = 0; i < num && !ret; i++)
		ret = i2c_xfer_msg(i2c_dev, &msgs[i], i == 0, i == (num -1));

	/* Because you can only set the address mode when I2C disabled,
	 * so you must disable I2C after each transfer
	 */
	i2c_disable(i2c_dev);

	pm_runtime_mark_last_busy(i2c_dev->dev);
	pm_runtime_put_autosuspend(i2c_dev->dev);

	return (ret < 0) ? ret : num;
}

static irqreturn_t i2c_isr(int irqno, void *dev_id)
{
	struct aic_i2c_dev *i2c_dev = dev_id;
	u32 intr_stat;

	intr_stat = i2c_readl(i2c_dev, I2C_INTR_CLR);
	/* clear all interrupt flags */
	i2c_writel(i2c_dev, 0xFFFF, I2C_INTR_CLR);

	if (intr_stat & I2C_INTR_TX_ABRT) {
		i2c_dev->msg_err |= I2C_INTR_ERROR_ABRT;
		i2c_dev->abort_source = i2c_readl(i2c_dev, I2C_TX_ABRT_SOURCE);
		goto tx_aborted;
	}

	if (intr_stat & (I2C_INTR_RX_OVER | I2C_INTR_RX_UNDER |
			I2C_INTR_TX_OVER)) {
		i2c_dev->msg_err |= I2C_INTR_ERROR_TX_RX;
		goto tx_aborted;
	}

	if (intr_stat & I2C_INTR_RX_FULL)
		i2c_handle_read(i2c_dev);

	if (intr_stat & I2C_INTR_TX_EMPTY)
		i2c_handle_write(i2c_dev);

tx_aborted:
	if (intr_stat & (I2C_INTR_TX_ABRT | I2C_INTR_STOP_DET |
			 I2C_INTR_RX_OVER | I2C_INTR_RX_UNDER |
			 I2C_INTR_TX_OVER)) {
		i2c_disable_interrupts(i2c_dev);
		complete(&i2c_dev->cmd_complete);
	}

	return IRQ_HANDLED;
}

static u32 i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm i2c_algo = {
	.master_xfer = i2c_xfer,
	.functionality = i2c_func,
};

int i2c_probe_master(struct aic_i2c_dev *i2c_dev)
{
	struct i2c_adapter *adap = &i2c_dev->adap;
	int ret;

	init_completion(&i2c_dev->cmd_complete);

	ret = i2c_set_timmings_master(i2c_dev);
	if (ret)
		return ret;

	i2c_init_master(i2c_dev);

	snprintf(adap->name, sizeof(adap->name),
		 "ArtInChip I2C adapter");
	adap->retries = 3;
	adap->algo = &i2c_algo;
	i2c_set_adapdata(adap, i2c_dev);

	/* disable all interrupt and clear interrupt flags */
	i2c_disable_interrupts(i2c_dev);
	i2c_writel(i2c_dev, 0xFFFF, I2C_INTR_CLR);

	ret = devm_request_irq(i2c_dev->dev, i2c_dev->irq, i2c_isr, 0,
				dev_name(i2c_dev->dev), i2c_dev);
	if (ret) {
		dev_err(i2c_dev->dev, "failed to request irq %i: %d\n",
			i2c_dev->irq, ret);
		return ret;
	}

	ret = i2c_add_numbered_adapter(adap);
	if (ret)
		dev_err(i2c_dev->dev, "failure adding adapter: %d\n", ret);

	return ret;
}
EXPORT_SYMBOL_GPL(i2c_probe_master);

MODULE_DESCRIPTION("ArtInChip I2C bus master adapter");
MODULE_LICENSE("GPL");
