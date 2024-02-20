// SPDX-License-Identifier: GPL-2.0-only
/*
 * PWM driver of ArtInChip SoC
 *
 * Copyright (C) 2020-2022 ArtInChip Technology Co., Ltd.
 * Authors:  Matteo <duanmt@artinchip.com>
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
#include <linux/pm.h>

#define AIC_PWM_NAME		"aic-pwm"
#define AIC_PWM_CH_NUM		4

/* Register definition of PWM Controller */

#define PWM_PWMx	0x300

#define PWM_CTL		0x000
#define PWM_MCTL	0x004
#define PWM_CKCTL	0x008
#define PWM_INTCTL	0x00C
#define PWM_INTSTS	0x010
#define PWM_TBCTL(n)	(PWM_PWMx + (((n) & 0x7) << 8) + 0x000)
#define PWM_TBSTS(n)	(PWM_PWMx + (((n) & 0x7) << 8) + 0x004)
#define PWM_TBPHS(n)	(PWM_PWMx + (((n) & 0x7) << 8) + 0x008)
#define PWM_TBCTR(n)	(PWM_PWMx + (((n) & 0x7) << 8) + 0x010)
#define PWM_TBPRD(n)	(PWM_PWMx + (((n) & 0x7) << 8) + 0x014)
#define PWM_CMPCTL(n)	(PWM_PWMx + (((n) & 0x7) << 8) + 0x018)
#define PWM_CMPAHR(n)	(PWM_PWMx + (((n) & 0x7) << 8) + 0x01C)
#define PWM_CMPA(n)	(PWM_PWMx + (((n) & 0x7) << 8) + 0x020)
#define PWM_CMPB(n)	(PWM_PWMx + (((n) & 0x7) << 8) + 0x024)
#define PWM_AQCTLA(n)	(PWM_PWMx + (((n) & 0x7) << 8) + 0x028)
#define PWM_AQCTLB(n)	(PWM_PWMx + (((n) & 0x7) << 8) + 0x02C)
#define PWM_AQSFRC(n)	(PWM_PWMx + (((n) & 0x7) << 8) + 0x030)
#define PWM_AQCSFRC(n)	(PWM_PWMx + (((n) & 0x7) << 8) + 0x034)
#define PWM_DBCTL(n)	(PWM_PWMx + (((n) & 0x7) << 8) + 0x038)
#define PWM_DBRED(n)	(PWM_PWMx + (((n) & 0x7) << 8) + 0x03C)
#define PWM_DBFED(n)	(PWM_PWMx + (((n) & 0x7) << 8) + 0x040)
#define PWM_ETSEL(n)	(PWM_PWMx + (((n) & 0x7) << 8) + 0x044)
#define PWM_ETPS(n)	(PWM_PWMx + (((n) & 0x7) << 8) + 0x048)
#define PWM_ETFLG(n)	(PWM_PWMx + (((n) & 0x7) << 8) + 0x04C)
#define PWM_ETCLR(n)	(PWM_PWMx + (((n) & 0x7) << 8) + 0x050)
#define PWM_ETFRC(n)	(PWM_PWMx + (((n) & 0x7) << 8) + 0x054)
#define PWM_VERSION	0xFFC

enum aic_pwm_mode {
	PWM_MODE_UP_COUNT = 0,
	PWM_MODE_DOWN_COUNT,
	PWM_MODE_UP_DOWN_COUNT,
	PWM_MODE_NUM
};

enum aic_pwm_action_type {
	PWM_ACT_NONE = 0,
	PWM_ACT_LOW,
	PWM_ACT_HIGH,
	PWM_ACT_INVERSE,
	PWM_ACT_NUM
};

enum aic_pwm_cmp_write_type {
	PWM_SET_CMPA = 0,
	PWM_SET_CMPB,
	PWM_SET_CMPA_CMPB
};

enum aic_pwm_int_event {
	PWM_CMPA_UP = 0,
	PWM_CMPA_DOWN,
	PWM_CMPB_UP,
	PWM_CMPB_DOWN
};

#define PWM_DEFAULT_TB_CLK_RATE	24000000
#define PWM_DEFAULT_DB_RED	20
#define PWM_DEFAULT_DB_FED	20

#define PWM_ACTION_CFG_NUM	6

