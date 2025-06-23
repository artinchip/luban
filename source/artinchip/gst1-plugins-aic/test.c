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

GType pm_dlist_get_type();
#define PM_TYPE_DLIST ( pm_dlist_get_type() )

typedef struct _PMDList PMDList;
struct _PMDList {
	GObject obj;
	PMDList *prev;
	PMDList *next;
};

typedef struct _PMDListClass PMDListClass;
struct _PMDListClass {
	GObjectClass parent_class;
};


G_DEFINE_TYPE (PMDList, pm_dlist, G_TYPE_OBJECT);

static void pm_dlist_init(PMDList* list)
{
	printf("instance init\n");
	list->prev = NULL;
	list->next = NULL;
}

static void
pm_dlist_finalize(GObject *object)
{
	printf("===> class finalize\n");
}

static void pm_dlist_class_init(PMDListClass* kclass)
{
	printf("class init\n");
	GObjectClass *object_class = G_OBJECT_CLASS(kclass);

	object_class->finalize       = GST_DEBUG_FUNCPTR(pm_dlist_finalize);
}

int main()
{
	#if 0
	PMDList* list;

	list = g_object_new(PM_TYPE_DLIST, NULL);
	printf("new list\n");
	g_object_unref(list);
	printf("unref list\n");

	list = g_object_new(PM_TYPE_DLIST, NULL);
	g_object_ref(list);
	g_object_ref(list);
	g_object_ref(list);

	g_object_unref(list);

	GObject *obj = G_OBJECT(list);
	int refcnt = g_atomic_int_get(&obj->ref_count);
	printf("refcnt: %d\n", refcnt);

	if (G_IS_OBJECT(list)) {
		printf("it is a object \n");
	}
	#endif

	printf("start\n");

	GHashTable *h;
	h = g_hash_table_new(NULL, NULL);

	int key = 1;
	int val = 2;
	g_hash_table_replace(h, (gpointer)key, (gpointer)val);

	key = 5;
	val = 6;
	g_hash_table_replace(h, (gpointer)key, (gpointer)val);

	printf("hash: key=1, val: %d\n", g_hash_table_lookup(h, 1));
	printf("hash: key=2, val: %d\n", g_hash_table_lookup(h, 2));
	g_hash_table_destroy(h);
	return 0;
}
