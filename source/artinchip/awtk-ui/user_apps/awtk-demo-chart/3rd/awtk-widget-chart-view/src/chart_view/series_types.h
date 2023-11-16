/**
 * File:   series_types.h
 * Author: AWTK Develop Team
 * Brief:  series types
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

#ifndef TK_SERIES_TYPES_H
#define TK_SERIES_TYPES_H

#include "base/widget.h"
#include "../base/series_fifo_default.h"
#include "chart_utils.h"

/**
 * @enum series_dispaly_mode_t
 * @prefix SERIES_DISPLAY_
 * series的显示模式。
 */
typedef enum _series_dispaly_mode_t {
  /**
   * @const SERIES_DISPLAY_AUTO
   * 自动选择。
   * @alias auto
   */
  SERIES_DISPLAY_AUTO = 0,
  /**
   * @const SERIES_DISPLAY_PUSH
   * 推进式（从右向左推进）。
   * @alias push
   */
  SERIES_DISPLAY_PUSH = 1,
  /**
   * @const SERIES_DISPLAY_COVER
   * 覆盖式（从左向右循环覆盖, 类似心电图）。
   * @alias cover
   */
  SERIES_DISPLAY_COVER = 2,
} series_dispaly_mode_t;
series_dispaly_mode_t series_dispaly_mode_from_str(const char* mode);

/**
 * 序列点形成的曲线/折线
 */
typedef struct _series_line_params_t {
  /**
   * 是否显示。
   */
  uint8_t show : 1;
  /**
   * 是否平滑线条。
   */
  uint8_t smooth : 1;
} series_line_params_t;

/**
 * 曲线/折线与轴围成的区域
 */
typedef struct _series_line_area_params_t {
  /**
   * 是否显示。
   */
  uint8_t show : 1;
} series_line_area_params_t;

/**
 * 序列点的标记
 */
typedef struct _series_symbol_params_t {
  /**
   * 大小
   */
  float_t size;
  /**
   * 是否显示
   */
  uint8_t show : 1;
} series_symbol_params_t;

/**
 * 指示序列值的柱条
 */
typedef struct _series_bar_params_t {
  /**
   * 是否在div上重叠显示。
   */
  uint8_t overlap : 1;
} series_bar_params_t;

/**
 * 取series的绘图数据的最大或最小值。
 * @param {void*} d 绘图数据。
 * @param {const void*} data 比较的数据。
 *
 * @return {ret_t} 返回绘图数据。
 */
typedef void* (*series_draw_data_minmax_t)(void* d, const void* data);

/**
 * 获取series的绘图数据的值。
 * @param {const void*} data 绘图数据。
 *
 * @return {ret_t} 返回绘图数据的值。
 */
typedef float_t (*series_draw_data_get_t)(const void* data);

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
typedef ret_t (*series_draw_data_set_t)(void* dst, float_t series, object_t* value,
                                        uint32_t value_index, float_t value_min,
                                        float_t value_range, float_t pixel_range, bool_t inverse);
/**
 * series绘图数据的信息
 */
typedef struct _series_draw_data_info_t {
  uint32_t unit_size;
  tk_compare_t compare_in_axis1;
  tk_compare_t compare_in_axis2;
  series_draw_data_minmax_t min_axis1;
  series_draw_data_minmax_t min_axis2;
  series_draw_data_minmax_t max_axis1;
  series_draw_data_minmax_t max_axis2;
  series_draw_data_get_t get_axis1;
  series_draw_data_get_t get_axis2;
  series_draw_data_set_t set_as_axis12;
  series_draw_data_set_t set_as_axis21;
} series_draw_data_info_t;

typedef widget_animator_t* (*series_animator_create_t)(widget_t* widget, uint32_t duration,
                                                       uint32_t delay, easing_type_t easing);
typedef uint32_t (*series_count_t)(widget_t* widget);
typedef ret_t (*series_set_t)(widget_t* widget, uint32_t index, const void* data, uint32_t nr);
typedef ret_t (*series_rset_t)(widget_t* widget, uint32_t index, const void* data, uint32_t nr);
typedef ret_t (*series_push_t)(widget_t* widget, const void* data, uint32_t nr);
typedef ret_t (*series_clear_t)(widget_t* widget);
typedef ret_t (*series_pop_t)(widget_t* widget, uint32_t nr);
typedef void* (*series_at_t)(widget_t* widget, uint32_t index);
typedef ret_t (*series_get_current_t)(widget_t* widget, uint32_t* begin, uint32_t* end,
                                      uint32_t* middle);
