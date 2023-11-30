/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Zequan Liang <zequan.liang@artinchip.com>
 */

#ifndef _ASSET_LIST_H_
#define _ASSET_LIST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>

struct asset_list;

struct asset_list {
  struct asset_list *next;
  struct asset_list *prev;
};

static inline void asset_list_init(struct asset_list *list) {
  list->next = list;
  list->prev = list;
}

static inline void asset_list_add_head(struct asset_list *elem,
                                       struct asset_list *head) {
  struct asset_list *prev = head;
  struct asset_list *next = head->next;

  assert(elem != NULL);
  assert(head != NULL);

  next->prev = elem;
  elem->next = next;
  elem->prev = prev;
  prev->next = elem;
}

static inline void asset_list_add_tail(struct asset_list *elem,
                                       struct asset_list *head) {
  struct asset_list *prev = head->prev;
  struct asset_list *next = head;

  assert(elem != NULL);
  assert(head != NULL);

  next->prev = elem;
  elem->next = next;
  elem->prev = prev;
  prev->next = elem;
}

static inline void asset_list_del(struct asset_list *elem) {
  struct asset_list *prev = elem->prev;
  struct asset_list *next = elem->next;

  next->prev = prev;
  prev->next = next;
  elem->next = NULL;
  elem->prev = NULL;
}

static inline void asset_list_del_init(struct asset_list *entry) {
  struct asset_list *prev = entry->prev;
  struct asset_list *next = entry->next;

  next->prev = prev;
  prev->next = next;
  entry->next = entry;
  entry->prev = entry;
}

static inline int asset_list_empty(struct asset_list *head) {
  return head->next == head;
}

#define asset_offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#ifndef container_of
#define container_of(ptr, type, member) ( { \
const typeof( ((type *)0)->member ) *__mptr = (ptr); \
(type *)( (char *)__mptr - asset_offsetof(type,member) ); } )
#endif

#define asset_list_entry(ptr, type, member) \
  container_of(ptr, type, member)

#define asset_list_first_entry(ptr, type, member) \
  asset_list_entry((ptr)->next, type, member)

#define asset_list_first_entry_or_null(ptr, type, member) ({ \
  struct asset_list *head__ = (ptr); \
  struct asset_list *pos__ = head__->next; \
  pos__ != head__ ? asset_list_entry(pos__, type, member) : NULL; \
})

#define asset_list_next_entry(pos, member) \
  asset_list_entry((pos)->member.next, typeof(*(pos)), member)

#define asset_list_entry_is_head(pos, head, member)				\
  (&pos->member == (head))

#define asset_list_for_each_entry(pos, head, member)				\
  for (pos = asset_list_first_entry(head, typeof(*pos), member);	\
    !asset_list_entry_is_head(pos, head, member);			\
      pos = asset_list_next_entry(pos, member))

#define asset_list_for_each_entry_safe(pos, n, head, member)			\
  for (pos = asset_list_first_entry(head, typeof(*pos), member),	\
    n = asset_list_next_entry(pos, member);			\
      !asset_list_entry_is_head(pos, head, member); 			\
      pos = n, n = asset_list_next_entry(n, member))

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _ASSET_LIST_H_ */
