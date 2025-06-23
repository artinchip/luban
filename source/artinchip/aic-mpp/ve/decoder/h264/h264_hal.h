/*
 * Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <qi.xu@artinchip.com>
 *  Desc:  avc register
 */

#ifndef __H264_HAL_H__
#define __H264_HAL_H__

#define AVC_RESET_REG    0x100

struct reg_avc_sps
{
	unsigned pic_height_in_map_units_minus1 : 7; //[6:0]
	unsigned r0 : 1;
	unsigned pic_width_in_mbs_minus1 : 7;// [14:8]
	unsigned r1 : 1;
	unsigned direct_8x8_inference_flag : 1;
	unsigned mb_adaptive_frame_filed_flag : 1;
	unsigned frame_mbs_only_flag : 1;
	unsigned uv_interleave : 1;
	unsigned uv_alter : 1;
	unsigned chroma_format_idc : 2; //[22:21]:
	unsigned r : 8;
	unsigned pic_init : 1;
};
#define AVC_SPS_REG    0x104

struct reg_avc_pps
{
	unsigned transform_8x8_mode_flag : 1;
	unsigned constrained_intra_pred_flag : 1;
	unsigned weighted_bipred_idc : 2;
	unsigned weighted_pred_flag : 1;
	unsigned entropy_coding_mode_flag : 1;
	unsigned r0 : 2;
	unsigned num_ref_idx_l1_active_minus1_pic : 5;
	unsigned r1 : 3;
	unsigned num_ref_idx_l0_active_minus1_pic : 5;
	unsigned r2 : 11;
};
#define AVC_PPS_REG    0x108

struct reg_avc_sh1
{
	unsigned cabac_init_idc : 2; // [1:0]
	unsigned direct_spatial_mv_pred_flag : 1;
	unsigned bottom_field_flag : 1;
	unsigned field_pic_flag : 1;
	unsigned first_slice_in_pic : 1;
	unsigned r0 : 2;
	unsigned slice_type : 4;
	unsigned nal_ref_flag : 1;
	unsigned r1 : 3;

	unsigned first_mb_y : 7;
	unsigned r2 : 1;
	unsigned first_mb_x : 7;
	unsigned r3 : 1;
};
#define AVC_SHS1_REG    0x10C

struct reg_avc_sh2
{
	unsigned slice_beta_offset_div2 : 4;
	unsigned slice_alpha_offset_div2 : 4;
	unsigned disable_deblocking_filter_idc : 2;
	unsigned r0 : 2;
	unsigned num_ref_idx_active_override_flag : 1;
	unsigned r1 : 3;
	unsigned num_ref_idx_l1_active_minus1 : 5;
	unsigned r2 : 3;
	unsigned num_ref_idx_l0_active_minus1 : 5;
	unsigned r3 : 3;
};
#define AVC_SHS2_REG    0x110

struct reg_avc_shs_wp
{
	unsigned r : 24;
	unsigned chroma_log2_weight_denom : 3;
	unsigned r0 : 1;
	unsigned luma_log2_weight_denom : 3;
	unsigned r1 : 1;
};
#define AVC_SHS_WP_REG    0x114

struct reg_avc_weight_pred
{
	unsigned offset : 8;  // [7:0]:offset
	unsigned weight : 9;  // [16:8]:weight
	unsigned r : 7;
	/*
		0x00-0x1F is Luma_L0;
		0x20-0x3F is Chroma_Cb_L0;
		0x40-0x5F is Chroma_Cr_L0;
		0x60-0x7F is Luma_L1;
		0x80-0x9F is Chroma_Cb_L1;
		0xA0-0xBF is Chroma_Cr_L1
	*/
	unsigned weight_pred_addr : 8;
};
#define AVC_WEIGHT_PRED_REGISTER    0x118

