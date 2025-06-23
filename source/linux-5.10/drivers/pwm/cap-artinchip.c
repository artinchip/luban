// SPDX-License-Identifier: GPL-2.0-only
/*
 * cap driver of ArtInChip SoC
 *
 * Copyright (C) 2020-2023 ArtInChip Technology Co., Ltd.
 * Authors:  zrq <ruiqi.zheng@artinchip.com>
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/pwm.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/pm.h>

#define AIC_CAP_NAME			"aic-cap"
#define AIC_CAP_CH_NUM			3

/* GLB register */
#define GLB_CLK_CTL			0x00
#define GLB_CLK_CTL_CAP_EN(n)		(BIT(16) << (n))

/* CAP register */
#define CAP_CNT_V(n)			((((n) & 0x7) << 8) + 0x00)
#define CAP_CNT_PHV(n)			((((n) & 0x7) << 8) + 0x04)
#define CAP_CNT_PRDV(n)			((((n) & 0x7) << 8) + 0x08)
#define CAP_CNT_CMPV(n)			((((n) & 0x7) << 8) + 0x0c)
#define CAP_CNT_PRDV_SH(n)		((((n) & 0x7) << 8) + 0x10)
#define CAP_CNT_CMPV_SH(n)		((((n) & 0x7) << 8) + 0x14)
#define CAP_CONF1(n)			((((n) & 0x7) << 8) + 0x18)
#define CAP_CONF2(n)			((((n) & 0x7) << 8) + 0x1c)
#define CAP_INT_EN(n)			((((n) & 0x7) << 8) + 0x20)
#define CAP_FLG(n)			((((n) & 0x7) << 8) + 0x24)
#define CAP_FLG_CLR(n)			((((n) & 0x7) << 8) + 0x28)
#define CAP_SW_FRC(n)			((((n) & 0x7) << 8) + 0x2c)
#define CAP_IN_FLT(n)			((((n) & 0x7) << 8) + 0x30)
#define CAP_IN_SRC(n)			((((n) & 0x7) << 8) + 0x34)
#define CAP_VER(n)			((((n) & 0x7) << 8) + 0xfc)

/* CAP_CONF1 */
#define CAP_EVNT0_POL_MASK		GENMASK(0, 0)
#define CAP_EVNT0_POL_SHIFT		0
#define CAP_EVNT0_RST_MASK		GENMASK(1, 1)
#define CAP_EVNT0_RST_SHIFT		1
#define CAP_EVNT1_POL_MASK		GENMASK(2, 2)
#define CAP_EVNT1_POL_SHIFT		2
#define CAP_EVNT2_POL_MASK		GENMASK(4, 4)
#define CAP_EVNT2_POL_SHIFT		4
#define CAP_EVNT3_POL_MASK		GENMASK(6, 6)
#define CAP_EVNT3_POL_SHIFT		6
#define CAP_REG_LD_EN_MASK		GENMASK(8, 8)
#define CAP_REG_LD_EN_SHIFT		8

/* CAP_INT_EN */
#define CAP_EVNT3_INT_EN_MASK		GENMASK(4, 4)
#define CAP_EVNT3_INT_EN_SHIFT		4

/* CAP_CONF2 */
#define CAP_CNT_EN_MASK			GENMASK(4, 4)
#define CAP_CNT_EN_SHIFT		4

#define CAP_EVENT3_FLG			BIT(4)

struct aic_cap_data {
	bool available;
	u32 val[2];
	u32 flag;
	struct mutex lock;
	wait_queue_head_t wait;
};

struct aic_cap_chip {
	struct pwm_chip chip;
	struct attribute_group attrs;
	struct aic_cap_data data[AIC_CAP_CH_NUM];
	unsigned long pll_rate;
	unsigned long clk_rate;
	void __iomem *regs;
	void __iomem *glb_regs;
	struct clk *clk;
	struct reset_control *rst;
	u32 irq;
};

