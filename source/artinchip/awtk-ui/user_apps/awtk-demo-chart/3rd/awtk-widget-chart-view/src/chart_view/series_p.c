/**
 * File:   series_p.c
 * Author: AWTK Develop Team
 * Brief:  series private
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

#include "tkc/mem.h"
#include "tkc/utils.h"
#include "base/style.h"
#include "series_p.h"
#include "chart_animator.h"
#include "base/widget_animator_manager.h"
#include "widget_animators/widget_animator_prop.h"
#include "x_axis.h"
#include "y_axis.h"

#include "../base/wuxiaolin_draw_line.inc"
#define _CANVAS_DRAW_LINE(c, x1, y1, x2, y2) \
  wuxiaolin_draw_line(c, c->ox + (x1), c->oy + (y1), c->ox + (x2), c->oy + (y2))

#define _COLOR_BLACK color_init(0, 0, 0, 0xff)
#define _COLOR_TRANS color_init(0, 0, 0, 0)

// 坐标取整加0.5，防止线宽为1时显示2个像素
#define _VG_XY(v) (float_t)((xy_t)(v) + 0.5)
#define _VGCANVAS_MOVE_TO(vg, x, y) vgcanvas_move_to(vg, _VG_XY(x), _VG_XY(y))
#define _VGCANVAS_LINE_TO(vg, x, y) vgcanvas_line_to(vg, _VG_XY(x), _VG_XY(y))
#define _VGCANVAS_RECT(vg, x, y, w, h) vgcanvas_rect(vg, _VG_XY(x), _VG_XY(y), w, h)
#define _VGCANVAS_ROUNDED_RECT(vg, x, y, w, h, r) \
  vgcanvas_rounded_rect(vg, _VG_XY(x), _VG_XY(y), w, h, r)
#define _VGCANVAS_ARC(vg, x, y, r, start, end, ccw) \
  vgcanvas_arc(vg, _VG_XY(x), _VG_XY(y), r, start, end, ccw)
#define _VACANVAS_BEZIER_TO(vg, cp1x, cp1y, cp2x, cp2y, x, y)                               \
  vgcanvas_bezier_to(vg, _VG_XY(cp1x), _VG_XY(cp1y), _VG_XY(cp2x), _VG_XY(cp2y), _VG_XY(x), \
                     _VG_XY(y))

// i == index时，由于cover类型的波形, 新数据第一个坐标点的前一个点在坐标轴的另一端, 故此处处理偏移
#define _VACANVAS_BEZIER_FIFO_ONE(i, index, size, fifo, d, dprev, ox, oy, draw_data_t)             \
  {                                                                                                \
    float_t cp1x, cp1y, cp2x, cp2y;                                                                \
    if (i == index + size - 2) {                                                                   \
      draw_data_t* dnext;                                                                          \
      dnext = (draw_data_t*)(series_fifo_get(fifo, i - 1));                                        \
      cp1x = ox + dprev->x;                                                                        \
      cp1y = oy + dprev->y;                                                                        \
      cp2x = ox + d->x - (dnext->x - dprev->x) / 4;                                                \
      cp2y = oy + d->y - (dnext->y - dprev->y) / 4;                                                \
    } else if (i == 0) {                                                                           \
      draw_data_t* dprev2;                                                                         \
      dprev2 = (draw_data_t*)(series_fifo_get(fifo, i + 2));                                       \
      cp1x = ox + dprev->x + (d->x - dprev2->x) / 4;                                               \
      cp1y = oy + dprev->y + (d->y - dprev2->y) / 4;                                               \
      cp2x = ox + d->x - (d->x - dprev->x) / 4;                                                    \
      cp2y = oy + d->y - (d->y - dprev->y) / 4;                                                    \
    } else if (i == index) {                                                                       \
      float_t nx, ny;                                                                              \
      draw_data_t* dnext;                                                                          \
      draw_data_t* dprev2;                                                                         \
      dnext = (draw_data_t*)(series_fifo_get(fifo, i - 1));                                        \
      dprev2 = (draw_data_t*)(series_fifo_get(fifo, i + 2));                                       \
      nx = (vertical && dnext->x - d->x != d->x - dprev->x) ? (d->x * 2 - dprev->x) : (dnext->x);  \
      ny = (!vertical && dnext->y - d->y != d->y - dprev->y) ? (d->y * 2 - dprev->y) : (dnext->y); \
      cp1x = ox + dprev->x + (d->x - dprev2->x) / 4;                                               \
      cp1y = oy + dprev->y + (d->y - dprev2->y) / 4;                                               \
      cp2x = ox + d->x - (nx - dprev->x) / 4;                                                      \
      cp2y = oy + d->y - (ny - dprev->y) / 4;                                                      \
    } else {                                                                                       \
      draw_data_t* dnext;                                                                          \
      draw_data_t* dprev2;                                                                         \
      dnext = (draw_data_t*)(series_fifo_get(fifo, i - 1));                                        \
      dprev2 = (draw_data_t*)(series_fifo_get(fifo, i + 2));                                       \
      cp1x = ox + dprev->x + (d->x - dprev2->x) / 4;                                               \
      cp1y = oy + dprev->y + (d->y - dprev2->y) / 4;                                               \
      cp2x = ox + d->x - (dnext->x - dprev->x) / 4;                                                \
      cp2y = oy + d->y - (dnext->y - dprev->y) / 4;                                                \
    }                                                                                              \
    _VACANVAS_BEZIER_TO(vg, cp1x, cp1y, cp2x, cp2y, ox + d->x, oy + d->y);                         \
  }

float_t series_p_draw_data_get_x(const void* data) {
  return ((series_data_draw_normal_t*)data)->x;
}

float_t series_p_draw_data_get_y(const void* data) {
  return ((series_data_draw_normal_t*)data)->y;
}

int series_p_draw_data_compare_x(const void* a, const void* b) {
  return tk_roundi(((series_data_draw_normal_t*)a)->x) -
         tk_roundi(((series_data_draw_normal_t*)b)->x);
}

int series_p_draw_data_compare_y(const void* a, const void* b) {
  return tk_roundi((int)((series_data_draw_normal_t*)a)->y) -
         tk_roundi(((series_data_draw_normal_t*)b)->y);
}

void* series_p_draw_data_min_x(void* d, const void* data) {
  series_data_draw_normal_t* a = (series_data_draw_normal_t*)d;
  series_data_draw_normal_t* b = (series_data_draw_normal_t*)data;
  if (a->x > b->x) {
    a->x = b->x;
  }
  return a;
}

void* series_p_draw_data_min_y(void* d, const void* data) {
  series_data_draw_normal_t* a = (series_data_draw_normal_t*)d;
  series_data_draw_normal_t* b = (series_data_draw_normal_t*)data;
  if (a->y > b->y) {
    a->y = b->y;
  }
  return a;
}

void* series_p_draw_data_max_x(void* d, const void* data) {
  series_data_draw_normal_t* a = (series_data_draw_normal_t*)d;
  series_data_draw_normal_t* b = (series_data_draw_normal_t*)data;
  if (a->x < b->x) {
    a->x = b->x;
  }
  return a;
}

void* series_p_draw_data_max_y(void* d, const void* data) {
  series_data_draw_normal_t* a = (series_data_draw_normal_t*)d;
  series_data_draw_normal_t* b = (series_data_draw_normal_t*)data;
  if (a->y < b->y) {
    a->y = b->y;
  }
  return a;
}

ret_t series_p_draw_data_set_yx(void* dst, float_t series, object_t* value, uint32_t value_index,
                                float_t value_min, float_t value_range, float_t pixel_range,
                                bool_t inverse) {
  series_data_draw_normal_t* d = (series_data_draw_normal_t*)dst;
  float_t v = *((float_t*)(series_fifo_get(value, value_index)));
  d->x = (v - value_min) / value_range * pixel_range;
  d->x = inverse ? -d->x : d->x;
  d->y = series;

  return RET_OK;
}

ret_t series_p_draw_data_set_xy(void* dst, float_t series, object_t* value, uint32_t value_index,
                                float_t value_min, float_t value_range, float_t pixel_range,
                                bool_t inverse) {
  series_data_draw_normal_t* d = (series_data_draw_normal_t*)dst;
  float_t v = *((float_t*)(series_fifo_get(value, value_index)));
  d->y = (v - value_min) / value_range * pixel_range;
  d->y = inverse ? d->y : -d->y;
  d->x = series;
  return RET_OK;
}

float_t series_p_colorful_draw_data_get_x(const void* data) {
  return ((series_data_draw_colorful_t*)data)->x;
}

float_t series_p_colorful_draw_data_get_y(const void* data) {
  return ((series_data_draw_colorful_t*)data)->y;
}

int series_p_colorful_draw_data_compare_x(const void* a, const void* b) {
  return tk_roundi(((series_data_draw_colorful_t*)a)->x) -
         tk_roundi(((series_data_draw_colorful_t*)b)->x);
}

int series_p_colorful_draw_data_compare_y(const void* a, const void* b) {
  return tk_roundi(((series_data_draw_colorful_t*)a)->y) -
         tk_roundi(((series_data_draw_colorful_t*)b)->y);
}

void* series_p_colorful_draw_data_min_x(void* d, const void* data) {
  series_data_draw_colorful_t* a = (series_data_draw_colorful_t*)d;
  series_data_draw_colorful_t* b = (series_data_draw_colorful_t*)data;
  if (a->x > b->x) {
    a->x = b->x;
    a->c = b->c;
  }
  return a;
}

void* series_p_colorful_draw_data_min_y(void* d, const void* data) {
  series_data_draw_colorful_t* a = (series_data_draw_colorful_t*)d;
  series_data_draw_colorful_t* b = (series_data_draw_colorful_t*)data;
  if (a->y > b->y) {
    a->y = b->y;
    a->c = b->c;
  }
  return a;
}

void* series_p_colorful_draw_data_max_x(void* d, const void* data) {
  series_data_draw_colorful_t* a = (series_data_draw_colorful_t*)d;
  series_data_draw_colorful_t* b = (series_data_draw_colorful_t*)data;
  if (a->x < b->x) {
    a->x = b->x;
    a->c = b->c;
  }
  return a;
}

void* series_p_colorful_draw_data_max_y(void* d, const void* data) {
  series_data_draw_colorful_t* a = (series_data_draw_colorful_t*)d;
  series_data_draw_colorful_t* b = (series_data_draw_colorful_t*)data;
  if (a->y < b->y) {
    a->y = b->y;
    a->c = b->c;
  }
  return a;
}

ret_t series_p_colorful_draw_data_set_yx(void* dst, float_t series, object_t* value,
                                         uint32_t value_index, float_t value_min,
                                         float_t value_range, float_t pixel_range, bool_t inverse) {
  series_data_draw_colorful_t* d = (series_data_draw_colorful_t*)dst;
  series_data_colorful_t* v = (series_data_colorful_t*)(series_fifo_get(value, value_index));
  d->x = (v->v - value_min) / value_range * pixel_range;
  d->x = inverse ? -d->x : d->x;
  d->y = series;
  d->c = v->c;

  return RET_OK;
}

ret_t series_p_colorful_draw_data_set_xy(void* dst, float_t series, object_t* value,
                                         uint32_t value_index, float_t value_min,
                                         float_t value_range, float_t pixel_range, bool_t inverse) {
  series_data_draw_colorful_t* d = (series_data_draw_colorful_t*)dst;
  series_data_colorful_t* v = (series_data_colorful_t*)(series_fifo_get(value, value_index));
  d->y = (v->v - value_min) / value_range * pixel_range;
  d->y = inverse ? d->y : -d->y;
  d->x = series;
  d->c = v->c;

  return RET_OK;
}

float_t series_p_minmax_draw_data_get_x(const void* data) {
  series_data_draw_minmax_t* d = (series_data_draw_minmax_t*)data;
  return (d->xmin + d->xmax) / 2;
}

float_t series_p_minmax_draw_data_get_y(const void* data) {
  series_data_draw_minmax_t* d = (series_data_draw_minmax_t*)data;
  return (d->ymin + d->ymax) / 2;
}

int series_p_minmax_draw_data_compare_x(const void* a, const void* b) {
  series_data_draw_minmax_t* da = (series_data_draw_minmax_t*)a;
  series_data_draw_minmax_t* db = (series_data_draw_minmax_t*)b;
  return (da->xmin == db->xmin) ? (tk_roundi(da->xmax) - tk_roundi(db->xmax))
                                : (tk_roundi(da->xmin) - tk_roundi(db->xmin));
}

int series_p_minmax_draw_data_compare_y(const void* a, const void* b) {
  series_data_draw_minmax_t* da = (series_data_draw_minmax_t*)a;
  series_data_draw_minmax_t* db = (series_data_draw_minmax_t*)b;
  return (da->ymin == db->ymin) ? (tk_roundi(da->ymax) - tk_roundi(db->ymax))
                                : (tk_roundi(da->ymin) - tk_roundi(db->ymin));
}

void* series_p_minmax_draw_data_min_x(void* d, const void* data) {
  series_data_draw_minmax_t* a = (series_data_draw_minmax_t*)d;
  series_data_draw_minmax_t* b = (series_data_draw_minmax_t*)data;
  if (a->xmin > b->xmin) {
    a->xmin = b->xmin;
  }
  return a;
}

void* series_p_minmax_draw_data_min_y(void* d, const void* data) {
  series_data_draw_minmax_t* a = (series_data_draw_minmax_t*)d;
  series_data_draw_minmax_t* b = (series_data_draw_minmax_t*)data;
  if (a->ymin > b->ymin) {
    a->ymin = b->ymin;
  }
  return a;
}

void* series_p_minmax_draw_data_max_x(void* d, const void* data) {
  series_data_draw_minmax_t* a = (series_data_draw_minmax_t*)d;
  series_data_draw_minmax_t* b = (series_data_draw_minmax_t*)data;
  if (a->xmax < b->xmax) {
    a->xmax = b->xmax;
  }
  return a;
}

void* series_p_minmax_draw_data_max_y(void* d, const void* data) {
  series_data_draw_minmax_t* a = (series_data_draw_minmax_t*)d;
  series_data_draw_minmax_t* b = (series_data_draw_minmax_t*)data;
  if (a->ymax < b->ymax) {
    a->ymax = b->ymax;
  }
  return a;
}

ret_t series_p_minmax_draw_data_set_yx(void* dst, float_t series, object_t* value,
                                       uint32_t value_index, float_t value_min, float_t value_range,
                                       float_t pixel_range, bool_t inverse) {
  series_data_draw_minmax_t* d = (series_data_draw_minmax_t*)dst;
  series_data_minmax_t* v = (series_data_minmax_t*)(series_fifo_get(value, value_index));
  d->xmin = (v->min - value_min) / value_range * pixel_range;
  d->xmin = inverse ? -d->xmin : d->xmin;
  d->xmax = (v->max - value_min) / value_range * pixel_range;
  d->xmax = inverse ? -d->xmax : d->xmax;
  d->ymin = d->ymax = series;

  return RET_OK;
}

ret_t series_p_minmax_draw_data_set_xy(void* dst, float_t series, object_t* value,
                                       uint32_t value_index, float_t value_min, float_t value_range,
                                       float_t pixel_range, bool_t inverse) {
  series_data_draw_minmax_t* d = (series_data_draw_minmax_t*)dst;
  series_data_minmax_t* v = (series_data_minmax_t*)(series_fifo_get(value, value_index));
  d->ymin = (v->min - value_min) / value_range * pixel_range;
  d->ymin = inverse ? d->ymin : -d->ymin;
  d->ymax = (v->max - value_min) / value_range * pixel_range;
  d->ymax = inverse ? d->ymax : -d->ymax;
  d->xmin = d->xmax = series;
  return RET_OK;
}

ret_t series_p_draw_line(widget_t* widget, canvas_t* c, vgcanvas_t* vg, style_t* style, float_t ox,
                         float_t oy, object_t* fifo, uint32_t index, uint32_t size) {
  color_t trans = color_init(0, 0, 0, 0);
  color_t color;
  float_t border_width;
  int32_t i;
  uint32_t fifo_size;
  series_data_draw_normal_t* d = NULL;
  bool_t use_canvas =
      c->lcd->type != LCD_VGCANVAS;  // 非 OpenGL 下默认使用打点的形式画线，以提高效率
  assert(widget != NULL && style != NULL && fifo != NULL);

  fifo_size = SERIES_FIFO_GET_SIZE(fifo);
  assert(index < fifo_size && index + size - 1 < fifo_size);
  return_value_if_true(fifo_size == 0 || size == 0, RET_OK);

  color = style_get_color(style, STYLE_ID_SERIES_LINE_BORDER_COLOR, trans);
  border_width = (float_t)style_get_int(style, STYLE_ID_SERIES_LINE_BORDER_WIDTH, 1);

  if (color.rgba.a) {
    if (use_canvas && border_width == 1) {
      series_data_draw_normal_t* dprev = NULL;
      return_value_if_fail(c != NULL, RET_BAD_PARAMS);

      d = (series_data_draw_normal_t*)(series_fifo_get(fifo, index + size - 1));

      canvas_set_stroke_color(c, color);

      for (i = index + size - 1; i > (int32_t)index; i--) {
        dprev = (series_data_draw_normal_t*)(series_fifo_get(fifo, i - 1));
        _CANVAS_DRAW_LINE(c, ox + d->x, oy + d->y, ox + dprev->x, oy + dprev->y);
        d = dprev;
      }

      canvas_draw_vline(c, ox + d->x, oy + d->y, 1);
    } else {
      return_value_if_fail(vg != NULL, RET_BAD_PARAMS);
      d = (series_data_draw_normal_t*)(series_fifo_get(fifo, index + size - 1));

      vgcanvas_set_line_width(vg, border_width);
      vgcanvas_set_stroke_color(vg, color);
      vgcanvas_begin_path(vg);
      _VGCANVAS_MOVE_TO(vg, ox + d->x, oy + d->y);

      for (i = index + size - 1; i >= (int32_t)index; i--) {
        d = (series_data_draw_normal_t*)(series_fifo_get(fifo, i));
        _VGCANVAS_LINE_TO(vg, ox + d->x, oy + d->y);
      }

      vgcanvas_stroke(vg);
    }
  }

  return RET_OK;
}

ret_t series_p_draw_line_colorful(widget_t* widget, canvas_t* c, vgcanvas_t* vg, style_t* style,
                                  float_t ox, float_t oy, object_t* fifo, uint32_t index,
                                  uint32_t size) {
  float_t border_width;
  int32_t i;
  uint32_t fifo_size;
  series_data_draw_colorful_t* d = NULL;
  series_data_draw_colorful_t* dprev = NULL;
  bool_t use_canvas =
      c->lcd->type != LCD_VGCANVAS;  // 非 OpenGL 下默认使用打点的形式画线，以提高效率
  assert(widget != NULL && style != NULL && fifo != NULL);

  fifo_size = SERIES_FIFO_GET_SIZE(fifo);
  assert(index < fifo_size && index + size - 1 < fifo_size);
  return_value_if_true(fifo_size == 0 || size == 0, RET_OK);

  border_width = style_get_int(style, STYLE_ID_SERIES_LINE_BORDER_WIDTH, 1);

  if (use_canvas && border_width == 1) {
    return_value_if_fail(c != NULL, RET_BAD_PARAMS);

    d = (series_data_draw_colorful_t*)(series_fifo_get(fifo, index + size - 1));

    for (i = index + size - 1; i > (int32_t)index; i--) {
      dprev = (series_data_draw_colorful_t*)(series_fifo_get(fifo, i - 1));
      canvas_set_stroke_color(c, d->c);
      _CANVAS_DRAW_LINE(c, ox + d->x, oy + d->y, ox + dprev->x, oy + dprev->y);
      d = dprev;
    }

    canvas_set_stroke_color(c, d->c);
    canvas_draw_vline(c, ox + d->x, oy + d->y, 1);
  } else {
    return_value_if_fail(vg != NULL, RET_BAD_PARAMS);

    vgcanvas_set_line_width(vg, border_width);

    dprev = d = (series_data_draw_colorful_t*)(series_fifo_get(fifo, index + size - 1));
    if (d->c.rgba.a) {
      vgcanvas_set_stroke_color(vg, d->c);
      vgcanvas_begin_path(vg);
      _VGCANVAS_MOVE_TO(vg, ox + d->x, oy + d->y);
    }

    for (i = index + size - 2; i >= (int32_t)index; i--) {
      d = (series_data_draw_colorful_t*)(series_fifo_get(fifo, i));

      if (dprev->c.rgba.a) {
        _VGCANVAS_LINE_TO(vg, ox + d->x, oy + d->y);
      }

      if (d->c.color != dprev->c.color) {
        if (dprev->c.rgba.a) {
          vgcanvas_stroke(vg);
        }

        if (d->c.rgba.a) {
          vgcanvas_set_stroke_color(vg, d->c);
          vgcanvas_begin_path(vg);
          _VGCANVAS_MOVE_TO(vg, ox + d->x, oy + d->y);
        }
      }

      dprev = d;
    }

    if (d->c.rgba.a) {
      vgcanvas_stroke(vg);
    }
  }

  return RET_OK;
}

ret_t series_p_draw_line_area(widget_t* widget, vgcanvas_t* vg, style_t* style, float_t ox,
                              float_t oy, object_t* fifo, uint32_t index, uint32_t size,
                              bool_t vertical) {
  color_t trans = color_init(0, 0, 0, 0);
  color_t color;
  int32_t i;
  uint32_t fifo_size;
  series_data_draw_normal_t* d = NULL;
  series_data_draw_normal_t* d0 = NULL;
  assert(widget != NULL && style != NULL && fifo != NULL);

  fifo_size = SERIES_FIFO_GET_SIZE(fifo);
  assert(index < fifo_size && index + size - 1 < fifo_size);
  return_value_if_fail(vg != NULL, RET_BAD_PARAMS);
  return_value_if_true(fifo_size == 0 || size == 0, RET_OK);

  color = style_get_color(style, STYLE_ID_SERIES_AREA_COLOR, trans);

  if (color.rgba.a) {
    d0 = d = (series_data_draw_normal_t*)(series_fifo_get(fifo, index + size - 1));
    vgcanvas_set_fill_color(vg, color);
    vgcanvas_begin_path(vg);
    _VGCANVAS_MOVE_TO(vg, ox + d->x, oy + d->y);

    for (i = index + size - 1; i >= (int32_t)index; i--) {
      d = (series_data_draw_normal_t*)(series_fifo_get(fifo, i));
      _VGCANVAS_LINE_TO(vg, ox + d->x, oy + d->y);
    }

    if (vertical) {
      _VGCANVAS_LINE_TO(vg, ox + d->x, oy);
      _VGCANVAS_LINE_TO(vg, ox + d0->x, oy);
    } else {
      _VGCANVAS_LINE_TO(vg, ox, oy + d->y);
      _VGCANVAS_LINE_TO(vg, ox, oy + d0->y);
    }
    vgcanvas_fill(vg);
  }

  return RET_OK;
}

ret_t series_p_draw_line_area_colorful(widget_t* widget, vgcanvas_t* vg, style_t* style, float_t ox,
                                       float_t oy, object_t* fifo, uint32_t index, uint32_t size,
                                       bool_t vertical) {
  int32_t i;
  uint32_t fifo_size;
  series_data_draw_colorful_t* d = NULL;
  series_data_draw_colorful_t* d0 = NULL;
  series_data_draw_colorful_t* dprev = NULL;
  assert(widget != NULL && style != NULL && fifo != NULL);

  fifo_size = SERIES_FIFO_GET_SIZE(fifo);
  assert(index < fifo_size && index + size - 1 < fifo_size);
  return_value_if_fail(vg != NULL, RET_BAD_PARAMS);
  return_value_if_true(fifo_size == 0 || size == 0, RET_OK);

  dprev = d0 = d = (series_data_draw_colorful_t*)(series_fifo_get(fifo, index + size - 1));
  if (d->c.rgba.a) {
    vgcanvas_set_fill_color(vg, d->c);
    vgcanvas_begin_path(vg);
    _VGCANVAS_MOVE_TO(vg, ox + d->x, oy + d->y);
  }

  for (i = index + size - 2; i >= (int32_t)index; i--) {
    d = (series_data_draw_colorful_t*)(series_fifo_get(fifo, i));

    if (dprev->c.rgba.a) {
      _VGCANVAS_LINE_TO(vg, ox + d->x, oy + d->y);
    }

    if (d->c.color != dprev->c.color) {
      if (dprev->c.rgba.a) {
        if (vertical) {
          _VGCANVAS_LINE_TO(vg, ox + d->x, oy);
          _VGCANVAS_LINE_TO(vg, ox + d0->x, oy);
        } else {
          _VGCANVAS_LINE_TO(vg, ox, oy + d->y);
          _VGCANVAS_LINE_TO(vg, ox, oy + d0->y);
        }
        vgcanvas_fill(vg);
      }

      d0 = d;

      if (d->c.rgba.a) {
        vgcanvas_set_fill_color(vg, d->c);
        vgcanvas_begin_path(vg);
        _VGCANVAS_MOVE_TO(vg, ox + d->x, oy + d->y);
      }
    }

    dprev = d;
  }

  if (d->c.rgba.a) {
    if (vertical) {
      _VGCANVAS_LINE_TO(vg, ox + d->x, oy);
      _VGCANVAS_LINE_TO(vg, ox + d0->x, oy);
    } else {
      _VGCANVAS_LINE_TO(vg, ox, oy + d->y);
      _VGCANVAS_LINE_TO(vg, ox, oy + d0->y);
    }
    vgcanvas_fill(vg);
  }

  return RET_OK;
}

ret_t series_p_draw_smooth_line(widget_t* widget, vgcanvas_t* vg, style_t* style, float_t ox,
                                float_t oy, object_t* fifo, uint32_t index, uint32_t size,
                                bool_t vertical) {
  color_t trans = color_init(0, 0, 0, 0);
  color_t color;
  float_t border_width;
  int32_t i;
  uint32_t fifo_size;
  series_data_draw_normal_t* d = NULL;
  series_data_draw_normal_t* dprev = NULL;
  assert(widget != NULL && style != NULL && fifo != NULL);

  fifo_size = SERIES_FIFO_GET_SIZE(fifo);
  assert(index < fifo_size && index + size - 1 < fifo_size);
  return_value_if_fail(vg != NULL, RET_BAD_PARAMS);
  return_value_if_true(fifo_size == 0 || size == 0, RET_OK);

  color = style_get_color(style, STYLE_ID_SERIES_LINE_BORDER_COLOR, trans);
  border_width = style_get_int(style, STYLE_ID_SERIES_LINE_BORDER_WIDTH, 1);

  if (color.rgba.a) {
    dprev = d = (series_data_draw_normal_t*)(series_fifo_get(fifo, index + size - 1));

    vgcanvas_set_line_width(vg, border_width);
    vgcanvas_set_stroke_color(vg, color);
    vgcanvas_begin_path(vg);
    _VGCANVAS_MOVE_TO(vg, ox + d->x, oy + d->y);

    if (size == 2) {
      d = (series_data_draw_normal_t*)(series_fifo_get(fifo, index));
      _VGCANVAS_LINE_TO(vg, ox + d->x, oy + d->y);
    } else {
      for (i = index + size - 2; i >= (int32_t)index; i--) {
        d = (series_data_draw_normal_t*)(series_fifo_get(fifo, i));
        _VACANVAS_BEZIER_FIFO_ONE(i, index, size, fifo, d, dprev, ox, oy,
                                  series_data_draw_normal_t);
        dprev = d;
      }
    }

    vgcanvas_stroke(vg);
  }

  return RET_OK;
}

ret_t series_p_draw_smooth_line_colorful(widget_t* widget, vgcanvas_t* vg, style_t* style,
                                         float_t ox, float_t oy, object_t* fifo, uint32_t index,
                                         uint32_t size, bool_t vertical) {
  float_t border_width;
  int32_t i;
  uint32_t fifo_size;
  series_data_draw_colorful_t* d = NULL;
  series_data_draw_colorful_t* dprev = NULL;
  assert(widget != NULL && style != NULL && fifo != NULL);

  fifo_size = SERIES_FIFO_GET_SIZE(fifo);
  assert(index < fifo_size && index + size - 1 < fifo_size);
  return_value_if_fail(vg != NULL, RET_BAD_PARAMS);
  return_value_if_true(fifo_size == 0 || size == 0, RET_OK);

  border_width = style_get_int(style, STYLE_ID_SERIES_LINE_BORDER_WIDTH, 1);

  vgcanvas_set_line_width(vg, border_width);

  dprev = d = (series_data_draw_colorful_t*)(series_fifo_get(fifo, index + size - 1));
  if (d->c.rgba.a) {
    vgcanvas_set_stroke_color(vg, d->c);
    vgcanvas_begin_path(vg);
    _VGCANVAS_MOVE_TO(vg, ox + d->x, oy + d->y);
  }

  if (size == 2) {
    d = (series_data_draw_colorful_t*)(series_fifo_get(fifo, index));

    if (dprev->c.rgba.a) {
      _VGCANVAS_LINE_TO(vg, ox + d->x, oy + d->y);
    }

    if (d->c.color != dprev->c.color) {
      if (dprev->c.rgba.a) {
        vgcanvas_stroke(vg);
      }

      if (d->c.rgba.a) {
        vgcanvas_set_stroke_color(vg, d->c);
        vgcanvas_begin_path(vg);
        _VGCANVAS_MOVE_TO(vg, ox + d->x, oy + d->y);
      }
    }
  } else {
    for (i = index + size - 2; i >= (int32_t)index; i--) {
      d = (series_data_draw_colorful_t*)(series_fifo_get(fifo, i));

      if (dprev->c.rgba.a) {
        _VACANVAS_BEZIER_FIFO_ONE(i, index, size, fifo, d, dprev, ox, oy,
                                  series_data_draw_colorful_t);
      }

      if (d->c.color != dprev->c.color) {
        if (dprev->c.rgba.a) {
          vgcanvas_stroke(vg);
        }

        if (d->c.rgba.a) {
          vgcanvas_set_stroke_color(vg, d->c);
          vgcanvas_begin_path(vg);
          _VGCANVAS_MOVE_TO(vg, ox + d->x, oy + d->y);
        }
      }

      dprev = d;
    }
  }

  if (d->c.rgba.a) {
    vgcanvas_stroke(vg);
  }

  return RET_OK;
}

ret_t series_p_draw_smooth_line_area(widget_t* widget, vgcanvas_t* vg, style_t* style, float_t ox,
                                     float_t oy, object_t* fifo, uint32_t index, uint32_t size,
                                     bool_t vertical) {
  color_t trans = color_init(0, 0, 0, 0);
  color_t color;
  int32_t i;
  uint32_t fifo_size;
  series_data_draw_normal_t* d = NULL;
  series_data_draw_normal_t* d0 = NULL;
  series_data_draw_normal_t* dprev = NULL;
  assert(widget != NULL && style != NULL && fifo != NULL);

  fifo_size = SERIES_FIFO_GET_SIZE(fifo);
  assert(index < fifo_size && index + size - 1 < fifo_size);
  return_value_if_fail(vg != NULL, RET_BAD_PARAMS);
  return_value_if_true(fifo_size == 0 || size == 0, RET_OK);

  color = style_get_color(style, STYLE_ID_SERIES_AREA_COLOR, trans);

  if (color.rgba.a) {
    dprev = d0 = d = (series_data_draw_normal_t*)(series_fifo_get(fifo, index + size - 1));
    vgcanvas_set_fill_color(vg, color);
    vgcanvas_begin_path(vg);
    _VGCANVAS_MOVE_TO(vg, ox + d->x, oy + d->y);

    if (size == 2) {
      d = (series_data_draw_normal_t*)(series_fifo_get(fifo, index));
      _VGCANVAS_LINE_TO(vg, ox + d->x, oy + d->y);
    } else {
      for (i = index + size - 2; i >= (int32_t)index; i--) {
        d = (series_data_draw_normal_t*)(series_fifo_get(fifo, i));
        _VACANVAS_BEZIER_FIFO_ONE(i, index, size, fifo, d, dprev, ox, oy,
                                  series_data_draw_normal_t);
        dprev = d;
      }
    }

    if (vertical) {
      _VGCANVAS_LINE_TO(vg, ox + d->x, oy);
      _VGCANVAS_LINE_TO(vg, ox + d0->x, oy);
    } else {
      _VGCANVAS_LINE_TO(vg, ox, oy + d->y);
      _VGCANVAS_LINE_TO(vg, ox, oy + d0->y);
    }
    vgcanvas_fill(vg);
  }

  return RET_OK;
}

ret_t series_p_draw_smooth_line_area_colorful(widget_t* widget, vgcanvas_t* vg, style_t* style,
                                              float_t ox, float_t oy, object_t* fifo,
                                              uint32_t index, uint32_t size, bool_t vertical) {
  int32_t i;
  uint32_t fifo_size;
  series_data_draw_colorful_t* d = NULL;
  series_data_draw_colorful_t* d0 = NULL;
  series_data_draw_colorful_t* dprev = NULL;
  assert(widget != NULL && style != NULL && fifo != NULL);

  fifo_size = SERIES_FIFO_GET_SIZE(fifo);
  assert(index < fifo_size && index + size - 1 < fifo_size);
  return_value_if_fail(vg != NULL, RET_BAD_PARAMS);
  return_value_if_true(fifo_size == 0 || size == 0, RET_OK);

  dprev = d0 = d = (series_data_draw_colorful_t*)(series_fifo_get(fifo, index + size - 1));
  if (d->c.rgba.a) {
    vgcanvas_set_fill_color(vg, d->c);
    vgcanvas_begin_path(vg);
    _VGCANVAS_MOVE_TO(vg, ox + d->x, oy + d->y);
  }

  if (size == 2) {
    d = (series_data_draw_colorful_t*)(series_fifo_get(fifo, index));

    if (dprev->c.rgba.a) {
      _VGCANVAS_LINE_TO(vg, ox + d->x, oy + d->y);
    }

    if (d->c.color != dprev->c.color) {
      if (dprev->c.rgba.a) {
        if (vertical) {
          _VGCANVAS_LINE_TO(vg, ox + d->x, oy);
          _VGCANVAS_LINE_TO(vg, ox + d0->x, oy);
        } else {
          _VGCANVAS_LINE_TO(vg, ox, oy + d->y);
          _VGCANVAS_LINE_TO(vg, ox, oy + d0->y);
        }
        vgcanvas_fill(vg);
      }

      d0 = d;

      if (d->c.rgba.a) {
        vgcanvas_set_fill_color(vg, d->c);
        vgcanvas_begin_path(vg);
        _VGCANVAS_MOVE_TO(vg, ox + d->x, oy + d->y);
      }
    }
  } else {
    for (i = index + size - 2; i >= (int32_t)index; i--) {
      d = (series_data_draw_colorful_t*)(series_fifo_get(fifo, i));

      if (dprev->c.rgba.a) {
        _VACANVAS_BEZIER_FIFO_ONE(i, index, size, fifo, d, dprev, ox, oy,
                                  series_data_draw_colorful_t);
      }

      if (d->c.color != dprev->c.color) {
        if (dprev->c.rgba.a) {
          if (vertical) {
            _VGCANVAS_LINE_TO(vg, ox + d->x, oy);
            _VGCANVAS_LINE_TO(vg, ox + d0->x, oy);
          } else {
            _VGCANVAS_LINE_TO(vg, ox, oy + d->y);
            _VGCANVAS_LINE_TO(vg, ox, oy + d0->y);
          }
          vgcanvas_fill(vg);
        }

        d0 = d;

        if (d->c.rgba.a) {
          vgcanvas_set_fill_color(vg, d->c);
          vgcanvas_begin_path(vg);
          _VGCANVAS_MOVE_TO(vg, ox + d->x, oy + d->y);
        }
      }

      dprev = d;
    }
  }

  if (d->c.rgba.a) {
    if (vertical) {
      _VGCANVAS_LINE_TO(vg, ox + d->x, oy);
      _VGCANVAS_LINE_TO(vg, ox + d0->x, oy);
    } else {
      _VGCANVAS_LINE_TO(vg, ox, oy + d->y);
      _VGCANVAS_LINE_TO(vg, ox, oy + d0->y);
    }
    vgcanvas_fill(vg);
  }

  return RET_OK;
}

ret_t series_p_draw_symbol(widget_t* widget, canvas_t* c, style_t* style, float_t ox, float_t oy,
                           object_t* fifo, uint32_t index, uint32_t size, float_t symbol_size) {
  rect_t r;
  color_t trans = color_init(0, 0, 0, 0);
  color_t bg_color, bd_color;
  float_t border_width;
  uint32_t radius;
  bitmap_t img;
  bitmap_t* pimg = &img;
  const char* image_name;
  image_draw_type_t draw_type;
  int32_t i;
  uint32_t fifo_size;
  series_data_draw_normal_t* d = NULL;
  assert(widget != NULL && c != NULL && style != NULL && fifo != NULL);

  fifo_size = SERIES_FIFO_GET_SIZE(fifo);
  assert(index < fifo_size && index + size - 1 < fifo_size);
  return_value_if_true(fifo_size == 0 || size == 0 || symbol_size == 0, RET_OK);

  bg_color = style_get_color(style, STYLE_ID_SERIES_SYMBOL_BG_COLOR, trans);
  bd_color = style_get_color(style, STYLE_ID_SERIES_SYMBOL_BORDER_COLOR, trans);
  border_width = style_get_int(style, STYLE_ID_SERIES_SYMBOL_BORDER_WIDTH, 1);
  radius = style_get_int(style, STYLE_ID_SERIES_SYMBOL_ROUND_RADIUS, 0);
  image_name = style_get_str(style, STYLE_ID_SERIES_SYMBOL_BG_IMAGE, NULL);
  draw_type = (image_draw_type_t)style_get_int(style, STYLE_ID_SERIES_SYMBOL_BG_IMAGE_DRAW_TYPE,
                                               IMAGE_DRAW_CENTER);

  if (image_name == NULL || image_name[0] == '\0' ||
      widget_load_image(widget, image_name, pimg) != RET_OK) {
    pimg = NULL;
  }

  if (bg_color.rgba.a || bd_color.rgba.a || pimg != NULL) {
    for (i = index; i < index + size; i++) {
      d = (series_data_draw_normal_t*)(series_fifo_get(fifo, i));
      r = rect_init(ox + d->x - symbol_size, oy + d->y - symbol_size, symbol_size * 2 + 1,
                    symbol_size * 2 + 1);

      chart_utils_draw_a_symbol(c, &r, bg_color, bd_color, border_width, radius, pimg, draw_type);
    }
  }

  return RET_OK;
}

ret_t series_p_draw_symbol_colorful(widget_t* widget, canvas_t* c, style_t* style, float_t ox,
                                    float_t oy, object_t* fifo, uint32_t index, uint32_t size,
                                    float_t symbol_size) {
  rect_t r;
  color_t trans = color_init(0, 0, 0, 0);
  color_t bd_color;
  float_t border_width;
  uint32_t radius;
  bitmap_t img;
  bitmap_t* pimg = &img;
  const char* image_name;
  image_draw_type_t draw_type;
  int32_t i;
  uint32_t fifo_size;
  series_data_draw_colorful_t* d = NULL;
  assert(widget != NULL && c != NULL && style != NULL && fifo != NULL);

  fifo_size = SERIES_FIFO_GET_SIZE(fifo);
  assert(index < fifo_size && index + size - 1 < fifo_size);
  return_value_if_true(fifo_size == 0 || size == 0 || symbol_size == 0, RET_OK);

  bd_color = style_get_color(style, STYLE_ID_SERIES_SYMBOL_BORDER_COLOR, trans);
  border_width = style_get_int(style, STYLE_ID_SERIES_SYMBOL_BORDER_WIDTH, 1);
  radius = style_get_int(style, STYLE_ID_SERIES_SYMBOL_ROUND_RADIUS, 0);
  image_name = style_get_str(style, STYLE_ID_SERIES_SYMBOL_BG_IMAGE, NULL);
  draw_type = (image_draw_type_t)style_get_int(style, STYLE_ID_SERIES_SYMBOL_BG_IMAGE_DRAW_TYPE,
                                               IMAGE_DRAW_CENTER);

  if (image_name == NULL || image_name[0] == '\0' ||
      widget_load_image(widget, image_name, &img) != RET_OK) {
    pimg = NULL;
  }

  for (i = index; i < index + size; i++) {
    d = (series_data_draw_colorful_t*)(series_fifo_get(fifo, i));
    r = rect_init(ox + d->x - symbol_size, oy + d->y - symbol_size, symbol_size * 2 + 1,
                  symbol_size * 2 + 1);

    chart_utils_draw_a_symbol(c, &r, d->c, bd_color, border_width, radius, pimg, draw_type);
  }

  return RET_OK;
}

static ret_t series_data_draw_normal_to_rect(void* data, float_t ox, float_t oy, float_t bar_width,
                                             bool_t vertical, rect_t* r) {
  series_data_draw_normal_t* d = (series_data_draw_normal_t*)(data);
  assert(d != NULL && r != NULL);
  if (vertical) {
    xy_t y = oy;
    wh_t h = d->y + (d->y >= 0 ? 1 : -1);
    *r = rect_init(ox + d->x, y, bar_width, h);
  } else {
    xy_t x = ox;
    wh_t w = d->x + (d->x >= 0 ? 1 : -1);
    *r = rect_init(x, oy + d->y, w, bar_width);
  }
  return RET_OK;
}

static ret_t series_data_draw_minmax_to_rect(void* data, float_t ox, float_t oy, float_t bar_width,
                                             bool_t vertical, rect_t* r) {
  series_data_draw_minmax_t* d = (series_data_draw_minmax_t*)(data);
  assert(d != NULL && r != NULL);
  if (vertical) {
    wh_t h = d->ymax >= d->ymin ? (d->ymax - d->ymin + 1) : -(d->ymin - d->ymax + 1);
    *r = rect_init(ox + d->xmin, oy + d->ymin, bar_width, h);
  } else {
    wh_t w = d->xmax >= d->xmin ? (d->xmax - d->xmin + 1) : -(d->xmin - d->xmax + 1);
    *r = rect_init(ox + d->xmin, oy + d->ymin, w, bar_width);
  }
  return RET_OK;
}

typedef ret_t (*series_data_draw_normal_to_rect_t)(void* data, float_t ox, float_t oy,
                                                   float_t bar_width, bool_t vertical, rect_t* r);

ret_t series_p_draw_bar(widget_t* widget, canvas_t* c, vgcanvas_t* vg, style_t* style, float_t ox,
                        float_t oy, object_t* fifo, uint32_t index, uint32_t size,
                        float_t bar_width, bool_t vertical, bool_t minmax) {
  color_t trans = color_init(0, 0, 0, 0);
  color_t fg_color, bd_color;
  uint32_t radius;
  float_t border_width;
  bitmap_t img;
  const char* image_name;
  image_draw_type_t draw_type;
  rect_t r;
  uint32_t i;
  uint32_t fifo_size;
  series_data_draw_normal_to_rect_t draw_data_to_rect =
      minmax ? series_data_draw_minmax_to_rect : series_data_draw_normal_to_rect;
  assert(widget != NULL && c != NULL && style != NULL && fifo != NULL);

  fifo_size = SERIES_FIFO_GET_SIZE(fifo);
  assert(index < fifo_size && index + size - 1 < fifo_size);
  return_value_if_true(fifo_size == 0 || size == 0, RET_OK);

  fg_color = style_get_color(style, STYLE_ID_FG_COLOR, trans);
  bd_color = style_get_color(style, STYLE_ID_BORDER_COLOR, trans);
  border_width = style_get_int(style, STYLE_ID_BORDER_WIDTH, 1);
  radius = style_get_int(style, STYLE_ID_ROUND_RADIUS, 0);
  image_name = style_get_str(style, STYLE_ID_FG_IMAGE, NULL);
  draw_type = vertical ? IMAGE_DRAW_PATCH3_Y : IMAGE_DRAW_PATCH3_X;
  draw_type = (image_draw_type_t)style_get_int(style, STYLE_ID_FG_IMAGE_DRAW_TYPE, draw_type);

  if (image_name != NULL) {
    if (widget_load_image(widget, image_name, &img) != RET_OK) {
      image_name = NULL;
    }
  }

  if (fg_color.rgba.a || bd_color.rgba.a || image_name != NULL) {
    bool_t use_vg = vg != NULL && ((radius > 3 || border_width > 1));

    canvas_set_fill_color(c, fg_color);
    canvas_set_stroke_color(c, bd_color);

    if (use_vg) {
      vgcanvas_set_fill_color(vg, fg_color);
      vgcanvas_set_stroke_color(vg, bd_color);
      vgcanvas_set_line_width(vg, border_width);
    }

    for (i = index; i < index + size; i++) {
      draw_data_to_rect(series_fifo_get(fifo, i), ox, oy, bar_width, vertical, &r);

      if (use_vg) {
        vgcanvas_translate(vg, c->ox, c->oy);
        vgcanvas_begin_path(vg);
        if (radius > 3) {
          _VGCANVAS_ROUNDED_RECT(vg, r.x, r.y, r.w, r.h, radius);
        } else {
          _VGCANVAS_RECT(vg, r.x, r.y, r.w, r.h);
        }

        if (fg_color.rgba.a) {
          vgcanvas_fill(vg);
        }

        if (bd_color.rgba.a) {
          vgcanvas_stroke(vg);
        }
        vgcanvas_translate(vg, -c->ox, -c->oy);
      } else {
        if (fg_color.rgba.a) {
          canvas_fill_rect(c, r.x, r.y, r.w, r.h);
        }

        if (bd_color.rgba.a) {
          canvas_stroke_rect(c, r.x, r.y, r.w, r.h);
        }
      }

      if (image_name != NULL) {
        canvas_draw_image_ex(c, &img, draw_type, &r);
      }
    }
  }

  return RET_OK;
}

static ret_t series_p_on_fifo_will_set(void* ctx, event_t* e) {
  widget_t* widget = WIDGET(ctx);
  series_t* series = SERIES(widget);
  widget_t* axis = widget_get_prop_pointer(widget, SERIES_PROP_SERIES_AXIS_OBJ);

  series_fifo_set_event_t* evt = series_fifo_set_event_cast(e);
  uint32_t index = evt->index;
  uint32_t nr = evt->nr;
  const void* data = evt->data;
  object_t* fifo = series->fifo;

  if (axis == NULL || series->value_animation == 0 || series->animator_create == NULL) {
    widget_invalidate(widget, NULL);
    return RET_OK;
  } else {
    widget_animator_t* wa = NULL;
    uint32_t range = (uint32_t)(axis_get_range(axis, TRUE));
    return_value_if_fail(range != 0, RET_BAD_PARAMS);

    widget_destroy_animator(widget, NULL);

    if (index + nr > SERIES_FIFO_GET_SIZE(fifo)) {
      series->new_period = (series->new_period + index + nr - SERIES_FIFO_GET_SIZE(fifo)) % range;
    }
    series->clip_range = 0;

    wa = series->animator_create(widget, series->value_animation, 0, SERIES_ANIMATION_EASING);
    assert(wa != NULL);

    chart_animator_fifo_value_set_params(wa, fifo, index, data, nr, range);

    if (series->inited) {
      widget_animator_start(wa);
    }

    return RET_STOP;
  }
}

static ret_t series_p_on_fifo_push(void* ctx, event_t* e) {
  object_t* fifo = OBJECT(e->target);
  widget_t* widget = WIDGET(ctx);
  widget_t* axis = widget_get_prop_pointer(widget, SERIES_PROP_SERIES_AXIS_OBJ);
  series_fifo_push_event_t* evt = series_fifo_push_event_cast(e);

  series_t* series = SERIES(widget);
  uint32_t capacity = SERIES_FIFO_GET_CAPACITY(fifo);
  uint32_t nr = evt->nr;

  if (axis != NULL) {
    uint32_t range = (uint32_t)(axis_get_range(axis, TRUE));
    return_value_if_fail(range != 0, RET_BAD_PARAMS);

    series->new_period = (series->new_period + nr) % range;
    if (series->new_period == 0) {
      series->new_period = range;
    }

    if (AXIS(axis)->data_from_series != NULL) {
      AXIS(axis)->need_update_data = TRUE;
    }

    if (series->value_animation != 0 && SERIES_FIFO_GET_SIZE(fifo) > 1) {
      widget_animator_t* wa = NULL;

      widget_destroy_animator(widget, NULL);

      series->clip_range = tk_min(series->new_period, tk_min(range, tk_min(nr, capacity)));

      wa = widget_animator_prop_create(widget, series->value_animation, 100,
                                       SERIES_ANIMATION_EASING, SERIES_PROP_CLIP_RANGE);
      assert(wa != NULL);
      widget_animator_prop_set_params(wa, series->clip_range, 0);

      if (series->inited) {
        widget_animator_start(wa);
      }

      // cover类型的最大offset与clip_range有关，故更新防止溢出
      if (series->display_mode == SERIES_DISPLAY_COVER) {
        uint32_t offset_max = series_p_get_offset_max(widget);
        if (series->offset > offset_max) {
          series->offset = offset_max;
        }
      }
    }
  }

  widget_invalidate(widget, NULL);

  return RET_OK;
}

static ret_t series_p_on_fifo_pop(void* ctx, event_t* e) {
  widget_t* widget = WIDGET(ctx);
  widget_t* axis = widget_get_prop_pointer(widget, SERIES_PROP_SERIES_AXIS_OBJ);

  return widget_invalidate(widget, NULL);
}

static ret_t series_p_fifo_emitter_on(widget_t* widget) {
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && series->fifo != NULL, RET_BAD_PARAMS);

  emitter_on(EMITTER(series->fifo), EVT_SERIES_FIFO_WILL_SET, series_p_on_fifo_will_set, widget);
  emitter_on(EMITTER(series->fifo), EVT_SERIES_FIFO_PUSH, series_p_on_fifo_push, widget);
  emitter_on(EMITTER(series->fifo), EVT_SERIES_FIFO_POP, series_p_on_fifo_pop, widget);

  return RET_OK;
}

static ret_t series_p_fifo_emitter_off(widget_t* widget) {
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && series->fifo != NULL, RET_BAD_PARAMS);

  emitter_off_by_func(EMITTER(series->fifo), EVT_SERIES_FIFO_WILL_SET, series_p_on_fifo_will_set,
                      widget);
  emitter_off_by_func(EMITTER(series->fifo), EVT_SERIES_FIFO_PUSH, series_p_on_fifo_push, widget);
  emitter_off_by_func(EMITTER(series->fifo), EVT_SERIES_FIFO_POP, series_p_on_fifo_pop, widget);

  return RET_OK;
}

static ret_t series_p_fifo_deinit(widget_t* widget) {
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL, RET_BAD_PARAMS);

  if (series->fifo != NULL) {
    series_p_fifo_emitter_off(widget);
    OBJECT_UNREF(series->fifo);
    series->fifo = NULL;
  }

  return RET_OK;
}

ret_t series_p_set_fifo(widget_t* widget, object_t* fifo) {
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && fifo != NULL, RET_BAD_PARAMS);
  return_value_if_fail(tk_str_start_with(object_get_type(fifo), "series_fifo"), RET_BAD_PARAMS);

  if (series->fifo != fifo) {
    widget_destroy_animator(widget, NULL);
    series_p_fifo_deinit(widget);
  }

  if (series->fifo == NULL) {
    series->fifo = fifo;
    series_p_fifo_emitter_on(widget);
  }

  return RET_OK;
}

ret_t series_p_set_capacity(widget_t* widget, uint32_t capacity) {
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && series->fifo != NULL && capacity > 0, RET_BAD_PARAMS);

  return series_fifo_set_capacity(series->fifo, capacity);
}

uint32_t series_p_count(widget_t* widget) {
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && series->fifo != NULL, 0);

  return SERIES_FIFO_GET_SIZE(series->fifo);
}

ret_t series_p_set(widget_t* widget, uint32_t index, const void* data, uint32_t nr) {
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && series->fifo != NULL && data != NULL && nr > 0,
                       RET_BAD_PARAMS);

  return series_fifo_set(series->fifo, index, data, nr);
}

ret_t series_p_rset(widget_t* widget, uint32_t index, const void* data, uint32_t nr) {
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && series->fifo != NULL && data != NULL, RET_BAD_PARAMS);

  return series_fifo_set_reverse(series->fifo, index, data, nr);
}

ret_t series_p_push(widget_t* widget, const void* data, uint32_t nr) {
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && series->fifo != NULL && data != NULL && nr > 0,
                       RET_BAD_PARAMS);

  return series_fifo_npush(series->fifo, data, nr);
}

ret_t series_p_clear(widget_t* widget) {
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && series->fifo != NULL, RET_BAD_PARAMS);

  return series_fifo_clear(series->fifo);
}

ret_t series_p_pop(widget_t* widget, uint32_t nr) {
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && series->fifo != NULL, RET_BAD_PARAMS);

  uint32_t size = SERIES_FIFO_GET_SIZE(series->fifo);
  return series_fifo_npop(series->fifo, nr == 0 ? size : nr);
}

void* series_p_at(widget_t* widget, uint32_t index) {
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && series->fifo != NULL, NULL);

  return series_fifo_get(series->fifo, index);
}

ret_t series_p_get_current(widget_t* widget, uint32_t* begin, uint32_t* end, uint32_t* middle) {
  object_t* fifo;
  uint32_t size;
  uint32_t b, e, m;
  uint32_t srange;
  widget_t* saxis;
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL, RET_BAD_PARAMS);

  fifo = series->fifo;
  return_value_if_fail(fifo != NULL, RET_BAD_PARAMS);

  size = SERIES_FIFO_GET_SIZE(fifo);
  return_value_if_fail(size > 0, RET_BAD_PARAMS);

  saxis = widget_get_prop_pointer(widget, SERIES_PROP_SERIES_AXIS_OBJ);
  return_value_if_fail(saxis != NULL, RET_BAD_PARAMS);

  srange = axis_get_range(saxis, TRUE);

  if (series->display_mode == SERIES_DISPLAY_COVER) {
    e = size - series->offset - 1;
    m = size - series->offset - series->new_period;
    b = size <= srange ? m : (e - srange + 1);
  } else {
    e = size - series->offset - 1;
    m = e - tk_min(size, srange) + 1;
    b = m;
  }

  begin != NULL ? (*begin = b) : 0;
  end != NULL ? (*end = e) : 0;
  middle != NULL ? (*middle = m) : 0;

  return RET_OK;
}

bool_t series_p_is_point_in(widget_t* widget, xy_t x, xy_t y, bool_t is_local) {
  point_t p = {x, y};
  rect_t r;
  float_t sinterval, srange;
  uint32_t size;
  axis_t* saxis;
  axis_t* vaxis;
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && series->fifo != NULL, FALSE);

  size = SERIES_FIFO_GET_SIZE(series->fifo);
  saxis = AXIS(widget_get_prop_pointer(widget, SERIES_PROP_SERIES_AXIS_OBJ));
  vaxis = AXIS(widget_get_prop_pointer(widget, SERIES_PROP_VALUE_AXIS_OBJ));
  return_value_if_fail(saxis != NULL && vaxis != NULL, FALSE);

  if (!is_local) {
    widget_to_local(widget, &p);
  }

  sinterval = axis_measure_series_interval(WIDGET(saxis));
  srange = axis_get_range(WIDGET(saxis), TRUE);

  if (tk_str_eq(widget_get_type(WIDGET(saxis)), WIDGET_TYPE_X_AXIS)) {
    if (saxis->inverse) {
      sinterval = -sinterval;
      r.x = saxis->draw_rect.x + saxis->draw_rect.w - 1 - sinterval / 2;
      r.w = sinterval * (tk_min(srange, size) + 1) + 1;
    } else {
      r.x = saxis->draw_rect.x - sinterval / 2;
      r.w = sinterval * (tk_min(srange, size) + 1) - 1;
    }

    if (r.w < 0) {
      r.w = -r.w;
      r.x = r.x - r.w + 1;
    }

    return (p.x >= r.x && p.x < (r.x + r.w));
  } else {
    if (saxis->inverse) {
      r.y = saxis->draw_rect.y - sinterval / 2;
      r.h = sinterval * (tk_min(srange, size) + 1) - 1;
    } else {
      sinterval = -sinterval;
      r.y = saxis->draw_rect.y + saxis->draw_rect.h - 1 - sinterval / 2;
      r.h = sinterval * (tk_min(srange, size) + 1) + 1;
    }

    if (r.h < 0) {
      r.h = -r.h;
      r.y = r.y - r.h + 1;
    }

    return (p.y >= r.y && p.y < (r.y + r.h));
  }
}

int32_t series_p_relative_index_of_point_in(widget_t* widget, xy_t x, xy_t y, bool_t is_local) {
  int32_t ret;
  point_t p = {x, y};
  point_t o = {0};
  widget_t* saxis;
  widget_t* vaxis;
  float_t sinterval;
  float_t soffset = 0;
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL, -1);

  saxis = widget_get_prop_pointer(widget, SERIES_PROP_SERIES_AXIS_OBJ);
  vaxis = widget_get_prop_pointer(widget, SERIES_PROP_VALUE_AXIS_OBJ);
  return_value_if_fail(saxis != NULL && vaxis != NULL, -1);

  if (!is_local) {
    widget_to_local(widget, &p);
  }

  sinterval = axis_measure_series_interval(saxis);
  if (AXIS(saxis)->axis_type != AXIS_TYPE_CATEGORY) {
    soffset = sinterval / 2;
  }

  series_p_get_origin_point(widget, saxis, vaxis, FALSE, &o);

  if (tk_str_eq(widget_get_type(saxis), WIDGET_TYPE_X_AXIS)) {
    ret = AXIS(saxis)->inverse ? ((o.x - p.x + soffset) / sinterval)
                               : ((p.x - o.x + soffset) / sinterval);
  } else {
    ret = AXIS(saxis)->inverse ? ((p.y - o.y + soffset) / sinterval)
                               : ((o.y - p.y + soffset) / sinterval);
  }

  return ret;
}

int32_t series_p_index_of_point_in(widget_t* widget, xy_t x, xy_t y, bool_t is_local) {
  uint32_t begin, end, middle, size;
  int32_t index = -1;
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && series->fifo != NULL, -1);

  size = SERIES_FIFO_GET_SIZE(series->fifo);
  return_value_if_true(size == 0, -1);

  series_get_current(widget, &begin, &end, &middle);
  index = series_p_relative_index_of_point_in(widget, x, y, is_local);
  return_value_if_true(index < 0, -1);

  if (index <= end - middle) {
    index += middle;
  } else if (index < end - begin + 1) {
    index += begin + middle - end - series->coverage;
  } else {
    index = -1;
  }

  if (index < begin || index > end) {
    index = -1;
  }

  return index;
}

ret_t series_p_to_local(widget_t* widget, uint32_t index, point_t* p) {
  uint32_t begin, end, middle, offset;
  point_t o = {0};
  widget_t* saxis;
  widget_t* vaxis;
  axis_t* s;
  axis_t* v;
  void* draw_data;
  uint32_t data_size;
  float_t soffset = 0;
  float_t sinterval, vrange, vmin;
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL, RET_BAD_PARAMS);

  series_get_current(widget, &begin, &end, &middle);
  return_value_if_fail(index >= begin && index <= end, RET_BAD_PARAMS);

  offset = index >= middle ? (index - middle) : (index - begin + end - middle + 1);

  saxis = widget_get_prop_pointer(widget, SERIES_PROP_SERIES_AXIS_OBJ);
  vaxis = widget_get_prop_pointer(widget, SERIES_PROP_VALUE_AXIS_OBJ);
  s = AXIS(saxis);
  v = AXIS(vaxis);
  return_value_if_fail(s != NULL && v != NULL, RET_BAD_PARAMS);

  draw_data = (void*)TKMEM_ALLOC(series->vt->draw_data_info->unit_size);
  return_value_if_fail(draw_data != NULL, RET_BAD_PARAMS);

  sinterval = axis_measure_series_interval(saxis);
  vrange = axis_get_range(vaxis, FALSE);
  vmin = v->min * v->max < 0 ? 0 : v->min;
  if (s->axis_type == AXIS_TYPE_CATEGORY) {
    soffset = sinterval / 2;
  }

  series_p_get_origin_point(widget, saxis, vaxis, FALSE, &o);

  if (tk_str_eq(widget_get_type(saxis), WIDGET_TYPE_X_AXIS)) {
    series->vt->draw_data_info->set_as_axis12(draw_data, 0, series->fifo, index, vmin, vrange,
                                              v->draw_rect.h, v->inverse);
    p->y = o.y + series->vt->draw_data_info->get_axis2(draw_data);
    p->x = s->inverse ? (o.x - offset * sinterval - soffset) : (o.x + offset * sinterval + soffset);
  } else {
    series->vt->draw_data_info->set_as_axis21(draw_data, 0, series->fifo, index, vmin, vrange,
                                              v->draw_rect.w, v->inverse);
    p->x = o.x + series->vt->draw_data_info->get_axis1(draw_data);
    p->y = s->inverse ? (o.y + offset * sinterval + soffset) : (o.y - offset * sinterval - soffset);
  }

  TKMEM_FREE(draw_data);

  return RET_OK;
}

uint32_t series_p_get_offset_max(widget_t* widget) {
  uint32_t range, size;
  widget_t* axis;
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && series->fifo != NULL, 0);

  size = SERIES_FIFO_GET_SIZE(series->fifo);
  return_value_if_true(size == 0, 0);

  axis = widget_get_prop_pointer(widget, SERIES_PROP_SERIES_AXIS_OBJ);
  return_value_if_fail(axis != NULL, 0);
  range = axis_get_range(axis, TRUE);

  if (size > range) {
    if (series->display_mode == SERIES_DISPLAY_COVER) {
      return size - range - tk_roundi(series->clip_range);
    } else {
      return size - range;
    }
  }

  return 0;
}

ret_t series_p_set_offset(widget_t* widget, uint32_t offset) {
  uint32_t offset_max;
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL, RET_BAD_PARAMS);

  offset_max = series_p_get_offset_max(widget);
  series->offset = tk_min(offset_max, offset);

  return RET_OK;
}

static ret_t series_p_set_new_period_internal(widget_t* widget, uint32_t period, bool_t animat) {
  uint32_t range, capacity, size;
  widget_t* axis;
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && series->fifo != NULL, RET_BAD_PARAMS);

  capacity = SERIES_FIFO_GET_CAPACITY(series->fifo);
  size = SERIES_FIFO_GET_SIZE(series->fifo);
  return_value_if_fail(capacity > 0 && period <= size, RET_BAD_PARAMS);

  axis = widget_get_prop_pointer(widget, SERIES_PROP_SERIES_AXIS_OBJ);
  return_value_if_fail(axis != NULL, RET_BAD_PARAMS);

  range = (uint32_t)(axis_get_range(axis, TRUE));
  return_value_if_fail(range != 0 && period <= range, RET_BAD_PARAMS);

  if (series->new_period != period) {
    if (series->value_animation == 0 || !animat) {
      series->new_period = period;
    } else {
      widget_animator_t* wa = NULL;

      widget_destroy_animator(widget, NULL);

      series->clip_range = 0;

      wa = widget_animator_prop_create(widget, series->value_animation, 0, SERIES_ANIMATION_EASING,
                                       SERIES_PROP_NEW_PERIOD);
      assert(wa != NULL);
      widget_animator_prop_set_params(wa, series->new_period, period);

      if (series->inited) {
        widget_animator_start(wa);
      }
    }
  }

  return RET_OK;
}

ret_t series_p_set_new_period(widget_t* widget, uint32_t period) {
  return series_p_set_new_period_internal(widget, period, TRUE);
}

ret_t series_p_get_prop(widget_t* widget, const char* name, value_t* v) {
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && name != NULL && v != NULL, RET_BAD_PARAMS);

  if (tk_str_eq(name, SERIES_PROP_OFFSET)) {
    value_set_uint32(v, series->offset);
    return RET_OK;
  } else if (tk_str_eq(name, SERIES_PROP_OFFSET_MAX)) {
    value_set_uint32(v, series_p_get_offset_max(widget));
    return RET_OK;
  } else if (tk_str_eq(name, SERIES_PROP_CAPACITY)) {
    value_set_uint32(v, SERIES_FIFO_GET_CAPACITY(series->fifo));
    return RET_OK;
  } else if (tk_str_eq(name, SERIES_PROP_COVERAGE)) {
    value_set_uint32(v, series->coverage);
    return RET_OK;
  } else if (tk_str_eq(name, SERIES_PROP_DISPLAY_MODE)) {
    value_set_int(v, series->display_mode);
    return RET_OK;
  } else if (tk_str_eq(name, SERIES_PROP_VALUE_ANIMATION)) {
    value_set_uint32(v, series->value_animation);
    return RET_OK;
  } else if (tk_str_eq(name, SERIES_PROP_NEW_PERIOD)) {
    value_set_uint32(v, series->new_period);
    return RET_OK;
  } else if (tk_str_eq(name, SERIES_PROP_TITLE)) {
    value_set_wstr(v, series_get_title(widget));
    return RET_OK;
  } else if (tk_str_eq(name, SERIES_PROP_FIFO)) {
    value_set_object(v, series->fifo);
    return RET_OK;
  }

  return RET_NOT_FOUND;
}

ret_t series_p_set_prop(widget_t* widget, const char* name, const value_t* v) {
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL && name != NULL && v != NULL, RET_BAD_PARAMS);

  if (tk_str_eq(name, SERIES_PROP_OFFSET)) {
    return series_p_set_offset(widget, value_uint32(v));
  } else if (tk_str_eq(name, SERIES_PROP_CLIP_RANGE)) {
    series->clip_range = value_float(v);
    return RET_OK;
  } else if (tk_str_eq(name, SERIES_PROP_CAPACITY)) {
    return series_set_capacity(widget, value_uint32(v));
  } else if (tk_str_eq(name, SERIES_PROP_COVERAGE)) {
    series->coverage = value_uint32(v);
    return RET_OK;
  } else if (tk_str_eq(name, SERIES_PROP_DISPLAY_MODE)) {
    if (v->type == VALUE_TYPE_STRING) {
      series->display_mode = series_dispaly_mode_from_str(value_str(v));
    } else {
      series->display_mode = (series_dispaly_mode_t)value_int(v);
    }
    return RET_OK;
  } else if (tk_str_eq(name, SERIES_PROP_VALUE_ANIMATION)) {
    series->value_animation = value_uint32(v);
    return RET_OK;
  } else if (tk_str_eq(name, SERIES_PROP_NEW_PERIOD)) {
    return series_p_set_new_period_internal(widget, value_uint32(v), FALSE);
  } else if (tk_str_eq(name, SERIES_PROP_TITLE)) {
    return series_set_title(widget, value_str(v));
  } else if (tk_str_eq(name, SERIES_PROP_FIFO)) {
    return series_set_fifo(widget, (object_t*)value_object(v));
  }

  return RET_NOT_FOUND;
}

widget_t* series_p_lookup_series_axis(widget_t* widget, const char* name) {
  return_value_if_fail(widget != NULL && widget->parent != NULL, NULL);

  if (name == NULL || name[0] == '\0') {
    return widget_lookup_by_type(widget->parent, WIDGET_TYPE_X_AXIS, FALSE);
  } else {
    return widget_lookup(widget->parent, name, FALSE);
  }
}

widget_t* series_p_lookup_value_axis(widget_t* widget, const char* name) {
  return_value_if_fail(widget != NULL && widget->parent != NULL, NULL);

  if (name == NULL || name[0] == '\0') {
    return widget_lookup_by_type(widget->parent, WIDGET_TYPE_Y_AXIS, FALSE);
  } else {
    return widget_lookup(widget->parent, name, FALSE);
  }
}

ret_t series_p_get_origin_point(widget_t* widget, widget_t* saxis, widget_t* vaxis, bool_t inverse,
                                point_t* p) {
  axis_t* s = AXIS(saxis);
  axis_t* v = AXIS(vaxis);
  rect_t* vruler;
  rect_t* sruler;
  float_t voffset = 0;
  return_value_if_fail(widget != NULL && s != NULL && v != NULL, RET_BAD_PARAMS);

  vruler = &(v->draw_rect);
  sruler = &(s->draw_rect);
  inverse = s->inverse ^ inverse;

  if (v->axis_type != AXIS_TYPE_CATEGORY && v->max * v->min < 0) {
    voffset = -v->min / axis_get_range(vaxis, FALSE);
  }

  if (tk_str_eq(widget_get_type(saxis), WIDGET_TYPE_X_AXIS)) {
    voffset = voffset * (vruler->h - 1);
    p->x = inverse ? (sruler->x + sruler->w - 1) : sruler->x;
    p->y = v->inverse ? (vruler->y + voffset) : (vruler->y + vruler->h - 1 - voffset);
  } else {
    voffset = voffset * (vruler->w - 1);
    p->y = inverse ? sruler->y : (sruler->y + sruler->h - 1);
    p->x = v->inverse ? (vruler->x + vruler->w - 1 - voffset) : (vruler->x + voffset);
  }

  return RET_OK;
}

float_t series_p_get_series_interval(widget_t* widget) {
  widget_t* axis = widget_get_prop_pointer(widget, SERIES_PROP_SERIES_AXIS_OBJ);
  return axis_measure_series_interval(axis);
}

bool_t series_p_is_vertical(widget_t* widget) {
  widget_t* axis = widget_get_prop_pointer(widget, SERIES_PROP_VALUE_AXIS_OBJ);
  return (axis && tk_str_eq(widget_get_type(axis), WIDGET_TYPE_Y_AXIS));
}

static ret_t series_p_delay_start_animator(void* ctx, event_t* e) {
  widget_start_animator(WIDGET(ctx), NULL);
  return RET_REMOVE;
}

ret_t series_p_start_animator_when_inited(widget_t* widget) {
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL, RET_BAD_PARAMS);
  return_value_if_true(series->value_animation == 0, RET_OK);
  return_value_if_true(
      widget_animator_manager_find(widget_animator_manager(), widget, NULL) == NULL, RET_OK);

  if (widget_is_window_opened(widget)) {
    widget_start_animator(widget, NULL);
  } else {
    widget_on(widget_get_window(widget), EVT_WINDOW_OPEN, series_p_delay_start_animator, widget);
  }

  return RET_OK;
}

static ret_t series_p_measure_draw_data(series_t* series, widget_t* saxis, widget_t* vaxis,
                                        object_t* fifo, uint32_t index_of_fold, bool_t vertical) {
  axis_measure_series_params_t params = {0};
  assert(series != NULL && saxis != NULL && vaxis != NULL && fifo != NULL);

  params.index_of_fifo = series->offset;
  params.index_of_fold = index_of_fold;
  params.series_axis = saxis;
  params.draw_data_compare_series = vertical ? series->vt->draw_data_info->compare_in_axis1
                                             : series->vt->draw_data_info->compare_in_axis2;
  params.draw_data_compare_value = vertical ? series->vt->draw_data_info->compare_in_axis2
                                            : series->vt->draw_data_info->compare_in_axis1;
  params.draw_data_min_value =
      vertical ? series->vt->draw_data_info->min_axis2 : series->vt->draw_data_info->min_axis1;
  params.draw_data_max_value =
      vertical ? series->vt->draw_data_info->max_axis2 : series->vt->draw_data_info->max_axis1;
  params.draw_data_set = vertical ? series->vt->draw_data_info->set_as_axis12
                                  : series->vt->draw_data_info->set_as_axis21;
  return axis_measure_series(vaxis, (void*)(&params), series->fifo, fifo);
}

static ret_t series_p_calc_clip_rect(series_t* series, widget_t* saxis, canvas_t* c, point_t* o,
                                     float_t interval, bool_t vertical, object_t* fifo, rect_t* r) {
  void* d;
  float_t w, h;
  uint32_t size;
  int32_t offset = tk_roundi(interval / 2);
  assert(series != NULL && saxis != NULL && fifo != NULL && c != NULL && o != NULL && r != NULL);

  size = SERIES_FIFO_GET_SIZE(fifo);
  d = series_fifo_get(fifo, size - 1);

  if (vertical) {
    assert(series->vt != NULL && series->vt->draw_data_info->get_axis1 != NULL);
    w = series->vt->draw_data_info->get_axis1(d) + (interval < 0 ? -1 : 1);
    r->x = c->ox + o->x - offset;
    r->w = tk_roundi(w - interval * series->clip_range) + offset;
    r->w = r->w == 0 ? 1 : r->w;
  } else {
    assert(series->vt != NULL && series->vt->draw_data_info->get_axis2 != NULL);
    h = series->vt->draw_data_info->get_axis2(d) + (interval < 0 ? 1 : -1);
    r->y = c->oy + o->y + offset;
    r->h = tk_roundi(h + interval * series->clip_range) - offset;
    r->h = r->h == 0 ? 1 : r->h;
  }

  return RET_OK;
}

static ret_t series_p_calc_coverd_clip_rect(series_t* series, canvas_t* c, point_t* o,
                                            float_t interval, bool_t vertical, rect_t* r) {
  assert(series != NULL && c != NULL && o != NULL && r != NULL);

  if (vertical) {
    r->x = tk_max(r->x, r->x + r->w + tk_roundi(interval * series->coverage) - 1);
    r->w = c->ox + o->x - r->x + (c->ox + o->x >= r->x ? 1 : -1);
  } else {
    r->y = tk_max(r->y, r->y + r->h + tk_roundi(interval * series->coverage) - 1);
    r->h = c->oy + o->y - r->y + (c->oy + o->y >= r->y ? 1 : -1);
  }

  return RET_OK;
}

ret_t series_p_on_paint_self_push(widget_t* widget, canvas_t* c) {
  point_t o = {0};
  object_t* fifo;
  uint32_t nr;
  float_t interval;
  widget_t* saxis;
  widget_t* vaxis;
  bool_t vertical;
  rect_t clip_rect = rect_init(c->ox, c->oy, widget->w, widget->h);

  series_t* series = SERIES(widget);

  return_value_if_fail(series != NULL && series->vt->on_paint != NULL, RET_BAD_PARAMS);
  return_value_if_true(SERIES_FIFO_GET_SIZE(series->fifo) == 0, RET_OK);

  saxis = widget_get_prop_pointer(widget, SERIES_PROP_SERIES_AXIS_OBJ);
  vaxis = widget_get_prop_pointer(widget, SERIES_PROP_VALUE_AXIS_OBJ);
  return_value_if_fail(saxis != NULL && vaxis != NULL, RET_BAD_PARAMS);

  vertical = tk_str_eq(widget_get_type(vaxis), WIDGET_TYPE_Y_AXIS);
  interval = axis_measure_series_interval(saxis);
  interval = AXIS(saxis)->inverse ? -interval : interval;
  nr = axis_get_range(saxis, TRUE);
  return_value_if_fail(nr > 0, RET_BAD_PARAMS);

  fifo = series_fifo_default_create(nr, series->vt->draw_data_info->unit_size);
  return_value_if_fail(fifo != NULL, RET_OOM);

  series_p_measure_draw_data(series, saxis, vaxis, fifo, 0, vertical);

  if (SERIES_FIFO_GET_SIZE(fifo) > 0) {
    series_p_get_origin_point(widget, saxis, vaxis, FALSE, &o);
    series_p_calc_clip_rect(series, saxis, c, &o, interval, vertical, fifo, &clip_rect);
    series->vt->on_paint(widget, c, o.x, o.y, fifo, 0, SERIES_FIFO_GET_SIZE(fifo), &clip_rect);
  }

  OBJECT_UNREF(fifo);

  return RET_OK;
}

ret_t series_p_on_paint_self_cover(widget_t* widget, canvas_t* c) {
  point_t o = {0};
  object_t* fifo;
  uint32_t nr, size;
  float_t range;
  float_t interval;
  widget_t* saxis;
  widget_t* vaxis;
  bool_t vertical;
  rect_t clip_rect = rect_init(c->ox, c->oy, widget->w, widget->h);
  uint32_t index_of_fold = 0;
  series_t* series = SERIES(widget);

  return_value_if_fail(series != NULL && series->vt->on_paint != NULL, RET_BAD_PARAMS);

  size = SERIES_FIFO_GET_SIZE(series->fifo);
  return_value_if_true(size == 0, RET_OK);

  saxis = widget_get_prop_pointer(widget, SERIES_PROP_SERIES_AXIS_OBJ);
  vaxis = widget_get_prop_pointer(widget, SERIES_PROP_VALUE_AXIS_OBJ);
  return_value_if_fail(saxis != NULL && vaxis != NULL, RET_BAD_PARAMS);

  vertical = tk_str_eq(widget_get_type(vaxis), WIDGET_TYPE_Y_AXIS);
  interval = axis_measure_series_interval(saxis);
  interval = AXIS(saxis)->inverse ? -interval : interval;
  nr = range = axis_get_range(saxis, TRUE);
  return_value_if_fail(nr > 0, RET_BAD_PARAMS);

  if (size > range) {
    if (series->offset == 0 || series->new_period < range) {
      // offset之前还有点, 则多取一个，以便bezier时保持一致
      nr += tk_max(series->offset + range == size ? 0 : 1, tk_roundi(series->clip_range));
      index_of_fold = nr - series->new_period;
    }
  }

  fifo = series_fifo_default_create(nr, series->vt->draw_data_info->unit_size);
  return_value_if_fail(fifo != NULL, RET_OOM);

  series_p_measure_draw_data(series, saxis, vaxis, fifo, index_of_fold, vertical);
  series_p_get_origin_point(widget, saxis, vaxis, FALSE, &o);

  if (SERIES_FIFO_GET_SIZE(fifo) - index_of_fold > 0) {
    series_p_calc_clip_rect(series, saxis, c, &o, interval, vertical, fifo, &clip_rect);
    series->vt->on_paint(widget, c, o.x, o.y, fifo, index_of_fold,
                         SERIES_FIFO_GET_SIZE(fifo) - index_of_fold, &clip_rect);
  } else {
    if (vertical) {
      clip_rect.x = c->ox + o.x;
      clip_rect.w = -interval;
    } else {
      clip_rect.y = c->oy + o.y;
      clip_rect.h = -interval;
    }
  }

  if (index_of_fold > 0) {
    point_t p;
    series_p_get_origin_point(widget, saxis, vaxis, TRUE, &p);
    series_p_calc_coverd_clip_rect(series, c, &p, interval, vertical, &clip_rect);
    series->vt->on_paint(widget, c, o.x, o.y, fifo, 0, index_of_fold, &clip_rect);
  }

  OBJECT_UNREF(fifo);

  return RET_OK;
}

ret_t series_p_on_destroy(widget_t* widget) {
  series_t* series = SERIES(widget);
  return_value_if_fail(series != NULL, RET_BAD_PARAMS);

  series_p_fifo_deinit(widget);

  return RET_OK;
}