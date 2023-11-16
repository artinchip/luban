/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: aic_file_stream
*/

/*why the macro definition is placed here:
after the header file ,the complier error*/

#define _LARGEFILE64_SOURCE

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "mpp_mem.h"
#include "mpp_log.h"
#include "aic_stream.h"
#include "aic_file_stream.h"

struct aic_file_stream{
	struct aic_stream base;
	s32 fd;
	s64 file_size;
	s64 file_pos;

	unsigned char *buf;
	int buf_len;
	unsigned char *read_ptr;
	int valid_len;
};

static int fill_buf(struct aic_file_stream *file_stream)
{
	int read_len = read(file_stream->fd, file_stream->buf, file_stream->buf_len);
	if (read_len <= 0)
		return read_len;

	file_stream->read_ptr = file_stream->buf;
	file_stream->valid_len = read_len;
	return read_len;
}

static s64 file_stream_read(struct aic_stream *stream, void *buf, s64 len)
{
	s64 ret;
	struct aic_file_stream *file_stream = (struct aic_file_stream *)stream;
	s64 read_len = file_stream->valid_len >= len ? len : file_stream->valid_len;
	s64 left = len;
	unsigned char* dst_buf = buf;

	memcpy(dst_buf, file_stream->read_ptr, read_len);
	file_stream->read_ptr += read_len;
	file_stream->valid_len -= read_len;
	file_stream->file_pos += read_len;
	dst_buf += read_len;
	left -= read_len;

	while (left > 0) {
		ret = fill_buf(file_stream);
		if (ret == 0) {
			return len - left;
		} else if (ret < 0) {
			return ret;
		}

		read_len = file_stream->valid_len >= left ? left : file_stream->valid_len;
		memcpy(dst_buf, file_stream->read_ptr, read_len);
		file_stream->read_ptr += read_len;
		file_stream->valid_len -= read_len;
		file_stream->file_pos += read_len;
		dst_buf += read_len;
		left -= read_len;
	}
	//unsigned char* ptr = buf;
	//logi("buf: %x, read_ptr: %x", ptr[0], file_stream->read_ptr[0]);

	return len;
}

static s64 file_stream_tell(struct aic_stream *stream)
{
	struct aic_file_stream *file_stream = (struct aic_file_stream *)stream;

	return file_stream->file_pos;
}

static s32 file_stream_close(struct aic_stream *stream)
{
	struct aic_file_stream *file_stream = (struct aic_file_stream *)stream;
	if (file_stream) {
		close(file_stream->fd);
		mpp_free(file_stream->buf);
		mpp_free(file_stream);
	}

	return 0;
}

static s64 file_stream_seek(struct aic_stream *stream, s64 offset, s32 whence)
{
	struct aic_file_stream *file_stream = (struct aic_file_stream *)stream;

	if (whence == SEEK_CUR) {
		if (file_stream->valid_len >= offset) {
			file_stream->file_pos += offset;
			file_stream->read_ptr += offset;
			file_stream->valid_len -= offset;
		} else {
			file_stream->file_pos = lseek64(file_stream->fd,
				offset - file_stream->valid_len, whence);
			file_stream->read_ptr = file_stream->buf;
			file_stream->valid_len = 0;
		}
	} else {
		file_stream->read_ptr = file_stream->buf;
		file_stream->valid_len = 0;
		file_stream->file_pos = lseek64(file_stream->fd, offset, whence);
	}

	return file_stream->file_pos;
}

static s64 file_stream_size(struct aic_stream *stream)
{
	struct aic_file_stream *file_stream = (struct aic_file_stream *)stream;
	return file_stream->file_size;
}

s32 file_stream_open(const char* uri,struct aic_stream **stream)
{
	s32 ret = 0;

	struct aic_file_stream *file_stream = (struct aic_file_stream *)mpp_alloc(sizeof(struct aic_file_stream));
	if(file_stream == NULL){
		loge("mpp_alloc aic_stream ailed!!!!!\n");
		ret = -1;
		goto exit;
	}

	file_stream->fd = open(uri, O_RDWR|O_LARGEFILE);
	if(file_stream->fd < 0){
		loge("open uri:%s failed!!!!!\n",uri);
		ret = -2;
		goto exit;
	}

	file_stream->buf_len = 32*1024;
	file_stream->buf = (unsigned char*)mpp_alloc(file_stream->buf_len);
	if (file_stream->buf == NULL) {
		loge("alloc buf failed");
		ret = -1;
		goto exit;
	}
	file_stream->read_ptr = file_stream->buf;
	file_stream->valid_len = 0;
	file_stream->file_pos = 0;

	file_stream->file_size = lseek64(file_stream->fd, 0, SEEK_END);
	lseek64(file_stream->fd, 0, SEEK_SET);

	file_stream->base.read =  file_stream_read;
	file_stream->base.close = file_stream_close;
	file_stream->base.seek = file_stream_seek;
	file_stream->base.size =  file_stream_size;
	file_stream->base.tell = file_stream_tell;
	*stream = &file_stream->base;
	return ret;

exit:
	if (file_stream != NULL) {
		mpp_free(file_stream);
		if (file_stream->fd)
			close(file_stream->fd);
	}

	*stream = NULL;
	return ret;
}
