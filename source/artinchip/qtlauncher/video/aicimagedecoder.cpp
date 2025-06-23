/*
 * Copyright (C) 2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aicimagedecoder.h"

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <QDebug>

#ifdef QTLAUNCHER_GE_SUPPORT
AiCImageDecoder::AiCImageDecoder()
{
    struct fb_fix_screeninfo fix;
    struct aicfb_layer_data layer;
    struct fb_var_screeninfo var;
    int fb_fd;

    memset(&fix, 0, sizeof(struct fb_fix_screeninfo));
    memset(&layer, 0, sizeof(struct aicfb_layer_data));
    memset(&var, 0, sizeof(struct fb_var_screeninfo));

    fb_fd= open("/dev/fb0", O_RDWR);
    if (fb_fd == -1) {
        qFatal("open fb0 failed");
        return;
    }

    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &fix) < 0) {
       qWarning("ioctl FBIOGET_FSCREENINFO failed");
        ::close(fb_fd);
        return;
    }

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &var) < 0) {
        qDebug() << "ioctl FBIOGET_FSCREENINFO failed";
        ::close(fb_fd);
        return;
    }

    if(ioctl(fb_fd, AICFB_GET_FB_LAYER_CONFIG, &layer) < 0) {
        qWarning("ioctl FBIOGET_FSCREENINFO failed");
        ::close(fb_fd);
        return;
    }

    mScreenW = var.xres;
    mScreenH = var.yres;
    mFbPhy = fix.smem_start;
    mFbStride = fix.line_length;
    mFbFormat = layer.buf.format;

    ::close(fb_fd);
}

AiCImageDecoder::~AiCImageDecoder()
{

}

void AiCImageDecoder::mpp_rander_frame(struct mpp_frame *frame, unsigned int x, unsigned y)
{
    struct ge_bitblt blt;
    memset(&blt, 0, sizeof(struct ge_bitblt));

    memcpy(&blt.src_buf, &frame->buf, sizeof(struct mpp_buf));

    struct mpp_ge *ge = mpp_ge_open();

    /* dstination buffer */
    blt.dst_buf.buf_type = MPP_PHY_ADDR;
    blt.dst_buf.phy_addr[0] = mFbPhy;
    blt.dst_buf.stride[0] = mFbStride;
    blt.dst_buf.size.width = mScreenW;
    blt.dst_buf.size.height = mScreenH;
    blt.dst_buf.format = mFbFormat;

    blt.dst_buf.crop_en = 1;
    blt.dst_buf.crop.x = x;
    blt.dst_buf.crop.y = y;
    blt.dst_buf.crop.width = frame->buf.size.width;
    blt.dst_buf.crop.height = frame->buf.size.height;

    mpp_ge_bitblt(ge, &blt);
    mpp_ge_emit(ge);
    mpp_ge_sync(ge);

    mpp_ge_close(ge);
}

void AiCImageDecoder::decodeImage(const char *fileName, unsigned int x, unsigned y)
{
    int ret = 0;
    int fileSize = 0;
    mpp_codec_type codecType = MPP_CODEC_VIDEO_DECODER_PNG;
    QFile file(fileName);

    if (file.open(QFile::ReadOnly))
        fileSize = file.size();
    else
        return;

    // 1. create mpp_decoder
    struct mpp_decoder *dec = mpp_decoder_create(MPP_CODEC_VIDEO_DECODER_PNG);

    struct decode_config config;
    config.bitstream_buffer_size = (fileSize + 0xFF) & (~0xFF);
    config.extra_frame_num = 0;
    config.packet_count = 1;

    // JPEG not supprt YUV2RGB
    if (codecType == MPP_CODEC_VIDEO_DECODER_MJPEG)
        config.pix_fmt = DECODER_MJPEG_OUTPUT_FORMAT;
    else if (codecType == MPP_CODEC_VIDEO_DECODER_PNG)
        config.pix_fmt = DECODER_PNG_OUTPUT_FORMAT;

    // 2. init mpp_decoder
    mpp_decoder_init(dec, &config);

    // 3. get an empty packet from mpp_decoder
    struct mpp_packet packet;
    memset(&packet, 0, sizeof(struct mpp_packet));
    mpp_decoder_get_packet(dec, &packet, fileSize);

    // 4. copy data to packet
    packet.size = file.read((char *)packet.data, fileSize);
    packet.flag = PACKET_FLAG_EOS;

    // 5. put the packet to mpp_decoder
    mpp_decoder_put_packet(dec, &packet);

    // 6. decode
    ret = mpp_decoder_decode(dec);
    if (ret < 0) {
        qDebug() << "decode error";
        mpp_decoder_destory(dec);
        return;
    }

    // 7. get a decoded frame
    struct mpp_frame frame;
    memset(&frame, 0, sizeof(struct mpp_frame));
    mpp_decoder_get_frame(dec, &frame);

    // 8. rander this buffer
    mpp_rander_frame(&frame, x, y);

    // 9. return this frame
    mpp_decoder_put_frame(dec, &frame);

    // 10. destroy mpp_decoder
    mpp_decoder_destory(dec);
}
#else /* QTLAUNCHER_GE_SUPPORT */
AiCImageDecoder::AiCImageDecoder()
{

}

AiCImageDecoder::~AiCImageDecoder()
{

}
#endif
