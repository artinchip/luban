/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author                                      Notes
 * 2022-xx-xx     Ning Fang <ning.fang@artinchip.com>         create
 * 2023-11-1      Zequan Liang <zequan.liang@artinchip.com>   adapt to awtk
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "tkc/mem.h"
#include "tkc/fs.h"
#include "base/bitmap.h"
#include "frame_allocator.h"
#include "aic_dec_asset_frame.h"

#define PNG_HEADER_SIZE (8 + 12 + 13) /* png signature + IHDR chuck */
#define PNGSIG 0x89504e470d0a1a0aull
#define MNGSIG 0x8a4d4e470d0a1a0aull
#define JPEG_SOI 0xFFD8
#define JPEG_SOF 0xFFC0
#define ALIGN_1024B(x) ((x+1023) & (~1023))
#define ALIGN_16B(x) (((x) + (15)) & ~(15))

static inline uint64_t stream_to_u64(uint8_t *ptr) {
  return ((uint64_t)ptr[0] << 56) | ((uint64_t)ptr[1] << 48) |
         ((uint64_t)ptr[2] << 40) | ((uint64_t)ptr[3] << 32) |
         ((uint64_t)ptr[4] << 24) | ((uint64_t)ptr[5] << 16) |
         ((uint64_t)ptr[6] << 8) | ((uint64_t)ptr[7]);
}

static inline unsigned int stream_to_u32(uint8_t *ptr) {
  return ((unsigned int)ptr[0] << 24) |
         ((unsigned int)ptr[1] << 16) |
         ((unsigned int)ptr[2] << 8) |
         (unsigned int)ptr[3];
}

static inline unsigned short stream_to_u16(uint8_t *ptr) {
  return  ((unsigned int)ptr[0] << 8) | (unsigned int)ptr[1];
}

static ret_t get_jpeg_format(uint8_t *buf, enum mpp_pixel_format *pix_fmt) {
  int i;
  uint8_t h_count[3] = { 0 };
  uint8_t v_count[3] = { 0 };
  uint8_t nb_components = *buf++;

  for (i = 0; i < nb_components; i++) {
    uint8_t h_v_cnt;

    /* skip component id */
    buf++;
    h_v_cnt = *buf++;
    h_count[i] = h_v_cnt >> 4;
    v_count[i] = h_v_cnt & 0xf;

    /*skip quant_index*/
    buf++;
  }

  if (h_count[0] == 2 && v_count[0] == 2 && h_count[1] == 1 &&
      v_count[1] == 1 && h_count[2] == 1 && v_count[2] == 1) {
      *pix_fmt = MPP_FMT_YUV420P;
  } else if (h_count[0] == 4 && v_count[0] == 1 && h_count[1] == 1 &&
             v_count[1] == 1 && h_count[2] == 1 && v_count[2] == 1) {
    return RET_FAIL;
  } else if (h_count[0] == 2 && v_count[0] == 1 && h_count[1] == 1 &&
             v_count[1] == 1 && h_count[2] == 1 && v_count[2] == 1) {
    *pix_fmt = MPP_FMT_YUV422P;
  } else if (h_count[0] == 1 && v_count[0] == 1 && h_count[1] == 1 &&
             v_count[1] == 1 && h_count[2] == 1 && v_count[2] == 1) {
    *pix_fmt = MPP_FMT_YUV444P;
    } else if (h_count[0] == 1 && v_count[0] == 2 && h_count[1] == 1 &&
             v_count[1] == 2 && h_count[2] == 1 && v_count[2] == 2) {
    *pix_fmt = MPP_FMT_YUV444P;
  } else if (h_count[0] == 1 && v_count[0] == 2 && h_count[1] == 1 &&
             v_count[1] == 1 && h_count[2] == 1 && v_count[2] == 1) {
    return RET_FAIL;
  } else if (h_count[1] == 0 && v_count[1] == 0 && h_count[2] == 0 &&
             v_count[2] == 0) {
    *pix_fmt = MPP_FMT_YUV400;
  } else {
    log_debug("Not support format! h_count: %d %d %d, v_count: %d %d %d\n",
               h_count[0], h_count[1], h_count[2],
               v_count[0], v_count[1], v_count[2]);
    return RET_FAIL;
  }

  return RET_OK;
}

static ret_t jpeg_get_img_size(fs_file_t *fp, int *w, int *h, enum mpp_pixel_format *pix_fmt) {
  uint32_t read_num = 0;
  uint8_t buf[128] = {0};
  ret_t res = RET_FAIL;
  int size = 0;
  /* read JPEG SOI */
  read_num = fs_file_read(fp, buf, 2);
  if (read_num != 2) {
    log_debug("read JPEG SOI failed\n");
    return RET_FAIL;
  }

  /* check SOI */
  if (stream_to_u16(buf) != JPEG_SOI) {
    log_debug("check SOI failed\n");
    return RET_FAIL;
  }

  /* find SOF */
  while (1) {
    read_num = fs_file_read(fp, buf, 4);
    if (read_num != 4) {
        log_debug("find SOF failed\n");
        return RET_FAIL;
    }

    if (stream_to_u16(buf) == JPEG_SOF) {
      read_num = fs_file_read(fp, buf, 15);
      if (read_num != 15) {
        log_debug("get SOF failed\n");
        return RET_FAIL;
      }

      *h = stream_to_u16(buf + 1);
      *w = stream_to_u16(buf + 3);

      res = get_jpeg_format(buf + 5, pix_fmt);
      if (res != RET_OK) {
        log_debug("error parsing jpeg image\n");
        return RET_FAIL;
      }
      break;
    } else {
      size = stream_to_u16(buf + 2) + fs_file_tell(fp);
      res = fs_file_seek(fp, size - 2);
      if (res == RET_FAIL) {
        log_debug("jpeg seek size failed, size = %d\n", size);
        return RET_OK;
      }
    }
  }

  return RET_OK;
}

