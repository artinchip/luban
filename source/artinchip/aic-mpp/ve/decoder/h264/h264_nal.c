/*
 * Copyright (C) 2020-2024 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <qi.xu@artinchip.com>
 *  Desc: h264 sps/pps parse
 *
 */
#include <stdlib.h>
#include <string.h>
#include "h264_decoder.h"
#include "h264_nal.h"
#include "read_bits.h"
#include "mpp_mem.h"
#include "mpp_log.h"

#define BASELINE_PROFILE	66
#define MAIN_PROFILE		77
#define EXTEND_PROFILE		88
#define HIGH_PROFILE		100

#define MAX_FRAME_NUM           18

#define MIN(a,b) ((a) > (b) ? (b) : (a))
static const u8 default_scaling4[2][16]=
{
	{
		6,13,20,28,
		13,20,28,32,
		20,28,32,37,
		28,32,37,42
	},
	{
		10,14,20,24,
		14,20,24,27,
		20,24,27,30,
		24,27,30,34
	}
};

static const u8 default_scaling8[2][64]=
{
	{
		6,10,13,16,18,23,25,27,
		10,11,16,18,23,25,27,29,
		13,16,18,23,25,27,29,31,
		16,18,23,25,27,29,31,33,
		18,23,25,27,29,31,33,36,
		23,25,27,29,31,33,36,38,
		25,27,29,31,33,36,38,40,
		27,29,31,33,36,38,40,42
	},
	{
		9,13,15,17,19,21,22,24,
		13,13,17,19,21,22,24,25,
		15,17,19,21,22,24,25,27,
		17,19,21,22,24,25,27,28,
		19,21,22,24,25,27,28,30,
		21,22,24,25,27,28,30,32,
		22,24,25,27,28,30,32,33,
		24,25,27,28,30,32,33,35
	}
};

static const u8 zigzag_scan[16]=
{
    0+0*4, 1+0*4, 0+1*4, 0+2*4,
    1+1*4, 2+0*4, 3+0*4, 2+1*4,
    1+2*4, 0+3*4, 1+3*4, 2+2*4,
    3+1*4, 3+2*4, 2+3*4, 3+3*4,
};

static const u8 zigzag_scan8x8[64]=
{
    0+0*8, 1+0*8, 0+1*8, 0+2*8,
    1+1*8, 2+0*8, 3+0*8, 2+1*8,
    1+2*8, 0+3*8, 0+4*8, 1+3*8,
    2+2*8, 3+1*8, 4+0*8, 5+0*8,
    4+1*8, 3+2*8, 2+3*8, 1+4*8,
    0+5*8, 0+6*8, 1+5*8, 2+4*8,
    3+3*8, 4+2*8, 5+1*8, 6+0*8,
    7+0*8, 6+1*8, 5+2*8, 4+3*8,
    3+4*8, 2+5*8, 1+6*8, 0+7*8,
    1+7*8, 2+6*8, 3+5*8, 4+4*8,
    5+3*8, 6+2*8, 7+1*8, 7+2*8,
    6+3*8, 5+4*8, 4+5*8, 3+6*8,
    2+7*8, 3+7*8, 4+6*8, 5+5*8,
    6+4*8, 7+3*8, 7+4*8, 6+5*8,
    5+6*8, 4+7*8, 5+7*8, 6+6*8,
    7+5*8, 7+6*8, 6+7*8, 7+7*8,
};

static int dec_ref_pic_marking(struct h264_dec_ctx *s)
{
	struct h264_slice_header* sh = &s->sh;
	int nb_mmco = 0;
	int i;

	if(s->idr_pic_flag == 1) { // NAL_IDR
		read_bits(&s->gb, 1); // no_output_of_prior_pics_flag
		if(read_bits(&s->gb, 1)) {
			sh->mmco[0].opcode	= MMCO_LONG;
			sh->mmco[0].long_arg	= 0;
			nb_mmco			= 1;
		}
		sh->explicit_ref_marking = 1;
	} else {
		sh->explicit_ref_marking = read_bits(&s->gb, 1);
		if(sh->explicit_ref_marking) {
			for(i=0; i<MAX_MMCO_COUNT; i++) {
				enum mmco_opcode opcode = read_ue_golomb(&s->gb);

				sh->mmco[i].opcode = opcode;
				if(opcode == MMCO_SHORT2UNUSED || opcode == MMCO_SHORT2LONG) {
					sh->mmco[i].short_pic_num = (sh->cur_pic_num - read_ue_golomb(&s->gb) -1) &
						(sh->max_pic_num -1);
				}
				if(opcode == MMCO_SHORT2LONG || opcode == MMCO_LONG2UNUSED ||
                    		   opcode == MMCO_LONG || opcode == MMCO_SET_MAX_LONG) {
					unsigned int long_arg = read_ue_golomb(&s->gb);
					if (long_arg >= 32 ||
						(long_arg >= 16 && !(opcode == MMCO_SET_MAX_LONG &&
								long_arg == 16) &&
						!(opcode == MMCO_LONG2UNUSED && s->picture_structure != PICT_FRAME))) {
						loge("illegal long ref in memory management control operation %d", opcode);
						sh->nb_mmco = i;
						return -1;
					}
					sh->mmco[i].long_arg = long_arg;
				}

				if(opcode > (unsigned)MMCO_LONG) {
					loge("illegal memory management control operation %d", opcode);
					sh->nb_mmco = i;
					return -1;
				}

				if(opcode == MMCO_END)
					break;
			}
			nb_mmco = i;
		}
	}
	sh->nb_mmco = nb_mmco;

	return 0;
}

static int pred_weight_table(struct h264_dec_ctx *s)
{
	int list, i, j;
	struct h264_sps_info* cur_sps = s->sps_buffers[s->active_sps_id];
	struct h264_slice_header* sh = &s->sh;
	struct pred_weight_table* pwt = &sh->pwt;
	int luma_def = 0, chroma_def = 0;

	pwt->luma_log2_weight_denom = read_ue_golomb(&s->gb);
	if(pwt->luma_log2_weight_denom > 7) {
		loge("luma_log2_weight_denom(%d) out of range", pwt->luma_log2_weight_denom);
		pwt->luma_log2_weight_denom = 0;
	}
	luma_def = 1 << pwt->luma_log2_weight_denom;

	if(cur_sps->chroma_format_idc) {
		pwt->chroma_log2_weight_denom = read_ue_golomb(&s->gb);
		if(pwt->chroma_log2_weight_denom > 7) {
			loge("chroma_log2_weight_denom(%d) out of range", pwt->chroma_log2_weight_denom);
			pwt->chroma_log2_weight_denom = 0;
		}
		chroma_def = 1 << pwt->chroma_log2_weight_denom;
	}

	for(list=0; list<2; list++) {
		for(i=0; i<sh->num_ref_idx[list]; i++) {
			if(read_bits(&s->gb, 1)) {
				pwt->wp_weight[list][i][0] = read_se_golomb(&s->gb);
				pwt->wp_offset[list][i][0] = read_se_golomb(&s->gb);
			} else {
				pwt->wp_weight[list][i][0] = luma_def;
				pwt->wp_offset[list][i][0] = 0;
			}

			if(cur_sps->chroma_format_idc) {
				if(read_bits(&s->gb, 1)) {
					for(j=1; j<3; j++) {
						pwt->wp_weight[list][i][j] = read_se_golomb(&s->gb);
						pwt->wp_offset[list][i][j] = read_se_golomb(&s->gb);
					}
				} else {
					for(j=1; j<3; j++) {
						pwt->wp_weight[list][i][j] = chroma_def;
						pwt->wp_offset[list][i][j] = 0;
					}
				}
			}
		}

		if(sh->slice_type != H264_SLICE_B)
			break;
	}

	return 0;
}

