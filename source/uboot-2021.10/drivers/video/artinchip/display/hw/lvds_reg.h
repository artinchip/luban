/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2020-2021 ArtInChip Technology Co., Ltd.
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#ifndef _LVDS_REG_H_
#define _LVDS_REG_H_

#include <linux/io.h>
#include <linux/types.h>

struct lvds_info {
	u32 pols;
	u32 phys;
	u32 swap;
	u32 link_swap;
};

/* lvds common mode voltage enum */
enum lvds_com_mode_volt {
	LVDS_CMV_1_10V  = 0x0, /* 1.10V */
	LVDS_CMV_1_19V  = 0x1, /* 1.19V */
	LVDS_CMV_1_30V  = 0x2, /* 1.30V */
	LVDS_CMV_1_43V  = 0x3  /* 1.43V */
};

/* lvds waveform slope ajust enum */
enum lvds_waveform_ajust {
	LVDS_WSA_0_8MA  = 0x0, /* 0.8mA */
	LVDS_WSA_1_0MA  = 0x1, /* 1.0mA */
	LVDS_WSA_1_2MA  = 0x2, /* 1.2mA */
	LVDS_WSA_1_4MA  = 0x3 /* 1.4mA */
};

/* lvds differential mode voltage enum */
enum lvds_diff_mode_volt {
	LVDS_DMV_250MV  = 0x0, /* 250mV */
	LVDS_DMV_300MV  = 0x1, /* 300mV */
	LVDS_DMV_350MV  = 0x2, /* 350mV */
	LVDS_DMV_400MV  = 0x3  /* 400mV */
};

enum lvds_swap {
	LVDS_D0  = 0x0,
	LVDS_D1  = 0x1,
	LVDS_D2  = 0x2,
	LVDS_CK  = 0x3,
	LVDS_D3  = 0x4
};

#define  LVDS_CTL_MODE_MASK          GENMASK(9, 8)
#define  LVDS_CTL_MODE(x)            (((x) & 0x3) << 8)
#define  LVDS_CTL_LINK_MASK          GENMASK(5, 4)
#define  LVDS_CTL_LINK(x)            (((x) & 0x3) << 4)
#define  LVDS_CTL_SWAP_MASK          BIT(2)
#define  LVDS_CTL_SWAP_EN(x)         (((x) & 0x1) << 2)
#define  LVDS_CTL_SYNC_MODE_MASK     BIT(1)
#define  LVDS_CTL_SYNC_MODE_EN(x)    (((x) & 0x1) << 1)
#define  LVDS_CTL_EN                 BIT(0)

#define  LVDS_SWAP_D3_SEL_MASK      GENMASK(18, 16)
#define  LVDS_SWAP_D3_SEL(x)        (((x) & 0x7) << 16)

#define  LVDS_SWAP_CK_SEL_MASK      GENMASK(14, 12)
#define  LVDS_SWAP_CK_SEL(x)        (((x) & 0x7) << 12)

#define  LVDS_SWAP_D2_SEL_MASK      GENMASK(10, 8)
#define  LVDS_SWAP_D2_SEL(x)        (((x) & 0x7) << 8)

#define  LVDS_SWAP_D1_SEL_MASK      GENMASK(6, 4)
#define  LVDS_SWAP_D1_SEL(x)        (((x) & 0x7) << 4)

#define  LVDS_SWAP_D0_SEL_MASK      GENMASK(2, 0)
#define  LVDS_SWAP_D0_SEL(x)        (((x) & 0x7) << 0)

#define  LVDS_POL_D3_INVERSE         BIT(4)
#define  LVDS_POL_CK_INVERSE         BIT(3)
#define  LVDS_POL_D2_INVERSE         BIT(2)
#define  LVDS_POL_D1_INVERSE         BIT(1)
#define  LVDS_POL_D0_INVERSE         BIT(0)

#define LVDS_PHY_D3_EN               BIT(16)
#define LVDS_PHY_CK_EN               BIT(15)
#define LVDS_PHY_D2_EN               BIT(14)
#define LVDS_PHY_D1_EN               BIT(13)
#define LVDS_PHY_D0_EN               BIT(12)

#define LVDS_PHY_EN_MB               BIT(7)
#define LVDS_PHY_EN_LDO              BIT(6)

/* common mode voltage */
#define LVDS_COMMON_MODE_VOLT_MASK   GENMASK(5, 4)
#define LVDS_COMMON_MODE_VOLT(x)     (((x) & 0x3) << 4)

#define LVDS_WAVEFORM_SLOPE_MASK     GENMASK(3, 2)
#define LVDS_WAVEFORM_SLOPE(x)       (((x) & 0x3) << 2)

/* differential mode voltage */
#define LVDS_DIFF_MODE_VOLT_MASK     GENMASK(1, 0)
#define LVDS_DIFF_MODE_VOLT(x)       (((x) & 0x3) << 0)

#define  LVDS_CTL                    0x00
#define  LVDS_CK_CFG                 0x10
#define  LVDS_0_SWAP                 0x20
#define  LVDS_1_SWAP                 0x24
#define  LVDS_0_POL_CTL              0x28
#define  LVDS_1_POL_CTL              0x2C
#define  LVDS_0_PHY_CTL              0x30
#define  LVDS_1_PHY_CTL              0x34
#define  LVDS_VERSION                0xFC

#endif /*_LVDS_REG_H_ */
