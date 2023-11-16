/**
 * File:   x_axis.h
 * Author: AWTK Develop Team
 * Brief:  x axis
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

#ifndef TK_X_AXIS_H
#define TK_X_AXIS_H

#include "axis.h"

BEGIN_C_DECLS

/**
 * @class x_axis_t
 * @parent axis_t
 * @annotation ["scriptable","design","widget"]
 * x 轴控件，作为chart\_view的一个子部件使用。
 *
 * 在xml中使用"x\_axis"标签创建 x 轴控件。如：
 *
 * ```xml
 * <!-- ui -->
 * <x_axis w="100" h="2" axis_type="value" min="0" max="9" tick="{show:true}"
 * split_line="{show:true}" label="{show:true}" data="[1,2,3,4,5,6,7,8,9,10]"/>
 * ```
 *
 * 可用通过style来设置控件的显示风格。其中，
 * spacer 用于设置刻度值与轴线之间的间距；
 * font_name 用于设置刻度值的字体；
 * font_size 用于设置刻度值的字体大小；
 * text_color 用于设置刻度值的文本颜色；
 * fg_color 用于设置轴线的颜色；
 * fg_image 用于设置轴线的图片；
 * fg_image_draw_type 用于设置轴线的图片的显示方式；
 * tick_color 用于设置刻度线的颜色；
 * tick_image 用于设置刻度线的图片；
 * tick_image_draw_type 用于设置刻度线的图片的显示方式；
 * split_line_color 用于设置分割线的颜色；
 * split_line_image 用于设置分割线的图片；
 * split_line_image_draw_type 用于设置分割线的图片的显示方式。
 * 如：
 *
 * ```xml
 * <!-- style -->
 * <x_axis>
 *   <style name="default">
 *     <normal text_color="#444444" font_size="16" split_line_color="#c2c2c2" tick_color="#c2c2c2"
 * fg_color="#c2c2c2"/>
 *   </style>
 * </x_axis>
 * ```
 */
typedef struct _x_axis_t {
  axis_t base;
  /**
   * @property {bool_t} x_defined
   * @annotation ["readable"]
   * x坐标是否确定。
   */
  uint8_t x_defined : 1;
  /**
   * @property {bool_t} w_defined
   * @annotation ["readable"]
   * w宽度是否确定。
   */
  uint8_t w_defined : 1;
} x_axis_t;

/**
 * @method x_axis_create
 * 创建axis对象
 * @annotation ["constructor", "scriptable"]
 * @param {widget_t*} parent 父控件
 * @param {xy_t} x x坐标
 * @param {xy_t} y y坐标
 * @param {wh_t} w 宽度
 * @param {wh_t} h 高度
 *
 * @return {widget_t*} 对象。
 */
widget_t* x_axis_create(widget_t* widget, xy_t x, xy_t y, wh_t w, wh_t h);

/**
 * @method x_axis_cast
 * 转换为axis对象(供脚本语言使用)。
 * @annotation ["cast", "scriptable"]
 * @param {widget_t*} widget axis对象。
 *
 * @return {widget_t*} axis对象。
 */
widget_t* x_axis_cast(widget_t* widget);

#define X_AXIS(widget) ((x_axis_t*)(x_axis_cast(WIDGET(widget))))

/*public for subclass and runtime type check*/
TK_EXTERN_VTABLE(x_axis);

END_C_DECLS

#endif /*TK_X_AXIS_H*/