static int h264_alloc_frame_buffer(struct h264_dec_ctx *s)
{
	int i;
	logi("h264_alloc_frame_buffer");
	int field_col_buf_size = 0;
	int total_col_buf_size = 0;
	int dblk_y_buf_size = 0;
	int dblk_c_buf_size = 0;
	int intrap_buf_size = 0;
	int mb_info_buf_size = 0;
	int mb_col_info_size = 0;
	struct h264_sps_info* cur_sps = s->sps_buffers[s->active_sps_id];

	s->frame_info.max_valid_frame_num = cur_sps->max_num_ref_frames + 1 + s->extra_frame_num;

	if(s->frame_info.max_valid_frame_num > MAX_FRAME_NUM) {
		s->frame_info.max_valid_frame_num = MAX_FRAME_NUM;
		if(cur_sps->max_num_ref_frames > 13)
			s->has_b_frames = 0;
	}

	logi("ref_num: %d, has_b_frame: %d, extra_num: %d",
		cur_sps->max_num_ref_frames, s->has_b_frames, s->extra_frame_num);

	if(cur_sps->chroma_format_idc == 0) {
		s->pix_format = MPP_FMT_YUV400;
	}

	// 1. create frame manager, alloc frame buffer
	struct frame_manager_init_cfg cfg;
	cfg.frame_count = s->frame_info.max_valid_frame_num;
	cfg.height = s->height;
	cfg.width = s->width;
	cfg.height_align = s->height;
	cfg.stride = s->width;
	cfg.pixel_format = s->pix_format;
	cfg.allocator = s->decoder.allocator;
	s->decoder.fm = fm_create(&cfg);

	// 2. create physic buffer for co-located buffer
	field_col_buf_size = cur_sps->pic_mb_height * (2 - cur_sps->frame_mbs_only_flag);
	field_col_buf_size = (field_col_buf_size + 1) / 2;
	field_col_buf_size = cur_sps->pic_mb_width * field_col_buf_size * 32;
	if(cur_sps->direct_8x8_inference_flag == 0)
		field_col_buf_size *= 2;
	field_col_buf_size = (field_col_buf_size + 1023) & (~1023); // 1024 byte align

	total_col_buf_size = field_col_buf_size * 2 * s->frame_info.max_valid_frame_num;
	s->frame_info.col_buf = ve_buffer_alloc(s->ve_buf_handle, total_col_buf_size, 0);
	if(s->frame_info.col_buf == NULL) {
		loge("alloc col_buf failed");
		return -1;
	}

	for(i=0; i<s->frame_info.max_valid_frame_num; i++) {
		s->frame_info.picture[i].top_field_col_addr = s->frame_info.col_buf->phy_addr + i*2*field_col_buf_size;
		s->frame_info.picture[i].bot_field_col_addr = s->frame_info.col_buf->phy_addr + (i*2+1)*field_col_buf_size;
		s->frame_info.picture[i].frame = NULL;
	}

	// 3. create physic buffer used by dblk
	dblk_y_buf_size = (cur_sps->pic_mb_width + 1)*16*4*(2 - cur_sps->frame_mbs_only_flag);
	dblk_c_buf_size = (cur_sps->pic_mb_width + 1)*16*4*(2 - cur_sps->frame_mbs_only_flag);
	s->frame_info.dblk_y_buf = ve_buffer_alloc(s->ve_buf_handle, dblk_y_buf_size, 0);
	s->frame_info.dblk_c_buf = ve_buffer_alloc(s->ve_buf_handle, dblk_c_buf_size, 0);
	if(s->frame_info.dblk_y_buf == NULL || s->frame_info.dblk_c_buf == NULL) {
		loge("alloc dblk buffer failed");
		return -1;
	}

	// 4. create physic buffer for intrap
	intrap_buf_size = cur_sps->pic_mb_width * 16 * 2 * (2 - cur_sps->frame_mbs_only_flag);
	s->frame_info.intrap_buf = ve_buffer_alloc(s->ve_buf_handle, intrap_buf_size, 0);
	if(s->frame_info.intrap_buf == NULL) {
		loge("alloc intrap buffer failed");
		return -1;
	}

	// 5. create mb info buffer
	mb_info_buf_size = 12*1024;
	s->frame_info.mb_info_buf = ve_buffer_alloc(s->ve_buf_handle, mb_info_buf_size, 0);
	if(s->frame_info.mb_info_buf == NULL) {
		loge("alloc mb_info buffer failed");
		return -1;
	}

	// 6. create mb co-located info buffer
	mb_col_info_size = 68*1024;
	s->frame_info.mb_col_info_buf = ve_buffer_alloc(s->ve_buf_handle, mb_col_info_size, 0);
	if(s->frame_info.mb_col_info_buf == NULL) {
		loge("alloc mb_col_info_buf buffer failed");
		return -1;
	}
	return 0;
}

static int decode_scaling_list(struct read_bit_context *gb, uint8_t *factors, int size,
                                const uint8_t *jvt_list,
                                const uint8_t *fallback_list)
{
	int i, last = 8, next = 8;
	const uint8_t *scan = size == 16 ? zigzag_scan : zigzag_scan8x8;
	if (!read_bits(gb, 1)) /* matrix not written, we use the predicted one */
		memcpy(factors, fallback_list, size * sizeof(uint8_t));
	else
		for (i = 0; i < size; i++) {
			if (next) {
			int v = read_se_golomb(gb);
			if (v < -128 || v > 127) {
				loge("delta scale %d is invalid", v);
				return -1;
			}
			next = (last + v) & 0xff;
			}
			if (!i && !next) { /* matrix not written, we use the preset one */
			memcpy(factors, jvt_list, size * sizeof(uint8_t));
			break;
			}
			last = factors[scan[i]] = next ? next : last;
		}
	return 0;
}

