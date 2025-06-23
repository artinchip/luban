// SPDX-License-Identifier: GPL-2.0-only
/*
 * EPWM driver of ArtInChip SoC
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

#define AIC_EPWM_NAME		"aic-epwm"
#define AIC_EPWM_CH_NUM		6

/* Register definition of EPWM Controller */
#define GLB_CLK_CTL		0x000
#define GLB_EPWM_INT_STS		0x004
#define GLB_EPWM_EN		0x014

#define EPWM_CNT_PRDV(n)	((((n) & 0x7) << 8) + 0x000)
#define EPWM_CNT_V(n)		((((n) & 0xF) << 8) + 0x008)
#define EPWM_CNT_CONF(n)	((((n) & 0x7) << 8) + 0x00C)
#define EPWM_CNT_AV(n)		((((n) & 0x7) << 8) + 0x014)
#define EPWM_CNT_BV(n)		((((n) & 0x7) << 8) + 0x018)
#define EPWMA_ACT(n)		((((n) & 0x7) << 8) + 0x020)
#define EPWMB_ACT(n)		((((n) & 0x7) << 8) + 0x024)
#define EPWM_FLT_PRTCT(n)	((((n) & 0x7) << 8) + 0x044)
#define EPWM_SW_ACT(n)		((((n) & 0xF) << 8) + 0x028)
#define EPWM_ACT_SW_CT(n)	((((n) & 0xF) << 8) + 0x02C)
#define EPWM_ADC_INT_CTL(n)	((((n) & 0x7) << 8) + 0x058)
#define EPWM_ADC_INT_PRE(n)	((((n) & 0x7) << 8) + 0x05C)
#define EPWM_EVNT_FLAG(n)	((((n) & 0x7) << 8) + 0x060)
#define EPWM_EVENT_CLR(n)	((((n) & 0x7) << 8) + 0x064)
#define EPWM_VERSION(n)		((((n) & 0x7) << 8) + 0x0FC)

enum aic_epwm_mode {
	EPWM_MODE_UP_COUNT = 0,
	EPWM_MODE_DOWN_COUNT,
	EPWM_MODE_UP_DOWN_COUNT,
	EPWM_MODE_STOP_COUNT,
	EPWM_MODE_NUM
};

enum aic_epwm_action_type {
	EPWM_ACT_NONE = 0,
	EPWM_ACT_LOW,
	EPWM_ACT_HIGH,
	EPWM_ACT_INVERSE,
	EPWM_ACT_NUM
};

enum aic_epwm_cmp_write_type {
	EPWM_SET_CMPA = 0,
	EPWM_SET_CMPB,
	EPWM_SET_CMPA_CMPB
};

enum aic_epwm_int_event {
	EPWM_CMPA_UP = 0,
	EPWM_CMPA_DOWN,
	EPWM_CMPB_UP,
	EPWM_CMPB_DOWN
};

#define EPWM_DEFAULT_TB_CLK_RATE	24000000
#define EPWM_ACTION_CFG_NUM		6

#define GLB_EPWM_EN_B			BIT(0)
#define EPWM_S0_CLK_EN			BIT(0)
#define EPMW_SX_CLK_EN(n)		(EPWM_S0_CLK_EN << (n))
#define EPWMA_ACT_CNTDBV_SHIFT		10
#define EPWMA_ACT_CNTUBV_SHIFT		8
#define EPWMA_ACT_CNTDAV_SHIFT		6
#define EPWMA_ACT_CNTUAV_SHIFT		4
#define EPWMA_ACT_CNTPRD_SHIFT		2
#define EPWM_TBPRD_MAX			0xFFFF
#define EPWM_A_INIT			BIT(16)
#define EPWM_B_INIT			BIT(18)
#define EPWM_CLK_DIV1_MAX		0x7
#define EPWM_CLK_DIV2_SHIFT		10
#define EPWM_CLK_DIV1_SHIFT		7
#define EPWM_CNT_MOD_MASK		GENMASK(1, 0)
#define EPWM_CNT_MOD_SHIFT		0
#define EPWM_INT_EN			BIT(3)
#define EPWM_INT_SEL_SHIFT		0
#define EPWM_INT_FLG			BIT(0)
#define EPWM_INT_CLR			BIT(0)
#define EPWM_ACT_SW_CT_UPDT		6
#define EPWM_SWACT_UPDT			3 << EPWM_ACT_SW_CT_UPDT
#define EPWM_ACT_SW_NONE		0x0
#define EPWM_ACT_SW_HIGH		0xA
#define EPWM_ACT_SW_LOW			0x5

