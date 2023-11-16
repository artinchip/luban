/**
 * File:   demo_ui_old_app.c
 * Author: AWTK Develop Team
 * Brief:  demoui old
 *
 * Copyright (c) 2018 - 2020  Guangzhou ZHIYUAN Electronics Co.,Ltd.
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
 * 2018-02-16 Li XianJing <xianjimli@hotmail.com> created
 *
 */

/*
 * XXX: 本示例只是为了展示功能，不适合作为编写代码参考，编写代码请参考：https://github.com/zlgopen/awtk-c-demos
 */

#include "awtk.h"
#include "ext_widgets.h"
#include "base/font_manager.h"
#include "base/event_recorder_player.h"

#define DEMOUI_MAIN_WINDOW_NAME "main"
#define SCROLL_BAR_H_WIDGT_NAME "bar_h"
#define SCROLL_BAR_V_WIDGT_NAME "bar_v"
#define SCROLL_GRID_SCROLL_WIDGT_NAME "grid_scroll_view"

static ret_t on_clone_tab(void* ctx, event_t* e);
static ret_t widget_clone_tab(widget_t* widget);
static void install_click_hander(widget_t* widget);
static void open_window(const char* name, widget_t* to_close);

static ret_t window_on_drop_file(void* ctx, event_t* e) {
  widget_t* win = WIDGET(ctx);
  widget_t* label = widget_lookup(win, "filename", TRUE);
  drop_file_event_t* drop = drop_file_event_cast(e);

  widget_set_text_utf8(label, drop->filename);

  return RET_OK;
}

uint32_t tk_mem_speed_test(void* buffer, uint32_t length, uint32_t* pmemcpy_speed,
                           uint32_t* pmemset_speed) {
  uint32_t i = 0;
  uint32_t cost = 0;
  uint32_t total_cost = 0;
  uint32_t memcpy_speed = 0;
  uint32_t memset_speed = 0;
  uint32_t max_size = 100 * 1024 * 1024;
  uint64_t start = time_now_ms();
  uint32_t nr = max_size / length;

  for (i = 0; i < nr; i++) {
    memset(buffer, 0x00, length);
  }
  cost = time_now_ms() - start;
  total_cost = cost;
  if (cost) {
    memset_speed = ((max_size / cost) * 1000) / 1024;
  }

  start = time_now_ms();
  for (i = 0; i < nr; i++) {
    uint32_t half = length >> 1;
    memcpy(buffer, (char*)buffer + half, half);
    memcpy((char*)buffer + half, buffer, half);
  }
  cost = time_now_ms() - start;
  total_cost += cost;

  if (cost) {
    memcpy_speed = ((max_size / cost) * 1000) / 1024;
  }

  if (pmemset_speed != NULL) {
    *pmemset_speed = memset_speed;
  }

  if (pmemcpy_speed != NULL) {
    *pmemcpy_speed = memcpy_speed;
  }

  return total_cost;
}

static ret_t main_window_on_key_up(void* ctx, event_t* e) {
  key_event_t* evt = (key_event_t*)e;

  if (evt->key == TK_KEY_s) {
    system_info_t* info = system_info();
    window_manager_resize(window_manager(), tk_max(160, info->lcd_w / 2),
                          tk_max(160, info->lcd_h / 2));
  } else if (evt->key == TK_KEY_d) {
    system_info_t* info = system_info();
    window_manager_resize(window_manager(), tk_min(1920, info->lcd_w * 2),
                          tk_min(1920, info->lcd_h * 2));
  } else if (evt->key == TK_KEY_a) {
    native_window_t* nw = widget_get_native_window(widget_get_child(window_manager(), 0));
    native_window_set_title(nw, "AWTK Simulator");
  }

  return RET_OK;
}

static ret_t window_to_background(void* ctx, event_t* e) {
  widget_t* win = WIDGET(e->target);
  log_debug("%s to_background\n", win->name);
  (void)win;
  return RET_OK;
}

static ret_t window_to_foreground(void* ctx, event_t* e) {
  widget_t* win = WIDGET(e->target);
  log_debug("%s to_foreground\n", win->name);
  (void)win;
  return RET_OK;
}

static ret_t on_context_menu(void* ctx, event_t* e) {
  open_window("menu_point", NULL);

  return RET_OK;
}

static ret_t update_title_on_timer(const timer_info_t* info) {
  char text[128];
  tk_snprintf(text, sizeof(text), "change title:%d", random() % 100);

  widget_set_text_utf8(WIDGET(info->ctx), text);

  return RET_REPEAT;
}

static void open_window(const char* name, widget_t* to_close) {
  bool_t is_single_main_win =
      widget_lookup(window_manager(), DEMOUI_MAIN_WINDOW_NAME, FALSE) == NULL;
  widget_t* win = to_close ? window_open_and_close(name, to_close) : window_open(name);

  if ((tk_str_eq(name, DEMOUI_MAIN_WINDOW_NAME) && is_single_main_win) || widget_is_support_highlighter(win)) {
    widget_on(win, EVT_KEY_UP, main_window_on_key_up, win);
  }

  widget_on(win, EVT_WINDOW_TO_BACKGROUND, window_to_background, win);
  widget_on(win, EVT_WINDOW_TO_FOREGROUND, window_to_foreground, win);

  install_click_hander(win);

  if (tk_str_eq(name, "tab_scrollable")) {
    widget_clone_tab(win);
    widget_clone_tab(win);
    widget_clone_tab(win);
    widget_clone_tab(win);
  }

  if (tk_str_eq(name, "list_view")) {
    widget_add_timer(win, update_title_on_timer, 1000);
  } else if (tk_str_eq(name, "drop_file")) {
    widget_on(win, EVT_DROP_FILE, window_on_drop_file, win);
  }

  if (tk_str_eq(widget_get_type(win), WIDGET_TYPE_DIALOG)) {
    int32_t ret = dialog_modal(win);

    if (tk_str_eq(win->name, "back_to_home") && ret == 0) {
      window_manager_back_to_home(window_manager());
    }
  }

  tk_mem_dump();
}

static ret_t on_paint_linear_gradient(void* ctx, event_t* e) {
  paint_event_t* evt = paint_event_cast(e);
  canvas_t* c = evt->c;
  widget_t* widget = WIDGET(e->target);
  vgcanvas_t* vg = canvas_get_vgcanvas(c);
  color_t scolor = color_init(0xff, 0, 0, 0xff);
  color_t ecolor = color_init(0xff, 0, 0, 0x0);
  uint32_t spacing = 10;
  uint32_t w = (widget->w - 3 * spacing) >> 1;
  uint32_t h = (widget->h - 3 * spacing) >> 1;
  rect_t r = rect_init(spacing, spacing, w, h);

  vgcanvas_save(vg);
  vgcanvas_translate(vg, c->ox, c->oy);

  vgcanvas_rect(vg, r.x, r.y, r.w, r.h);
  vgcanvas_set_fill_linear_gradient(vg, r.x, r.y, r.x + r.w, r.y + r.h, scolor, ecolor);
  vgcanvas_fill(vg);

  vgcanvas_translate(vg, r.x + r.w, 0);
  vgcanvas_rect(vg, r.x, r.y, r.w, r.h);
  ecolor = color_init(0, 0, 0xff, 0xff);
  vgcanvas_set_fill_linear_gradient(vg, r.x, r.y, r.x + r.w, r.y, scolor, ecolor);
  vgcanvas_fill(vg);

  vgcanvas_translate(vg, 0, r.y + r.h);
  vgcanvas_rect(vg, r.x, r.y, r.w, r.h);
  ecolor = color_init(0, 0, 0xff, 0xff);
  vgcanvas_set_fill_linear_gradient(vg, r.x + r.w * 0.6, r.y, r.x + r.w * 0.4, r.y, scolor, ecolor);
  vgcanvas_fill(vg);

  vgcanvas_translate(vg, -(r.x + r.w), 0);
  vgcanvas_rect(vg, r.x, r.y, r.w, r.h);
  ecolor = color_init(0, 0, 0xff, 0xff);
  vgcanvas_set_fill_linear_gradient(vg, r.x, r.y, r.x, r.y + r.h, scolor, ecolor);
  vgcanvas_fill(vg);

  vgcanvas_restore(vg);

  return RET_OK;
}

