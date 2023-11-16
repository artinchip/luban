﻿/**
 * File:   native_window_fb_gl.h
 * Author: AWTK Develop Team
 * Brief:  native window for egl
 *
 * Copyright (c) 2019 - 2023  Guangzhou ZHIYUAN Electronics Co.,Ltd.
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
 * 2019-10-31 Lou ZhiMing <luozhiming@zlg.com> created
 *
 */

#ifndef TK_NATIVE_WINDOW_FB_GL_H
#define TK_NATIVE_WINDOW_FB_GL_H

#include "base/lcd.h"
#include "base/native_window.h"

BEGIN_C_DECLS

typedef ret_t (*native_window_destroy_t)(native_window_t* win);

ret_t native_window_fb_gl_deinit(void);
native_window_t* native_window_fb_gl_init(uint32_t w, uint32_t h, float_t ratio);

ret_t native_window_fb_gl_set_swap_buffer_func(native_window_t* win,
                                               native_window_swap_buffer_t swap_buffer);

ret_t native_window_fb_gl_set_make_current_func(native_window_t* win,
                                                native_window_gl_make_current_t make_current);

ret_t native_window_fb_gl_set_destroy_func(native_window_t* win, native_window_destroy_t destroy);

lcd_t* native_window_get_lcd(native_window_t* win);

END_C_DECLS

#endif /*TK_NATIVE_WINDOW_FB_GL_H*/
