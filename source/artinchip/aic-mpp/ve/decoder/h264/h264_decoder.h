/*
 * Copyright (C) 2020-2024 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *   author: <qi.xu@artinchip.com>
 *  Desc: h264 decoder contxet
 *
 */
#ifndef _H264_DECODER_H_
#define _H264_DECODER_H_

#include "mpp_codec.h"
#include "ve_buffer.h"
#include "mpp_dec_type.h"
#include "read_bits.h"

// #define SAVE_REG

// slice_type
#define H264_SLICE_P  0 // Predicted
#define H264_SLICE_B  1 // Bi-dir predicted
#define H264_SLICE_I  2 // Intra
#define H264_SLICE_SP 3 // Switching Predicted
#define H264_SLICE_SI 4 // Switching Intra

/* picture type */
#define PICT_TOP_FIELD     1
#define PICT_BOTTOM_FIELD  2
#define PICT_FRAME         3

#define DELAYED_PIC_REF 4

#define MAX_DELAYED_PIC_COUNT 16

#define MAX_MMCO_COUNT 66

// extra frames for picture reorder
#define MAX_B_FRAMES 2

/*
	planing to perform appropriate error handling base on error indicators
*/
enum h264_decoder_error{
	H264_DECODER_ERROR_NONE = 0,
	H264_DECODER_ERROR_SLICETYPE,
	H264_DECODER_ERROR_SLICENUM,
	H264_DECODER_ERROR_SPS,
	H264_DECODER_ERROR_PPS,
	H264_DECODER_ERROR_PICTURESTRUCTUTE,
	H264_DECODER_ERROR_NOEMPTYFRAME,
	H264_DECODER_ERROR_REFPICMARKING,
	H264_DECODER_ERROR_DEBLOCKINGFILTER,
	H264_DECODER_ERROR_HARD,
	H264_DECODER_ERROR_REFLISTREORDERING,
	H264_DECODER_ERROR_CHECKLASTFRAME,
};

/**
 * Memory management control operation opcode.
 */
enum mmco_opcode {
    MMCO_END = 0,
    MMCO_SHORT2UNUSED,
    MMCO_LONG2UNUSED,
    MMCO_SHORT2LONG,
    MMCO_SET_MAX_LONG,
    MMCO_RESET,
    MMCO_LONG,
};

/**
 * Memory management control operation.
 */
struct mmco {
    enum mmco_opcode opcode;
    int short_pic_num;  ///< pic_num without wrapping (pic_num & max_pic_num)
    int long_arg;       ///< index, pic_num, or num long refs depending on opcode
};

enum NAL_TYPE
{
    NAL_TYPE_SLICE		= 1,
    NAL_TYPE_DPA		= 2,
    NAL_TYPE_DPB		= 3,
    NAL_TYPE_DPC   		= 4,
    NAL_TYPE_IDR		= 5,
    NAL_TYPE_SEI		= 6,
    NAL_TYPE_SPS		= 7,
    NAL_TYPE_PPS		= 8,
    NAL_TYPE_AUD		= 9,
    NAL_TYPE_EOSEQ		= 10,
    NAL_TYPE_EOSTREAM		= 11,
    NAL_TYPE_FILL		= 12,
};

struct pred_weight_table
{
	int luma_log2_weight_denom;
	int chroma_log2_weight_denom;
	int wp_weight[2][32][3];	// [list][index][component], range(-128~127)
	int wp_offset[2][32][3];	// [list][index][component], range(-128~127)
};

struct h264_sps_info
{
	u8 mbaff;
	u8 frame_mbs_only_flag;
	u8 direct_8x8_inference_flag;
	u8 scaling_matrix_present_flag;			// u(1)
	u32 level_idc;
	u32 profile_idc;
	u32 constraint_set_flags;
	u32 chroma_format_idc;				// use(v)
	u8 scaling_matrix4[6][16];
	u8 scaling_matrix8[2][64];
	int log2_max_frm_num;				// ue(v), log2_max_frm_num_minus4 + 4
	int poc_type;
	// poc_type == 0
	int log2_max_poc_lsb;				// ue(v), log2_max_pic_order_cnt_lsb_minus4 + 4
	// poc_type == 1
	int delta_pic_order_always_zero_flag;		// u(1)
	int offset_for_non_ref_pic;			// se(v)
	int offset_for_top_to_bottom_field;		// se(v)
	int num_ref_frames_in_poc_cycle;		// ue(v) (0~255)
	u16 offset_for_ref_frame[256];
	int max_num_ref_frames;				// ue(v)
	int gaps_in_frame_num_value_allowed_flag;
	int pic_mb_width;
	int pic_mb_height;
	int frame_cropping_rect_left_offset;
	int frame_cropping_rect_right_offset;
	int frame_cropping_rect_top_offset;
	int frame_cropping_rect_bottom_offset;
};