/* returns non zero if the provided SPS scaling matrix has been filled */
static int decode_scaling_matrices(struct read_bit_context *gb, struct h264_sps_info *sps,
                                    struct h264_pps_info *pps, int is_sps,
                                    uint8_t(*scaling_matrix4)[16],
                                    uint8_t(*scaling_matrix8)[64])
{
	// 1) if decode scaling matrix in sps, fallback_sps is defalut scaling matrix;
	// 2) if decode scaling matrix in pps, fallback_sps is scaling matrix in pps
	int fallback_sps = !is_sps && sps->scaling_matrix_present_flag;
	const uint8_t *fallback[4] = {
		fallback_sps ? sps->scaling_matrix4[0] : default_scaling4[0],
		fallback_sps ? sps->scaling_matrix4[3] : default_scaling4[1],
		fallback_sps ? sps->scaling_matrix8[0] : default_scaling8[0],
		fallback_sps ? sps->scaling_matrix8[1] : default_scaling8[1]
	};
	int ret = 0;
	if (read_bits(gb, 1)) {
		ret |= decode_scaling_list(gb, scaling_matrix4[0], 16, default_scaling4[0], fallback[0]);        // Intra, Y
		ret |= decode_scaling_list(gb, scaling_matrix4[1], 16, default_scaling4[0], scaling_matrix4[0]); // Intra, Cr
		ret |= decode_scaling_list(gb, scaling_matrix4[2], 16, default_scaling4[0], scaling_matrix4[1]); // Intra, Cb
		ret |= decode_scaling_list(gb, scaling_matrix4[3], 16, default_scaling4[1], fallback[1]);        // Inter, Y
		ret |= decode_scaling_list(gb, scaling_matrix4[4], 16, default_scaling4[1], scaling_matrix4[3]); // Inter, Cr
		ret |= decode_scaling_list(gb, scaling_matrix4[5], 16, default_scaling4[1], scaling_matrix4[4]); // Inter, Cb
		if (is_sps || pps->transform_8x8_mode_flag) {
			ret |= decode_scaling_list(gb, scaling_matrix8[0], 64, default_scaling8[0], fallback[2]); // Intra, Y
			ret |= decode_scaling_list(gb, scaling_matrix8[1], 64, default_scaling8[1], fallback[3]); // Inter, Y
		}
		if (!ret)
			ret = is_sps;
	}

	return ret;
}

static void print_sps_info(struct h264_sps_info *sps)
{
	logd("====== sps info ========");
	logd("profile_idc: %d", sps->profile_idc);
	logd("level_idc: %d", sps->level_idc);
	logd("chroma_format_idc: %d", sps->chroma_format_idc);
	logd("frame_mbs_only_flag: %d", sps->frame_mbs_only_flag);
	logd("direct_8x8_inference_flag: %d", sps->direct_8x8_inference_flag);
	logd("max_num_ref_frames: %d", sps->max_num_ref_frames);
	logd("mb_width: %d, mb_height: %d", sps->pic_mb_width, sps->pic_mb_height);
	logd("poc_type: %d", sps->poc_type);
	logd("scaling_matrix_present_flag: %d", sps->scaling_matrix_present_flag);
	logd("crop: l: %d, r: %d, t: %d, b: %d", sps->frame_cropping_rect_left_offset,
		sps->frame_cropping_rect_right_offset, sps->frame_cropping_rect_top_offset,
		sps->frame_cropping_rect_bottom_offset);
}

int h264_decode_sps(struct h264_dec_ctx *s)
{
	struct h264_sps_info *sps = NULL;
	int ret;
	int i;
	int constraint_set_flags = 0;
	int profile_idc = read_bits(&s->gb, 8);
	constraint_set_flags |= read_bits(&s->gb, 1);
	constraint_set_flags |= read_bits(&s->gb, 1) << 1;
	constraint_set_flags |= read_bits(&s->gb, 1) << 2;
	constraint_set_flags |= read_bits(&s->gb, 1) << 3;
	constraint_set_flags |= read_bits(&s->gb, 1) << 4;
	constraint_set_flags |= read_bits(&s->gb, 1) << 5;
	skip_bits(&s->gb, 2); // reserved
	int level_idc =  read_bits(&s->gb, 8);
	int sps_id = read_ue_golomb(&s->gb);

	if (sps_id > SPS_MAX_NUM - 1) {
		loge("sps_id:%d\n",sps_id);
		return -1;
	}

	// if currenct sps is not exist before, alloc buffer for it
	if(s->sps_buffers[sps_id] == NULL) {
		s->sps_buffers[sps_id] = (struct h264_sps_info*)mpp_alloc(sizeof(struct h264_sps_info));
		if(NULL == s->sps_buffers[sps_id]) {
			loge("malloc sps info fail");
			return -1;
		}
		memset(s->sps_buffers[sps_id], 0, sizeof(struct h264_sps_info));
	}
	sps = s->sps_buffers[sps_id];

	if(profile_idc != BASELINE_PROFILE && profile_idc != MAIN_PROFILE &&
		profile_idc != EXTEND_PROFILE && profile_idc != HIGH_PROFILE) {
		loge("unsupport profile(%d)", profile_idc);
		return -1;
	}
	sps->level_idc = level_idc;
	sps->profile_idc = profile_idc;
	sps->constraint_set_flags = constraint_set_flags;

	memset(sps->scaling_matrix4, 16, sizeof(sps->scaling_matrix4));
	memset(sps->scaling_matrix8, 16, sizeof(sps->scaling_matrix8));
	if(profile_idc == HIGH_PROFILE) {
		s->sps_buffers[sps_id]->chroma_format_idc = read_ue_golomb(&s->gb);
		if(s->sps_buffers[sps_id]->chroma_format_idc > 1) {
			loge("unsupport chroma format(%d)", s->sps_buffers[sps_id]->chroma_format_idc);
			return -1;
		}

		// luma/chroma bit depth
		if(read_ue_golomb(&s->gb) || read_ue_golomb(&s->gb)) {
			loge("unsupport bitdepth");
			return -1;
		}

		read_bits(&s->gb, 1); // transform bypass
		ret = decode_scaling_matrices(&s->gb, sps, NULL, 1, sps->scaling_matrix4, sps->scaling_matrix8);
		if(ret < 0) {
			loge("decode scaling matrix fail");
			return -1;
		}

		sps->scaling_matrix_present_flag |= ret;
	} else {
		s->sps_buffers[sps_id]->chroma_format_idc = 1;
	}

	sps->log2_max_frm_num = read_ue_golomb(&s->gb) + 4;
	sps->poc_type = read_ue_golomb(&s->gb);

	if(sps->poc_type == 0) {
		sps->log2_max_poc_lsb = read_ue_golomb(&s->gb) + 4;
	} else if (sps->poc_type == 1) {
		sps->delta_pic_order_always_zero_flag = read_bits(&s->gb, 1);
		sps->offset_for_non_ref_pic = read_se_golomb(&s->gb);
		sps->offset_for_top_to_bottom_field = read_se_golomb(&s->gb);
		sps->num_ref_frames_in_poc_cycle = read_ue_golomb(&s->gb);
		for(i=0; i<sps->num_ref_frames_in_poc_cycle; i++)
			sps->offset_for_ref_frame[i] = read_se_golomb(&s->gb);
	} else if (sps->poc_type != 2) {
		loge("poc type error (%d)", sps->poc_type);
		return -1;
	}

	sps->max_num_ref_frames = read_ue_golomb(&s->gb);
	sps->gaps_in_frame_num_value_allowed_flag = read_bits(&s->gb, 1);
	if (sps->gaps_in_frame_num_value_allowed_flag) {
		//gaps_in_frame_num_value_allowed_flag
		logw("gaps_in_frame_num_value_allowed_flag ==1, careful");
	}

	sps->pic_mb_width = read_ue_golomb(&s->gb) + 1;
	sps->pic_mb_height = read_ue_golomb(&s->gb) + 1;
	sps->frame_mbs_only_flag = read_bits(&s->gb, 1);
	if(!sps->frame_mbs_only_flag)
		sps->mbaff = read_bits(&s->gb, 1);
	sps->direct_8x8_inference_flag = read_bits(&s->gb, 1);

	if(read_bits(&s->gb, 1)) {
		sps->frame_cropping_rect_left_offset = read_ue_golomb(&s->gb);
		sps->frame_cropping_rect_right_offset = read_ue_golomb(&s->gb);
		sps->frame_cropping_rect_top_offset = read_ue_golomb(&s->gb);
		sps->frame_cropping_rect_bottom_offset = read_ue_golomb(&s->gb);
	} else {
		sps->frame_cropping_rect_left_offset = 0;
		sps->frame_cropping_rect_right_offset = 0;
		sps->frame_cropping_rect_top_offset = 0;
		sps->frame_cropping_rect_bottom_offset = 0;
	}

	// skip vui parameter
	print_sps_info(sps);
	return 0;
}

