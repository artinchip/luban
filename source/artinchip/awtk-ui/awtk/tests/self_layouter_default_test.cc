﻿#include <string>
#include "gtest/gtest.h"
#include "widgets/button.h"
#include "widgets/overlay.h"
#include "base/window.h"
#include "base/layout.h"
#include "layouters/self_layouter_default.h"
#include "base/self_layouter_factory.h"

using std::string;

TEST(SelfLayoutDefault, basic) {
  rect_t r;
  widget_t* w = window_create(NULL, 0, 0, 0, 0);
  widget_t* b = button_create(w, 0, 0, 0, 0);
  const char* layout_params = "default(x=10,y=20,w=30,h=40)";
  self_layouter_t* layouter = self_layouter_create(layout_params);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x", 0), 10);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y", 0), 20);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w", 0), 30);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h", 0), 40);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x_attr", 0), X_ATTR_DEFAULT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y_attr", 0), Y_ATTR_DEFAULT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w_attr", 0), W_ATTR_PIXEL);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h_attr", 0), H_ATTR_PIXEL);

  r = rect_init(0, 0, 400, 300);
  ASSERT_EQ(self_layouter_layout(layouter, b, &r), RET_OK);

  ASSERT_EQ(b->x, 10);
  ASSERT_EQ(b->y, 20);
  ASSERT_EQ(b->w, 30);
  ASSERT_EQ(b->h, 40);
  ASSERT_EQ(string(self_layouter_to_string(layouter)), string(layout_params));

  self_layouter_destroy(layouter);
  widget_destroy(w);
}

TEST(SelfLayoutDefault, undef) {
  rect_t r;
  widget_t* w = window_create(NULL, 0, 0, 0, 0);
  widget_t* b = button_create(w, 0, 0, 0, 0);
  self_layouter_t* layouter = self_layouter_create("default()");

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x", 0), 0);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y", 0), 0);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w", 0), 0);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h", 0), 0);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x_attr", 0), X_ATTR_UNDEF);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y_attr", 0), Y_ATTR_UNDEF);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w_attr", 0), W_ATTR_UNDEF);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h_attr", 0), H_ATTR_UNDEF);

  r = rect_init(0, 0, 400, 300);
  widget_move_resize(b, 10, 20, 30, 40);
  ASSERT_EQ(self_layouter_layout(layouter, b, &r), RET_OK);

  ASSERT_EQ(b->x, 10);
  ASSERT_EQ(b->y, 20);
  ASSERT_EQ(b->w, 30);
  ASSERT_EQ(b->h, 40);
  ASSERT_EQ(string(self_layouter_to_string(layouter)), string("default()"));

  self_layouter_destroy(layouter);
  widget_destroy(w);
}

TEST(SelfLayoutDefault, minuswh) {
  rect_t r;
  widget_t* w = window_create(NULL, 0, 0, 0, 0);
  widget_t* b = button_create(w, 0, 0, 0, 0);
  const char* layout_params = "default(x=10,y=20,w=-30,h=-40)";
  self_layouter_t* layouter = self_layouter_create(layout_params);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x", 0), 10);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y", 0), 20);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w", 0), -30);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h", 0), -40);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x_attr", 0), X_ATTR_DEFAULT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y_attr", 0), Y_ATTR_DEFAULT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w_attr", 0), W_ATTR_PIXEL);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h_attr", 0), H_ATTR_PIXEL);

  r = rect_init(0, 0, 400, 300);
  ASSERT_EQ(self_layouter_layout(layouter, b, &r), RET_OK);
  ASSERT_EQ(string(self_layouter_to_string(layouter)), string(layout_params));

  ASSERT_EQ(b->x, 10);
  ASSERT_EQ(b->y, 20);
  ASSERT_EQ(b->w, 400 - 30);
  ASSERT_EQ(b->h, 300 - 40);

  self_layouter_destroy(layouter);
  widget_destroy(w);
}

TEST(SelfLayoutDefault, percent) {
  rect_t r;
  widget_t* w = window_create(NULL, 0, 0, 0, 0);
  widget_t* b = button_create(w, 0, 0, 0, 0);
  const char* layout_params = "default(x=10%,y=20%,w=30%,h=40%)";
  self_layouter_t* layouter = self_layouter_create(layout_params);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x", 0), 10);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y", 0), 20);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w", 0), 30);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h", 0), 40);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x_attr", 0), X_ATTR_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y_attr", 0), Y_ATTR_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w_attr", 0), W_ATTR_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h_attr", 0), H_ATTR_PERCENT);

  r = rect_init(0, 0, 400, 300);
  ASSERT_EQ(self_layouter_layout(layouter, b, &r), RET_OK);
  ASSERT_EQ(string(self_layouter_to_string(layouter)), string(layout_params));

  ASSERT_EQ(b->x, 40);
  ASSERT_EQ(b->y, 60);
  ASSERT_EQ(b->w, 120);
  ASSERT_EQ(b->h, 120);

  self_layouter_destroy(layouter);
  widget_destroy(w);
}

