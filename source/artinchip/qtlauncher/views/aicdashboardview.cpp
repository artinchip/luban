 /*
  * Copyright (C) 2023 ArtInChip Technology Co., Ltd.
 *
  * SPDX-License-Identifier: Apache-2.0
 *
  * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#include "aicdashboardview.h"

#include <QPainter>
#include <QFile>
#include <QDebug>
#include <QPainter>
#include <QString>
#include <qmath.h>

AiCDashBoardView::AiCDashBoardView(QSize size, QWidget *parent) : QWidget(parent)
{
    setFixedSize(size);
    initView(size.width(), size.height());
}

#ifdef QTLAUNCHER_GE_SUPPORT
#define ANGLE_SIN(x) qSin(x * 3.14159265 / 180) * 4096
#define ANGLE_COS(x) qCos(x * 3.14159265 / 180) * 4096

void AiCDashBoardView::initView(int width, int height)
{
    int dmafd;
    mDegree1 = 0;
    mDegree2 = 180;

    mBoardFormat = QImage::Format_ARGB32;
    mMppFormat = convertFormat(mBoardFormat);

    dmafd = dmabuf_device_open();
    initMppBuf(dmafd, &mBoardBuf, width, height, mMppFormat);
    initMppBuf(dmafd, &mBGImgBuf, 490, 498, mMppFormat);
    initMppBuf(dmafd, &mPointerImgBuf, 48, 220, mMppFormat);

    dmabuf_device_close(dmafd);
    // 490 x 498
    decoderImage(":/resources/ge/ge-clock.png", &mBGImgBuf, 0);
    // 48 x 220
    decoderImage(":/resources/ge/ge-second.png", &mPointerImgBuf, 0);

    setSpeed(50);
}

void AiCDashBoardView::setSpeed(int speed)
{
    mTimer = new QTimer(this);
    connect(mTimer, SIGNAL(timeout()), this, SLOT(onTimeOut()));
    mTimer->start(speed);
}

void AiCDashBoardView::onTimeOut()
{
    mDegree1 = (mDegree1 + 5) % 360;

    // first clock + second
    BLIT_DST_CENTER_X = 11; //(width() / 2 - 490)/2;
    BLIT_DST_CENTER_Y = 11; //(height() - 498)/2;
    ROT_SRC_CENTER_X = 24;
    ROT_SRC_CENTER_Y = 194;
    ROT_DST_CENTER_X = 255; //(width() / 2 - 48)/2 + BLIT_DST_CENTER_X + 10;
    ROT_DST_CENTER_Y = 257; // height()/2 - BLIT_DST_CENTER_Y + 8;
    geBlit(&mBoardBuf, &mBGImgBuf, 0);
    geRotate(&mBoardBuf, &mPointerImgBuf, mDegree1);

    // second clock + second
    mDegree2 = (mDegree2 + 5) % 360;
    BLIT_DST_CENTER_X = 523; // width() / 2 + (width() / 2 - 490)/2;
    BLIT_DST_CENTER_Y = 11;  //(height() - 498)/2;
    ROT_SRC_CENTER_X = 24;
    ROT_SRC_CENTER_Y = 194;
    ROT_DST_CENTER_X = 767; //(width() / 2 - 48)/2 + BLIT_DST_CENTER_X + 10;
    ROT_DST_CENTER_Y = 257; // height()/2 - BLIT_DST_CENTER_Y + 8;
    geBlit(&mBoardBuf, &mBGImgBuf, 0);
    geRotate(&mBoardBuf, &mPointerImgBuf, mDegree2);

    update();
}

void AiCDashBoardView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    int size = calMppBufSize(&mBoardBuf);
    unsigned char *buf = dmabuf_mmap(mBoardBuf.fd[0], size);
    painter.drawImage(0, 0, QImage(buf, width(), height(), mBoardFormat));
    dmabuf_munmap(buf, size);
}

int AiCDashBoardView::calMppBufSize(struct mpp_buf *buf)
{
    return buf->stride[0] * buf->size.height;
}

int AiCDashBoardView::calDmaBufSize(int width, int height, mpp_pixel_format mppFormat)
{
    int bytes = 4;
    switch (mppFormat)
    {
    case MPP_FMT_ARGB_8888:
        bytes = 4;
        break;
    case MPP_FMT_RGB_888:
        bytes = 3;
        break;
    case MPP_FMT_RGB_565:
        bytes = 2;
        break;
    default:
        bytes = 4;
        break;
    }

    return width * height * bytes;
}

int AiCDashBoardView::createDmaBuf(int dmafd, int width, int height, mpp_pixel_format mppFormat)
{
    int size = calDmaBufSize(width, height, mppFormat);
    int fd = dmabuf_alloc(dmafd, size);
    if (fd < 0)
        return -1;

    unsigned char *buf = dmabuf_mmap(fd, size);
    if (buf == NULL)
        return -1;

    memset(buf, 0, size);
    dmabuf_munmap(buf, size);
    return fd;
}

void AiCDashBoardView::initMppBuf(int dmafd, struct mpp_buf *buf, int width, int height, mpp_pixel_format mppFormat)
{
    buf->buf_type = MPP_DMA_BUF_FD;
    buf->fd[0] = createDmaBuf(dmafd, width, height, mppFormat);
    buf->stride[0] = calStrideSize(width, mppFormat);
    buf->size.width = width;
    buf->size.height = height;
    buf->format = mppFormat;
    buf->crop_en = 0;
    buf->crop.x = 0;
    buf->crop.y = 0;
    buf->crop.width = 0;
    buf->crop.height = 0;
}

void AiCDashBoardView::copyMppBuf(struct mpp_buf *dst, struct mpp_buf *src)
{
    dst->buf_type = src->buf_type;
    dst->fd[0] = src->fd[0];
    dst->stride[0] = src->stride[0];
    dst->size.width = src->size.width;
    dst->size.height = src->size.height;
    dst->format = src->format;
    dst->crop_en = src->crop_en;
    dst->crop.x = src->crop.x;
    dst->crop.y = src->crop.x;
    dst->crop.width = src->crop.width;
    dst->crop.height = src->crop.height;
}

int AiCDashBoardView::calStrideSize(int width, mpp_pixel_format mppFormat)
{
    int bytes = 4;
    switch (mppFormat)
    {
    case MPP_FMT_ARGB_8888:
        bytes = 4;
        break;
    case MPP_FMT_RGB_888:
        bytes = 3;
        break;
    case MPP_FMT_RGB_565:
        bytes = 2;
        break;
    default:
        bytes = 4;
        break;
    }

    return width * bytes;
}

mpp_pixel_format AiCDashBoardView::convertFormat(QImage::Format format)
{
    switch (format)
    {
    case QImage::Format_ARGB32:
        return MPP_FMT_ARGB_8888;
    case QImage::Format_RGB888:
        return MPP_FMT_RGB_888;
    case QImage::Format_RGB16:
        return MPP_FMT_RGB_565;
    default:
        return MPP_FMT_ARGB_8888;
    }
}

void AiCDashBoardView::decoderImage(const char *fileName, struct mpp_buf *buf, int rotate)
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
        config.pix_fmt = MPP_FMT_YUV420P;
    else if (codecType == MPP_CODEC_VIDEO_DECODER_PNG)
        config.pix_fmt = mMppFormat;

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
    if (ret < 0)
    {
        qDebug() << "decode error";
        mpp_decoder_destory(dec);
        return;
    }

    // 7. get a decoded frame
    struct mpp_frame frame;
    memset(&frame, 0, sizeof(struct mpp_frame));
    mpp_decoder_get_frame(dec, &frame);

    // 8. ge copy, copy frame to the right dma buffer
    geCopy(buf, &frame.buf, rotate);
    // 9. return this frame
    mpp_decoder_put_frame(dec, &frame);

    // 10. destroy mpp_decoder
    mpp_decoder_destory(dec);
}

void AiCDashBoardView::geCopy(struct mpp_buf *dstbuf, struct mpp_buf *srcbuf, int rotate)
{
    struct mpp_ge *ge = mpp_ge_open();
    struct ge_bitblt blt;
    memset(&blt, 0, sizeof(blt));

    blt.src_buf.buf_type = MPP_DMA_BUF_FD;
    if (srcbuf->format == MPP_FMT_ARGB_8888 || srcbuf->format == MPP_FMT_RGB_888 || srcbuf->format == MPP_FMT_RGB_565)
    {
        mpp_ge_add_dmabuf(ge, srcbuf->fd[0]);
        blt.src_buf.fd[0] = srcbuf->fd[0];
        blt.src_buf.stride[0] = srcbuf->stride[0];
        blt.src_buf.format = srcbuf->format;
    }
    else if (srcbuf->format == MPP_FMT_YUV420P)
    {
        mpp_ge_add_dmabuf(ge, srcbuf->fd[0]);
        mpp_ge_add_dmabuf(ge, srcbuf->fd[1]);
        mpp_ge_add_dmabuf(ge, srcbuf->fd[2]);
        blt.src_buf.fd[0] = srcbuf->fd[0];
        blt.src_buf.fd[1] = srcbuf->fd[1];
        blt.src_buf.fd[2] = srcbuf->fd[2];
        blt.src_buf.stride[0] = srcbuf->stride[0];
        blt.src_buf.stride[1] = srcbuf->stride[1];
        blt.src_buf.stride[2] = srcbuf->stride[2];
        blt.src_buf.format = srcbuf->format;
    }

    blt.src_buf.size.width = srcbuf->size.width;
    blt.src_buf.size.height = srcbuf->size.height;
    blt.src_buf.crop_en = 0;

    blt.ctrl.flags = rotate / 90; // rotate flag
    blt.ctrl.ck_en = 0;

    copyMppBuf(&blt.dst_buf, dstbuf);
    blt.dst_buf.crop_en = 0;
    blt.dst_buf.crop.width = 0;
    blt.dst_buf.crop.height = 0;
    blt.dst_buf.crop.x = 0;
    blt.dst_buf.crop.y = 0;

    mpp_ge_bitblt(ge, &blt);
    mpp_ge_emit(ge);
    mpp_ge_sync(ge);
    mpp_ge_close(ge);
}

void AiCDashBoardView::geBlit(struct mpp_buf *dst, struct mpp_buf *src, int rotate)
{
    struct mpp_ge *ge = mpp_ge_open();
    struct ge_bitblt blt;

    memset(&blt, 0, sizeof(struct ge_bitblt));
    copyMppBuf(&blt.src_buf, src);
    copyMppBuf(&blt.dst_buf, dst);
    blt.dst_buf.crop_en = 1;
    blt.dst_buf.crop.x = BLIT_DST_CENTER_X;
    blt.dst_buf.crop.y = BLIT_DST_CENTER_Y;
    blt.dst_buf.crop.width = src->size.width;
    blt.dst_buf.crop.height = src->size.height;

    blt.ctrl.flags = rotate / 90;

    mpp_ge_bitblt(ge, &blt);

    mpp_ge_emit(ge);

    mpp_ge_sync(ge);

    mpp_ge_close(ge);
}

void AiCDashBoardView::geRotate(struct mpp_buf *dst, struct mpp_buf *src, int angle)
{
    struct mpp_ge *ge = mpp_ge_open();
    struct ge_rotation rot;

    memset(&rot, 0, sizeof(rot));
    // source buffer
    copyMppBuf(&rot.src_buf, src);

    rot.src_buf.crop_en = 0;
    rot.src_rot_center.x = ROT_SRC_CENTER_X;
    rot.src_rot_center.y = ROT_SRC_CENTER_Y;

    // destination buffer
    copyMppBuf(&rot.dst_buf, dst);

    rot.dst_buf.crop_en = 0;
    rot.dst_rot_center.x = ROT_DST_CENTER_X;
    rot.dst_rot_center.y = ROT_DST_CENTER_Y;

    rot.ctrl.alpha_en = 1;

    rot.angle_sin = (int)(ANGLE_SIN(angle));
    rot.angle_cos = (int)(ANGLE_COS(angle));

    mpp_ge_rotate(ge, &rot);
    mpp_ge_emit(ge);
    mpp_ge_sync(ge);
    mpp_ge_close(ge);
}

AiCDashBoardView::~AiCDashBoardView()
{
    dmabuf_free(mBoardBuf.fd[0]);
    dmabuf_free(mPointerImgBuf.fd[0]);
    dmabuf_free(mBGImgBuf.fd[0]);
}
#else
void AiCDashBoardView::initView(int width, int height)
{
    qDebug() << width << ":" << height;
}

AiCDashBoardView::~AiCDashBoardView()
{
}
#endif
