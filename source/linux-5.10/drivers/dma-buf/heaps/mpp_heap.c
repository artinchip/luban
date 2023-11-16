// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF CMA MPP heap exporter
 *
 * Copyright (C) 2022-2023 ArtInChip Technology Co., Ltd.
 * Authors:  Huahui Mai <huahui.mai@artinchip.com>
 */

#include <linux/cma.h>
#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/dma-map-ops.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/highmem.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/sched/signal.h>
#include <linux/genalloc.h>

#include "heap-helpers.h"
#include "mpp_heap.h"

#ifdef CONFIG_CMA_MPP_SIZE_MBYTES
#define CMA_MPP_SIZE_MBYTES CONFIG_CMA_MPP_SIZE_MBYTES
#else
#define CMA_MPP_SIZE_MBYTES 0
#endif

#ifdef CONFIG_MPP_DEBUGFS
struct mpp_heap *mpp_debugfs;
#endif

static const size_t pages_count __initconst =
	(size_t)CMA_MPP_SIZE_MBYTES * SZ_1M / PAGE_SIZE;

static void mpp_heap_free(struct heap_helper_buffer *buffer)
{
	struct mpp_heap *mpp_heap = dma_heap_get_drvdata(buffer->heap);
	struct page *mpp_pages = buffer->priv_virt;

	/* free page list */
	kfree(buffer->pages);
	/* release memory to gen pool */
	gen_pool_free(mpp_heap->pool, page_to_phys(mpp_pages), buffer->size);
	kfree(buffer);
}

static int mpp_heap_allocate(struct dma_heap *heap,
				unsigned long len,
				unsigned long fd_flags,
				unsigned long heap_flags)
{
	struct mpp_heap *mpp_heap = dma_heap_get_drvdata(heap);
	struct heap_helper_buffer *helper_buffer;
	struct page *mpp_pages;
	struct dma_buf *dmabuf;
	phys_addr_t addr;
	int ret = -ENOMEM;
	pgoff_t pg = 0;

	helper_buffer = kzalloc(sizeof(*helper_buffer), GFP_KERNEL);
	if (!helper_buffer)
		return -ENOMEM;

	init_heap_helper_buffer(helper_buffer, mpp_heap_free);
	helper_buffer->heap = heap;
	helper_buffer->size = len;

	helper_buffer->pagecount = len / PAGE_SIZE;
	helper_buffer->pages = kmalloc_array(helper_buffer->pagecount,
					     sizeof(*helper_buffer->pages),
					     GFP_KERNEL);
	if (!helper_buffer->pages) {
		ret = -ENOMEM;
		goto err0;
	}

	addr = gen_pool_alloc(mpp_heap->pool, len);
	if (!addr)
		goto err1;

	mpp_pages = phys_to_page(addr);

	for (pg = 0; pg < helper_buffer->pagecount; pg++)
		helper_buffer->pages[pg] = &mpp_pages[pg];

	/* create the dmabuf */
	dmabuf = heap_helper_export_dmabuf(helper_buffer, fd_flags);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		goto err1;
	}

	helper_buffer->dmabuf = dmabuf;
	helper_buffer->priv_virt = mpp_pages;

	ret = dma_buf_fd(dmabuf, fd_flags);
	if (ret < 0) {
		dma_buf_put(dmabuf);
		/* just return, as put will call release and that will free */
		return ret;
	}

	return ret;

err1:
	if (pg > 0)
		gen_pool_free(mpp_heap->pool, addr, len);
	kfree(helper_buffer->pages);
err0:
	kfree(helper_buffer);

	return ret;
}

static void mpp_heap_destroy(struct dma_heap *heap)
{
	struct mpp_heap *mpp_heap = dma_heap_get_drvdata(heap);

	gen_pool_destroy(mpp_heap->pool);
	cma_release(mpp_heap->cma, mpp_heap->cma_pages, mpp_heap->nr_pages);
}

static const struct dma_heap_ops mpp_heap_ops = {
	.allocate = mpp_heap_allocate,
	.destroy_heap = mpp_heap_destroy,
};

static int __add_gen_pool(struct mpp_heap *mpp_heap)
{
	size_t count = pages_count;
	struct page *cma_pages;
	struct gen_pool *pool;
	phys_addr_t addr;

	if (!count)
		return -EINVAL;

	cma_pages = cma_alloc(mpp_heap->cma, count, 0, false);
	if (!cma_pages)
		return -ENOMEM;

	addr = page_to_phys(cma_pages);

	mpp_heap->cma_pages = cma_pages;
	mpp_heap->nr_pages = count;

	pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!pool) {
		pr_err("Failed to create MPP gen pool\n");
		return -ENOMEM;
	}
	gen_pool_set_algo(pool, gen_pool_best_fit, NULL);

	if (gen_pool_add(pool, addr, count * PAGE_SIZE, -1)) {
		pr_err("Failed to add MPP chunks\n");
		gen_pool_destroy(pool);
		return -ENOMEM;
	}

	mpp_heap->pool = pool;

	pr_info("MPP: Reserved %d MiB at %#llx\n",
			CMA_MPP_SIZE_MBYTES, addr);

	return 0;
}

static int __add_mpp_heap(struct cma *cma, void *data)
{
	struct mpp_heap *mpp_heap;
	struct dma_heap_export_info exp_info;

	mpp_heap = kzalloc(sizeof(*mpp_heap), GFP_KERNEL);
	if (!mpp_heap)
		return -ENOMEM;
	mpp_heap->cma = cma;

	exp_info.name = "mpp";
	exp_info.ops = &mpp_heap_ops;
	exp_info.priv = mpp_heap;

	mpp_heap->heap = dma_heap_add(&exp_info);
	if (IS_ERR(mpp_heap->heap)) {
		int ret = PTR_ERR(mpp_heap->heap);

		kfree(mpp_heap);
		return ret;
	}

	if (__add_gen_pool(mpp_heap)) {
		kfree(mpp_heap);
		return -ENOMEM;
	}
#ifdef CONFIG_MPP_DEBUGFS
	mpp_debugfs = mpp_heap;
#endif
	return 0;
}

static int mpp_heap_create(void)
{
	struct cma *default_cma = dev_get_cma_area(NULL);
	int ret = 0;

	if (default_cma)
		ret = __add_mpp_heap(default_cma, NULL);

	return ret;
}
module_init(mpp_heap_create);
MODULE_LICENSE("GPL v2");