TEST(SelfLayoutDefault, center_middle) {
  rect_t r;
  widget_t* w = window_create(NULL, 0, 0, 0, 0);
  widget_t* b = button_create(w, 0, 0, 0, 0);
  self_layouter_t* layouter = self_layouter_create("default(x=c, y=m, w=30%, h=40%)");

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x", 0), 0);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y", 0), 0);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w", 0), 30);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h", 0), 40);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x_attr", 0), X_ATTR_CENTER);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y_attr", 0), Y_ATTR_MIDDLE);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w_attr", 0), W_ATTR_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h_attr", 0), H_ATTR_PERCENT);

  r = rect_init(0, 0, 400, 300);
  ASSERT_EQ(self_layouter_layout(layouter, b, &r), RET_OK);

  ASSERT_EQ(b->x, 140);
  ASSERT_EQ(b->y, 90);
  ASSERT_EQ(b->w, 120);
  ASSERT_EQ(b->h, 120);
  ASSERT_EQ(string(self_layouter_to_string(layouter)), string("default(x=c:0,y=m:0,w=30%,h=40%)"));

  self_layouter_destroy(layouter);
  widget_destroy(w);
}

TEST(SelfLayoutDefault, center_middle1020) {
  rect_t r;
  widget_t* w = window_create(NULL, 0, 0, 0, 0);
  widget_t* b = button_create(w, 0, 0, 0, 0);
  const char* layout_params = "default(x=c:10,y=m:20,w=30%,h=40%)";
  self_layouter_t* layouter = self_layouter_create(layout_params);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x", 0), 10);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y", 0), 20);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w", 0), 30);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h", 0), 40);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x_attr", 0), X_ATTR_CENTER);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y_attr", 0), Y_ATTR_MIDDLE);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w_attr", 0), W_ATTR_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h_attr", 0), H_ATTR_PERCENT);

  r = rect_init(0, 0, 400, 300);
  ASSERT_EQ(self_layouter_layout(layouter, b, &r), RET_OK);

  ASSERT_EQ(b->x, 140 + 10);
  ASSERT_EQ(b->y, 90 + 20);
  ASSERT_EQ(b->w, 120);
  ASSERT_EQ(b->h, 120);
  ASSERT_EQ(string(self_layouter_to_string(layouter)), string(layout_params));

  self_layouter_destroy(layouter);
  widget_destroy(w);
}

TEST(SelfLayoutDefault, center_middle1020minus) {
  rect_t r;
  widget_t* w = window_create(NULL, 0, 0, 0, 0);
  widget_t* b = button_create(w, 0, 0, 0, 0);
  const char* layout_params = "default(x=c:-10,y=m:-20,w=30%,h=40%)";
  self_layouter_t* layouter = self_layouter_create(layout_params);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x", 0), -10);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y", 0), -20);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w", 0), 30);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h", 0), 40);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x_attr", 0), X_ATTR_CENTER);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y_attr", 0), Y_ATTR_MIDDLE);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w_attr", 0), W_ATTR_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h_attr", 0), H_ATTR_PERCENT);

  r = rect_init(0, 0, 400, 300);
  ASSERT_EQ(self_layouter_layout(layouter, b, &r), RET_OK);

  ASSERT_EQ(b->x, 140 - 10);
  ASSERT_EQ(b->y, 90 - 20);
  ASSERT_EQ(b->w, 120);
  ASSERT_EQ(b->h, 120);
  ASSERT_EQ(string(self_layouter_to_string(layouter)), string(layout_params));

  self_layouter_destroy(layouter);
  widget_destroy(w);
}

