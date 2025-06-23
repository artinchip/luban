/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2020 ArtInChip Inc.
 */
#ifndef __DRV_CLK_AIC_H
#define __DRV_CLK_AIC_H

#include <clk.h>

#define AIC_CLK_PERIPH(_id, _parent, _reg)                                     \
	CLK_PERIPH(_id, _parent, _reg, 12, 8, 0, 5)
#define AIC_CLK_OUT(_id, _reg)                                                 \
	CLK_OUT(_id, outclk_src_sels, _reg, ARRAY_SIZE(outclk_src_sels), 12,   \
		3, 0, 8)

#define PLL_SDM_AMP_BIT		(0)
#define PLL_SDM_FREQ_BIT	(17)
#define PLL_SDM_STEP_BIT	(20)
#define PLL_SDM_MODE_BIT	(29)
#define PLL_SDM_EN_BIT		(31)

#define PLL_SDM_AMP_MAX		(0x20000)
#define PLL_SDM_SPREAD_PPM	(10000)
#define PLL_SDM_SPREAD_FREQ	(33000)

/* define types of pll */
enum aic_pll_type {
	AIC_PLL_INT,	/* integer pll */
	AIC_PLL_FRA,	/* fractional pll */
	AIC_PLL_SDM,	/* spread spectrum pll */
};

enum aic_sys_type {
	AIC_CPU_CLK = 0,
	AIC_BUS_CLK = 1,
};

enum aic_clk_type {
	AIC_CLK_FIXED_RATE = 0,
	AIC_CLK_PLL = 1,
	AIC_CLK_SYSTEM = 2,
	AIC_CLK_PERIPHERAL = 3,
	AIC_CLK_DISP = 4,
	AIC_CLK_OUTPUT = 5,
	AIC_CLK_CROSS_ZONE = 6,
	AIC_CLK_UNKNOWN = 7
};

struct aic_clks {
	ulong id;
	const char *name;
};

struct pll_vco {
	ulong vco_min;
	ulong vco_max;
	ulong id;
};

/**
 * struct aic_fixed_rate - A handle to (allowing control of) a fixed rate clock.
 *
 * Clocks such as HOSC, LOSC for ex., they just have a rate information.
 *
 * @id: The clock signal ID within the provider.
 * @rate: The clock rate (in HZ).
 */
struct aic_fixed_rate {
	u32 id;
	u32 rate;
};


/**
 * struct clk_aic_pll - A handle to (allowing control of) a pll clock.
 *
 * @id: The clock signal ID within the provider.
 * @gen_reg: Register address for general configuration.
 * @frac_reg: Register address for fractional configuration.
 * @sdm_reg: Register address for spread configuration.
 * @type: Type of the pll, defined as enum aic_pll_type.
 */
struct aic_pll {
	u32 id;
	u32 gen_reg;
	u32 frac_reg;
	u32 sdm_reg;
	enum aic_pll_type type;
	ulong min_rate;
	ulong max_rate;
};

/**
 * struct aic_sys_clk - A handle to (allowing control of) a system module clock.
 *
 * System modules such as CPU, BUS for ex. , they have a mux to to switch
 * parents.
 *
 * @id: The clock signal ID within the provider.
 * @reg: Register address for clock configuration.
 * @parent: Parents' id array.
 * @parent_cnt: count of parents in the parent array;
 * @mux_shift: bit shift for getting mux;
 * @mux_mask: bits mask for getting mux;
 * @div_shift: bit shift for getting dividor;
 * @div_mask: mask bits for getting dividor;
 */
struct aic_sys_clk {
	u32 id;
	u32 reg;
	u32 *parent;
	u8  parent_cnt;
	enum aic_sys_type type;
	u8  mux_shift;
	u8  mux_mask;
	s8  div_shift;
	u8  div_mask;
	void *clk_attr;
};

struct aic_cpu_attr {
	u32 key_val;
	s8 key_bit;
	u8 key_mask;
	s8 mod_gate;
	s8 rst_bit;
};

/**
 * struct aic_periph_clk - A handle to (allowing control of) a module clock.
 *
 * The periphral modules' clock has a fixed parent, a bus gate, a module
 * gate and a dividor.
 *
 * @id: The clock signal ID within the provider.
 * @parent: Parent list, the count should be matched with width of @mux_mask.
 * @reg: Register address for clock configuration.
 * @bus_gate: bit shift of bus gate;
 * @mod_gate: bit shift of module gate;
 * @div_shift: bit shift for getting dividor
 * @div_mask: bits mask for getting dividor
 */
struct aic_periph_clk {
	u32 id;
	u32 parent;
	u32 reg;
	s8  bus_gate;
	s8  mod_gate;
	s8  div_shift;
	u8  div_mask;
};

