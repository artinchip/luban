// SPDX-License-Identifier: GPL-2.0-only
/*
 * Thermal Sensor driver of Artinchip SoC
 *
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
 * Authors:  Matteo <duanmt@artinchip.com>
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/reset.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/thermal.h>
#include <linux/nvmem-consumer.h>

#define AIC_TSEN_NAME		"aic-tsen"
#define AIC_TSEN_MAX_CH		4
#define AIC_TSEN_TIMEOUT	msecs_to_jiffies(10000)

enum aic_tsen_ch_id {
	AIC_TSEN_CH_CPU = 0,
	AIC_TSEN_CH_GPAI = 1,
};

struct aic_tsen_ch_dat {
	char name[16];
	int slope;	// 10000 * actual slope
	int offset;	// 10000 * actual offset
};

/* tsen compile-time platform data */
struct aic_tsen_plat_data {
	const u32		num;
	struct aic_tsen_ch_dat	ch[AIC_TSEN_MAX_CH];
};

enum aic_tsen_mode {
	AIC_TSEN_MODE_SINGLE = 0,
	AIC_TSEN_MODE_PERIOD = 1
};

/* Register definition of Thermal Sensor Controller */
#define TSEN_MCR	0x000
#define TSEN_INTR	0x004
#define TSENn_CFG(n)	(0x100 + (((n) & 0x3) << 5))
#define TSENn_ITV(n)	(0x100 + (((n) & 0x3) << 5) + 0x4)
#define TSENn_FIL(n)	(0x100 + (((n) & 0x3) << 5) + 0x8)
#define TSENn_DATA(n)	(0x100 + (((n) & 0x3) << 5) + 0xC)
#define TSENn_INT(n)	(0x100 + (((n) & 0x3) << 5) + 0x10)
#define TSENn_LTAV(n)	(0x100 + (((n) & 0x3) << 5) + 0x14)
#define TSENn_HTAV(n)	(0x100 + (((n) & 0x3) << 5) + 0x18)
#define TSENn_OTPV(n)	(0x100 + (((n) & 0x3) << 5) + 0x1C)
#define TSEN_VERSION	0xFFC

#define TSEN_MCR_CH0_EN			BIT(16)
#define TSEN_MCR_CH_EN(n)		(TSEN_MCR_CH0_EN << (n))
#define TSEN_MCR_ACQ_SHIFT		8
#define TSEN_MCR_ACQ_MASK		GENMASK(15, 8)
#define TSEN_MCR_MAX_SHIFT		4
#define TSEN_MCR_MAX_MASK		GENMASK(5, 4)
#define TSEN_MCR_EN			BIT(0)

#define TSEN_INTR_CH_INT_FLAG(n)	(BIT(16) << (n))
#define TSEN_INTR_CH_INT_EN(n)		(BIT(0) << (n))

#define TSENn_CFG_HIGH_ADC_PRIORITY	BIT(4)
#define TSENn_CFG_PERIOD_SAMPLE_EN	BIT(1)
#define TSENn_CFG_SINGLE_SAMPLE_EN	BIT(0)

#define TSENn_ITV_SHIFT			16

#define TSENn_FIL_2_POINTS		1
#define TSENn_FIL_4_POINTS		2
#define TSENn_FIL_8_POINTS		3

#define TSENn_INT_OTP_FLAG		BIT(28)
#define TSENn_INT_HTA_RM_FLAG		BIT(27)
#define TSENn_INT_HTA_VALID_FLAG	BIT(26)
#define TSENn_INT_LTA_RM_FLAG		BIT(25)
#define TSENn_INT_LTA_VALID_FLAG	BIT(24)
#define TSENn_INT_DAT_OVW_FLAG		BIT(17)
#define TSENn_INT_DAT_RDY_FLAG		BIT(16)
#define TSENn_INT_OTP_RESET		(0xA << 12)
#define TSENn_INT_OTP_IE		(0x5 << 12)
#define TSENn_INT_HTA_RM_IE		BIT(11)
#define TSENn_INT_HTA_VALID_IE		BIT(10)
#define TSENn_INT_LTA_RM_IE		BIT(9)
#define TSENn_INT_LTA_VALID_IE		BIT(8)
#define TSENn_INT_DAT_OVW_IE		BIT(1)
#define TSENn_INT_DATA_RDY_IE		BIT(0)

#define TSENn_HLTA_EN			BIT(31)
#define TSENn_HLTA_RM_THD_SHIFT		16
#define TSENn_HLTA_RM_THD_MASK		GENMASK(27, 16)
#define TSENn_HLTA_THD_MASK		GENMASK(11, 0)
#define TSENn_HTA_RM_THD(t)		((t) - 3)
#define TSENn_LTA_RM_THD(t)		((t) + 3)

#define TSENn_OTPV_EN			BIT(31)
#define TSENn_OTPV_VAL_MASK		GENMASK(11, 0)

#define TSEN_NVMEM_CELL_NUM			8

