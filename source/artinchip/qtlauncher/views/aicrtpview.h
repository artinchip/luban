/*
 * Copyright (C) 2024 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: artinchip
 */
#ifndef AICRTPVIEW_H
#define AICRTPVIEW_H

#include <QWidget>
#include <QSize>
#include <QPoint>
#include <QPen>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QImage>
#include <QTime>

class AiCRtpView : public QWidget
{
    Q_OBJECT
public:
    explicit AiCRtpView(QSize size, int margin = 0, QWidget *parent = 0);
    ~AiCRtpView();
    void cleanView();

private:
    void initView(int width, int height, int margin);
    bool isFar(QPoint p1, QPoint p2);
    bool validPoint(QPoint point);

protected:
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    void paintEvent(QPaintEvent *);

private:
    int mWidth;
    int mHeight;
    int mMargin;
    QImage mImage;
    QPen mPen;
    QPoint mLastPoint;
    QPoint mCurrentPoint;
};

#endif // AICRTPVIEW_H
