#include "awtk.h"
#include "../common/navigator.h"

/**
 * 中英文互译
 */
static ret_t change_locale(const char* str) {
  char country[3];
  char language[3];

  strncpy(language, str, 2);
  strncpy(country, str + 3, 2);
  locale_info_change(locale_info(), language, country);

  return RET_OK;
}

static ret_t on_language_btn_click(void* ctx, event_t* e) {
  // TODO: 在此添加控件事件处理程序代码
  (void)ctx;
  value_t v;
  static bool_t langZH = TRUE;

  if (langZH) {
    change_locale("en_US");
  } else {
    change_locale("zh_CN");
  }
  langZH = !langZH;

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
    if (tk_str_eq(name, "language_btn")) {
      widget_on(widget, EVT_CLICK, on_language_btn_click, NULL);
    }
  }

  return RET_OK;
}

/**
 * 初始化窗口
 */
ret_t system_bar_init(widget_t* win, void* ctx) {
  (void)ctx;
  return_value_if_fail(win != NULL, RET_BAD_PARAMS);

  change_locale("zh_CN");
  widget_foreach(win, visit_init_child, win);

  return RET_OK;
}