struct aic_epwm_action {
	enum aic_epwm_action_type CBD;
	enum aic_epwm_action_type CBU;
	enum aic_epwm_action_type CAD;
	enum aic_epwm_action_type CAU;
	enum aic_epwm_action_type PRD;
	enum aic_epwm_action_type ZRO;
};

struct aic_epwm_arg {
	bool available;
	enum aic_epwm_mode mode;
	u32 tb_clk_rate;
	u32 freq;
	u32 db_red; /* Rising edge delay count of Dead-band */
	u32 db_fed; /* Failing edge delay count of Dead-band */
	struct aic_epwm_action action0;
	struct aic_epwm_action action1;
	u32 period;
	bool def_level;
	enum pwm_polarity polarity;
};

struct aic_epwm_chip {
	struct pwm_chip chip;
	struct attribute_group attrs;
	struct aic_epwm_arg args[AIC_EPWM_CH_NUM];
	unsigned long pll_rate;
	unsigned long clk_rate;
	void __iomem *regs;
	void __iomem *glb_regs;
	struct clk *clk;
	struct reset_control *rst;
	u32 irq;
};

struct aic_epwm_pulse_para {
	u32 prd_ns;
	u32 duty_ns;
	u32 irq_mode;
	u32 pulse_cnt;
};

struct aic_epwm_pulse_para g_epwm_pulse_para[AIC_EPWM_CH_NUM] = {0};

static void aic_epwm_ch_info(char *buf, u32 ch, struct aic_epwm_arg *arg)
{
	const static char *mode[] = {"Up", "Down", "UpDw"};
	const static char *act[] = {"-", "Low", "Hgh", "Inv"};

	sprintf(buf, "%2d %4s %11d %3d %3s %3s %3s %3s %3s %3s\n"
		"%27s %3s %3s %3s %3s %3s\n",
		ch,
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
	struct aic_epwm_chip *epwm = dev_get_drvdata(dev);
	struct aic_epwm_arg *arg = epwm->args;
	char info[AIC_EPWM_CH_NUM][128] = {{0}};
	u32 i;

	for (i = 0; i < AIC_EPWM_CH_NUM; i++)
		aic_epwm_ch_info(info[i], i, &arg[i]);

	return sprintf(buf,
		       "Ch Mode Tb-clk-rate Def CBD CBU CAD CAU PRD ZRO\n"
		       "%s%s%s%s%s%s",
		       info[0], info[1], info[2], info[3], info[4], info[5]);
}
static DEVICE_ATTR_RO(status);

#define AIC_EPWM_VALID(dev, dat, min, max, name) \
	do { \
		if (((u32)dat < min) || ((u32)dat > max)) { \
			dev_err(dev, "Invalid %s: %d", name, dat); \
			return -EINVAL; \
		} \
	} while (0)

static int aic_epwm_enable(struct pwm_chip *chip, struct pwm_device *pwm);
static void aic_epwm_disable(struct pwm_chip *chip, struct pwm_device *pwm);
static void epwm_reg_enable(void __iomem *base, int offset, int bit, int enable);

static ssize_t config_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t len)
{
	struct aic_epwm_chip *epwm = dev_get_drvdata(dev);
	struct aic_epwm_arg arg = {0}, *prev;
	struct aic_epwm_action action = {0};
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

	AIC_EPWM_VALID(dev, ch, 0, AIC_EPWM_CH_NUM, "ch");
	AIC_EPWM_VALID(dev, act_num, 0, 1, "action num");
	AIC_EPWM_VALID(dev, arg.tb_clk_rate, 0, 24000000, "tb_clk_rate");
	AIC_EPWM_VALID(dev, arg.mode, 0, EPWM_MODE_UP_DOWN_COUNT, "mode");
	AIC_EPWM_VALID(dev, arg.def_level, 0, 1, "default level");
	AIC_EPWM_VALID(dev, action.CBD, EPWM_ACT_NONE, EPWM_ACT_INVERSE, "CBD");
	AIC_EPWM_VALID(dev, action.CBU, EPWM_ACT_NONE, EPWM_ACT_INVERSE, "CBU");
	AIC_EPWM_VALID(dev, action.CAD, EPWM_ACT_NONE, EPWM_ACT_INVERSE, "CAD");
	AIC_EPWM_VALID(dev, action.CAU, EPWM_ACT_NONE, EPWM_ACT_INVERSE, "CAU");
	AIC_EPWM_VALID(dev, action.PRD, EPWM_ACT_NONE, EPWM_ACT_INVERSE, "PRD");
	AIC_EPWM_VALID(dev, action.ZRO, EPWM_ACT_NONE, EPWM_ACT_INVERSE, "ZRO");

	dev_info(dev, "Disable ch%d first", ch);
	aic_epwm_disable(&epwm->chip, &epwm->chip.pwms[ch]);

	prev = &epwm->args[ch];
	prev->tb_clk_rate = arg.tb_clk_rate;
	prev->mode = arg.mode;
	prev->def_level = arg.def_level;
	if (act_num)
		memcpy(&prev->action1, &action, sizeof(struct aic_epwm_action));
	else
		memcpy(&prev->action0, &action, sizeof(struct aic_epwm_action));

	dev_info(dev, "Enable ch%d", ch);
	aic_epwm_enable(&epwm->chip, &epwm->chip.pwms[ch]);

	return len;
}
static DEVICE_ATTR_WO(config);

