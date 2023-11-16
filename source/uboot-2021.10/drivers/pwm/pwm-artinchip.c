// SPDX-License-Identifier: GPL-2.0-only
/*
 * PWM driver of ArtInChip SoC
 *
 * Copyright (C) 2020-2022 ArtInChip Technology Co., Ltd.
 * Authors:  Matteo <duanmt@artinchip.com>
 */

#include <common.h>
#include <log.h>
#include <pwm.h>
#include <asm/io.h>
#include <dm/device_compat.h>
#include <clk.h>
#include <reset.h>
#include <dm.h>
#include <div64.h>

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

#define PWM_DEFAULT_TB_CLK_RATE	24000000
#define PWM_DEFAULT_DB_RED	20
#define PWM_DEFAULT_DB_FED	20

#define PWM_ACTION_CFG_NUM	6

#define PWM_CTL_EN			BIT(0)
#define PWM_MCTL_PWM0_EN		BIT(0)
#define PWM_MCTL_PWM_EN(n)		(PWM_MCTL_PWM0_EN << (n))
#define PWM_CKCTL_PWM0_ON		BIT(0)
#define PWM_CKCTL_PWM_ON(n)		(PWM_CKCTL_PWM0_ON << (n))
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

#define NSEC_PER_SEC 1000000000L

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
	bool polarity;
};

struct aic_pwm_chip {
	struct aic_pwm_arg args[AIC_PWM_CH_NUM];
	unsigned long pll_rate;
	unsigned long clk_rate;
	void __iomem *regs;
	struct clk clk;
	struct reset_ctl rst;
	u32 irq;
};

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

int pwm_status_show(struct udevice *dev)
{
	struct aic_pwm_chip *apwm = dev_get_priv(dev);
	struct aic_pwm_arg *arg = apwm->args;
	void __iomem *regs = apwm->regs;
	int ver = readl(regs + PWM_VERSION);
	int enable = readl(regs + PWM_MCTL);
	char info[AIC_PWM_CH_NUM][128] = {{0}};
	u32 i;

	for (i = 0; i < AIC_PWM_CH_NUM; i++)
		aic_pwm_ch_info(info[i], i, enable, &arg[i]);

	pr_info("In PWM V%d.%02d:\n"
		"Module Enable: %d, IRQ Enable: %#x\n"
		"Ch En Mode Tb-clk-rate Def CBD CBU CAD CAU PRD ZRO\n"
		"%s%s%s%s",
		ver >> 8, ver & 0xFF,
		readl(regs + PWM_CTL), readl(regs + PWM_INTCTL),
		info[0], info[1], info[2], info[3]);
	return 0;
}

static void pwm_reg_enable(void __iomem *base, int offset, int bit, int enable)
{
	int tmp;

	tmp = readl(base + offset);
	tmp &= ~bit;
	if (enable)
		tmp |= bit;

	writel(tmp, base + offset);
}

static int aic_pwm_config(struct udevice *dev, uint ch, uint period_ns,
			  uint duty_ns)
{
	struct aic_pwm_chip *apwm = dev_get_priv(dev);
	u32 prd;
	u64 duty;
	struct aic_pwm_arg *arg = &apwm->args[ch];

	if (!arg->available) {
		dev_err(dev, "%s() ch%d is unavailable!\n", __func__, ch);
		return -ENODEV;
	}
	dev_dbg(dev, "ch%d duty %d period %d\n", ch, duty_ns, period_ns);
	if (period_ns < 1 || period_ns > NSEC_PER_SEC) {
		dev_err(dev, "ch%d invalid period %d\n", ch, period_ns);
		return -ERANGE;
	}

	arg->freq = NSEC_PER_SEC / period_ns;
	prd = arg->tb_clk_rate / arg->freq;
	if (arg->mode == PWM_MODE_UP_DOWN_COUNT)
		prd >>= 1;
	else
		prd--;

	if (prd > PWM_TBPRD_MAX) {
		dev_err(dev, "ch%d period %d is too big\n", ch, prd);
		return -ERANGE;
	}
	arg->period = prd;
	writel(prd, apwm->regs + PWM_TBPRD(ch));

	duty = (u64)duty_ns * (u64)prd;
	do_div(duty, period_ns);
	if (duty == prd)
		duty--;

	dev_dbg(dev, "Set CMP %llu/%u\n", duty, prd);
	writel((u32)duty, apwm->regs + PWM_CMPA(ch));
	writel((u32)duty, apwm->regs + PWM_CMPB(ch));
	return 0;
}

