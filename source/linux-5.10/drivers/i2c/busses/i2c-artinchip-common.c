// SPDX-License-Identifier: GPL-2.0-only
/*
 * I2C driver of ArtInChip SoC
 *
 * Copyright (C) 2020-2023 ArtInChip Technology Co., Ltd.
 * Authors:  dwj <weijie.ding@artinchip.com>
 */

#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/time64.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/swab.h>
#include <linux/iopoll.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>


#include "i2c-artinchip.h"

#define FS_MIN_SCL_HIGH         600
#define FS_MIN_SCL_LOW          1300
#define SS_MIN_SCL_HIGH         4000
#define SS_MIN_SCL_LOW          4700

static char *abort_sources[] = {
	[ABRT_7BIT_ADDR_NOACK] =
		"slave address not acknowledged (7bit mode)",
	[ABRT_10BIT_ADDR1_NOACK] =
		"first address byte not acknowledged (10bit mode)",
	[ABRT_10BIT_ADDR2_NOACK] =
		"second address byte not acknowledged (10bit mode)",
	[ABRT_TXDATA_NOACK] =
		"the first data byte after address byte not acknowledged",
	[ABRT_GCALL_NOACK] =
		"no acknowledgment for a general call",
	[ABRT_GCALL_READ] =
		"read after general call",
	[ABRT_SBYTE_ACKDET] =
		"start byte acknowledged",
	[ABRT_SBYTE_NORSTRT] =
		"trying to send start byte when restart is disabled",
	[ABRT_10BIT_RD_NORSTRT] =
		"trying to read when restart is disabled (10bit mode)",
	[ABRT_MASTER_DIS] =
		"trying to use disabled adapter",
	[ABRT_LOST] =
		"lost arbitration",
	[ABRT_SLVFLUSH_TXFIFO] =
		"read command so flush old data in the TX FIFO",
	[ABRT_SLV_ARBLOST] =
		"slave lost the bus while transmitting data to a remote master",
	[ABRT_SLVRD_INTX] =
		"incorrect slave-transmitter mode configuration",
	[ABRT_USER_ABRT] =
		"detect transfer abort in master mode",
	[ABRT_SDA_STUCK_AT_LOW] =
		"SDA held low level timeout",
};

int i2c_handle_tx_abort(struct aic_i2c_dev *i2c_dev)
{
	unsigned long abort_source = i2c_dev->abort_source;
	int i;

	if (abort_source & I2C_TX_ABRT_NOACK) {
		for_each_set_bit(i, &abort_source, ARRAY_SIZE(abort_sources))
			dev_dbg(i2c_dev->dev, "%s: %s\n",
			__func__, abort_sources[i]);
		return -EREMOTEIO;
	}

	for_each_set_bit(i, &abort_source, ARRAY_SIZE(abort_sources))
		dev_err(i2c_dev->dev, "%s: %s\n", __func__, abort_sources[i]);

	if (abort_source & I2C_ABRT_LOST)
		return -EAGAIN;
	else if (abort_source & I2C_ABRT_GCALL_READ)
		return -EINVAL;
	else
		return -EIO;
}

int i2c_wait_bus_free(struct aic_i2c_dev *i2c_dev)
{
	u32 status;
	int ret;

	ret = readl_relaxed_poll_timeout(i2c_dev->base + I2C_STATUS, status,
			!(status & I2C_STATUS_ACTIVITY), 10, 1000);
	if (ret) {
		dev_dbg(i2c_dev->dev, "I2C bus not free!\n");
		ret = -EBUSY;
	}

	return ret;
}


u32 i2c_readl(struct aic_i2c_dev *i2c_dev, int offset)
{
	return readl(i2c_dev->base + offset);
}

void i2c_writel(struct aic_i2c_dev *i2c_dev, u32 val, int offset)
{
	writel(val, i2c_dev->base + offset);
}

