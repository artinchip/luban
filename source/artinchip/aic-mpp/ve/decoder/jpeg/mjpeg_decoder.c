/*
 * Copyright (C) 2020-2022 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <qi.xu@artinchip.com>
 *  Desc: jpeg decode
 *
 */

#define LOG_TAG "jpeg_decoder"

#include <stdlib.h>

#include "mjpeg.h"
#include "mjpeg_decoder.h"
#include "jpeg_hal.h"
#include "mpp_log.h"
#include "ve.h"
#include "mpp_mem.h"

static void fill_huffman_startcode(struct mjpeg_dec_ctx *s, int class, int index, const uint8_t *bits_table)
{
	int i,j,k,nb,code;

	code = 0;
	k = 0;
	for(i=1;i<=16;i++) {
		nb = bits_table[i];
		s->huffman_table[class][index].start_code[i - 1] = code;
		s->huffman_table[class][index].max_code[i - 1] = code + nb-1;

		for(j=0;j<nb;j++) {
			s->huffman_table[class][index].code[k] = code;
			s->huffman_table[class][index].len[k] = i;
			k++;
			code++;
		}
		code <<= 1;
	}

	s->huffman_table[class][index].total_code = k;
	for(i=16;i>=1;i--) {
		if (bits_table[i] == 0) {
			s->huffman_table[class][index].start_code[i - 1] = 0xffff;
			s->huffman_table[class][index].max_code[i - 1] = 0xffff;
		}
		else
			break;
	}
}

/*
___________________________                  _______________________________
|                       | |                  |                             |
|    real data          | |    rotate 180    | |---------------------------|
|-----------------------| | -------------->  | |      real data            |
|_________________________|                  |_|___________________________|
*/
static void get_start_offset(struct mjpeg_dec_ctx *s)
{
	int rotate = MPP_ROTATION_GET(s->decoder.rotmir_flag);
	int flip_v = MPP_FLIP_V_GET(s->decoder.rotmir_flag);
	int flip_h = MPP_FLIP_H_GET(s->decoder.rotmir_flag);

	s->h_offset[0] = s->h_offset[1] = s->h_offset[2] = 0;
	s->v_offset[0] = s->v_offset[1] = s->v_offset[2] = 0;
	if ((rotate == MPP_ROTATION_270 && !flip_h && !flip_v) // 0001
		|| (rotate == MPP_ROTATION_90 && (flip_h && flip_v))) { // 1111
		for(int k=0; k<3; k++)
			s->v_offset[k] = s->rm_v_stride[k] - s->rm_v_real_size[k];
	} else if ((rotate == MPP_ROTATION_180 && !flip_h && !flip_v) // 0010
		|| (rotate == MPP_ROTATION_0 && (flip_h && flip_v))) { //1100
		for (int k = 0; k < 3; k++) {
			s->v_offset[k] = s->rm_v_stride[k] - s->rm_v_real_size[k];
			s->h_offset[k] = s->rm_h_stride[k] - s->rm_h_real_size[k];
		}
	} else if ((rotate == MPP_ROTATION_90 && !flip_h && !flip_v) // 0011
		|| (rotate == MPP_ROTATION_270 && (flip_h && flip_v))) { //1101
		for (int k = 0; k<3; k++)
			s->h_offset[k] = s->rm_h_stride[k] - s->rm_h_real_size[k];
	} else if ((rotate == MPP_ROTATION_0 && flip_v) //0100
		|| (rotate == MPP_ROTATION_180 && flip_h)) { // 1010
		for (int k = 0; k<3; k++)
			s->v_offset[k] = s->rm_v_stride[k] - s->rm_v_real_size[k];
	} else if ((rotate == MPP_ROTATION_0 && flip_h) // 1000
		|| (rotate == MPP_ROTATION_180 && flip_v)) { // 0110
		for (int k = 0; k<3; k++)
			s->h_offset[k] = s->rm_h_stride[k] - s->rm_h_real_size[k];
	} else if ((rotate == MPP_ROTATION_90 && flip_h) // 1011
		|| (rotate == MPP_ROTATION_270 && flip_v)) { // 0101
		for (int k = 0; k < 3; k++) {
			s->h_offset[k] = s->rm_h_stride[k] - s->rm_h_real_size[k];
			s->v_offset[k] = s->rm_v_stride[k] - s->rm_v_real_size[k];
		}
	}

	// do nothing for the cases below
	// (s->rotate == MPP_ROTATION_270 && flip_h) //1001
	// (s->rotate == MPP_ROTATION_90 && flip_v) // 0111
	// (s->rotate == MPP_ROTATION_0 && s->mirror == 0) 	// 0000
	// (s->rotate == MPP_ROTATION_180 && s->mirror == (MPP_FLIP_V | MPP_FLIP_H)) // 1110
}