static ret_t on_paint_radial_gradient(void* ctx, event_t* e) {
  paint_event_t* evt = paint_event_cast(e);
  canvas_t* c = evt->c;
  widget_t* widget = WIDGET(e->target);
  vgcanvas_t* vg = canvas_get_vgcanvas(c);
  color_t scolor = color_init(0xff, 0, 0, 0xff);
  color_t ecolor = color_init(0xff, 0, 0, 0);
  uint32_t spacing = 10;
  uint32_t w = (widget->w - 3 * spacing) >> 1;
  uint32_t h = (widget->h - 3 * spacing) >> 1;
  rect_t r = rect_init(spacing, spacing, w, h);
  uint32_t radial = tk_min(w, h) / 2;

  vgcanvas_save(vg);
  vgcanvas_translate(vg, c->ox, c->oy);

  vgcanvas_rect(vg, r.x, r.y, r.w, r.h);
  vgcanvas_set_fill_radial_gradient(vg, r.x + w / 2, r.y + h / 2, 0, radial, scolor, ecolor);
  vgcanvas_fill(vg);

  vgcanvas_translate(vg, r.x + r.w, 0);
  vgcanvas_rect(vg, r.x, r.y, r.w, r.h);
  ecolor = color_init(0, 0, 0xff, 0xff);
  vgcanvas_set_fill_radial_gradient(vg, r.x + w / 2, r.y + h / 2, radial / 4, radial, scolor,
                                    ecolor);
  vgcanvas_fill(vg);

  vgcanvas_translate(vg, 0, r.y + r.h);
  vgcanvas_rect(vg, r.x, r.y, r.w, r.h);
  ecolor = color_init(0, 0xff, 0xff, 0xff);
  vgcanvas_set_fill_radial_gradient(vg, r.x + w / 2, r.y + h / 2, radial / 3, radial, scolor,
                                    ecolor);
  vgcanvas_fill(vg);

  vgcanvas_translate(vg, -(r.x + r.w), 0);
  vgcanvas_rect(vg, r.x, r.y, r.w, r.h);
  ecolor = color_init(0, 0, 0xff, 0xff);
  vgcanvas_set_fill_radial_gradient(vg, r.x + w / 2, r.y + h / 2, radial / 2, radial, scolor,
                                    ecolor);
  vgcanvas_fill(vg);

  vgcanvas_restore(vg);

  return RET_OK;
}

static ret_t on_paint_stroke_gradient(void* ctx, event_t* e) {
  paint_event_t* evt = paint_event_cast(e);
  canvas_t* c = evt->c;
  widget_t* widget = WIDGET(e->target);
  vgcanvas_t* vg = canvas_get_vgcanvas(c);
  color_t scolor = color_init(0xff, 0, 0, 0xff);
  color_t ecolor = color_init(0, 0xff, 0, 0xff);
  uint32_t r = tk_min(widget->w, widget->h) / 3;

  vgcanvas_save(vg);
  vgcanvas_translate(vg, c->ox, c->oy);
  vgcanvas_set_stroke_linear_gradient(vg, 0, 0, widget->w, widget->h, scolor, ecolor);

  vgcanvas_move_to(vg, 0, 0);
  vgcanvas_set_line_width(vg, 5);

  vgcanvas_line_to(vg, widget->w / 2, widget->h);
  vgcanvas_line_to(vg, widget->w / 2, 0);
  vgcanvas_line_to(vg, widget->w, widget->h);
  vgcanvas_stroke(vg);

  vgcanvas_begin_path(vg);
  vgcanvas_arc(vg, widget->w / 2, widget->h / 2, r, 0, M_PI * 2, FALSE);
  vgcanvas_stroke(vg);

  vgcanvas_restore(vg);

  return RET_OK;
}

#include "vg_common.inc"

static ret_t on_paint_vgcanvas(void* ctx, event_t* e) {
  paint_event_t* evt = paint_event_cast(e);
  canvas_t* c = evt->c;
  vgcanvas_t* vg = canvas_get_vgcanvas(c);

  vgcanvas_save(vg);
  vgcanvas_translate(vg, c->ox, c->oy);
  vgcanvas_set_line_width(vg, 1);
  vgcanvas_set_stroke_color(vg, color_init(0, 0xff, 0, 0xff));
  vgcanvas_set_fill_color(vg, color_init(0xff, 0, 0, 0xff));

  draw_basic_shapes(vg, FALSE);
  vgcanvas_translate(vg, 0, 50);
  draw_basic_shapes(vg, TRUE);
  vgcanvas_translate(vg, 0, 50);
  stroke_lines(vg);
  vgcanvas_translate(vg, 0, 50);
  draw_image(vg);

  vgcanvas_translate(vg, 50, 100);
  draw_matrix(vg);
  vgcanvas_translate(vg, 0, 100);

  draw_text(vg);
  vgcanvas_restore(vg);

  return RET_OK;
}

static ret_t on_timer_show_toast_when_opening(const timer_info_t* info) {
  dialog_toast("Hello AWTK!\nThis is a toast(2)!", 2000);

  return RET_REMOVE;
}

static ret_t on_timer_show_toast_when_closing(const timer_info_t* info) {
  dialog_toast("Hello AWTK!\nThis is a toast(3)!", 2000);

  return RET_REMOVE;
}

static ret_t on_switch_to_window(void* ctx, event_t* e) {
  const char* name = (const char*)ctx;
  widget_t* win = widget_get_window(WIDGET(e->target));
  widget_t* home = widget_lookup(window_manager(), name, TRUE);

  window_manager_switch_to(window_manager(), win, home, TRUE);

  return RET_OK;
}

