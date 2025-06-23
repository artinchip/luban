/*
 * Copyright (C) 2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aicnavigationbar.h"
#include "utils/aicconsts.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QPainter>
#include <QLabel>
#include <QIcon>

AiCNavigationBar::AiCNavigationBar(QWidget *parent) : QWidget(parent)
{
    mSize = QSize(AIC_NAVIGATION_BAR_WIDTH, AIC_NAVIGATION_BAR_HEIGHT);
    initBar();
}

AiCNavigationBar::AiCNavigationBar(QSize size, QWidget *parent) : QWidget(parent)
{
    mSize = size;
    initBar();
}

void AiCNavigationBar::initBar()
{
    mLayout = new QHBoxLayout(this);
    mMenuButton = new QPushButton;
    mHomeButton = new QPushButton;
    mBackButton = new QPushButton;

    mMenuButton->setIcon(QIcon(":/resources/navbar/menu.png"));
    mHomeButton->setIcon(QIcon(":/resources/navbar/home.png"));
    mBackButton->setIcon(QIcon(":/resources/navbar/back.png"));

    mMenuButton->setIconSize(QSize(mSize.height(), mSize.height()));
    mHomeButton->setIconSize(QSize(mSize.height(), mSize.height()));
    mBackButton->setIconSize(QSize(mSize.height(), mSize.height()));

    mMenuButton->setFlat(true);
    mHomeButton->setFlat(true);
    mBackButton->setFlat(true);

    mMenuButton->setFixedWidth(mSize.width()/3);
    mHomeButton->setFixedWidth(mSize.width()/3);
    mBackButton->setFixedWidth(mSize.width()/3);

    mLayout->addStretch(1);
    mLayout->addWidget(mMenuButton);
    mLayout->addWidget(mHomeButton);
    mLayout->addWidget(mBackButton);
    mLayout->addStretch(1);
    mLayout->setMargin(0);
    mLayout->setSpacing(0);

    connect(mMenuButton, SIGNAL(clicked()), SIGNAL(MenuClick()));
    connect(mHomeButton, SIGNAL(clicked()), SIGNAL(HomeClick()));
    connect(mBackButton, SIGNAL(clicked()), SIGNAL(BackClick()));

    mBGPixmap = QPixmap(":/resources/navbar/bg.png");
    this->setFixedSize(mSize);
}

void AiCNavigationBar::disableMenu(bool enable)
{
    if (enable)
        mMenuButton->setIcon(QIcon(":/resources/navbar/menuhide.png"));
    else
        mMenuButton->setIcon(QIcon(":/resources/navbar/menu.png"));

    mMenuButton->setDisabled(enable);
}

void AiCNavigationBar::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.drawPixmap(0, 0, mBGPixmap.scaled(size()));
    QWidget::paintEvent(event);
}

AiCNavigationBar::~AiCNavigationBar()
{
    delete mMenuButton;
    delete mHomeButton;
    delete mBackButton;
    delete mLayout;
}
