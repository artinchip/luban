/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Zequan Liang <zequan.liang@artinchip.com>
 */

#ifdef WITH_AIC_G2D

#include "aic_g2d.h"
#include "fb_info.h"
#include "base/g2d.h"
#include "base/bitmap.h"
#include "base/system_info.h"
#include "mpp_ge.h"
#include "mpp_decoder.h"

#include "aic_decode_asset.h"
#include "aic_graphic_buffer.h"
#include "stdio.h"

#define WITH_AIC_G2D_FILL_RECT_SIZE  5000

struct mpp_ge *g_ge = NULL;
image_loader_t* image_loader_aic();

/* there are currently issues:if hardware decoding of GE functionality fails, the entire system
 * will crash. The main problem lies in the resoure acquisition step. It will be fixed later.
 */
void tk_aic_g2d_open(void)
{
  log_info("tk_aic_g2d_open\n");
  if (g_ge == NULL)
  	g_ge = mpp_ge_open();

  aic_decode_asset_init();
  image_loader_register(image_loader_aic());
}

void tk_aic_g2d_close(void)
{
  log_info("awtk_aic_g2d_clse\n");
  if (g_ge)
      mpp_ge_close(g_ge);
}

int aic_cma_buf_add_ge(cma_buffer *data)
{
  int ret = -1;

  log_debug("data->type == %d, fd = %d, phy = 0x%08x\n", data->type, data->fd, data->phy_addr);
  if (data->type == FD_TYPE && data->fd > 0) {
    mpp_ge_add_dmabuf(g_ge, data->fd);
  }

  ret = aic_cma_buf_add(data);
  if (ret < 0) {
    log_error("aic_cma_buf_add err\n");
    return -1;
  }

  return ret;
}

int aic_cma_buf_find_ge(void *buf, cma_buffer *back)
{
  return aic_cma_buf_find(buf, back);
}

int aic_cma_buf_del_ge(void *buf)
{
  int ret = -1;
  cma_buffer node = { 0 };

  ret = aic_cma_buf_find(buf, &node);
  if (ret < 0) {
    log_error("cma_buf_hash_find err\n");
    return -1;
  }

  if (node.type == FD_TYPE)
    ret = mpp_ge_rm_dmabuf(g_ge, node.fd);
  if (ret < 0) {
    log_error("mpp_ge_rm_dmabuf err\n");
    return -1;
  }

  aic_cma_buf_del(node.buf);

  return 0;
}

static enum mpp_pixel_format tk_fmt_to_aic_fmt(bitmap_format_t awtk_fmt)
{
  enum mpp_pixel_format aic_fmt = 0;

  switch (awtk_fmt)
  {
  /* endian conversion */
  case BITMAP_FMT_RGBA8888:
    aic_fmt = MPP_FMT_ABGR_8888;
    break;
  case BITMAP_FMT_ABGR8888:
    aic_fmt = MPP_FMT_RGBA_8888;
    break;
  case BITMAP_FMT_BGRA8888:
    aic_fmt = MPP_FMT_ARGB_8888;
    break;
  case BITMAP_FMT_ARGB8888:
    aic_fmt = MPP_FMT_BGRA_8888;
    break;
  case BITMAP_FMT_RGB565:
    aic_fmt = MPP_FMT_BGR_565;
    break;
  case BITMAP_FMT_BGR565:
    aic_fmt = MPP_FMT_RGB_565;
    break;
  case BITMAP_FMT_RGB888:
    aic_fmt = MPP_FMT_BGR_888;
    break;
  case BITMAP_FMT_BGR888:
    aic_fmt = MPP_FMT_RGB_888;
    break;
  default:
    aic_fmt = RET_FAIL;
    break;
  }

  return aic_fmt;
}

ret_t g2d_fill_use(const rect_t* dst)
{
  int size = dst->w * dst->h;

  if (size < WITH_AIC_G2D_FILL_RECT_SIZE) {
    return FALSE;
  }

  return TRUE;
}