struct h264_pps_info
{
	u8 deblocking_filter_control_present_flag;
	u8 transform_8x8_mode_flag;
	u8 constrained_intra_pred_flag;
	u8 entropy_coding_flag;				// 0: cavlc; 1: cabac
	u8 bottom_field_pic_order_in_frame_flag;
	u8 weighted_pred_flag;
	u8 weighted_bipred_flag;
	u8 redundant_pic_cnt_present_flag;
	u32 sps_id;
	int num_ref_idx[2];				// num_ref_idx_l0/l1_active_minus1 +1
	u32 pic_init_qp; 				// pic_init_qp_minus26 + 26
	int chroma_qp_index_offset[2];
	u8 scaling_matrix4[6][16];
	u8 scaling_matrix8[2][64];
};

struct h264_slice_header
{
	u8 field_pic_flag;		// u(1)
	u8 bottom_field_flag;		// u(1)
	u8 mbaff_frame;
	u32 first_mb_in_slice;		// ue(v)
	u32 slice_type;			// ue(v)
	u32 pps_id;			// ue(v)
	int frame_num;			// u(v)
	u32 idr_pic_id;			// ue(v)

	// poc type == 0
	int poc_lsb;			// u(v)
	int poc_msb;
	int delta_poc_bottom;		// se(v)
	// poc_type == 1 && !delta_pic_order_always_zero_flag
	int delta_poc[2];		// se(v)

	int num_ref_idx[2];
	int direct_spatial_mv_pred;
	int num_ref_idx_active_override;

	int list_count;			// count of ref lists(I:0, P:1, B:2)

	struct mmco mmco[MAX_MMCO_COUNT];
	int nb_mmco;
	int explicit_ref_marking;	// adaptive_ref_pic_marking_mode_flag

	int cur_pic_num;
	int max_pic_num;
	struct pred_weight_table pwt;
	int cabac_init_idc;
	int qp_y;

	int deblocking_filter;
	int slice_alpha_c0_offset_div2;
	int slice_beta_offset_div2;
};

/*
 picture infomation
*/
struct h264_picture {
	u8 mbaff_frame;			///< 1 -> MBAFF frame 0-> not MBAFF
	u8 nal_ref_idc[2];
	u8 refrence;			// 0: not used for refrence; PICT_FRAME: the frame used by refrence;
	                                // PICT_TOP/PICT_BOT: one field of this frame used by refrence
					// DELAYED_PIC_REF: not used by refrence, and not send to render (need renorder)
	u8  key_frame;
	u8  mmco_reset;
	u8 picture_structure;
	u8 displayed_flag;		// this picture is displayed
	int field_poc[2];		// h264 top/bottom POC
	int poc;			// h264 frame POC
	int frame_num;			// h264 frame_num (raw frame_num from slice header)
	int pic_id;			//< h264 pic_num (short -> no wrap version of pic_num,
						//pic_num & max_pic_num; long -> long_pic_num)
	int long_ref;			// 1->long term nReference 0->short term nReference
	int ref_poc[2][16];		// h264 POCs of the frames used as nReference
	int ref_count[2];		// number of entries in ref_nPoc

	u32 buf_idx;
	u32 top_field_col_addr;		// phy addr for top field col buf, it is a part of col_buf in struct h264_frame_info
	u32 bot_field_col_addr;		// phy addr for bot field col buf
	struct frame* frame;		// frame ptr get from frame manager
};

/*
the refrence field/frame context.
if it is one field of picture, the variable refrence is set PICT_TOP/PICT_FIELD
*/
struct h264_ref {
	int refrence;			// refrence type of current field/frame
	int poc;			// poc of current field/frame
	int pic_id;
	struct h264_picture* parent;	// pointer to picture in struct h264_frame_info
};

