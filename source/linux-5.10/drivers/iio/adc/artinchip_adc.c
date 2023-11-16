// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * GPAI driver of Artinchip SoC
 *
 * Copyright (C) 2020-2021 Artinchip Technology Co., Ltd.
 * Authors:  Matteo <duanmt@artinchip.com>
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/reset.h>
#include <linux/iio/iio.h>
#include <linux/iio/events.h>

#define AIC_GPAI_NAME		"aic-gpai"
#define AIC_GPAI_MAX_CH		8
#define AIC_GPAI_TIMEOUT	msecs_to_jiffies(1000)

enum aic_gpai_mode {
	AIC_GPAI_MODE_SINGLE = 0,
	AIC_GPAI_MODE_PERIOD = 1
};

/* Register definition of GPAI Controller */
#define GPAI_MCR		0x000
#define GPAI_INTR		0x004
#define GPAI_CHnCR(n)		(0x100 + (((n) & 0x7) << 6) + 0x00)
#define GPAI_CHnINT(n)		(0x100 + (((n) & 0x7) << 6) + 0x04)
#define GPAI_CHnPSI(n)		(0x100 + (((n) & 0x7) << 6) + 0x08)
#define GPAI_CHnHLAT(n)		(0x100 + (((n) & 0x7) << 6) + 0x10)
#define GPAI_CHnLLAT(n)		(0x100 + (((n) & 0x7) << 6) + 0x14)
#define GPAI_CHnACR(n)		(0x100 + (((n) & 0x7) << 6) + 0x18)
#define GPAI_CHnFCR(n)		(0x100 + (((n) & 0x7) << 6) + 0x20)
#define GPAI_CHnDATA(n)		(0x100 + (((n) & 0x7) << 6) + 0x24)
#define GPAI_VERSION		0xFFC

#define GPAI_MCR_CH0_EN			BIT(8)
#define GPAI_MCR_CH_EN(n)		(GPAI_MCR_CH0_EN << (n))
#define GPAI_MCR_EN			BIT(0)

#define GPAI_INTR_CH0_INT_FLAG		BIT(16)
#define GPAI_INTR_CH_INT_FLAG(n)	(GPAI_INTR_CH0_INT_FLAG << (n))
#define GPAI_INTR_CH0_INT_EN		BIT(0)
#define GPAI_INTR_CH_INT_EN(n)		(GPAI_INTR_CH0_INT_EN << (n))

#define GPAI_CHnCR_SBC_SHIFT		24
#define GPAI_CHnCR_SBC_2_POINTS		1
#define GPAI_CHnCR_SBC_4_POINTS		2
#define GPAI_CHnCR_SBC_8_POINTS		3

#define GPAI_CHnCR_SBC_SHIFT		24
#define GPAI_CHnCR_SBC_MASK		GENMASK(25, 24)
#define GPAI_CHnCR_HIGH_ADC_PRIORITY	BIT(4)
#define GPAI_CHnCR_PERIOD_SAMPLE_EN	BIT(1)
#define GPAI_CHnCR_SINGLE_SAMPLE_EN	BIT(0)

#define GPAI_CHnINT_LLA_RM_FLAG		BIT(23)
#define GPAI_CHnINT_LLA_VALID_FLAG	BIT(22)
#define GPAI_CHnINT_HLA_RM_FLAG		BIT(21)
#define GPAI_CHnINT_HLA_VALID_FLAG	BIT(20)
#define GPAI_CHnINT_FIFO_ERR_FLAG	BIT(17)
#define GPAI_CHnINT_DRDY_FLG		BIT(16)
#define GPAI_CHnINT_LLA_RM_IE		BIT(7)
#define GPAI_CHnINT_LLA_VALID_IE	BIT(6)
#define GPAI_CHnINT_HLA_RM_IE		BIT(5)
#define GPAI_CHnINT_HLA_VALID_IE	BIT(4)
#define GPAI_CHnINT_FIFO_ERR_IE		BIT(1)
#define GPAI_CHnINT_DAT_RDY_IE		BIT(0)

