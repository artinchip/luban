/**
 * File:   axis_types.h
 * Author: AWTK Develop Team
 * Brief:  axis types
 *
 * Copyright (c) 2018 - 2018  Guangzhou ZHIYUAN Electronics Co.,Ltd.
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
 * 2018-12-05 Xu ChaoZe <xuchaoze@zlg.cn> created
 *
 */

#ifndef TK_AXIS_TYPES_H
#define TK_AXIS_TYPES_H

#include "base/widget.h"
#include "../base/series_fifo_default.h"
#include "chart_utils.h"

BEGIN_C_DECLS

/**
 * 根据series的显示数据生成轴的刻度数据
 * @param {widget_t*} widget 轴控件。
 * @param {uint32_t} recent_index series最新数据点的索引。
 * @param {uint32_t} begin series当前显示数据的起始点。
 * @param {uint32_t} end series当前显示数据的结束点。
 * @param {uint32_t} middle series当前显示数据的中间点（cover时有效）。
 * @param {float_t} interval series显示数据的间隔。
 * @param {darray_t*} data 刻度数据的缓存。
 * @param {void*} ctx 上下文。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
typedef ret_t (*axis_data_from_series_t)(widget_t* widget, uint32_t recent_index, uint32_t begin,
                                         uint32_t end, uint32_t middle, float_t interval,
                                         darray_t* data, void* ctx);

/**
 * 设置series的绘图数据。
 * @param {void*} dst 绘图数据。
 * @param {float_t} series 序列点的位置。
 * @param {object_t*} value 序列点的值fifo。
 * @param {uint32_t} value_index 序列点的值在fifo中的位置。
 * @param {float_t} value_min 序列点的最小值。
 * @param {float_t} value_range 序列点的值范围。
 * @param {float_t} pixel_range 序列点的值对应的像素范围。
 * @param {bool_t} inverse 是否反向。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
typedef ret_t (*axis_measure_series_draw_data_set_t)(void* dst, float_t series, object_t* value,
                                                     uint32_t value_index, float_t value_min,
                                                     float_t value_range, float_t pixel_range,
                                                     bool_t inverse);

/**
 * 取series的绘图数据的最大或最小值。
 * @param {void*} d 绘图数据。
 * @param {const void*} data 比较的数据。
 *
 * @return {ret_t} 返回绘图数据。
 */
typedef void* (*axis_measure_series_draw_data_minmax_t)(void* d, const void* data);

/**
 * 测量series的绘图数据需要的参数。
 */
typedef struct _axis_measure_series_params_t {
  /**
   * 起始数据在fifo中的位置。
   */
  uint32_t index_of_fifo;
  /**
   * 输出数据的折叠位置。
   */
  uint32_t index_of_fold;
  /**
   * 指示序列位置的坐标轴。
   */
  widget_t* series_axis;
  /**
   * 比较绘图数据中的序列位置。
   */
  tk_compare_t draw_data_compare_series;
  /**
   * 比较绘图数据中的序列值。
   */
  tk_compare_t draw_data_compare_value;
  /**
   * 取绘图数据的最小值。
   */
  axis_measure_series_draw_data_minmax_t draw_data_min_value;
  /**
   * 取绘图数据的最大值。
   */
  axis_measure_series_draw_data_minmax_t draw_data_max_value;
  /**
   * 设置绘图数据。
   */
  axis_measure_series_draw_data_set_t draw_data_set;
} axis_measure_series_params_t;

/**
 * 测量坐标轴上序列点之间的间隔。
 * @param {widget_t*} widget 轴对象。
 *
 * @return {float_t} 间隔。
 */
typedef float_t (*axis_measure_series_interval_t)(widget_t* widget);