struct reg_avc_scaling_matrix
{
	unsigned scaling_matrix_data : 9;  // [8:0],
	unsigned r0 : 7;
	/*
	000: S1_4x4_intra_Y
	001: S1_4x4_intra_Cb
	010: S1_4x4_intra_Cr
	011: S1_4x4_inter_Y
	100: S1_4x4_inter_Cb
	101: S1_4x4_inter_Cr
	110: S1_8x8_intra_Y
	111: S1_8x8_inter_Y
	*/
	unsigned matrix_addr : 8;   // [23:16]
	unsigned r1 : 6;
	unsigned write_enable : 1; // [30]
	unsigned matrix_access : 1; // [31]
};
#define AVC_SCALING_MATRIX_REGISTER    0x11C

struct reg_avc_shs_qp
{
	unsigned slice_qpy : 6;				// [5:0]: Slice header qp_y, value range 0~51
	unsigned r0 : 2;
	unsigned chroma_qp_idx_offset : 6;		// [13:8]: Cb qp offset, value range -12~12
	unsigned r1 : 2;
	unsigned second_chroma_qp_idx_offset : 6;	// [21:16]: Cr qp offset, value range -12~12
	unsigned r2 : 10;
};
#define AVC_SHS_QP_REG    0x120

struct reg_avc_ctrl
{
	unsigned r0 : 3;
	unsigned dec_finish_int_enable : 1;	// [3]: finish irq enable
	unsigned dec_error_int_enable : 1;	// [4]: error irq enable
	unsigned bit_request_int_enable : 1;	// [5]: bit request irq enable
	unsigned r1 : 7;
	unsigned eptb_detection : 1;		// [13], detect emulation_prevention_three_byte (0x03) enable
	unsigned stcd_detect_en : 1;		// [14], detect startcode enable
	unsigned r2 : 16;
	unsigned slice_start : 1;		// [31], slice decode start
};
#define AVC_CTRL_REG    0x124

struct reg_ve_status
{
	unsigned busy_status : 9;	// [0]: (RO) internal module status
	unsigned r0 : 7;

	unsigned ve_finish : 1;		// [16]: finish status, write 1 clear irq
	unsigned bit_request : 1;	// [17]: bitrequest status, write 1 clear irq
	unsigned ve_error : 1;		// [18]: error status, write 1 clear irq

	unsigned r1 : 9;
	unsigned error_case : 4;	// [28]: mb_prefix dec err
					// [29]: mb header dec err
					// [30]: blk coeff dec err
					// [31]: bit stream err
};
#define VE_STATUS_REG    0x128

#define CORRECT_DECODE_MB_NUMBER_REG    0x12C

struct reg_ve_bit_buffer_valid
{
	unsigned data_first : 1;     // [0]: first part of a picture data
	unsigned data_last : 1;      // [1]: last part
	unsigned r : 29;
	unsigned data_valid : 1;     // [31]: valid data
};
#define BIT_BUFFER_DATA_VALID_REG    0x130

#define BIT_BUFFER_START_ADDR_REG    0x134

#define BIT_BUFFER_END_ADDR_REG    0x138

#define BIT_BUFFER_BIT_OFFSET_REG    0x13C

#define BIT_BUFFER_BIT_LEN_REG    0x140

struct reg_avc_ref_list
{
	unsigned field_sel_list0 : 1;  // [0]: 0:top field, 1:bottom field
	unsigned buf_idx_list0 : 5;    // [5:1]: frame idx of forward reference
	unsigned r0 : 2;
	unsigned field_sel_list1 : 1;  // [8]: 0:top field, 1:bottom field
	unsigned buf_idx_list1 : 5;    // [13:9]: frame idx of backward reference
	unsigned r1 : 2;
	unsigned ref_idx : 5;          // [20:16]: ref idx of refrence list, val range 0-31.
	unsigned r2 : 10;
	unsigned ref_idx_rw : 1;       // [31]: 0:read from ve; 1: write to ve
};
#define AVC_REF_LIST_REGISTER    0x144


