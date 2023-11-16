/**
 * File:   chart_view.h
 * Author: AWTK Develop Team
 * Brief:  chart view
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

#ifndef TK_CHART_VIEW_H
#define TK_CHART_VIEW_H

#include "base/widget.h"
#include "base/widget_animator.h"

BEGIN_C_DECLS

/**
 * @class chart_view_t
 * @parent widget_t
 * @annotation ["scriptable","design","widget"]
 * 图表容器控件。
 *
 * 在xml中使用"chart\_view"标签创建图表容器控件，内部可添加axis、series、tooltip等控件。如：
 *
 * ```xml
 * <!-- ui -->
 * <chart_view name="chartview" x="6%" y="13%" w="400" h="200">
 *   <x_axis name="x_axis" axis_type="value" min="0" max="9" tick="{show:true}"
 * split_line="{show:true}" label="{show:true}" data="[1,2,3,4,5,6,7,8,9,10]"/>
 *   <y_axis name="y_axis" axis_type="value" min="0" max="140" tick="{show:true}"
 * split_line="{show:true}" label="{show:true}" data="[0,20,40,60,80,100,120,140]"/>
 *   <bar_series name="bar_series" text="s1" capacity="10" value_animation="500"
 * value="15,75,40,60,140,80,100,120,25,90"/>
 *   <line_series name="line_series" text="s2" capacity="10" value_animation="500"
 * line="{smooth:true}" area="{show:true}" symbol="{show:true}"
 * value="15,75,40,60,140,80,100,120,25,90"/>
 *   <tooltip name="tooltip" />
 * </chart_view>
 * ```
 *
 * 可用通过style来设置控件的显示风格，如字体的大小和颜色等等。如：
 *
 * ```xml
 * <!-- style -->
 * <chart_view>
 *   <style name="default">
 *     <normal margin_bottom="20" margin_left="20" margin_right="10" margin_top="10"/>
 *   </style>
 * </chart_view>
 * ```
 */
typedef struct _chart_view_t {
  widget_t widget;

  /**
   * @property {int32_t} top_series
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * 位于顶层的series的索引。
   */
  int32_t top_series;

  /* private */
  uint32_t* last_series_offset;
  uint32_t series_cnt;
  bool_t pressed;
  bool_t dragged;
  point_t down;
  uint8_t need_relayout_axes : 1;
} chart_view_t;

/**
 * @method chart_view_create
 * 创建chart_view_对象
 * @annotation ["constructor", "scriptable"]
 * @param {widget_t*} parent 父控件
 * @param {xy_t} x x坐标
 * @param {xy_t} y y坐标
 * @param {wh_t} w 宽度
 * @param {wh_t} h 高度
 *
 * @return {widget_t*} 对象。
 */
widget_t* chart_view_create(widget_t* parent, xy_t x, xy_t y, wh_t w, wh_t h);

/**
 * @method chart_view_set_top_series
 * 设置需置顶的series的索引。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget 控件对象。
 * @param {int32_t} index series索引。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t chart_view_set_top_series(widget_t* widget, int32_t index);

/**
 * @method chart_view_cast
 * 转换chart_view对象(供脚本语言使用)。
 * @annotation ["cast", "scriptable"]
 * @param {widget_t*} widget chart_view对象。
 *
 * @return {widget_t*} chart_view对象。
 */
widget_t* chart_view_cast(widget_t* widget);

/**
 * @enum widget_prop_t
 * @annotation ["scriptable", "string"]
 * @prefix CHART_VIEW_PROP_
 * 控件的属性。
 */

/**
 * @const CHART_VIEW_PROP_TOP_SERIES
 * 位于顶层的series的索引
 */
#define CHART_VIEW_PROP_TOP_SERIES "top_series"

/**
 * @enum widget_type_t
 * @annotation ["scriptable", "string"]
 * @prefix WIDGET_TYPE_
 * 控件的类型。
 */

/**
 * @const WIDGET_TYPE_CHART_VIEW
 * 图表容器控件。
 */
#define WIDGET_TYPE_CHART_VIEW "chart_view"

#define CHART_VIEW(widget) ((chart_view_t*)(chart_view_cast(WIDGET(widget))))

/*public for subclass and runtime type check*/
TK_EXTERN_VTABLE(chart_view);

END_C_DECLS

#endif /*TK_CHART_VIEW_H*/