/**
 * struct aic_disp_clk - A handle to (allowing control of) a disp clock.
 *
 * @id: The clock signal ID within the provider.
 * @parent: Parent id
 * @reg: Register address for clock configuration.
 * @divn_shift: bit shift for SCLK divider
 * @divn_mask: bits mask for SCLK divider
 * @divm_shift: bit shift for pixclk factor m
 * @divm_mask: bits mask for pixclk factor m
 * @divl_shift: bit shift for pixclk factor l
 * @divl_mask: bits mask for pixclk factor l
 * @pix_divsel_shift: bit shift for pixclk selector
 * @pix_divsel_mask: bits mask for pixclk selector
 */
struct aic_disp_clk {
	u32 id;
	u32 parent;
	u32 reg;
	u8 divn_shift;
	u8 divn_mask;
	u8 divm_shift;
	u8 divm_mask;
	u8 divl_shift;
	u8 divl_mask;
	u8 pix_divsel_shift;
	u8 pix_divsel_mask;
};

/**
 * struct aic_clk_out - A handle to (allowing control of) a output clock.
 *
 * @id: The clock ID within the provider.
 * @parent: parents' id array.
 * @reg: Register address for clock configuration.
 * @parent_cnt: count of parents in array
 * @mux_shift: bit shift for get mux
 * @mux_mask: bits mask for get mux
 * @div?_shift: bit shift to get dividor
 * @div?_mask: bits mask for getting dividor
 */
struct aic_clk_out {
	u32 id;
	u32 *parent;
	u32 reg;
	u8  parent_cnt;
	u8  mux_shift;
	u8  mux_mask;
	s8  div0_shift;
	u8  div0_mask;
};

/**
 * struct aic_clk_crosszone - A handle to (allowing control of) a PRCM core clock.
 *
 * The crosszone clock has a fixed parent, a bus gate, a module
 * gate and a dividor.
 *
 * @id: The clock signal ID within the provider.
 * @parent: Parent list, the count should be matched with width of @mux_mask.
 * @reg: Register address for clock configuration.
 * @bus_gate: bit shift of bus gate;
 * @mod_gate: bit shift of module gate;
 * @div_shift: bit shift for getting dividor
 * @div_mask: bits mask for getting dividor
 */
struct aic_clk_crosszone {
	u32 id;
	u32 parent;
	u32 reg;
	s8  bus_gate;
	s8  mod_gate;
	s8  div_shift;
	u8  div_mask;
};

/**
 * struct aic_clk_tree - clock tree information.
 *
 * @fixed_rate_base: the first clock id of fixed rate clocks
 * @fixed_rate_end: the last clock id of fixed rate clocks
 * @fixed_factor_base: the first clock id of fixed factor clocks
 * @fixed_factor_end: the last clock id of fixed factor clocks
 * @pll_base: the first clock id of plls
 * @pll_end: the last clock id of plls
 * @system_base: the first clock id of system clocks
 * @system_end: the last clock id of system clocks
 * @periph_base: the first clock id of periph clocks
 * @periph_end: the last clock id of periph clocks
 * @disp_base: the first clock id of disp clocks
 * @disp_end: the last clock id of disp clocks
 * @clkout_base: the first clock id of output clocks
 * @clkout_end: the last clock id of output clocks
 * @fixed_rate: fixed rate clocks array
 * @fixed_factor: fixed factor clocks array
 * @plls: pll clocks array
 * @system: system clocks array
 * @periph: periph clocks array
 * @clkout: output clocks array
 */
struct aic_clk_tree {
	u16 fixed_rate_base;
	u16 fixed_rate_cnt;
	u16 pll_base;
	u16 pll_cnt;
	u16 system_base;
	u16 system_cnt;
	u16 periph_base;
	u16 periph_cnt;
	u16 disp_base;
	u16 disp_cnt;
	u16 clkout_base;
	u16 clkout_cnt;
#ifdef CONFIG_CLK_ARTINCHIP_CMU_V2_0
	u16 cross_zone_base;
	u16 cross_zone_cnt;
#endif
	struct aic_fixed_rate   *fixed_rate;
	struct aic_pll          *plls;
	struct aic_sys_clk      *system;
	struct aic_periph_clk   *periph;
	struct aic_disp_clk	*disp;
	struct aic_clk_out      *clkout;
#ifdef CONFIG_CLK_ARTINCHIP_CMU_V2_0
	struct aic_clk_crosszone *clk_cz;
#endif
};

struct aic_clk_ops {
	int (*enable)(struct clk *clk, int index);
	int (*disable)(struct clk *clk, int index);
	ulong (*get_rate)(struct clk *clk, int index);
	ulong (*set_rate)(struct clk *clk, ulong rate, int index);
	int (*set_parent)(struct clk *clk, struct clk *parent, int index);
	ulong (*round_rate)(struct clk *clk, ulong rate, int index);
};

struct aic_clk_priv {
	void *base;
	void *cz_base;
	struct aic_clk_tree *tree;
};