static ret_t on_open_window(void* ctx, event_t* e) {
  const char* name = (const char*)ctx;

  if (tk_str_eq(name, "toast")) {
    timer_add(on_timer_show_toast_when_opening, NULL, 0);
    timer_add(on_timer_show_toast_when_closing, NULL, 4700);
    dialog_toast("Hello AWTK!\nThis is a toast(1)!", 4000);
  } else if (tk_str_eq(name, "info")) {
    dialog_info("info", "hello awtk");
  } else if (tk_str_eq(name, "warn")) {
    dialog_warn(NULL, "Hello AWTK!\nDanger!!!");
  } else if (tk_str_eq(name, "confirm")) {
    dialog_confirm(NULL, "Hello AWTK!\nAre you sure to close?");
  } else {
    widget_t* target = widget_lookup(window_manager(), name, TRUE);
    if (target != NULL && !(widget_is_overlay(target) && !widget_is_always_on_top(target))) {
      widget_t* win = widget_get_window(WIDGET(e->target));
      window_manager_switch_to(window_manager(), win, target, FALSE);
    } else {
      open_window(name, NULL);
    }
  }

  (void)e;

#if 0
  /*for test only*/
  widget_on(WIDGET(e->target), EVT_CLICK, on_open_window, (void*)name);
  return RET_REMOVE;
#else
  return RET_OK;
#endif
}

static ret_t on_fullscreen(void* ctx, event_t* e) {
  widget_t* btn = WIDGET(ctx);
  window_t* win = WINDOW(widget_get_window(btn));

  if (win->fullscreen) {
    window_set_fullscreen(WIDGET(win), FALSE);
    widget_set_text_utf8(btn, "Fullscreen");
  } else {
    window_set_fullscreen(WIDGET(win), TRUE);
    widget_set_text_utf8(btn, "Unfullscreen");
  }

  return RET_OK;
}

static ret_t on_unload_image(void* ctx, event_t* e) {
  image_manager_unload_unused(image_manager(), 0);

  return RET_OK;
}

static ret_t on_close(void* ctx, event_t* e) {
  widget_t* win = WIDGET(ctx);
  (void)e;
  return window_close(win);
}

static ret_t on_start(void* ctx, event_t* e) {
  widget_start_animator(NULL, NULL);

  return RET_OK;
}

static ret_t on_pause(void* ctx, event_t* e) {
  widget_pause_animator(NULL, NULL);

  return RET_OK;
}

static ret_t on_stop(void* ctx, event_t* e) {
  widget_stop_animator(NULL, NULL);

  return RET_OK;
}

static ret_t on_send_key(void* ctx, event_t* e) {
  widget_t* button = WIDGET(e->target);
  char text[2];
  text[0] = (char)button->text.str[0];
  text[1] = '\0';

  input_method_commit_text(input_method(), text);

  return RET_OK;
}

static ret_t on_backspace(void* ctx, event_t* e) {
  input_method_dispatch_key(input_method(), TK_KEY_BACKSPACE);

  return RET_OK;
}

static ret_t on_quit(void* ctx, event_t* e) {
  widget_t* dialog = WIDGET(ctx);

  dialog_quit(dialog, 0);
  (void)e;
  return RET_OK;
}

static ret_t on_back_to_home(void* ctx, event_t* e) {
  widget_t* dialog = WIDGET(ctx);

  dialog_quit(dialog, 0);

  (void)e;
  return RET_OK;
}

static ret_t on_quit_app(void* ctx, event_t* e) {
  tk_quit();

  return RET_OK;
}

static ret_t on_change_cursor(void* ctx, event_t* e) {
  widget_t* widget = WIDGET(e->target);
  widget_set_pointer_cursor(widget, widget->name);

  return RET_OK;
}

static ret_t on_combo_box_will_change(void* ctx, event_t* e) {
  widget_t* combo_box = WIDGET(ctx);
  widget_t* win = widget_get_window(combo_box);
  widget_t* value = widget_lookup(win, "old_value", TRUE);

  if (value != NULL) {
    widget_set_tr_text(value, combo_box_get_text(combo_box));
  }

  return RET_OK;
}

static ret_t on_image_animation_set_interval(void* ctx, event_t* e) {
  widget_t* s_image_animation = WIDGET(ctx);
  int32_t interval = widget_get_prop_int(s_image_animation, "interval", 0);

  widget_set_prop_int(s_image_animation, "interval", interval / 100);
  return RET_OK;
}

static ret_t on_pages_add_child(void* ctx, event_t* e) {
  widget_t* widget = WIDGET(ctx);
  widget_t* win = widget_get_window(widget);

  widget_t* close_btn = widget_lookup(win, "close", TRUE);
  widget_t* text_label = widget_lookup(win, "text", TRUE);
  widget_t* tab_button_parent = widget_lookup_by_type(win, "tab_button", TRUE)->parent;

  if (close_btn != NULL) {
    widget_on(close_btn, EVT_CLICK, on_close, win);
  }

  if (text_label != NULL) {
    WIDGET_FOR_EACH_CHILD_BEGIN(tab_button_parent, iter, i)

    if (tk_str_eq(iter->vt->type, "tab_button")) {
      if (TAB_BUTTON(iter)->value) {
        widget_set_text_utf8(text_label, iter->name);
      }
    }

    WIDGET_FOR_EACH_CHILD_END();
  }

  return RET_OK;
}

static ret_t on_combo_box_changed(void* ctx, event_t* e) {
  widget_t* combo_box = WIDGET(ctx);
  widget_t* win = widget_get_window(combo_box);
  widget_t* value = widget_lookup(win, "value", TRUE);

  if (value != NULL) {
    widget_set_tr_text(value, combo_box_get_text(combo_box));
  }

  return RET_OK;
}

static ret_t on_combo_box_item_click(void* ctx, event_t* e) {
  char name[10] = "";
  char price[10] = "";
  char fruit_price[20] = "";
  widget_t* combo_box = WIDGET(ctx);
  widget_t* combo_box_item = e->target;
  widget_t* name_label = widget_lookup(combo_box_item, "name", FALSE);
  widget_t* price_label = widget_lookup(combo_box_item, "price", FALSE);

  widget_get_text_utf8(name_label, name, sizeof(name));
  widget_get_text_utf8(price_label, price, sizeof(price));

  tk_snprintf(fruit_price, sizeof(fruit_price), "%s  %s", name, price);
  widget_set_text_utf8(combo_box, fruit_price);

  combo_box_set_selected_index(combo_box, widget_index_of(combo_box_item));

  combo_box->target = NULL;
  combo_box->key_target = NULL;
  window_close(widget_get_window(combo_box_item));
  widget_set_focused_internal(combo_box, TRUE);

  return RET_OK;
}

static ret_t on_combo_box_ex_item_click(void* ctx, event_t* e) {
  widget_t* win = WIDGET(ctx);
  widget_t* combo_box_item = e->target;
  widget_t* value = widget_lookup(win, "value", TRUE);
  return_value_if_fail(win != NULL && combo_box_item != NULL && value != NULL, RET_BAD_PARAMS);

  widget_set_text(value, widget_get_text(combo_box_item));

  return RET_CONTINUE;
}

static ret_t on_remove_self(void* ctx, event_t* e) {
  widget_t* widget = WIDGET(ctx);
  widget_destroy(widget);

  return RET_OK;
}

static ret_t on_remove_view(void* ctx, event_t* e) {
  widget_t* widget = WIDGET(ctx);
  widget_t* iter = widget;

  while (iter != NULL) {
    if (tk_str_eq(widget_get_type(iter), WIDGET_TYPE_VIEW)) {
      if (iter->parent != NULL &&
          tk_str_eq(widget_get_type(iter->parent), WIDGET_TYPE_SLIDE_VIEW)) {
        slide_view_remove_index(iter->parent, widget_index_of(iter));
      } else {
        widget_destroy(iter);
      }
      return RET_OK;
    }
    iter = iter->parent;
  }

  return RET_FAIL;
}

