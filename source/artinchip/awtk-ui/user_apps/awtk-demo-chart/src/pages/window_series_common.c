#include "awtk.h"
#include "tkc/date_time.h"
#include "../../3rd/awtk-widget-chart-view/src/chart_view/chart_view.h"
#include "../../3rd/awtk-widget-chart-view/src/chart_view/line_series.h"
#include "../../3rd/awtk-widget-chart-view/src/chart_view/bar_series.h"
#include "../../3rd/awtk-widget-chart-view/src/chart_view/axis.h"

#include <math.h>
#include <stdlib.h>

#define PROP_UPDATED "updated"
#define TIME_PER_DIV 2000        //每个DIV的持续时间
#define SAMPLE_COUNT_PER_DIV 10  //每个DIV的采样点数

static ret_t on_series_prop_changed(void* ctx, event_t* e, const char* prop, const char* btn_name,
                                    const char* style, const char* style_select) {
  widget_t* win = WIDGET(ctx);
  widget_t* chart_view = widget_lookup(win, "chartview", TRUE);
  if (chart_view) {
    WIDGET_FOR_EACH_CHILD_BEGIN(chart_view, iter, i)
    if (widget_is_series(iter)) {
      value_t v;
      if (widget_get_prop(iter, prop, &v) == RET_OK) {
        value_set_bool(&v, !value_bool(&v));
        widget_set_prop(iter, prop, &v);

        widget_t* btn = widget_lookup(win, btn_name, TRUE);
        if (btn) {
          widget_use_style(btn, value_bool(&v) ? style_select : style);
        }
      }
    }
    WIDGET_FOR_EACH_CHILD_END()
  }

  return RET_OK;
}

ret_t on_series_line_show_changed(void* ctx, event_t* e) {
  return on_series_prop_changed(ctx, e, SERIES_PROP_LINE_SHOW, "line", "line", "line_select");
}

ret_t on_series_area_show_changed(void* ctx, event_t* e) {
  return on_series_prop_changed(ctx, e, SERIES_PROP_LINE_AREA_SHOW, "area", "area", "area_select");
}

ret_t on_series_symbol_show_changed(void* ctx, event_t* e) {
  return on_series_prop_changed(ctx, e, SERIES_PROP_SYMBOL_SHOW, "symbol", "symbol",
                                "symbol_select");
}

ret_t on_series_smooth_changed(void* ctx, event_t* e) {
  return on_series_prop_changed(ctx, e, SERIES_PROP_LINE_SMOOTH, "smooth", "smooth",
                                "smooth_select");
}

static ret_t on_series_visible_changed(void* ctx, event_t* e, uint32_t index, const char* icon_name,
                                       const char* show, const char* hide) {
  uint32_t cnt = 0;
  widget_t* icon;
  widget_t* win = WIDGET(ctx);
  widget_t* chart_view = widget_lookup(win, "chartview", TRUE);
  return_value_if_fail(chart_view != NULL, RET_BAD_PARAMS);

  WIDGET_FOR_EACH_CHILD_BEGIN(chart_view, iter, i)
  if (widget_is_series(iter)) {
    if (index == cnt) {
      widget_set_visible(iter, !iter->visible, FALSE);

      icon = widget_lookup(win, icon_name, TRUE);
      if (icon) {
        widget_use_style(icon, iter->visible ? show : hide);
      }
    }

    cnt++;
  }
  WIDGET_FOR_EACH_CHILD_END()

  return RET_OK;
}

ret_t on_series_typeie_visible_changed(void* ctx, event_t* e) {
  return on_series_visible_changed(ctx, e, 0, "typeie_icon", "typeie", "series_hide");
}

ret_t on_series_dayas_visible_changed(void* ctx, event_t* e) {
  return on_series_visible_changed(ctx, e, 1, "dayas_icon", "dayas", "series_hide");
}

ret_t on_series_drean_visible_changed(void* ctx, event_t* e) {
  return on_series_visible_changed(ctx, e, 2, "drean_icon", "drean", "series_hide");
}

ret_t on_series_bring_to_top(widget_t* win, uint32_t index, const char* icon_name,
                             const char* unfocus, const char* focus) {
  widget_t* chart_view = widget_lookup(win, "chartview", TRUE);
  return_value_if_fail(chart_view != NULL, RET_BAD_PARAMS);

  if (chart_view_set_top_series(chart_view, index) == RET_OK) {
    const char* icons[] = {"typeie_icon", "dayas_icon", "drean_icon"};
    const char* unfocus_styles[] = {"typeie", "dayas", "drean"};
    const char* focus_styles[] = {"typeie_focus", "dayas_focus", "drean_focus"};
    widget_t* icon;
    uint32_t i;
    for (i = 0; i < 3; i++) {
      icon = widget_lookup(win, icons[i], TRUE);
      if (icon) {
        widget_set_focused(icon, tk_str_eq(icons[i], icon_name));
        widget_use_style(icon, icon->focused ? unfocus_styles[i] : focus_styles[i]);
      }
    }
  }

  return RET_OK;
}

