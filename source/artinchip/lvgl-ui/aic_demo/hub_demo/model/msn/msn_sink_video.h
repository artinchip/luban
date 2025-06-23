/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors: artinchip
 */

#include "linksupport.h"
/**
   * @brief Video start callback
   */
void aic_video_start(LinkType type);
/**
   * @brief Video frame recv callback
   * @param datas [in] h264 video frame datas
   * @param len[in] frame data len
   * @param idrFrame[in] is idr frame(has sps nal and pps nal)
   */
void aic_video_play(LinkType type, void * datas, int len, bool idrFrame);

/**
   * @brief Video stop callback
   */
void aic_video_stop(LinkType type);