ret_t g2d_fill_rect(bitmap_t* fb, const rect_t* dst, color_t c) {
  int ret = RET_FAIL;
  struct ge_fillrect fill = { 0 };
  cma_buffer dst_buf;
  enum mpp_pixel_format dst_fmt;
  uint32_t aic_color = 0;
  uint8_t* dst_data = NULL;
  uint32_t dst_w = bitmap_get_physical_width(fb);
  uint32_t dst_h = bitmap_get_physical_height(fb);
  uint32_t dst_stride = bitmap_get_physical_line_length(fb);
  rgba_t rgba = c.rgba;
  bool_t dark = rgba.r == 0 && rgba.g == 0 && rgba.b == 0;

  return_value_if_fail(fb != NULL, RET_BAD_PARAMS);

  if (g2d_fill_use(dst) == FALSE) {
    return RET_FAIL;
  }

  dst_data = bitmap_lock_buffer_for_write(fb);
  return_value_if_fail(dst_data != NULL, RET_BAD_PARAMS);

  dst_fmt = tk_fmt_to_aic_fmt(fb->format);
  if (dst_fmt == (int)RET_FAIL) {
    log_error("Ge don't support format, format = %d\n", fb->format);
    goto FILL_ERR;
  }

  ret = aic_cma_buf_find_ge(dst_data, &dst_buf);
  if (ret < 0) {
#ifdef WITH_G2D_DEBUG
    log_debug("in file, can't find cma mem, dst_data = 0x%08x\n", (unsigned int)dst_data);
#endif
    goto FILL_ERR;
  }

  aic_color = (rgba.a << 24) | (rgba.r << 16) |
  	      (rgba.g << 8) | (rgba.b << 0);
  /* fill info */
  fill.type = GE_NO_GRADIENT;
  fill.start_color = aic_color;
  fill.end_color = 0;

  /* dst buf */
  if (dst_buf.type == FD_TYPE) {
    fill.dst_buf.buf_type = MPP_DMA_BUF_FD;
    fill.dst_buf.fd[0] = dst_buf.fd;
  } else if (dst_buf.type == PHY_TYPE) {
    fill.dst_buf.buf_type = MPP_PHY_ADDR;
    fill.dst_buf.phy_addr[0] = dst_buf.phy_addr;
  } else {
    goto FILL_ERR;
  }

  fill.dst_buf.stride[0] = dst_stride;
  fill.dst_buf.size.width = dst_w;
  fill.dst_buf.size.height = dst_h;
  fill.dst_buf.format = dst_fmt;
  fill.dst_buf.crop_en = 1;
  fill.dst_buf.crop.x = dst->x;
  fill.dst_buf.crop.y = dst->y;
  fill.dst_buf.crop.width = dst->w;
  fill.dst_buf.crop.height = dst->h;

  if (rgba.a > 0xf8) {
    fill.ctrl.alpha_en = 0;
  } else {
    /* out = dst * (1 - a) */
    if (dark) {
      fill.ctrl.alpha_rules = GE_PD_DST_OUT;
    } else {
    /* out = (src * a) + (dst * (1 - a)) */
      fill.ctrl.alpha_rules = GE_PD_NONE;
    }
  }
  fill.ctrl.src_alpha_mode = 1;
  fill.ctrl.src_global_alpha = rgba.a;

  ret = mpp_ge_fillrect(g_ge, &fill);
  if (ret < 0) {
    log_error("fillrect fail\n");
    goto FILL_ERR;
  }
  ret = mpp_ge_emit(g_ge);
  if (ret < 0) {
    log_error("emit fail\n");
    goto FILL_ERR;
  }
  ret = mpp_ge_sync(g_ge);
  if (ret < 0) {
    log_error("sync fail\n");
    goto FILL_ERR;
  }

  bitmap_unlock_buffer(fb);

  return RET_OK;

FILL_ERR:
  bitmap_unlock_buffer(fb);

  return RET_FAIL;
}

