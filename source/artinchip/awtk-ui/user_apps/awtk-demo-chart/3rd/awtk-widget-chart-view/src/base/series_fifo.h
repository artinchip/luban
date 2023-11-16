/**
 * File:   series_fifo.h
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

#ifndef TK_SERIES_FIFO_H
#define TK_SERIES_FIFO_H

#include "tkc/object.h"
#include "series_data.h"
#include "series_fifo_event.h"

BEGIN_C_DECLS

struct _series_fifo_t;
typedef struct _series_fifo_t series_fifo_t;

typedef void* (*series_fifo_get_t)(object_t* obj, uint32_t index);
typedef ret_t (*series_fifo_set_t)(object_t* obj, uint32_t index, const void* data, uint32_t nr);
typedef int (*series_fifo_compare_t)(object_t* obj, const void* a, const void* b);
typedef object_t* (*series_fifo_part_clone_t)(object_t* obj, uint32_t index, uint32_t nr);
typedef ret_t (*series_fifo_set_capacity_t)(object_t* obj, uint32_t capacity);

typedef struct _series_fifo_vtable_t {
  series_fifo_get_t get;
  series_fifo_set_t set;
  series_fifo_compare_t compare;
  series_fifo_part_clone_t part_clone;
  series_fifo_set_capacity_t set_capacity;
} series_fifo_vtable_t;

/**
 * @class series_fifo_t
 * @parent object_t
 *
 * FIFO，先进先出队列，环形缓存。
 * 可以使用 series_fifo_default_t 实例化该类。
 *
 */
struct _series_fifo_t {
  object_t obj;

  /**
   * @property {bool_t} block_event
   * @annotation ["readable","writable"]
   * 阻止分发 series_fifo_event 事件。
   */
  bool_t block_event;
  /**
   * @property {series_fifo_vtable_t} vt
   * @annotation ["readable"]
   * 虚函数表。
   */
  const series_fifo_vtable_t* vt;
};

/**
 * @method series_fifo_part_clone
 * clone部分。
 *
 * @param {object_t*} obj series_fifo对象。
 * @param {uint32_t*} index 被clone元素在FIFO中的位置。
 * @param {uint32_t*} nr 被clone元素的数量。
 *
 * @return {object_t*} 返回clone的对象。
 */
object_t* series_fifo_part_clone(object_t* obj, uint32_t index, uint32_t nr);

/**
 * @method series_fifo_get
 * 返回特定位置的元素。
 *
 * @param {object_t*} obj series_fifo对象。
 * @param {uint32_t} index 元素在FIFO中的位置。
 *
 * @return {void*} 如果找到，返回特定位置的元素，否则返回NULL。
 */
void* series_fifo_get(object_t* obj, uint32_t index);

/**
 * @method series_fifo_set
 * 设置特定位置开始的多个元素。
 *
 * @param {object_t*} obj series_fifo对象。
 * @param {uint32_t} index 元素在FIFO中的位置。
 * @param {const void*} data 元素数据。
 * @param {uint32_t} nr 元素数量。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t series_fifo_set(object_t* obj, uint32_t index, const void* data, uint32_t nr);

/**
 * @method series_fifo_set_reverse
 * 设置特定位置开始的多个元素（反向）。
 *
 * @param {object_t*} obj series_fifo对象。
 * @param {uint32_t} index 元素在FIFO中的位置。
 * @param {const void*} data 元素数据。
 * @param {uint32_t} nr 元素数量。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t series_fifo_set_reverse(object_t* obj, uint32_t index, const void* data, uint32_t nr);

/**
 * @method series_fifo_compare
 * 比较两个元素。
 *
 * @param {object_t*} obj series_fifo对象。
 * @param {const void*} a 元素a。
 * @param {const void*} b 元素b。
 *
 * @return {int} 两元素相等返回0。
 */
int series_fifo_compare(object_t* obj, const void* a, const void* b);

/**
 * @method series_fifo_find
 * 查找第一个满足条件的元素。
 *
 * @param {object_t*} obj series_fifo对象。
 * @param {void*} ctx 比较函数的上下文。
 *
 * @return {void*} 如果找到，返回满足条件的对象，否则返回NULL。
 */
void* series_fifo_find(object_t* obj, void* ctx);

/**
 * @method series_fifo_find_index
 * 查找第一个满足条件的元素，并返回位置。
 *
 * @param {object_t*} obj series_fifo对象。
 * @param {void*} ctx 比较函数的上下文。
 *
 * @return {int} 如果找到，返回满足条件的对象的位置，否则返回-1。
 */
int series_fifo_find_index(object_t* obj, void* ctx);

/**
 * @method series_fifo_push
 * 在尾巴追加一个元素。
 *
 * @param {object_t*} obj series_fifo对象。
 * @param {const void*} data 待追加的元素。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t series_fifo_push(object_t* obj, const void* data);

/**
 * @method series_fifo_pop
 * 弹出第一个元素。
 *
 * @param {object_t*} obj series_fifo对象。
 *
 * @return {void*} 成功返回第一个元素，失败返回NULL。
 */
void* series_fifo_pop(object_t* obj);

/**
 * @method series_fifo_npush
 * 在尾巴追加多个元素。
 *
 * @param {object_t*} obj series_fifo对象。
 * @param {const void*} data 待追加的元素。
 * @param {uint32_t} nr 待追加的元素个数。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t series_fifo_npush(object_t* obj, const void* data, uint32_t nr);

/**
 * @method series_fifo_npop
 * 弹出开头多个元素。
 *
 * @param {object_t*} obj series_fifo对象。
 * @param {uint32_t} nr 待弹出的元素个数。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t series_fifo_npop(object_t* obj, uint32_t nr);

/**
 * @method series_fifo_clear
 * 清除全部元素。
 *
 * @param {object_t*} obj series_fifo对象。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t series_fifo_clear(object_t* obj);

/**
 * @method series_fifo_set_capacity
 * 设置FIFO容量（会导致FIFO被清空）。
 *
 * @param {object_t*} obj series_fifo对象。
 * @param {uint32_t} capacity 容量。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t series_fifo_set_capacity(object_t* obj, uint32_t capacity);

/**
 * @method series_fifo_set_block_event
 * 设置 block_event 属性。
 *
 * @param {object_t*} obj series_fifo对象。
 * @param {bool_t} block_event 是否阻止分发事件。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t series_fifo_set_block_event(object_t* obj, bool_t block_event);

#define SERIES_FIFO(obj) ((series_fifo_t*)(obj))

#define SERIES_FIFO_PROP_CAPACITY "capacity"
#define SERIES_FIFO_PROP_SIZE "size"
#define SERIES_FIFO_PROP_CURSOR "cursor"
#define SERIES_FIFO_PROP_UNIT_SIZE "unit_size"

#define SERIES_FIFO_GET_CAPACITY(obj) object_get_prop_uint32(obj, SERIES_FIFO_PROP_CAPACITY, 0)
#define SERIES_FIFO_GET_SIZE(obj) object_get_prop_uint32(obj, SERIES_FIFO_PROP_SIZE, 0)
#define SERIES_FIFO_GET_CURSOR(obj) object_get_prop_uint32(obj, SERIES_FIFO_PROP_CURSOR, 0)
#define SERIES_FIFO_GET_UNIT_SIZE(obj) object_get_prop_uint32(obj, SERIES_FIFO_PROP_UNIT_SIZE, 0)

END_C_DECLS

#endif /*TK_SERIES_FIFO_H*/