#define GPAI_CHnLAT_HLLA_RM_THD_SHIFT	16
#define GPAI_CHnLAT_HLLA_RM_THD_MASK	GENMASK(27, 16)
#define GPAI_CHnLAT_HLLA_THD_MASK	GENMASK(11, 0)
#define GPAI_CHnLAT_HLA_RM_THD(n)	((n) - 30)
#define GPAI_CHnLAT_LLA_RM_THD(n)	((n) + 30)

#define GPAI_CHnACR_DISCARD_NOR_DAT	BIT(6)
#define GPAI_CHnACR_DISCARD_LL_DAT	BIT(5)
#define GPAI_CHnACR_DISCARD_HL_DAT	BIT(4)
#define GPAI_CHnACR_LLA_EN		BIT(1)
#define GPAI_CHnACR_HLA_EN		BIT(0)

#define GPAI_CHnFCR_DAT_CNT_MAX(ch)	((ch) > 1 ? 0x8 : 0x40)
#define GPAI_CHnFCR_DAT_CNT_SHIFT	24
#define GPAI_CHnFCR_DAT_CNT_MASK	GENMASK(30, 24)
#define GPAI_CHnFCR_UF_STS		BIT(18)
#define GPAI_CHnFCR_OF_STS		BIT(17)
#define GPAI_CHnFCR_DAT_RDY_THD_SHIFT	8
#define GPAI_CHnFCR_DAT_RDY_THD_MASK	GENMASK(15, 8)
#define GPAI_CHnFCR_FLUSH		BIT(0)
#define GPAI_INVALID_DATA		0xFFF
#define GPAI_ADC_DATA_MIN		0x0
#define GPAI_ADC_DATA_STEP		0x1

struct aic_gpai_ch {
	u32 id;
	bool available;
	enum aic_gpai_mode mode;
	u16 latest_data;
	u16 fifo_thd;
	u32 smp_period;

	bool hla_enable; // high-level alarm
	u16 hla_thd;
	u16 hla_rm_thd;
	bool lla_enable; // low-level alarm
	u16 lla_thd;
	u16 lla_rm_thd;

	struct completion complete;
};

struct aic_gpai_data {
	int				num_bits;
	const struct iio_chan_spec	*channels;
	int				num_channels;
	u32				fifo_depth[AIC_GPAI_MAX_CH];
};

struct aic_gpai_dev {
	struct platform_device		*pdev;
	void __iomem			*regs;
	struct clk			*clk;
	struct reset_control		*rst;
	u32				irq;
	u32				pclk_rate;

	struct aic_gpai_ch		chan[AIC_GPAI_MAX_CH];
	const struct aic_gpai_data	*data;
};

static DEFINE_SPINLOCK(user_lock);

static const int aic_gpai_adc_raw_available[] = {
	GPAI_ADC_DATA_MIN, GPAI_ADC_DATA_STEP, GPAI_INVALID_DATA,
};

// TODO: Add the transform algorithm, offered by SD later
static s32 gpai_data2vol(u16 data)
{
	return data;
}

static u16 gpai_vol2data(s32 vol)
{
	return vol;
}

static u32 gpai_ms2itv(struct aic_gpai_dev *gpai, u32 ms)
{
	u32 tmp = 0;

	if (of_device_is_compatible(gpai->pdev->dev.of_node,
				    "artinchip,aic-gpai-v0.1")) {
		dev_info(&gpai->pdev->dev, "Use a max period");
		/* BUG: Only the low 16bit is valid, so set a max number */
		return 0xFFFF;
	}

	tmp = gpai->pclk_rate / 1000;
	tmp *= ms;
	return tmp;
}

static void gpai_reg_enable(void __iomem *base, int offset, int bit, int enable)
{
	int tmp = readl(base + offset);

	if (enable)
		tmp |= bit;
	else
		tmp &= ~bit;

	writel(tmp, base + offset);
}

static void gpai_enable(void __iomem *regs, int enable)
{
	spin_lock(&user_lock);
	gpai_reg_enable(regs, GPAI_MCR, GPAI_MCR_EN, enable);
	spin_unlock(&user_lock);
}

static void gpai_ch_enable(void __iomem *regs, u32 ch, int enable)
{
	spin_lock(&user_lock);
	gpai_reg_enable(regs, GPAI_MCR, GPAI_MCR_CH_EN(ch), enable);
	spin_unlock(&user_lock);
}