/**
 * 测量坐标轴上序列点的坐标。
 * @param {widget_t*} widget 轴对象。
 * @param {void*} params 测量时需要的参数。
 * @param {object_t*} src 原始序列。
 * @param {object_t*} dst 坐标序列。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
typedef ret_t (*axis_measure_series_t)(widget_t* widget, void* params, object_t* src,
                                       object_t* dst);

/**
 * 调整坐标轴自身的布局
 * @param {widget_t*} widget 轴对象。
 * @param {rect_t*} r series的显示区域。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
typedef ret_t (*axis_on_self_layout_t)(widget_t* widget, rect_t* r);

/**
 * 坐标轴绘制前的处理。
 */
typedef ret_t (*axis_on_paint_before_t)(widget_t* widget, canvas_t* c);

/**
 * axis控件的虚函数表。
 */
typedef struct _axis_vtable_t {
  axis_measure_series_interval_t measure_series_interval;
  axis_measure_series_t measure_series;
  axis_on_self_layout_t on_self_layout;
  axis_on_paint_before_t on_paint_before;
} axis_vtable_t;

/**
 * @enum axis_type_t
 * @annotation ["scriptable"]
 * @prefix AXIS_TYPE_
 * 坐标轴类型。
 */
typedef enum _axis_type_t {
  /**
   * @const AXIS_TYPE_CATEGORY
   * 种类坐标轴, 用于表示坐标点的类别，坐标点默认在interval内居中。
   * @alias category
   */
  AXIS_TYPE_CATEGORY = 0,
  /**
   * @const AXIS_TYPE_VALUE
   * 值坐标轴, 用于表示坐标点的值, 坐标点默认在interval的边缘。
   * @alias value
   */
  AXIS_TYPE_VALUE,
  /**
   * @const AXIS_TYPE_TIME
   * 时间坐标轴, 用于表示坐标点的时间, 坐标点默认在interval的边缘。
   * @alias value
   */
  AXIS_TYPE_TIME
} axis_type_t;

/**
 * @enum axis_at_type_t
 * @annotation ["scriptable"]
 * @prefix AXIS_AT_
 * 坐标轴位置。
 */
typedef enum _axis_at_type_t {
  /**
   * @const AXIS_AT_AUTO
   * 自动选择位置。
   * @alias auto
   */
  AXIS_AT_AUTO = 0,
  /**
   * @const AXIS_AT_LEFT
   * 水平左边。
   * @alias left
   */
  AXIS_AT_LEFT = 1,
  /**
   * @const AXIS_AT_RIGHT
   * @alias right
   * 水平右边。
   */
  AXIS_AT_RIGHT = 2,
  /**
   * @const AXIS_AT_TOP
   * 垂直顶部。
   * @alias top
   */
  AXIS_AT_TOP = 3,
  /**
   * @const AXIS_AT_BOTTOM
   * 垂直低部。
   * @alias bottom
   */
  AXIS_AT_BOTTOM = 4
} axis_at_type_t;

/**
 * @class axis_label_params_t
 * 轴上标尺的刻度值。
 */
typedef struct _axis_label_params_t {
  /**
   * 是否显示。
   */
  uint8_t show : 1;
  /**
   * 是否在内部显示。
   */
  uint8_t inside : 1;
} axis_label_params_t;

/**
 * @class axis_title_params_t
 * 轴上的标题。
 */
typedef struct _axis_title_params_t {
  /**
   * 标题。
   */
  wstr_t text;
  /**
   * 是否显示。
   */
  uint8_t show : 1;
} axis_title_params_t;

/**
 * @class axis_tick_params_t
 * 轴上的刻度线。
 */
typedef struct _axis_tick_params_t {
  /**
   * 是否显示。
   */
  uint8_t show : 1;
  /**
   * 是否与刻度值对齐。
   */
  uint8_t align_with_label : 1;
  /**
   * 是否在内部显示。
   */
  uint8_t inside : 1;
} axis_tick_params_t;

/**
 * @class axis_line_params_t
 * 轴线。
 */
typedef struct _axis_line_params_t {
  /**
   * 两端的延长。
   */
  uint32_t lengthen;
  /**
   * 是否显示。
   */
  uint8_t show : 1;
} axis_line_params_t;

