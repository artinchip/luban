// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2020 ArtInChip Inc.
 */

#include <common.h>
#include <command.h>
#include <clk-uclass.h>
#include <dm.h>
#include <dt-structs.h>
#include <errno.h>
#include <asm/io.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <dm/uclass-internal.h>
#include <linux/delay.h>
#include <div64.h>
#include "clk-aic.h"

static enum aic_clk_type aic_get_clk_info(struct aic_clk_tree *tree, u32 id,
					  u32 *index)
{
	int i;

	if (id >= tree->clkout_base) {
		for (i = 0; i < tree->clkout_cnt; i++) {
			if (id == tree->clkout[i].id) {
				*index = i;
				return AIC_CLK_OUTPUT;
			}
		}
	} else
	if (id >= tree->disp_base) {
		for (i = 0; i < tree->disp_cnt; i++) {
			if (id == tree->disp[i].id) {
				*index = i;
				return AIC_CLK_DISP;
			}
		}
	} else
	if (id >= tree->periph_base) {
		for (i = 0; i < tree->periph_cnt; i++) {
			if (id == tree->periph[i].id) {
				*index = i;
				return AIC_CLK_PERIPHERAL;
			}
		}
	} else if (id >= tree->system_base) {
		for (i = 0; i < tree->system_cnt; i++) {
			if (id == tree->system[i].id) {
				*index = i;
				return AIC_CLK_SYSTEM;
			}
		}
	} else if (id >= tree->pll_base) {
		for (i = 0; i < tree->pll_cnt; i++) {
			if (id == tree->plls[i].id) {
				*index = i;
				return AIC_CLK_PLL;
			}
		}
	} else if (id >= tree->fixed_rate_base) {
		for (i = 0; i < tree->fixed_rate_cnt; i++) {
			if (id == tree->fixed_rate[i].id) {
				*index = i;
				return AIC_CLK_FIXED_RATE;
			}
		}
	}

	return AIC_CLK_UNKNOWN;
}

static ulong fixed_rate_clk_get_rate(struct clk *clk, int index)
{
	struct aic_clk_priv *priv = dev_get_priv(clk->dev);
	struct aic_clk_tree *tree = priv->tree;

	return tree->fixed_rate[index].rate;
}

static ulong artinchip_get_parent_rate(struct clk *clk, u32 id)
{
	struct clk parent = {.id = id, .dev = clk->dev };
	return clk_get_rate(&parent);
}

static int pll_clk_enable(struct clk *clk, int index)
{
	u32 value;
	struct aic_clk_priv *priv = dev_get_priv(clk->dev);
	struct aic_clk_tree *tree = priv->tree;
	struct aic_pll *pll = &tree->plls[index];

	value = readl(priv->base + pll->gen_reg);
	/* Set ICP value to 8 */
	value &= ~(0x1F << 24);
	value |= (1UL << 31) | (8 << 24) | (1 << 20) | (1 << 18) | (1 << 16);
	writel(value, priv->base + pll->gen_reg);

	return 0;
}

static int pll_clk_disable(struct clk *clk, int index)
{
	u32 value;
	struct aic_clk_priv *priv = dev_get_priv(clk->dev);
	struct aic_clk_tree *tree = priv->tree;
	struct aic_pll *pll = &tree->plls[index];

	value = readl(priv->base + pll->gen_reg);
	value &= ~(1 << 16);
	writel(value, priv->base + pll->gen_reg);

	return 0;
}

static ulong pll_clk_get_rate(struct clk *clk, int index)
{
	u32 value, div_p, div_n, div_m, fra_in;
	u64 rate, rate_int, rate_fra;
	u32 fra_en = 0;
	struct aic_clk_priv *priv = dev_get_priv(clk->dev);
	struct aic_clk_tree *tree = priv->tree;
	struct aic_pll *pll = &tree->plls[index];

	value = readl(priv->base + pll->gen_reg);
	if (!(value & (1 << 20)))
		return 24000000;
	div_p = (value & 0x01);
	div_m = (value >> 4) & 0x03;
	div_n = (value >> 8) & 0xff;

	if (pll->type == AIC_PLL_FRA)
		fra_en = readl(priv->base + pll->frac_reg) & (1 << 20);

	if (pll->type != AIC_PLL_FRA || !fra_en)
		rate = 24000000 / (div_p + 1) * (div_n + 1) / (div_m + 1);
	else {
		fra_in = readl(priv->base + pll->frac_reg) & 0x1FFFF;
		rate_int = 24000000 / (div_p + 1) * (div_n + 1) / (div_m + 1);
		rate_fra = (u64)24000000 / (div_p + 1) * fra_in;
		do_div(rate_fra, 0x1FFFF * (div_m + 1));
		rate = rate_int + rate_fra;
	}

	return rate;
}