static ret_t on_clone_self(void* ctx, event_t* e) {
  widget_t* widget = WIDGET(ctx);
  widget_t* clone = widget_clone(widget, NULL);
  widget_insert_child(widget->parent, widget_index_of(widget) + 1, clone);
  widget_on(clone, EVT_CLICK, on_clone_self, clone);

  return RET_OK;
}

static ret_t on_clone_view(void* ctx, event_t* e) {
  widget_t* widget = WIDGET(ctx);
  widget_t* iter = widget;
  widget_t* native_window = widget_get_window(widget);

  while (iter != NULL) {
    if (tk_str_eq(widget_get_type(iter), WIDGET_TYPE_VIEW)) {
      widget_t* clone = widget_clone(iter, iter->parent);
      widget_t* lb_view_index = widget_lookup(clone, "view_index", TRUE);

      if (lb_view_index != NULL) {
        char text[32];
        tk_snprintf(text, ARRAY_SIZE(text), "%d", widget_index_of(clone));
        widget_set_text_utf8(lb_view_index, text);
      }
      install_click_hander(clone);
      return widget_invalidate(native_window, NULL);
    }
    iter = iter->parent;
  }

  return RET_FAIL;
}

static ret_t on_remove_tab_idle(const idle_info_t* idle) {
  widget_t* iter = WIDGET(idle->ctx);
  int32_t remove_index = widget_index_of(iter);
  widget_t* pages = widget_lookup_by_type(iter->parent->parent, WIDGET_TYPE_PAGES, FALSE);
  widget_t* tab_btn_group =
      widget_lookup_by_type(iter->parent->parent, WIDGET_TYPE_TAB_BUTTON_GROUP, FALSE);

  return_value_if_fail(remove_index >= 0, RET_BAD_PARAMS);

  if (tab_btn_group != NULL) {
    widget_t* tab_btn = widget_get_child(tab_btn_group, remove_index);
    if (tab_btn != NULL) {
      widget_destroy(tab_btn);
    }
  }

  if (pages != NULL) {
    widget_t* page = widget_get_child(pages, remove_index);
    if (page != NULL) {
      widget_destroy(page);
    }
  }

  return RET_REMOVE;
}

static ret_t on_remove_tab(void* ctx, event_t* e) {
  widget_t* iter = WIDGET(e->target);

  while (iter != NULL && iter->parent != NULL && iter->parent->parent != NULL) {
    if (tk_str_eq(widget_get_type(iter->parent), WIDGET_TYPE_PAGES) ||
        tk_str_eq(widget_get_type(iter->parent), WIDGET_TYPE_TAB_BUTTON_GROUP)) {
      widget_add_idle(iter, on_remove_tab_idle);
      return RET_STOP;
    }
    iter = iter->parent;
  }

  return RET_STOP;
}

static ret_t widget_clone_tab(widget_t* widget) {
  char text[32];
  widget_t* view = widget_lookup(widget, "clone_view", TRUE);
  widget_t* button = widget_lookup(widget, "clone_button", TRUE);
  widget_t* new_view = widget_clone(view, view->parent);
  widget_t* new_button = widget_clone(button, button->parent);
  widget_t* remove_tab_btn = widget_lookup(new_button, "remove_tab", TRUE);

  if (remove_tab_btn != NULL) {
    widget_on(remove_tab_btn, EVT_POINTER_UP, on_remove_tab, widget);
    tk_snprintf(text, sizeof(text), "Clone(%d)    ", widget_index_of(new_button));
  } else {
    tk_snprintf(text, sizeof(text), "Clone(%d)", widget_index_of(new_button));
  }
  widget_set_text_utf8(new_button, text);

  WIDGET_FOR_EACH_CHILD_BEGIN(new_button->parent, iter, i)
  if (widget_get_value(iter)) {
    widget_set_value(iter, FALSE);
  }
  WIDGET_FOR_EACH_CHILD_END();

  widget_layout_children(new_button->parent);
  widget_set_value(new_button, TRUE);

  remove_tab_btn = widget_lookup(new_view, "remove_tab", TRUE);
  if (remove_tab_btn != NULL) {
    widget_on(remove_tab_btn, EVT_CLICK, on_remove_tab, widget);
  }
  widget_child_on(new_view, "clone_tab", EVT_CLICK, on_clone_tab, widget_get_window(widget));

  widget_set_text_utf8(widget_lookup_by_type(new_view, WIDGET_TYPE_LABEL, TRUE), text);

  return RET_OK;
}

static ret_t on_clone_tab(void* ctx, event_t* e) {
  return widget_clone_tab(WIDGET(ctx));
}

static widget_t* find_tab_visible_target(widget_t* widget, const char* name) {
  widget_t* tab_btn_group =
      widget_lookup_by_type(widget->parent, WIDGET_TYPE_TAB_BUTTON_GROUP, FALSE);

  if (tab_btn_group != NULL) {
    return widget_lookup(tab_btn_group, name, FALSE);
  }
  return NULL;
}

static ret_t on_tab_visible_changed(void* ctx, event_t* e) {
  widget_t* target = WIDGET(ctx);
  widget_t* widget = WIDGET(e->target);
  bool_t tab_visible = widget_get_value_int(widget) != 0;

  widget_set_visible(target, tab_visible);
  widget_set_enable(target, tab_visible);
  return RET_OK;
}

static ret_t on_show_fps(void* ctx, event_t* e) {
  widget_t* button = WIDGET(ctx);
  widget_t* widget = window_manager();
  window_manager_t* wm = WINDOW_MANAGER(widget);

  widget_invalidate(widget, NULL);
  window_manager_set_show_fps(widget, !wm->show_fps);
  widget_set_text(button, wm->show_fps ? L"Hide FPS" : L"Show FPS");

  return RET_OK;
}

extern ret_t assets_set_global_theme(const char* name);
static ret_t on_reload_theme_test(void* ctx, event_t* e) {
  widget_t* widget = WIDGET(e->target);
  assets_manager_t* am = widget_get_assets_manager(widget);
  const char* theme = "default";

  if (tk_str_eq(am->theme, theme)) {
    theme = "dark";
  }

  assets_set_global_theme(theme);

  return RET_OK;
}

static ret_t on_snapshot(void* ctx, event_t* e) {
#ifndef AWTK_WEB
  bitmap_t* bitmap = widget_take_snapshot(window_manager());
  bitmap_save_png(bitmap, "test.png");
  bitmap_destroy(bitmap);
#endif /*AWTK_WEB*/

  return RET_OK;
}

