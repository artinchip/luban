/**
 * File:   series_fifo_event.h
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

#ifndef TK_SERIES_FIFO_EVENT_H
#define TK_SERIES_FIFO_EVENT_H

#include "tkc/event.h"
#include "tkc/object.h"

BEGIN_C_DECLS

/**
 * @enum series_fifo_event_type_t
 * series_fifo事件类型。
 */
typedef enum _series_fifo_event_type_t {
  /**
   * @const EVT_SERIES_FIFO_WILL_SET
   *
   * 即将设置元素事件。
   */
  EVT_SERIES_FIFO_WILL_SET = 0x3000,
  /**
   * @const EVT_SERIES_FIFO_SET
   *
   * 设置元素事件。
   */
  EVT_SERIES_FIFO_SET,
  /**
   * @const EVT_SERIES_FIFO_WILL_PUSH
   *
   * 即将追加元素事件。
   */
  EVT_SERIES_FIFO_WILL_PUSH,
  /**
   * @const EVT_SERIES_FIFO_PUSH
   *
   * 追加元素事件。
   */
  EVT_SERIES_FIFO_PUSH,
  /**
   * @const EVT_SERIES_FIFO_WILL_POP
   *
   * 即将弹出元素事件。
   */
  EVT_SERIES_FIFO_WILL_POP,
  /**
   * @const EVT_SERIES_FIFO_POP
   *
   * 弹出元素事件。
   */
  EVT_SERIES_FIFO_POP,
} series_fifo_event_type_t;

/**
 * @class series_fifo_set_event_t
 * @annotation ["scriptable"]
 * @parent event_t
 * 设置元素事件。
 */
typedef struct _series_fifo_set_event_t {
  event_t e;
  /**
   * @property {uint32_t} index
   * @annotation ["readable", "scriptable"]
   * 设置元素时的指定位置。
   */
  uint32_t index;
  /**
   * @property {uint32_t} nr
   * @annotation ["readable", "scriptable"]
   * 设置元素的个数。
   */
  uint32_t nr;
  /**
   * @property {void*} data
   * @annotation ["readable", "scriptable"]
   * 设置数据。
   */
  void* data;

  /*private*/
  void* ctx;
} series_fifo_set_event_t;

/**
 * @class series_fifo_push_event_t
 * @annotation ["scriptable"]
 * @parent event_t
 * 追加元素事件。
 */
typedef struct _series_fifo_push_event_t {
  event_t e;
  /**
   * @property {uint32_t} nr
   * @annotation ["readable", "scriptable"]
   * 追加元素的个数。
   */
  uint32_t nr;
  /**
   * @property {void*} data
   * @annotation ["readable", "scriptable"]
   * 追加数据。
   */
  void* data;

  /*private*/
  void* ctx;
} series_fifo_push_event_t;

/**
 * @class series_fifo_pop_event_t
 * @annotation ["scriptable"]
 * @parent event_t
 * 弹出元素事件。
 */
typedef struct _series_fifo_pop_event_t {
  event_t e;
  /**
   * @property {uint32_t} nr
   * @annotation ["readable", "scriptable"]
   * 弹出元素的个数。
   */
  uint32_t nr;

  /*private*/
  void* ctx;
} series_fifo_pop_event_t;

/**
 * @method series_fifo_set_event_cast
 * @annotation ["cast", "scriptable"]
 * 把event对象转series_fifo_event_set_t对象，主要给脚本语言使用。
 * @param {event_t*} event event对象。
 *
 * @return {series_fifo_set_event_t*} event对象。
 */
series_fifo_set_event_t* series_fifo_set_event_cast(event_t* event);

/**
 * @method series_fifo_set_event_init
 * 初始化series_fifo_set_event_t事件。
 * @param {series_fifo_set_event_t*} event event对象。
 * @param {uint32_t} etype 事件类型。
 * @param {void*} target 事件目标。
 *
 * @return {event_t*} event对象。
 */
event_t* series_fifo_set_event_init(series_fifo_set_event_t* event, uint32_t etype, void* target);

/**
 * @method series_fifo_push_event_cast
 * @annotation ["cast", "scriptable"]
 * 把event对象转series_fifo_push_event_t对象，主要给脚本语言使用。
 * @param {event_t*} event event对象。
 *
 * @return {series_fifo_push_event_t*} event对象。
 */
series_fifo_push_event_t* series_fifo_push_event_cast(event_t* event);

/**
 * @method series_fifo_push_event_init
 * 初始化series_fifo_push_event_init事件。
 * @param {series_fifo_push_event_t*} event event对象。
 * @param {uint32_t} etype 事件类型。
 * @param {void*} target 事件目标。
 *
 * @return {event_t*} event对象。
 */
event_t* series_fifo_push_event_init(series_fifo_push_event_t* event, uint32_t etype, void* target);

/**
 * @method series_fifo_pop_event_cast
 * @annotation ["cast", "scriptable"]
 * 把event对象转series_fifo_pop_event_t对象，主要给脚本语言使用。
 * @param {event_t*} event event对象。
 *
 * @return {series_fifo_pop_event_t*} event对象。
 */
series_fifo_pop_event_t* series_fifo_pop_event_cast(event_t* event);

/**
 * @method series_fifo_pop_event_init
 * 初始化series_fifo_pop_event_t事件。
 * @param {series_fifo_pop_event_t*} event event对象。
 * @param {uint32_t} etype 事件类型。
 * @param {void*} target 事件目标。
 *
 * @return {event_t*} event对象。
 */
event_t* series_fifo_pop_event_init(series_fifo_pop_event_t* event, uint32_t etype, void* target);

END_C_DECLS

#endif /*TK_se_FIFO_EVENT_H*/
