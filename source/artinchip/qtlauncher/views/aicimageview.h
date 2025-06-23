/*
 * Copyright (C) 2024 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: artinchip
 */
#ifndef AICIMAGEVIEW_H
#define AICIMAGEVIEW_H

#include <QWidget>
#include <QtGui>

class AiCImageView : public QWidget
{
    Q_OBJECT
public:
    explicit AiCImageView(QSize size, QWidget *parent = 0);
    ~AiCImageView();

    void setImageList(QStringList imageFileNameList);
    void addImage(QString imageFileName);
    void startPlay();

private:
    void initImages();
    void initChangeImageButton();
    void paintEvent(QPaintEvent *);
    void mousePressEvent(QMouseEvent* event);
public slots:
    void onImageChangeTimeout();

    private:
    int SWITCH_DURATION;
    QList<QString> mImageFileNameList;

    QTimer mImageChangeTimer;
    int mCurrentDrawImageIndx;

    QPixmap mCurrentPixmap;
    QPixmap mNextPixmap;
    QPropertyAnimation* mOpacityAnimation;
    QPropertyAnimation* mScaleAnimation1;
    QPropertyAnimation* mScaleAnimation2;
};

#endif // AICIMAGEVIEW_H
