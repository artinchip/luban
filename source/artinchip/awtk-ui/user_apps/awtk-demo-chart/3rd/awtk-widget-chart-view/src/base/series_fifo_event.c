/**
 * File:   series_fifo_event.c
 * Author: AWTK Develop Team
 * Brief:  series_fifo_event.
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

#include "series_fifo_event.h"

series_fifo_set_event_t* series_fifo_set_event_cast(event_t* event) {
  return_value_if_fail(event != NULL, NULL);
  return_value_if_fail(event->type >= EVT_SERIES_FIFO_WILL_SET, NULL);
  return_value_if_fail(event->type <= EVT_SERIES_FIFO_SET, NULL);
  return_value_if_fail(event->size == sizeof(series_fifo_set_event_t), NULL);

  return (series_fifo_set_event_t*)event;
}

event_t* series_fifo_set_event_init(series_fifo_set_event_t* event, uint32_t etype, void* target) {
  return_value_if_fail(event != NULL, NULL);

  memset(event, 0x00, sizeof(*event));
  event->e = event_init(etype, target);
  event->e.size = sizeof(*event);

  return (event_t*)event;
}

series_fifo_push_event_t* series_fifo_push_event_cast(event_t* event) {
  return_value_if_fail(event != NULL, NULL);
  return_value_if_fail(event->type >= EVT_SERIES_FIFO_WILL_PUSH, NULL);
  return_value_if_fail(event->type <= EVT_SERIES_FIFO_PUSH, NULL);
  return_value_if_fail(event->size == sizeof(series_fifo_push_event_t), NULL);

  return (series_fifo_push_event_t*)event;
}

event_t* series_fifo_push_event_init(series_fifo_push_event_t* event, uint32_t etype,
                                     void* target) {
  return_value_if_fail(event != NULL, NULL);

  memset(event, 0x00, sizeof(*event));
  event->e = event_init(etype, target);
  event->e.size = sizeof(*event);

  return (event_t*)event;
}

series_fifo_pop_event_t* series_fifo_pop_event_cast(event_t* event) {
  return_value_if_fail(event != NULL, NULL);
  return_value_if_fail(event->type >= EVT_SERIES_FIFO_WILL_POP, NULL);
  return_value_if_fail(event->type <= EVT_SERIES_FIFO_POP, NULL);
  return_value_if_fail(event->size == sizeof(series_fifo_pop_event_t), NULL);

  return (series_fifo_pop_event_t*)event;
}

event_t* series_fifo_pop_event_init(series_fifo_pop_event_t* event, uint32_t etype, void* target) {
  return_value_if_fail(event != NULL, NULL);

  memset(event, 0x00, sizeof(*event));
  event->e = event_init(etype, target);
  event->e.size = sizeof(*event);

  return (event_t*)event;
}
