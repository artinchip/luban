// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * PSADC driver of Artinchip SoC
 *
 * Copyright (C) 2020-2023 Artinchip Technology Co., Ltd.
 * Authors:  Siyao.Li <siyao.li@artinchip.com>
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

#define AIC_PSADC_NAME			"aic-psadc"
#define AIC_PSADC_MAX_CH		12
#define AIC_PSADC_FIFO1_NUM_BITS	20
#define AIC_PSADC_FIFO2_NUM_BITS	12
#define AIC_PSADC_TIMEOUT		msecs_to_jiffies(1000)

enum aic_psadc_mode {
	AIC_PSADC_MODE_SINGLE = 0
};

/* Register definition of PSADC Controller */
#define PSADC_MCR		0x000
#define PSADC_TCR		0x004
#define PSADC_NODE1		0x008
#define PSADC_NODE2		0x00C
#define PSADC_MSR		0x010
#define PSADC_CALCSR		0x014
#define PSADC_FILTER		0x01C
#define PSADC_Q1FCR		0x020
#define PSADC_Q2FCR		0x024
#define PSADC_Q1FDR		0x040
#define PSADC_Q2FDR		0x080
#define PSADC_VERSION		0xFFC

#define PSADC_MCR_Q1_TRIGS		BIT(22)
#define PSADC_MCR_Q1_INTE		BIT(18)
#define PSADC_MCR_QUE_COMB		BIT(1)
#define PSADC_MCR_EN			BIT(0)

#define PSADC_MSR_Q1_FERR		BIT(2)
#define PSADC_MSR_Q1_INT		BIT(0)

#define PSADC_Q1FCR_FIFO_DRTH_SHIFT	11
#define PSADC_Q1FCR_UF_STS		BIT(17)
#define PSADC_Q1FCR_OF_STS		BIT(16)
#define PSADC_Q1FCR_FIFO_ERRIE		BIT(3)
#define PSADC_Q1FCR_FIFO_FLUSH		BIT(0)

#define PSADC_Q1FDR_CHNUM_SHIFT		12
#define PSADC_Q1FDR_DATA_MASK		GENMASK(11, 0)
#define PSADC_Q1FDR_DATA		BIT(0)
#define PSADC_INVALID_DATA		0xFFF
#define PSADC_ADC_DATA_MIN		0x0
#define PSADC_ADC_DATA_STEP		0x1

struct aic_psadc_ch {
	u32 id;
	bool available;
	enum aic_psadc_mode mode;
	u16 latest_data;
	struct completion complete;
};

struct aic_psadc_data {
	int				num_bits;
	const struct iio_chan_spec	*channels;
	int				num_channels;
	u32				fifo_depth[AIC_PSADC_MAX_CH];
};

struct aic_psadc_dev {
	struct platform_device		*pdev;
	void __iomem			*regs;
	struct clk			*clk;
	struct reset_control		*rst;
	u32				irq;
	u32				pclk_rate;

	struct aic_psadc_ch		chan[AIC_PSADC_MAX_CH];
	const struct aic_psadc_data	*data;
};

static DEFINE_SPINLOCK(user_lock);

static const int aic_psadc_adc_raw_available[] = {
	PSADC_ADC_DATA_MIN, PSADC_ADC_DATA_STEP, PSADC_INVALID_DATA,
};

// TODO: Add the transform algorithm, offered by SD later
static s32 psadc_data2vol(u16 data)
{
	return data;
}

static void psadc_reg_enable(void __iomem *base, int offset, int bit, int enable)
{
	int tmp = readl(base + offset);

	if (enable)
		tmp |= bit;
	else
		tmp &= ~bit;

	writel(tmp, base + offset);
}

static void psadc_enable(void __iomem *regs, int enable)
{
	spin_lock(&user_lock);
	psadc_reg_enable(regs, PSADC_MCR, PSADC_MCR_EN, enable);
	spin_unlock(&user_lock);
}

static void psadc_single_queue_mode(void __iomem *regs, int enable)
{
	psadc_reg_enable(regs, PSADC_MCR, PSADC_MCR_QUE_COMB, enable);
}

static void psadc_qc_irq_enable(void __iomem *regs, int enable)
{
	psadc_reg_enable(regs, PSADC_MCR, PSADC_MCR_Q1_INTE, enable);
}

