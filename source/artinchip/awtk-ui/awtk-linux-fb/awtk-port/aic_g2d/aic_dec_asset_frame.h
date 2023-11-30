/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author                                      Notes
 * 2022-xx-xx     Ning Fang <ning.fang@artinchip.com>         create
 * 2023-11-14     Zequan Liang <zequan.liang@artinchip.com>   adapt to awtk for linux
 */

#ifndef __AIC_ASSET_FRAME_H
#define __AIC_ASSET_FRAME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mpp_decoder.h"
#include "aic_linux_mem.h"

typedef struct _framebuf_asset {
  cma_buffer cma_buf[3];
  struct mpp_frame frame;
}framebuf_asset;

ret_t aic_image_info(int *w, int *h, enum mpp_pixel_format *pix_fmt, const char *path);
framebuf_asset *aic_asset_get_frame(struct mpp_decoder* dec, int width, int height, int format);
int aic_asset_put_frame(framebuf_asset *info);

#ifdef AIC_DEC_ASSET_FRAME_DEBUG
void aic_asset_frame_debug(void);
#endif

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* __AIC_ASSET_FRAME_H */