static ret_t on_mem_test(void* ctx, event_t* e) {
  char text[32];
  uint32_t size = 100 * 1024;
  uint32_t memset_speed = 0;
  uint32_t memcpy_speed = 0;
  widget_t* win = WIDGET(ctx);
  widget_t* label_memset = widget_lookup(win, "memset", TRUE);
  widget_t* label_cost = widget_lookup(win, "cost", TRUE);
  widget_t* label_memcpy = widget_lookup(win, "memcpy", TRUE);
  void* buff = TKMEM_ALLOC(size);
  uint32_t cost = tk_mem_speed_test(buff, size, &memcpy_speed, &memset_speed);
  TKMEM_FREE(buff);

  tk_snprintf(text, sizeof(text), "%ums", cost);
  widget_set_text_utf8(label_cost, text);

  tk_snprintf(text, sizeof(text), "memset: %uK/s", memset_speed);
  widget_set_text_utf8(label_memset, text);

  tk_snprintf(text, sizeof(text), "memcpy: %uK/s", memcpy_speed);
  widget_set_text_utf8(label_memcpy, text);

  font_manager_shrink_cache(font_manager(), 1);

  return RET_OK;
}

static ret_t progress_bar_animate_delta(widget_t* win, const char* name, int32_t delta) {
  widget_t* progress_bar = widget_lookup(win, name, TRUE);
  int32_t value = (PROGRESS_BAR(progress_bar)->value + delta);
  widget_animate_value_to(progress_bar, tk_max(0, tk_min(100, value)), 500);

  return RET_OK;
}

static ret_t on_inc(void* ctx, event_t* e) {
  widget_t* win = WIDGET(ctx);
  progress_bar_animate_delta(win, "bar1", 10);
  progress_bar_animate_delta(win, "bar2", 10);
  (void)e;
  return RET_OK;
}

static ret_t on_dec(void* ctx, event_t* e) {
  widget_t* win = WIDGET(ctx);
  progress_bar_animate_delta(win, "bar1", -10);
  progress_bar_animate_delta(win, "bar2", -10);

  (void)e;
  return RET_OK;
}

static ret_t on_change_font_size(void* ctx, event_t* e) {
  float_t font_scale = 1;
  widget_t* win = WIDGET(ctx);

  if (widget_get_value(widget_lookup(win, "font_small", TRUE))) {
    font_scale = 0.9;
  } else if (widget_get_value(widget_lookup(win, "font_big", TRUE))) {
    font_scale = 1.1;
  }
  system_info_set_font_scale(system_info(), font_scale);
  widget_invalidate_force(win, NULL);

  return RET_OK;
}

static ret_t on_change_locale(void* ctx, event_t* e) {
  char country[3];
  char language[3];
  const char* str = (const char*)ctx;

  tk_strncpy(language, str, 2);
  tk_strncpy(country, str + 3, 2);
  locale_info_change(locale_info(), language, country);

  return RET_OK;
}

static widget_t* find_bind_value_target(widget_t* widget, const char* name) {
  widget_t* target = NULL;
  return_value_if_fail(widget != NULL && name != NULL, NULL);

  if (tk_str_start_with(name, "bind_value:")) {
    widget_t* parent = widget->parent;
    const char* subname = NULL;
    tokenizer_t t;

    name += 11;
    tokenizer_init(&t, name, tk_strlen(name), "/");

    while ((subname = tokenizer_next(&t)) != NULL) {
      if (tokenizer_has_more(&t)) {
        if (tk_str_eq(subname, "..")) {
          parent = parent->parent;
        } else {
          while (parent != NULL) {
            widget_t* tmp = widget_lookup(parent, subname, FALSE);
            if (tmp != NULL) {
              parent = tmp;
              break;
            } else {
              parent = parent->parent;
            }
          }
        }
      } else {
        target = widget_lookup(parent, subname, FALSE);
      }
    }
    tokenizer_deinit(&t);
  }
  return target;
}

static ret_t on_bind_value_changed(void* ctx, event_t* e) {
  char prop_name[16] = {0};
  widget_t* widget = WIDGET(ctx);
  widget_t* target = WIDGET(e->target);
  return_value_if_fail(widget != NULL && target != NULL, RET_BAD_PARAMS);

  tk_snprintf(prop_name, sizeof(prop_name), "%s%s", WIDGET_PROP_ANIMATE_PREFIX, WIDGET_PROP_VALUE);

  return widget_set_prop_float(widget, prop_name, widget_get_value(target));
}

static ret_t on_action_list(void* ctx, event_t* e) {
  widget_t* target = NULL;
  const char* name = NULL;
  widget_t* widget = WIDGET(ctx);
  return_value_if_fail(widget != NULL, RET_BAD_PARAMS);
  name = widget->name;
  if (name != NULL) {
    name = name + sizeof("action_list:");
    do {
      if (strstr(name, "open:") == name) {
        char win_name[128] = {0};
        uint32_t win_name_len = 0;
        name = name + sizeof("open:") - 1;
        win_name_len = strchr(name, ',') - name;
        win_name_len = tk_min(win_name_len, strchr(name, ')') - name);

        tk_strncpy(win_name, name, win_name_len);
        target = window_open(win_name);
        install_click_hander(target);
        name = name + win_name_len + 1;
      } else if (strstr(name, "close") == name) {
        name = name + sizeof("close");
        if (target != NULL) {
          window_close(target);
        }
      } else if (strstr(name, "quit") == name) {
        name = name + sizeof("quit");
        if (strstr(name, "this")) {
          widget_t* this_win = widget_get_window(widget);
          name = name + sizeof("this");
          if (this_win != NULL) {
            dialog_quit(this_win, 0);
          }
        } else {
          if (target != NULL) {
            dialog_quit(target, 0);
          }
        }
      } else {
        break;
      }
    } while (1);
  }
  return RET_OK;
}

static int32_t scroll_bar_value_to_scroll_view_offset_y(scroll_bar_t* scroll_bar,
                                                        scroll_view_t* sv) {
  int32_t range = 0;
  float_t percent = 0;
  range = scroll_bar->virtual_size;
  percent = range > 0 ? (float_t)scroll_bar->value / (float_t)(range) : 0;
  return percent * (sv->virtual_h - sv->widget.h);
}

static int32_t scroll_bar_value_to_scroll_view_offset_x(scroll_bar_t* scroll_bar,
                                                        scroll_view_t* sv) {
  int32_t range = 0;
  float_t percent = 0;
  range = scroll_bar->virtual_size;
  percent = range > 0 ? (float_t)scroll_bar->value / (float_t)(range) : 0;
  return percent * (sv->virtual_w - sv->widget.w);
}

static ret_t scroll_bar_on_value_changed(void* ctx, event_t* e) {
  widget_t* tmp = WIDGET(ctx);
  widget_t* parent = tmp->parent;
  scroll_view_t* sv = SCROLL_VIEW(widget_lookup(parent, SCROLL_GRID_SCROLL_WIDGT_NAME, FALSE));
  scroll_bar_t* scroll_bar_h = SCROLL_BAR(widget_lookup(parent, SCROLL_BAR_H_WIDGT_NAME, FALSE));
  scroll_bar_t* scroll_bar_v = SCROLL_BAR(widget_lookup(parent, SCROLL_BAR_V_WIDGT_NAME, FALSE));

  int32_t offset_x = scroll_bar_value_to_scroll_view_offset_x(scroll_bar_h, sv);
  int32_t offset_y = scroll_bar_value_to_scroll_view_offset_y(scroll_bar_v, sv);

  scroll_view_set_offset(WIDGET(sv), offset_x, offset_y);

  return RET_OK;
}

