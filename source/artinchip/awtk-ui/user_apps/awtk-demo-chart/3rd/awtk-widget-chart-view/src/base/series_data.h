/**
 * File:   series_data.h
 * Author: AWTK Develop Team
 * Brief:  series_data.
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

#ifndef TK_SERIES_DATA_H
#define TK_SERIES_DATA_H

#include "tkc/color.h"

BEGIN_C_DECLS

/**
 * 普通FIFO数据类型为 float_t ，适用于line_series、bar_series控件。
 */

/**
 * 彩色FIFO数据结构，适用于line_series_colorful控件。
 */
typedef struct _series_data_colorful_t {
  float_t v;
  color_t c;
} series_data_colorful_t;

/**
 * 最大最小值FIFO数据结构，适用于bar_series_minmax控件。
 */
typedef struct _series_data_minmax_t {
  float_t min;
  float_t max;
} series_data_minmax_t;

/**
 * 绘制时使用的普通FIFO数据结构（仅供控件内部使用）。
 */
typedef struct _series_data_draw_normal_t {
  float_t x;
  float_t y;
} series_data_draw_normal_t;

/**
 * 绘制时使用的彩色FIFO数据结构（仅供控件内部使用）。
 */
typedef struct _series_data_draw_colorful_t {
  float_t x;
  float_t y;
  color_t c;
} series_data_draw_colorful_t;

/**
 * 绘制时使用的最大最小值FIFO数据结构（仅供控件内部使用）。
 */
typedef struct _series_data_draw_minmax_t {
  float_t xmin;
  float_t xmax;
  float_t ymin;
  float_t ymax;
} series_data_draw_minmax_t;

END_C_DECLS

#endif /*TK_SERIES_DATA_H*/