void i2c_disable_interrupts(struct aic_i2c_dev *i2c_dev)
{
	writel(0, i2c_dev->base + I2C_INTR_MASK);
}

void i2c_enable(struct aic_i2c_dev *i2c_dev)
{
	int ret;

	ret = readl(i2c_dev->base + I2C_ENABLE);
	ret |= I2C_SDA_STUCK_RECOVERY_ENABLE | I2C_ENABLE_BIT;

	writel(ret, i2c_dev->base + I2C_ENABLE);
}

void i2c_disable(struct aic_i2c_dev *i2c_dev)
{
	int ret = 0;

	ret = i2c_wait_bus_free(i2c_dev);
	if (ret) {
		dev_err(i2c_dev->dev, "I2C disable failed\n");
		return;
	}

	ret = readl(i2c_dev->base + I2C_ENABLE);
	ret &= ~I2C_ENABLE_BIT;

	writel(ret, i2c_dev->base + I2C_ENABLE);
}

int i2c_scl_cnt(u32 module_clk, u32 rate, u16 *hcnt, u16 *lcnt)
{
	u16 hcnt_tmp;
	u16 lcnt_tmp;
	u32 temp;
	u32 duty_cycle = 50;	/* low/(low + high) time */

	if (rate > 384000) {
		/* When the i2c rate is greater than 384KHz,
		 * it is necessary to ensure that lcnt is not less than 1300ns,
		 * so the duty cycle needs to be adjusted.
		 */
		duty_cycle = (13 * rate) / 100000 + (((13 * rate) % 100000) ? 1 : 0);
	}

	temp = module_clk / rate;

	if (!hcnt || !lcnt)
		return -EINVAL;

	lcnt_tmp = temp * duty_cycle / 100 + ((temp * duty_cycle % 100) ? 1 : 0) - 1;
	hcnt_tmp = temp - lcnt_tmp - 8 - 2;

	*hcnt = hcnt_tmp;
	*lcnt = lcnt_tmp;
	return 0;
}

static int i2c_probe(struct platform_device *pdev)
{
	struct aic_i2c_dev *i2c_dev;
	struct i2c_timings *t;
	struct resource *mem;
	int irq, ret;
	u32 module_clk, hold_time;
	u8 setup_time;

	i2c_dev = devm_kzalloc(&pdev->dev,
				sizeof(struct aic_i2c_dev), GFP_KERNEL);
	if (!i2c_dev)
		return -ENOMEM;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	i2c_dev->base = devm_ioremap_resource(&pdev->dev, mem);
	if (IS_ERR(i2c_dev->base)) {
		ret = PTR_ERR(i2c_dev->base);
		goto exit_done;
	}

	i2c_dev->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(i2c_dev->clk)) {
		dev_err(i2c_dev->dev, "Failed to get I2C clock\n");
		return PTR_ERR(i2c_dev->clk);
	}

	module_clk = clk_get_rate(i2c_dev->clk);
	if (!module_clk) {
		dev_err(i2c_dev->dev, "get i2c clock rate failed\n");
		return -EINVAL;
	}

	if (clk_prepare_enable(i2c_dev->clk)) {
		dev_err(&pdev->dev, "try to enable I2C clock failed\n");
		return -EINVAL;
	}

	i2c_dev->rst = devm_reset_control_get_exclusive(&pdev->dev, NULL);
	if (IS_ERR(i2c_dev->rst))
		return PTR_ERR(i2c_dev->rst);
	ret = reset_control_deassert(i2c_dev->rst);
	if (ret)
		goto exit_clk_disable;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		ret = irq;
		goto exit_reset_assert;
	}

	i2c_dev->dev = &pdev->dev;
	i2c_dev->irq = irq;

	i2c_dev->adap.owner = THIS_MODULE;
	i2c_dev->adap.class = I2C_CLASS_DEPRECATED;
	i2c_dev->adap.dev.parent = i2c_dev->dev;
	i2c_dev->adap.nr = -1;
	i2c_dev->adap.dev.of_node = pdev->dev.of_node;
	i2c_dev->adap.timeout = HZ;

	platform_set_drvdata(pdev, i2c_dev);

	t = &i2c_dev->timings;
	i2c_parse_fw_timings(&pdev->dev, t, false);
	if (t->bus_freq_hz >= 200 && t->bus_freq_hz <= 100000) {
		i2c_dev->i2c_speed = I2C_SPEED_STD;
		i2c_dev->target_rate = t->bus_freq_hz;
	} else if (t->bus_freq_hz > 100000 && t->bus_freq_hz <= 400000) {
		i2c_dev->i2c_speed = I2C_SPEED_FAST;
		i2c_dev->target_rate = t->bus_freq_hz;
	} else {
		i2c_dev->i2c_speed = I2C_SPEED_FAST;
		i2c_dev->target_rate = 400000;
	}

	if (t->sda_hold_ns != 0) {
		hold_time = t->sda_hold_ns / (1000000000 / module_clk) + 1;
		hold_time |= hold_time << 16;
		i2c_writel(i2c_dev, hold_time, I2C_SDA_HOLD);
	}

	if (t->scl_int_delay_ns != 0) {
		setup_time = t->scl_int_delay_ns / (1000000000 / module_clk) + 1;
		i2c_writel(i2c_dev, setup_time, I2C_SDA_SETUP);
	}

	WARN_ON(pm_runtime_enabled(&pdev->dev));

	pm_runtime_set_autosuspend_delay(&pdev->dev, 1000);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	if (i2c_detect_slave_mode(&pdev->dev))
		ret = i2c_probe_slave(i2c_dev);
	else
		ret = i2c_probe_master(i2c_dev);

	if (ret)
		return ret;

	dev_info(&pdev->dev, "I2C initialized\n");
	return 0;