static ret_t aic_g2d_bltlet(bitmap_t* dst, bitmap_t* src,
			    const rect_t* dst_rect, const rect_t* src_rect,
                            uint8_t global_alpha, lcd_orientation_t o)
{
  int ret = RET_FAIL;
  struct ge_bitblt blt = { 0 };
  enum mpp_pixel_format src_fmt;
  enum mpp_pixel_format dst_fmt;
  cma_buffer src_buf;
  cma_buffer dst_buf;
  uint8_t* src_data = NULL;
  uint8_t* dst_data = NULL;
  uint32_t dst_stride = bitmap_get_physical_line_length(dst);
  uint32_t src_stride = bitmap_get_physical_line_length(src);
  uint32_t src_w = bitmap_get_physical_width(src);
  uint32_t src_h = bitmap_get_physical_height(src);
  uint32_t dst_w = bitmap_get_physical_width(dst);
  uint32_t dst_h = bitmap_get_physical_height(dst);

  return_value_if_fail(dst_rect != NULL && src_rect != NULL, RET_BAD_PARAMS);

  src_data = bitmap_lock_buffer_for_read(src);
  dst_data = bitmap_lock_buffer_for_write(dst);
  return_value_if_fail(src_data != NULL && dst_data != NULL, RET_BAD_PARAMS);

  src_fmt = tk_fmt_to_aic_fmt(src->format);
  dst_fmt = tk_fmt_to_aic_fmt(dst->format);
  if (dst_fmt == (int)RET_FAIL || src_fmt == (int)RET_FAIL) {
    log_error("Ge don't support format, src_fmt = %d, dst_fmt = %d\n", src->format, dst->format);
    goto BITBLT_ERR;
  }

  ret = aic_cma_buf_find_ge(src_data, &src_buf);
  if (ret < 0) {
#ifdef WITH_G2D_DEBUG
    log_error("in bitlet, can't find cma mem, src_data = 0x%08x\n", (unsigned int)src_data);
#endif
    goto BITBLT_ERR;
  }

  ret = aic_cma_buf_find_ge(dst_data, &dst_buf);
  if (ret < 0) {
#ifdef WITH_G2D_DEBUG
    log_error("in bitlet, can't find cma mem, dst_data = 0x%08x\n", (unsigned int)dst_data);
#endif
    goto BITBLT_ERR;
  }

  if (src_buf.type == FD_TYPE) {
    blt.src_buf.buf_type = MPP_DMA_BUF_FD;
    blt.src_buf.fd[0] =  src_buf.fd;
  } else if (src_buf.type == PHY_TYPE) {
    blt.src_buf.buf_type = MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0] = src_buf.phy_addr;
  } else {
    goto BITBLT_ERR;
  }

  /* src buf */
  blt.src_buf.stride[0] = src_stride;
  blt.src_buf.format = src_fmt;
  blt.src_buf.size.width = src_w;
  blt.src_buf.size.height = src_h;
  blt.src_buf.crop_en = 1;
  blt.src_buf.crop.x = src_rect->x;
  blt.src_buf.crop.y = src_rect->y;
  blt.src_buf.crop.width = src_rect->w;
  blt.src_buf.crop.height = src_rect->h;

  /* dst buf */
  if (dst_buf.type == FD_TYPE) {
    blt.dst_buf.buf_type = MPP_DMA_BUF_FD;
    blt.dst_buf.fd[0] =  dst_buf.fd;
  } else if (dst_buf.type == PHY_TYPE) {
    blt.dst_buf.buf_type = MPP_PHY_ADDR;
    blt.dst_buf.phy_addr[0] = dst_buf.phy_addr;
  } else {
    return RET_FAIL;
  }

  blt.dst_buf.stride[0] = dst_stride;
  blt.dst_buf.size.width = dst_w;
  blt.dst_buf.size.height = dst_h;
  blt.dst_buf.format = dst_fmt;
  blt.dst_buf.crop_en = 1;
  blt.dst_buf.crop.x = dst_rect->x;
  blt.dst_buf.crop.y = dst_rect->y;
  blt.dst_buf.crop.width = dst_rect->w;
  blt.dst_buf.crop.height = dst_rect->h;

  /* the rotation angle of awtk is counterclockwise, while aic it the opposite  */
  switch (o) {
  case LCD_ORIENTATION_0:
    blt.ctrl.flags |= MPP_ROTATION_0;
    break;
  case LCD_ORIENTATION_90:
    blt.ctrl.flags |= MPP_ROTATION_270;
    break;
  case LCD_ORIENTATION_180:
    blt.ctrl.flags |= MPP_ROTATION_180;
    break;
  case LCD_ORIENTATION_270:
    blt.ctrl.flags |= MPP_ROTATION_90;
    break;
  default:
    break;
  }

  if (global_alpha == 0 || global_alpha == 255)
    blt.ctrl.src_alpha_mode = 0;
  else
    blt.ctrl.src_alpha_mode = 2;
  blt.ctrl.alpha_en = 1;
  blt.ctrl.alpha_rules = 0;
  blt.ctrl.src_global_alpha = global_alpha;

  ret = mpp_ge_bitblt(g_ge, &blt);
  if (ret < 0) {
    log_error("bitblt fail\n");
    goto BITBLT_ERR;
  }
  ret = mpp_ge_emit(g_ge);
  if (ret < 0) {
    log_error("emit fail\n");
    goto BITBLT_ERR;
  }
  ret = mpp_ge_sync(g_ge);
  if (ret < 0) {
    log_error("sync fail\n");
    goto BITBLT_ERR;
  }

  bitmap_unlock_buffer(src);
  bitmap_unlock_buffer(dst);
  return RET_OK;

