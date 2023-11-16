/**
 * File:   tooltip_types.h
 * Author: AWTK Develop Team
 * Brief:  tooltip types
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

#ifndef TK_TOOLTIP_TYPES_H
#define TK_TOOLTIP_TYPES_H

#include "base/widget.h"
#include "chart_utils.h"

BEGIN_C_DECLS

/**
 * @class tooltip_line_params_t
 * 指示序列点位置的线。
 */
typedef struct _tooltip_line_params_t {
  /**
   * 是否显示。
   */
  uint8_t show : 1;
} tooltip_line_params_t;

/**
 * @class tooltip_symbol_params_t
 * 标记点。
 */
typedef struct _tooltip_symbol_params_t {
  /**
   * 大小
   */
  float_t size;
  /**
   * 是否显示。
   */
  uint8_t show : 1;
} tooltip_symbol_params_t;

/**
 * @class tooltip_tip_params_t
 * 提示文本。
 */
typedef struct _tooltip_tip_params_t {
  /**
   * 是否显示。
   */
  uint8_t show : 1;
} tooltip_tip_params_t;

/**
 * @enum widget_prop_t
 * @annotation ["scriptable", "string"]
 * @prefix TOOLTIP_PROP_
 * 控件的属性。
 */

/**
 * @const TOOLTIP_PROP_DOWN_X
 * pointer按下时的x坐标
 */
#define TOOLTIP_PROP_DOWN_X "down_x"

/**
 * @const TOOLTIP_PROP_DOWN_Y
 * pointer按下时的y坐标
 */
#define TOOLTIP_PROP_DOWN_Y "down_y"

/**
 * @const TOOLTIP_PROP_LINE
 * 指示序列点位置的线
 */
#define TOOLTIP_PROP_LINE "line"

/**
 * @const TOOLTIP_PROP_LINE_SHOW
 * 指示序列点位置的线是否显示
 */
#define TOOLTIP_PROP_LINE_SHOW "line:show"

/**
 * @const TOOLTIP_PROP_SYMBOL
 * 标记点
 */
#define TOOLTIP_PROP_SYMBOL "symbol"

/**
 * @const TOOLTIP_PROP_SYMBOL_SHOW
 * 标记点是否显示
 */
#define TOOLTIP_PROP_SYMBOL_SHOW "symbol:show"

/**
 * @const TOOLTIP_PROP_TIP
 * 提示文本
 */
#define TOOLTIP_PROP_TIP "tip"

/**
 * @const TOOLTIP_PROP_TIP_SHOW
 * 提示文本是否显示
 */
#define TOOLTIP_PROP_TIP_SHOW "tip:show"

/**
 * @enum widget_type_t
 * @annotation ["scriptable", "string"]
 * @prefix WIDGET_TYPE_
 * 控件的类型。
 */

/**
 * @const WIDGET_TYPE_TOOLTIP
 * 提示信息控件。
 */
#define WIDGET_TYPE_TOOLTIP "tooltip"

/**
 * @enum style_tooltip_t
 * @prefix STYLE_ID_TOOLTIP_
 * @annotation ["scriptable", "string"]
 * style常量定义。
 */
/**
 * @const STYLE_ID_TOOLTIP_SYMBOL_BORDER_COLOR
 * 标记点的边框颜色
 */
#define STYLE_ID_TOOLTIP_SYMBOL_BORDER_COLOR "symbol_border_color"

/**
 * @const STYLE_ID_TOOLTIP_SYMBOL_BORDER_WIDTH
 * 标记点的边框宽度
 */
#define STYLE_ID_TOOLTIP_SYMBOL_BORDER_WIDTH "symbol_border_width"

/**
 * @const STYLE_ID_TOOLTIP_SYMBOL_BG_COLOR
 * 标记点的颜色
 */
#define STYLE_ID_TOOLTIP_SYMBOL_BG_COLOR "symbol_bg_color"

/**
 * @const STYLE_ID_TOOLTIP_SYMBOL_ROUND_RADIUS
 * 标记点的圆角半径
 */
#define STYLE_ID_TOOLTIP_SYMBOL_ROUND_RADIUS "symbol_round_radius"

/**
 * @const STYLE_ID_TOOLTIP_SYMBOL_BG_IMAGE
 * 标记点的背景图片
 */
#define STYLE_ID_TOOLTIP_SYMBOL_BG_IMAGE "symbol_bg_image"

/**
 * @const STYLE_ID_TOOLTIP_SYMBOL_BG_IMAGE_DRAW_TYPE
 * 标记点的背景图片的显示方式
 */
#define STYLE_ID_TOOLTIP_SYMBOL_BG_IMAGE_DRAW_TYPE "symbol_bg_image_draw_type"

END_C_DECLS

#endif /*TK_TOOLTIP_TYPES_H*/
