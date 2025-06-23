// SPDX-License-Identifier: GPL-2.0-only
/*
 * Resistive Touch Panel driver of ArtInChip SoC
 *
 * Copyright (C) 2020-2025 ArtInChip Technology Co., Ltd.
 * Authors:  Matteo <duanmt@artinchip.com>
 */

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/reset.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#define AIC_RTP_NAME			"aic-rtp"
#define AIC_RTP_FIFO_DEPTH		16
#define AIC_RTP_DEFAULT_X_PLATE		280
#define AIC_RTP_DEFAULT_Y_PLATE		600
#define AIC_RTP_SCATTER_THD		48
#define AIC_RTP_MAX_VAL			0xFFF
#define AIC_RTP_VAL_RANGE		(AIC_RTP_MAX_VAL + 1)
#define AIC_RTP_INVALID_VAL		(AIC_RTP_MAX_VAL + 1)
#define AIC_RTP_MAX_PDEB_VAL		0xFFFFFFFF
#define AIC_RTP_MAX_DELAY_VAL		0xFFFFFFFF
#define AIC_RTP_DEFALUT_DELAY_VAL	0x4f00004f

/* Register definition for RTP */
#define RTP_MCR			0x000
#define RTP_INTR		0x004
#define RTP_PDEB		0x008
#define RTP_PCTL		0x00C
#define RTP_CHCFG		0x010
#define RTP_MMSC		0x014
#define RTP_FIL			0x018
#define RTP_AMSC		0x01C
#define RTP_FCR			0x020
#define RTP_DATA		0x024
#define RTP_DLY			0x028
#define RTP_VERSION		0xFFC

#define RTP_MCR_PRES_DET_BYPASS BIT(16)
#define RTP_MCR_RISE_STS	BIT(12)
#define RTP_MCR_PRES_DET_EN	BIT(8)
#define RTR_MCR_MODE_SHIFT	4
#define RTR_MCR_MODE_MASK	GENMASK(7, 4)
#define RTP_MCR_EN		BIT(0)

#define RTP_INTR_SCI_FLG	BIT(21)
#define RTP_INTR_DOUR_FLG	BIT(20)
#define RTP_INTR_FIFO_FLG	BIT(19)
#define RTP_INTR_DRDY_FLG	BIT(18)
#define RTP_INTR_RISE_DET_FLG	BIT(17)
#define RTP_INTR_PRES_DET_FLG	BIT(16)
#define RTP_INTR_SCI_IE         BIT(5)
#define RTP_INTR_DOUR_INTEN	BIT(4)
#define RTP_INTR_FIFO_ERR_IE	BIT(3)
#define RTP_INTR_DAT_RDY_IE	BIT(2)
#define RTP_INTR_RISE_DET_IE	BIT(1)
#define RTP_INTR_PRES_DET_IE	BIT(0)

#define RTP_PCTL_PRES_DET_BYPASS		BIT(16)

#define RTP_MMSC_VREF_MINUS_SEL_SHIFT		22
#define RTP_MMSC_VREF_PLUS_SEL_SHIFT		20
#define RTP_MMSC_XY_DRV_X_PLUS			BIT(19)
#define RTP_MMSC_XY_DRV_Y_PLUS			BIT(18)
#define RTP_MMSC_XY_DRV_X_MINUS			BIT(17)
#define RTP_MMSC_XY_DRV_Y_MINUS			BIT(16)
#define RTP_MMSC_XY_DRV_SHIFT			16
#define RTP_MMSC_SMP_CNT_PER_TRIG_SHIFT		8
#define RTP_MMSC_SMP_CH_SEL_SHIFT		4
#define RTP_MMSC_SMP_TRIG			BIT(0)

#define RTP_FIL_Z_REL_RANGE_SHIFT		28
#define RTP_FIL_X_ABS_RANGE_SHIFT		24
#define RTP_FIL_XY_REL_RANGE_SHIFT		20
#define RTP_FIL_XY_ABS_RANGE_SHIFT		16

#define RTP_AMSC_PERIOD_SAMPLE_INT_SHIFT	12
#define RTP_AMSC_PERIOD_SAMPLE_EN		BIT(1)
#define RTP_AMSC_SINGLE_SAMPLE_EN		BIT(0)

#define RTP_FCR_DAT_CNT_SHIFT		24
#define RTP_FCR_DAT_CNT_MASK		GENMASK(28, 24)
#define RTP_FCR_UF_FLAG			BIT(18)
#define RTP_FCR_OF_FLAG			BIT(17)
#define RTP_FCR_DAT_RDY_THD_SHIFT	8
#define RTP_FCR_DAT_RDY_THD_MASK	GENMASK(12, 8)
#define RTP_FCR_UF_IE			BIT(2)
#define RTP_FCR_OF_IE			BIT(1)
#define RTP_FCR_FLUSH			BIT(0)

#define RTP_DATA_CH_NUM_SHIFT		12
#define RTP_DATA_CH_NUM_MASK		GENMASK(13, 12)
#define RTP_DATA_DATA_MASK		GENMASK(11, 0)

enum aic_rtp_mode {
	RTP_MODE_MANUAL = 0,
	RTP_MODE_AUTO1,
	RTP_MODE_AUTO2,
	RTP_MODE_AUTO3,
	RTP_MODE_AUTO4
};

enum aic_rtp_vref_minus_sel {
	RTP_VREF_MINUS_2_GND = 0,
	RTP_VREF_MINUS_2_X_MINUS,
	RTP_VREF_MINUS_2_Y_MINUS
};

enum aic_rtp_vref_plus_sel {
	RTP_VREF_PLUS_2_VCC = 0,
	RTP_VREF_PLUS_2_X_PLUS,
	RTP_VREF_PLUS_2_Y_PLUS
};