BITBLT_ERR:
  bitmap_unlock_buffer(src);
  bitmap_unlock_buffer(dst);

  return RET_FAIL;
}

ret_t g2d_copy_image(bitmap_t* fb, bitmap_t* img, const rect_t* src, xy_t x, xy_t y) {
  rect_t rect_dst = { 0 };

  rect_dst.x = x;
  rect_dst.y = y;
  rect_dst.w = src->w;
  rect_dst.h = src->h;

  return aic_g2d_bltlet(fb, img, &rect_dst, src, 0, 0);
}

static void rotate_get_rect_dst(bitmap_t* fb, bitmap_t* img, const rect_t* src,
                               lcd_orientation_t o, rect_t *rect_dst)
{
  uint32_t dst_w = bitmap_get_physical_width(fb);
  uint32_t dst_h = bitmap_get_physical_height(fb);
  uint32_t src_w = bitmap_get_physical_width(img);
  uint32_t src_h = bitmap_get_physical_height(img);

  switch (o) {
  case LCD_ORIENTATION_0:
    rect_dst->x = src->x;
    rect_dst->y = src->y;
    rect_dst->w = src->w;
    rect_dst->h = src->h;
    break;
  case LCD_ORIENTATION_90:
    rect_dst->x = src->y;
    rect_dst->y = src_w - src->x - src->w;
    rect_dst->w = src->h;
    rect_dst->h = src->w;
    break;
  case LCD_ORIENTATION_180:
    rect_dst->x = dst_w - src->w - src->x;
    rect_dst->y = dst_h - src->h - src->y;
    rect_dst->w = src->w;
    rect_dst->h = src->h;
    break;
  case LCD_ORIENTATION_270:
    rect_dst->x = src_h - src->y - src->h;
    rect_dst->y = src->x;
    rect_dst->w = src->h;
    rect_dst->h = src->w;
    break;
  }
}

ret_t g2d_rotate_image(bitmap_t* fb, bitmap_t* img, const rect_t* src,
                               lcd_orientation_t o) {
  rect_t rect_dst = { 0 };

  rotate_get_rect_dst(fb, img, src, o, &rect_dst);

  return aic_g2d_bltlet(fb, img, &rect_dst, src, 0, o);
}

ret_t g2d_blend_image(bitmap_t* fb, bitmap_t* img, const rect_t* dst, const rect_t* src,
                              uint8_t global_alpha) {
  return aic_g2d_bltlet(fb, img, dst, src, global_alpha, 0);
}

/*
static bool_t aic_image_is_opaque(const uint8_t* data, uint32_t w, uint32_t h, bitmap_format_t fmt) {
  int rgb_a_pos = -1;
  switch (fmt) {
  case BITMAP_FMT_RGBA8888:
    rgb_a_pos = 0;
    break;
  case BITMAP_FMT_ABGR8888:
    rgb_a_pos = 3;
    break;
  case BITMAP_FMT_BGRA8888:
    rgb_a_pos = 0;
    break;
  case BITMAP_FMT_ARGB8888:
    rgb_a_pos = 3;
    break;
  default:
    rgb_a_pos = -1;
    break;
  }

  if (rgb_a_pos != -1) {
    uint32_t i = 0;
    uint32_t n = w * h;
    const uint8_t* s = data;

    for (i = 0; i < n; i++) {
      if (s[rgb_a_pos] != 0xff) {
        return FALSE;
      }
      s += 4;
    }
  }

  return TRUE;
}
*/

