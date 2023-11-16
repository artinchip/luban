/*
* Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
*
*  author: <qi.xu@artinchip.com>
*  Desc: h264 decoder interface
*
*/

#define LOG_TAG "h264_decoder"
#include <stdlib.h>
#include <string.h>

#include "h264_decoder.h"
#include "h264_nal.h"
#include "ve.h"
#include "mpp_mem.h"
#include "mpp_log.h"

void h264_free_frame_buffer(struct h264_dec_ctx *s)
{
	if(s->decoder.fm)
		fm_destory(s->decoder.fm);

	if(s->frame_info.col_buf)
		ve_buffer_free(s->ve_buf_handle, s->frame_info.col_buf);
	if(s->frame_info.dblk_y_buf)
		ve_buffer_free(s->ve_buf_handle, s->frame_info.dblk_y_buf);
	if(s->frame_info.dblk_c_buf)
		ve_buffer_free(s->ve_buf_handle, s->frame_info.dblk_c_buf);
	if(s->frame_info.intrap_buf)
		ve_buffer_free(s->ve_buf_handle, s->frame_info.intrap_buf);
	if(s->frame_info.mb_info_buf)
		ve_buffer_free(s->ve_buf_handle, s->frame_info.mb_info_buf);
	if(s->frame_info.mb_col_info_buf)
		ve_buffer_free(s->ve_buf_handle, s->frame_info.mb_col_info_buf);
}

static int find_startcode(unsigned char* buf, int len)
{
	int i = 0;
	while(i + 2 < len) {
		if(buf[i] == 0 && buf[i+1]==0 && buf[i+2] == 1) {
			return i+3;
		}
		i++;
	}

	return i == len-2 ? -1 : 0;
}

/**
* @dst: [out] remove eptb buffer
* @offset: [out] offset of first 0x03 byte in eptb
* @src: [in]  input data
* @len: [in]  length of input buffer
* return: remove bytes number
*/
static int remove_eptb(unsigned char* dst, int* offset, unsigned char* src, int len )
{
	int si = 0;
	int di = 0;
	while(si+2<len && si<RBSP_BYTES) {
		if(src[si+2] > 3) {
			dst[di++] = src[si++];
			dst[di++] = src[si++];
		} else if(src[si]==0 && src[si+1]==0 && src[si+2]!=0) {
			if(src[si+2] == 3) { // escape, remove 0x03
				dst[di++] = 0;
				dst[di++] = 0;
				*offset = si+2;
				si += 3;
				continue;
			} else { // next start code
				break;
			}
		}
		dst[di++] = src[si++];
	}

	while(si < len && si<RBSP_BYTES)
		dst[di++] = src[si++];

	return si-di;
}

static int process_extradata(struct h264_dec_ctx *s, unsigned char* buf, int len)
{
	int i, cnt, nal_size;
	const unsigned char* p = buf;
	if (buf[0] != 1) {
		loge("extradata is not avcc");
		return -1;
	}
	if (len < 7) {
		loge("avcc is too short");
		return -1;
	}

	// decode sps
	cnt = *(p+5) & 0x1f; // number of sps
	p+=6;
	for (i=0; i<cnt; i++) {
		nal_size = (p[0] << 8) | p[1];
		p += 2;
		init_read_bits(&s->gb, p, nal_size * 8);
		skip_bits(&s->gb, 8); // nalu type
		if (h264_decode_sps(s)) {
			loge("decode sps failed");
			return -1;
		}
		p += nal_size;
	}

	// decode pps
	cnt = *(p++);
	for (i=0; i<cnt; i++) {
		nal_size = (p[0] << 8) | p[1];
		p += 2;
		logi("pps cnt: %d, nal_size: %d", cnt, nal_size);
		init_read_bits(&s->gb, p, nal_size * 8);
		skip_bits(&s->gb, 8); // nalu type
		if (h264_decode_pps(s)) {
			loge("decode pps failed");
			return -1;
		}
		p += nal_size;
	}
	return 0;
}

