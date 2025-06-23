/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2020-2022 ArtInChip Technology Co., Ltd.
 * Authors:  matteo <duanmt@artinchip.com>
 */

#ifndef _MIPI_DSI_REG_V1_0_H_
#define _MIPI_DSI_REG_V1_0_H_

#include <linux/fb.h>

#define DSI_MAX_LANE_NUM	4

#define DSI_CTL_PKG_CFG_CRC_RX_EN	BIT(7)
#define DSI_CTL_PKG_CFG_ECC_RX_EN	BIT(6)
#define DSI_CTL_PKG_CFG_BTA_EN		BIT(5)
#define DSI_CTL_PKG_CFG_EOTP_RX_EN	BIT(4)
#define DSI_CTL_PKG_CFG_EOTP_TX_EN	BIT(3)
#define DSI_CTL_DSI_MODE		BIT(1)
#define DSI_CTL_EN			BIT(0)

#define DSI_CLK_CFG_TO_DIV_MASK		GENMASK(15, 8)
#define DSI_CLK_CFG_TO_DIV(x)		(((x) & 0xFF) << 8)
#define DSI_CLK_CFG_LP_DIV_MASK		GENMASK(7, 0)
#define DSI_CLK_CFG_LP_DIV(x)		(((x) & 0xFF) << 0)

#define DSI_DPI_VC_NUM_MASK		GENMASK(1, 0)
#define DSI_DPI_VC_NUM(x)		(((x) & 0x3) << 0)

#define DSI_DPI_IN_FMT_LOOSELY18	BIT(8)
#define DSI_DPI_IN_FMT_DT_MASK		GENMASK(3, 0)
#define DSI_DPI_IN_FMT_DT(x)		(((x) & 0xF) << 0)

#define DSI_DPI_IN_POL_COLORM		BIT(4)
#define DSI_DPI_IN_POL_SHUTDOWN		BIT(3)
#define DSI_DPI_IN_POL_HSYNC		BIT(2)
#define DSI_DPI_IN_POL_VSYNC		BIT(1)
#define DSI_DPI_IN_POL_DE		BIT(0)

#define DSI_POL_HIGH_ACTIVE		0
#define DSI_POL_LOW_ACTIVE		1

#define DSI_DPI_LPTX_TIME_OUTVACT_MASK	GENMASK(24, 16)
#define DSI_DPI_LPTX_TIME_OUTVACT(x)	(((x) & 0xFF) << 16)
#define DSI_DPI_LPTX_TIME_INVACT_MASK	GENMASK(7, 0)
#define DSI_DPI_LPTX_TIME_INVACT(x)	(((x) & 0xFF) << 0)

#define DSI_VID_MODE_CFG_TEST_DIR		BIT(24)
#define DSI_VID_MODE_CFG_TEST_MODE		BIT(20)
#define DSI_VID_MODE_CFG_TEST_EN		BIT(16)
#define DSI_VID_MODE_CFG_CMD_LPTX_FORCE		BIT(15)
#define DSI_VID_MODE_CFG_FRAME_BTA_ACK_EN	BIT(14)
#define DSI_VID_MODE_CFG_LP_EN_HFP		BIT(13)
#define DSI_VID_MODE_CFG_LP_EN_HBP		BIT(12)
#define DSI_VID_MODE_CFG_LP_EN_VACT		BIT(11)
#define DSI_VID_MODE_CFG_LP_EN_VFP		BIT(10)
#define DSI_VID_MODE_CFG_LP_EN_VBP		BIT(9)
#define DSI_VID_MODE_CFG_LP_EN_VSA		BIT(8)
#define DSI_VID_MODE_CFG_TYPE_MASK		GENMASK(1, 0)
#define DSI_VID_MODE_CFG_TYPE(x)		((((x) >> 1) & 0x3) << 0)

#define DSI_VID_HBP_TIME_MASK			GENMASK(27, 16)
#define DSI_VID_HBP_TIME(x)			(((x) & 0xFFF) << 16)
#define DSI_VID_HSA_TIME_MASK			GENMASK(11, 0)
#define DSI_VID_HSA_TIME(x)			(((x) & 0xFFF) << 0)

#define DSI_VID_VFP_LINE_MASK			GENMASK(25, 16)
#define DSI_VID_VFP_LINE(x)			(((x) & 0x3FF) << 16)
#define DSI_VID_VBP_TIME_MASK			GENMASK(9, 0)
#define DSI_VID_VBP_TIME(x)			(((x) & 0x3FF) << 0)

#define DSI_VID_VSA_LINE_MASK			GENMASK(25, 16)
#define DSI_VID_VSA_LINE(x)			(((x) & 0x3FF) << 16)
#define DSI_VID_VACT_TIME_MASK			GENMASK(13, 0)
#define DSI_VID_VACT_TIME(x)			(((x) & 0x3FFF) << 0)

