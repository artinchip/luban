/*
 * Copyright (C) 2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AICDECODETHREAD_H
#define AICDECODETHREAD_H

#include <QThread>
#include <QMutex>

#ifdef QTLAUNCHER_GE_SUPPORT
#include <video/mpp_types.h>
#include <linux/dma-heap.h>
#include <dma_allocator.h>
#include <frame_allocator.h>
#include <mpp_decoder.h>
#include <mpp_dec_type.h>
#include <mpp_ge.h>
#endif

class AiCDecodeThread : public QThread
{
    Q_OBJECT
public:
    AiCDecodeThread(void *data);
    ~AiCDecodeThread();

#ifdef QTLAUNCHER_GE_SUPPORT
protected:
    void run();

private:
    void *mData;
#endif
};

#endif /* AICDECODETHREAD_H */
