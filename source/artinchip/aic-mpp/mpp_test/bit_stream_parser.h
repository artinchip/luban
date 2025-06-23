/*
 * Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <qi.xu@artinchip.com>
 *  Desc:
 */

#include <stdio.h>
#include <stdint.h>

#include "mpp_dec_type.h"

#ifndef BIT_STREAM_PARSER_H
#define BIT_STREAM_PARSER_H

struct bit_stream_parser;

struct bit_stream_parser* bs_create(int fd);
int bs_close(struct bit_stream_parser* p);
int bs_prefetch(struct bit_stream_parser* p, struct mpp_packet* pkt);
int bs_read(struct bit_stream_parser* pCtx, struct mpp_packet* pkt);

#endif