static int more_rbsp_data_in_pps(struct h264_sps_info *sps)
{
    int profile_idc = sps->profile_idc;

    if ((profile_idc == BASELINE_PROFILE || profile_idc == MAIN_PROFILE ||
         profile_idc == EXTEND_PROFILE) && (sps->constraint_set_flags & 7)) {
        return 0;
    }

    return 1;
}

static void print_pps(struct h264_pps_info *pps)
{
	logd("======== pps info ========");
	logd("PPS: sps_id: %d", pps->sps_id);
	logd("PPS: entropy_coding_mode: %d", pps->entropy_coding_flag);
	logd("PPS: num_ref_idx: %d, %d", pps->num_ref_idx[0], pps->num_ref_idx[1]);
	logd("PPS: weighted_pred_flag: %d", pps->weighted_pred_flag);
	logd("PPS: weighted_bipred_flag: %d", pps->weighted_bipred_flag);
}

int h264_decode_pps(struct h264_dec_ctx *s)
{
	int ret = 0;
	struct h264_pps_info *pps = NULL;
	struct h264_sps_info *sps = NULL;
	logd("show_bits 16: %x", show_bits(&s->gb, 16));
	int pps_id = read_ue_golomb(&s->gb);
	logd("pps_id: %d", pps_id);
	logd("gb index: %d", read_bits_count(&s->gb));

	if (pps_id > PPS_MAX_NUM - 1) {
		loge("pps_id:%d\n",pps_id);
		return -1;
	}

	if(s->pps_buffers[pps_id] == NULL) {
		s->pps_buffers[pps_id] = (struct h264_pps_info*)mpp_alloc(sizeof(struct h264_pps_info));
		if(s->pps_buffers[pps_id] == NULL) {
			loge("malloc pps buffer failed");
			return -1;
		}
		memset(s->pps_buffers[pps_id], 0, sizeof(struct h264_pps_info));
	}
	pps = s->pps_buffers[pps_id];

	pps->sps_id = read_ue_golomb(&s->gb);
	if(pps->sps_id >= SPS_MAX_NUM) {
		loge("sps id error(%d)", pps->sps_id);
		return -1;
	}
	sps = s->sps_buffers[pps->sps_id];
	logd("PPS: sps_id: %d", pps->sps_id);

	pps->entropy_coding_flag = read_bits(&s->gb, 1);
	pps->bottom_field_pic_order_in_frame_flag = read_bits(&s->gb, 1);
	logd("PPS: entropy_coding_mode: %d, count: %d", pps->entropy_coding_flag, read_bits_count(&s->gb));
	logd("PPS: bottom_field_pic_order_in_frame_flag: %d", pps->bottom_field_pic_order_in_frame_flag);
	logd("gb index: %d", read_bits_count(&s->gb));

	int slice_group_num = read_ue_golomb(&s->gb) + 1;
	if(slice_group_num > 1) {
		loge("slice group num(%d) not support", slice_group_num);
		return -1;
	}

	pps->num_ref_idx[0] = read_ue_golomb(&s->gb) + 1;
	pps->num_ref_idx[1] = read_ue_golomb(&s->gb) + 1;

	if(pps->num_ref_idx[0] > 32 || pps->num_ref_idx[1] > 32) {
		loge("num ref idx > 32, error");
		return -1;
	}
	logd("PPS: ref_num: %d %d, count: %d", pps->num_ref_idx[0], pps->num_ref_idx[1], read_bits_count(&s->gb));

	pps->weighted_pred_flag = read_bits(&s->gb, 1);
	pps->weighted_bipred_flag = read_bits(&s->gb, 2);
	pps->pic_init_qp = read_se_golomb(&s->gb) + 26;
	logd("PPS: pic_init_qp: %d, count: %d",pps->pic_init_qp, read_bits_count(&s->gb));
	read_se_golomb(&s->gb); // pic_init_qs_minus26
	logd("PPS:pic_init_qs_minus26, count: %d", read_bits_count(&s->gb));
	pps->chroma_qp_index_offset[1] = pps->chroma_qp_index_offset[0] = read_se_golomb(&s->gb);
	logd("PPS:chroma_qp_index_offset: %d, count: %d", pps->chroma_qp_index_offset[1], read_bits_count(&s->gb));
	pps->deblocking_filter_control_present_flag = read_bits(&s->gb, 1);
	pps->constrained_intra_pred_flag = read_bits(&s->gb, 1);
	pps->redundant_pic_cnt_present_flag = read_bits(&s->gb, 1);

	pps->transform_8x8_mode_flag = 0;
	memcpy(pps->scaling_matrix4, sps->scaling_matrix4, sizeof(pps->scaling_matrix4));
	memcpy(pps->scaling_matrix8, sps->scaling_matrix8, sizeof(pps->scaling_matrix8));

	logd("PPS: redundant_pic_cnt_present_flag: %d, left bits: %d, count: %d",
		pps->redundant_pic_cnt_present_flag, read_bits_left(&s->gb), read_bits_count(&s->gb));

	int left_cnt = read_bits_left(&s->gb);
	int more_bits = 1;
	int left_cnt_7 = left_cnt & 7;
	if (left_cnt <= 8 && (show_bits(&s->gb, left_cnt) == (1<<(left_cnt-1))))
		more_bits = 0;
	if (left_cnt > 16 && (show_bits(&s->gb, left_cnt_7+16) == (1<<(left_cnt_7+15)) )) {
		// if pps and slice data in the same packet,
		// judge the end of PPS with the start code.
		more_bits = 0;
	}
	if (more_bits && more_rbsp_data_in_pps(sps)) {
		pps->transform_8x8_mode_flag = read_bits(&s->gb, 1);
		logd("PPS: pic_scaling_matrix_present_flag: %d, left bits: %d", show_bits(&s->gb, 1), read_bits_left(&s->gb));
		ret = decode_scaling_matrices(&s->gb, sps, pps, 0,
							pps->scaling_matrix4, pps->scaling_matrix8);
		if(ret < 0) {
			loge("decoding scaling matrix in pps fail");
			return -1;
		}

		pps->chroma_qp_index_offset[1] = read_se_golomb(&s->gb);
		if(pps->chroma_qp_index_offset[1] < -12 || pps->chroma_qp_index_offset[1] > 12) {
			loge("pps->chroma_qp_index_offset[1](%d), out of range", pps->chroma_qp_index_offset[1]);
			return -1;
		}
	}

	print_pps(pps);
	return 0;
}

