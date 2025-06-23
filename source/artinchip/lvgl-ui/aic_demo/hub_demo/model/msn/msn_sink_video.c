
/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors: artinchip
 */

#ifdef USE_MSNLINK
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <video/artinchip_fb.h>
#include <mpp_dec_type.h>
#include <mpp_decoder.h>

#include "mpp_log.h"
#include "msn_sink_video.h"

#define FRAME_BUF_NUM       (10)

struct frame_info {
    int fd[3];      // dma-buf fd
    int fd_num;     // number of dma-buf
    int used;       // if the dma-buf of this frame add to de drive
};

struct sink_video_ctx {
    struct mpp_decoder* dec;
    int render_eos;
    struct frame_info frame_info[FRAME_BUF_NUM];
    int dec_err;
    int stop;
    pthread_t render_thread_id;
    pthread_t decode_thread_id;
    struct mpp_size screen_size;
};

static long long get_now_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000ll + tv.tv_usec;
}

static struct sink_video_ctx *g_p_sinK_video_ctx = NULL;

/********************* timer *****************************/

struct av_timer {
    int64_t start_time_us; // us
    int64_t system_time_us; // us
};

struct av_timer* av_timer_create()
{
    struct av_timer* timer = (struct av_timer*)malloc(sizeof(struct av_timer));
    if (timer == NULL) {
        return NULL;
    }
    memset(timer, 0, sizeof(struct av_timer));

    return timer;
}

void av_timer_destroy(struct av_timer* timer)
{
    free(timer);
}

int av_timer_set_time(struct av_timer* timer, int64_t t)
{
    timer->start_time_us = t;
    timer->system_time_us = get_now_us();
    return 0;
}

int64_t av_timer_get_time(struct av_timer* timer)
{
    int64_t current_time = get_now_us();
    int64_t pass_time = current_time - timer->system_time_us;

    pass_time += timer->start_time_us;
    timer->start_time_us = pass_time;
    timer->system_time_us = current_time;

    return pass_time;
}
/*********************************************************/

int set_fb_layer_alpha(int fb0_fd, int val)
{
    int ret = 0;
    struct aicfb_alpha_config alpha = {0};

    if (fb0_fd < 0)
        return -1;

    alpha.layer_id = 1;
    alpha.enable = 1;
    alpha.mode = 1;
    alpha.value = val;
    ret = ioctl(fb0_fd, AICFB_UPDATE_ALPHA_CONFIG, &alpha);
    if (ret < 0)
        loge("fb ioctl() AICFB_UPDATE_ALPHA_CONFIG failed!");

    return ret;
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))
void video_layer_set(int fb0_fd, struct mpp_buf *picture_buf, struct frame_info* frame, struct sink_video_ctx* p)
{
    struct aicfb_layer_data layer = {0};
    int dmabuf_num = 0;
    struct dma_buf_info dmabuf_fd[3];
    int i;

    if (fb0_fd < 0)
        return;

    layer.layer_id = AICFB_LAYER_TYPE_VIDEO;
    layer.enable = 1;

    // if (picture_buf->format == MPP_FMT_YUV420P || picture_buf->format == MPP_FMT_NV12
    //   || picture_buf->format == MPP_FMT_NV21) {
    //  layer.scale_size.width = p->disp_width;
    //  layer.scale_size.height= p->disp_height;
    // }

    //p->screen_size.width
    //p->screen_size.height
    //picture_buf->size.width
    //picture_buf->size.height
    // layer.pos.x = p->disp_pos_x;
    // layer.pos.y = p->disp_pos_y;
    // layer.scale_size.width = p->disp_width;
    // layer.scale_size.height= p->disp_height;

    if (picture_buf->size.width < p->screen_size.width) {
        layer.pos.x = (p->screen_size.width - picture_buf->size.width)/2;
        layer.scale_size.width = picture_buf->size.width;
    } else {
        layer.pos.x = 0;
        layer.scale_size.width = p->screen_size.width;
    }

    if (picture_buf->size.height < p->screen_size.height) {
        layer.pos.y = (p->screen_size.height - picture_buf->size.height)/2;
        layer.scale_size.height = picture_buf->size.height;
    } else {
        layer.pos.y = 0;
        layer.scale_size.height = p->screen_size.height;
    }

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
        loge("no support picture foramt %d, default argb8888", picture_buf->format);
    }

    // add dmabuf to de driver
    if(!frame->used) {
        for(i=0; i<dmabuf_num; i++) {
            frame->fd[i] = picture_buf->fd[i];
            dmabuf_fd[i].fd = picture_buf->fd[i];
            if (ioctl(fb0_fd, AICFB_ADD_DMABUF, &dmabuf_fd[i]) < 0)
                loge("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");
        }
        frame->used = 1;
        frame->fd_num = dmabuf_num;
    }

    logi("width: %d, height %d, stride: %d, %d, crop_en: %d, crop_w: %d, crop_h: %d",
        layer.buf.size.width, layer.buf.size.height,
        layer.buf.stride[0], layer.buf.stride[1], layer.buf.crop_en,
        layer.buf.crop.width, layer.buf.crop.height);
    // display
    if (ioctl(fb0_fd, AICFB_UPDATE_LAYER_CONFIG, &layer) < 0)
        loge("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");

    // wait vsync (wait layer config)
    ioctl(fb0_fd, AICFB_WAIT_FOR_VSYNC, NULL);
}