static void clk_pll_bypass(struct aic_pll *pll, void *base_addr, bool bypass)
{
	u32 val;

	val = readl(base_addr + pll->gen_reg);
	val &= ~(1 << 20);
	val |= (!bypass << 20);
	writel(val, base_addr + pll->gen_reg);
}

static ulong pll_clk_round_rate(struct clk *clk, ulong rate, int index)
{
	u32 factor_n, factor_m, factor_p;
	ulong rrate, vco_rate;
	struct aic_clk_priv *priv = dev_get_priv(clk->dev);
	struct aic_clk_tree *tree = priv->tree;
	struct aic_pll *pll = &tree->plls[index];

	if (pll->type != AIC_PLL_INT) {
		if (rate < pll->min_rate)
			return pll->min_rate;
		else if (pll->max_rate && rate > pll->max_rate)
			return pll->max_rate;
		else if (pll->type == AIC_PLL_FRA)
			return rate;
	}

	/* The frequency constraint of PLL_VCO is between 768M and 1560M */
	if (rate < PLL_VCO_MIN)
		factor_m = DIV_ROUND_UP(PLL_VCO_MIN, rate) - 1;
	else
		factor_m = 0;

	if (factor_m > 3)
		factor_m = 3;

	vco_rate = (factor_m + 1) * rate;
	if (vco_rate > PLL_VCO_MAX)
		vco_rate = PLL_VCO_MAX;

	factor_p = (rate % 24000000) ? 1 : 0;
	if (!factor_p)
		return rate;
	else if (!(rate % (24000000 / (factor_p + 1))))
		return rate;

	factor_n = vco_rate * (factor_p + 1) / 24000000  - 1;

	rrate = 24000000 / (factor_p + 1) * (factor_n + 1) / (factor_m + 1);

	return rrate;
}


static ulong pll_clk_set_rate(struct clk *clk, ulong rate, int index)
{
	u32 reg_val, factor_p, factor_n, factor_m;
	u64 val, fra_in = 0;
	u8 fra_en, factor_m_en;
	ulong vco_rate;
#ifdef CONFIG_CLK_ARTINCHIP_PLL_SDM
	u32 ppm_max, sdm_amp, sdm_step;
#endif
	struct aic_clk_priv *priv = dev_get_priv(clk->dev);
	struct aic_clk_tree *tree = priv->tree;
	struct aic_pll *pll = &tree->plls[index];

	if (rate == 24000000) {
		reg_val = readl(priv->base + pll->gen_reg);
		reg_val &= ~(1 << 20);
		writel(reg_val, priv->base + pll->gen_reg);
		return 0;
	}

	/* Switch the output of PLL to 24MHz */
	clk_pll_bypass(pll, priv->base, true);
	/* Calculate PLL parameters.
	 * The frequency constraint of PLL_VCO
	 * is between 768M and 1560M
	 */
	if (rate < PLL_VCO_MIN)
		factor_m = DIV_ROUND_UP(PLL_VCO_MIN, rate) - 1;
	else
		factor_m = 0;

	if (factor_m) {
		factor_m_en = 1;
		if (factor_m > 3)
			factor_m = 3;
	} else
		factor_m_en = 0;


	vco_rate = (factor_m + 1) * rate;
	if (vco_rate > PLL_VCO_MAX)
		vco_rate = PLL_VCO_MAX;

	factor_p = (vco_rate % 24000000) ? 1 : 0;
	factor_n = vco_rate * (factor_p + 1) / 24000000  - 1;

	reg_val = readl(priv->base + pll->gen_reg);
	reg_val &= ~0xFFFF;
	reg_val |= (factor_m_en << 19) | (factor_n << 8) | (factor_m << 4) | (factor_p << 0);
	writel(reg_val, priv->base + pll->gen_reg);

	if (pll->type == AIC_PLL_FRA) {
		val = rate % (24000000 * (factor_n + 1) /
			      (factor_m + 1) / (factor_p + 1));
		fra_en = val ? 1 : 0;
		if (fra_en) {
			fra_in = val * (factor_p + 1) *
				 (factor_m + 1) * 0x1FFFF;
			do_div(fra_in, 24000000);
		}
		/* Configure fractional division */
		writel(fra_en << 20 | fra_in, priv->base + pll->frac_reg);
	}

#ifdef CONFIG_CLK_ARTINCHIP_PLL_SDM
	if (pll->type == AIC_PLL_SDM) {
		ppm_max = 1000000 / (factor_n + 1);
		/* 1% spread */
		if (ppm_max < PLL_SDM_SPREAD_PPM)
			sdm_amp = 0;
		else
			sdm_amp = PLL_SDM_AMP_MAX -
				PLL_SDM_SPREAD_PPM * PLL_SDM_AMP_MAX / ppm_max;

		/* SDM uses triangular wave, 33KHz by default  */
		sdm_step = (u64)(PLL_SDM_AMP_MAX - sdm_amp) * 2 *
				PLL_SDM_SPREAD_FREQ / 24000000;
		if (sdm_step > 511)
			sdm_step = 511;

		reg_val = (1UL << 31) | (2 << PLL_SDM_MODE_BIT) |
			  (sdm_step << PLL_SDM_STEP_BIT) |
			  (3 << PLL_SDM_FREQ_BIT) |
			  (sdm_amp << PLL_SDM_AMP_BIT);

		writel(reg_val, priv->base + pll->sdm_reg);
	}
#endif
	/* Switch PLL output */
	clk_pll_bypass(pll, priv->base, false);

	return 0;
}


