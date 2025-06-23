/*
 * Copyright (C) 2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AICVIDEOTHREAD_H
#define AICVIDEOTHREAD_H

#include <QThread>

#include "aicdecodethread.h"
#include "aicrenderthread.h"
#include "aicimagedecoder.h"

#ifdef QTLAUNCHER_GE_SUPPORT
#include <video/mpp_types.h>
#include <linux/dma-heap.h>
#include <dma_allocator.h>
#include <frame_allocator.h>
#include <mpp_decoder.h>
#include <mpp_dec_type.h>
#include <mpp_ge.h>

/*
 * dec decode
 */
#define DECODER_NUM             (2)
#define FRAME_BUF_NUM           (3)
#define MAX_TEST_FILE           (256)
#define SCREEN_WIDTH            1024
#define SCREEN_HEIGHT           600
#define FRAME_COUNT             30

struct frame_info {
    int fd[3];          // dma-buf fd
    int fd_num;         // number of dma-buf
    int used;           // if the dma-buf of this frame add to de drive
};

struct dec_ctx {
    struct mpp_decoder  *decoder[4];
    struct frame_info frame_info[FRAME_BUF_NUM];

    struct bit_stream_parser *parser[4];

    int stream_eos;
    int render_eos;
    int dec_err;
    int cmp_data_err;

    char file_input[MAX_TEST_FILE][1024];       // test file name
    int file_num;                               // test file number

    int decode_num;
    int output_format;
    int dma_fd;
    struct mpp_frame frame[FRAME_BUF_NUM];
};

/*
 *  frame allocator
 */
struct ext_frame_allocator {
    struct frame_allocator base;
    struct mpp_frame frame[FRAME_BUF_NUM];
    int frame_idx;
};

#define ALIGN_16B(x) (((x) + (15)) & ~(15))

#define BUF_STRIDE ALIGN_16B(SCREEN_WIDTH)
#define BUF_HEIGHT ALIGN_16B(SCREEN_HEIGHT)

/*
 * bit stream parser
 */
#define STEAM_BUF_LEN (512 * 1024)

struct bit_stream_parser {
    int fd;
    int file_size;
    int cur_read_len;

    char* stream_buf;
    int cur_read_pos;
    int buf_len;
    int valid_size;
};

/*
 * video
 */
#define VIDEO_LOOP_TIME         1

#define VIDEO_H264_FILE0        "/usr/local/launcher/h264/VID_20240701_144046.264"
#define VIDEO_H264_FILE1        "/usr/local/launcher/h264/VID_20240701_144245.264"
#define VIDEO_H264_FILE2        "/usr/local/launcher/h264/VID_20240701_144046.264"
#define VIDEO_H264_FILE3        "/usr/local/launcher/h264/VID_20240701_144245.264"

#define VIDEO_DEC0_OUTPUT_POS_X        0
#define VIDEO_DEC0_OUTPUT_POS_Y        0

#define VIDEO_DEC1_OUTPUT_POS_X        512
#define VIDEO_DEC1_OUTPUT_POS_Y        0

#define VIDEO_DEC2_OUTPUT_POS_X        0
#define VIDEO_DEC2_OUTPUT_POS_Y        600

#define VIDEO_DEC3_OUTPUT_POS_X        512
#define VIDEO_DEC3_OUTPUT_POS_Y        600
#endif /* QTLAUNCHER_GE_SUPPORT */

class AiCVideoThread : public QThread
{
    Q_OBJECT
public:
    AiCVideoThread();
    ~AiCVideoThread();

#ifdef QTLAUNCHER_GE_SUPPORT
    void stop();

protected:
    void run();

private:
    /*
     * decode
     */
    int dec_decode(struct dec_ctx *data);
    struct frame_allocator* open_allocator(struct dec_ctx* ctx);

    AiCImageDecoder *mImageDecoder;

    /*
     * bit stream parser
     */
    int get_data(struct bit_stream_parser* p);
    struct bit_stream_parser* bs_create(int fd);
    int bs_close(struct bit_stream_parser* p);
    int bs_prefetch(struct bit_stream_parser* p, struct mpp_packet* pkt);
    int bs_read(struct bit_stream_parser* pCtx, struct mpp_packet* pkt);

    /*
     * mem
     */
    int alloc_framebuffers(struct dec_ctx* ctx);
    void free_framebuffers(struct dec_ctx* ctx);

    /*
     * thread
     */
    AiCRenderThread *mRenderThread;
    AiCDecodeThread *mDecodeThread;

    bool mStop;
#endif /* QTLAUNCHER_GE_SUPPORT */
};

#endif /* AICVIDEOTHREAD_H */
