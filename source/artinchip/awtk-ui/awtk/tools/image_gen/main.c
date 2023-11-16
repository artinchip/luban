/**
 * File:   main.c
 * Author: AWTK Develop Team
 * Brief:  bitmap font generator
 *
 * Copyright (c) 2018 - 2020  Guangzhou ZHIYUAN Electronics Co.,Ltd.
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
 * 2018-01-21 Li XianJing <xianjimli@hotmail.com> created
 *
 */

#include "tkc/fs.h"
#include "tkc/path.h"
#include "tkc/mem.h"
#include "image_gen.h"
#include "common/utils.h"
#include "base/image_manager.h"
#include "image_loader/image_loader_stb.h"

typedef struct _image_format_t {
  bitmap_format_t opaque_bitmap_format;
  bitmap_format_t transparent_bitmap_format;
} image_format_t;

ret_t image_format_set(image_format_t* image_format, const wchar_t* format) {
  if (format != NULL) {
    if (wcsstr(format, L"bgra")) {
      image_format->opaque_bitmap_format = BITMAP_FMT_BGRA8888;
      image_format->transparent_bitmap_format = BITMAP_FMT_BGRA8888;
    }

    if (wcsstr(format, L"bgr565")) {
      image_format->opaque_bitmap_format = BITMAP_FMT_BGR565;
    } else if (wcsstr(format, L"rgb565")) {
      image_format->opaque_bitmap_format = BITMAP_FMT_RGB565;
    } else if (wcsstr(format, L"bgr888")) {
      image_format->opaque_bitmap_format = BITMAP_FMT_BGR888;
    } else if (wcsstr(format, L"rgb888")) {
      image_format->opaque_bitmap_format = BITMAP_FMT_RGB888;
    }

    if (wcsstr(format, L"mono")) {
      image_format->opaque_bitmap_format = BITMAP_FMT_MONO;
    }
  }
  return RET_OK;
}

ret_t gen_one(const char* input_file, const char* output_file, const char* theme,
              image_format_t* image_format, lcd_orientation_t o) {
  ret_t ret = RET_OK;
  if (!exit_if_need_not_update(input_file, output_file)) {
    bitmap_t image;
    uint32_t size = 0;
    uint8_t* buff = NULL;
    buff = (uint8_t*)read_file(input_file, &size);
    if (buff != NULL) {
      ret = stb_load_image(0, buff, size, &image, image_format->transparent_bitmap_format,
                           image_format->opaque_bitmap_format, o);
      if (ret == RET_OK) {
        ret = image_gen(&image, output_file, theme,
                        image_format->opaque_bitmap_format == BITMAP_FMT_MONO);
      }
      TKMEM_FREE(buff);
    } else {
      ret = RET_FAIL;
    }
    if (ret != RET_OK) {
      GEN_ERROR(input_file);
    }
  }
  return ret;
}

static ret_t gen_folder(const char* in_foldername, const char* out_foldername, const char* theme,
                        image_format_t* image_format, lcd_orientation_t o) {
  fs_item_t item;
  ret_t ret = RET_OK;
  char in_name[MAX_PATH] = {0};
  char out_name[MAX_PATH] = {0};
  fs_dir_t* dir = fs_open_dir(os_fs(), in_foldername);

  while (fs_dir_read(dir, &item) != RET_FAIL) {
    if (item.is_reg_file) {
      str_t str_name;
      char ext_array[MAX_PATH] = {0};
      path_extname(item.name, ext_array, MAX_PATH);

      str_init(&str_name, 0);
      str_set(&str_name, item.name);
      str_replace(&str_name, ext_array, "");
      filter_name(str_name.str);
      str_append(&str_name, ".data");
      path_build(in_name, MAX_PATH, in_foldername, item.name, NULL);
      path_build(out_name, MAX_PATH, out_foldername, str_name.str, NULL);
      ret = gen_one(in_name, out_name, theme, image_format, o);
      str_reset(&str_name);
      if (ret != RET_OK) {
        break;
      }
    } else if (item.is_dir && !tk_str_eq(item.name, ".") && !tk_str_eq(item.name, "..")) {
      path_build(in_name, MAX_PATH, in_foldername, item.name, NULL);
      path_build(out_name, MAX_PATH, out_foldername, item.name, NULL);

      if (!fs_dir_exist(os_fs(), out_name)) {
        fs_create_dir(os_fs(), out_name);
      }

      ret = gen_folder(in_name, out_name, theme, image_format, o);
      if (ret != RET_OK) {
        break;
      }
    }
  }
  fs_dir_close(dir);
  return RET_OK;
}

int wmain(int argc, wchar_t* argv[]) {
  const char* in_filename = NULL;
  const char* out_filename = NULL;
  const wchar_t* format = NULL;
  lcd_orientation_t lcd_orientation = LCD_ORIENTATION_0;

  platform_prepare();

  if (argc < 3) {
    printf("Usage: %S in_filename out_filename (bgra|bgr565|rgb565|bgr888|rgb888|mono)\n", argv[0]);

    return 0;
  }

  if (argc > 3) {
    format = argv[3];
  }
  image_format_t image_format = {BITMAP_FMT_RGBA8888, BITMAP_FMT_RGBA8888};
  image_format_set(&image_format, format);

  str_t theme_name;
  str_init(&theme_name, 0);
  if (argc > 4) {
    str_from_wstr(&theme_name, argv[4]);
  }

  if (argc > 5) {
    wstr_t str_lcd_orientation;
    int tmp_lcd_orientation = 0;
    wstr_init(&str_lcd_orientation, 0);
    wstr_append(&str_lcd_orientation, argv[5]);
    if (wstr_to_int(&str_lcd_orientation, &tmp_lcd_orientation) == RET_OK) {
      lcd_orientation = (lcd_orientation_t)tmp_lcd_orientation;
    }
    wstr_reset(&str_lcd_orientation);
  }

  str_t in_file;
  str_t out_file;

  str_init(&in_file, 0);
  str_init(&out_file, 0);

  str_from_wstr(&in_file, argv[1]);
  str_from_wstr(&out_file, argv[2]);

  in_filename = in_file.str;
  out_filename = out_file.str;

  fs_stat_info_t in_stat_info;
  fs_stat_info_t out_stat_info;
  fs_stat(os_fs(), in_filename, &in_stat_info);
  fs_stat(os_fs(), out_filename, &out_stat_info);
  if (in_stat_info.is_dir == TRUE && out_stat_info.is_dir == TRUE) {
    gen_folder(in_filename, out_filename, theme_name.str, &image_format, lcd_orientation);
  } else if (in_stat_info.is_reg_file == TRUE) {
    gen_one(in_filename, out_filename, theme_name.str, &image_format, lcd_orientation);
  } else {
    GEN_ERROR(in_filename);
  }

  str_reset(&in_file);
  str_reset(&out_file);
  str_reset(&theme_name);

  return 0;
}

#include "common/main.inc"