static ulong system_clk_set_rate(struct clk *clk, ulong rate, int index)
{
	u32 value, parent, parent_rate, div;
	struct aic_clk_priv *priv = dev_get_priv(clk->dev);
	struct aic_clk_tree *tree = priv->tree;
	struct aic_sys_clk *system = &tree->system[index];

	value = readl(priv->base + system->reg);

	parent = system->parent[(value >> system->mux_shift) &
		system->mux_mask];
	parent_rate = artinchip_get_parent_rate(clk, parent);

	if (system->div_shift >= 0) {
		div = DIV_ROUND_CLOSEST(parent_rate, rate);
		div -= div > 0 ? 1 : 0;
		div = div > system->div_mask ? system->div_mask : div;
		value = readl(priv->base + system->reg);
		value &= ~(system->div_mask << system->div_shift);
		value |= div << system->div_shift;
		writel(value, priv->base + system->reg);
	}

	return 0;
}

static ulong system_clk_get_rate(struct clk *clk, int index)
{
	u32 value, parent, parent_rate;
	struct aic_clk_priv *priv = dev_get_priv(clk->dev);
	struct aic_clk_tree *tree = priv->tree;
	struct aic_sys_clk *system = &tree->system[index];

	value = readl(priv->base + system->reg);
	if (!(value & (1 << 8)))
		return 24000000;

	parent = system->parent[(value >> system->mux_shift) &
		system->mux_mask];
	parent_rate = artinchip_get_parent_rate(clk, parent);

	if (system->div_shift >= 0) {
		value = readl(priv->base + system->reg);
		value = (value >> system->div_shift) & system->div_mask;
	}
	value += 1;

	return parent_rate / value;
}

static int system_clk_set_parent(struct clk *clk,
					struct clk *parent, int index)
{
	int i;
	u32 value;
	struct aic_clk_priv *priv = dev_get_priv(clk->dev);
	struct aic_clk_tree *tree = priv->tree;
	struct aic_sys_clk *system = &tree->system[index];

	value = readl((void *)((long)priv->base + system->reg));
	value &= ~(system->mux_mask << system->mux_shift);
	for (i = 0; i < system->parent_cnt; i++) {
		if (system->parent[i] == parent->id) {
			value |= (i << system->mux_shift);
			writel(value, (void *)((long)priv->base + system->reg));
			return 0;
		}
	}

	return -EPERM;
}


static int periph_clk_enable(struct clk *clk, int index)
{
	u32 value;
	struct aic_clk_priv *priv = dev_get_priv(clk->dev);
	struct aic_clk_tree *tree = priv->tree;
	struct aic_periph_clk *periph = &tree->periph[index];

	value = readl(priv->base + periph->reg);
	if (periph->bus_gate >= 0)
		value |= 1 << periph->bus_gate;
	if (periph->mod_gate >= 0)
		value |= 1 << periph->mod_gate;
	writel(value, priv->base + periph->reg);

	value = readl(priv->base + periph->reg);
	return 0;
}