ret_t on_series_typeie_bring_to_top(void* ctx, event_t* e) {
  (void)e;
  return on_series_bring_to_top(WIDGET(ctx), 0, "typeie_icon", "typeie", "typeie_focus");
}

ret_t on_series_dayas_bring_to_top(void* ctx, event_t* e) {
  (void)e;
  return on_series_bring_to_top(WIDGET(ctx), 1, "dayas_icon", "dayas", "dayas_focus");
}

ret_t on_series_drean_bring_to_top(void* ctx, event_t* e) {
  (void)e;
  return on_series_bring_to_top(WIDGET(ctx), 2, "drean_icon", "drean", "drean_fucus");
}

static void generate_ufloat_data(void* buffer, uint32_t size) {
  uint32_t i;
  float_t* b = (float_t*)buffer;
  for (i = 0; i < size; i++) {
    b[i] = (float_t)(rand() % 120 + 10);
  }
}

static void generate_float_data(void* buffer, uint32_t size) {
  uint32_t i;
  float_t* b = (float_t*)buffer;
  for (i = 0; i < size; i++) {
    b[i] = (float_t)(rand() % 240 - 120);
  }
}

static void generate_colorful_data(void* buffer, uint32_t size) {
  uint32_t i;
  series_data_colorful_t* b = (series_data_colorful_t*)buffer;
  for (i = 0; i < size; i++) {
    b[i].v = (float_t)(rand() % 120 + 10);
    b[i].c.color = rand();
    b[i].c.rgba.a = 0xff;
  }
}

static void generate_minmax_data(void* buffer, uint32_t size) {
  uint32_t i;
  series_data_minmax_t* b = (series_data_minmax_t*)buffer;
  for (i = 0; i < size; i++) {
    b[i].min = (float_t)(rand() % 280 - 140);
    b[i].max = b[i].min + rand() % 200;
  }
}

typedef void (*_generate_data_t)(void* buffer, uint32_t size);
static void on_series_push_data(widget_t* widget, uint32_t count, uint32_t unit_size,
                                _generate_data_t gen) {
  value_t v;
  uint64_t recent_time = 0;
  uint32_t tdiv = TIME_PER_DIV;
  uint32_t sdiv = SAMPLE_COUNT_PER_DIV;
  bool_t updated = FALSE;
  widget_t* axis;
  void* buffer = TKMEM_CALLOC(count, unit_size);

  WIDGET_FOR_EACH_CHILD_BEGIN(widget, iter, i)
  if (widget_is_series(iter)) {
    gen(buffer, count);
    series_push(iter, buffer, count);

    axis = widget_get_prop_pointer(iter, SERIES_PROP_SERIES_AXIS);
    if (axis) {
      updated = widget_get_prop_bool(axis, PROP_UPDATED, FALSE);
      if (!updated) {
        widget_get_prop(axis, AXIS_PROP_TIME_RECENT_TIME, &v);
        recent_time = value_uint64(&v) + count * tdiv / sdiv / 1000;
        widget_set_prop_int(axis, AXIS_PROP_TIME_RECENT_TIME, recent_time);
        widget_set_prop_bool(axis, PROP_UPDATED, TRUE);
      }
    }
  }
  WIDGET_FOR_EACH_CHILD_END()

  WIDGET_FOR_EACH_CHILD_BEGIN(widget, iter, i)
  if (widget_is_axis(iter)) {
    widget_set_prop_bool(iter, PROP_UPDATED, FALSE);
  }
  WIDGET_FOR_EACH_CHILD_END()

  TKMEM_FREE(buffer);
}

void on_series_push_ufloat_data(widget_t* widget, uint32_t count) {
  on_series_push_data(widget, count, sizeof(float_t), generate_ufloat_data);
}

void on_series_push_float_data(widget_t* widget, uint32_t count) {
  on_series_push_data(widget, count, sizeof(float_t), generate_float_data);
}

void on_series_push_colorful_data(widget_t* widget, uint32_t count) {
  on_series_push_data(widget, count, sizeof(series_data_colorful_t), generate_colorful_data);
}

void on_series_push_minmax_data(widget_t* widget, uint32_t count) {
  on_series_push_data(widget, count, sizeof(series_data_minmax_t), generate_minmax_data);
}

static void on_series_rset_data(widget_t* widget, uint32_t count, uint32_t unit_size,
                                _generate_data_t gen) {
  void* buffer = TKMEM_CALLOC(count, unit_size);

  WIDGET_FOR_EACH_CHILD_BEGIN(widget, iter, i)
  if (widget_is_series(iter)) {
    gen(buffer, count);
    series_rset(iter, count - 1, buffer, count);
  }
  WIDGET_FOR_EACH_CHILD_END()

  TKMEM_FREE(buffer);
}

void on_series_rset_ufloat_data(widget_t* widget, uint32_t count) {
  on_series_rset_data(widget, count, sizeof(float_t), generate_ufloat_data);
}