static void gpai_int_enable(void __iomem *regs, u32 ch, u32 enable, u32 detail)
{
	u32 val = 0;

	val = readl(regs + GPAI_INTR);
	if (enable) {
		val |= GPAI_INTR_CH_INT_EN(ch);
		writel(detail, regs + GPAI_CHnINT(ch));
	} else {
		val &= ~GPAI_INTR_CH_INT_EN(ch);
		writel(0, regs + GPAI_CHnINT(ch));
	}
	writel(val, regs + GPAI_INTR);
}

static void gpai_fifo_init(struct aic_gpai_dev *gpai, u32 ch)
{
	u32 val = 0;

	val = 1 << GPAI_CHnFCR_DAT_RDY_THD_SHIFT;
	writel(val, gpai->regs + GPAI_CHnFCR(ch));
}

static void gpai_fifo_flush(struct aic_gpai_dev *gpai, u32 ch)
{
	struct device *dev = &gpai->pdev->dev;
	void __iomem *regs = gpai->regs;
	u32 val = readl(regs + GPAI_CHnFCR(ch));

	if (val & GPAI_CHnFCR_UF_STS)
		dev_err(dev, "ch%d FIFO is Underflow!%#x\n", ch, val);
	if (val & GPAI_CHnFCR_OF_STS)
		dev_err(dev, "ch%d FIFO is Overflow!%#x\n", ch, val);

	writel(val | GPAI_CHnFCR_FLUSH, regs + GPAI_CHnFCR(ch));
}

static void gpai_single_mode(void __iomem *regs, u32 ch)
{
	u32 val = 0;

	spin_lock(&user_lock);

	val = readl(regs + GPAI_CHnCR(ch));
	val |= GPAI_CHnCR_SBC_8_POINTS << GPAI_CHnCR_SBC_SHIFT
		| GPAI_CHnCR_SINGLE_SAMPLE_EN;
	writel(val, regs + GPAI_CHnCR(ch));

	gpai_int_enable(regs, ch, 1,
			GPAI_CHnINT_DAT_RDY_IE | GPAI_CHnINT_FIFO_ERR_IE);
	spin_unlock(&user_lock);
}

/* Only in period mode, HLA and LLA are available */
static void gpai_period_mode(struct aic_gpai_dev *gpai, u32 ch)
{
	u32 val, acr = 0;
	u32 detail = GPAI_CHnINT_DAT_RDY_IE | GPAI_CHnINT_FIFO_ERR_IE;
	void __iomem *regs = gpai->regs;
	struct aic_gpai_ch *chan = &gpai->chan[ch];

	spin_lock(&user_lock);
	if (chan->hla_enable) {
		detail |= GPAI_CHnINT_HLA_RM_IE | GPAI_CHnINT_HLA_VALID_IE;
		val = ((chan->hla_rm_thd << GPAI_CHnLAT_HLLA_RM_THD_SHIFT)
			& GPAI_CHnLAT_HLLA_RM_THD_MASK)
			| (chan->hla_thd & GPAI_CHnLAT_HLLA_THD_MASK);
		writel(val, regs + GPAI_CHnHLAT(ch));
		acr |= GPAI_CHnACR_HLA_EN;
	}

	if (chan->lla_enable) {
		detail |= GPAI_CHnINT_LLA_VALID_IE | GPAI_CHnINT_LLA_RM_IE;
		val = ((chan->lla_rm_thd << GPAI_CHnLAT_HLLA_RM_THD_SHIFT)
			& GPAI_CHnLAT_HLLA_RM_THD_MASK)
			| (chan->lla_thd & GPAI_CHnLAT_HLLA_THD_MASK);
		writel(val, regs + GPAI_CHnLLAT(ch));
		acr |= GPAI_CHnACR_LLA_EN;
	}

	gpai_int_enable(regs, ch, 1, detail);

	writel(acr, regs + GPAI_CHnACR(ch));
	writel(chan->smp_period, regs + GPAI_CHnPSI(ch));

	val = readl(regs + GPAI_CHnCR(ch));
	val |= GPAI_CHnCR_SBC_8_POINTS << GPAI_CHnCR_SBC_SHIFT
		| GPAI_CHnCR_PERIOD_SAMPLE_EN;
	writel(val, regs + GPAI_CHnCR(ch));

	spin_unlock(&user_lock);
}