#define TSEN_THS0_ADC_VAL_LOW			0
#define TSEN_THS1_ADC_VAL_LOW			1
#define TSEN_THS0_ADC_VAL_HIGH			2
#define TSEN_THS1_ADC_VAL_HIGH			3
#define TSEN_THS_ENV_TEMP_LOW			4
#define TSEN_THS_ENV_TEMP_HIGH			5
#define TSEN_LDO30_BG_CTRL			6
#define TSEN_CP_VERSION				7
#define TSEN_ENV_TEMP_LOW_SIGN_MASK		BIT(3)
#define TSEN_ENV_TEMP_HIGH_SIGN_MASK		BIT(7)
#define TSEN_ENV_TEMP_LOW_BASE			25
#define TSEN_ENV_TEMP_HIGH_BASE			65

#define TSEN_VOLTAGE_SCALE_UNIT			2.14285
#define TSEN_TRIM_VOLTAGE_BOUNDARY_VAL		0x80
#define TSEN_ORIGIN_STANDARD_VOLTAGE		3000 // = 3 * 1000

#define TSEN_CP_VERSION_DIFF_TYPE		0xA
#define TSEN_SINGLE_POINT_CALI_K		-1151

#define TSEN_CPU_ZONE_TRIPS_NUM			1
#define TSEN_GPAI_ZONE_TRIPS_NUM		0

#define THERMAL_CORE_TEMP_AMPN_SCALE		1000
#define TSEN_CALIB_ACCURACY_SCALE		10 // = 10000 / 1000

struct aic_tsen_ch {
	bool available;
	enum aic_tsen_mode mode;
	u32 latest_data; // 1000 * actual temperature value
	u32 smp_period;  // in seconds

	bool hta_enable; // high temperature alarm
	u32 hta_thd;	 // 1000 * temperature value
	u32 hta_rm_thd;	 // 1000 * temperature value
	bool lta_enable; // low temperature alarm
	u32 lta_thd;	 // 1000 * temperature value
	u32 lta_rm_thd;	 // 1000 * temperature value
	bool otp_enable; // over temperature protection
	u32 otp_thd;	 // 1000 * temperature value
	bool htp_enable; // high temperature protection

	struct completion complete;
	struct thermal_zone_device *zone;
};

/* Aic Thermal Sensor Dev Structure */
struct aic_tsen_dev {
	struct attribute_group attrs;
	void __iomem *regs;
	struct platform_device *pdev;
	struct clk *clk;
	struct reset_control *rst;
	u32 pclk_rate;
	u32 irq;
	u32 ch_num;

	struct aic_tsen_ch chan[AIC_TSEN_MAX_CH];
	struct aic_tsen_plat_data *data;
	u16 cell_data[TSEN_NVMEM_CELL_NUM];
};

static DEFINE_SPINLOCK(user_lock);

/* Temperature = ADC data * slope + offset.
 * 1. Temperature accuracy adopts 1000.
 * 2. Slope and Offset accuracy adopts 10000.
 * Because the difference between the slope value of the CPU position and the
 * gpai position lies in the fourth digit.
 */
static s32 tsen_data2temp(u32 ch, u16 data, struct aic_tsen_ch_dat *dat)
{
	int temp;

	if (data == 4095 || data == 0)
		return 0;

	temp = dat->slope * data;
	temp += dat->offset;
	if ((temp % TSEN_CALIB_ACCURACY_SCALE) < TSEN_CALIB_ACCURACY_SCALE / 2)
		temp = temp / TSEN_CALIB_ACCURACY_SCALE;
	else
		temp = temp / TSEN_CALIB_ACCURACY_SCALE + 1;
	pr_debug("%s() ch%d temp: %d -> %d.%03d\n", __func__, ch, data,
		 temp / THERMAL_CORE_TEMP_AMPN_SCALE,
		 temp % THERMAL_CORE_TEMP_AMPN_SCALE);
	return temp;
}

/* ADC data = (Temperature - offset) / slope */
static u16 tsen_temp2data(u32 ch, s32 temp, struct aic_tsen_ch_dat *dat)
{
	int data = temp * TSEN_CALIB_ACCURACY_SCALE - dat->offset;

	data /= dat->slope;
	pr_debug("%s() ch%d data: %d -> %d\n", __func__, ch, temp, data);
	return (u16)data;
}

static u32 tsen_sec2itv(struct aic_tsen_dev *tsen, u32 sec)
{
	u32 tmp = 0;

	tmp = tsen->pclk_rate >> 16;
	tmp *= sec;
	return tmp;
}

static void tsen_enable(void __iomem *regs, int enable)
{
	int mcr;
	unsigned long flags;

	spin_lock_irqsave(&user_lock, flags);

	mcr = readl(regs + TSEN_MCR);
	if (enable)
		mcr |= TSEN_MCR_EN;
	else
		mcr &= ~TSEN_MCR_EN;

	writel(mcr, regs + TSEN_MCR);
	spin_unlock_irqrestore(&user_lock, flags);
}