/* quantize tables */
int mjpeg_decode_dqt(struct mjpeg_dec_ctx *s)
{
	int len, index, i;

	len = read_bits(&s->gb, 16) - 2;

	if (8 * len > read_bits_left(&s->gb)) {
		loge("dqt: len %d is too large", len);
		return -1;
	}

	while (len >= 65) {
		int pr = read_bits(&s->gb, 4);
		if (pr > 1) {
			loge("dqt: invalid precision");
			return -1;
		}
		if (pr != 0) {
			loge("dqt: only support 8 bit precision");
			return -1;
		}
		index = read_bits(&s->gb, 4);
		if (index >= 4)
			return -1;
		logd("index=%d", index);
		/* read quant table */
		for (i = 0; i < 64; i++) {
			s->q_matrixes[index][i] = read_bits(&s->gb, pr ? 16 : 8);
		}

		len -= 1 + 64 * (1 + pr);
	}
	return 0;
}

/* decode huffman tables and build VLC decoders */
int mjpeg_decode_dht(struct mjpeg_dec_ctx *s)
{
	int len, index, i, class, n, v, code_max;
	uint8_t bits_table[17];

	logd("====== DHT (huffman table) ======");

	len = read_bits(&s->gb, 16) - 2;

	if (8*len > read_bits_left(&s->gb)) {
		loge("dht: len %d is too large", len);
		return -1;
	}

	while (len > 0) {
		if (len < 17)
			return -1;
		class = read_bits(&s->gb, 4);
		if (class >= 2)
			return -1;
		index = read_bits(&s->gb, 4);
		if (index >= 4)
			return -1;

		logi("class: %d, index: %d", class, index);
		// initial huffman table
		for (i = 0; i < 16; i++) {
			s->huffman_table[class][index].offset[i] = 0;
			s->huffman_table[class][index].start_code[i] = 0;
		}
		for (i = 0; i < 256; i++)
			s->huffman_table[class][index].symbol[i] = 0;

		n = 0;
		// number of huffman code with code length i
		bits_table[0] = 0;

		// 1. parse BITS(BITS is the number of huffman code which is the same
		// code length,
		//     the max code length is 16), generate HUFFSIZE according BITS
		//     table
		for (i = 1; i <= 16; i++) {
			s->huffman_table[class][index].offset[i - 1] = n;
			bits_table[i] = read_bits(&s->gb, 8);
			s->huffman_table[class][index].bits_table[i - 1] = bits_table[i];
			n += bits_table[i];
		}
		len -= 17;
		if (len < n || n > 256)
			return -1;

		code_max = 0;
		// 2.parse HUFFVAL table, the huffman code to symbol
		for (i = 0; i < n; i++) {
			v = read_bits(&s->gb, 8);
			if (v > code_max)
				code_max = v;
			s->huffman_table[class][index].symbol[i] = v;
		}
		len -= n;

		// 3. generate HUFFCODE table, it is used for config ve
		fill_huffman_startcode(s, class, index, bits_table);
	}
	return 0;
}

