#include "awtk.h"
#include "../common/navigator.h"
#include "../../3rd/awtk-widget-chart-view/src/chart_view/chart_view.h"
#include "../../3rd/awtk-widget-chart-view/src/chart_view/bar_series.h"

#define SERIES_TIMER_UPDATE 1

extern ret_t on_series_rset_rand_ufloat_data(void* ctx, event_t* e);
extern void on_series_rset_ufloat_data(widget_t* widget, uint32_t count);
extern ret_t on_series_push_rand_ufloat_data(const timer_info_t* timer);
extern ret_t on_series_typeie_visible_changed(void* ctx, event_t* e);
extern ret_t on_series_dayas_visible_changed(void* ctx, event_t* e);
extern ret_t on_series_drean_visible_changed(void* ctx, event_t* e);
extern ret_t axis_time_init(widget_t* widget);
static ret_t on_close_click(void* ctx, event_t* e) {
  return navigator_back();
}

static ret_t on_typeie_click(void* ctx, event_t* e) {
  // TODO: 在此添加控件事件处理程序代码
  on_series_typeie_visible_changed(ctx, e);

  return RET_OK;
}

static ret_t on_dayas_click(void* ctx, event_t* e) {
  // TODO: 在此添加控件事件处理程序代码
  on_series_dayas_visible_changed(ctx, e);

  return RET_OK;
}

static ret_t on_drean_click(void* ctx, event_t* e) {
  // TODO: 在此添加控件事件处理程序代码
  on_series_drean_visible_changed(ctx, e);

  return RET_OK;
}

static ret_t on_new_graph_click(void* ctx, event_t* e) {
  // TODO: 在此添加控件事件处理程序代码
  on_series_rset_rand_ufloat_data(ctx, e);

  return RET_OK;
}

/**
 * 初始化窗口的子控件
 */
static ret_t visit_init_child(void* ctx, const void* iter) {
  widget_t* win = WIDGET(ctx);
  widget_t* widget = WIDGET(iter);
  const char* name = widget->name;

  // 初始化指定名称的控件（设置属性或注册事件），请保证控件名称在窗口上唯一
  if (name != NULL && *name != '\0') {
    if (tk_str_eq(name, "close")) {
      widget_on(widget, EVT_CLICK, on_close_click, NULL);
    } else if (tk_str_eq(name, "typeie")) {
      widget_on(widget, EVT_CLICK, on_typeie_click, win);
    } else if (tk_str_eq(name, "dayas")) {
      widget_on(widget, EVT_CLICK, on_dayas_click, win);
    } else if (tk_str_eq(name, "drean")) {
      widget_on(widget, EVT_CLICK, on_drean_click, win);
    } else if (tk_str_eq(name, "chartview")) {
      on_series_rset_ufloat_data(widget, 7);
    } else if (tk_str_eq(name, "new_graph")) {
      widget_on(widget, EVT_CLICK, on_new_graph_click, win);
    } else if (tk_str_eq(name, "s1") || tk_str_eq(name, "s2") || tk_str_eq(name, "s3")) {
      series_clear(widget);
    }
  }

  return RET_OK;
}

/**
 * 初始化窗口
 */
ret_t window_bar_init(widget_t* win, void* ctx) {
  (void)ctx;
  return_value_if_fail(win != NULL, RET_BAD_PARAMS);

#if SERIES_TIMER_UPDATE
  widget_add_timer(win, on_series_push_rand_ufloat_data, 1000);
#endif

  widget_foreach(win, visit_init_child, win);
  axis_time_init(win);

  return RET_OK;
}
