// SPDX-License-Identifier: GPL-2.0
/*
 * MPP HEAP DebugFS Interface
 *
 * Copyright (c) 2023 Huahui Mai <huahui.mai@artinchip.com>
 */

#include <linux/debugfs.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/mm_types.h>
#include <linux/genalloc.h>

#include "mpp_heap.h"

static int mpp_count_get(void *data, u64 *val)
{
	unsigned long *p = data;

	*val = *p;

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(mpp_count_fops, mpp_count_get, NULL, "%llu\n");

static int mpp_used_get(void *data, u64 *val)
{
	struct mpp_heap *mpp = data;
	struct gen_pool *pool = mpp->pool;
	struct gen_pool_chunk *chunk;
	unsigned long used;

	rcu_read_lock();
	list_for_each_entry_rcu(chunk, &(pool)->chunks, next_chunk)
		used = bitmap_weight(chunk->bits, (int)mpp->count);
	rcu_read_unlock();
	*val = (u64)used;

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(mpp_used_fops, mpp_used_get, NULL, "%llu\n");

static int mpp_maxchunk_get(void *data, u64 *val)
{
	struct mpp_heap *mpp = data;
	struct gen_pool *pool = mpp->pool;
	unsigned long bitmap_maxno = mpp->count;
	unsigned long maxchunk = 0;
	unsigned long start, end = 0;
	struct gen_pool_chunk *chunk;

	rcu_read_lock();
	list_for_each_entry_rcu(chunk, &(pool)->chunks, next_chunk) {
		for (;;) {
			start = find_next_zero_bit(chunk->bits, bitmap_maxno, end);
			if (start >= bitmap_maxno)
				break;
			end = find_next_bit(chunk->bits, bitmap_maxno, start);
			maxchunk = max(end - start, maxchunk);
		}
	}
	rcu_read_unlock();
	*val = (u64)maxchunk;

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(mpp_maxchunk_fops, mpp_maxchunk_get, NULL, "%llu\n");

static int __init mpp_debugfs_init(void)
{
	struct mpp_heap *mpp = mpp_debugfs;
	struct gen_pool_chunk *chunk;
	struct gen_pool *pool;
	struct dentry *root;
	size_t chunk_size;
	int order;

	if (!mpp)
		return -EINVAL;

	pool = mpp->pool;
	order = pool->min_alloc_order;

	root = debugfs_create_dir("mpp", NULL);

	rcu_read_lock();
	/* MPP HEAP has only one chunk */
	list_for_each_entry_rcu(chunk, &(pool)->chunks, next_chunk) {
		chunk_size = chunk->end_addr - chunk->start_addr + 1;
		mpp->dfs_bitmap.array = (u32 *)chunk->bits;
		mpp->dfs_bitmap.n_elements = DIV_ROUND_UP(chunk_size >> order,
							  BITS_PER_BYTE * sizeof(u32));
	}
	rcu_read_unlock();

	mpp->count = chunk_size >> order;

	debugfs_create_file("count", 0444, root, &mpp->count, &mpp_count_fops);
	debugfs_create_file("used", 0444, root, mpp, &mpp_used_fops);
	debugfs_create_file("maxchunk", 0444, root, mpp, &mpp_maxchunk_fops);
	debugfs_create_u32_array("bitmap", 0444, root, &mpp->dfs_bitmap);

	return 0;
}
late_initcall(mpp_debugfs_init);
