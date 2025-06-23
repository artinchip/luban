// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021-2025, ArtInChip Technology Co., Ltd
 * Author: Dehuang Wu <dehuang.wu@artinchip.com>
 *         Matteo <duanmt@artinchip.com>
 */
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/of.h>
#include <linux/reboot-reason.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/pm_wakeirq.h>

#define AIC_RTC_NAME			"aic-rtc"

#define RTC_REG_CTL			(0x0000)
#define RTC_REG_INIT			(0x0004)
#define RTC_REG_IRQ_EN			(0x0008)
#define RTC_REG_IRQ_STA			(0x000C)
#define RTC_REG_TIME0			(0x0020)
#define RTC_REG_TIME1			(0x0024)
#define RTC_REG_TIME2			(0x0028)
#define RTC_REG_TIME3			(0x002C)
#define RTC_REG_ALARM0			(0x0030)
#define RTC_REG_ALARM1			(0x0034)
#define RTC_REG_ALARM2			(0x0038)
#define RTC_REG_ALARM3			(0x003C)
#define RTC_REG_CALI0			(0x0040)
#define RTC_REG_CALI1			(0x0044)
#define RTC_REG_ANALOG0			(0x0050)
#define RTC_REG_ANALOG1			(0x0054)
#define RTC_REG_ANALOG2			(0x0058)
#define RTC_REG_ANALOG3			(0x005C)
#define RTC_REG_WR_EN			(0x00FC)
#define RTC_REG_SYSBAK			(0x0100)
#define RTC_REG_TCNT			(0x0800)
#define RTC_REG_32K_DET			(0x0804)
#define RTC_REG_VER			(0x08FC)

#define RTC_CTL_IO_OUTPUT_ENABLE	1
#define RTC_CTL_IO_ALARM_OUTPUT		2
#define RTC_CTL_IO_32K_CLK_OUTPUT	3
#define RTC_CTL_IO_SEL_SHIFT		4
#define RTC_CTL_IO_SEL_MASK		GENMASK(5, 4)
#define RTC_CTL_IO_SEL(x)		(((x) & 0x3) << RTC_CTL_IO_SEL_SHIFT)
#define RTC_CTL_ALARM_EN		BIT(2)
#define RTC_CTL_TCNT_EN			BIT(0)

#define RTC_IRQ_32K_ERR_EN		BIT(2)
#define RTC_IRQ_ALARM_EN		BIT(0)

#define RTC_IRQ_STA_32K_ERR		BIT(2)
#define RTC_IRQ_STA_ALARM_IO		BIT(1)
#define RTC_IRQ_STA_ALARM		BIT(0)

#define RTC_CAL1_FAST_DIR		BIT(7)

#define RTC_ANA0_RC1M_ISEL		BIT(7)
#define RTC_ANA0_RC1M_EN		BIT(6)
#define RTC_ANA0_LDO18_BYPASS		BIT(4)
#define RTC_ANA0_LDO18_VOL_MASK		GENMASK(3, 1)
#define RTC_ANA0_LDO18_VOL_SHIFT	(1)
#define RTC_ANA0_LDO18_EN		BIT(0)

#define RTC_ANA0_LDO18_VOL_120		7

#define RTC_ANA1_PD_CUR_SEL_MASK	GENMASK(6, 5)
#define RTC_ANA1_PD_CUR_SEL_SHIFT	(5)
#define RTC_ANA1_PD_CUR_EN		BIT(4)
#define RTC_ANA1_LDO11_VOL_MASK		GENMASK(3, 1)
#define RTC_ANA1_LDO11_VOL_SHIFT	(1)
#define RTC_ANA1_LDO11_LPEN		BIT(0)

#define RTC_ANA1_PD_CUR_SEL_025		0
#define RTC_ANA1_PD_CUR_SEL_050		1
#define RTC_ANA1_PD_CUR_SEL_075		2
#define RTC_ANA1_PD_CUR_SEL_100		3

#define RTC_ANA1_LDO11_VOL_090		4
#define RTC_ANA1_LDO11_VOL_080		6

#define RTC_ANA2_XTAL32K_DRV_MASK	GENMASK(3, 0)