exit_reset_assert:
	reset_control_assert(i2c_dev->rst);
exit_clk_disable:
	clk_disable_unprepare(i2c_dev->clk);
exit_done:
	return ret;
}

static int i2c_remove(struct platform_device *pdev)
{
	struct aic_i2c_dev *i2c_dev = platform_get_drvdata(pdev);

	pm_runtime_get_sync(&pdev->dev);
	i2c_del_adapter(&i2c_dev->adap);
	i2c_disable(i2c_dev);
	pm_runtime_put_sync(&pdev->dev);

	reset_control_assert(i2c_dev->rst);

	return 0;
}

#ifdef CONFIG_PM
static int i2c_runtime_suspend(struct device *dev)
{
	struct aic_i2c_dev *i2c_dev = dev_get_drvdata(dev);

	clk_disable_unprepare(i2c_dev->clk);
	return 0;
}

static int i2c_runtime_resume(struct device *dev)
{
	struct aic_i2c_dev *i2c_dev = dev_get_drvdata(dev);

	return clk_prepare_enable(i2c_dev->clk);
}

static const struct dev_pm_ops i2c_pm_ops = {
	SET_RUNTIME_PM_OPS(i2c_runtime_suspend, i2c_runtime_resume, NULL)
};

#define I2C_DEV_PM_OPS	(&i2c_pm_ops)
#else
#define I2C_DEV_PM_OPS	NULL
#endif

static const struct of_device_id aic_i2c_match[] = {
	{
		.compatible = "artinchip,aic-i2c",
	},
	{}
};

static struct platform_driver aic_i2c_driver = {
	.probe = i2c_probe,
	.remove = i2c_remove,
	.driver = {
		.name = "aic-i2c",
		.of_match_table = aic_i2c_match,
		.pm = I2C_DEV_PM_OPS,
	},
};

module_platform_driver(aic_i2c_driver);
MODULE_DESCRIPTION("ArtInChip I2C controller driver");
MODULE_AUTHOR("dwj <weijie.ding@artinchip.com>");
MODULE_LICENSE("GPL");

