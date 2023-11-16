/**
 * File:   axis.h
 * Author: AWTK Develop Team
 * Brief:  axis
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

#ifndef TK_AXIS_H
#define TK_AXIS_H

#include "base/widget.h"
#include "axis_types.h"

BEGIN_C_DECLS

/**
 * @class axis_t
 * @parent widget_t
 * @annotation ["scriptable","design","widget"]
 * 坐标轴控件的基类。
 */
typedef struct _axis_t {
  widget_t widget;
  /**
   * @property {axis_type_t} axis_type
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * 坐标轴的类型，可选项有 value、category、time。
   */
  axis_type_t axis_type;
  /**
   * @property {axis_at_type_t} at
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * 坐标轴的位置，可选项有 top、bottom、left、right，x 轴缺省为 bottom，y 轴缺省为 left。
   */
  axis_at_type_t at;
  /**
   * @property {float_t} min
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * 量程的最小值。
   */
  float_t min;
  /**
   * @property {float_t} max
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * 量程的最大值。
   */
  float_t max;
  /**
   * @property {darray_t*} data
   * @annotation ["set_prop","readable","persitent","design","scriptable:custom"]
   * 显示的刻度值。
   */
  darray_t* data;
  /**
   * @property {axis_data_from_series_t} data_from_series
   * @annotation ["readable"]
   * 显示的刻度值生成器。
   */
  axis_data_from_series_t data_from_series;
  /**
   * @property {void*} data_from_series_ctx
   * @annotation ["readable"]
   * 刻度值生成器的上下文。
   */
  void* data_from_series_ctx;
  /**
   * @property {axis_split_line_params_t} split_line
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * 分割线的参数，比如"{show:true}"。
   */
  axis_split_line_params_t split_line;
  /**
   * @property {axis_tick_params_t} tick
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * 刻度线的参数，比如"{show:true, align_with_label:true, inside:false}"。
   */
  axis_tick_params_t tick;
  /**
   * @property {axis_line_params_t} line
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * 轴线的参数，比如"{show:true, lengthen:20}"。
   */
  axis_line_params_t line;
  /**
   * @property {axis_label_params_t} label
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * 刻度值的参数，比如"{show:true, inside:false}"。
   */
  axis_label_params_t label;
  /**
   * @property {axis_title_params_t} title
   * @annotation ["set_prop","get_prop","readable","persitent","scriptable"]
   * 标题的参数，比如"{show:false}"。
   */
  axis_title_params_t title;
  /**
   * @property {axis_time_params_t} time
   * @annotation ["set_prop","get_prop","readable","persitent","scriptable"]
   * 时间的参数，比如"{format:Y-M-D hh:mm:ss}"。
   */
  axis_time_params_t time;
  /**
   * @property {float_t} offset
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * （相对于初始位置的）偏移（像素）。
   */
  float_t offset;
  /**
   * @property {bool_t} offset_percent
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * （相对于初始位置的）偏移是否为百分比。
   */
  uint8_t offset_percent : 1;
  /**
   * @property {bool_t} inverse
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * 是否反向
   */
  uint8_t inverse : 1;
  /**
   * @property {bool_t} data_fixed
   * @annotation ["readable"]
   * 刻度值是否固定。
   */
  uint8_t data_fixed : 1;
  /**
   * @property {bool_t} need_update_data
   * @annotation ["readable"]
   * 刻度值是否需要更新。
   */
  uint8_t need_update_data : 1;
  /**
   * @property {bool_t} painted_before
   * @annotation ["readable"]
   * 是否已完成绘图的前置处理。
   */
  uint8_t painted_before : 1;

  /**
   * @property {axis_vtable_t} vt
   * @annotation ["readable"]
   * 虚函数表。
   */
  const axis_vtable_t* vt;

  /* private */
  rect_t draw_rect;
} axis_t;

/**
 * @method axis_create
 * 创建对象。
 *
 * > 仅供子类调用。
 *
 * @param {widget_t*} parent widget的父控件。
 * @param {const widget_vtable_t*} vt 虚函数表。
 * @param {xy_t}   x x坐标。
 * @param {xy_t}   y y坐标。
 * @param {wh_t}   w 宽度。
 * @param {wh_t}   h 高度。
 *
 * @return {widget_t*} widget对象本身。
 */
