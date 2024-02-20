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
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include "clk-aic.h"

#define PLL_LOCK_BIT		(17)
#define PLL_EN_BIT		(16)

#define PLL_FACTORN_BIT		(8)
#define PLL_FACTORN_MASK	(0xff)
#define PLL_FACTORN_MIN		(14)
#define PLL_FACTORN_MAX		(199)

#define PLL_FACTORM_BIT		(4)
#define PLL_FACTORM_MASK	(0x3)
#define PLL_FACTORM_MIN		(0)
#define PLL_FACTORM_MAX		(3)
#define PLL_FACTORM_EN_BIT	(19)

#define PLL_FACTORP_BIT		(0)
#define PLL_FACTORP_MASK	(0x1)
#define PLL_FACTORP_MIN		(0)
#define PLL_FACTORP_MAX		(1)

#define PLL_DITHER_EN_BIT	(24)
#define PLL_FRAC_EN_BIT		(20)
#define PLL_FRAC_DIV_BIT	(0)
#define PLL_FRAC_DIV_MASK	(0x1ffff)

#define PLL_OUT_MUX		(20)
#define PLL_OUT_SYS		(18)

#define PLL_SDM_AMP_BIT		(0)
#define PLL_SDM_FREQ_BIT	(17)
#define PLL_SDM_STEP_BIT	(20)
#define PLL_SDM_MODE_BIT	(29)
#define PLL_SDM_EN_BIT		(31)

#define PLL_VCO_MIN		(768000000)
#define PLL_VCO_MAX		(1560000000)
#define PLL_SDM_AMP_MAX		(0x20000)
#define PLL_SDM_SPREAD_PPM	(10000)
#define PLL_SDM_SPREAD_FREQ	(33000)

struct clk_pll {
	struct clk_hw hw;
	const char *name;
	enum aic_pll_type type;
	void __iomem *gen_reg;
	void __iomem *fra_reg;
	void __iomem *sdm_reg;
	unsigned long min_rate;
	unsigned long max_rate;
#ifdef CONFIG_DEBUG_ON_FPGA_BOARD_ARTINCHIP
	unsigned long id;
#endif
};

#define to_clk_pll(_hw) container_of(_hw, struct clk_pll, hw)

static int clk_pll_wait_lock(struct clk_pll *pll)
{
	udelay(100);
	return 0;
}

static void clk_pll_bypass(struct clk_pll *pll, bool bypass)
{
	u32 val;

	val = readl(pll->gen_reg);
	val &= ~(1 << PLL_OUT_MUX);
	val |= (!bypass << PLL_OUT_MUX);
	writel(val, pll->gen_reg);
}

static int clk_pll_prepare(struct clk_hw *hw)
{
	struct clk_pll *pll = to_clk_pll(hw);
	u32 val;

	val = readl(pll->gen_reg);
	val |= (1 << PLL_OUT_SYS | 1 << PLL_EN_BIT);
	writel(val, pll->gen_reg);

	return clk_pll_wait_lock(pll);
}

static void clk_pll_unprepare(struct clk_hw *hw)
{
	struct clk_pll *pll = to_clk_pll(hw);
	u32 val;

	val = readl(pll->gen_reg);
	val &= ~(1 << PLL_OUT_SYS | 1 << PLL_EN_BIT);
	writel(val, pll->gen_reg);
}

static int clk_pll_is_prepared(struct clk_hw *hw)
{
	struct clk_pll *pll = to_clk_pll(hw);
	u32 reg_val = readl(pll->gen_reg);

	if ((reg_val & (1 << PLL_EN_BIT)) &&
	    (reg_val & (1 << PLL_OUT_SYS)))
		return 1;

	return 0;
}

