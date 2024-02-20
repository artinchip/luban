// SPDX-License-Identifier: GPL-2.0-only
/*
 * ADCIM driver of Artinchip SoC
 *
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
 * Authors:  Matteo <duanmt@artinchip.com>
 */
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/reset.h>
#include <linux/of.h>
#include <linux/delay.h>

/* Register of ADCIM */
#define ADCIM_MCSR       0x000
#define ADCIM_CALCSR     0x004
#define ADCIM_FIFOSTS    0x008
#define ADCIM_VERSION    0xFFC

#define ADCIM_MCSR_BUSY			BIT(16)
#define ADCIM_MCSR_SEMFLAG_SHIFT	8
#define ADCIM_MCSR_SEMFLAG_MASK		GENMASK(15, 8)
#define ADCIM_MCSR_CHN_MASK		GENMASK(3, 0)
#define ADCIM_CALCSR_CALVAL_UPD		BIT(31)
#define ADCIM_CALCSR_CALVAL_SHIFT	16
#define ADCIM_CALCSR_CALVAL_MASK	GENMASK(27, 16)
#define ADCIM_CALCSR_ADC_ACQ_SHIFT	8
#define ADCIM_CALCSR_ADC_ACQ_MASK	GENMASK(15, 8)
#define ADCIM_CALCSR_DCAL_MASK		BIT(1)
#define ADCIM_CALCSR_CAL_ENABLE		BIT(0)
#define ADCIM_FIFOSTS_ADC_ARBITER_IDLE	BIT(6)
#define ADCIM_FIFOSTS_FIFO_ERR		BIT(5)
#define ADCIM_FIFOSTS_CTR_MASK		GENMASK(4, 0)
#define ADCDM_CAL_ADC_STANDARD_VAL      0x800
#define ADCIM_CAL_ADC_VAL_OFFSET        0X32

#ifdef CONFIG_ARTINCHIP_ADCIM_DM
#define ADCDM_RTP_CFG	0x03F0
#define ADCDM_RTP_STS	0x03F4
#define ADCDM_SRAM_CTL	0x03F8
#define ADCDM_SRAM_BASE	0x0400

#define ADCDM_RTP_CAL_VAL_SHIFT		16
#define ADCDM_RTP_CAL_VAL_MASK		GENMASK(27, 16)
#define ADCDM_RTP_PDET			BIT(0)

#define ADCDM_RTP_DRV_SHIFT		4
#define ADCDM_RTP_DRV_MASK		GENMASK(7, 4)
#define ADCDM_RTP_VPSEL_SHIFT		2
#define ADCDM_RTP_VPSEL_MASK		GENMASK(3, 2)
#define ADCDM_RTP_VNSEL_MASK		GENMASK(1, 0)

#define ADCDM_SRAM_CLR_SHIFT		16
#define ADCDM_SRAM_CLR(n)		(1 << ((n) + ADCDM_SRAM_CLR_SHIFT))
#define ADCDM_SRAM_MODE_SHIFT		8
#define ADCDM_SRAM_MODE			BIT(8)
#define ADCDM_SRAM_SEL_MASK		GENMASK(3, 0)
#define ADCDM_SRAM_SEL(n)		(n)

#define ADCDM_SRAM_SIZE			(512 * 4)
#define ADCDM_CHAN_NUM			16

enum adcdm_sram_mode {
	ADCDM_NORMAL_MODE,
	ADCDM_DEBUG_MODE
};
#endif

struct adcim_dev {
	struct attribute_group attrs;
	void __iomem *regs;
#ifdef CONFIG_ARTINCHIP_ADCIM_DM
	u32 dm_cur_chan;
#endif
	struct platform_device *pdev;
	struct clk *clk;
	struct reset_control *rst;
	int usr_cnt;
};
static struct adcim_dev *g_adcim_dev;

static DEFINE_SPINLOCK(user_lock);

void adcim_request(void)
{
	spin_lock(&user_lock);
	g_adcim_dev->usr_cnt++;
	spin_unlock(&user_lock);
}
EXPORT_SYMBOL(adcim_request);

void adcim_release(void)
{
	spin_lock(&user_lock);
	g_adcim_dev->usr_cnt--;
	spin_unlock(&user_lock);
}
EXPORT_SYMBOL(adcim_release);

int adcim_set_calibration(unsigned int val)
{
	int cal;

	if (val > 2048) {
		pr_err("The calibration value %d is too big\n", val);
		return -EINVAL;
	}

	spin_lock(&user_lock);
	cal = readl(g_adcim_dev->regs + ADCIM_CALCSR);
	cal = (cal & ~ADCIM_CALCSR_CALVAL_MASK)
		| (val << ADCIM_CALCSR_CALVAL_SHIFT);
	cal = cal | ADCIM_CALCSR_CALVAL_UPD;
	writel(cal, g_adcim_dev->regs + ADCIM_CALCSR);
	spin_unlock(&user_lock);

	return 0;
}
EXPORT_SYMBOL(adcim_set_calibration);

