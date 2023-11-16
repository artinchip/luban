/**
 * File:   line series.h
 * Author: AWTK Develop Team
 * Brief:  line series
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

#ifndef TK_LINE_SERIES_H
#define TK_LINE_SERIES_H

#include "series.h"

BEGIN_C_DECLS

/**
 * @class line_series_t
 * @parent series_t
 * @annotation ["scriptable","design","widget"]
 * 线形序列控件，作为chart\_view的一个子部件使用，用于实现趋势图。
 *
 * 在xml中使用"line\_series"标签创建线形序列控件。
 * value属性的格式为"采样值,采样值,..."。
 * 如：
 *
 * ```xml
 * <!-- ui -->
 * <line_series w="100" h="100" capacity="10" value_animation="500" line="{smooth:true}"
 * area="{show:true}" symbol="{show:true}" value="15,75,40,60,140,80,100,120,25,90"/>
 * ```
 *
 * 可用通过style来设置控件的显示风格。其中，
 * line_border_color 用于设置序列曲线的颜色；
 * line_border_width 用于设置序列曲线的宽度；
 * area_color 用于设置序列曲线与坐标轴围成的区域的颜色；
 * symbol_bg_image 用于设置序列点的背景图片；
 * symbol_bg_image_draw_type 用于设置序列点的背景图片的显示方式；
 * symbol_bg_color 用于设置序列点的背景颜色；
 * symbol_border_color 用于设置序列点的边框颜色；
 * symbol_border_width 用于设置序列点的边框宽度；
 * symbol_round_radius 用于设置序列点的圆角。
 * 如：
 *
 * ```xml
 * <!-- style -->
 * <line_series>
 *   <style name="default">
 *     <normal line_border_color="#338fff" line_border_width="1" area_color="#338fff66"
 * symbol_border_color="#338fff" symbol_bg_color="#ffffff" symbol_round_radius="4"/>
 *   </style>
 * </line_series>
 * ```
 */
typedef struct _line_series_t {
  series_t base;
  /**
   * @property {char*} series_axis
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable:custom"]
   * 标示序列位置的轴（名称），为空时默认为检索到的第一个x_axis。
   */
  char* series_axis;
  /**
   * @property {char*} value_axis
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable:custom"]
   * 标示序列值的轴（名称）为空时默认为检索到的第一个y_axis。
   */
  char* value_axis;
  /**
   * @property {series_line_params_t} line
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable:custom"]
   * 序列曲线的参数，比如"{show:true, smooth:true}"。
   */
  series_line_params_t line;
  /**
   * @property {series_line_area_params_t} area
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable:custom"]
   * 序列曲线与坐标轴围成的区域的参数，比如"{show:true}"。
   */
  series_line_area_params_t area;
  /**
   * @property {series_symbol_params_t} symbol
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable:custom"]
   * 序列点的参数，比如"{show:true, size:4}"。
   */
  series_symbol_params_t symbol;
} line_series_t;

/**
 * @method line_series_create
 * 创建line_series对象
 * @annotation ["constructor", "scriptable"]
 * @param {widget_t*} parent 父控件
 * @param {xy_t} x x坐标
 * @param {xy_t} y y坐标
 * @param {wh_t} w 宽度
 * @param {wh_t} h 高度
 *
 * @return {widget_t*} 对象。
 */
widget_t* line_series_create(widget_t* widget, xy_t x, xy_t y, wh_t w, wh_t h);

/**
 * @method line_series_cast
 * 转换为line_series对象(供脚本语言使用)。
 * @annotation ["cast", "scriptable"]
 * @param {widget_t*} widget line_series对象。
 *
 * @return {widget_t*} line_series对象。
 */
widget_t* line_series_cast(widget_t* series);

#define LINE_SERIES(widget) ((line_series_t*)(line_series_cast(WIDGET(widget))))

/*public for subclass and runtime type check*/
TK_EXTERN_VTABLE(line_series);

END_C_DECLS

#endif /*TK_LINE_SERIES_H*/