static int aic_epwm_set_prd_duty(struct aic_epwm_chip *epwm, u32 ch,
			u32 prd_ns, u32 duty_ns, enum aic_epwm_cmp_write_type type)
{
	u32 prd;
	u64 duty;
	struct aic_epwm_arg *arg = &epwm->args[ch];

	if (!arg->available) {
		dev_err(epwm->chip.dev, "%s() ch%d is unavailable!\n", __func__, ch);
		return -ENODEV;
	}
	dev_dbg(epwm->chip.dev, "ch%d config: duty %d period %d\n",
		ch, duty_ns, prd_ns);
	if ((prd_ns < 1) || (prd_ns > NSEC_PER_SEC)) {
		dev_err(epwm->chip.dev, "ch%d invalid period %d\n", ch, prd_ns);
		return -ERANGE;
	}

	arg->freq = NSEC_PER_SEC / prd_ns;
	prd = arg->tb_clk_rate / arg->freq;
	if (arg->mode == EPWM_MODE_UP_DOWN_COUNT)
		prd >>= 1;
	else
		prd--;

	if (prd > EPWM_TBPRD_MAX) {
		dev_err(epwm->chip.dev, "ch%d period %d is too big\n", ch, prd);
		return -ERANGE;
	}
	arg->period = prd;
	writel(prd, epwm->regs + EPWM_CNT_PRDV(ch));

	duty = (u64)duty_ns*(u64)prd;
	do_div(duty, prd_ns);
	if (duty == prd)
		duty++;

	switch (type) {
	case EPWM_SET_CMPA:
		writel((u32)duty, epwm->regs + EPWM_CNT_AV(ch));
		dev_dbg(epwm->chip.dev, "Set CMPA %llu/%u\n", duty, arg->period);
		break;
	case EPWM_SET_CMPB:
		writel((u32)duty, epwm->regs + EPWM_CNT_BV(ch));
		dev_dbg(epwm->chip.dev, "Set CMPB %llu/%u\n", duty, arg->period);
		break;
	case EPWM_SET_CMPA_CMPB:
		writel((u32)duty, epwm->regs + EPWM_CNT_AV(ch));
		writel((u32)duty, epwm->regs + EPWM_CNT_BV(ch));
	dev_dbg(epwm->chip.dev, "Set CMPA&B %llu/%u\n", duty, arg->period);
		break;
	default:
		break;
	}

	return 0;
}