static int gpai_ch_init(struct aic_gpai_dev *gpai, u32 ch)
{
	struct aic_gpai_ch *chan = &gpai->chan[ch];

	init_completion(&chan->complete);
	gpai_ch_enable(gpai->regs, ch, 1);
	gpai_fifo_init(gpai, ch);
	if (chan->mode == AIC_GPAI_MODE_PERIOD)
		gpai_period_mode(gpai, ch);

	/* For single mode, should init the channel in .read_raw() */
	return 0;
}

static int aic_gpai_read_dat(struct aic_gpai_dev *gpai, u32 ch)
{
	u32 i;
	void __iomem *regs = gpai->regs;
	struct device *dev = &gpai->pdev->dev;
	struct aic_gpai_ch *chan = &gpai->chan[ch];
	u32 cnt = (readl(regs + GPAI_CHnFCR(ch)) & GPAI_CHnFCR_DAT_CNT_MASK)
			>> GPAI_CHnFCR_DAT_CNT_SHIFT;

	if (unlikely(cnt == 0 || cnt > GPAI_CHnFCR_DAT_CNT_MAX(ch))) {
		dev_err(dev, "ch%d invalid data count %d\n", ch, cnt);
		return -1;
	}

	/* Just record the last data as to now */
	for (i = 0; i < cnt; i++) {
		chan->latest_data = readl(regs + GPAI_CHnDATA(ch));
		// dev_dbg(dev, "ch%d data%d %d\n", ch, i, chan->latest_data);
	}
	dev_dbg(dev, "There are %d data ready in ch%d, last %d\n", cnt,
		ch, chan->latest_data);

	return 0;
}

static int aic_gpai_read_raw(struct iio_dev *iodev,
				struct iio_chan_spec const *chan,
				int *val, int *val2, long mask)
{
	struct aic_gpai_dev *gpai = iio_priv(iodev);
	struct device *dev = &gpai->pdev->dev;
	struct aic_gpai_ch *gpai_ch = NULL;
	u32 ch = chan->channel;

	if (unlikely(chan->channel < 0 || chan->channel >= AIC_GPAI_MAX_CH)) {
		dev_err(dev, "Invalid channel No.%d", chan->channel);
		return -ENODEV;
	}
	gpai_ch = &gpai->chan[ch];
	if (!gpai_ch->available) {
		dev_warn(dev, "Channel %d is unavailable", ch);
		return -ENODEV;
	}

	dev_dbg(&gpai->pdev->dev, "ch %d, mask %#lx\n", ch, mask);
	switch (mask) {
	case IIO_CHAN_INFO_RAW:
#ifndef CONFIG_ARTINCHIP_ADCIM_DM
		if (gpai_ch->mode == AIC_GPAI_MODE_PERIOD) {
			*val = gpai_data2vol(gpai_ch->latest_data);
			return IIO_VAL_INT;
		}
#endif

		spin_lock(&user_lock);
		reinit_completion(&gpai_ch->complete);
		spin_unlock(&user_lock);

		gpai_ch_enable(gpai->regs, ch, 1);
		gpai_single_mode(gpai->regs, ch);
		if (!wait_for_completion_timeout(&gpai_ch->complete,
						 AIC_GPAI_TIMEOUT)) {
			dev_err(dev, "Ch%d read timeout!\n", ch);
			gpai_ch_enable(gpai->regs, ch, 0);
			return -ETIMEDOUT;
		}
		gpai_ch_enable(gpai->regs, ch, 0);

		*val = gpai_data2vol(gpai_ch->latest_data);
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_SCALE:
	default:
		return -EINVAL;
	}
}

static const struct iio_event_spec aic_gpai_event[] = {
{	.type = IIO_EV_TYPE_THRESH,
	.dir = IIO_EV_DIR_RISING,
	.mask_separate = BIT(IIO_EV_INFO_VALUE) |
			BIT(IIO_EV_INFO_ENABLE),
},
{
	.type = IIO_EV_TYPE_THRESH,
	.dir = IIO_EV_DIR_FALLING,
	.mask_separate = BIT(IIO_EV_INFO_VALUE) |
			BIT(IIO_EV_INFO_ENABLE),
}

};