int mjpeg_decode_sof(struct mjpeg_dec_ctx *s)
{
	int len, nb_components, i, bits;
	int phy_h_stride[4]; // hor stride ( before post-process)
	int phy_v_stride[4]; // ver stride ( before post-process)
	int h_real_size[4];  // hor real size ( before post-process)
	int v_real_size[4];  // hor real size ( before post-process)

	logd("===== ff_mjpeg_decode_sof ====== ");

	len = read_bits(&s->gb, 16);
	bits = read_bits(&s->gb, 8);

	if (bits > 16 || bits < 1) {
		loge("bits %d is invalid", bits);
		return -1;
	}

	/* only 8 bits/component accepted */
	if (bits != 8) {
		loge("only support 8 bits, bits %d is invalid", bits);
		return -1;
	}

	s->height = read_bits(&s->gb, 16);
	s->width = read_bits(&s->gb, 16);

	if (s->width < 1 || s->height < 1) {
		loge("size too small: width %d, height %d", s->width, s->height);
		return -1;
	}

	nb_components = read_bits(&s->gb, 8);
	if (nb_components <= 0 || nb_components > MAX_COMPONENTS) {
		return -1;
	}

	if (len != 8 + 3 * nb_components) {
		loge("decode_sof0: error, len(%d) mismatch %d components", len, nb_components);
		return -1;
	}

	s->nb_components = nb_components;
	for (i = 0; i < nb_components; i++) {
		/* component id */
		s->component_id[i] = read_bits(&s->gb, 8) - 1;
		s->h_count[i]      = read_bits(&s->gb, 4);
		s->v_count[i]      = read_bits(&s->gb, 4);
		s->quant_index[i] = read_bits(&s->gb, 8);
		if (s->quant_index[i] >= 4) {
			loge("quant_index is invalid");
			return -1;
		}
		if (!s->h_count[i] || !s->v_count[i]) {
			loge("Invalid sampling factor in component %d %d:%d",
				i, s->h_count[i], s->v_count[i]);
			return -1;
		}

		logd("component %d %d:%d id: %d quant:%d",i, s->h_count[i], s->v_count[i],
			s->component_id[i], s->quant_index[i]);
	}

	logi("s->width %d, s->height %d", s->width, s->height);

	if (s->h_count[0] == 2 && s->v_count[0] == 2 && s->h_count[1] == 1 &&
		s->v_count[1] == 1 && s->h_count[2] == 1 && s->v_count[2] == 1) {
		// not support nv21
		if (s->out_pix_fmt == MPP_FMT_NV12 || s->out_pix_fmt == MPP_FMT_NV21) {
			s->uv_interleave = 1;
			s->pix_fmt = MPP_FMT_NV12;
		} else {
			s->pix_fmt = MPP_FMT_YUV420P;
		}
		logi("pixel format: yuv420");
	} else if (s->h_count[0] == 4 && s->v_count[0] == 1 && s->h_count[1] == 1 &&
			   s->v_count[1] == 1 && s->h_count[2] == 1 && s->v_count[2] == 1) {
		logi("not support pixel format: yuv411");
		return -1;
	} else if (s->h_count[0] == 2 && s->v_count[0] == 1 && s->h_count[1] == 1 &&
			   s->v_count[1] == 1 && s->h_count[2] == 1 && s->v_count[2] == 1) {
		if (s->out_pix_fmt == MPP_FMT_NV16 || s->out_pix_fmt == MPP_FMT_NV61) {
			s->uv_interleave = 1;
			s->pix_fmt = MPP_FMT_NV16;
		} else {
			s->pix_fmt = MPP_FMT_YUV422P;
		}
		logi("pixel format: yuv422");
	} else if (s->h_count[0] == 1 && s->v_count[0] == 1 && s->h_count[1] == 1 &&
			   s->v_count[1] == 1 && s->h_count[2] == 1 && s->v_count[2] == 1) {
		s->pix_fmt = MPP_FMT_YUV444P;
		logi("pixel format: yuv444");
	} else if (s->h_count[0] == 1 && s->v_count[0] == 2 && s->h_count[1] == 1 &&
			   s->v_count[1] == 2 && s->h_count[2] == 1 && s->v_count[2] == 2) {
		s->pix_fmt = MPP_FMT_YUV444P;
		logi("pixel format: ffmpeg yuv444");
	} else if (s->h_count[0] == 1 && s->v_count[0] == 2 && s->h_count[1] == 1 &&
			   s->v_count[1] == 1 && s->h_count[2] == 1 && s->v_count[2] == 1) {
		logi("not support pixel format: yuv422t");
		return -1;
	} else if (s->h_count[1] == 0 && s->v_count[1] == 0 && s->h_count[2] == 0 &&
			   s->v_count[2] == 0) {
		s->pix_fmt = MPP_FMT_YUV400;
		logi("pixel format: yuv400");
	} else {
		loge("Not support format! h_count: %d %d %d, v_count: %d %d %d",
			s->h_count[0], s->h_count[1], s->h_count[2],
			s->v_count[0], s->v_count[1], s->v_count[2]);
		return -1;
	}

	// get the output size of scale down
	s->nb_mcu_width = (s->width + 8 * s->h_count[0] - 1) / (8 * s->h_count[0]);
	s->nb_mcu_height = (s->height + 8 * s->v_count[0] - 1) / (8 * s->v_count[0]);
	int h_stride_y = (s->nb_mcu_width * s->h_count[0] * 8) >> s->decoder.hor_scale;
	int v_stride_y = (s->nb_mcu_height * s->v_count[0] * 8) >> s->decoder.ver_scale;
	phy_h_stride[0] = phy_h_stride[1] = phy_h_stride[2] = (h_stride_y + 15) / 16 * 16;
	phy_v_stride[0] = phy_v_stride[1] = phy_v_stride[2] = (v_stride_y + 15) / 16 * 16;
	h_real_size[0] = s->width >> s->decoder.hor_scale;
	v_real_size[0] = s->height >> s->decoder.ver_scale;

	if (s->pix_fmt == MPP_FMT_YUV420P) {
		phy_h_stride[1] = phy_h_stride[2] = phy_h_stride[0] / 2;
		phy_v_stride[1] = phy_v_stride[2] = phy_v_stride[0] / 2;
		h_real_size[1] = h_real_size[2] = h_real_size[0] / 2;
		v_real_size[1] = v_real_size[2] = v_real_size[0] / 2;
	} else if (s->pix_fmt == MPP_FMT_YUV444P || s->pix_fmt == MPP_FMT_YUV400) {
		phy_h_stride[0] = (h_stride_y + 7) / 8 * 8;
		phy_v_stride[0] = (v_stride_y + 7) / 8 * 8;
		phy_h_stride[1] = phy_h_stride[2] = phy_h_stride[0];
		phy_v_stride[1] = phy_v_stride[2] = phy_v_stride[0];
		h_real_size[1] = h_real_size[2] = h_real_size[0];
		v_real_size[1] = v_real_size[2] = v_real_size[0];
	} else if (s->pix_fmt == MPP_FMT_YUV422P) {
		phy_h_stride[1] = phy_h_stride[2] = phy_h_stride[0] / 2;
		phy_v_stride[1] = phy_v_stride[2] = phy_v_stride[0];
		h_real_size[1] = h_real_size[2] = h_real_size[0] / 2;
		v_real_size[1] = v_real_size[2] = v_real_size[0];
	}

	// get the output size of rotate
	if (MPP_ROTATION_GET(s->decoder.rotmir_flag) == MPP_ROTATION_270 ||
		MPP_ROTATION_GET(s->decoder.rotmir_flag) == MPP_ROTATION_90) {
		for (int k = 0; k < 3; k++) {
			s->rm_h_real_size[k] = v_real_size[k];
			s->rm_v_real_size[k] = h_real_size[k];
			s->rm_h_stride[k] = phy_v_stride[k];
			s->rm_v_stride[k] = phy_h_stride[k];
		}
	} else {
		for (int k = 0; k < 3; k++) {
			s->rm_h_real_size[k] = h_real_size[k];
			s->rm_v_real_size[k] = v_real_size[k];
			s->rm_h_stride[k] = phy_h_stride[k];
			s->rm_v_stride[k] = phy_v_stride[k];
		}
	}

	s->scale_width  = s->rm_h_stride[0];
	s->scale_height = s->rm_v_stride[0];
	// get the start offset of output
	get_start_offset(s);

	s->got_picture = 1;

	return 0;
}

