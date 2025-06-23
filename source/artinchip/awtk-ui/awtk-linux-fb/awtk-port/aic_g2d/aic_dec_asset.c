/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Zequan Liang <zequan.liang@artinchip.com>
 */

#include "tkc/mem.h"
#include "tkc/path.h"
#include "tkc/utils.h"
#include "tkc/fs.h"
#include "base/types_def.h"
#include "base/asset_loader.h"
#include "base/assets_manager.h"
#include "base/image_manager.h"
#include "aic_dec_asset.h"
#include "mpp_decoder.h"

#define RAW_DIR "raw"
#define ASSETS_DIR "assets"
#define THEME_DEFAULT "default"

#ifdef WITH_AIC_AWTK_DEBUG
#define AIC_DEC_ASSET_DEBUG
#endif

extern int lcd_format_get(void);
extern enum mpp_pixel_format tk_fmt_to_aic_fmt(bitmap_format_t tk_format);
asset_info_t* aic_assets_manager_load_impl(assets_manager_t* am, asset_type_t type,
                                                  uint16_t subtype, const char* name);

static struct asset_list aic_dec_mgr_list_head;

void aic_dec_asset_debug(void) {
  int num = 0;
  int frame_size = 0;
  aic_dec_asset *now_asset, *next_asset;
  asset_list_for_each_entry_safe(now_asset, next_asset, &aic_dec_mgr_list_head, list) {
    num++;
    frame_size = now_asset->frame_asset->cma_buf[0].size +
                 now_asset->frame_asset->cma_buf[1].size +
                 now_asset->frame_asset->cma_buf[2].size;
    log_debug("asset debug, num = %d, name = %s, frame_size = %d, fd = %d\n",
               num, now_asset->name, frame_size,
               (unsigned int)now_asset->frame.buf.fd[0]);
  }
}

static int aic_dec_asset_add(aic_dec_asset *asset, asset_info_t* info) {
  aic_dec_asset *new_asset;

  new_asset = TKMEM_ZALLOC(aic_dec_asset);
  memcpy(new_asset, asset, sizeof(aic_dec_asset));
  asset_list_add_tail(&new_asset->list, &aic_dec_mgr_list_head);
}

int aic_decode_asset_init(void) {
  int ret = -1;
  /* get default manager */
  assets_manager_t* assert = assets_manager();
  ret = assets_manager_set_custom_load_asset(assert, aic_assets_manager_load_impl, assert);
  if (ret != RET_OK) {
    log_error("assets_manager_set_custom_load_asset failed\n");
    return -1;
  }

  asset_list_init(&aic_dec_mgr_list_head);

  return 0;
}

aic_dec_asset *aic_dec_asset_get(const asset_info_t* info) {
  aic_dec_asset *now_asset, *next_asset;

  asset_list_for_each_entry_safe(now_asset, next_asset, &aic_dec_mgr_list_head, list) {
    if (info->flags & ASSET_INFO_FLAG_FULL_NAME) {
      if (now_asset->image_type == info->subtype &&
          strncmp(now_asset->name, info->name.full_name, TK_FUNC_NAME_LEN) == 0) {
        return now_asset;
      }
    } else {
      if (now_asset->image_type == info->subtype &&
          strncmp(now_asset->name, info->name.small_name, TK_FUNC_NAME_LEN) == 0) {
        return now_asset;
      }
    }
  }

  return NULL;
}

int aic_dec_asset_del(aic_dec_asset *asset) {
  aic_dec_asset *now_asset, *next_asset;

  asset_list_for_each_entry_safe(now_asset, next_asset, &aic_dec_mgr_list_head, list) {
    if (now_asset->image_type == asset->image_type &&
        strncmp(now_asset->name, asset->name, TK_FUNC_NAME_LEN) == 0) {

        if (now_asset->frame_asset != NULL)
          aic_asset_put_frame(now_asset->frame_asset);

        asset_list_del(&now_asset->list);

        TKMEM_FREE(now_asset);
    }
  }

  return 0;
}

