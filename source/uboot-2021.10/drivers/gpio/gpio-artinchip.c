// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2020 ArtInChip Inc.
 */

#include <common.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <dm/pinctrl.h>
#include <errno.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/arch/gpio.h>

#define GEN_IOE_SHIFT				(16)
#define GEN_IOE_MASK				(3)
#define GEN_IOE_IE				(1)
#define GEN_IOE_OE				(2)

#define PIN_FUNCTION_SHIFT			(0)
#define PIN_FUNCTION_MASK			(7)
#define PIN_FUNCTION_GPIO			(1)
#define PIN_FUNCTION_DISABLE			(0)

static int aic_gpio_get_function(struct udevice *dev, unsigned int off);

static int aic_gpio_get(struct udevice *dev, unsigned int off)
{
	u32 value;
	int dir;
	void __iomem *reg;
	struct aic_gpio_priv *priv = dev_get_priv(dev);

	if (!(priv->gpio_range & (1 << off))) {
		pr_err("%s: pin (off=%u) is invalid!\n", __func__, off);
		return -EINVAL;
	}

	dir = aic_gpio_get_function(dev, off);

	if (!dir)
		reg = priv->base + priv->bank->regs.dat_in;
	else if (dir == GPIOF_OUTPUT)
		reg = priv->base + priv->bank->regs.dat_out;
	else
		return dir;

	value = readl(reg);

	return !!(value & (1 << off));
}

static int aic_gpio_set(struct udevice *dev,
				unsigned int off, int val)
{
	u32 value;
	void __iomem *reg;
	struct aic_gpio_priv *priv = dev_get_priv(dev);

	if (!(priv->gpio_range & (1 << off))) {
		pr_err("%s: pin (off=%u) is invalid!\n", __func__, off);
		return -EINVAL;
	}

	/* set data to value */
	reg = priv->base + priv->bank->regs.dat_out;

	value = readl(reg);
	value &= ~(1 << off);
	value |= !!val << off;
	writel(value, reg);
	return 0;
}

static int aic_gpio_get_function(struct udevice *dev, unsigned int off)
{
	u32 ret = 0;
	u32 value;
	void __iomem *reg;
	struct aic_gpio_priv *priv = dev_get_priv(dev);

	if (!(priv->gpio_range & (1 << off))) {
		pr_err("%s: pin (off=%u) is invalid!\n", __func__, off);
		return -EINVAL;
	}

	/* switch function to general input/output, and enable input */
	reg = priv->base + priv->bank->regs.pin_cfg + off*4;

	value = readl(reg);
	switch (value & PIN_FUNCTION_MASK) {
	case PIN_FUNCTION_DISABLE:
		ret = GPIOF_UNUSED;
		break;
	case PIN_FUNCTION_GPIO:
		switch ((value >> GEN_IOE_SHIFT) & GEN_IOE_MASK) {
		case GEN_IOE_IE:
			ret = GPIOF_INPUT;
			break;
		case GEN_IOE_OE:
			ret = GPIOF_OUTPUT;
			break;
		default:
			ret = GPIOF_UNKNOWN;
			break;
		}
		break;
	default:
		ret = GPIOF_FUNC;
		break;
	}

	return ret;
}

static int aic_gpio_direction_input(struct udevice *dev, unsigned int off)
{
	u32 value;
	void __iomem *reg;
	struct aic_gpio_priv *priv = dev_get_priv(dev);

	if (!(priv->gpio_range & (1 << off))) {
		pr_err("%s: pin (off=%u) is invalid!\n", __func__, off);
		return -EINVAL;
	}

	/* switch function to general input/output, and enable input */
	reg = priv->base + priv->bank->regs.pin_cfg + off*4;
	value = readl(reg);
	value &= ~((PIN_FUNCTION_MASK << PIN_FUNCTION_SHIFT) |
		   (GEN_IOE_MASK << GEN_IOE_SHIFT));
	value |= (PIN_FUNCTION_GPIO << PIN_FUNCTION_SHIFT) |
		 (GEN_IOE_IE << GEN_IOE_SHIFT);
	writel(value, reg);
	return 0;
}