static int procese_nalu(struct h264_dec_ctx *s, unsigned char* buf, int len, int *use_len)
{
	int ret = 0;
	int i = 0;
	int error_flag = 0;

	// remove startcode
	s->sc_byte_offset = s->avcc? 4: find_startcode(buf, len);
	if (s->sc_byte_offset < 0) {
		// cannot find startcode, skip it
		*use_len = len;
		return 0;
	}

	s->nal_ref_idc = buf[s->sc_byte_offset] & 0x60;
	s->nal_unit_type = buf[s->sc_byte_offset] & 0x1f;

	// remove eptb only in Slice NALU, maybe error ??
	if(s->nal_unit_type == NAL_TYPE_IDR || s->nal_unit_type == NAL_TYPE_SLICE) {
		s->remove_bytes = remove_eptb(s->rbsp_buffer, &s->first_eptb_offset, buf+s->sc_byte_offset, len-s->sc_byte_offset);
		s->rbsp_len = len - s->sc_byte_offset - s->remove_bytes;
		logd("s->sc_byte_offset: %d, s->remove_bytes: %d, s->rbsp_len: %d", s->sc_byte_offset, s->remove_bytes, s->rbsp_len);
		init_read_bits(&s->gb, s->rbsp_buffer, s->rbsp_len * 8);
	} else {
		init_read_bits(&s->gb, buf+s->sc_byte_offset, (len-s->sc_byte_offset) * 8);
	}

	read_bits(&s->gb, 8); // nalu type

	s->idr_pic_flag = 0;
	switch(s->nal_unit_type) {
		case NAL_TYPE_IDR:
		{
			logd("idr frame");
			s->idr_pic_flag = 1;
			s->prev_frame_num = s->prev_frame_num_offset = 0;
			s->prev_poc_msb = 1 << 16;
			s->prev_poc_lsb = -1;
			render_all_delayed_frame(s);

			// refresh reference frame
			reference_refresh(s);
			s->next_output_poc = INT_MIN;
			for (i = 0; i < MAX_DELAYED_PIC_COUNT; i++)
				s->frame_info.last_pocs[i] = INT_MIN;
		}
		case NAL_TYPE_SLICE:
		{
			logd("decode slice");
			ret = h264_decode_slice_header(s);
			if (ret) {
				return ret;
			}

			ret = decode_slice(s);
			if(ret) {
				loge("decode_slice error, ret: %d", ret);
				error_flag = 1;
				ret = DEC_ERR_NOT_SUPPORT;
			}

			*use_len = len;
			break;
		}
		case NAL_TYPE_SPS:
		{
			logd("sps");
			ret = h264_decode_sps(s);
			*use_len = read_bits_count(&s->gb) / 8 +
				s->sc_byte_offset + s->remove_bytes;
			break;
		}
		case NAL_TYPE_PPS:
		{
			logd("pps");
			ret = h264_decode_pps(s);
			*use_len = read_bits_count(&s->gb) / 8 +
				s->sc_byte_offset + s->remove_bytes;
			break;
		}
		default:
			// maybe it will waste time here
			*use_len = read_bits_count(&s->gb) / 8 +
				s->sc_byte_offset + s->remove_bytes + 30;
			return 0;
	}

	if(s->nal_unit_type != NAL_TYPE_IDR && s->nal_unit_type != NAL_TYPE_SLICE)
		return ret;

	s->prev_poc_lsb = s->sh.poc_lsb;
	s->prev_poc_msb = s->sh.poc_msb;
	s->prev_frame_num = s->sh.frame_num;
	s->prev_frame_num_offset = s->frame_num_offset;

	if ((s->decode_mb_num >= s->mbs_in_pic || error_flag) && s->nal_ref_idc) {
		// execute_ref_pic
		execute_ref_pic_marking(s);
	}
	logd("s->first_field: %d", s->first_field);

	if(s->first_field == 1) {
		// wait for second field
		return 0;
	}

	// sort the display picture
	if ((s->decode_mb_num >= s->mbs_in_pic || error_flag) && s->frame_info.cur_pic_ptr) {
		select_output_frame(s);
		// sort
	}

	return ret;
}

int __h264_decode_init(struct mpp_decoder *ctx, struct decode_config *config)
{
	struct h264_dec_ctx *s = (struct h264_dec_ctx*)ctx;
	s->ve_buf_handle = ve_buffer_allocator_create(VE_BUFFER_TYPE_DMA);

	s->extra_frame_num = config->extra_frame_num;
	s->b_frames_max_num = MAX_B_FRAMES; // it is a test val

	struct packet_manager_init_cfg cfg;
	cfg.buffer_size = config->bitstream_buffer_size;
	cfg.ve_buf_handle = s->ve_buf_handle;
	cfg.packet_count = config->packet_count;
	s->decoder.pm = pm_create(&cfg);

	s->pix_format = config->pix_fmt;
	if(config->pix_fmt != MPP_FMT_YUV420P && config->pix_fmt != MPP_FMT_NV12
		&& config->pix_fmt != MPP_FMT_NV21) {
		logw("unsupport pix format, force to yuv420p");
		s->pix_format = MPP_FMT_YUV420P;
	}

	return 0;
}

