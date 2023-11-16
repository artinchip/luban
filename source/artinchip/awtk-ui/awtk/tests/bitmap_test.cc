﻿#include "tkc/mem.h"
#include "base/bitmap.h"
#include "gtest/gtest.h"

TEST(Bitmap, basic) {
  uint32_t n = 100;
  uint32_t i = 0;
  for (i = 0; i < n; i++) {
    bitmap_t* b = bitmap_create_ex(i + 1, i + 1, 0, BITMAP_FMT_BGRA8888);
    uint8_t* bdata = bitmap_lock_buffer_for_write(b);
    ASSERT_EQ(((intptr_t)(bdata)) % BITMAP_ALIGN_SIZE, (intptr_t)0);
    ASSERT_EQ(bitmap_get_line_length(b), b->w * 4u);
    bitmap_unlock_buffer(b);
    bitmap_destroy(b);
  }

  for (i = 0; i < n; i++) {
    bitmap_t* b = bitmap_create_ex(i + 1, i + 1, 0, BITMAP_FMT_BGR565);
    uint8_t* bdata = bitmap_lock_buffer_for_write(b);
    ASSERT_EQ(((intptr_t)(bdata)) % BITMAP_ALIGN_SIZE, (intptr_t)0);
    ASSERT_EQ(bitmap_get_line_length(b), b->w * 2u);
    bitmap_unlock_buffer(b);
    bitmap_destroy(b);
  }
}

TEST(Bitmap, row_size) {
  ASSERT_EQ(TK_BITMAP_MONO_LINE_LENGTH(8), 2);
  ASSERT_EQ(TK_BITMAP_MONO_LINE_LENGTH(9), 2);
  ASSERT_EQ(TK_BITMAP_MONO_LINE_LENGTH(13), 2);
  ASSERT_EQ(TK_BITMAP_MONO_LINE_LENGTH(15), 2);
  ASSERT_EQ(TK_BITMAP_MONO_LINE_LENGTH(16), 2);
  ASSERT_EQ(TK_BITMAP_MONO_LINE_LENGTH(17), 4);
  ASSERT_EQ(TK_BITMAP_MONO_LINE_LENGTH(23), 4);
  ASSERT_EQ(TK_BITMAP_MONO_LINE_LENGTH(24), 4);
  ASSERT_EQ(TK_BITMAP_MONO_LINE_LENGTH(25), 4);
}

static void test_get_set(uint32_t w, uint32_t h) {
  uint32_t i = 0;
  uint32_t j = 0;
  uint8_t* buff = bitmap_mono_create_data(w, h);

  for (j = 0; j < h; j++) {
    for (i = 0; i < w; i++) {
      ASSERT_EQ(bitmap_mono_get_pixel(buff, w, h, i, j), FALSE);
      ASSERT_EQ(bitmap_mono_set_pixel(buff, w, h, i, j, TRUE), RET_OK);
      ASSERT_EQ(bitmap_mono_get_pixel(buff, w, h, i, j), TRUE);
    }
  }
  TKMEM_FREE(buff);
}

TEST(Bitmap, set_get) {
  test_get_set(8, 1);
  test_get_set(8, 2);
  test_get_set(8, 3);
  test_get_set(8, 4);
  test_get_set(9, 1);
  test_get_set(9, 2);
  test_get_set(9, 3);
  test_get_set(9, 4);
  test_get_set(24, 1);
  test_get_set(24, 2);
  test_get_set(24, 3);
  test_get_set(24, 4);
  test_get_set(32, 16);
}

static uint8_t* gen_rgba_data(uint32_t w, uint32_t h) {
  uint32_t i = 0;
  uint32_t j = 0;
  uint32_t size = w * h * 4;
  uint8_t* data = (uint8_t*)TKMEM_ALLOC(size);
  uint8_t* p = data;

  for (j = 0; j < h; j++) {
    for (i = 0; i < w; i++) {
      if (j % 2) {
        p[0] = 0xff;
        p[1] = 0xff;
        p[2] = 0xff;
        p[3] = 0xff;
      } else {
        p[0] = 0;
        p[1] = 0;
        p[2] = 0;
        p[3] = 0xff;
      }
      p += 4;
    }
  }

  return data;
}