static void psadc_fifo_init(struct aic_psadc_dev *psadc)
{
	u32 val = 0;

	val = 1 << PSADC_Q1FCR_FIFO_DRTH_SHIFT;

	writel(val, psadc->regs + PSADC_Q1FCR);
	writel(val, psadc->regs + PSADC_Q2FCR);
}

static void psadc_fifo_flush(struct aic_psadc_dev *psadc, u32 ch)
{
	struct device *dev = &psadc->pdev->dev;
	void __iomem *regs = psadc->regs;
	u32 val = readl(regs + PSADC_Q1FCR);

	if (val & PSADC_Q1FCR_UF_STS)
		dev_err(dev, "ch%d FIFO is Underflow!%#x\n", ch, val);
	if (val & PSADC_Q1FCR_OF_STS)
		dev_err(dev, "ch%d FIFO is Overflow!%#x\n", ch, val);

	writel(val | PSADC_Q1FCR_FIFO_FLUSH, regs + PSADC_Q1FCR);
}

static int psadc_ch_init(struct aic_psadc_dev *psadc, u32 ch)
{
	void __iomem *regs = psadc->regs;
	struct aic_psadc_ch *chan = &psadc->chan[ch];

	init_completion(&chan->complete);
	psadc_fifo_init(psadc);
	writel(ch, regs + PSADC_NODE1);
	psadc_reg_enable(regs, PSADC_MCR, PSADC_MCR_Q1_TRIGS, 1);
	psadc_reg_enable(regs, PSADC_MCR, PSADC_MCR_Q1_INTE, 1);

	/* For single mode, should init the channel in .read_raw() */
	return 0;
}

static int aic_psadc_read_dat(struct aic_psadc_dev *psadc, u32 ch)
{
	void __iomem *regs = psadc->regs;
	struct aic_psadc_ch *chan = &psadc->chan[ch];

	chan->latest_data = readl(regs + PSADC_Q1FDR) & PSADC_Q1FDR_DATA_MASK;

	return 0;
}

static int aic_psadc_read_raw(struct iio_dev *iodev,
			      struct iio_chan_spec const *chan,
			      int *val, int *val2, long mask)
{
	struct aic_psadc_dev *psadc = iio_priv(iodev);
	struct device *dev = &psadc->pdev->dev;
	struct aic_psadc_ch *psadc_ch = NULL;
	void __iomem *regs = psadc->regs;
	u32 ch = chan->channel;

	if (unlikely(chan->channel < 0 || chan->channel >= AIC_PSADC_MAX_CH)) {
		dev_err(dev, "Invalid channel No.%d", chan->channel);
		return -ENODEV;
	}
	psadc_ch = &psadc->chan[ch];
	if (!psadc_ch->available) {
		dev_warn(dev, "Channel %d is unavailable", ch);
		return -ENODEV;
	}

	dev_dbg(&psadc->pdev->dev, "ch %d, mask %#lx\n", ch, mask);
	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		psadc_ch_init(psadc, ch);
		spin_lock(&user_lock);
		reinit_completion(&psadc_ch->complete);
		spin_unlock(&user_lock);

		if (!wait_for_completion_timeout(&psadc_ch->complete,
						 AIC_PSADC_TIMEOUT)) {
			dev_err(dev, "Ch%d read timeout!\n", ch);
			psadc_qc_irq_enable(regs, 0);
			return -ETIMEDOUT;
		}

		psadc_qc_irq_enable(regs, 0);
		*val = psadc_data2vol(psadc_ch->latest_data);
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_SCALE:
	default:
		return -EINVAL;
	}
}