static unsigned long clk_pll_recalc_rate(struct clk_hw *hw,
					 unsigned long parent_rate)
{
	struct clk_pll *pll = to_clk_pll(hw);
	u32 factor_n, factor_m, factor_p, fra_in;
	u64 rate, rate_int, rate_fra;
	u8 fra_en = 0;

	/* PLL output mux is CLK_24M */
	if (!((readl(pll->gen_reg) >> PLL_OUT_MUX) & 0x1))
		return 24000000;

	factor_n = (readl(pll->gen_reg) >> PLL_FACTORN_BIT) & PLL_FACTORN_MASK;
	factor_m = (readl(pll->gen_reg) >> PLL_FACTORM_BIT) & PLL_FACTORM_MASK;
	factor_p = (readl(pll->gen_reg) >> PLL_FACTORP_BIT) & PLL_FACTORP_MASK;
	if (pll->type == AIC_PLL_FRA)
		fra_en = (readl(pll->fra_reg) >> PLL_FRAC_EN_BIT) & 0x1;

	if (pll->type != AIC_PLL_FRA || !fra_en)
		rate = parent_rate / (factor_p + 1) *
			(factor_n + 1) / (factor_m + 1);
	else {
		fra_in = readl(pll->fra_reg) & PLL_FRAC_DIV_MASK;
		rate_int = parent_rate / (factor_p + 1) *
			   (factor_n + 1) / (factor_m + 1);
		rate_fra = (u64)parent_rate / (factor_p + 1) * fra_in;
		do_div(rate_fra, PLL_FRAC_DIV_MASK * (factor_m + 1));
		rate = rate_int + rate_fra;
	}
#ifdef CONFIG_DEBUG_ON_FPGA_BOARD_ARTINCHIP
	rate = fpga_board_rate[pll->id];
#endif
	return rate;
}

static long clk_pll_round_rate(struct clk_hw *hw, unsigned long rate,
			       unsigned long *prate)
{
	struct clk_pll *pll = to_clk_pll(hw);
	u32 factor_n, factor_m, factor_p;
	long rrate, vco_rate;
	unsigned long parent_rate = *prate;

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

	if (factor_m > PLL_FACTORM_MASK)
		factor_m = PLL_FACTORM_MASK;

	vco_rate = (factor_m + 1) * rate;
	if (vco_rate > PLL_VCO_MAX)
		vco_rate = PLL_VCO_MAX;

	factor_p = (vco_rate % parent_rate) ? 1 : 0;
	if (!factor_p)
		return rate;
	else if (!(vco_rate % (parent_rate / (factor_p + 1))))
		return rate;

	factor_n = vco_rate / parent_rate * (factor_p + 1) - 1;

	rrate = parent_rate / (factor_p + 1) * (factor_n + 1) / (factor_m + 1);

#ifdef CONFIG_DEBUG_ON_FPGA_BOARD_ARTINCHIP
		rrate = fpga_board_rate[pll->id];
#endif
	return rrate;
}