#define PWM_CTL_EN			BIT(0)
#define PWM_MCTL_PWM0_EN		BIT(0)
#define PWM_MCTL_PWM_EN(n)		(PWM_MCTL_PWM0_EN << (n))
#define PWM_CKCTL_PWM0_ON		BIT(0)
#define PWM_CKCTL_PWM_ON(n)		(PWM_CKCTL_PWM0_ON << (n))
#define PWM_INTCTL_PWM0_ON		BIT(0)
#define PWM_INTCTL_PWM_ON(n)		(PWM_INTCTL_PWM0_ON << (n))
#define PWM_TBCTL_CLKDIV_MAX		0xFFF
#define PWM_TBCTL_CLKDIV_SHIFT		16
#define PWM_TBCTL_CTR_MODE_MASK		GENMASK(1, 0)
#define PWM_TBPRD_MAX			0xFFFF
#define PWM_AQCTL_DEF_LEVEL		BIT(16)
#define PWM_AQCTL_CBD_SHIFT		10
#define PWM_AQCTL_CBU_SHIFT		8
#define PWM_AQCTL_CAD_SHIFT		6
#define PWM_AQCTL_CAU_SHIFT		4
#define PWM_AQCTL_PRD_SHIFT		2
#define PWM_AQCTL_MASK			0x3
#define PWM_ETSEL_INTEN			BIT(3)
#define PWM_ETSEL_INTSEL_SHIFT		0
#define PWM_SHADOW_SEL_ZRQ_PRD		0xa

struct aic_pwm_action {
	enum aic_pwm_action_type CBD;
	enum aic_pwm_action_type CBU;
	enum aic_pwm_action_type CAD;
	enum aic_pwm_action_type CAU;
	enum aic_pwm_action_type PRD;
	enum aic_pwm_action_type ZRO;
};

struct aic_pwm_arg {
	bool available;
	enum aic_pwm_mode mode;
	u32 tb_clk_rate;
	u32 freq;
	u32 db_red; /* Rising edge delay count of Dead-band */
	u32 db_fed; /* Failing edge delay count of Dead-band */
	struct aic_pwm_action action0;
	struct aic_pwm_action action1;
	u32 period;
	bool def_level;
	enum pwm_polarity polarity;
};

struct aic_pwm_chip {
	struct pwm_chip chip;
	struct attribute_group attrs;
	struct aic_pwm_arg args[AIC_PWM_CH_NUM];
	unsigned long pll_rate;
	unsigned long clk_rate;
	void __iomem *regs;
	struct clk *clk;
	struct reset_control *rst;
	u32 irq;
};

struct aic_pwm_pulse_para {
	u32 prd_ns;
	u32 duty_ns;
	u32 irq_mode;
	u32 pulse_cnt;
};
struct aic_pwm_pulse_para g_pulse_para[AIC_PWM_CH_NUM] = {0};

static void aic_pwm_ch_info(char *buf, u32 ch, u32 en, struct aic_pwm_arg *arg)
{
	const static char *mode[] = {"Up", "Down", "UpDw"};
	const static char *act[] = {"-", "Low", "Hgh", "Inv"};

	sprintf(buf, "%2d %2d %4s %11d %3d %3s %3s %3s %3s %3s %3s\n"
		"%30s %3s %3s %3s %3s %3s\n",
		ch, en & PWM_MCTL_PWM_EN(ch) ? 1 : 0,
		mode[arg->mode], arg->tb_clk_rate, arg->def_level,
		act[arg->action0.CBD], act[arg->action0.CBU],
		act[arg->action0.CAD], act[arg->action0.CAU],
		act[arg->action0.PRD], act[arg->action0.ZRO],
		act[arg->action1.CBD], act[arg->action1.CBU],
		act[arg->action1.CAD], act[arg->action1.CAU],
		act[arg->action1.PRD], act[arg->action1.ZRO]);
}

static ssize_t status_show(struct device *dev,
			   struct device_attribute *devattr, char *buf)
{
	struct aic_pwm_chip *apwm = dev_get_drvdata(dev);
	struct aic_pwm_arg *arg = apwm->args;
	void __iomem *regs = apwm->regs;
	int ver = readl(regs + PWM_VERSION);
	int enable = readl(regs + PWM_MCTL);
	char info[AIC_PWM_CH_NUM][128] = {{0}};
	u32 i;

	for (i = 0; i < AIC_PWM_CH_NUM; i++)
		aic_pwm_ch_info(info[i], i, enable, &arg[i]);

	return sprintf(buf, "In PWM V%d.%02d:\n"
		       "Module Enable: %d, IRQ Enable: %#x\n"
		       "Ch En Mode Tb-clk-rate Def CBD CBU CAD CAU PRD ZRO\n"
		       "%s%s%s%s",
		       ver >> 8, ver & 0xFF,
		       readl(regs + PWM_CTL), readl(regs + PWM_INTCTL),
		       info[0], info[1], info[2], info[3]);
}
static DEVICE_ATTR_RO(status);

