/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  <qi.xu@artinchip.com>
 */

#include "gstmppallocator.h"

GST_DEBUG_CATEGORY_STATIC (gst_mpp_allocator_debug_category);
#define GST_CAT_DEFAULT gst_mpp_allocator_debug_category

G_DEFINE_TYPE(GstMppAllocator, gst_mpp_allocator, GST_TYPE_ALLOCATOR);

GQuark gst_mpp_memory_quark (void)
{
	static GQuark quark = 0;

	if (quark == 0)
		quark = g_quark_from_static_string ("GstMppMemory");

	return quark;
}

static void gst_mpp_allocator_init (GstMppAllocator * allocator)
{
	GstAllocator *alloc = GST_ALLOCATOR_CAST (allocator);

	alloc->mem_type = GST_MPP_MEMORY_TYPE;

	alloc->mem_map = NULL;
	alloc->mem_unmap = NULL;
	alloc->mem_share = NULL;

	GST_OBJECT_FLAG_SET (allocator, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);
}

static void gst_mpp_allocator_free (GstAllocator * alloc, GstMemory * mem)
{
	GstMppAllocator *allocator = (GstMppAllocator*) alloc;
	GstMppMemory *mpp_mem = (GstMppMemory *)mem;
	struct mpp_frame *mframe = mpp_mem->frame;

	mpp_decoder_put_frame(allocator->decoder, mframe);
	GST_DEBUG_OBJECT(allocator, "return mpp_frame (%d)", mframe->id);

	free(mpp_mem);
}

static GstMemory *gst_mpp_allocator_alloc (GstAllocator *alloc, gsize size,
    GstAllocationParams * params)
{
	int slice_size = 0;
	guint8 *data;
	GstMppMemory *mem = NULL;
	GstMppAllocator *allocator = (GstMppAllocator*) alloc;
	if (size != sizeof(struct mpp_frame)) {
		GST_ERROR_OBJECT(allocator, "memory size(%lu) is not right", size);
		return NULL;
	}

	slice_size = sizeof(GstMppMemory) + size;

	mem = (GstMppMemory*)malloc(slice_size);
	data = (guint8 *) mem + sizeof (GstMppMemory);
	mem->frame = (struct mpp_frame*)data;

	gst_memory_init (GST_MEMORY_CAST (mem),
      		0, alloc, NULL, size, 0, 0, size);

	GST_DEBUG_OBJECT(allocator, "alloc mpp size(%lu), ref_count: %d",
		size, GST_MINI_OBJECT_REFCOUNT_VALUE(mem));

	return (GstMemory*)mem;
}

struct mpp_frame *gst_get_mpp_frame_from_mem(GstMppAllocator * allocator, GstMemory * mem)
{
	GstMppMemory *mpp_mem = (GstMppMemory*)mem;
	return mpp_mem->frame;
}

GstMppAllocator* gst_mpp_allocator_new(struct mpp_decoder* dec)
{
	GstMppAllocator *allocator;

	allocator = g_object_new (gst_mpp_allocator_get_type (), NULL);
	allocator->decoder = dec;

	return allocator;
}

static void gst_mpp_allocator_finalize (GObject * object)
{
	G_OBJECT_CLASS (gst_mpp_allocator_parent_class)->finalize (object);
}

static void gst_mpp_allocator_class_init (GstMppAllocatorClass * klass)
{
  GObjectClass *object_class;
  GstAllocatorClass *allocator_class;

  object_class = (GObjectClass *) klass;
  allocator_class = (GstAllocatorClass *) klass;

  object_class->finalize = gst_mpp_allocator_finalize;
  allocator_class->alloc = gst_mpp_allocator_alloc;
  allocator_class->free = gst_mpp_allocator_free;
}
