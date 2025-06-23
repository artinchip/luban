/*
 * Copyright (C) 2024 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: artinchip
 */
#include "aicbriefview.h"

#include <QDebug>

AiCBriefView::AiCBriefView(QSize size, QWidget *parent) : QWidget(parent)
{
    initView(size.width(), size.height());
}

void AiCBriefView::initView(int width, int height)
{
    mLayout = new QVBoxLayout(this);
    /*mTitleLabel = new QLabel();
    mTitleLabel->setFixedSize(width,50);
    mTitleLabel->setText("AIC 1602");
    mTitleLabel->setAlignment(Qt::AlignCenter);
    mTitleLabel->setStyleSheet("color: #fff;font-size:38px;");
    mLayout->addWidget(mTitleLabel, Qt::AlignCenter);
    */

    mImageLabel = new QLabel();
    mImageLabel->setFixedSize(width,height);
    mImageLabel->setPixmap(QPixmap(":/resources/brief.png"));
    mImageLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    mLayout->addWidget(mImageLabel, Qt::AlignCenter);
}

AiCBriefView::~AiCBriefView()
{
    delete mImageLabel;
    delete mLayout;
}
