/*
 * Copyright (C) 2024 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: artinchip
 */
#ifndef AICBRIEFVIEW_H
#define AICBRIEFVIEW_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>

class AiCBriefView : public QWidget
{
    Q_OBJECT
public:
    explicit AiCBriefView(QSize size, QWidget *parent = 0);
    ~AiCBriefView();

private:
    void initView(int width, int height);

signals:

public slots:

private:
    QVBoxLayout *mLayout;
    QLabel *mTitleLabel;
    QLabel *mImageLabel;
};

#endif // AICBRIEFVIEW_H