static ssize_t output0_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t len)
{
	struct aic_epwm_chip *epwm = dev_get_drvdata(dev);
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

	AIC_EPWM_VALID(dev, ch, 0, AIC_EPWM_CH_NUM, "ch");

	aic_epwm_set_prd_duty(epwm, ch, prd_ns, duty_ns, EPWM_SET_CMPA);

	return len;
}
static DEVICE_ATTR_WO(output0);

static ssize_t output1_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t len)
{
	struct aic_epwm_chip *epwm = dev_get_drvdata(dev);
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

	AIC_EPWM_VALID(dev, ch, 0, AIC_EPWM_CH_NUM, "ch");

	aic_epwm_set_prd_duty(epwm, ch, prd_ns, duty_ns, EPWM_SET_CMPB);

	return len;
}
static DEVICE_ATTR_WO(output1);

static ssize_t pulse_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t len)
{
	struct aic_epwm_chip *epwm = dev_get_drvdata(dev);
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

	AIC_EPWM_VALID(dev, ch, 0, AIC_EPWM_CH_NUM, "ch");

	if ((irq_mode > EPWM_CMPB_DOWN) || (irq_mode < EPWM_CMPA_UP)) {
		dev_err(dev, "irq mode error");
		return -EINVAL;
	}

	memset(g_epwm_pulse_para, 0, sizeof(g_epwm_pulse_para));

	g_epwm_pulse_para[ch].pulse_cnt = pulse_cnt;
	g_epwm_pulse_para[ch].duty_ns = duty_ns;
	g_epwm_pulse_para[ch].prd_ns = prd_ns;
	g_epwm_pulse_para[ch].irq_mode = irq_mode;

	if ((irq_mode == EPWM_CMPA_UP) || (irq_mode == EPWM_CMPA_DOWN))
		aic_epwm_set_prd_duty(epwm, ch, prd_ns, duty_ns, EPWM_SET_CMPA);
	if ((irq_mode == EPWM_CMPB_UP) || (irq_mode == EPWM_CMPB_DOWN))
		aic_epwm_set_prd_duty(epwm, ch, prd_ns, duty_ns, EPWM_SET_CMPB);

	writel(0x1, epwm->regs + EPWM_ADC_INT_PRE(ch));
	writel(EPWM_INT_EN | ((irq_mode + 2) << EPWM_INT_SEL_SHIFT), epwm->regs + EPWM_ADC_INT_CTL(ch));

	aic_epwm_enable(&epwm->chip, &epwm->chip.pwms[ch]);

	return len;
}
static DEVICE_ATTR_WO(pulse);

static struct attribute *aic_epwm_attr[] = {
	&dev_attr_status.attr,
	&dev_attr_config.attr,
	&dev_attr_output0.attr,
	&dev_attr_output1.attr,
	&dev_attr_pulse.attr,
	NULL
};

static void epwm_reg_enable(void __iomem *base, int offset, int bit, int enable)
{
	int tmp;

	tmp = readl(base + offset);
	tmp &= ~bit;
	if (enable)
		tmp |= bit;

	writel(tmp, base + offset);
}

static inline struct aic_epwm_chip *to_aic_epwm_dev(struct pwm_chip *chip)
{
	return container_of(chip, struct aic_epwm_chip, chip);
}

static void aic_epwm_get_state(struct pwm_chip *chip, struct pwm_device *pwm,
			      struct pwm_state *state)
{
	struct aic_epwm_chip *epwm = to_aic_epwm_dev(chip);
	u32 prd, ch = pwm->hwpwm;
	u64 duty;
	struct aic_epwm_arg *arg = &epwm->args[ch];

	if (!arg->available) {
		dev_err(chip->dev, "%s() ch%d is unavailable!\n", __func__, ch);
		return;
	}

	prd = readl(epwm->regs + EPWM_CNT_PRDV(ch));
	if (arg->mode == EPWM_MODE_UP_DOWN_COUNT)
		prd <<= 1;
	else
		prd++;

	state->period = NSEC_PER_SEC / (arg->tb_clk_rate / prd);
	arg->period = state->period;

	duty = (u64)readl(epwm->regs + EPWM_CNT_AV(ch)) * state->period;
	do_div(duty, prd);
	state->duty_cycle = duty;

