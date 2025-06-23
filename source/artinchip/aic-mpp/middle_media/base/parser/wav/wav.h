/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: mp3
 */

#ifndef __WAV_H__
#define __WAV_H__

#include <unistd.h>
#include "aic_parser.h"

struct wave_header
{
    char riff_id[4];     // 'R','I','F','F'
    uint32_t riff_size;
    char riff_format[4]; // 'W','A','V','E'
};

struct wave_format
{
    uint16_t format;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t avg_bytes_per_sec;
    uint16_t block_align;
    uint16_t bits_per_sample;
};

struct wave_format_block
{
    char fmt_id[4];    // 'f','m','t',' '
    uint32_t fmt_size;
    struct wave_format wav_format;
};

struct wave_data
{
    char data_id[4];     // 'R','I','F','F'
    uint32_t data_size;
};

struct wave_info
{
    struct wave_header header;
    struct wave_format_block fmt_block;
    struct wave_data  data_block;
};

struct aic_wav_parser {
    struct aic_parser base;
    struct aic_stream* stream;
    struct wave_info wav_info;
    uint64_t first_packet_pos;

    uint64_t file_size;
    uint64_t duration;//us
    uint32_t frame_id;

    int frame_duration;//us
    int frame_size;
    int data_size_per_sec;

    uint16_t channels;
    uint32_t sample_rate;
    uint16_t bits_per_sample;
};

int wav_read_header(struct aic_wav_parser *s);
int wav_close(struct aic_wav_parser *s);
int wav_peek_packet(struct aic_wav_parser *s, struct aic_parser_packet *pkt);
int wav_seek_packet(struct aic_wav_parser *s, s64 seek_time);
int wav_read_packet(struct aic_wav_parser *s, struct aic_parser_packet *pkt);
#endif