enum aic_rtp_relative_range {
	RTP_REL_RANGE_DISABLE = 0,
	RTP_REL_RANGE_MAX_1_8,
	RTP_REL_RANGE_MAX_1_16,
	RTP_REL_RANGE_MAX_1_32,
	RTP_REL_RANGE_MAX_1_64,
	RTP_REL_RANGE_MAX_1_128,
	RTP_REL_RANGE_MAX_1_256,
	RTP_REL_RANGE_MAX_1_512
};

enum aic_rtp_absolute_range {
	RTP_ABS_RANGE_DISABLE = 0,
	RTP_ABS_RANGE_MAX_2_9,
	RTP_ABS_RANGE_MAX_2_8,
	RTP_ABS_RANGE_MAX_2_7,
	RTP_ABS_RANGE_MAX_2_6,
	RTP_ABS_RANGE_MAX_2_5,
	RTP_ABS_RANGE_MAX_2_4,
	RTP_ABS_RANGE_MAX_2_3
};

/* n_m - Pick out n samples from m continuous samples,
 * and drop (m-n)/2 max and (m-n)/2 min samples.
 */
enum aic_rtp_filter_type {
	RTP_FILTER_NONE = 0,
	RTP_FILTER_2_4,
	RTP_FILTER_4_6,
	RTP_FILTER_4_8
};

enum aic_rtp_ch {
	RTP_CH_Y_MINUS = 0,
	RTP_CH_X_MINUS,
	RTP_CH_Y_PLUS,
	RTP_CH_X_PLUS,
	RTP_CH_Z_A,
	RTP_CH_Z_B,
	RTP_CH_Z_C,
	RTP_CH_Z_D,

	RTP_CH_MAX
};

struct aic_rtp_dat {
	u16 y_minus;
	u16 x_minus;
	u16 y_plus;
	u16 x_plus;
	u16 z_a;
	u16 z_b;
	u16 z_c;
	u16 z_d;
};

enum aic_rtp_manual_mode_status {
	RTP_MMS_X_MINUS = 0,
	RTP_MMS_Y_MINUS,
	RTP_MMS_Z_A,
	RTP_MMS_Z_B,
	RTP_MMS_DOWN,
	RTP_MMS_IDLE,
	RTP_MMS_FINISH
};

struct aic_rtp_dev {
	struct platform_device *pdev;
	struct attribute_group attrs;
	struct clk *clk;
	struct reset_control *rst;

	struct device *dev;
	struct input_dev *idev;
	void __iomem *regs;
	u32 irq;
	u32 pclk_rate;

	bool two_points;
	bool x_flip;
	bool y_flip;
	bool pressure_det;
	bool ignore_fifo_data;
	enum aic_rtp_mode mode;
	u32 max_press;
	u32 smp_period;
	u32 x_plate;
	u32 y_plate;
	u32 fuzz;
	u32 pdeb;
	u32 delay;

	struct workqueue_struct *workq;
	struct work_struct event_work;
	u32 intr;
	u32 fcr;
	struct aic_rtp_dat latest;
	enum aic_rtp_manual_mode_status mms;
};

static DEFINE_SPINLOCK(user_lock);

static ssize_t status_show(struct device *dev,
			   struct device_attribute *devattr, char *buf)
{
	int mcr, version;
	struct aic_rtp_dev *rtp = dev_get_drvdata(dev);
	void __iomem *regs = rtp->regs;

	spin_lock(&user_lock);
	mcr = readl(regs + RTP_MCR);
	version = readl(regs + RTP_VERSION);
	spin_unlock(&user_lock);

	return sprintf(buf, "In RTP controller V%d.%02d:\n"
		       "Mode %d/%d, RTP enale %d, Press detect enable %d\n"
		       "Manual mode status %d\n"
		       "Pressure enable %d, max %d, x-plate %d, y-plate %d\n"
		       "Point num: %d, Sample period: %d, Fuzz: %d\n",
		       version >> 8, version & 0xff,
		       (u32)((mcr & RTR_MCR_MODE_MASK) >> RTR_MCR_MODE_SHIFT),
		       rtp->mode,
		       (u32)(mcr & RTP_MCR_EN),
		       (mcr & RTP_MCR_PRES_DET_EN) ? 1 : 0, rtp->mms,
		       rtp->pressure_det, rtp->max_press,
		       rtp->x_plate, rtp->y_plate,
		       rtp->two_points + 1, rtp->smp_period, rtp->fuzz);
}
static DEVICE_ATTR_RO(status);

static struct attribute *aic_rtp_attr[] = {
	&dev_attr_status.attr,
	NULL
};

static u32 rtp_ms2itv(struct aic_rtp_dev *rtp, u32 ms)
{
	u32 tmp = rtp->pclk_rate / 1000;

	tmp = (tmp * ms) >> 12;
	return tmp;
}

static u32 rtp_average(u16 *dat, u32 size)
{
	u32 max = dat[0], min = dat[0];
	u32 i, sum = 0;

	if (unlikely(size < 3)) {
		pr_warn("%s() - Invalid data size %d!\n", __func__, size);
		return 0;
	}

	for (i = 0; i < size; i++) {
		if (dat[i] > max)
			max = dat[i];
		if (dat[i] < min)
			min = dat[i];
		sum += dat[i];
	}
	if (max - min > AIC_RTP_SCATTER_THD) {
		pr_warn("%s() Scatter data! %d/%d", __func__, min, max);
		return AIC_RTP_INVALID_VAL;
	}

	return (sum - min - max) / (size - 2);
}

static bool rtp_is_rise(void __iomem *regs)
{
	return readl(regs + RTP_MCR) & RTP_MCR_RISE_STS ? true : false;
}

static void rtp_reg_enable(void __iomem *base, int offset, int bit, int enable)
{
	int tmp;

	tmp = readl(base + offset);
	tmp &= ~bit;
	if (enable)
		tmp |= bit;

	writel(tmp, base + offset);
}

