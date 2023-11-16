﻿#include "tkc/str.h"
#include "gtest/gtest.h"
#include "mledit/mledit.h"
#include "base/text_edit.h"
#include "base/clip_board.h"
#include "base/window_manager.h"
#include "base/window.h"
#include "lcd/lcd_mem_rgba8888.h"

TEST(TextEdit, basic) {
  str_t str;
  key_event_t keye;
  text_edit_state_t state;
  widget_t* w = mledit_create(NULL, 10, 20, 30, 40);
  text_edit_t* text_edit = text_edit_create(w, FALSE);

  str_init(&str, 0);

  widget_set_text(w, L"hello");
  ASSERT_EQ(text_edit_set_select(text_edit, 1, 2), RET_OK);
  ASSERT_EQ(text_edit_set_cursor(text_edit, -1), RET_OK);

  ASSERT_EQ(text_edit_get_state(text_edit, &state), RET_OK);
  ASSERT_EQ(state.select_start, 1u);
  ASSERT_EQ(state.select_end, 2u);
  ASSERT_EQ(state.cursor, 5u);
  ASSERT_EQ(state.mask, FALSE);
  ASSERT_EQ(state.mask_char, FALSE);
  ASSERT_EQ(state.single_line, FALSE);

  ASSERT_EQ(text_edit_paste(text_edit, L"123", 3), RET_OK);
  str_from_wstr(&str, w->text.str);
  ASSERT_STREQ(str.str, "h123llo");

  ASSERT_EQ(text_edit_set_select(text_edit, 0, 100), RET_OK);
  ASSERT_EQ(text_edit_copy(text_edit), RET_OK);

  clip_board_clear(clip_board());
  ASSERT_EQ(text_edit_cut(text_edit), RET_OK);
  ASSERT_EQ(text_edit_get_state(text_edit, &state), RET_OK);
  ASSERT_EQ(state.select_start, 0u);
  ASSERT_EQ(state.select_end, 0u);
  ASSERT_EQ(state.cursor, 0u);

  key_event_init(&keye, EVT_KEY_DOWN, w, TK_KEY_0);
  ASSERT_EQ(text_edit_key_down(text_edit, &keye), RET_OK);
  ASSERT_EQ(text_edit_key_down(text_edit, &keye), RET_OK);

  str_from_wstr(&str, w->text.str);
  ASSERT_STREQ(str.str, "00");

  str_reset(&str);
  widget_destroy(w);
  text_edit_destroy(text_edit);
}

TEST(TextEdit, cursor) {
  const char* str = "it is ok";
  widget_t* w = mledit_create(NULL, 10, 20, 30, 40);
  text_edit_t* text_edit = text_edit_create(w, FALSE);
  widget_set_text_utf8(w, str);

  ASSERT_EQ(text_edit_set_cursor(text_edit, 0), RET_OK);
  ASSERT_EQ(text_edit_get_cursor(text_edit), 0u);

  ASSERT_EQ(text_edit_set_cursor(text_edit, 5), RET_OK);
  ASSERT_EQ(text_edit_get_cursor(text_edit), 5u);

  ASSERT_EQ(text_edit_set_cursor(text_edit, 500), RET_OK);
  ASSERT_EQ(text_edit_get_cursor(text_edit), (uint32_t)strlen(str));

  widget_destroy(w);
  text_edit_destroy(text_edit);
}

TEST(TextEdit, select) {
  char* select = NULL;
  const char* str = "it is ok";
  widget_t* w = mledit_create(NULL, 10, 20, 30, 40);
  text_edit_t* text_edit = text_edit_create(w, FALSE);
  widget_set_text_utf8(w, str);

  ASSERT_EQ(text_edit_set_select(text_edit, 0, 0), RET_OK);
  select = text_edit_get_selected_text(text_edit);
  ASSERT_STREQ(select, NULL);

  ASSERT_EQ(text_edit_set_select(text_edit, 0, 3), RET_OK);
  select = text_edit_get_selected_text(text_edit);
  ASSERT_STREQ(select, "it ");
  TKMEM_FREE(select);

  ASSERT_EQ(text_edit_set_select(text_edit, 2, 5), RET_OK);
  select = text_edit_get_selected_text(text_edit);
  ASSERT_STREQ(select, " is");
  TKMEM_FREE(select);

  ASSERT_EQ(text_edit_set_select(text_edit, 2, 50), RET_OK);
  select = text_edit_get_selected_text(text_edit);
  ASSERT_STREQ(select, " is ok");
  TKMEM_FREE(select);

  widget_destroy(w);
  text_edit_destroy(text_edit);
}

