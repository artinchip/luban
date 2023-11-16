﻿#include "awtk.h"
#include "../common/navigator.h"
#include "../../3rd/awtk-widget-chart-view/src/pie_slice/pie_slice.h"

#define SAVE_EXPLODED "save_exploded"
#define DELAY_TIME 100
#define DURATION_TIME 120

static bool_t save_pie_exploded[6];

typedef struct _pie_value_t {
  int32_t start_angle;
  int32_t value;
} pie_value_t;

static pie_value_t old_pie_data[] = {
    {270, 90}, {200, 70}, {130, 70}, {90, 40}, {30, 60}, {0, 30},
};

static pie_value_t new_pie_data[] = {
    {240, 120}, {150, 90}, {90, 60}, {40, 50}, {10, 30}, {0, 10},
};

/**
 * 设置按钮是否可以按下
 */
static ret_t set_btn_enable(widget_t* win, bool_t enable) {
  widget_t* new_pie = widget_lookup(win, "function_new_pie", TRUE);
  widget_t* arch = widget_lookup(win, "function_arch", TRUE);
  if (new_pie) {
    widget_set_enable(new_pie, enable);
  }
  if (arch) {
    widget_set_enable(arch, enable);
  }

  return RET_OK;
}

static widget_t* get_pie_slice(widget_t* win, const char* name) {
  widget_t* pie_slice = NULL;
  str_t str;
  str_init(&str, 20);
  str_append_with_len(&str, name, 4);
  str_append(&str, "_slice");
  pie_slice = widget_lookup(win, str.str, TRUE);
  str_reset(&str);

  return pie_slice;
}

/**
 * 恢复扇形原来是否展开的状态
 */
static ret_t on_save_exploded_timer(const timer_info_t* timer) {
  widget_t* win = WIDGET(timer->ctx);

  uint32_t count = widget_animator_manager_count(widget_animator_manager());
  if (count == 0) {
    widget_t* pie_view = widget_lookup(win, "pie_view", TRUE);
    if (pie_view) {
      WIDGET_FOR_EACH_CHILD_BEGIN_R(pie_view, iter, i)
      value_t v;
      value_set_bool(&v, !save_pie_exploded[i]);
      widget_set_prop(iter, PIE_SLICE_PROP_IS_EXPLODED, &v);
      pie_slice_set_exploded(iter);
      WIDGET_FOR_EACH_CHILD_END();
    }
    set_btn_enable(win, TRUE);

    return RET_REMOVE;
  }

  return RET_REPEAT;
}

/**
 * 点击创建新饼图或者拱形定时器
 */
static ret_t on_new_pie_timer(const timer_info_t* timer) {
  pie_value_t* pie_data = NULL;
  widget_t* win = WIDGET(timer->ctx);

  uint32_t count = widget_animator_manager_count(widget_animator_manager());
  if (count == 0) {
    value_t v;
    value_t v1;
    ret_t result = widget_get_prop(win, "is_new", &v);
    bool_t flag = (result == RET_NOT_FOUND) ? FALSE : value_bool(&v);
    if (flag) {
      pie_data = old_pie_data;
      widget_set_prop(win, "is_new", value_set_bool(&v1, FALSE));
    } else {
      pie_data = new_pie_data;
      widget_set_prop(win, "is_new", value_set_bool(&v1, TRUE));
    }
    widget_t* pie_view = widget_lookup(win, "pie_view", TRUE);
    if (pie_view) {
      WIDGET_FOR_EACH_CHILD_BEGIN(pie_view, iter, i)
      int32_t delay = DELAY_TIME;
      int32_t duration = DURATION_TIME;
      delay = delay * i;
      const pie_value_t* new_pie = pie_data + nr - 1 - i;
      pie_slice_set_start_angle(iter, new_pie->start_angle);
      char param[100];
      tk_snprintf(param, sizeof(param),
                  "value(name=%s, to=%d, duration=%d, delay=%d, easing=sin_out)", SAVE_EXPLODED,
                  new_pie->value, duration, delay);
      widget_create_animator(iter, param);
      WIDGET_FOR_EACH_CHILD_END();
    }
    timer_add(on_save_exploded_timer, win, 1000 / 60);

    return RET_REMOVE;
  }

  return RET_REPEAT;
}

