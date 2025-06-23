/*
 * Copyright (C) 2024 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: artinchip
 */
#include "aicdatetimeview.h"

#include <QDebug>
#include <QTime>

AiCDateTimeView::AiCDateTimeView(QSize size, QWidget *parent) : QWidget(parent)
{
    this->setFixedSize(size);
    initView();
}


void AiCDateTimeView::initView()
{
    mLayout = new QVBoxLayout(this);
    mHLayout = new QHBoxLayout();

    mLayout->setContentsMargins(5,20,5,150);

    mTimeNum = new QLCDNumber();
    mTimeNum->setFixedSize(500,50);
    mTimeNum->setDigitCount(19);
    mTimeNum->setSegmentStyle(QLCDNumber::Flat);
    mTimeNum->setStyleSheet("background-color:#fff; color: #009230; font-size:18px;");
    mLayout->addWidget(mTimeNum, 0, Qt::AlignCenter);

    mLayout->addLayout(mHLayout);

    mTimeWidget1 = new AiCTimeWidget(QSize(210,210));
    mTimeWidget1->setShowStyle(0);
    mHLayout->addWidget(mTimeWidget1, Qt::AlignTop);

    mTimeWidget2 = new AiCTimeWidget(QSize(210,210));
    mTimeWidget2->setShowStyle(1);
    mHLayout->addWidget(mTimeWidget2, Qt::AlignTop);

    mTimeWidget3 = new AiCTimeWidget(QSize(210,210));
    mTimeWidget3->setShowStyle(2);
    mHLayout->addWidget(mTimeWidget3, Qt::AlignTop);

    mTimer = new QTimer(this);
    connect(mTimer, SIGNAL(timeout()), this, SLOT(onTimeOut()));
    mTimer->start(1000);
}

void AiCDateTimeView::onTimeOut()
{
    mTimeWidget1->update();
    mTimeWidget2->update();
    mTimeWidget3->update();

    QTime time = QTime::currentTime();
    QDate date = QDate::currentDate();

    if ((time.second() % 2) == 0)
        mTimeNum->display(date.toString("yyyy-MM-dd ") + time.toString("hh:mm ss"));
    else
        mTimeNum->display(date.toString("yyyy-MM-dd ") + time.toString("hh:mm:ss"));
}

AiCDateTimeView::~AiCDateTimeView()
{
    mTimer->stop();
    delete mTimer;
    delete mLayout;
    delete mTimeWidget1;
    delete mTimeWidget2;
    delete mTimeWidget3;
}
