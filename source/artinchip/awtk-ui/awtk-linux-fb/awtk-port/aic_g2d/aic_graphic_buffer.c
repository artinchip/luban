/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Zequan Liang <zequan.liang@artinchip.com>
 */

#ifdef WITH_AIC_G2D

#include "tkc/mem.h"
#include "base/bitmap.h"
#include "base/graphic_buffer.h"

#include "aic_graphic_buffer.h"
#include "aic_linux_mem.h"
/**
 * @class graphic_buffer_aic_t
 * graphic_buffer aic
 */
typedef struct _graphic_buffer_aic_t {
  graphic_buffer_t graphic_buffer;

  uint8_t* data;
  uint8_t* data_head;
  uint32_t w;
  uint32_t h;
  uint32_t line_length;

  cma_buffer cma_buff;
} graphic_buffer_aic_t;

static graphic_buffer_aic_t* graphic_buffer_aic_cast(graphic_buffer_t* buffer);

#define GRAPHIC_BUFFER_AIC(buffer) graphic_buffer_aic_cast(buffer)

static bool_t graphic_buffer_aic_is_valid_for(graphic_buffer_t* buffer, bitmap_t* bitmap) {
  graphic_buffer_aic_t* b = GRAPHIC_BUFFER_AIC(buffer);
  return_value_if_fail(b != NULL && bitmap != NULL, FALSE);
  if (bitmap->orientation == LCD_ORIENTATION_0 || bitmap->orientation == LCD_ORIENTATION_180) {
    return b->w == bitmap->w && b->h == bitmap->h;
  } else {
    return b->w == bitmap->h && b->h == bitmap->w;
  }
}

static uint8_t* graphic_buffer_aic_lock_for_read(graphic_buffer_t* buffer) {
  graphic_buffer_aic_t* b = GRAPHIC_BUFFER_AIC(buffer);
  return_value_if_fail(b != NULL, NULL);

  return b->data;
}

static uint8_t* graphic_buffer_aic_lock_for_write(graphic_buffer_t* buffer) {
  graphic_buffer_aic_t* b = GRAPHIC_BUFFER_AIC(buffer);
  return_value_if_fail(b != NULL, NULL);

  return b->data;
}

static ret_t graphic_buffer_aic_unlock(graphic_buffer_t* buffer) {
  return RET_OK;
}

static ret_t graphic_buffer_aic_attach(graphic_buffer_t* buffer, void* data, uint32_t w,
                                           uint32_t h) {
  graphic_buffer_aic_t* b = GRAPHIC_BUFFER_AIC(buffer);
  return_value_if_fail(b != NULL, RET_BAD_PARAMS);
  return_value_if_fail(b->data_head == NULL, RET_NOT_IMPL);

  b->w = w;
  b->h = h;
  b->data = data;

  return RET_OK;
}

static ret_t graphic_buffer_aic_destroy(graphic_buffer_t* buffer) {
  graphic_buffer_aic_t* b = GRAPHIC_BUFFER_AIC(buffer);
  return_value_if_fail(b != NULL, RET_BAD_PARAMS);

  aic_cma_buf_del(b->data_head);
  aic_cma_buf_debug(AIC_CMA_BUF_DEBUG_SIZE);
  TKMEM_FREE(b);

  return RET_OK;
}

static uint32_t graphic_buffer_aic_get_physical_width(graphic_buffer_t* buffer) {
  graphic_buffer_aic_t* b = GRAPHIC_BUFFER_AIC(buffer);
  return_value_if_fail(b != NULL, 0);

  return b->w;
}

static uint32_t graphic_buffer_aic_get_physical_height(graphic_buffer_t* buffer) {
  graphic_buffer_aic_t* b = GRAPHIC_BUFFER_AIC(buffer);
  return_value_if_fail(b != NULL, 0);

  return b->h;
}

static uint32_t graphic_buffer_aic_get_physical_line_length(graphic_buffer_t* buffer) {
  graphic_buffer_aic_t* b = GRAPHIC_BUFFER_AIC(buffer);
  return_value_if_fail(b != NULL, 0);

  return b->line_length;
}

static const graphic_buffer_vtable_t s_graphic_buffer_aic_vtable = {
    .lock_for_read = graphic_buffer_aic_lock_for_read,
    .lock_for_write = graphic_buffer_aic_lock_for_write,
    .unlock = graphic_buffer_aic_unlock,
    .attach = graphic_buffer_aic_attach,
    .is_valid_for = graphic_buffer_aic_is_valid_for,
    .get_width = graphic_buffer_aic_get_physical_width,
    .get_height = graphic_buffer_aic_get_physical_height,
    .get_line_length = graphic_buffer_aic_get_physical_line_length,
    .destroy = graphic_buffer_aic_destroy};

static graphic_buffer_t* graphic_buffer_aic_create(uint32_t w, uint32_t h,
                                                       bitmap_format_t format,
                                                       uint32_t line_length) {
  int ret = -1;
  uint32_t size = 0;
  graphic_buffer_aic_t* buffer = NULL;
  uint32_t min_line_length = bitmap_get_bpp_of_format(format) * w;
  return_value_if_fail(w > 0 && h > 0, NULL);

  static int create_time = 0;
  create_time++;
  buffer = TKMEM_ZALLOC(graphic_buffer_aic_t);
  return_value_if_fail(buffer != NULL, NULL);

  buffer->line_length = tk_max(min_line_length, line_length);
  size = buffer->line_length * h;
  size = TK_ROUND_TO(size, BITMAP_ALIGN_SIZE) + BITMAP_ALIGN_SIZE; // 32位对齐

  ret = aic_cma_buf_malloc(&buffer->cma_buff, size);
  if (ret == 0) {
    aic_cma_buf_add(&buffer->cma_buff);
    buffer->data_head = buffer->cma_buff.buf;
    memset(buffer->data_head, 0x00, size);
    buffer->w = w;
    buffer->h = h;
    buffer->data = buffer->cma_buff.buf;
    buffer->graphic_buffer.vt = &s_graphic_buffer_aic_vtable;
    return GRAPHIC_BUFFER(buffer);
  } else {
    log_error("in graphic_buffer_aic_create, aic_dmabuf_malloc err\n");
    TKMEM_FREE(buffer);
    return NULL;
  }
}

/*public functions*/
ret_t aic_graphic_buffer_create_for_bitmap(bitmap_t* bitmap) {
  uint32_t line_length = bitmap_get_line_length(bitmap);
  return_value_if_fail(bitmap != NULL && bitmap->buffer == NULL, RET_BAD_PARAMS);

  bitmap->buffer = graphic_buffer_aic_create(bitmap->w, bitmap->h,
                                                 (bitmap_format_t)(bitmap->format), line_length);
  bitmap->should_free_data = bitmap->buffer != NULL;

  return bitmap->buffer != NULL ? RET_OK : RET_OOM;
}

static graphic_buffer_aic_t* graphic_buffer_aic_cast(graphic_buffer_t* buffer) {
  return_value_if_fail(buffer != NULL && buffer->vt == &s_graphic_buffer_aic_vtable, NULL);

  return (graphic_buffer_aic_t*)(buffer);
}

#endif