static void set_frame_info(struct mjpeg_dec_ctx *s)
{
	if(s->curr_packet->flag & PACKET_FLAG_EOS)
		s->curr_frame->mpp_frame.flags |= FRAME_FLAG_EOS;
	s->curr_frame->mpp_frame.buf.flags |= MPP_COLOR_SPACE_BT601_FULL_RANGE;
	s->curr_frame->mpp_frame.buf.crop_en = 1;
	s->curr_frame->mpp_frame.buf.crop.x = 0;
	s->curr_frame->mpp_frame.buf.crop.y = 0;
	s->curr_frame->mpp_frame.buf.crop.width = s->rm_h_real_size[0];
	s->curr_frame->mpp_frame.buf.crop.height = s->rm_v_real_size[0];
	s->curr_frame->mpp_frame.pts = s->curr_packet->pts;
}

int mjpeg_decode_sos(struct mjpeg_dec_ctx *s,
                     const uint8_t *mb_bitmask,
                     int mb_bitmask_size)
{
	int len, nb_components, i;
	int index, id;

	logd("===== ff_mjpeg_decode_sos ====== ");

	if (!s->got_picture) {
		logw("Can not process SOS before SOF, skipping");
		return -1;
	}

	// 1. parse SOS info
	len = read_bits(&s->gb, 16);
	nb_components = read_bits(&s->gb, 8);
	if (nb_components == 0 || nb_components > MAX_COMPONENTS) {
		return -1;
	}
	if (len != 6 + 2 * nb_components) {
		loge("decode_sos: invalid len (%d)", len);
		return -1;
	}

	for (i = 0; i < nb_components; i++) {
		id = read_bits(&s->gb, 8) - 1;
		/* find component index */
		for (index = 0; index < s->nb_components; index++)
			if (id == s->component_id[index])
				break;
		if (index == s->nb_components) {
			loge("decode_sos: index(%d) out of components", index);
			return -1;
		}

		s->dc_index[i] = read_bits(&s->gb, 4);
		s->ac_index[i] = read_bits(&s->gb, 4);

		if (s->dc_index[i] <  0 || s->ac_index[i] < 0 ||
			s->dc_index[i] >= 4 || s->ac_index[i] >= 4) {
			loge("index out of range");
			return -1;
		}
	}

	read_bits(&s->gb, 8);	/* JPEG Ss / lossless JPEG predictor /JPEG-LS NEAR */
	read_bits(&s->gb, 8);	/* JPEG Se / JPEG-LS ILV */
	read_bits(&s->gb, 4);	/* Ah */
	read_bits(&s->gb, 4);	/* Al */

	int sos_size = read_bits_count(&s->gb) / 8;
	logd("sos_size %d", sos_size);

	if(s->decoder.fm == NULL) {
		struct frame_manager_init_cfg cfg;
		cfg.frame_count = 1 + s->extra_frame_num;
		cfg.height = s->scale_height;
		cfg.width = s->scale_width;
		cfg.height_align = s->rm_v_stride[0];
		cfg.stride = s->rm_h_stride[0];
		cfg.pixel_format = s->pix_fmt;
		cfg.allocator = s->decoder.allocator;
		s->decoder.fm = fm_create(&cfg);
	}

	s->curr_frame = fm_decoder_get_frame(s->decoder.fm);
	if(s->curr_frame == NULL) {
		pm_reclaim_ready_packet(s->decoder.pm, s->curr_packet);
		return DEC_NO_EMPTY_FRAME;
	}

#ifdef COPY_DATA
	s->sos_length = s->raw_scan_buffer_size - sos_size;
	s->sos_buf = ve_buffer_alloc(s->ve_buf_handle, (s->sos_length / 512 +1) * 512, ALLOC_NEED_VIR_ADDR);
	memcpy(s->sos_buf->vir_addr, s->raw_scan_buffer+sos_size, s->sos_length);
	ve_buffer_sync(s->sos_buf, CACHE_CLEAN);
#endif

	int offset = (s->raw_scan_buffer - s->curr_packet->data) + sos_size;

	logd("offste: %d", offset);
	if(ve_decode_jpeg(s, offset))
		return -1;

	set_frame_info(s);

	fm_decoder_frame_to_render(s->decoder.fm, s->curr_frame, 1);
	fm_decoder_put_frame(s->decoder.fm, s->curr_frame);

	return 0;
}