static int clk_pll_set_rate(struct clk_hw *hw, unsigned long rate,
			    unsigned long parent_rate)
{
	u32 factor_n, factor_m, factor_p, reg_val;
	u64 val, fra_in = 0;
	u8 fra_en, factor_m_en;
	unsigned long vco_rate;
	u32 ppm_max, sdm_amp, sdm_en = 0;
	u64 sdm_step;
	struct clk_pll *pll = to_clk_pll(hw);

	if (rate == 24000000) {
		val = readl(pll->gen_reg);
		val &= ~(1 << PLL_OUT_MUX);
		writel(val, pll->gen_reg);
		return 0;
	}

	/* Switch the output of PLL to 24MHz */
	clk_pll_bypass(pll, true);
	/* Calculate PLL parameters.
	 * The frequency constraint of PLL_VCO
	 * is between 768M and 1560M
	 */
	if (rate < PLL_VCO_MIN)
		factor_m = DIV_ROUND_UP(PLL_VCO_MIN, rate) - 1;
	else
		factor_m = 0;

	if (factor_m > PLL_FACTORM_MASK)
		factor_m = PLL_FACTORM_MASK;

	if (factor_m)
		factor_m_en = 1;
	else
		factor_m_en = 0;

	vco_rate = (factor_m + 1) * rate;
	if (vco_rate > PLL_VCO_MAX)
		vco_rate = PLL_VCO_MAX;

	factor_p = (vco_rate % parent_rate) ? 1 : 0;
	factor_n = vco_rate * (factor_p + 1) / parent_rate  - 1;

	reg_val = readl(pll->gen_reg);
	reg_val &= ~0xFFFF;
	reg_val |= (factor_m_en << PLL_FACTORM_EN_BIT) |
			(factor_n << PLL_FACTORN_BIT) |
			(factor_m << PLL_FACTORM_BIT) |
			(factor_p << PLL_FACTORP_BIT);
	writel(reg_val, pll->gen_reg);

	if (pll->type == AIC_PLL_FRA) {
		val = rate % (parent_rate * (factor_n + 1) /
			      (factor_m + 1) / (factor_p + 1));
		fra_en = val ? 1 : 0;
		if (fra_en) {
			fra_in = val * (factor_p + 1) *
				 (factor_m + 1) * PLL_FRAC_DIV_MASK;
			do_div(fra_in, parent_rate);
		}
		/* Configure fractional division */
		writel(fra_en << PLL_FRAC_EN_BIT | fra_in, pll->fra_reg);
		/* when using decimal divsion, do not configure spreading parameters */
		sdm_en = (1UL << PLL_SDM_EN_BIT) | (2UL << PLL_SDM_MODE_BIT);
		writel(sdm_en, pll->sdm_reg);
	}

	if (pll->type == AIC_PLL_SDM) {
		sdm_en = readl(pll->sdm_reg);
		sdm_en >>= 31;

		if (sdm_en) {
			ppm_max = 1000000 / (factor_n + 1);
			/* 1% spread */
			if (ppm_max < PLL_SDM_SPREAD_PPM)
				sdm_amp = 0;
			else
				sdm_amp = PLL_SDM_AMP_MAX -
					PLL_SDM_SPREAD_PPM *
					PLL_SDM_AMP_MAX / ppm_max;

			/* SDM uses triangular wave, 33KHz by default  */
			sdm_step = (PLL_SDM_AMP_MAX - sdm_amp) * 2 *
				PLL_SDM_SPREAD_FREQ;
			do_div(sdm_step, parent_rate);
			if (sdm_step > 511)
				sdm_step = 511;

			reg_val = (1UL << PLL_SDM_EN_BIT) |
				  (2 << PLL_SDM_MODE_BIT) |
				  (sdm_step << PLL_SDM_STEP_BIT) |
				  (3 << PLL_SDM_FREQ_BIT) |
				  (sdm_amp << PLL_SDM_AMP_BIT);

			writel(reg_val, pll->sdm_reg);
		}
	}

	if (!clk_pll_wait_lock(pll))
		clk_pll_bypass(pll, false);
	else {
		pr_err("%s not lock\n", pll->name);
		return -EAGAIN;
	}

	return 0;
}

static const struct clk_ops clk_pll_ops = {
	.prepare	= clk_pll_prepare,
	.unprepare	= clk_pll_unprepare,
	.is_prepared	= clk_pll_is_prepared,
	.recalc_rate	= clk_pll_recalc_rate,
	.round_rate	= clk_pll_round_rate,
	.set_rate	= clk_pll_set_rate,
};

struct clk_hw *aic_clk_hw_pll(void __iomem *base, const struct pll_clk_cfg *cfg)
{
	struct clk_init_data init;
	struct clk_pll *pll;
	struct clk_hw *hw;
	int ret;

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return ERR_PTR(-ENOMEM);

	switch (cfg->type) {
	case AIC_PLL_INT:
		pll->fra_reg = 0;
		pll->sdm_reg = 0;
		break;
	case AIC_PLL_FRA:
		pll->fra_reg = base + cfg->offset_fra;
		pll->sdm_reg = base + cfg->offset_sdm;
		break;
	case AIC_PLL_SDM:
		pll->sdm_reg = base + cfg->offset_sdm;
		break;
	default:
		break;
	}

	pll->gen_reg = base + cfg->offset_gen;
	pll->name = cfg->name;
	pll->type = cfg->type;
#ifdef CONFIG_DEBUG_ON_FPGA_BOARD_ARTINCHIP
	pll->id = cfg->id;
#endif
	pll->min_rate = cfg->min_rate;
	pll->max_rate = cfg->max_rate;

	init.name = cfg->name;
	init.ops = &clk_pll_ops;
	init.flags = cfg->flags;
	init.parent_names = cfg->parent_names;
	init.num_parents = cfg->num_parents;

	pll->hw.init = &init;
	hw = &pll->hw;

	ret = clk_hw_register(NULL, hw);
	if (ret) {
		kfree(pll);
		return ERR_PTR(ret);
	}

	if (pll->min_rate || pll->max_rate)
		clk_hw_set_rate_range(hw, pll->min_rate, pll->max_rate);

	return hw;
}