static ret_t jpeg_frame_info(int *w, int *h, enum mpp_pixel_format *pix_fmt, const char *path) {
  fs_file_t* fp = NULL;
  ret_t res = RET_FAIL;

  fp = fs_open_file(os_fs(), path, "rb");
  if (fp == NULL) {
    log_debug("open jpeg file failed\n");
    return RET_FAIL;
  }
  res = jpeg_get_img_size(fp, w, h, pix_fmt);
  if (res != RET_OK) {
    log_debug("get jpeg image size failed\n");
    return RET_FAIL;
  }
  fs_file_close(fp);

  return RET_OK;
}

static ret_t png_get_img_size(fs_file_t *fp, int *w, int *h, enum mpp_pixel_format *fomat) {
  uint32_t read_num = 0;
  unsigned char buf[64] = {0};
  int color_type = {0};

  read_num = fs_file_read(fp, buf, PNG_HEADER_SIZE);
  if (PNG_HEADER_SIZE != read_num) {
    return RET_FAIL;
  }

  *w = stream_to_u32(buf + 8 + 8);
  *h = stream_to_u32(buf + 8 + 8 + 4);

  color_type = buf[8 + 8 + 8 + 1];
  if (color_type == 2)
    *fomat = MPP_FMT_RGB_888;
  else
    *fomat = MPP_FMT_ARGB_8888;

  return RET_OK;
}

static ret_t png_frame_info(int *w, int *h, enum mpp_pixel_format *pix_fmt, const char *path) {
  fs_file_t* fp = NULL;
  ret_t res = RET_FAIL;

  fp = fs_open_file(os_fs(), path, "rb");
  if (fp == NULL) {
    log_debug("open png file failed\n");
    return RET_FAIL;
  }
  res = png_get_img_size(fp, w, h, pix_fmt);
  if (res != RET_OK) {
    log_debug("get png image size failed\n");
    return RET_FAIL;
  }
  fs_file_close(fp);

  return RET_OK;
}

ret_t aic_image_info(int *w, int *h, enum mpp_pixel_format *pix_fmt, const char *path) {
  char* ptr = NULL;

  ptr = strrchr(path, '.');
  if (!strcmp(ptr, ".png")) {
    return png_frame_info(w, h, pix_fmt, path);
  } else if ((!strcmp(ptr, ".jpg")) || (!strcmp(ptr, ".jpeg"))) {
    return jpeg_frame_info(w, h, pix_fmt, path);
  } else {
    return RET_FAIL;
  }

  return RET_OK;
}

struct ext_frame_allocator {
  struct frame_allocator base;
  struct mpp_frame* frame;
};

static int alloc_frame_buffer(struct frame_allocator *p, struct mpp_frame* frame,
                              int width, int height, enum mpp_pixel_format format) {
  struct ext_frame_allocator* impl = (struct ext_frame_allocator*)p;

  memcpy(frame, impl->frame, sizeof(struct mpp_frame));
  return 0;
}

static int free_frame_buffer(struct frame_allocator *p, struct mpp_frame *frame) {
  return 0;
}

static int close_allocator(struct frame_allocator *p) {
  struct ext_frame_allocator* impl = (struct ext_frame_allocator*)p;

  free(impl);

  return 0;
}

static struct alloc_ops def_ops = {
  .alloc_frame_buffer = alloc_frame_buffer,
  .free_frame_buffer = free_frame_buffer,
  .close_allocator = close_allocator,
};

static struct frame_allocator* open_allocator(struct mpp_frame* frame) {
  struct ext_frame_allocator* impl = (struct ext_frame_allocator*)malloc(sizeof(struct ext_frame_allocator));

  if(impl == NULL) {
    return NULL;
  }

  memset(impl, 0, sizeof(struct ext_frame_allocator));

  impl->frame = frame;
  impl->base.ops = &def_ops;
  return &impl->base;
}

/*
 * the application and release of framebuf asset are managed by awtk instead of decoder.
 * ensure that only framebuf resources are needed and there is no need to save the decoder.
 */
static void framebuf_asset_free(framebuf_asset *asset) {
 int i = 0;

 for (i = 0; i < 3; i++) {
  if (asset->cma_buf[i].size != 0)
    aic_cma_buf_free(&asset->cma_buf[i]);
  }
}

