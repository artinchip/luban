/*
 * Copyright (C) 2024 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: artinchip
 */
#ifndef AICTIMEWIDGET_H
#define AICTIMEWIDGET_H

#include <QWidget>

class AiCTimeWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AiCTimeWidget(QWidget *parent = 0);
    explicit AiCTimeWidget(QSize size, QWidget *parent = 0);
    ~AiCTimeWidget();

    void setAutoMode(bool autoMode);
    void setShowStyle(int style);
protected:
    void paintEvent(QPaintEvent *event);
    void paintManually();
    void paintPng();
    void initWidget();

signals:

public slots:


private:
    int mStyle;
    bool mAutoMode;
    QTimer *mTimer;
    QPixmap mClockPixmap;
    QPixmap mCenterPixmap;
    QPixmap mHourPixmap;
    QPixmap mMinutePixmap;
    QPixmap mSecondPixmap;
};

#endif // AICTIMEWIDGET_H
