/**
 * File:   axis_p.c
 * Author: AWTK Develop Team
 * Brief:  axis private
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

#include "axis_p.h"
#include "tkc/utils.h"
#include "tkc/wstr.h"
#include "base/style.h"
#include "base/date_time_format.h"
#include "series.h"

void axis_p_parse_line_params(void* ctx, const char* name, const value_t* v) {
  axis_t* axis = AXIS(ctx);
  ENSURE(axis != NULL && name != NULL && v != NULL);

  if (tk_str_eq(name, "show")) {
    axis->line.show = value_bool(v);
  } else if (tk_str_eq(name, "lengthen")) {
    axis->line.lengthen = value_uint32(v);
  }
}

void axis_p_parse_split_line_params(void* ctx, const char* name, const value_t* v) {
  axis_t* axis = AXIS(ctx);
  ENSURE(axis != NULL && name != NULL && v != NULL);

  if (tk_str_eq(name, "show")) {
    axis->split_line.show = value_bool(v);
  }
}

void axis_p_parse_tick_params(void* ctx, const char* name, const value_t* v) {
  axis_t* axis = AXIS(ctx);
  ENSURE(axis != NULL && name != NULL && v != NULL);

  if (tk_str_eq(name, "show")) {
    axis->tick.show = value_bool(v);
  } else if (tk_str_eq(name, "inside")) {
    axis->tick.inside = value_bool(v);
  } else if (tk_str_eq(name, "align_with_label")) {
    axis->tick.align_with_label = value_bool(v);
  }
}

void axis_p_parse_label_params(void* ctx, const char* name, const value_t* v) {
  axis_t* axis = AXIS(ctx);
  ENSURE(axis != NULL && name != NULL && v != NULL);

  if (tk_str_eq(name, "show")) {
    axis->label.show = value_bool(v);
  } else if (tk_str_eq(name, "inside")) {
    axis->label.inside = value_bool(v);
  }
}

void axis_p_parse_title_params(void* ctx, const char* name, const value_t* v) {
  axis_t* axis = AXIS(ctx);
  ENSURE(axis != NULL && name != NULL && v != NULL);

  if (tk_str_eq(name, "show")) {
    axis->title.show = value_bool(v);
  } else if (tk_str_eq(name, "text")) {
    wstr_from_value(&(axis->title.text), v);
  }
}

void axis_p_parse_time_params(void* ctx, const char* name, const value_t* v) {
  axis_t* axis = AXIS(ctx);
  ENSURE(axis != NULL && name != NULL && v != NULL);

  if (tk_str_eq(name, "recent_time")) {
    axis->time.recent_time = value_uint64(v);
  } else if (tk_str_eq(name, "div")) {
    axis->time.div = value_uint32(v);
  } else if (tk_str_eq(name, "sampling_rate")) {
    axis->time.sampling_rate = value_uint32(v);
  } else if (tk_str_eq(name, "format")) {
    wstr_from_value(&(axis->time.format), v);
  }
}

static ret_t axis_set_type(axis_t* axis, const value_t* v) {
  if (v->type == VALUE_TYPE_STRING) {
    const char* str = value_str(v);
    if (tk_str_eq(str, "value")) {
      axis->axis_type = AXIS_TYPE_VALUE;
    } else if (tk_str_eq(str, "category")) {
      axis->axis_type = AXIS_TYPE_CATEGORY;
    } else if (tk_str_eq(str, "time")) {
      axis->axis_type = AXIS_TYPE_TIME;
    }
  } else {
    axis->axis_type = (axis_type_t)value_int(v);
  }

  return RET_OK;
}

static ret_t axis_set_at(axis_t* axis, const value_t* v) {
  if (v->type == VALUE_TYPE_STRING) {
    const char* str = value_str(v);
    if (tk_str_eq(str, "top")) {
      axis->at = AXIS_AT_TOP;
    } else if (tk_str_eq(str, "bottom")) {
      axis->at = AXIS_AT_BOTTOM;
    } else if (tk_str_eq(str, "left")) {
      axis->at = AXIS_AT_LEFT;
    } else if (tk_str_eq(str, "right")) {
      axis->at = AXIS_AT_RIGHT;
    } else {
      axis->at = AXIS_AT_AUTO;
    }
  } else {
    axis->at = (axis_at_type_t)value_int(v);
  }

  return RET_OK;
}

ret_t axis_p_get_prop(widget_t* widget, const char* name, value_t* v) {
  axis_t* axis = AXIS(widget);
  return_value_if_fail(axis != NULL && name != NULL && v != NULL, RET_BAD_PARAMS);

  if (tk_str_eq(name, AXIS_PROP_TYPE)) {
    value_set_int(v, axis->axis_type);
    return RET_OK;
  } else if (tk_str_eq(name, AXIS_PROP_AT)) {
    value_set_int(v, axis->at);
    return RET_OK;
  } else if (tk_str_eq(name, WIDGET_PROP_MIN)) {
    value_set_float(v, axis->min);
    return RET_OK;
  } else if (tk_str_eq(name, WIDGET_PROP_MAX)) {
    value_set_float(v, axis->max);
    return RET_OK;
  } else if (tk_str_eq(name, AXIS_PROP_OFFSET)) {
    value_set_float(v, axis->offset);
    return RET_OK;
  } else if (tk_str_eq(name, AXIS_PROP_OFFSET_PERCENT)) {
    value_set_bool(v, axis->offset_percent);
    return RET_OK;
  } else if (tk_str_eq(name, AXIS_PROP_INVERSE)) {
    value_set_bool(v, axis->inverse);
    return RET_OK;
  } else if (tk_str_eq(name, AXIS_PROP_SPLIT_LINE_SHOW)) {
    value_set_bool(v, axis->split_line.show);
    return RET_OK;
  } else if (tk_str_eq(name, AXIS_PROP_LINE_SHOW)) {
    value_set_bool(v, axis->line.show);
    return RET_OK;
  } else if (tk_str_eq(name, AXIS_PROP_TICK_SHOW)) {
    value_set_bool(v, axis->tick.show);
    return RET_OK;
  } else if (tk_str_eq(name, AXIS_PROP_LABEL_SHOW)) {
    value_set_bool(v, axis->label.show);
    return RET_OK;
  } else if (tk_str_eq(name, AXIS_PROP_TITLE_SHOW)) {
    value_set_bool(v, axis->title.show);
    return RET_OK;
  } else if (tk_str_eq(name, AXIS_PROP_TIME_RECENT_TIME)) {
    value_set_uint64(v, axis->time.recent_time);
    return RET_OK;
  } else if (tk_str_eq(name, AXIS_PROP_TIME_DIV)) {
    value_set_uint32(v, axis->time.div);
    return RET_OK;
  } else if (tk_str_eq(name, AXIS_PROP_TIME_SAMPLING_RATE)) {
    value_set_uint32(v, axis->time.sampling_rate);
    return RET_OK;
  }

  return RET_NOT_FOUND;
}

ret_t axis_p_set_prop(widget_t* widget, const char* name, const value_t* v) {
  axis_t* axis = AXIS(widget);
  return_value_if_fail(axis != NULL && name != NULL && v != NULL, RET_BAD_PARAMS);

  if (tk_str_eq(name, AXIS_PROP_TYPE)) {
    return axis_set_type(axis, v);
  } else if (tk_str_eq(name, AXIS_PROP_AT)) {
    return axis_set_at(axis, v);
  } else if (tk_str_eq(name, WIDGET_PROP_MIN)) {
    axis->min = value_float(v);
    return RET_OK;
  } else if (tk_str_eq(name, WIDGET_PROP_MAX)) {
    axis->max = value_float(v);
    return RET_OK;
  } else if (tk_str_eq(name, AXIS_PROP_OFFSET)) {
    axis->offset = value_double(v);
    return RET_OK;
  } else if (tk_str_eq(name, AXIS_PROP_OFFSET_PERCENT)) {
    axis->offset_percent = value_bool(v);
    return RET_OK;
  } else if (tk_str_eq(name, AXIS_PROP_DATA)) {
    return axis_set_data(widget, value_str(v));
  } else if (tk_str_eq(name, AXIS_PROP_INVERSE)) {
    axis->inverse = value_bool(v);
    return RET_OK;
  } else if (tk_str_eq(name, AXIS_PROP_SPLIT_LINE)) {
    return chart_utils_object_parse(axis_p_parse_split_line_params, axis, value_str(v));
  } else if (tk_str_eq(name, AXIS_PROP_SPLIT_LINE_SHOW)) {
    axis->split_line.show = value_bool(v);
    return RET_OK;
  } else if (tk_str_eq(name, AXIS_PROP_LINE)) {
    return chart_utils_object_parse(axis_p_parse_line_params, axis, value_str(v));
  } else if (tk_str_eq(name, AXIS_PROP_LINE_SHOW)) {
    axis->line.show = value_bool(v);
    return RET_OK;
  } else if (tk_str_eq(name, AXIS_PROP_TICK)) {
    return chart_utils_object_parse(axis_p_parse_tick_params, axis, value_str(v));
  } else if (tk_str_eq(name, AXIS_PROP_TICK_SHOW)) {
    axis->tick.show = value_bool(v);
    return RET_OK;
  } else if (tk_str_eq(name, AXIS_PROP_LABEL)) {
    return chart_utils_object_parse(axis_p_parse_label_params, axis, value_str(v));
  } else if (tk_str_eq(name, AXIS_PROP_LABEL_SHOW)) {
    axis->label.show = value_bool(v);
    return RET_OK;
  } else if (tk_str_eq(name, AXIS_PROP_TITLE)) {
    return chart_utils_object_parse(axis_p_parse_title_params, axis, value_str(v));
  } else if (tk_str_eq(name, AXIS_PROP_TITLE_SHOW)) {
    axis->title.show = value_bool(v);
    return RET_OK;
  } else if (tk_str_eq(name, AXIS_PROP_TIME)) {
    return chart_utils_object_parse(axis_p_parse_time_params, axis, value_str(v));
  } else if (tk_str_eq(name, AXIS_PROP_TIME_RECENT_TIME)) {
    axis->time.recent_time = value_uint64(v);
  } else if (tk_str_eq(name, AXIS_PROP_TIME_DIV)) {
    axis->time.div = value_uint32(v);
  } else if (tk_str_eq(name, AXIS_PROP_TIME_SAMPLING_RATE)) {
    axis->time.sampling_rate = value_uint32(v);
    return RET_OK;
  } else if (tk_str_eq(name, AXIS_PROP_TIME_FORMAT)) {
    wstr_from_value(&(axis->time.format), v);
  }

  return RET_NOT_FOUND;
}

static const char* axis_p_format_time(char* str, uint32_t size, uint64_t time, wstr_t* format) {
  wstr_t wstr;
  wstr_init(&wstr, 0);
  char format_text[64] = {0};

  if (!format->str) {
    wstr_set_utf8(format, "hh:mm:ss");
  }

  wstr_get_utf8(format, format_text, sizeof(format_text));
  wstr_format_time(&wstr, format_text, time);
  wstr_get_utf8(&wstr, str, size);
  wstr_reset(&wstr);

  return str;
}

ret_t axis_p_time_generate_default(widget_t* widget, uint32_t recent_index, uint32_t begin,
                                   uint32_t end, uint32_t middle, float_t interval, darray_t* v,
                                   void* ctx) {
  axis_t* axis = AXIS(widget);
  axis_data_t* axis_data;
  uint32_t i = 0;
  uint32_t nr = 0;
  uint32_t cnt = 0;
  uint32_t inc = 0;
  uint32_t index = 0;
  uint32_t offset = 0;
  uint32_t prefix = 0;
  uint64_t time_ms = 0;
  uint64_t data_time = 0;
  uint64_t reserve = pow(10, 15);
  uint32_t sampling_rate = axis->time.sampling_rate;
  uint64_t recent_time = axis->time.recent_time;
  uint64_t div_time = axis->time.div;
  uint32_t div_count = 0;
  wstr_t* format = &(axis->time.format);
  char show_time[128] = {0};
  return_value_if_fail(recent_index > 0 && div_time > 0 && sampling_rate > 0 && recent_time > 0,
                       RET_BAD_PARAMS);

  div_count = div_time / sampling_rate;
  prefix = recent_time / reserve;
  recent_time = (prefix == 0 ? recent_time : recent_time - prefix * reserve) * 1000;

  time_ms = recent_time - (recent_index - middle) * div_time / div_count;
  offset = (div_time - time_ms % div_time) * div_count / div_time;
  nr = end - middle + 1;

  for (i = 0; i < 2; i++) {
    if (i == 0) {
      time_ms = recent_time - (recent_index - middle) * div_time / div_count;
      offset = time_ms % div_time ? (div_time - time_ms % div_time) * div_count / div_time : 0;
      nr = end - middle + 1;
    } else {
      time_ms = recent_time - (recent_index - begin) * div_time / div_count;
      offset = (div_time - time_ms % div_time) * div_count / div_time + nr;
      nr = end - begin + 1;
      index = cnt = 0;
    }

    while ((index = cnt * div_count + offset) < nr) {
      inc = offset > 0 ? 1 : 0;
      axis_data = axis_data_create(index * interval, NULL);
      data_time = prefix * reserve + (time_ms / div_time + cnt + inc) * div_time / 1000;
      axis_p_format_time(show_time, sizeof(show_time), data_time, format);
      wstr_set_utf8(&(axis_data->text), show_time);
      darray_push(v, axis_data);
      cnt++;
    }
  }

  return RET_OK;
}