#define AIC_PWM_VALID(dev, dat, min, max, name) \
	do { \
		if (((u32)dat < min) || ((u32)dat > max)) { \
			dev_err(dev, "Invalid %s: %d", name, dat); \
			return -EINVAL; \
		} \
	} while (0)

static int aic_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm);
static void aic_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm);
static void pwm_reg_enable(void __iomem *base, int offset, int bit, int enable);

static ssize_t config_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t len)
{
	struct aic_pwm_chip *apwm = dev_get_drvdata(dev);
	struct aic_pwm_arg arg = {0}, *prev;
	struct aic_pwm_action action = {0};
	u32 ch = 0, act_num = 0, ret = 0;

	if (len < 20) {
		dev_err(dev, "The input string maybe too short: %s", buf);
		return -EINVAL;
	}
	dev_dbg(dev, "Input argument: %s", buf);

	ret = sscanf(buf, "%u %u %u %u %u %u %u %u %u %u %u\n",
		     &ch, &act_num, (u32 *)&arg.mode,
		     &arg.tb_clk_rate, (u32 *)&arg.def_level,
		     (u32 *)&action.CBD, (u32 *)&action.CBU,
		     (u32 *)&action.CAD, (u32 *)&action.CAU,
		     (u32 *)&action.PRD, (u32 *)&action.ZRO);
	if (ret < 11) {
		dev_err(dev, "There are not enough valid argument");
		return -EINVAL;
	}

	AIC_PWM_VALID(dev, ch, 0, AIC_PWM_CH_NUM, "ch");
	AIC_PWM_VALID(dev, act_num, 0, 1, "action num");
	AIC_PWM_VALID(dev, arg.tb_clk_rate, 0, 24000000, "tb_clk_rate");
	AIC_PWM_VALID(dev, arg.mode, 0, PWM_MODE_UP_DOWN_COUNT, "mode");
	AIC_PWM_VALID(dev, arg.def_level, 0, 1, "default level");
	AIC_PWM_VALID(dev, action.CBD, PWM_ACT_NONE, PWM_ACT_INVERSE, "CBD");
	AIC_PWM_VALID(dev, action.CBU, PWM_ACT_NONE, PWM_ACT_INVERSE, "CBU");
	AIC_PWM_VALID(dev, action.CAD, PWM_ACT_NONE, PWM_ACT_INVERSE, "CAD");
	AIC_PWM_VALID(dev, action.CAU, PWM_ACT_NONE, PWM_ACT_INVERSE, "CAU");
	AIC_PWM_VALID(dev, action.PRD, PWM_ACT_NONE, PWM_ACT_INVERSE, "PRD");
	AIC_PWM_VALID(dev, action.ZRO, PWM_ACT_NONE, PWM_ACT_INVERSE, "ZRO");

	dev_info(dev, "Disable ch%d first", ch);
	aic_pwm_disable(&apwm->chip, &apwm->chip.pwms[ch]);

	prev = &apwm->args[ch];
	prev->tb_clk_rate = arg.tb_clk_rate;
	prev->mode = arg.mode;
	prev->def_level = arg.def_level;
	if (act_num)
		memcpy(&prev->action1, &action, sizeof(struct aic_pwm_action));
	else
		memcpy(&prev->action0, &action, sizeof(struct aic_pwm_action));

	dev_info(dev, "Enable ch%d", ch);
	aic_pwm_enable(&apwm->chip, &apwm->chip.pwms[ch]);

	return len;
}
static DEVICE_ATTR_WO(config);