	if ((readl(epwm->regs + EPWM_CNT_CONF(ch)) & EPWM_CNT_MOD_MASK) == EPWM_MODE_STOP_COUNT)
		state->enabled = false;
	else
		state->enabled = true;

	state->polarity = PWM_POLARITY_NORMAL;
	memcpy(&pwm->state, state, sizeof(struct pwm_state));
	dev_dbg(chip->dev, "ch%d state: enable %d duty %lld period %lld\n",
		ch, state->enabled, state->duty_cycle, state->period);
}

static int aic_epwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
			     int duty_ns, int period_ns)
{
	struct aic_epwm_chip *epwm = to_aic_epwm_dev(chip);
	u32 ch = pwm->hwpwm;
	u32 ret;

	ret = aic_epwm_set_prd_duty(epwm, ch, period_ns, duty_ns, EPWM_SET_CMPA_CMPB);
	if (ret < 0)
		return ret;
	return 0;
}

static int aic_epwm_set_polarity(struct pwm_chip *chip,
				struct pwm_device *pwm,
				enum pwm_polarity polarity)
{
	struct aic_epwm_chip *epwm = to_aic_epwm_dev(chip);
	u32 ch = pwm->hwpwm;
	struct aic_epwm_arg *arg = &epwm->args[ch];

	if (!arg->available) {
		dev_err(chip->dev, "%s() ch%d is unavailable!\n", __func__, ch);
		return -ENODEV;
	}
	dev_dbg(chip->dev, "ch%d polarity %d\n", ch, polarity);
	/* Configuration of polarity in hardware delayed, do at enable */
	arg->polarity = polarity;
	return 0;
}

static void epwm_action_set(struct aic_epwm_chip *epwm, u32 ch,
		struct aic_epwm_action *act, char *name)
{
	u32 offset;
	u32 action = 0;

	if (strcmp(name, "action0") == 0)
		offset = EPWMA_ACT(ch);
	else
		offset = EPWMB_ACT(ch);

	action |= (act->CBD << EPWMA_ACT_CNTDBV_SHIFT) |
		  (act->CBU << EPWMA_ACT_CNTUBV_SHIFT) |
		  (act->CAD << EPWMA_ACT_CNTDAV_SHIFT) |
		  (act->CAU << EPWMA_ACT_CNTUAV_SHIFT) |
		  (act->PRD << EPWMA_ACT_CNTPRD_SHIFT) | act->ZRO;
	writel(action, epwm->regs + offset);
}

static int aic_epwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct aic_epwm_chip *epwm = to_aic_epwm_dev(chip);
	u32 div1, ch = pwm->hwpwm;
	struct aic_epwm_arg *arg = &epwm->args[ch];

	if (!arg->available) {
		dev_err(chip->dev, "%s() ch%d is unavailable!\n", __func__, ch);
		return -ENODEV;
	}
	dev_dbg(chip->dev, "ch%d enable\n", ch);
	div1 = epwm->clk_rate / arg->tb_clk_rate / 2;
	if (div1 > EPWM_CLK_DIV1_MAX) {
		dev_err(chip->dev, "ch%d clkdiv %d is too big", ch, div1);
		return -ERANGE;
	}

	writel(EPWM_ACT_SW_NONE, epwm->regs +  EPWM_ACT_SW_CT(ch));

	epwm_action_set(epwm, ch, &arg->action0, "action0");
	epwm_action_set(epwm, ch, &arg->action1, "action1");

	writel((div1 << EPWM_CLK_DIV1_SHIFT) | arg->mode,
		epwm->regs + EPWM_CNT_CONF(ch));

	return 0;
}

static void aic_epwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct aic_epwm_chip *epwm = to_aic_epwm_dev(chip);
	u32 ch = pwm->hwpwm;

	dev_dbg(chip->dev, "ch%d disable\n", pwm->hwpwm);

	if (epwm->args[ch].def_level)
		writel(EPWM_ACT_SW_HIGH, epwm->regs + EPWM_ACT_SW_CT(ch));
	else
		writel(EPWM_ACT_SW_LOW, epwm->regs + EPWM_ACT_SW_CT(ch));

	writel((u32)EPWM_MODE_STOP_COUNT, epwm->regs + EPWM_CNT_CONF(ch));
	writel(0, epwm->regs + EPWM_CNT_V(ch));
}

