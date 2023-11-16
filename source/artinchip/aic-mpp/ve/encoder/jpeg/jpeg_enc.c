/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <qi.xu@artinchip.com>
*  Desc: jpeg encode
*/

#include <stdio.h>
#include <string.h>
#include "ve.h"
#include "ve_buffer.h"
#include "put_bits.h"
#include "jpeg_tables.h"
#include "jpeg_enc_ctx.h"
#include "mpp_mem.h"
#include "mpp_log.h"

static struct jpeg_ctx* jpeg_encoder_create()
{
	struct jpeg_ctx *impl = (struct jpeg_ctx*)mpp_alloc(sizeof(struct jpeg_ctx));
	if(impl == NULL) {
		return NULL;
	}
	memset(impl, 0, sizeof(struct jpeg_ctx));

	impl->ve_fd = ve_open_device();
	if (impl->ve_fd < 0) {
		loge("ve open failed");
		return NULL;
	}

	impl->regs_base = ve_get_reg_base();
	impl->alloc = ve_buffer_allocator_create(VE_BUFFER_TYPE_DMA);

	mjpeg_build_huffman_codes(impl->huff_size_dc_luminance,
		impl->huff_code_dc_luminance,
		avpriv_mjpeg_bits_dc_luminance,
		avpriv_mjpeg_val_dc);
	mjpeg_build_huffman_codes(impl->huff_size_dc_chrominance,
		impl->huff_code_dc_chrominance,
		avpriv_mjpeg_bits_dc_chrominance,
		avpriv_mjpeg_val_dc);
	mjpeg_build_huffman_codes(impl->huff_size_ac_luminance,
		impl->huff_code_ac_luminance,
		avpriv_mjpeg_bits_ac_luminance,
		avpriv_mjpeg_val_ac_luminance);
	mjpeg_build_huffman_codes(impl->huff_size_ac_chrominance,
		impl->huff_code_ac_chrominance,
		avpriv_mjpeg_bits_ac_chrominance,
		avpriv_mjpeg_val_ac_chrominance);

	return impl;
}

void jpeg_encoder_destory(struct jpeg_ctx* impl)
{
	ve_close_device();
	ve_buffer_allocator_destroy(impl->alloc);
	mpp_free(impl);
}

/* table_class: 0 = DC coef, 1 = AC coefs */
static int put_huffman_table(struct put_bit_ctx *p, int table_class, int table_id,
	const uint8_t *bits_table, const uint8_t *value_table)
{
	int n, i;

	put_bits(p, 4, table_class);
	put_bits(p, 4, table_id);

	n = 0;
	for (i = 1; i <= 16; i++) {
		n += bits_table[i];
		put_bits(p, 8, bits_table[i]);
	}

	for (i = 0; i < n; i++)
		put_bits(p, 8, value_table[i]);

	return n + 17;
}

static inline void put_marker(struct put_bit_ctx *p, enum JpegMarker code)
{
	put_bits(p, 8, 0xff);
	put_bits(p, 8, code);
}

static void jpeg_table_header(struct jpeg_ctx* s, struct put_bit_ctx *p)
{
	int i, j, size;
	uint8_t *ptr;

	/* quant matrixes */
	put_marker(p, DQT);
	put_bits(p, 16, 2 + 2 * (1 + 64));
	put_bits(p, 4, 0); /* 8 bit precision */
	put_bits(p, 4, 0); /* table 0 */
	for (i = 0; i < 64; i++) {
		j = zigzag_direct[i];
		put_bits(p, 8, s->luma_quant_table[j]);
	}

	put_bits(p, 4, 0); /* 8 bit precision */
	put_bits(p, 4, 1); /* table 1 */
	for (i = 0; i < 64; i++) {
		j = zigzag_direct[i];
		put_bits(p, 8, s->chroma_quant_table[j]);
	}

	/* huffman table */
	put_marker(p, DHT);
	flush_put_bits(p);
	ptr = put_bits_ptr(p);
	put_bits(p, 16, 0); /* patched later */
	size = 2;

	size += put_huffman_table(p, 0, 0, avpriv_mjpeg_bits_dc_luminance,
		avpriv_mjpeg_val_dc);
	size += put_huffman_table(p, 0, 1, avpriv_mjpeg_bits_dc_chrominance,
		avpriv_mjpeg_val_dc);

	size += put_huffman_table(p, 1, 0, avpriv_mjpeg_bits_ac_luminance,
		avpriv_mjpeg_val_ac_luminance);
	size += put_huffman_table(p, 1, 1, avpriv_mjpeg_bits_ac_chrominance,
		avpriv_mjpeg_val_ac_chrominance);

	ptr[0] = (size >> 8) & 0xff;
	ptr[1] = (size) & 0xff;
}