TEST(SelfLayoutDefault, right_bottom) {
  rect_t r;
  widget_t* w = window_create(NULL, 0, 0, 0, 0);
  widget_t* b = button_create(w, 0, 0, 0, 0);
  const char* layout_params = "default(x=r,y=b,w=30%,h=40%)";
  self_layouter_t* layouter = self_layouter_create(layout_params);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x", 0), 0);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y", 0), 0);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w", 0), 30);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h", 0), 40);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x_attr", 0), X_ATTR_RIGHT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y_attr", 0), Y_ATTR_BOTTOM);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w_attr", 0), W_ATTR_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h_attr", 0), H_ATTR_PERCENT);

  r = rect_init(0, 0, 400, 300);
  ASSERT_EQ(self_layouter_layout(layouter, b, &r), RET_OK);

  ASSERT_EQ(b->x, 400 - 120);
  ASSERT_EQ(b->y, 300 - 120);
  ASSERT_EQ(b->w, 120);
  ASSERT_EQ(b->h, 120);
  ASSERT_EQ(string(self_layouter_to_string(layouter)), string("default(x=r:0,y=b:0,w=30%,h=40%)"));

  self_layouter_destroy(layouter);
  widget_destroy(w);
}

TEST(SelfLayoutDefault, right_bottom1020) {
  rect_t r;
  widget_t* w = window_create(NULL, 0, 0, 0, 0);
  widget_t* b = button_create(w, 0, 0, 0, 0);
  const char* layout_params = "default(x=r:10,y=b:20,w=30%,h=40%)";
  self_layouter_t* layouter = self_layouter_create(layout_params);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x", 0), 10);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y", 0), 20);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w", 0), 30);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h", 0), 40);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x_attr", 0), X_ATTR_RIGHT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y_attr", 0), Y_ATTR_BOTTOM);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w_attr", 0), W_ATTR_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h_attr", 0), H_ATTR_PERCENT);

  r = rect_init(0, 0, 400, 300);
  ASSERT_EQ(self_layouter_layout(layouter, b, &r), RET_OK);

  ASSERT_EQ(b->x, 400 - 120 - 10);
  ASSERT_EQ(b->y, 300 - 120 - 20);
  ASSERT_EQ(b->w, 120);
  ASSERT_EQ(b->h, 120);

  ASSERT_EQ(string(self_layouter_to_string(layouter)), string(layout_params));

  self_layouter_destroy(layouter);
  widget_destroy(w);
}

TEST(SelfLayoutDefault, right_bottom1020_percent) {
  rect_t r;
  widget_t* w = window_create(NULL, 0, 0, 0, 0);
  widget_t* b = button_create(w, 0, 0, 0, 0);
  const char* layout_params = "default(x=r:10%,y=b:20%,w=30%,h=40%)";
  self_layouter_t* layouter = self_layouter_create(layout_params);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x", 0), 10);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y", 0), 20);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w", 0), 30);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h", 0), 40);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x_attr", 0), X_ATTR_RIGHT_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y_attr", 0), Y_ATTR_BOTTOM_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w_attr", 0), W_ATTR_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h_attr", 0), H_ATTR_PERCENT);

  r = rect_init(0, 0, 400, 300);
  ASSERT_EQ(self_layouter_layout(layouter, b, &r), RET_OK);

  ASSERT_EQ(b->x, 400 - 120 - 400 * 10 / 100);
  ASSERT_EQ(b->y, 300 - 120 - 300 * 20 / 100);
  ASSERT_EQ(b->w, 120);
  ASSERT_EQ(b->h, 120);

  ASSERT_EQ(string(self_layouter_to_string(layouter)), string(layout_params));

  self_layouter_destroy(layouter);
  widget_destroy(w);
}

TEST(SelfLayoutDefault, center_middle1020_percent) {
  rect_t r;
  widget_t* w = window_create(NULL, 0, 0, 0, 0);
  widget_t* b = button_create(w, 0, 0, 0, 0);
  const char* layout_params = "default(x=c:10%,y=m:20%,w=30%,h=40%)";
  self_layouter_t* layouter = self_layouter_create(layout_params);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x", 0), 10);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y", 0), 20);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w", 0), 30);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h", 0), 40);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x_attr", 0), X_ATTR_CENTER_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y_attr", 0), Y_ATTR_MIDDLE_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w_attr", 0), W_ATTR_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h_attr", 0), H_ATTR_PERCENT);

  r = rect_init(0, 0, 400, 300);
  ASSERT_EQ(self_layouter_layout(layouter, b, &r), RET_OK);

  ASSERT_EQ(b->x, 140 + 400 * 10 / 100);
  ASSERT_EQ(b->y, 90 + 300 * 20 / 100);
  ASSERT_EQ(b->w, 120);
  ASSERT_EQ(b->h, 120);
  ASSERT_EQ(string(self_layouter_to_string(layouter)), string(layout_params));

  self_layouter_destroy(layouter);
  widget_destroy(w);
}

