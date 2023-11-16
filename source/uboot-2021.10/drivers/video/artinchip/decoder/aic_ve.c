//SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 ArtInChip Technology Co.,Ltd
 * Author: Hao Xiong <hao.xiong@artinchip.com>
 */

#include <common.h>
#include <asm/io.h>
#include <dm.h>
#include <dm/device.h>
#include <dm/device_compat.h>
#include <clk.h>
#include <reset.h>
#include <linux/iopoll.h>
#include <artinchip_ve.h>

#define INFLATE_STATUS_FINISH	BIT(0)
#define INFLATE_STATUS_ERR	BIT(1)
#define INFLATE_STATUS_BITREQ	BIT(2)
#define INFLATE_STATUS_OVERTIME	BIT(3)

#define LZ77_WINDOW_SIZE	(32*1024)
#define LZ77_MINALIGN	1024
#define INFLATE_MAX_OUTPUT	(1024*1024*16)
#define VE_TIMEOUT		(2000000)

#define HEAD_CRC        2
#define EXTRA_FIELD     4
#define ORIG_NAME       8
#define COMMENT         0x10
#define RESERVED        0xe0

#define DEFLATED        8

struct aic_ve_data {
	ulong *lenp;
	u32 dstlen;
	uintptr_t dst_addr;
	uintptr_t src_addr;
	uintptr_t lz77_addr;
	u32 offset;
	u8 *lz77_buf;
};

struct aic_ve_priv {
	struct aic_ve_data *ve_data;
	struct clk clk;
	struct reset_ctl reset;
	void __iomem *base;
	ulong mclk_rate;
};

static inline u32 read_le16(const u8 *src)
{
	return (((u32)src[1]) << 8) | (((u32)src[0]) << 0);
}

static int aic_ve_gzip_parse_header(unsigned char *src, unsigned long len)
{
	int i, flags;

	/* skip header */
	i = 10;
	flags = src[3];
	if (src[2] != DEFLATED || (flags & RESERVED) != 0) {
		puts("Error: Bad gzipped data\n");
		return (-1);
	}
	if ((flags & EXTRA_FIELD) != 0)
		i = 12 + src[10] + (src[11] << 8);
	if ((flags & ORIG_NAME) != 0)
		while (src[i++] != 0)
			;
	if ((flags & COMMENT) != 0)
		while (src[i++] != 0)
			;
	if ((flags & HEAD_CRC) != 0)
		i += 2;
	if (i >= len) {
		puts("Error: gunzip out of data in header\n");
		return (-1);
	}
	return i;
}

static int aic_ve_gunzip_init(struct udevice *dev, void *dst, int dstlen,
				unsigned char *src, unsigned long *lenp)
{
	struct aic_ve_priv *priv;
	struct aic_ve_data *ve_data;
	u32 offset;
	u8 *lz77_buf;

	priv = dev_get_priv(dev);

	ve_data = (struct aic_ve_data *)malloc(sizeof(struct aic_ve_data));
	if (!ve_data) {
		pr_err("alloc ve_data buffer failed!\n");
		return -1;
	}

	lz77_buf = (u8 *)memalign(LZ77_MINALIGN, LZ77_WINDOW_SIZE);
	if (!lz77_buf) {
		pr_err("alloc lz77 buffer failed!\n");
		goto out;
	}

	offset = aic_ve_gzip_parse_header(src, *lenp);
	if (offset < 0) {
		pr_err("parse zip header failed\n");
		goto out;
	}

	/* src addr 16 byte align */
	offset += (uintptr_t)src % 16;

	priv->ve_data = ve_data;
	ve_data->offset = offset;
	ve_data->lz77_buf = lz77_buf;
	ve_data->lz77_addr = (uintptr_t)lz77_buf;
	ve_data->dst_addr = (uintptr_t)dst;
	/* address 16 bytes are aligned down */
	ve_data->src_addr = (uintptr_t)src;
	ve_data->dstlen = dstlen;
	ve_data->lenp = lenp;

	printf("dst:0x%p,src:0x%p,lz77_buf:0x%p\n", dst, src, lz77_buf);
	return 0;
out:
	if (ve_data)
		free(ve_data);

	return -1;
}

