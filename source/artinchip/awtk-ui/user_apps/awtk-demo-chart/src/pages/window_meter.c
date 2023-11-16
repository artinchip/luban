#include "awtk.h"
#include "../common/navigator.h"

/**
 * 点击开始/关闭按钮时改变样式
 */
static ret_t set_btn_style(widget_t* win, event_t* e) {
  widget_t* target = e->target;
  widget_t* start_btn = widget_lookup(win, "function_start", TRUE);

  if (start_btn) {
    value_t v;
    widget_get_prop(start_btn, WIDGET_PROP_STYLE, &v);
    const char* style = value_str(&v);
    widget_use_style(start_btn,
                     strstr(style, "pressed") == NULL ? "meter_start_pressed" : "meter_start");
  }

  widget_t* stop_btn = widget_lookup(win, "function_stop", TRUE);
  if (stop_btn) {
    value_t v;
    widget_get_prop(stop_btn, WIDGET_PROP_STYLE, &v);
    const char* style = value_str(&v);
    widget_use_style(stop_btn,
                     strstr(style, "pressed") == NULL ? "meter_stop_pressed" : "meter_stop");
  }

  if (target == start_btn) {
    widget_use_style(stop_btn, "meter_stop");
    widget_set_enable(stop_btn, TRUE);
  } else {
    widget_use_style(start_btn, "meter_start");
    widget_set_enable(start_btn, TRUE);
  }
  widget_set_enable(target, FALSE);

  return RET_OK;
}

/**
 * 点击开始按钮
 */
static ret_t on_function_start_click(void* ctx, event_t* e) {
  // TODO: 在此添加控件事件处理程序代码
  widget_t* win = (widget_t*)ctx;
  set_btn_style(win, e);
  widget_start_animator(NULL, NULL);

  return RET_OK;
}

/**
 * 点击停止按钮
 */
static ret_t on_function_stop_click(void* ctx, event_t* e) {
  // TODO: 在此添加控件事件处理程序代码
  widget_t* win = (widget_t*)ctx;
  set_btn_style(win, e);
  widget_stop_animator(NULL, NULL);

  return RET_OK;
}

/**
 * 点击关闭按钮
 */
static ret_t on_close_click(void* ctx, event_t* e) {
  return navigator_back();
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
    if (tk_str_eq(name, "function_start")) {
      widget_on(widget, EVT_CLICK, on_function_start_click, win);
    } else if (tk_str_eq(name, "function_stop")) {
      widget_on(widget, EVT_CLICK, on_function_stop_click, win);
    } else if (tk_str_eq(name, "close")) {
      widget_on(widget, EVT_CLICK, on_close_click, win);
    }
  }

  return RET_OK;
}

/**
 * 初始化窗口
 */
ret_t window_meter_init(widget_t* win, void* ctx) {
  (void)ctx;
  return_value_if_fail(win != NULL, RET_BAD_PARAMS);

  widget_foreach(win, visit_init_child, win);

  return RET_OK;
}
