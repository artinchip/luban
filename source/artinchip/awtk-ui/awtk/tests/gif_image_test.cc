﻿#include "base/window.h"
#include "gif_image/gif_image.h"
#include "gtest/gtest.h"

TEST(GifImage, basic) {
  value_t v;
  value_t v1;
  widget_t* w = window_create(NULL, 0, 0, 0, 0);
  widget_t* img = gif_image_create(w, 0, 0, 100, 100);

  value_set_str(&v, "earth");
  ASSERT_EQ(widget_set_prop(img, WIDGET_PROP_IMAGE, &v), RET_OK);
  ASSERT_EQ(widget_get_prop(img, WIDGET_PROP_IMAGE, &v1), RET_OK);
  ASSERT_EQ(strcmp(value_str(&v), value_str(&v1)), 0);
  ASSERT_EQ(widget_count_children(w), 1);

  ASSERT_EQ((uint32_t)widget_get_prop_int(img, WIDGET_PROP_LOOP, 0), 0xffffffff);
  ASSERT_EQ(widget_set_prop_int(img, WIDGET_PROP_LOOP, 1), RET_OK);
  ASSERT_EQ(widget_get_prop_int(img, WIDGET_PROP_LOOP, 0), 1);

  widget_destroy(w);
}

TEST(GifImage, cast) {
  widget_t* w = window_create(NULL, 0, 0, 0, 0);
  widget_t* img = gif_image_create(w, 0, 0, 100, 100);

  ASSERT_EQ(img, gif_image_cast(img));
  ASSERT_EQ(img, image_base_cast(img));

  widget_destroy(w);
}

TEST(GifImage, state) {
  widget_t* w = window_create(NULL, 0, 0, 0, 0);
  widget_t* img = gif_image_create(w, 0, 0, 100, 100);

  ASSERT_EQ(GIF_IMAGE(img)->running, TRUE);

  gif_image_stop(img);
  ASSERT_EQ(GIF_IMAGE(img)->running, FALSE);

  gif_image_play(img);
  ASSERT_EQ(GIF_IMAGE(img)->running, TRUE);

  gif_image_pause(img);
  ASSERT_EQ(GIF_IMAGE(img)->running, FALSE);

  widget_destroy(w);
}
