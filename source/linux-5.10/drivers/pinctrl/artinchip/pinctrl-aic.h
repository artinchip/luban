/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2020 Artinchip Inc.
 */

#ifndef __PINCTRL_AIC_H__
#define __PINCTRL_AIC_H__

#include <linux/gpio/driver.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/clk.h>
#include <linux/reset.h>

#define AIC_GPIO_PINS_PER_BANK		(32)

struct aic_desc_function {
	const unsigned char num;
	const char *name;
};

struct aic_desc_pin {
	struct pinctrl_pin_desc pin;
	const struct aic_desc_function *functions;
};

#define AIC_PIN(_pin, ...)			\
	{			\
		.pin = _pin,			\
		.functions = (struct aic_desc_function[]) {	\
			__VA_ARGS__, { } },			\
	}

#define AIC_FUNCTION(_num, _name)			\
	{			\
		.num = _num,			\
		.name = _name,			\
	}

struct aic_pinctrl_match_data {
	struct aic_desc_pin	*pins;
	u32 npins;
};

struct aic_pinctrl_group {
	const char *name;
	u32 config;
	u32 pin;
};

struct aic_pinctrl_function {
	const char *name;
	const char **groups;
	u32 ngroups;
};

struct aic_gpio_regs {
	u32 dat_in;
	u32 dat_out;
	u32 irq_en;
	u32 irq_sta;
#ifndef CONFIG_PINCTRL_ARTINCHIP_V1
	u32 samp;
#endif
	u32 pin_cfg;
};

struct aic_gpio_bank {
	u32 bank_nr;
	int irq;
	u32 saved_mask;
	spinlock_t lock;
	struct gpio_chip gpio_chip;
	struct pinctrl_gpio_range range;
	struct irq_domain *domain;
	struct aic_gpio_regs regs;
	struct aic_pinctrl *pctl;
};

struct aic_pinctrl {
	void __iomem *base;
	struct device *dev;
	struct pinctrl_dev *pctl_dev;
	struct pinctrl_desc pctl_desc;
	struct aic_pinctrl_group *groups;
	u32 ngroups;
	const char **grp_names;
	struct aic_pinctrl_function	*functions;
	u32 nfunctions;
	struct aic_gpio_bank *banks;
	u32 nbanks;
	struct aic_desc_pin *pins;
	u32 npins;
	struct reset_control *reset;
	struct clk *clk;
};


int aic_pinctl_probe(struct platform_device *pdev);

#endif /* __PINCTRL_AIC_H__ */

