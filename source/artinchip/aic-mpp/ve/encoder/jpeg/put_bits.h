/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <qi.xu@artinchip.com>
*  Desc: put bits
*/

#ifndef __PUT_BITS_H_
#define __PUT_BITS_H_

#include <unistd.h>
#include "mpp_log.h"

static const int BUF_BITS = 8 * sizeof(uint32_t);

struct put_bit_ctx {
	uint32_t bit_buf;
	int bit_left;
	uint8_t *buf, *buf_ptr, *buf_end;
	int size_in_bits;
};

static inline void init_put_bits(struct put_bit_ctx *s, uint8_t *buffer, int buffer_size)
{
	if (buffer_size < 0) {
		buffer_size = 0;
		buffer = NULL;
		return;
	}

	s->size_in_bits = 8 * buffer_size;
	s->buf = buffer;
	s->buf_end = s->buf + buffer_size;
	s->buf_ptr = s->buf;
	s->bit_left = BUF_BITS;
	s->bit_buf = 0;
}

static inline int put_bits_count(struct put_bit_ctx *s)
{
	return (s->buf_ptr - s->buf) * 8 + BUF_BITS - s->bit_left;
}

static inline int put_bits_left(struct put_bit_ctx* s)
{
	return (s->buf_end - s->buf_ptr) * 8 - BUF_BITS + s->bit_left;
}

#define AV_BSWAP16C(x) (((x) << 8 & 0xff00)  | ((x) >> 8 & 0x00ff))
#define AV_BSWAP32C(x) (AV_BSWAP16C(x) << 16 | AV_BSWAP16C((x) >> 16))
#define AV_WLBUF(p, v) (*(( uint64_t*)(p)) = (v))
#define AV_WBBUF(p, v) (*(( uint64_t*)(p)) = (v))
static inline void put_bits_no_assert(struct put_bit_ctx* s, int n, uint32_t value)
{
	uint32_t bit_buf;
	int bit_left;

	bit_buf = s->bit_buf;
	bit_left = s->bit_left;

	if (n < bit_left) {
		bit_buf = (bit_buf << n) | value;
		bit_left -= n;
	} else {
		bit_buf <<= bit_left;
		bit_buf |= value >> (n - bit_left);
		if (s->buf_end - s->buf_ptr >= sizeof(uint32_t)) {
			AV_WBBUF(s->buf_ptr, AV_BSWAP32C(bit_buf));
			s->buf_ptr += sizeof(uint32_t);
		} else {
			loge("Internal error, put_bits buffer too small\n");
			mpp_assert(0);
		}
		bit_left += BUF_BITS - n;
		bit_buf = value;
	}

	s->bit_buf = bit_buf;
	s->bit_left = bit_left;
}

/**
* Write up to 31 bits into a bitstream.
* Use put_bits32 to write 32 bits.
*/
static inline void put_bits(struct put_bit_ctx *s, int n, uint32_t value)
{
	if(n <= 31 && value < (1UL << n))
		put_bits_no_assert(s, n, value);
	else
	{
		loge("put bits fail, n: %d, val: 0x%x", n, value);
	}
}

static unsigned av_mod_uintp2_c(unsigned a, unsigned p)
{
	return a & ((1U << p) - 1);
}

static inline void put_sbits(struct put_bit_ctx *pb, int n, int32_t value)
{
	if (n < 0 || n>31)
	{
		loge("put_sbits error");
		return;
	}

	put_bits(pb, n, av_mod_uintp2_c(value, n));
}

/**
* Pad the end of the output stream with zeros.
*/
static inline void flush_put_bits(struct put_bit_ctx *s)
{
	if (s->bit_left < BUF_BITS)
		s->bit_buf <<= s->bit_left;
	while (s->bit_left < BUF_BITS) {
		mpp_assert(s->buf_ptr < s->buf_end);
		*s->buf_ptr++ = s->bit_buf >> (BUF_BITS - 8);
		s->bit_buf <<= 8;
		s->bit_left += 8;
	}
	s->bit_left = BUF_BITS;
	s->bit_buf = 0;
}

static inline uint8_t *put_bits_ptr(struct put_bit_ctx *s)
{
	return s->buf_ptr;
}

static inline void skip_put_bytes(struct put_bit_ctx *s, int n)
{
	mpp_assert((put_bits_count(s) & 7) == 0);
	mpp_assert(s->bit_left == BUF_BITS);
	mpp_assert(n <= s->buf_end - s->buf_ptr);
	s->buf_ptr += n;
}

#endif
