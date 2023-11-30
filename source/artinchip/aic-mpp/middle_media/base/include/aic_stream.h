/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: aic_stream
*/

#ifndef __AIC_STREAM_H__
#define __AIC_STREAM_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "mpp_dec_type.h"

struct aic_stream {
	/* read data */
	s64 (*read)(struct aic_stream *stream, void *buf, s64 len);
	/* write data */
	s64 (*write)(struct aic_stream *stream, void *buf, s64 len);
	/* return current file offset */
	s64 (*tell)(struct aic_stream *stream);
	/* close stream */
	s32 (*close)(struct aic_stream *stream);
	/* seek */
	s64 (*seek)(struct aic_stream *stream, s64 offset, s32 whence);
	/* get stream total size */
	s64 (*size)(struct aic_stream *stream);
};

#define aic_stream_read(      \
		   stream,            \
		   buf,				  \
		   len)               \
	    ((struct aic_stream*)stream)->read(stream,buf,len)

#define aic_stream_write(      \
		   stream,            \
		   buf,				  \
		   len)               \
	    ((struct aic_stream*)stream)->write(stream,buf,len)

#define aic_stream_seek(      \
		   stream,            \
		   offset,			  \
		   whence)            \
	    ((struct aic_stream*)stream)->seek(stream,offset,whence)

#define aic_stream_tell(stream)\
	    ((struct aic_stream*)stream)->tell(stream)

#define aic_stream_size(stream)\
	    ((struct aic_stream*)stream)->size(stream)

#define aic_stream_close(stream)\
	    ((struct aic_stream*)stream)->close(stream)

s32 aic_stream_open(char *uri, struct aic_stream **stream, int flags);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */


#endif





