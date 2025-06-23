//SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 Artinchip Technology Co.,Ltd
 */

#include <common.h>
#include <asm/io.h>
#include <dm.h>
#include <dm/device.h>
#include <dm/uclass-internal.h>
#include <dm/device_compat.h>
#include <cpu_func.h>
#include <asm/cache.h>
#include <cache.h>
#include <mtd.h>
#include <video.h>
#include <linux/iopoll.h>
#include <artinchip_ve.h>

#define SHOW_TAG(a, b, c, d)	((a) | ((b) << 8) | ((c) << 16) | ((unsigned)(d) << 24))
#define ALIGN_1024B(x)		(((x) + (1023)) & ~(1023))
#define ALIGN_64B(x)		(((x) + (63)) & ~(63))
#define PAL_BUF_SIZE		(1024)
#define LZ77_BUF_SIZE		(32 * 1024)

/* color type */
#define PNG_COLOR_MASK_PALETTE	1
#define PNG_COLOR_MASK_COLOR	2
#define PNG_COLOR_MASK_ALPHA	4
#define PNG_COLOR_TYPE_GRAY	0
#define PNG_COLOR_TYPE_PALETTE    (PNG_COLOR_MASK_COLOR | PNG_COLOR_MASK_PALETTE)
#define PNG_COLOR_TYPE_RGB        (PNG_COLOR_MASK_COLOR)
#define PNG_COLOR_TYPE_RGB_ALPHA  (PNG_COLOR_MASK_COLOR | PNG_COLOR_MASK_ALPHA)
#define PNG_COLOR_TYPE_GRAY_ALPHA (PNG_COLOR_MASK_ALPHA)

/* output type */
#define ARGB8888   0
#define ABGR8888   1
#define RGBA8888   2
#define BGRA8888   3
#define RGB888     4
#define BGR888     5

#define OUTPUT_FORMAT ARGB8888

/* register */
#define INFLATE_INTERRUPT_REG	0xC00
#define PNG_CTRL_REG		0xC10
#define PNG_SIZE_REG		0xC14
#define PNG_STRIDE_REG		0xC18
#define PNG_FORMAT_REG		0xC1C
#define OUTPUT_BUFFER_REG	0xC30
#define OUTPUT_LENGTH_REG	0xC34
#define LZ77_REG		0xC40
#define FILTER_REG		0xC44
#define PALETTE_REG		0xC4C

#define write_reg_u32(a, v)	writel((v), (void __iomem *)(a))
#define read_reg_u32(a)		readl((void __iomem *)(a))

struct png_ctx {
	int width;
	int height;
	int color_type;
	unsigned int palette[256];
	unsigned char *buf_start;
	unsigned char *ptr;
	int length;
};

static inline int format_pixel_byte(int format)
{
	switch (format) {
	case ARGB8888:
	case ABGR8888:
	case RGBA8888:
	case BGRA8888:
		return 4;
	default:
		break;
	};

	return 3;
}

static unsigned int get_byte(struct png_ctx *ctx)
{
	unsigned char *buf = ctx->ptr;

	ctx->ptr += 1;
	return (unsigned int)buf[0];
}

static unsigned int get_be32(struct png_ctx *ctx)
{
	unsigned char *buf = ctx->ptr;

	ctx->ptr += 4;
	return ((unsigned int)buf[0] << 24) | ((unsigned int)buf[1] << 16) |
		((unsigned int)buf[2] << 8) | (unsigned int)buf[3];
}

static unsigned int get_le32(struct png_ctx *ctx)
{
	unsigned char *buf = ctx->ptr;

	ctx->ptr += 4;
	return ((unsigned int)buf[3] << 24) | ((unsigned int)buf[2] << 16) |
		((unsigned int)buf[1] << 8) | (unsigned int)buf[0];
}

static void skip_bytes(struct png_ctx *ctx, int n)
{
	ctx->ptr += n;
}

static int get_left_byte(struct png_ctx *ctx)
{
	return ctx->length - (ctx->ptr - ctx->buf_start);
}

