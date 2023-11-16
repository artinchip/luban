/**
 * File:   bar_series_minmax.c
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

#include "tkc/utils.h"
#include "axis.h"
#include "series_p.h"
#include "bar_series_minmax.h"
#include "chart_animator.h"

extern ret_t bar_series_get_prop(widget_t* widget, const char* name, value_t* v);
extern ret_t bar_series_set_prop_internal(widget_t* widget, const char* name, const value_t* v);
extern ret_t bar_series_on_paint_internal(widget_t* widget, canvas_t* c, float_t ox, float_t oy,
                                          object_t* fifo, uint32_t index, uint32_t size,
                                          rect_t* clip_rect, bool_t minmax);
extern ret_t bar_series_on_paint_self(widget_t* widget, canvas_t* c);
extern int32_t bar_series_index_of_point_in(widget_t* widget, xy_t x, xy_t y, bool_t is_local);
extern ret_t bar_series_to_local(widget_t* widget, uint32_t index, point_t* p);
extern ret_t bar_series_on_destroy(widget_t* widget);
extern widget_t* bar_series_create_internal(widget_t* parent, xy_t x, xy_t y, wh_t w, wh_t h,
                                            const widget_vtable_t* wvt, const series_vtable_t* svt);

static ret_t bar_series_minmax_set_value(widget_t* widget, const char* value) {
  const char* token = NULL;
  tokenizer_t tokenizer;
  series_data_minmax_t v;
  object_t* fifo;
  uint32_t capacity;
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && value != NULL, RET_BAD_PARAMS);

  capacity = widget_get_prop_int(widget, SERIES_PROP_CAPACITY, 0);
  fifo = series_fifo_default_create(capacity, sizeof(series_data_minmax_t));
  return_value_if_fail(fifo != NULL, RET_OOM);

  tokenizer_init(&tokenizer, value, strlen(value), ",");

  while (tokenizer_has_more(&tokenizer) && SERIES_FIFO_GET_SIZE(fifo) < capacity) {
    token = tokenizer_next(&tokenizer);
    v.min = tk_atof(token);

    if (tokenizer_has_more(&tokenizer)) {
      token = tokenizer_next(&tokenizer);
      v.max = tk_atof(token);

      series_fifo_push(fifo, &v);
    }
  }

  series_set(widget, 0, SERIES_FIFO_DEFAULT(fifo)->buffer, SERIES_FIFO_GET_SIZE(fifo));

  OBJECT_UNREF(fifo);
  tokenizer_deinit(&tokenizer);

  return RET_OK;
}

static ret_t bar_series_minmax_set_prop(widget_t* widget, const char* name, const value_t* v) {
  ret_t ret = bar_series_set_prop_internal(widget, name, v);

  if (ret == RET_NOT_FOUND) {
    if (tk_str_eq(name, WIDGET_PROP_VALUE)) {
      return bar_series_minmax_set_value(widget, value_str(v));
    }
  }

  return ret;
}

static ret_t bar_series_minmax_on_paint(widget_t* widget, canvas_t* c, float_t ox, float_t oy,
                                        object_t* fifo, uint32_t index, uint32_t size,
                                        rect_t* clip_rect) {
  return bar_series_on_paint_internal(widget, c, ox, oy, fifo, index, size, clip_rect, TRUE);
}

static ret_t bar_series_minmax_tooltip_format(void* ctx, const void* data, wstr_t* str) {
  wstr_t temp;
  const wchar_t* title;
  widget_t* widget = WIDGET(ctx);
  series_data_minmax_t* d = (series_data_minmax_t*)(data);
  return_value_if_fail(widget != NULL && d != NULL && str != NULL, RET_BAD_PARAMS);

  wstr_init(str, 0);

  title = series_get_title(widget);
  if (title != NULL && wcslen(title) > 0) {
    wstr_append(str, title);
    wstr_append(str, L": ");
  }

  wstr_init(&temp, 0);
  wstr_from_float(&temp, d->min);
  wstr_append(str, temp.str);
  wstr_append(str, L" ");
  wstr_from_float(&temp, d->max);
  wstr_append(str, temp.str);
  wstr_reset(&temp);

  return RET_OK;
}

static const char* s_bar_series_minmax_properties[] = {SERIES_PROP_FIFO,
                                                       SERIES_PROP_COVERAGE,
                                                       SERIES_PROP_DISPLAY_MODE,
                                                       SERIES_PROP_VALUE_ANIMATION,
                                                       SERIES_PROP_TITLE,
                                                       SERIES_PROP_BAR_OVERLAP,
                                                       NULL};

static const series_draw_data_info_t s_series_p_minmax_draw_data_info = {
    .unit_size = sizeof(series_data_draw_minmax_t),
    .compare_in_axis1 = series_p_minmax_draw_data_compare_x,
    .compare_in_axis2 = series_p_minmax_draw_data_compare_y,
    .min_axis1 = series_p_minmax_draw_data_min_x,
    .min_axis2 = series_p_minmax_draw_data_min_y,
    .max_axis1 = series_p_minmax_draw_data_max_x,
    .max_axis2 = series_p_minmax_draw_data_max_y,
    .get_axis1 = series_p_minmax_draw_data_get_x,
    .get_axis2 = series_p_minmax_draw_data_get_y,
    .set_as_axis21 = series_p_minmax_draw_data_set_yx,
    .set_as_axis12 = series_p_minmax_draw_data_set_xy};

static const series_vtable_t s_bar_series_minmax_internal_vtable = {
    .count = series_p_count,
    .rset = series_p_rset,
    .push = series_p_push,
    .clear = series_p_clear,
    .at = series_p_at,
    .get_current = series_p_get_current,
    .is_point_in = series_p_is_point_in,
    .index_of_point_in = bar_series_index_of_point_in,
    .to_local = bar_series_to_local,
    .set = series_p_set,
    .on_paint = bar_series_minmax_on_paint,
    .draw_data_info = &s_series_p_minmax_draw_data_info};

TK_DECL_VTABLE(bar_series_minmax) = {.size = sizeof(bar_series_t),
                                     .type = WIDGET_TYPE_BAR_SERIES_MINMAX,
                                     .parent = TK_PARENT_VTABLE(series),
                                     .clone_properties = s_bar_series_minmax_properties,
                                     .persistent_properties = s_bar_series_minmax_properties,
                                     .create = bar_series_minmax_create,
                                     .on_paint_self = bar_series_on_paint_self,
                                     .set_prop = bar_series_minmax_set_prop,
                                     .get_prop = bar_series_get_prop,
                                     .on_destroy = bar_series_on_destroy};

widget_t* bar_series_minmax_create(widget_t* parent, xy_t x, xy_t y, wh_t w, wh_t h) {
  widget_t* widget = bar_series_create_internal(
      parent, x, y, w, h, TK_REF_VTABLE(bar_series_minmax), &s_bar_series_minmax_internal_vtable);
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL, NULL);

  object_t* fifo = series_fifo_default_create(10, sizeof(series_data_minmax_t));
  series_p_set_fifo(widget, fifo);

  series->animator_create = chart_animator_fifo_minmax_value_create;
  series->tooltip_format = bar_series_minmax_tooltip_format;
  series->tooltip_format_ctx = widget;

  return widget;
}

bool_t widget_is_bar_series_minmax(widget_t* widget) {
  return WIDGET_IS_INSTANCE_OF(widget, bar_series_minmax);
}
