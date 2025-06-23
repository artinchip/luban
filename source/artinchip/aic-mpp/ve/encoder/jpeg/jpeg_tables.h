/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <qi.xu@artinchip.com>
 *  Desc: jpeg table
 */

#ifndef JPEG_TABLES_H
#define JPEG_TABLES_H

#include <stdint.h>

extern const unsigned char std_luminance_quant_tbl[];
extern const unsigned char std_chrominance_quant_tbl[];

extern const uint8_t avpriv_mjpeg_bits_dc_luminance[];
extern const uint8_t avpriv_mjpeg_val_dc[];

extern const uint8_t avpriv_mjpeg_bits_dc_chrominance[];

extern const uint8_t avpriv_mjpeg_bits_ac_luminance[];
extern const uint8_t avpriv_mjpeg_val_ac_luminance[];

extern const uint8_t avpriv_mjpeg_bits_ac_chrominance[];
extern const uint8_t avpriv_mjpeg_val_ac_chrominance[];

extern uint8_t zigzag_direct[64];

void mjpeg_build_huffman_codes(uint8_t *huff_size, uint16_t *huff_code,
                                  const uint8_t *bits_table,
                                  const uint8_t *val_table);

#endif
