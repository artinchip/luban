// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, Artinchip Technology Co., Ltd
 */
#ifndef __MACH_AIC_CLK_H
#define __MACH_AIC_CLK_H

#include <linux/spinlock.h>
#include <linux/clk-provider.h>

/* Define types of pll */
enum aic_pll_type {
	AIC_PLL_INT,	/* integer pll */
	AIC_PLL_FRA,	/* fractional pll */
	AIC_PLL_SDM,    /* spread spectrum pll */
};

struct pll_clk_cfg {
	u32 id;
	enum aic_pll_type type;
	const char *name;
	const char * const *parent_names;
	int num_parents;
	u32 offset_gen;
	u32 offset_fra;
	u32 offset_sdm;
	unsigned long min_rate;
	unsigned long max_rate;
	unsigned long flags;
	struct clk_hw *(*func)(void __iomem *base,
			       const struct pll_clk_cfg *cfg);
};

enum aic_fixed_parent_type {
	AIC_FPCLK_NORMAL,
	AIC_FPCLK_FIXED_FACTOR,
};

struct fixed_parent_clk_cfg {
	u32 id;
	u16 type;
	unsigned int fact_mult;
	unsigned int fact_div;
	const char *name;
	const char * const *parent_names;
	int num_parents;
	u32 offset_reg;
	s8 bus_gate_bit;
	s8 mod_gate_bit;
	u8 div_bit;
	u8 div_width;
	struct clk_hw *(*func)(void __iomem *base,
			       const struct fixed_parent_clk_cfg *cfg);
};

struct multi_parent_clk_cfg {
	u32 id;
	const char *name;
	const char * const *parent_names;
	int num_parents;
	u32 offset_reg;
	s32 gate_bit;
	u8 mux_bit;
	u8 mux_width;
	u8 div0_bit;
	u8 div0_width;
	struct clk_hw *(*func)(void __iomem *base,
			       const struct multi_parent_clk_cfg *cfg);
};

struct disp_clk_cfg {
	u32 id;
	const char *name;
	const char * const *parent_names;
	int num_parents;
	u32 offset_reg;
	u8 divn_bit;
	u8 divn_width;
	u8 divm_bit;
	u8 divm_width;
	u8 divl_bit;
	u8 divl_width;
	u8 pix_divsel_bit;
	u8 pix_divsel_width;
	unsigned long flags;
	struct clk_hw *(*func)(void __iomem *base,
				   const struct disp_clk_cfg *cfg);
};

#ifdef CONFIG_DEBUG_ON_FPGA_BOARD_ARTINCHIP
extern const unsigned long fpga_board_rate[];
#endif

/**
 * aic_clk_hw_fixed - register a fixed rate clock to artchip clock framework
 * @name: name of this clock
 * @rate: rate of this clock
 */
static inline struct clk_hw *aic_clk_hw_fixed(const char *name, int rate)
{
	return clk_hw_register_fixed_rate(NULL, name, NULL, 0, rate);
}

static inline struct clk_hw *aic_clk_hw_fixed_factor(const char *name,
						     const char *parent_name,
						     unsigned int mult,
						     unsigned int div)
{
	return clk_hw_register_fixed_factor(NULL, name, parent_name, 0, mult,
					    div);
}


/**
 * aic_clk_hw_pll - register a pll to artinchip clock framework
 * @base: register base address
 * @cfg: clock's configuration parameters
 */
struct clk_hw *aic_clk_hw_pll(void __iomem *base,
			      const struct pll_clk_cfg *cfg);

/**
 * aic_clk_hw_fixed_parent - register a fixed parent module's clock to
 *                      artinchip clock framework
 * @base: register base address
 * @cfg: clock's configuration parameters
 */
struct clk_hw *aic_clk_hw_fixed_parent(void __iomem *base,
				       const struct fixed_parent_clk_cfg *cfg);

/**
 * aic_clk_hw_multi_parent - register a multi-parent module's clock to
 *                      artinchip clock framework
 * @base: register base address
 * @cfg: clock's configuration parameters
 */
struct clk_hw *aic_clk_hw_multi_parent(void __iomem *base,
				       const struct multi_parent_clk_cfg *cfg);

/**
 * aic_clk_hw_disp - register a display module's clock to
 *                      artinchip clock framework
 * @base: register base address
 * @cfg: clock's configuration parameters
 */
struct clk_hw *aic_clk_hw_disp(void __iomem *base,
				const struct disp_clk_cfg *cfg);

#endif	/* __MACH_AIC_CLK_H */