static int tk_asset_subtype_to_aic_decode_type(uint16_t subtype) {
  switch (subtype) {
  case ASSET_TYPE_IMAGE_PNG:
    return MPP_CODEC_VIDEO_DECODER_PNG;
  case ASSET_TYPE_IMAGE_JPG:
    return MPP_CODEC_VIDEO_DECODER_MJPEG;
  default:
    return -1;
  }
}

static int get_expected_decode_format(int format, int is_png)
{
#ifdef AIC_VE_DRV_V10
  if (is_png) {
    if (format >= MPP_FMT_ARGB_8888 && format <= MPP_FMT_BGR_888) {
      return format;
    } else if (format >= MPP_FMT_RGB_565 && format <= MPP_FMT_BGR_565) {
      return MPP_FMT_RGB_888;
    } else if (format >= MPP_FMT_ARGB_1555 && format <= MPP_FMT_BGRA_5551 ||
               format >= MPP_FMT_ARGB_4444 && format <= MPP_FMT_BGRA_4444) {
      return MPP_FMT_ARGB_8888;
    } else {
      return -1;
    }
  } else {
    return format;
  }
#else
  if (is_png) {
    if (format >= MPP_FMT_ARGB_8888 && format <= MPP_FMT_BGR_888) {
      return format;
    } else if (format >= MPP_FMT_RGB_565 && format <= MPP_FMT_BGR_565) {
      return MPP_FMT_RGB_888;
    } else if (format >= MPP_FMT_ARGB_1555 && format <= MPP_FMT_BGRA_5551 ||
               format >= MPP_FMT_ARGB_4444 && format <= MPP_FMT_BGRA_4444) {
      return MPP_FMT_ARGB_8888;
    } else {
      return -1;
    }
  } else {
    switch (format) {
    case MPP_FMT_ARGB_8888:
    case MPP_FMT_ABGR_8888:
    case MPP_FMT_RGBA_8888:
    case MPP_FMT_BGRA_8888:
    case MPP_FMT_RGB_888:
    case MPP_FMT_BGR_888:
    case MPP_FMT_RGB_565:
    case MPP_FMT_BGR_565:
      return format;
    default:
/* canvas animation did't supported RGB888 format */
#ifndef WITHOUT_WINDOW_ANIMATORS
      return MPP_FMT_RGB_565;
#endif
      return MPP_FMT_RGB_888;
    }
  }
#endif

  return -1;
}

#ifdef AIC_DEC_ASSET_DEBUG
static void save_image(void *data, int size, const char *name) {
  char path[126] = {0};
  snprintf(path, sizeof(path), "/sdcard/%s", name);
  if (file_write(path, data, size) == RET_FAIL) {
    log_debug("save image faile, path = %s\n", path);
  }
}
#endif

