/**
 * File:   axis_p.h
 * Author: AWTK Develop Team
 * Brief:  axis private
 *
 * Copyright (c) 2018 - 2018  Guangzhou ZHIYUAN Electronics Co.,Ltd.
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
 * 2018-12-05 Xu ChaoZe <xuchaoze@zlg.cn> created
 *
 */

#ifndef TK_AXIS_PRIVATE_H
#define TK_AXIS_PRIVATE_H

#include "axis.h"

BEGIN_C_DECLS

void axis_p_parse_line_params(void* ctx, const char* name, const value_t* v);
void axis_p_parse_split_line_params(void* ctx, const char* name, const value_t* v);
void axis_p_parse_tick_params(void* ctx, const char* name, const value_t* v);
void axis_p_parse_label_params(void* ctx, const char* name, const value_t* v);
void axis_p_parse_title_params(void* ctx, const char* name, const value_t* v);
void axis_p_parse_time_params(void* ctx, const char* name, const value_t* v);

ret_t axis_p_get_prop(widget_t* widget, const char* name, value_t* v);
ret_t axis_p_set_prop(widget_t* widget, const char* name, const value_t* v);

ret_t axis_p_time_generate_default(widget_t* widget, uint32_t recent_index, uint32_t begin,
                                   uint32_t end, uint32_t middle, float_t interval, darray_t* v,
                                   void* ctx);

END_C_DECLS

#endif /*TK_AXIS_PRIVATE_H*/