static irqreturn_t aic_gpai_isr(int irq, void *dev_id)
{
	struct aic_gpai_dev *gpai = dev_id;
	struct device *dev = &gpai->pdev->dev;
	struct iio_dev *iodev = dev_get_drvdata(dev);
	void __iomem *regs = gpai->regs;
	u32 ch_flag = 0, ch_int = 0;
	int i;
	struct aic_gpai_ch *chan = NULL;
	unsigned long flags;

	spin_lock_irqsave(&user_lock, flags);

	ch_flag = readl(regs + GPAI_INTR);
	for (i = 0; i < AIC_GPAI_MAX_CH; i++) {
		if (!(ch_flag & GPAI_INTR_CH_INT_FLAG(i)))
			continue;

		chan = &gpai->chan[i];
		ch_int = readl(regs + GPAI_CHnINT(i));
		writel(ch_int, regs + GPAI_CHnINT(i));
		if (ch_int & GPAI_CHnINT_DRDY_FLG) {
			aic_gpai_read_dat(gpai, i);
			if (chan->mode == AIC_GPAI_MODE_SINGLE)
				complete(&chan->complete);
			if (chan->latest_data == GPAI_INVALID_DATA)
				continue;
		}

		if (ch_int & GPAI_CHnINT_LLA_VALID_FLAG) {
			iio_push_event(iodev,
				       IIO_UNMOD_EVENT_CODE(IIO_ANGL, i,
							    IIO_EV_TYPE_THRESH,
							    IIO_EV_DIR_FALLING),
				       iio_get_time_ns(iodev));
			dev_warn(dev, "LLA: ch%d %d!\n", i, chan->latest_data);
		}
		if (ch_int & GPAI_CHnINT_LLA_RM_FLAG) {
			dev_warn(dev, "LLA removed: ch%d %d\n", i,
				 chan->latest_data);
		}

		if (ch_int & GPAI_CHnINT_HLA_VALID_FLAG) {
			iio_push_event(iodev,
				       IIO_UNMOD_EVENT_CODE(IIO_ANGL, i,
							    IIO_EV_TYPE_THRESH,
							    IIO_EV_DIR_RISING),
				       iio_get_time_ns(iodev));
			dev_warn(dev, "HLA: ch%d %d!\n", i, chan->latest_data);
		}

		if (ch_int & GPAI_CHnINT_HLA_RM_FLAG) {
			dev_warn(dev, "HLA removed: ch%d %d\n", i,
				 chan->latest_data);
		}
		if (ch_int & GPAI_CHnINT_FIFO_ERR_FLAG)
			gpai_fifo_flush(gpai, i);
	}
	dev_dbg(dev, "IRQ flag %#x, detail %#x\n", ch_flag, ch_int);

	spin_unlock_irqrestore(&user_lock, flags);
	return IRQ_HANDLED;
}

static int aic_gpai_read_avail(struct iio_dev *iodev,
			     struct iio_chan_spec const *chan,
			     const int **vals, int *type, int *length,
			     long mask)
{
	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		*vals = aic_gpai_adc_raw_available;
		*type = IIO_VAL_INT;
		return IIO_AVAIL_RANGE;
	default:
		return -EINVAL;
	}
}

static const struct iio_info aic_gpai_iio_info = {
	.read_raw = aic_gpai_read_raw,
	.read_avail = aic_gpai_read_avail,
};

#define GPAI_CHANNEL(_index, _id) {				\
	.type = IIO_VOLTAGE,					\
	.indexed = 1,						\
	.channel = _index,					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),	\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
	.info_mask_separate_available = BIT(IIO_CHAN_INFO_RAW),	\
	.datasheet_name = _id,					\
}

