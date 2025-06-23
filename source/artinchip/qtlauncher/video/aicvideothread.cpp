/*
 * Copyright (C) 2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aicvideothread.h"
#include "utils/aicconsts.h"

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <QDebug>

#ifdef QTLAUNCHER_GE_SUPPORT
AiCVideoThread::AiCVideoThread()
{
    mStop = false;
    mImageDecoder = new AiCImageDecoder();
    mDecodeThread = NULL;
    mRenderThread = NULL;
}

AiCVideoThread::~AiCVideoThread()
{
    if (mImageDecoder != NULL)
        delete mImageDecoder;

    if (mDecodeThread != NULL)
        delete mDecodeThread;

    if (mRenderThread != NULL)
        delete mRenderThread;
}

void AiCVideoThread::stop(void)
{
    mStop = true;

    if (mRenderThread != NULL)
        mRenderThread->stop();
}

void AiCVideoThread::run(void)
{
    struct dec_ctx dec_data;
    int j;

    memset(&dec_data, 0, sizeof(struct dec_ctx));
    dec_data.output_format = MPP_FMT_YUV420P;

    for(j = 0; j < VIDEO_LOOP_TIME; j++) {
        qDebug() << "video loop time: " << j;

        dec_data.render_eos   = 0;
        dec_data.stream_eos   = 0;
        dec_data.cmp_data_err = 0;
        dec_data.dec_err      = 0;

        memset(dec_data.frame_info, 0, sizeof(struct frame_info)*FRAME_BUF_NUM);
        dec_decode(&dec_data);
    }

    if (!mStop)
        mImageDecoder->decodeImage(":/resources/video/play_normal.png", AIC_PLAY_BUTTON_XMARGIN, AIC_PLAY_BUTTON_YMARGIN);
}

struct bit_stream_parser* AiCVideoThread::bs_create(int fd)
{
    struct bit_stream_parser* ctx = NULL;

    ctx = (struct bit_stream_parser*)malloc(sizeof(struct bit_stream_parser));
    if(ctx == NULL) {
        qWarning() << "malloc for rawParserContext failed";
        return NULL;
    }
    memset(ctx, 0, sizeof(struct bit_stream_parser));

    ctx->fd = fd;

    ctx->stream_buf = (char*)malloc(STEAM_BUF_LEN);
    if(ctx->stream_buf == NULL) {
        qWarning() << "malloc for stream data failed";
        free(ctx);
        return NULL;
    }
    ctx->buf_len = STEAM_BUF_LEN;

    ctx->file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    return ctx;
}

int AiCVideoThread::bs_close(struct bit_stream_parser* p)
{
    if(p) {
        if(p->stream_buf)
            free(p->stream_buf);

        free(p);
    }
    return 0;
}

int AiCVideoThread::get_data(struct bit_stream_parser* p)
{
    int r_len = 0;

    if(p->valid_size <= 0) {
        r_len = read(p->fd, p->stream_buf, p->buf_len);
        if(r_len <= 0)
            return r_len;

        p->cur_read_len += r_len;

        p->valid_size = r_len;
        p->cur_read_pos = 0;
    } else {
        memmove(p->stream_buf, (p->stream_buf + p->cur_read_pos), p->valid_size);

        int len = p->buf_len - p->valid_size;
        r_len = read(p->fd, p->stream_buf + p->valid_size, len);
        p->cur_read_len += r_len;

        p->valid_size += r_len;
        p->cur_read_pos = 0;
    }
    return r_len;
}

int AiCVideoThread::bs_prefetch(struct bit_stream_parser* p, struct mpp_packet* pkt)
{
    int i = 0;
    char tmp_buf[3];
    int find_start_code = 0;
    int nStart = 0;
    int stream_data_len = -1;
    int ret = 0;

    char* cur_data_ptr = NULL;

    if(p->valid_size <= 0) {
        ret = get_data(p);
        if(ret == -1) {
            qDebug("get data error");
            return -1;
        }
    }

find_startCode:

    cur_data_ptr = p->stream_buf + p->cur_read_pos;

    //* find the first startcode
    for(i = 0; i < (p->valid_size - 3); i++) {
        tmp_buf[0] = *(cur_data_ptr + i);
        tmp_buf[1] = *(cur_data_ptr + i + 1);
        tmp_buf[2] = *(cur_data_ptr + i + 2);

        if(tmp_buf[0] == 0 && tmp_buf[1] == 0 && tmp_buf[2] == 1) {
            find_start_code = 1;
            break;
        }
    }

    if(find_start_code == 1) {
        p->cur_read_pos += i;
        nStart = i;

        if (p->cur_read_pos && (*(cur_data_ptr + i -1) == 0)) {
            p->cur_read_pos -= 1;
            nStart -= 1;
        }
        find_start_code = 0;

        //* find the next startcode
        for(i += 3; i < (p->valid_size - 3); i++) {
            tmp_buf[0] = *(cur_data_ptr + i);
            tmp_buf[1] = *(cur_data_ptr + i + 1);
            tmp_buf[2] = *(cur_data_ptr + i + 2);

            if(tmp_buf[0] == 0 && tmp_buf[1] == 0 && tmp_buf[2] == 1) {
                find_start_code = 1;
                break;
            }
        }

        if(find_start_code == 1) {
            if(*(cur_data_ptr + i - 1) == 0) {
                stream_data_len = i - nStart - 1;
            } else {
                stream_data_len = i - nStart;
            }
        } else {
            ret = get_data(p);
            if(ret == -1)
                return -1;

            if(ret == 0) {
                stream_data_len = p->valid_size - nStart;
                pkt->flag |= PACKET_FLAG_EOS;
                goto out;
            }

            goto find_startCode;
        }
    } else {
        ret = get_data(p);
        if(ret == -1 || ret == 0)
            return -1;

        goto find_startCode;
    }

out:
    pkt->size = stream_data_len;
    return 0;
}

int AiCVideoThread::bs_read(struct bit_stream_parser* p, struct mpp_packet* pkt)
{
    if(pkt->size <= 0)
        return -1;

    char* read_ptr = p->stream_buf + p->cur_read_pos;

    memcpy(pkt->data, read_ptr, pkt->size);

    p->cur_read_pos += pkt->size;
    p->valid_size -= pkt->size;

    return 0;
}

static int alloc_frame_buffer(struct frame_allocator *p, struct mpp_frame* frame,
        int width, int height, enum mpp_pixel_format format)
{
    struct ext_frame_allocator* impl = (struct ext_frame_allocator*)p;

    (void)width;
    (void)height;

    if (format != MPP_FMT_NV12)
        qDebug("format error (%d), we need NV12", format);

    frame->buf.fd[0]       = impl->frame[impl->frame_idx].buf.fd[0];
    frame->buf.fd[1]       = impl->frame[impl->frame_idx].buf.fd[1];
    frame->buf.fd[2]       = impl->frame[impl->frame_idx].buf.fd[2];
    frame->buf.size.width  = impl->frame[impl->frame_idx].buf.size.width;
    frame->buf.size.height = impl->frame[impl->frame_idx].buf.size.height;
    frame->buf.stride[0]   = impl->frame[impl->frame_idx].buf.stride[0];
    frame->buf.stride[1]   = impl->frame[impl->frame_idx].buf.stride[1];
    frame->buf.stride[2]   = impl->frame[impl->frame_idx].buf.stride[2];

    impl->frame_idx ++;
    return 0;
}

static int free_frame_buffer(struct frame_allocator *p, struct mpp_frame *frame)
{
    // do nothing
    (void)p;
    (void)frame;

    return 0;
}

static int close_allocator(struct frame_allocator *p)
{
    struct ext_frame_allocator* impl = (struct ext_frame_allocator*)p;

    free(impl);

    return 0;
}

static struct alloc_ops def_ops = {
    .alloc_frame_buffer = alloc_frame_buffer,
    .free_frame_buffer  = free_frame_buffer,
    .close_allocator    = close_allocator,
};

struct frame_allocator* AiCVideoThread::open_allocator(struct dec_ctx* ctx)
{
    struct ext_frame_allocator* impl = (struct ext_frame_allocator*)malloc(sizeof(struct ext_frame_allocator));
    if(impl == NULL)
        return NULL;

    memset(impl, 0, sizeof(struct ext_frame_allocator));

    memcpy(impl->frame, ctx->frame, sizeof(struct mpp_frame)* FRAME_BUF_NUM);
    impl->base.ops = &def_ops;

    return &impl->base;
}

int AiCVideoThread::alloc_framebuffers(struct dec_ctx* ctx)
{
    int i;

    ctx->dma_fd = dmabuf_device_open();
    for(i = 0; i < FRAME_BUF_NUM; i++) {
        ctx->frame[i].id              = i;
        ctx->frame[i].buf.buf_type    = MPP_DMA_BUF_FD;
        ctx->frame[i].buf.size.height = BUF_HEIGHT;
        ctx->frame[i].buf.size.width  = BUF_STRIDE;
        ctx->frame[i].buf.stride[0]   = BUF_STRIDE;
        ctx->frame[i].buf.stride[1]   = BUF_STRIDE;
        ctx->frame[i].buf.stride[2]   = 0;
        ctx->frame[i].buf.format      = MPP_FMT_NV12;
        if(mpp_buf_alloc(ctx->dma_fd, &ctx->frame[i].buf)) {
            qDebug("failed to malloc mpp buf");
            return -1;
        }
    }
    return 0;
}

void AiCVideoThread::free_framebuffers(struct dec_ctx* ctx)
{
    int i;

    for(i = 0; i < FRAME_BUF_NUM; i++)
        mpp_buf_free(&ctx->frame[i].buf);

    dmabuf_free(ctx->dma_fd);
}

int AiCVideoThread::dec_decode(struct dec_ctx *data)
{
    enum mpp_codec_type dec_type = MPP_CODEC_VIDEO_DECODER_H264;
    long long pts = 0;
    int file_fd[4];
    int ret;
    int i;

    alloc_framebuffers(data);

    const char filename[4][1024] = { VIDEO_H264_FILE0, VIDEO_H264_FILE1,
                                     VIDEO_H264_FILE2, VIDEO_H264_FILE3 };

    struct mpp_dec_output_pos pos[4] = {
            { VIDEO_DEC0_OUTPUT_POS_X, VIDEO_DEC0_OUTPUT_POS_Y },
            { VIDEO_DEC1_OUTPUT_POS_X, VIDEO_DEC1_OUTPUT_POS_Y },
            { VIDEO_DEC2_OUTPUT_POS_X, VIDEO_DEC2_OUTPUT_POS_Y },
            { VIDEO_DEC3_OUTPUT_POS_X, VIDEO_DEC3_OUTPUT_POS_Y }};

    for (i = 0; i < DECODER_NUM; i++) {
        //* 1. read data
        file_fd[i] = open(filename[i], O_RDONLY);
        if (file_fd[i] < 0) {
            qDebug() << "failed to open input file" << filename[i];
            ret = -1;
            goto out;
        }

        //* 2. create and init mpp_decoder
        data->decoder[i] = mpp_decoder_create(dec_type);
        if (!data->decoder[i]) {
            qDebug() << "mpp_dec_create failed, i:" << i;
            ret = -1;
            goto out;
        }

        struct frame_allocator* allocator = open_allocator(data);
        mpp_decoder_control(data->decoder[i], MPP_DEC_INIT_CMD_SET_EXT_FRAME_ALLOCATOR, (void*)allocator);
        mpp_decoder_control(data->decoder[i], MPP_DEC_INIT_CMD_SET_OUTPUT_POS, (void*)&pos[i]);

        struct decode_config config;
        config.bitstream_buffer_size = 512 * 1024;
        config.extra_frame_num       = 1;
        config.packet_count          = 10;
        config.pix_fmt               = MPP_FMT_NV12;
        ret = mpp_decoder_init(data->decoder[i], &config);
        if (ret) {
            qDebug("mpp_dec_init type %d failed", dec_type);
            goto out;
        }
    }

    //* 3. create decode thread
    mDecodeThread = new AiCDecodeThread(data);

    //* 4. create render thread
    mRenderThread = new AiCRenderThread(data);

    mDecodeThread->start();
    mRenderThread->start();

    //* 5. send data
    for (i = 0; i < DECODER_NUM; i++)
        data->parser[i] = bs_create(file_fd[i]);

    struct mpp_packet packet;
    memset(&packet, 0, sizeof(struct mpp_packet));

    while((packet.flag & PACKET_FLAG_EOS) == 0) {
        for (i = 0; i < DECODER_NUM; i++) {
            memset(&packet, 0, sizeof(struct mpp_packet));
            bs_prefetch(data->parser[i], &packet);

            // get an empty packet
            do {
                if (data->dec_err) {
                    qDebug() << "decode error, break now";
                    return -1;
                }

                if (data->render_eos || mStop)
                    goto eos;

                ret = mpp_decoder_get_packet(data->decoder[i], &packet, packet.size);
                if (ret == 0)
                    break;

                usleep(1000);
            } while (1);

            bs_read(data->parser[i], &packet);
            packet.pts = pts;

            ret = mpp_decoder_put_packet(data->decoder[i], &packet);
        }
        pts += 30000; //us
    }

eos:
    for (i = 0; i < DECODER_NUM; i++)
        bs_close(data->parser[i]);

    mDecodeThread->wait();
    mRenderThread->wait();

out:
    for (i = 0; i < DECODER_NUM; i++) {
        if (data->decoder[i]) {
            mpp_decoder_destory(data->decoder[i]);
            data->decoder[i] = NULL;
        }
    }

    for (i = 0; i < DECODER_NUM; i++) {
        if(file_fd[i])
            ::close(file_fd[i]);
    }
    free_framebuffers(data);

    return ret;
}
#else /* QTLAUNCHER_GE_SUPPORT */
AiCVideoThread::AiCVideoThread()
{

}

AiCVideoThread::~AiCVideoThread()
{

}
#endif