static int mjpeg_decode_dri(struct mjpeg_dec_ctx *s)
{
	if (read_bits(&s->gb, 16) != 4)
		return -1;
	s->restart_interval = read_bits(&s->gb, 16);
	s->restart_count    = 0;
	logd("restart interval: %d", s->restart_interval);

	return 0;
}

/* return the 8 bit start code value and update the search
   state. Return -1 if no start code found */
static int find_marker(const uint8_t **pbuf_ptr, const uint8_t *buf_end)
{
	const uint8_t *buf_ptr;
	unsigned int v, v2;
	int val;
	int skipped = 0;

	buf_ptr = *pbuf_ptr;
	while (buf_end - buf_ptr > 1) {
		v  = *buf_ptr++;
		v2 = *buf_ptr;
		if ((v == 0xff) && (v2 >= SOF0) && (v2 <= COM) && buf_ptr < buf_end) {
			val = *buf_ptr++;
			goto found;
		}
		skipped++;
	}
	buf_ptr = buf_end;
	val = -1;
found:
	*pbuf_ptr = buf_ptr;

	return val;
}

int mjpeg_find_marker(struct mjpeg_dec_ctx *s,
			const uint8_t **buf_ptr, const uint8_t *buf_end,
			const uint8_t **unescaped_buf_ptr,
			int *unescaped_buf_size)
{
	int start_code;
	start_code = find_marker(buf_ptr, buf_end);

	*unescaped_buf_ptr  = *buf_ptr;
	*unescaped_buf_size = buf_end - *buf_ptr;

	return start_code;
}

