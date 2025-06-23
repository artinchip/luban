/*
 * Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <qi.xu@artinchip.com>
 *  Desc: byte stream reader
 *
 */

#ifndef BYTESTREAM_H
#define BYTESTREAM_H

#include <stdint.h>
#include <string.h>

typedef struct get_byte_ctx {
    const uint8_t *buffer, *buffer_end, *buffer_start;
} get_byte_ctx;

static inline void bytestream2_init(get_byte_ctx *g,
                                              const uint8_t *buf,
                                              int buf_size)
{
    g->buffer       = buf;
    g->buffer_start = buf;
    g->buffer_end   = buf + buf_size;
}

static inline unsigned int bytestream2_get_byte(get_byte_ctx *g)
{
	const uint8_t* ptr = g->buffer;
	g->buffer += 1;
	return ptr[0];
}

static inline unsigned int bytestream2_get_be32(get_byte_ctx *g)
{
	const uint8_t* ptr = g->buffer;
	g->buffer += 4;
	return ((unsigned int)ptr[0] << 24) |
			((unsigned int)ptr[1] << 16) |
			((unsigned int)ptr[2] << 8) |
			(unsigned int)ptr[3];
}

static inline uint64_t bytestream2_get_be64(get_byte_ctx *g)
{
	const uint8_t* ptr = g->buffer;
	g->buffer += 8;
	return 	((uint64_t)ptr[0] << 56) | ((uint64_t)ptr[1] << 48) |
			((uint64_t)ptr[2] << 40) | ((uint64_t)ptr[3] << 32) |
			((uint64_t)ptr[4] << 24) | ((uint64_t)ptr[5] << 16) |
			((uint64_t)ptr[6] <<  8) | ((uint64_t)ptr[7]);
}

static inline unsigned int bytestream2_get_le32(get_byte_ctx *g)
{
	const uint8_t* ptr = g->buffer;
	g->buffer += 4;
	return 	((unsigned int)ptr[3] << 24) | ((unsigned int)ptr[2] << 16) |
	 		((unsigned int)ptr[1] <<  8) | (unsigned int)ptr[0];
}

static inline unsigned int bytestream2_get_bytes_left(get_byte_ctx *g)
{
    return g->buffer_end - g->buffer;
}

static inline void bytestream2_skip(get_byte_ctx *g,
                                              unsigned int size)
{
	int left = g->buffer_end - g->buffer;

    g->buffer += left < size ? left : size;
}

static inline int bytestream2_tell(get_byte_ctx *g)
{
    return (int)(g->buffer - g->buffer_start);
}

#endif /* BYTESTREAM_H */
