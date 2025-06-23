/*
 * Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <qi.xu@artinchip.com>
 *  Desc: gzip demo
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

#define ALIGN_1024B(x) (((x) + (1023)) & ~(1023))

#define INFLATE_INTERRUPT_REG	(0xC00)
#define INFLATE_STATUS_REG	(0xC04)
#define INFLATE_START_REG	(0xC08)
#define PNG_CTRL_REG		(0xC10)
#define INPUT_BS_START_ADDR_REG	(0xC20)
#define INPUT_BS_END_ADDR_REG	(0xC24)
#define INPUT_BS_OFFSET_REG	(0xC28)
#define INPUT_BS_LENGTH_REG	(0xC2C)
#define OUTPUT_BUFFER_REG	(0xC30)
#define OUTPUT_LENGTH_REG	(0xC34)
#define OUTPUT_COUNT_REG	(0xC38)
#define LZ77_REG		(0xC40)
#define INPUT_BS_DATA_VALID_REG	(0xC48)

static void print_help(void)
{
	printf("Usage: gz_test [OPTIONS] [SLICES PATH]\n\n"
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

static unsigned int read_le16(const unsigned char *p)
{
	return ((unsigned int) p[0])
		| ((unsigned int) p[1] << 8);
}

static int get_gzip_header(unsigned char* src)
{
	unsigned char flg;
	unsigned char *start;
	flg = src[3];
	start = src + 10;

	if(flg & 4) { // FEXTRA
		unsigned int xlen = read_le16(start);
		start += (xlen + 2);
	}

	if(flg & 8) { // FNAME
		do {
		} while(*start++);
	}

	if(flg & 16) { // FCOMMENT
		do {
		} while(*start++);
	}

	if(flg & 2) { // FHCRC
		start += 2;
	}

	return start - src;
}

static void config_ve_top(unsigned long reg_base)
{
	write_reg_u32(reg_base + VE_CLK_REG, 1);
	write_reg_u32(reg_base + VE_RST_REG, 0);

	while( (read_reg_u32(reg_base + VE_RST_REG) >> 16)) {
	}

	write_reg_u32(reg_base + VE_INIT_REG, 1);
	write_reg_u32(reg_base + VE_IRQ_REG, 1);
	write_reg_u32(reg_base + VE_PNG_EN_REG, 1);
}

static int decode_gzip(FILE* fp)
{
	int len = get_file_size(fp);
	int align_len = ALIGN_1024B(len);
	int output_len = 1024 * 1024;
	int real_out_len = 0;

	ve_open_device();

	struct ve_buffer_allocator* ctx = ve_buffer_allocator_create(VE_BUFFER_TYPE_DMA);
	struct ve_buffer* input_buf = ve_buffer_alloc(ctx, align_len, ALLOC_NEED_VIR_ADDR);
	struct ve_buffer* output_buf = ve_buffer_alloc(ctx, output_len, ALLOC_NEED_VIR_ADDR);
	struct ve_buffer* lz77_buf = ve_buffer_alloc(ctx, 32*1024, 0);

	fread(input_buf->vir_addr, 1, len, fp);
	ve_buffer_sync(input_buf, CACHE_CLEAN);

	int offset = get_gzip_header(input_buf->vir_addr);

	unsigned long reg_base = ve_get_reg_base();

	ve_get_client();

	config_ve_top(reg_base);
	write_reg_u32(reg_base+PNG_CTRL_REG, 0);
	write_reg_u32(reg_base+OUTPUT_BUFFER_REG, output_buf->phy_addr);
	write_reg_u32(reg_base+OUTPUT_LENGTH_REG, output_buf->size);
	write_reg_u32(reg_base+LZ77_REG, lz77_buf->phy_addr);
	write_reg_u32(reg_base+INFLATE_INTERRUPT_REG, 15);
	write_reg_u32(reg_base+INFLATE_STATUS_REG, 15);
	write_reg_u32(reg_base+INFLATE_START_REG, 1);

	write_reg_u32(reg_base+INPUT_BS_START_ADDR_REG, input_buf->phy_addr);
	write_reg_u32(reg_base+INPUT_BS_END_ADDR_REG, input_buf->phy_addr+align_len-1);
	write_reg_u32(reg_base+INPUT_BS_OFFSET_REG, offset*8);
	write_reg_u32(reg_base+INPUT_BS_LENGTH_REG, (len-offset)*8 );
	write_reg_u32(reg_base+INPUT_BS_DATA_VALID_REG, 0x80000003);

	unsigned int status;
	if(ve_wait(&status) < 0) {
		loge("png timeout");
		return -1;
	}
	logi("status: %08x", status);

	if((status & 1) == 1) {
		logi("finish");
	} else {
		loge("png bit request, not support now");
	}


	real_out_len = read_reg_u32(reg_base+OUTPUT_COUNT_REG);
	logi("out data len: %d", real_out_len);
	ve_put_client();

	FILE* fp_out = fopen("out.bin", "wb");
	fwrite(output_buf->vir_addr, 1, real_out_len, fp_out);
	fclose(fp_out);

	ve_buffer_free(ctx, input_buf);
	ve_buffer_free(ctx, output_buf);
	ve_buffer_free(ctx, lz77_buf);
	ve_buffer_allocator_destroy(ctx);

	ve_close_device();
	return 0;
}

int main(int argc, char **argv)
{
	int opt;
	FILE* fp = NULL;

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
			print_help();
			return -1;
		}
	}

	if(fp == NULL) {
		loge("please input the right file path");
		return -1;
	}

	decode_gzip(fp);

	return 0;
}