#define DSI_CMD_MODE_CFG_PKG_TX_HS		0
#define DSI_CMD_MODE_CFG_PKG_TX_LP		1
#define DSI_CMD_MODE_CFG_MAX_RD_PKG_SIZE	BIT(24)
#define DSI_CMD_MODE_CFG_DCS_LW			BIT(19)
#define DSI_CMD_MODE_CFG_DCS_SR_0P		BIT(18)
#define DSI_CMD_MODE_CFG_DCS_SW_1P		BIT(17)
#define DSI_CMD_MODE_CFG_DCS_SW_0P		BIT(16)
#define DSI_CMD_MODE_CFG_GEN_LW			BIT(14)
#define DSI_CMD_MODE_CFG_GEN_SR_2P		BIT(13)
#define DSI_CMD_MODE_CFG_GEN_SR_1P		BIT(12)
#define DSI_CMD_MODE_CFG_GEN_SR_0P		BIT(11)
#define DSI_CMD_MODE_CFG_GEN_SW_2P		BIT(10)
#define DSI_CMD_MODE_CFG_GEN_SW_1P		BIT(9)
#define DSI_CMD_MODE_CFG_GEN_SW_0P		BIT(8)
#define DSI_CMD_MODE_CFG_ACK_REQ_EN		BIT(1)
#define DSI_CMD_MODE_CFG_TE_EN			BIT(0)

#define DSI_CMD_PKG_STA_RD_CMD_BUSY		BIT(6)
#define DSI_CMD_PKG_STA_PLD_R_FULL		BIT(5)
#define DSI_CMD_PKG_STA_PLD_R_EMPTY		BIT(4)
#define DSI_CMD_PKG_STA_PLD_W_FULL		BIT(3)
#define DSI_CMD_PKG_STA_PLD_W_EMPTY_SHIFT	2
#define DSI_CMD_PKG_STA_PLD_W_EMPTY		BIT(2)
#define DSI_CMD_PKG_STA_CMD_FULL		BIT(1)
#define DSI_CMD_PKG_STA_CMD_EMPTY		BIT(0)

#define DSI_TO_CNT_CFG_HSTX_MASK	GENMASK(31, 16)
#define DSI_TO_CNT_CFG_HSTX(x)		(((x) & 0x3FF) << 16)
#define DSI_TO_CNT_CFG_LPRX_MASK	GENMASK(15, 0)
#define DSI_TO_CNT_CFG_LPRX(x)		(((x) & 0x3FF) << 0)

#define DSI_PHY_CLK_TIME_HS2LP_MASK	GENMASK(25, 16)
#define DSI_PHY_CLK_TIME_HS2LP(x)	(((x) & 0x1FF) << 16)
#define DSI_PHY_CLK_TIME_LP2HS_MASK	GENMASK(9, 0)
#define DSI_PHY_CLK_TIME_LP2HS(x)	(((x) & 0x1FF) << 0)

#define DSI_PHY_DATA_TIME_HS2LP_MASK	GENMASK(25, 16)
#define DSI_PHY_DATA_TIME_HS2LP(x)	(((x) & 0x1FF) << 16)
#define DSI_PHY_DATA_TIME_LP2HS_MASK	GENMASK(9, 0)
#define DSI_PHY_DATA_TIME_LP2HS(x)	(((x) & 0x1FF) << 0)

#define DSI_PHY_CFG_RST_PLL_FORCE		BIT(7)
#define DSI_PHY_CFG_RST_CLK_EN			BIT(6)
#define DSI_PHY_CFG_RST_RSTN			BIT(5)
#define DSI_PHY_CFG_RST_SHUTDOWNZ		BIT(4)
#define DSI_PHY_CFG_AUTO_CLK_EN			BIT(3)
#define DSI_PHY_CFG_HSCLK_REQ			BIT(2)

#define DSI_PHY_CFG_LP11_TIME_SHIFT	8
#define DSI_PHY_CFG_LP11_TIME_MASK	GENMASK(15, 8)
#define DSI_PHY_CFG_LP11_TIME(x)	(((x) & 0xFF) << 8)
#define DSI_PHY_CFG_DATA_LANE_MASK	GENMASK(1, 0)
#define DSI_PHY_CFG_DATA_LANE(x)	(((x) & 0x3) << 0)

#define DSI_PHY_STA_ULPS_ACTIVE_NOT_4	BIT(12)
#define DSI_PHY_STA_STOP_STATE_3	BIT(11)
#define DSI_PHY_STA_ULPS_ACTIVE_NOT_2	BIT(10)
#define DSI_PHY_STA_STOP_STATE_2	BIT(9)
#define DSI_PHY_STA_ULPS_ACTIVE_NOT_1	BIT(8)
#define DSI_PHY_STA_STOP_STATE_1	BIT(7)
#define DSI_PHY_STA_RXULPS_ESC_0	BIT(6)
#define DSI_PHY_STA_ULPS_ACTIVE_NOT_0	BIT(5)
#define DSI_PHY_STA_STOP_STATE_0_SHIFT	4
#define DSI_PHY_STA_STOP_STATE_0	BIT(4)
#define DSI_PHY_STA_ULPS_ACTIVE_NOT_C	BIT(3)
#define DSI_PHY_STA_STOP_STATE_C_SHIFT	2
#define DSI_PHY_STA_STOP_STATE_C	BIT(2)
#define DSI_PHY_STA_PHY_DIRECTION	BIT(1)
#define DSI_PHY_STA_PHY_LOCK		BIT(0)