static int aic_bitmap_fmt_check(bitmap_format_t format)
{
  switch (format) {
  case BITMAP_FMT_RGBA8888:
    return RET_OK;
  case BITMAP_FMT_ABGR8888:
    return RET_OK;
  case BITMAP_FMT_BGRA8888:
    return RET_OK;
  case BITMAP_FMT_ARGB8888:
    return RET_OK;
  case BITMAP_FMT_BGR888:
    return RET_OK;
  case BITMAP_FMT_RGB888:
    return RET_OK;
  case BITMAP_FMT_BGR565:
    return RET_OK;
  case BITMAP_FMT_RGB565:
    return RET_OK;
  default:
    return RET_NOT_IMPL;
  }

  return RET_NOT_IMPL;
}

static ret_t aic_bitmap_init(bitmap_t* bitmap, int width, int height,
                            bitmap_format_t format, lcd_orientation_t o)
{
  ret_t ret = RET_OK;
  return_value_if_fail(bitmap != NULL, RET_BAD_PARAMS);

  memset(bitmap, 0x00, sizeof(bitmap_t));

  ret = aic_bitmap_fmt_check(format);
  if (ret != RET_OK) {
    log_info("in aic_bitmap_init, aic_bitmap_fmt_check err, format = %d\n", format);
    return RET_FAIL;
  }

  bitmap->format = format;
  bitmap->flags = BITMAP_FLAG_IMMUTABLE;

  if (o == LCD_ORIENTATION_0 || o == LCD_ORIENTATION_180) {
    bitmap->w = width;
    bitmap->h = height;
  } else {
    bitmap->w = height;
    bitmap->h = width;
  }

  /* graphic buffer saves the actual width and height */
  bitmap_set_line_length(bitmap, 0);
  return_value_if_fail(aic_graphic_buffer_create_for_bitmap(bitmap) == RET_OK, RET_OOM);
  if (o != LCD_ORIENTATION_0) {
    bitmap->orientation = o;
    bitmap->flags |= BITMAP_FLAG_LCD_ORIENTATION;
  }

  /* set the width and height of bitmap */
  bitmap->w = width;
  bitmap->h = height;
  bitmap_set_line_length(bitmap, 0);

  return RET_OK;
}