static void cap_reg_enable(void __iomem *base, int offset, int bit, int enable)
{
	int tmp;

	tmp = readl(base + offset);
	tmp &= ~bit;
	if (enable)
		tmp |= bit;

	writel(tmp, base + offset);
}

static inline void cap_reg_config(void __iomem *base, u32 offset, u32 mask, u32 shift, u32 val)
{
    u32 cur;
    cur = readl((base + offset));
    setbits(val, mask, shift, cur);
    writel(cur, (base + offset));
}

static inline struct aic_cap_chip *to_aic_cap_dev(struct pwm_chip *chip)
{
	return container_of(chip, struct aic_cap_chip, chip);
}

static int aic_cap_init(struct aic_cap_chip *cap, u32 ch)
{
	/* action configuration */
	cap_reg_config(cap->regs, CAP_CONF1(ch), CAP_REG_LD_EN_MASK, CAP_REG_LD_EN_SHIFT, 1);
	cap_reg_config(cap->regs, CAP_CONF1(ch), CAP_EVNT3_POL_MASK, CAP_EVNT3_POL_SHIFT, 1);
	cap_reg_config(cap->regs, CAP_CONF1(ch), CAP_EVNT2_POL_MASK, CAP_EVNT2_POL_SHIFT, 0);
	cap_reg_config(cap->regs, CAP_CONF1(ch), CAP_EVNT1_POL_MASK, CAP_EVNT1_POL_SHIFT, 1);
	cap_reg_config(cap->regs, CAP_CONF1(ch), CAP_EVNT0_POL_MASK, CAP_EVNT0_POL_SHIFT, 0);
	cap_reg_config(cap->regs, CAP_CONF1(ch), CAP_EVNT0_RST_MASK, CAP_EVNT0_RST_SHIFT, 1);

	/* interrupt configuration */
	cap_reg_config(cap->regs, CAP_INT_EN(ch), CAP_EVNT3_INT_EN_MASK, CAP_EVNT3_INT_EN_SHIFT, 1);

	return 0;
}

static int aic_cap_apply(struct pwm_chip *chip, struct pwm_device *pwm,
			     const struct pwm_state *state)
{
	return 0;
}

static int aic_pwm_capture(struct pwm_chip *chip, struct pwm_device *pwm,
			   struct pwm_capture *result, unsigned long timeout)
{
	struct aic_cap_chip *cap = to_aic_cap_dev(chip);
	struct device *dev = cap->chip.dev;
	int ret;
	u32 ch = pwm->hwpwm;
	u64 period_temp = 0, duty_clcyle_temp = 0;

	if (!cap->data[ch].available) {
		dev_err(chip->dev, "%s() ch%d is unavailable!\n", __func__, ch);
		return -ENODEV;
	}

	mutex_lock(&cap->data[ch].lock);
	cap->data[ch].flag = 0;

	/* Enable capture */
	cap_reg_config(cap->regs, CAP_CONF2(ch), CAP_CNT_EN_MASK, CAP_CNT_EN_SHIFT, 1);

	ret = wait_event_interruptible_timeout(cap->data[ch].wait, cap->data[ch].flag != 0,
					       msecs_to_jiffies(timeout));

	if (ret == 0) {
		ret = -ETIMEDOUT;
		dev_err(dev, "wait interrupt time out\n");
		goto out;
	}

	period_temp = (u64)NSEC_PER_SEC  * (u64)cap->data[ch].val[1];
	duty_clcyle_temp = (u64)NSEC_PER_SEC  * (u64)cap->data[ch].val[0];

	result->period = period_temp / cap->clk_rate;
	result->duty_cycle = duty_clcyle_temp / cap->clk_rate;

	dev_dbg(dev, "period:%d duty_cycle:%d\n", result->period, result->duty_cycle);
	dev_dbg(dev, "cap->data[ch].val[1]:%d, cap->data[ch].val[0]:%d\n",
			cap->data[ch].val[1], cap->data[ch].val[0]);

