// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2022, ArtInChip Technology Co., Ltd
 * Dehuang Wu <dehuang.wu@artinchip.com>
 */

#include <common.h>
#include <asm/io.h>
#include <dm.h>
#include <dm/device.h>
#include <dm/device_compat.h>
#include <clk.h>
#include <reset.h>
#include <misc.h>

#ifndef CONFIG_SPL_BUILD

#define SID_MAX_WORDS 64
#define SID_REG_CTL 0x0
#define SID_REG_ADDR 0x4
#define SID_REG_WDATA 0x8
#define SID_REG_RDATA 0xC
#define SID_REG_TIMING 0x10
#define SID_REG_VERSION 0xFC

struct aic_sid_platdata {
	void __iomem *base;
	struct clk clk;
	struct reset_ctl reset;
	u32 max_words;
};

#define SID_VERSION_200 0x200

#define SID_OPCODE_OFS 16
#define SID_OPCODE_MSK (0xFFF << SID_OPCODE_OFS)
#define SID_OPCODE 0xA1C

#define SID_STS_OFS 8
#define SID_STS_MSK (0x1FU << SID_STS_OFS)

#define SID_READ_START_OFS 4
#define SID_READ_START_MSK (0x1 << SID_READ_START_OFS)

#define SID_WRITE_START_OFS 0
#define SID_WRITE_START_MSK (0x1 << SID_WRITE_START_OFS)

static int wait_sid_ready(struct aic_sid_platdata *plat)
{
	u32 val, msk, idle = 0;
	s32 i;

	val = readl(plat->base + SID_REG_VERSION);
	if (0x200 != val)
		idle = 2;

	msk = SID_STS_MSK | SID_READ_START_MSK | SID_WRITE_START_MSK;
	for (i = 1000; i > 0; i--) {
		val = readl(plat->base + SID_REG_CTL);
		if ((val & msk) == (idle << SID_STS_OFS))
			return 0;
	}

	return(-1);
}

static u32 aic_sid_read_word(struct aic_sid_platdata *plat, u32 wid)
{
	u32 addr, rval = 0, val = 0;

	for (int i = 0; i < 2; i++) {
		addr = (wid + plat->max_words * i) << 2;
		writel(addr, plat->base + SID_REG_ADDR);

		val = readl(plat->base + SID_REG_CTL);
		val &= ~(SID_OPCODE_MSK | SID_READ_START_MSK);
		val |= ((SID_OPCODE << SID_OPCODE_OFS) | (SID_READ_START_MSK));
		writel(val, plat->base + SID_REG_CTL);

		/* Wait read finish */
		while ((readl(plat->base + SID_REG_CTL) & SID_READ_START_MSK))
			;

		rval |= readl(plat->base + SID_REG_RDATA);
	}

	return rval;
}

static int aic_sid_write_word(struct aic_sid_platdata *plat, u32 wid, u32 wval)
{
	u32 addr, val;

	for (int i = 0; i < 2; i++) {
		addr = (wid + plat->max_words * i) << 2;
		writel(addr, plat->base + SID_REG_ADDR);
		writel(wval, plat->base + SID_REG_WDATA);

		val = readl(plat->base + SID_REG_CTL);
		val &= ~(SID_OPCODE_MSK);
		val &= ~(SID_WRITE_START_MSK);
		val |= ((SID_OPCODE << SID_OPCODE_OFS) | (SID_WRITE_START_MSK));
		writel(val, plat->base + SID_REG_CTL);
		//udelay(10);

		/* Wait write done */
		while ((readl(plat->base + SID_REG_CTL) & SID_WRITE_START_MSK))
			;
	}

	return 0;
}

static int aic_sid_read(struct udevice *dev, int offset, void *buf, int size)
{
	struct aic_sid_platdata *plat = dev_get_plat(dev);
	u32 val, wid, ofs, end, end_siz;
	u8 *p;

	if (offset < 0 || (offset + size) > (4 * plat->max_words)) {
		pr_err("Invalid offset %d.\n", offset);
		return -EINVAL;
	}

	if (wait_sid_ready(plat)) {
		pr_err("Failed to wait SID status ready.\n");
		return -1;
	}

	wid = offset >> 2;
	ofs = offset % 4;
	end = (offset + size + 3) >> 2;
	end_siz = (offset + size) % 4;
	p = buf;
	while (wid < end) {
		val = aic_sid_read_word(plat, wid);
		if (wid == (end - 1) && end_siz) {
			memcpy(p, ((u8 *)&val) + ofs, end_siz);
			p += end_siz;
		} else {
			memcpy(p, ((u8 *)&val) + ofs, 4 - ofs);
			p += (4 - ofs);
		}
		wid++;
		ofs = 0;
	}

	return size;
}