static ret_t aic_g2d_image_transform(struct mpp_frame *frame, bitmap_t* image,
			             bitmap_format_t transparent_bitmap_format, lcd_orientation_t o)
{
  int ret = RET_FAIL;
  struct ge_bitblt blt = { 0 };
  enum mpp_pixel_format dst_fmt;
  cma_buffer dst_buf;
  /* physical line、data、width and height is set to graphic_buffer_aic_t */
  uint8_t* dst_data = NULL;
  uint32_t dst_stride = bitmap_get_physical_line_length(image);
  uint32_t dst_w = bitmap_get_physical_width(image);
  uint32_t dst_h = bitmap_get_physical_height(image);

  dst_data = bitmap_lock_buffer_for_write(image);
  return_value_if_fail(frame != NULL && dst_data != NULL, RET_BAD_PARAMS);

  dst_fmt = tk_fmt_to_aic_fmt(transparent_bitmap_format);
  if (dst_fmt == (int)RET_FAIL) {
    log_info("Ge don't support format, dst_fmt = %d\n", transparent_bitmap_format);
    goto IMAGE_TRANSFORM_ERR;
  }

  /* add cma_buf when create graphic_buffer_aic_t */
  ret = aic_cma_buf_find_ge(dst_data, &dst_buf);
  if (ret < 0) {
    goto IMAGE_TRANSFORM_ERR;
  }

  blt.src_buf.buf_type = MPP_DMA_BUF_FD;
  if (frame->buf.format == MPP_FMT_ARGB_8888
        || frame->buf.format == MPP_FMT_RGBA_8888
        || frame->buf.format == MPP_FMT_RGB_888) {
    mpp_ge_add_dmabuf(g_ge, frame->buf.fd[0]);
    blt.src_buf.fd[0] = frame->buf.fd[0];
    blt.src_buf.stride[0] = frame->buf.stride[0];
    blt.src_buf.format = frame->buf.format;
  } else if (frame->buf.format == MPP_FMT_YUV420P
        || frame->buf.format == MPP_FMT_YUV444P
        || frame->buf.format == MPP_FMT_YUV422P) {
      mpp_ge_add_dmabuf(g_ge, frame->buf.fd[0]);
      mpp_ge_add_dmabuf(g_ge, frame->buf.fd[1]);
      mpp_ge_add_dmabuf(g_ge, frame->buf.fd[2]);
      blt.src_buf.fd[0] = frame->buf.fd[0];
      blt.src_buf.fd[1] = frame->buf.fd[1];
      blt.src_buf.fd[2] = frame->buf.fd[2];
      blt.src_buf.stride[0] = frame->buf.stride[0];
      blt.src_buf.stride[1] = frame->buf.stride[1];
      blt.src_buf.stride[2] = frame->buf.stride[2];
      blt.src_buf.format = frame->buf.format;
    } else if (frame->buf.format == MPP_FMT_NV12
        || frame->buf.format == MPP_FMT_NV21
        || frame->buf.format == MPP_FMT_NV16
        || frame->buf.format == MPP_FMT_NV61) {
      mpp_ge_add_dmabuf(g_ge, frame->buf.fd[0]);
      mpp_ge_add_dmabuf(g_ge, frame->buf.fd[1]);
      blt.src_buf.fd[0] = frame->buf.fd[0];
      blt.src_buf.fd[1] = frame->buf.fd[1];
      blt.src_buf.stride[0] = frame->buf.stride[0];
      blt.src_buf.stride[1] = frame->buf.stride[1];
      blt.src_buf.format = frame->buf.format;
    }

  blt.src_buf.size.width = frame->buf.size.width;
  blt.src_buf.size.height = frame->buf.size.height;
  blt.src_buf.crop_en = 0;

  /* dst buf */
  if (dst_buf.type == FD_TYPE) {
    blt.dst_buf.buf_type = MPP_DMA_BUF_FD;
    blt.dst_buf.fd[0] =  dst_buf.fd;
  } else {
    log_error("In aic_g2d_image_transform, dst_buf.type != FD_TYPE\n");
    return RET_FAIL;
  }

  blt.dst_buf.stride[0] = dst_stride;
  blt.dst_buf.size.width = dst_w;
  blt.dst_buf.size.height = dst_h;
  blt.dst_buf.format = dst_fmt;
  blt.dst_buf.crop_en = 0;

  blt.ctrl.alpha_en = 0;
  /* the rotation angle of awtk is counterclockwise, while aic it the opposite  */
  switch (o) {
  case LCD_ORIENTATION_0:
    blt.ctrl.flags |= MPP_ROTATION_0;
    blt.dst_buf.crop.width = dst_w;
    blt.dst_buf.crop.height = dst_h;
    break;
  case LCD_ORIENTATION_90:
    blt.ctrl.flags |= MPP_ROTATION_270;
    blt.dst_buf.crop.width = dst_h;
    blt.dst_buf.crop.height = dst_w;
    break;
  case LCD_ORIENTATION_180:
    blt.ctrl.flags |= MPP_ROTATION_180;
    blt.dst_buf.crop.width = dst_w;
    blt.dst_buf.crop.height = dst_h;
    break;
  case LCD_ORIENTATION_270:
    blt.ctrl.flags |= MPP_ROTATION_90;
    blt.dst_buf.crop.width = dst_h;
    blt.dst_buf.crop.height = dst_w;
    break;
  default:
    break;
  }

  ret = mpp_ge_bitblt(g_ge, &blt);
  if (ret < 0) {
    log_error("bitblt fail\n");
    goto IMAGE_TRANSFORM_ERR;
  }
  ret = mpp_ge_emit(g_ge);
  if (ret < 0) {
    log_error("emit fail\n");
    goto IMAGE_TRANSFORM_ERR;
  }
  ret = mpp_ge_sync(g_ge);
  if (ret < 0) {
    log_error("sync fail\n");
    goto IMAGE_TRANSFORM_ERR;
  }

  bitmap_unlock_buffer(image);
  return RET_OK;

IMAGE_TRANSFORM_ERR:
  bitmap_unlock_buffer(image);
  return RET_FAIL;
}

