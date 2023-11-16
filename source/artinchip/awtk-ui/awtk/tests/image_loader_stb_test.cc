﻿#include "tkc/fs.h"
#include "tkc/mem.h"
#include "gtest/gtest.h"
#include "tools/common/utils.h"
#include "base/image_manager.h"
#include "base/assets_manager.h"
#include "tools/image_gen/image_gen.h"
#include "image_loader/image_loader_stb.h"

#define PNG_NAME TK_ROOT "/tests/testdata/test.png"
#define JPG_NAME TK_ROOT "/tests/testdata/test.jpg"
#define PNG_OPAQUE_NAME TK_ROOT "/tests/testdata/test_opaque.png"

static ret_t load_image(const char* filename, bitmap_t* image) {
  ret_t ret = RET_OK;
  image_loader_t* loader = image_loader_stb();
  int32_t size = file_get_size(filename);
  asset_info_t* info = asset_info_create(ASSET_TYPE_IMAGE, ASSET_TYPE_IMAGE_PNG, "name", size);
  return_value_if_fail(info != NULL, RET_OOM);

  ENSURE(file_read_part(filename, info->data, size, 0) == size);
  ret = image_loader_load(loader, info, image);
  asset_info_destroy(info);

  return ret;
}

TEST(ImageLoaderStb, basic) {
  bitmap_t image;
  memset(&image, 0x00, sizeof(image));

  ret_t ret = load_image(PNG_NAME, &image);

  ASSERT_EQ(ret, RET_OK);
  ASSERT_EQ(32, image.w);
  ASSERT_EQ(32, image.h);
  ASSERT_EQ(!!(image.flags & BITMAP_FLAG_IMMUTABLE), true);
  ASSERT_EQ(!!(image.flags & BITMAP_FLAG_OPAQUE), false);
  bitmap_destroy(&image);

  ret = load_image(JPG_NAME, &image);
  ASSERT_EQ(ret, RET_OK);
  ASSERT_EQ(32, image.w);
  ASSERT_EQ(32, image.h);
  ASSERT_EQ(!!(image.flags & BITMAP_FLAG_IMMUTABLE), true);
  ASSERT_EQ(!!(image.flags & BITMAP_FLAG_OPAQUE), true);
  bitmap_destroy(&image);

  ret = load_image(PNG_OPAQUE_NAME, &image);
  ASSERT_EQ(ret, RET_OK);
  ASSERT_EQ(32, image.w);
  ASSERT_EQ(32, image.h);
  ASSERT_EQ(!!(image.flags & BITMAP_FLAG_IMMUTABLE), true);
  ASSERT_EQ(!!(image.flags & BITMAP_FLAG_OPAQUE), true);
  bitmap_destroy(&image);
}

static ret_t load_image_ex(const char* filename, bitmap_t* image,
                           bitmap_format_t transparent_bitmap_format,
                           bitmap_format_t opaque_bitmap_format) {
  uint32_t size = 0;
  ret_t ret = RET_OK;
  printf("%s\n", filename);
  uint8_t* buff = (uint8_t*)read_file(filename, &size);
  ret = stb_load_image(0, buff, size, image, transparent_bitmap_format, opaque_bitmap_format,
                       LCD_ORIENTATION_0);
  TKMEM_FREE(buff);

  return ret;
}

TEST(ImageLoaderStb, bgr565_apaque) {
  bitmap_t image;
  ASSERT_EQ(load_image_ex(PNG_OPAQUE_NAME, &image, BITMAP_FMT_BGRA8888, BITMAP_FMT_BGR565), RET_OK);
  ASSERT_EQ(!!(image.flags & BITMAP_FLAG_OPAQUE), TRUE);
  ASSERT_EQ(image.format, BITMAP_FMT_BGR565);
  bitmap_destroy(&image);

  ASSERT_EQ(load_image_ex(PNG_OPAQUE_NAME, &image, BITMAP_FMT_BGRA8888, BITMAP_FMT_BGRA8888),
            RET_OK);
  ASSERT_EQ(!!(image.flags & BITMAP_FLAG_OPAQUE), TRUE);
  ASSERT_EQ(image.format, BITMAP_FMT_BGRA8888);
  bitmap_destroy(&image);
}

