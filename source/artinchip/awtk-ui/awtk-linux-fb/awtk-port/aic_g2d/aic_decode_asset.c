/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Zequan Liang <zequan.liang@artinchip.com>
 */

#ifdef WITH_AIC_G2D

#include "tkc/mem.h"
#include "tkc/path.h"
#include "tkc/utils.h"
#include "tkc/fs.h"
#include "tkc/asset_info.h"
#include "base/types_def.h"
#include "base/asset_loader.h"
#include "base/assets_manager.h"
#include "base/image_manager.h"

#include "aic_decode_asset.h"
#include "mpp_decoder.h"

struct mpp_decoder* g_decoder = NULL;
static struct mpp_packet packet;

#define AIC_ASSET_DEBUG

static int tk_asset_type_to_aic_asset_type(uint16_t subtype)
{
  switch (subtype)
  {
  case ASSET_TYPE_IMAGE_PNG:
    return MPP_CODEC_VIDEO_DECODER_PNG;
  case ASSET_TYPE_IMAGE_JPG:
    return MPP_CODEC_VIDEO_DECODER_MJPEG;
  default:
    return -1;
  }
}

static asset_info_t* aic_asset_info_create(uint16_t type, uint16_t subtype, const char* name, int32_t size) {
  int ret = -1;
  struct decode_config config;
  asset_info_t* info = NULL;
  return_value_if_fail(name != NULL, NULL);

  info = TKMEM_ALLOC(sizeof(asset_info_t));
  return_value_if_fail(info != NULL, NULL);

  memset(info, 0x00, (sizeof(asset_info_t)));

  info->size = size;
  info->type = type;
  info->subtype = subtype;
  info->refcount = 1;
  info->map = NULL;

  asset_info_set_is_in_rom(info, FALSE);
  /* here malloc a str(info->name.full_name) */
  asset_info_set_name(info, name, TRUE);

  int decoder_type = tk_asset_type_to_aic_asset_type(subtype);
  if (decoder_type == -1) {
    goto ASSERT_CEATE_EXIT;
  }

  memset(&config, 0, sizeof(struct decode_config));
  config.bitstream_buffer_size = (size + 1023) & (~1023);
  config.extra_frame_num = 0;
  config.packet_count = 1;
  if(decoder_type == MPP_CODEC_VIDEO_DECODER_MJPEG)
    config.pix_fmt = MPP_FMT_YUV420P;
  else if(type == MPP_CODEC_VIDEO_DECODER_PNG)
    config.pix_fmt = MPP_FMT_ARGB_8888;

  g_decoder = mpp_decoder_create(decoder_type);
  ret = mpp_decoder_init(g_decoder, &config);
  if (ret < 0) {
    log_error("in aic_asset_info_create, mpp_decoder_init err, g_decoder = 0x%08x\n", g_decoder);
    goto ASSERT_CEATE_EXIT;
  }
  memset(&packet, 0, sizeof(struct mpp_packet));
  ret = mpp_decoder_get_packet(g_decoder, &packet, size);
  if (ret < 0) {
    log_error("in aic_asset_info_create, mpp_decoder_get_packet err, g_decoder = 0x%08x\n", g_decoder);
    goto ASSERT_CEATE_EXIT;
  }

  packet.size = size;
  packet.flag = PACKET_FLAG_EOS;

  /*
   * note that info->data LOAD_ASSET_WITH_MMAP is not defined as a unsigned data[4] type here.
   * The main reason why the info->data is not used during hardware acceleration if for safety
   * reasons.
  */
  info->data = NULL;

  return info;

ASSERT_CEATE_EXIT:
  log_error("in aic_asset_info_create, ASSERT_CEATE_EXIT\n");
  TKMEM_FREE(info);
  return NULL;
}

static asset_info_t* aic_load_asset_from_file(uint16_t type, uint16_t subtype, const char* path,
                                              const char* name) {
  asset_info_t* info = NULL;
  if (file_exist(path)) {
    int32_t size = file_get_size(path);
    info = aic_asset_info_create(type, subtype, name, size);
    return_value_if_fail(info != NULL, NULL);

    ENSURE(file_read_part(path, packet.data, size, 0) == size);
  }

  return info;
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

asset_info_t* aic_assets_manager_load_impl(assets_manager_t* am, asset_type_t type,
                                                  uint16_t subtype, const char* name) {
  int ret = -1;
  asset_info_t* info = NULL;

  if (name == NULL) {
    return NULL;
  }
  /* currently, we are only reading images from the file system and decoding them. */
  if (strncmp(name, STR_SCHEMA_FILE, strlen(STR_SCHEMA_FILE)) == 0) {
    const char* path = name + strlen(STR_SCHEMA_FILE);
    const char* extname = strrchr(path, '.');
    subtype = subtype_from_extname(extname);
    if (subtype == ASSET_TYPE_IMAGE_JPG || subtype == ASSET_TYPE_IMAGE_PNG) {
      info = aic_load_asset_from_file(type, subtype, path, name);
      if (info == NULL) {
        log_error("in aic_assets_manager_load_impl, aic_load_asset_from_file err\n");
        aic_decode_asset_release();
        return NULL;
      }
      /* put the packet to mpp_decoder */
      ret = mpp_decoder_put_packet(g_decoder, &packet);
      if (ret < 0) {
        log_error("in aic_assets_manager_load_impl, mpp_decoder_put_packet err\n");
        aic_decode_asset_release();
        return NULL;
      }
        /* maintain the same logic as assets_manager_add internally
           in the assets_manager_load_asset function */
      if (info != NULL && assets_manager_is_save_assets_list(type)) {
        assets_manager_add(am, info);
      }
      return info;
    }
  } else {
    log_debug("in aic_assets_manager_load_impl, load from other, name = %s\n",name);
    return NULL;
  }
  /* return to default loading */
  return NULL;
}

void aic_decode_asset_init(void)
{
  int ret = -1;
  /* get default manager */
  image_manager_t* imm = image_manager();
  ret = assets_manager_set_custom_load_asset(imm->assets_manager, aic_assets_manager_load_impl, NULL);
  if (ret != RET_OK) {
    log_error("in aic_decode_asset_init, assets_manager_set_custom_load_asset err\n");
  }
}

struct mpp_decoder* aic_decode_asset_get(void)
{
  return g_decoder;
}

void aic_decode_asset_release()
{
  if (g_decoder != NULL) {
    mpp_decoder_destory(g_decoder);
  }

  g_decoder = NULL;
}

#endif