static asset_info_t* aic_asset_info_create(uint16_t type, uint16_t subtype, const char* name,
                                           int32_t size, aic_dec_asset *dec_asset, const char *path) {
  int ret = -1;
  int width = 0;
  int height = 0;
  enum mpp_pixel_format format = 0;
  int decoder_get_frame = 0;
  int expect_format = -1;
  struct decode_config config;
  struct mpp_packet packet;
  struct mpp_frame frame;
  asset_info_t* info = NULL;
  return_value_if_fail(name != NULL, NULL);

  info = TKMEM_ALLOC(sizeof(asset_info_t));
  return_value_if_fail(info != NULL, NULL);

  memset(info, 0x00, (sizeof(asset_info_t)));
  memset(dec_asset, 0x00, sizeof(aic_dec_asset));

  info->size = size;
  info->type = type;
  info->subtype = subtype;
  info->refcount = 1;

  asset_info_set_is_in_rom(info, FALSE);
  asset_info_set_name(info, name, TRUE);  /* here malloc a str(info->name.full_name) */

  int decoder_type = tk_asset_subtype_to_aic_decode_type(subtype);
  if (decoder_type == -1) {
    goto ASSERT_CEATE_EXIT;
  }
  dec_asset->dec = mpp_decoder_create(decoder_type);
  if (dec_asset->dec == NULL) {
    log_info("mpp decoder create failed\n");
    goto ASSERT_CEATE_EXIT;
  }

  memset(&config, 0, sizeof(struct decode_config));
  config.bitstream_buffer_size = (size + 1023) & (~1023);
  config.extra_frame_num = 0;
  config.packet_count = 1;

  ret = aic_image_info(&width, &height, &format, path);
  if (ret == RET_FAIL) {
    log_info("can't get image info, path = %s\n", path);
    goto ASSERT_CEATE_EXIT;
  }

/* decode to screen format to avoid further conversion, but may also lose image information. */
#ifdef WITH_DEC_FRAME_BUFFER_FMT
  if(decoder_type == MPP_CODEC_VIDEO_DECODER_PNG)
    expect_format = get_expected_decode_format(lcd_format_get(), 1);
  else
    expect_format = get_expected_decode_format(lcd_format_get(), 0);
  if (expect_format < 0) {
    log_info("did't supported format, origin format = %d\n", lcd_format_get());
    goto ASSERT_CEATE_EXIT;
  }
#else
  if(decoder_type == MPP_CODEC_VIDEO_DECODER_PNG) {
    expect_format = get_expected_decode_format(format, 1);
  } else {
    expect_format = get_expected_decode_format(format, 0);
  }
  if (expect_format < 0) {
    log_info("did't supported format, origin format = %d\n", format);
    goto ASSERT_CEATE_EXIT;
  }
#endif

  /* log_debug("raw data path = %s, w = %d, h = %d, fmt = %d, expect_fmt = %d, decoder_type = %d, sub_type = %d\n",
             path,  width, height, format, expect_format, decoder_type, subtype); */
  dec_asset->frame_asset = aic_asset_get_frame(dec_asset->dec, width, height, expect_format);
  if (dec_asset->frame_asset == NULL) {
    log_info("can't get aic frame asset\n");
    goto ASSERT_CEATE_EXIT;
  }

#ifdef AIC_VE_DRV_V10
  if(decoder_type == MPP_CODEC_VIDEO_DECODER_MJPEG) {
    config.pix_fmt = MPP_FMT_YUV420P;
  } else if(decoder_type == MPP_CODEC_VIDEO_DECODER_PNG) {
    config.pix_fmt = dec_asset->frame_asset->frame.buf.format;
  }
#else
  config.pix_fmt = dec_asset->frame_asset->frame.buf.format;
#endif

  ret = mpp_decoder_init(dec_asset->dec, &config);
  if (ret < 0) {
    log_info("mpp_decoder_init err, dec = 0x%08x\n", (unsigned int)dec_asset->dec);
    goto ASSERT_CEATE_EXIT;
  }

  memset(&packet, 0, sizeof(struct mpp_packet));
  ret = mpp_decoder_get_packet(dec_asset->dec, &packet, size);
  if (ret < 0) {
    log_info("mpp_decoder_get_packet err, dec = 0x%08x\n", (unsigned int)dec_asset->dec);
    goto ASSERT_CEATE_EXIT;
  }
  strncpy(dec_asset->name, name, TK_FUNC_NAME_LEN);
  dec_asset->image_type = subtype;
  packet.size = size;
  packet.flag = PACKET_FLAG_EOS;

  if (file_read_part(path, packet.data, size, 0) != size) {
    log_info("file_read_part read file filed, name = %s\n", name);
    goto ASSERT_CEATE_EXIT;
  }

  /* put the packet to mpp_decoder */
  ret = mpp_decoder_put_packet(dec_asset->dec, &packet);
  if (ret < 0) {
    log_info("Can't put back dec packet\n");
    goto ASSERT_CEATE_EXIT;
  }

  ret = mpp_decoder_decode(dec_asset->dec);
  if (ret < 0) {
    log_error("mpp decoder decode failed:%d\n", ret);
    goto ASSERT_CEATE_EXIT;
  }

  memset(&frame, 0, sizeof(struct mpp_frame));
  ret = mpp_decoder_get_frame(dec_asset->dec, &frame);
  if (ret < 0) {
    log_error("mpp decoder get frame failed:%d\n", ret);
    goto ASSERT_CEATE_EXIT;
  }
  decoder_get_frame = 1;
  memcpy(&dec_asset->frame, &frame, sizeof(struct mpp_frame));
  mpp_decoder_put_frame(dec_asset->dec, &frame);
  decoder_get_frame = 0;

#ifdef AIC_DEC_ASSET_DEBUG
  if (strncmp(name, "bg_main",strlen("bg_main")) == 0)
    save_image((void *)dec_asset->frame.buf.phy_addr[0],
    dec_asset->frame.buf.stride[0] * dec_asset->frame.buf.size.height,
    "bg_main.rgb");

  log_debug("decode data path = %s, w = %d, h = %d, fmt = %d, stride = %d\n",
    path,  dec_asset->frame.buf.size.width, dec_asset->frame.buf.size.height,
    dec_asset->frame.buf.format, dec_asset->frame.buf.stride[0]);
#endif

  mpp_decoder_destory(dec_asset->dec);

  return info;

ASSERT_CEATE_EXIT:
  if (decoder_get_frame)
    mpp_decoder_put_frame(dec_asset->dec, &frame);
  if (dec_asset->frame_asset != NULL)
    aic_asset_put_frame(dec_asset->frame_asset);
  if (dec_asset->dec != NULL)
    mpp_decoder_destory(dec_asset->dec);

  TKMEM_FREE(info);
  return NULL;
}

