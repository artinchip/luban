/**
 * File:   series_fifo.c
 * Author: AWTK Develop Team
 * Brief:  series_fifo.
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

#include "series_fifo.h"

static bool_t series_fifo_is_valid(object_t* obj, uint32_t index) {
  series_fifo_t* series_fifo = SERIES_FIFO(obj);
  return_value_if_fail(series_fifo != NULL && series_fifo->vt != NULL, FALSE);
  return index >= 0 && index < SERIES_FIFO_GET_SIZE(obj);
}

static uint32_t series_fifo_to_abs_index(object_t* obj, uint32_t index) {
  return_value_if_fail(obj != NULL, 0);

  uint32_t size = SERIES_FIFO_GET_SIZE(obj);
  uint32_t cursor = SERIES_FIFO_GET_CURSOR(obj);
  uint32_t capacity = SERIES_FIFO_GET_CAPACITY(obj);

  return (cursor - size + 1 + capacity + index) % capacity;
}

void* series_fifo_get(object_t* obj, uint32_t index) {
  series_fifo_t* series_fifo = SERIES_FIFO(obj);
  return_value_if_fail(series_fifo_is_valid(obj, index) && SERIES_FIFO_GET_SIZE(obj) > 0, NULL);

  if (series_fifo->vt->get) {
    uint32_t abs_index = series_fifo_to_abs_index(obj, index);
    return series_fifo->vt->get(obj, abs_index);
  }

  return NULL;
}

ret_t series_fifo_set(object_t* obj, uint32_t index, const void* data, uint32_t nr) {
  ret_t ret = RET_NOT_IMPL;
  series_fifo_t* series_fifo = SERIES_FIFO(obj);
  uint32_t size, capacity, unit_size;
  return_value_if_fail(series_fifo_is_valid(obj, index) || index == 0, RET_BAD_PARAMS);

  if (series_fifo->vt->set) {
    size = SERIES_FIFO_GET_SIZE(obj);
    capacity = SERIES_FIFO_GET_CAPACITY(obj);
    unit_size = SERIES_FIFO_GET_UNIT_SIZE(obj);

    /* 使用空元素占位，并去除多余数据 */
    if (index + nr > size) {
      bool_t save = series_fifo->block_event;
      series_fifo_set_block_event(obj, TRUE);
      series_fifo_npush(obj, NULL, index + nr - size);
      series_fifo_set_block_event(obj, save);

      if (nr >= capacity) {
        index = 0;
        data = (void*)((uint8_t*)data + unit_size * (nr - capacity));
        nr = capacity;
      }
    }

    if (!series_fifo->block_event) {
      series_fifo_set_event_t will_set_evt;
      series_fifo_set_event_init(&will_set_evt, EVT_SERIES_FIFO_WILL_SET, (void*)obj);
      will_set_evt.index = index;
      will_set_evt.data = (void*)data;
      will_set_evt.nr = nr;
      ret = emitter_dispatch(EMITTER(obj), (event_t*)(&will_set_evt));
    }

    if (ret == RET_STOP) {
      ret = RET_OK;
    } else {
      uint32_t abs_index = series_fifo_to_abs_index(obj, index);
      ret = series_fifo->vt->set(obj, abs_index, data, nr);

      if (!series_fifo->block_event) {
        series_fifo_set_event_t set_evt;
        series_fifo_set_event_init(&set_evt, EVT_SERIES_FIFO_SET, (void*)obj);
        set_evt.index = index;
        set_evt.data = (void*)data;
        set_evt.nr = nr;
        emitter_dispatch(EMITTER(obj), (event_t*)(&set_evt));
      }
    }
  }

  return ret;
}

int series_fifo_compare(object_t* obj, const void* a, const void* b) {
  series_fifo_t* series_fifo = SERIES_FIFO(obj);
  return_value_if_fail(series_fifo != NULL && series_fifo->vt != NULL, -1);

  if (series_fifo->vt->compare) {
    return series_fifo->vt->compare(obj, a, b);
  }

  return -1;
}

