﻿#include "widgets/spin_box.h"
#include "base/canvas.h"
#include "base/widget.h"
#include "font_dummy.h"
#include "lcd_log.h"
#include "gtest/gtest.h"
#include <stdlib.h>
#include <string>
#include "ui_loader/ui_serializer.h"

using std::string;

TEST(SpinBox, clone) {
  value_t v1;
  widget_t* w1 = spin_box_create(NULL, 10, 20, 30, 40);

  value_set_int(&v1, 200);
  ASSERT_EQ(widget_set_prop(w1, WIDGET_PROP_MAX, &v1), RET_OK);

  value_set_int(&v1, 20);
  ASSERT_EQ(widget_set_prop(w1, WIDGET_PROP_MIN, &v1), RET_OK);

  widget_t* w2 = widget_clone(w1, NULL);
  ASSERT_EQ(widget_equal(w1, w2), TRUE);

  widget_destroy(w1);
  widget_destroy(w2);
}

TEST(SpinBox, to_xml) {
  str_t str;
  widget_t* w1 = spin_box_create(NULL, 10, 20, 30, 40);

  str_init(&str, 1024);
  ASSERT_EQ(widget_to_xml(w1, &str), RET_OK);
  ASSERT_STREQ(str.str,
               "<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>\r\n<spin_box x=\"10\" "
               "y=\"20\" w=\"30\" h=\"40\" focusable=\"true\" min=\"0\" max=\"1024\" "
               "step=\"1.000000\" input_type=\"1\" readonly=\"false\" cancelable=\"false\" "
               "auto_fix=\"false\" left_margin=\"2\" right_margin=\"2\" top_margin=\"2\" "
               "bottom_margin=\"2\" action_text=\"done\" password_visible=\"false\" repeat=\"300\" "
               "easy_touch_mode=\"false\" button_position=\"none\">\n</spin_box>\n");

  str_reset(&str);
  widget_destroy(w1);
}

TEST(SpinBox, cast) {
  widget_t* w = spin_box_create(NULL, 10, 20, 30, 40);

  ASSERT_EQ(w, spin_box_cast(w));
  ASSERT_EQ(w, edit_cast(w));

  widget_destroy(w);
}

TEST(SpinBox, set_value) {
  widget_t* w = spin_box_create(NULL, 10, 20, 30, 40);

  edit_set_int_limit(w, -100, 100, 1);

  ASSERT_EQ(widget_set_value(w, -50), RET_OK);
  ASSERT_EQ(widget_get_value(w), -50);

  widget_destroy(w);
}

TEST(SpinBox, easy_touch_mode) {
  widget_t* w = spin_box_create(NULL, 0, 0, 100, 40);
  widget_t* inc = widget_lookup(w, STR_EDIT_INC_NAME, TRUE);
  widget_t* dec = widget_lookup(w, STR_EDIT_DEC_NAME, TRUE);
  widget_layout(w);

  ASSERT_EQ(inc->w, inc->h);
  ASSERT_EQ(dec->w, dec->h);
  ASSERT_EQ(inc->w, 20);
  ASSERT_EQ(dec->h, 20);
  ASSERT_EQ(inc->x, w->w - inc->w);
  ASSERT_EQ(inc->y, 0);
  ASSERT_EQ(dec->x, w->w - inc->w);
  ASSERT_EQ(dec->y, inc->h);
  ASSERT_EQ(dec->w, dec->h);

  spin_box_set_easy_touch_mode(w, TRUE);
  widget_layout(w);

  ASSERT_EQ(inc->w, w->h);
  ASSERT_EQ(inc->h, w->h);
  ASSERT_EQ(inc->y, 0);
  ASSERT_EQ(inc->x, w->w - inc->w);
  ASSERT_EQ(dec->w, w->h);
  ASSERT_EQ(dec->h, w->h);
  ASSERT_EQ(dec->y, 0);
  ASSERT_EQ(dec->x, 0);

  widget_resize(w, 80, 90);
  widget_layout(w);

  ASSERT_EQ(inc->w, w->w);
  ASSERT_EQ(inc->h, w->h / 3);
  ASSERT_EQ(inc->x, 0);
  ASSERT_EQ(inc->y, 0);

  ASSERT_EQ(dec->w, w->w);
  ASSERT_EQ(dec->h, w->h / 3);
  ASSERT_EQ(dec->x, 0);
  ASSERT_EQ(dec->y, 2 * w->h / 3);

  widget_t* w1 = widget_clone(w, NULL);
  ASSERT_EQ(SPIN_BOX(w1)->easy_touch_mode, TRUE);

  widget_destroy(w);
  widget_destroy(w1);
}