static asset_info_t* aic_load_asset_from_file(uint16_t type, uint16_t subtype, const char* path,
                                              const char* name, aic_dec_asset *dec_asset) {
  asset_info_t* info = NULL;
  if (file_exist(path)) {
    int32_t size = file_get_size(path);
    info = aic_asset_info_create(type, subtype, name, size, dec_asset, path);
    return_value_if_fail(info != NULL, NULL);
  }

  return info;
}

static int aic_try_get_path(assets_manager_t* am, const char* theme, const char* name,
                                    asset_image_type_t subtype, bool_t ratio, char *path) {
  const char* extname = NULL;
  const char* subpath = ratio ? "images" : "images/xx";

  switch (subtype) {
    case ASSET_TYPE_IMAGE_JPG: {
      extname = ".jpg";
      break;
    }
    case ASSET_TYPE_IMAGE_PNG: {
      extname = ".png";
      break;
    }
    default: {
      return -1;
    }
  }

  return_value_if_fail(assets_manager_build_asset_filename(am, path, MAX_PATH, theme, ratio,
                                                           subpath, name, extname) == RET_OK, -1);

  if (subtype == ASSET_TYPE_IMAGE_JPG && !asset_loader_exist(am->loader, path)) {
    uint32_t len = strlen(path);
    return_value_if_fail(MAX_PATH > len, NULL);
    memcpy(path + len - 4, ".jpeg", 5);
    path[len + 1] = '\0';
  }

  return 0;
}

static uint16_t subtype_from_extname(const char* extname) {
  uint16_t subtype = 0;
  return_value_if_fail(extname != NULL, 0);

  if (tk_str_ieq(extname, ".gif")) {
    subtype = ASSET_TYPE_IMAGE_GIF;
  } else if (tk_str_ieq(extname, ".png")) {
    subtype = ASSET_TYPE_IMAGE_PNG;
  } else if (tk_str_ieq(extname, ".bmp")) {
    subtype = ASSET_TYPE_IMAGE_BMP;
  } else if (tk_str_ieq(extname, ".bsvg")) {
    subtype = ASSET_TYPE_IMAGE_BSVG;
  } else if (tk_str_ieq(extname, ".jpg")) {
    subtype = ASSET_TYPE_IMAGE_JPG;
  } else if (tk_str_ieq(extname, ".jpeg")) {
    subtype = ASSET_TYPE_IMAGE_JPG;
  } else if (tk_str_ieq(extname, ".ttf")) {
    subtype = ASSET_TYPE_FONT_TTF;
  } else {
    log_debug("not supported %s\n", extname);
  }

  return subtype;
}