static void aic_epwm_free(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct aic_epwm_chip *epwm = to_aic_epwm_dev(chip);

	dev_dbg(chip->dev, "ch%d free\n", pwm->hwpwm);
	if (pwm_is_enabled(pwm))
		dev_warn(chip->dev, "Removing PWM device without disabling\n");

	epwm->args[pwm->hwpwm].period = 0;
}

static const struct pwm_ops aic_epwm_ops = {
	.free = aic_epwm_free,
	.get_state = aic_epwm_get_state,
	.config = aic_epwm_config,
	.set_polarity = aic_epwm_set_polarity,
	.enable = aic_epwm_enable,
	.disable = aic_epwm_disable,
	.owner = THIS_MODULE,
};

static irqreturn_t aic_epwm_isr(int irq, void *dev_id)
{
	struct aic_epwm_chip *epwm = dev_id;
	static u32 isr_cnt[AIC_EPWM_CH_NUM] = {0};
	u32 stat;
	int i;

	for (i = 0; i < AIC_EPWM_CH_NUM; i++) {
		stat = readl(epwm->regs + EPWM_EVNT_FLAG(i));
		if (stat & EPWM_INT_FLG) {
			isr_cnt[i]++;
			if (isr_cnt[i] == g_epwm_pulse_para[i].pulse_cnt) {
				if ((g_epwm_pulse_para[i].irq_mode == EPWM_CMPA_UP) || (g_epwm_pulse_para[i].irq_mode == EPWM_CMPA_DOWN))
					aic_epwm_set_prd_duty(epwm, i, g_epwm_pulse_para[i].prd_ns, g_epwm_pulse_para[i].prd_ns, EPWM_SET_CMPA);
				if ((g_epwm_pulse_para[i].irq_mode == EPWM_CMPB_UP) || (g_epwm_pulse_para[i].irq_mode == EPWM_CMPB_DOWN))
					aic_epwm_set_prd_duty(epwm, i, g_epwm_pulse_para[i].prd_ns, g_epwm_pulse_para[i].prd_ns, EPWM_SET_CMPB);
				writel(0x0, epwm->regs + EPWM_ADC_INT_PRE(i));
				epwm_reg_enable(epwm->regs, EPWM_ADC_INT_CTL(i), EPWM_INT_EN, 0);
				dev_info(epwm->chip.dev, "isr cnt:%d,disabled the epwm%d interrupt now.\n", isr_cnt[i], i);
				isr_cnt[i] = 0;
			}
			epwm_reg_enable(epwm->regs, EPWM_EVENT_CLR(i), EPWM_INT_CLR, 1);
		}
	}

	return IRQ_HANDLED;
}

static const struct of_device_id aic_epwm_of_match[] = {
	{ .compatible = "artinchip,aic-epwm-v1.0" },
	{},
};
MODULE_DEVICE_TABLE(of, aic_epwm_of_match);

static int aic_epwm_parse_mode(struct device *dev, struct device_node *np)
{
	int ret, i;
	char *mode_str[EPWM_MODE_NUM] = {"up-count",
			"down-count", "up-down-count"};
	const char *tmp;

	ret = of_property_read_string(np, "aic,mode", &tmp);
	if (ret) {
		dev_warn(dev, "Can't parse %s.mode\n", of_node_full_name(np));
		return EPWM_MODE_UP_COUNT;
	}

	for (i = 0; i < EPWM_MODE_NUM; i++)
		if (strcmp(mode_str[i], tmp) == 0)
			return i;

	/* Otherwise, return the default mode */
	return EPWM_MODE_UP_COUNT;
}