typedef int32_t (*series_index_of_point_in_t)(widget_t* widget, xy_t x, xy_t y, bool_t is_local);
typedef ret_t (*series_to_local_t)(widget_t* widget, uint32_t index, point_t* p);
typedef bool_t (*series_is_point_in_t)(widget_t* widget, xy_t x, xy_t y, bool_t is_local);
typedef ret_t (*series_on_paint_t)(widget_t* widget, canvas_t* c, float_t ox, float_t oy,
                                   object_t* fifo, uint32_t index, uint32_t size,
                                   rect_t* clip_rect);
typedef ret_t (*series_tooltip_format_t)(void* ctx, const void* data, wstr_t* str);
typedef object_t* (*series_prepare_fifo_t)(widget_t* widget, void* ctx, object_t* obj);

/**
 * series控件的虚函数表。
 */
typedef struct _series_vtable_t {
  series_count_t count;
  series_set_t set;
  series_rset_t rset;
  series_push_t push;
  series_clear_t clear;
  series_pop_t pop;
  series_at_t at;
  series_get_current_t get_current;
  series_is_point_in_t is_point_in;
  series_index_of_point_in_t index_of_point_in;
  series_to_local_t to_local;
  series_on_paint_t on_paint;

  const series_draw_data_info_t* draw_data_info;
} series_vtable_t;

/**
 * @enum widget_prop_t
 * @annotation ["scriptable", "string"]
 * @prefix SERIES_PROP_
 * 控件的属性。
 */

/**
 * @const SERIES_PROP_FIFO
 * 序列object对象
 */
#define SERIES_PROP_FIFO "fifo"

/**
 * @const SERIES_PROP_CAPACITY
 * 序列的容量
 */
#define SERIES_PROP_CAPACITY "capacity"

/**
 * @const SERIES_PROP_OFFSET
 * 序列的偏移位置
 */
#define SERIES_PROP_OFFSET "offset"

/**
 * @const SERIES_PROP_OFFSET_MAX
 * 序列的偏移位置的最大值
 */
#define SERIES_PROP_OFFSET_MAX "offset_max"

/**
 * @const SERIES_PROP_CLIP_RANGE
 * 序列的裁剪范围
 */
#define SERIES_PROP_CLIP_RANGE "clip_range"

/**
 * @const SERIES_PROP_COVERAGE
 * 序列的覆盖范围（点数）
 */
#define SERIES_PROP_COVERAGE "coverage"

/**
 * @const SERIES_PROP_SERIES_AXIS
 * 指示序列位置的轴
 */
#define SERIES_PROP_SERIES_AXIS "series_axis"

/**
 * @const SERIES_PROP_SERIES_AXIS_OBJ
 * 指示序列位置的轴对象
 */
#define SERIES_PROP_SERIES_AXIS_OBJ "series_axis_obj"

/**
 * @const SERIES_PROP_VALUE_AXIS
 * 指示序列值的轴
 */
#define SERIES_PROP_VALUE_AXIS "value_axis"

/**
 * @const SERIES_PROP_VALUE_AXIS_OBJ
 * 指示序列值的轴对象
 */
#define SERIES_PROP_VALUE_AXIS_OBJ "value_axis_obj"

/**
 * @const SERIES_PROP_DISPLAY_MODE
 * 序列的显示模式
 */
#define SERIES_PROP_DISPLAY_MODE "display_mode"

/**
 * @const SERIES_PROP_VALUE_ANIMATION
 * 序列值动画的持续时间
 */
#define SERIES_PROP_VALUE_ANIMATION "value_animation"

/**
 * @const SERIES_PROP_NEW_PERIOD
 * 序列新周期部分的点数
 */
#define SERIES_PROP_NEW_PERIOD "new_period"

/**
 * @const SERIES_PROP_TITLE
 * 序列的标题
 */
#define SERIES_PROP_TITLE "title"

/**
 * @const SERIES_PROP_LINE
 * 序列形成的曲线
 */
#define SERIES_PROP_LINE "line"

/**
 * @const SERIES_PROP_LINE_SHOW
 * 序列形成的曲线是否显示
 */
#define SERIES_PROP_LINE_SHOW "line:show"

/**
 * @const SERIES_PROP_LINE_SMOOTH
 * 序列形成的曲线是否平滑显示
 */
#define SERIES_PROP_LINE_SMOOTH "line:smooth"

/**
 * @const SERIES_PROP_LINE_AREA
 * 序列的曲线与轴围成的区域
 */
#define SERIES_PROP_LINE_AREA "area"