/* Driver x- and y- to GND */
static void rtp_drv_xy2gnd(void __iomem *regs)
{
	writel(RTP_MMSC_XY_DRV_X_MINUS | RTP_MMSC_XY_DRV_Y_MINUS,
	       regs + RTP_MMSC);
}

static void rtp_mm_smp(void __iomem *regs, u32 mmsc, char *name)
{
	u32 val = 7 << RTP_MMSC_SMP_CNT_PER_TRIG_SHIFT | mmsc;

	rtp_drv_xy2gnd(regs);

	writel(val, regs + RTP_MMSC);
	val |= RTP_MMSC_SMP_TRIG;
	pr_debug("%s() %s MMSC: %#x\n", __func__, name, val);

	usleep_range(100, 1000);
	writel(val, regs + RTP_MMSC);
}

static void rtp_mm_smp_xn(void __iomem *regs)
{
	rtp_mm_smp(regs, RTP_CH_X_MINUS << RTP_MMSC_SMP_CH_SEL_SHIFT
		   | RTP_MMSC_XY_DRV_Y_PLUS | RTP_MMSC_XY_DRV_Y_MINUS
		   | RTP_VREF_PLUS_2_Y_PLUS << RTP_MMSC_VREF_PLUS_SEL_SHIFT
		   | RTP_VREF_MINUS_2_Y_MINUS << RTP_MMSC_VREF_MINUS_SEL_SHIFT,
		   "XN");
}

static void rtp_mm_smp_yn(void __iomem *regs)
{
	rtp_mm_smp(regs, RTP_MMSC_XY_DRV_X_PLUS | RTP_MMSC_XY_DRV_X_MINUS
		   | RTP_VREF_PLUS_2_X_PLUS << RTP_MMSC_VREF_PLUS_SEL_SHIFT
		   | RTP_VREF_MINUS_2_X_MINUS << RTP_MMSC_VREF_MINUS_SEL_SHIFT,
		   "YN");
}

static void rtp_mm_smp_zb(void __iomem *regs)
{
	rtp_mm_smp(regs, RTP_CH_X_MINUS << RTP_MMSC_SMP_CH_SEL_SHIFT
		   | RTP_MMSC_XY_DRV_X_PLUS | RTP_MMSC_XY_DRV_Y_MINUS
		   | RTP_VREF_PLUS_2_X_PLUS << RTP_MMSC_VREF_PLUS_SEL_SHIFT
		   | RTP_VREF_MINUS_2_Y_MINUS << RTP_MMSC_VREF_MINUS_SEL_SHIFT,
		   "ZB");
}

static void rtp_mm_smp_za(void __iomem *regs)
{
	rtp_mm_smp(regs, RTP_CH_Y_PLUS << RTP_MMSC_SMP_CH_SEL_SHIFT
		   | RTP_MMSC_XY_DRV_X_PLUS | RTP_MMSC_XY_DRV_Y_MINUS
		   | RTP_VREF_PLUS_2_X_PLUS << RTP_MMSC_VREF_PLUS_SEL_SHIFT
		   | RTP_VREF_MINUS_2_Y_MINUS << RTP_MMSC_VREF_MINUS_SEL_SHIFT,
		   "ZA");
}

static void rtp_det_enable(void __iomem *regs, bool enable)
{
	if (enable) {
		rtp_drv_xy2gnd(regs);
		writel(RTP_MMSC_XY_DRV_Y_MINUS, regs + RTP_MMSC);
	}
	rtp_reg_enable(regs, RTP_MCR, RTP_MCR_PRES_DET_EN, enable);
}

static void rtp_det_refresh(void __iomem *regs)
{
	rtp_det_enable(regs, true); // Make RISE_STS updated
	usleep_range(500, 2000);
	rtp_det_enable(regs, false);
}

static void rtp_enable(struct aic_rtp_dev *rtp, int en)
{
	void __iomem *regs = rtp->regs;
	enum aic_rtp_mode mode = rtp->mode;

	spin_lock(&user_lock);
	rtp_reg_enable(regs, RTP_MCR,
		mode << RTR_MCR_MODE_SHIFT | RTP_MCR_PRES_DET_EN | RTP_MCR_EN,
		en);

	if (mode == RTP_MODE_MANUAL)
		writel(0x09C409C4, regs + RTP_PDEB);
	else {
#if defined(CONFIG_ARTINCHIP_ADCIM_DM)
		writel(0, regs + RTP_PDEB);
#else
		if (of_device_is_compatible(rtp->dev->of_node,
						"artinchip,aic-rtp-v0.1")) {
			writel(rtp->pdeb, regs + RTP_PDEB);
		}
#endif
	}

	if (mode != RTP_MODE_MANUAL) {
		if (of_device_is_compatible(rtp->dev->of_node,
						"artinchip,aic-rtp-v0.1")) {
			rtp_reg_enable(regs, RTP_PCTL, RTP_PCTL_PRES_DET_BYPASS, en);
		} else {
			writel(rtp->delay, regs + RTP_DLY);
			rtp_reg_enable(regs, RTP_MCR, RTP_MCR_PRES_DET_BYPASS, en);
		}
	}

	spin_unlock(&user_lock);
}

static void rtp_int_enable(struct aic_rtp_dev *rtp, int en)
{
	u32 val = RTP_INTR_FIFO_ERR_IE | RTP_INTR_DAT_RDY_IE
			| RTP_INTR_RISE_DET_IE | RTP_INTR_SCI_IE;

	if (rtp->mode == RTP_MODE_MANUAL)
		val |= RTP_INTR_PRES_DET_IE;

	spin_lock(&user_lock);
	rtp_reg_enable(rtp->regs, RTP_INTR, val, en);
	spin_unlock(&user_lock);
}