/**
 * 创建扇形还原到原点动画
 */
static ret_t create_animator_to_zero(widget_t* win) {
  set_btn_enable(win, FALSE);

  widget_t* pie_view = widget_lookup(win, "pie_view", TRUE);
  if (pie_view) {
    WIDGET_FOR_EACH_CHILD_BEGIN_R(pie_view, iter, i)
    value_t v;
    widget_get_prop(iter, PIE_SLICE_PROP_IS_EXPLODED, &v);
    save_pie_exploded[i] = value_bool(&v);

    pie_slice_t* pie_slice = PIE_SLICE(iter);
    if (pie_slice->is_exploded) {
      pie_slice_set_exploded(iter);
    }
    int32_t delay = 80;
    delay = delay * (nr - 1 - i);
    char param[100];
    tk_snprintf(param, sizeof(param), "value(to=0, duration=50, delay=%d, easing=sin_out)", delay);
    widget_create_animator(iter, param);
    WIDGET_FOR_EACH_CHILD_END();
  }

  return RET_OK;
}

/**
 * 拱形定时器
 */
static ret_t on_arch_timer(const timer_info_t* timer) {
  widget_t* win = WIDGET(timer->ctx);

  uint32_t new_inner_radius = win->h / 5;
  uint32_t inner_radius = 0;

  uint32_t count = widget_animator_manager_count(widget_animator_manager());
  if (count == 0) {
    value_t v;
    value_t v1;
    ret_t result = widget_get_prop(win, "is_arch", &v);
    bool_t flag = (result == RET_NOT_FOUND) ? FALSE : value_bool(&v);
    if (flag) {
      widget_set_prop(win, "is_arch", value_set_bool(&v1, FALSE));
      inner_radius = 550;
    } else {
      widget_set_prop(win, "is_arch", value_set_bool(&v1, TRUE));
      inner_radius = new_inner_radius;
    }
    widget_t* pie_view = widget_lookup(win, "pie_view", TRUE);
    if (pie_view) {
      WIDGET_FOR_EACH_CHILD_BEGIN(pie_view, iter, i)
      pie_slice_set_inner_radius(iter, inner_radius);

      if (flag) {
        pie_slice_set_semicircle(iter, FALSE);
        pie_slice_set_counter_clock_wise(iter, FALSE);
      } else {
        pie_slice_set_semicircle(iter, TRUE);
        pie_slice_set_counter_clock_wise(iter, TRUE);
      }

      int32_t delay = DELAY_TIME;
      int32_t duration = DURATION_TIME;
      delay = delay * i;
      const pie_value_t* new_pie = old_pie_data + nr - 1 - i;
      pie_slice_set_start_angle(iter, new_pie->start_angle);
      char param[100];
      tk_snprintf(param, sizeof(param),
                  "value(name=%s, to=%d, duration=%d, delay=%d, easing=sin_out)", SAVE_EXPLODED,
                  new_pie->value, duration, delay);
      widget_create_animator(iter, param);
      WIDGET_FOR_EACH_CHILD_END();
    }
    timer_add(on_save_exploded_timer, win, 1000 / 60);

    return RET_REMOVE;
  }

  return RET_REPEAT;
}

/**
 * 点击创建新饼图或者拱形
 */
static ret_t on_function_new_pie_click(void* ctx, event_t* e) {
  // TODO: 在此添加控件事件处理程序代码
  widget_t* win = (widget_t*)ctx;
  (void)e;

  create_animator_to_zero(win);
  timer_add(on_new_pie_timer, win, 1000 / 60);

  return RET_OK;
}

/**
 * 点击拱形按钮
 */
static ret_t on_function_arch_click(void* ctx, event_t* e) {
  // TODO: 在此添加控件事件处理程序代码
  widget_t* win = (widget_t*)ctx;

  create_animator_to_zero(win);
  timer_add(on_arch_timer, win, 1000 / 60);

  widget_t* target = (widget_t*)e->target;

  value_t v;
  ret_t result = widget_get_prop(win, "is_arch", &v);
  bool_t flag = (result == RET_NOT_FOUND) ? FALSE : value_bool(&v);
  if (flag) {
    widget_use_style(target, "pie_annular");
  } else {
    widget_use_style(target, "pie_circle");
  }
  widget_invalidate(target, NULL);

  return RET_OK;
}

