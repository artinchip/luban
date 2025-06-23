// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2020 Artinchip Inc.
 */

#include <linux/gpio/driver.h>
#include <linux/hwspinlock.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/platform_device.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/slab.h>
#include <dt-bindings/pinctrl/artinchip.h>

#include "../core.h"
#include "../pinconf.h"
#include "../pinctrl-utils.h"
#include "pinctrl-aic.h"

#ifndef CONFIG_PINCTRL_ARTINCHIP_V1
#define GEN_IRQ_SHIFT				(20)
#else
#define GEN_IRQ_SHIFT				(12)
#endif
#define GEN_IRQ_MASK				(7)
#define GEN_IRQ_FALLING				(0)
#define GEN_IRQ_RISING				(1)
#define GEN_IRQ_LOW				(2)
#define GEN_IRQ_HIGH				(3)
#define GEN_IRQ_EDGE				(4)

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

#ifdef CONFIG_PINCTRL_ARTINCHIP_V1
#define GEN_IN_DB0_POINT_SHIFT			(20)
#define GEN_IN_DB0_POINT_MASK			(0xF)

#define GEN_IN_DB1_SAMP_SHIFT			(24)
#define GEN_IN_DB1_SAMP_MASK			(0xF)

#define GEN_IN_DB1_POINT_SHIFT			(28)
#define GEN_IN_DB1_POINT_MASK			(0xF)
#endif

static void aic_gpio_set_reg(void __iomem *reg, u32 val,
					u32 mask, u32 shift)
{
	u32	value;

	value = readl(reg);
	value &= ~(mask << shift);
	value |= (val & mask) << shift;
	writel(value, reg);
}

static u32 aic_gpio_get_reg(void __iomem *reg, u32 mask, u32 shift)
{
	return (readl(reg) >> shift) & mask;
}

static int aic_pconf_group_get(struct pinctrl_dev *pctldev,
				 unsigned int group,
				 unsigned long *config)
{
	struct aic_pinctrl	*pctl = pinctrl_dev_get_drvdata(pctldev);

	*config = pctl->groups[group].config;

	return 0;
}

static int aic_pconf_parse_conf(struct pinctrl_dev *pctldev,
		unsigned int pin, enum pin_config_param param, u32 arg)
{
	void *reg;
	int offset, ret = 0, freq;
	struct aic_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	struct pinctrl_gpio_range *range;
	struct aic_gpio_bank *bank;
#ifdef CONFIG_PINCTRL_ARTINCHIP_V1
	u32 tap_resolution, tap_point;
	#define MAX_SAMP_POINT	15
	#define MAX_TAP_POINT	15
#endif

	range = pinctrl_find_gpio_range_from_pin_nolock(pctldev, pin);
	if (!range) {
		dev_err(pctl->dev, "No gpio range defined.\n");
		return -EINVAL;
	}

	bank = gpiochip_get_data(range->gc);
	offset = pin - range->pin_base;
	reg = bank->pctl->base + bank->regs.pin_cfg + offset * 4;

	switch (param) {
	case PIN_CONFIG_BIAS_PULL_UP:
		aic_gpio_set_reg(reg, PIN_PULL_UP, PIN_PULL_MASK, PIN_PULL_SHIFT);
		break;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		aic_gpio_set_reg(reg, PIN_PULL_DOWN, PIN_PULL_MASK, PIN_PULL_SHIFT);
		break;
	case PIN_CONFIG_BIAS_DISABLE:
		aic_gpio_set_reg(reg, PIN_PULL_DISABLE, PIN_PULL_MASK, PIN_PULL_SHIFT);
		break;
	case PIN_CONFIG_DRIVE_STRENGTH:
		if (arg > PIN_DRIVE_MAX) {
			dev_err(pctl->dev,
				"%s: drive strength invalid!\n", __func__);
			ret = -EINVAL;
			break;
		}
		aic_gpio_set_reg(reg, arg, PIN_DRIVE_MASK, PIN_DRIVE_SHIFT);
		break;
	case PIN_CONFIG_INPUT_ENABLE:
		aic_gpio_set_reg(reg, GEN_IOE_IE, GEN_IOE_MASK, GEN_IOE_SHIFT);
		break;
	case PIN_CONFIG_OUTPUT_ENABLE:
		aic_gpio_set_reg(reg, GEN_IOE_OE, GEN_IOE_MASK, GEN_IOE_SHIFT);
		break;
	case PIN_CONFIG_OUTPUT:
		reg = bank->pctl->base + bank->regs.dat_out;
		aic_gpio_set_reg(reg, arg, 0x01, offset);
		break;
	case PIN_CONFIG_INPUT_DEBOUNCE:
#ifndef CONFIG_PINCTRL_ARTINCHIP_V1
		/* arg is debounce time, unit is microsecond */
		freq = clk_get_rate(pctl->clk);
		arg = freq / 1000 * arg;
		reg = bank->pctl->base + bank->regs.samp;
		aic_gpio_set_reg(reg, arg, 0xFFFFFFF, 0);
#else
		freq = clk_get_rate(pctl->clk);
		/* tap_resolution indicates maximum
		 * sampling period in each tap pointer
		 */
		tap_resolution = (1 << (MAX_SAMP_POINT + 1)) / (freq / 1000000);
		tap_point = DIV_ROUND_UP(arg, tap_resolution);
		if (tap_point > MAX_TAP_POINT)
			tap_point = MAX_TAP_POINT;
		aic_gpio_set_reg(reg, (tap_point << 4) | MAX_SAMP_POINT,
			0xFF, GEN_IN_DB1_SAMP_SHIFT);
#endif
		break;
	default:
		ret = -ENOTSUPP;
		break;
	}

	return ret;
}