static ret_t on_idle_scroll_view_set_virtual_wh(const idle_info_t* idle) {
  scroll_view_t* sv = SCROLL_VIEW(idle->ctx);
  widget_t* parent = sv->widget.parent;
  widget_t* bar_h = widget_lookup(parent, SCROLL_BAR_H_WIDGT_NAME, FALSE);
  widget_t* bar_v = widget_lookup(parent, SCROLL_BAR_V_WIDGT_NAME, FALSE);
  scroll_bar_set_params(bar_h, sv->virtual_w, 10);
  scroll_bar_set_params(bar_v, sv->virtual_h, 10);
  return RET_OK;
}

static ret_t on_click_next_page(void* ctx, event_t* e) {
  const char* name = (const char*)ctx;
  widget_t* pages = widget_lookup(window_manager(), name, TRUE);
  int32_t curr_page = widget_get_prop_int(pages, WIDGET_PROP_CURR_PAGE, 0);
  int32_t max_page = widget_get_prop_int(pages, WIDGET_PROP_PAGE_MAX_NUMBER, 0);
  int32_t next_page = (curr_page + 1) % max_page;

  return widget_set_prop_int(pages, WIDGET_PROP_CURR_PAGE, next_page);
}

static ret_t on_click_prev_page(void* ctx, event_t* e) {
  const char* name = (const char*)ctx;
  widget_t* pages = widget_lookup(window_manager(), name, TRUE);
  int32_t curr_page = widget_get_prop_int(pages, WIDGET_PROP_CURR_PAGE, 0);
  int32_t max_page = widget_get_prop_int(pages, WIDGET_PROP_PAGE_MAX_NUMBER, 0);
  int32_t next_page = curr_page == 0 ? max_page - 1 : (curr_page - 1) % max_page;

  return widget_set_prop_int(pages, WIDGET_PROP_CURR_PAGE, next_page);
}

static ret_t on_click_clone_combo_box_ex(void* ctx, event_t* e) {
  int32_t x = 0;
  int32_t y = 0;
  widget_t* clone = NULL;
  widget_t* win = WIDGET(ctx);
  widget_t* combo_box = widget_lookup(win, "combo_box_ex_for_clone", TRUE);
  return_value_if_fail(combo_box != NULL, RET_BAD_PARAMS);

  clone = widget_clone(combo_box, win);
  x = win->w - clone->w;
  x = x > 0 ? x : 0;
  y = clone->y;
  widget_move(clone, x, y);

  return RET_OK;
}

static ret_t on_click_scroll(void* ctx, event_t* e) {
  const char* name = (const char*)ctx;
  widget_t* scroll_bar = widget_lookup(window_manager(), "scroll_bar", TRUE);
  widget_t* scroll_view = widget_lookup(window_manager(), "scroll_view", TRUE);
  return_value_if_fail(name != NULL && scroll_bar != NULL && scroll_view != NULL, RET_BAD_PARAMS);

  if (tk_str_eq(name, "bar_top")) {
    widget_set_prop_int(scroll_bar, WIDGET_PROP_VALUE, 0);
  } else if (tk_str_eq(name, "view_top")) {
    widget_set_prop_int(scroll_view, WIDGET_PROP_YOFFSET, 0);
  } else if (tk_str_eq(name, "bar_bot")) {
    int32_t max = widget_get_prop_int(scroll_bar, WIDGET_PROP_MAX, 0);
    widget_set_prop_int(scroll_bar, WIDGET_PROP_VALUE, max);
  } else if (tk_str_eq(name, "view_bot")) {
    int32_t h = widget_get_prop_int(scroll_view, WIDGET_PROP_H, 0);
    int32_t vh = widget_get_prop_int(scroll_view, WIDGET_PROP_VIRTUAL_H, 0);
    widget_set_prop_int(scroll_view, WIDGET_PROP_YOFFSET, vh - h);
  } else if (tk_str_eq(name, "item1") || tk_str_eq(name, "item2")) {
    widget_t* item = widget_lookup(window_manager(), name, TRUE);
    widget_ensure_visible_in_viewport(item);
  }

  return RET_OK;
}

static ret_t on_click_slide_view_appoint_remove_evt(void* ctx, event_t* e) {
  widget_t* widget = WIDGET(ctx);
  return_value_if_fail(widget != NULL, RET_BAD_PARAMS);
  widget_t* native_window = widget_get_window(widget);

  widget_t* slide_view = widget_lookup(widget->parent, "appoint_view", TRUE);
  return_value_if_fail(slide_view != NULL, RET_BAD_PARAMS);

  widget_t* spin_box = widget_lookup(widget->parent, "spin_appoint_index", TRUE);
  return_value_if_fail(spin_box != NULL, RET_BAD_PARAMS);

  int32_t val = edit_get_int(WIDGET(&SPIN_BOX(spin_box)->edit));
  slide_view_remove_index(slide_view, val - 1);

  return widget_invalidate(native_window, NULL);
}