static const struct iio_event_spec aic_psadc_event[] = {
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

static irqreturn_t aic_psadc_isr(int irq, void *dev_id)
{
	unsigned long flags;
	u32 q_flag = 0, chan_flag = 0;
	struct aic_psadc_ch *chan = NULL;
	struct aic_psadc_dev *psadc = dev_id;
	void __iomem *regs = psadc->regs;

	spin_lock_irqsave(&user_lock, flags);

	chan_flag = readl(regs + PSADC_NODE1);
	q_flag = readl(regs + PSADC_MSR);
	writel(q_flag, regs + PSADC_MSR);
	chan = &psadc->chan[chan_flag];

	if (q_flag | PSADC_MSR_Q1_INT) {
		aic_psadc_read_dat(psadc, chan_flag);
		complete(&chan->complete);
	}
	if (q_flag | PSADC_MSR_Q1_FERR)
		psadc_fifo_flush(psadc, chan_flag);

	spin_unlock_irqrestore(&user_lock, flags);
	return IRQ_HANDLED;
}

static int aic_psadc_read_avail(struct iio_dev *iodev,
				struct iio_chan_spec const *chan,
				const int **vals, int *type, int *length,
				long mask)
{
	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		*vals = aic_psadc_adc_raw_available;
		*type = IIO_VAL_INT;
		return IIO_AVAIL_RANGE;
	default:
		return -EINVAL;
	}
}

static const struct iio_info aic_psadc_iio_info = {
	.read_raw = aic_psadc_read_raw,
	.read_avail = aic_psadc_read_avail,
};

#define PSADC_CHANNEL(_index, _id) {				\
	.type = IIO_VOLTAGE,					\
	.indexed = 1,						\
	.channel = _index,					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),	\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
	.info_mask_separate_available = BIT(IIO_CHAN_INFO_RAW),	\
	.datasheet_name = _id,					\
}

static const struct iio_chan_spec aic_psadc_v1_iio_channels[] = {
	PSADC_CHANNEL(0, "adc0"),
	PSADC_CHANNEL(1, "adc1"),
	PSADC_CHANNEL(2, "adc2"),
	PSADC_CHANNEL(3, "adc3"),
	PSADC_CHANNEL(4, "adc4"),
	PSADC_CHANNEL(5, "adc5"),
	PSADC_CHANNEL(6, "adc6"),
	PSADC_CHANNEL(7, "adc7"),
	PSADC_CHANNEL(8, "adc8"),
	PSADC_CHANNEL(9, "adc9"),
	PSADC_CHANNEL(10, "adc10"),
	PSADC_CHANNEL(11, "adc11"),
	{
		.event_spec = aic_psadc_event,
		.num_event_specs = ARRAY_SIZE(aic_psadc_event),
	}
};

static const struct aic_psadc_data aic_psadc_v1_data = {
	.num_bits = 12,
	.channels = aic_psadc_v1_iio_channels,
	.num_channels = ARRAY_SIZE(aic_psadc_v1_iio_channels),
	.fifo_depth = {12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12},
};

