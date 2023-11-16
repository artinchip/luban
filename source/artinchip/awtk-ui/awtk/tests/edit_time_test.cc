﻿#include "tkc/utils.h"
#include "widgets/edit_time.h"
#include "gtest/gtest.h"

TEST(Edit, time_is_valid) {
  widget_t* w = edit_create(NULL, 10, 20, 30, 40);

  widget_set_text_utf8(w, "18:12");
  ASSERT_EQ(edit_time_is_valid(w), TRUE);

  widget_set_text_utf8(w, "0:0:0");
  ASSERT_EQ(edit_time_is_valid(w), FALSE);

  widget_set_text_utf8(w, "0:0");
  ASSERT_EQ(edit_time_is_valid(w), FALSE);

  widget_set_text_utf8(w, "0");
  ASSERT_EQ(edit_time_is_valid(w), FALSE);

  widget_set_text_utf8(w, "18:60");
  ASSERT_EQ(edit_time_is_valid(w), FALSE);

  widget_set_text_utf8(w, "018:12:40");
  ASSERT_EQ(edit_time_is_valid(w), FALSE);

  widget_set_text_utf8(w, "18:12:");
  ASSERT_EQ(edit_time_is_valid(w), FALSE);

  widget_set_text_utf8(w, "24:12");
  ASSERT_EQ(edit_time_is_valid(w), FALSE);

  widget_set_text_utf8(w, "25:00");
  ASSERT_EQ(edit_time_is_valid(w), FALSE);

  widget_destroy(w);
}

TEST(Edit, time_fix) {
  widget_t* w = edit_create(NULL, 10, 20, 30, 40);

  widget_set_text_utf8(w, "180:12");
  ASSERT_EQ(edit_time_fix_ex(w, TRUE), RET_OK);
  ASSERT_EQ(wcscmp(w->text.str, L"18:12"), 0);

  widget_set_text_utf8(w, "180:60");
  ASSERT_EQ(edit_time_fix_ex(w, TRUE), RET_OK);
  ASSERT_EQ(wcscmp(w->text.str, L"18:00"), 0);

  widget_set_text_utf8(w, "180:60");
  ASSERT_EQ(edit_time_fix(w), RET_OK);
  ASSERT_EQ(wcscmp(w->text.str, L"18:00"), 0);

  widget_destroy(w);
}

TEST(Edit, time_inc_dec) {
  widget_t* w = edit_create(NULL, 10, 20, 30, 40);

  widget_set_text_utf8(w, "18:12");
  ASSERT_EQ(edit_time_inc_value(w), RET_OK);
  ASSERT_EQ(wcscmp(w->text.str, L"18:13"), 0);
  ASSERT_EQ(edit_time_dec_value(w), RET_OK);
  ASSERT_EQ(wcscmp(w->text.str, L"18:12"), 0);

  widget_destroy(w);
}

TEST(Edit, time_is_valid_char) {
  widget_t* w = edit_create(NULL, 10, 20, 30, 40);

  widget_set_text_utf8(w, "18:1");
  ASSERT_EQ(edit_time_pre_input(w, '0'), RET_OK);
  ASSERT_EQ(edit_time_is_valid_char(w, '0'), TRUE);

  widget_set_text_utf8(w, "18:11");
  ASSERT_EQ(edit_time_pre_input(w, '0'), RET_OK);
  ASSERT_EQ(edit_time_is_valid_char(w, '0'), FALSE);

  widget_destroy(w);
}
