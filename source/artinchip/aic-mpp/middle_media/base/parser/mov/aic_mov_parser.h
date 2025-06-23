/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *   author: <jun.ma@artinchip.com>
 *  Desc: aic_mov_parser
 */

#ifndef __AIC_MOV_PARSER_H__
#define __AIC_MOV_PARSER_H__

#include "aic_parser.h"
#include "aic_stream.h"

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */

s32 aic_mov_parser_create(unsigned char* uri, struct aic_parser **parser);

#ifdef __cplusplus
}
#endif /* End of #ifdef __cplusplus */

#endif