// decode poc, see spec 8.2.1 Decoding process for picture order count
static int h264_init_poc(struct h264_dec_ctx *s)
{
	int i;
	int field_poc[2] = {0};
	struct h264_sps_info* cur_sps = s->sps_buffers[s->active_sps_id];
	struct h264_slice_header* sh = &s->sh;
	int max_frame_num = 1 << cur_sps->log2_max_frm_num;

	/* Shorten frame num gaps so we don't have to allocate reference
	* frames just to throw them away */
	if (s->frame_num != s->prev_frame_num) {
		int unwrap_prev_frame_num = s->prev_frame_num;

		if (unwrap_prev_frame_num > s->frame_num)
			unwrap_prev_frame_num -= max_frame_num;

		if ((s->frame_num - unwrap_prev_frame_num) > cur_sps->max_num_ref_frames) {
			unwrap_prev_frame_num = (s->frame_num - cur_sps->max_num_ref_frames) - 1;
			if (unwrap_prev_frame_num < 0)
				unwrap_prev_frame_num += max_frame_num;

			s->prev_frame_num = unwrap_prev_frame_num;
		}
	}

	// gaps in frame_num
	while (s->frame_num != s->prev_frame_num && !s->first_field &&
		s->frame_num != (s->prev_frame_num+1) % max_frame_num) {
		logw("frame gaps, frame_num: %d, prev_frame_num: %d", s->frame_num, s->prev_frame_num);
		if (!cur_sps->gaps_in_frame_num_value_allowed_flag) {
			// reset next_output_poc here, fix bug(PMS: 1304)
			// it is not reset in ffmpeg, maybe error???
			s->next_output_poc = INT_MIN;

			for (i=0; i<MAX_DELAYED_PIC_COUNT; i++)
				s->frame_info.last_pocs[i] = INT_MIN;
		}

		s->prev_frame_num++;
		s->prev_frame_num %= max_frame_num;
		s->frame_info.cur_pic_ptr->frame_num = s->prev_frame_num;
	}

	if(s->idr_pic_flag == 1)
		s->frame_num_offset = 0;
	else if(sh->frame_num < s->prev_frame_num)
		s->frame_num_offset = s->prev_frame_num_offset + max_frame_num;
	else
		s->frame_num_offset = s->prev_frame_num_offset;

	logd("s->frame_num_offset: %d, cur_sps->poc_type: %d", s->frame_num_offset, cur_sps->poc_type);
	if(cur_sps->poc_type == 0) {
		int max_poc_lsb = 1 << cur_sps->log2_max_poc_lsb;

		if(s->idr_pic_flag) {
			s->prev_poc_lsb = s->prev_poc_msb = 0;
		}

		// 1. calc poc_msb
		if (sh->poc_lsb < s->prev_poc_lsb &&
			s->prev_poc_lsb - sh->poc_lsb >= max_poc_lsb / 2)
			sh->poc_msb = s->prev_poc_msb + max_poc_lsb;
		else if (sh->poc_lsb > s->prev_poc_lsb &&
			s->prev_poc_lsb - sh->poc_lsb < -max_poc_lsb / 2)
			sh->poc_msb = s->prev_poc_msb - max_poc_lsb;
		else
			sh->poc_msb = s->prev_poc_msb;

		// 2. calc top_field/bottom field poc
		field_poc[0] = field_poc[1] = sh->poc_msb + sh->poc_lsb;
		if (s->picture_structure == PICT_FRAME)
			field_poc[1] += sh->delta_poc_bottom;
	} else if(cur_sps->poc_type == 1) {
		int abs_frame_num = 0;
		int64_t expected_delta_per_poc_cycle, expectedpoc;
		int i;

		// 1. calc abs_frame_num
		if(cur_sps->num_ref_frames_in_poc_cycle != 0) {
			abs_frame_num = s->frame_num_offset + sh->frame_num;
		}
		if(s->nal_ref_idc == 0 && abs_frame_num > 0) {
			abs_frame_num --;
		}

		// 2. calc expectedpoc
		expected_delta_per_poc_cycle = 0;
		for(i=0; i<cur_sps->num_ref_frames_in_poc_cycle; i++) {
			expected_delta_per_poc_cycle += cur_sps->offset_for_ref_frame[i];
		}
		if(abs_frame_num > 0) {
			int poc_cycle_cnt          = (abs_frame_num - 1) / cur_sps->num_ref_frames_in_poc_cycle;
			int frame_num_in_poc_cycle = (abs_frame_num - 1) % cur_sps->num_ref_frames_in_poc_cycle;
			expectedpoc = poc_cycle_cnt * expected_delta_per_poc_cycle;
			for (i = 0; i <= frame_num_in_poc_cycle; i++)
				expectedpoc = expectedpoc + cur_sps->offset_for_ref_frame[i];
		} else {
			expectedpoc = 0;
		}

		if(s->nal_ref_idc == 0)
			expectedpoc = expectedpoc + cur_sps->offset_for_non_ref_pic;

		// 3. top field/bottom field poc
		field_poc[0] = expectedpoc + sh->delta_poc[0];
		field_poc[1] = field_poc[0] + cur_sps->offset_for_top_to_bottom_field;
		if(s->picture_structure == PICT_FRAME)
			field_poc[1] += sh->delta_poc[1];
	} else {
		int poc = 2 * (s->frame_num_offset + sh->frame_num);

		if (!s->nal_ref_idc)
			poc--;

		field_poc[0] = poc;
		field_poc[1] = poc;
	}

	if(s->picture_structure != PICT_BOTTOM_FIELD) {
		s->poc_delta = (field_poc[0] & 1) ? 1 : 2;

		s->frame_info.cur_pic_ptr->field_poc[0] = field_poc[0];
		s->frame_info.cur_pic_ptr->poc = field_poc[0];
		s->frame_info.cur_pic_ptr->mbaff_frame = cur_sps->mbaff && (s->picture_structure == PICT_FRAME);
	}

	if(s->picture_structure != PICT_TOP_FIELD) {
		s->frame_info.cur_pic_ptr->field_poc[1] = field_poc[1];
		s->frame_info.cur_pic_ptr->poc = field_poc[1];
	}

	if(s->picture_structure == PICT_FRAME) {
		s->frame_info.cur_pic_ptr->poc = MIN(s->frame_info.cur_pic_ptr->field_poc[0],
				s->frame_info.cur_pic_ptr->field_poc[1]);
	}

	logd("cur pic poc: %d field poc: %d, %d", s->frame_info.cur_pic_ptr->poc,
		s->frame_info.cur_pic_ptr->field_poc[0], s->frame_info.cur_pic_ptr->field_poc[1]);

	return 0;
}