static void skip_variable_marker(struct mjpeg_dec_ctx *s)
{
	int left = read_bits(&s->gb, 16);
	left -= 2;
	while(left) {
		skip_bits(&s->gb, 8);
		left --;
	}
}

int __mjpeg_decode_frame(struct mpp_decoder *ctx)
{
	struct mjpeg_dec_ctx *s = (struct mjpeg_dec_ctx*)ctx;
	int buf_size;
	const uint8_t *buf_end, *buf_ptr;
	const uint8_t *unescaped_buf_ptr;
	int unescaped_buf_size;
	int start_code;
	int ret = 0;

	s->curr_packet = pm_dequeue_ready_packet(s->decoder.pm);

	if(s->curr_packet == NULL) {
		loge("pm_dequeue_ready_packet error, ready_packet num: %d", pm_get_ready_packet_num(s->decoder.pm));
		return DEC_NO_READY_PACKET;
	}

	buf_size = s->curr_packet->size;
	buf_ptr = s->curr_packet->data;
	buf_end = buf_ptr + buf_size;

	start_code = 0xffd8;
	if (buf_size < 8 || !memchr(buf_ptr, start_code, 8)) {
		loge("The file is not jpeg!");
		return -1;
	}

	while (buf_ptr < buf_end) {
		/* find start next marker */
		start_code = mjpeg_find_marker(s, &buf_ptr, buf_end,
							&unescaped_buf_ptr,
							&unescaped_buf_size);
		/* EOF */
		if (start_code < 0) {
			break;
		} else if (unescaped_buf_size > INT_MAX / 8) {
			loge("MJPEG packet 0x%x too big (%d/%d), corrupt data?",
				start_code, unescaped_buf_size, buf_size);
			return -1;
		}

		ret = init_read_bits(&s->gb, unescaped_buf_ptr, unescaped_buf_size*8);

		if (ret < 0) {
			loge("invalid buffer");
			goto fail;
		}

		s->start_code = start_code;
		logi("startcode: %02x", start_code);

		/* process markers */
		if (start_code >= RST0 && start_code <= RST7) {
			logd("restart marker: %d", start_code & 0xff);
			/* APP fields */
		} else if (start_code >= APP0 && start_code <= APP15) {
			logd("app marker: %d", start_code & 0xff);
			/* Comment */
		} else if (start_code == COM) {
			logd("com marker: %d", start_code & 0xff);
		}

		ret = -1;

		switch (start_code) {
		case DQT:
			ret = mjpeg_decode_dqt(s);
			if (ret < 0)
				return ret;
			break;
		case SOI:
			s->restart_interval = 0;
			s->restart_count = 0;
			/* nothing to do on SOI */
			break;
		case DHT:
			s->have_dht = 1;
			if ((ret = mjpeg_decode_dht(s)) < 0) {
				loge("huffman table decode error");
				goto fail;
			}
			break;
		case SOF0:
			logi("baseline");
		case SOF1:
			logi("SOF1");
			if ((ret = mjpeg_decode_sof(s)) < 0)
				goto fail;
			break;
		case SOF2:
			loge("progressive, not support");
			if ((ret = mjpeg_decode_sof(s)) < 0)
				goto fail;
			break;
		case LSE:
			break;
		case EOI:
			if (!s->got_picture) {
				logw("Found EOI before any SOF, ignoring");
				break;
			}
			s->got_picture = 0;

			goto the_end;
		case SOS:
			s->raw_scan_buffer = buf_ptr;
			s->raw_scan_buffer_size = buf_end - buf_ptr;

			if ((ret = mjpeg_decode_sos(s, NULL, 0)) != 0)
				goto fail;

			pm_enqueue_empty_packet(s->decoder.pm, s->curr_packet);
			goto the_end;
		case DRI:
			if ((ret = mjpeg_decode_dri(s)) < 0)
				return ret;
			break;
		case SOF3:
		case SOF48:
		case SOF5:
		case SOF6:
		case SOF7:
		case SOF9:
		case SOF10:
		case SOF11:
		case SOF13:
		case SOF14:
		case SOF15:
		case JPG:
			loge("mjpeg: unsupported coding type (%x)", start_code);
			break;
		default:
			logi("skip this marker: %02x", start_code);
			skip_variable_marker(s);
			break;
		}

		/* eof process start code */
		buf_ptr += (read_bits_count(&s->gb) + 7) / 8;
		logd("marker parser used %d bytes (%d bits)", (read_bits_count(&s->gb) + 7) / 8, read_bits_count(&s->gb));
	}

	loge("No JPEG data found in image");
	return -1;
fail:
	s->got_picture = 0;
	return ret;
the_end:
	return 0;
}

