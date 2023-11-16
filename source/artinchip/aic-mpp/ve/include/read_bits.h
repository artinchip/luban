/*
* Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
*
*  author: <qi.xu@artinchip.com>
*  Desc: parse bitstream
*/

#ifndef READ_BITS_H
#define READ_BITS_H

#include <math.h>
#include <stdint.h>
#include "mpp_log.h"

#define MIN_CACHE_BITS 25
#define NEG_USR32(a, s) (((uint32_t)(a)) >> (32 - (s)))
#define BSWAP32(x) ((((x) << 8 & 0xff00) | ((x) >> 8 & 0x00ff)) << 16 | \
		((((x) >> 16) << 8 & 0xff00) | (((x) >> 16) >> 8 & 0x00ff)))

struct read_bit_context {
    const unsigned char *buffer, *buffer_end;

    int index;
    int size_in_bits;        // bit size in buffer
};

static inline int init_read_bits(struct read_bit_context* s, const unsigned char* buf, int bit_size)
{
    int ret = 0;
    if (bit_size < 0 || !buf) {
        bit_size = 0;
        buf = NULL;
        ret = -1;
    }

    int buffer_size = (bit_size + 7) >> 3;

    s->buffer = buf;
    s->size_in_bits = bit_size;
    s->buffer_end = buf + buffer_size;
    s->index = 0;

    return ret;
}

/**
* Read 1-25 bits.
* careful: we donot check the end of bitstream
*/
static inline unsigned int read_bits(struct read_bit_context *s, int n)
{
    unsigned int re_index = s->index;
    unsigned char* t = (unsigned char*)(s->buffer) + (re_index >> 3);
    unsigned int re_cache = (t[0]<<24| t[1]<<16 | t[2]<<8| t[3]) << (re_index & 7);
    unsigned int tmp = NEG_USR32(re_cache, n);

    re_index += n;
    s->index = re_index;

    return tmp;
}

static inline void skip_bits(struct read_bit_context *s, int n)
{
    unsigned int re_index = s->index;
    re_index += n;
    s->index = re_index;
}

/**
* Show 1-25 bits.
*/
static inline unsigned int show_bits(struct read_bit_context *s, int n)
{
    unsigned int re_index = (s)->index; unsigned int re_cache;
    re_cache = BSWAP32((*((const uint32_t*)((s)->buffer + (re_index >> 3))))) << (re_index & 7);

    unsigned int tmp = NEG_USR32(re_cache, n);

    return tmp;
}

/**
* get current bit offset.
*/
static inline int read_bits_count(struct read_bit_context *s)
{
    return s->index;
}

/**
* get bits left in stream.
*/
static inline int read_bits_left(struct read_bit_context *s)
{
    return s->size_in_bits - s->index;
}

/**
 * Read 0-32 bits.(big edient)
 */
static inline unsigned int read_bits_long(struct read_bit_context *s, int n)
{
    mpp_assert(n>=0 && n<=32);
    if (!n) {
        return 0;
    } else if (n <= MIN_CACHE_BITS) {
        return read_bits(s, n);
    } else {
        unsigned ret = read_bits(s, 16) << (n - 16);
        return ret | read_bits(s, n - 16);
    }

}

/**
 * Show 0-32 bits.
 */
static inline unsigned int show_bits_long(struct read_bit_context *s, int n)
{
    if (n <= MIN_CACHE_BITS) {
        return show_bits(s, n);
    } else {
        struct read_bit_context gb = *s;
        return read_bits_long(&gb, n);
    }
}

/**
 * read ue(v) for avc.
 */
static inline int read_ue_golomb(struct read_bit_context *gb)
{
    int prefix_zero_cnt = 0;
    int val;
    int prefix = 0;
    int surfix = 0;
    int i=0;

    while(1)
    {
        val = read_bits(gb, 1);
        if(val == 0)
		prefix_zero_cnt++;
        else
		break;
    }
    prefix = (1 << prefix_zero_cnt) -1;
    for(i=0; i<prefix_zero_cnt; i++)
    {
        val = read_bits(gb, 1);
        surfix += val*(1 << (prefix_zero_cnt-i-1));
    }

    return prefix + surfix;
}

/**
 * read se(v) for avc.
 */
static inline int read_se_golomb(struct read_bit_context *gb)
{
    int uev = read_ue_golomb(gb);
    int sign = (uev & 1) ? 1: -1;
    int sev = sign * ((uev+1) >> 1);

    return sev;
}

#endif /* READ_BITS_H */