static void print_ref_list(struct h264_dec_ctx *s)
{
	int list = 0;
	int i = 0;
	struct h264_picture* pic = NULL;

	for(list=0; list<s->sh.list_count; list++) {
		logd("==== ref list%d, count: %d ====", list, s->sh.num_ref_idx[list]);
		for(i=0; i<s->sh.num_ref_idx[list]; i++) {
			pic = s->frame_info.ref_list[list][i].parent;
			if(pic) {
				logd("list: %d, i: %d, buf_idx: %d, structure: %d, poc: %d", list, i,
			  	pic->buf_idx, s->frame_info.ref_list[list][i].refrence,
			  	s->frame_info.ref_list[list][i].poc);
			}
		}
	}
}

/*
 * if the reference picture is error, current picture will decode error too.
 */
static void set_error_from_ref_list(struct h264_dec_ctx *s)
{
	int list = 0;
	int i = 0;
	struct h264_picture* pic = NULL;
	struct h264_picture* cur_pic = s->frame_info.cur_pic_ptr;

	for (list=0; list<s->sh.list_count; list++) {
		for (i=0; i<s->sh.num_ref_idx[list]; i++) {
			pic = s->frame_info.ref_list[list][i].parent;
			if (pic && pic->frame->mpp_frame.flags & FRAME_FLAG_ERROR) {
				cur_pic->frame->mpp_frame.flags |= FRAME_FLAG_ERROR;
				loge("ref error, so cur error");
				break;
			}
		}
	}
}

static void set_frame_info(struct h264_dec_ctx *s)
{
	struct h264_sps_info* cur_sps = s->sps_buffers[s->active_sps_id];
	struct frame* f = s->frame_info.cur_pic_ptr->frame;
	int sub = (cur_sps->chroma_format_idc == 1) ? 1 : 0;
	int crop_unit_y = (2 - cur_sps->frame_mbs_only_flag) << sub;
	int crop_unit_x = 1 << sub;

	f->mpp_frame.pts = s->curr_packet->pts;
	f->mpp_frame.buf.crop_en = 1;
	f->mpp_frame.buf.crop.x	= crop_unit_x * cur_sps->frame_cropping_rect_left_offset;
	f->mpp_frame.buf.crop.y	= crop_unit_y * cur_sps->frame_cropping_rect_top_offset;
	f->mpp_frame.buf.crop.height	= s->height - crop_unit_y * cur_sps->frame_cropping_rect_bottom_offset
		- crop_unit_y * cur_sps->frame_cropping_rect_top_offset;
	f->mpp_frame.buf.crop.width	= s->width - crop_unit_x * cur_sps->frame_cropping_rect_left_offset
		- crop_unit_x * cur_sps->frame_cropping_rect_right_offset;
}