/**
 * @class axis_split_line_params_t
 * 坐标系上的分割线。
 */
typedef struct _axis_split_line_params_t {
  /**
   * 是否显示。
   */
  uint8_t show : 1;

  /* private */
  int32_t line_len;
} axis_split_line_params_t;

/**
 * @class axis_time_params_t
 * 时间坐标轴参数。
 */
typedef struct _axis_time_params_t {
  /**
   * 最近时间（以1970-1-1零点零分为起始的秒数）。
   */
  uint64_t recent_time;
  /**
   * 刻度值之间的间隔时间（单位：毫秒）。
   */
  uint32_t div;
  /**
   * 采样率（单位：毫秒）。
   */
  uint32_t sampling_rate;
  /**
   * 显示格式。
   *
   * > 格式规则请参考：
   * [awtk/src/base/date_time_format.h](https://github.com/zlgopen/awtk/blob/master/src/base/date_time_format.h)
   *
   */
  wstr_t format;
} axis_time_params_t;

/**
 * @class axis_data_t
 * @annotation ["scriptable"]
 * 坐标轴上的刻度数据。
 */
typedef struct _axis_data_t {
  float_t tick;
  wstr_t text;
} axis_data_t;

/**
 * @method axis_data_create
 * 创建axis_data对象
 * @annotation ["constructor", "scriptable"]
 * @param {float_t} tick 刻度位置
 * @param {const char*} text 刻度值
 *
 * @return {axis_data_t*} 对象。
 */
axis_data_t* axis_data_create(float_t tick, const char* text);

/**
 * @method axis_data_destroy
 * 销毁axis_data对象
 * @annotation ["deconstructor", "scriptable", "gc"]
 * @param {axis_data_t*} data axis_data对象
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t axis_data_destroy(axis_data_t* data);

/**
 * 坐标轴的刻度的默认长度
 */
#define AXIS_DEFAULT_TICK_LEN 4

/**
 * @enum widget_prop_t
 * @annotation ["scriptable", "string"]
 * @prefix AXIS_PROP_
 * 控件的属性。
 */

/**
 * @const AXIS_PROP_TYPE
 * 坐标轴的类型
 */
#define AXIS_PROP_TYPE "axis_type"

/**
 * @const AXIS_PROP_AT
 * 坐标轴的位置
 */
#define AXIS_PROP_AT "at"

/**
 * @const AXIS_PROP_OFFSET
 * 坐标轴的偏移
 */
#define AXIS_PROP_OFFSET "offset"

/**
 * @const AXIS_PROP_OFFSET_PERCENT
 * 坐标轴的偏移是否为百分比
 */
#define AXIS_PROP_OFFSET_PERCENT "offset_percent"

/**
 * @const AXIS_PROP_DATA
 * 坐标轴的刻度数据
 */
#define AXIS_PROP_DATA "data"

/**
 * @const AXIS_PROP_INVERSE
 * 坐标轴是否反向
 */
#define AXIS_PROP_INVERSE "inverse"

/**
 * @const AXIS_PROP_SPLIT_LINE
 * 坐标轴上的分割线
 */
#define AXIS_PROP_SPLIT_LINE "split_line"

/**
 * @const AXIS_PROP_SPLIT_LINE_SHOW
 * 坐标轴上的分割线是否显示
 */
#define AXIS_PROP_SPLIT_LINE_SHOW "split_line:show"

/**
 * @const AXIS_PROP_LINE
 * 坐标轴的轴线
 */
#define AXIS_PROP_LINE "line"

/**
 * @const AXIS_PROP_LINE_SHOW
 * 坐标轴的轴线是否显示
 */
#define AXIS_PROP_LINE_SHOW "line:show"

/**
 * @const AXIS_PROP_TICK
 * 坐标轴的刻度线
 */
#define AXIS_PROP_TICK "tick"

/**
 * @const AXIS_PROP_TICK_SHOW
 * 坐标轴的刻度线是否显示
 */