static int decode_ihdr(struct png_ctx *ctx, int length)
{
	pr_debug("%x %x %x %x", ctx->ptr[0], ctx->ptr[1], ctx->ptr[2], ctx->ptr[3]);
	ctx->width = get_be32(ctx);
	ctx->height = get_be32(ctx);

	int bit_depth = get_byte(ctx);

	ctx->color_type = get_byte(ctx);
	pr_debug("width: %d, height: %d, color: %d, bit_depth: %d\n",
		ctx->width, ctx->height, ctx->color_type, bit_depth);

	/* compreesion type */
	get_byte(ctx);

	/* filter type */
	get_byte(ctx);

	/* interleace type */
	if (get_byte(ctx)) {
		pr_err("Don't support interleace type\n");
		return -1;
	}

	/* crc */
	skip_bytes(ctx, 4);
	return 0;
}

static int decode_plte(struct png_ctx *ctx, int length)
{
	int i, r, g, b;
	int n = length / 3;

	for (i = 0; i < n; i++) {
		r = get_byte(ctx);
		g = get_byte(ctx);
		b = get_byte(ctx);
		ctx->palette[i] = (0xFFU << 24) | (r << 16) | (g << 8) | b;
	}

	for (; i < 256; i++)
		ctx->palette[i] = (0xFFU << 24);

	/* crc */
	skip_bytes(ctx, 4);
	return 0;
}

static void config_ve_top(unsigned long reg_base)
{
	write_reg_u32(reg_base + VE_CLK_REG, 1);
	write_reg_u32(reg_base + VE_RST_REG, 0);

	while (read_reg_u32(reg_base + VE_RST_REG) >> 16) {
	}

	write_reg_u32(reg_base + VE_INIT_REG, 1);
	write_reg_u32(reg_base + VE_PNG_EN_REG, 1);
}

static int config_output_info(struct udevice *dev, struct png_ctx *ctx)
{
	unsigned long reg_base = dev_read_addr(dev);
	struct video_priv *uc_priv;
	struct udevice *vdev;
	uchar *dst_buf;
	int ret, x, y;

	ret = uclass_find_first_device(UCLASS_VIDEO, &vdev);
	if (ret) {
		pr_err("Failed to find aicfb udevice\n");
		return ret;
	}
	uc_priv = dev_get_uclass_priv(vdev);

	if (ctx->width > uc_priv->xsize || ctx->height > uc_priv->ysize) {
		pr_err("Video buffer %d x %d y"
			" but PNG image has %d x %d y\n",
			uc_priv->xsize, uc_priv->ysize,
			ctx->width, ctx->height);
		return -EINVAL;
	}

	x = (uc_priv->xsize - ctx->width) / 2;
	y = (uc_priv->ysize - ctx->height) / 2;

	dst_buf = (uchar *)(uc_priv->fb +
			y * uc_priv->line_length + x * VNBYTES(uc_priv->bpix));

	write_reg_u32(reg_base + PNG_FORMAT_REG, OUTPUT_FORMAT);
	write_reg_u32(reg_base + PNG_STRIDE_REG, uc_priv->line_length);

	write_reg_u32(reg_base + OUTPUT_BUFFER_REG, (uintptr_t)dst_buf);
	write_reg_u32(reg_base + OUTPUT_LENGTH_REG, uc_priv->fb_size);

	return 0;
}

