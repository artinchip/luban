// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2022 ArtInChip Technology Co., Ltd.
 * Authors:  matteo <duanmt@artinchip.com>
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>

#include "aic_dvp.h"

void aic_dvp_reg_enable(void __iomem *base,
				int offset, int bit, int enable)
{
	int tmp;

	tmp = readl(base + offset);
	tmp &= ~bit;
	if (enable)
		tmp |= bit;

	writel(tmp, base + offset);
}

void aic_dvp_enable(struct aic_dvp *dvp, int enable)
{
	if (!dvp->cfg.field) {
		aic_dvp_reg_enable(dvp->regs, DVP_CTL, DVP_CTL_DROP_FRAME_EN,
				   enable);
	}
	aic_dvp_reg_enable(dvp->regs, DVP_CTL, DVP_CTL_EN, enable);
}

void aic_dvp_capture_start(struct aic_dvp *dvp)
{
	writel(DVP_OUT_CTL_CAP_ON, dvp->regs + DVP_OUT_CTL(dvp->ch));
}

void aic_dvp_capture_stop(struct aic_dvp *dvp)
{
	writel(0, dvp->regs + DVP_OUT_CTL(dvp->ch));
}

void aic_dvp_clr_fifo(struct aic_dvp *dvp)
{
	aic_dvp_reg_enable(dvp->regs, DVP_CTL, DVP_CTL_CLR, 1);
}

int aic_dvp_clr_int(struct aic_dvp *dvp)
{
	int sta = readl(dvp->regs + DVP_IRQ_STA(dvp->ch));

	writel(sta, dvp->regs + DVP_IRQ_STA(dvp->ch));
	return sta;
}

void aic_dvp_enable_int(struct aic_dvp *dvp, int enable)
{
	/* When HNUM IRQ happened, so we can update the address */
	if (enable)
		writel((dvp->cfg.height / 4) << DVP_IRQ_CFG_HNUM_SHIFT,
		       dvp->regs + DVP_IRQ_CFG(dvp->ch));

	aic_dvp_reg_enable(dvp->regs, DVP_IRQ_EN(dvp->ch),
			   DVP_IRQ_EN_FRAME_DONE | DVP_IRQ_EN_HNUM, enable);
}

void aic_dvp_set_pol(struct aic_dvp *dvp)
{
	struct v4l2_fwnode_bus_parallel *bus = &dvp->bus;
	u32 field_pol, vref_pol, href_pol, pclk_pol;

	dev_dbg(dvp->dev, "%s() flags: %#x\n", __func__, bus->flags);

	field_pol = dvp->cfg.field_active ? 0 : DVP_IN_CFG_FILED_POL_ACTIVE_LOW;
	vref_pol = bus->flags & V4L2_MBUS_VSYNC_ACTIVE_LOW ?
				DVP_IN_CFG_VSYNC_POL_FALLING : 0;
	href_pol = bus->flags & V4L2_MBUS_HSYNC_ACTIVE_HIGH ?
				DVP_IN_CFG_HREF_POL_ACTIVE_HIGH : 0;
	pclk_pol = bus->flags & V4L2_MBUS_PCLK_SAMPLE_FALLING ?
				DVP_IN_CFG_PCLK_POL_FALLING : 0;
	writel(href_pol	| vref_pol | pclk_pol | field_pol,
	       dvp->regs + DVP_IN_CFG(dvp->ch));
}

void aic_dvp_set_cfg(struct aic_dvp *dvp)
{
	u32 height, stride0, stride1, val;

	WARN_ON((dvp->cfg.stride[0] == 0) || (dvp->cfg.stride[1] == 0));
	if (dvp->cfg.field) {
		height  = dvp->cfg.height / 2;
		stride0 = dvp->cfg.stride[0] * 2;
		stride1 = dvp->cfg.stride[1] * 2;
	} else {
		height  = dvp->cfg.height;
		stride0 = dvp->cfg.stride[0];
		stride1 = dvp->cfg.stride[1];
	}

	val = DVP_CTL_IN_FMT(dvp->cfg.input)
		| DVP_CTL_IN_SEQ(dvp->cfg.input_seq)
		| DVP_CTL_OUT_FMT(dvp->cfg.output)
		| DVP_CTL_EN;
	if (!dvp->cfg.field)
		val |= DVP_CTL_DROP_FRAME_EN;
	writel(val, dvp->regs + DVP_CTL);

	writel(DVP_OUT_HOR_NUM(dvp->cfg.width, 0),
	       dvp->regs + DVP_OUT_HOR_SIZE(dvp->ch));
	writel(DVP_OUT_VER_NUM(height, 0),
	       dvp->regs + DVP_OUT_VER_SIZE(dvp->ch));

	writel(stride0, dvp->regs + DVP_OUT_LINE_STRIDE0(dvp->ch));
	writel(stride1, dvp->regs + DVP_OUT_LINE_STRIDE1(dvp->ch));
}

void aic_dvp_update_buf_addr(struct aic_dvp *dvp, struct aic_dvp_buf *buf,
			     u32 offset)
{
	WARN_ON((buf->paddr[0] == 0) || (buf->paddr[1] == 0));
	writel(buf->paddr[0] + offset, dvp->regs + DVP_OUT_ADDR_BUF(dvp->ch, 0));
	writel(buf->paddr[1] + offset, dvp->regs + DVP_OUT_ADDR_BUF(dvp->ch, 1));
}

void aic_dvp_update_ctl(struct aic_dvp *dvp)
{
	writel(1, dvp->regs + DVP_OUT_UPDATE_CTL(dvp->ch));
}

static int g_top_field;

u32 aic_dvp_get_current_xy(struct aic_dvp *dvp)
{
	u32 val = readl(dvp->regs + DVP_IN_HOR_SIZE(dvp->ch));

	g_top_field = val & DVP_IN_HOR_SIZE_XY_CODE_F ? 0 : 1;
	dev_dbg(dvp->dev, "XY: %#x, is %s\n",
		(u32)(val & DVP_IN_HOR_SIZE_XY_CODE_MASK),
		g_top_field ? "Top" : "Bottom");

	return g_top_field;
}

u32 aic_dvp_is_top_field(void)
{
	return g_top_field;
}

u32 aic_dvp_is_bottom_field(void)
{
	return !g_top_field;
}

void aic_dvp_field_tag_clr(void)
{
	g_top_field = 0;
}
