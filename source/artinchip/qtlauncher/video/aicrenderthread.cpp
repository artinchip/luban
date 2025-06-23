/*
 * Copyright (C) 2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aicrenderthread.h"
#include "aicvideothread.h"
#include "../utils/aicconsts.h"

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <video/artinchip_fb.h>

#include <QDebug>

#ifdef QTLAUNCHER_GE_SUPPORT
AiCRenderThread::AiCRenderThread(void *data)
{
    mData = data;
    mStop = false;
}

AiCRenderThread::~AiCRenderThread()
{

}

void AiCRenderThread::stop(void)
{
    mStop = true;
}

static void video_layer_set(int fb0_fd, struct mpp_buf *picture_buf, struct frame_info* frame)
{
    struct aicfb_layer_data layer;
    struct dma_buf_info dmabuf_fd[3];
    int dmabuf_num = 0;
    int i;

    if (fb0_fd < 0)
        return;

    memset(&layer, 0x0, sizeof(struct aicfb_layer_data));

    layer.layer_id = AICFB_LAYER_TYPE_VIDEO;
    layer.enable = 1;
    if (picture_buf->format == MPP_FMT_YUV420P ||
        picture_buf->format == MPP_FMT_NV12    ||
        picture_buf->format == MPP_FMT_NV21) {
          // rgb format not support scale
        layer.scale_size.width  = SCREEN_WIDTH;
        layer.scale_size.height = SCREEN_HEIGHT;
    }

    picture_buf->crop_en     = 0;
    picture_buf->crop.x      = 0;
    picture_buf->crop.y      = 0;
    picture_buf->crop.width  = 0;
    picture_buf->crop.height = 0;

    layer.pos.x = 0;
    layer.pos.y = AIC_STATUS_BAR_HEIGHT;
    memcpy(&layer.buf, picture_buf, sizeof(struct mpp_buf));

    if (picture_buf->format == MPP_FMT_ARGB_8888) {
        dmabuf_num = 1;
    } else if (picture_buf->format == MPP_FMT_RGBA_8888) {
        dmabuf_num = 1;
    } else if (picture_buf->format == MPP_FMT_RGB_888) {
        dmabuf_num = 1;
    } else if (picture_buf->format == MPP_FMT_YUV420P) {
        dmabuf_num = 3;
    } else if (picture_buf->format == MPP_FMT_NV12 || picture_buf->format == MPP_FMT_NV21) {
        dmabuf_num = 2;
    } else if (picture_buf->format == MPP_FMT_YUV444P) {
        dmabuf_num = 3;
    } else if (picture_buf->format == MPP_FMT_YUV422P) {
        dmabuf_num = 3;
    } else if (picture_buf->format == MPP_FMT_YUV400) {
        dmabuf_num = 1;
    } else {
       qDebug("no support picture foramt %d, default argb8888", picture_buf->format);
    }

    //* add dmabuf to de driver
    if(!frame->used) {
        for(i = 0; i < dmabuf_num; i++) {
            frame->fd[i] = picture_buf->fd[i];
            dmabuf_fd[i].fd = picture_buf->fd[i];

            if (ioctl(fb0_fd, AICFB_ADD_DMABUF, &dmabuf_fd[i]) < 0)
                qDebug("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");
        }
        frame->used = 1;
        frame->fd_num = dmabuf_num;
    }

    //* display
    if (ioctl(fb0_fd, AICFB_UPDATE_LAYER_CONFIG, &layer) < 0)
        qDebug("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");

    //* wait vsync (wait layer config)
    ioctl(fb0_fd, AICFB_WAIT_FOR_VSYNC, NULL);
}

static void swap(int *a, int *b)
{
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

void AiCRenderThread::run()
{
     int fb0_fd = -1;
     int cur_frame_id = 0;
     int last_frame_id = 1;
     struct mpp_frame frame[2];
     struct mpp_buf *pic_buffer = NULL;
     int frame_num = 0;
     int i, j, ret;
     int disp_frame_cnt = 0;
     struct dec_ctx *data = (struct dec_ctx*)mData;
     int display_num = 0;

     //* 1. open fb0
     fb0_fd = open("/dev/fb0", O_RDWR);
     if (fb0_fd < 0) {
         qDebug("open fb0 failed!, exit render thender\n");
         return;
     }

     //* 2. render frame until eos
     while(!data->render_eos) {
         memset(&frame[cur_frame_id], 0, sizeof(struct mpp_frame));

         if(data->dec_err)
             break;

         if(display_num > RENDER_FRAME_NUM_MAX  || mStop) {
             qDebug("render exit, display_num: %d", display_num);
             data->render_eos = 1;
             break;
         }

         //* 2.1 get frame
         for (i = 0; i < DECODER_NUM; i++) {
             while (1) {
                 ret = mpp_decoder_get_frame(data->decoder[i], &frame[cur_frame_id]);
                 if (ret == DEC_NO_RENDER_FRAME || ret == DEC_ERR_FM_NOT_CREATE
                     || ret == DEC_NO_EMPTY_FRAME || ret == DEC_ERR_FM_NOT_CREATE) {
                     usleep(10000);
                     continue;
                 } else if (ret) {
                     qDebug("mpp_dec_get_frame error, ret: %x", ret);
                     data->dec_err = 1;
                     break;
                 } else {
                     // get frame success
                     break;
                 }
             }
         }

         disp_frame_cnt += 1;
         data->render_eos = frame[cur_frame_id].flags & FRAME_FLAG_EOS;

         pic_buffer = &frame[cur_frame_id].buf;
         //* 2.2 compare data
         // save_data(data, pic_buffer, fp_save);

         //* 2.3 disp frame;
         video_layer_set(fb0_fd, pic_buffer, &data->frame_info[frame[cur_frame_id].id]);

         //* 2.4 return the last frame
         if (frame_num) {
             for (i = 0; i < DECODER_NUM; i++)
                 mpp_decoder_put_frame(data->decoder[i], &frame[last_frame_id]);
         }

         swap(&cur_frame_id, &last_frame_id);

         if(disp_frame_cnt > FRAME_COUNT)
             disp_frame_cnt = 0;

         frame_num++;
         display_num++;
     }

     //* put the last frame when eos
     for (i = 0; i < DECODER_NUM; i++)
         mpp_decoder_put_frame(data->decoder[i], &frame[last_frame_id]);

     //* disable video layer
     struct aicfb_layer_data layer;
     memset(&layer, 0x0, sizeof(struct aicfb_layer_data));

     layer.enable = 0;
     if (ioctl(fb0_fd, AICFB_UPDATE_LAYER_CONFIG, &layer) < 0)
         qDebug("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");

     //* remove all dmabuf from de driver
     for(i = 0; i < FRAME_BUF_NUM; i++) {
         if(data->frame_info[i].used == 0)
             continue;

         for(j = 0; j < data->frame_info[i].fd_num; j++) {
             if (ioctl(fb0_fd, AICFB_RM_DMABUF, &data->frame_info[i].fd[j]) < 0)
                 qDebug("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");
         }
     }

     if (fb0_fd >= 0)
         close(fb0_fd);

     qDebug("render thread exit");
}
#else /* QTLAUNCHER_GE_SUPPORT */
AiCRenderThread::AiCRenderThread(void *data)
{
    (void)data;
}

AiCRenderThread::~AiCRenderThread()
{

}
#endif
