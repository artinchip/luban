// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021, Artinchip Technology Co., Ltd
 * Author: Dehuang Wu <dehuang.wu@artinchip.com>
 *         Matteo <duanmt@artinchip.com>
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/reset.h>
#include <linux/moduleparam.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/watchdog.h>

#define AIC_WDT_NAME			"aic-wdt"

#define WDT_REG_CTL			(0x000)
#define WDT_REG_CNT			(0x004)
#define WDT_REG_IRQ_EN			(0x008)
#define WDT_REG_IRQ_STA			(0x00C)
#define WDT_REG_CLR_THD(n)		(0x040 + (n) * 0x10)
#define WDT_REG_IRQ_THD(n)		(0x044 + (n) * 0x10)
#define WDT_REG_RST_THD(n)		(0x048 + (n) * 0x10)
#define WDT_REG_OP			(0x0E8)
#define WDT_REG_VER			(0xFFC)

#define WDT_WR_DIS_SHIFT		28
#define WDT_WR_DIS_MASK			GENMASK(29, 28)
#define WDT_CFG_ID_SHIFT		24
#define WDT_CFG_ID_MASK			GENMASK(27, 24)
#define WDT_DBG_CNT_CONTINUE_SHIFT	1
#define WDT_CNT_EN			BIT(0)

#define WDT_OP_CNT_CLR_CMD0		0xA1C55555
#define WDT_OP_CNT_CLR_CMD1		0xA1CAAAAA
#define WDT_OP_CFG_SW_CMD0(n)		(0xA1C5A5A0 | (n))
#define WDT_OP_CFG_SW_CMD1(n)		(0xA1CA5A50 | (n))
#define WDT_OP_WR_EN_CMD0		0xA1C99999
#define WDT_OP_WR_EN_CMD1		0xA1C66666

#define WDT_SEC_TO_CNT(n)		((n) * 32000)
#define WDT_CNT_TO_SEC(n)		((n) / 32000)
#define WDT_CHAN_NUM			4
#define WDT_MAX_TIMEOUT			(60 * 60)
#define WDT_MIN_TIMEOUT			1
#define WDT_DEFAULT_TIMEOUT		10
#define DRV_NAME			"Artinchip Watchdog timer"

enum aic_wdt_wr_mode {
	WDT_WR_ENABLE = 0,
	WDT_WR_DISABLE = 1, // Only can write WDT_REG_OP
	WDT_WR_DISABLE_ALL = 3 // Only can reset
};

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0644);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started "
		 "(default=" __MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

struct aic_wdt {
	u32 clr_thd;
	u32 irq_thd;
	u32 rst_thd;
};

struct aic_wdt_dev {
	struct watchdog_device wdt_dev[WDT_CHAN_NUM];
	void __iomem *base;
	struct attribute_group attrs;
	struct clk *clk;
	struct reset_control *rst;
	u32 wdt_no;
	struct aic_wdt wdt[WDT_CHAN_NUM];
	bool dbg_continue;
	u32 clr_thd;
};

static void aic_wdt_op_clr(void __iomem *base, u32 thd)
{
	writel(WDT_OP_CNT_CLR_CMD0, base + WDT_REG_OP);
	writel(WDT_OP_CNT_CLR_CMD1, base + WDT_REG_OP);
}

static void aic_wdt_thd_set(void __iomem *base, u32 ch, struct aic_wdt *wdt)
{
	writel(wdt->clr_thd, base + WDT_REG_CLR_THD(ch));
	writel(wdt->irq_thd, base + WDT_REG_IRQ_THD(ch));
	writel(wdt->rst_thd, base + WDT_REG_RST_THD(ch));
}

static void aic_wdt_switch_chan(void __iomem *base, int chan)
{
	writel(WDT_OP_CFG_SW_CMD0(chan), base + WDT_REG_OP);
	writel(WDT_OP_CFG_SW_CMD1(chan), base + WDT_REG_OP);
}

static bool aic_wdt_is_running(void __iomem *base)
{
	u32 val = readl(base + WDT_REG_CTL);

	return val & WDT_CNT_EN;
}

static u32 aic_wdt_cur_id(void __iomem *base)
{
	u32 val = readl(base + WDT_REG_CTL);

	return (val & WDT_CFG_ID_MASK) >> WDT_CFG_ID_SHIFT;
}

static void aic_wdt_enable(void __iomem *base, u32 enable, bool dbg_continue)
{
	u32 val = 0;

	if (enable) {
		writel(WDT_CNT_EN
		       | (dbg_continue << WDT_DBG_CNT_CONTINUE_SHIFT),
		       base + WDT_REG_CTL);
		return;
	}

	val = readl(base + WDT_REG_CTL);
	val &= ~WDT_CNT_EN;
	writel(val, base + WDT_REG_CTL);
}