#define RTC_ANA3_XTAL32K_EN		BIT(0)

#define RTC_32K_DET_EN			BIT(0)

#define RTC_WR_EN_KEY			0xAC

#define RTC_DRV_TIMEOUT			msecs_to_jiffies(2000)
#define RTC_32K_FREQ			(32 * 1024 * 100)

#define RTC_WRITE_ENABLE(base)	writeb(RTC_WR_EN_KEY, base + RTC_REG_WR_EN)
#define RTC_WRITE_DISABLE(base)	writeb(0, base + RTC_REG_WR_EN)
#define RTC_WRITEB(val, reg, base) \
	do { \
		RTC_WRITE_ENABLE(base); \
		writeb((val) & 0xFF, (base) + (reg)); \
		RTC_WRITE_DISABLE(base); \
	} while (0)
#define RTC_WRITEL(val, reg, base) \
	do { \
		RTC_WRITE_ENABLE(base); \
		writeb((val) & 0xFF, (base) + (reg)); \
		writeb(((val) >> 8) & 0xFF, (base) + (reg) + 0x4); \
		writeb(((val) >> 16) & 0xFF, (base) + (reg) + 0x8); \
		writeb(((val) >> 24) & 0xFF, (base) + (reg) + 0xC); \
		RTC_WRITE_DISABLE(base); \
	} while (0)
#define RTC_READL(reg)	(readb(reg) | (readb((reg) + 0x4) << 8) \
			| (readb((reg) + 0x8) << 16) \
			| (readb((reg) + 0xC) << 24))

struct aic_rtc_dev {
	void __iomem *base;
	struct rtc_device *rtc_dev;
	struct attribute_group attrs;
	struct clk *clk;
	u32  clk_rate;
	u32  clk_drv;
	int  irq;
	bool alarm_io;
	bool output_32k;
	bool cal_fast;
	s32  cal_val;
	s32  pwkey_gpio;

	struct completion complete;
};

static void __iomem *g_rtc_base;

static ssize_t status_show(struct device *dev,
			   struct device_attribute *devattr, char *buf)
{
	struct aic_rtc_dev *chip = dev_get_drvdata(dev);
	void __iomem *base = chip->base;
	u32 ver = readl(base + RTC_REG_VER);
	u32 ctl = readl(base + RTC_REG_CTL);

	return sprintf(buf, "In RTC V%d.%02d:\n"
		       "Module Enable: %d\n"
		       "Alarm Enable: %d, Output alarm IO: %d/%d, Output 32K: %d\n"
		       "Clock rate: %d, Driver: %d\n"
		       "Calibration %s, Value: %d\n",
		       ver >> 8, ver & 0xFF, (u32)(ctl & RTC_CTL_TCNT_EN),
		       ctl & RTC_CTL_ALARM_EN ? 1 : 0,
		       (u32)(ctl & RTC_CTL_IO_SEL_MASK) >> RTC_CTL_IO_SEL_SHIFT,
		       chip->alarm_io, chip->output_32k,
		       chip->clk_rate, chip->clk_drv,
		       chip->cal_fast ? "Fast" : "Slow", chip->cal_val);
	return 0;
}
static DEVICE_ATTR_RO(status);


static void aic_rtc_low_power(void __iomem *base)
{
	u8 val = 0;

	val |= RTC_ANA0_RC1M_EN | RTC_ANA0_LDO18_EN;
	val |= RTC_ANA0_LDO18_VOL_120 << RTC_ANA0_LDO18_VOL_SHIFT;
	RTC_WRITEB(val, RTC_REG_ANALOG0, base);

	val = RTC_ANA1_PD_CUR_SEL_075 << RTC_ANA1_PD_CUR_SEL_SHIFT;
	val |= RTC_ANA1_LDO11_VOL_090 << RTC_ANA1_LDO11_VOL_SHIFT;
	val |= RTC_ANA1_LDO11_LPEN;
	RTC_WRITEB(val, RTC_REG_ANALOG1, base);
}