static void jpeg_encode_pic_header(struct jpeg_ctx* s)
{
	put_marker(&s->pb, SOI);
	jpeg_table_header(s, &s->pb);

	put_marker(&s->pb, SOF0);
	int len = 2 + 1 + 2 + 2 + 1 + 3 * s->comp_num;
	put_bits(&s->pb, 16, len);
	put_bits(&s->pb, 8, 8); /* precision 8 bits/component */

	if (s->cur_frame->buf.crop_en) {
		put_bits(&s->pb, 16, s->cur_frame->buf.crop.height);
		put_bits(&s->pb, 16, s->cur_frame->buf.crop.width);
	} else {
		put_bits(&s->pb, 16, s->height);
		put_bits(&s->pb, 16, s->width);
	}
	put_bits(&s->pb, 8, s->comp_num); /* 3 or 4 components */

	/* Y component */
	put_bits(&s->pb, 8, 1); /* component number */
	put_bits(&s->pb, 4, s->h_count[0]); /* H factor */
	put_bits(&s->pb, 4, s->v_count[0]); /* V factor */
	put_bits(&s->pb, 8, 0); /* select quant table */

	if (s->comp_num > 1) {
		/* Cb */
		put_bits(&s->pb, 8, 2); /* component number */
		put_bits(&s->pb, 4, s->h_count[1]); /* H factor */
		put_bits(&s->pb, 4, s->v_count[1]); /* V factor */
		put_bits(&s->pb, 8, 1); /* select quant table */

		/* Cr */
		put_bits(&s->pb, 8, 3); /* component number */
		put_bits(&s->pb, 4, s->h_count[2]); /* H factor */
		put_bits(&s->pb, 4, s->v_count[2]); /* V factor */
		put_bits(&s->pb, 8, 1); /* select quant table */
	}

	logi("offset: %d", put_bits_count(&s->pb) / 8);
	// the start address of VE must be 8 bytes aligned,
	// we cannot pad 0x00 here, because some customer app
	// cannot compatible this case.
	// So we use COM marker.
	int pad = (put_bits_count(&s->pb) / 8) % 8;
	if (pad) {
		pad = 8 - pad;
		if (pad < 4) {
			pad += 8;
		}

		put_marker(&s->pb, COM);
		put_bits(&s->pb, 16, pad-2); // length

		pad -= 4;
		while (pad--) {
			put_bits(&s->pb, 8, 0);
		}
	}
	flush_put_bits(&s->pb);

	s->header_offset = put_bits_count(&s->pb) / 8;
}

static void jpeg_init_hvcount(struct jpeg_ctx* s)
{
	s->h_count[1] = s->h_count[2] = 1;
	s->v_count[1] = s->v_count[2] = 1;
	s->comp_num = 3;
	s->uv_interleave = 0;
	if (s->cur_frame->buf.format == MPP_FMT_YUV400) {
		s->h_count[0] = s->v_count[0] = 1;
		s->comp_num = 1;
	} else if (s->cur_frame->buf.format == MPP_FMT_YUV444P) {
		s->h_count[0] = s->v_count[0] = 1;
	} else if (s->cur_frame->buf.format == MPP_FMT_YUV420P ||
		s->cur_frame->buf.format == MPP_FMT_NV12) {
		s->h_count[0] = s->v_count[0] = 2;
		s->uv_interleave = s->cur_frame->buf.format != MPP_FMT_YUV420P;
	} else if (s->cur_frame->buf.format == MPP_FMT_YUV422P) {
		s->h_count[0] = 2;
		s->v_count[0] = 1;
	} else {
		loge("not supprt this format: %d", s->cur_frame->buf.format);
	}
}