static void aic_wdt_irq_enable(void __iomem *base, bool enable)
{
	writel(enable, base + WDT_REG_IRQ_EN);
}

static ssize_t status_show(struct device *dev,
			   struct device_attribute *devattr, char *buf)
{
	struct aic_wdt_dev *aic_wdt = dev_get_drvdata(dev);
	void __iomem *base = aic_wdt->base;
	int ver = readl(base + WDT_REG_VER);

	return sprintf(buf, "In Watchdog V%d.%02d:\n"
		       "Module Enable: %d\n"
		       "Dbg continue: %d\n"
		       "clr_thd: %d\n"
		       "Write disable: %d\n"
		       "IRQ Enable: %d\n"
		       "Current chan: hw %d, sw %d\n"
		       "Current cnt: %d\n"
		       "chan clr_thd irq_thd rst_thd\n"
		       "   0 %7d %7d %7d\n"
		       "   1 %7d %7d %7d\n"
		       "   2 %7d %7d %7d\n"
		       "   3 %7d %7d %7d\n",
		       ver >> 8, ver & 0xFF, aic_wdt_is_running(base),
		       aic_wdt->dbg_continue, aic_wdt->clr_thd,
		       readl(base + WDT_REG_CTL) >> WDT_WR_DIS_SHIFT,
		       readl(base + WDT_REG_IRQ_EN),
		       aic_wdt_cur_id(base), aic_wdt->wdt_no,
		       readl(base + WDT_REG_CNT),
		       readl(base + WDT_REG_CLR_THD(0)),
		       readl(base + WDT_REG_IRQ_THD(0)),
		       readl(base + WDT_REG_RST_THD(0)),
		       readl(base + WDT_REG_CLR_THD(1)),
		       readl(base + WDT_REG_IRQ_THD(1)),
		       readl(base + WDT_REG_RST_THD(1)),
		       readl(base + WDT_REG_CLR_THD(2)),
		       readl(base + WDT_REG_IRQ_THD(2)),
		       readl(base + WDT_REG_RST_THD(2)),
		       readl(base + WDT_REG_CLR_THD(3)),
		       readl(base + WDT_REG_IRQ_THD(3)),
		       readl(base + WDT_REG_RST_THD(3)));
}
static DEVICE_ATTR_RO(status);

static ssize_t timeout_show(struct device *dev,
			    struct device_attribute *devattr, char *buf)
{
	struct aic_wdt_dev *aic_wdt = dev_get_drvdata(dev);
	struct watchdog_device *wdt_dev = &aic_wdt->wdt_dev[aic_wdt->wdt_no];

	return sprintf(buf, "%d\n", wdt_dev->timeout);
}
static DEVICE_ATTR_RO(timeout);

static ssize_t pretimeout_show(struct device *dev,
			       struct device_attribute *devattr, char *buf)
{
	struct aic_wdt_dev *aic_wdt = dev_get_drvdata(dev);
	struct watchdog_device *wdt_dev = &aic_wdt->wdt_dev[aic_wdt->wdt_no];

	return sprintf(buf, "%d\n", wdt_dev->pretimeout);
}
static DEVICE_ATTR_RO(pretimeout);

static ssize_t channel_show(struct device *dev,
			    struct device_attribute *devattr, char *buf)
{
	struct aic_wdt_dev *aic_wdt = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", aic_wdt->wdt_no);
}
static DEVICE_ATTR_RO(channel);

static struct attribute *aic_wdt_attr[] = {
	&dev_attr_status.attr,
	&dev_attr_timeout.attr,
	&dev_attr_pretimeout.attr,
	&dev_attr_channel.attr,
	NULL
};

static int aic_wdt_ping(struct watchdog_device *wdt_dev)
{
	struct aic_wdt_dev *aic_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *base = aic_wdt->base;
	struct aic_wdt *wdt = &aic_wdt->wdt[aic_wdt->wdt_no];

	aic_wdt_op_clr(base, wdt->clr_thd);
	dev_dbg(wdt_dev->parent, "%s() Clear the watchdog %d\n",
		__func__, wdt_dev->id);
	return 0;
}