static int periph_clk_disable(struct clk *clk, int index)
{
	u32 value;
	struct aic_clk_priv *priv = dev_get_priv(clk->dev);
	struct aic_clk_tree *tree = priv->tree;
	struct aic_periph_clk *periph = &tree->periph[index];

	value = readl(priv->base + periph->reg);
	if (periph->bus_gate >= 0)
		value &= ~(1 << periph->bus_gate);
	if (periph->mod_gate >= 0)
		value &= ~(1 << periph->mod_gate);
	writel(value, priv->base + periph->reg);

	return 0;
}

static ulong periph_clk_get_rate(struct clk *clk, int index)
{
	u32 value, parent_rate;
	struct aic_clk_priv *priv = dev_get_priv(clk->dev);
	struct aic_clk_tree *tree = priv->tree;
	struct aic_periph_clk *periph = &tree->periph[index];

	parent_rate = artinchip_get_parent_rate(clk, periph->parent);

	value = readl(priv->base + periph->reg);
	value = (value >> periph->div_shift) & periph->div_mask;

	return parent_rate / (value + 1);
}

static ulong periph_clk_set_rate(struct clk *clk, ulong rate, int index)
{
	u32 value, parent_rate, div;
	struct aic_clk_priv *priv = dev_get_priv(clk->dev);
	struct aic_clk_tree *tree = priv->tree;
	struct aic_periph_clk *periph = &tree->periph[index];

	parent_rate = artinchip_get_parent_rate(clk, periph->parent);
	div = DIV_ROUND_CLOSEST(parent_rate, rate);
	if (div > 0)
		div = div - 1;
	value = readl(priv->base + periph->reg);
	value &= ~(periph->div_mask << periph->div_shift);
	value |= (div & periph->div_mask) << periph->div_shift;
	writel(value, priv->base + periph->reg);

	return 0;
}