// check whether last frame is error
// 1. frame structure: the number of last decoded mb less than total mbs,
//     and first_mb_in_slice is not 0.
// 2. field structure: TODO
static int check_last_frame(struct h264_dec_ctx *s)
{
	struct h264_picture* cur_pic = s->frame_info.cur_pic_ptr;
	struct h264_slice_header* sh = &s->sh;

	if (cur_pic == NULL) {
		return 0;
	}

	if (s->picture_structure == PICT_FRAME) {
		if (s->decode_mb_num < s->mbs_in_pic && !sh->first_mb_in_slice) {
			cur_pic->frame->mpp_frame.flags |= FRAME_FLAG_ERROR;
			loge("last picture error, decoded_mb: %d, total_mbs: %d",
				s->decode_mb_num, s->mbs_in_pic);
			if (s->nal_ref_idc_pre) {
				execute_ref_pic_marking(s);
			}
			select_output_frame(s);
			s->frame_info.cur_pic_ptr = NULL;
		}
	}

	return 0;
}
/*
1) parse slice header syntax
2) alloc physic memory for frame buffer
3) get one frame
4) init reference list and reorder

return -1, if syntax parse error before get frame
return DEC_NO_EMPTY_FRAME, if no empty frame for decoding
*/
int h264_decode_slice_header(struct h264_dec_ctx *s)
{
	int width, height;
	int last_pic_structure;
	int last_first_field = s->first_field;
	struct h264_sps_info* cur_sps = NULL;
	struct h264_pps_info* cur_pps = NULL;
	struct h264_slice_header* sh = &s->sh;

	last_pic_structure = s->picture_structure;
	sh->first_mb_in_slice = read_ue_golomb(&s->gb);
	logi("sh->first_mb_in_slice: %d", sh->first_mb_in_slice);
	if(sh->first_mb_in_slice == 0) {
		s->cur_slice_num = 0;
	} else if(s->cur_slice_num == 0) {
		s->error = H264_DECODER_ERROR_SLICENUM;
		loge("first_mb_in_slice:%d,cur_slice_num:%d\n", sh->first_mb_in_slice,s->cur_slice_num);
		return -1;
	}

	check_last_frame(s);

	sh->slice_type = read_ue_golomb(&s->gb);
	if(sh->slice_type > 9) {
		loge("slice type(%d) error", sh->slice_type);
		s->error = H264_DECODER_ERROR_SLICETYPE;
		return -1;
	}
	sh->slice_type = (sh->slice_type > 4) ? sh->slice_type-5 : sh->slice_type;

	sh->pps_id = read_ue_golomb(&s->gb);

	if(sh->pps_id > 255) {
		s->error = H264_DECODER_ERROR_PPS;
		loge("slice header pps id(%d) error", sh->pps_id);
		return -1;
	}

	if(s->pps_buffers[sh->pps_id] == NULL) {
		s->error = H264_DECODER_ERROR_PPS;
		loge("slice header pps id:%d,pps_buffers==NULL", sh->pps_id);
		return -1;
	}

	//logi("SH: slice_type: %d, pps_id: %d, bit offset: %d", sh->slice_type, sh->pps_id, read_bits_count(&s->gb));

	s->active_pps_id = sh->pps_id;
	s->active_sps_id = s->pps_buffers[s->active_pps_id]->sps_id;
	cur_sps = s->sps_buffers[s->active_sps_id];
	cur_pps = s->pps_buffers[s->active_pps_id];

	width = cur_sps->pic_mb_width * 16;
	height = cur_sps->pic_mb_height * (2-cur_sps->frame_mbs_only_flag) * 16;
	if((s->width && s->width != width) || (s->height && s->height != height)) {
		logw("resolution change");
	}

	s->width = width;
	s->height = height;

	// 1. alloc frame buffer if it is the first time
	if(s->frame_info.max_valid_frame_num == 0)
		h264_alloc_frame_buffer(s);

	sh->frame_num = read_bits(&s->gb, cur_sps->log2_max_frm_num);
	s->frame_num = sh->frame_num;

	sh->mbaff_frame = 0;
	sh->field_pic_flag = 0;
	sh->bottom_field_flag = 0;
	if(cur_sps->frame_mbs_only_flag) {
		s->picture_structure = PICT_FRAME;
	} else {
		sh->field_pic_flag = read_bits(&s->gb, 1);
		if(sh->field_pic_flag == 1) {
			sh->bottom_field_flag = read_bits(&s->gb, 1);
			s->picture_structure = PICT_TOP_FIELD + sh->bottom_field_flag;
		} else {
			// MBAFF
			s->picture_structure = PICT_FRAME;
			sh->mbaff_frame = cur_sps->mbaff;
		}
	}

	if(sh->first_mb_in_slice != 0) {
		// the picture structrue must be same in difference slices of a picture
		// warning here, maybe sytax error
		if(s->picture_structure != last_pic_structure) {
			logw("picture struct different: %d %d", s->picture_structure, last_pic_structure);
			s->picture_structure = last_pic_structure;
		}
	}

	s->mbs_in_pic = cur_sps->pic_mb_width *  cur_sps->pic_mb_height * (2-cur_sps->frame_mbs_only_flag);
	if(s->picture_structure != PICT_FRAME)
		s->mbs_in_pic >>= 1;

	logi("s->picture_structure: %d", s->picture_structure);
	// 2. get an empty frame for decode output
	if(s->cur_slice_num == 0) {
		// set first_field flag
		if(s->picture_structure == PICT_FRAME) {
			s->first_field = 0;
		} else if(s->first_field == 1) {
			// last field not match
			if(s->picture_structure == PICT_FRAME || s->picture_structure == last_pic_structure) {
				s->first_field = 0;
				s->error = H264_DECODER_ERROR_PICTURESTRUCTUTE;
				loge("picture struct error");
				return -1;
			} else {
				s->first_field = 0;
			}
		} else {
			s->first_field = (s->picture_structure != PICT_FRAME);
		}

		struct frame* f = NULL;

		// get frame
		if(sh->first_mb_in_slice == 0 && (s->picture_structure == PICT_FRAME || s->first_field == 1)) {
			logi("empty number: %d", fm_get_empty_frame_num(s->decoder.fm));

			f = fm_decoder_get_frame(s->decoder.fm);
			if(f == NULL) {
				s->picture_structure = last_pic_structure;
				s->first_field = last_first_field;
				//pm_reclaim_ready_packet(s->decoder.pm, s->curr_packet);
				s->error = H264_DECODER_ERROR_NOEMPTYFRAME;
				return DEC_NO_EMPTY_FRAME;
			}
			// top_field_col_addr & top_field_col_addr are fixed addr.
			// s->frame_info.picture[f->mpp_frame.id].top_field_col_addr = 0;
			// s->frame_info.picture[f->mpp_frame.id].bot_field_col_addr = 0;
			s->frame_info.picture[f->mpp_frame.id].mbaff_frame = 0;
			s->frame_info.picture[f->mpp_frame.id].nal_ref_idc[0] = 0;
			s->frame_info.picture[f->mpp_frame.id].nal_ref_idc[1] = 0;
			s->frame_info.picture[f->mpp_frame.id].picture_structure = 0;
			s->frame_info.picture[f->mpp_frame.id].displayed_flag = 0;
			s->frame_info.picture[f->mpp_frame.id].field_poc[0] = 0;
			s->frame_info.picture[f->mpp_frame.id].field_poc[1] = 0;
			s->frame_info.picture[f->mpp_frame.id].poc = 0;
			s->frame_info.picture[f->mpp_frame.id].frame_num = 0;
			s->frame_info.picture[f->mpp_frame.id].pic_id = 0;
			s->frame_info.picture[f->mpp_frame.id].long_ref = 0;
			s->frame_info.picture[f->mpp_frame.id].ref_count[0] = 0;
			s->frame_info.picture[f->mpp_frame.id].ref_count[1] = 0;
			f->mpp_frame.flags = 0;
			s->frame_info.picture[f->mpp_frame.id].frame = f;
			s->frame_info.picture[f->mpp_frame.id].refrence = 0;
			s->frame_info.picture[f->mpp_frame.id].buf_idx = f->mpp_frame.id;
			s->frame_info.picture[f->mpp_frame.id].mmco_reset = 0;
			s->frame_info.picture[f->mpp_frame.id].key_frame = s->idr_pic_flag;
			s->frame_info.cur_pic_ptr = &s->frame_info.picture[f->mpp_frame.id];

			set_frame_info(s);
		}

		int cur_buf_id = s->frame_info.cur_pic_ptr->buf_idx;
		if(s->picture_structure == PICT_FRAME) {
			s->frame_info.picture[cur_buf_id].nal_ref_idc[0] = s->nal_ref_idc;
			s->frame_info.picture[cur_buf_id].nal_ref_idc[1] = s->nal_ref_idc;
		} else if(s->picture_structure == PICT_TOP_FIELD) {
			s->frame_info.picture[cur_buf_id].nal_ref_idc[0] = s->nal_ref_idc;
		} else { // bottom field
			s->frame_info.picture[cur_buf_id].nal_ref_idc[1] = s->nal_ref_idc;
		}
	}

	s->frame_info.cur_pic_ptr->frame_num = sh->frame_num;
	if(s->picture_structure == PICT_FRAME) {
		sh->cur_pic_num = sh->frame_num;
		sh->max_pic_num = 1 << cur_sps->log2_max_frm_num;
	} else {
		sh->cur_pic_num = 2*sh->frame_num + 1;
		sh->max_pic_num = 1 << (cur_sps->log2_max_frm_num + 1);
	}

	if(s->idr_pic_flag == 1) {
		sh->idr_pic_id = read_ue_golomb(&s->gb);
	}

	if(cur_sps->poc_type == 0) {
		sh->poc_lsb = read_bits(&s->gb, cur_sps->log2_max_poc_lsb);
		if(cur_pps->bottom_field_pic_order_in_frame_flag && s->picture_structure == PICT_FRAME) {
			sh->delta_poc_bottom = read_se_golomb(&s->gb);
		}
		logi("SH: poc_lsb: %d, offset: %d", sh->poc_lsb, read_bits_count(&s->gb));
	}

	if(cur_sps->poc_type == 1 && !cur_sps->delta_pic_order_always_zero_flag) {
		sh->delta_poc[0] = read_se_golomb(&s->gb);
		if(cur_pps->bottom_field_pic_order_in_frame_flag && s->picture_structure == PICT_FRAME) {
			sh->delta_poc[1] = read_se_golomb(&s->gb);
		}
	}

	// 3. calc the poc of current frame
	if(!sh->first_mb_in_slice) {
		h264_init_poc(s);
	}

	if(cur_pps->redundant_pic_cnt_present_flag) {
		read_ue_golomb(&s->gb); // redundant_pic_cnt
	}

	sh->num_ref_idx[0] = cur_pps->num_ref_idx[0];
	sh->num_ref_idx[1] = cur_pps->num_ref_idx[1];
	sh->direct_spatial_mv_pred = 0;
	sh->num_ref_idx_active_override = 0;
	sh->list_count = 0;
	if(sh->slice_type == H264_SLICE_B)
		sh->direct_spatial_mv_pred = read_bits(&s->gb, 1);

	if(sh->slice_type == H264_SLICE_B || sh->slice_type == H264_SLICE_P || sh->slice_type == H264_SLICE_SP) {
		sh->num_ref_idx_active_override = read_bits(&s->gb, 1);
		logi("SH: num_ref_idx_active_override: %d", sh->num_ref_idx_active_override);
		if(sh->num_ref_idx_active_override) {
			sh->num_ref_idx[0] = read_ue_golomb(&s->gb) + 1;
			logi("SH: num_ref_idx_l0: %d", sh->num_ref_idx[0]);
			if(sh->slice_type == H264_SLICE_B) {
				sh->num_ref_idx[1] = read_ue_golomb(&s->gb) + 1;
			}
		}
		sh->list_count = (sh->slice_type == H264_SLICE_B) ? 2: 1;
	}

	// init ref list
	if(sh->slice_type != H264_SLICE_I && sh->slice_type != H264_SLICE_SI) {
		init_ref_list(s);

		// reorder
		ref_pic_list_reordering(s);
		set_error_from_ref_list(s);

		print_ref_list(s);
	}

	// parse pred weight table
	if((cur_pps->weighted_pred_flag && (sh->slice_type == H264_SLICE_P || sh->slice_type == H264_SLICE_SP))
		|| (cur_pps->weighted_bipred_flag == 1 && sh->slice_type == H264_SLICE_B)) {
		pred_weight_table(s);
	}

	// parse dec_ref_pic_marking
	if(s->nal_ref_idc) {
		if(dec_ref_pic_marking(s) < 0) {
			loge("dec_ref_pic_marking error");
			s->error = H264_DECODER_ERROR_REFPICMARKING;
		}
	}

	if(sh->slice_type != H264_SLICE_I && sh->slice_type != H264_SLICE_SI && cur_pps->entropy_coding_flag) {
		sh->cabac_init_idc = read_ue_golomb(&s->gb);
		logd("sh->cabac_init_idc: %d, offset: %d", sh->cabac_init_idc, read_bits_count(&s->gb));
	}

	int qp_delta = read_se_golomb(&s->gb);
	logd("qp_delta: %d, offset: %d", qp_delta, read_bits_count(&s->gb));
	int tmp = cur_pps->pic_init_qp + qp_delta;
	if(tmp > 51) {
		logw("qp(%d) large than 51", tmp);
		tmp = 30;
	}
	sh->qp_y = tmp;
	logd("SH: slice_qp_delta: %d, sh->slice_type: %d", qp_delta, sh->slice_type);

	if(sh->slice_type == H264_SLICE_SP)
		read_bits(&s->gb, 1);
	if(sh->slice_type == H264_SLICE_SI || sh->slice_type == H264_SLICE_SP)
		read_se_golomb(&s->gb);

	if(cur_pps->deblocking_filter_control_present_flag) {
		sh->deblocking_filter = read_ue_golomb(&s->gb);
		if(sh->deblocking_filter > 2) {
			s->error = H264_DECODER_ERROR_DEBLOCKINGFILTER;
			loge("deblocking_filter(%d) out of range", sh->deblocking_filter);
		}

		if(sh->deblocking_filter != 1) {
			sh->slice_alpha_c0_offset_div2 = read_se_golomb(&s->gb);
			sh->slice_beta_offset_div2     = read_se_golomb(&s->gb);
		}
	} else {
		sh->deblocking_filter = 0;
		sh->slice_alpha_c0_offset_div2 = 0;
		sh->slice_beta_offset_div2 = 0;
	}

	// we are not support slice group, so no need to parse slice_group_change_cycle here
	s->bit_offset = read_bits_count(&s->gb);
	if(cur_pps->entropy_coding_flag)
		s->bit_offset = (s->bit_offset + 7) & 0x7ffffff8;

	logd("s->first_eptb_offset: %d s->bit_offset: %d", s->first_eptb_offset, s->bit_offset);
	// if the first_eptb_offset < bit_offset, the eptb is in slice header;
	// or the eptb is in slice data, we cannot remove it
	if(s->first_eptb_offset*8 <= s->bit_offset)
		s->bit_offset += (s->sc_byte_offset + s->remove_bytes)*8;
	else
		s->bit_offset += s->sc_byte_offset*8;
	s->bit_offset += s->slice_offset*8;
	s->frame_info.cur_pic_ptr->picture_structure = s->picture_structure;

	s->cur_slice_num ++;
	return 0;
}
