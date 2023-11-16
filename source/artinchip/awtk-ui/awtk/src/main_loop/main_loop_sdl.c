﻿/**
 * File:   main_loop_sdl.c
 * Author: AWTK Develop Team
 * Brief:  sdl2 implemented main_loop interface
 *
 * Copyright (c) 2018 - 2023  Guangzhou ZHIYUAN Electronics Co.,Ltd.
 *
 * this program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * merchantability or fitness for a particular purpose.  see the
 * license file for more details.
 *
 */

/**
 * history:
 * ================================================================
 * 2018-01-13 li xianjing <xianjimli@hotmail.com> created
 *
 */

#include "native_window/native_window_sdl.h"
#include "main_loop/main_loop_simple.h"
#include "main_loop/main_loop_sdl.h"
#include "base/window_manager.h"
#include "base/font_manager.h"
#include "lcd/lcd_sdl2.h"
#include "base/idle.h"
#include "base/events.h"
#include "base/timer.h"
#include "base/system_info.h"
#include <SDL.h>

#include <stdio.h>
#include "awtk_global.h"
#include "tkc/time_now.h"
#include "base/input_method.h"

static ret_t main_loop_sdl2_dispatch_text_input(main_loop_simple_t* loop, SDL_Event* sdl_event) {
  im_commit_event_t event;
  SDL_TextInputEvent* text_input_event = (SDL_TextInputEvent*)sdl_event;

  memset(&event, 0x00, sizeof(event));
  event.e = event_init(EVT_IM_COMMIT, NULL);
  event.text = text_input_event->text;

  return input_method_dispatch_to_widget(input_method(), &(event.e));
}

static ret_t main_loop_sdl2_dispatch_text_editing(main_loop_simple_t* loop, SDL_Event* sdl_event) {
  return RET_OK;
}

static ret_t main_loop_sdl2_set_key_event_mod(key_event_t* event, uint16_t mod) {
  event->capslock = (mod & KMOD_CAPS) != 0;
  event->numlock = (mod & KMOD_NUM) != 0;
  return RET_OK;
}

static ret_t main_loop_sdl2_dispatch_key_event(main_loop_simple_t* loop, SDL_Event* sdl_event) {
  key_event_t event;
  int type = sdl_event->type;
  widget_t* widget = loop->base.wm;

  switch (type) {
    case SDL_KEYDOWN: {
      key_event_init(&event, EVT_KEY_DOWN, widget, sdl_event->key.keysym.sym);
      main_loop_sdl2_set_key_event_mod(&event, sdl_event->key.keysym.mod);
      event.e.native_window_handle = SDL_GetWindowFromID(sdl_event->key.windowID);
      window_manager_dispatch_input_event(widget, (event_t*)&event);
      break;
    }
    case SDL_KEYUP: {
      key_event_init(&event, EVT_KEY_UP, widget, sdl_event->key.keysym.sym);
      main_loop_sdl2_set_key_event_mod(&event, sdl_event->key.keysym.mod);
      event.e.native_window_handle = SDL_GetWindowFromID(sdl_event->key.windowID);
      window_manager_dispatch_input_event(widget, (event_t*)&event);
      break;
    }
    default:
      break;
  }

  return RET_OK;
}

#define MIN_WHEEL_DELTA 12

static ret_t main_loop_sdl2_dispatch_wheel_event(main_loop_simple_t* loop, SDL_Event* sdl_event) {
  wheel_event_t event;
  widget_t* widget = loop->base.wm;
  event_t* e = wheel_event_init(&event, EVT_WHEEL, widget, sdl_event->wheel.y);

  event.e.native_window_handle = SDL_GetWindowFromID(sdl_event->wheel.windowID);
  if (event.dy > 0) {
    event.dy = tk_max(MIN_WHEEL_DELTA, event.dy);
  } else if (event.dy < 0) {
    event.dy = tk_min(-MIN_WHEEL_DELTA, event.dy);
  }
  window_manager_dispatch_input_event(widget, e);

  return RET_OK;
}

static ret_t main_loop_sdl2_dispatch_multi_gesture_event(main_loop_simple_t* loop,
                                                         SDL_Event* sdl_event) {
  multi_gesture_event_t event;
  widget_t* widget = loop->base.wm;
  int32_t x = sdl_event->mgesture.x * widget->w;
  int32_t y = sdl_event->mgesture.y * widget->h;
  float rotation = sdl_event->mgesture.dTheta;
  float distance = sdl_event->mgesture.dDist;
  event_t* e = multi_gesture_event_init(&event, widget, x, y, rotation, distance);

  event.e.native_window_handle = NULL;
  window_manager_dispatch_input_event(widget, e);

  return RET_OK;
}

