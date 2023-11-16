﻿#include "gtest/gtest.h"
#include <string>
#include "base/keys.h"
#include "widgets/button.h"
#include "base/input_device_status.h"

using std::string;

#define TK_TRUE 1u
#define TK_FALSE 0u

TEST(InputDeviceStatus, basic) {
  input_device_status_t input_device_status;
  input_device_status_t* ids = input_device_status_init(&input_device_status);
  ASSERT_EQ(ids->alt, TK_FALSE);
  ASSERT_EQ(ids->ctrl, TK_FALSE);
  ASSERT_EQ(ids->capslock, TK_FALSE);
  ASSERT_EQ(ids->numlock, TK_FALSE);
  ASSERT_EQ(ids->pressed, TK_FALSE);
  ASSERT_EQ(ids->last_x, 0);
  ASSERT_EQ(ids->last_y, 0);
}

static string s_log;
static ret_t on_event(void* ctx, event_t* e) {
  if (e->type == EVT_KEY_DOWN) {
    s_log = "keydown";
  } else if (e->type == EVT_KEY_UP) {
    s_log = "keyup";
  } else if (e->type == EVT_POINTER_DOWN) {
    s_log = "pointerdown";
  } else if (e->type == EVT_POINTER_MOVE) {
    s_log = "pointermove";
  } else if (e->type == EVT_POINTER_UP) {
    s_log = "pointerup";
  }

  return RET_OK;
}

TEST(InputDeviceStatus, alt) {
  key_event_t e;
  widget_t* w = button_create(NULL, 0, 0, 0, 0);
  input_device_status_t input_device_status;
  input_device_status_t* ids = input_device_status_init(&input_device_status);

  widget_on(w, EVT_KEY_DOWN, on_event, w);
  widget_on(w, EVT_KEY_UP, on_event, w);

  key_event_init(&e, EVT_KEY_DOWN, NULL, TK_KEY_LALT);
  input_device_status_on_input_event(ids, w, (event_t*)(&e));
  ASSERT_EQ(ids->alt, TK_TRUE);
  ASSERT_EQ(s_log, "keydown");

  key_event_init(&e, EVT_KEY_UP, NULL, TK_KEY_LALT);
  input_device_status_on_input_event(ids, w, (event_t*)(&e));
  ASSERT_EQ(ids->alt, TK_FALSE);
  ASSERT_EQ(s_log, "keyup");

  key_event_init(&e, EVT_KEY_DOWN, NULL, TK_KEY_RALT);
  input_device_status_on_input_event(ids, w, (event_t*)(&e));
  ASSERT_EQ(ids->alt, TK_TRUE);
  ASSERT_EQ(s_log, "keydown");

  key_event_init(&e, EVT_KEY_UP, NULL, TK_KEY_RALT);
  input_device_status_on_input_event(ids, w, (event_t*)(&e));
  ASSERT_EQ(ids->alt, TK_FALSE);
  ASSERT_EQ(s_log, "keyup");

  widget_destroy(w);
}

TEST(InputDeviceStatus, ctrl) {
  key_event_t e;
  widget_t* w = button_create(NULL, 0, 0, 0, 0);
  input_device_status_t input_device_status;
  input_device_status_t* ids = input_device_status_init(&input_device_status);

  widget_on(w, EVT_KEY_DOWN, on_event, w);
  widget_on(w, EVT_KEY_UP, on_event, w);

  key_event_init(&e, EVT_KEY_DOWN, NULL, TK_KEY_LCTRL);
  input_device_status_on_input_event(ids, w, (event_t*)(&e));
  ASSERT_EQ(ids->ctrl, TK_TRUE);
  ASSERT_EQ(s_log, "keydown");

  key_event_init(&e, EVT_KEY_UP, NULL, TK_KEY_LCTRL);
  input_device_status_on_input_event(ids, w, (event_t*)(&e));
  ASSERT_EQ(ids->ctrl, TK_FALSE);
  ASSERT_EQ(s_log, "keyup");

  key_event_init(&e, EVT_KEY_DOWN, NULL, TK_KEY_RCTRL);
  input_device_status_on_input_event(ids, w, (event_t*)(&e));
  ASSERT_EQ(ids->ctrl, TK_TRUE);
  ASSERT_EQ(s_log, "keydown");

  key_event_init(&e, EVT_KEY_UP, NULL, TK_KEY_RCTRL);
  input_device_status_on_input_event(ids, w, (event_t*)(&e));
  ASSERT_EQ(ids->ctrl, TK_FALSE);
  ASSERT_EQ(s_log, "keyup");

  widget_destroy(w);
}

TEST(InputDeviceStatus, capslock) {
  key_event_t e;
  widget_t* w = button_create(NULL, 0, 0, 0, 0);
  input_device_status_t input_device_status;
  input_device_status_t* ids = input_device_status_init(&input_device_status);

  widget_on(w, EVT_KEY_DOWN, on_event, w);
  widget_on(w, EVT_KEY_UP, on_event, w);

  key_event_init(&e, EVT_KEY_DOWN, NULL, TK_KEY_CAPSLOCK);
  input_device_status_on_input_event(ids, w, (event_t*)(&e));
  ASSERT_EQ(s_log, "keydown");

  key_event_init(&e, EVT_KEY_UP, NULL, TK_KEY_CAPSLOCK);
  e.capslock = TK_TRUE;
  input_device_status_on_input_event(ids, w, (event_t*)(&e));
  ASSERT_EQ(s_log, "keyup");

  ASSERT_EQ(ids->capslock, TK_TRUE);

  key_event_init(&e, EVT_KEY_DOWN, NULL, TK_KEY_CAPSLOCK);
  input_device_status_on_input_event(ids, w, (event_t*)(&e));
  ASSERT_EQ(s_log, "keydown");

  key_event_init(&e, EVT_KEY_UP, NULL, TK_KEY_CAPSLOCK);
  e.capslock = TK_FALSE;
  input_device_status_on_input_event(ids, w, (event_t*)(&e));
  ASSERT_EQ(s_log, "keyup");

  ASSERT_EQ(ids->capslock, TK_FALSE);

  widget_destroy(w);
}

