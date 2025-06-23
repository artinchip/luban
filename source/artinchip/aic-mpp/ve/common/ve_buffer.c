/*
 * Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <qi.xu@artinchip.com>
 *  Desc: allocator of physic continuous buffer used by ve
 */

#define LOG_TAG "ve_buffer"
#include <pthread.h>

#include "ve_buffer.h"
#include "mpp_mem.h"
#include "mpp_list.h"
#include "dma_allocator.h"
#include "ve.h"
#include "mpp_log.h"

struct ve_buffer_allocator {
	int dma_dev_fd;
	int total_count;
	int total_size;
	int total_max;
	pthread_mutex_t lock;
	struct mpp_list list;
};

struct ve_buffer_impl {
	struct ve_buffer ve_buf;
	struct mpp_list list_status;
};

struct ve_buffer_allocator *ve_buffer_allocator_create(enum ve_buffer_type type)
{
	struct ve_buffer_allocator *ctx;

	ctx = mpp_alloc(sizeof(struct ve_buffer_allocator));
	if (!ctx)
		return NULL;
	memset(ctx, 0, sizeof(struct ve_buffer_allocator));

	ctx->dma_dev_fd = dmabuf_device_open();
	ctx->total_size = 0;
	ctx->total_max = 0;
	ctx->total_count = 0;

	if (ctx->dma_dev_fd < 0) {
		mpp_free(ctx);
		loge("mppbuffer ctx init failed!");
		return NULL;
	}

	pthread_mutex_init(&ctx->lock, NULL);
	mpp_list_init(&ctx->list);

	return ctx;
}

void ve_buffer_allocator_destroy(struct ve_buffer_allocator *ctx)
{
	struct ve_buffer_impl *node = NULL;
	struct ve_buffer_impl *m = NULL;

	if (!ctx)
		return;

	pthread_mutex_lock(&ctx->lock);

	if (!mpp_list_empty(&ctx->list)) {
		mpp_list_for_each_entry_safe(node, m, &ctx->list, list_status) {
			ctx->total_size -= node->ve_buf.size;
			ctx->total_count--;
			mpp_list_del_init(&node->list_status);

			logw("ve_buffer leak, dma-buf fd: %d, size: %zu", node->ve_buf.fd, node->ve_buf.size);
			ve_rm_dma_buf(node->ve_buf.fd, node->ve_buf.phy_addr);
			if(node->ve_buf.vir_addr)
				dmabuf_munmap(node->ve_buf.vir_addr, node->ve_buf.size);
			dmabuf_free(node->ve_buf.fd);

			mpp_free(node);
			node = NULL;
		}
	}
	mpp_list_del_init(&ctx->list);

	pthread_mutex_unlock(&ctx->lock);

	if (ctx->total_count > 0) {
		loge("mppbuffer ctx deinit, there are %d buffers not released.", ctx->total_count);
	}

	pthread_mutex_destroy(&ctx->lock);
	dmabuf_device_close(ctx->dma_dev_fd);
	mpp_free(ctx);
}

struct ve_buffer *ve_buffer_alloc(struct ve_buffer_allocator *ctx, int size, enum ve_buffer_flag flag)
{
	struct ve_buffer_impl *buf_impl;

	if (!ctx || size <= 0) {
		return NULL;
	}

	buf_impl = (struct ve_buffer_impl*)mpp_alloc(sizeof(struct ve_buffer_impl));
	if(!buf_impl) {
		return NULL;
	}
	memset(buf_impl, 0, sizeof(struct ve_buffer_impl));

	//* 1. alloc dma-buf
	buf_impl->ve_buf.fd = dmabuf_alloc(ctx->dma_dev_fd, size);
	if(buf_impl->ve_buf.fd < 0) {
		if(buf_impl)
			mpp_free(buf_impl);
		return NULL;
	}

	//* 2. mmap
	if(flag & ALLOC_NEED_VIR_ADDR)
		buf_impl->ve_buf.vir_addr = dmabuf_mmap(buf_impl->ve_buf.fd, size);

	//* 3. add dma-buf to ve
	ve_add_dma_buf(buf_impl->ve_buf.fd, &buf_impl->ve_buf.phy_addr);
	buf_impl->ve_buf.size = size;

	pthread_mutex_lock(&ctx->lock);

	ctx->total_size += size;
	if (ctx->total_size > ctx->total_max)
		ctx->total_max = ctx->total_size;
	ctx->total_count++;

	mpp_list_init(&buf_impl->list_status);
	mpp_list_add_tail(&buf_impl->list_status, &ctx->list);

	pthread_mutex_unlock(&ctx->lock);

	logi("ve_buffer alloc success, fd: %d, size: %zu", buf_impl->ve_buf.fd, buf_impl->ve_buf.size);

	return (struct ve_buffer *)buf_impl;
}

int ve_buffer_sync(struct ve_buffer *buf, enum dma_buf_sync_flag flag)
{
	if(buf == NULL)
		return -1;

	return dmabuf_sync(buf->fd, flag);
}

int ve_buffer_sync_range(struct ve_buffer *buf, unsigned char* start_addr, int size, enum dma_buf_sync_flag flag)
{
	if(buf == NULL)
		return -1;

	return dmabuf_sync_range(buf->fd, start_addr, size, flag);
}

void ve_buffer_free(struct ve_buffer_allocator *ctx, struct ve_buffer *buf)
{
	struct ve_buffer_impl* buf_impl = (struct ve_buffer_impl*)buf;

	if(!ctx || !buf)
		return;

	logi("ve_buffer_free, fd: %d, size: %zu", buf->fd, buf->size);
	pthread_mutex_lock(&ctx->lock);

	ctx->total_size -= buf->size;
	ctx->total_count--;
	mpp_list_del_init(&buf_impl->list_status);

	pthread_mutex_unlock(&ctx->lock);

	ve_rm_dma_buf(buf->fd, buf->phy_addr);
	if(buf->vir_addr)
		dmabuf_munmap(buf->vir_addr, buf->size);
	dmabuf_free(buf->fd);

	mpp_free(buf_impl);
}