static void aic_rtc_set_32k_drv(void __iomem *base, u8 drv)
{
	u8 val = 0;

	val = readb(base + RTC_REG_ANALOG2);
	val &= ~RTC_ANA2_XTAL32K_DRV_MASK;
	val |= drv;
	RTC_WRITEB(val, RTC_REG_ANALOG2, base);
}

static ssize_t driver_capability_show(struct device *dev,
				      struct device_attribute *devattr,
				      char *buf)
{
	struct aic_rtc_dev *chip = dev_get_drvdata(dev);
	void __iomem *base = chip->base;
	s8 drv[50] = "";
	s8 sta[50] = "";
	u32 i, detect = 0;

	init_completion(&chip->complete);

	for (i = 0; i <= RTC_ANA2_XTAL32K_DRV_MASK; i++) {
		aic_rtc_set_32k_drv(base, i);

		detect = readl(base + RTC_REG_32K_DET);
		detect |= RTC_32K_DET_EN;
		writel(detect, base + RTC_REG_32K_DET);

		sprintf(drv + strlen(drv), " %2d", i);
		if (!wait_for_completion_timeout(&chip->complete,
						 RTC_DRV_TIMEOUT)) {
			pr_debug("32K-clk driver %d is OK\n", i);
			sprintf(sta + strlen(sta), " OK");
		} else {
			pr_debug("32k-clk driver %d is failure\n", i);
			sprintf(sta + strlen(sta), "  -");
		}
	}

	return sprintf(buf, "The status of RTC driver:\nDriver%s\nStatus%s\n",
		       drv, sta);
}
static DEVICE_ATTR_RO(driver_capability);

static struct attribute *aic_rtc_attr[] = {
	&dev_attr_status.attr,
	&dev_attr_driver_capability.attr,
	NULL
};

static void aic_rtc_enable(struct aic_rtc_dev *chip, u32 enable)
{
	u8 val = 0;

	val = readb(chip->base + RTC_REG_CTL);
	if (enable)
		val |= RTC_CTL_TCNT_EN;
	else
		val &= ~RTC_CTL_TCNT_EN;
	RTC_WRITEB(val, RTC_REG_CTL, chip->base);
}

static int aic_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct aic_rtc_dev *chip = dev_get_drvdata(dev);
	u32 time = readl(chip->base + RTC_REG_TCNT);

	rtc_time64_to_tm(time, tm);
	dev_dbg(dev, "Get RTC time: %04d-%02d-%02d %02d:%02d:%02d\n",
		tm->tm_year, tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);
	return 0;
}

static int aic_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct aic_rtc_dev *chip = dev_get_drvdata(dev);
	u32 time = 0;

	aic_rtc_enable(chip, 0);
	time = (u32)rtc_tm_to_time64(tm);
	RTC_WRITEL(time, RTC_REG_TIME0, chip->base);
	RTC_WRITEB(1, RTC_REG_INIT, chip->base);
	aic_rtc_enable(chip, 1);

	dev_dbg(dev, "Set RTC time: %04d-%02d-%02d %02d:%02d:%02d\n",
		tm->tm_year, tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);
	return 0;
}

static int aic_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	struct aic_rtc_dev *chip = dev_get_drvdata(dev);
	u32 alarm_time, now;

	/* get alarm target time */
	alarm_time = RTC_READL(chip->base + RTC_REG_ALARM0);
	rtc_time64_to_tm(alarm_time, &alarm->time);

	/* check if alarm has triggered */
	now = readl(chip->base + RTC_REG_TCNT);
	alarm->pending = alarm_time > now;
	alarm->enabled = readb(chip->base + RTC_REG_IRQ_EN) & RTC_IRQ_ALARM_EN;

	return 0;
}

static int aic_rtc_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
	struct aic_rtc_dev *chip = dev_get_drvdata(dev);

	RTC_WRITEB(enabled ? (RTC_IRQ_32K_ERR_EN | RTC_IRQ_ALARM_EN) : 0,
	       RTC_REG_IRQ_EN, chip->base);
	return 0;
}

