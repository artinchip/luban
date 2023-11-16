/**
 * File:   bar_series_minmax.h
 * Author: AWTK Develop Team
 * Brief:  bar series
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

#ifndef TK_BAR_SERIES_MINMAX_H
#define TK_BAR_SERIES_MINMAX_H

#include "bar_series.h"

BEGIN_C_DECLS

/**
 * @class bar_series_minmax_t
 * @parent bar_series_t
 * @annotation ["scriptable","design","widget"]
 * 峰峰值柱条序列控件，作为chart\_view的一个子部件使用，可同时显示最大最小值，可实现类似k线图的效果。
 *
 * 在xml中使用"bar\_series\_minmax"标签创建峰峰值柱条序列。
 * value属性的格式为"最小值,最大值,最小值,最大值,..."。
 * 如：
 *
 * ```xml
 * <!-- ui -->
 * <bar_series_minmax w="100" h="100" capacity="10" value_animation="500"
 * value="15,75,40,60,80,140,100,120,25,90,40,140,60,80,100,120,0,20,44,98"/>
 * ```
 *
 * 可用通过style来设置控件的显示风格。其中
 * fg_image 用于设置柱条的填充图片；
 * fg_image_draw_type 用于设置柱条的填充图片的显示方式；
 * fg_color 用于设置柱条的填充颜色；
 * border_color 用于设置柱条的边框颜色；
 * border_width 用于设置柱条的边框宽度；
 * round_radius 用于设置柱条的圆角；
 * margin_right、margin_right 用于设置垂直柱条两侧的留白边距；
 * margin_top、margin_bottom 用于设置水平柱条两侧的留白边距。
 * 如：
 *
 * ```xml
 * <!-- style -->
 * <bar_series_minmax>
 *   <style name="default">
 *     <normal margin_left="4" margin_right="4" fg_color="#0070c0" round_radius="20"/>
 *   </style>
 * </bar_series_minmax>
 * ```
 */

/**
 * @method bar_series_minmax_create
 * 创建bar_series_minmax对象（同时显示最大最小值）
 * @annotation ["constructor", "scriptable"]
 * @param {widget_t*} parent 父控件
 * @param {xy_t} x x坐标
 * @param {xy_t} y y坐标
 * @param {wh_t} w 宽度
 * @param {wh_t} h 高度
 *
 * @return {widget_t*} 对象。
 */
widget_t* bar_series_minmax_create(widget_t* widget, xy_t x, xy_t y, wh_t w, wh_t h);

/**
 * @method widget_is_bar_series_minmax
 * 判断当前控件是否为bar_series_minmax控件。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget 控件对象。
 *
 * @return {bool_t} 返回当前控件是否为tooltip控件。
 */
bool_t widget_is_bar_series_minmax(widget_t* widget);

/*public for subclass and runtime type check*/
TK_EXTERN_VTABLE(bar_series_minmax);

END_C_DECLS

#endif /*TK_BAR_SERIES_MINMAX_H*/