static ret_t main_loop_sdl2_dispatch_mouse_event(main_loop_simple_t* loop, SDL_Event* sdl_event) {
  key_event_t key_event;
  pointer_event_t event;
  int type = sdl_event->type;
  widget_t* widget = loop->base.wm;

  memset(&event, 0x00, sizeof(event));
  switch (type) {
    case SDL_MOUSEBUTTONDOWN: {
      if (sdl_event->button.button == 1) {
        loop->pressed = 1;
        pointer_event_init(&event, EVT_POINTER_DOWN, widget, sdl_event->button.x,
                           sdl_event->button.y);
        event.button = sdl_event->button.button;
        event.pressed = loop->pressed;
        event.e.native_window_handle = SDL_GetWindowFromID(sdl_event->button.windowID);

        SDL_CaptureMouse(TRUE);
        window_manager_dispatch_input_event(widget, (event_t*)&event);
      } else if (sdl_event->button.button == 2) {
        key_event_init(&key_event, EVT_KEY_DOWN, widget, TK_KEY_WHEEL);
        window_manager_dispatch_input_event(widget, (event_t*)&key_event);
      }
      break;
    }
    case SDL_MOUSEBUTTONUP: {
      if (sdl_event->button.button == 1) {
        SDL_CaptureMouse(FALSE);
        pointer_event_init(&event, EVT_POINTER_UP, widget, sdl_event->button.x,
                           sdl_event->button.y);
        event.button = sdl_event->button.button;
        event.pressed = loop->pressed;
        event.e.native_window_handle = SDL_GetWindowFromID(sdl_event->button.windowID);

        window_manager_dispatch_input_event(widget, (event_t*)&event);
        loop->pressed = 0;
      } else if (sdl_event->button.button == 3) {
        pointer_event_init(&event, EVT_CONTEXT_MENU, widget, sdl_event->button.x,
                           sdl_event->button.y);
        event.button = sdl_event->button.button;
        event.e.native_window_handle = SDL_GetWindowFromID(sdl_event->button.windowID);
        window_manager_dispatch_input_event(widget, (event_t*)&event);
      } else if (sdl_event->button.button == 2) {
        key_event_init(&key_event, EVT_KEY_UP, widget, TK_KEY_WHEEL);
        window_manager_dispatch_input_event(widget, (event_t*)&key_event);
      }
      break;
    }
    case SDL_MOUSEMOTION: {
      pointer_event_init(&event, EVT_POINTER_MOVE, widget, sdl_event->button.x,
                         sdl_event->button.y);
      event.button = 0;
      event.pressed = loop->pressed;
      event.e.native_window_handle = SDL_GetWindowFromID(sdl_event->button.windowID);

      window_manager_dispatch_input_event(widget, (event_t*)&event);
      break;
    }
    default:
      break;
  }

  return RET_OK;
}

static ret_t on_resized_timer(const timer_info_t* info) {
  widget_t* wm = WIDGET(info->ctx);
  widget_set_need_relayout_children(wm);
  widget_invalidate_force(wm, NULL);

  log_debug("on_resized_timer\n");
  return RET_REMOVE;
}

static ret_t main_loop_sdl2_dispatch_window_event(main_loop_simple_t* loop, SDL_Event* event) {
  main_loop_t* l = (main_loop_t*)(loop);

  switch (event->window.event) {
    case SDL_WINDOWEVENT_SHOWN:
      log_debug("Window %d shown\n", event->window.windowID);
      widget_invalidate_force(l->wm, NULL);
      break;
    case SDL_WINDOWEVENT_HIDDEN:
      log_debug("Window %d hidden\n", event->window.windowID);
      break;
    case SDL_WINDOWEVENT_EXPOSED:
      log_debug("Window %d exposed\n", event->window.windowID);
      widget_invalidate_force(l->wm, NULL);
      break;
    case SDL_WINDOWEVENT_MOVED:
      log_debug("Window %d moved to %d,%d\n", event->window.windowID, event->window.data1,
                event->window.data2);
      break;
    case SDL_WINDOWEVENT_RESIZED:
      log_debug("Window %d resized to %dx%d\n", event->window.windowID, event->window.data1,
                event->window.data2);
      timer_add(on_resized_timer, l->wm, 100);
      break;
    case SDL_WINDOWEVENT_SIZE_CHANGED: {
      native_window_info_t info;
      event_t e = event_init(EVT_NATIVE_WINDOW_RESIZED, NULL);
      SDL_Window* win = SDL_GetWindowFromID(event->window.windowID);
      native_window_t* native_window =
          (native_window_t*)widget_get_prop_pointer(window_manager(), WIDGET_PROP_NATIVE_WINDOW);
      native_window_get_info(native_window, &info);
      system_info_set_lcd_w(system_info(), info.w);
      system_info_set_lcd_h(system_info(), info.h);
      window_manager_dispatch_native_window_event(l->wm, &e, win);
      timer_add(on_resized_timer, l->wm, 100);
      break;
    }
    case SDL_WINDOWEVENT_MINIMIZED:
      log_debug("Window %d minimized\n", event->window.windowID);
      break;
    case SDL_WINDOWEVENT_MAXIMIZED:
      log_debug("Window %d maximized\n", event->window.windowID);
      widget_invalidate_force(l->wm, NULL);
      break;
    case SDL_WINDOWEVENT_RESTORED:
      log_debug("Window %d restored\n", event->window.windowID);
      widget_invalidate_force(l->wm, NULL);
      break;
    case SDL_WINDOWEVENT_ENTER: {
      int x = 0;
      int y = 0;
      pointer_event_t e;
      SDL_Window* win = SDL_GetWindowFromID(event->window.windowID);

      log_debug("Mouse entered window %d\n", event->window.windowID);
      SDL_GetMouseState(&x, &y);
      pointer_event_init(&e, EVT_NATIVE_WINDOW_ENTER, l->wm, x, y);
      window_manager_dispatch_native_window_event(l->wm, (event_t*)&e, win);
      break;
    }
    case SDL_WINDOWEVENT_LEAVE: {
      event_t e = event_init(EVT_NATIVE_WINDOW_LEAVE, NULL);
      SDL_Window* win = SDL_GetWindowFromID(event->window.windowID);

      log_debug("Mouse left window %d\n", event->window.windowID);
      window_manager_dispatch_native_window_event(l->wm, &e, win);
      break;
    }
    case SDL_WINDOWEVENT_FOCUS_GAINED:
      log_debug("Window %d gained keyboard focus\n", event->window.windowID);
      break;
    case SDL_WINDOWEVENT_FOCUS_LOST:
      SDL_CaptureMouse(FALSE);
      log_debug("Window %d lost keyboard focus\n", event->window.windowID);
      break;
#if SDL_VERSION_ATLEAST(2, 0, 5)
    case SDL_WINDOWEVENT_TAKE_FOCUS:
      log_debug("Window %d is offered a focus\n", event->window.windowID);
      break;
    case SDL_WINDOWEVENT_HIT_TEST:
      log_debug("Window %d has a special hit test\n", event->window.windowID);
      break;
#endif
    default:
      log_debug("Window %d got unknown event %d\n", event->window.windowID, event->window.event);
      break;
  }

  return RET_OK;
}