static int aic_pconf_group_set(struct pinctrl_dev *pctldev,
				unsigned int group, unsigned long *configs,
				unsigned int num_configs)
{
	int	i, ret;
	struct aic_pinctrl	*pctl = pinctrl_dev_get_drvdata(pctldev);
	struct aic_pinctrl_group	*grp = &pctl->groups[group];

	for (i = 0; i < num_configs; i++) {
		ret = aic_pconf_parse_conf(pctldev, grp->pin,
			pinconf_to_config_param(configs[i]),
			pinconf_to_config_argument(configs[i]));
		if (ret < 0)
			return ret;

		grp->config = configs[i];
	}

	return 0;
}

static int aic_pconf_pin_set(struct pinctrl_dev *pctldev, unsigned int pin,
			       unsigned long *configs, unsigned int num_configs)
{
	return aic_pconf_group_set(pctldev, pin, configs, num_configs);
}

static void aic_pconf_dbg_show(struct pinctrl_dev *pctldev,
				 struct seq_file *s,
				 unsigned int pin)
{
	int offset;
	u32	mux, pull, drive, data;
	void __iomem *reg;
	struct aic_pinctrl	*pctl = pinctrl_dev_get_drvdata(pctldev);
	struct pinctrl_gpio_range	*range;
	struct aic_gpio_bank	*bank;
	static const char * const pull_up_dn[] = {"", "", "pullup", "pulldn"};

	range = pinctrl_find_gpio_range_from_pin_nolock(pctldev, pin);
	if (!range)
		return;

	offset = pin - range->pin_base;
	bank = gpiochip_get_data(range->gc);
	reg = pctl->base + bank->regs.pin_cfg + offset*4;
	mux = aic_gpio_get_reg(reg, PIN_FUNCTION_MASK, PIN_FUNCTION_SHIFT);
	pull = aic_gpio_get_reg(reg, PIN_PULL_MASK, PIN_PULL_SHIFT);
	drive = aic_gpio_get_reg(reg, PIN_DRIVE_MASK, PIN_DRIVE_SHIFT);

	if (mux == 0) {
		seq_puts(s, "disable");
	} else if (mux == 1) {
		data = aic_gpio_get_reg(reg, GEN_IOE_MASK, GEN_IOE_SHIFT);
		switch (data) {
		case 0:
			seq_puts(s, "gpio - disable");
			break;

		case 1:
			seq_printf(s, "gpio - input - %d",
			aic_gpio_get_reg(pctl->base + bank->regs.dat_in,
						0x01, offset));
			break;

		case 2:
			seq_printf(s, "gpio - output - %d",
			aic_gpio_get_reg(pctl->base + bank->regs.dat_out,
						0x01, offset));
			break;

		default:
			seq_puts(s, "gpio - in/out conflict");
			break;
		}
	} else {
		seq_printf(s, "func(%d) - drive(%d) - %s", mux, drive,
				pull_up_dn[pull]);
	}
}

static const struct pinconf_ops aic_pconf_ops = {
	.pin_config_group_get	= aic_pconf_group_get,
	.pin_config_group_set	= aic_pconf_group_set,
	.pin_config_set = aic_pconf_pin_set,
	.pin_config_dbg_show	= aic_pconf_dbg_show,
};

static const char *aic_pctrl_get_function_name
		(struct aic_pinctrl *pctl, u32 pin_num, u32 func_num)
{
	int i;
	const struct aic_desc_pin *pin;
	const struct aic_desc_function *func;

	for (i = 0; i < pctl->npins; i++) {
		pin = pctl->pins + i;
		if (pin->pin.number != pin_num)
			continue;

		func = pin->functions;
		while (func && func->name) {
			if (func->num == func_num)
				return func->name;
			func++;
		}

		break;
	}

	return NULL;
}

static struct aic_pinctrl_group *aic_pctrl_find_group_by_pin
					(struct aic_pinctrl *pctl, u32 pin)
{
	int i;
	struct aic_pinctrl_group *group;

	for (i = 0; i < pctl->ngroups; i++) {
		group = pctl->groups + i;
		if (group->pin == pin)
			return group;
	}

	return NULL;
}