/**
 * @const SERIES_PROP_LINE_AREA_SHOW
 * 序列的曲线与轴围成的区域是否显示
 */
#define SERIES_PROP_LINE_AREA_SHOW "area:show"

/**
 * @const SERIES_PROP_SYMBOL
 * 序列点的标记
 */
#define SERIES_PROP_SYMBOL "symbol"

/**
 * @const SERIES_PROP_SYMBOL_SIZE
 * 序列点的标记的大小
 */
#define SERIES_PROP_SYMBOL_SIZE "symbol:size"
/**
 * @const SERIES_PROP_SYMBOL_SHOW
 * 序列点的标记是否显示
 */
#define SERIES_PROP_SYMBOL_SHOW "symbol:show"

/**
 * @const SERIES_PROP_BAR
 * 指示序列值的柱条
 */
#define SERIES_PROP_BAR "bar"

/**
 * @const SERIES_PROP_BAR_OVERLAP
 * 指示序列值的柱条是否重叠显示
 */
#define SERIES_PROP_BAR_OVERLAP "bar:overlap"

/**
 * @enum widget_type_t
 * @annotation ["scriptable", "string"]
 * @prefix WIDGET_TYPE_
 * 控件的类型。
 */

/**
 * @const WIDGET_TYPE_SERIES
 * 序列。
 */
#define WIDGET_TYPE_SERIES "series"

/**
 * @const WIDGET_TYPE_LINE_SERIES
 * 线形序列。
 */
#define WIDGET_TYPE_LINE_SERIES "line_series"

/**
 * @const WIDGET_TYPE_LINE_SERIES_COLORFUL
 * 彩色线形序列。
 */
#define WIDGET_TYPE_LINE_SERIES_COLORFUL "line_series_colorful"

/**
 * @const WIDGET_TYPE_BAR_SERIES
 * 柱条序列。
 */
#define WIDGET_TYPE_BAR_SERIES "bar_series"

/**
 * @const WIDGET_TYPE_BAR_SERIES_MINMAX
 * 包含最大最小值的柱条序列。
 */
#define WIDGET_TYPE_BAR_SERIES_MINMAX "bar_series_minmax"

/**
 * @enum style_id_series_t
 * @prefix STYLE_ID_SERIES_
 * @annotation ["scriptable", "string"]
 * style常量定义。
 */
/**
 * @const STYLE_ID_SERIES_LINE_BORDER_COLOR
 * 序列曲线的颜色
 */
#define STYLE_ID_SERIES_LINE_BORDER_COLOR "line_border_color"

/**
 * @const STYLE_ID_SERIES_LINE_BORDER_WIDTH
 * 序列曲线的宽度
 */
#define STYLE_ID_SERIES_LINE_BORDER_WIDTH "line_border_width"

/**
 * @const STYLE_ID_SERIES_AREA_COLOR
 * 序列曲线与轴围成的区域的颜色
 */
#define STYLE_ID_SERIES_AREA_COLOR "area_color"

/**
 * @const STYLE_ID_SERIES_SYMBOL_BG_IMAGE
 * 序列点的背景图片
 */
#define STYLE_ID_SERIES_SYMBOL_BG_IMAGE "symbol_bg_image"

/**
 * @const STYLE_ID_SERIES_SYMBOL_BG_IMAGE_DRAW_TYPE
 * 序列点的背景图片的显示方式
 */
#define STYLE_ID_SERIES_SYMBOL_BG_IMAGE_DRAW_TYPE "symbol_bg_image_draw_type"

/**
 * @const STYLE_ID_SERIES_SYMBOL_BG_COLOR
 * 序列点的背景颜色
 */
#define STYLE_ID_SERIES_SYMBOL_BG_COLOR "symbol_bg_color"

/**
 * @const STYLE_ID_SERIES_SYMBOL_BORDER_COLOR
 * 序列点的边框颜色
 */
#define STYLE_ID_SERIES_SYMBOL_BORDER_COLOR "symbol_border_color"

/**
 * @const STYLE_ID_SERIES_SYMBOL_BORDER_WIDTH
 * 序列点的边框宽度
 */
#define STYLE_ID_SERIES_SYMBOL_BORDER_WIDTH "symbol_border_width"

/**
 * @const STYLE_ID_SERIES_SYMBOL_BORDER_WIDTH
 * 序列点的圆角半径
 */
#define STYLE_ID_SERIES_SYMBOL_ROUND_RADIUS "symbol_round_radius"

#endif /*TK_SERIES_TYPES_H*/
