// SPDX-License-Identifier: GPL-2.0
/*
 * This file is used to control RGB/LVDS/MIPIDSI clocks
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

struct clk_disp_mod {
	struct clk_hw hw;
	void __iomem *reg;
	const char *name;
	u8 divn_bit;
	u8 divn_mask;
	u8 divm_bit;
	u8 divm_mask;
	u8 divl_bit;
	u8 divl_mask;
	u8 pix_divsel_bit;
	u8 pix_divsel_mask;
#ifdef CONFIG_DEBUG_ON_FPGA_BOARD_ARTINCHIP
	unsigned long id;
#endif
};

#define to_clk_disp_mod(_hw) container_of(_hw, struct clk_disp_mod, hw)

static unsigned long clk_disp_mod_recalc_rate(struct clk_hw *hw,
					unsigned long parent_rate)
{
	struct clk_disp_mod *mod = to_clk_disp_mod(hw);
	unsigned long rate, pix_divsel, divn, divm, divl;
	u32 reg_val;

	if (mod->divn_mask) {
		/* For sclk */
		divn = (readl(mod->reg) >> mod->divn_bit) & mod->divn_mask;
		rate = parent_rate / (1 << divn);
	} else if (mod->divm_mask) {
		/* For pixclk */
		reg_val = readl(mod->reg);
		pix_divsel = (reg_val >> mod->pix_divsel_bit) &
				mod->pix_divsel_mask;
		switch (pix_divsel) {
		case 0:
			divl = (reg_val >> mod->divl_bit) & mod->divl_mask;
			divm = (reg_val >> mod->divm_bit) & mod->divm_mask;
			rate = parent_rate / (1 << divl) / (divm + 1);
			break;
		case 1:
			rate = parent_rate * 2 / 7;
			break;
		case 2:
			rate = parent_rate * 2 / 9;
			break;
		default:
			return -EINVAL;
		}
	} else
		return -EINVAL;

#ifdef CONFIG_DEBUG_ON_FPGA_BOARD_ARTINCHIP
	rate = fpga_board_rate[mod->id];
#endif
	return rate;

}

static void clk_disp_try_best_divider(struct clk_hw *hw, u32 rate,
			unsigned long *parent_rate, u32 max_divn, u32 *divn)
{
	struct clk_hw *phw;
	u32 tmp, i, prrate;

	phw = clk_hw_get_parent(hw);

	for (i = 0; i <= max_divn; i++) {
		tmp = (1 << i) * rate;
		prrate = clk_hw_round_rate(phw, tmp);
		if (prrate <= tmp) {
			*divn = i;
			*parent_rate = prrate;
			return;
		}
	}

	*divn = 0;
}

static void clk_disp_try_best_divider_pixclk(u32 rate, u32 parent_rate,
	u32 divm_max, u32 *divm, u32 divl_max, u32 *divl, u8 *pix_divsel)
{
	u32 tmp, tmpm, tmpl, val0, val1, val2, i;

	/* Calculate clock division */
	tmp = DIV_ROUND_CLOSEST(parent_rate, rate);
	/* Calculate the value of divl */
	tmpl = DIV_ROUND_UP(tmp, divm_max);
	for (i = 0; i < divl_max; i++)
		if (1 << i >= tmpl)
			break;
	tmpm = tmp / (1 << i);

	if ((1 << i) * tmpm * rate == parent_rate) {
		*pix_divsel = 0;
	} else {
		val0 = abs((1 << i) * tmpm * rate - parent_rate);
		val1 = abs(rate * 7 / 2 - parent_rate);
		val2 = abs(rate * 9 / 2 - parent_rate);

		if (val0 < val1 && val0 < val2)
			*pix_divsel = 0;
		else if (val1 < val0 && val1 < val2)
			*pix_divsel = 1;
		else if (val2 < val0 && val2 < val1)
			*pix_divsel = 2;
		else
			*pix_divsel = 0;
	}

	*divm = tmpm - 1;
	*divl = i;
}