static int aic_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	struct aic_rtc_dev *chip = dev_get_drvdata(dev);
	time64_t alarm_time;
	u32 value;

	/* First, disable alarm */
	value = readb(chip->base + RTC_REG_CTL);
	value &= ~RTC_CTL_ALARM_EN;
	RTC_WRITEB(value, RTC_REG_CTL, chip->base);

	/* set alarm time value */
	alarm_time = rtc_tm_to_time64(&alarm->time);
	RTC_WRITEL((u32)alarm_time, RTC_REG_ALARM0, chip->base);
	dev_dbg(dev, "Set a alarm(%d): %04d-%02d-%02d %02d:%02d:%02d\n",
		alarm->enabled,
		alarm->time.tm_year, alarm->time.tm_mon,
		alarm->time.tm_mday, alarm->time.tm_hour,
		alarm->time.tm_min, alarm->time.tm_sec);

	/* Then, enable alarm */
	value |= RTC_CTL_ALARM_EN;
	RTC_WRITEB(value, RTC_REG_CTL, chip->base);

	/* enable alarm irq */
	aic_rtc_alarm_irq_enable(dev, 1);

	return 0;
}

static const struct rtc_class_ops aic_rtc_ops = {
	.read_time		= aic_rtc_read_time,
	.set_time		= aic_rtc_set_time,
	.read_alarm		= aic_rtc_read_alarm,
	.set_alarm		= aic_rtc_set_alarm,
	.alarm_irq_enable	= aic_rtc_alarm_irq_enable,
};