static int tsen_ch_num_read(void __iomem *regs)
{
	u32 num = 0;

	num = ((readl(regs + TSEN_MCR) & TSEN_MCR_MAX_MASK)
		>> TSEN_MCR_MAX_SHIFT) + 1;
	if (num > AIC_TSEN_MAX_CH) {
		pr_warn("The max channel num %d is too big!\n", num);
		num = AIC_TSEN_MAX_CH;
	}
	return num;
}

static void tsen_ch_enable(void __iomem *regs, u32 ch, int enable)
{
	int mcr;
	unsigned long flags;

	spin_lock_irqsave(&user_lock, flags);

	mcr = readl(regs + TSEN_MCR);
	if (enable)
		mcr |= TSEN_MCR_CH_EN(ch);
	else
		mcr &= ~TSEN_MCR_CH_EN(ch);

	writel(mcr, regs + TSEN_MCR);
	spin_unlock_irqrestore(&user_lock, flags);
}

static void tsen_int_enable(void __iomem *regs, u32 ch, u32 enable, u32 detail)
{
	u32 val = 0;

	val = readl(regs + TSEN_INTR);
	if (enable) {
		val |= TSEN_INTR_CH_INT_EN(ch);
		writel(detail, regs + TSENn_INT(ch));
	} else {
		val &= ~TSEN_INTR_CH_INT_EN(ch);
		writel(0, regs + TSENn_INT(ch));
	}
	writel(val, regs + TSEN_INTR);
}

static void tsen_single_mode(void __iomem *regs, u32 ch)
{
	unsigned long flags;

	spin_lock_irqsave(&user_lock, flags);

	writel(TSENn_FIL_8_POINTS, regs + TSENn_FIL(ch));
	writel(TSENn_CFG_SINGLE_SAMPLE_EN | readl(regs + TSENn_CFG(ch)),
	       regs + TSENn_CFG(ch));

	tsen_int_enable(regs, ch, 1, TSENn_INT_DATA_RDY_IE);
	spin_unlock_irqrestore(&user_lock, flags);
}

static bool tsen_support_otp(struct aic_tsen_dev *tsen)
{
	static bool checked;
	static bool support;

	if (checked)
		return support;

	checked = true;
	if (of_device_is_compatible(tsen->pdev->dev.of_node,
				    "artinchip,aic-tsen-v0.1"))
		return false;

	if (of_device_is_compatible(tsen->pdev->dev.of_node,
				    "artinchip,aic-tsen-v1.0"))
		return false;

	support = true;
	return true;
}

/* Only in period mode, HTA, LTA, HTP and OTP are available */
static void tsen_period_mode(struct aic_tsen_dev *tsen, u32 ch)
{
	u32 val, detail = TSENn_INT_DATA_RDY_IE;
	void __iomem *regs = tsen->regs;
	struct aic_tsen_ch *chan = &tsen->chan[ch];
	unsigned long flags;

	spin_lock_irqsave(&user_lock, flags);
	if (chan->hta_enable) {
		detail |= TSENn_INT_HTA_RM_IE | TSENn_INT_HTA_VALID_IE;
		val = TSENn_HLTA_EN
			| ((chan->hta_rm_thd << TSENn_HLTA_RM_THD_SHIFT)
			& TSENn_HLTA_RM_THD_MASK)
			| (chan->hta_thd & TSENn_HLTA_THD_MASK);
		writel(val, regs + TSENn_HTAV(ch));
	}

	if (chan->lta_enable) {
		detail |= TSENn_INT_LTA_RM_IE | TSENn_INT_LTA_VALID_IE;
		val = TSENn_HLTA_EN
			| ((chan->lta_rm_thd << TSENn_HLTA_RM_THD_SHIFT)
			& TSENn_HLTA_RM_THD_MASK)
			| (chan->lta_thd & TSENn_HLTA_THD_MASK);
		writel(val, regs + TSENn_LTAV(ch));
	}

	if (tsen_support_otp(tsen) && chan->otp_enable) {
		detail |= TSENn_INT_OTP_RESET;
		val = TSENn_OTPV_EN | (chan->otp_thd & TSENn_OTPV_VAL_MASK);
		writel(val, regs + TSENn_OTPV(ch));
	}
	tsen_int_enable(regs, ch, 1, detail);

	writel(TSENn_FIL_8_POINTS, regs + TSENn_FIL(ch));
	if (of_device_is_compatible(tsen->pdev->dev.of_node,
				    "artinchip,aic-tsen-v0.1"))
		writel(chan->smp_period << TSENn_ITV_SHIFT,
		       regs + TSENn_ITV(ch));
	else
		writel(chan->smp_period << TSENn_ITV_SHIFT | 0xFFFF,
		       regs + TSENn_ITV(ch));

	writel(readl(regs + TSENn_CFG(ch)) | TSENn_CFG_PERIOD_SAMPLE_EN,
	       regs + TSENn_CFG(ch));

	spin_unlock_irqrestore(&user_lock, flags);
	tsen_ch_enable(regs, ch, 1);
}

