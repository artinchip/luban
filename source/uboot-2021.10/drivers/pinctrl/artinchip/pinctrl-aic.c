// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2020 ArtInChip Inc.
 */

#include <common.h>
#include <dm.h>
#include <hwspinlock.h>
#include <asm/arch/gpio.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <dm/lists.h>
#include <dm/pinctrl.h>
#include <dt-bindings/pinctrl/aic-pinctrl.h>

#define GEN_IOE_SHIFT				(16)
#define GEN_IOE_MASK				(3)
#define GEN_IOE_IE				(1)
#define GEN_IOE_OE				(2)

#define PIN_PULL_SHIFT				(8)
#define PIN_PULL_MASK				(3)
#define PIN_PULL_UP				(3)
#define PIN_PULL_DOWN				(2)
#define PIN_PULL_DISABLE			(0)

#define PIN_DRIVE_SHIFT				(4)
#define PIN_DRIVE_MASK				(7)
#define PIN_DRIVE_MAX				PIN_DRIVE_MASK

#define PIN_FUNCTION_SHIFT			(0)
#define PIN_FUNCTION_MASK			(0xf)
#define PIN_FUNCTION_GPIO			(1)

static int aic_get_pin_dsc(struct aic_pin_dsc *pin_dsc, u32 port_pin)
{
	pin_dsc->port = (port_pin >> AIC_PORTID_OFFSET) & 0xff;
	pin_dsc->pin = (port_pin >> AIC_PINID_OFFSET) & 0xff;

	return 0;
}

static int aic_get_pin_ctl(struct aic_pin_ctl *pin_ctl,
				u32 gpio_fn, int node)
{
	pin_ctl->pinmux = (gpio_fn >> AIC_PINMUX_OFFSET) & PIN_FUNCTION_MASK;

	if (fdtdec_get_bool(gd->fdt_blob, node, "bias-pull-up"))
		pin_ctl->pupd = AIC_GPIO_PUPD_UP;
	else if (fdtdec_get_bool(gd->fdt_blob, node, "bias-pull-down"))
		pin_ctl->pupd = AIC_GPIO_PUPD_DOWN;
	else
		pin_ctl->pupd = AIC_GPIO_PUPD_NO;

	pin_ctl->drive = fdtdec_get_int(gd->fdt_blob, node, "drive-strength", 0);

	return 0;
}

static int aic_pin_config(struct gpio_desc *desc,
				const struct aic_pin_ctl *pin_ctl)
{
	u32 value;
	void __iomem *reg;
	struct aic_gpio_priv *priv = dev_get_priv(desc->dev);

	if (!pin_ctl || (pin_ctl->pinmux > PIN_FUNCTION_MASK) ||
			(pin_ctl->drive > PIN_DRIVE_MAX) ||
			(pin_ctl->pupd > PIN_PULL_MASK)) {
		pr_err("%s: para invalid!\n", __func__);
		return -EINVAL;
	}

	reg = priv->base + priv->bank->regs.pin_cfg + desc->offset*4;

	value = readl(reg);
	value &= ~((PIN_FUNCTION_MASK << PIN_FUNCTION_SHIFT) |
		   (PIN_DRIVE_MASK << PIN_DRIVE_SHIFT) |
		   (PIN_PULL_MASK << PIN_PULL_SHIFT) |
		   (GEN_IOE_MASK << GEN_IOE_SHIFT));
	value |= (pin_ctl->pinmux << 0) | (pin_ctl->drive << 4) |
		 (pin_ctl->pupd << 8);
	writel(value, reg);

	return 0;
}

static int aic_pinctrl_config(int offset)
{
	int i, cnt, ret;
	struct gpio_desc desc;
	struct aic_pin_dsc pin_dsc;
	struct aic_pin_ctl pin_ctl;
	u32 pin_mux[AIC_MAX_PINMUX_ENTRIES];

	fdt_for_each_subnode(offset, gd->fdt_blob, offset) {
		cnt = fdtdec_get_int_array_count(gd->fdt_blob, offset,
						 "pinmux", pin_mux, ARRAY_SIZE(pin_mux));
		if (cnt < 0) {
			pr_err("%s: try get pinmux list failed!\n", __func__);
			return -EINVAL;
		}

		for (i = 0; i < cnt; i++) {
			aic_get_pin_dsc(&pin_dsc, *(pin_mux + i));
			aic_get_pin_ctl(&pin_ctl, *(pin_mux + i), offset);
			ret = uclass_get_device_by_seq(UCLASS_GPIO,
							pin_dsc.port, &desc.dev);
			if (ret) {
				pr_err("%s: get gpio device failed!\n", __func__);
				return ret;
			}

			desc.offset = pin_dsc.pin;
			ret = aic_pin_config(&desc, &pin_ctl);
			if (ret) {
				pr_err("%s: try to config one pin failed!\n", __func__);
				return ret;
			}
		}
	}

	return 0;
}

#if CONFIG_IS_ENABLED(PINCTRL_FULL)
static int aic_pinctrl_set_state(struct udevice *dev,
					struct udevice *config)
{
	return aic_pinctrl_config(dev_of_offset(config));
}
#else	/* #if CONFIG_IS_ENABLED(PINCTRL_FULL) */
static int aic_pinctrl_set_state_simple(struct udevice *dev,
					struct udevice *periph)
{
	const fdt32_t *list;
	uint32_t phandle;
	int size, i, ret, config_offset;

