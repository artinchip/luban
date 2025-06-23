/*
 * Copyright (C) 2024 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: artinchip
 */
#include "aicbasewindow.h"
#include "utils/aicconsts.h"

#include <QVBoxLayout>
#include <QPainter>
#include <QKeyEvent>
#include <QDebug>
#include <QDesktopWidget>
#include <QApplication>

AiCBaseWindow::AiCBaseWindow(QWidget *parent, Qt::WindowFlags flags)
    :QWidget(parent, flags)
{
    initWindow();
}

AiCBaseWindow::AiCBaseWindow(QPixmap background, QWidget *parent, Qt::WindowFlags flags)
    :QWidget(parent, flags)
    ,mBGPixmap(background)
{
    initWindow();
}

void AiCBaseWindow::initWindow()
{
    QDesktopWidget *pd = QApplication::desktop();
    QRect screenRect = pd->screenGeometry();

    mLayout = new QVBoxLayout(this);
    mCentralLayout = new QHBoxLayout();
    mNavBar = new AiCNavigationBar(QSize(screenRect.width(),AIC_NAVIGATION_BAR_HEIGHT));
    mStatusBar = new AiCStatusBar(QSize(screenRect.width(),AIC_STATUS_BAR_HEIGHT));

    mLayout->addWidget(mStatusBar, Qt::AlignTop);
    mLayout->addLayout(mCentralLayout, Qt::AlignCenter);
    mLayout->addWidget(mNavBar, Qt::AlignBottom);

    mLayout->setMargin(0);
    mLayout->setSpacing(0);

    connect(mNavBar, SIGNAL(MenuClick()), SLOT(onMenuClicked()));
    connect(mNavBar, SIGNAL(HomeClick()), SLOT(onHomeClicked()));
    connect(mNavBar, SIGNAL(BackClick()), SLOT(onBackClicked()));
}

void AiCBaseWindow::onMenuClicked()
{
    qDebug() << __FILE__ << __func__ << __LINE__;
}

void AiCBaseWindow::onHomeClicked()
{
    qDebug() << __FILE__ << __func__ << __LINE__;
    emit quit();
    close();
}

void AiCBaseWindow::onBackClicked()
{
    qDebug() << __FILE__ << __func__ << __LINE__;
    emit quit();
    close();
}

void AiCBaseWindow::paintEvent(QPaintEvent *event)
{
    if (!mBGPixmap.isNull()){
        QPainter painter(this);
        painter.drawPixmap(0, 0, mBGPixmap.scaled(size()));
    }

    QWidget::paintEvent(event);
}

AiCBaseWindow::~AiCBaseWindow()
{
    delete mNavBar;
    delete mStatusBar;
    delete mCentralLayout;
    delete mLayout;
}