static int tsen_ch_init(struct aic_tsen_dev *tsen, u32 ch)
{
	struct aic_tsen_ch *chan = &tsen->chan[ch];

	init_completion(&chan->complete);
	if (chan->mode == AIC_TSEN_MODE_PERIOD)
		tsen_period_mode(tsen, ch);

	/* For single mode, should init the channel in .get_temp() */
	return 0;
}

static int tsen_get_temp(struct aic_tsen_dev *tsen, u32 ch)
{
	unsigned long left = 0;
	struct aic_tsen_ch *chan = NULL;
	struct aic_tsen_ch_dat *dat = tsen->data->ch;
	unsigned long flags;

	if (unlikely(ch >= tsen->ch_num)) {
		dev_err(&tsen->pdev->dev, "Invalid channel num %d\n", ch);
		return -ECHRNG;
	}
	chan = &tsen->chan[ch];
	if (!chan->available) {
		dev_err(&tsen->pdev->dev, "ch%d is unavailable!\n", ch);
		return -ENODATA;
	}

#ifndef CONFIG_ARTINCHIP_ADCIM_DM
	if (chan->mode == AIC_TSEN_MODE_PERIOD)
		return tsen_data2temp(ch, chan->latest_data, &dat[ch]);
#endif

	spin_lock_irqsave(&user_lock, flags);
	reinit_completion(&chan->complete);
	spin_unlock_irqrestore(&user_lock, flags);

	tsen_single_mode(tsen->regs, ch);
	tsen_ch_enable(tsen->regs, ch, 1);

	left = wait_for_completion_timeout(&chan->complete, AIC_TSEN_TIMEOUT);
	if (!left) {
		dev_err(&tsen->pdev->dev, "Ch%d read timeout!\n", ch);
		tsen_ch_enable(tsen->regs, ch, 0);
		return -ETIMEDOUT;
	}
	tsen_ch_enable(tsen->regs, ch, 0);

	return tsen_data2temp(ch, chan->latest_data, &dat[ch]);
}

static inline int tsen_cpu_get_temp(struct thermal_zone_device *zone, int *temp)
{
	struct aic_tsen_dev *tsen = zone->devdata;

	*temp = tsen_get_temp(tsen, AIC_TSEN_CH_CPU);
	return 0;
}

static inline int tsen_gpai_get_temp(struct thermal_zone_device *zone,
				     int *temp)
{
	struct aic_tsen_dev *tsen = zone->devdata;

	*temp = tsen_get_temp(tsen, AIC_TSEN_CH_GPAI);
	return 0;
}

static int tsen_cpu_get_trip_type(struct thermal_zone_device *zone, int trip,
				  enum thermal_trip_type *type)
{
	struct aic_tsen_dev *tsen = zone->devdata;
	struct device *dev = &tsen->pdev->dev;

	switch (trip) {
	case 0:
		*type = THERMAL_TRIP_CRITICAL;
		break;
	default:
		dev_err(dev, "driver trip error\n");
		return -EINVAL;
	}

	return 0;
}

static int tsen_cpu_get_trip_temp(struct thermal_zone_device *zone, int trip,
				  int *temp)
{
	struct aic_tsen_dev *tsen = zone->devdata;
	struct device *dev = &tsen->pdev->dev;

	switch (trip) {
	case 0:
		*temp = tsen_data2temp(AIC_TSEN_CH_CPU,
				       tsen->chan[0].hta_thd,
				       &tsen->data->ch[AIC_TSEN_CH_CPU]);
		break;
	default:
		dev_err(dev, "driver trip error\n");
		return -EINVAL;
	}

	return 0;
}

static int tsen_cpu_notify(struct thermal_zone_device *zone,
			   int trip, enum thermal_trip_type type)
{
	struct aic_tsen_dev *tsen = zone->devdata;
	struct device *dev = &tsen->pdev->dev;

	switch (type) {
	case THERMAL_TRIP_CRITICAL:
		if (tsen->chan[0].htp_enable)
			dev_warn(dev,
				 "Thermal reached to critical temperature\n");
		break;
	default:
		break;
	}

	return 0;
}

static struct thermal_zone_device_ops tsen_cpu_ops = {
	.get_temp = tsen_cpu_get_temp,
	.get_trip_type	= tsen_cpu_get_trip_type,
	.get_trip_temp	= tsen_cpu_get_trip_temp,
	.notify = tsen_cpu_notify,
};

static struct thermal_zone_device_ops tsen_gpai_ops = {
	.get_temp = tsen_gpai_get_temp,
};

