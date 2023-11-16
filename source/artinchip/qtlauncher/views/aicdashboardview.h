#ifndef AICDASHBOARDVIEW_H
#define AICDASHBOARDVIEW_H

#include <QWidget>
#include <QSize>
#include <QTimer>
#include <QImage>

#ifdef QTLAUNCHER_GE_SUPPORT
#include <video/mpp_types.h>
#include <linux/dma-heap.h>
#include <dma_allocator.h>
#include <mpp_decoder.h>
#include <mpp_ge.h>
#endif

class AiCDashBoardView : public QWidget
{
    Q_OBJECT

public:
    explicit AiCDashBoardView(QSize size, QWidget *parent = 0);
    ~AiCDashBoardView();
private:
    void initView(int width, int height);

#ifdef QTLAUNCHER_GE_SUPPORT
protected slots:
    void onTimeOut();

private:
    void setSpeed(int speed);
    void paintEvent(QPaintEvent *);
    mpp_pixel_format convertFormat(QImage::Format format);
    int calStrideSize(int width, mpp_pixel_format mppFormat);
    int calMppBufSize(struct mpp_buf *buf);
    int calDmaBufSize(int width, int height, mpp_pixel_format mppFormat);
    int createDmaBuf(int dmafd, int width, int height, mpp_pixel_format mppFormat);
    void initMppBuf(int dmafd, struct mpp_buf *buf, int width, int height, mpp_pixel_format mppFormat);
    void copyMppBuf(struct mpp_buf *dst, struct mpp_buf *src);

    void decoderImage(const char *fileName, struct mpp_buf *buf, int rotate);
    void geCopy(struct mpp_buf *dst, struct mpp_buf *src, int rotate);
    void geBlit(struct mpp_buf *dst, struct mpp_buf *src, int rotate);
    void geRotate(struct mpp_buf *dst, struct mpp_buf *src, int angle);

private:
    QImage::Format mBoardFormat;
    struct mpp_buf mBoardBuf;
    struct mpp_buf mBGImgBuf;
    struct mpp_buf mPointerImgBuf;

    QTimer *mTimer;
    mpp_pixel_format mMppFormat;
    int mImgWidth;
    int mImgHeight;
    int mDegree1;
    int mDegree2;

    int BLIT_DST_CENTER_X;
    int BLIT_DST_CENTER_Y;
    int ROT_SRC_CENTER_X;
    int ROT_SRC_CENTER_Y;
    int ROT_DST_CENTER_X;
    int ROT_DST_CENTER_Y;
#endif
};

#endif // AICDASHBOARDVIEW_H
