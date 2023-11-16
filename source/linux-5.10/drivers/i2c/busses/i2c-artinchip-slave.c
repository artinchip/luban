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

static void i2c_configure_fifo_slave(struct aic_i2c_dev *i2c_dev)
{
	i2c_writel(i2c_dev, 0, I2C_TX_TL);
	i2c_writel(i2c_dev, 0, I2C_RX_TL);
}

static void i2c_init_slave(struct aic_i2c_dev *i2c_dev)
{
	i2c_disable(i2c_dev);

	i2c_dev->slave_cfg = I2C_ENABLE_SLAVE_DISABLE_MASTER |
			     I2C_CTL_STOP_DET_IFADDR;

	i2c_configure_fifo_slave(i2c_dev);
	/* Configure I2C slave mode */
	i2c_writel(i2c_dev, i2c_dev->slave_cfg, I2C_CTL);
	/* clear all interrupt flags */
	i2c_writel(i2c_dev, 0xFFFF, I2C_INTR_CLR);
	/* Enable slave interrupt */
	i2c_writel(i2c_dev, I2C_INTR_SLAVE_MASK, I2C_INTR_MASK);
	/* Enable I2C */
	i2c_enable(i2c_dev);
}

static int i2c_reg_slave(struct i2c_client *slave)
{
	u32 ic_ctl;
	struct aic_i2c_dev *i2c_dev = i2c_get_adapdata(slave->adapter);

	pm_runtime_get_sync(i2c_dev->dev);
	/* disable I2C adapter */
	i2c_disable(i2c_dev);

	ic_ctl = i2c_readl(i2c_dev, I2C_CTL);
	if (i2c_dev->slave)
		return -EBUSY;

	/* Configure slave address mode */
	if (slave->flags & I2C_CLIENT_TEN)
		ic_ctl |= I2C_CTL_10BIT_SELECT_SLAVE;
	else
		ic_ctl &= ~I2C_CTL_10BIT_SELECT_SLAVE;
	i2c_writel(i2c_dev, ic_ctl, I2C_CTL);

	/* Configure slave address */
	i2c_writel(i2c_dev, slave->addr, I2C_SAR);
	i2c_dev->slave = slave;
	i2c_enable(i2c_dev);

	return 0;
}

static int i2c_unreg_slave(struct i2c_client *slave)
{
	struct aic_i2c_dev *i2c_dev = i2c_get_adapdata(slave->adapter);

	i2c_disable_interrupts(i2c_dev);
	i2c_disable(i2c_dev);
	i2c_dev->slave = NULL;
	pm_runtime_put(i2c_dev->dev);

	return 0;
}

static u32 i2c_func_slave(struct i2c_adapter *adapter)
{
	return I2C_FUNC_SLAVE | I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static irqreturn_t i2c_irq_handler_slave(int this_irq, void *dev_id)
{
	struct aic_i2c_dev *i2c_dev = dev_id;
	u32 intr_stat;
	u8 val;

	intr_stat = i2c_readl(i2c_dev, I2C_INTR_CLR);

	if (intr_stat & I2C_INTR_START_DET) {
		i2c_slave_event(i2c_dev->slave,
				I2C_SLAVE_WRITE_REQUESTED, &val);
		i2c_writel(i2c_dev, I2C_INTR_START_DET, I2C_INTR_CLR);
	}

	if (intr_stat & I2C_INTR_RX_FULL) {
		val = i2c_readl(i2c_dev, I2C_DATA_CMD);
		if (!i2c_slave_event(i2c_dev->slave,
				I2C_SLAVE_WRITE_RECEIVED, &val))
			dev_dbg(i2c_dev->dev, "Byte 0x%02x acked!\n", val);
		i2c_writel(i2c_dev, I2C_INTR_RX_FULL, I2C_INTR_CLR);
	}

	if (intr_stat & I2C_INTR_RD_REQ) {
		if (!i2c_slave_event(i2c_dev->slave,
				I2C_SLAVE_READ_REQUESTED, &val))
			i2c_writel(i2c_dev, val, I2C_DATA_CMD);
		i2c_slave_event(i2c_dev->slave, I2C_SLAVE_READ_PROCESSED, &val);
		i2c_writel(i2c_dev, I2C_INTR_RD_REQ, I2C_INTR_CLR);
	}

	if (intr_stat & I2C_INTR_RX_DONE) {
		i2c_slave_event(i2c_dev->slave, I2C_SLAVE_STOP, &val);
		i2c_writel(i2c_dev, I2C_INTR_RX_DONE, I2C_INTR_CLR);
	}

	return IRQ_HANDLED;
}

static const struct i2c_algorithm i2c_algo_slave = {
	.reg_slave = i2c_reg_slave,
	.unreg_slave = i2c_unreg_slave,
	.functionality = i2c_func_slave,
};

int i2c_probe_slave(struct aic_i2c_dev *i2c_dev)
{
	struct i2c_adapter *adap = &i2c_dev->adap;
	int ret;

	i2c_init_slave(i2c_dev);
	snprintf(adap->name, sizeof(adap->name),
		 "ArtInChip I2C slave adapter");
	adap->retries = 3;
	adap->algo = &i2c_algo_slave;
	adap->dev.parent = i2c_dev->dev;
	i2c_set_adapdata(adap, i2c_dev);

	ret = devm_request_irq(i2c_dev->dev, i2c_dev->irq,
				i2c_irq_handler_slave, 0,
				dev_name(i2c_dev->dev), i2c_dev);
	if (ret) {
		dev_err(i2c_dev->dev, "failure requesting irq %i: %d\n",
			i2c_dev->irq, ret);
		return ret;
	}

	ret = i2c_add_numbered_adapter(adap);
	if (ret)
		dev_err(i2c_dev->dev, "failure adding adapter: %d\n", ret);

	return ret;
}
EXPORT_SYMBOL_GPL(i2c_probe_slave);

MODULE_DESCRIPTION("ArtInChip I2C bus slave adapter");
MODULE_LICENSE("GPL");