static int aic_sid_write(struct udevice *dev, int offset, const void *buf,
			 int size)
{
	struct aic_sid_platdata *plat = dev_get_plat(dev);
	u32 val, wid, ofs, end, end_siz, cpsiz;
	const u8 *p;

	if (offset < 0 || (offset + size) > (4 * plat->max_words)) {
		pr_err("Invalid offset %d.\n", offset);
		return -EINVAL;
	}

	if (wait_sid_ready(plat)) {
		pr_err("Failed to wait SID status ready.\n");
		return -1;
	}

	val = 0;
	wid = offset >> 2;
	ofs = offset % 4;
	end = (offset + size + 3) >> 2;
	end_siz = (offset + size) % 4;
	p = buf;

	while (wid < end) {
		if (wid == (end - 1) && end_siz) {
			cpsiz = end_siz;
			if (cpsiz > size)
				cpsiz = size;
			memcpy(((u8 *)&val) + ofs, p, cpsiz);
			p += cpsiz;
		} else {
			cpsiz = 4 - ofs;
			if (cpsiz > size)
				cpsiz = size;
			memcpy(((u8 *)&val) + ofs, p, cpsiz);
			p += (4 - ofs);
		}
		aic_sid_write_word(plat, wid, val);
		ofs = 0;
		wid++;
	}

	return size;
}

static inline int aic_sid_set_clock(struct udevice *dev, bool enable)
{
	int ret = 0;
#if !defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_CLK_ARTINCHIP)
	struct aic_sid_platdata *plat = dev_get_plat(dev);

	if (!enable) {
		clk_disable(&plat->clk);
		if (reset_valid(&plat->reset))
			reset_assert(&plat->reset);
		return 0;
	}

	ret = clk_enable(&plat->clk);
	if (ret) {
		dev_err(dev, "failed to enable clock (ret=%d)\n", ret);
		return ret;
	}

	if (reset_valid(&plat->reset)) {
		ret = reset_deassert(&plat->reset);
		if (ret) {
			dev_err(dev, "failed to deassert reset\n");
			goto err;
		}
	}
	return 0;

err:
	clk_disable(&plat->clk);
#endif
	return ret;
}

static int aic_sid_probe(struct udevice *dev)
{
	struct aic_sid_platdata *plat = dev_get_plat(dev);
	ofnode node = dev_ofnode(dev);
	int ret = 0;
	u32 timing = 0;

#if !defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_CLK_ARTINCHIP)
	ret = clk_get_by_index(dev, 0, &plat->clk);
	if (ret < 0)
		return ret;

	ret = reset_get_by_index(dev, 0, &plat->reset);
	if (ret && ret != -ENOENT) {
		dev_err(dev, "failed to get reset\n");
		return ret;
	}
#endif

	plat->base = (void *)devfdt_get_addr(dev);

	ret = aic_sid_set_clock(dev, true);

	if (!ofnode_read_u32(node, "aic,timing", &timing)) {
		writel(timing, plat->base + SID_REG_TIMING);
	}

	if (ofnode_read_u32(node, "aic,max-words", &(plat->max_words))) {
		dev_info(dev, "Can't parse max-words value\n");
		plat->max_words = SID_MAX_WORDS;
	}

	return ret;
}

static const struct misc_ops aic_sid_ops = {
	.read = aic_sid_read,
	.write = aic_sid_write,
};

static const struct udevice_id aic_sid_ids[] = {
	{ .compatible = "artinchip,aic-sid-v1.0" },
	{ }
};

U_BOOT_DRIVER(artinchip_sid) = {
	.name      = "artinchip_aic_sid",
	.id        = UCLASS_MISC,
	.ops       = &aic_sid_ops,
	.of_match  = aic_sid_ids,
	.probe     = aic_sid_probe,
	.plat_auto = sizeof(struct aic_sid_platdata),
};
#endif