static int aic_pwm_set_prd_duty(struct aic_pwm_chip *apwm, u32 ch,
			u32 prd_ns, u32 duty_ns, enum aic_pwm_cmp_write_type type)
{
	u32 prd;
	u64 duty;
	struct aic_pwm_arg *arg = &apwm->args[ch];

	if (!arg->available) {
		dev_err(apwm->chip.dev, "%s() ch%d is unavailable!\n", __func__, ch);
		return -ENODEV;
	}
	dev_dbg(apwm->chip.dev, "ch%d config: duty %d period %d\n",
		ch, duty_ns, prd_ns);
	if ((prd_ns < 1) || (prd_ns > NSEC_PER_SEC)) {
		dev_err(apwm->chip.dev, "ch%d invalid period %d\n", ch, prd_ns);
		return -ERANGE;
	}

	arg->freq = NSEC_PER_SEC / prd_ns;
	prd = arg->tb_clk_rate / arg->freq;
	if (arg->mode == PWM_MODE_UP_DOWN_COUNT)
		prd >>= 1;
	else
		prd--;

	if (prd > PWM_TBPRD_MAX) {
		dev_err(apwm->chip.dev, "ch%d period %d is too big\n", ch, prd);
		return -ERANGE;
	}
	arg->period = prd;
	writel(prd, apwm->regs + PWM_TBPRD(ch));

	duty = (u64)duty_ns*(u64)prd;
	do_div(duty, prd_ns);
	if (duty == prd)
		duty++;

	switch (type) {
	case PWM_SET_CMPA:
		writel((u32)duty, apwm->regs + PWM_CMPA(ch));
		dev_dbg(apwm->chip.dev, "Set CMPA %llu/%u\n", duty, arg->period);
		break;
	case PWM_SET_CMPB:
		writel((u32)duty, apwm->regs + PWM_CMPB(ch));
		dev_dbg(apwm->chip.dev, "Set CMPB %llu/%u\n", duty, arg->period);
		break;
	case PWM_SET_CMPA_CMPB:
		writel((u32)duty, apwm->regs + PWM_CMPA(ch));
		writel((u32)duty, apwm->regs + PWM_CMPB(ch));
	dev_dbg(apwm->chip.dev, "Set CMPA&B %llu/%u\n", duty, arg->period);
		break;
	default:
		break;
	}

	return 0;
}

static ssize_t output0_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t len)
{
	struct aic_pwm_chip *apwm = dev_get_drvdata(dev);
	u32 ch = 0, prd_ns = 0, duty_ns = 0, ret = 0;

	if (len < 5) {
		dev_err(dev, "The input string maybe too short: %s", buf);
		return -EINVAL;
	}
	dev_dbg(dev, "Input argument: %s", buf);

	ret = sscanf(buf, "%u %u %u\n", &ch, &prd_ns, &duty_ns);
	if (ret < 3) {
		dev_err(dev, "There are not enough valid argument");
		return -EINVAL;
	}

	AIC_PWM_VALID(dev, ch, 0, AIC_PWM_CH_NUM, "ch");

	aic_pwm_set_prd_duty(apwm, ch, prd_ns, duty_ns, PWM_SET_CMPA);

	return len;
}
static DEVICE_ATTR_WO(output0);

static ssize_t output1_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t len)
{
	struct aic_pwm_chip *apwm = dev_get_drvdata(dev);
	u32 ch = 0, prd_ns = 0, duty_ns = 0, ret = 0;

	if (len < 5) {
		dev_err(dev, "The input string maybe too short: %s", buf);
		return -EINVAL;
	}
	dev_dbg(dev, "Input argument: %s", buf);

	ret = sscanf(buf, "%u %u %u\n", &ch, &prd_ns, &duty_ns);
	if (ret < 3) {
		dev_err(dev, "There are not enough valid argument");
		return -EINVAL;
	}

	AIC_PWM_VALID(dev, ch, 0, AIC_PWM_CH_NUM, "ch");

	aic_pwm_set_prd_duty(apwm, ch, prd_ns, duty_ns, PWM_SET_CMPB);

	return len;
}
static DEVICE_ATTR_WO(output1);

static ssize_t pulse_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t len)
{
	struct aic_pwm_chip *apwm = dev_get_drvdata(dev);
	u32 ch = 0, irq_mode = 0, prd_ns = 0, duty_ns = 0, pulse_cnt = 0, ret = 0;

	if (len < 5) {
		dev_err(dev, "The input string maybe too short: %s", buf);
		return -EINVAL;
	}
	dev_dbg(dev, "Input argument: %s", buf);

	ret = sscanf(buf, "%u %u %u %u %u\n", &ch, &irq_mode, &prd_ns, &duty_ns, &pulse_cnt);
	if (ret < 5) {
		dev_err(dev, "There are not enough valid argument");
		return -EINVAL;
	}

	AIC_PWM_VALID(dev, ch, 0, AIC_PWM_CH_NUM, "ch");

	if ((irq_mode > PWM_CMPB_DOWN) || (irq_mode < PWM_CMPA_UP)) {
		dev_err(dev, "irq mode error");
		return -EINVAL;
	}

	memset(g_pulse_para, 0, sizeof(g_pulse_para));

	g_pulse_para[ch].pulse_cnt = pulse_cnt;
	g_pulse_para[ch].duty_ns = duty_ns;
	g_pulse_para[ch].prd_ns = prd_ns;
	g_pulse_para[ch].irq_mode = irq_mode;

	aic_pwm_disable(&apwm->chip, &apwm->chip.pwms[ch]);

	if ((irq_mode == PWM_CMPA_UP) || (irq_mode == PWM_CMPA_DOWN))
		aic_pwm_set_prd_duty(apwm, ch, prd_ns, duty_ns, PWM_SET_CMPA);
	if ((irq_mode == PWM_CMPB_UP) || (irq_mode == PWM_CMPB_DOWN))
		aic_pwm_set_prd_duty(apwm, ch, prd_ns, duty_ns, PWM_SET_CMPB);

	writel(PWM_ETSEL_INTEN | ((irq_mode + 4) << PWM_ETSEL_INTSEL_SHIFT), apwm->regs + PWM_ETSEL(ch));
	pwm_reg_enable(apwm->regs, PWM_INTCTL, PWM_INTCTL_PWM_ON(ch), 1);

	aic_pwm_enable(&apwm->chip, &apwm->chip.pwms[ch]);

	return len;
}
static DEVICE_ATTR_WO(pulse);