static ssize_t status_show(struct device *dev,
				 struct device_attribute *devattr,
				 char *buf)
{
	int i;
	int mcr;
	int version;
	int val[AIC_TSEN_MAX_CH];
	struct aic_tsen_dev *tsen = dev_get_drvdata(dev);
	struct aic_tsen_plat_data *data = tsen->data;

	for (i = 0; i < 2; i++)
		val[i] = tsen_get_temp(tsen, i);

	mcr = readl(tsen->regs + TSEN_MCR);
	version = readl(tsen->regs + TSEN_VERSION);

	return sprintf(buf, "In Thermal Sensor V%d.%02d:\n"
		       "Number: %d/%d\n"
		       "Ch %-13s Mode En Value  LTA  HTA  OTP Slope Offset\n"
		       "0  %-13s %4s %2d %5d %4d %4d %4d %5d %-8d\n"
		       "1  %-13s %4s %2d %5d %4d %4d %4d %5d %-8d\n",
		       version >> 8, version & 0xff,
		       data->num, tsen_ch_num_read(tsen->regs), "Name",
		       data->ch[0].name, tsen->chan[0].mode ? "P" : "S",
		       mcr & TSEN_MCR_CH_EN(0) ? 1 : 0, val[0],
		       tsen->chan[0].lta_thd, tsen->chan[0].hta_thd,
		       tsen->chan[0].otp_thd,
		       data->ch[0].slope, data->ch[0].offset,
		       data->ch[1].name, tsen->chan[1].mode ? "P" : "S",
		       mcr & TSEN_MCR_CH_EN(1) ? 1 : 0, val[1],
		       tsen->chan[1].lta_thd, tsen->chan[1].hta_thd,
		       tsen->chan[1].otp_thd,
		       data->ch[1].slope, data->ch[1].offset);
}
static DEVICE_ATTR_RO(status);

static struct attribute *aic_tsen_attr[] = {
	&dev_attr_status.attr,
	NULL
};

static irqreturn_t aic_tsen_irq_handle(int irq, void *dev_id)
{
	struct aic_tsen_dev *tsen = (struct aic_tsen_dev *)dev_id;
	struct device *dev = &tsen->pdev->dev;
	struct aic_tsen_ch_dat *dat = tsen->data->ch;
	void __iomem *regs = tsen->regs;
	int i, status, detail;
	struct aic_tsen_ch *chan = NULL;

	spin_lock(&user_lock);

	status = readl(regs + TSEN_INTR);
	for (i = 0; i < tsen->ch_num; i++) {
		if (!(status & TSEN_INTR_CH_INT_FLAG(i)))
			continue;

		chan = &tsen->chan[i];
		detail = readl(regs + TSENn_INT(i));
		writel(detail, regs + TSENn_INT(i));
		if (detail & TSENn_INT_DAT_RDY_FLAG) {
			chan->latest_data = readl(regs + TSENn_DATA(i));
			dev_dbg(dev, "ch%d data %d\n", i, chan->latest_data);
			complete(&chan->complete);
		}

		if (detail & TSENn_INT_LTA_VALID_FLAG)
			dev_warn(dev, "LTA: ch%d %d(%d)!\n", i,
				 tsen_data2temp(i, chan->latest_data, &dat[i]),
				 chan->latest_data);
		if (detail & TSENn_INT_LTA_RM_FLAG)
			dev_warn(dev, "LTA removed: ch%d %d(%d)\n", i,
				 tsen_data2temp(i, chan->latest_data, &dat[i]),
				 chan->latest_data);
		if (detail & TSENn_INT_HTA_VALID_FLAG) {
			dev_warn(dev, "HTA: ch%d %d(%d)!\n", i,
				 tsen_data2temp(i, chan->latest_data, &dat[i]),
				 chan->latest_data);
			spin_unlock(&user_lock);
			return IRQ_WAKE_THREAD;
		}
		if (detail & TSENn_INT_HTA_RM_FLAG)
			dev_warn(dev, "HTA removed: ch%d %d(%d)\n", i,
				 tsen_data2temp(i, chan->latest_data, &dat[i]),
				 chan->latest_data);
		if (tsen_support_otp(tsen) && (detail & TSENn_INT_OTP_FLAG))
			dev_warn(dev, "OTP: ch%d %d(%d)!\n", i,
				 tsen_data2temp(i, chan->latest_data, &dat[i]),
				 chan->latest_data);
	}

	spin_unlock(&user_lock);
	dev_dbg(dev, "IRQ status %#x, detail %#x\n", status, detail);

	return IRQ_NONE;
}

static irqreturn_t aic_tsen_irq_thread(int irq, void *dev_id)
{
	struct aic_tsen_dev *tsen = (struct aic_tsen_dev *)dev_id;

	if (tsen->chan[0].htp_enable)
		thermal_zone_device_update(tsen->chan[0].zone,
					   THERMAL_EVENT_UNSPECIFIED);
	return IRQ_HANDLED;
}

static int __maybe_unused spear_thermal_suspend(struct device *dev)
{
	struct aic_tsen_dev *tsen = dev_get_drvdata(dev);

	tsen_enable(tsen->regs, 0);

	reset_control_assert(tsen->rst);
	clk_disable_unprepare(tsen->clk);
	dev_info(dev, "Suspended.\n");

	return 0;
}