TEST(SelfLayoutDefault, center_middle1020minus_percent) {
  rect_t r;
  widget_t* w = window_create(NULL, 0, 0, 0, 0);
  widget_t* b = button_create(w, 0, 0, 0, 0);
  const char* layout_params = "default(x=c:-10%,y=m:-20%,w=30%,h=40%)";
  self_layouter_t* layouter = self_layouter_create(layout_params);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x", 0), -10);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y", 0), -20);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w", 0), 30);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h", 0), 40);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x_attr", 0), X_ATTR_CENTER_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y_attr", 0), Y_ATTR_MIDDLE_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w_attr", 0), W_ATTR_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h_attr", 0), H_ATTR_PERCENT);

  r = rect_init(0, 0, 400, 300);
  ASSERT_EQ(self_layouter_layout(layouter, b, &r), RET_OK);

  ASSERT_EQ(b->x, 140 - 400 * 10 / 100);
  ASSERT_EQ(b->y, 90 - 300 * 20 / 100);
  ASSERT_EQ(b->w, 120);
  ASSERT_EQ(b->h, 120);
  ASSERT_EQ(string(self_layouter_to_string(layouter)), string(layout_params));

  self_layouter_destroy(layouter);
  widget_destroy(w);
}

TEST(SelfLayoutDefault, double_percent1) {
  rect_t r;
  widget_t* w = window_create(NULL, 0, 0, 0, 0);
  widget_t* b = button_create(w, 0, 0, 0, 0);
  const char* layout_params = "default(x=10.10%,y=20.20%,w=30.30%,h=40.40%)";
  self_layouter_t* layouter = self_layouter_create(layout_params);

  ASSERT_EQ(self_layouter_get_param_float(layouter, "x", 0), 10.1f);
  ASSERT_EQ(self_layouter_get_param_float(layouter, "y", 0), 20.2f);
  ASSERT_EQ(self_layouter_get_param_float(layouter, "w", 0), 30.3f);
  ASSERT_EQ(self_layouter_get_param_float(layouter, "h", 0), 40.4f);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x_attr", 0), X_ATTR_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y_attr", 0), Y_ATTR_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w_attr", 0), W_ATTR_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h_attr", 0), H_ATTR_PERCENT);

  r = rect_init(0, 0, 1000, 1000);
  ASSERT_EQ(self_layouter_layout(layouter, b, &r), RET_OK);

  ASSERT_EQ(b->x, 101);
  ASSERT_EQ(b->y, 202);
  ASSERT_EQ(b->w, 303);
  ASSERT_EQ(b->h, 404);
  ASSERT_EQ(string(self_layouter_to_string(layouter)), string(layout_params));

  self_layouter_destroy(layouter);
  widget_destroy(w);
}

TEST(SelfLayoutDefault, double_percent2) {
  rect_t r;
  widget_t* w = window_create(NULL, 0, 0, 0, 0);
  widget_t* b = button_create(w, 0, 0, 0, 0);
  const char* layout_params = "default(x=r:10.10%,y=b:20.20%,w=30.30%,h=40.40%)";
  self_layouter_t* layouter = self_layouter_create(layout_params);

  ASSERT_EQ(self_layouter_get_param_float(layouter, "x", 0), 10.1f);
  ASSERT_EQ(self_layouter_get_param_float(layouter, "y", 0), 20.2f);
  ASSERT_EQ(self_layouter_get_param_float(layouter, "w", 0), 30.3f);
  ASSERT_EQ(self_layouter_get_param_float(layouter, "h", 0), 40.4f);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x_attr", 0), X_ATTR_RIGHT_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y_attr", 0), Y_ATTR_BOTTOM_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w_attr", 0), W_ATTR_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h_attr", 0), H_ATTR_PERCENT);

  r = rect_init(0, 0, 1000, 1000);
  ASSERT_EQ(self_layouter_layout(layouter, b, &r), RET_OK);

  ASSERT_EQ(b->w, 303);
  ASSERT_EQ(b->h, 404);
  ASSERT_EQ(b->x, 1000 - 101 - b->w);
  ASSERT_EQ(b->y, 1000 - 202 - b->h);
  ASSERT_EQ(string(self_layouter_to_string(layouter)), string(layout_params));

  self_layouter_destroy(layouter);
  widget_destroy(w);
}

