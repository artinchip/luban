/*
 * Copyright (C) 2024 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: artinchip
 */
#ifndef AICDATETIMEVIEW_H
#define AICDATETIMEVIEW_H

#include "widgets/aictimewidget.h"

#include <QWidget>
#include <QTimer>
#include <QLCDNumber>
#include <QHBoxLayout>
#include <QVBoxLayout>

class AiCDateTimeView : public QWidget
{
    Q_OBJECT
public:
    explicit AiCDateTimeView(QSize size, QWidget *parent = 0);
    ~AiCDateTimeView();

private:
    void initView();

signals:

protected slots:
    void onTimeOut();

private:
    QTimer *mTimer;
    QVBoxLayout *mLayout;
    QHBoxLayout *mHLayout;
    QLCDNumber *mTimeNum;
    AiCTimeWidget *mTimeWidget1;
    AiCTimeWidget *mTimeWidget2;
    AiCTimeWidget *mTimeWidget3;
};

#endif // AICDATETIMEVIEW_H