struct frame_struct_ref_info
{
	// top field refrence type: 00 (short-term); 01 (long-term); 10 (non-refrence); 11(reserve)
	unsigned top_ref_type : 2;
	unsigned r0 : 2;
	// bottom field refrence type: 00 (short-term); 01 (long-term); 10 (non-refrence); 11(reserve)
	unsigned bot_ref_type : 2;
	unsigned r1 : 2;
	// picture type: 00 (frame); 01(field); 10(mbaff)
	unsigned frm_struct : 2;
	unsigned r2 : 22;
};

struct reg_avc_buf_info
{
	/*
	select content of frame buffer
	000：top poc
	001：bottom poc
	010：pic info, Frame_Struct_Ref_Info
	101：top field/frame mv collocated info start address in DRAM
	110：bottpm field/frame mv collocated info start address in DRAM
	*/
	unsigned curr_frame_idx : 5; // [4:0]
	unsigned r0 : 3;
	unsigned buf_idx : 5;  // [12:8]
	unsigned r1 : 3;
	unsigned content_select : 3;  // [18:16]
	unsigned r2 : 12;
	unsigned buf_info_rw : 1;     // [31]:
};
#define AVC_BUF_INFO_REGISTER	0x148
#define AVC_BUF_INFO_CONTENT_REGISTER	0x14C

// co-located info used for direct pred in B-skip, need memory size(17 * 4K byte)
#define MB_COL_BUF_ADDR_REG	0x150

#define MBINFO_BUF_ADDR_REG	0x154

//* store the last line data of last mb line for intra-pred
#define MB_INTRAP_ADDR_REG	0x1D4

#define CYCLES_REG		0x1F8

/***************************************************************************************************/
/***************************** DBLK reg **********************************************************/

struct reg_pic_type
{
	unsigned bottom_field_flag : 1; // 0: top field, 1: bottom field;
	unsigned field : 1;		// 0: frame picture, 1: field
	unsigned mbaff : 1;		// mbaffFrameFlag
	unsigned r1 : 29;
};
#define DBLK_PIC_TYPE_REG	0x210

struct reg_pic_size
{
	unsigned pic_ysize : 12;	// [11:0]
	unsigned r0 : 4;
	unsigned pic_xsize : 12;	// [27:16]
	unsigned r1 : 4;
};
#define DBLK_PIC_SIZE_REG	0x214

struct reg_dblk_type
{
	unsigned dblk_enable : 1;	//
	unsigned r1 : 31;
};
#define DBLK_EN_REG		0x240
#define DBLK_BUF_Y_REG		0x244
#define DBLK_BUF_C_REG		0x248

struct reg_dec_config
{
	unsigned r0 : 2;
	unsigned uv_interleave : 1; //
	unsigned uv_alternative : 1; // 0:cbcr 1:crcb
	unsigned dec_chroma_idc : 2; // 00-420; 01-400
	unsigned dec_luma_only : 1;
	unsigned r1 : 1;
	unsigned dec_wr_en : 1; // write 1
	unsigned r : 23;
};

#define DEC_CONFIG_REG		0x24C
#define DEC_FRAME_IDX_REG	0x250

/********************************** MC *************************************************************/

#define MC_DMA_MODE_REG		0x448

struct mc_wp_en
{
	unsigned implicited_en: 1; // B slice && weighted_bipred_idc == 2, set 1
	unsigned weighted_en : 1;  // weighted pred enable
	unsigned r : 30;
};
#define MC_WP_EN_REG		0x480

struct mc_wp_logwd
{
	unsigned logwd_y : 3;
	unsigned logwd_c : 3;
	unsigned r : 26;
};
#define MC_WP_LOGWD_REG		0x484

/*******************************************************************************************************/
enum AVC_STATUS
{
	AVC_FINISH = 1<<16,
	AVC_BIT_REQ = 1 << 17,
	AVC_ERROR = 1 << 18,
};

#endif