TEST(TextEdit, get_height) {
  canvas_t c;
  lcd_t* lcd = lcd_mem_rgba8888_create(100, 100, TRUE);
  canvas_init(&c, lcd, font_manager());
  const char* str = "it\nis\nok\nsecond line";
  widget_t* win = window_create(NULL, 0, 0, 0, 0);
  widget_t* w = mledit_create(win, 10, 20, 30, 40);
  text_edit_t* text_edit = text_edit_create(w, FALSE);

  widget_set_text_utf8(w, str);
  widget_set_prop_pointer(win, WIDGET_PROP_CANVAS, &c);
  text_edit_set_canvas(text_edit, widget_get_canvas(w));
  widget_set_prop_pointer(win, WIDGET_PROP_CANVAS, NULL);

  ASSERT_EQ(text_edit_get_height(text_edit, 0), 0u);
  ASSERT_EQ(text_edit_get_height(text_edit, 4), 22u);

  widget_destroy(win);
  text_edit_destroy(text_edit);
  canvas_reset(&c);
  lcd_destroy(lcd);
}

TEST(TextEdit, get_rows_line) {
  canvas_t c;
  lcd_t* lcd = lcd_mem_rgba8888_create(150, 150, TRUE);
  canvas_init(&c, lcd, font_manager());
  const char* str = "1\n22222222222222222222222";
  widget_t* win = window_create(NULL, 0, 0, 0, 0);
  widget_t* w = mledit_create(win, 10, 20, 100, 100);
  text_edit_t* text_edit = text_edit_create(w, FALSE);

  widget_set_text_utf8(w, str);
  widget_set_prop_pointer(win, WIDGET_PROP_CANVAS, &c);
  text_edit_set_canvas(text_edit, widget_get_canvas(w));
  widget_set_prop_pointer(win, WIDGET_PROP_CANVAS, NULL);

  const uint32_t* rows_line = text_edit_get_lines_of_each_row(text_edit);

  ASSERT_EQ(rows_line[0], 1u);
  ASSERT_EQ(rows_line[1], 2u);

  widget_destroy(win);
  text_edit_destroy(text_edit);
  canvas_reset(&c);
  lcd_destroy(lcd);
}

TEST(TextEdit, insert_text) {
  char get_text[16] = {0};
  const char* str = "it ok";
  const char* str2 = "it\nis";
  canvas_t c;
  lcd_t* lcd = lcd_mem_rgba8888_create(150, 150, TRUE);
  widget_t* win = window_create(NULL, 0, 0, 0, 0);
  widget_t* w = mledit_create(win, 10, 20, 30, 40);
  text_edit_t* text_edit = ((mledit_t*)w)->model;

  canvas_init(&c, lcd, font_manager());
  widget_set_prop_pointer(win, WIDGET_PROP_CANVAS, &c);
  mledit_set_max_lines(w, 100);
  mledit_set_max_chars(w, 0);

  widget_set_text_utf8(w, str);
  text_edit_insert_text(text_edit, 2, " is");
  widget_get_text_utf8(text_edit->widget, get_text, sizeof(get_text));
  ASSERT_STREQ("it is ok", get_text);
  memset(get_text, 0, sizeof(get_text));

  widget_set_text_utf8(w, str);
  text_edit_insert_text(text_edit, -1, " is");
  widget_get_text_utf8(text_edit->widget, get_text, sizeof(get_text));
  ASSERT_STREQ("it ok is", get_text);
  memset(get_text, 0, sizeof(get_text));

  widget_set_text_utf8(w, str);
  text_edit_insert_text(text_edit, 0, "");
  widget_get_text_utf8(text_edit->widget, get_text, sizeof(get_text));
  ASSERT_STREQ("it ok", get_text);
  memset(get_text, 0, sizeof(get_text));

  widget_set_text_utf8(w, str);
  text_edit_insert_text(text_edit, 0, "\0");
  widget_get_text_utf8(text_edit->widget, get_text, sizeof(get_text));
  ASSERT_STREQ("it ok", get_text);
  memset(get_text, 0, sizeof(get_text));

  mledit_set_max_lines(w, 3);
  widget_set_text_utf8(w, str2);
  text_edit_insert_text(text_edit, -1, "\nok\n!");
  widget_get_text_utf8(text_edit->widget, get_text, sizeof(get_text));
  ASSERT_STREQ("it\nis\nok", get_text);
  mledit_set_max_lines(w, 100);
  memset(get_text, 0, sizeof(get_text));

  mledit_set_max_chars(w, 8);
  widget_set_text_utf8(w, str);
  text_edit_insert_text(text_edit, -1, " is?");
  widget_get_text_utf8(text_edit->widget, get_text, sizeof(get_text));
  ASSERT_STREQ("it ok is", get_text);
  mledit_set_max_chars(w, 0);
  memset(get_text, 0, sizeof(get_text));

  widget_set_text_utf8(w, str);
  text_edit_set_cursor(text_edit, 1);
  text_edit_insert_text(text_edit, 2, " is");
  widget_get_text_utf8(text_edit->widget, get_text, sizeof(get_text));
  ASSERT_STREQ("it is ok", get_text);
  ASSERT_EQ(text_edit_get_cursor(text_edit), 5u);
  memset(get_text, 0, sizeof(get_text));

  widget_set_prop_pointer(win, WIDGET_PROP_CANVAS, NULL);
  widget_destroy(win);
  canvas_reset(&c);
  lcd_destroy(lcd);
}
