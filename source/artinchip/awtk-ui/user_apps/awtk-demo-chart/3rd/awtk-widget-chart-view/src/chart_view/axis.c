/**
 * File:   axis.c
 * Author: AWTK Develop Team
 * Brief:  axis
 *
 * Copyright (c) 2018 - 2018  Guangzhou ZHIYUAN Electronics Co.,Ltd.
 *
 * This program is dirich_text_nodeibuted in the hope that it will be useful,
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

#include "axis.h"
#include "base/style_factory.h"
#include "tkc/utils.h"
#include "tkc/wstr.h"
#include "axis_p.h"
#include "series.h"

axis_data_t* axis_data_create(float_t tick, const char* text) {
  axis_data_t* d = (axis_data_t*)TKMEM_ZALLOC(axis_data_t);
  return_value_if_fail(d != NULL, NULL);

  d->tick = tick;
  wstr_init(&(d->text), 0);
  if (text) {
    wstr_set_utf8(&(d->text), text);
  }

  return d;
}

ret_t axis_data_destroy(axis_data_t* data) {
  return_value_if_fail(data != NULL, RET_BAD_PARAMS);

  wstr_reset(&(data->text));
  TKMEM_FREE(data);

  return RET_OK;
}

ret_t axis_on_destroy(widget_t* widget) {
  axis_t* axis = AXIS(widget);
  return_value_if_fail(axis != NULL, RET_BAD_PARAMS);

  if (axis->data != NULL) {
    darray_destroy(axis->data);
  }

  wstr_reset(&(axis->title.text));
  wstr_reset(&(axis->time.format));

  return RET_OK;
}

ret_t axis_set_range(widget_t* widget, float_t min, float_t max) {
  axis_t* axis = AXIS(widget);
  return_value_if_fail(axis != NULL, RET_BAD_PARAMS);

  axis->min = tk_min(min, max);
  axis->max = tk_max(min, max);
  axis->need_update_data = !axis->data_fixed;

  return widget_invalidate(widget, NULL);
}

float_t axis_get_range(widget_t* widget, bool_t is_series_axis) {
  axis_t* axis = AXIS(widget);
  return_value_if_fail(axis != NULL, 0.0);

  if (axis->axis_type != AXIS_TYPE_CATEGORY && axis->max != axis->min) {
    return tk_abs(axis->max - axis->min) + (is_series_axis ? 1 : 0);
  } else {
    return (float_t)(axis->data->size);
  }
}

ret_t axis_set_offset(widget_t* widget, float_t offset, bool_t percent) {
  axis_t* axis = AXIS(widget);
  return_value_if_fail(axis != NULL, RET_BAD_PARAMS);

  axis->offset = offset;
  axis->offset_percent = percent;

  return RET_OK;
}

static ret_t axis_calc_series_rect(widget_t* widget, rect_t* r) {
  style_t* style;
  int32_t margin = 0;
  int32_t margin_left = 0;
  int32_t margin_right = 0;
  int32_t margin_top = 0;
  int32_t margin_bottom = 0;
  return_value_if_fail(widget != NULL && r != NULL, RET_BAD_PARAMS);

  style = widget->astyle;
  return_value_if_fail(style != NULL, RET_BAD_PARAMS);

  margin = style_get_int(style, STYLE_ID_MARGIN, 0);
  margin_top = style_get_int(style, STYLE_ID_MARGIN_TOP, margin);
  margin_left = style_get_int(style, STYLE_ID_MARGIN_LEFT, margin);
  margin_right = style_get_int(style, STYLE_ID_MARGIN_RIGHT, margin);
  margin_bottom = style_get_int(style, STYLE_ID_MARGIN_BOTTOM, margin);

  r->x = margin_left;
  r->y = margin_top;
  r->w = widget->w - margin_left - margin_right;
  r->h = widget->h - margin_top - margin_bottom;
  return RET_OK;
}

float_t axis_get_offset(widget_t* widget, float_t defval) {
  axis_t* axis = AXIS(widget);
  return_value_if_fail(axis != NULL, defval);

  if (!axis->offset_percent) {
    return axis->offset;
  } else {
    rect_t r = rect_init(0, 0, 0, 0);
    axis_calc_series_rect(widget->parent, &r);
    if (tk_str_eq(widget_get_type(widget), WIDGET_TYPE_X_AXIS)) {
      return (axis->offset * r.h / 100);
    } else {
      return (axis->offset * r.w / 100);
    }
  }
}

ret_t axis_set_data(widget_t* widget, const char* data) {
  const char* token = NULL;
  tokenizer_t tokenizer;
  axis_t* axis = AXIS(widget);
  return_value_if_fail(axis != NULL && axis->data != NULL && data != NULL, RET_BAD_PARAMS);

  darray_clear(axis->data);

  tokenizer_init(&tokenizer, data, strlen(data), "[,]");
  while (tokenizer_has_more(&tokenizer)) {
    token = tokenizer_next(&tokenizer);
    darray_push(axis->data, axis_data_create(0, token));
  }
  tokenizer_deinit(&tokenizer);

  axis->need_update_data = TRUE;

  return RET_OK;
}

ret_t axis_set_data_from_series(widget_t* widget, axis_data_from_series_t from_series, void* ctx) {
  axis_t* axis = AXIS(widget);
  return_value_if_fail(axis != NULL, RET_BAD_PARAMS);

  axis->data_from_series = from_series;
  axis->data_from_series_ctx = ctx;
  axis->need_update_data = TRUE;
  return widget_invalidate(widget, NULL);
}

ret_t axis_set_need_update_data(widget_t* widget) {
  axis_t* axis = AXIS(widget);
  return_value_if_fail(axis != NULL, RET_BAD_PARAMS);

  axis->need_update_data = TRUE;
  return widget_invalidate(widget, NULL);
}

static widget_t* axis_lookup_indicated_series(widget_t* widget) {
  return_value_if_fail(widget != NULL && widget->parent != NULL, NULL);

  WIDGET_FOR_EACH_CHILD_BEGIN(widget->parent, iter, i)
  if (widget_is_series(iter)) {
    if (widget_get_prop_pointer(iter, SERIES_PROP_SERIES_AXIS_OBJ) == widget) {
      return iter;
    }
  }
  WIDGET_FOR_EACH_CHILD_END()

  return NULL;
}

ret_t axis_update_data_from_series(widget_t* widget) {
  uint32_t b = 0;
  uint32_t e = 0;
  uint32_t m = 0;
  uint32_t size = 0;
  uint32_t recent_index = 0;
  float_t interval;
  widget_t* series;
  object_t* fifo;
  axis_t* axis = AXIS(widget);
  return_value_if_fail(axis != NULL && axis->data != NULL, RET_BAD_PARAMS);

  series = axis_lookup_indicated_series(widget);
  return_value_if_fail(series != NULL, RET_BAD_PARAMS);

  fifo = SERIES(series)->fifo;
  return_value_if_fail(fifo != NULL, RET_BAD_PARAMS);

  size = SERIES_FIFO_GET_SIZE(fifo);
  if (size > 0) {
    return_value_if_fail(series_get_current(series, &b, &e, &m) == RET_OK, RET_FAIL);

    darray_clear(axis->data);

    interval = axis_measure_series_interval(widget);
    recent_index = tk_max(series_count(series) - 1, 0);
    axis->data_from_series(widget, recent_index, b, e, m, interval, axis->data,
                           axis->data_from_series_ctx);
  }

  return RET_OK;
}

ret_t axis_on_paint_before(widget_t* widget, canvas_t* c) {
  ret_t ret = RET_NOT_IMPL;
  axis_t* axis = AXIS(widget);
  return_value_if_fail(axis != NULL && axis->vt != NULL, RET_BAD_PARAMS);

  if (axis->vt->on_paint_before) {
    ret = axis->vt->on_paint_before(widget, c);
  }

  return ret;
}

ret_t axis_on_self_layout(widget_t* widget, rect_t* r) {
  ret_t ret = RET_NOT_IMPL;
  axis_t* axis = AXIS(widget);
  return_value_if_fail(axis != NULL && axis->vt != NULL, RET_BAD_PARAMS);

  if (axis->vt->on_self_layout) {
    ret = axis->vt->on_self_layout(widget, r);
  }

  return ret;
}

float_t axis_measure_series_interval(widget_t* widget) {
  float_t ret = 0;
  axis_t* axis = AXIS(widget);
  return_value_if_fail(axis != NULL && axis->vt != NULL, 0);

  if (axis->vt->measure_series_interval) {
    ret = axis->vt->measure_series_interval(widget);
  }

  return ret;
}

ret_t axis_measure_series(widget_t* widget, void* sample_params, object_t* src, object_t* dst) {
  ret_t ret = RET_NOT_IMPL;
  axis_t* axis = AXIS(widget);
  return_value_if_fail(axis != NULL && axis->vt != NULL, 0);

  if (axis->vt->measure_series) {
    ret = axis->vt->measure_series(widget, sample_params, src, dst);
  }

  return ret;
}

TK_DECL_VTABLE(axis) = {.size = sizeof(axis_t), .parent = TK_PARENT_VTABLE(widget)};

widget_t* axis_create(widget_t* parent, const widget_vtable_t* vt, xy_t x, xy_t y, wh_t w, wh_t h) {
  widget_t* widget = widget_create(parent, vt, x, y, w, h);
  axis_t* axis = AXIS(widget);
  return_value_if_fail(axis != NULL, NULL);

  wstr_init(&(axis->title.text), 0);
  wstr_init(&(axis->time.format), 0);
  axis->label.show = TRUE;
  axis->tick.show = TRUE;
  axis->line.show = TRUE;
  axis->split_line.show = TRUE;
  axis->data = darray_create(4, (tk_destroy_t)axis_data_destroy, NULL);

  return widget;
}

widget_t* axis_cast(widget_t* widget) {
  return_value_if_fail(widget_is_axis(widget), NULL);

  return widget;
}

bool_t widget_is_axis(widget_t* widget) {
#ifdef WITH_WIDGET_TYPE_CHECK
  return WIDGET_IS_INSTANCE_OF(widget, axis);
#else
  return (widget != NULL && strstr(widget->vt->type, "_axis") != NULL);
#endif
}