	mutex_unlock(&cap->data[ch].lock);

	return 0;
out:
	/* Disable capture */
	cap_reg_config(cap->regs, CAP_CONF2(ch), CAP_CNT_EN_MASK, CAP_CNT_EN_SHIFT, 0);

	mutex_unlock(&cap->data[ch].lock);

	return ret;
}


static const struct pwm_ops aic_cap_ops = {
	.apply = aic_cap_apply,
	.capture = aic_pwm_capture,
	.owner = THIS_MODULE,
};

static irqreturn_t aic_cap_isr(int irq, void *dev_id)
{
	struct aic_cap_chip *cap = dev_id;
	u32 stat;
	int i;

	for (i = 0; i < AIC_CAP_CH_NUM; i++) {
		stat = readl(cap->regs + CAP_FLG(i));

		if (stat & CAP_EVENT3_FLG) {
			cap->data[i].flag = 1;

			/* Store the data */
			cap->data[i].val[0] = readl((cap->regs + CAP_CNT_CMPV(i)));//reg1
			cap->data[i].val[1] = readl((cap->regs + CAP_CNT_PRDV_SH(i)));//reg2

			/* Disable capture */
			cap_reg_config(cap->regs, CAP_CONF2(i), CAP_CNT_EN_MASK, CAP_CNT_EN_SHIFT, 0);

			/* Clear the flag */
			writel(CAP_EVENT3_FLG, cap->regs + CAP_FLG_CLR(i));

			wake_up(&cap->data[i].wait);
		}
	}

	return IRQ_HANDLED;
}

static const struct of_device_id aic_cap_of_match[] = {
	{ .compatible = "artinchip,aic-cap-v1.0" },
	{},
};
MODULE_DEVICE_TABLE(of, aic_cap_of_match);

static int aic_cap_parse_dt(struct device *dev)
{
	int ret, i = 0;
	struct device_node *child, *np = dev->of_node;
	struct aic_cap_chip *cap = dev_get_drvdata(dev);

	ret = of_property_read_u32(np, "clock-rate", (u32 *)&cap->clk_rate);
	if (ret) {
		dev_warn(dev, "Can't parse clock-rate\n");
		return ret;
	}

	for_each_child_of_node(np, child) {
		struct aic_cap_data *arg = &cap->data[i];

		arg->available = of_device_is_available(child);
		if (!arg->available) {
			dev_dbg(dev, "ch%d is unavailable.\n", i);
			i++;
			continue;
		}
		dev_dbg(dev, "ch%d is available\n", i);

		i++;
	}

	return 0;
}

