/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2020 ArtInChip Inc.
 */

#ifndef __GPIO_H__
#define __GPIO_H__

#define AIC_GPIOS_PER_BANK		32
#define AIC_MAX_PIN_BANKS		8
#define AIC_MAX_PINMUX_ENTRIES	32

enum aic_gpio_pupd {
	AIC_GPIO_PUPD_NO = 0,
	AIC_GPIO_PUPD_RSV,
	AIC_GPIO_PUPD_DOWN,
	AIC_GPIO_PUPD_UP
};

struct aic_gpio_regs {
	u32		dat_in;
	u32		dat_out;
	u32		irq_en;
	u32		irq_sta;
	u32		pin_cfg;
};

struct aic_pin_dsc {
	int		port;
	int		pin;
};

struct aic_pin_ctl {
	int		pinmux;
	int		drive;
	enum aic_gpio_pupd	pupd;
};

struct aic_pin_bank_dsc {
	u16		offset;
	u16		count;
	int		debounce;
	struct udevice *dev;
	struct aic_gpio_regs	regs;
};

struct aic_pinctrl_priv {
	void __iomem	*base;
	int		gpio_cnt;
	struct aic_pin_bank_dsc	banks[AIC_MAX_PIN_BANKS];
};

struct aic_gpio_priv {
	int		index;
	void __iomem	*base;
	unsigned int gpio_range;
	struct aic_pin_bank_dsc	*bank;
};

#endif /* __GPIO_H__ */
