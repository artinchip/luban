/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Zequan Liang <zequan.liang@artinchip.com>
 */

#ifndef TK_AIC_ASSET_LOADER_H
#define TK_AIC_ASSET_LOADER_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "mpp_decoder.h"
#include "tkc/asset_info.h"
#include "aic_dec_asset_list.h"
#include "aic_dec_asset_frame.h"

typedef struct __aic_dec_asset {
  char name[TK_FUNC_NAME_LEN + 1];
  int image_type;
  framebuf_asset *frame_asset;
  struct mpp_frame frame; /* hardware decoded frame */
  struct mpp_decoder* dec;
  struct asset_list list;
} aic_dec_asset;

int aic_decode_asset_init(void);
aic_dec_asset *aic_dec_asset_get(const asset_info_t* info);
int aic_dec_asset_del(aic_dec_asset *asset);
void aic_dec_asset_debug(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*TK_AIC_ASSET_LOADER_H*/