static long clk_disp_mod_round_rate(struct clk_hw *hw,
				unsigned long rate, unsigned long *prate)
{
	struct clk_disp_mod *mod = to_clk_disp_mod(hw);
	u32 parent_rate, divn, divm, divl, rrate;
	u8 pix_divsel = 0;

	parent_rate = *prate;

	if (mod->divm_mask) {
		/* For pixclk */
		clk_disp_try_best_divider_pixclk(rate, parent_rate,
				mod->divm_mask + 1, &divm,
				mod->divl_mask + 1, &divl,
				&pix_divsel);
		if (pix_divsel == 1)
			rrate = parent_rate * 2 / 7;
		else if (pix_divsel == 2)
			rrate = parent_rate * 2 / 9;
		else
			rrate = parent_rate / (divm + 1) / (1 << divl);
	} else if (mod->divn_mask) {
		/* For sclk */
		clk_disp_try_best_divider(hw, rate, prate, mod->divn_mask,
					  &divn);
		rrate = *prate / (1 << divn);
	} else
		return -EINVAL;

#ifdef CONFIG_DEBUG_ON_FPGA_BOARD_ARTINCHIP
	rrate = fpga_board_rate[mod->id];
#endif
	return rrate;
}

static int clk_disp_mod_set_rate(struct clk_hw *hw, unsigned long rate,
				 unsigned long parent_rate)
{
	struct clk_disp_mod *mod = to_clk_disp_mod(hw);
	u32 divn, divm, divl, val;
	u8 pix_divsel = 0;

	val = readl(mod->reg);

	if (mod->divm_mask) {
		/* For pixclk */
		clk_disp_try_best_divider_pixclk(rate, parent_rate,
				mod->divm_mask + 1, &divm,
				mod->divl_mask + 1, &divl,
				&pix_divsel);
		if (pix_divsel) {
			val &= ~(mod->pix_divsel_mask << mod->pix_divsel_bit);
			val |= (pix_divsel << mod->pix_divsel_bit);
		} else {
			val &= ~((mod->pix_divsel_mask << mod->pix_divsel_bit) |
				 (mod->divm_mask << mod->divm_bit) |
				 (mod->divl_mask << mod->divl_bit));
			val |= (divm << mod->divm_bit) |
			       (divl << mod->divl_bit);
		}

		writel(val, mod->reg);
	} else if (mod->divn_mask) {
		/* For sclk */
		clk_disp_try_best_divider(hw, rate, &parent_rate, mod->divn_mask,
					  &divn);
		val &= ~(mod->divn_mask << mod->divn_bit);
		val |= (divn << mod->divn_bit);

		writel(val, mod->reg);
	}

	return 0;
}


static const struct clk_ops clk_disp_mod_ops = {
	.recalc_rate = clk_disp_mod_recalc_rate,
	.round_rate = clk_disp_mod_round_rate,
	.set_rate = clk_disp_mod_set_rate,
};

struct clk_hw *aic_clk_hw_disp(void __iomem *base,
			const struct disp_clk_cfg *cfg)
{
	struct clk_disp_mod *mod;
	struct clk_init_data init;
	struct clk_hw *hw;
	int ret;

	mod = kzalloc(sizeof(*mod), GFP_KERNEL);
	if (!mod)
		return ERR_PTR(-ENOMEM);

	mod->reg = base + cfg->offset_reg;
	mod->divn_bit = cfg->divn_bit;
	mod->divn_mask = (1 << cfg->divn_width) - 1;
	mod->divm_bit = cfg->divm_bit;
	mod->divm_mask = (1 << cfg->divm_width) - 1;
	mod->divl_bit = cfg->divl_bit;
	mod->divl_mask = (1 << cfg->divl_width) - 1;
	mod->pix_divsel_bit = cfg->pix_divsel_bit;
	mod->pix_divsel_mask = (1 << cfg->pix_divsel_width) - 1;
	mod->name = cfg->name;
#ifdef CONFIG_DEBUG_ON_FPGA_BOARD_ARTINCHIP
	mod->id = cfg->id;
#endif

	init.name = cfg->name;
	init.ops = &clk_disp_mod_ops;
	init.flags = cfg->flags;
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