static int aic_wdt_set_timeout(struct watchdog_device *wdt_dev,
			       unsigned int timeout)
{
	struct aic_wdt_dev *aic_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *base = aic_wdt->base;
	struct aic_wdt *wdt = &aic_wdt->wdt[aic_wdt->wdt_no];
	u32 prev_clr_thd = wdt->clr_thd;
	bool changed = false;

	if (aic_wdt->wdt_no != wdt_dev->id) {
		dev_info(wdt_dev->parent, "Change chan%d -> %d\n",
			 aic_wdt->wdt_no, wdt_dev->id);
		changed = true;
		aic_wdt->wdt_no = wdt_dev->id;
		wdt = &aic_wdt->wdt[aic_wdt->wdt_no];
	}

	dev_dbg(wdt_dev->parent, "%s() Set chan%d timeout: %d\n", __func__,
		wdt_dev->id, timeout);
	wdt_dev->timeout = timeout;
	wdt->clr_thd = aic_wdt->clr_thd;
	wdt->rst_thd = WDT_SEC_TO_CNT(timeout);

	aic_wdt_thd_set(base, aic_wdt->wdt_no, wdt);
	if (changed)
		aic_wdt_switch_chan(base, aic_wdt->wdt_no);
	aic_wdt_op_clr(base, prev_clr_thd);
	return 0;
}

static int aic_wdt_set_pretimeout(struct watchdog_device *wdt_dev,
				  unsigned int pretimeout)
{
	struct aic_wdt_dev *aic_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *base = aic_wdt->base;
	struct aic_wdt *wdt = &aic_wdt->wdt[aic_wdt->wdt_no];
	u32 prev_clr_thd = wdt->clr_thd;
	bool changed = false;

	if (aic_wdt->wdt_no != wdt_dev->id) {
		changed = true;
		aic_wdt->wdt_no = wdt_dev->id;
		wdt = &aic_wdt->wdt[aic_wdt->wdt_no];
	}

	dev_dbg(wdt_dev->parent, "%s() Set chan%d pretimeout: %d\n",
		__func__, wdt_dev->id, pretimeout);
	wdt_dev->pretimeout = pretimeout;
	wdt->irq_thd = WDT_SEC_TO_CNT(pretimeout);

	aic_wdt_thd_set(base, aic_wdt->wdt_no, wdt);
	if (changed)
		aic_wdt_switch_chan(base, aic_wdt->wdt_no);
	aic_wdt_op_clr(base, prev_clr_thd);
	aic_wdt_irq_enable(base, 1);
	return 0;
}

static int aic_wdt_stop(struct watchdog_device *wdt_dev)
{
	struct aic_wdt_dev *aic_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *base = aic_wdt->base;

	dev_dbg(wdt_dev->parent, "%s() Stop chan%d\n", __func__, wdt_dev->id);
	aic_wdt_enable(base, 0, aic_wdt->dbg_continue);
	aic_wdt_irq_enable(base, 0);
	return 0;
}

static int aic_wdt_start(struct watchdog_device *wdt_dev)
{
	struct aic_wdt_dev *aic_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *base = aic_wdt->base;

	dev_dbg(wdt_dev->parent, "%s() Start chan%d\n", __func__, wdt_dev->id);
	if (aic_wdt_is_running(base))
		return aic_wdt_ping(wdt_dev);

	aic_wdt_enable(base, 1, aic_wdt->dbg_continue);
	aic_wdt_set_timeout(wdt_dev, wdt_dev->timeout);
	return 0;
}

/*
 * Function is called to restart system.
 */
static int aic_wdt_restart(struct watchdog_device *wdt_dev,
			   unsigned long action, void *data)
{
	dev_dbg(wdt_dev->parent, "%s() Set chan%d to reset system\n",
		 __func__, wdt_dev->id);
	wdt_dev->timeout = 0; /* 0s to Reset system */
	aic_wdt_start(wdt_dev);

	/* Wait until hardware reset */
	mdelay(1000);
	dev_err(wdt_dev->parent, "%s() Failed to reset system\n", __func__);

	return 0;
}

static irqreturn_t aic_wdt_isr(int irq, void *arg)
{
	struct aic_wdt_dev *aic_wdt = arg;
	struct watchdog_device *wdt_dev = &aic_wdt->wdt_dev[aic_wdt->wdt_no];

	writel(1, aic_wdt->base + WDT_REG_IRQ_STA);
	watchdog_notify_pretimeout(wdt_dev);
	dev_dbg(wdt_dev->parent, "%s() IRQ timeout happened\n", __func__);

	return IRQ_HANDLED;
}

static const struct watchdog_info aic_wdt_info = {
	.identity	= DRV_NAME,
	.options	= WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING |
				WDIOF_MAGICCLOSE | WDIOF_PRETIMEOUT,
};