static ret_t main_loop_sdl2_dispatch(main_loop_simple_t* loop) {
  SDL_Event event;
  ret_t ret = RET_OK;
  widget_t* wm = loop->base.wm;
  while (SDL_PollEvent(&event) && loop->base.running) {
    switch (event.type) {
      case SDL_DROPFILE: {
        drop_file_event_t drop;
        widget_t* top = window_manager_get_top_window(wm);
        event_t* e = drop_file_event_init(&drop, NULL, event.drop.file);

        widget_dispatch(wm, e);
        if (top != NULL) {
          widget_dispatch(top, e);
        }

        break;
      }
      case SDL_KEYDOWN:
      case SDL_KEYUP: {
        ret = main_loop_sdl2_dispatch_key_event(loop, &event);
        break;
      }
      case SDL_MOUSEMOTION:
      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP: {
        ret = main_loop_sdl2_dispatch_mouse_event(loop, &event);
        break;
      }
      case SDL_TEXTINPUT: {
        ret = main_loop_sdl2_dispatch_text_input(loop, &event);
        break;
      }
      case SDL_TEXTEDITING: {
        ret = main_loop_sdl2_dispatch_text_editing(loop, &event);
        break;
      }
      case SDL_MOUSEWHEEL: {
        ret = main_loop_sdl2_dispatch_wheel_event(loop, &event);
        break;
      }
      case SDL_MULTIGESTURE: {
        ret = main_loop_sdl2_dispatch_multi_gesture_event(loop, &event);
        break;
      }
      case SDL_WINDOWEVENT: {
        main_loop_sdl2_dispatch_window_event(loop, &event);
        break;
      }
      case SDL_SYSWMEVENT: {
        system_event_t e;
        widget_dispatch(wm, system_event_init(&e, NULL, &event));
        break;
      }
      case SDL_QUIT: {
        event_t e = event_init(EVT_REQUEST_QUIT_APP, NULL);
        if (widget_dispatch(wm, &e) == RET_OK) {
          main_loop_quit((main_loop_t*)loop);
        }
        break;
      }
    }
  }

  return ret;
}

static ret_t main_loop_sdl2_destroy(main_loop_t* l) {
  main_loop_simple_t* loop = (main_loop_simple_t*)l;

  main_loop_simple_reset(loop);
  native_window_sdl_deinit();

  return RET_OK;
}

main_loop_t* main_loop_init(int w, int h) {
  main_loop_simple_t* loop = NULL;
#ifdef MULTI_NATIVE_WINDOW
  native_window_sdl_init(FALSE, w, h);
#else
  native_window_sdl_init(TRUE, w, h);
#endif /*MULTI_NATIVE_WINDOW*/
  loop = main_loop_simple_init(w, h, NULL, NULL);
  loop->base.destroy = main_loop_sdl2_destroy;
  loop->dispatch_input = main_loop_sdl2_dispatch;
  SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
  SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);

  return (main_loop_t*)loop;
}