static int aic_cap_probe(struct platform_device *pdev)
{
	struct aic_cap_chip *cap;
	struct clk *clk;
	int ret, i;
	int irq;

	cap = devm_kzalloc(&pdev->dev,
			sizeof(struct aic_cap_chip), GFP_KERNEL);
	if (!cap)
		return -ENOMEM;

	cap->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(cap->regs))
		return PTR_ERR(cap->regs);

	clk = devm_clk_get(&pdev->dev, "sysclk");
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "Failed to get sysclk clock\n");
		return PTR_ERR(clk);
	}

	cap->pll_rate = clk_get_rate(clk);
	if (!cap->pll_rate) {
		dev_err(&pdev->dev, "Failed to get sysclk clock rate\n");
		return -EINVAL;
	}

	cap->clk = devm_clk_get(&pdev->dev, "pwmcs");
	if (IS_ERR(cap->clk)) {
		dev_err(&pdev->dev, "Failed to get cap clk\n");
		return PTR_ERR(cap->clk);
	}
	ret = clk_prepare_enable(cap->clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "clk_prepare_enable() failed: %d\n", ret);
		return ret;
	}

	cap->rst = devm_reset_control_get_optional_shared(&pdev->dev, NULL);
	if (IS_ERR(cap->rst)) {
		ret = PTR_ERR(cap->rst);
		goto out_disable_clk;
	}
	reset_control_deassert(cap->rst);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "%s() Failed to get IRQ\n", __func__);
		ret = irq;
		goto out_disable_rst;
	}

	ret = devm_request_irq(&pdev->dev, irq, aic_cap_isr,
			       0, AIC_CAP_NAME, cap);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to request IRQ %d\n", irq);
		goto out_disable_rst;
	}
	cap->irq = irq;

	platform_set_drvdata(pdev, cap);

	cap->chip.dev = &pdev->dev;
	cap->chip.ops = &aic_cap_ops;
	cap->chip.of_xlate = of_pwm_xlate_with_flags;
	cap->chip.of_pwm_n_cells = 3;
	cap->chip.base = -1;
	cap->chip.npwm = AIC_CAP_CH_NUM;
	ret = pwmchip_add(&cap->chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "pwmchip_add() failed: %d\n", ret);
		goto out_disable_rst;
	}

	ret = aic_cap_parse_dt(&pdev->dev);
	if (ret)
		goto out_pwmchip_remove;

	ret = clk_set_rate(cap->clk, cap->clk_rate);
	if (ret) {
		dev_err(&pdev->dev, "Failed to set clk_rate %ld\n",
			cap->clk_rate);
		goto out_pwmchip_remove;
	}

	cap->glb_regs = of_iomap(pdev->dev.of_node, 1);
	if (IS_ERR(cap->glb_regs))
		goto out_pwmchip_remove;

	for (i = 0; i < AIC_CAP_CH_NUM; i++) {
		if (cap->data[i].available) {
			/* enable the glb clock */
			cap_reg_enable(cap->glb_regs, GLB_CLK_CTL, GLB_CLK_CTL_CAP_EN(i), 1);

			aic_cap_init(cap, i);

			init_waitqueue_head(&cap->data[i].wait);
			mutex_init(&cap->data[i].lock);
		}
	}

	//unmap to be used by other PWMCS Submodules
	iounmap(cap->glb_regs);

	dev_info(&pdev->dev, "ArtInChip CAP Loaded.\n");
	return 0;

out_pwmchip_remove:
	pwmchip_remove(&cap->chip);
out_disable_rst:
	reset_control_assert(cap->rst);
out_disable_clk:
	clk_disable_unprepare(cap->clk);
	return ret;
}

static int aic_cap_remove(struct platform_device *pdev)
{
	struct aic_cap_chip *cap = platform_get_drvdata(pdev);

	pwmchip_remove(&cap->chip);
	reset_control_assert(cap->rst);
	clk_unprepare(cap->clk);

	return 0;
}

#ifdef CONFIG_PM
static int aic_cap_pm_suspend(struct device *dev)
{
	struct aic_cap_chip *cap = dev_get_drvdata(dev);

	clk_disable_unprepare(cap->clk);
	return 0;
}

static int aic_cap_pm_resume(struct device *dev)
{
	struct aic_cap_chip *cap = dev_get_drvdata(dev);

	clk_set_rate(cap->clk, cap->clk_rate);
	clk_prepare_enable(cap->clk);
	return 0;
}

static const struct dev_pm_ops aic_cap_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(aic_cap_pm_suspend, aic_cap_pm_resume)
};
#endif
static struct platform_driver aic_cap_driver = {
	.driver = {
		.name = AIC_CAP_NAME,
		.of_match_table = aic_cap_of_match,
#ifdef CONFIG_PM
		.pm = &aic_cap_pm_ops,
#endif

	},
	.probe = aic_cap_probe,
	.remove = aic_cap_remove,
};

module_platform_driver(aic_cap_driver);

MODULE_AUTHOR("zrq <ruiqi.zheng@artinchip.com>");
MODULE_DESCRIPTION("cap driver of ArtInChip SoC");
MODULE_LICENSE("GPL");