TEST(SelfLayoutDefault, double_percent3) {
  rect_t r;
  widget_t* w = window_create(NULL, 0, 0, 0, 0);
  widget_t* b = button_create(w, 0, 0, 0, 0);
  const char* layout_params = "default(x=c:10.10%,y=m:20.20%,w=30.30%,h=40.40%)";
  self_layouter_t* layouter = self_layouter_create(layout_params);

  ASSERT_EQ(self_layouter_get_param_float(layouter, "x", 0), 10.1f);
  ASSERT_EQ(self_layouter_get_param_float(layouter, "y", 0), 20.2f);
  ASSERT_EQ(self_layouter_get_param_float(layouter, "w", 0), 30.3f);
  ASSERT_EQ(self_layouter_get_param_float(layouter, "h", 0), 40.4f);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x_attr", 0), X_ATTR_CENTER_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y_attr", 0), Y_ATTR_MIDDLE_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w_attr", 0), W_ATTR_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h_attr", 0), H_ATTR_PERCENT);

  r = rect_init(0, 0, 1000, 1000);
  ASSERT_EQ(self_layouter_layout(layouter, b, &r), RET_OK);

  ASSERT_EQ(b->w, 303);
  ASSERT_EQ(b->h, 404);
  ASSERT_EQ(b->x, 450);
  ASSERT_EQ(b->y, 500);
  ASSERT_EQ(string(self_layouter_to_string(layouter)), string(layout_params));

  self_layouter_destroy(layouter);
  widget_destroy(w);
}

TEST(SelfLayoutDefault, center_middle_and_widget_move) {
  self_layouter_t* layouter = NULL;
  widget_t* w = window_create(NULL, 0, 0, 0, 0);
  widget_t* b = button_create(w, 0, 0, 0, 0);
  widget_set_self_layout(b, "default(x=c, y=m, w=30%, h=40%)");
  layouter = b->self_layout;

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x", 0), 0);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y", 0), 0);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w", 0), 30);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h", 0), 40);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x_attr", 0), X_ATTR_CENTER);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y_attr", 0), Y_ATTR_MIDDLE);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w_attr", 0), W_ATTR_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h_attr", 0), H_ATTR_PERCENT);

  widget_resize(w, 400, 300);
  widget_move(b, 10, 20);

  ASSERT_EQ(widget_layout_self(b), RET_OK);

  ASSERT_EQ(b->x, 10);
  ASSERT_EQ(b->y, 20);
  ASSERT_EQ(b->w, 120);
  ASSERT_EQ(b->h, 120);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "x_attr", 0), X_ATTR_UNDEF);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y_attr", 0), X_ATTR_UNDEF);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w_attr", 0), W_ATTR_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h_attr", 0), H_ATTR_PERCENT);

  widget_destroy(w);
}

TEST(SelfLayoutDefault, center_middle_and_widget_resize) {
  self_layouter_t* layouter = NULL;
  widget_t* w = window_create(NULL, 0, 0, 0, 0);
  widget_t* b = button_create(w, 0, 0, 0, 0);
  widget_set_self_layout(b, "default(x=c, y=m, w=30%, h=40%)");
  layouter = b->self_layout;

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x", 0), 0);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y", 0), 0);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w", 0), 30);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h", 0), 40);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x_attr", 0), X_ATTR_CENTER);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y_attr", 0), Y_ATTR_MIDDLE);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w_attr", 0), W_ATTR_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h_attr", 0), H_ATTR_PERCENT);

  widget_resize(w, 400, 300);
  widget_resize(b, 200, 200);

  ASSERT_EQ(widget_layout_self(b), RET_OK);

  ASSERT_EQ(b->x, 100);
  ASSERT_EQ(b->y, 50);
  ASSERT_EQ(b->w, 200);
  ASSERT_EQ(b->h, 200);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "x_attr", 0), X_ATTR_CENTER);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y_attr", 0), X_ATTR_CENTER);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w_attr", 0), W_ATTR_UNDEF);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h_attr", 0), H_ATTR_UNDEF);

  widget_destroy(w);
}

TEST(SelfLayoutDefault, center_middle_and_widget_move_resize) {
  self_layouter_t* layouter = NULL;
  widget_t* w = window_create(NULL, 0, 0, 0, 0);
  widget_t* b = button_create(w, 0, 0, 0, 0);
  widget_set_self_layout(b, "default(x=c, y=m, w=30%, h=40%)");
  layouter = b->self_layout;

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x", 0), 0);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y", 0), 0);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w", 0), 30);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h", 0), 40);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x_attr", 0), X_ATTR_CENTER);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y_attr", 0), Y_ATTR_MIDDLE);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w_attr", 0), W_ATTR_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h_attr", 0), H_ATTR_PERCENT);

  widget_resize(w, 400, 300);
  widget_move_resize(b, 50, 10, 200, 200);

  ASSERT_EQ(widget_layout_self(b), RET_OK);

  ASSERT_EQ(b->x, 50);
  ASSERT_EQ(b->y, 10);
  ASSERT_EQ(b->w, 200);
  ASSERT_EQ(b->h, 200);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "x_attr", 0), X_ATTR_UNDEF);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y_attr", 0), Y_ATTR_UNDEF);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w_attr", 0), W_ATTR_UNDEF);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h_attr", 0), H_ATTR_UNDEF);

  widget_destroy(w);
}