static ret_t hw_image_loader(struct mpp_decoder* dec, bitmap_t* image,
                             bitmap_format_t transparent_bitmap_format, lcd_orientation_t o)
{
  int ret = -1;
  struct mpp_frame frame = { 0 };
  uint8_t* data = NULL;
  uint32_t data_w = bitmap_get_physical_width(image);
  uint32_t data_h = bitmap_get_physical_height(image);

  mpp_decoder_decode(dec);
  ret = mpp_decoder_get_frame(dec, &frame);
  if (ret < 0) {
    log_error("mpp decoder get frame failed:%d\n", ret);
    aic_decode_asset_release(dec);
    return RET_FAIL;
  }

  /* init bitmap */
  ret = aic_bitmap_init(image, frame.buf.size.width, frame.buf.size.height,
                        transparent_bitmap_format, o);
  if (ret != RET_OK) {
    log_error("in hw_image_loader, aic_bitmap_init err\n");
    goto DREE_DEC;
  }

  /* GE transform, include rotate */
  ret = aic_g2d_image_transform(&frame, image, transparent_bitmap_format, o);
  if (ret != RET_OK) {
    log_error("in hw_image_loader, aic_g2d_image_transform err\n");
    goto DREE_DEC;
  }

  data = bitmap_lock_buffer_for_write(image);
  image->flags |= BITMAP_FLAG_NONE;
  /*
   * low efficiency of line by line judgment
   * if (aic_image_is_opaque(data, data_w, data_h, transparent_bitmap_format)) {
   *   image->flags |= BITMAP_FLAG_OPAQUE;
   * }
  */

  bitmap_unlock_buffer(image);
  mpp_decoder_put_frame(dec, &frame);
  aic_decode_asset_release(dec);
  return RET_OK;

DREE_DEC:
  mpp_decoder_put_frame(dec, &frame);
  aic_decode_asset_release(dec);
  return RET_FAIL;
}

static ret_t image_loader_aic_load(image_loader_t* l, const asset_info_t* asset, bitmap_t* image) {
  ret_t ret = RET_FAIL;
  system_info_t* info = system_info();
  lcd_orientation_t o = LCD_ORIENTATION_0;
  bitmap_format_t transparent_bitmap_format = BITMAP_FMT_RGBA8888;
  return_value_if_fail(l != NULL && image != NULL && info != NULL, RET_BAD_PARAMS);

  if (asset->subtype != ASSET_TYPE_IMAGE_JPG && asset->subtype != ASSET_TYPE_IMAGE_PNG) {
    log_debug("in image_loader_aic_load, hw decode in not support, subtype = %d\n", asset->subtype);
    return RET_NOT_IMPL;
  }

#if !defined(WITH_GPU) && !defined(WITH_VGCANVAS_CAIRO) && defined(WITH_FAST_LCD_PORTRAIT)
  if (system_info()->flags & SYSTEM_INFO_FLAG_FAST_LCD_PORTRAIT) {
    o = info->lcd_orientation;
  }
#endif

#ifdef WITHOUT_FAST_LCD_PORTRAIT_FOR_IMAGE
  o = LCD_ORIENTATION_0;
#endif

#ifdef WITH_BITMAP_BGR565
  transparent_bitmap_format = BITMAP_FMT_BGR565;
#elif defined(WITH_BITMAP_RGB565)
  transparent_bitmap_format = BITMAP_FMT_RGB565;
#elif defined(WITH_BITMAP_BGR888)
  transparent_bitmap_format = BITMAP_FMT_BGR888;
#elif defined(WITH_BITMAP_RGB888)
  transparent_bitmap_format = BITMAP_FMT_RGB888;
#endif

#ifdef WITH_BITMAP_BGRA
  transparent_bitmap_format = BITMAP_FMT_BGRA8888;
#endif

  struct mpp_decoder* dec = aic_decode_asset_get();
  if (dec == NULL) {
    log_debug("in image_loader_aic_load, dec is NULL\n");
    return RET_FAIL;
  } else {
    ret = hw_image_loader(dec, image, transparent_bitmap_format, o);
  }

  return ret;
}

static const image_loader_t aic_loader = {.load = image_loader_aic_load};

image_loader_t* image_loader_aic() {
  return (image_loader_t*)&aic_loader;
}

#endif
