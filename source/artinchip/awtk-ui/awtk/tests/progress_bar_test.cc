﻿#include "widgets/progress_bar.h"
#include "gtest/gtest.h"
#include <string.h>

using std::string;

TEST(progress_bar, basic) {
  value_t v1;
  value_t v2;
  widget_t* s = progress_bar_create(NULL, 10, 20, 30, 40);

  value_set_int(&v1, 10);
  ASSERT_EQ(widget_set_prop(s, WIDGET_PROP_VALUE, &v1), RET_OK);
  ASSERT_EQ(widget_get_prop(s, WIDGET_PROP_VALUE, &v2), RET_OK);
  ASSERT_EQ(value_int(&v1), value_int(&v2));

  value_set_bool(&v1, TRUE);
  ASSERT_EQ(widget_set_prop(s, WIDGET_PROP_VERTICAL, &v1), RET_OK);
  ASSERT_EQ(widget_get_prop(s, WIDGET_PROP_VERTICAL, &v2), RET_OK);
  ASSERT_EQ(value_bool(&v1), value_bool(&v2));

  value_set_bool(&v1, TRUE);
  ASSERT_EQ(widget_set_prop(s, WIDGET_PROP_REVERSE, &v1), RET_OK);
  ASSERT_EQ(widget_get_prop(s, WIDGET_PROP_REVERSE, &v2), RET_OK);
  ASSERT_EQ(value_bool(&v1), value_bool(&v2));

  value_set_str(&v1, "%02.2f");
  ASSERT_EQ(widget_set_prop(s, WIDGET_PROP_FORMAT, &v1), RET_OK);
  ASSERT_EQ(widget_get_prop(s, WIDGET_PROP_FORMAT, &v2), RET_OK);
  ASSERT_EQ(string(value_str(&v1)), string(value_str(&v2)));

  widget_destroy(s);
}

TEST(progress_bar, max) {
  value_t v1;
  value_t v2;
  widget_t* s = progress_bar_create(NULL, 10, 20, 30, 40);

  ASSERT_EQ(widget_get_prop_int(s, WIDGET_PROP_MAX, 0), 100);

  value_set_int(&v1, 1000);
  ASSERT_EQ(widget_set_prop(s, WIDGET_PROP_MAX, &v1), RET_OK);
  ASSERT_EQ(widget_get_prop(s, WIDGET_PROP_MAX, &v2), RET_OK);
  ASSERT_EQ(value_int(&v1), value_int(&v2));

  ASSERT_EQ(progress_bar_set_value(s, 500), RET_OK);
  ASSERT_EQ(progress_bar_get_percent(s), 50u);

  widget_destroy(s);
}

#include "log_change_events.inc"

TEST(ProgressBar, event) {
  widget_t* w = progress_bar_create(NULL, 0, 0, 100, 100);

  progress_bar_set_value(w, 10);

  s_log = "";
  widget_on(w, EVT_VALUE_WILL_CHANGE, on_change_events, NULL);
  widget_on(w, EVT_VALUE_CHANGED, on_change_events, NULL);

  progress_bar_set_value(w, 10);
  ASSERT_EQ(s_log, "");

  progress_bar_set_value(w, 20);
  ASSERT_EQ(s_log, "will_change;change;");

  widget_destroy(w);
}

TEST(ProgressBar, cast) {
  widget_t* w = progress_bar_create(NULL, 0, 0, 100, 100);

  ASSERT_EQ(w, progress_bar_cast(w));

  widget_destroy(w);
}

TEST(ProgressBar, change_value) {
  widget_t* w = progress_bar_create(NULL, 10, 20, 30, 40);
  value_change_event_t evt;
  memset(&evt, 0x00, sizeof(evt));

  widget_on(w, EVT_VALUE_WILL_CHANGE, on_value_will_changed_accept, NULL);
  widget_on(w, EVT_VALUE_CHANGED, on_value_changed, &evt);
  ASSERT_EQ(widget_set_prop_int(w, WIDGET_PROP_VALUE, 3), RET_OK);
  ASSERT_EQ(widget_get_prop_int(w, WIDGET_PROP_VALUE, 0), 3);

  ASSERT_EQ(value_int(&(evt.old_value)), 0);
  ASSERT_EQ(value_int(&(evt.new_value)), 3);

  widget_destroy(w);
}

TEST(ProgressBar, change_value_abort) {
  widget_t* w = progress_bar_create(NULL, 10, 20, 30, 40);
  value_change_event_t evt;
  memset(&evt, 0x00, sizeof(evt));

  widget_on(w, EVT_VALUE_WILL_CHANGE, on_value_will_changed_abort, NULL);
  ASSERT_EQ(widget_set_prop_int(w, WIDGET_PROP_VALUE, 3), RET_OK);
  ASSERT_EQ(widget_get_prop_int(w, WIDGET_PROP_VALUE, 3), 0);

  widget_destroy(w);
}