TEST(SelfLayoutDefault, center_middle_and_widget_set_prop) {
  self_layouter_t* layouter = NULL;
  widget_t* w = window_create(NULL, 0, 0, 0, 0);
  widget_t* b = button_create(w, 0, 0, 0, 0);
  widget_set_self_layout(b, "default(x=c, y=m, w=30%, h=40%)");
  layouter = b->self_layout;

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x", 0), 0);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y", 0), 0);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w", 0), 30);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h", 0), 40);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x_attr", 0), X_ATTR_CENTER);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y_attr", 0), Y_ATTR_MIDDLE);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w_attr", 0), W_ATTR_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h_attr", 0), H_ATTR_PERCENT);

  widget_resize(w, 400, 300);
  widget_set_prop_int(b, "x", 10);

  ASSERT_EQ(widget_layout_self(b), RET_OK);

  ASSERT_EQ(b->x, 10);
  ASSERT_EQ(b->y, 90);
  ASSERT_EQ(b->w, 120);
  ASSERT_EQ(b->h, 120);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "x_attr", 0), X_ATTR_UNDEF);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y_attr", 0), Y_ATTR_MIDDLE);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w_attr", 0), W_ATTR_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h_attr", 0), H_ATTR_PERCENT);
  ASSERT_EQ(self_layouter_set_param_str(layouter, "x", "c"), RET_OK);

  widget_set_prop_int(b, "y", 10);
  ASSERT_EQ(widget_layout_self(b), RET_OK);

  ASSERT_EQ(b->x, 140);
  ASSERT_EQ(b->y, 10);
  ASSERT_EQ(b->w, 120);
  ASSERT_EQ(b->h, 120);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "x_attr", 0), X_ATTR_CENTER);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y_attr", 0), Y_ATTR_UNDEF);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w_attr", 0), W_ATTR_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h_attr", 0), H_ATTR_PERCENT);
  ASSERT_EQ(self_layouter_set_param_str(layouter, "y", "m"), RET_OK);

  widget_set_prop_int(b, "w", 10);
  ASSERT_EQ(widget_layout_self(b), RET_OK);

  ASSERT_EQ(b->x, 195);
  ASSERT_EQ(b->y, 90);
  ASSERT_EQ(b->w, 10);
  ASSERT_EQ(b->h, 120);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "x_attr", 0), X_ATTR_CENTER);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y_attr", 0), Y_ATTR_MIDDLE);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w_attr", 0), W_ATTR_UNDEF);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h_attr", 0), H_ATTR_PERCENT);
  ASSERT_EQ(self_layouter_set_param_str(layouter, "w", "30%"), RET_OK);

  widget_set_prop_int(b, "h", 10);
  ASSERT_EQ(widget_layout_self(b), RET_OK);

  ASSERT_EQ(b->x, 140);
  ASSERT_EQ(b->y, 145);
  ASSERT_EQ(b->w, 120);
  ASSERT_EQ(b->h, 10);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "x_attr", 0), X_ATTR_CENTER);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y_attr", 0), Y_ATTR_MIDDLE);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w_attr", 0), W_ATTR_PERCENT);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h_attr", 0), H_ATTR_UNDEF);

  widget_destroy(w);
}