static struct attribute *aic_pwm_attr[] = {
	&dev_attr_status.attr,
	&dev_attr_config.attr,
	&dev_attr_output0.attr,
	&dev_attr_output1.attr,
	&dev_attr_pulse.attr,
	NULL
};

static void pwm_reg_enable(void __iomem *base, int offset, int bit, int enable)
{
	int tmp;

	tmp = readl(base + offset);
	tmp &= ~bit;
	if (enable)
		tmp |= bit;

	writel(tmp, base + offset);
}

static inline struct aic_pwm_chip *to_aic_pwm_dev(struct pwm_chip *chip)
{
	return container_of(chip, struct aic_pwm_chip, chip);
}

static void aic_pwm_get_state(struct pwm_chip *chip, struct pwm_device *pwm,
			      struct pwm_state *state)
{
	struct aic_pwm_chip *apwm = to_aic_pwm_dev(chip);
	u32 prd, ch = pwm->hwpwm;
	u64 duty;
	struct aic_pwm_arg *arg = &apwm->args[ch];

	if (!arg->available) {
		dev_err(chip->dev, "%s() ch%d is unavailable!\n", __func__, ch);
		return;
	}

	prd = readl(apwm->regs + PWM_TBPRD(ch));
	if (arg->mode == PWM_MODE_UP_DOWN_COUNT)
		prd <<= 1;
	else
		prd++;

	state->period = NSEC_PER_SEC / (arg->tb_clk_rate / prd);
	arg->period = state->period;

	duty = (u64)readl(apwm->regs + PWM_CMPA(ch)) * state->period;
	do_div(duty, prd);
	state->duty_cycle = duty;

	if (readl(apwm->regs + PWM_MCTL) & PWM_MCTL_PWM_EN(ch))
		state->enabled = true;
	else
		state->enabled = false;

	state->polarity = PWM_POLARITY_NORMAL;
	memcpy(&pwm->state, state, sizeof(struct pwm_state));
	dev_dbg(chip->dev, "ch%d state: enable %d duty %lld period %lld\n",
		ch, state->enabled, state->duty_cycle, state->period);
}

static int aic_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
			     int duty_ns, int period_ns)
{
	struct aic_pwm_chip *apwm = to_aic_pwm_dev(chip);
	u32 ch = pwm->hwpwm;
	u32 ret;

	ret = aic_pwm_set_prd_duty(apwm, ch, period_ns, duty_ns, PWM_SET_CMPA_CMPB);
	if (ret < 0)
		return ret;

	return 0;
}

static int aic_pwm_set_polarity(struct pwm_chip *chip,
				struct pwm_device *pwm,
				enum pwm_polarity polarity)
{
	struct aic_pwm_chip *apwm = to_aic_pwm_dev(chip);
	u32 ch = pwm->hwpwm;
	struct aic_pwm_arg *arg = &apwm->args[ch];

	if (!arg->available) {
		dev_err(chip->dev, "%s() ch%d is unavailable!\n", __func__, ch);
		return -ENODEV;
	}
	dev_dbg(chip->dev, "ch%d polarity %d\n", ch, polarity);
	/* Configuration of polarity in hardware delayed, do at enable */
	arg->polarity = polarity;
	return 0;
}

static void pwm_action_set(struct aic_pwm_chip *apwm, u32 ch,
		struct aic_pwm_action *act, char *name)
{
	u32 offset;
	u32 action = apwm->args[ch].def_level ? PWM_AQCTL_DEF_LEVEL : 0;

	if (strcmp(name, "action0") == 0)
		offset = PWM_AQCTLA(ch);
	else
		offset = PWM_AQCTLB(ch);

	action |= (act->CBD << PWM_AQCTL_CBD_SHIFT) |
		  (act->CBU << PWM_AQCTL_CBU_SHIFT) |
		  (act->CAD << PWM_AQCTL_CAD_SHIFT) |
		  (act->CAU << PWM_AQCTL_CAU_SHIFT) |
		  (act->PRD << PWM_AQCTL_PRD_SHIFT) | act->ZRO;
	writel(action, apwm->regs + offset);
}