static int __maybe_unused spear_thermal_resume(struct device *dev)
{
	struct aic_tsen_dev *tsen = dev_get_drvdata(dev);
	int ret = 0;

	ret = clk_prepare_enable(tsen->clk);
	if (ret)
		return ret;

	ret = reset_control_deassert(tsen->rst);
	if (ret)
		return ret;

	tsen_enable(tsen->regs, 1);
	dev_info(dev, "Resumed.\n");

	return 0;
}

static SIMPLE_DEV_PM_OPS(aic_tsen_pm_ops, spear_thermal_suspend,
		spear_thermal_resume);

static int tsen_parse_dt(struct aic_tsen_dev *tsen)
{
	struct device *dev = &tsen->pdev->dev;
	struct device_node *child, *np = dev->of_node;
	struct aic_tsen_ch_dat *dat = tsen->data->ch;
	s32 ret = 0, hta = 0;
	u32 val = 0, i = 0;

	for_each_child_of_node(np, child) {
		struct aic_tsen_ch *chan = &tsen->chan[i];

		chan->available = of_device_is_available(child);
		if (!chan->available) {
			dev_warn(dev, "ch%d is unavailable.\n", i);
			i++;
			continue;
		}
		dev_dbg(dev, "ch%d is available\n", i);

		ret = of_property_read_u32(child, "aic,sample-period", &val);
		if (ret || val == 0) {
			dev_info(dev, "ch%d is single sample mode\n", i);
			chan->mode = AIC_TSEN_MODE_SINGLE;
			i++;
			continue;
		}
		chan->mode = AIC_TSEN_MODE_PERIOD;
		chan->smp_period = tsen_sec2itv(tsen, val);

		ret = of_property_read_u32(child, "aic,high-temp-thd", &hta);
		if (ret || hta == 0) {
			dev_warn(dev, "Invalid ch%d HTA thd, return %d\n",
				 i, ret ? ret : hta);
			chan->hta_enable = false;
		} else {
			chan->hta_enable = true;
			chan->hta_thd = tsen_temp2data(i, hta, &dat[i]);
			chan->hta_rm_thd = tsen_temp2data(i,
					TSENn_HTA_RM_THD(hta), &dat[i]);
		}

		ret = of_property_read_u32(child, "aic,low-temp-thd", &val);
		if (ret || val == 0 || val >= hta) {
			dev_warn(dev, "Invalid ch%d LTA thd, return %d\n",
				 i, ret ? ret : val);
			chan->lta_enable = false;
		} else {
			chan->lta_enable = true;
			chan->lta_thd = tsen_temp2data(i, val, &dat[i]);
			chan->lta_rm_thd = tsen_temp2data(i,
					TSENn_LTA_RM_THD(val), &dat[i]);
		}

		if (chan->hta_enable) {
			chan->htp_enable = of_property_read_bool(child,
							"aic,htp-enable");
		} else {
			chan->htp_enable = false;
		}

		if (!tsen_support_otp(tsen)) {
			i++;
			continue;
		}

		ret = of_property_read_u32(child, "aic,over-temp-thd", &val);
		if (ret || val == 0 || val < hta) {
			dev_warn(dev, "Invalid ch%d OTP thd %d\n",
				 i, ret ? ret : val);
			chan->otp_enable = false;
		} else {
			chan->otp_enable = true;
			chan->otp_thd = tsen_temp2data(i, val, &dat[i]);
		}
		i++;
	}

	return 0;
}

/* For temperature's slope and offset calculated by y=kx+b equation.
 * THS0_ADC_VAL as y, THS_ENV_TEMP as x.
 */
static void aic_tsen_get_cali_param(int ch_id, int y1, int y2, int x1, int x2,
				    struct aic_tsen_ch_dat *dat)
{
	int slope, offset;
	int calib_scale;

	calib_scale = THERMAL_CORE_TEMP_AMPN_SCALE * TSEN_CALIB_ACCURACY_SCALE;
	if ((x2 - x1) != 0) {
		slope = (y1 - y2) * calib_scale / (x2 - x1);
		offset = y1 * calib_scale - slope * x1;
		dat[ch_id].offset = offset;
		dat[ch_id].slope = slope;
	}
	return;
}

/* The temperature obtained from nvmem contains sign bits.
 * For this purpose, this function converts data through sign bit mask
 */
static u8 aic_tsen_env_temp_cali(u8 sign_mask, u8 val)
{
	if (val & sign_mask)
		return -(val & (sign_mask - 1));
	else
		return val & (sign_mask - 1);
}

static int aic_tsen_get_nvmem_cell(struct aic_tsen_dev *tsen)
{
	int i;
	size_t len;
	struct device *dev = &tsen->pdev->dev;
	char *cell_name[TSEN_NVMEM_CELL_NUM] = {"t0_low", "t1_low", "t0_high",
						"t1_high", "envtemp_low",
						"envtemp_high",
						"ldo30_bg_ctrl", "cp_version"};

	for (i = 0; i < TSEN_NVMEM_CELL_NUM ; i++) {
		struct nvmem_cell *cell;

		cell = devm_nvmem_cell_get(dev, cell_name[i]);
		if (IS_ERR(cell)) {
			dev_info(dev, "Failed to get cell\n");
			return -1;
		}
		tsen->cell_data[i] = *(u16 *)nvmem_cell_read(cell, &len);
		if (tsen->cell_data[i] == 0) {
			dev_info(dev, "Efuse didn't burn calibration value\n");
			return 0;
		}
	}
	return 0;
}