static irqreturn_t aic_rtc_irq(int irq, void *dev_id)
{
	struct aic_rtc_dev *chip = (struct aic_rtc_dev *)dev_id;
	struct device *dev = &chip->rtc_dev->dev;
	u8 val;

	val = readb(chip->base + RTC_REG_IRQ_STA);
	RTC_WRITEB(val, RTC_REG_IRQ_STA, chip->base);
	dev_dbg(dev, "IRQ status %#x\n", val);
	if (val & RTC_IRQ_STA_32K_ERR) {
		dev_err(dev, "The 32K clk is not fast enough");
		complete(&chip->complete);
	}

	if (val & RTC_IRQ_STA_ALARM) {
		rtc_update_irq(chip->rtc_dev, 1, RTC_AF | RTC_IRQF);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

/* calibrate: (100 * 1024 * 1024 + cval)/( freq / 32) = 1024
 * freq = 100 * actual OSC frequency
 */
static void aic_rtc_cali(struct aic_rtc_dev *chip)
{
	s32 cval = 0;

	if (chip->clk_rate == RTC_32K_FREQ)
		return; /* It's perfect, need not calibrate */

	cval = 100 * 1024 * 1024 - chip->clk_rate * 32;
	if (chip->clk_rate > RTC_32K_FREQ)
		cval -= 50;
	else
		cval += 50;

	cval /= 100;
	dev_dbg(&chip->rtc_dev->dev, "Calibration %d(clk %d)",
		cval, chip->clk_rate);
	chip->cal_val = cval;
	if (cval > 0) {
		cval |= RTC_CAL1_FAST_DIR << 8;
		chip->cal_fast = true;
	} else {
		cval = -cval;
	}

	RTC_WRITEB(cval >> 8, RTC_REG_CALI1, chip->base);
	RTC_WRITEB(cval & 0xFF, RTC_REG_CALI0, chip->base);
}

static int aic_rtc_parse_dt(struct device *dev, struct aic_rtc_dev *chip)
{
	u8 val = 0;
	u32 ret = 0;
	struct device_node *np = dev->of_node;

	chip->alarm_io = of_property_read_bool(np, "aic,alarm-io-output");
	if (chip->alarm_io) {
		/* Use power-gpios to replace alarm_io, if power on by alarm */
		chip->pwkey_gpio = of_get_named_gpio(np, "power-gpios", 0);
		if (gpio_is_valid(chip->pwkey_gpio)) {
			gpio_direction_output(chip->pwkey_gpio, 1);
			dev_info(dev, "Set power-gpios on.\n");
		}

		/* Check & clean poweroff alarm status */
		val = readb(chip->base + RTC_REG_IRQ_STA);
		if (val) {
			dev_dbg(dev, "IRQ_STA is %#x\n", val);
			RTC_WRITEB(val, RTC_REG_IRQ_STA, chip->base);
			if (val & RTC_IRQ_STA_ALARM_IO)
				dev_info(dev, "Powered by RTC alarm.\n");
		}

		/* RTC_IO = alarm output */
		val = readb(chip->base + RTC_REG_CTL);
		val |= RTC_CTL_IO_SEL(RTC_CTL_IO_ALARM_OUTPUT);
		RTC_WRITEB(val, RTC_REG_CTL, chip->base);
	} else {
		chip->output_32k =  of_property_read_bool(np,
							  "aic,32k-io-output");
		if (chip->output_32k) {
			val = readb(chip->base + RTC_REG_CTL);
			val |= RTC_CTL_IO_SEL(RTC_CTL_IO_32K_CLK_OUTPUT);
			RTC_WRITEB(val, RTC_REG_CTL, chip->base);
		}
	}

	ret = of_property_read_u32(np, "clock-rate", &chip->clk_rate);
	if (ret)
		dev_warn(dev, "Can't parse RTC clock-rate\n");
	else
		aic_rtc_cali(chip);

	ret = of_property_read_u32(np, "aic,clock-driver", &chip->clk_drv);
	if (ret)
		dev_warn(dev, "Can't parse RTC 32K clock driver\n");
	else
		aic_rtc_set_32k_drv(chip->base, chip->clk_drv);

	if (of_property_read_bool(np, "wakeup-source")) {
		device_init_wakeup(dev, true);
		dev_pm_set_wake_irq(dev, chip->irq);
	}

	return 0;
}

static int aic_rtc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct aic_rtc_dev *chip;
	struct resource *res;
	int ret;

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	chip->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(chip->base))
		return PTR_ERR(chip->base);

	g_rtc_base = chip->base;

	chip->rtc_dev = devm_rtc_allocate_device(dev);
	if (IS_ERR(chip->rtc_dev))
		return PTR_ERR(chip->rtc_dev);

	/* Before access RTC register, need to enable bus clock */
	chip->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(chip->clk))
		return PTR_ERR(chip->clk);

	ret = clk_prepare_enable(chip->clk);
	if (ret)
		return ret;

	ret = platform_get_irq(pdev, 0);
	if (ret < 0)
		return ret;

	chip->irq = ret;
	ret = devm_request_irq(dev, chip->irq, aic_rtc_irq, 0, AIC_RTC_NAME,
			       chip);
	if (ret < 0)
		return ret;

	aic_rtc_parse_dt(dev, chip);

	chip->attrs.attrs = aic_rtc_attr;
	ret = sysfs_create_group(&pdev->dev.kobj, &chip->attrs);
	if (ret)
		return ret;

	init_completion(&chip->complete);
	aic_rtc_enable(chip, 1);
	aic_rtc_low_power(chip->base);
	platform_set_drvdata(pdev, chip);

	chip->rtc_dev->ops = &aic_rtc_ops;
	chip->rtc_dev->range_max = U32_MAX;

	device_set_wakeup_capable(dev, 1);
	ret = rtc_register_device(chip->rtc_dev);
	if (ret)
		return ret;

	dev_info(dev, "Artinchip RTC loaded\n");
	return 0;
}

static int __maybe_unused aic_rtc_suspend(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);

	/* process some voltage configuration if needed */

	return 0;
}

static int __maybe_unused aic_rtc_resume(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);

	/* process some voltage configuration if needed */

	return 0;
}

static SIMPLE_DEV_PM_OPS(aic_rtc_pm_ops, aic_rtc_suspend, aic_rtc_resume);

static const struct of_device_id aic_rtc_match[] = {
	{ .compatible = "artinchip,aic-rtc-v1.0" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, aic_rtc_match);

static struct platform_driver aic_rtc_driver = {
	.probe		= aic_rtc_probe,
	.driver		= {
		.name		= AIC_RTC_NAME,
		.of_match_table = aic_rtc_match,
		.pm = &aic_rtc_pm_ops,
	},
};

module_platform_driver(aic_rtc_driver);