#define AXIS_PROP_TICK_SHOW "tick:show"

/**
 * @const AXIS_PROP_LABEL
 * 坐标轴上的刻度值
 */
#define AXIS_PROP_LABEL "label"

/**
 * @const AXIS_PROP_LABEL_SHOW
 * 坐标轴上的刻度值是否显示
 */
#define AXIS_PROP_LABEL_SHOW "label:show"

/**
 * @const AXIS_PROP_TITLE
 * 坐标轴的标题
 */
#define AXIS_PROP_TITLE "title"

/**
 * @const AXIS_PROP_TITLE_SHOW
 * 坐标轴的标题是否显示
 */
#define AXIS_PROP_TITLE_SHOW "title:show"

/**
 * @const AXIS_PROP_TIME
 * 坐标轴的时间
 */
#define AXIS_PROP_TIME "time"

/**
 * @const AXIS_PROP_TIME_RECENT_TIME
 * 坐标轴的最近时间
 */
#define AXIS_PROP_TIME_RECENT_TIME "time:recent_time"

/**
 * @const AXIS_PROP_TIME_DIV
 * 坐标轴的间隔时间
 */
#define AXIS_PROP_TIME_DIV "time:div"

/**
 * @const AXIS_PROP_TIME_SAMPLING_RATE
 *  坐标轴的采样率
 */
#define AXIS_PROP_TIME_SAMPLING_RATE "time:sampling_rate"

/**
 * @const AXIS_PROP_TIME_FORMAT
 * 坐标轴的时间显示格式
 */
#define AXIS_PROP_TIME_FORMAT "time:format"

/**
 * @enum widget_type_t
 * @annotation ["scriptable", "string"]
 * @prefix WIDGET_TYPE_
 * 控件的类型。
 */

/**
 * @const WIDGET_TYPE_AXIS
 * 坐标轴。
 */
#define WIDGET_TYPE_AXIS "axis"

/**
 * @const WIDGET_TYPE_X_AXIS
 * x坐标轴。
 */
#define WIDGET_TYPE_X_AXIS "x_axis"

/**
 * @const WIDGET_TYPE_Y_AXIS
 * y坐标轴。
 */
#define WIDGET_TYPE_Y_AXIS "y_axis"

/**
 * @enum style_id_axis_t
 * @prefix STYLE_ID_AXIS_
 * @annotation ["scriptable", "string"]
 * style常量定义。
 */
/**
 * @const STYLE_ID_AXIS_SPLIT_LINE_COLOR
 * 坐标轴分割线的颜色。
 */
#define STYLE_ID_AXIS_SPLIT_LINE_COLOR "split_line_color"

/**
 * @const STYLE_ID_AXIS_SPLIT_LINE_IMAGE
 * 坐标轴分割线的图片。
 */
#define STYLE_ID_AXIS_SPLIT_LINE_IMAGE "split_line_image"

/**
 * @const STYLE_ID_AXIS_SPLIT_LINE_IMAGE
 * 坐标轴分割线的图片的显示方式。
 */
#define STYLE_ID_AXIS_SPLIT_LINE_IMAGE_DRAW_TYPE "split_line_image_draw_type"

/**
 * @const STYLE_ID_AXIS_TICK_COLOR
 * 坐标轴刻度线的颜色。
 */
#define STYLE_ID_AXIS_TICK_COLOR "tick_color"

/**
 * @const STYLE_ID_AXIS_TICK_IMAGE
 * 坐标轴刻度线的图片。
 */
#define STYLE_ID_AXIS_TICK_IMAGE "tick_image"

/**
 * @const STYLE_ID_AXIS_TICK_IMAGE
 * 坐标轴刻度线的图片的显示方式。
 */
#define STYLE_ID_AXIS_TICK_IMAGE_DRAW_TYPE "tick_image_draw_type"

END_C_DECLS

#endif /*TK_AXIS_TYPES_H*/