static ret_t install_one(void* ctx, const void* iter) {
  widget_t* widget = WIDGET(iter);
  widget_t* win = widget_get_window(widget);

  if (widget->name != NULL) {
    const char* name = widget->name;
    if (strstr(name, "open:") == name) {
      widget_on(widget, EVT_CLICK, on_open_window, (void*)(name + 5));
      widget_on(widget, EVT_LONG_PRESS, on_open_window, (void*)(name + 5));
      if (tk_str_eq(name, "open:menu_point")) {
        widget_on(widget, EVT_CONTEXT_MENU, on_context_menu, win);
      }
    } else if (strstr(name, "switch_to:") != NULL) {
      widget_on(widget, EVT_CLICK, on_switch_to_window, (void*)(name + sizeof("switch_to")));
    } else if (tk_str_eq(name, "paint_linear_gradient")) {
      widget_on(widget, EVT_PAINT, on_paint_linear_gradient, NULL);
    } else if (tk_str_eq(name, "paint_radial_gradient")) {
      widget_on(widget, EVT_PAINT, on_paint_radial_gradient, NULL);
    } else if (tk_str_eq(name, "paint_stroke_gradient")) {
      widget_on(widget, EVT_PAINT, on_paint_stroke_gradient, NULL);
    } else if (tk_str_eq(name, "paint_vgcanvas")) {
      widget_on(widget, EVT_PAINT, on_paint_vgcanvas, NULL);
    } else if (tk_str_eq(name, "snapshot")) {
      widget_on(widget, EVT_CLICK, on_snapshot, NULL);
    } else if (tk_str_eq(name, "memtest")) {
      widget_t* win = widget_get_window(widget);
      widget_on(widget, EVT_CLICK, on_mem_test, win);
    } else if (tk_str_eq(name, "reload_theme")) {
      widget_t* win = widget_get_window(widget);
      widget_on(widget, EVT_CLICK, on_reload_theme_test, win);
    } else if (tk_str_eq(name, "show_fps")) {
      widget_on(widget, EVT_CLICK, on_show_fps, widget);
    } else if (tk_str_eq(name, "clone_self")) {
      widget_on(widget, EVT_CLICK, on_clone_self, widget);
    } else if (tk_str_eq(name, "clone_view")) {
      widget_on(widget, EVT_CLICK, on_clone_view, widget);
    } else if (tk_str_eq(name, "clone_tab")) {
      widget_t* win = widget_get_window(widget);
      widget_on(widget, EVT_CLICK, on_clone_tab, win);
    } else if (strstr(name, "tab_visible:") != NULL) {
      widget_t* target = find_tab_visible_target(widget, name + strlen("tab_visible:"));
      widget_set_value_int(widget, widget_get_visible(target) ? 1 : 0);
      widget_on(widget, EVT_VALUE_CHANGED, on_tab_visible_changed, (void*)target);
    } else if (tk_str_eq(name, "remove_tab")) {
      if (widget->parent != NULL &&
          tk_str_eq(WIDGET_TYPE_TAB_BUTTON, widget_get_type(widget->parent))) {
        widget_on(widget, EVT_POINTER_UP, on_remove_tab, widget);
      } else {
        widget_on(widget, EVT_CLICK, on_remove_tab, widget);
      }
    } else if (tk_str_eq(name, "remove_self")) {
      widget_on(widget, EVT_CLICK, on_remove_self, widget);
    } else if (tk_str_eq(name, "remove_view")) {
      widget_on(widget, EVT_CLICK, on_remove_view, widget);
    } else if (tk_str_eq(name, "chinese")) {
      widget_on(widget, EVT_CLICK, on_change_locale, (void*)"zh_CN");
    } else if (tk_str_eq(name, "english")) {
      widget_on(widget, EVT_CLICK, on_change_locale, (void*)"en_US");
    } else if (tk_str_eq(name, "font_small") || tk_str_eq(name, "font_normal") ||
               tk_str_eq(name, "font_big")) {
      widget_t* win = widget_get_window(widget);
      widget_on(widget, EVT_VALUE_CHANGED, on_change_font_size, win);
    } else if (tk_str_eq(name, "inc_value")) {
      widget_t* win = widget_get_window(widget);
      widget_on(widget, EVT_CLICK, on_inc, win);
    } else if (strstr(name, "dec_value") != NULL) {
      widget_t* win = widget_get_window(widget);
      widget_on(widget, EVT_CLICK, on_dec, win);
    } else if (tk_str_eq(name, "close")) {
      widget_on(widget, EVT_CLICK, on_close, win);
    } else if (tk_str_eq(name, "fullscreen")) {
      widget_on(widget, EVT_CLICK, on_fullscreen, widget);
    } else if (tk_str_eq(name, "unload_image")) {
      widget_on(widget, EVT_CLICK, on_unload_image, widget);
    } else if (tk_str_eq(name, "start")) {
      widget_on(widget, EVT_CLICK, on_start, win);
    } else if (tk_str_eq(name, "pause")) {
      widget_on(widget, EVT_CLICK, on_pause, win);
    } else if (tk_str_eq(name, "stop")) {
      widget_on(widget, EVT_CLICK, on_stop, win);
    } else if (tk_str_eq(name, "key")) {
      widget_on(widget, EVT_CLICK, on_send_key, NULL);
    } else if (tk_str_eq(name, "backspace")) {
      widget_on(widget, EVT_CLICK, on_backspace, NULL);
    } else if (tk_str_eq(name, "quit")) {
      widget_t* win = widget_get_window(widget);
      if (win) {
        widget_on(widget, EVT_CLICK, on_quit, win);
      }
    } else if (tk_str_eq(name, "back_to_home")) {
      widget_t* win = widget_get_window(widget);
      if (win) {
        widget_on(widget, EVT_CLICK, on_back_to_home, win);
      }
    } else if (tk_str_eq(name, "exit")) {
      widget_t* win = widget_get_window(widget);
      if (win) {
        widget_on(widget, EVT_CLICK, on_quit_app, win);
      }
    } else if (tk_str_eq(name, "pages")) {
      widget_on(widget, EVT_WIDGET_ADD_CHILD, on_pages_add_child, widget);
    } else if (strstr(name, "bind_value:") != NULL) {
      widget_t* target = find_bind_value_target(widget, name);
      widget_on(target, EVT_VALUE_CHANGED, on_bind_value_changed, (void*)widget);
    } else if (strstr(name, "action_list:") != NULL) {
      widget_on(widget, EVT_CLICK, on_action_list, (void*)widget);
    } else if (strstr(name, "cursor") != NULL) {
      widget_on(widget, EVT_CLICK, on_change_cursor, win);
    } else if (tk_str_eq(name, "ani_interval") && tk_str_eq(widget->vt->type, "image_animation")) {
      widget_on(widget, EVT_POINTER_DOWN, on_image_animation_set_interval, widget);
    } else if (tk_str_eq(name, SCROLL_GRID_SCROLL_WIDGT_NAME)) {
      widget_add_idle(widget, on_idle_scroll_view_set_virtual_wh);
    } else if (tk_str_eq(name, SCROLL_BAR_H_WIDGT_NAME)) {
      widget_on(widget, EVT_VALUE_CHANGED, scroll_bar_on_value_changed, widget);
    } else if (tk_str_eq(name, SCROLL_BAR_V_WIDGT_NAME)) {
      widget_on(widget, EVT_VALUE_CHANGED, scroll_bar_on_value_changed, widget);
    } else if (strstr(name, "next_page:") == name) {
      widget_on(widget, EVT_CLICK, on_click_next_page, (void*)(name + strlen("next_page:")));
    } else if (strstr(name, "prev_page:") == name) {
      widget_on(widget, EVT_CLICK, on_click_prev_page, (void*)(name + strlen("last_page:")));
    } else if (strstr(name, "clone_combo_box_ex") == name) {
      widget_on(widget, EVT_CLICK, on_click_clone_combo_box_ex, win);
    } else if (strstr(name, "combo_box_ex_for_clone") == name) {
      combo_box_set_on_item_click(widget, on_combo_box_ex_item_click, win);
    } else if (tk_str_eq(name, "remove_appoint_index")) {
      widget_on(widget, EVT_CLICK, on_click_slide_view_appoint_remove_evt, widget);
    } else if (strstr(name, "scroll:") == name) {
      widget_on(widget, EVT_CLICK, on_click_scroll, (void*)(name + strlen("scroll:")));
    }
  } else if (tk_str_eq(widget->vt->type, "combo_box")) {
    widget_on(widget, EVT_VALUE_CHANGED, on_combo_box_changed, widget);
    widget_on(widget, EVT_VALUE_WILL_CHANGE, on_combo_box_will_change, widget);
    if (tk_str_eq("fruit", (COMBO_BOX(widget))->open_window)) {
      combo_box_set_on_item_click(widget, on_combo_box_item_click, widget);
    }
  }
  (void)ctx;

  return RET_OK;
}

static void install_click_hander(widget_t* widget) {
  widget_foreach(widget, install_one, widget);
}

#include "base/idle.h"
#include "base/assets_manager.h"

static uint32_t s_preload_nr = 0;
static const preload_res_t s_preload_res[] = {{ASSET_TYPE_IMAGE, "earth"},
                                              {ASSET_TYPE_IMAGE, "dialog_title"},
                                              {ASSET_TYPE_IMAGE, "rgb"},
                                              {ASSET_TYPE_IMAGE, "rgba"}};

