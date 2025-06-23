/*
 * Copyright (C) 2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aicdecodethread.h"
#include "aicvideothread.h"

#include <QDebug>

#ifdef QTLAUNCHER_GE_SUPPORT
AiCDecodeThread::AiCDecodeThread(void *data)
{
    mData = data;
}

AiCDecodeThread::~AiCDecodeThread()
{

}

void AiCDecodeThread::run()
{
     struct dec_ctx *data = (struct dec_ctx*)mData;
     int ret = 0;
     int i;

     while(!data->render_eos) {
         for (i = 0; i < DECODER_NUM; i++) {
             while(1) {
                 if(data->render_eos)
                     goto out;
                 ret = mpp_decoder_decode(data->decoder[i]);
                 if (ret == DEC_NO_READY_PACKET || ret == DEC_NO_EMPTY_FRAME) {
                     usleep(1000);
                     continue;
                 } else if (ret) {
                     qDebug("decode ret: %x", ret);
                     data->dec_err = 1;
                     break;
                 }
                 /* decoder finish one frame */
                 break;
             }
         }
         /* all of decoders finish one frame */
         data->decode_num++;
     }

 out:
     qDebug("decode thread exit");
}
#else /* QTLAUNCHER_GE_SUPPORT */
AiCDecodeThread::AiCDecodeThread(void *data)
{
    (void)data;
}

AiCDecodeThread::~AiCDecodeThread()
{

}
#endif

