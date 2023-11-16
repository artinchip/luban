/**
 * File:   series_fifo_default.c
 * Author: AWTK Develop Team
 * Brief:  series_fifo_default.
 *
 * Copyright (c) 2018 - 2021  Guangzhou ZHIYUAN Electronics Co.,Ltd.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * License file for more details.
 *
 */

/**
 * History:
 * ================================================================
 * 2021-06-22 Liu YuXin <liuyuxin@zlg.cn> created
 *
 */

#include "tkc/mem.h"
#include "series_fifo_default.h"

static void* series_fifo_default_get(object_t* obj, uint32_t index) {
  series_fifo_default_t* fifo = SERIES_FIFO_DEFAULT(obj);
  return_value_if_fail(fifo != NULL && fifo->buffer != NULL, NULL);

  return (void*)(fifo->buffer + fifo->unit_size * index);
}

static ret_t series_fifo_default_set(object_t* obj, uint32_t index, const void* data, uint32_t nr) {
  series_fifo_default_t* fifo = SERIES_FIFO_DEFAULT(obj);
  uint8_t* start = (uint8_t*)(data);
  uint8_t* elem = (uint8_t*)(fifo->buffer + fifo->unit_size * index);

  if (nr > fifo->capacity) {
    if (start != NULL) {
      start += (nr - fifo->capacity) * fifo->unit_size;
    }
    nr = fifo->capacity;
  }

  if (index + nr <= fifo->capacity) {
    if (start != NULL) {
      memcpy(elem, start, fifo->unit_size * nr);
    } else {
      memset(elem, 0x00, fifo->unit_size * nr);
    }
  } else {
    uint32_t part = fifo->capacity - index;
    if (start != NULL) {
      memcpy(elem, start, fifo->unit_size * part);
      memcpy(fifo->buffer, start + fifo->unit_size * part, fifo->unit_size * (nr - part));
    } else {
      memset(elem, 0x00, fifo->unit_size * part);
      memset(fifo->buffer, 0x00, fifo->unit_size * (nr - part));
    }
  }

  return RET_OK;
}

static object_t* series_fifo_default_part_clone(object_t* obj, uint32_t index, uint32_t nr) {
  object_t* clone = NULL;
  series_fifo_default_t* fifo = SERIES_FIFO_DEFAULT(obj);
  return_value_if_fail(fifo != NULL && fifo->buffer != NULL, NULL);

  clone = series_fifo_default_create(nr, fifo->unit_size);
  series_fifo_default_t* fifo_clone = SERIES_FIFO_DEFAULT(clone);

  if (fifo_clone) {
    uint8_t* data = fifo_clone->buffer;

    for (int32_t i = 0; i < nr; i++) {
      void* iter = series_fifo_default_get(obj, index + i);
      memcpy(data + i * fifo->unit_size, iter, fifo->unit_size);
    }

    fifo_clone->size = nr;
    fifo_clone->cursor = fifo_clone->cursor + nr - 1;
  }

  return clone;
}

static int series_fifo_default_compare(object_t* obj, const void* a, const void* b) {
  series_fifo_default_t* fifo = SERIES_FIFO_DEFAULT(obj);
  return_value_if_fail(obj != NULL, RET_BAD_PARAMS);

  return memcmp(a, b, fifo->unit_size);
}

static ret_t series_fifo_default_set_capacity(object_t* obj, uint32_t capacity) {
  series_fifo_default_t* fifo = SERIES_FIFO_DEFAULT(obj);
  return_value_if_fail(fifo != NULL && fifo->buffer != NULL, RET_BAD_PARAMS);

  TKMEM_FREE(fifo->buffer);

  fifo->buffer = TKMEM_ZALLOCN(uint8_t, (fifo->unit_size * capacity));
  return_value_if_fail(fifo->buffer != NULL, RET_OOM);

  fifo->size = 0;
  fifo->cursor = 0;
  fifo->capacity = capacity;

  return RET_OK;
}

static ret_t series_fifo_default_on_destroy(object_t* obj) {
  series_fifo_default_t* fifo = SERIES_FIFO_DEFAULT(obj);
  return_value_if_fail(fifo != NULL, RET_BAD_PARAMS);

  TKMEM_FREE(fifo->buffer);

  return RET_OK;
}

static ret_t series_fifo_default_get_prop(object_t* obj, const char* name, value_t* v) {
  series_fifo_default_t* fifo = SERIES_FIFO_DEFAULT(obj);
  return_value_if_fail(fifo != NULL, RET_BAD_PARAMS);

  if (tk_str_eq(name, SERIES_FIFO_PROP_CAPACITY)) {
    value_set_uint32(v, fifo->capacity);
    return RET_OK;
  } else if (tk_str_eq(name, SERIES_FIFO_PROP_SIZE)) {
    value_set_uint32(v, fifo->size);
    return RET_OK;
  } else if (tk_str_eq(name, SERIES_FIFO_PROP_CURSOR)) {
    value_set_uint32(v, fifo->cursor);
    return RET_OK;
  } else if (tk_str_eq(name, SERIES_FIFO_PROP_UNIT_SIZE)) {
    value_set_uint32(v, fifo->unit_size);
    return RET_OK;
  }

  return RET_NOT_FOUND;
}

static ret_t series_fifo_default_set_prop(object_t* obj, const char* name, const value_t* v) {
  series_fifo_default_t* fifo = SERIES_FIFO_DEFAULT(obj);
  return_value_if_fail(fifo != NULL, RET_BAD_PARAMS);

  if (tk_str_eq(name, SERIES_FIFO_PROP_CAPACITY)) {
    return RET_OK;
  } else if (tk_str_eq(name, SERIES_FIFO_PROP_SIZE)) {
    fifo->size = value_uint32(v);
    return RET_OK;
  } else if (tk_str_eq(name, SERIES_FIFO_PROP_CURSOR)) {
    fifo->cursor = value_uint32(v);
    return RET_OK;
  }

  return RET_NOT_FOUND;
}

static const object_vtable_t s_object_vtable = {.type = "series_fifo_default",
                                                .desc = "series_fifo_default",
                                                .size = sizeof(series_fifo_default_t),
                                                .is_collection = FALSE,
                                                .on_destroy = series_fifo_default_on_destroy,
                                                .get_prop = series_fifo_default_get_prop,
                                                .set_prop = series_fifo_default_set_prop};

static const series_fifo_vtable_t s_series_fifo_vtable = {
    .part_clone = series_fifo_default_part_clone,
    .get = series_fifo_default_get,
    .set = series_fifo_default_set,
    .compare = series_fifo_default_compare,
    .set_capacity = series_fifo_default_set_capacity,
};

object_t* series_fifo_default_create(uint32_t capacity, uint32_t unit_size) {
  object_t* obj = NULL;
  series_fifo_t* series_fifo = NULL;
  series_fifo_default_t* fifo = NULL;
  return_value_if_fail(capacity > 0 && unit_size > 0, NULL);

  obj = object_create(&s_object_vtable);
  series_fifo = SERIES_FIFO(obj);
  fifo = SERIES_FIFO_DEFAULT(obj);
  return_value_if_fail(obj != NULL && series_fifo != NULL && fifo != NULL, NULL);

  series_fifo->vt = &s_series_fifo_vtable;

  fifo->buffer = TKMEM_ZALLOCN(uint8_t, (unit_size * capacity));
  if (fifo->buffer == NULL) {
    OBJECT_UNREF(obj);
    return NULL;
  }

  fifo->capacity = capacity;
  fifo->unit_size = unit_size;
  fifo->cursor = 0;
  fifo->size = 0;

  return obj;
}