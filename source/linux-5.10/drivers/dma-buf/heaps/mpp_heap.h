/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __MPP_HEAP_H__
#define __MPP_HEAP_H__

#include <linux/debugfs.h>

struct mpp_heap {
	struct dma_heap *heap;
	struct cma *cma;
	struct page *cma_pages;
	struct gen_pool *pool;
#ifdef CONFIG_MPP_DEBUGFS
	struct debugfs_u32_array dfs_bitmap;
	unsigned long count;
#endif
	size_t nr_pages;
};

#ifdef CONFIG_MPP_DEBUGFS
extern struct mpp_heap *mpp_debugfs;
#endif

#endif