static ssize_t status_show(struct device *dev,
				 struct device_attribute *devattr,
				 char *buf)
{
	int mcsr;
	int fifo;
	int version;
	struct adcim_dev *adcim = dev_get_drvdata(dev);
	void __iomem *regs = adcim->regs;
#ifdef CONFIG_ARTINCHIP_ADCIM_DM
	int rtp_cfg, rtp_sts, sram_ctl;
#endif

	spin_lock(&user_lock);
	mcsr = readl(regs + ADCIM_MCSR);
	fifo = readl(regs + ADCIM_FIFOSTS);
	version = readl(regs + ADCIM_VERSION);

#ifdef CONFIG_ARTINCHIP_ADCIM_DM
	rtp_cfg = readl(regs + ADCDM_RTP_CFG);
	rtp_sts = readl(regs + ADCDM_RTP_STS);
	sram_ctl = readl(regs + ADCDM_SRAM_CTL);
#endif
	spin_unlock(&user_lock);

	return sprintf(buf, "In ADCIM V%d.%02d:\n"
		       "Busy state: %d\n"
		       "Semflag: %ld\n"
		       "Current Channel: %ld\n"
		       "ADC Arbiter Idel: %d\n"
		       "FIFO Error: %d\n"
		       "FIFO Counter: %ld\n"
#ifdef CONFIG_ARTINCHIP_ADCIM_DM
		       "\nIn DM:\nMode: %s, Current channel: %d/%ld\n"
		       "Calibration val: %ld\n"
		       "RTP PDET: %ld, DRV: %ld, VPSEL: %ld, VNSEL: %ld\n"
#endif
		       , version >> 8, version & 0xff,
		       (mcsr & ADCIM_MCSR_BUSY) ? 1 : 0,
		       (mcsr & ADCIM_MCSR_SEMFLAG_MASK)
				>> ADCIM_MCSR_SEMFLAG_SHIFT,
		       mcsr & ADCIM_MCSR_CHN_MASK,
		       fifo & ADCIM_FIFOSTS_ADC_ARBITER_IDLE ? 1 : 0,
		       fifo & ADCIM_FIFOSTS_FIFO_ERR ? 1 : 0,
		       fifo & ADCIM_FIFOSTS_CTR_MASK
#ifdef CONFIG_ARTINCHIP_ADCIM_DM
		       , sram_ctl & ADCDM_SRAM_MODE ? "Debug" : "Normal",
		       adcim->dm_cur_chan, sram_ctl & ADCDM_SRAM_SEL_MASK,
		       (rtp_cfg & ADCDM_RTP_CAL_VAL_MASK)
				>> ADCDM_RTP_CAL_VAL_SHIFT,
		       rtp_cfg & ADCDM_RTP_PDET,
		       (rtp_sts & ADCDM_RTP_DRV_MASK) >> ADCDM_RTP_DRV_SHIFT,
		       (rtp_sts & ADCDM_RTP_VPSEL_MASK)
				>> ADCDM_RTP_VPSEL_SHIFT,
		       rtp_sts & ADCDM_RTP_VNSEL_MASK
#endif
		       );
}
static DEVICE_ATTR_RO(status);

static ssize_t version_show(struct device *dev,
			    struct device_attribute *devattr, char *buf)
{
	int version;
	struct adcim_dev *adcim = dev_get_drvdata(dev);
	void __iomem *regs = adcim->regs;

	version = readl(regs + ADCIM_VERSION);

	return sprintf(buf, "%d.%02d\n", version >> 8, version & 0xff);
}
static DEVICE_ATTR_RO(version);

static ssize_t calibration_show(struct device *dev,
				struct device_attribute *devattr,
				char *buf)
{
	int cal;

	spin_lock(&user_lock);
	cal = readl(g_adcim_dev->regs + ADCIM_CALCSR);
	spin_unlock(&user_lock);

	return sprintf(buf, "Calibration Enable: %d\n \
Current value: %ld\n \
ADC ACQ: %ld\n", (cal & ADCIM_CALCSR_CAL_ENABLE) ? 0 : 1,
	       (cal & ADCIM_CALCSR_CALVAL_MASK) >> ADCIM_CALCSR_CALVAL_SHIFT,
	       (cal & ADCIM_CALCSR_ADC_ACQ_MASK) >> ADCIM_CALCSR_ADC_ACQ_SHIFT);
}