static void rtp_fifo_flush(struct aic_rtp_dev *rtp)
{
	struct device *dev = &rtp->pdev->dev;
	void __iomem *regs = rtp->regs;
	u32 sta = readl(regs + RTP_FCR);

	if (sta & RTP_FCR_UF_FLAG)
		dev_err(dev, "FIFO is Underflow!%#x\n", sta);
	if (sta & RTP_FCR_OF_FLAG)
		dev_err(dev, "FIFO is Overflow!%#x\n", sta);

	writel(sta | RTP_FCR_FLUSH, regs + RTP_FCR);
}

static void rtp_fifo_init(struct aic_rtp_dev *rtp)
{
	u32 thd = 0;

	switch (rtp->mode) {
	case RTP_MODE_AUTO1:
		if (rtp->smp_period)
			thd = 8;
		else
			thd = 2;
		break;
	case RTP_MODE_AUTO2:
		if (rtp->smp_period)
			thd = 12;
		else
			thd = 4;
		break;
	case RTP_MODE_AUTO3:
		if (rtp->smp_period)
			thd = 12;
		else
			thd = 6;
		break;
	case RTP_MODE_AUTO4:
	default:
		thd = 8;
		break;
	}
	thd <<= RTP_FCR_DAT_RDY_THD_SHIFT;

	writel(thd | RTP_FCR_UF_IE | RTP_FCR_OF_IE, rtp->regs + RTP_FCR);
}

static u32 rtp_press_calc(struct aic_rtp_dev *rtp)
{
	struct device *dev = &rtp->pdev->dev;
	struct aic_rtp_dat *dat = &rtp->latest;
	u32 pressure = rtp->x_plate * dat->x_minus / AIC_RTP_VAL_RANGE;

	if (rtp->y_plate) {
		pressure = pressure * (AIC_RTP_VAL_RANGE - dat->z_a) / dat->z_a;
		pressure -= rtp->y_plate * (AIC_RTP_VAL_RANGE - dat->y_minus)
				/ AIC_RTP_VAL_RANGE;
	} else {
		pressure = pressure * (dat->z_b - dat->z_a) / dat->z_a;
	}
	dev_dbg(dev, "%s() Current pressure: %d", __func__, pressure);

	if (pressure > rtp->max_press) {
		dev_dbg(dev, "Invalid pressure %d", pressure);
		pressure = AIC_RTP_INVALID_VAL;
	}
#if defined(CONFIG_ARTINCHIP_ADCIM_DM)
	return (dat->z_a + dat->z_b) / 2;
#else
	return pressure;
#endif
}

static void aic_rtp_xy_flip(struct aic_rtp_dev *rtp, u16 *x_data, u16 *y_data)
{
	if (rtp->y_flip)
		*y_data = AIC_RTP_MAX_VAL - *y_data;

	if (rtp->x_flip)
		*x_data = AIC_RTP_MAX_VAL - *x_data;
}

static void rtp_report_abs(struct aic_rtp_dev *rtp)
{
	struct aic_rtp_dat *dat = &rtp->latest;

	if (dat->x_minus == AIC_RTP_INVALID_VAL
	    || dat->y_minus == AIC_RTP_INVALID_VAL)
		return;

	if (rtp->pressure_det) {
		int pressure = rtp_press_calc(rtp);

		if (pressure == AIC_RTP_INVALID_VAL)
			return;
		input_report_abs(rtp->idev, ABS_PRESSURE, pressure);
	}
	aic_rtp_xy_flip(rtp, &dat->x_minus, &dat->y_minus);
	input_report_abs(rtp->idev, ABS_X, dat->x_minus);
	input_report_abs(rtp->idev, ABS_Y, dat->y_minus);
	input_report_key(rtp->idev, BTN_TOUCH, 1);
	input_sync(rtp->idev);
}

static void rtp_manual_mode(struct aic_rtp_dev *rtp, u32 data)
{
	struct device *dev = &rtp->pdev->dev;
	void __iomem *regs = rtp->regs;

	dev_dbg(dev, "Current MMS %d, data %d\n", rtp->mms, data);
	/* The normal status transfer:
	 * IDLE -> DOWN -> X_MINUS -> Y_MINUS -> Z_B -> Z_A
	 *                    ^___________|_______|______|
	 */
	switch (rtp->mms) {
	case RTP_MMS_IDLE:
		rtp_fifo_init(rtp);
		rtp_det_enable(regs, true);
		rtp->mms = RTP_MMS_DOWN;
		memset(&rtp->latest, 0, sizeof(struct aic_rtp_dat));
		break;

	case RTP_MMS_DOWN:
		rtp_det_enable(regs, false);
		rtp_mm_smp_xn(regs);
		rtp->mms = RTP_MMS_X_MINUS;
		break;

	case RTP_MMS_X_MINUS:
		rtp->latest.x_minus = data;
		if (rtp_is_rise(regs)) {
			rtp_det_enable(regs, true);
			rtp->mms = RTP_MMS_DOWN;
			break;
		}

		rtp_mm_smp_yn(regs);
		rtp->mms = RTP_MMS_Y_MINUS;
		break;

	case RTP_MMS_Y_MINUS:
		rtp->latest.y_minus = data;
		if (!rtp->pressure_det) {
			rtp_report_abs(rtp);
			rtp_det_refresh(regs);

			if (rtp_is_rise(regs)) {
				rtp_det_enable(regs, true);
				rtp->mms = RTP_MMS_DOWN;
			} else {
				rtp_mm_smp_xn(regs);
				rtp->mms = RTP_MMS_X_MINUS;
			}
			break;
		}
		rtp_mm_smp_za(regs);
		rtp->mms = RTP_MMS_Z_A;
		break;

	case RTP_MMS_Z_A:
		rtp->latest.z_a = data;
		if (rtp->y_plate) {
			rtp_report_abs(rtp);
			rtp_det_refresh(regs);

			if (rtp_is_rise(regs)) {
				rtp_det_enable(regs, true);
				rtp->mms = RTP_MMS_DOWN;
			} else {
				rtp_mm_smp_xn(regs);
				rtp->mms = RTP_MMS_X_MINUS;
			}
			break;
		}
		rtp_mm_smp_zb(regs);
		rtp->mms = RTP_MMS_Z_B;
		break;

	case RTP_MMS_Z_B:
		rtp->latest.z_b = data;
		rtp_report_abs(rtp);
		rtp_det_refresh(regs);

		if (rtp_is_rise(regs)) {
			rtp_det_enable(regs, true);
			rtp->mms = RTP_MMS_DOWN;
		} else {
			rtp_mm_smp_xn(regs);
			rtp->mms = RTP_MMS_X_MINUS;
		}
		break;

	default:
		dev_err(dev, "Invalid manual mode status %d", rtp->mms);
		break;
	}
}