struct h264_frame_info {
	int max_valid_frame_num;		// total frame number in frame_manager
	struct ve_buffer* col_buf;		// col buf for all frames, used by ve
	struct ve_buffer* dblk_y_buf;		// y data buffer for deblk, used by ve
	struct ve_buffer* dblk_c_buf;		// c data buffer for deblk, used by ve
	struct ve_buffer* intrap_buf;		// intra-pred buffer, used by ve
	struct ve_buffer* mb_info_buf;		// mb info buffer, used by ve
	struct ve_buffer* mb_col_info_buf;	// mb co-located info

	struct h264_picture picture[32];
	struct h264_picture* long_ref[32];
	struct h264_picture* short_ref[32];
	struct h264_ref  def_ref_list[2][32];
	struct h264_ref  ref_list[2][48];
	struct h264_picture* delayed_output_pic;
	struct h264_picture* delayed_pic[MAX_DELAYED_PIC_COUNT + 2];  // render delayed picture in decoder, for reorder
	int delay_pic_num;
	int last_pocs[MAX_DELAYED_PIC_COUNT];
	struct h264_picture* cur_pic_ptr;	// current decoding picture
};

#define SPS_MAX_NUM 32
#define PPS_MAX_NUM 256
#define RBSP_BYTES  128
struct h264_dec_ctx
{
	struct mpp_decoder decoder;
	unsigned long regs_base;

	int active_pps_id;				// the pps_buffers id
	int active_sps_id;				// the sps_buffers id
	struct h264_pps_info* pps_buffers[PPS_MAX_NUM];	// we need malloc buffer if we get a new pps
	struct h264_sps_info* sps_buffers[SPS_MAX_NUM];	// we need malloc buffer if we get a new pps
	struct h264_slice_header sh;
	struct h264_frame_info frame_info;
	int extra_frame_num;

	struct ve_buffer_allocator* ve_buf_handle;
	struct packet* curr_packet;
	struct frame* curr_frame;
	enum mpp_pixel_format pix_format;		// output pixel format
	int eos;

	int avcc; // nalu_size + nalu, like mov

	int sc_byte_offset;				// byte offset of startcode
	int remove_bytes;				// remove 0x03 bytes
	int first_eptb_offset;				// offste of first 0x03 byte in eptb
	unsigned char rbsp_buffer[RBSP_BYTES];		// rbsp buffer (remove eptb)
	int rbsp_len;

	struct read_bit_context gb;
	int nal_ref_idc;
	int nal_ref_idc_pre;
	int nal_unit_type;
	int idr_pic_flag; // IDR nalu

	int width;
	int height;
	int cur_slice_num;
	int first_field;
	int picture_structure;
	int decode_mb_num;		// current decode mb number
	int mbs_in_pic;			// total mb number in a picture

	int frame_num;			// for ref_mark

	int avc_start;			// for hw reset

	// for poc, ffmpeg s->poc
	int prev_poc_msb;		// poc_msb of the last reference pic for POC type 0
	int prev_poc_lsb;		// poc_lsb of the last reference pic for POC type 0
	int frame_num_offset;		// for POC type 2
	int prev_frame_num_offset;	// for POC type 2
	int prev_frame_num;		// frame_num of the last pic for POC type 1/2

	int min_display_poc;		// last displayed poc, for reorder display picture
	int poc_delta;			// poc delta between two pictures
	int next_output_poc;		// next poc, for reorder display picture

	int has_b_frames;		//
	int b_frames_max_num;

	int long_ref_count;		// number of actual long term references
	int short_ref_count;		// number of actual short term references

	int bit_offset;			// bit offset of slice data this NALU

	int slice_offset;		// slice offset in packet, in byte unit

	FILE* fp_reg;

	int error;
};


/**************** h264_refs *********************/
// init ref list
int init_ref_list(struct h264_dec_ctx *s);

// ref list reordering
int ref_pic_list_reordering(struct h264_dec_ctx *s);

int execute_ref_pic_marking(struct h264_dec_ctx *s);

void select_output_frame(struct h264_dec_ctx *s);

/*
flush all the delay picture, if decoder reset
*/
void flush_all_delay_picture(struct h264_dec_ctx *s);

/*
put all the frame in delayed list to render, used in 2 cases:
1) IDR
2) eos
*/
int render_all_delayed_frame(struct h264_dec_ctx *s);

void reference_refresh(struct h264_dec_ctx *s);

/***************** h264_hal ********************/
int decode_slice(struct h264_dec_ctx *s);

#endif
