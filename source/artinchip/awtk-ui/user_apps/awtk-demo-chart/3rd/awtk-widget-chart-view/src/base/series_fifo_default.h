/**
 * File:   series_fifo_default.h
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

#ifndef TK_SERIES_FIFO_DEFAULT_H
#define TK_SERIES_FIFO_DEFAULT_H

#include "series_fifo.h"

BEGIN_C_DECLS

struct _series_fifo_default_t;
typedef struct _series_fifo_default_t series_fifo_default_t;

/**
 * @class series_fifo_default_t
 * @parent series_fifo_t
 *
 * series_fifo的缺省实现。
 *
 */
struct _series_fifo_default_t {
  series_fifo_t base;

  /**
   * @property {uint32_t} capacity
   * @annotation ["readable"]
   * FIFO的容量大小。
   */
  uint32_t capacity;
  /**
   * @property {uint32_t} size
   * @annotation ["readable"]
   * FIFO中元素的个数。
   */
  uint32_t size;
  /**
   * @property {uint32_t} cursor
   * @annotation ["readable"]
   * FIFO中最后一个元素的索引。
   */
  uint32_t cursor;
  /**
   * @property {uint32_t} unit_size
   * @annotation ["readable"]
   * FIFO中单个元素的大小。
   */
  uint32_t unit_size;
  /**
   * @property {uint8_t*} buffer
   * @annotation ["readable"]
   * FIFO中的数据缓存。
   */
  uint8_t* buffer;
};

/**
 * @method series_fifo_default_create
 * 创建series_fifo_default对象。
 *
 * @annotation ["constructor"]
 * @param {uint32_t} capacity FIFO初始容量。
 * @param {uint32_t} unit_size FIFO单个元素的大小。
 *
 * @return {object_t} 返回series_fifo对象。
 */
object_t* series_fifo_default_create(uint32_t capacity, uint32_t unit_size);

#define SERIES_FIFO_DEFAULT(obj) ((series_fifo_default_t*)(obj))

END_C_DECLS

#endif /*TK_SERIES_FIFO_DEFAULT_H*/
