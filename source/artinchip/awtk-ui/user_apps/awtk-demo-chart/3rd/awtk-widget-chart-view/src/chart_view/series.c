/**
 * File:   series.c
 * Author: AWTK Develop Team
 * Brief:  series
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

#include "tkc/mem.h"
#include "tkc/utils.h"
#include "base/enums.h"
#include "base/widget_vtable.h"
#include "series.h"
#include "series_p.h"
#include "chart_animator.h"

static const key_type_value_t s_series_dispaly_mode_value[] = {{"push", 0, SERIES_DISPLAY_PUSH},
                                                               {"cover", 0, SERIES_DISPLAY_COVER}};

const key_type_value_t* series_dispaly_mode_find(const char* name) {
  return find_item(s_series_dispaly_mode_value, ARRAY_SIZE(s_series_dispaly_mode_value), name);
}

series_dispaly_mode_t series_dispaly_mode_from_str(const char* mode) {
  const key_type_value_t* kv = series_dispaly_mode_find(mode);
  if (kv != NULL) {
    return (series_dispaly_mode_t)(kv->value);
  }
  return SERIES_DISPLAY_AUTO;
}

uint32_t series_count(widget_t* widget) {
  uint32_t ret = 0;
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && series->vt != NULL, 0);

  if (series->vt->count) {
    ret = series->vt->count(widget);
  }

  return ret;
}

ret_t series_set(widget_t* widget, uint32_t index, const void* data, uint32_t nr) {
  ret_t ret = RET_NOT_IMPL;
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && series->vt != NULL, RET_BAD_PARAMS);

  if (series->vt->set) {
    ret = series->vt->set(widget, index, data, nr);
  }

  return ret;
}

ret_t series_rset(widget_t* widget, uint32_t index, const void* data, uint32_t nr) {
  ret_t ret = RET_NOT_IMPL;
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && series->vt != NULL, RET_BAD_PARAMS);

  if (series->vt->rset) {
    ret = series->vt->rset(widget, index, data, nr);
  }

  return ret;
}

ret_t series_push(widget_t* widget, const void* data, uint32_t nr) {
  ret_t ret = RET_NOT_IMPL;
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && series->vt != NULL, RET_BAD_PARAMS);

  if (series->vt->push) {
    ret = series->vt->push(widget, data, nr);
  }

  return ret;
}

ret_t series_clear(widget_t* widget) {
  ret_t ret = RET_NOT_IMPL;
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && series->vt != NULL, RET_BAD_PARAMS);

  if (series->vt->clear) {
    ret = series->vt->clear(widget);
  }

  return ret;
}

void* series_at(widget_t* widget, uint32_t index) {
  void* ret = NULL;
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && series->vt != NULL, NULL);

  if (series->vt->at) {
    ret = series->vt->at(widget, index);
  }

  return ret;
}

ret_t series_get_current(widget_t* widget, uint32_t* begin, uint32_t* end, uint32_t* middle) {
  ret_t ret = RET_NOT_IMPL;
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && series->vt != NULL, RET_BAD_PARAMS);

  if (series->vt->get_current) {
    ret = series->vt->get_current(widget, begin, end, middle);
  }

  return ret;
}

bool_t series_is_point_in(widget_t* widget, xy_t x, xy_t y, bool_t is_local) {
  bool_t ret = FALSE;
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && series->vt != NULL, RET_BAD_PARAMS);

  if (series->vt->is_point_in) {
    ret = series->vt->is_point_in(widget, x, y, is_local);
  }

  return ret;
}

int32_t series_index_of_point_in(widget_t* widget, xy_t x, xy_t y, bool_t is_local) {
  int32_t ret = -1;
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && series->vt != NULL, RET_BAD_PARAMS);

  if (series->vt->index_of_point_in) {
    ret = series->vt->index_of_point_in(widget, x, y, is_local);
  }

  return ret;
}
ret_t series_to_local(widget_t* widget, uint32_t index, point_t* p) {
  ret_t ret = RET_NOT_IMPL;
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && series->vt != NULL, RET_BAD_PARAMS);

  if (series->vt->to_local) {
    ret = series->vt->to_local(widget, index, p);
  }

  return ret;
}

ret_t series_set_tooltip_format(widget_t* widget, series_tooltip_format_t format, void* ctx) {
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL, RET_BAD_PARAMS);

  series->tooltip_format = format;
  series->tooltip_format_ctx = ctx;

  return RET_OK;
}

ret_t series_get_tooltip(widget_t* widget, uint32_t index, wstr_t* v) {
  void* data = NULL;
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL, RET_BAD_PARAMS);

  data = series_at(widget, index);
  return_value_if_fail(data != NULL, RET_NOT_FOUND);

  if (series->tooltip_format != NULL) {
    return series->tooltip_format(series->tooltip_format_ctx, data, v);
  } else {
    const wchar_t* title = series_get_title(widget);
    wstr_from_float(v, *((float_t*)data));
    if (title != NULL && wcslen(title) > 0) {
      wstr_insert(v, 0, L": ", wcslen(L": "));
      wstr_insert(v, 0, title, wcslen(title));
    }
    return RET_OK;
  }
}

ret_t series_set_title(widget_t* widget, const char* title) {
  return widget_set_text_utf8(widget, title);
}

const wchar_t* series_get_title(widget_t* widget) {
  return widget_get_text(widget);
}

ret_t series_set_capacity(widget_t* widget, uint32_t capacity) {
  return series_p_set_capacity(widget, capacity);
}

ret_t series_set_new_period(widget_t* widget, uint32_t new_period) {
  return series_p_set_new_period(widget, new_period);
}

ret_t series_set_fifo(widget_t* widget, object_t* obj) {
  object_t* fifo = obj;
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && fifo != NULL, RET_BAD_PARAMS);

  if (series->prepare_fifo != NULL) {
    fifo = series->prepare_fifo(widget, series->prepare_fifo_ctx, obj);
  } else if (series->fifo != obj) {
    OBJECT_REF(fifo);
  }

  return series_p_set_fifo(widget, fifo);
}

ret_t series_set_prepare_fifo(widget_t* widget, series_prepare_fifo_t prepare, void* ctx) {
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL, RET_BAD_PARAMS);

  series->prepare_fifo = prepare;
  series->prepare_fifo_ctx = ctx;

  return RET_OK;
}

static ret_t series_on_destroy(void* ctx, event_t* e){
  widget_t* widget = WIDGET(e->target);
  return series_p_on_destroy(widget);
}

TK_DECL_VTABLE(series) = {.size = sizeof(series_t), .parent = TK_PARENT_VTABLE(widget)};

widget_t* series_create(widget_t* parent, const widget_vtable_t* vt, xy_t x, xy_t y, wh_t w,
                        wh_t h) {
  widget_t* widget = widget_create(parent, vt, x, y, w, h);
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL, NULL);

  widget_on(widget, EVT_DESTROY, series_on_destroy, NULL);

  series->animator_create = chart_animator_fifo_float_value_create;
  series->value_animation = 500;
  series->coverage = 1;

  return widget;
}

widget_t* series_cast(widget_t* widget) {
  return_value_if_fail(widget_is_series(widget), NULL);

  return widget;
}

bool_t widget_is_series(widget_t* widget) {
#ifdef WITH_WIDGET_TYPE_CHECK
  return WIDGET_IS_INSTANCE_OF(widget, series);
#else
  return (widget != NULL && strstr(widget->vt->type, "_series") != NULL);
#endif
}