static int aic_pwm_set_polarity(struct udevice *dev, uint ch, bool polarity)
{
	struct aic_pwm_chip *apwm = dev_get_priv(dev);
	struct aic_pwm_arg *arg = &apwm->args[ch];

	if (!arg->available) {
		dev_err(dev, "%s() ch%d is unavailable!\n", __func__, ch);
		return -ENODEV;
	}
	dev_dbg(dev, "ch%d polarity %d\n", ch, polarity);
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

static int aic_pwm_enable(struct udevice *dev, uint ch, bool enable)
{
	struct aic_pwm_chip *apwm = dev_get_priv(dev);
	u32 div;
	struct aic_pwm_arg *arg = &apwm->args[ch];

	if (!arg->available) {
		dev_err(dev, "%s() ch%d is unavailable!\n", __func__, ch);
		return -ENODEV;
	}
	dev_dbg(dev, "ch%d %s\n", ch, enable ? "enable" : "disable");
	if (!enable) {
		pwm_reg_enable(apwm->regs, PWM_MCTL, PWM_MCTL_PWM_EN(ch), 0);
		return 0;
	}

	div = apwm->clk_rate / arg->tb_clk_rate - 1;
	if (div > PWM_TBCTL_CLKDIV_MAX) {
		dev_err(dev, "ch%d clkdiv %d is too big", ch, div);
		return -ERANGE;
	}
	writel((div << PWM_TBCTL_CLKDIV_SHIFT) | arg->mode,
	       apwm->regs + PWM_TBCTL(ch));

	pwm_action_set(apwm, ch, &arg->action0, "action0");
	pwm_action_set(apwm, ch, &arg->action1, "action1");

	pwm_reg_enable(apwm->regs, PWM_MCTL, PWM_MCTL_PWM_EN(ch), 1);
	return 0;
}

static const struct pwm_ops aic_pwm_ops = {
	.set_config = aic_pwm_config,
	.set_invert = aic_pwm_set_polarity,
	.set_enable = aic_pwm_enable,
};

static int aic_pwm_parse_mode(struct udevice *dev, ofnode node)
{
	int i;
	char *mode_str[PWM_MODE_NUM] = {"up-count",
			"down-count", "up-down-count"};
	const char *tmp;

	tmp = ofnode_read_string(node, "aic,mode");
	if (!tmp) {
		dev_warn(dev, "Can't parse %s.mode\n", ofnode_get_name(node));
		return PWM_MODE_UP_COUNT;
	}

	for (i = 0; i < PWM_MODE_NUM; i++)
		if (strcmp(mode_str[i], tmp) == 0)
			return i;

	/* Otherwise, return the default mode */
	return PWM_MODE_UP_COUNT;
}

static void aic_pwm_parse_action(struct udevice *dev, ofnode node,
				 char *name, struct aic_pwm_action *act)
{
	int ret, i, j;
	const char *tmp;
	char *act_str[PWM_ACT_NUM] = {"none", "low", "high", "inverse"};
	enum aic_pwm_action_type *pa = (enum aic_pwm_action_type *)act;

	memset(act, PWM_ACT_NONE, sizeof(struct aic_pwm_action));
	for (i = 0; i < PWM_ACTION_CFG_NUM; i++) {
		ret = ofnode_read_string_index(node, name, i, &tmp);
		if (ret < 0) {
			dev_warn(dev, "Invalid %s.%s\n",
				 ofnode_get_name(node), name);
		}

		for (j = 0; j < PWM_ACT_NUM; j++)
			if (strcmp(act_str[j], tmp) == 0) {
				pa[i] = j;
				break;
			}
	}
}

static int aic_pwm_parse_dt(struct udevice *dev)
{
	int ret, i = 0;
	ofnode child;
	struct aic_pwm_chip *apwm = dev_get_priv(dev);

	ret = dev_read_u32(dev, "clock-rate", (u32 *)&apwm->clk_rate);
	if (ret) {
		dev_warn(dev, "Can't parse clock-rate\n");
		return ret;
	}

	ofnode_for_each_subnode(child, dev_ofnode(dev)) {
		struct aic_pwm_arg *arg = &apwm->args[i];
		// const struct device_node *child = ofnode_to_np(node);

		arg->available = ofnode_is_available(child);
		if (!arg->available) {
			dev_dbg(dev, "ch%d is unavailable.\n", i);
			i++;
			continue;
		}
		dev_dbg(dev, "ch%d is available\n", i);
		pwm_reg_enable(apwm->regs, PWM_CKCTL, PWM_CKCTL_PWM_ON(i), 1);

		ret = ofnode_read_u32(child, "aic,tb-clk-rate",
				      (u32 *)&arg->tb_clk_rate);
		if (ret || arg->tb_clk_rate == 0) {
			dev_err(dev, "Invalid ch%d tb-clk-rate %d\n",
				i, arg->tb_clk_rate);
			arg->tb_clk_rate = PWM_DEFAULT_TB_CLK_RATE;
		}

		ret = ofnode_read_u32(child, "aic,rise-edge-delay",
				      (u32 *)&arg->db_red);
		if (ret) {
			dev_info(dev, "Can't parse %d.rise-edge-delay\n", i);
			arg->db_red = PWM_DEFAULT_DB_RED;
		}

		ret = ofnode_read_u32(child, "aic,fall-edge-delay",
				      (u32 *)&arg->db_fed);
		if (ret) {
			dev_info(dev, "Can't parse %d.fall-edge-delay\n", i);
			arg->db_fed = PWM_DEFAULT_DB_FED;
		}

		arg->mode = aic_pwm_parse_mode(dev, child);
		aic_pwm_parse_action(dev, child, "aic,action0", &arg->action0);
		aic_pwm_parse_action(dev, child, "aic,action1", &arg->action1);

		ret = ofnode_read_u32(child, "aic,default-level",
				      (u32 *)&arg->def_level);
		if (ret < 0) {
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

static int aic_pwm_of_to_plat(struct udevice *dev)
{
	struct aic_pwm_chip *apwm = dev_get_priv(dev);

	apwm->regs = (void __iomem *)dev_read_addr(dev);
	return 0;
}

static int aic_pwm_probe(struct udevice *dev)
{
	struct aic_pwm_chip *apwm = dev_get_priv(dev);
	struct clk clk;
	int ret;

	ret = clk_get_by_name(dev, "sysclk", &clk);
	if (ret < 0) {
		dev_err(dev, "Failed to get sysclk clock\n");
		return ret;
	}

	apwm->pll_rate = clk_get_rate(&clk);
	if (apwm->pll_rate < 0) {
		dev_err(dev, "Failed to get sysclk clock rate\n");
		return -EINVAL;
	}

	ret = clk_get_by_name(dev, "pwm", &apwm->clk);
	if (ret < 0) {
		dev_err(dev, "Failed to get PWM clk\n");
		return ret;
	}
	ret = clk_enable(&apwm->clk);
	if (ret < 0) {
		dev_err(dev, "clk_enable() failed: %d\n", ret);
		return ret;
	}

	ret = reset_get_by_index(dev, 0, &apwm->rst);
	if (ret < 0) {
		dev_err(dev, "Failed to get PWM reset\n");
		goto out_disable_clk;
	}
	reset_deassert(&apwm->rst);

	pwm_reg_enable(apwm->regs, PWM_CTL, PWM_CTL_EN, 1);
	ret = aic_pwm_parse_dt(dev);
	if (ret)
		goto out_disable_rst;

	ret = clk_set_rate(&apwm->clk, apwm->clk_rate);
	if (ret) {
		dev_err(dev, "Failed to set clk_rate %ld\n", apwm->clk_rate);
		goto out_disable_rst;
	}

	dev_info(dev, "ArtInChip PWM Loaded.\n");
	return 0;

out_disable_rst:
	reset_assert(&apwm->rst);
out_disable_clk:
	clk_disable(&apwm->clk);
	return ret;
}

static const struct udevice_id aic_pwm_ids[] = {
	{ .compatible = "artinchip,aic-pwm-v1.0" },
	{},
};

U_BOOT_DRIVER(aic_pwm) = {
	.name		= AIC_PWM_NAME,
	.id		= UCLASS_PWM,
	.of_match	= aic_pwm_ids,
	.ops		= &aic_pwm_ops,
	.of_to_plat	= aic_pwm_of_to_plat,
	.probe		= aic_pwm_probe,
	.priv_auto	= sizeof(struct aic_pwm_chip),
};