static int aic_gpio_direction_output(struct udevice *dev,
				     unsigned int off, int val)
{
	u32 value;
	void __iomem *reg;
	struct aic_gpio_priv *priv = dev_get_priv(dev);

	if (!(priv->gpio_range & (1 << off))) {
		pr_err("%s: pin (off=%u) is invalid!\n", __func__, off);
		return -EINVAL;
	}

	/* set output value */
	aic_gpio_set(dev, off, val);

	/* switch function to general input/output, and enable output */
	reg = priv->base + priv->bank->regs.pin_cfg + off*4;
	value = readl(reg);
	value &= ~((PIN_FUNCTION_MASK << PIN_FUNCTION_SHIFT) |
		   (GEN_IOE_MASK << GEN_IOE_SHIFT));
	value |= (PIN_FUNCTION_GPIO << PIN_FUNCTION_SHIFT) |
		 (GEN_IOE_OE << GEN_IOE_SHIFT);
	writel(value, reg);
	return 0;
}

static int aic_gpio_request(struct udevice *dev, unsigned int off,
			    const char *label)
{
	u32 value;
	void __iomem *reg;
	struct aic_gpio_priv *priv = dev_get_priv(dev);

	if (!(priv->gpio_range & (1 << off))) {
		pr_err("%s: pin (off=%u) is invalid!\n", __func__, off);
		return -EINVAL;
	}
	/* switch function to general input/output */
	reg = priv->base + priv->bank->regs.pin_cfg + off*4;
	value = readl(reg);
	value &= ~PIN_FUNCTION_MASK;
	value |= PIN_FUNCTION_GPIO;
	writel(value, reg);
	return 0;
}

static int aic_gpio_free(struct udevice *dev, unsigned int off)
{
	u32 value;
	void __iomem *reg;
	struct aic_gpio_priv *priv = dev_get_priv(dev);

	if (!(priv->gpio_range & (1 << off))) {
		pr_err("%s: pin (off=%u) is invalid!\n", __func__, off);
		return -EINVAL;
	}

	/* disable gpio function */
	reg = priv->base + priv->bank->regs.pin_cfg + off*4;
	value = readl(reg);
	value &= ~PIN_FUNCTION_MASK;
	writel(value, reg);
	return 0;
}

static const struct dm_gpio_ops aic_gpio_ops = {
	.request = aic_gpio_request,
	.rfree = aic_gpio_free,
	.direction_input = aic_gpio_direction_input,
	.direction_output = aic_gpio_direction_output,
	.get_value = aic_gpio_get,
	.set_value = aic_gpio_set,
	.get_function = aic_gpio_get_function,
};

static int aic_gpio_probe(struct udevice *dev)
{
	int ret;
	const char *name;
	struct ofnode_phandle_args args;
	struct aic_gpio_priv *priv = dev_get_priv(dev);
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	struct aic_pinctrl_priv *p_priv = dev_get_priv(dev->parent);

	priv->base = p_priv->base;

	priv->index = dev_read_u32_default(dev, "artinchip,bank-port", -1);
	if (priv->index == -1) {
		pr_err("%s: artinchip,bank-port not defined!\n", __func__);
		return -EINVAL;
	}

	/* get gpio bank name */
	name = dev_read_string(dev, "artinchip,bank-name");
	if (!name) {
		pr_err("%s: artinchip,bank-name not defined!\n", __func__);
		return -EINVAL;
	}
	uc_priv->bank_name = name;

	/* get gpio bank range */
	if (dev_read_phandle_with_args(dev, "gpio-ranges", NULL, 3, 0, &args)) {
		uc_priv->gpio_count = AIC_GPIOS_PER_BANK;
		priv->gpio_range = GENMASK(AIC_GPIOS_PER_BANK - 1, 0);
	} else {
		priv->gpio_range |= GENMASK(args.args[2] + args.args[0] - 1,
					args.args[0]);
		uc_priv->gpio_count = args.args[2];
	}
	priv->bank = &p_priv->banks[priv->index];

	/* get gpio register */
	ret = dev_read_u32_array(dev, "gpio_regs", (u32 *)&priv->bank->regs,
				sizeof(priv->bank->regs)/sizeof(u32));
	if (ret) {
		pr_err("%s: try to get gpio regs failed!\n", __func__);
		return ret;
	}

	priv->bank->dev = dev;
	priv->bank->offset = args.args[1];
	priv->bank->count = uc_priv->gpio_count;
	p_priv->gpio_cnt += uc_priv->gpio_count;

	pr_info("%s: bank_name:%s, index:%d, gpio_cnt:%d\n", __func__, name,
		priv->index, uc_priv->gpio_count);

	return 0;
}

U_BOOT_DRIVER(aic_gpio) = {
	.name = "aic_gpio",
	.id	= UCLASS_GPIO,
	.probe = aic_gpio_probe,
	.ops = &aic_gpio_ops,
	.flags	= DM_UC_FLAG_SEQ_ALIAS | DM_FLAG_PRE_RELOC,
	.priv_auto = sizeof(struct aic_gpio_priv),
};