static int aic_pctrl_dt_node_to_map_func
		(struct aic_pinctrl *pctl, u32 pin, const char *func_name,
		struct aic_pinctrl_group *group, struct pinctrl_map **map,
		unsigned int *reserved_maps, unsigned int *num_maps)
{
	/* check if any space left */
	if (*num_maps >= *reserved_maps)
		return -ENOSPC;

	(*map)[*num_maps].type = PIN_MAP_TYPE_MUX_GROUP;
	(*map)[*num_maps].data.mux.group = group->name;
	(*map)[*num_maps].data.mux.function = func_name;
	(*num_maps)++;

	return 0;
}


static int aic_pctrl_dt_subnode_to_map(struct pinctrl_dev *pctldev,
				      struct device_node *node,
				      struct pinctrl_map **map,
				      u32 *reserved_maps, u32 *num_maps)
{
	int i, num_pins, num_funcs, maps_per_pin, ret;
	struct aic_pinctrl *pctl;
	struct aic_pinctrl_group *group;
	struct pinctrl_gpio_range *range;
	struct property *pins;
	u32 pinmux, port, pin, func;
	unsigned long *configs;
	unsigned int num_configs;
	unsigned int reserve = 0;
	const char *func_name;

	pctl = pinctrl_dev_get_drvdata(pctldev);

	/* try get 'pinumx' array */
	pins = of_find_property(node, "pinmux", NULL);
	if (!pins) {
		dev_err(pctl->dev, "missing pins property in node %p.\n", node);
		return -EINVAL;
	}
	num_pins = pins->length / sizeof(u32);
	if (!num_pins) {
		dev_err(pctl->dev, "none pinmux defined!\n");
		return -EINVAL;
	}
	num_funcs = num_pins;

	/*
	 * parse generic configs, such as "bias-pull-up",
	 * "drive-strength", for ex.
	 */
	ret = pinconf_generic_parse_dt_config(node, pctldev,
				&configs, &num_configs);
	if (ret) {
		dev_err(pctl->dev, "pinconf parse failed!\n");
		return ret;
	}

	maps_per_pin = 0;
	if (num_funcs)
		maps_per_pin++;
	if (num_configs)
		maps_per_pin++;

	reserve = num_pins * maps_per_pin;

	ret = pinctrl_utils_reserve_map(pctldev, map,
			reserved_maps, num_maps, reserve);
	if (ret) {
		dev_err(pctl->dev, "process map area failed!\n");
		goto exit;
	}

	for (i = 0; i < num_pins; i++) {
		ret = of_property_read_u32_index(node, "pinmux",
					i, &pinmux);
		if (ret) {
			dev_err(pctl->dev, "read node failed!\n");
			goto exit;
		}

		port = AIC_PINCTL_GET_PORT(pinmux);
		pin = AIC_PINCTL_GET_PIN(pinmux);
		func = AIC_PINCTL_GET_FUNC(pinmux);

		/* translate pin id in gpio bank to global id */
		range = &pctl->banks[port].range;
		pin += range->pin_base;

		func_name = aic_pctrl_get_function_name(pctl, pin, func);
		if (!func_name) {
			dev_err(pctl->dev, "invalid pin function.\n");
			ret = -EINVAL;
			goto exit;
		}

		group = aic_pctrl_find_group_by_pin(pctl, pin);
		if (!group) {
			dev_err(pctl->dev,
				"unable to match pin %d to group\n", pin);
			ret = -EINVAL;
			goto exit;
		}

		ret = aic_pctrl_dt_node_to_map_func(pctl, pin, func_name,
					group, map, reserved_maps, num_maps);
		if (ret) {
			dev_err(pctl->dev, "add mux group failed!\n");
			goto exit;
		}

		if (!!num_configs) {
			ret = pinctrl_utils_add_map_configs(pctldev, map,
					reserved_maps, num_maps, group->name,
					configs, num_configs,
					PIN_MAP_TYPE_CONFIGS_GROUP);
			if (ret) {
				dev_err(pctl->dev,
					"add config group failed!\n");
				goto exit;
			}
		}
	}

exit:
	kfree(configs);
	return ret;
}

static int aic_pctrl_dt_node_to_map(struct pinctrl_dev *pctldev,
				struct device_node *node,
				struct pinctrl_map **map,
				unsigned int *num_maps)
{
	int ret;
	struct device_node *np;
	unsigned int reserved_maps;

	*map = NULL;
	*num_maps = 0;
	reserved_maps = 0;

	/*
	 * node is a pinc config for a device, such as
	 * 'uart0_pins_a: uart0@0 {}' for ex.
	 */
	for_each_child_of_node(node, np) {
		/* parse the child node, such as 'pins {}' for ex. */
		ret = aic_pctrl_dt_subnode_to_map(pctldev, np,
					map, &reserved_maps, num_maps);
		if (ret < 0) {
			dev_err(pctldev->dev, "create pin map failed!\n");
			pinctrl_utils_free_map(pctldev, *map, *num_maps);
			of_node_put(np);
			return ret;
		}
	}

	return 0;
}