int __h264_decode_frame(struct mpp_decoder *ctx)
{
	logd("__h264_decode_frame");
	int ret = 0;
	struct h264_dec_ctx *s = (struct h264_dec_ctx*)ctx;
	s->slice_offset = 0;

	//* 1. get a packet data
	s->curr_packet = pm_dequeue_ready_packet(s->decoder.pm);
	if(s->curr_packet == NULL) {
		loge("pm_dequeue_ready_packet error, ready_packet num: %d", pm_get_ready_packet_num(s->decoder.pm));
		return DEC_NO_READY_PACKET;
	}

	s->eos = s->curr_packet->flag & PACKET_FLAG_EOS;

	//* 2. process extra data
	if(s->curr_packet->flag & PACKET_FLAG_EXTRA_DATA) {
		s->avcc = 1;
		ret = process_extradata(s, s->curr_packet->data, s->curr_packet->size);
		pm_enqueue_empty_packet(s->decoder.pm, s->curr_packet);
		return ret;
	}

	//* 3. process this packet
	while (s->slice_offset+4 < s->curr_packet->size) {
		int use_len = 0;
		unsigned char* pos = s->curr_packet->data + s->slice_offset;
		ret = procese_nalu(s, s->curr_packet->data + s->slice_offset,
			s->curr_packet->size - s->slice_offset, &use_len);
		if(ret) {
			if(ret == DEC_NO_EMPTY_FRAME)
				pm_reclaim_ready_packet(s->decoder.pm, s->curr_packet);
			else
				pm_enqueue_empty_packet(s->decoder.pm, s->curr_packet);
			return ret;
		}

		if (s->avcc)
			use_len = ((uint32_t)pos[0]<<24 | (uint32_t)pos[1]<<16 |
				(uint32_t)pos[2] << 8 | (uint32_t)pos[3]) + 4;

		s->slice_offset += use_len;
		logi("offset: %d 0x%x", s->slice_offset, s->slice_offset);
	}

	pm_enqueue_empty_packet(s->decoder.pm, s->curr_packet);

	if(s->curr_packet->flag & PACKET_FLAG_EOS) {
		render_all_delayed_frame(s);
	}
	logd("__h264_decode_frame end");

	return 0;
}

int __h264_decode_destroy(struct mpp_decoder *ctx)
{
	int i;
	struct h264_dec_ctx *s = (struct h264_dec_ctx *)ctx;

	h264_free_frame_buffer(s);

	if(s->decoder.pm)
		pm_destroy(s->decoder.pm);

	if(s->ve_buf_handle)
		ve_buffer_allocator_destroy(s->ve_buf_handle);

	for(i=0; i<SPS_MAX_NUM; i++) {
		if(s->sps_buffers[i]) {
			mpp_free(s->sps_buffers[i]);
		}
	}

	for(i=0; i<PPS_MAX_NUM; i++) {
		if(s->pps_buffers[i]) {
			mpp_free(s->pps_buffers[i]);
		}
	}

	ve_close_device();

	if(s->fp_reg)
		fclose(s->fp_reg);

	mpp_free(s);
	return 0;
}

int __h264_decode_control(struct mpp_decoder *ctx, int cmd, void *param)
{
	// TODO
	return 0;
}

int __h264_decode_reset(struct mpp_decoder *ctx)
{
	int i = 0;
	struct h264_dec_ctx *s = (struct h264_dec_ctx *)ctx;
	render_all_delayed_frame(s);
	// refresh reference frame
	reference_refresh(s);
	s->next_output_poc = INT_MIN;
	for (i = 0; i < MAX_DELAYED_PIC_COUNT; i++)
		s->frame_info.last_pocs[i] = INT_MIN;

	fm_reset(s->decoder.fm);
	pm_reset(s->decoder.pm);

	return 0;
}

struct dec_ops h264_decoder = {
	.name           = "h264",
	.init           = __h264_decode_init,
	.destory        = __h264_decode_destroy,
	.decode         = __h264_decode_frame,
	.control        = __h264_decode_control,
	.reset          = __h264_decode_reset,
};

struct mpp_decoder* create_h264_decoder()
{
	int i;
	struct h264_dec_ctx *s = (struct h264_dec_ctx*)mpp_alloc(sizeof(struct h264_dec_ctx));
	if(s == NULL)
		return NULL;
	memset(s, 0, sizeof(struct h264_dec_ctx));

	s->decoder.ops = &h264_decoder;
	if(ve_open_device() < 0) {
		mpp_free(s);
		return NULL;
	}
	s->regs_base = ve_get_reg_base();

#ifdef SAVE_REG
	s->fp_reg = fopen("/usr/reg_trace.txt", "wb");
	if(s->fp_reg == NULL) {
		loge("reg_trace.txt open fialed");
	}
#endif

	s->next_output_poc = INT_MIN;
	s->avc_start = 1;
	for(i=0; i<MAX_DELAYED_PIC_COUNT; i++) {
		s->frame_info.last_pocs[i] = INT_MIN;
	}

	return &s->decoder;
}