static asset_info_t* aic_assets_manager_load(assets_manager_t* am, asset_type_t type,
                                             uint16_t subtype, const char* name,
                                             const char *path) {
  asset_info_t* info = NULL;
  aic_dec_asset dec_asset = {0};

  if (name == NULL) {
    return NULL;
  }
  const char* extname = strrchr(path, '.');
  int aic_subtype = subtype_from_extname(extname);

  /* currently, we are only reading images from the file system and decoding them. */
  if (aic_subtype == ASSET_TYPE_IMAGE_JPG || aic_subtype == ASSET_TYPE_IMAGE_PNG) {
    info = aic_load_asset_from_file(type, aic_subtype, path, name, &dec_asset);
    if (info == NULL) {
      return NULL;
    }

    /* add in aic dec asset manager */
    if (info != NULL) {
      aic_dec_asset_add(&dec_asset, info);
    }
    return info;
  }

  return info;
}

static asset_info_t* aic_assets_manager_load_asset(assets_manager_t* am, asset_type_t type,
                                               uint16_t subtype, const char* theme,
                                               const char* name) {
  int ret = -1;
  char path[MAX_PATH + 1];
  asset_info_t* info = NULL;

  if (type != ASSET_TYPE_IMAGE) {
    return NULL;
  }

  ret = aic_try_get_path(am, theme, name, ASSET_TYPE_IMAGE_PNG, TRUE, path);
  if (ret == 0 && info == NULL) {
    info = aic_assets_manager_load(am, type, subtype, name, path);
  }

  ret = aic_try_get_path(am, theme, name, ASSET_TYPE_IMAGE_JPG, TRUE, path);
  if (ret == 0 && info == NULL) {
    info = aic_assets_manager_load(am, type, subtype, name, path);
  }

  /*try ratio-insensitive image.*/
  ret = aic_try_get_path(am, theme, name, ASSET_TYPE_IMAGE_PNG, FALSE, path);
  if (ret == 0 && info == NULL) {
    info = aic_assets_manager_load(am, type, subtype, name, path);
  }

  ret = aic_try_get_path(am, theme, name, ASSET_TYPE_IMAGE_JPG, FALSE, path);
  if (ret == 0 && info == NULL) {
    info = aic_assets_manager_load(am, type, subtype, name, path);
  }

  return info;
}

asset_info_t* aic_assets_manager_load_impl(assets_manager_t* am, asset_type_t type,
                                                  uint16_t subtype, const char* name) {
  asset_info_t* info = NULL;

  if (name == NULL) {
    return NULL;
  }

  /* currently, we are only reading images from the file system and decoding them. */
  if (strncmp(name, STR_SCHEMA_FILE, strlen(STR_SCHEMA_FILE)) == 0) {
    const char* path = name + strlen(STR_SCHEMA_FILE);
    const char* extname = strrchr(path, '.');
    int aic_subtype = subtype_from_extname(extname);

    info = aic_assets_manager_load(am, type, aic_subtype, name, path);
  } else {
    const char* theme = am->theme ? am->theme : THEME_DEFAULT;
    info = aic_assets_manager_load_asset(am, type, subtype, theme, name);
    if (info == NULL && !tk_str_eq(theme, THEME_DEFAULT)) {
      info = aic_assets_manager_load_asset(am, type, subtype, THEME_DEFAULT, name);
    }
  }

  /* return to default loading */
  return info;
}
