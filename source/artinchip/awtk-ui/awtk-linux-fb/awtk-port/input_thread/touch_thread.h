/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Zequan Liang <zequan.liang@artinchip.com>
 */

#ifndef _AIC_TK_TOUCH_THREAD_H
#define _AIC_TK_TOUCH_THREAD_H

#include "tkc/thread.h"
#include "input_dispatcher.h"

BEGIN_C_DECLS

tk_thread_t* touch_thread_run(const char* filename, input_dispatch_t dispatch, void* ctx,
                              int32_t max_x, int32_t max_y);

END_C_DECLS

#endif /*_AIC_TK_TOUCH_THREAD_H*/
