/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Zequan Liang <zequan.liang@artinchip.com>
 */

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "tkc/mem.h"
#include "base/keys.h"
#include "tkc/thread.h"
#include "touch_thread.h"
#include "tkc/utils.h"

#include <time.h>
#include <linux/input.h>

typedef struct _run_info_t {
  int32_t max_x;
  int32_t max_y;
  void* dispatch_ctx;
  char* filename;
  input_dispatch_t dispatch;

  int evdev_fd;
  int tk_evdev_root_x;
  int tk_evdev_root_y;
  int tk_evdev_type;

#if USE_TSLIB
  struct tsdev *ts;
#endif
  event_queue_req_t req;
} run_info_t;

#if USE_TSLIB
#include "tslib.h"
#endif

ret_t evdev_set_file(char* dev_name);

int evdev_init(run_info_t *info)
{
  if(info->evdev_fd != -1) {
    close(info->evdev_fd);
  }
#if USE_TSLIB == 0
#if USE_BSD_EVDEV
  info->evdev_fd = open(info->filename, O_RDWR | O_NOCTTY);
#else
  info->evdev_fd = open("/dev/input/event0", O_RDWR | O_NOCTTY | O_NDELAY);
  log_error("info->evdev_fd = %d\n", info->evdev_fd);
#endif

  if(info->evdev_fd == -1) {
    log_error("unable to open evdev interface:");
    return RET_FAIL;
  }

#if USE_BSD_EVDEV
  fcntl(info->evdev_fd, F_SETFL, O_NONBLOCK);
#else
  fcntl(info->evdev_fd, F_SETFL, O_ASYNC);
#endif
#else
  info->ts = ts_setup(NULL, 1);
  if(!ts) {
    log_error("ts_setup");
    return RET_FAIL;
  }
#endif
  info->tk_evdev_root_x = 0;
  info->tk_evdev_root_y = 0;
  info->tk_evdev_type = 0;
  return RET_OK;
}

void evdev_read(run_info_t *info)
{
#if USE_TSLIB == 0
  struct input_event in;
  if(read(info->evdev_fd, &in, sizeof(struct input_event)) > 0) {
    if(in.type == EV_REL) {
      if(in.code == REL_X)
        info->tk_evdev_root_x += in.value;
      else if(in.code == REL_Y)
        info->tk_evdev_root_y += in.value;
      info->tk_evdev_type = EVT_POINTER_MOVE;
    } else if (in.type == EV_ABS) {
        if(in.code == ABS_X)
          info->tk_evdev_root_x = in.value;
        else if(in.code == ABS_Y)
          info->tk_evdev_root_y = in.value;
        else if(in.code == ABS_MT_POSITION_X)
          info->tk_evdev_root_x = in.value;
        else if(in.code == ABS_MT_POSITION_Y)
          info->tk_evdev_root_y = in.value;
        else if(in.code == ABS_MT_TRACKING_ID) {
          if(in.value == -1) {
            info->tk_evdev_type = EVT_POINTER_UP;
	  } else if(in.value == 0) {
            info->tk_evdev_type = EVT_POINTER_DOWN;
	  }
        }
    } else if(in.type == EV_KEY) {
        if(in.code == BTN_MOUSE || in.code == BTN_TOUCH) {
          if(in.value == 0) {
            info->tk_evdev_type = EVT_POINTER_UP;
	  } else if(in.value == 1) {
	    info->tk_evdev_type = EVT_POINTER_DOWN;
	  }
      }
    }
  }
#else
  struct ts_sample samp;
    while(ts_read(ts, &samp, 1) == 1) {
      info->tk_evdev_root_x = samp.x;
      info->tk_evdev_root_y = samp.y;

      event_queue_req_t* req = &(info->req);
      if (samp.pressure > 0) {
        if (req->pointer_event.pressed) {
          info->tk_evdev_type = EVT_POINTER_MOVE;
        } else {
          info->tk_evdev_type = EVT_POINTER_DOWN;
          req->pointer_event.pressed = RET_OK;
        }
      } else {
        if (req->pointer_event.pressed) {
          info->tk_evdev_type = EVT_POINTER_UP;
        }
        req->pointer_event.pressed = RET_FAIL;
      }

      break;
  }
#endif
    return;
}

static ret_t touch_dispatch(run_info_t* info) {
  ret_t ret = info->dispatch(info->dispatch_ctx, &(info->req), "touch");
  info->req.event.type = EVT_NONE;
  info->tk_evdev_type = info->req.event.type;

  return ret;
}

static ret_t touch_dispatch_one_event(run_info_t* info) {
  evdev_read(info);
  event_queue_req_t* req = &(info->req);
  static int press = RET_FAIL;

  req->pointer_event.x = info->tk_evdev_root_x;
  req->pointer_event.y = info->tk_evdev_root_y;
  req->event.type = info->tk_evdev_type;
  if (info->tk_evdev_type == EVT_POINTER_DOWN) {
    press = RET_OK;
    req->event.type = EVT_POINTER_DOWN;
  } else if (press == RET_OK && info->tk_evdev_type == 0) {
    req->event.type = EVT_POINTER_MOVE;
  } else if (info->tk_evdev_type == EVT_POINTER_UP) {
    req->event.type = EVT_POINTER_UP;
    press = RET_FAIL;
  } else {
    req->event.type = EVT_NONE;
    req->pointer_event.pressed = RET_FAIL;
  }
  return touch_dispatch(info);
}

void* touch_run(void* ctx) {
  run_info_t info = *(run_info_t*)ctx;

  if (info.evdev_fd > 0) {
    log_debug("%s:%d: open touch failed, filename=%s\n", __func__, __LINE__, info.filename);
  } else {
    log_debug("%s:%d: open touch successful, filename=%s\n", __func__, __LINE__, info.filename);
  }

  TKMEM_FREE(ctx);
  while (touch_dispatch_one_event(&info) == RET_OK);
  TKMEM_FREE(info.filename);


  return NULL;
}

static run_info_t* info_dup(run_info_t* info) {
  run_info_t* new_info = TKMEM_ZALLOC(run_info_t);

  *new_info = *info;

  return new_info;
}

tk_thread_t* touch_thread_run(const char* filename, input_dispatch_t dispatch, void* ctx,
                              int32_t max_x, int32_t max_y) {
  run_info_t info;
  tk_thread_t* thread = NULL;
  return_value_if_fail(filename != NULL && dispatch != NULL, NULL);

  memset(&info, 0x00, sizeof(info));

  info.max_x = max_x;
  info.max_y = max_y;
  info.dispatch_ctx = ctx;
  info.dispatch = dispatch;

  info.filename = tk_strdup(filename);
  evdev_init(&info);

  thread = tk_thread_create(touch_run, info_dup(&info));
  if (thread != NULL) {
    tk_thread_start(thread);
  } else {
    TKMEM_FREE(info.filename);
  }

  return thread;
}