/**
 * 点击关闭按钮
 */
static ret_t on_close_click(void* ctx, event_t* e) {
  return navigator_back();
}

/**
 * 点击扇形文本
 */
static ret_t on_pie_label_click(void* ctx, event_t* e) {
  // TODO: 在此添加控件事件处理程序代码
  widget_t* win = WIDGET(ctx);
  widget_t* target = (widget_t*)e->target;
  char text[64] = "";
  widget_get_text_utf8(target, text, ARRAY_SIZE(text));
  widget_t* pie_slice = get_pie_slice(win, text);
  pie_slice_set_exploded_4_others(pie_slice);

  str_t str;
  str_init(&str, 20);

  WIDGET_FOR_EACH_CHILD_BEGIN(target->parent, iter, i)
  const char* name = iter->name;
  str_set(&str, name);
  if (tk_str_eq(name, "pie_label")) {
    widget_get_text_utf8(iter, text, ARRAY_SIZE(text));
    widget_t* pie_slice = get_pie_slice(win, text);
    bool_t flag = widget_get_prop_bool(pie_slice, PIE_SLICE_PROP_IS_EXPLODED, FALSE);
    if (flag) {
      widget_use_style(iter, "pie_label_press");
    } else {
      widget_use_style(iter, "pie_label");
    }
    widget_invalidate(iter, NULL);
  }
  WIDGET_FOR_EACH_CHILD_END();
  str_reset(&str);

  return RET_OK;
}

/**
 * 点击扇形
 */
static ret_t on_pie_slice_pointer_down(void* ctx, event_t* e) {
  // TODO: 在此添加控件事件处理程序代码
  widget_t* win = WIDGET(ctx);
  widget_t* target = (widget_t*)e->target;

  str_t str;
  str_init(&str, 20);

  WIDGET_FOR_EACH_CHILD_BEGIN(target, iter, i)
  pie_slice_t* pie_slice = (pie_slice_t*)iter;
  str_set(&str, pie_slice->widget.name);
  str_replace(&str, "slice", "label");
  widget_t* result = widget_lookup(win, str.str, TRUE);
  if (pie_slice->is_exploded) {
    widget_use_style(result, "pie_label_press");
  } else {
    widget_use_style(result, "pie_label");
  }
  widget_invalidate(result, NULL);
  WIDGET_FOR_EACH_CHILD_END();
  str_reset(&str);

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
    if (tk_str_eq(name, "function_new_pie")) {
      widget_on(widget, EVT_CLICK, on_function_new_pie_click, win);
    } else if (tk_str_eq(name, "function_arch")) {
      widget_on(widget, EVT_CLICK, on_function_arch_click, win);
    } else if (tk_str_eq(name, "close")) {
      widget_on(widget, EVT_CLICK, on_close_click, NULL);
    } else if (tk_str_eq(name, "pie_label")) {
      widget_on(widget, EVT_CLICK, on_pie_label_click, win);
    } else if (strstr(name, "_slice")) {
      pie_slice_set_value(widget, 0);
      widget_on(widget, EVT_POINTER_DOWN, on_pie_slice_pointer_down, win);
    }
  }

  return RET_OK;
}

/**
 * 打开窗口时使用动画展示饼图
 */
static ret_t on_window_open(void* ctx, event_t* e) {
  widget_t* open = WIDGET(e->target);

  if (open != NULL && open->name != NULL) {
    timer_info_t info;
    info.ctx = ctx;
    on_new_pie_timer(&info);
  }

  return RET_REMOVE;
}

/**
 * 初始化窗口
 */
ret_t window_pie_init(widget_t* win, void* ctx) {
  (void)ctx;
  return_value_if_fail(win != NULL, RET_BAD_PARAMS);

  widget_foreach(win, visit_init_child, win);
  widget_on(win, EVT_WINDOW_OPEN, on_window_open, win);

  return RET_OK;
}
