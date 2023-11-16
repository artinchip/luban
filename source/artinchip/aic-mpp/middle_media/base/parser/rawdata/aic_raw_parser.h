/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <qi.xu@artinchip.com>
*  Desc: aic_raw_parser
*/

#ifndef __AIC_RAW_PARSER_H__
#define __AIC_RAW_PARSER_H__

#include "aic_parser.h"

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */

s32 aic_raw_parser_create(unsigned char* uri, struct aic_parser **parser);

#ifdef __cplusplus
}
#endif /* End of #ifdef __cplusplus */

#endif