	list = fdt_getprop(gd->fdt_blob, dev_of_offset(periph),
				"pinctrl-0", &size);
	if (!list)
		return -EINVAL;

	size /= sizeof(*list);
	for (i = 0; i < size; i++) {
		phandle = fdt32_to_cpu(*list++);

		config_offset = fdt_node_offset_by_phandle(gd->fdt_blob, phandle);
		if (config_offset < 0) {
			pr_err("pinctrl-0 index %d invalid\n", i);
			return -EINVAL;
		}

		ret = aic_pinctrl_config(config_offset);
		if (ret) {
			pr_err("config pinctrl-0 index %d failed!\n", i);
			return ret;
		}
	}

	return 0;
}
#endif	/* #if CONFIG_IS_ENABLED(PINCTRL_FULL) */

#ifndef CONFIG_SPL_BUILD

static const char *aic_dummy_name = "_dummy";
static char aic_tmp_name[PINNAME_SIZE];

static const char *aic_pinctrl_get_pin_name(struct udevice *dev,
					unsigned int selector)
{
	int		i, gpio_idx;
	struct gpio_dev_priv *uc_priv;
	struct aic_pinctrl_priv *priv = dev_get_priv(dev);

	if (selector >= priv->gpio_cnt) {
		pr_err("%s: selector is invalid!\n", __func__);
		return aic_dummy_name;
	}

	for (i = 0; i < AIC_MAX_PIN_BANKS; i++) {
		if (selector < priv->banks[i].offset + priv->banks[i].count)
			break;
	}

	gpio_idx = selector - priv->banks[i].offset;
	uc_priv = dev_get_uclass_priv(priv->banks[i].dev);
	snprintf(aic_tmp_name, PINNAME_SIZE, "%s%d",
			uc_priv->bank_name, gpio_idx);

	return aic_tmp_name;
}

static int aic_pinctrl_get_pins_count(struct udevice *dev)
{
	struct aic_pinctrl_priv *priv = dev_get_priv(dev);

	return priv->gpio_cnt;
}

static int aic_pinctrl_get_pin_muxing(struct udevice *dev,
					unsigned int selector,
					char *buf,
					int size)
{
	int i, offset;
	u32 value;
	void __iomem *reg;
	struct aic_pinctrl_priv *priv = dev_get_priv(dev);

	if (selector >= priv->gpio_cnt) {
		pr_err("%s: selector is invalid!\n", __func__);
		snprintf(buf, size, "%s", aic_dummy_name);
		return -EINVAL;
	}

	for (i = 0; i < AIC_MAX_PIN_BANKS; i++) {
		if (selector < priv->banks[i].offset + priv->banks[i].count)
			break;
	}

	offset = selector - priv->banks[i].offset;
	reg = priv->base + priv->banks[i].regs.pin_cfg + offset*4;
	value = readl(reg);
	snprintf(buf, size, "func%d", value&0x07);

	return 0;
}
#endif	/* #ifndef CONFIG_SPL_BUILD */

static struct pinctrl_ops aic_pinctrl_ops = {
#if CONFIG_IS_ENABLED(PINCTRL_FULL)
	.set_state			= aic_pinctrl_set_state,
#else	/* #if CONFIG_IS_ENABLED(PINCTRL_FULL) */
	.set_state_simple	= aic_pinctrl_set_state_simple,
#endif	/* #if CONFIG_IS_ENABLED(PINCTRL_FULL) */

#ifndef CONFIG_SPL_BUILD
	.get_pin_name		= aic_pinctrl_get_pin_name,
	.get_pins_count		= aic_pinctrl_get_pins_count,
	.get_pin_muxing		= aic_pinctrl_get_pin_muxing,
#endif	/* ifndef CONFIG_SPL_BUILD */
};

static int aic_pinctrl_bind(struct udevice *dev)
{
	int		ret;
	ofnode	node;
	const char	*name;

	/* bind gpio driver */
	dev_for_each_subnode(node, dev) {
		ofnode_get_property(node, "gpio-controller", &ret);
		if (ret < 0)
			continue;

		/* Get the name of each gpio node */
		name = ofnode_get_name(node);
		if (!name) {
			pr_err("%s: get node name failed!\n", __func__);
			return -EINVAL;
		}

		/* Bind each gpio node */
		ret = device_bind_driver_to_node(dev, "aic_gpio",
						name, node, NULL);
		if (ret) {
			pr_err("%s: bind gpio driver failed!\n", __func__);
			return ret;
		}
	}

	return 0;
}

static int aic_pinctrl_probe(struct udevice *dev)
{
	struct aic_pinctrl_priv *priv = dev_get_priv(dev);

	priv->base = dev_read_addr_ptr(dev);

	return 0;
}

static const struct udevice_id aic_pinctrl_ids[] = {
	{ .compatible = "artinchip,aic-pinctrl-v0.1" },
	{ .compatible = "artinchip,aic-pinctrl-v1.0" },
	{ /* sentinel */ }
};

U_BOOT_DRIVER(pinctrl_aic) = {
	.name		= "pinctrl_aic",
	.id		= UCLASS_PINCTRL,
	.of_match	= aic_pinctrl_ids,
	.ops		= &aic_pinctrl_ops,
	.bind		= aic_pinctrl_bind,
	.probe		= aic_pinctrl_probe,
	.flags		= DM_FLAG_PRE_RELOC,
	.priv_auto	= sizeof(struct aic_pinctrl_priv),
};