TEST(SelfLayoutDefault, reinit) {
  rect_t r;
  widget_t* w = window_create(NULL, 0, 0, 0, 0);
  widget_t* b = button_create(w, 0, 0, 0, 0);
  ASSERT_EQ(widget_set_self_layout(b, "default(x=10,y=20,w=30,h=40)"), RET_OK);

  ASSERT_EQ(self_layouter_get_param_int(b->self_layout, "x_attr", 0), X_ATTR_DEFAULT);
  ASSERT_EQ(self_layouter_get_param_int(b->self_layout, "y_attr", 0), Y_ATTR_DEFAULT);
  ASSERT_EQ(self_layouter_get_param_int(b->self_layout, "w_attr", 0), W_ATTR_PIXEL);
  ASSERT_EQ(self_layouter_get_param_int(b->self_layout, "h_attr", 0), H_ATTR_PIXEL);

  r = rect_init(0, 0, 400, 300);
  ASSERT_EQ(self_layouter_layout(b->self_layout, b, &r), RET_OK);
  ASSERT_EQ(b->x, 10);
  ASSERT_EQ(b->y, 20);
  ASSERT_EQ(b->w, 30);
  ASSERT_EQ(b->h, 40);

  ASSERT_EQ(widget_move_resize(b, 0, 0, 0, 0), RET_OK);
  ASSERT_EQ(b->x, 0);
  ASSERT_EQ(b->y, 0);
  ASSERT_EQ(b->w, 0);
  ASSERT_EQ(b->h, 0);

  ASSERT_EQ(self_layouter_get_param_int(b->self_layout, "x_attr", 0), X_ATTR_UNDEF);
  ASSERT_EQ(self_layouter_get_param_int(b->self_layout, "y_attr", 0), Y_ATTR_UNDEF);
  ASSERT_EQ(self_layouter_get_param_int(b->self_layout, "w_attr", 0), W_ATTR_UNDEF);
  ASSERT_EQ(self_layouter_get_param_int(b->self_layout, "h_attr", 0), H_ATTR_UNDEF);

  ASSERT_EQ(self_layouter_reinit(b->self_layout), RET_OK);
  ASSERT_EQ(self_layouter_layout(b->self_layout, b, &r), RET_OK);
  ASSERT_EQ(b->x, 10);
  ASSERT_EQ(b->y, 20);
  ASSERT_EQ(b->w, 30);
  ASSERT_EQ(b->h, 40);

  widget_destroy(w);
}

TEST(SelfLayoutDefault, set_self_layout_params) {
  widget_t* win = window_create(NULL, 0, 0, 400, 300);
  widget_t* btn = button_create(win, 10, 20, 30, 40);

  widget_set_self_layout_params(btn, "c:100", NULL, NULL, NULL);
  ASSERT_EQ(string(btn->self_layout->params.str), "default(x=c:100)");

  self_layouter_destroy(btn->self_layout);
  btn->self_layout = NULL;
  widget_set_self_layout_params(btn, NULL, "b:10", NULL, NULL);
  ASSERT_EQ(string(btn->self_layout->params.str), "default(y=b:10)");

  self_layouter_destroy(btn->self_layout);
  btn->self_layout = NULL;
  widget_set_self_layout_params(btn, NULL, NULL, "100%", NULL);
  ASSERT_EQ(string(btn->self_layout->params.str), "default(w=100%)");

  self_layouter_destroy(btn->self_layout);
  btn->self_layout = NULL;
  widget_set_self_layout_params(btn, NULL, NULL, NULL, "10%");
  ASSERT_EQ(string(btn->self_layout->params.str), "default(h=10%)");

  self_layouter_destroy(btn->self_layout);
  btn->self_layout = NULL;
  widget_set_self_layout_params(btn, "c:100", "b:10", NULL, NULL);
  ASSERT_EQ(string(btn->self_layout->params.str), "default(x=c:100, y=b:10)");

  self_layouter_destroy(btn->self_layout);
  btn->self_layout = NULL;
  widget_set_self_layout_params(btn, "c:100", NULL, "100%", NULL);
  ASSERT_EQ(string(btn->self_layout->params.str), "default(x=c:100, w=100%)");

  self_layouter_destroy(btn->self_layout);
  btn->self_layout = NULL;
  widget_set_self_layout_params(btn, "c:100", NULL, NULL, "10%");
  ASSERT_EQ(string(btn->self_layout->params.str), "default(x=c:100, h=10%)");

  self_layouter_destroy(btn->self_layout);
  btn->self_layout = NULL;
  widget_set_self_layout_params(btn, NULL, "b:10", "100%", NULL);
  ASSERT_EQ(string(btn->self_layout->params.str), "default(y=b:10, w=100%)");

  self_layouter_destroy(btn->self_layout);
  btn->self_layout = NULL;
  widget_set_self_layout_params(btn, NULL, "b:10", NULL, "10%");
  ASSERT_EQ(string(btn->self_layout->params.str), "default(y=b:10, h=10%)");

  self_layouter_destroy(btn->self_layout);
  btn->self_layout = NULL;
  widget_set_self_layout_params(btn, NULL, NULL, "100%", "10%");
  ASSERT_EQ(string(btn->self_layout->params.str), "default(w=100%, h=10%)");

  self_layouter_destroy(btn->self_layout);
  btn->self_layout = NULL;
  widget_set_self_layout_params(btn, "c:100", "b:10", "100%", NULL);
  ASSERT_EQ(string(btn->self_layout->params.str), "default(x=c:100, y=b:10, w=100%)");

  self_layouter_destroy(btn->self_layout);
  btn->self_layout = NULL;
  widget_set_self_layout_params(btn, "c:100", "b:10", NULL, "10%");
  ASSERT_EQ(string(btn->self_layout->params.str), "default(x=c:100, y=b:10, h=10%)");

  self_layouter_destroy(btn->self_layout);
  btn->self_layout = NULL;
  widget_set_self_layout_params(btn, NULL, "b:10", "100%", "10%");
  ASSERT_EQ(string(btn->self_layout->params.str), "default(y=b:10, w=100%, h=10%)");

  self_layouter_destroy(btn->self_layout);
  btn->self_layout = NULL;
  widget_set_self_layout_params(btn, "c:100", NULL, "100%", "10%");
  ASSERT_EQ(string(btn->self_layout->params.str), "default(x=c:100, w=100%, h=10%)");
}

