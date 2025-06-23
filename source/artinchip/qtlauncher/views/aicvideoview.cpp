/*
 * Copyright (C) 2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aicvideoview.h"
#include "utils/aicconsts.h"

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#ifdef QTLAUNCHER_GE_SUPPORT
AiCVideoView::AiCVideoView(QSize size, QWidget *parent) : QWidget(parent)
{
    initView(size.width(), size.height());
    mImageDecoder = new AiCImageDecoder();
    mVideoThread  = new AiCVideoThread();
}

void AiCVideoView::initView(int width, int height)
{
    struct fb_fix_screeninfo fix;
    struct aicfb_layer_data layer;
    struct fb_var_screeninfo var;
    int fb_fd;

    memset(&fix, 0, sizeof(struct fb_fix_screeninfo));
    memset(&layer, 0, sizeof(struct aicfb_layer_data));
    memset(&var, 0, sizeof(struct fb_var_screeninfo));

    qDebug("VideoView width: %d, heigt: %d\n", width, height);

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

    mScreenW   = var.xres;
    mScreenH   = var.yres;
    mFbPhy     = fix.smem_start;
    mFbStride  = fix.line_length;
    mFbFormat  = layer.buf.format;

    ::close(fb_fd);

    mPlayLabel = new QLabel(this);

    mPlayLabel->resize(AIC_PLAY_BUTTON_WIDTH, AIC_PLAY_BUTTON_HEIGHT);
    mPlayLabel->move(AIC_PLAY_BUTTON_XMARGIN, AIC_PLAY_BUTTON_YMARGIN);
    mPlayLabel->installEventFilter(this);
}

void AiCVideoView::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);

    painter.setBrush(Qt::black);
    painter.drawRect(this->rect());

    QWidget::paintEvent(event);
}

void AiCVideoView::geBgFill(void)
{
    struct mpp_ge *ge = mpp_ge_open();
    struct ge_fillrect fill;

    memset(&fill, 0, sizeof(struct ge_fillrect));

    fill.type                = GE_NO_GRADIENT;
    fill.start_color         = 0x0;
    fill.end_color           = 0x0;
    fill.dst_buf.buf_type    = MPP_PHY_ADDR;
    fill.dst_buf.phy_addr[0] = mFbPhy;
    fill.dst_buf.stride[0]   = mFbStride;
    fill.dst_buf.size.width  = mScreenW;
    fill.dst_buf.size.height = mScreenH;
    fill.dst_buf.format      = mFbFormat;
    fill.ctrl.flags          = 0;

    fill.dst_buf.crop_en     = 1;
    fill.dst_buf.crop.x      = 0;
    fill.dst_buf.crop.y      = AIC_STATUS_BAR_HEIGHT;
    fill.dst_buf.crop.width  = mScreenW;
    fill.dst_buf.crop.height = AIC_CENTRAL_VIEW_HEIGHT;

    mpp_ge_fillrect(ge, &fill);
    mpp_ge_emit(ge);
    mpp_ge_sync(ge);
    mpp_ge_close(ge);
}

bool AiCVideoView::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == this->mPlayLabel) {
        if (event->type() == QEvent::MouseButtonPress) {
            if (!mVideoThread->isRunning()) {

                mImageDecoder->decodeImage(":/resources/video/pause_norma_1.png", AIC_PLAY_BUTTON_XMARGIN, AIC_PLAY_BUTTON_YMARGIN);
                mVideoThread->start();
            }
            return true;
        }
        if (event->type() == QEvent::MouseButtonRelease)
            return true;
    }
    return QWidget::eventFilter(obj, event);
}

void AiCVideoView::geBtnBlt(void)
{
    if (mImageDecoder)
        mImageDecoder->decodeImage(":/resources/video/play_normal.png", AIC_PLAY_BUTTON_XMARGIN, AIC_PLAY_BUTTON_YMARGIN);
}

void AiCVideoView::videoStop(void)
{
    if (mVideoThread && mVideoThread->isRunning())
        mVideoThread->stop();
}

AiCVideoView::~AiCVideoView()
{
    if (mImageDecoder != NULL)
        delete mImageDecoder;
    if (mVideoThread != NULL)
        delete mVideoThread;
    if (mPlayLabel != NULL)
        delete mPlayLabel;
}
#else /* QTLAUNCHER_GE_SUPPORT */
AiCVideoView::AiCVideoView(QSize size, QWidget *parent) : QWidget(parent)
{
    initView(size.width(), size.height());
}

void AiCVideoView::initView(int width, int height)
{
    qDebug() << __func__ << width << ":" << height;
}

AiCVideoView::~AiCVideoView()
{

}

void AiCVideoView::videoStop(void)
{

}
#endif