static int aic_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct aic_pwm_chip *apwm = to_aic_pwm_dev(chip);
	u32 div, ch = pwm->hwpwm;
	struct aic_pwm_arg *arg = &apwm->args[ch];

	if (!arg->available) {
		dev_err(chip->dev, "%s() ch%d is unavailable!\n", __func__, ch);
		return -ENODEV;
	}
	dev_dbg(chip->dev, "ch%d enable\n", ch);
	div = apwm->clk_rate / arg->tb_clk_rate - 1;
	if (div > PWM_TBCTL_CLKDIV_MAX) {
		dev_err(chip->dev, "ch%d clkdiv %d is too big", ch, div);
		return -ERANGE;
	}
	writel((div << PWM_TBCTL_CLKDIV_SHIFT) | arg->mode,
		apwm->regs + PWM_TBCTL(ch));

	pwm_action_set(apwm, ch, &arg->action0, "action0");
	pwm_action_set(apwm, ch, &arg->action1, "action1");

	pwm_reg_enable(apwm->regs, PWM_MCTL, PWM_MCTL_PWM_EN(ch), 1);

	return 0;
}

static void aic_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct aic_pwm_chip *apwm = to_aic_pwm_dev(chip);

	dev_dbg(chip->dev, "ch%d disable\n", pwm->hwpwm);
	pwm_reg_enable(apwm->regs, PWM_MCTL, PWM_MCTL_PWM_EN(pwm->hwpwm), 0);
}

static void aic_pwm_free(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct aic_pwm_chip *apwm = to_aic_pwm_dev(chip);

	dev_dbg(chip->dev, "ch%d free\n", pwm->hwpwm);
	if (pwm_is_enabled(pwm))
		dev_warn(chip->dev, "Removing PWM device without disabling\n");

	apwm->args[pwm->hwpwm].period = 0;
}

static const struct pwm_ops aic_pwm_ops = {
	.free = aic_pwm_free,
	.get_state = aic_pwm_get_state,
	.config = aic_pwm_config,
	.set_polarity = aic_pwm_set_polarity,
	.enable = aic_pwm_enable,
	.disable = aic_pwm_disable,
	.owner = THIS_MODULE,
};

static irqreturn_t aic_pwm_isr(int irq, void *dev_id)
{
	struct aic_pwm_chip *apwm = dev_id;
	static u32 isr_cnt[AIC_PWM_CH_NUM] = {0};
	u32 stat;
	int i;

	stat = readl(apwm->regs + PWM_INTSTS);

	for (i = 0; i < AIC_PWM_CH_NUM; i++) {
		if (stat & (1 << i)) {
			isr_cnt[i]++;
			if (isr_cnt[i] == g_pulse_para[i].pulse_cnt) {
				if ((g_pulse_para[i].irq_mode == PWM_CMPA_UP) || (g_pulse_para[i].irq_mode == PWM_CMPA_DOWN))
					aic_pwm_set_prd_duty(apwm, i, g_pulse_para[i].prd_ns, g_pulse_para[i].prd_ns, PWM_SET_CMPA);
				if ((g_pulse_para[i].irq_mode == PWM_CMPB_UP) || (g_pulse_para[i].irq_mode == PWM_CMPB_DOWN))
					aic_pwm_set_prd_duty(apwm, i, g_pulse_para[i].prd_ns, g_pulse_para[i].prd_ns, PWM_SET_CMPB);
				pwm_reg_enable(apwm->regs, PWM_ETSEL(i), PWM_ETSEL_INTEN, 0);
				pwm_reg_enable(apwm->regs, PWM_INTCTL, PWM_INTCTL_PWM_ON(i), 0);
				dev_info(apwm->chip.dev, "isr cnt:%d,disabled the pwm%d interrupt now.\n", isr_cnt[i], i);
				isr_cnt[i] = 0;
			}
		}
	}

	writel(stat, apwm->regs + PWM_INTSTS);

	return IRQ_HANDLED;
}

static const struct of_device_id aic_pwm_of_match[] = {
	{ .compatible = "artinchip,aic-pwm-v1.0" },
	{},
};
MODULE_DEVICE_TABLE(of, aic_pwm_of_match);