static int aic_tsen_double_point_cali(struct aic_tsen_dev *tsen)
{
	struct aic_tsen_ch_dat *dat = tsen->data->ch;

	int env_temp_low = TSEN_ENV_TEMP_LOW_BASE;
	int env_temp_high = TSEN_ENV_TEMP_HIGH_BASE;
	u16 *buf = tsen->cell_data;

	env_temp_low += aic_tsen_env_temp_cali(TSEN_ENV_TEMP_LOW_SIGN_MASK,
					       buf[TSEN_THS_ENV_TEMP_LOW]);
	env_temp_high += aic_tsen_env_temp_cali(TSEN_ENV_TEMP_HIGH_SIGN_MASK,
						buf[TSEN_THS_ENV_TEMP_HIGH]);
	aic_tsen_get_cali_param(AIC_TSEN_CH_CPU, buf[TSEN_THS0_ADC_VAL_LOW],
				buf[TSEN_THS0_ADC_VAL_HIGH], env_temp_low,
				env_temp_high, dat);
	aic_tsen_get_cali_param(AIC_TSEN_CH_GPAI, buf[TSEN_THS1_ADC_VAL_LOW],
				buf[TSEN_THS0_ADC_VAL_HIGH], env_temp_low,
				env_temp_high, dat);
	return 0;
}

void aic_tsen_single_point_cali(struct aic_tsen_dev *tsen)
{
	int origin_vol;
	int origin_adc;
	int cali_scale;
	u16 *buf = tsen->cell_data;
	u32 ldo30_bg_ctrl;
	int env_temp_low = TSEN_ENV_TEMP_LOW_BASE;
	int standard_vol = TSEN_ORIGIN_STANDARD_VOLTAGE;
	int vol_scale_unit = TSEN_VOLTAGE_SCALE_UNIT;
	struct aic_tsen_ch_dat dat = tsen->data->ch[AIC_TSEN_CH_CPU];

	ldo30_bg_ctrl = buf[TSEN_LDO30_BG_CTRL];

	if (ldo30_bg_ctrl > TSEN_TRIM_VOLTAGE_BOUNDARY_VAL)
		origin_vol = standard_vol - (255 - ldo30_bg_ctrl) * vol_scale_unit;
	else
		origin_vol = standard_vol + ldo30_bg_ctrl * vol_scale_unit;

	origin_adc = origin_vol * buf[TSEN_THS0_ADC_VAL_LOW] / standard_vol;
	env_temp_low += aic_tsen_env_temp_cali(TSEN_ENV_TEMP_LOW_SIGN_MASK,
					       buf[TSEN_THS_ENV_TEMP_LOW]);

	cali_scale = THERMAL_CORE_TEMP_AMPN_SCALE * TSEN_CALIB_ACCURACY_SCALE;
	dat.offset = TSEN_SINGLE_POINT_CALI_K;
	dat.slope = env_temp_low * cali_scale - dat.slope * origin_adc;
}

void aic_tsen_curve_fitting(struct aic_tsen_dev *tsen)
{
	int cp_version = tsen->cell_data[TSEN_CP_VERSION];

	if (cp_version >= TSEN_CP_VERSION_DIFF_TYPE)
		aic_tsen_double_point_cali(tsen);
	else
		aic_tsen_single_point_cali(tsen);
}

static const struct of_device_id aic_tsen_id_table[];