static void aic_pctrl_dt_free_map(struct pinctrl_dev *pctldev,
				struct pinctrl_map *map, unsigned int num_maps)
{
	int i;

	for (i = 0; i < num_maps; i++) {
		if (map[i].type == PIN_MAP_TYPE_CONFIGS_GROUP)
			kfree(map[i].data.configs.configs);
	}

	kfree(map);
}

static int aic_pctrl_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct aic_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return pctl->ngroups;
}

static const char *aic_pctrl_get_group_name
			(struct pinctrl_dev *pctldev, unsigned int group)
{
	struct aic_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return pctl->groups[group].name;
}

static int aic_pctrl_get_group_pins(struct pinctrl_dev *pctldev,
					unsigned int group,
					const unsigned int **pins,
					unsigned int *num_pins)
{
	struct aic_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	*pins = (unsigned int *)&pctl->groups[group].pin;
	*num_pins = 1;

	return 0;
}

static const struct pinctrl_ops aic_pctrl_ops = {
	.dt_node_to_map		= aic_pctrl_dt_node_to_map,
	.dt_free_map		= aic_pctrl_dt_free_map,
	.get_groups_count	= aic_pctrl_get_groups_count,
	.get_group_name		= aic_pctrl_get_group_name,
	.get_group_pins		= aic_pctrl_get_group_pins,
};

static int aic_pmx_get_funcs_cnt(struct pinctrl_dev *pctldev)
{
	struct aic_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return pctl->nfunctions;
}

static const char *aic_pmx_get_func_name
		(struct pinctrl_dev *pctldev, unsigned int function)
{
	struct aic_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return pctl->functions[function].name;
}

static int aic_pmx_get_func_groups(struct pinctrl_dev *pctldev,
				     unsigned int function,
				     const char * const **groups,
				     unsigned * const num_groups)
{
	struct aic_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	*groups = pctl->functions[function].groups;
	*num_groups = pctl->functions[function].ngroups;

	return 0;
}

static int aic_pmx_set_mux(struct pinctrl_dev *pctldev,
			     unsigned int function, unsigned int group)
{
	int	mux;
	u32	offset;
	void __iomem *reg;
	const char *func_name;
	struct pinctrl_gpio_range	*range;
	struct aic_gpio_bank	*bank;
	struct aic_pinctrl	*pctl = pinctrl_dev_get_drvdata(pctldev);
	struct aic_pinctrl_group	*grp = pctl->groups + group;
	struct aic_pinctrl_function	*func;
	const struct aic_desc_function	*func_desc;
	struct aic_desc_pin	*pin = pctl->pins + grp->pin;

	/* get gpio bank */
	range = pinctrl_find_gpio_range_from_pin(pctldev, grp->pin);
	if (!range) {
		dev_err(pctl->dev, "No gpio range defined.\n");
		return -EINVAL;
	}
	bank = gpiochip_get_data(range->gc);

	func = pctl->functions + function;
	func_name = func->name;

	/* get function mux */
	mux = -1;
	for (func_desc = pin->functions; func_desc->name; func_desc++) {
		if (!strcmp(func_name, func_desc->name)) {
			mux = func_desc->num;
			break;
		}
	}
	if (mux < 0) {
		dev_err(pctl->dev, "%s: pin function invalid!\n", __func__);
		return -EINVAL;
	}

	offset = grp->pin - range->pin_base;
	reg = pctl->base + bank->regs.pin_cfg + offset * 4;
	aic_gpio_set_reg(reg, mux, PIN_FUNCTION_MASK, PIN_FUNCTION_SHIFT);

	return 0;
}

static int aic_pmx_gpio_set_direction(struct pinctrl_dev *pctldev,
			struct pinctrl_gpio_range *range,
			unsigned int gpio, bool input)
{
	u32	offset;
	void __iomem *reg;
	struct aic_gpio_bank *bank = gpiochip_get_data(range->gc);

	offset = gpio - range->pin_base;
	reg = bank->pctl->base + bank->regs.pin_cfg + offset * 4;
	aic_gpio_set_reg(reg, PIN_FUNCTION_GPIO, PIN_FUNCTION_MASK,
			PIN_FUNCTION_SHIFT);
	if (input)
		aic_gpio_set_reg(reg, GEN_IOE_IE, GEN_IOE_MASK, GEN_IOE_SHIFT);
	else
		aic_gpio_set_reg(reg, GEN_IOE_OE, GEN_IOE_MASK, GEN_IOE_SHIFT);

	return 0;
}

static const struct pinmux_ops aic_pmx_ops = {
	.get_functions_count	= aic_pmx_get_funcs_cnt,
	.get_function_name		= aic_pmx_get_func_name,
	.get_function_groups	= aic_pmx_get_func_groups,
	.set_mux				= aic_pmx_set_mux,
	.gpio_set_direction		= aic_pmx_gpio_set_direction,
	.strict					= true,
};