static void set_quality(struct jpeg_ctx* s)
{
	int i;
	if (s->quality <= 0)
		s->quality = 1;
	if (s->quality > 100)
		s->quality = 100;

	//* 1. quality = 1, produce "worst" quality, 5000*std_quant_table
	//* 2. quality = 50, produce "good" quality, std_quant_table
	//* 3. quality =100, produce "best" quality, the value of table are all 1
	if (s->quality < 50) {
		s->quality = 5000 / s->quality;
	} else {
		s->quality = 200 - s->quality * 2;
	}

	for (i=0; i<64; i++) {
		s->luma_quant_table[i] = (std_luminance_quant_tbl[i] * s->quality + 50) / 100;
		if (s->luma_quant_table[i] <= 0)
			s->luma_quant_table[i] = 1;
		if (s->luma_quant_table[i] > 255)
			s->luma_quant_table[i] = 255;

		s->chroma_quant_table[i] = (std_chrominance_quant_tbl[i] * s->quality + 50) / 100;
		if (s->chroma_quant_table[i] <= 0)
			s->chroma_quant_table[i] = 1;
		if (s->chroma_quant_table[i] > 255)
			s->chroma_quant_table[i] = 255;
	}
}

int mpp_encode_jpeg(struct mpp_frame* frame, int quality, int dma_buf_fd, int buf_len, int *len)
{
	int ret = 0;
	int i;
	int comp = 3;
	struct jpeg_ctx* s = jpeg_encoder_create();
	s->width = frame->buf.size.width;
	s->height = frame->buf.size.height;
	s->quality = quality;

	if (frame->buf.format == MPP_FMT_NV12) {
		comp = 2;
	} else if (frame->buf.format == MPP_FMT_YUV420P
		|| frame->buf.format == MPP_FMT_YUV422P) {
		comp = 3;
	} else if (frame->buf.format == MPP_FMT_YUV400) {
		comp = 1;
	} else {
		loge("unsupport format, %d", frame->buf.format);
		return -1;
	}

	set_quality(s);

	s->stream_num = buf_len / 256;
	s->bitstream_vir_addr = dmabuf_mmap(dma_buf_fd, buf_len);
	ve_add_dma_buf(dma_buf_fd, &s->bitstream_phy_addr);

	s->cur_frame = frame;

	init_put_bits(&s->pb, s->bitstream_vir_addr, buf_len);
	for (i=0; i<comp; i++) {
		ve_add_dma_buf(frame->buf.fd[i], &s->phy_addr[i]);
	}

	jpeg_init_hvcount(s);
	jpeg_encode_pic_header(s);
	// we should flush (s->header_offset + 64) bytes, because
	// flush_put_bits will write some data of SOS
	dmabuf_sync_range(dma_buf_fd, s->bitstream_vir_addr, s->header_offset + 64, CACHE_CLEAN);

	if (jpeg_hw_encode(s) < 0) {
		loge("encode failed");
		ret = -1;
		goto out;
	}

	if (s->encode_data_len > buf_len) {
		loge("buf len(%d) is too small, we need (%d) bytes",
			buf_len, s->encode_data_len + s->header_offset);
		ret = -1;
		goto out;
	}
	*len = s->encode_data_len;
	dmabuf_sync_range(dma_buf_fd, s->bitstream_vir_addr, s->encode_data_len, CACHE_INVALID);

out:
	for (i=0; i<comp; i++) {
		ve_rm_dma_buf(frame->buf.fd[i], s->phy_addr[i]);
	}
	dmabuf_munmap(s->bitstream_vir_addr, buf_len);
	ve_rm_dma_buf(dma_buf_fd, s->bitstream_phy_addr);

	jpeg_encoder_destory(s);

	return ret;
}