static const struct iio_chan_spec aic_gpai_v1_iio_channels[] = {
	GPAI_CHANNEL(0, "adc0"),
	GPAI_CHANNEL(1, "adc1"),
	GPAI_CHANNEL(2, "adc2"),
	GPAI_CHANNEL(3, "adc3"),
	GPAI_CHANNEL(4, "adc4"),
	GPAI_CHANNEL(5, "adc5"),
	GPAI_CHANNEL(6, "adc6"),
	GPAI_CHANNEL(7, "adc7"),
	{
		.event_spec = aic_gpai_event,
		.num_event_specs = ARRAY_SIZE(aic_gpai_event),
	}
};

static const struct aic_gpai_data aic_gpai_v1_data = {
	.num_bits = 12,
	.channels = aic_gpai_v1_iio_channels,
	.num_channels = ARRAY_SIZE(aic_gpai_v1_iio_channels),
	.fifo_depth = {64, 64, 8, 8, 8, 8, 8, 8},
};

static const struct of_device_id aic_gpai_match[] = {
	{
		.compatible = "artinchip,aic-gpai-v0.1",
		.data = &aic_gpai_v1_data,
	},
	{
		.compatible = "artinchip,aic-gpai-v1.0",
		.data = &aic_gpai_v1_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, aic_gpai_match);

#ifdef CONFIG_PM_SLEEP
static int aic_gpai_suspend(struct device *dev)
{
	struct iio_dev *iodev = dev_get_drvdata(dev);
	struct aic_gpai_dev *gpai = iio_priv(iodev);

	gpai_enable(gpai->regs, 0);

	reset_control_assert(gpai->rst);
	clk_disable_unprepare(gpai->clk);

	return 0;
}

static int aic_gpai_resume(struct device *dev)
{
	struct iio_dev *iodev = dev_get_drvdata(dev);
	struct aic_gpai_dev *gpai = iio_priv(iodev);
	int ret;

	ret = clk_prepare_enable(gpai->clk);
	if (ret)
		return ret;

	ret = reset_control_deassert(gpai->rst);
	if (ret)
		return ret;

	gpai_enable(gpai->regs, 1);
	return ret;
}
#endif

static SIMPLE_DEV_PM_OPS(aic_gpai_pm_ops,
			 aic_gpai_suspend, aic_gpai_resume);

static int gpai_parse_dt(struct aic_gpai_dev *gpai)
{
	struct device *dev = &gpai->pdev->dev;
	struct device_node *child, *np = dev->of_node;
	s32 ret = 0, hla = 0;
	u32 val = 0, i = 0;

	for_each_child_of_node(np, child) {
		struct aic_gpai_ch *chan = &gpai->chan[i];

		chan->id = i;
		chan->available = of_device_is_available(child);
		if (!chan->available) {
			dev_dbg(dev, "ch%d is unavailable.\n", i);
			i++;
			continue;
		}
		dev_dbg(dev, "ch%d is available\n", i);

		ret = of_property_read_u32(child, "aic,sample-period-ms", &val);
		if (ret || val == 0) {
			dev_info(dev, "ch%d is single sample mode\n", i);
			chan->mode = AIC_GPAI_MODE_SINGLE;
			i++;
			continue;
		}
		chan->mode = AIC_GPAI_MODE_PERIOD;
		chan->smp_period = gpai_ms2itv(gpai, val);

		ret = of_property_read_u32(child, "aic,high-level-thd", &hla);
		if (ret || hla == 0) {
			dev_warn(dev, "Invalid ch%d HLA thd, return %d\n",
				 i, ret ? ret : hla);
			chan->hla_enable = false;
		} else {
			chan->hla_enable = true;
			chan->hla_thd = gpai_vol2data(hla);
			chan->hla_rm_thd =
				gpai_vol2data(GPAI_CHnLAT_HLA_RM_THD(hla));
		}

		ret = of_property_read_u32(child, "aic,low-level-thd", &val);
		if (ret || val == 0 || val >= hla) {
			dev_warn(dev, "Invalid ch%d LLA thd, return %d\n",
				 i, ret ? ret : val);
			chan->lla_enable = false;
		} else {
			chan->lla_enable = true;
			chan->lla_thd = gpai_vol2data(val);
			chan->lla_rm_thd =
				gpai_vol2data(GPAI_CHnLAT_LLA_RM_THD(val));
		}

		chan->fifo_thd = 1;
		i++;
	}

	return 0;
}

static int aic_gpai_probe(struct platform_device *pdev)
{
	struct aic_gpai_dev *gpai = NULL;
	struct iio_dev *iodev = NULL;
	const struct of_device_id *match;
	struct clk *clk;
	int ret, irq, i;

	if (!pdev->dev.of_node)
		return -ENODEV;

	iodev = devm_iio_device_alloc(&pdev->dev, sizeof(*gpai));
	if (!iodev)
		return -ENOMEM;
	gpai = iio_priv(iodev);

	match = of_match_device(aic_gpai_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "failed to match device\n");
		return -ENODEV;
	}
	gpai->data = match->data;

	gpai->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(gpai->regs))
		return PTR_ERR(gpai->regs);

	clk = devm_clk_get(&pdev->dev, "pclk");
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "Failed to get pclk\n");
		return PTR_ERR(clk);
	}
	gpai->pclk_rate = clk_get_rate(clk);
	dev_dbg(&pdev->dev, "PCLK rate %d\n", gpai->pclk_rate);

	gpai->clk = devm_clk_get(&pdev->dev, "gpai");
	if (IS_ERR(gpai->clk)) {
		dev_err(&pdev->dev, "no clock defined\n");
		return PTR_ERR(gpai->clk);
	}
	ret = clk_prepare_enable(gpai->clk);
	if (ret) {
		dev_err(&pdev->dev, "Failed to enable clk, return %d\n", ret);
		return ret;
	}

	gpai->rst = devm_reset_control_get_optional_shared(&pdev->dev, NULL);
	if (IS_ERR(gpai->rst)) {
		ret = PTR_ERR(gpai->rst);
		goto disable_clk;
	}
	reset_control_deassert(gpai->rst);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "Failed to get irq\n");
		goto disable_rst;
	}

	ret = devm_request_irq(&pdev->dev, irq, aic_gpai_isr,
			       0, AIC_GPAI_NAME, gpai);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to request IRQ %d\n", irq);
		goto disable_rst;
	}
	gpai->irq = irq;
	gpai->pdev = pdev;

	gpai_enable(gpai->regs, 1);
	platform_set_drvdata(pdev, iodev);
	gpai_parse_dt(gpai);

	iodev->name = AIC_GPAI_NAME;
	iodev->dev.parent = &pdev->dev;
	iodev->dev.of_node = pdev->dev.of_node;
	iodev->info = &aic_gpai_iio_info;
	iodev->modes = INDIO_DIRECT_MODE;

	iodev->channels = gpai->data->channels;
	iodev->num_channels = gpai->data->num_channels;

	ret = iio_device_register(iodev);
	if (ret)
		goto free_irq;

	for (i = 0; i < AIC_GPAI_MAX_CH; i++) {
		if (gpai->chan[i].available)
			gpai_ch_init(gpai, i);
	}

	dev_info(&pdev->dev, "Artinchip GPAI Loaded.\n");
	return 0;

free_irq:
	free_irq(gpai->irq, gpai);
disable_rst:
	reset_control_assert(gpai->rst);
disable_clk:
	clk_disable_unprepare(gpai->clk);

	return ret;
}

static int aic_gpai_remove(struct platform_device *pdev)
{
	struct iio_dev *iodev = platform_get_drvdata(pdev);
	struct aic_gpai_dev *gpai = iio_priv(iodev);

	iio_device_unregister(iodev);
	gpai_enable(gpai->regs, 0);

	free_irq(gpai->irq, gpai);
	reset_control_assert(gpai->rst);
	clk_disable_unprepare(gpai->clk);
	return 0;
}


static struct platform_driver aic_gpai_driver = {
	.probe		= aic_gpai_probe,
	.remove		= aic_gpai_remove,
	.driver		= {
		.name	= AIC_GPAI_NAME,
		.of_match_table = aic_gpai_match,
		.pm	= &aic_gpai_pm_ops,
	},
};
module_platform_driver(aic_gpai_driver);

MODULE_AUTHOR("Matteo <duanmt@artinchip.com>");
MODULE_DESCRIPTION("GPAI driver of Artinchip SoC");
MODULE_LICENSE("GPL");
