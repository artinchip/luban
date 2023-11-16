/*
* Copyright (C) 2020-2023 Artinchip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: aic_file_stream
*/

#ifndef __AIC_FILE_STREAM_H__
#define __AIC_FILE_STREAM_H__

#include "aic_stream.h"

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */

s32 file_stream_open(const char* uri, struct aic_stream **file_stream);

#ifdef __cplusplus
}
#endif /* End of #ifdef __cplusplus */
#endif