static const struct of_device_id aic_psadc_match[] = {
	{
		.compatible = "artinchip,aic-psadc-v1.0",
		.data = &aic_psadc_v1_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, aic_psadc_match);

#ifdef CONFIG_PM_SLEEP
static int aic_psadc_suspend(struct device *dev)
{
	struct iio_dev *iodev = dev_get_drvdata(dev);
	struct aic_psadc_dev *psadc = iio_priv(iodev);

	psadc_enable(psadc->regs, 0);

	reset_control_assert(psadc->rst);
	clk_disable_unprepare(psadc->clk);

	return 0;
}

static int aic_psadc_resume(struct device *dev)
{
	struct iio_dev *iodev = dev_get_drvdata(dev);
	struct aic_psadc_dev *psadc = iio_priv(iodev);
	int ret;

	ret = clk_prepare_enable(psadc->clk);
	if (ret)
		return ret;

	ret = reset_control_deassert(psadc->rst);
	if (ret)
		return ret;

	psadc_enable(psadc->regs, 1);
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(aic_psadc_pm_ops, aic_psadc_suspend,
			 aic_psadc_resume);

static int psadc_parse_dt(struct aic_psadc_dev *psadc)
{
	struct device *dev = &psadc->pdev->dev;
	struct device_node *child, *np = dev->of_node;
	u32 i = 0;

	for_each_child_of_node(np, child) {
		struct aic_psadc_ch *chan = &psadc->chan[i];

		chan->id = i;
		chan->available = of_device_is_available(child);
		if (!chan->available) {
			dev_dbg(dev, "ch%d is unavailable.\n", i);
			i++;
			continue;
		}
		dev_dbg(dev, "ch%d is available\n", i);

		chan->mode = AIC_PSADC_MODE_SINGLE;
		i++;
	}

	return 0;
}

static int aic_psadc_probe(struct platform_device *pdev)
{
	struct aic_psadc_dev *psadc = NULL;
	struct iio_dev *iodev = NULL;
	const struct of_device_id *match;
	struct clk *clk;
	int ret, irq, i;

	if (!pdev->dev.of_node)
		return -ENODEV;

	iodev = devm_iio_device_alloc(&pdev->dev, sizeof(*psadc));
	if (!iodev)
		return -ENOMEM;
	psadc = iio_priv(iodev);

	match = of_match_device(aic_psadc_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "failed to match device\n");
		return -ENODEV;
	}
	psadc->data = match->data;

	psadc->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(psadc->regs))
		return PTR_ERR(psadc->regs);

	clk = devm_clk_get(&pdev->dev, "pclk");
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "Failed to get pclk\n");
		return PTR_ERR(clk);
	}
	psadc->pclk_rate = clk_get_rate(clk);
	dev_dbg(&pdev->dev, "PCLK rate %d\n", psadc->pclk_rate);

	psadc->clk = devm_clk_get(&pdev->dev, "psadc");
	if (IS_ERR(psadc->clk)) {
		dev_err(&pdev->dev, "no clock defined\n");
		return PTR_ERR(psadc->clk);
	}
	ret = clk_prepare_enable(psadc->clk);
	if (ret) {
		dev_err(&pdev->dev, "Failed to enable clk, return %d\n", ret);
		return ret;
	}

	psadc->rst = devm_reset_control_get_optional_shared(&pdev->dev, NULL);
	if (IS_ERR(psadc->rst)) {
		ret = PTR_ERR(psadc->rst);
		goto disable_clk;
	}
	reset_control_deassert(psadc->rst);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "Failed to get irq\n");
		goto disable_rst;
	}

	ret = devm_request_irq(&pdev->dev, irq, aic_psadc_isr,
			       0, AIC_PSADC_NAME, psadc);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to request IRQ %d\n", irq);
		goto disable_rst;
	}
	psadc->irq = irq;
	psadc->pdev = pdev;
	psadc_single_queue_mode(psadc->regs, 1);
	psadc_enable(psadc->regs, 1);
	platform_set_drvdata(pdev, iodev);
	psadc_parse_dt(psadc);

	iodev->name = AIC_PSADC_NAME;
	iodev->dev.parent = &pdev->dev;
	iodev->dev.of_node = pdev->dev.of_node;
	iodev->info = &aic_psadc_iio_info;
	iodev->modes = INDIO_DIRECT_MODE;

	iodev->channels = psadc->data->channels;
	iodev->num_channels = psadc->data->num_channels;

	ret = iio_device_register(iodev);
	if (ret)
		goto free_irq;

	for (i = 0; i < AIC_PSADC_MAX_CH; i++) {
		if (psadc->chan[i].available)
			psadc_ch_init(psadc, i);
	}

	dev_info(&pdev->dev, "Artinchip PSADC Loaded.\n");
	return 0;

free_irq:
	free_irq(psadc->irq, psadc);
disable_rst:
	reset_control_assert(psadc->rst);
disable_clk:
	clk_disable_unprepare(psadc->clk);

	return ret;
}

static int aic_psadc_remove(struct platform_device *pdev)
{
	struct iio_dev *iodev = platform_get_drvdata(pdev);
	struct aic_psadc_dev *psadc = iio_priv(iodev);

	iio_device_unregister(iodev);
	psadc_enable(psadc->regs, 0);

	free_irq(psadc->irq, psadc);
	reset_control_assert(psadc->rst);
	clk_disable_unprepare(psadc->clk);
	return 0;
}

static struct platform_driver aic_psadc_driver = {
	.probe		= aic_psadc_probe,
	.remove		= aic_psadc_remove,
	.driver		= {
		.name	= AIC_PSADC_NAME,
		.of_match_table = aic_psadc_match,
		.pm	= &aic_psadc_pm_ops,
	},
};
module_platform_driver(aic_psadc_driver);

MODULE_AUTHOR("Siyao.Li <siyao.li@artinchip.com>");
MODULE_DESCRIPTION("PSADC driver of Artinchip SoC");
MODULE_LICENSE("GPL");
