/*
 * Copyright (C) 2024 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: artinchip
 */
#include "aicrtpview.h"
#include <QPainter>
#include <QDebug>

AiCRtpView::AiCRtpView(QSize size, int margin, QWidget *parent) : QWidget(parent)
{
    initView(size.width(),size.height(), margin);
}

void AiCRtpView::initView(int width, int height, int margin)
{
    mWidth = width;
    mHeight = height;
    mMargin = margin;
    mImage = QImage(QSize(width - mMargin * 2,height - mMargin * 2), QImage::Format_RGB32);
    //mImage.fill(QColor(Qt::lightGray).rgb());
    mImage.fill(QColor(Qt::white).rgb());

    mPen = QPen(Qt::blue, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    mPen.setWidthF(2.0);
}

void AiCRtpView::cleanView()
{
    mImage.fill(QColor(Qt::white).rgb());
    update();
}

bool AiCRtpView::validPoint(QPoint point)
{
    int x = point.x();
    int y = point.y();
    return (x >0 && x < mWidth) && (y > 0 && y < mHeight);
}

bool AiCRtpView::isFar(QPoint p1, QPoint p2){
    QPoint distance = p2 - p1;

    int caldis = distance.x() * distance.x() + distance.y() * distance.y();
    if (caldis > 5)
        return TRUE;
    else
        return FALSE;
}

void AiCRtpView::mousePressEvent(QMouseEvent *event)
{
    if(validPoint(event->pos())){
        mCurrentPoint = mLastPoint = event->pos();
    }
}

void AiCRtpView::mouseMoveEvent(QMouseEvent *event)
{
    if(validPoint(event->pos())){
        if(isFar(event->pos(), mLastPoint)){
            mLastPoint = mCurrentPoint;
            mCurrentPoint = event->pos();
            QPainter painter(&mImage);
            painter.setPen(mPen);
            painter.drawLine(mLastPoint, mCurrentPoint);

            update();
        }
    }
}

void AiCRtpView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.drawImage(QPoint(mMargin, mMargin), mImage);
}

AiCRtpView::~AiCRtpView()
{

}
