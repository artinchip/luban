/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co., Ltd.
 *
 * Authors:  <qi.xu@artinchip.com>
 */

#ifndef __GST_MPP_ALLOCATOR_H__
#define __GST_MPP_ALLOCATOR_H__

#include <gst/gst.h>
#include "mpp_decoder.h"

G_BEGIN_DECLS

#define GST_MPP_MEMORY_TYPE "mpp"

#define GST_MPP_MEMORY_QUARK gst_mpp_memory_quark ()

#define GST_TYPE_MPP_ALLOCATOR    (gst_mpp_allocator_get_type())
#define GST_IS_MPP_ALLOCATOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_MPP_ALLOCATOR))
#define GST_MPP_ALLOCATOR(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_MPP_ALLOCATOR, GstMppAllocator))

typedef struct _GstMppMemory GstMppMemory;
typedef struct _GstMppAllocator GstMppAllocator;
typedef struct _GstMppAllocatorClass GstMppAllocatorClass;

struct _GstMppMemory
{
	GstMemory mem;
	struct mpp_frame* frame; // pointer to GstMemory.data
};

struct _GstMppAllocator
{
	GstAllocator parent;

	struct mpp_decoder* decoder;
};

struct _GstMppAllocatorClass
{
	GstAllocatorClass parent_class;
};

GstMppAllocator* gst_mpp_allocator_new(struct mpp_decoder* dec);

G_END_DECLS

#endif