void on_series_rset_float_data(widget_t* widget, uint32_t count) {
  on_series_rset_data(widget, count, sizeof(float_t), generate_float_data);
}

void on_series_rset_colorful_data(widget_t* widget, uint32_t count) {
  on_series_rset_data(widget, count, sizeof(series_data_colorful_t), generate_colorful_data);
}

void on_series_rset_minmax_data(widget_t* widget, uint32_t count) {
  on_series_rset_data(widget, count, sizeof(series_data_minmax_t), generate_minmax_data);
}

static uint32_t get_series_capacity_min(widget_t* widget) {
  uint32_t ret = -1;

  WIDGET_FOR_EACH_CHILD_BEGIN(widget, iter, i)
  if (widget_is_series(iter)) {
    ret = tk_min(ret, widget_get_prop_int(iter, SERIES_PROP_CAPACITY, 0));
  }
  WIDGET_FOR_EACH_CHILD_END()

  ret = tk_min(ret, 100);

  return ret;
}

ret_t on_series_rset_rand_ufloat_data(void* ctx, event_t* e) {
  widget_t* win = WIDGET(ctx);
  widget_t* chart_view = widget_lookup(win, "chartview", TRUE);
  if (chart_view) {
    on_series_rset_ufloat_data(chart_view, get_series_capacity_min(chart_view));
  }
  return RET_OK;
}

ret_t on_series_push_rand_ufloat_data(const timer_info_t* timer) {
  widget_t* win = WIDGET(timer->ctx);
  widget_t* chart_view = widget_lookup(win, "chartview", TRUE);
  if (chart_view) {
    on_series_push_ufloat_data(chart_view, SAMPLE_COUNT_PER_DIV * 1000 / TIME_PER_DIV);
  }
  return RET_REPEAT;
}

ret_t on_series_rset_rand_float_data(void* ctx, event_t* e) {
  widget_t* win = WIDGET(ctx);
  widget_t* chart_view = widget_lookup(win, "chartview", TRUE);
  if (chart_view) {
    on_series_rset_float_data(chart_view, get_series_capacity_min(chart_view));
  }
  return RET_OK;
}

ret_t on_series_push_rand_float_data(const timer_info_t* timer) {
  widget_t* win = WIDGET(timer->ctx);
  widget_t* chart_view = widget_lookup(win, "chartview", TRUE);
  if (chart_view) {
    on_series_push_float_data(chart_view, 1);
  }
  return RET_REPEAT;
}

ret_t on_series_rset_rand_colorful_data(void* ctx, event_t* e) {
  widget_t* win = WIDGET(ctx);
  widget_t* chart_view = widget_lookup(win, "chartview", TRUE);
  if (chart_view) {
    on_series_rset_colorful_data(chart_view, get_series_capacity_min(chart_view));
  }
  return RET_OK;
}

ret_t on_series_push_rand_colorful_data(const timer_info_t* timer) {
  widget_t* win = WIDGET(timer->ctx);
  widget_t* chart_view = widget_lookup(win, "chartview", TRUE);
  if (chart_view) {
    on_series_push_colorful_data(chart_view, SAMPLE_COUNT_PER_DIV * 1000 / TIME_PER_DIV);
  }
  return RET_REPEAT;
}

ret_t on_series_rset_rand_minmax_data(void* ctx, event_t* e) {
  widget_t* win = WIDGET(ctx);
  widget_t* chart_view = widget_lookup(win, "chartview", TRUE);
  if (chart_view) {
    on_series_rset_minmax_data(chart_view, get_series_capacity_min(chart_view));
  }
  return RET_OK;
}

ret_t on_series_push_rand_minmax_data(const timer_info_t* timer) {
  widget_t* win = WIDGET(timer->ctx);
  widget_t* chart_view = widget_lookup(win, "chartview", TRUE);
  if (chart_view) {
    on_series_push_minmax_data(chart_view, SAMPLE_COUNT_PER_DIV * 1000 / TIME_PER_DIV);
  }
  return RET_REPEAT;
}

ret_t axis_time_init(widget_t* widget) {
  value_t v;
  widget_t* chart_view = widget_lookup(widget, "chartview", TRUE);
  return_value_if_fail(chart_view != NULL, RET_BAD_PARAMS);

  date_time_t dt;
  date_time_init(&dt);

  WIDGET_FOR_EACH_CHILD_BEGIN(chart_view, iter, i)
  if (widget_is_axis(iter) && strstr(iter->name, "from_series") != NULL) {
    value_set_uint64(&v, date_time_to_time(&dt));
    widget_set_prop(iter, AXIS_PROP_TIME_RECENT_TIME, &v);
    widget_set_prop_int(iter, AXIS_PROP_TIME_DIV, TIME_PER_DIV);
    widget_set_prop_int(iter, AXIS_PROP_TIME_SAMPLING_RATE, TIME_PER_DIV / SAMPLE_COUNT_PER_DIV);
    widget_set_prop_bool(iter, PROP_UPDATED, FALSE);
  }
  WIDGET_FOR_EACH_CHILD_END()

  return RET_OK;
}