static int framebuf_asset_alloc(framebuf_asset *asset, int size[]) {
 int i = 0;
 int ret = RET_FAIL;

  for (i = 0; i < 3 && size[i] != 0; i++) {
    ret = aic_cma_buf_malloc(&asset->cma_buf[i], size[i]);
    if (ret < 0) {
      log_info("frambuf asset alloc failed\n");
      return RET_FAIL;
    }
    asset->frame.buf.fd[i] = asset->cma_buf[i].fd;
  }
  return RET_OK;
}

static void aic_asset_framebuf_get_info(int width, int height, enum mpp_pixel_format pix_fmt,
                                        framebuf_asset *asset, int buf_size[]) {
  int height_align = 0;

  memset(&asset->frame, 0, sizeof(struct mpp_frame));
  asset->frame.id = 0;
  asset->frame.buf.size.width = width;
  asset->frame.buf.size.height = height;
  asset->frame.buf.format = pix_fmt;
  asset->frame.buf.buf_type = MPP_DMA_BUF_FD;

  if (pix_fmt == MPP_FMT_YUV420P) {
    height_align = ALIGN_16B(height);
    asset->frame.buf.stride[0] =  ALIGN_16B(width);
    asset->frame.buf.stride[1] =  asset->frame.buf.stride[0] >> 1;
    asset->frame.buf.stride[2] =  asset->frame.buf.stride[0] >> 1;
    buf_size[0] = asset->frame.buf.stride[0] * height_align;
    buf_size[1] = asset->frame.buf.stride[1] * (height_align >> 1);
    buf_size[2] = asset->frame.buf.stride[2] * (height_align >> 1);
  } else if (pix_fmt == MPP_FMT_YUV422P) {
    height_align = ALIGN_16B(height);
    asset->frame.buf.stride[0] =  ALIGN_16B(width);
    asset->frame.buf.stride[1] =  asset->frame.buf.stride[0] >> 1;
    asset->frame.buf.stride[2] =  asset->frame.buf.stride[0] >> 1;
    buf_size[0] = asset->frame.buf.stride[0] * height_align;
    buf_size[1] = asset->frame.buf.stride[1] * height_align;
    buf_size[2] = asset->frame.buf.stride[2] * height_align;
  } else if (pix_fmt == MPP_FMT_YUV444P) {
    height_align = ALIGN_16B(height);
    asset->frame.buf.stride[0] =  ALIGN_16B(width);
    asset->frame.buf.stride[1] =  asset->frame.buf.stride[0];
    asset->frame.buf.stride[2] =  asset->frame.buf.stride[0];
    buf_size[0] = asset->frame.buf.stride[0] * height_align;
    buf_size[1] = asset->frame.buf.stride[1] * height_align;
    buf_size[2] = asset->frame.buf.stride[2] * height_align;
  } else if (pix_fmt == MPP_FMT_RGB_565) {
    height_align = ALIGN_16B(height);
    asset->frame.buf.stride[0] =  ALIGN_16B(width) * 2;
    buf_size[0] = asset->frame.buf.stride[0] * height_align;
  } else if (pix_fmt == MPP_FMT_RGB_888) {
    height_align = ALIGN_16B(height);
    asset->frame.buf.stride[0] =  ALIGN_16B(width * 3);
    buf_size[0] = asset->frame.buf.stride[0] * height_align;
  } else {
    asset->frame.buf.format = MPP_FMT_ARGB_8888;
    asset->frame.buf.stride[0] =  ALIGN_16B(width * 4);
    buf_size[0] = asset->frame.buf.stride[0] * height;
  }
}

framebuf_asset *aic_asset_get_frame(struct mpp_decoder* dec, int width, int height, int format) {
  int ret = 0;
  framebuf_asset *asset  = NULL;
  int size[3] = {0};

  if (dec == NULL) {
    log_info("can't get aic framebuf asset, dec == null\n");
    return NULL;
  }

  asset = TKMEM_ZALLOC(framebuf_asset);
  if (asset == NULL) {
    log_info("malloc framebuf asset failed\n");
    return NULL;
  }

  aic_asset_framebuf_get_info(width, height, format, asset, size);
  ret = framebuf_asset_alloc(asset, size);
  if (ret == RET_FAIL) {
    log_info("malloc framebuf asset failed, size = %d\n", size[0] + size[1] + size[2]);
    goto AIC_GET_FRAME_EXIT;
  }

  struct frame_allocator* allocator = open_allocator(&asset->frame);
  ret = mpp_decoder_control(dec, MPP_DEC_INIT_CMD_SET_EXT_FRAME_ALLOCATOR, (void*)allocator);
  if (ret < 0) {
    log_info("control decoder failed\n");
    goto AIC_GET_FRAME_EXIT;
  }

  return asset;

AIC_GET_FRAME_EXIT:
  if (allocator)
    close_allocator(allocator);

  framebuf_asset_free(asset);

  TKMEM_FREE(asset);

  return NULL;
}

int aic_asset_put_frame(framebuf_asset *asset) {
  framebuf_asset_free(asset);

  if (asset != NULL) {
    TKMEM_FREE(asset);
  }

  return 0;
}