static ulong disp_clk_get_rate(struct clk *clk, int index)
{
	u32 reg_val, parent_rate, pix_divsel, divn, divm, divl, rate;
	struct aic_clk_priv *priv = dev_get_priv(clk->dev);
	struct aic_clk_tree *tree = priv->tree;
	struct aic_disp_clk *disp = &tree->disp[index];

	parent_rate = artinchip_get_parent_rate(clk, disp->parent);
	reg_val = readl(priv->base + disp->reg);

	if (disp->divm_mask) {
		/* for pixclk */
		pix_divsel = (reg_val >> disp->pix_divsel_shift) &
				disp->pix_divsel_mask;
		switch (pix_divsel) {
		case 0:
			divl = (reg_val >> disp->divl_shift) & disp->divl_mask;
			divm = (reg_val >> disp->divm_shift) & disp->divm_mask;
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
	} else {
		/* for SCLK */
		divn = (reg_val >> disp->divn_shift) & disp->divn_mask;
		rate = parent_rate / (1 << divn);
	}

	return rate;
}

static void disp_clk_try_best_divider_pixclk(ulong rate, ulong parent_rate,
						u32 divm_max, u32 *divm,
						u32 divl_max, u32 *divl,
						u8 *pix_divsel)
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

	if ((1 << i) * tmpm * rate == parent_rate)
		*pix_divsel = 0;
	else {
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

static void disp_clk_try_best_divider(struct clk *clk,
					struct aic_disp_clk *disp,
					ulong rate, ulong *parent_rate,
					u32 max_divn, u32 *divn)
{
	u32 tmp, i;
	ulong prrate;

	struct clk parent = { .id = disp->parent, .dev = clk->dev };

	for (i = 0; i < max_divn; i++) {
		tmp = (1 << i) * rate;
		prrate = clk_round_rate(&parent, tmp);
		if (prrate <= tmp) {
			*divn = i;
			*parent_rate = prrate;
			return;
		}
	}

	*divn = 0;
}

static ulong disp_clk_set_rate(struct clk *clk, ulong rate, int index)
{
	u32 reg_val, divm, divn, divl;
	ulong parent_rate, parent_rate_old;
	u8 pix_divsel = 0;
	struct aic_clk_priv *priv = dev_get_priv(clk->dev);
	struct aic_clk_tree *tree = priv->tree;
	struct aic_disp_clk *disp = &tree->disp[index];

	parent_rate = artinchip_get_parent_rate(clk, disp->parent);
	reg_val = readl(priv->base + disp->reg);

	if (disp->divm_mask) {
		/* for pixclk */
		disp_clk_try_best_divider_pixclk(rate, parent_rate,
					disp->divm_mask + 1, &divm,
					disp->divl_mask + 1, &divl,
					&pix_divsel);
		if (pix_divsel) {
			reg_val &= ~(disp->pix_divsel_mask <<
				     disp->pix_divsel_shift);
			reg_val |= (pix_divsel << disp->pix_divsel_shift);
		} else {
			reg_val &= ~((disp->pix_divsel_mask <<
				      disp->pix_divsel_shift) |
				     (disp->divm_mask << disp->divm_shift) |
				     (disp->divl_mask << disp->divl_shift));
			reg_val |= (divm << disp->divm_shift) |
				   (divl << disp->divl_shift);
		}
	} else {
		/* for SCLK */
		parent_rate_old = parent_rate;
		disp_clk_try_best_divider(clk, disp, rate, &parent_rate,
			disp->divn_mask + 1, &divn);
		/* If new parent clock rate is needed, set the parent rate */
		if (parent_rate != parent_rate_old) {
			struct clk parent = { .id = disp->parent,
					      .dev = clk->dev };
			clk_set_rate(&parent, parent_rate);
		}

		reg_val &= ~(disp->divn_mask << disp->divn_shift);
		reg_val |= (divn << disp->divn_shift);
	}

	writel(reg_val, priv->base + disp->reg);
	return 0;
}

static int output_clk_enable(struct clk *clk, int index)
{
	u32 value;
	struct aic_clk_priv *priv = dev_get_priv(clk->dev);
	struct aic_clk_tree *tree = priv->tree;
	struct aic_clk_out *clkout = &tree->clkout[index];

	value = readl(priv->base + clkout->reg);
	value |= 1 << 16;
	writel(value, priv->base + clkout->reg);

	return 0;
}

static int output_clk_disable(struct clk *clk, int index)
{
	u32 value;
	struct aic_clk_priv *priv = dev_get_priv(clk->dev);
	struct aic_clk_tree *tree = priv->tree;
	struct aic_clk_out *clkout = &tree->clkout[index];

	value = readl(priv->base + clkout->reg);
	value &= ~(1 << 16);
	writel(value, priv->base + clkout->reg);

	return 0;
}

static ulong output_clk_get_rate(struct clk *clk, int index)
{
	u32 value, parent, parent_rate, div_n0;
	struct aic_clk_priv *priv = dev_get_priv(clk->dev);
	struct aic_clk_tree *tree = priv->tree;
	struct aic_clk_out *clkout = &tree->clkout[index];

	value = readl(priv->base + clkout->reg);
	div_n0 = (value >> clkout->div0_shift) & clkout->div0_mask;
	parent = clkout->parent[(value >> clkout->mux_shift) &
		clkout->mux_mask];

	parent_rate = artinchip_get_parent_rate(clk, parent);

	return parent_rate / (div_n0 + 1);
}

static ulong output_clk_set_rate(struct clk *clk, ulong rate, int index)
{
	u32 value, parent, parent_rate, div_n0;
	struct aic_clk_priv *priv = dev_get_priv(clk->dev);
	struct aic_clk_tree *tree = priv->tree;
	struct aic_clk_out *clkout = &tree->clkout[index];

	value = readl(priv->base + clkout->reg);

	parent = clkout->parent[(value >> clkout->mux_shift) &
		clkout->mux_mask];
	parent_rate = artinchip_get_parent_rate(clk, parent);
	div_n0 = DIV_ROUND_CLOSEST(parent_rate, rate);
	if (div_n0 > 0)
		div_n0 -= 1;

	value &= ~(clkout->div0_mask << clkout->div0_shift);
	value |= (div_n0 << clkout->div0_shift);
	writel(value, priv->base + clkout->reg);

	return 0;
}

static int output_clk_set_parent(struct clk *clk,
					struct clk *parent, int index)
{
	int i;
	u32 value;
	struct aic_clk_priv *priv = dev_get_priv(clk->dev);
	struct aic_clk_tree *tree = priv->tree;
	struct aic_clk_out *clkout = &tree->clkout[index];

	value = readl(priv->base + clkout->reg);
	value &= ~(clkout->mux_mask << clkout->mux_shift);
	for (i = 0; i < clkout->parent_cnt; i++) {
		if (clkout->parent[i] == parent->id) {
			value |= (i << clkout->mux_shift);
			writel(value, priv->base + clkout->reg);
			return 0;
		}
	}

	return -EPERM;
}


static struct aic_clk_ops aic_clk_type_ops[] = {
	/* ops handle for fixed rate clocks */
	{
		.get_rate = fixed_rate_clk_get_rate,
	},

	/* ops handle for pll clocks */
	{
		.enable = pll_clk_enable,
		.disable = pll_clk_disable,
		.get_rate = pll_clk_get_rate,
		.set_rate = pll_clk_set_rate,
		.round_rate = pll_clk_round_rate,
	},

	/* ops handle for system clocks */
	{
		.set_rate = system_clk_set_rate,
		.get_rate = system_clk_get_rate,
		.set_parent = system_clk_set_parent,
	},

	/* ops handle for periph clocks */
	{
		.enable = periph_clk_enable,
		.disable = periph_clk_disable,
		.get_rate = periph_clk_get_rate,
		.set_rate = periph_clk_set_rate,
	},

	/* ops handle for disp clocks */
	{
		.get_rate = disp_clk_get_rate,
		.set_rate = disp_clk_set_rate,
	},

	/* ops handle for output clocks */
	{
		.enable = output_clk_enable,
		.disable = output_clk_disable,
		.get_rate = output_clk_get_rate,
		.set_rate = output_clk_set_rate,
		.set_parent = output_clk_set_parent,
	},
};

static ulong artinchip_clk_get_rate(struct clk *clk)
{
	u32 index;
	enum aic_clk_type type;

	struct aic_clk_priv *priv = dev_get_priv(clk->dev);

	type = aic_get_clk_info(priv->tree, clk->id, &index);
	if ((type != AIC_CLK_UNKNOWN) && aic_clk_type_ops[type].get_rate)
		return aic_clk_type_ops[type].get_rate(clk, index);

	return 0;
}

static ulong artinchip_clk_set_rate(struct clk *clk, ulong rate)
{
	u32 index;
	enum aic_clk_type type;
	struct aic_clk_priv *priv = dev_get_priv(clk->dev);

	type = aic_get_clk_info(priv->tree, clk->id, &index);
	if ((type != AIC_CLK_UNKNOWN) && aic_clk_type_ops[type].set_rate)
		return aic_clk_type_ops[type].set_rate(clk, rate, index);

	return 0;
}

static int artinchip_clk_set_parent(struct clk *clk, struct clk *parent)
{
	u32 index;
	enum aic_clk_type type;

	struct aic_clk_priv *priv = dev_get_priv(clk->dev);

	type = aic_get_clk_info(priv->tree, clk->id, &index);
	if ((type != AIC_CLK_UNKNOWN) && aic_clk_type_ops[type].set_parent)
		return aic_clk_type_ops[type].set_parent(clk, parent, index);

	return 0;
}

static int artinchip_clk_enable(struct clk *clk)
{
	u32 index;
	enum aic_clk_type type;

	struct aic_clk_priv *priv = dev_get_priv(clk->dev);

	type = aic_get_clk_info(priv->tree, clk->id, &index);
	if ((type != AIC_CLK_UNKNOWN) && aic_clk_type_ops[type].enable)
		return aic_clk_type_ops[type].enable(clk, index);

	return 0;
}

static int artinchip_clk_disable(struct clk *clk)
{
	u32 index;
	enum aic_clk_type type;
	struct aic_clk_priv *priv = dev_get_priv(clk->dev);

	type = aic_get_clk_info(priv->tree, clk->id, &index);
	if ((type != AIC_CLK_UNKNOWN) && aic_clk_type_ops[type].disable)
		return aic_clk_type_ops[type].disable(clk, index);

	return 0;
}

static ulong artinchip_clk_round_rate(struct clk *clk, ulong rate)
{
	u32 index;
	enum aic_clk_type type;
	struct aic_clk_priv *priv = dev_get_priv(clk->dev);

	type = aic_get_clk_info(priv->tree, clk->id, &index);
	if ((type != AIC_CLK_UNKNOWN) && aic_clk_type_ops[type].round_rate)
		return aic_clk_type_ops[type].round_rate(clk, rate, index);

	return 0;
}

const struct clk_ops artinchip_clk_ops = {
	.get_rate   = artinchip_clk_get_rate,
	.set_rate   = artinchip_clk_set_rate,
	.set_parent = artinchip_clk_set_parent,
	.enable     = artinchip_clk_enable,
	.disable    = artinchip_clk_disable,
	.round_rate = artinchip_clk_round_rate,
};


int aic_clk_common_init(struct udevice *dev, struct aic_clk_tree *tree)
{
	struct aic_clk_priv *priv = dev_get_priv(dev);

	priv->base = dev_read_addr_ptr(dev);
	if (!priv->base)
		return -ENOENT;

	priv->tree = tree;

	return 0;
}