static void aic_ve_config_top_reg(struct aic_ve_priv *priv)
{
	u32 val;

	writel(VE_CLK_BIT_EN, priv->base + VE_CLK_REG);

	val = readl(priv->base + VE_RST_REG);
	val &= ~VE_RST_MSK;
	writel(val, priv->base + VE_RST_REG);

	while ((readl(priv->base + VE_RST_REG) >> 16)) {
	}

	writel(VE_INIT_BIT_EN, priv->base + VE_INIT_REG);

	val = readl(priv->base + VE_IRQ_REG);
	val &= ~VE_IRQ_BIT_EN;
	writel(val, priv->base + VE_IRQ_REG);

	writel(VE_PNG_BIT_EN, priv->base + VE_PNG_EN_REG);
}

static void aic_ve_config_inflate_reg(struct aic_ve_priv *priv)
{
	u32 val;
	struct aic_ve_data *ve_data = priv->ve_data;

	/* set decode type to inflate */
	writel(INFLATE_CTRL_GZIP_DECODE_TYPE, priv->base + INFLATE_CTRL_REG);

	/* set inflate output reg */
	writel(ve_data->dst_addr, priv->base + OUTPUT_BUF_ADDR_REG);
	writel(ve_data->dstlen, priv->base + OUTPUT_MAX_LENGTH_REG);

	/* set LZ77 buffer 32K */
	writel(ve_data->lz77_addr, priv->base + INFLATE_WINDOW_BUF_REG);

	/* enable interrupt and clear status */
	writel(0xF, priv->base + INFLATE_INT_REG);
	writel(0xF, priv->base + INFLATE_STATUS_REG);

	/* decode start */
	writel(INFLATE_START_BIT_EN, priv->base + INFLATE_START_REG);

	/* set intput bitstream reg */
	writel(ve_data->src_addr, priv->base + INPUT_BS_START_ADDR_REG);

	val = ve_data->src_addr + *(ve_data->lenp) - 1;
	writel(val, priv->base + INPUT_BS_END_ADDR_REG);
	writel(ve_data->offset * 8, priv->base + INPUT_BS_OFFSET_REG);
	val = (*(ve_data->lenp) - ve_data->offset) * 8;
	writel(val, priv->base + INPUT_BS_LENGTH_REG);

	writel(0x80000003, priv->base + INPUT_BS_DATA_VALID_REG);

}

static int aic_ve_gunzip(struct udevice *dev)
{
	struct aic_ve_priv *priv;
	u32 status, val;
	int ret;

	priv = dev_get_priv(dev);

	aic_ve_config_top_reg(priv);
	aic_ve_config_inflate_reg(priv);
	/* wait IRQ */
	ret = readl_poll_timeout(priv->base + INFLATE_STATUS_REG, status,
							status, VE_TIMEOUT);
	if (status < 0) {
		pr_err("%s: Timeout to decode data\n", __func__);
		return ret;
	}

	if (status & INFLATE_STATUS_OVERTIME) {
		pr_err("gzip decode timeout\n");
		ret = -1;
	} else if (status & INFLATE_STATUS_ERR) {
		pr_err("gzip decode error\n");
		ret = -1;
	} else if (status  & INFLATE_STATUS_BITREQ) {
		pr_err("gzip bit request, not support now\n");
		ret = -1;
	} else if (status & INFLATE_STATUS_FINISH) {
		pr_info("gzip decode finish.\n");
	} else if (status) {
		pr_err("inflate status error:0x%x\n", val);
		ret = -1;
	}

	val = readl(priv->base + OUTPUT_COUNT_REG);
	if (val <= INFLATE_MAX_OUTPUT) {
		pr_info("gzip decode size:%u\n", val);
	} else {
		pr_err("decode size:%u is too big\n", val);
		return -1;
	}

	*(priv->ve_data->lenp) = val;
	if (priv->ve_data->lz77_buf)
		free(priv->ve_data->lz77_buf);
	if (priv->ve_data)
		free(priv->ve_data);

	return ret;
}

