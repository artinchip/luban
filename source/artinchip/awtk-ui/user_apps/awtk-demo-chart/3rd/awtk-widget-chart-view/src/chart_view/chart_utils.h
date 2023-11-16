/**
 * File:   chart_utils.h
 * Author: AWTK Develop Team
 * Brief:  chart utils
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

#ifndef TK_CHART_UTILS_H
#define TK_CHART_UTILS_H

#include "base/widget.h"
#include "tkc/str.h"
#include "tkc/value.h"
#include "tkc/color_parser.h"
#include "tkc/tokenizer.h"

BEGIN_C_DECLS

#ifndef return_if_true
#define return_if_true(p) \
  if ((p)) {              \
    return;               \
  }
#endif

#ifndef continue_if_true
#define continue_if_true(p) \
  if ((p)) {                \
    continue;               \
  }
#endif

#ifndef break_if_true
#define break_if_true(p) \
  if ((p)) {             \
    break;               \
  }
#endif

#ifndef return_value_if_true
#define return_value_if_true(p, value) \
  if ((p)) {                           \
    return (value);                    \
  }
#endif

color_t chart_utils_value_color(const value_t* v);

typedef void (*chart_utils_on_object_parse_t)(void* ctx, const char* name, const value_t* v);
ret_t chart_utils_object_parse(chart_utils_on_object_parse_t on_parse, void* ctx,
                               const char* object);

rect_t* chart_utils_rect_fix(rect_t* rect);

ret_t chart_utils_draw_a_symbol(canvas_t* c, rect_t* r, color_t bg, color_t bd,
                                uint32_t border_width, uint32_t radius, bitmap_t* img,
                                image_draw_type_t draw_type);

END_C_DECLS

#endif /*TK_CHART_UTILS_H*/