static void rtp_smp_period(void __iomem *regs, u32 period)
{
	u32 val = 0;

	if (period) {
		val = period << RTP_AMSC_PERIOD_SAMPLE_INT_SHIFT
			| RTP_AMSC_PERIOD_SAMPLE_EN;
	} else {
		val = RTP_AMSC_SINGLE_SAMPLE_EN;
		writel(0, regs + RTP_AMSC);
	}
	writel(val, regs + RTP_AMSC);
}

static void rtp_auto_mode(struct aic_rtp_dev *rtp)
{
	void __iomem *regs = rtp->regs;

	writel(RTP_FILTER_4_8, regs + RTP_FIL);
	rtp_smp_period(regs, rtp->smp_period);
	rtp_fifo_init(rtp);
}

/* Data format: XN, YN */
static void rtp_report_abs_auto1(struct aic_rtp_dev *rtp, u16 *ori, u32 cnt)
{
	u32 i = 0;
	struct aic_rtp_dat *latest = &rtp->latest;

	for (i = 0; i < cnt; ) {
		latest->x_minus = ori[i];
		latest->y_minus = ori[i + 1];
		rtp_report_abs(rtp);

		i += 2;
		dev_dbg(&rtp->pdev->dev, "X %d, Y %d",
			latest->x_minus, latest->y_minus);
	}
}

/* Data format: XN, YN, ZA, ZB */
static void rtp_report_abs_auto2(struct aic_rtp_dev *rtp, u16 *ori, u32 cnt)
{
	u32 i = 0;
	struct aic_rtp_dat *latest = &rtp->latest;

	for (i = 0; i < cnt; ) {
		latest->x_minus = ori[i];
		latest->y_minus = ori[i + 1];
		latest->z_a = ori[i + 2];
		latest->z_b = ori[i + 3];
		rtp_report_abs(rtp);

		i += 4;
		dev_dbg(&rtp->pdev->dev, "X %d, Y %d, ZA %d ZB %d",
			latest->x_minus, latest->y_minus,
			latest->z_a, latest->z_b);
	}
}

static bool rtp_distance_is_far(struct aic_rtp_dat *latest)
{
	bool ret = false;

	if (latest->x_minus != latest->x_plus) {
		if (latest->x_plus > latest->x_minus)
			ret = latest->x_plus > latest->x_minus +
						AIC_RTP_SCATTER_THD;
		else
			ret = latest->x_minus > latest->x_plus +
						AIC_RTP_SCATTER_THD;
	}
	if (ret)
		return true;

	if (latest->y_minus != latest->y_plus) {
		if (latest->y_plus > latest->y_minus)
			ret = latest->y_plus > latest->y_minus +
						AIC_RTP_SCATTER_THD;
		else
			ret = latest->y_minus > latest->y_plus +
						AIC_RTP_SCATTER_THD;
	}
	return ret;
}

/* Data format: XN, XP, YN, YP, ZA, ZB */
static void rtp_report_abs_auto3(struct aic_rtp_dev *rtp, u16 *ori, u32 cnt)
{
	u32 i = 0;
	struct aic_rtp_dat *latest = &rtp->latest;

	for (i = 0; i < cnt; ) {
		latest->x_minus = ori[i];
		latest->x_plus = ori[i + 1];
		latest->y_minus = ori[i + 2];
		latest->y_plus = ori[i + 3];
		latest->z_a = ori[i + 4];
		latest->z_b = ori[i + 5];
		dev_dbg(&rtp->pdev->dev, "X %u-%u, Y %u-%u, ZA %u, ZB %u",
			latest->x_minus, latest->x_plus,
			latest->y_minus, latest->y_plus,
			latest->z_a, latest->z_b);

		if (!rtp_distance_is_far(latest)) {
			latest->x_minus += latest->x_plus;
			latest->x_minus >>= 1;
			latest->y_minus += latest->y_plus;
			latest->y_minus >>= 1;
			rtp_report_abs(rtp);
		} else {
			dev_dbg(&rtp->pdev->dev, "Distance is so far");
		}

		i += 6;
		if (i + 6 > cnt)
			break;
	}
}

