/*
 * Copyright (C) 2024 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: artinchip
 */
#ifndef AICSTATUSBAR_H
#define AICSTATUSBAR_H

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QPaintEvent>
#include <QTime>
#include <QTimer>
#include <QSize>

#include "utils/aictypes.h"

class AiCStatusBar : public QWidget
{
    Q_OBJECT
public:
    explicit AiCStatusBar(QWidget *parent = 0);
    explicit AiCStatusBar(QSize size, QWidget *parent = 0);
    ~AiCStatusBar();

    void updateTime();
    void updateTitle(QString strTitle);
    void updateWiFi(WifiStatusType status);

protected:
    void paintEvent(QPaintEvent *event);

private:
    void initBar();
    void setupTimer();

signals:

protected slots:
    void onTimeOut();

private:
    QSize mSize;
    QTimer *mTimer;
    QHBoxLayout *mLayout;
    QLabel *mTimeLabel;
    QLabel *mTitleLabel;
    QLabel *mWiFiLabel;

    QPixmap mBGPixmap;
    QPixmap mWiFiOnPixmap;
    QPixmap mWiFiOffPixmap;
    WifiStatusType mWifiStatus;
};

#endif // AICSTATUSBAR_H
