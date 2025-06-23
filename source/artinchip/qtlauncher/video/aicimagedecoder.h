/*
 * Copyright (C) 2024-2025 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: Huahui Mai <huahui.mai@artinchip.com>
 */

#ifndef AICIMAGEDECODER_H
#define AICIMAGEDECODER_H

#include <QWidget>
#include <QFile>

#ifdef QTLAUNCHER_GE_SUPPORT

#include <linux/fb.h>
#include <video/artinchip_fb.h>
#include <video/mpp_types.h>
#include <linux/dma-heap.h>
#include <dma_allocator.h>
#include <frame_allocator.h>
#include <mpp_decoder.h>
#include <mpp_dec_type.h>
#include <mpp_ge.h>
#endif

#define DECODER_PNG_OUTPUT_FORMAT       MPP_FMT_ARGB_8888
#define DECODER_MJPEG_OUTPUT_FORMAT     MPP_FMT_YUV420P

class AiCImageDecoder : public QWidget
{
    Q_OBJECT
public:
    AiCImageDecoder();
    ~AiCImageDecoder();

#ifdef QTLAUNCHER_GE_SUPPORT
    void decodeImage(const char *fileName, unsigned int x, unsigned y);

private:
        int mScreenW;
        int mScreenH;
        int mFbStride;
        mpp_pixel_format mFbFormat;
        unsigned int mFbPhy;

private:
    void mpp_rander_frame(struct mpp_frame *frame, unsigned int x, unsigned y);
#endif
};

#endif // AICIMAGEDECODER_H