static int aic_tsen_probe(struct platform_device *pdev)
{
	struct thermal_zone_device *zone_dev = NULL;
	struct aic_tsen_dev *tsen;
	struct clk *clk;
	int ret = 0, i;
	const struct of_device_id *of_id = NULL;
	struct thermal_zone_device_ops *ops[AIC_TSEN_MAX_CH] = {
		&tsen_cpu_ops, &tsen_gpai_ops, NULL, NULL};

	tsen = devm_kzalloc(&pdev->dev, sizeof(*tsen), GFP_KERNEL);
	if (!tsen)
		return -ENOMEM;

	of_id = of_match_node(aic_tsen_id_table, pdev->dev.of_node);
	if (!of_id)
		return -EINVAL;
	tsen->data = (struct aic_tsen_plat_data *)of_id->data;

	tsen->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(tsen->regs))
		return PTR_ERR(tsen->regs);

	clk = devm_clk_get(&pdev->dev, "pclk");
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "Failed to get pclk\n");
		return PTR_ERR(clk);
	}
	tsen->pclk_rate = clk_get_rate(clk);
	dev_dbg(&pdev->dev, "PCLK rate %d\n", tsen->pclk_rate);

	tsen->clk = devm_clk_get(&pdev->dev, "tsen");
	if (IS_ERR(tsen->clk)) {
		dev_err(&pdev->dev, "no clock defined\n");
		return PTR_ERR(tsen->clk);
	}
	ret = clk_prepare_enable(tsen->clk);
	if (ret) {
		dev_err(&pdev->dev, "Failed to enable clk, return %d\n", ret);
		return ret;
	}

	tsen->rst = devm_reset_control_get_optional_shared(&pdev->dev, NULL);
	if (IS_ERR(tsen->rst)) {
		ret = PTR_ERR(tsen->rst);
		goto disable_clk;
	}
	reset_control_deassert(tsen->rst);

	ret = platform_get_irq(pdev, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to get irq\n");
		goto disable_clk;
	}
	tsen->irq = ret;
	ret = request_threaded_irq(tsen->irq,
				   aic_tsen_irq_handle, aic_tsen_irq_thread,
				   IRQF_ONESHOT, AIC_TSEN_NAME, tsen);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to request IRQ %d\n", tsen->irq);
		goto disable_rst;
	}

	tsen_enable(tsen->regs, 1);
	tsen->ch_num = tsen->data->num;
	tsen->pdev = pdev;
	tsen_parse_dt(tsen);
	for (i = 0; i < tsen->ch_num; i++) {
		struct aic_tsen_ch *chan = &tsen->chan[i];
		int trips[AIC_TSEN_MAX_CH] = {TSEN_CPU_ZONE_TRIPS_NUM,
					      TSEN_GPAI_ZONE_TRIPS_NUM};

		if (!chan->available)
			continue;

		zone_dev = thermal_zone_device_register(tsen->data->ch[i].name,
							trips[i], 0, tsen,
							ops[i], NULL, 0, 0);
		if (IS_ERR(zone_dev)) {
			dev_err(&pdev->dev, "zone device %d failed!\n", i);
			ret = PTR_ERR(zone_dev);
			goto disable_rst;
		}

		chan->zone = zone_dev;

		tsen_ch_init(tsen, i);

		if (i == AIC_TSEN_CH_CPU) {
			ret = thermal_zone_device_enable(zone_dev);
			if (ret) {
				dev_err(&pdev->dev,
					"zone device %d enable failed!\n",
					AIC_TSEN_CH_CPU);
				thermal_zone_device_unregister(zone_dev);
				zone_dev = ERR_PTR(ret);
				}
		}
	}
	aic_tsen_get_nvmem_cell(tsen);
	aic_tsen_curve_fitting(tsen);

	tsen->attrs.attrs = aic_tsen_attr;
	ret = sysfs_create_group(&pdev->dev.kobj, &tsen->attrs);
	if (ret)
		goto free_irq;

	platform_set_drvdata(pdev, tsen);
	dev_info(&pdev->dev, "Artinchip Thermal Sensor Loaded.\n");

	return 0;

free_irq:
	free_irq(tsen->irq, tsen);
disable_rst:
	reset_control_assert(tsen->rst);
disable_clk:
	clk_disable_unprepare(tsen->clk);

	return ret;
}

static int aic_tsen_exit(struct platform_device *pdev)
{
	int i;
	struct aic_tsen_dev *tsen = platform_get_drvdata(pdev);

	for (i = 0; i < tsen->ch_num; i++) {
		tsen_ch_enable(tsen->regs, i, 0);
		thermal_zone_device_unregister(tsen->chan[i].zone);
	}

	tsen_enable(tsen->regs, 0);
	free_irq(tsen->irq, tsen);
	reset_control_assert(tsen->rst);
	clk_disable_unprepare(tsen->clk);
	return 0;
}

static struct aic_tsen_plat_data aic_tsen_data_v01 = {2, {
	{AIC_TSEN_NAME "-cpu",  -1065, 2300981},
	{AIC_TSEN_NAME "-gpai", -1070, 2312717}}
};

static struct aic_tsen_plat_data aic_tsen_data_v10 = {2, {
	{AIC_TSEN_NAME "-cpu",  -1134, 2439001},
	{AIC_TSEN_NAME "-gpai", -1139, 2450566}}
};

static const struct of_device_id aic_tsen_id_table[] = {
	{
		.compatible = "artinchip,aic-tsen-v0.1",
		.data = &aic_tsen_data_v01,
	}, {
		.compatible = "artinchip,aic-tsen-v1.0",
		.data = &aic_tsen_data_v10,
	},
	{}
};
MODULE_DEVICE_TABLE(of, aic_tsen_id_table);

static struct platform_driver aic_tsen_driver = {
	.probe = aic_tsen_probe,
	.remove = aic_tsen_exit,
	.driver = {
		.name = AIC_TSEN_NAME,
		.pm = &aic_tsen_pm_ops,
		.of_match_table = aic_tsen_id_table,
	},
};
module_platform_driver(aic_tsen_driver);

MODULE_AUTHOR("Matteo <duanmt@artinchip.com>");
MODULE_DESCRIPTION("Thermal Sensor driver of Artinchip SoC");
MODULE_LICENSE("GPL");