static void aic_pinctrl_add_function(struct aic_pinctrl *pctl,
					const char *name)
{
	struct aic_pinctrl_function *func = pctl->functions;

	while (func->name) {
		/* function already there */
		if (strcmp(func->name, name) == 0) {
			func->ngroups++;
			return;
		}
		func++;
	}

	func->name = name;
	func->ngroups = 1;

	pctl->nfunctions++;
}

static struct aic_pinctrl_function *
aic_pinctrl_find_function_by_name(struct aic_pinctrl *pctl,
					const char *name)
{
	int		i;
	struct aic_pinctrl_function *func = pctl->functions;

	for (i = 0; i < pctl->nfunctions; i++) {
		if (!func[i].name)
			break;

		if (!strcmp(func[i].name, name))
			return func + i;
	}

	return NULL;
}

static int aic_pctrl_build_state(struct platform_device *pdev)
{
	int		i;
	void	*ptr;
	const char **func_group;
	const struct aic_desc_pin	*pin;
	struct aic_pinctrl_group	*group;
	struct aic_pinctrl_function	*func;
	const struct aic_desc_function	*func_desc;
	struct aic_pinctrl *pctl = platform_get_drvdata(pdev);

	/* assume that one pin is one group */
	pctl->ngroups = pctl->npins;
	pctl->groups = devm_kcalloc(&pdev->dev, pctl->ngroups,
				    sizeof(*pctl->groups), GFP_KERNEL);
	if (!pctl->groups)
		return -ENOMEM;

	pctl->grp_names = devm_kcalloc(&pdev->dev, pctl->ngroups,
				       sizeof(*pctl->grp_names), GFP_KERNEL);
	if (!pctl->grp_names)
		return -ENOMEM;

	pctl->functions = devm_kcalloc(&pdev->dev,
			pctl->npins * AIC_FUNCS_PER_PIN,
			sizeof(*pctl->functions), GFP_KERNEL);
	if (!pctl->functions) {
		dev_err(&pdev->dev, "%s: malloc failed!\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < pctl->npins; i++) {
		pin = pctl->pins + i;
		group = pctl->groups + i;

		group->name = pin->pin.name;
		group->pin = pin->pin.number;
		pctl->grp_names[i] = pin->pin.name;

		/* build temporary function list */
		for (func_desc = pin->functions; func_desc->name; func_desc++)
			aic_pinctrl_add_function(pctl, func_desc->name);
	}

	/*
	 * realloc the functions buffer, it should be much less
	 * than the original buffer.
	 */
	ptr = krealloc(pctl->functions,
				pctl->nfunctions * sizeof(*pctl->functions),
				GFP_KERNEL);
	if (!ptr) {
		dev_err(&pdev->dev, "%s: malloc failed!\n", __func__);
		kfree(pctl->functions);
		pctl->functions = NULL;
		return -ENOMEM;
	}
	pctl->functions = ptr;

	/* build the real function list */
	for (i = 0; i < pctl->npins; i++) {
		pin = pctl->pins + i;
		for (func_desc = pin->functions; func_desc->name; func_desc++) {
			func = aic_pinctrl_find_function_by_name(pctl,
							func_desc->name);
			if (!func) {
				dev_err(&pdev->dev,
					"%s: look for function failed!\n",
					__func__);
				kfree(pctl->functions);
				return -EINVAL;
			}

			/* check if group list of the function exist */
			if (!func->groups) {
				func->groups = devm_kcalloc(&pdev->dev,
							func->ngroups,
							sizeof(*func->groups),
							GFP_KERNEL);
				if (!func->groups) {
					kfree(pctl->functions);
					dev_err(&pdev->dev,
						"%s: malloc failed!\n",
						__func__);
					return -ENOMEM;
				}
			}

			func_group = func->groups;
			while (*func_group)
				func_group++;
			*func_group = pin->pin.name;
		}
	}

	return 0;
}

static int aic_gpio_request(struct gpio_chip *chip,
					unsigned int offset)
{
	return pinctrl_gpio_request(chip->base + offset);
}

static void aic_gpio_free(struct gpio_chip *chip, unsigned int offset)
{
	pinctrl_gpio_free(chip->base + offset);
}

static int aic_gpio_get_direction(struct gpio_chip *chip,
						unsigned int offset)
{
	void __iomem *reg;
	u32 dir;
	struct aic_gpio_bank *bank = gpiochip_get_data(chip);

	reg = bank->pctl->base + bank->regs.pin_cfg + offset * 4;
	dir = aic_gpio_get_reg(reg, GEN_IOE_MASK, GEN_IOE_SHIFT);

	if (dir == GEN_IOE_MASK)
		return -EINVAL;

	if (dir == GEN_IOE_IE)
		return 1;
	else
		return 0;
}

static int aic_gpio_get(struct gpio_chip *chip, unsigned int offset)
{
	void __iomem *reg;
	struct aic_gpio_bank *bank = gpiochip_get_data(chip);

	if (aic_gpio_get_direction(chip, offset) == 1)
		reg = bank->pctl->base + bank->regs.dat_in;
	else if (aic_gpio_get_direction(chip, offset) == 0)
		reg = bank->pctl->base + bank->regs.dat_out;
	else
		return -EINVAL;
	return aic_gpio_get_reg(reg, 1, offset);
}

static void aic_gpio_set(struct gpio_chip *chip,
				unsigned int offset, int value)
{
	void __iomem *reg;
	struct aic_gpio_bank *bank = gpiochip_get_data(chip);

	reg = bank->pctl->base + bank->regs.dat_out;
	aic_gpio_set_reg(reg, !!value, 1, offset);
}

static int aic_gpio_direction_input(struct gpio_chip *chip,
						unsigned int offset)
{
	return pinctrl_gpio_direction_input(chip->base + offset);
}

static int aic_gpio_direction_output(struct gpio_chip *chip,
				unsigned int offset, int value)
{
	aic_gpio_set(chip, offset, value);
	return pinctrl_gpio_direction_output(chip->base + offset);
}

static int aic_gpio_to_irq(struct gpio_chip *chip, unsigned int offset)
{
	struct aic_gpio_bank *bank = gpiochip_get_data(chip);

	return irq_find_mapping(bank->domain, offset);
}

static const struct gpio_chip aic_gpio_chip = {
	.request		= aic_gpio_request,
	.free			= aic_gpio_free,
	.get			= aic_gpio_get,
	.set			= aic_gpio_set,
	.direction_input	= aic_gpio_direction_input,
	.direction_output	= aic_gpio_direction_output,
	.to_irq				= aic_gpio_to_irq,
	.get_direction		= aic_gpio_get_direction,
	.set_config		= gpiochip_generic_config,
};

static int aic_gpio_irq_set_type(struct irq_data *d, unsigned int type)
{
	int	ret;
	void *reg;
	u32 offset = d->hwirq;
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	struct aic_gpio_bank *bank = gc->private;

	/* make sure the pin is configured as gpio input */
	ret = aic_gpio_direction_input(&bank->gpio_chip, offset);
	if (ret < 0)
		return ret;

	reg = bank->pctl->base + bank->regs.pin_cfg + offset * sizeof(u32);

	switch (type) {
	case IRQ_TYPE_EDGE_BOTH:
		irq_set_handler_locked(d, handle_edge_irq);
		aic_gpio_set_reg(reg, GEN_IRQ_EDGE,
				GEN_IRQ_MASK, GEN_IRQ_SHIFT);
		break;

	case IRQ_TYPE_EDGE_RISING:
		irq_set_handler_locked(d, handle_edge_irq);
		aic_gpio_set_reg(reg, GEN_IRQ_RISING,
				GEN_IRQ_MASK, GEN_IRQ_SHIFT);
		break;

	case IRQ_TYPE_EDGE_FALLING:
		irq_set_handler_locked(d, handle_edge_irq);
		aic_gpio_set_reg(reg, GEN_IRQ_FALLING,
				GEN_IRQ_MASK, GEN_IRQ_SHIFT);
		break;

	case IRQ_TYPE_LEVEL_HIGH:
		irq_set_handler_locked(d, handle_level_irq);
		aic_gpio_set_reg(reg, GEN_IRQ_HIGH,
				GEN_IRQ_MASK, GEN_IRQ_SHIFT);
		break;

	case IRQ_TYPE_LEVEL_LOW:
		irq_set_handler_locked(d, handle_level_irq);
		aic_gpio_set_reg(reg, GEN_IRQ_LOW,
				GEN_IRQ_MASK, GEN_IRQ_SHIFT);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static void aic_gpio_irq_mask(struct irq_data *d)
{
	u32 irq_en;
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	struct aic_gpio_bank *bank = gc->private;

	irq_en = irq_reg_readl(gc, bank->regs.irq_en);
	irq_reg_writel(gc, irq_en & ~(1 << d->hwirq), bank->regs.irq_en);
}

static void aic_gpio_irq_unmask(struct irq_data *d)
{
	u32 irq_en;
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	struct aic_gpio_bank *bank = gc->private;

	irq_en = irq_reg_readl(gc, bank->regs.irq_en);
	irq_reg_writel(gc, irq_en | (1 << d->hwirq), bank->regs.irq_en);
}

static void aic_gpio_irq_ack(struct irq_data *d)
{
	u32 irq_sta;
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	struct aic_gpio_bank *bank = gc->private;

	irq_sta = irq_reg_readl(gc, bank->regs.irq_sta);
	irq_reg_writel(gc, irq_sta | (1 << d->hwirq), bank->regs.irq_sta);
}

static void aic_gpio_irq_suspend(struct irq_data *d)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	struct aic_gpio_bank *bank = gc->private;

	bank->saved_mask = irq_reg_readl(gc, bank->regs.irq_en);
	irq_reg_writel(gc, gc->wake_active, bank->regs.irq_en);
}

static void aic_gpio_irq_resume(struct irq_data *d)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	struct aic_gpio_bank *bank = gc->private;

	irq_reg_writel(gc, bank->saved_mask, bank->regs.irq_en);
}

static void aic_gpio_irq_demux(struct irq_desc *desc)
{
	u32 pend, irq, virq;
	void __iomem *reg;
	struct irq_chip *chip = irq_desc_get_chip(desc);
	struct aic_gpio_bank *bank = irq_desc_get_handler_data(desc);

	dev_dbg(bank->pctl->dev, "Got irq for bank %d\n", bank->bank_nr);

	chained_irq_enter(chip, desc);

	reg = bank->pctl->base + bank->regs.irq_sta;
	pend = readl(reg);

	while (pend) {
		irq = __ffs(pend);
		pend &= ~BIT(irq);
		virq = irq_linear_revmap(bank->domain, irq);

		if (!virq) {
			dev_err(bank->pctl->dev, "unmapped irq %d\n", irq);
			continue;
		}

		dev_dbg(bank->pctl->dev, "handling irq %d\n", irq);
		generic_handle_irq(virq);
	}

	chained_irq_exit(chip, desc);
}

static int aic_gpiolib_register_bank(struct aic_pinctrl *pctl,
					struct device_node *np)
{
	int i, irqno, bank_nr, ret;
	struct aic_gpio_bank *bank;
	struct pinctrl_gpio_range *range;
	struct of_phandle_args args;
	struct device *dev = pctl->dev;
	struct irq_chip_generic *gc;

	ret = of_property_read_u32(np, "artinchip,bank-port", &bank_nr);
	if (ret) {
		dev_warn(dev, "artinchip,bank-port not defined in dts!\n");
		bank_nr = pctl->nbanks;
	}
	bank = &pctl->banks[bank_nr];
	range = &bank->range;
	bank->gpio_chip = aic_gpio_chip;
	bank->pctl = pctl;

	ret = of_property_read_string(np, "artinchip,bank-name",
					&bank->gpio_chip.label);
	if (ret) {
		dev_warn(dev, "artinchip,bank-name not define!\n");
		sprintf((char *)bank->gpio_chip.label, "P%c", 'A' + bank_nr);
	}

	ret = of_parse_phandle_with_fixed_args(np, "gpio-ranges", 3, 0, &args);
	if (!ret) {
		bank->gpio_chip.base = args.args[1];
		range->base = args.args[0];
		range->npins = args.args[2];
	} else {
		bank->gpio_chip.base = bank_nr * AIC_GPIO_PINS_PER_BANK;
		range->base = 0;
		range->npins = AIC_GPIO_PINS_PER_BANK;
	}
	range->name = bank->gpio_chip.label;
	range->id = bank_nr;
	range->pin_base = bank->gpio_chip.base;
	range->base = range->pin_base;
	range->gc = &bank->gpio_chip;
	pinctrl_add_gpio_range(pctl->pctl_dev, &pctl->banks[bank_nr].range);

	/* get gpio bank registers */
	ret = of_property_read_variable_u32_array(np,
			       "gpio_regs", (u32 *)&bank->regs,
			       sizeof(bank->regs) / sizeof(u32),
			       sizeof(bank->regs) / sizeof(u32));
	if (ret != sizeof(bank->regs) / sizeof(u32)) {
		dev_err(dev, "try get gpio bank regs failed!\n");
		return -EINVAL;
	}

	bank->gpio_chip.ngpio = range->npins;
	bank->gpio_chip.of_node = np;
	bank->gpio_chip.parent = dev;
	bank->bank_nr = bank_nr;
	spin_lock_init(&bank->lock);

	ret = gpiochip_add_data(&bank->gpio_chip, bank);
	if (ret) {
		dev_err(dev, "gpiochip_add_data failed(%d)!\n", bank_nr);
		return ret;
	}
	dev_info(dev, "%s bank added\n", bank->gpio_chip.label);

	/* create irq domain */
	bank->domain = irq_domain_add_linear(np,
					     range->npins,
					     &irq_generic_chip_ops,
					     bank);
	if (!bank->domain) {
		dev_err(dev, "try create irq domain failed!\n");
		return -ENOMEM;
	}

	ret = irq_alloc_domain_generic_chips(bank->domain, range->npins, 1,
					"artinchip_gpio_irq", handle_level_irq,
					IRQ_NOREQUEST | IRQ_NOPROBE |
					IRQ_NOAUTOEN,
					0, IRQ_GC_INIT_MASK_CACHE);

	/* clear gpio irq pending */
	writel(0xffffffff, bank->pctl->base + bank->regs.irq_sta);

	gc = irq_get_domain_generic_chip(bank->domain, 0);
	gc->reg_base = pctl->base;
	gc->private = bank;
	gc->chip_types[0].chip.irq_mask = aic_gpio_irq_mask;
	gc->chip_types[0].chip.irq_unmask = aic_gpio_irq_unmask;
	gc->chip_types[0].chip.irq_ack = aic_gpio_irq_ack;
	gc->chip_types[0].chip.irq_set_wake = irq_gc_set_wake;
	gc->chip_types[0].chip.irq_suspend = aic_gpio_irq_suspend;
	gc->chip_types[0].chip.irq_resume = aic_gpio_irq_resume;
	gc->chip_types[0].chip.irq_set_type = aic_gpio_irq_set_type;
	gc->wake_enabled = IRQ_MSK(bank->range.npins);

	/* map gpio irqs */
	for (i = 0; i < range->npins; i++)
		irqno = irq_create_mapping(bank->domain, i);

	/* register irq for gpio bank */
	bank->irq = irq_of_parse_and_map(np, 0);
	irq_set_chained_handler_and_data(bank->irq,
						 aic_gpio_irq_demux,
						 bank);

	return 0;
}

int aic_pinctl_probe(struct platform_device *pdev)
{
	int i, ret, banks = 0;
	struct device_node *child, *np = pdev->dev.of_node;
	const struct of_device_id *match;
	struct device *dev = &pdev->dev;
	struct aic_pinctrl *pctl;
	struct pinctrl_pin_desc *pins;
	struct aic_pinctrl_match_data *match_data;
	struct resource *res;

	if (!np)
		return -EINVAL;

	match = of_match_device(dev->driver->of_match_table, dev);
	if (!match || !match->data)
		return -EINVAL;

	pctl = devm_kzalloc(dev, sizeof(*pctl), GFP_KERNEL);
	if (!pctl)
		return -ENOMEM;

	platform_set_drvdata(pdev, pctl);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pctl->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(pctl->base)) {
		dev_err(&pdev->dev, "%s: get io address failed!\n", __func__);
		return PTR_ERR(pctl->base);
	}

	pctl->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(pctl->clk)) {
		dev_err(dev, "Couldn't get clock\n");
		return PTR_ERR(pctl->clk);
	}

	pctl->reset = devm_reset_control_get(dev, NULL);
	if (IS_ERR(pctl->reset)) {
		dev_err(dev, "Couldn't get reset\n");
		return PTR_ERR(pctl->reset);
	}
	reset_control_deassert(pctl->reset);
	clk_prepare_enable(pctl->clk);

	pctl->dev = dev;
	match_data = (struct aic_pinctrl_match_data *)match->data;

	pctl->pins = match_data->pins;
	pctl->npins = match_data->npins;

	ret = aic_pctrl_build_state(pdev);
	if (ret) {
		dev_err(dev, "build state failed: %d\n", ret);
		return -EINVAL;
	}

	/* prepare <struct pinctrl_desc> for register pinctrl */
	pins = devm_kcalloc(&pdev->dev, pctl->npins, sizeof(*pins), GFP_KERNEL);
	if (!pins)
		return -ENOMEM;

	for (i = 0; i < pctl->npins; i++)
		pins[i] = pctl->pins[i].pin;

	pctl->pctl_desc.name = dev_name(&pdev->dev);
	pctl->pctl_desc.owner = THIS_MODULE;
	pctl->pctl_desc.pins = pins;
	pctl->pctl_desc.npins = pctl->npins;
	pctl->pctl_desc.link_consumers = true;
	pctl->pctl_desc.confops = &aic_pconf_ops;
	pctl->pctl_desc.pctlops = &aic_pctrl_ops;
	pctl->pctl_desc.pmxops = &aic_pmx_ops;

	pctl->pctl_dev = devm_pinctrl_register(&pdev->dev,
				&pctl->pctl_desc, pctl);
	if (IS_ERR(pctl->pctl_dev)) {
		dev_err(&pdev->dev, "Failed pinctrl registration\n");
		return PTR_ERR(pctl->pctl_dev);
	}

	/* scan node for getting gpio bank count */
	for_each_available_child_of_node(np, child)
		if (of_property_read_bool(child, "gpio-controller"))
			banks++;

	if (!banks) {
		dev_err(dev, "no gpio bank found!\n");
		return -EINVAL;
	}

	/* process gpio banks */
	pctl->banks = devm_kcalloc(dev, banks, sizeof(*pctl->banks),
			GFP_KERNEL);
	if (!pctl->banks)
		return -ENOMEM;

	for_each_available_child_of_node(np, child) {
		if (of_property_read_bool(child, "gpio-controller")) {
			ret = aic_gpiolib_register_bank(pctl, child);
			if (ret) {
				of_node_put(child);
				return ret;
			}

			pctl->nbanks++;
		}
	}

	dev_info(dev, "pinctrl-aic initialized.\n");

	return 0;
}