static ssize_t calibration_store(struct device *dev,
				struct device_attribute *devattr,
				const char *buf, size_t count)
{
	int ret;
	unsigned long val = 0;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return ret;

	ret = adcim_set_calibration(val);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(calibration);

u16 adcim_auto_calibration(u16 adc_val, struct device *dev)
{
	u32 flag = 1;
	u32 data = 0;
	u16 caled_adc_value;
	int count = 0;
	void __iomem *regs = g_adcim_dev->regs;

	writel(0x083F2f03, regs + ADCIM_CALCSR);//auto cal
	do {
		flag = readl(regs + ADCIM_CALCSR) & 0x00000001;
		count++;
		if (count > 10000) {
			dev_err(dev, "Adcim auto calibration parameter acquisition timeout");
			return adc_val;
		}
	} while (flag);

	data = (readl(regs + ADCIM_CALCSR) >> ADCIM_CALCSR_CALVAL_SHIFT) & 0xfff;
	caled_adc_value = adc_val + ADCDM_CAL_ADC_STANDARD_VAL - data + ADCIM_CAL_ADC_VAL_OFFSET;

	return caled_adc_value;
}
EXPORT_SYMBOL(adcim_auto_calibration);

#ifdef CONFIG_ARTINCHIP_ADCIM_DM
static ssize_t dm_chan_show(struct device *dev,
			    struct device_attribute *devattr,
			    char *buf)
{
	struct adcim_dev *adcim = dev_get_drvdata(dev);

	return sprintf(buf, "Current chan: %d/%ld\n", adcim->dm_cur_chan,
		       readl(adcim->regs + ADCDM_SRAM_CTL)
		       & ADCDM_SRAM_SEL_MASK);
}

static ssize_t dm_chan_store(struct device *dev,
			     struct device_attribute *devattr,
			     const char *buf, size_t count)
{
	int ret;
	unsigned long val = 0;
	struct adcim_dev *adcim = dev_get_drvdata(dev);

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return 0;

	if (val >= ADCDM_CHAN_NUM) {
		dev_err(dev, "Invalid channel number %lu", val);
		return 0;
	}

	spin_lock(&user_lock);
	adcim->dm_cur_chan = val;
	ret = readl(adcim->regs + ADCDM_SRAM_CTL);
	ret &= ~ADCDM_SRAM_SEL_MASK;
	ret |= val;
	writel(ret, adcim->regs + ADCDM_SRAM_CTL);
	spin_unlock(&user_lock);

	return count;
}
static DEVICE_ATTR_RW(dm_chan);

static ssize_t rtp_down_store(struct device *dev,
			      struct device_attribute *devattr,
			      const char *buf, size_t count)
{
	int ret;
	unsigned long val = 0;
	struct adcim_dev *adcim = dev_get_drvdata(dev);

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return 0;

	spin_lock(&user_lock);
	ret = readl(adcim->regs + ADCDM_RTP_CFG);
	if (val)
		ret &= ~ADCDM_RTP_PDET;
	else
		ret |= ADCDM_RTP_PDET;
	writel(ret, adcim->regs + ADCDM_RTP_CFG);
	spin_unlock(&user_lock);

	return count;
}

static DEVICE_ATTR_WO(rtp_down);
#endif

static struct attribute *adcim_attr[] = {
	&dev_attr_status.attr,
	&dev_attr_calibration.attr,
	&dev_attr_version.attr,
#ifdef CONFIG_ARTINCHIP_ADCIM_DM
	&dev_attr_dm_chan.attr,
	&dev_attr_rtp_down.attr,
#endif
	NULL
};

#ifdef CONFIG_ARTINCHIP_ADCIM_DM

static void adcdm_sram_clear(u32 chan)
{
	u32 val = 0;
	void __iomem *regs = g_adcim_dev->regs;

	spin_lock(&user_lock);

	val = readl(regs + ADCDM_SRAM_CTL);
	val |= ADCDM_SRAM_CLR(chan);
	writel(val, regs + ADCDM_SRAM_CTL);

	udelay(10);
	val &= ~ADCDM_SRAM_CLR(chan);
	writel(val, regs + ADCDM_SRAM_CTL);

	spin_unlock(&user_lock);
}

static void adcdm_sram_mode(enum adcdm_sram_mode mode)
{
	u32 val = 0;
	void __iomem *regs = g_adcim_dev->regs;

	spin_lock(&user_lock);

	val = readl(regs + ADCDM_SRAM_CTL);
	if (mode)
		val |= mode << ADCDM_SRAM_MODE_SHIFT;
	else
		val &= ~ADCDM_SRAM_MODE;
	writel(val, regs + ADCDM_SRAM_CTL);

	spin_unlock(&user_lock);
}

static void adcdm_sram_select(u32 chan)
{
	u32 val = 0;
	void __iomem *regs = g_adcim_dev->regs;

	spin_lock(&user_lock);

	val = readl(regs + ADCDM_SRAM_CTL);
	val &= ~ADCDM_SRAM_SEL_MASK;
	val |= chan;
	writel(val, regs + ADCDM_SRAM_CTL);

	spin_unlock(&user_lock);
}

static ssize_t adcdm_sram_write(struct file *filp, struct kobject *kobj,
				struct bin_attribute *attr, char *buf,
				loff_t offset, size_t count)
{
	int i;
	int *data = (int *)buf;

	if (count + offset > ADCDM_SRAM_SIZE)
		return 0;

	adcdm_sram_mode(ADCDM_DEBUG_MODE);
	adcdm_sram_select(g_adcim_dev->dm_cur_chan);

	for (i = 0; i < count / 4; i++)
		writel(*data++, g_adcim_dev->regs + ADCDM_SRAM_BASE + i * 4);

	adcdm_sram_mode(ADCDM_NORMAL_MODE);
	for (i = 0; i < ADCDM_CHAN_NUM; i++)
		adcdm_sram_clear(i);

	return count;
}

static struct bin_attribute sram_attr = {
	.attr = {.name = "sram", .mode = (0200)},
	.read = NULL,
	.write = adcdm_sram_write,
	.mmap = NULL,
};
#endif

static int adcim_probe(struct platform_device *pdev)
{
	int ret;
	struct adcim_dev *adcim;

	adcim = devm_kzalloc(&pdev->dev, sizeof(struct adcim_dev), GFP_KERNEL);
	if (!adcim)
		return -ENOMEM;
	adcim->pdev = pdev;

	adcim->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(adcim->regs))
		return PTR_ERR(adcim->regs);

	adcim->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(adcim->clk)) {
		dev_err(&pdev->dev, "no clock defined\n");
		return PTR_ERR(adcim->clk);
	}

	ret = clk_prepare_enable(adcim->clk);
	if (ret)
		return ret;

	adcim->rst = devm_reset_control_get_optional_shared(&pdev->dev, NULL);
	if (IS_ERR(adcim->rst)) {
		ret = PTR_ERR(adcim->rst);
		goto disable_clk;
	}
	reset_control_deassert(adcim->rst);

	adcim->attrs.attrs = adcim_attr;
	ret = sysfs_create_group(&pdev->dev.kobj, &adcim->attrs);
	if (ret)
		goto disable_rst;

