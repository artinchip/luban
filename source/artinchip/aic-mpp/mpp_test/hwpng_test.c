/*
* Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
*
*  author: <qi.xu@artinchip.com>
*  Desc: png hardware decode demo (only support png width one IDAT chunk)
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <errno.h>
#include <linux/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include "mpp_dec_type.h"
#include "ve_buffer.h"
#include "ve.h"
#include "ve_top_register.h"
#include "mpp_log.h"

#define SHOW_TAG(a,b,c,d) ((a) | ((b) << 8) | ((c) << 16) | ((unsigned)(d) << 24))
#define ALIGN_1024B(x) (((x) + (1023)) & ~(1023))

// color type
#define PNG_COLOR_MASK_PALETTE	1
#define PNG_COLOR_MASK_COLOR	2
#define PNG_COLOR_MASK_ALPHA	4
#define PNG_COLOR_TYPE_GRAY	0
#define PNG_COLOR_TYPE_PALETTE    (PNG_COLOR_MASK_COLOR | PNG_COLOR_MASK_PALETTE)
#define PNG_COLOR_TYPE_RGB        (PNG_COLOR_MASK_COLOR)
#define PNG_COLOR_TYPE_RGB_ALPHA  (PNG_COLOR_MASK_COLOR | PNG_COLOR_MASK_ALPHA)
#define PNG_COLOR_TYPE_GRAY_ALPHA (PNG_COLOR_MASK_ALPHA)

#define ARGB8888   0
#define ABGR8888   1
#define RGBA8888   2
#define BGRA8888   3
#define RGB888     4
#define BGR888     5

// register
#define INFLATE_INTERRUPT_REG	(0xC00)
#define INFLATE_STATUS_REG	(0xC04)
#define INFLATE_START_REG	(0xC08)
#define PNG_CTRL_REG		(0xC10)
#define PNG_SIZE_REG		(0xC14)
#define PNG_STRIDE_REG		(0xC18)
#define PNG_FORMAT_REG		(0xC1C)
#define INPUT_BS_START_ADDR_REG	(0xC20)
#define INPUT_BS_END_ADDR_REG	(0xC24)
#define INPUT_BS_OFFSET_REG	(0xC28)
#define INPUT_BS_LENGTH_REG	(0xC2C)
#define OUTPUT_BUFFER_REG	(0xC30)
#define OUTPUT_LENGTH_REG	(0xC34)
#define OUTPUT_COUNT_REG	(0xC38)
#define LZ77_REG		(0xC40)
#define FILTER_REG		(0xC44)
#define INPUT_BS_DATA_VALID_REG	(0xC48)
#define PALETTE_REG		(0xC4C)

struct png_ctx {
	int width;
	int height;
	int color_type;
	unsigned int palette[256];
	unsigned char* buf_start; // buffer start addr
	unsigned char* ptr;	// read ptr
	int length;
};

static unsigned int get_byte(struct png_ctx *ctx)
{
	unsigned char* buf = ctx->ptr;
	ctx->ptr += 1;
	return (unsigned int)buf[0];
}

static unsigned int get_be32(struct png_ctx *ctx)
{
	unsigned char* buf = ctx->ptr;
	ctx->ptr += 4;
	return ((unsigned int)buf[0] << 24) | ((unsigned int)buf[1] << 16) |
		((unsigned int)buf[2] << 8) | (unsigned int)buf[3];
}

static unsigned int get_le32(struct png_ctx *ctx)
{
	unsigned char* buf = ctx->ptr;
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

static void print_help(char* prog)
{
	printf("%s: hardware png decoder, \n", prog);
	printf("    only support PNG with one IDAT chunk\n");
	printf("Compile time: %s\n", __TIME__);
	printf("Usage:\n\n"
		"Options:\n"
		" -i                             input file name\n"
		" -h                             help\n\n"
		"End:\n");
}

static int get_file_size(FILE* fp)
{
	int len = 0;
	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	return len;
}

static int decode_ihdr(struct png_ctx *ctx, int length)
{
	int bit_depth;
	ctx->width = get_be32(ctx);
	ctx->height = get_be32(ctx);

	bit_depth = get_byte(ctx);
	if (bit_depth != 8) {
		loge("not support bit depth %d", bit_depth);
	}

	ctx->color_type = get_byte(ctx);
	logi("width: %d, height: %d, color: %d", ctx->width, ctx->height, ctx->color_type);

	get_byte(ctx); // compreesion_type
	get_byte(ctx); // filter_type
	if (get_byte(ctx)) {
		loge("interleace type not support");
		return -1;
	}
	skip_bytes(ctx, 4); // crc
	return 0;
}

static int decode_plte(struct png_ctx *ctx, int length)
{
	int i, r, g, b;
	int n = length / 3;
	for (i=0; i<n; i++) {
		r = get_byte(ctx);
		g = get_byte(ctx);
		b = get_byte(ctx);
		ctx->palette[i] = (0xFFU << 24) | (r << 16) | (g << 8) | b;
	}

	for (; i < 256; i++)
		ctx->palette[i] = (0xFFU << 24);

	skip_bytes(ctx, 4); // crc
	return 0;
}

static void config_ve_top(unsigned long reg_base)
{
	write_reg_u32(reg_base + VE_CLK_REG, 1);
	write_reg_u32(reg_base + VE_RST_REG, 0);

	while ((read_reg_u32(reg_base + VE_RST_REG) >> 16)) {
	}

	write_reg_u32(reg_base + VE_INIT_REG, 1);
	write_reg_u32(reg_base + VE_IRQ_REG, 1);
	write_reg_u32(reg_base + VE_PNG_EN_REG, 1);
}

static int decode_idat(struct png_ctx *ctx, int length)
{
	int align_size = ALIGN_1024B(ctx->length);
	int offset = ctx->ptr - ctx->buf_start + 2;
	int left_len = ctx->length - offset;
	ve_open_device();

	struct ve_buffer_allocator* alloc = ve_buffer_allocator_create(VE_BUFFER_TYPE_DMA);

	// png input data
	struct ve_buffer* input_buf = ve_buffer_alloc(alloc, align_size, ALLOC_NEED_VIR_ADDR);
	// png output rgba data
	struct ve_buffer* output_buf = ve_buffer_alloc(alloc, ctx->width * 4 * ctx->height, ALLOC_NEED_VIR_ADDR);
	struct ve_buffer* lz77_buf = ve_buffer_alloc(alloc, 32*1024, 0);
	struct ve_buffer* filter_buf = ve_buffer_alloc(alloc, ctx->width * 4, 0);
	struct ve_buffer* pal_buff = NULL;
	if (ctx->color_type == PNG_COLOR_TYPE_PALETTE) {
		pal_buff = ve_buffer_alloc(alloc, 1024, 0);
	}

	memcpy(input_buf->vir_addr, ctx->buf_start, ctx->length);
	ve_buffer_sync(input_buf, CACHE_CLEAN);

	unsigned long reg_base = ve_get_reg_base();

	ve_get_client();
	config_ve_top(reg_base);

	write_reg_u32(reg_base+PNG_CTRL_REG, (ctx->color_type << 8) | 1);
	write_reg_u32(reg_base+PNG_SIZE_REG, (ctx->height << 16) | ctx->width);
	write_reg_u32(reg_base+PNG_STRIDE_REG, ctx->width*4);

	int format = RGBA8888; // output format
	write_reg_u32(reg_base+PNG_FORMAT_REG, format);
	write_reg_u32(reg_base + OUTPUT_BUFFER_REG, output_buf->phy_addr);
	write_reg_u32(reg_base + OUTPUT_LENGTH_REG, output_buf->size);

	write_reg_u32(reg_base+LZ77_REG, lz77_buf->phy_addr);
	write_reg_u32(reg_base+FILTER_REG, filter_buf->phy_addr);
	if (ctx->color_type == PNG_COLOR_TYPE_PALETTE) {
		memcpy(pal_buff->vir_addr, ctx->palette, 256*4);
		ve_buffer_sync(pal_buff, CACHE_CLEAN);
		write_reg_u32(reg_base + PALETTE_REG, pal_buff->phy_addr);
	}

	write_reg_u32(reg_base+INFLATE_INTERRUPT_REG, 15);
	write_reg_u32(reg_base+INFLATE_STATUS_REG, 15);
	write_reg_u32(reg_base+INFLATE_START_REG, 1);

	write_reg_u32(reg_base+INPUT_BS_START_ADDR_REG, input_buf->phy_addr);
	write_reg_u32(reg_base+INPUT_BS_END_ADDR_REG, input_buf->phy_addr+align_size-1);
	write_reg_u32(reg_base+INPUT_BS_OFFSET_REG, offset *8);
	write_reg_u32(reg_base+INPUT_BS_LENGTH_REG, left_len*8 );
	write_reg_u32(reg_base+INPUT_BS_DATA_VALID_REG, 0x80000003);

	unsigned int status;
	if (ve_wait(&status) < 0) {
		loge("png timeout");
		return -1;
	}
	logi("status: %08x", status);

	if ((status & 2) == 2) {
		loge("error");
	} else if((status & 1) == 1) {
		logi("finish");
	} else {
		loge("png bit request, not support now");
	}

	ve_put_client();

	FILE* fp_out = fopen("png_out.bin", "wb");
	fwrite(output_buf->vir_addr, 1, ctx->width * 4 * ctx->height, fp_out);
	fclose(fp_out);

	ve_buffer_free(alloc, input_buf);
	ve_buffer_free(alloc, output_buf);
	ve_buffer_free(alloc, lz77_buf);
	ve_buffer_free(alloc, filter_buf);
	if (pal_buff)
		ve_buffer_free(alloc, pal_buff);
	ve_buffer_allocator_destroy(alloc);

	ve_close_device();

	return 0;
}

static int decode_png(struct png_ctx *ctx, unsigned char* buf, int len)
{
	int chunk_len;
	int chunk_tag;
	ctx->ptr = buf;
	ctx->buf_start = buf;
	ctx->length = len;

	skip_bytes(ctx, 8); // signature

	while (get_left_byte(ctx)) {
		chunk_len = get_be32(ctx);
		chunk_tag = get_le32(ctx);
		logi("chunk tag: %x, len: %x", chunk_tag, chunk_len);

		switch (chunk_tag) {
		case SHOW_TAG('I', 'H', 'D', 'R'):
			logi("IHDR");
			decode_ihdr(ctx, chunk_len);
			break;
		case SHOW_TAG('I', 'D', 'A', 'T'):
			logi("IDAT");
			decode_idat(ctx, chunk_len);
			return 0;
		case SHOW_TAG('P', 'L', 'T', 'E'):
			decode_plte(ctx, chunk_len);
			break;
		case SHOW_TAG('I', 'E', 'N', 'D'):
			logi("IEND");
			return 0;
		default:
			skip_bytes(ctx, chunk_len + 4);
			break;
		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	int opt;
	int file_len;
	FILE* fp = NULL;
	struct png_ctx ctx;
	memset(&ctx, 0, sizeof(struct png_ctx));
	unsigned char* buf = NULL;

	while (1) {
		opt = getopt(argc, argv, "i:h");
		if (opt == -1) {
			break;
		}
		switch (opt) {
		case 'i':
			logd("file path: %s", optarg);
			fp = fopen(optarg, "rb");

			break;
		case 'h':
		default:
			print_help(argv[0]);
			return -1;
		}
	}

	if (fp == NULL) {
		print_help(argv[0]);
		return -1;
	}


	file_len = get_file_size(fp);
	logi("file_len: %d", file_len);
	buf = (unsigned char*)malloc(file_len);
	fread(buf, 1, file_len, fp);

	decode_png(&ctx, buf, file_len);

	free(buf);
	fclose(fp);

	return 0;
}