#define CLK_FIXED_RATE(_id, _rate)		{.id = _id, .rate = _rate}

#define CLK_PLL(_id, _gen_reg, _frac_reg, _sdm_reg, _type)                     \
	{                                                                      \
		.id = _id,                                                     \
		.gen_reg = _gen_reg,                                           \
		.frac_reg = _frac_reg,                                         \
		.sdm_reg = _sdm_reg,                                           \
		.type = _type                                                  \
	}

#define CLK_PLL_VIDEO(_id, _gen_reg, _frac_reg, _sdm_reg, _type, _min_rate,    \
		      _max_rate)					       \
	{								       \
		.id = _id,                                                     \
		.gen_reg = _gen_reg,                                           \
		.frac_reg = _frac_reg,                                         \
		.sdm_reg = _sdm_reg,                                           \
		.type = _type,                                                 \
		.min_rate = _min_rate,					       \
		.max_rate = _max_rate,					       \
	}

#define CLK_SYS(_id, _reg, _parent, _parent_cnt, _type, _mux_shift, _mux_width, \
		_div_shift, _div_width, _clk_attr)       \
	{                                                                  \
		.id = _id,                                                     \
		.reg = _reg,                                                   \
		.parent = _parent,                                             \
		.parent_cnt = _parent_cnt,                                     \
		.type = _type,							\
		.mux_shift = _mux_shift,                                       \
		.mux_mask = BIT(_mux_width) - 1,                               \
		.div_shift = _div_shift,                                       \
		.div_mask = BIT(_div_width) - 1,                               \
		.clk_attr = _clk_attr,						\
	}
#define CLK_SYS_BUS(_id, _reg, _parent, _parent_cnt, _mux_shift, _mux_width,	\
		_div_shift, _div_width)						\
	CLK_SYS(_id, _reg, _parent, _parent_cnt, AIC_BUS_CLK, _mux_shift, _mux_width, \
		_div_shift, _div_width, NULL)

#define CLK_SYS_CPU(_id, _reg, _parent, _parent_cnt, _mux_shift, _mux_width,	\
		_div_shift, _div_width, _clk_attr)				\
	CLK_SYS(_id, _reg, _parent, _parent_cnt, AIC_CPU_CLK, _mux_shift, _mux_width, \
		_div_shift, _div_width, _clk_attr)

#define CLK_CPU_ATTR(_name, _key_val, _key_bit, _key_mask, _rst_bit, _mod_gate)	\
	static struct aic_cpu_attr _name = {						\
		.key_val = _key_val,						\
		.key_bit = _key_bit,						\
		.key_mask = _key_mask,					\
		.rst_bit = _rst_bit,						\
		.mod_gate = _mod_gate,						\
	}

#define CLK_PERIPH(_id, _parent, _reg, _bus_gate, _mod_gate, _div_shift,       \
		   _div_width)                                                 \
	{                                                                      \
		.id = _id,                                                     \
		.parent = _parent,                                             \
		.reg = _reg,                                                   \
		.bus_gate = _bus_gate,                                         \
		.mod_gate = _mod_gate,                                         \
		.div_shift = _div_shift,                                       \
		.div_mask = BIT(_div_width) - 1                                \
	}

#define CLK_DISP(_id, _parent, _reg, _divn_shift, _divn_width,		       \
		 _divm_shift, _divm_width,				       \
		 _divl_shift, _divl_width,				       \
		 _pix_divsel_shift, _pix_divsel_width)			       \
	{								       \
		.id = _id,						       \
		.parent = _parent,					       \
		.reg = _reg,						       \
		.divn_shift = _divn_shift,				       \
		.divn_mask = BIT(_divn_width) - 1,			       \
		.divm_shift = _divm_shift,				       \
		.divm_mask = BIT(_divm_width) - 1,			       \
		.divl_shift = _divl_shift,				       \
		.divl_mask = BIT(_divl_width) - 1,			       \
		.pix_divsel_shift = _pix_divsel_shift,			       \
		.pix_divsel_mask = BIT(_pix_divsel_width) - 1,		       \
	}

#define CLK_OUT(_id, _parent, _reg, _parent_cnt, _mux_shift, _mux_mask,        \
		_div0_shift, _div0_width)                                      \
	{                                                                      \
		.id = _id,                                                     \
		.parent = _parent,                                             \
		.reg = _reg,                                                   \
		.parent_cnt = _parent_cnt,                                     \
		.mux_shift = _mux_shift,                                       \
		.mux_mask = BIT(_mux_mask) - 1,                                \
		.div0_shift = _div0_shift,                                     \
		.div0_mask = BIT(_div0_width) - 1,                             \
	}

int aic_clk_common_init(struct udevice *dev, struct aic_clk_tree *tree);
extern const struct clk_ops artinchip_clk_ops;

#endif	/* __DRV_CLK_AIC_H */
