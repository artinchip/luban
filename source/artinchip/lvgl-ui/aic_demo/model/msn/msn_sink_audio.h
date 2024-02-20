/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors: artinchip
 */

#include "linksupport.h"
/**
   * @brief Audio start callback
   * @param streamType [in] audio stream type
   * @param audioType [in] audio type
   * @param rate [in] audio sample rate
   * @param format[in] audio format usually is 16 (PCM_FORMAT_S16_LE)
   * @param channel[in] audio channel
   */
void aic_audio_start(LinkType type, int streamType, int audioType, int rate, int format, int channel);

/**
   * @brief Audio frame recv callback
   * @param streamType [in] audio stream type
   * @param audioType [in] audio type
   * @param datas [in] pcm frame datas
   * @param len[in] frame data len (is not frame size)
   */
void aic_audio_play(LinkType type, int streamType, int audioType, void * datas, int len);

/**
   * @brief Audio stop callback
   * @param streamType [in] audio stream type
   * @param audioType [in] audio type
   */
void aic_audio_stop(LinkType type, int streamType, int audioType);
