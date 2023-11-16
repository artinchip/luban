/*
* Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
*
*  author: <qi.xu@artinchip.com>
*  Desc: allocator of physic continuous buffer used by ve
*/

#ifndef VE_BUFFER_H
#define VE_BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <limits.h>
#include <stdint.h>
#include <stddef.h>
#include "dma_allocator.h"

enum ve_buffer_type {
	VE_BUFFER_TYPE_DMA,
};

enum ve_buffer_flag {
	// need virtual address
	ALLOC_NEED_VIR_ADDR = 1,
};

/**
 * struct ve_buffer
 * @vir_addr: virtual address of this buffer
 * @phy_addr: physic address of this buffer
 * @size: buffer size
 * @fd: dma-buf fd
 */
struct ve_buffer {
	unsigned char *vir_addr;        // virtual address
	unsigned int phy_addr;          // physic address
	size_t size;                    // buffer size
	int fd;	                        // dma-buf fd
};

struct ve_buffer_allocator;

/**
 * ve_buffer_allocator_create - create ve buffer allocator
 * @type: buffer type ( DMA or others)
 */
struct ve_buffer_allocator *ve_buffer_allocator_create(enum ve_buffer_type type);

/**
 * ve_buffer_allocator_destroy - destory ve buffer allocator
 * @ctx: ve buffer allocator
 */
void ve_buffer_allocator_destroy(struct ve_buffer_allocator *ctx);

/**
 * ve_buffer_alloc - alloc one ve buffer
 * @ctx: ve buffer allocator
 * @size: buffer size
 * @flag: alloc flag
 */
struct ve_buffer *ve_buffer_alloc(struct ve_buffer_allocator *ctx, int size, enum ve_buffer_flag flag);

/**
 * ve_buffer_free - free a ve buffer
 * @ctx: ve buffer allocator
 * @buf: the ve buffer need free
 */
void ve_buffer_free(struct ve_buffer_allocator *ctx, struct ve_buffer *buf);

/**
 * ve_buffer_sync - flush cache for data sync
 * @buf: ve buffer
 * @flag: enum mpp_cache_sync_flag;
 */
int ve_buffer_sync(struct ve_buffer *buf, enum dma_buf_sync_flag flag);

/**
 * ve_buffer_sync_range - flush cache for data sync
 * @buf: ve buffer
 * @start_addr: the virtual address of buffer need sync
 * @size: the size of buffer
 * @flag: enum mpp_cache_sync_flag;
 */
int ve_buffer_sync_range(struct ve_buffer *buf, unsigned char* start_addr, int size, enum dma_buf_sync_flag flag);

#ifdef __cplusplus
}
#endif

#endif