int __mjpeg_decode_init(struct mpp_decoder *ctx, struct decode_config *config)
{
	if (!ctx)
		return -1;

	struct mjpeg_dec_ctx *s = (struct mjpeg_dec_ctx *)ctx;
	s->out_pix_fmt = config->pix_fmt;

	s->ve_fd = ve_open_device();
	if (s->ve_fd < 0) {
		loge("ve open failed");
		return -1;
	}

	s->regs_base = ve_get_reg_base();

	s->ve_buf_handle = ve_buffer_allocator_create(VE_BUFFER_TYPE_DMA);

	struct packet_manager_init_cfg cfg;
	cfg.ve_buf_handle = s->ve_buf_handle;
	cfg.buffer_size = config->bitstream_buffer_size;
	cfg.packet_count = config->packet_count;
	s->decoder.pm = pm_create(&cfg);
	s->extra_frame_num = config->extra_frame_num;
	s->start_code    = -1;
	s->first_picture = 1;
	s->got_picture   = 0;

	if (s->out_pix_fmt == MPP_FMT_YUV420P)
		logw("default pix fmt %d", MPP_FMT_YUV420P);

	s->reg_list = mpp_alloc(sizeof(jpg_reg_list));
	if(!s->reg_list)
		return -1;
	memset(s->reg_list, 0, sizeof(jpg_reg_list));

    return 0;
}

int __mjpeg_decode_destroy(struct mpp_decoder *ctx)
{
	struct mjpeg_dec_ctx *s = (struct mjpeg_dec_ctx *)ctx;

	if (s->reg_list)
		mpp_free(s->reg_list);

	if(s->decoder.fm)
		fm_destory(s->decoder.fm);

	if(s->decoder.pm)
		pm_destroy(s->decoder.pm);

#ifdef COPY_DATA
	if(s->sos_buf)
		ve_buffer_free(s->ve_buf_handle, s->sos_buf);
#endif
	if(s->ve_buf_handle)
		ve_buffer_allocator_destroy(s->ve_buf_handle);

	ve_close_device();

	mpp_free(s);
	return 0;
}

int __mjpeg_decode_control(struct mpp_decoder *ctx, int cmd, void *param)
{
	// TODO
	return 0;
}

int __mjpeg_decode_reset(struct mpp_decoder *ctx)
{
	// TODO
	struct mjpeg_dec_ctx *s = (struct mjpeg_dec_ctx *)ctx;
	fm_decoder_reclaim_all_used_frame(s->decoder.fm);
	fm_reset(s->decoder.fm);
	pm_reset(s->decoder.pm);
	return 0;
}

struct dec_ops mjpeg_decoder = {
	.name           = "mjpeg",
	.init           = __mjpeg_decode_init,
	.destory        = __mjpeg_decode_destroy,
	.decode         = __mjpeg_decode_frame,
	.control        = __mjpeg_decode_control,
	.reset          = __mjpeg_decode_reset,
};

struct mpp_decoder* create_jpeg_decoder()
{
	struct mjpeg_dec_ctx *s = (struct mjpeg_dec_ctx*)mpp_alloc(sizeof(struct mjpeg_dec_ctx));
	if(s == NULL)
		return NULL;
	memset(s, 0, sizeof(struct mjpeg_dec_ctx));

	s->decoder.ops = &mjpeg_decoder;

	return &s->decoder;
}

