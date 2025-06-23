/*
 * Copyright (C) 2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AICRANDERTHREAD_H
#define AICRANDERTHREAD_H

#include <QThread>

#ifdef QTLAUNCHER_GE_SUPPORT
#include <video/mpp_types.h>
#include <linux/dma-heap.h>
#include <dma_allocator.h>
#include <frame_allocator.h>
#include <mpp_decoder.h>
#include <mpp_dec_type.h>
#include <mpp_ge.h>
#endif

#define RENDER_FRAME_NUM_MAX    1300

class AiCRenderThread : public QThread
{
    Q_OBJECT
public:
    AiCRenderThread(void *data);
    ~AiCRenderThread();

#ifdef QTLAUNCHER_GE_SUPPORT
    void stop();

protected:
    void run();

private:
    void *mData;
    bool mStop;
#endif
};

#endif /* AICRANDERTHREAD_H */