object_t* series_fifo_part_clone(object_t* obj, uint32_t index, uint32_t nr) {
  series_fifo_t* series_fifo = SERIES_FIFO(obj);
  return_value_if_fail(series_fifo_is_valid(obj, index), NULL);
  return_value_if_fail(series_fifo_is_valid(obj, index + nr - 1), NULL);

  if (series_fifo->vt->part_clone) {
    return series_fifo->vt->part_clone(obj, index, nr);
  }

  return NULL;
}

ret_t series_fifo_set_reverse(object_t* obj, uint32_t index, const void* data, uint32_t nr) {
  return_value_if_fail(obj != NULL, RET_BAD_PARAMS);

  uint32_t size = SERIES_FIFO_GET_SIZE(obj);
  index = size >= index + 1 ? (size - index - 1) : 0;

  return series_fifo_set(obj, index, data, nr);
}

void* series_fifo_find(object_t* obj, void* ctx) {
  uint32_t i = 0;
  return_value_if_fail(obj != NULL, NULL);

  for (i = 0; i < SERIES_FIFO_GET_SIZE(obj); i++) {
    void* iter = series_fifo_get(obj, i);
    if (series_fifo_compare(obj, iter, ctx) == 0) {
      return iter;
    }
  }

  return NULL;
}

int series_fifo_find_index(object_t* obj, void* ctx) {
  uint32_t i = 0;
  return_value_if_fail(obj != NULL, -1);

  for (i = 0; i < SERIES_FIFO_GET_SIZE(obj); i++) {
    void* iter = series_fifo_get(obj, i);
    if (series_fifo_compare(obj, iter, ctx) == 0) {
      return i;
    }
  }

  return -1;
}

ret_t series_fifo_push(object_t* obj, const void* data) {
  return_value_if_fail(obj != NULL, RET_BAD_PARAMS);

  return series_fifo_npush(obj, data, 1);
}

void* series_fifo_pop(object_t* obj) {
  return_value_if_fail(obj != NULL, NULL);

  void* elem = series_fifo_get(obj, 0);
  return_value_if_fail(elem != NULL, NULL);

  series_fifo_npop(obj, 1);

  return elem;
}

ret_t series_fifo_npush(object_t* obj, const void* data, uint32_t nr) {
  ret_t ret = RET_NOT_IMPL;
  series_fifo_t* series_fifo = SERIES_FIFO(obj);
  return_value_if_fail(series_fifo != NULL && series_fifo->vt != NULL, RET_BAD_PARAMS);

  if (series_fifo->vt->set) {
    uint32_t head_index;
    uint32_t size = SERIES_FIFO_GET_SIZE(obj);
    uint32_t cursor = SERIES_FIFO_GET_CURSOR(obj);
    uint32_t capacity = SERIES_FIFO_GET_CAPACITY(obj);

    if (!series_fifo->block_event) {
      series_fifo_push_event_t will_push_evt;
      series_fifo_push_event_init(&will_push_evt, EVT_SERIES_FIFO_WILL_PUSH, (void*)obj);
      will_push_evt.nr = nr;
      will_push_evt.data = (void*)data;
      ret = emitter_dispatch(EMITTER(obj), (event_t*)(&will_push_evt));
    }

    if (ret == RET_STOP) {
      ret = RET_OK;
    } else {
      if (size) {
        cursor = (cursor + nr) % capacity;
      } else {
        cursor = (cursor + nr - 1) % capacity;
      }

      size = tk_min(size + nr, capacity);
      object_set_prop_uint32(obj, SERIES_FIFO_PROP_SIZE, size);
      object_set_prop_uint32(obj, SERIES_FIFO_PROP_CURSOR, cursor);

      head_index = (cursor - tk_min(nr, capacity) + 1 + capacity) % capacity;
      ret = series_fifo->vt->set(obj, head_index, data, nr);

      if (!series_fifo->block_event) {
        series_fifo_push_event_t push_evt;
        series_fifo_push_event_init(&push_evt, EVT_SERIES_FIFO_PUSH, (void*)obj);
        push_evt.nr = nr;
        push_evt.data = (void*)data;
        emitter_dispatch(EMITTER(obj), (event_t*)(&push_evt));
      }
    }
  }

  return ret;
}