static int decode_idat(struct udevice *dev, struct png_ctx *ctx, int length)
{
	unsigned long reg_base = dev_read_addr(dev);
	int align_size = ALIGN_1024B(ctx->length);
	int offset = ctx->ptr - ctx->buf_start + 2;
	int left_len = ctx->length - offset;
	void *lz77_buf = NULL, *filter_buf = NULL, *pal_buf = NULL;
	uintptr_t input_start, input_end, filter_buf_len;
	int ret = -1;

	lz77_buf = memalign(DECODE_ALIGN, LZ77_BUF_SIZE);
	if (!lz77_buf) {
		pr_err("failed to alloc lz77_buf\n");
		goto out;
	}
	flush_dcache_range((uintptr_t)lz77_buf, (uintptr_t)LZ77_BUF_SIZE);

	filter_buf_len = ALIGN_64B(ctx->width * format_pixel_byte(OUTPUT_FORMAT));
	filter_buf = memalign(DECODE_ALIGN, filter_buf_len);
	if (!filter_buf) {
		pr_err("failed to alloc filter_buf\n");
		goto out;
	}
	flush_dcache_range((uintptr_t)filter_buf, filter_buf_len);

	if (ctx->color_type == PNG_COLOR_TYPE_PALETTE) {
		pal_buf = memalign(DECODE_ALIGN, PAL_BUF_SIZE);
		if (!pal_buf) {
			pr_err("failed to alloc pal_buf\n");
			goto out;
		}
	}

	config_ve_top(reg_base);

	write_reg_u32(reg_base + PNG_CTRL_REG, (ctx->color_type << 8) | 1);
	write_reg_u32(reg_base + PNG_SIZE_REG, (ctx->height << 16) | ctx->width);

	if (config_output_info(dev, ctx))
		goto out;

	write_reg_u32(reg_base + LZ77_REG, (uintptr_t)lz77_buf);
	write_reg_u32(reg_base + FILTER_REG, (uintptr_t)filter_buf);
	if (ctx->color_type == PNG_COLOR_TYPE_PALETTE) {
		memcpy(pal_buf, ctx->palette, PAL_BUF_SIZE);
		flush_dcache_range((uintptr_t)pal_buf, (uintptr_t)pal_buf + PAL_BUF_SIZE);
		write_reg_u32(reg_base + PALETTE_REG, (uintptr_t)pal_buf);
	}

	write_reg_u32(reg_base + INFLATE_INTERRUPT_REG, 15);
	write_reg_u32(reg_base + INFLATE_STATUS_REG, 15);
	write_reg_u32(reg_base + INFLATE_START_REG, 1);

	input_start = (uintptr_t)ctx->buf_start;
	input_end = input_start + align_size - 1;

	write_reg_u32(reg_base + INPUT_BS_START_ADDR_REG, input_start);
	write_reg_u32(reg_base + INPUT_BS_END_ADDR_REG, input_end);
	write_reg_u32(reg_base + INPUT_BS_OFFSET_REG, offset * 8);
	write_reg_u32(reg_base + INPUT_BS_LENGTH_REG, left_len * 8);
	write_reg_u32(reg_base + INPUT_BS_DATA_VALID_REG, 0x80000003);

	unsigned int status = 0;
	/* Wait VE PNG decode done */
	ret = readl_poll_timeout((void *)(reg_base + INFLATE_STATUS_REG),
			status, status & 0x7, 1000 * 100);
	if (ret) {
		pr_err("VE PNG decode timeout, status: %08x\n", status);
		goto out;
	}

	write_reg_u32(reg_base + INFLATE_START_REG, status);

	if ((status & 0x2) == 2) {
		ret = -1;
		pr_err("PNG decode error, status: %d\n", status);
		goto out;
	} else if ((status & 0x1) == 1) {
		pr_info("PNG decode finish\n");
	} else {
		ret = -1;
		pr_err("png bit request, not support now \n");
		goto out;
	}

out:
	if (lz77_buf)
		free(lz77_buf);
	if (filter_buf)
		free(filter_buf);
	if (pal_buf)
		free(pal_buf);

	return ret;
}

static int decode_png(struct udevice *dev, struct png_ctx *ctx,
			unsigned char *buf, int len)
{
	int chunk_len;
	int chunk_tag;
	int ret;

	ctx->ptr = buf;
	ctx->buf_start = buf;
	ctx->length = len;

	/* signature */
	skip_bytes(ctx, 8);

	while (get_left_byte(ctx)) {
		chunk_len = get_be32(ctx);
		chunk_tag = get_le32(ctx);

		pr_debug("chunk_tag: %08x, offset: %ld \n",
				chunk_tag, ctx->ptr - ctx->buf_start);
		switch (chunk_tag) {
		case SHOW_TAG('I', 'H', 'D', 'R'):
			ret = decode_ihdr(ctx, chunk_len);
			if (ret) {
				pr_err("Don't support PNG type\n");
				return ret;
			}
			break;
		case SHOW_TAG('I', 'D', 'A', 'T'):
			ret = decode_idat(dev, ctx, chunk_len);
			return ret;
		case SHOW_TAG('P', 'L', 'T', 'E'):
			decode_plte(ctx, chunk_len);
			break;
		default:
			skip_bytes(ctx, chunk_len + 4);
			break;
		}
	}

	return 0;
}

int __png_decode(struct udevice *dev, void *src, unsigned int size)
{
	struct png_ctx ctx;

	memset(&ctx, 0, sizeof(struct png_ctx));

	return decode_png(dev, &ctx, src, size);
}