static int aic_ve_png(struct udevice *dev, void *src, unsigned int size)
{
	return __png_decode(dev, src, size);
}

#ifdef CONFIG_JPEG_LOGO_IMAGE
static int aic_ve_jpeg(struct udevice *dev, void *src, unsigned int size)
{
	return __jpeg_decode(dev, src, size);
}
#endif

static inline int aic_ve_set_clock(struct udevice *dev, bool enable)
{
	int ret = 0;
	struct aic_ve_priv *priv = dev_get_priv(dev);

	dev_dbg(dev, "enable %d\n", enable);
	if (!enable) {
		clk_disable(&priv->clk);
		if (reset_valid(&priv->reset))
			reset_assert(&priv->reset);
		return 0;
	}

	ret = clk_enable(&priv->clk);
	if (ret) {
		dev_err(dev, "failed to enable clock (ret=%d)\n", ret);
		return ret;
	}

	ret = clk_set_rate(&priv->clk, priv->mclk_rate);
	if (ret) {
		dev_err(dev, "Failed to set CLK_VE %ld\n", priv->mclk_rate);
		return ret;
	}

	if (reset_valid(&priv->reset)) {
		ret = reset_deassert(&priv->reset);
		if (ret) {
			dev_err(dev, "failed to deassert reset\n");
			goto err;
		}
	}

	return 0;

err:
	clk_disable(&priv->clk);
	return ret;
}

static int aic_ve_probe(struct udevice *dev)
{
	int ret = 0;
	struct aic_ve_priv *priv = dev_get_priv(dev);
	ofnode node = dev_ofnode(dev);

	priv->base = (void *)devfdt_get_addr(dev);

	ret = clk_get_by_index(dev, 0, &priv->clk);
	if (ret < 0) {
		dev_err(dev, "failed to get clock\n");
		return ret;
	}

	ret = ofnode_read_u32(node, "mclk", (u32 *)&priv->mclk_rate);
	if (ret) {
		dev_err(dev, "Can't parse CLK_VE rate\n");
		return -EINVAL;
	}

	ret = reset_get_by_index(dev, 0, &priv->reset);
	if (ret && ret != -ENOENT) {
		dev_err(dev, "failed to get reset\n");
		return ret;
	}

	return ret;
}

static int aic_ve_init(struct udevice *dev)
{
	int ret;

	ret = aic_ve_set_clock(dev, true);
	if (ret) {
		dev_err(dev, "aic ve set clk failed!\n");
		return ret;
	}

	return 0;
}

static void aic_ve_release(struct udevice *dev)
{
	struct aic_ve_priv *priv = dev_get_priv(dev);

	if (clk_valid(&priv->clk))
		clk_disable(&priv->clk);

	if (reset_valid(&priv->reset))
		reset_assert(&priv->reset);
}

static const struct decoder_ops aic_ve_ops = {
	.init = aic_ve_init,
	.decompress_init = aic_ve_gunzip_init,
	.decompress = aic_ve_gunzip,
	.png_decode = aic_ve_png,
#ifdef CONFIG_JPEG_LOGO_IMAGE
	.jpeg_decode = aic_ve_jpeg,
#endif
	.release = aic_ve_release,
};

static const struct udevice_id aic_ve_match_ids[] = {
	{.compatible = "artinchip,aic-ve-v1.0"},
	{ /* sentinel*/ },
};

U_BOOT_DRIVER(aic_ve) = {
	.name                     = "artinchip_aic_ve",
	.id                       = UCLASS_DECODER,
	.of_match                 = aic_ve_match_ids,
	.ops                      = &aic_ve_ops,
	.priv_auto                = sizeof(struct aic_ve_priv),
	.probe                    = aic_ve_probe,
};
