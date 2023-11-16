/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: author: <qi.xu@artinchip.com>
*  Desc: jpeg hal
*/

#ifndef JPEG_HAL_H
#define JPEG_HAL_H

#include "jpeg_register.h"
#include "mjpeg_decoder.h"

typedef struct JPG_REGISTER_LIST {
	reg_jpg_ctrl 			_10_ctrl_reg;
	reg_jpg_size 			_14_size_reg;
	reg_jpg_mcu_info 		_18_mcu_reg;
	reg_jpg_rotmir 			_1c_rotmir_reg;
	reg_jpg_scale 			_20_scale_reg;
	reg_jpg_huff_info 		_80_huff_info_reg;
	reg_jpg_huff_addr 		_84_huff_addr_reg;
	reg_jpg_qmat_info 		_90_qmat_info_reg;
	reg_jpg_qmat_addr 		_94_qmat_addr_reg;
} jpg_reg_list;

int ve_decode_jpeg(struct mjpeg_dec_ctx *s, int byte_offset);

#endif
