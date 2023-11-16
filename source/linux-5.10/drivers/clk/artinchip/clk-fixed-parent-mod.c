// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, Artinchip Technology Co., Ltd
 */
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include "clk-aic.h"

struct clk_fixed_parent_mod {
	struct clk_hw hw;
	void __iomem *reg;
	const char *name;
	s8 bus_gate_bit;
	s8 mod_gate_bit;
	u8 div_bit;
	u8 div_mask;
#ifdef CONFIG_DEBUG_ON_FPGA_BOARD_ARTINCHIP
	unsigned long id;
#endif
};
#define to_clk_fixed_parent_mod(_hw)                                           \
	container_of(_hw, struct clk_fixed_parent_mod, hw)

static int clk_fixed_parent_mod_prepare(struct clk_hw *hw)
{
	struct clk_fixed_parent_mod *mod = to_clk_fixed_parent_mod(hw);
	u32 val;


	val = readl(mod->reg);

	if (mod->mod_gate_bit >= 0)
		val |= (1 << mod->mod_gate_bit);
	if (mod->bus_gate_bit >= 0)
		val |= (1 << mod->bus_gate_bit);

	writel(val, mod->reg);

	return 0;
}

static void clk_fixed_parent_mod_unprepare(struct clk_hw *hw)
{
	struct clk_fixed_parent_mod *mod = to_clk_fixed_parent_mod(hw);
	u32 val;

	val = readl(mod->reg);

	if (mod->mod_gate_bit >= 0)
		val &= ~(1 << mod->mod_gate_bit);
	if (mod->bus_gate_bit >= 0)
		val &= ~(1 << mod->bus_gate_bit);

	writel(val, mod->reg);
}

static int clk_fixed_parent_mod_is_prepared(struct clk_hw *hw)
{
	struct clk_fixed_parent_mod *mod = to_clk_fixed_parent_mod(hw);
	u32 val, mod_gate, bus_gate;

	val = readl(mod->reg);
	if (mod->mod_gate_bit >= 0)
		mod_gate = val & (1 << mod->mod_gate_bit);
	else
		mod_gate = 1;
	if (mod->bus_gate_bit >= 0)
		bus_gate = val & (1 << mod->bus_gate_bit);
	else
		bus_gate = 1;

	if (mod_gate && bus_gate)
		return 1;

	return 0;
}

static unsigned long clk_fixed_parent_mod_recalc_rate(struct clk_hw *hw,
						      unsigned long parent_rate)
{
	struct clk_fixed_parent_mod *mod = to_clk_fixed_parent_mod(hw);
	unsigned long rate, div;

	if (!mod->div_mask)
		return parent_rate;

	div = (readl(mod->reg) >> mod->div_bit) & mod->div_mask;
	rate = parent_rate / (div + 1);
#ifdef CONFIG_DEBUG_ON_FPGA_BOARD_ARTINCHIP
	rate = fpga_board_rate[mod->id];
#endif
	return rate;
}

static long clk_fixed_parent_mod_round_rate(struct clk_hw *hw,
					    unsigned long rate,
					    unsigned long *prate)
{
	struct clk_fixed_parent_mod *mod = to_clk_fixed_parent_mod(hw);
	unsigned long rrate, parent_rate, div;

	parent_rate = *prate;
	if (!rate || !mod->div_mask)
		return parent_rate;

	div = parent_rate / rate;
	div += (parent_rate - rate * div) > (rate / 2) ? 1 : 0;
	div = div ? div : 1;

	rrate = parent_rate / div;
#ifdef CONFIG_DEBUG_ON_FPGA_BOARD_ARTINCHIP
	rrate = fpga_board_rate[mod->id];
#endif
	return rrate;
}

static u8 clk_fixed_parent_mod_get_parent(struct clk_hw *hw)
{
	return 0;
}

static int clk_fixed_parent_mod_set_rate(struct clk_hw *hw, unsigned long rate,
					 unsigned long parent_rate)
{
	struct clk_fixed_parent_mod *mod = to_clk_fixed_parent_mod(hw);
	u32 val, div;

	if (!mod->div_mask)
		return 0;

	div = DIV_ROUND_CLOSEST(parent_rate, rate);
	if (!div)
		div = 1;

	val = readl(mod->reg);
	val &= ~(mod->div_mask << mod->div_bit);
	val |= ((div - 1) << mod->div_bit);
	writel(val, mod->reg);

	return 0;
}

static const struct clk_ops clk_fixed_parent_mod_ops = {
	.prepare	= clk_fixed_parent_mod_prepare,
	.unprepare	= clk_fixed_parent_mod_unprepare,
	.is_prepared	= clk_fixed_parent_mod_is_prepared,
	.recalc_rate	= clk_fixed_parent_mod_recalc_rate,
	.round_rate	= clk_fixed_parent_mod_round_rate,
	.get_parent	= clk_fixed_parent_mod_get_parent,
	.set_rate	= clk_fixed_parent_mod_set_rate,
};

struct clk_hw *aic_clk_hw_fixed_parent(void __iomem *base,
				       const struct fixed_parent_clk_cfg *cfg)
{
	struct clk_fixed_parent_mod *mod;
	struct clk_init_data init;
	struct clk_hw *hw;
	int ret;

	if (cfg->type == AIC_FPCLK_FIXED_FACTOR) {
		hw = aic_clk_hw_fixed_factor(cfg->name, cfg->parent_names[0],
					     cfg->fact_mult, cfg->fact_div);
		return hw;
	}

	/* Otherwise AIC_FPCLK_NORMAL */
	mod = kzalloc(sizeof(*mod), GFP_KERNEL);
	if (!mod)
		return ERR_PTR(-ENOMEM);

	mod->reg = base + cfg->offset_reg;
	mod->bus_gate_bit = cfg->bus_gate_bit;
	mod->mod_gate_bit = cfg->mod_gate_bit;
	mod->div_bit = cfg->div_bit;
	mod->div_mask = (1 << cfg->div_width) - 1;
	mod->name = cfg->name;
#ifdef CONFIG_DEBUG_ON_FPGA_BOARD_ARTINCHIP
	mod->id = cfg->id;
#endif

	init.name = cfg->name;
	init.ops = &clk_fixed_parent_mod_ops;
	init.flags = 0;
	init.parent_names = cfg->parent_names;
	init.num_parents = cfg->num_parents;

	mod->hw.init = &init;
	hw = &mod->hw;

	ret = clk_hw_register(NULL, hw);
	if (ret) {
		kfree(mod);
		return ERR_PTR(ret);
	}

	return hw;
}
