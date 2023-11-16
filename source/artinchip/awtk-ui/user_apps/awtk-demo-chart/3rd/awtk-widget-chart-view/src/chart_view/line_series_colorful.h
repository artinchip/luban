/**
 * File:   line_series_colorful.h
 * Author: AWTK Develop Team
 * Brief:  colorful line series
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

#ifndef TK_LINE_SERIES_COLORFUL_H
#define TK_LINE_SERIES_COLORFUL_H

#include "line_series.h"

BEGIN_C_DECLS

/**
 * @class line_series_colorful_t
 * @parent line_series_t
 * @annotation ["scriptable","design","widget"]
 * 彩色线形序列控件，作为chart\_view的一个子部件使用，可按指定颜色显示曲线。
 *
 * 在xml中使用"line\_series\_colorful"标签创建彩色线形控件。
 * value属性的格式为"色值,采样值,采样值,色值,采样值,色值,..."。
 * 如果没有显式指定则沿用上一个采样点的色值，默认颜色为黑色。
 * 如：
 *
 * ```xml
 * <!-- ui -->
 * <line_series_colorful w="100" h="100" capacity="10" value_animation="500" line="{smooth:true}"
 * area="{show:true}" symbol="{show:true}"
 * value="#69CF5C,50,20,40,#327AD3,60,140,80,100,#36B3C3,120,15,89"/>
 * ```
 *
 * 可用通过style来设置控件的显示风格。其中，
 * line_border_width 用于设置序列曲线的宽度；
 * symbol_bg_image 用于设置序列点的背景图片；
 * symbol_bg_image_draw_type 用于设置序列点的背景图片的显示方式；
 * symbol_border_color 用于设置序列点的边框颜色；
 * symbol_border_width 用于设置序列点的边框宽度；
 * symbol_round_radius 用于设置序列点的圆角。
 * 如：
 *
 * ```xml
 * <!-- style -->
 * <line_series_colorful>
 *   <style name="default">
 *     <normal line_border_width="1" symbol_border_color="#ffc393" symbol_round_radius="4"/>
 *   </style>
 * </line_series_colorful>
 * ```
 */

/**
 * @method line_series_colorful_create
 * 创建彩色line_series对象
 * @annotation ["constructor", "scriptable"]
 * @param {widget_t*} parent 父控件
 * @param {xy_t} x x坐标
 * @param {xy_t} y y坐标
 * @param {wh_t} w 宽度
 * @param {wh_t} h 高度
 *
 * @return {widget_t*} 对象。
 */
widget_t* line_series_colorful_create(widget_t* widget, xy_t x, xy_t y, wh_t w, wh_t h);

/**
 * @method widget_is_line_series_colorful
 * 判断当前控件是否为line_series_colorful控件。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget 控件对象。
 *
 * @return {bool_t} 返回当前控件是否为tooltip控件。
 */
bool_t widget_is_line_series_colorful(widget_t* widget);

/*public for subclass and runtime type check*/
TK_EXTERN_VTABLE(line_series_colorful);

END_C_DECLS

#endif /*TK_LINE_SERIES_COLORFUL_H*/
