/**
 * File:   chart_animator.c
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

#include "chart_animator.h"
#include "series_types.h"

static ret_t chart_animator_update(widget_animator_t* animator, float_t percent) {
  chart_animator_t* c = (chart_animator_t*)animator;
  return_value_if_fail(c != NULL && c->from != NULL && c->to != NULL && c->cur != NULL,
                       RET_BAD_PARAMS);

  if (c->update) {
    c->update(c->ctx, c->from, c->to, percent, c->cur);
    widget_invalidate(animator->widget, NULL);
  }

  return RET_OK;
}

static ret_t chart_animator_destroy(widget_animator_t* animator) {
  chart_animator_t* c = (chart_animator_t*)animator;
  assert(c);

  if (c->destroy) {
    if (c->should_free_from) {
      c->destroy(c->from);
    }

    if (c->should_free_to) {
      c->destroy(c->to);
    }

    if (c->should_free_cur) {
      c->destroy(c->cur);
    }
  }

  TKMEM_FREE(animator);
  return RET_OK;
}

widget_animator_t* chart_animator_create(widget_t* widget, uint32_t duration, uint32_t delay,
                                         easing_type_t easing, chart_animator_update_t update,
                                         tk_destroy_t destroy, bool_t should_free_from,
                                         bool_t should_free_to, bool_t should_free_cur) {
  widget_animator_t* animator = NULL;
  return_value_if_fail(widget != NULL && duration > 0, NULL);

  animator = (widget_animator_t*)TKMEM_ZALLOC(chart_animator_t);
  return_value_if_fail(animator != NULL, NULL);

  if (widget_animator_init(animator, widget, duration, delay, easing_get(easing)) != RET_OK) {
    widget_animator_destroy(animator);
    return NULL;
  } else {
    chart_animator_t* v = (chart_animator_t*)animator;
    v->should_free_from = should_free_from;
    v->should_free_to = should_free_to;
    v->should_free_cur = should_free_cur;
    v->update = update;
    v->destroy = destroy;
    animator->update = chart_animator_update;
    animator->destroy = chart_animator_destroy;
  }

  return animator;
}

static ret_t chart_animator_fifo_float_value_update(void* ctx, const void* from, const void* to,
                                                    float_t percent, void* cur) {
  object_t* f = (object_t*)from;
  object_t* t = (object_t*)to;
  object_t* c = (object_t*)cur;
  uint32_t index = (uint32_t)((char*)ctx - (char*)NULL);
  uint32_t f_size = SERIES_FIFO_GET_SIZE(f);
  uint32_t t_size = SERIES_FIFO_GET_SIZE(t);
  uint32_t c_size = SERIES_FIFO_GET_SIZE(c);
  uint32_t nr = tk_min(tk_min(f_size, t_size), c_size);
  uint32_t i;
  float_t* df;
  float_t* dt;
  float_t* dc;

  for (i = 0; i < nr; i++) {
    bool_t save = SERIES_FIFO(c)->block_event;
    float_t* temp = TKMEM_ZALLOC(float_t);
    df = (float_t*)series_fifo_get(f, i);
    dt = (float_t*)series_fifo_get(t, i);
    *temp = *df + (*dt - *df) * percent;
    series_fifo_set_block_event(c, TRUE);
    series_fifo_set(c, index + i, (void*)temp, 1);
    series_fifo_set_block_event(c, save);
    TKMEM_FREE(temp);
  }

  return RET_OK;
}

static ret_t chart_animator_fifo_colorful_value_update(void* ctx, const void* from, const void* to,
                                                       float_t percent, void* cur) {
  object_t* f = (object_t*)from;
  object_t* t = (object_t*)to;
  object_t* c = (object_t*)cur;
  uint32_t index = (uint32_t)((char*)ctx - (char*)NULL);
  uint32_t f_size = SERIES_FIFO_GET_SIZE(f);
  uint32_t t_size = SERIES_FIFO_GET_SIZE(t);
  uint32_t c_size = SERIES_FIFO_GET_SIZE(c);
  uint32_t nr = tk_min(tk_min(f_size, t_size), c_size);
  uint32_t i;
  series_data_colorful_t* df;
  series_data_colorful_t* dt;
  series_data_colorful_t* dc;

  for (i = 0; i < nr; i++) {
    bool_t save = SERIES_FIFO(c)->block_event;
    series_data_colorful_t* temp = TKMEM_ZALLOC(series_data_colorful_t);
    df = (series_data_colorful_t*)series_fifo_get(f, i);
    dt = (series_data_colorful_t*)series_fifo_get(t, i);
    temp->v = df->v + (dt->v - df->v) * percent;
    temp->c.rgba.r = (uint8_t)(df->c.rgba.r + (dt->c.rgba.r - df->c.rgba.r) * percent);
    temp->c.rgba.g = (uint8_t)(df->c.rgba.g + (dt->c.rgba.g - df->c.rgba.g) * percent);
    temp->c.rgba.b = (uint8_t)(df->c.rgba.b + (dt->c.rgba.b - df->c.rgba.b) * percent);
    temp->c.rgba.a = (uint8_t)(df->c.rgba.a + (dt->c.rgba.a - df->c.rgba.a) * percent);
    series_fifo_set_block_event(c, TRUE);
    series_fifo_set(c, index + i, (void*)temp, 1);
    series_fifo_set_block_event(c, save);
    TKMEM_FREE(temp);
  }

  return RET_OK;
}

static ret_t chart_animator_fifo_minmax_value_update(void* ctx, const void* from, const void* to,
                                                     float_t percent, void* cur) {
  object_t* f = (object_t*)from;
  object_t* t = (object_t*)to;
  object_t* c = (object_t*)cur;
  uint32_t index = (uint32_t)((char*)ctx - (char*)NULL);
  uint32_t f_size = SERIES_FIFO_GET_SIZE(f);
  uint32_t t_size = SERIES_FIFO_GET_SIZE(t);
  uint32_t c_size = SERIES_FIFO_GET_SIZE(c);
  uint32_t nr = tk_min(tk_min(f_size, t_size), c_size);
  uint32_t i;
  series_data_minmax_t* df;
  series_data_minmax_t* dt;
  series_data_minmax_t* dc;

  for (i = 0; i < nr; i++) {
    bool_t save = SERIES_FIFO(c)->block_event;
    series_data_minmax_t* temp = TKMEM_ZALLOC(series_data_minmax_t);
    df = (series_data_minmax_t*)series_fifo_get(f, i);
    dt = (series_data_minmax_t*)series_fifo_get(t, i);
    temp->min = df->min + (dt->min - df->min) * percent;
    temp->max = df->max + (dt->max - df->max) * percent;
    series_fifo_set_block_event(c, TRUE);
    series_fifo_set(c, index + i, (void*)temp, 1);
    series_fifo_set_block_event(c, save);
    TKMEM_FREE(temp);
  }

  return RET_OK;
}

static ret_t on_series_fifo_destroy(void* data) {
  object_t* fifo = OBJECT(data);
  return_value_if_fail(fifo, RET_BAD_PARAMS);

  OBJECT_UNREF(fifo);
  return RET_OK;
}

widget_animator_t* chart_animator_fifo_float_value_create(widget_t* widget, uint32_t duration,
                                                          uint32_t delay, easing_type_t easing) {
  return chart_animator_create(widget, duration, delay, easing,
                               chart_animator_fifo_float_value_update,
                               (tk_destroy_t)on_series_fifo_destroy, TRUE, TRUE, FALSE);
}

widget_animator_t* chart_animator_fifo_colorful_value_create(widget_t* widget, uint32_t duration,
                                                             uint32_t delay, easing_type_t easing) {
  return chart_animator_create(widget, duration, delay, easing,
                               chart_animator_fifo_colorful_value_update,
                               (tk_destroy_t)on_series_fifo_destroy, TRUE, TRUE, FALSE);
}

widget_animator_t* chart_animator_fifo_minmax_value_create(widget_t* widget, uint32_t duration,
                                                           uint32_t delay, easing_type_t easing) {
  return chart_animator_create(widget, duration, delay, easing,
                               chart_animator_fifo_minmax_value_update,
                               (tk_destroy_t)on_series_fifo_destroy, TRUE, TRUE, FALSE);
}

ret_t chart_animator_fifo_value_set_params(widget_animator_t* animator, object_t* fifo,
                                           uint32_t index, const void* data, uint32_t nr,
                                           uint32_t update_max) {
  object_t* from;
  object_t* to;
  uint32_t fifo_size, fifo_unit_size, copy_size;
  chart_animator_t* c = (chart_animator_t*)animator;
  return_value_if_fail(c != NULL && fifo != NULL && data != NULL && nr > 0 && update_max > 0,
                       RET_BAD_PARAMS);

  fifo_size = SERIES_FIFO_GET_SIZE(fifo);
  fifo_unit_size = SERIES_FIFO_GET_UNIT_SIZE(fifo);
  return_value_if_fail(index <= fifo_size - 1, RET_BAD_PARAMS);
  return_value_if_fail(index + nr - 1 <= fifo_size - 1, RET_BAD_PARAMS);

  copy_size = tk_min(nr, update_max);

  from = series_fifo_part_clone(fifo, fifo_size - copy_size, copy_size);
  return_value_if_fail(from != NULL, RET_OOM);

  to = series_fifo_default_create(copy_size, fifo_unit_size);
  if (to == NULL) {
    OBJECT_UNREF(from)
    return RET_OOM;
  }
  series_fifo_npush(to, (uint8_t*)data + fifo_unit_size * (nr - copy_size), copy_size);

  if (copy_size != nr) {
    bool_t save = SERIES_FIFO(fifo)->block_event;
    series_fifo_set_block_event(fifo, TRUE);
    series_fifo_set(fifo, index, data, nr - copy_size);
    series_fifo_set_block_event(fifo, save);
    index += nr - copy_size;
  }

  c->from = from;
  c->to = to;
  c->cur = fifo;
  c->ctx = (char*)NULL + index;

  return RET_OK;
}
