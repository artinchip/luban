/*
 * Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <qi.xu@artinchip.com>
 *  Desc: jpeg marker define
 *
 */

#ifndef MJPEG_H
#define MJPEG_H

enum jpeg_marker {
	/* start of frame */
	SOF0  = 0xc0,       /* baseline */
	SOF1  = 0xc1,       /* extended sequential, huffman */
	SOF2  = 0xc2,       /* progressive, huffman */
	SOF3  = 0xc3,       /* lossless, huffman */

	SOF5  = 0xc5,       /* differential sequential, huffman */
	SOF6  = 0xc6,       /* differential progressive, huffman */
	SOF7  = 0xc7,       /* differential lossless, huffman */
	JPG   = 0xc8,       /* reserved for JPEG extension */
	SOF9  = 0xc9,       /* extended sequential, arithmetic */
	SOF10 = 0xca,       /* progressive, arithmetic */
	SOF11 = 0xcb,       /* lossless, arithmetic */

	SOF13 = 0xcd,       /* differential sequential, arithmetic */
	SOF14 = 0xce,       /* differential progressive, arithmetic */
	SOF15 = 0xcf,       /* differential lossless, arithmetic */

	DHT   = 0xc4,       /* define huffman tables */

	DAC   = 0xcc,       /* define arithmetic-coding conditioning */

	RST0  = 0xd0,
	RST1  = 0xd1,
	RST2  = 0xd2,
	RST3  = 0xd3,
	RST4  = 0xd4,
	RST5  = 0xd5,
	RST6  = 0xd6,
	RST7  = 0xd7,

	SOI   = 0xd8,       /* start of image */
	EOI   = 0xd9,       /* end of image */
	SOS   = 0xda,       /* start of scan */
	DQT   = 0xdb,       /* define quantization tables */
	DNL   = 0xdc,       /* define number of lines */
	DRI   = 0xdd,       /* define restart interval */
	DHP   = 0xde,       /* define hierarchical progression */
	EXP   = 0xdf,       /* expand reference components */

	APP0  = 0xe0,
	APP1  = 0xe1,
	APP2  = 0xe2,
	APP3  = 0xe3,
	APP4  = 0xe4,
	APP5  = 0xe5,
	APP6  = 0xe6,
	APP7  = 0xe7,
	APP8  = 0xe8,
	APP9  = 0xe9,
	APP10 = 0xea,
	APP11 = 0xeb,
	APP12 = 0xec,
	APP13 = 0xed,
	APP14 = 0xee,
	APP15 = 0xef,

	JPG0  = 0xf0,
	JPG1  = 0xf1,
	JPG2  = 0xf2,
	JPG3  = 0xf3,
	JPG4  = 0xf4,
	JPG5  = 0xf5,
	JPG6  = 0xf6,
	SOF48 = 0xf7,       ///< JPEG-LS
	LSE   = 0xf8,       ///< JPEG-LS extension parameters
	JPG9  = 0xf9,
	JPG10 = 0xfa,
	JPG11 = 0xfb,
	JPG12 = 0xfc,
	JPG13 = 0xfd,

	COM   = 0xfe,       /* comment */
	TEM   = 0x01,       /* temporary private use for arithmetic coding */
	/* 0x02 -> 0xbf reserved */
};

#endif /* MJPEG_H */