static ret_t timer_preload(const timer_info_t* timer) {
  char text[64];
  widget_t* win = WIDGET(timer->ctx);
  uint32_t total = ARRAY_SIZE(s_preload_res);
  widget_t* bar = widget_lookup(win, "bar", TRUE);
  widget_t* status = widget_lookup(win, "status", TRUE);

  if (s_preload_nr == total) {
#if !defined(MOBILE_APP)
    window_open("system_bar");
/*    window_open("system_bar_bottom");*/
#endif /*MOBILE_APP*/

    open_window("top", NULL);
    open_window(DEMOUI_MAIN_WINDOW_NAME, win);

    return RET_REMOVE;
  } else {
    uint32_t value = 0;
    const preload_res_t* iter = s_preload_res + s_preload_nr++;
    switch (iter->type) {
      case ASSET_TYPE_IMAGE: {
        bitmap_t img;
        image_manager_get_bitmap(image_manager(), iter->name, &img);
        break;
      }
      default: {
        assets_manager_ref(assets_manager(), iter->type, iter->name);
        break;
      }
    }

    value = (s_preload_nr * 100) / total;
    tk_snprintf(text, sizeof(text), "Load: %s(%u/%u)", iter->name, s_preload_nr, total);
    widget_set_value(bar, value);
    widget_set_text_utf8(status, text);

    return RET_REPEAT;
  }
}

static ret_t show_preload_res_window() {
  uint32_t interval = 500 / ARRAY_SIZE(s_preload_res);
  widget_t* win = window_open("preload");
  window_manager_set_show_fps(window_manager(), TRUE);

  timer_add(timer_preload, win, interval);

  return RET_OK;
}

static ret_t close_window_on_event(void* ctx, event_t* e) {
  window_close(WIDGET(ctx));

  return RET_REMOVE;
}

static ret_t on_screen_saver(void* ctx, event_t* e) {
  widget_t* win = NULL;
  const char* screen_saver_win = "image_animation";

  if (widget_child(window_manager(), screen_saver_win) != NULL) {
    log_debug("screen saver exist.\n");
    return RET_OK;
  }

  win = window_open(screen_saver_win);
  widget_on(win, EVT_POINTER_MOVE, close_window_on_event, win);
  widget_on(win, EVT_POINTER_UP, close_window_on_event, win);
  widget_on(win, EVT_KEY_UP, close_window_on_event, win);

  return RET_OK;
}

static ret_t on_key_record_play_events(void* ctx, event_t* e) {
  key_event_t* evt = (key_event_t*)e;
#ifdef WITH_EVENT_RECORDER_PLAYER
  if (evt->key == TK_KEY_F5) {
    event_recorder_player_start_record("event_log.bin");
    return RET_STOP;
  } else if (evt->key == TK_KEY_F6) {
    event_recorder_player_stop_record();
    return RET_STOP;
  } else if (evt->key == TK_KEY_F7) {
    event_recorder_player_start_play("event_log.bin", 0xffff);
    return RET_STOP;
  } else if (evt->key == TK_KEY_F8) {
    event_recorder_player_stop_play();
    return RET_STOP;
  } else if (evt->key == TK_KEY_F9) {
    tk_mem_dump();
    return RET_STOP;
  } else if (evt->key == TK_KEY_F10) {
    font_manager_unload_all(font_manager());
    image_manager_unload_all(image_manager());
    assets_manager_clear_cache(assets_manager(), ASSET_TYPE_UI);
    tk_mem_dump();
    return RET_STOP;
  }
#endif /*WITH_EVENT_RECORDER_PLAYER*/
  if (evt->key == TK_KEY_WHEEL) {
    uint32_t o = system_info()->lcd_orientation + 90;
    if (o > 270) {
      o = 0;
    }
#if defined(WITH_FAST_LCD_PORTRAIT)
    tk_enable_fast_lcd_portrait(TRUE);
#endif
    tk_set_lcd_orientation((lcd_orientation_t)o);
  }
  return RET_OK;
}

static ret_t on_key_back_or_back_to_home(void* ctx, event_t* e) {
  key_event_t* evt = (key_event_t*)e;
  if (evt->key == TK_KEY_F2) {
    window_manager_back(WIDGET(ctx));

    return RET_STOP;
  } else if (evt->key == TK_KEY_F3) {
    window_manager_back_to_home(WIDGET(ctx));

    return RET_STOP;
  } else if (evt->key == TK_KEY_F4) {
    window_manager_back_to(WIDGET(ctx), DEMOUI_MAIN_WINDOW_NAME);

    return RET_STOP;
  } else if (evt->key == TK_KEY_WHEEL) {
    log_debug("TK_KEY_WHEEL_DOWN\r\n");
  }

  return RET_OK;
}

static ret_t wm_on_before_paint(void* ctx, event_t* e) {
  return RET_OK;
}

static ret_t wm_on_after_paint(void* ctx, event_t* e) {
  return RET_OK;
}

static ret_t wm_on_low_memory(void* ctx, event_t* evt) {
  log_debug("low memory\n");
  return RET_OK;
}

static ret_t wm_on_out_of_memory(void* ctx, event_t* evt) {
  log_debug("out of memory\n");
  return RET_OK;
}

static ret_t wm_on_request_quit(void* ctx, event_t* evt) {
  /*
   * do some cleanup work here
   * return RET_STOP to ignore the request
   */
  /*return RET_STOP;*/

  return RET_OK;
}

static ret_t wm_on_ime_start(void* ctx, event_t* evt) {
  log_debug("wm_on_ime_start\n");
  return RET_OK;
}

static ret_t wm_on_ime_stop(void* ctx, event_t* evt) {
  log_debug("wm_on_ime_stop\n");
  return RET_OK;
}

ret_t application_init() {
  char path[MAX_PATH + 1];
  widget_t* wm = window_manager();

  image_manager_set_max_mem_size_of_cached_images(image_manager(), 8 * 1024 * 1024);

  /*enable screen saver*/
  window_manager_set_screen_saver_time(wm, 180 * 1000);
  widget_on(wm, EVT_SCREEN_SAVER, on_screen_saver, NULL);

  widget_on(wm, EVT_KEY_DOWN, on_key_back_or_back_to_home, wm);
  widget_on(wm, EVT_KEY_UP, on_key_record_play_events, wm);
  widget_on(wm, EVT_BEFORE_PAINT, wm_on_before_paint, wm);
  widget_on(wm, EVT_AFTER_PAINT, wm_on_after_paint, wm);
  widget_on(wm, EVT_LOW_MEMORY, wm_on_low_memory, wm);
  widget_on(wm, EVT_OUT_OF_MEMORY, wm_on_out_of_memory, wm);
  widget_on(wm, EVT_REQUEST_QUIT_APP, wm_on_request_quit, wm);
  widget_on(wm, EVT_IM_START, wm_on_ime_start, wm);
  widget_on(wm, EVT_IM_STOP, wm_on_ime_stop, wm);

  fs_get_user_storage_path(os_fs(), path);
  log_debug("user storage path:%s\n", path);

  return show_preload_res_window();
}

ret_t application_exit() {
  log_debug("application_exit\n");
  return RET_OK;
}

#ifdef WITH_FS_RES
#define APP_DEFAULT_FONT "default_full"
#endif /*WITH_FS_RES*/

#include "awtk_main.inc"