static const struct watchdog_ops aic_wdt_ops = {
	.owner		= THIS_MODULE,
	.start		= aic_wdt_start,
	.stop		= aic_wdt_stop,
	.ping		= aic_wdt_ping,
	.set_timeout	= aic_wdt_set_timeout,
	.set_pretimeout = aic_wdt_set_pretimeout,
	.restart	= aic_wdt_restart,
};

static const struct of_device_id aic_wdt_match[] = {
	{ .compatible = "artinchip,aic-wdt-v1.0"},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, aic_wdt_match);

static int aic_wdt_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct aic_wdt_dev *aic_wdt;
	int err, irq, i;

	aic_wdt = devm_kzalloc(dev, sizeof(*aic_wdt), GFP_KERNEL);
	if (!aic_wdt)
		return -EINVAL;

	aic_wdt->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(aic_wdt->base))
		return PTR_ERR(aic_wdt->base);

	aic_wdt->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(aic_wdt->clk)) {
		dev_err(dev, "%s() Failed to get clk\n", __func__);
		return PTR_ERR(aic_wdt->clk);
	}
	clk_prepare_enable(aic_wdt->clk);

	aic_wdt->rst = devm_reset_control_get_optional_shared(&pdev->dev, NULL);
	if (IS_ERR(aic_wdt->rst)) {
		dev_err(dev, "%s() Failed to get reset\n", __func__);
		err = PTR_ERR(aic_wdt->rst);
		goto out_disable_clk;
	}
	reset_control_deassert(aic_wdt->rst);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "%s() Failed to get IRQ\n", __func__);
		err = irq;
		goto out_disable_rst;
	}
	err = devm_request_irq(dev, irq, aic_wdt_isr, 0, AIC_WDT_NAME,
			       aic_wdt);
	if (err < 0) {
		dev_err(dev, "%s() Failed to request IRQ\n", __func__);
		goto out_disable_rst;
	}

	aic_wdt->dbg_continue = device_property_read_bool(dev, "dbg_continue");
	if (device_property_read_u32(dev, "clr_thd", &aic_wdt->clr_thd) == 0)
		aic_wdt->clr_thd = WDT_SEC_TO_CNT(aic_wdt->clr_thd);

	aic_wdt->attrs.attrs = aic_wdt_attr;
	err = sysfs_create_group(&pdev->dev.kobj, &aic_wdt->attrs);
	if (err)
		goto out_disable_rst;

	for (i = 0; i < WDT_CHAN_NUM; i++) {
		aic_wdt->wdt_dev[i].info = &aic_wdt_info;
		aic_wdt->wdt_dev[i].ops = &aic_wdt_ops;
		aic_wdt->wdt_dev[i].timeout = WDT_DEFAULT_TIMEOUT;
		aic_wdt->wdt_dev[i].max_timeout = WDT_MAX_TIMEOUT;
		aic_wdt->wdt_dev[i].min_timeout = WDT_MIN_TIMEOUT;
		aic_wdt->wdt_dev[i].parent = dev;

		watchdog_init_timeout(&aic_wdt->wdt_dev[i],
				      WDT_DEFAULT_TIMEOUT, dev);
		watchdog_set_nowayout(&aic_wdt->wdt_dev[i], nowayout);
		watchdog_set_restart_priority(&aic_wdt->wdt_dev[i], 128);
		watchdog_set_drvdata(&aic_wdt->wdt_dev[i], aic_wdt);

		watchdog_stop_on_reboot(&aic_wdt->wdt_dev[i]);
		err = devm_watchdog_register_device(dev, &aic_wdt->wdt_dev[i]);
		if (unlikely(err))
			goto out_disable_rst;

		dev_dbg(dev, "Register watchdog%d\n", aic_wdt->wdt_dev[i].id);
	}

	dev_set_drvdata(dev, aic_wdt);
	dev_info(dev, "Artinchip watchdog loaded\n");
	return 0;

out_disable_rst:
	reset_control_assert(aic_wdt->rst);
out_disable_clk:
	clk_disable_unprepare(aic_wdt->clk);
	return err;
}

static struct platform_driver aic_wdt_driver = {
	.probe		= aic_wdt_probe,
	.driver		= {
		.name		= AIC_WDT_NAME,
		.of_match_table	= aic_wdt_match,
	},
};

module_platform_driver(aic_wdt_driver);

MODULE_AUTHOR("Dehuang Wu <dehuang.wu@artinchip.com>");
MODULE_DESCRIPTION("Driver for Artinchip watchdog timer");
MODULE_LICENSE("GPL");