void* render_thread(void *arg)
{
    int fb0_fd = -1;
    int cur_frame_id = 0;
    int last_frame_id = 1;
    struct mpp_frame frame[2] = {0};
    struct mpp_buf *pic_buffer = NULL;
    int frame_num = 0;
    int ret = 0;
    int i = 0, j = 0;
    struct sink_video_ctx* p = (struct sink_video_ctx*)arg;

    struct av_timer* timer = av_timer_create();

    // 1. open fb0
    fb0_fd = open("/dev/fb0", O_RDWR);
    if (fb0_fd < 0) {
        logw("open fb0 failed!");
    }

    ioctl(fb0_fd, AICFB_GET_SCREEN_SIZE, &p->screen_size);
    printf("width:%d,height:%d\n",p->screen_size.width,p->screen_size.height);
    // 2. render frame until eos
    while(!p->render_eos) {
        memset(&frame[cur_frame_id], 0, sizeof(struct mpp_frame));

        if(p->dec_err || p->stop)
            break;

        // 2.1 get frame
        ret = mpp_decoder_get_frame(p->dec, &frame[cur_frame_id]);
        if(ret == DEC_NO_RENDER_FRAME || ret == DEC_ERR_FM_NOT_CREATE
            || ret == DEC_NO_EMPTY_FRAME) {
            usleep(10000);
            continue;
        } else if(ret) {
            logw("mpp_dec_get_frame error, ret: %x", ret);
            p->dec_err = 1;
            break;
        }

        p->render_eos = frame[cur_frame_id].flags & FRAME_FLAG_EOS;
        logi("decode_get_frame successful: frame id %d, number %d, flag: %d",
            frame[cur_frame_id].id, frame_num, frame[cur_frame_id].flags);

        if (frame[cur_frame_id].flags & FRAME_FLAG_ERROR) {
            loge("frame error");
        }
        pic_buffer = &frame[cur_frame_id].buf;

        // set first start time
        if (0 == frame_num) {
            av_timer_set_time(timer, frame[cur_frame_id].pts);
        }

        int64_t diff_time = frame[cur_frame_id].pts - av_timer_get_time(timer);
        if (diff_time > 10000) {
            loge("diff_time: %lld ms, wait", (long long)diff_time/1000);
            usleep(diff_time);
        }

        // 2.3 disp frame;
        if(!(frame[cur_frame_id].flags & FRAME_FLAG_ERROR)) {
            set_fb_layer_alpha(fb0_fd, 0);
            video_layer_set(fb0_fd, pic_buffer, &p->frame_info[frame[cur_frame_id].id], p);
        }

        // 2.4 return the last frame
        if (frame_num) {
            ret = mpp_decoder_put_frame(p->dec, &frame[last_frame_id]);
        }

        int tmp = cur_frame_id;
        cur_frame_id = last_frame_id;
        last_frame_id = tmp;

        frame_num++;
    }

    // put the last frame when eos
    mpp_decoder_put_frame(p->dec, &frame[last_frame_id]);

    // disable layer
    struct aicfb_layer_data layer = {0};
    layer.enable = 0;
    if (ioctl(fb0_fd, AICFB_UPDATE_LAYER_CONFIG, &layer) < 0)
        loge("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");

    // remove all dmabuf from de driver
    for (i=0; i<FRAME_BUF_NUM; i++) {
        if(p->frame_info[i].used == 0)
            continue;

        for(j=0; j<p->frame_info[i].fd_num; j++) {
            if (ioctl(fb0_fd, AICFB_RM_DMABUF, &p->frame_info[i].fd[j]) < 0) {
                loge("fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");
            }
        }
    }

    struct aicfb_alpha_config alpha = {0};
    alpha.layer_id = 1;
    alpha.enable = 1;
    alpha.mode = 0;
    alpha.value = 255;
    ioctl(fb0_fd, AICFB_UPDATE_ALPHA_CONFIG, &alpha);

    if (fb0_fd >= 0)
        close(fb0_fd);

    av_timer_destroy(timer);

    return NULL;
}