/* Data format: XN, XP, YN, YP, ZA, ZB, ZC, ZD */
static void rtp_report_abs_auto4(struct aic_rtp_dev *rtp, u16 *ori, u32 cnt)
{
	u32 i = 0;
	struct aic_rtp_dat *latest = &rtp->latest;

	for (i = 0; i < cnt; ) {
		latest->x_minus = ori[i];
		latest->x_plus = ori[i + 1];
		latest->y_minus = ori[i + 2];
		latest->y_plus = ori[i + 3];
		latest->z_a = ori[i + 4];
		latest->z_b = ori[i + 5];
		latest->z_c = ori[i + 6];
		latest->z_d = ori[i + 7];
		dev_dbg(&rtp->pdev->dev, "X %u-%u, Y %u-%u, ZA %u-%u, ZB %u-%u",
			latest->x_minus, latest->x_plus,
			latest->y_minus, latest->y_plus,
			latest->z_a, latest->z_c,
			latest->z_b, latest->z_d);

#if defined(CONFIG_ARTINCHIP_ADCIM_DM)
		rtp_report_abs(rtp);
		latest->x_minus = latest->x_plus;
		latest->y_minus = latest->y_plus;
		latest->z_a = latest->z_c;
		latest->z_b = latest->z_d;
		rtp_report_abs(rtp);
#else
		if (!rtp_distance_is_far(latest)) {
			latest->x_minus += latest->x_plus;
			latest->x_minus >>= 1;
			latest->y_minus += latest->y_plus;
			latest->y_minus >>= 1;
			rtp_report_abs(rtp);
		} else {
			dev_dbg(&rtp->pdev->dev, "Distance is so far");
		}
#endif

		i += 8;
		if (i + 8 > cnt)
			break;
	}
}

static void aic_rtp_read_fifo(struct aic_rtp_dev *rtp, u32 cnt)
{
	int i;
	u32 tmp;
	u16 data[AIC_RTP_FIFO_DEPTH] = {0};
	void __iomem *regs = rtp->regs;
	struct device *dev = &rtp->pdev->dev;

	tmp = (readl(regs + RTP_FCR) & RTP_FCR_DAT_CNT_MASK)
		>> RTP_FCR_DAT_CNT_SHIFT;
	if (tmp != cnt) {
		if (rtp->mode == RTP_MODE_MANUAL)
			dev_err(dev, "FIFO did changed %d/%d", tmp, cnt);
		else
			cnt = tmp;
	}

	for (i = 0; i < cnt; i++) {
		if (!(readl(regs + RTP_FCR) & RTP_FCR_DAT_CNT_MASK)) {
			dev_err(dev, "FIFO is empty %d/%d", i, cnt);
			return;
		}
		data[i] = readl(regs + RTP_DATA) & RTP_DATA_DATA_MASK;
	}

	tmp = readl(regs + RTP_FCR) & RTP_FCR_DAT_CNT_MASK;
	if (tmp) {
		dev_err(dev, "FIFO is not empty! %d",
			tmp >> RTP_FCR_DAT_CNT_SHIFT);
		rtp_fifo_flush(rtp);
	}

	if (rtp->mode == RTP_MODE_MANUAL) {
		tmp = rtp_average(data, cnt);
		return rtp_manual_mode(rtp, tmp);
	}

	switch (rtp->mode) {
	case RTP_MODE_AUTO1:
		rtp_report_abs_auto1(rtp, data, cnt);
		break;
	case RTP_MODE_AUTO2:
		rtp_report_abs_auto2(rtp, data, cnt);
		break;
	case RTP_MODE_AUTO3:
		rtp_report_abs_auto3(rtp, data, cnt);
		break;
	case RTP_MODE_AUTO4:
		rtp_report_abs_auto4(rtp, data, cnt);
		break;
	default:
		return;
	}
}

static void aic_rtp_manual_worker(struct work_struct *work)
{
	struct aic_rtp_dev *rtp = container_of(work, struct aic_rtp_dev,
					       event_work);

	spin_lock(&user_lock);

	if (rtp->intr & RTP_INTR_PRES_DET_FLG)
		rtp_manual_mode(rtp, 0);

	if (rtp->intr & RTP_INTR_DRDY_FLG)
		aic_rtp_read_fifo(rtp, (rtp->fcr & RTP_FCR_DAT_CNT_MASK)
					  >> RTP_FCR_DAT_CNT_SHIFT);

	spin_unlock(&user_lock);
}

static void aic_rtp_single_smp_worker(struct work_struct *work)
{
	struct aic_rtp_dev *rtp = container_of(work, struct aic_rtp_dev,
					       event_work);

	spin_lock(&user_lock);
	if (rtp->intr & RTP_INTR_DRDY_FLG) {
		aic_rtp_read_fifo(rtp, (rtp->fcr & RTP_FCR_DAT_CNT_MASK)
				  >> RTP_FCR_DAT_CNT_SHIFT);
		spin_unlock(&user_lock);
		usleep_range(9000, 11000);
		spin_lock(&user_lock);
	}

	rtp_smp_period(rtp->regs, rtp->smp_period);
	spin_unlock(&user_lock);
}