static void aic_epwm_parse_action(struct device *dev, struct device_node *np,
		char *name, struct aic_epwm_action *act)
{
	int ret, i, j;
	const char *tmp[EPWM_ACTION_CFG_NUM];
	char *act_str[EPWM_ACT_NUM] = {"none", "low", "high", "inverse"};
	enum aic_epwm_action_type *pa = (enum aic_epwm_action_type *)act;

	memset(act, EPWM_ACT_NONE, sizeof(struct aic_epwm_action));
	ret = of_property_read_string_array(np, name, tmp, EPWM_ACTION_CFG_NUM);
	if (ret != EPWM_ACTION_CFG_NUM) {
		dev_warn(dev, "Can't parse %s.%s\n",
			of_node_full_name(np), name);
		return;
	}

	for (i = 0; i < EPWM_ACTION_CFG_NUM; i++)
		for (j = 0; j < EPWM_ACT_NUM; j++)
			if (strcmp(act_str[j], tmp[i]) == 0) {
				pa[i] = j;
				break;
			}
}

static int aic_epwm_parse_dt(struct device *dev)
{
	int ret, i = 0;
	struct device_node *child, *np = dev->of_node;
	struct aic_epwm_chip *epwm = dev_get_drvdata(dev);

	ret = of_property_read_u32(np, "clock-rate", (u32 *)&epwm->clk_rate);
	if (ret) {
		dev_warn(dev, "Can't parse clock-rate\n");
		return ret;
	}

	for_each_child_of_node(np, child) {
		struct aic_epwm_arg *arg = &epwm->args[i];

		arg->available = of_device_is_available(child);
		if (!arg->available) {
			dev_dbg(dev, "ch%d is unavailable.\n", i);
			i++;
			continue;
		}
		dev_dbg(dev, "ch%d is available\n", i);
		epwm_reg_enable(epwm->glb_regs, GLB_CLK_CTL, EPMW_SX_CLK_EN(i), 1);
		writel(EPWM_SWACT_UPDT, epwm->regs + EPWM_SW_ACT(i));

		ret = of_property_read_u32(child, "aic,tb-clk-rate",
					   (u32 *)&arg->tb_clk_rate);
		if (ret || (arg->tb_clk_rate == 0)) {
			dev_err(dev, "Invalid ch%d tb-clk-rate %d\n",
				i, arg->tb_clk_rate);
			arg->tb_clk_rate = EPWM_DEFAULT_TB_CLK_RATE;
		}

		arg->mode = aic_epwm_parse_mode(dev, child);
		aic_epwm_parse_action(dev, child, "aic,action0", &arg->action0);
		aic_epwm_parse_action(dev, child, "aic,action1", &arg->action1);

		ret = of_property_read_u32(child, "aic,default-level",
					   (u32 *)&arg->def_level);
		if (ret) {
			dev_dbg(dev, "Set default level by PRD/ZRO");
			if (arg->action0.PRD == EPWM_ACT_LOW ||
			    arg->action0.ZRO == EPWM_ACT_LOW)
				arg->def_level = true;
			if (arg->mode == EPWM_MODE_DOWN_COUNT)
				arg->def_level = !arg->def_level;
			if (arg->mode == EPWM_MODE_UP_DOWN_COUNT) {
				if (arg->action0.ZRO == EPWM_ACT_HIGH)
					arg->def_level = false;
				else
					arg->def_level = true;
			}
		}
		i++;
	}
	return 0;
}