widget_t* axis_create(widget_t* parent, const widget_vtable_t* vt, xy_t x, xy_t y, wh_t w, wh_t h);

/**
 * @method axis_on_destroy
 * 销毁对象时的处理。
 *
 * > 仅供子类调用。
 *
 * @param {widget_t*} widget widget对象。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t axis_on_destroy(widget_t* widget);

/**
 * @method axis_on_paint_before
 * 绘图的前置处理。
 * @annotation ["private"]
 * @param {widget_t*} widget widget对象。
 * @param {canvas_t*} c 画布对象。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t axis_on_paint_before(widget_t* widget, canvas_t* c);

/**
 * @method axis_on_self_layout
 * 调整坐标轴自身的布局。
 * @annotation ["private"]
 * @param {widget_t*} widget widget对象。
 * @param {rect_t*} r series的显示区域。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t axis_on_self_layout(widget_t* widget, rect_t* r);

/**
 * @method axis_set_range
 * 设置坐标轴的量程。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {float_t} min 最小值。
 * @param {float_t} max 最大值。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t axis_set_range(widget_t* widget, float_t min, float_t max);

/**
 * @method axis_get_range
 * 获取坐标轴的量程。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {bool_t} is_series_axis 是否为指示序列位置的轴
 *
 * @return {float_t} 量程范围。
 */
float_t axis_get_range(widget_t* widget, bool_t is_series_axis);

/**
 * @method axis_set_offset
 * 设置坐标轴（相对初始位置的）偏移。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {float_t} offset 偏移。
 * @param {bool_t} percent 是否为百分比。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t axis_set_offset(widget_t* widget, float_t offset, bool_t percent);

/**
 * @method axis_get_offset
 * 获取坐标轴（相对初始位置的）偏移。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {float_t} defval 默认值。
 *
 * @return {float_t} 偏移。
 */
float_t axis_get_offset(widget_t* widget, float_t defval);

/**
 * @method axis_set_data
 * 设置坐标轴上显示的刻度值。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {const char*} data 显示值。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t axis_set_data(widget_t* widget, const char* data);

/**
 * @method axis_set_data_from_series
 * 设置坐标轴的刻度显示值的生成器。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {axis_data_from_series_t} from_series 生成回调。
 * @param {void*} ctx 上下文。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t axis_set_data_from_series(widget_t* widget, axis_data_from_series_t from_series, void* ctx);

/**
 * @method axis_set_need_update_data
 * 设置需要更新坐标轴的刻度显示值。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t axis_set_need_update_data(widget_t* widget);

/**
 * @method axis_update_data_from_series
 * 更新坐标轴的刻度显示值。
 * @annotation ["private"]
 * @param {widget_t*} widget widget对象。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t axis_update_data_from_series(widget_t* widget);

/**
 * @method axis_measure_series_interval
 * 测量坐标轴上的序列点之间的间隔。
 * @annotation ["private"]
 * @param {widget_t*} widget widget对象。
 *
 * @return {float_t} 返回序列点之间的间隔。
 */
float_t axis_measure_series_interval(widget_t* widget);

/**
 * @method axis_measure_series_interval
 * 测量坐标轴上的序列点的坐标。
 * @annotation ["private"]
 * @param {widget_t*} widget widget对象。
 * @param {void*} params 测量时需要的参数。
 * @param {object_t*} src 原始序列。
 * @param {object_t*} dst 坐标序列。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t axis_measure_series(widget_t* widget, void* sample_params, object_t* src, object_t* dst);

/**
 * @method axis_cast
 * 转换为axis_base对象(供脚本语言使用)。
 * @annotation ["cast", "scriptable"]
 * @param {widget_t*} widget axis_base对象。
 *
 * @return {widget_t*} axis_base对象。
 */
widget_t* axis_cast(widget_t* widget);

#define AXIS(widget) ((axis_t*)(axis_cast(WIDGET(widget))))

/**
 * @method widget_is_axis
 * 判断当前控件是否为axis控件。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget 控件对象。
 *
 * @return {bool_t} 返回当前控件是否为axis控件。
 */
bool_t widget_is_axis(widget_t* widget);

/*public for subclass and runtime type check*/
TK_EXTERN_VTABLE(axis);

END_C_DECLS

#endif /*TK_AXIS_H*/