void* decode_thread(void *p)
{
    struct sink_video_ctx* data = (struct sink_video_ctx*)p;
    int ret = 0;

    while (!data->render_eos) {
        if (data->stop)
            break;

        ret = mpp_decoder_decode(data->dec);
        if (ret == DEC_NO_READY_PACKET || ret == DEC_NO_EMPTY_FRAME) {
            usleep(1000);
            continue;
        } else if(ret) {
            logw("decode ret: %x", ret);
            //data->dec_err = 1;
            //break;
        }
        usleep(10000);
    }

    return NULL;
}

void aic_video_start(LinkType type)
{
    struct decode_config config = {0};
    struct sink_video_ctx *p_sink_ctx =  (struct sink_video_ctx*)malloc(sizeof(struct sink_video_ctx));
    if (NULL == p_sink_ctx) {
        loge("mpp_alloc error!!!");
        return;
    }

    memset(p_sink_ctx, 0, sizeof(struct sink_video_ctx));
    config.bitstream_buffer_size = 1024*1024;
    config.extra_frame_num = 1;
    config.packet_count = 10;
    config.pix_fmt = MPP_FMT_YUV420P;
    p_sink_ctx->dec = mpp_decoder_create(MPP_CODEC_VIDEO_DECODER_H264);
    if (NULL == p_sink_ctx->dec) {
	free(p_sink_ctx);
        loge("mpp_decoder_create error!!!");
        return;
    }

    mpp_decoder_init(p_sink_ctx->dec, &config);
    pthread_create(&p_sink_ctx->decode_thread_id, NULL, decode_thread, p_sink_ctx);
    pthread_create(&p_sink_ctx->render_thread_id, NULL, render_thread, p_sink_ctx);
    g_p_sinK_video_ctx = p_sink_ctx;
    return;
}

#define MAX_TRY_TIMES 10
void aic_video_play(LinkType type, void * datas, int len, bool idrFrame)
{
    struct mpp_packet packet = {0};
    int i = 0;
    static int fame_id = 0;
    if (!g_p_sinK_video_ctx) {
        loge("do not start");
        return;
    }

    for (i = 0;i < MAX_TRY_TIMES; i++) {
        if (mpp_decoder_get_packet(g_p_sinK_video_ctx->dec, &packet,len) == 0) {
            memcpy(packet.data, datas, len);
            packet.size = len;
            packet.pts = fame_id*40*1000;// frame rate  25fps
            mpp_decoder_put_packet(g_p_sinK_video_ctx->dec, &packet);
            return;
        } else {
            usleep(5*1000);
        }
    }
    return;
}

void aic_video_stop(LinkType type)
{
    if (!g_p_sinK_video_ctx) {
        logw("do not start, no need to stop");
        return;
    }
    g_p_sinK_video_ctx->stop = 1;
    pthread_join(g_p_sinK_video_ctx->decode_thread_id, NULL);
    pthread_join(g_p_sinK_video_ctx->render_thread_id, NULL);
    mpp_decoder_destory(g_p_sinK_video_ctx->dec);
    free(g_p_sinK_video_ctx);
    g_p_sinK_video_ctx = NULL;
    return;
}
#endif
