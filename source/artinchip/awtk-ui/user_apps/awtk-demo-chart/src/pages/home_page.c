#include "awtk.h"
#include "../common/navigator.h"

static ret_t on_meter_image_click(void* ctx, event_t* e) {
  return navigator_to("window_meter");
}

static ret_t on_pie_image_click(void* ctx, event_t* e) {
  return navigator_to("window_pie");
}

static ret_t on_graph_image_click(void* ctx, event_t* e) {
  return navigator_to("window_line");
}

static ret_t on_histogram_image_click(void* ctx, event_t* e) {
  return navigator_to("window_bar");
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
    if (tk_str_eq(name, "meter_image")) {
      widget_on(widget, EVT_CLICK, on_meter_image_click, NULL);
    } else if (tk_str_eq(name, "pie_image")) {
      widget_on(widget, EVT_CLICK, on_pie_image_click, NULL);
    } else if (tk_str_eq(name, "graph_image")) {
      widget_on(widget, EVT_CLICK, on_graph_image_click, NULL);
    } else if (tk_str_eq(name, "histogram_image")) {
      widget_on(widget, EVT_CLICK, on_histogram_image_click, NULL);
    }
  }

  return RET_OK;
}

/**
 * 初始化窗口
 */
ret_t home_page_init(widget_t* win, void* ctx) {
  (void)ctx;
  return_value_if_fail(win != NULL, RET_BAD_PARAMS);

  widget_foreach(win, visit_init_child, win);

  return RET_OK;
}
