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

struct clk_multi_parent_mod {
	struct clk_hw hw;
	void __iomem *reg;
	const char *name;
	s8 gate_bit;
	u8 mux_bit;
	u8 mux_mask;
	u8 div0_bit;
	u8 div0_mask;
#ifdef CONFIG_DEBUG_ON_FPGA_BOARD_ARTINCHIP
	unsigned long id;
#endif
};

#define to_clk_multi_parent_mod(_hw)                                           \
	container_of(_hw, struct clk_multi_parent_mod, hw)

static int clk_multi_parent_mod_prepare(struct clk_hw *hw)
{
	struct clk_multi_parent_mod *mod = to_clk_multi_parent_mod(hw);
	u32 val;

	val = readl(mod->reg);

	if (mod->gate_bit >= 0)
		val |= (1 << mod->gate_bit);

	writel(val, mod->reg);

	return 0;
}

static void clk_multi_parent_mod_unprepare(struct clk_hw *hw)
{
	struct clk_multi_parent_mod *mod = to_clk_multi_parent_mod(hw);
	u32 val;

	val = readl(mod->reg);

	if (mod->gate_bit >= 0)
		val &= ~(1 << mod->gate_bit);

	writel(val, mod->reg);
}

static int clk_multi_parent_mod_is_prepared(struct clk_hw *hw)
{
	struct clk_multi_parent_mod *mod = to_clk_multi_parent_mod(hw);
	u32 val;

	val = readl(mod->reg);
	if (mod->gate_bit >= 0)
		return val & (1 << mod->gate_bit);

	return 1;
}

static unsigned long clk_multi_parent_mod_recalc_rate(struct clk_hw *hw,
						     unsigned long parent_rate)
{
	unsigned long rate, div0 = 0, parent_index = 0;
	struct clk_multi_parent_mod *mod = to_clk_multi_parent_mod(hw);

	parent_index = (readl(mod->reg) >> mod->mux_bit) & mod->mux_mask;

	if (mod->mux_mask == 7 || parent_index == 1) {
		div0 = (readl(mod->reg) >> mod->div0_bit) & mod->div0_mask;
		rate = parent_rate / (div0 + 1);
	} else
		rate = parent_rate;

#ifdef CONFIG_DEBUG_ON_FPGA_BOARD_ARTINCHIP
	rate = fpga_board_rate[mod->id];
#endif
	return rate;
}

static void try_best_divider(u32 rate, u32 parent_rate,
				u32 max_div0, u32 *div0)
{
	u32 tmp, i, min_delta = U32_MAX, best_div0 = 1;

	for (i = 1; i <= max_div0; i++) {
		tmp = i * rate;
		if (parent_rate == tmp) {
			best_div0 = i;
			goto __out;
		} else if (parent_rate < tmp) {
			best_div0 = i - 1;
			if (best_div0 <= 0)
				best_div0 = 1;
			goto __out;
		}

		if (abs(parent_rate - tmp) < min_delta) {
			min_delta = abs(parent_rate - tmp);
			best_div0 = i;
		}
	}

__out:
	*div0 = best_div0;
}

static u8 clk_multi_parent_mod_get_parent(struct clk_hw *hw)
{
	struct clk_multi_parent_mod *mod = to_clk_multi_parent_mod(hw);

	return (readl(mod->reg) >> mod->mux_bit) & mod->mux_mask;
}

static int clk_multi_parent_mod_set_parent(struct clk_hw *hw, u8 index)
{
	struct clk_multi_parent_mod *mod = to_clk_multi_parent_mod(hw);
	u32 val;

	val = readl(mod->reg);
	val &= ~(mod->mux_mask << mod->mux_bit);
	val |= index << mod->mux_bit;
	writel(val, mod->reg);

	return 0;
}

static int clk_multi_parent_mod_set_rate(struct clk_hw *hw, unsigned long rate,
					 unsigned long parent_rate)
{
	struct clk_multi_parent_mod *mod = to_clk_multi_parent_mod(hw);
	u32 val, div0, parent_index;

	val = readl(mod->reg);
	parent_index = (readl(mod->reg) >> mod->mux_bit) & mod->mux_mask;

	if (mod->mux_mask == 7 || parent_index == 1) {
		try_best_divider(rate, parent_rate, mod->div0_mask + 1, &div0);
		val &= ~(mod->div0_mask << mod->div0_bit);
		val |= ((div0 - 1) << mod->div0_bit);
	}

	writel(val, mod->reg);
	return 0;
}

static int clk_multi_parent_mod_determine_rate(struct clk_hw *hw,
					  struct clk_rate_request *req)
{
	int num_parents, parent_index;
	uint32_t best_parent_rate;
	struct clk_hw *parent;

	num_parents = clk_hw_get_num_parents(hw);
	if (num_parents == 2) {
		if (req->rate == 24000000) {
			parent_index = 0;
			parent = clk_hw_get_parent_by_index(hw, parent_index);
			req->best_parent_hw = parent;
			best_parent_rate = 24000000;

		} else {
			parent_index = 1;
			parent = clk_hw_get_parent_by_index(hw, parent_index);
			req->best_parent_hw = parent;
			best_parent_rate = clk_hw_get_rate(parent);
		}

		req->best_parent_rate = best_parent_rate;
	} else {
		parent_index = 0;
		parent = clk_hw_get_parent_by_index(hw, parent_index);
		req->best_parent_hw = parent;
		best_parent_rate = clk_hw_get_rate(parent);
		req->best_parent_rate = best_parent_rate;
	}

	return 0;
}


static const struct clk_ops clk_multi_parent_mod_ops = {
	.prepare	= clk_multi_parent_mod_prepare,
	.unprepare	= clk_multi_parent_mod_unprepare,
	.is_prepared	= clk_multi_parent_mod_is_prepared,
	.recalc_rate	= clk_multi_parent_mod_recalc_rate,
	.determine_rate = clk_multi_parent_mod_determine_rate,
	.get_parent	= clk_multi_parent_mod_get_parent,
	.set_parent	= clk_multi_parent_mod_set_parent,
	.set_rate	= clk_multi_parent_mod_set_rate,
};


struct clk_hw *aic_clk_hw_multi_parent(void __iomem *base,
				       const struct multi_parent_clk_cfg *cfg)
{
	struct clk_multi_parent_mod *mod;
	struct clk_hw *hw;
	struct clk_init_data init;
	int ret;

	mod = kzalloc(sizeof(*mod), GFP_KERNEL);
	if (!mod)
		return ERR_PTR(-ENOMEM);

	mod->reg = base + cfg->offset_reg;
	mod->gate_bit = cfg->gate_bit;
	mod->mux_bit = cfg->mux_bit;
	mod->mux_mask = (1 << cfg->mux_width) - 1;
	mod->div0_bit = cfg->div0_bit;
	mod->div0_mask = (1 << cfg->div0_width) - 1;
	mod->name = cfg->name;
#ifdef CONFIG_DEBUG_ON_FPGA_BOARD_ARTINCHIP
	mod->id = cfg->id;
#endif

	init.name = cfg->name;
	init.ops = &clk_multi_parent_mod_ops;
	init.flags = CLK_GET_RATE_NOCACHE;
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
