/**
 * File:   line_series_colorful.c
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

#include "tkc/utils.h"
#include "axis.h"
#include "series_p.h"
#include "line_series_colorful.h"
#include "chart_animator.h"

extern ret_t line_series_get_prop(widget_t* widget, const char* name, value_t* v);
extern ret_t line_series_set_prop_internal(widget_t* widget, const char* name, const value_t* v);
extern ret_t line_series_draw_one_series(widget_t* widget, canvas_t* c, float_t ox, float_t oy,
                                         object_t* fifo, uint32_t index, uint32_t size,
                                         rect_t* clip_rect, series_p_draw_line_t draw_line,
                                         series_p_draw_line_area_t draw_area,
                                         series_p_draw_smooth_line_t draw_smooth_line,
                                         series_p_draw_smooth_line_area_t draw_smooth_area,
                                         series_p_draw_symbol_t draw_symbol);
extern ret_t line_series_on_paint_self(widget_t* widget, canvas_t* c);
extern ret_t line_series_on_destroy(widget_t* widget);
extern widget_t* line_series_create_internal(widget_t* parent, xy_t x, xy_t y, wh_t w, wh_t h,
                                             const widget_vtable_t* wvt,
                                             const series_vtable_t* svt);

static ret_t line_series_colorful_set_value(widget_t* widget, const char* value) {
  const char* token = NULL;
  tokenizer_t tokenizer;
  series_data_colorful_t v;
  object_t* fifo;
  uint32_t capacity;
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && value != NULL, RET_BAD_PARAMS);

  v.c = color_init(0, 0, 0, 0xff);
  capacity = widget_get_prop_int(widget, SERIES_PROP_CAPACITY, 0);
  fifo = series_fifo_default_create(capacity, sizeof(series_data_colorful_t));
  return_value_if_fail(fifo != NULL, RET_OOM);

  tokenizer_init(&tokenizer, value, strlen(value), ",");

  while (tokenizer_has_more(&tokenizer) && SERIES_FIFO_GET_SIZE(fifo) < capacity) {
    token = tokenizer_next(&tokenizer);
    if (token[0] == '#') {
      v.c = color_parse(token);
    } else {
      v.v = tk_atof(token);
      series_fifo_push(fifo, &v);
    }
  }

  series_set(widget, 0, SERIES_FIFO_DEFAULT(fifo)->buffer, SERIES_FIFO_GET_SIZE(fifo));

  OBJECT_UNREF(fifo);
  tokenizer_deinit(&tokenizer);

  return RET_OK;
}

static ret_t line_series_colorful_set_prop(widget_t* widget, const char* name, const value_t* v) {
  ret_t ret = line_series_set_prop_internal(widget, name, v);

  if (ret == RET_NOT_FOUND) {
    if (tk_str_eq(name, WIDGET_PROP_VALUE)) {
      return line_series_colorful_set_value(widget, value_str(v));
    }
  }

  return ret;
}

static ret_t line_series_colorful_on_paint(widget_t* widget, canvas_t* c, float_t ox, float_t oy,
                                           object_t* fifo, uint32_t index, uint32_t size,
                                           rect_t* clip_rect) {
  return line_series_draw_one_series(
      widget, c, ox, oy, fifo, index, size, clip_rect, series_p_draw_line_colorful,
      series_p_draw_line_area_colorful, series_p_draw_smooth_line_colorful,
      series_p_draw_smooth_line_area_colorful, series_p_draw_symbol_colorful);
}

static const char* s_line_series_colorful_properties[] = {SERIES_PROP_FIFO,
                                                          SERIES_PROP_COVERAGE,
                                                          SERIES_PROP_DISPLAY_MODE,
                                                          SERIES_PROP_VALUE_ANIMATION,
                                                          SERIES_PROP_TITLE,
                                                          SERIES_PROP_LINE_SHOW,
                                                          SERIES_PROP_LINE_SMOOTH,
                                                          SERIES_PROP_LINE_AREA_SHOW,
                                                          SERIES_PROP_SYMBOL_SIZE,
                                                          SERIES_PROP_SYMBOL_SHOW,
                                                          NULL};

static const series_draw_data_info_t s_series_p_colorful_draw_data_info = {
    .unit_size = sizeof(series_data_draw_colorful_t),
    .compare_in_axis1 = series_p_colorful_draw_data_compare_x,
    .compare_in_axis2 = series_p_colorful_draw_data_compare_y,
    .min_axis1 = series_p_colorful_draw_data_min_x,
    .min_axis2 = series_p_colorful_draw_data_min_y,
    .max_axis1 = series_p_colorful_draw_data_max_x,
    .max_axis2 = series_p_colorful_draw_data_max_y,
    .get_axis1 = series_p_colorful_draw_data_get_x,
    .get_axis2 = series_p_colorful_draw_data_get_y,
    .set_as_axis21 = series_p_colorful_draw_data_set_yx,
    .set_as_axis12 = series_p_colorful_draw_data_set_xy};

static const series_vtable_t s_line_series_colorful_internal_vtable = {
    .count = series_p_count,
    .rset = series_p_rset,
    .push = series_p_push,
    .at = series_p_at,
    .clear = series_p_clear,
    .get_current = series_p_get_current,
    .is_point_in = series_p_is_point_in,
    .index_of_point_in = series_p_index_of_point_in,
    .to_local = series_p_to_local,
    .set = series_p_set,
    .on_paint = line_series_colorful_on_paint,
    .draw_data_info = &s_series_p_colorful_draw_data_info};

TK_DECL_VTABLE(line_series_colorful) = {.size = sizeof(line_series_t),
                                        .type = WIDGET_TYPE_LINE_SERIES_COLORFUL,
                                        .parent = TK_PARENT_VTABLE(series),
                                        .clone_properties = s_line_series_colorful_properties,
                                        .persistent_properties = s_line_series_colorful_properties,
                                        .create = line_series_colorful_create,
                                        .on_paint_self = line_series_on_paint_self,
                                        .set_prop = line_series_colorful_set_prop,
                                        .get_prop = line_series_get_prop,
                                        .on_destroy = line_series_on_destroy};

widget_t* line_series_colorful_create(widget_t* parent, xy_t x, xy_t y, wh_t w, wh_t h) {
  widget_t* widget =
      line_series_create_internal(parent, x, y, w, h, TK_REF_VTABLE(line_series_colorful),
                                  &s_line_series_colorful_internal_vtable);
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL, NULL);

  object_t* fifo = series_fifo_default_create(10, sizeof(series_data_colorful_t));
  series_p_set_fifo(widget, fifo);

  series->animator_create = chart_animator_fifo_colorful_value_create;

  return widget;
}

bool_t widget_is_line_series_colorful(widget_t* widget) {
  return WIDGET_IS_INSTANCE_OF(widget, line_series_colorful);
}
