/*
 * Copyright (C) 2024 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: artinchip
 */
#include "aicdesktopbutton.h"

#include <QPainter>
#include <QColor>
#include <QSize>
#include <QDebug>

AiCDesktopButton::AiCDesktopButton()
{
    mSize = QSize(220,160);
    mBGColor = BG_WHITE;
    initButton();
}

AiCDesktopButton::AiCDesktopButton(QSize size, AiCDesktopColor color)
{
    mSize = size;
    mBGColor = color;
    initButton();
}

void AiCDesktopButton::initButton()
{
    mLayout = new QHBoxLayout(this);
    setFixedSize(mSize);
}

QColor AiCDesktopButton::getColor(AiCDesktopColor color)
{
    switch (color){
        case BG_WHITE:
            return QColor(255,255,255);
        case BG_DEEP_RED:
            return QColor(208,0,39);
        case BG_LIGHT_RED:
            return QColor(204,12,93);
        case BG_DEEP_GREEN:
            return QColor(208,0,39);
        case BG_LIGHT_GREEN:
            return QColor(0,146,68);
        case BG_DEEP_PURPLE:
            return QColor(131,53,255);
        case BG_LIGHT_PURPLE:
            return QColor(125,165,0);
        case BG_DEEP_BLUE:
            return QColor(2,141,192);
        case BG_LIGHT_BLUE:
            return QColor(91,153,212);
        case BG_DEEP_YELLOW:
            return QColor(208,0,39);
        case BG_LIGHT_YELLOW:
            return QColor(234,219,1);
        default:
            return QColor(255,255,255);
    }
}

void AiCDesktopButton::setParameters(QPixmap pixmap, QString title)
{
#if 1
    mLayout->setContentsMargins(5, 40 , 5, 40);

    mIconLabel = new QLabel(this);
    mIconLabel->setPixmap(pixmap);
    mIconLabel->setAlignment(Qt::AlignRight);
    mLayout->addWidget(mIconLabel,0, Qt::AlignVCenter);

    mTitleLabel = new QLabel(this);
    mTitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
    mTitleLabel->setText(title);
    mTitleLabel->setStyleSheet("color: #ffffff;font-size:28px;");
    mLayout->addWidget(mTitleLabel, 0, Qt::AlignBottom);
#else
    mLayout->setContentsMargins(5, 5 , 5, 5);

    mIconLabel = new QLabel(this);
    mIconLabel->setPixmap(pixmap);
    mIconLabel->setAlignment(Qt::AlignRight);
    mLayout->addWidget(mIconLabel,0, Qt::AlignVCenter);

    mTitleLabel = new QLabel(this);
    mTitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
    mTitleLabel->setText(title);
    mTitleLabel->setStyleSheet("color: #ffffff;font-size:28px;");
    mLayout->addWidget(mTitleLabel, 0, Qt::AlignVCenter);
#endif
}

void AiCDesktopButton::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.fillRect(this->rect(),getColor(mBGColor));
    QWidget::paintEvent(event);
}

AiCDesktopButton::~AiCDesktopButton()
{
    delete mTitleLabel;
    delete mIconLabel;
    delete mLayout;
}
