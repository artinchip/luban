/*
* Copyright (C) 2020-2022 ArtInChip Technology Co. Ltd
*
*  author: <qi.xu@artinchip.com>
*  Desc: bit stream parser
*/

#define LOG_TAG "bs_parser"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bit_stream_parser.h"
#include "mpp_mem.h"
#include "mpp_log.h"

#define STEAM_BUF_LEN (512*1024)

struct bit_stream_parser {
	int fd;
	int file_size;
	int cur_read_len;

	char* stream_buf;
	int cur_read_pos;
	int buf_len;
	int valid_size;
};

struct bit_stream_parser* bs_create(int fd)
{
	struct bit_stream_parser* ctx = NULL;

	ctx = (struct bit_stream_parser*)mpp_alloc(sizeof(struct bit_stream_parser));
	if(ctx == NULL) {
		loge("malloc for rawParserContext failed");
		return NULL;
	}
	memset(ctx, 0, sizeof(struct bit_stream_parser));

	ctx->fd = fd;

	ctx->stream_buf = (char*)mpp_alloc(STEAM_BUF_LEN);
	if(ctx->stream_buf == NULL) {
		loge(" malloc for stream data failed");
		free(ctx);
		return NULL;
	}
	ctx->buf_len = STEAM_BUF_LEN;

	ctx->file_size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);

	return ctx;
}

int bs_close(struct bit_stream_parser* p)
{
	if(p) {
		if(p->stream_buf) {
			free(p->stream_buf);
		}
		free(p);
	}
	return 0;
}

static int get_data(struct bit_stream_parser* p)
{
	int r_len = 0;
	if(p->valid_size <= 0) {
		r_len = read(p->fd, p->stream_buf, p->buf_len);
		if(r_len <= 0) {
			return r_len;
		}
		p->cur_read_len += r_len;

		p->valid_size = r_len;
		p->cur_read_pos = 0;
	} else {
		memmove(p->stream_buf, (p->stream_buf + p->cur_read_pos), p->valid_size);

		int len = p->buf_len - p->valid_size;
		r_len = read(p->fd, p->stream_buf + p->valid_size, len);
		p->cur_read_len += r_len;

		p->valid_size += r_len;
		p->cur_read_pos = 0;
	}
	return r_len;
}

int bs_prefetch(struct bit_stream_parser* p, struct mpp_packet* pkt)
{
	int i = 0;
	char tmp_buf[3];
	int find_start_code = 0;
	int nStart = 0;
	int stream_data_len = -1;
	int ret = 0;

	char* cur_data_ptr = NULL;

	if(p->valid_size <= 0) {
		ret = get_data(p);
		if(ret == -1) {
			loge("get data error");
			return -1;
		}
	}

find_startCode:

	cur_data_ptr = p->stream_buf + p->cur_read_pos;

	logd("data: %x, %x, %x, %x, %x, %x, %x, %x", *(cur_data_ptr), *(cur_data_ptr + 1),
		*(cur_data_ptr+2),*(cur_data_ptr+3),
		*(cur_data_ptr+4),*(cur_data_ptr+5),*(cur_data_ptr+6),*(cur_data_ptr+7));
	//* find the first startcode
	for(i = 0; i < (p->valid_size - 3); i++) {
		tmp_buf[0] = *(cur_data_ptr + i);
		tmp_buf[1] = *(cur_data_ptr + i + 1);
		tmp_buf[2] = *(cur_data_ptr + i + 2);
		if(tmp_buf[0] == 0 && tmp_buf[1] == 0 && tmp_buf[2] == 1) {
			find_start_code = 1;
			break;
		}
	}

	logd("find_start_code = %d, i = %d, valisSize = %d",\
		find_start_code, i, p->valid_size);
	if(find_start_code == 1) {
		p->cur_read_pos += i;
		nStart = i;
		if(*(cur_data_ptr + i -1) == 0) {
			p->cur_read_pos -= 1;
			nStart -= 1;
		}
		find_start_code = 0;

		//* find the next startcode
		for(i += 3; i < (p->valid_size - 3); i++) {
			logv("cur_data_ptr = %p, i = %d", cur_data_ptr, i);
			tmp_buf[0] = *(cur_data_ptr + i);
			tmp_buf[1] = *(cur_data_ptr + i + 1);
			tmp_buf[2] = *(cur_data_ptr + i + 2);
			if(tmp_buf[0] == 0 && tmp_buf[1] == 0 && tmp_buf[2] == 1) {
				find_start_code = 1;
				break;
			}
		}

		if(find_start_code == 1) {
			if(*(cur_data_ptr + i - 1) == 0) {
				stream_data_len = i - nStart - 1;
			} else {
				stream_data_len = i - nStart;
			}
		} else {
			ret = get_data(p);
			if(ret == -1)
				return -1;
			if(ret == 0) {
				logi("eos, file_size: %d, cur_read: %d", p->file_size, p->cur_read_len);
				stream_data_len = p->valid_size - nStart;
				pkt->flag |= PACKET_FLAG_EOS;
				goto out;
			}

			goto find_startCode;
		}
	} else {
		ret = get_data(p);
		if(ret == -1 || ret == 0)
			return -1;

		goto find_startCode;
	}

out:
	pkt->size = stream_data_len;
	logd("packet size = %d", pkt->size);
	return 0;
}

int bs_read(struct bit_stream_parser* p, struct mpp_packet* pkt)
{
    if(pkt->size <= 0)
        return -1;

    char* read_ptr = p->stream_buf + p->cur_read_pos;

    logd("read data: %x, %x, %x, %x", *read_ptr,*(read_ptr+1), *(read_ptr+2), *(read_ptr+3));

    memcpy(pkt->data, read_ptr, pkt->size);

    p->cur_read_pos += pkt->size;
    p->valid_size -= pkt->size;

    return 0;

}


