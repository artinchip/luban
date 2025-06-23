/*
 * Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: qi.xu@artinchip.com
 *  Desc: virtual memory allocator
 */

#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mpp_mem.h"

#define DEBUG_MEM  (0)

#if DEBUG_MEM
#include <pthread.h>
#define MAX_NUM  20
struct memory_node {
	void* addr;
	int size;
	int used;
};

static pthread_mutex_t 	g_mem_mutex = PTHREAD_MUTEX_INITIALIZER;
int 			g_mem_count = 0;
int 			g_mem_total_size = 0;
struct memory_node	g_mem_info[MAX_NUM] = {{0,0,0}};

#endif

void *mpp_alloc(size_t size)
{
	void *ptr = NULL;

	ptr = malloc(size);

#if DEBUG_MEM
	int i = 0;
	pthread_mutex_lock(&g_mem_mutex);
	g_mem_count ++;
	g_mem_total_size += size;

	for(i=0; i<MAX_NUM; i++) {
		if(g_mem_info[i].used == 0) {
			g_mem_info[i].addr = ptr;
			g_mem_info[i].size = size;
			g_mem_info[i].used = 1;
			logi("alloc memory, addr: %x, %d", ptr, size);
			break;
		}
	}
	if(i == MAX_NUM) {
		loge("exceed max number");
	}
	pthread_mutex_unlock(&g_mem_mutex);
#endif
	return ptr;
}

void mpp_free(void *ptr)
{
	if(ptr == NULL)
		return;

	free(ptr);

#if DEBUG_MEM
	int i;
	pthread_mutex_lock(&g_mem_mutex);
	for(i=0; i<MAX_NUM; i++) {
		if(g_mem_info[i].used && g_mem_info[i].addr == ptr) {
			g_mem_count--;
			g_mem_total_size -= g_mem_info[i].size;
			logi("free memory, addr: %x, %d", ptr, g_mem_info[i].size);
			break;
		}
	}
	if(i == MAX_NUM) {
		loge("cannot find this memory");
	}
	logi("total memory size: %d", g_mem_total_size);
	pthread_mutex_unlock(&g_mem_mutex);
#endif
}

void *mpp_realloc(void *ptr,size_t size)
{
	return realloc(ptr,size);
}