TEST(ImageLoaderStb, bgr565_trans) {
  bitmap_t image;
  ASSERT_EQ(load_image_ex(PNG_NAME, &image, BITMAP_FMT_BGRA8888, BITMAP_FMT_BGR565), RET_OK);
  ASSERT_EQ(!!(image.flags & BITMAP_FLAG_OPAQUE), FALSE);
  ASSERT_EQ(image.format, BITMAP_FMT_BGRA8888);
  bitmap_destroy(&image);

  ASSERT_EQ(load_image_ex(PNG_NAME, &image, BITMAP_FMT_BGRA8888, BITMAP_FMT_BGRA8888), RET_OK);
  ASSERT_EQ(!!(image.flags & BITMAP_FLAG_OPAQUE), FALSE);
  ASSERT_EQ(image.format, BITMAP_FMT_BGRA8888);
  bitmap_destroy(&image);
}

TEST(ImageLoaderStb, rgba) {
  bitmap_t image;
  ASSERT_EQ(load_image_ex(PNG_NAME, &image, BITMAP_FMT_RGBA8888, BITMAP_FMT_RGB565), RET_OK);
  ASSERT_EQ(!!(image.flags & BITMAP_FLAG_OPAQUE), FALSE);
  ASSERT_EQ(image.format, BITMAP_FMT_RGBA8888);
  bitmap_destroy(&image);

  ASSERT_EQ(load_image_ex(PNG_NAME, &image, BITMAP_FMT_BGRA8888, BITMAP_FMT_RGB565), RET_OK);
  ASSERT_EQ(!!(image.flags & BITMAP_FLAG_OPAQUE), FALSE);
  ASSERT_EQ(image.format, BITMAP_FMT_BGRA8888);
  bitmap_destroy(&image);
}

TEST(ImageLoaderStb, rgb) {
  bitmap_t image;

  ASSERT_EQ(load_image_ex(PNG_OPAQUE_NAME, &image, BITMAP_FMT_BGRA8888, BITMAP_FMT_RGB888), RET_OK);
  ASSERT_EQ(!!(image.flags & BITMAP_FLAG_OPAQUE), TRUE);
  ASSERT_EQ(image.format, BITMAP_FMT_RGB888);
  bitmap_destroy(&image);

  ASSERT_EQ(load_image_ex(PNG_NAME, &image, BITMAP_FMT_RGBA8888, BITMAP_FMT_RGB888), RET_OK);
  ASSERT_EQ(!!(image.flags & BITMAP_FLAG_OPAQUE), FALSE);
  ASSERT_EQ(image.format, BITMAP_FMT_RGBA8888);
  bitmap_destroy(&image);

  ASSERT_EQ(load_image_ex(PNG_NAME, &image, BITMAP_FMT_BGRA8888, BITMAP_FMT_RGB888), RET_OK);
  ASSERT_EQ(!!(image.flags & BITMAP_FLAG_OPAQUE), FALSE);
  ASSERT_EQ(image.format, BITMAP_FMT_BGRA8888);
  bitmap_destroy(&image);
}

TEST(ImageLoaderStb, bgr) {
  bitmap_t image;

  ASSERT_EQ(load_image_ex(PNG_OPAQUE_NAME, &image, BITMAP_FMT_BGRA8888, BITMAP_FMT_BGR888), RET_OK);
  ASSERT_EQ(!!(image.flags & BITMAP_FLAG_OPAQUE), TRUE);
  ASSERT_EQ(image.format, BITMAP_FMT_BGR888);
  bitmap_destroy(&image);

  ASSERT_EQ(load_image_ex(PNG_NAME, &image, BITMAP_FMT_RGBA8888, BITMAP_FMT_BGR888), RET_OK);
  ASSERT_EQ(!!(image.flags & BITMAP_FLAG_OPAQUE), FALSE);
  ASSERT_EQ(image.format, BITMAP_FMT_RGBA8888);
  bitmap_destroy(&image);

  ASSERT_EQ(load_image_ex(PNG_NAME, &image, BITMAP_FMT_BGRA8888, BITMAP_FMT_BGR888), RET_OK);
  ASSERT_EQ(!!(image.flags & BITMAP_FLAG_OPAQUE), FALSE);
  ASSERT_EQ(image.format, BITMAP_FMT_BGRA8888);
  bitmap_destroy(&image);
}

TEST(ImageLoaderStb, mono) {
  bitmap_t image;

  ASSERT_EQ(load_image_ex(PNG_OPAQUE_NAME, &image, BITMAP_FMT_BGRA8888, BITMAP_FMT_MONO), RET_OK);
  ASSERT_EQ(image.format, BITMAP_FMT_MONO);
  bitmap_destroy(&image);

  ASSERT_EQ(load_image_ex(PNG_NAME, &image, BITMAP_FMT_RGBA8888, BITMAP_FMT_MONO), RET_OK);
  ASSERT_EQ(image.format, BITMAP_FMT_MONO);
  bitmap_destroy(&image);
}