static int aic_pwm_parse_mode(struct device *dev, struct device_node *np)
{
	int ret, i;
	char *mode_str[PWM_MODE_NUM] = {"up-count",
			"down-count", "up-down-count"};
	const char *tmp;

	ret = of_property_read_string(np, "aic,mode", &tmp);
	if (ret) {
		dev_warn(dev, "Can't parse %s.mode\n", of_node_full_name(np));
		return PWM_MODE_UP_COUNT;
	}

	for (i = 0; i < PWM_MODE_NUM; i++)
		if (strcmp(mode_str[i], tmp) == 0)
			return i;

	/* Otherwise, return the default mode */
	return PWM_MODE_UP_COUNT;
}

static void aic_pwm_parse_action(struct device *dev, struct device_node *np,
		char *name, struct aic_pwm_action *act)
{
	int ret, i, j;
	const char *tmp[PWM_ACTION_CFG_NUM];
	char *act_str[PWM_ACT_NUM] = {"none", "low", "high", "inverse"};
	enum aic_pwm_action_type *pa = (enum aic_pwm_action_type *)act;

	memset(act, PWM_ACT_NONE, sizeof(struct aic_pwm_action));
	ret = of_property_read_string_array(np, name, tmp, PWM_ACTION_CFG_NUM);
	if (ret != PWM_ACTION_CFG_NUM) {
		dev_warn(dev, "Can't parse %s.%s\n",
			of_node_full_name(np), name);
		return;
	}

	for (i = 0; i < PWM_ACTION_CFG_NUM; i++)
		for (j = 0; j < PWM_ACT_NUM; j++)
			if (strcmp(act_str[j], tmp[i]) == 0) {
				pa[i] = j;
				break;
			}
}

static int aic_pwm_parse_dt(struct device *dev)
{
	int ret, i = 0;
	struct device_node *child, *np = dev->of_node;
	struct aic_pwm_chip *apwm = dev_get_drvdata(dev);

	ret = of_property_read_u32(np, "clock-rate", (u32 *)&apwm->clk_rate);
	if (ret) {
		dev_warn(dev, "Can't parse clock-rate\n");
		return ret;
	}

	for_each_child_of_node(np, child) {
		struct aic_pwm_arg *arg = &apwm->args[i];

		arg->available = of_device_is_available(child);
		if (!arg->available) {
			dev_dbg(dev, "ch%d is unavailable.\n", i);
			i++;
			continue;
		}
		dev_dbg(dev, "ch%d is available\n", i);
		pwm_reg_enable(apwm->regs, PWM_CKCTL, PWM_CKCTL_PWM_ON(i), 1);
		writel(PWM_SHADOW_SEL_ZRQ_PRD, apwm->regs + PWM_CMPCTL(i));

		ret = of_property_read_u32(child, "aic,tb-clk-rate",
					   (u32 *)&arg->tb_clk_rate);
		if (ret || (arg->tb_clk_rate == 0)) {
			dev_err(dev, "Invalid ch%d tb-clk-rate %d\n",
				i, arg->tb_clk_rate);
			arg->tb_clk_rate = PWM_DEFAULT_TB_CLK_RATE;
		}

		ret = of_property_read_u32(child, "aic,rise-edge-delay",
					   (u32 *)&arg->db_red);
		if (ret) {
			dev_info(dev, "Can't parse %d.rise-edge-delay\n", i);
			arg->db_red = PWM_DEFAULT_DB_RED;
		}

		ret = of_property_read_u32(child, "aic,fall-edge-delay",
					   (u32 *)&arg->db_fed);
		if (ret) {
			dev_info(dev, "Can't parse %d.fall-edge-delay\n", i);
			arg->db_fed = PWM_DEFAULT_DB_FED;
		}

		arg->mode = aic_pwm_parse_mode(dev, child);
		aic_pwm_parse_action(dev, child, "aic,action0", &arg->action0);
		aic_pwm_parse_action(dev, child, "aic,action1", &arg->action1);

		ret = of_property_read_u32(child, "aic,default-level",
					   (u32 *)&arg->def_level);
		if (ret) {
			dev_dbg(dev, "Set default level by PRD/ZRO");
			if (arg->action0.PRD == PWM_ACT_LOW ||
			    arg->action0.ZRO == PWM_ACT_LOW)
				arg->def_level = true;
			if (arg->mode == PWM_MODE_DOWN_COUNT)
				arg->def_level = !arg->def_level;
			if (arg->mode == PWM_MODE_UP_DOWN_COUNT) {
				if (arg->action0.ZRO == PWM_ACT_HIGH)
					arg->def_level = false;
				else
					arg->def_level = true;
			}
		}

		i++;
	}
	return 0;
}