static int aic_epwm_probe(struct platform_device *pdev)
{
	struct aic_epwm_chip *epwm;
	struct clk *clk;
	int ret;
	int irq;

	epwm = devm_kzalloc(&pdev->dev,
			sizeof(struct aic_epwm_chip), GFP_KERNEL);
	if (!epwm)
		return -ENOMEM;

	epwm->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(epwm->regs))
		return PTR_ERR(epwm->regs);

	clk = devm_clk_get(&pdev->dev, "sysclk");
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "Failed to get sysclk clock\n");
		return PTR_ERR(clk);
	}

	epwm->pll_rate = clk_get_rate(clk);
	if (!epwm->pll_rate) {
		dev_err(&pdev->dev, "Failed to get sysclk clock rate\n");
		return -EINVAL;
	}

	epwm->clk = devm_clk_get(&pdev->dev, "pwmcs");
	if (IS_ERR(epwm->clk)) {
		dev_err(&pdev->dev, "Failed to get epwm clk\n");
		return PTR_ERR(epwm->clk);
	}
	ret = clk_prepare_enable(epwm->clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "clk_prepare_enable() failed: %d\n", ret);
		return ret;
	}

	epwm->rst = devm_reset_control_get_optional_shared(&pdev->dev, NULL);
	if (IS_ERR(epwm->rst)) {
		ret = PTR_ERR(epwm->rst);
		goto out_disable_clk;
	}
	reset_control_deassert(epwm->rst);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "%s() Failed to get IRQ\n", __func__);
		ret = irq;
		goto out_disable_rst;
	}
	ret = devm_request_irq(&pdev->dev, irq, aic_epwm_isr,
			       0, AIC_EPWM_NAME, epwm);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to request IRQ %d\n", irq);
		goto out_disable_rst;
	}
	epwm->irq = irq;

	platform_set_drvdata(pdev, epwm);

	epwm->attrs.attrs = aic_epwm_attr;
	ret = sysfs_create_group(&pdev->dev.kobj, &epwm->attrs);
	if (ret)
		goto out_disable_rst;

	epwm->chip.dev = &pdev->dev;
	epwm->chip.ops = &aic_epwm_ops;
	epwm->chip.of_xlate = of_pwm_xlate_with_flags;
	epwm->chip.of_pwm_n_cells = 3;
	epwm->chip.base = -1;
	epwm->chip.npwm = AIC_EPWM_CH_NUM;
	ret = pwmchip_add(&epwm->chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "pwmchip_add() failed: %d\n", ret);
		goto out_disable_rst;
	}

	epwm->glb_regs = of_iomap(pdev->dev.of_node, 1);
	if (IS_ERR(epwm->glb_regs))
		goto out_pwmchip_remove;

	epwm_reg_enable(epwm->glb_regs, GLB_EPWM_EN, GLB_EPWM_EN_B, 1);
	ret = aic_epwm_parse_dt(&pdev->dev);
	if (ret)
		goto out_pwmchip_remove;

	//unmap to be used by other PWMCS Submodules
	iounmap(epwm->glb_regs);

	ret = clk_set_rate(epwm->clk, epwm->clk_rate);
	if (ret) {
		dev_err(&pdev->dev, "Failed to set clk_rate %ld\n",
			epwm->clk_rate);
		goto out_pwmchip_remove;
	}

	dev_info(&pdev->dev, "ArtInChip EPWM Loaded.\n");
	return 0;

out_pwmchip_remove:
	pwmchip_remove(&epwm->chip);
out_disable_rst:
	reset_control_assert(epwm->rst);
out_disable_clk:
	clk_disable_unprepare(epwm->clk);
	return ret;
}

static int aic_epwm_remove(struct platform_device *pdev)
{
	struct aic_epwm_chip *epwm = platform_get_drvdata(pdev);

	pwmchip_remove(&epwm->chip);

	reset_control_assert(epwm->rst);
	clk_unprepare(epwm->clk);

	return 0;
}

#ifdef CONFIG_PM
static int aic_epwm_pm_suspend(struct device *dev)
{
	struct aic_epwm_chip *epwm = dev_get_drvdata(dev);

	clk_disable_unprepare(epwm->clk);
	return 0;
}

static int aic_epwm_pm_resume(struct device *dev)
{
	struct aic_epwm_chip *epwm = dev_get_drvdata(dev);

	clk_set_rate(epwm->clk, epwm->clk_rate);
	clk_prepare_enable(epwm->clk);
	return 0;
}

static const struct dev_pm_ops aic_epwm_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(aic_epwm_pm_suspend, aic_epwm_pm_resume)
};
#endif

static struct platform_driver aic_epwm_driver = {
	.driver = {
		.name = AIC_EPWM_NAME,
		.of_match_table = aic_epwm_of_match,
#ifdef CONFIG_PM
		.pm = &aic_epwm_pm_ops,
#endif

	},
	.probe = aic_epwm_probe,
	.remove = aic_epwm_remove,
};
module_platform_driver(aic_epwm_driver);

MODULE_AUTHOR("zrq <ruiqi.zheng@artinchip.com>");
MODULE_DESCRIPTION("EPWM driver of ArtInChip SoC");
MODULE_LICENSE("GPL");
