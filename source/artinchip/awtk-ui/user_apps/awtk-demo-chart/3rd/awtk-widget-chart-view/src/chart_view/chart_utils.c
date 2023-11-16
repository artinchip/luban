/**
 * File:   chart_utils.c
 * Author: AWTK Develop Team
 * Brief:  animate widget by change its value
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

#include "chart_utils.h"
#include "tkc/utils.h"

// 坐标取整加0.5，防止线宽为1时显示2个像素
#define _VG_XY(v) (float_t)((xy_t)(v) + 0.5)
#define _VGCANVAS_ARC(vg, x, y, r, start, end, ccw) \
  vgcanvas_arc(vg, _VG_XY(x), _VG_XY(y), r, start, end, ccw)

color_t chart_utils_value_color(const value_t* v) {
  if (v->type == VALUE_TYPE_STRING) {
    return color_parse(value_str(v));
  } else {
    color_t color;
    color.color = value_uint32(v);
    return color;
  }
}

ret_t chart_utils_object_parse(chart_utils_on_object_parse_t on_parse, void* ctx,
                               const char* object) {
  ENSURE(on_parse != NULL && object != NULL);

  char name[TK_NAME_LEN + 1];
  value_t v;
  const char* token = NULL;
  tokenizer_t tokenizer;

  tokenizer_init(&tokenizer, object, strlen(object), "{,:} ");

  while (tokenizer_has_more(&tokenizer)) {
    token = tokenizer_next(&tokenizer);

    if (tokenizer_has_more(&tokenizer)) {
      tk_strncpy(name, token, TK_NAME_LEN);
      value_set_str(&v, tokenizer_next(&tokenizer));
      on_parse(ctx, name, &v);
    }
  }

  tokenizer_deinit(&tokenizer);

  return RET_OK;
}

rect_t* chart_utils_rect_fix(rect_t* r) {
  if (r != NULL) {
    if (r->w < 0) {
      r->w = -r->w;
      r->x = r->x - r->w + 1;
    }

    if (r->h < 0) {
      r->h = -r->h;
      r->y = r->y - r->h + 1;
    }
  }

  return r;
}

ret_t chart_utils_draw_a_symbol(canvas_t* c, rect_t* r, color_t bg, color_t bd,
                                uint32_t border_width, uint32_t radius, bitmap_t* img,
                                image_draw_type_t draw_type) {
  ENSURE(c != NULL && r != NULL);

  ret_t ret;
  vgcanvas_t* vg = canvas_get_vgcanvas(c);
  bool_t use_vg = vg != NULL && radius >= r->w / 2 && radius >= r->h / 2;
  float_t halfwidth = (float_t)r->w / 2;

  if (bg.rgba.a) {
    if (use_vg) {
      vgcanvas_save(vg);
      vgcanvas_translate(vg, c->ox, c->oy);
      vgcanvas_set_fill_color(vg, bg);
      vgcanvas_begin_path(vg);
      _VGCANVAS_ARC(vg, r->x + halfwidth, r->y + halfwidth, halfwidth, 0, 2 * M_PI, FALSE);
      vgcanvas_fill(vg);
      vgcanvas_restore(vg);
    } else {
      canvas_set_fill_color(c, bg);
      if (radius > 3) {
        ret = canvas_fill_rounded_rect(c, r, NULL, &bg, radius);
        if (ret == RET_FAIL) {
          canvas_fill_rect(c, r->x, r->y, r->w, r->h);
        }
      } else {
        canvas_fill_rect(c, r->x, r->y, r->w, r->h);
      }
    }
  }

  if (img != NULL) {
    canvas_draw_image_ex(c, img, draw_type, r);
  }

  if (bd.rgba.a) {
    if (use_vg) {
      vgcanvas_save(vg);
      vgcanvas_translate(vg, c->ox, c->oy);
      vgcanvas_set_stroke_color(vg, bd);
      vgcanvas_set_line_width(vg, border_width);
      vgcanvas_begin_path(vg);
      _VGCANVAS_ARC(vg, r->x + halfwidth, r->y + halfwidth, halfwidth, 0, 2 * M_PI, FALSE);
      vgcanvas_stroke(vg);
      vgcanvas_restore(vg);
    } else {
      canvas_set_stroke_color(c, bd);
      if (radius > 3) {
        if (canvas_stroke_rounded_rect(c, r, NULL, &bd, radius, border_width) != RET_OK) {
          canvas_stroke_rect(c, r->x, r->y, r->w, r->h);
        }
      } else {
        canvas_stroke_rect(c, r->x, r->y, r->w, r->h);
      }
    }
  }

  return RET_OK;
}