static irqreturn_t aic_rtp_irq(int irq, void *dev_id)
{
	struct aic_rtp_dev *rtp = dev_id;
	void __iomem *regs = rtp->regs;
	struct device *dev = &rtp->pdev->dev;
	enum aic_rtp_mode mode = rtp->mode;
	u32 intr, fcr;
	unsigned long flags;

	spin_lock_irqsave(&user_lock, flags);

	intr = readl(regs + RTP_INTR);
	fcr = readl(regs + RTP_FCR);
	writel(fcr, regs + RTP_FCR);
	writel(intr, regs + RTP_INTR);

	dev_dbg(dev, "INTS %#x, FCR %#x, Pressed %d\n",
		intr, fcr, rtp_is_rise(regs));
	if ((intr & RTP_INTR_PRES_DET_FLG) && (intr & RTP_INTR_RISE_DET_FLG)) {
		dev_info(dev, "Press&rise happened at the same time!");
		if (rtp_is_rise(regs))
			intr &= ~RTP_INTR_PRES_DET_FLG;
		else
			intr &= ~RTP_INTR_RISE_DET_FLG;
	}

	if (intr & RTP_INTR_SCI_FLG) {
		dev_dbg(dev, "SCI error, flush the FIFO ...\n");
		goto irq_clean_fifo;
	}

	if (intr & RTP_INTR_DOUR_FLG) {
		dev_err(dev, "DOUR error, flush the FIFO ...\n");
		goto irq_clean_fifo;
	}

	if (intr & RTP_INTR_FIFO_FLG) {
		/* When FIFO is overflow, the FIFO data is valid, so read it */
		if (mode == RTP_MODE_MANUAL || !(fcr & RTP_FCR_OF_FLAG)
		    || !(intr & RTP_INTR_RISE_DET_FLG)) {
			dev_err(dev, "FIFO error, flush the FIFO ...\n");
			goto irq_clean_fifo;
		}
	}

	if (intr & RTP_INTR_RISE_DET_FLG) {
		input_report_key(rtp->idev, BTN_TOUCH, 0);
		input_sync(rtp->idev);
	}

	if (mode == RTP_MODE_MANUAL) {
		rtp->intr = intr;
		rtp->fcr = fcr;
		if (!queue_work(rtp->workq, &rtp->event_work))
			dev_dbg(dev, "Failed to queue workq!");
		goto irq_done;
	}

	/* For auto mode: */
	if (!rtp->smp_period) {
		rtp->intr = intr;
		rtp->fcr = fcr;
		if (!queue_work(rtp->workq, &rtp->event_work)) {
			dev_dbg(dev, "Failed to queue workq!");
			goto irq_clean_fifo;
		}
	} else {
		if (intr & RTP_INTR_DRDY_FLG)
			aic_rtp_read_fifo(rtp, (fcr & RTP_FCR_DAT_CNT_MASK)
					  >> RTP_FCR_DAT_CNT_SHIFT);
	}

	goto irq_done;

irq_clean_fifo:
	rtp_fifo_flush(rtp);
	if (mode == RTP_MODE_MANUAL) {
		rtp->mms = RTP_MMS_IDLE;
		rtp_manual_mode(rtp, 0);
	} else if (!rtp->smp_period) {
		rtp_smp_period(rtp->regs, rtp->smp_period);
	}

irq_done:
	spin_unlock_irqrestore(&user_lock, flags);
	return IRQ_HANDLED;
}

static int aic_rtp_open(struct input_dev *dev)
{
	struct aic_rtp_dev *rtp = input_get_drvdata(dev);

	rtp->workq = alloc_ordered_workqueue(AIC_RTP_NAME, 0);
	if (rtp->workq == NULL) {
		dev_err(&rtp->pdev->dev, "Failed to create workqueue\n");
		return -EINVAL;
	}
	if (rtp->mode == RTP_MODE_MANUAL)
		INIT_WORK(&rtp->event_work, aic_rtp_manual_worker);
	else
		INIT_WORK(&rtp->event_work, aic_rtp_single_smp_worker);

	rtp_enable(rtp, 1);
	rtp_int_enable(rtp, 1);
	if (rtp->mode == RTP_MODE_MANUAL) {
		rtp->mms = RTP_MMS_IDLE;
		rtp_manual_mode(rtp, 0);
	} else {
		rtp_auto_mode(rtp);
	}

	return 0;
}

static void aic_rtp_close(struct input_dev *dev)
{
	struct aic_rtp_dev *rtp = input_get_drvdata(dev);

	cancel_work_sync(&rtp->event_work);
	rtp_int_enable(rtp, 0);
	rtp_enable(rtp, 0);
	destroy_workqueue(rtp->workq);
}

static u32 rtp_rtp_parse_plate(struct device *dev, char *name, u32 def)
{
	u32 ret = 0, val = 0;

	ret = of_property_read_u32(dev->of_node, name, &val);
	if (ret) {
		dev_dbg(dev, "%s doesn't exist", name);
		return 0;
	}

	if (val == 0) {
		dev_warn(dev, "Invalid %s: %d", name, val);
		return def;
	}

	return val;
}

static int aic_rtp_parse_dt(struct device *dev)
{
	u32 ret = 0, val = 0;
	struct device_node *np = dev->of_node;
	struct aic_rtp_dev *rtp = dev_get_drvdata(dev);

	ret = of_property_read_u32(np, "aic,max-pressure", &val);
	if (ret || val == 0 || val > AIC_RTP_MAX_VAL)
		rtp->max_press = AIC_RTP_MAX_VAL;
	else
		rtp->max_press = val;

	if (!of_property_read_u32(np, "aic,fuzz", &val))
		rtp->fuzz = val;

	rtp->two_points = of_property_read_bool(np, "aic,two-points");

	rtp->y_flip = of_property_read_bool(np, "aic,rtp-y-flip");
	if (rtp->y_flip)
		dev_info(dev, "RTP Y-COORDINATE flip mode\n");

	rtp->x_flip = of_property_read_bool(np, "aic,rtp-x-flip");
	if (rtp->x_flip)
		dev_info(dev, "RTP X-COORDINATE flip mode\n");

	rtp->x_plate = rtp_rtp_parse_plate(dev, "aic,x-plate",
					   AIC_RTP_DEFAULT_X_PLATE);
	rtp->y_plate = rtp_rtp_parse_plate(dev, "aic,y-plate",
					   AIC_RTP_DEFAULT_Y_PLATE);
	if (rtp->x_plate || rtp->y_plate)
		rtp->pressure_det = true;

	if (rtp->pressure_det) {
		if (rtp->two_points)
#if defined(CONFIG_ARTINCHIP_ADCIM_DM)
			rtp->mode = RTP_MODE_AUTO4;
#else
			rtp->mode = RTP_MODE_AUTO3;
#endif
		else
			rtp->mode = RTP_MODE_AUTO2;
	} else {
		rtp->mode = RTP_MODE_AUTO1;
	}

	if (of_property_read_bool(np, "aic,manual-mode")) {
		rtp->mode = RTP_MODE_MANUAL;
		dev_info(dev, "RTP is manual mode\n");
		return 0;
	}

	ret = of_property_read_u32(np, "aic,sample-period-ms", &val);
	if (ret || val == 0)
		rtp->smp_period = 0;
	else
		rtp->smp_period = rtp_ms2itv(rtp, val);

	ret = of_property_read_u32(np, "aic,pdeb", &val);
	if (ret || val == 0 || val > AIC_RTP_MAX_PDEB_VAL)
		rtp->pdeb = AIC_RTP_MAX_PDEB_VAL;
	else
		rtp->pdeb = val;

	ret = of_property_read_u32(np, "aic,delay", &val);
	if (ret || val == 0 || val > AIC_RTP_MAX_DELAY_VAL)
		rtp->delay = AIC_RTP_DEFALUT_DELAY_VAL;
	else
		rtp->delay = val;

	dev_dbg(dev, "RTP mode: %d\n", rtp->mode);
	return 0;
}