ret_t series_fifo_npop(object_t* obj, uint32_t nr) {
  ret_t ret = RET_OK;
  series_fifo_t* series_fifo = SERIES_FIFO(obj);
  return_value_if_fail(series_fifo != NULL && series_fifo->vt != NULL, RET_BAD_PARAMS);

  uint32_t size = SERIES_FIFO_GET_SIZE(obj);
  return_value_if_fail(nr > 0 && nr <= size, RET_BAD_PARAMS);

  if (!series_fifo->block_event) {
    series_fifo_pop_event_t will_pop_evt;
    series_fifo_pop_event_init(&will_pop_evt, EVT_SERIES_FIFO_WILL_POP, (void*)obj);
    will_pop_evt.nr = nr;
    ret = emitter_dispatch(EMITTER(obj), (event_t*)(&will_pop_evt));
  }

  if (ret == RET_STOP) {
    ret = RET_OK;
  } else {
    object_set_prop_uint32(obj, SERIES_FIFO_PROP_SIZE, size - nr);

    if (!series_fifo->block_event) {
      series_fifo_pop_event_t pop_evt;
      series_fifo_pop_event_init(&pop_evt, EVT_SERIES_FIFO_POP, (void*)obj);
      pop_evt.nr = nr;
      emitter_dispatch(EMITTER(obj), (event_t*)(&pop_evt));
    }
  }

  return ret;
}

ret_t series_fifo_clear(object_t* obj) {
  ret_t ret = RET_OK;
  series_fifo_t* series_fifo = SERIES_FIFO(obj);
  return_value_if_fail(series_fifo != NULL && series_fifo->vt != NULL, RET_BAD_PARAMS);

  uint32_t size = SERIES_FIFO_GET_SIZE(obj);

  if (!series_fifo->block_event) {
    series_fifo_pop_event_t will_pop_evt;
    series_fifo_pop_event_init(&will_pop_evt, EVT_SERIES_FIFO_WILL_POP, (void*)obj);
    will_pop_evt.nr = size;
    ret = emitter_dispatch(EMITTER(obj), (event_t*)(&will_pop_evt));
  }
  if (ret == RET_STOP) {
    ret = RET_OK;
  } else {
    object_set_prop_uint32(obj, SERIES_FIFO_PROP_SIZE, 0);
    object_set_prop_uint32(obj, SERIES_FIFO_PROP_CURSOR, 0);

    if (!series_fifo->block_event) {
      series_fifo_pop_event_t pop_evt;
      series_fifo_pop_event_init(&pop_evt, EVT_SERIES_FIFO_POP, (void*)obj);
      pop_evt.nr = size;
      emitter_dispatch(EMITTER(obj), (event_t*)(&pop_evt));
    }
  }

  return ret;
}

ret_t series_fifo_set_capacity(object_t* obj, uint32_t capacity) {
  ret_t ret = RET_NOT_IMPL;
  series_fifo_t* series_fifo = SERIES_FIFO(obj);
  return_value_if_fail(series_fifo != NULL && series_fifo->vt != NULL, RET_BAD_PARAMS);

  if (series_fifo->vt->set_capacity) {
    ret = series_fifo->vt->set_capacity(obj, capacity);
  }

  return ret;
}

ret_t series_fifo_set_block_event(object_t* obj, bool_t block_event) {
  series_fifo_t* series_fifo = SERIES_FIFO(obj);
  return_value_if_fail(series_fifo != NULL, RET_BAD_PARAMS);

  series_fifo->block_event = block_event;

  return RET_OK;
}
