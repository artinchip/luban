/*
 * Copyright (C) 2024 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: artinchip
 */
#include <stdlib.h>
#include <stdio.h>
#include <glib-object.h>
#include <gst/gst.h>
#include <gst/gstmemory.h>
#include <gst/gstminiobject.h>
#include "gstmppallocator.h"

int main()
{
	printf("Compile time: %s\n", __TIME__);
	printf("start\n");

	GstAllocator *alloc = gst_allocator_find(GST_ALLOCATOR_SYSMEM);

	printf("alloc: %p\n", alloc);
	GstBuffer *mem = gst_buffer_new_and_alloc(alloc, 100, NULL);
	GstMiniObject *obj = GST_MINI_OBJECT(mem);
	int refcnt = obj->refcount;
	printf("gst_buffer_new_allocate refcnt: %d\n", refcnt);

	gst_buffer_ref(mem);
	refcnt = obj->refcount;
	printf("gst_buffer_ref refcnt: %d\n", refcnt);

	gst_buffer_unref(mem);
	refcnt = obj->refcount;
	printf("gst_buffer_unref refcnt: %d\n", refcnt);

	return 0;
}
