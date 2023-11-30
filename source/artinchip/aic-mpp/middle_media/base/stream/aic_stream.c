/*
* Copyright (C) 2020-2023 Artinchip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: aic_stream
*/

#include "aic_stream.h"
#include "aic_file_stream.h"

s32 aic_stream_open(char *uri, struct aic_stream **stream, int flags)
{
	s32 ret = 0;
	// now only file_stream,if more,it shoud probe which stream
	ret = file_stream_open(uri, stream, flags);
	return ret;
}
