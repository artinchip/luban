/**
 * File:   bar_series.h
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

#ifndef TK_BAR_SERIES_H
#define TK_BAR_SERIES_H

#include "series.h"

BEGIN_C_DECLS

/**
 * @class bar_series_t
 * @parent series_t
 * @annotation ["scriptable","design","widget"]
 * 柱条序列控件，作为chart\_view的一个子部件使用，用于实现柱状图。
 *
 * 在xml中使用"bar\_series"标签创建柱条序列控件。
 * value属性的格式为"采样值,采样值,..."。
 * 如：
 *
 * ```xml
 * <!-- ui -->
 * <bar_series w="100" h="100" capacity="10" value_animation="500"
 * value="15,75,40,60,140,80,100,120,25,90"/>
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
 * <bar_series>
 *   <style name="default">
 *     <normal margin_left="4" margin_right="4" fg_color="#0070c0"/>
 *   </style>
 * </bar_series>
 * ```
 */
typedef struct _bar_series_t {
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
   * 标示序列值的轴（名称），为空时默认为检索到的第一个y_axis。
   */
  char* value_axis;
  /**
   * @property {series_bar_params_t} bar
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable:custom"]
   * 柱条的参数，比如"{overlap:true}"。
   */
  series_bar_params_t bar;
} bar_series_t;

/**
 * @method bar_series_create
 * 创建bar_series对象
 * @annotation ["constructor", "scriptable"]
 * @param {widget_t*} parent 父控件
 * @param {xy_t} x x坐标
 * @param {xy_t} y y坐标
 * @param {wh_t} w 宽度
 * @param {wh_t} h 高度
 *
 * @return {widget_t*} 对象。
 */
widget_t* bar_series_create(widget_t* widget, xy_t x, xy_t y, wh_t w, wh_t h);

/**
 * @method bar_series_cast
 * 转换为bar_series对象(供脚本语言使用)。
 * @annotation ["cast", "scriptable"]
 * @param {widget_t*} widget bar_series对象。
 *
 * @return {widget_t*} bar_series对象。
 */
widget_t* bar_series_cast(widget_t* series);

#define BAR_SERIES(widget) ((bar_series_t*)(bar_series_cast(WIDGET(widget))))

/*public for subclass and runtime type check*/
TK_EXTERN_VTABLE(bar_series);

END_C_DECLS

#endif /*TK_BAR_SERIES_H*/
