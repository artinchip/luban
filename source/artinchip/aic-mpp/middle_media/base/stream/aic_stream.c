/*
 * Copyright (C) 2020-2023 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: aic_stream
 */
#include <stdio.h>
#include <unistd.h>
#include "aic_stream.h"
#include "aic_file_stream.h"

s32 aic_stream_open(char *uri, struct aic_stream **stream, int flags)
{
	s32 ret = 0;
	// now only file_stream,if more,it shoud probe which stream
	ret = file_stream_open(uri, stream, flags);
	return ret;
}

int aic_stream_skip(struct aic_stream *s, int len)
{
    return aic_stream_seek(s, len, SEEK_CUR);
}


void aic_stream_w8(struct aic_stream *s, int b)
{
    /* aic_stream_write(s, &val, 1);*/
}

void aic_stream_wl32(struct aic_stream *s, unsigned int val)
{
    aic_stream_w8(s, (uint8_t)val);
    aic_stream_w8(s, (uint8_t)(val >> 8));
    aic_stream_w8(s, (uint8_t)(val >> 16));
    aic_stream_w8(s, val >> 24);
}

void aic_stream_wb32(struct aic_stream *s, unsigned int val)
{
    aic_stream_w8(s, val >> 24);
    aic_stream_w8(s, (uint8_t)(val >> 16));
    aic_stream_w8(s, (uint8_t)(val >> 8));
    aic_stream_w8(s, (uint8_t)val);
}

void aic_stream_wl64(struct aic_stream *s, uint64_t val)
{
    aic_stream_wl32(s, (uint32_t)(val & 0xffffffff));
    aic_stream_wl32(s, (uint32_t)(val >> 32));
}

void aic_stream_wb64(struct aic_stream *s, uint64_t val)
{
    aic_stream_wb32(s, (uint32_t)(val >> 32));
    aic_stream_wb32(s, (uint32_t)(val & 0xffffffff));
}

void aic_stream_wl16(struct aic_stream *s, unsigned int val)
{
    aic_stream_w8(s, (uint8_t)val);
    aic_stream_w8(s, (int)val >> 8);
}

void aic_stream_wb16(struct aic_stream *s, unsigned int val)
{
    aic_stream_w8(s, (int)val >> 8);
    aic_stream_w8(s, (uint8_t)val);
}

void aic_stream_wl24(struct aic_stream *s, unsigned int val)
{
    aic_stream_wl16(s, val & 0xffff);
    aic_stream_w8(s, (int)val >> 16);
}

void aic_stream_wb24(struct aic_stream *s, unsigned int val)
{
    aic_stream_wb16(s, (int)val >> 8);
    aic_stream_w8(s, (uint8_t)val);
}

int aic_stream_r8(struct aic_stream *s)
{
    unsigned char val;
    aic_stream_read(s, &val, 1);
    return val;
}

unsigned int aic_stream_rl16(struct aic_stream *s)
{
    unsigned int val;
    val = aic_stream_r8(s);
    val |= aic_stream_r8(s) << 8;
    return val;
}

unsigned int aic_stream_rl24(struct aic_stream *s)
{
    unsigned int val;
    val = aic_stream_rl16(s);
    val |= aic_stream_r8(s) << 16;
    return val;
}

unsigned int aic_stream_rl32(struct aic_stream *s)
{
    unsigned int val;
    val = aic_stream_rl16(s);
    val |= aic_stream_rl16(s) << 16;
    return val;
}

uint64_t aic_stream_rl64(struct aic_stream *s)
{
    uint64_t val;
    val = (uint64_t)aic_stream_rl32(s);
    val |= (uint64_t)aic_stream_rl32(s) << 32;
    return val;
}

unsigned int aic_stream_rb16(struct aic_stream *s)
{
    unsigned int val;
    val = aic_stream_r8(s) << 8;
    val |= aic_stream_r8(s);
    return val;
}

unsigned int aic_stream_rb24(struct aic_stream *s)
{
    unsigned int val;
    val = aic_stream_rb16(s) << 8;
    val |= aic_stream_r8(s);
    return val;
}
unsigned int aic_stream_rb32(struct aic_stream *s)
{
    unsigned int val;
    val = aic_stream_rb16(s) << 16;
    val |= aic_stream_rb16(s);
    return val;
}

uint64_t aic_stream_rb64(struct aic_stream *s)
{
    uint64_t val;
    val = (uint64_t)aic_stream_rb32(s) << 32;
    val |= (uint64_t)aic_stream_rb32(s);
    return val;
}