static int aic_rtp_probe(struct platform_device *pdev)
{
	struct aic_rtp_dev *rtp;
	struct device *dev = &pdev->dev;
	struct clk *clk;
	struct input_dev *idev;
	int ret;

	rtp = devm_kzalloc(dev, sizeof(struct aic_rtp_dev), GFP_KERNEL);
	if (!rtp)
		return -ENOMEM;

	rtp->dev = dev;
	rtp->ignore_fifo_data = true;

	rtp->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(rtp->regs))
		return PTR_ERR(rtp->regs);

	clk = devm_clk_get(&pdev->dev, "pclk");
	if (IS_ERR(clk)) {
		dev_err(dev, "Failed to get pclk\n");
		return PTR_ERR(clk);
	}
	rtp->pclk_rate = clk_get_rate(clk);
	dev_dbg(&pdev->dev, "PCLK rate %d\n", rtp->pclk_rate);

	rtp->clk = devm_clk_get(dev, "rtp");
	if (IS_ERR(rtp->clk)) {
		dev_err(dev, "no clock defined\n");
		return PTR_ERR(rtp->clk);
	}

	ret = clk_prepare_enable(rtp->clk);
	if (ret) {
		dev_err(dev, "Failed to enable clk, return %d\n", ret);
		return ret;
	}

	rtp->rst = devm_reset_control_get_optional_shared(dev, NULL);
	if (IS_ERR(rtp->rst)) {
		ret = PTR_ERR(rtp->rst);
		goto disable_clk;
	}
	reset_control_deassert(rtp->rst);

	ret = platform_get_irq(pdev, 0);
	if (ret < 0) {
		dev_err(dev, "Failed to get irq\n");
		goto disable_rst;
	}
	rtp->irq = ret;

	ret = devm_request_irq(dev, rtp->irq, aic_rtp_irq,
			       0, AIC_RTP_NAME, rtp);
	if (ret) {
		dev_err(dev, "Failed to request IRQ %d\n", rtp->irq);
		goto disable_rst;
	}

	rtp->pdev = pdev;
	platform_set_drvdata(pdev, rtp);
	ret = aic_rtp_parse_dt(dev);
	if (ret)
		goto free_irq;

	idev = devm_input_allocate_device(dev);
	if (!idev)
		return -ENOMEM;

	rtp->idev = idev;
	idev->name = pdev->name;
	idev->phys = AIC_RTP_NAME "/input0";
	idev->open = aic_rtp_open;
	idev->close = aic_rtp_close;
	idev->id.bustype = BUS_HOST;
	idev->evbit[0] =  BIT(EV_SYN) | BIT(EV_KEY) | BIT(EV_ABS);
	input_set_capability(idev, EV_KEY, BTN_TOUCH);
	input_set_abs_params(idev, ABS_X, 0, AIC_RTP_MAX_VAL, rtp->fuzz, 0);
	input_set_abs_params(idev, ABS_Y, 0, AIC_RTP_MAX_VAL, rtp->fuzz, 0);
	if (rtp->pressure_det)
		input_set_abs_params(idev, ABS_PRESSURE, 0, AIC_RTP_MAX_VAL,
				     rtp->fuzz, 0);

	input_set_drvdata(idev, rtp);

	ret = input_register_device(idev);
	if (ret) {
		dev_err(dev, "Failed to register input dev\n");
		goto free_irq;
	}

	rtp->attrs.attrs = aic_rtp_attr;
	ret = sysfs_create_group(&pdev->dev.kobj, &rtp->attrs);
	if (ret)
		goto free_irq;

	dev_info(&pdev->dev, "Artinchip RTP Loaded.\n");
	return 0;

free_irq:
	free_irq(rtp->irq, rtp);
disable_rst:
	reset_control_assert(rtp->rst);
disable_clk:
	clk_disable_unprepare(rtp->clk);

	return ret;
}

static int aic_rtp_remove(struct platform_device *pdev)
{
	struct aic_rtp_dev *rtp = platform_get_drvdata(pdev);

	if (rtp->idev)
		input_unregister_device(rtp->idev);

	rtp_int_enable(rtp, 0);
	rtp_enable(rtp, 0);

	free_irq(rtp->irq, rtp);
	reset_control_assert(rtp->rst);
	clk_disable_unprepare(rtp->clk);

	return 0;
}

static const struct of_device_id aic_rtp_of_match[] = {
	{ .compatible = "artinchip,aic-rtp-v0.1", },
	{ .compatible = "artinchip,aic-rtp-v1.0", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, aic_rtp_of_match);

static struct platform_driver aic_rtp_driver = {
	.driver = {
		.name	= AIC_RTP_NAME,
		.of_match_table = of_match_ptr(aic_rtp_of_match),
	},
	.probe	= aic_rtp_probe,
	.remove	= aic_rtp_remove,
};
module_platform_driver(aic_rtp_driver);

MODULE_AUTHOR("Matteo <duanmt@artinchip.com>");
MODULE_DESCRIPTION("Resistive Touch Panel driver of Artinchip SoC");
MODULE_LICENSE("GPL");