#ifdef CONFIG_ARTINCHIP_ADCIM_DM
	ret = device_create_bin_file(&pdev->dev, &sram_attr);
	if (ret)
		goto disable_rst;
#endif

	adcim->usr_cnt = 0;
	dev_info(&pdev->dev, "Artinchip ADCIM Loaded\n");
	platform_set_drvdata(pdev, adcim);
	g_adcim_dev = adcim;
	return 0;

disable_rst:
	reset_control_assert(adcim->rst);
disable_clk:
	clk_disable_unprepare(adcim->clk);
	return ret;
}

static int adcim_remove(struct platform_device *pdev)
{
	struct adcim_dev *adcim = platform_get_drvdata(pdev);

#ifdef CONFIG_ARTINCHIP_ADCIM_DM
	device_remove_bin_file(&pdev->dev, &sram_attr);
#endif

	spin_lock(&user_lock);
	if (adcim->usr_cnt == 0) {
		reset_control_assert(adcim->rst);
		clk_disable_unprepare(adcim->clk);
	}
	spin_unlock(&user_lock);
	return 0;
}

static const struct of_device_id aic_adcim_dt_ids[] = {
	{.compatible = "artinchip,aic-adcim-v1.0"},
	{}
};
MODULE_DEVICE_TABLE(of, aic_adcim_dt_ids);

static struct platform_driver adcim_driver = {
	.driver		= {
		.name		= "adcim",
		.of_match_table	= of_match_ptr(aic_adcim_dt_ids),
	},
	.probe		= adcim_probe,
	.remove		= adcim_remove,
};

/* Must init ADCIM before ADC/THS/RTP, so use postcore_initcall() */
static int __init adcim_init(void)
{
	return platform_driver_register(&adcim_driver);
}
postcore_initcall(adcim_init);

MODULE_AUTHOR("Matteo <duanmt@artinchip.com>");
MODULE_DESCRIPTION("ADCIM driver of Artinchip SoC");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:adcim");
