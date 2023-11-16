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

#ifdef WITH_AIC_G2D
#include "mpp_decoder.h"

void aic_decode_asset_init(void);
struct mpp_decoder* aic_decode_asset_get(void);
void aic_decode_asset_release();
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*TK_AIC_ASSET_LOADER_H*/