#define DSI_PHY_TEST1_CLK		BIT(1)
#define DSI_PHY_TEST1_CLR		BIT(0)

#define DSI_PHY_TEST2_EN		BIT(16)
#define DSI_PHY_TEST2_DOUT_MASK		GENMASK(15, 8)
#define DSI_PHY_TEST2_DOUT(x)		(((x) & 0xFF) << 8)
#define DSI_PHY_TEST2_DIN_MASK		GENMASK(7, 0)
#define DSI_PHY_TEST2_DIN(x)		(((x) & 0xFF) << 0)

#define DSI_DPI_IF_CFG_COLORM		BIT(3)
#define DSI_DPI_IF_CFG_SHUTD		BIT(2)
#define DSI_DPI_IF_CFG_FMT_MASK		GENMASK(1, 0)
#define DSI_DPI_IF_CFG_FMT(x)		(((x) & 0x3) << 0)

#define DSI_ANA_CFG2_EN_CLK		BIT(12)
#define DSI_ANA_CFG2_RCAL_SHIFT		10
#define DSI_ANA_CFG2_RCAL_FLAG		BIT(10)
#define DSI_ANA_CFG2_ON_RESCAL		BIT(9)
#define DSI_ANA_CFG2_EN_RESCAL		BIT(8)
#define DSI_ANA_CFG2_EN_LDO_MASK	GENMASK(7, 3)
#define DSI_ANA_CFG2_EN_LDO(x)		(((x) & 0x1F) << 3)
#define DSI_ANA_CFG2_EN_VP_MASK		GENMASK(2, 1)
#define DSI_ANA_CFG2_EN_VP(x)		(((x) & 0x3) << 1)
#define DSI_ANA_CFG2_EN_BIAS		BIT(0)

#define DSI_DATA_LANE_POL_MASK		GENMASK(19, 16)
#define DSI_DATA_LANE_POL(x)		(((x) & 0xF) << 16)
#define DSI_DATA_CLK_POL		BIT(15)

/* The register of MIPI DSI controller */
#define DSI_CTL			0x00
#define DSI_CLK_CFG		0x04
#define DSI_DPI_IN_POL		0x10
#define DSI_DPI_IN_FMT		0x14
#define DSI_DPI_VC		0x18
#define DSI_DPI_LPTX_TIME	0x1C
#define DSI_GEN_PH_CFG		0x20
#define DSI_GEN_PD_CFG		0x24
#define DSI_GEN_VC_RX		0x28
#define DSI_VID_MODE_CFG	0x30
#define DSI_VID_PKG_SIZE	0x34
#define DSI_VID_CHK_NUM		0x38
#define DSI_VID_NULL_SIZE	0x3C
#define DSI_VID_HINACT_TIME	0x40
#define DSI_VID_HT_TIME		0x44
#define DSI_VID_VBLANK_LINE	0x48
#define DSI_VID_VACT_LINE	0x4C
#define DSI_CMD_MODE_CFG	0x50
#define DSI_EDPI_CMD_SIZE	0x54
#define DSI_CMD_PKG_STA		0x58
#define DSI_IRQ_EN1		0x60
#define DSI_IRQ_EN2		0x64
#define DSI_IRQ_STA1		0x68
#define DSI_IRQ_STA2		0x6C
#define DSI_TO_CNT_CFG		0x70
#define DSI_TO_RD		0x74
#define DSI_TO_WR		0x78
#define DSI_TO_BTA		0x7C
#define DSI_PHY_CFG		0x80
#define DSI_PHY_STA		0x84
#define DSI_PHY_CLK_TIME	0x88
#define DSI_PHY_DATA_TIME	0x8C
#define DSI_PHY_RD_TIME		0x90
#define DSI_PHY_TEST1		0xC0
#define DSI_PHY_TEST2		0xC4
#define DSI_DPI_IF_CFG		0x400
#define DSI_LANE_CFG		0x410
#define DSI_ANA_CFG1		0x500
#define DSI_ANA_CFG2		0x504
#define DSI_ANA_CFG3		0x508
#define DSI_ANA_CFG4		0x50C

void dsi_set_lane_assign(void __iomem *base, u32 ln_assign);
void dsi_set_lane_polrs(void __iomem *base, u32 ln_polrs);
void dsi_set_data_clk_polrs(void __iomem *base, u32 dc_inv);

void dsi_set_clk_div(void __iomem *base, ulong mclk);
void dsi_pkg_init(void __iomem *base);
void dsi_phy_init(void __iomem *base, ulong mclk, u32 lane, enum dsi_mode mode);
void dsi_hs_clk(void __iomem *base, u32 enable);
void dsi_set_vm(void __iomem *base, enum dsi_mode mode, enum dsi_format format,
		u32 lane, u32 vc, struct videomode *vm);
void dsi_cmd_wr(void __iomem *base, u32 dt, u32 vc, const u8 *data, u32 len);
void dsi_dcs_lw(void __iomem *base, u32 enable);

#endif // end of _MIPI_DSI_REG_V1_0_H_