TEST(InputDeviceStatus, numlock) {
  key_event_t e;
  widget_t* w = button_create(NULL, 0, 0, 0, 0);
  input_device_status_t input_device_status;
  input_device_status_t* ids = input_device_status_init(&input_device_status);

  widget_on(w, EVT_KEY_DOWN, on_event, w);
  widget_on(w, EVT_KEY_UP, on_event, w);

  key_event_init(&e, EVT_KEY_DOWN, NULL, TK_KEY_NUMLOCKCLEAR);
  input_device_status_on_input_event(ids, w, (event_t*)(&e));
  ASSERT_EQ(s_log, "keydown");

  key_event_init(&e, EVT_KEY_UP, NULL, TK_KEY_NUMLOCKCLEAR);
  e.numlock = TK_TRUE;
  input_device_status_on_input_event(ids, w, (event_t*)(&e));
  ASSERT_EQ(s_log, "keyup");

  ASSERT_EQ(ids->numlock, TK_TRUE);

  key_event_init(&e, EVT_KEY_DOWN, NULL, TK_KEY_NUMLOCKCLEAR);
  input_device_status_on_input_event(ids, w, (event_t*)(&e));
  ASSERT_EQ(s_log, "keydown");

  key_event_init(&e, EVT_KEY_UP, NULL, TK_KEY_NUMLOCKCLEAR);
  e.numlock = TK_FALSE;
  input_device_status_on_input_event(ids, w, (event_t*)(&e));
  ASSERT_EQ(s_log, "keyup");

  ASSERT_EQ(ids->numlock, TK_FALSE);

  widget_destroy(w);
}

TEST(InputDeviceStatus, pointer) {
  pointer_event_t e;
  widget_t* w = button_create(NULL, 0, 0, 0, 0);
  input_device_status_t input_device_status;
  input_device_status_t* ids = input_device_status_init(&input_device_status);

  widget_on(w, EVT_POINTER_DOWN, on_event, w);
  widget_on(w, EVT_POINTER_MOVE, on_event, w);
  widget_on(w, EVT_POINTER_UP, on_event, w);

  pointer_event_init(&e, EVT_POINTER_DOWN, NULL, 10, 20);
  e.pressed = TK_TRUE;
  input_device_status_on_input_event(ids, w, (event_t*)(&e));

  ASSERT_EQ(ids->last_x, e.x);
  ASSERT_EQ(ids->last_y, e.y);
  ASSERT_EQ(ids->pressed, TK_TRUE);
  ASSERT_EQ(s_log, "pointerdown");

  pointer_event_init(&e, EVT_POINTER_MOVE, NULL, 20, 30);
  e.pressed = TK_TRUE;
  input_device_status_on_input_event(ids, w, (event_t*)(&e));

  ASSERT_EQ(ids->last_x, e.x);
  ASSERT_EQ(ids->last_y, e.y);
  ASSERT_EQ(ids->pressed, TK_TRUE);
  ASSERT_EQ(s_log, "pointermove");

  pointer_event_init(&e, EVT_POINTER_UP, NULL, 30, 40);
  e.pressed = TK_TRUE;
  input_device_status_on_input_event(ids, w, (event_t*)(&e));

  ASSERT_EQ(ids->last_x, e.x);
  ASSERT_EQ(ids->last_y, e.y);
  ASSERT_EQ(ids->pressed, TK_FALSE);
  ASSERT_EQ(s_log, "pointerup");

  widget_destroy(w);
}

TEST(InputDeviceStatus, should_abort) {
  key_event_t e;
  widget_t* w = button_create(NULL, 0, 0, 0, 0);
  input_device_status_t input_device_status;
  input_device_status_t* ids = input_device_status_init(&input_device_status);

  widget_on(w, EVT_KEY_DOWN, on_event, w);
  widget_on(w, EVT_KEY_UP, on_event, w);

  key_event_init(&e, EVT_KEY_DOWN, NULL, TK_KEY_CAPSLOCK);
  input_device_status_on_input_event(ids, w, (event_t*)(&e));
  ASSERT_EQ(s_log, "keydown");

  input_device_status_abort_all_pressed_keys(ids);

  s_log = "";
  key_event_init(&e, EVT_KEY_DOWN, NULL, TK_KEY_CAPSLOCK);
  input_device_status_on_input_event(ids, w, (event_t*)(&e));
  ASSERT_EQ(s_log, "");

  s_log = "";
  key_event_init(&e, EVT_KEY_UP, NULL, TK_KEY_CAPSLOCK);
  input_device_status_on_input_event(ids, w, (event_t*)(&e));
  ASSERT_EQ(s_log, "");

  s_log = "";
  key_event_init(&e, EVT_KEY_DOWN, NULL, TK_KEY_CAPSLOCK);
  input_device_status_on_input_event(ids, w, (event_t*)(&e));
  ASSERT_EQ(s_log, "keydown");

  widget_destroy(w);
}