TEST(SelfLayoutDefault, set_self_layout_params2) {
  widget_t* win = overlay_create(NULL, 0, 0, 200, 100);
  widget_t* btn = button_create(win, 0, 0, 0, 0);
  widget_set_self_layout(btn, "default(x=r, y=b, w=50%, h=10%)");

  ASSERT_EQ(widget_layout_self(btn), RET_OK);

  self_layouter_t* layouter = btn->self_layout;

  ASSERT_EQ(widget_get_prop_int(btn, "x", 0), 100);
  ASSERT_EQ(widget_get_prop_int(btn, "y", 0), 90);
  ASSERT_EQ(widget_get_prop_int(btn, "w", 0), 100);
  ASSERT_EQ(widget_get_prop_int(btn, "h", 0), 10);

  widget_set_self_layout_params(btn, "c", NULL, NULL, NULL);
  ASSERT_EQ(widget_layout_self(btn), RET_OK);
  ASSERT_EQ(widget_get_prop_int(btn, "x", 0), 50);

  widget_set_self_layout_params(btn, NULL, "m", NULL, NULL);
  ASSERT_EQ(widget_layout_self(btn), RET_OK);
  ASSERT_EQ(widget_get_prop_int(btn, "x", 0), 50);
  ASSERT_EQ(widget_get_prop_int(btn, "y", 0), 45);

  widget_set_self_layout_params(btn, NULL, NULL, "100%", NULL);
  ASSERT_EQ(widget_layout_self(btn), RET_OK);
  ASSERT_EQ(widget_get_prop_int(btn, "x", 0), 0);
  ASSERT_EQ(widget_get_prop_int(btn, "y", 0), 45);
  ASSERT_EQ(widget_get_prop_int(btn, "w", 0), 200);

  widget_set_self_layout_params(btn, NULL, NULL, NULL, "100%");
  ASSERT_EQ(widget_layout_self(btn), RET_OK);
  ASSERT_EQ(widget_get_prop_int(btn, "x", 0), 0);
  ASSERT_EQ(widget_get_prop_int(btn, "y", 0), 0);
  ASSERT_EQ(widget_get_prop_int(btn, "w", 0), 200);
  ASSERT_EQ(widget_get_prop_int(btn, "h", 0), 100);

  widget_set_prop_int(btn, "w", 100);
  ASSERT_EQ(widget_layout_self(btn), RET_OK);
  ASSERT_EQ(widget_get_prop_int(btn, "x", 0), 50);
  ASSERT_EQ(widget_get_prop_int(btn, "y", 0), 0);
  ASSERT_EQ(widget_get_prop_int(btn, "w", 0), 100);
  ASSERT_EQ(widget_get_prop_int(btn, "h", 0), 100);

  widget_set_prop_int(btn, "h", 50);
  ASSERT_EQ(widget_layout_self(btn), RET_OK);
  ASSERT_EQ(widget_get_prop_int(btn, "x", 0), 50);
  ASSERT_EQ(widget_get_prop_int(btn, "y", 0), 25);
  ASSERT_EQ(widget_get_prop_int(btn, "w", 0), 100);
  ASSERT_EQ(widget_get_prop_int(btn, "h", 0), 50);

  ASSERT_EQ(self_layouter_get_param_int(layouter, "x_attr", 0), X_ATTR_CENTER);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "y_attr", 0), Y_ATTR_MIDDLE);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "w_attr", 0), W_ATTR_UNDEF);
  ASSERT_EQ(self_layouter_get_param_int(layouter, "h_attr", 0), W_ATTR_UNDEF);
}