static int aic_pwm_probe(struct platform_device *pdev)
{
	struct aic_pwm_chip *apwm;
	struct clk *clk;
	int ret;
	int irq;

	apwm = devm_kzalloc(&pdev->dev,
			sizeof(struct aic_pwm_chip), GFP_KERNEL);
	if (!apwm)
		return -ENOMEM;

	apwm->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(apwm->regs))
		return PTR_ERR(apwm->regs);

	clk = devm_clk_get(&pdev->dev, "sysclk");
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "Failed to get sysclk clock\n");
		return PTR_ERR(clk);
	}

	apwm->pll_rate = clk_get_rate(clk);
	if (!apwm->pll_rate) {
		dev_err(&pdev->dev, "Failed to get sysclk clock rate\n");
		return -EINVAL;
	}

	apwm->clk = devm_clk_get(&pdev->dev, "pwm");
	if (IS_ERR(apwm->clk)) {
		dev_err(&pdev->dev, "Failed to get apwm clk\n");
		return PTR_ERR(apwm->clk);
	}
	ret = clk_prepare_enable(apwm->clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "clk_prepare_enable() failed: %d\n", ret);
		return ret;
	}

	apwm->rst = devm_reset_control_get_optional_shared(&pdev->dev, NULL);
	if (IS_ERR(apwm->rst)) {
		ret = PTR_ERR(apwm->rst);
		goto out_disable_clk;
	}
	reset_control_deassert(apwm->rst);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "%s() Failed to get IRQ\n", __func__);
		ret = irq;
		goto out_disable_rst;
	}
	ret = devm_request_irq(&pdev->dev, irq, aic_pwm_isr,
			       0, AIC_PWM_NAME, apwm);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to request IRQ %d\n", irq);
		goto free_irq;
	}
	apwm->irq = irq;

	platform_set_drvdata(pdev, apwm);

	apwm->attrs.attrs = aic_pwm_attr;
	ret = sysfs_create_group(&pdev->dev.kobj, &apwm->attrs);
	if (ret)
		goto free_irq;

	apwm->chip.dev = &pdev->dev;
	apwm->chip.ops = &aic_pwm_ops;
	apwm->chip.of_xlate = of_pwm_xlate_with_flags;
	apwm->chip.of_pwm_n_cells = 3;
	apwm->chip.base = -1;
	apwm->chip.npwm = AIC_PWM_CH_NUM;
	ret = pwmchip_add(&apwm->chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "pwmchip_add() failed: %d\n", ret);
		goto free_irq;
	}

	pwm_reg_enable(apwm->regs, PWM_CTL, PWM_CTL_EN, 1);
	ret = aic_pwm_parse_dt(&pdev->dev);
	if (ret)
		goto free_irq;

	ret = clk_set_rate(apwm->clk, apwm->clk_rate);
	if (ret) {
		dev_err(&pdev->dev, "Failed to set clk_rate %ld\n",
			apwm->clk_rate);
		goto free_irq;
	}

	dev_info(&pdev->dev, "ArtInChip PWM Loaded.\n");
	return 0;

free_irq:
	free_irq(apwm->irq, apwm);
out_disable_rst:
	reset_control_assert(apwm->rst);
out_disable_clk:
	clk_disable_unprepare(apwm->clk);
	return ret;
}

static int aic_pwm_remove(struct platform_device *pdev)
{
	struct aic_pwm_chip *apwm = platform_get_drvdata(pdev);

	pwmchip_remove(&apwm->chip);

	free_irq(apwm->irq, apwm);
	reset_control_assert(apwm->rst);
	clk_unprepare(apwm->clk);

	return 0;
}

#ifdef CONFIG_PM

static int aic_pwm_pm_suspend(struct device *dev)
{
	struct aic_pwm_chip *apwm = dev_get_drvdata(dev);

	clk_disable_unprepare(apwm->clk);
	return 0;
}

static int aic_pwm_pm_resume(struct device *dev)
{
	struct aic_pwm_chip *apwm = dev_get_drvdata(dev);

	clk_set_rate(apwm->clk, apwm->clk_rate);
	clk_prepare_enable(apwm->clk);
	return 0;
}

static const struct dev_pm_ops aic_pwm_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(aic_pwm_pm_suspend, aic_pwm_pm_resume)
};

#endif

static struct platform_driver aic_pwm_driver = {
	.driver = {
		.name = AIC_PWM_NAME,
		.of_match_table = aic_pwm_of_match,
#ifdef CONFIG_PM
		.pm = &aic_pwm_pm_ops,
#endif
	},
	.probe = aic_pwm_probe,
	.remove = aic_pwm_remove,
};
module_platform_driver(aic_pwm_driver);

MODULE_AUTHOR("Matteo <duanmt@artinchip.com>");
MODULE_DESCRIPTION("PWM driver of ArtInChip SoC");
MODULE_LICENSE("GPL");