static void check_bitmap_mono(bitmap_t* b) {
  uint32_t i = 0;
  uint32_t j = 0;
  uint32_t w = b->w;
  uint32_t h = b->h;
  uint8_t* bdata = bitmap_lock_buffer_for_read(b);

  for (j = 0; j < h; j++) {
    for (i = 0; i < w; i++) {
      if (j % 2) {
        ASSERT_EQ(bitmap_mono_get_pixel(bdata, w, h, i, j), TRUE);
      } else {
        ASSERT_EQ(bitmap_mono_get_pixel(bdata, w, h, i, j), FALSE);
      }
    }
  }
  bitmap_unlock_buffer(b);

  return;
}

static void test_bitmap_mono(uint32_t w, uint32_t h) {
  bitmap_t b;
  uint8_t* data = gen_rgba_data(w, h);
  bitmap_init_from_rgba(&b, w, h, BITMAP_FMT_MONO, data, 4, LCD_ORIENTATION_0);
  check_bitmap_mono(&b);
  bitmap_destroy(&b);
  TKMEM_FREE(data);
}

TEST(Bitmap, mono_from_rgba) {
  test_bitmap_mono(1, 1);
  test_bitmap_mono(2, 2);
  test_bitmap_mono(7, 2);
  test_bitmap_mono(8, 2);
  test_bitmap_mono(15, 2);
  test_bitmap_mono(16, 2);
  test_bitmap_mono(17, 2);
  test_bitmap_mono(23, 2);
  test_bitmap_mono(24, 24);
}

TEST(Bitmap, clone) {
  bitmap_t* b = bitmap_create_ex(10, 20, 0, BITMAP_FMT_BGRA8888);
  uint8_t* buff = bitmap_lock_buffer_for_write(b);
  memset(buff, 0x12, b->w * b->h * 4);
  bitmap_unlock_buffer(b);
  bitmap_t* b1 = bitmap_clone(b);
  bitmap_destroy(b);
  ASSERT_EQ(b1->w, 10);
  ASSERT_EQ(b1->h, 20);
  ASSERT_EQ(b1->format, BITMAP_FMT_BGRA8888);
  buff = bitmap_lock_buffer_for_write(b1);
  ASSERT_EQ(buff[0], 0x12);
  ASSERT_EQ(buff[1], 0x12);
  ASSERT_EQ(buff[2], 0x12);
  ASSERT_EQ(buff[3], 0x12);
  bitmap_unlock_buffer(b1);

  bitmap_destroy(b1);
}

static ret_t clear_r(void* ctx, bitmap_t* bitmap, uint32_t x, uint32_t y, rgba_t* pixel) {
  pixel->r = *((uint8_t*)ctx);

  return RET_OK;
}

TEST(Bitmap, transform1) {
  rgba_t rgba;
  uint8_t red = 0x88;
  bitmap_t* b = bitmap_create_ex(10, 20, 0, BITMAP_FMT_BGRA8888);
  uint8_t* buff = bitmap_lock_buffer_for_write(b);
  memset(buff, 0x12, b->w * b->h * 4);
  bitmap_unlock_buffer(b);

  ASSERT_EQ(bitmap_transform(b, clear_r, &red), RET_OK);
  ASSERT_EQ((b->flags & BITMAP_FLAG_CHANGED) != 0, true);

  bitmap_get_pixel(b, 0, 0, &rgba);
  ASSERT_EQ(rgba.r, red);

  bitmap_get_pixel(b, 5, 5, &rgba);
  ASSERT_EQ(rgba.r, red);

  bitmap_destroy(b);
}

TEST(Bitmap, transform2) {
  rgba_t rgba;
  uint8_t red = 0x80;
  bitmap_t* b = bitmap_create_ex(10, 20, 0, BITMAP_FMT_BGR565);
  uint8_t* buff = bitmap_lock_buffer_for_write(b);
  memset(buff, 0x12, b->w * b->h * 2);
  bitmap_unlock_buffer(b);

  ASSERT_EQ(bitmap_transform(b, clear_r, &red), RET_OK);
  ASSERT_EQ((b->flags & BITMAP_FLAG_CHANGED) != 0, true);

  bitmap_get_pixel(b, 0, 0, &rgba);
  ASSERT_EQ(rgba.r, red);

  bitmap_get_pixel(b, 5, 5, &rgba);
  ASSERT_EQ(rgba.r, red);

  bitmap_destroy(b);
}
