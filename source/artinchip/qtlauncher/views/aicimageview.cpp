/*
 * Copyright (C) 2024 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: artinchip
 */
#include "aicimageview.h"

AiCImageView::AiCImageView(QSize size, QWidget *parent) : QWidget(parent)
{
    SWITCH_DURATION = 5000;
    mCurrentDrawImageIndx = 0;
    this->setFixedSize(size);
    mOpacityAnimation = NULL;

#ifdef QTLAUNCHER_SMALL_MEMORY
    mCurrentPixmap = QPixmap(":/resources/images/image1.jpeg");
#else
    mOpacityAnimation = new QPropertyAnimation(this, "ImageOpacity");
    mOpacityAnimation->setDuration(SWITCH_DURATION - 1000);

    mOpacityAnimation->setStartValue(1.0);
    mOpacityAnimation->setEndValue(0.0);
    connect(mOpacityAnimation, SIGNAL(valueChanged(const QVariant&)), this, SLOT(update()));
    connect(&mImageChangeTimer, SIGNAL(timeout()), this, SLOT(onImageChangeTimeout()));
    mImageChangeTimer.start(SWITCH_DURATION);
    startPlay();
#endif
}

void AiCImageView::initImages()
{
    addImage(":/resources/images/image1.jpeg");
    addImage(":/resources/images/image2.jpeg");
    addImage(":/resources/images/image3.jpeg");
    addImage(":/resources/images/image4.jpeg");
    addImage(":/resources/images/image5.jpeg");
    addImage(":/resources/images/image6.jpeg");
}

void AiCImageView::setImageList(QStringList imageFileNameList)
{
    mImageFileNameList = imageFileNameList;
}

void AiCImageView::addImage(QString imageFileName)
{
    mImageFileNameList.append(imageFileName);
}

void AiCImageView::startPlay()
{
    if (mImageFileNameList.count() > 1)
    {
        mCurrentPixmap = QPixmap(mImageFileNameList.at(0));
        mNextPixmap = QPixmap(mImageFileNameList.at(1));
        mImageChangeTimer.start(SWITCH_DURATION);
        mOpacityAnimation->start();

        update();
    }
}

void AiCImageView::onImageChangeTimeout()
{
    int total = mImageFileNameList.count();
    mCurrentPixmap = QPixmap(mImageFileNameList.at(mCurrentDrawImageIndx));
    mCurrentDrawImageIndx = (mCurrentDrawImageIndx + 1) % total;
    mNextPixmap = QPixmap(mImageFileNameList.at(mCurrentDrawImageIndx));
#if QTLAUNCHER_SMALL_MEMORY
    update();
#else
    mOpacityAnimation->start();
#endif
}

void AiCImageView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    QRect imageRect = this->rect();
 #if QTLAUNCHER_SMALL_MEMORY
    painter.setOpacity(0.5);
    painter.drawPixmap(imageRect, mCurrentPixmap);
 #else
    float imageOpacity = this->property("ImageOpacity").toFloat();
    painter.setOpacity(1);
    painter.drawPixmap(imageRect, mNextPixmap.scaled(imageRect.size()));
    painter.setOpacity(imageOpacity);
    painter.drawPixmap(imageRect, mCurrentPixmap.scaled(imageRect.size()));
 #endif
}

void AiCImageView::mousePressEvent(QMouseEvent* event)
{
    return QWidget::mousePressEvent(event);
}

AiCImageView::~AiCImageView()
{
    if(mOpacityAnimation)
        delete mOpacityAnimation;

}
